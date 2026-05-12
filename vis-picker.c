#include "vis-core.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Picker mode input handler: printable chars go to filter */
void vis_picker_input(Vis *vis, const char *data, size_t len) {
	if (!vis->picker.active)
		return;

	for (size_t i = 0; i < len; i++) {
		char c = data[i];
		if (c >= 0x20 && c < 0x7f && vis->picker.filter_len < sizeof(vis->picker.filter) - 2) {
			vis->picker.filter[vis->picker.filter_len++] = c;
		}
	}
	vis->picker.filter[vis->picker.filter_len] = '\0';
	picker_refilter(vis);
	vis->picker.selected = 0;
	vis->picker.scroll_offset = 0;
	vis_draw(vis);
}

static void picker_preview_load(Vis *vis, const char *path, int line);

static void picker_item_free(PickerItem *item) {
	if (!item)
		return;
	free(item->label);
	free(item->path);
	free(item->detail);
	free(item->preview_path);
	memset(item, 0, sizeof(*item));
}

static void picker_items_free(PickerItem *items, int count) {
	for (int i = 0; i < count; i++)
		picker_item_free(&items[i]);
	free(items);
}

static bool picker_item_set(PickerItem *item, PickerItemKind kind, const char *label, const char *path) {
	memset(item, 0, sizeof(*item));
	item->kind = kind;
	item->label = strdup(label ? label : "");
	item->path = path ? strdup(path) : NULL;
	item->line = 0;
	item->column = 0;
	if (!item->label || (path && !item->path)) {
		picker_item_free(item);
		return false;
	}
	return true;
}

static bool picker_item_copy(PickerItem *dst, const PickerItem *src) {
	memset(dst, 0, sizeof(*dst));
	if (!src)
		return false;
	dst->kind = src->kind;
	dst->line = src->line;
	dst->column = src->column;
	dst->label = src->label ? strdup(src->label) : NULL;
	dst->path = src->path ? strdup(src->path) : NULL;
	dst->detail = src->detail ? strdup(src->detail) : NULL;
	dst->preview_path = src->preview_path ? strdup(src->preview_path) : NULL;
	if ((src->label && !dst->label) || (src->path && !dst->path) ||
	    (src->detail && !dst->detail) || (src->preview_path && !dst->preview_path)) {
		picker_item_free(dst);
		return false;
	}
	return true;
}

static const char *picker_item_preview_path(const PickerItem *item) {
	if (!item)
		return NULL;
	if (item->preview_path)
		return item->preview_path;
	if (item->path)
		return item->path;
	return item->label;
}

/* Set up key bindings for the picker mode (called once) */
static bool picker_bindings_init(Vis *vis) {
	Mode *picker_mode = &vis_modes[VIS_MODE_PICKER];
	if (picker_mode->bindings)
		return true; /* already initialized */

	picker_mode->bindings = map_new();
	if (!picker_mode->bindings)
		return false;

	static const struct { const char *key; const char *action; } bindings[] = {
		{ "<Down>",          "vis-picker-down" },
		{ "<Tab>",           "vis-picker-down" },
		{ "<C-n>",           "vis-picker-down" },
		{ "<Up>",            "vis-picker-up" },
		{ "<S-Tab>",         "vis-picker-up" },
		{ "<C-p>",           "vis-picker-up" },
		{ "<PageDown>",      "vis-picker-page-down" },
		{ "<C-d>",           "vis-picker-page-down" },
		{ "<PageUp>",        "vis-picker-page-up" },
		{ "<C-u>",           "vis-picker-page-up" },
		{ "<Home>",          "vis-picker-first" },
		{ "<End>",           "vis-picker-last" },
		{ "<C-t>",           "vis-picker-toggle-preview" },
		{ "<Enter>",         "vis-picker-accept" },
		{ "<C-j>",           "vis-picker-accept" },
		{ "<M-s>",           "vis-picker-accept-split" },
		{ "<M-v>",           "vis-picker-accept-vsplit" },
		{ "<Escape>",        "vis-picker-cancel" },
		{ "<C-c>",           "vis-picker-cancel" },
		{ "<C-g>",           "vis-picker-cancel" },
		{ "<Backspace>",     "vis-picker-backspace" },
		{ "<C-h>",           "vis-picker-backspace" },
		{ "<C-w>",           "vis-picker-delete-word" },
		{ "<C-x>",           "vis-picker-clear-filter" },
	};

	for (size_t i = 0; i < LENGTH(bindings); i++) {
		const KeyAction *action = map_get(vis->actions, bindings[i].action);
		if (!action) continue;
		KeyBinding *binding = calloc(1, sizeof(KeyBinding));
		if (!binding) continue;
		binding->action = action;
		map_put(picker_mode->bindings, bindings[i].key, binding);
	}
	return true;
}

/* Open the picker with a list of items */
void picker_open(Vis *vis, PickerItem *items, int count, void (*on_select)(Vis*, const PickerItem*, PickerOpenMode)) {
	if (!picker_bindings_init(vis))
		goto err;

	vis->picker.active = true;
	vis->picker.filter[0] = '\0';
	vis->picker.filter_len = 0;
	vis->picker.selected = 0;
	vis->picker.scroll_offset = 0;
	vis->picker.preview_visible = true;
	vis->picker.items = items;
	vis->picker.item_count = count;
	vis->picker.on_select = on_select;
	vis->picker.saved_win = vis->win;
	vis->picker.saved_mode = vis->mode;

	vis->picker.filtered = count > 0 ? calloc(count, sizeof(PickerItem*)) : NULL;
	if (count > 0 && !vis->picker.filtered)
		goto err;
	vis->picker.filtered_count = count;

	for (int i = 0; i < count; i++) {
		vis->picker.filtered[i] = &items[i];
	}

	/* Don't call vis_mode_switch — it invokes enter/leave callbacks.
	   Just set the mode directly, like prompt_restore does. */
	vis->mode = &vis_modes[VIS_MODE_PICKER];
	vis_draw(vis);
	return;

err:
	picker_items_free(items, count);
	free(vis->picker.filtered);
	memset(&vis->picker, 0, sizeof(vis->picker));
}

/* Close the picker, optionally accepting the selection */
static void picker_close(Vis *vis, bool accept, PickerOpenMode open_mode) {
	if (!vis->picker.active)
		return;

	PickerItem selection = {0};
	bool has_selection = false;
	if (accept && vis->picker.filtered_count > 0) {
		has_selection = picker_item_copy(&selection, vis->picker.filtered[vis->picker.selected]);
	}

	void (*on_select)(Vis*, const PickerItem*, PickerOpenMode) = vis->picker.on_select;

	picker_items_free(vis->picker.items, vis->picker.item_count);
	free(vis->picker.filtered);

	/* Clean up preview */
	picker_preview_load(vis, NULL, 0);

	vis->picker.active = false;
	vis->picker.items = NULL;
	vis->picker.filtered = NULL;

	if (vis->picker.saved_win)
		vis->win = vis->picker.saved_win;
	if (vis->picker.saved_mode)
		vis->mode = vis->picker.saved_mode;
	else
		vis->mode = &vis_modes[VIS_MODE_NORMAL];

	if (on_select && has_selection)
		on_select(vis, &selection, open_mode);

	picker_item_free(&selection);

	vis_draw(vis);
}

/* Called when mode is switched away from PICKER externally
 * (e.g. :set keymap, or any other mode switch bypassing picker_close).
 * Cleans up stale picker state to prevent state leaks between tests. */
void vis_picker_leave(Vis *vis, Mode *old_mode) {
	(void)old_mode;
	if (!vis->picker.active)
		return;
	picker_close(vis, false, PICKER_OPEN_CURRENT);
}

/* Fuzzy match: returns score if needle matches haystack, 0 if no match.
   Higher score = better match. Uses fzy-style scoring. */
static double picker_fuzzy_score(const char *haystack, const char *needle) {
	if (!needle[0]) return 1.0;
	if (!haystack[0]) return 0.0;

	size_t nlen = strlen(needle);
	size_t hlen = strlen(haystack);
	if (nlen > hlen) return 0.0;

	/* Find match positions greedily */
	int positions[256]; /* positions[i] = index in haystack where needle[i] matches */
	size_t hi = 0;
	for (size_t ni = 0; ni < nlen; ni++) {
		char nc = needle[ni];
		char nc_lower = (nc >= 'A' && nc <= 'Z') ? nc + 32 : nc;
		bool found = false;
		while (hi < hlen) {
			char hc = haystack[hi];
			char hc_lower = (hc >= 'A' && hc <= 'Z') ? hc + 32 : hc;
			if (hc_lower == nc_lower) {
				positions[ni] = (int)hi;
				hi++;
				found = true;
				break;
			}
			hi++;
		}
		if (!found) return 0.0;
	}

	/* Calculate score */
	double score = 0.0;
	for (size_t i = 0; i < nlen; i++) {
		int pos = positions[i];
		score += 1.0;
		/* Bonus for matching at start */
		if (pos == 0) score += 5.0;
		/* Bonus for matching after separator */
		if (pos > 0) {
			char prev = haystack[pos - 1];
			if (prev == '/' || prev == '_' || prev == '-' || prev == '.' || prev == ' ')
				score += 2.0;
		}
		/* Bonus for consecutive matches */
		if (i > 0 && positions[i] == positions[i-1] + 1) score += 2.0;
		/* Bonus for exact case match */
		if (needle[i] == haystack[pos]) score += 0.5;
	}
	/* Penalty for gaps */
	for (size_t i = 1; i < nlen; i++) {
		int gap = positions[i] - positions[i-1] - 1;
		score -= gap * 0.5;
	}
	/* Prefer shorter spans */
	int span = positions[nlen-1] - positions[0] + 1;
	score += ((int)hlen - span) * 0.1;

	return score;
}

/* Internal: scored item for sorting */
typedef struct {
	int original_index;
	double score;
} ScoredItem;

static int scored_item_cmp(const void *a, const void *b) {
	const ScoredItem *sa = (const ScoredItem *)a;
	const ScoredItem *sb = (const ScoredItem *)b;
	/* Higher score first */
	if (sa->score > sb->score) return -1;
	if (sa->score < sb->score) return 1;
	/* Break ties by original order */
	return (sa->original_index > sb->original_index) - (sa->original_index < sb->original_index);
}

/* Refilter items based on current filter string, sorted by fuzzy score */
void picker_refilter(Vis *vis) {
	if (vis->picker.filter_len == 0) {
		/* No filter: show all items in original order */
		vis->picker.filtered_count = vis->picker.item_count;
		for (int i = 0; i < vis->picker.item_count; i++) {
			vis->picker.filtered[i] = &vis->picker.items[i];
		}
		return;
	}

	/* Score all items */
	int max_items = vis->picker.item_count;
	ScoredItem *scored = malloc(max_items * sizeof(ScoredItem));
	if (!scored) {
		vis->picker.filtered_count = 0;
		return;
	}
	int scored_count = 0;
	for (int i = 0; i < max_items; i++) {
		double s = picker_fuzzy_score(vis->picker.items[i].label, vis->picker.filter);
		if (s > 0.0) {
			scored[scored_count].original_index = i;
			scored[scored_count].score = s;
			scored_count++;
		}
	}

	/* Sort by score descending */
	qsort(scored, scored_count, sizeof(ScoredItem), scored_item_cmp);

	/* Build filtered list */
	vis->picker.filtered_count = 0;
	for (int i = 0; i < scored_count; i++) {
		int idx = scored[i].original_index;
		vis->picker.filtered[i] = &vis->picker.items[idx];
		vis->picker.filtered_count++;
	}
	free(scored);
}

/* Picker key actions */
static KEY_ACTION_FN(ka_picker_down) {
	if (!vis->picker.active) return keys;
	if (vis->picker.filtered_count > 0) {
		vis->picker.selected++;
		if (vis->picker.selected >= vis->picker.filtered_count)
			vis->picker.selected = vis->picker.filtered_count - 1;
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_up) {
	if (!vis->picker.active) return keys;
	if (vis->picker.filtered_count > 0) {
		vis->picker.selected--;
		if (vis->picker.selected < 0)
			vis->picker.selected = 0;
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_page) {
	if (!vis->picker.active) return keys;
	if (vis->picker.filtered_count > 0) {
		int page = vis->ui.height - 4;
		if (page < 1) page = 1;
		vis->picker.selected += arg->i * page;
		if (vis->picker.selected < 0)
			vis->picker.selected = 0;
		if (vis->picker.selected >= vis->picker.filtered_count)
			vis->picker.selected = vis->picker.filtered_count - 1;
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_edge) {
	if (!vis->picker.active) return keys;
	if (vis->picker.filtered_count > 0)
		vis->picker.selected = arg->i < 0 ? 0 : vis->picker.filtered_count - 1;
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_toggle_preview) {
	if (!vis->picker.active) return keys;
	vis->picker.preview_visible = !vis->picker.preview_visible;
	if (!vis->picker.preview_visible)
		picker_preview_load(vis, NULL, 0);
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_accept) {
	picker_close(vis, true, PICKER_OPEN_CURRENT);
	return keys;
}

static KEY_ACTION_FN(ka_picker_accept_open) {
	picker_close(vis, true, arg->i);
	return keys;
}

static KEY_ACTION_FN(ka_picker_cancel) {
	picker_close(vis, false, PICKER_OPEN_CURRENT);
	return keys;
}

static KEY_ACTION_FN(ka_picker_backspace) {
	if (!vis->picker.active) return keys;
	if (vis->picker.filter_len > 0) {
		vis->picker.filter_len--;
		vis->picker.filter[vis->picker.filter_len] = '\0';
		picker_refilter(vis);
		vis->picker.selected = 0;
		vis->picker.scroll_offset = 0;
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_delete_word) {
	if (!vis->picker.active) return keys;
	while (vis->picker.filter_len > 0) {
		vis->picker.filter_len--;
		vis->picker.filter[vis->picker.filter_len] = '\0';
		if (vis->picker.filter_len > 0 &&
		    vis->picker.filter[vis->picker.filter_len - 1] == ' ')
			break;
	}
	picker_refilter(vis);
	vis->picker.selected = 0;
	vis->picker.scroll_offset = 0;
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_clear_filter) {
	if (!vis->picker.active) return keys;
	vis->picker.filter_len = 0;
	vis->picker.filter[0] = '\0';
	picker_refilter(vis);
	vis->picker.selected = 0;
	vis->picker.scroll_offset = 0;
	vis_draw(vis);
	return keys;
}

static void picker_jump_to_item(Vis *vis, const PickerItem *item) {
	if (!vis->win || !vis->win->file || !item || item->line <= 0)
		return;
	size_t pos = text_pos_by_lineno(vis->win->file->text, item->line);
	if (pos == EPOS)
		return;
	if (item->column > 0)
		pos = text_line_char_set(vis->win->file->text, pos, item->column - 1);
	if (pos != EPOS)
		view_cursors_to(vis->win->view.selection, pos);
}

static bool picker_open_path(Vis *vis, const char *path, PickerOpenMode open_mode) {
	if (!path || !vis->win)
		return false;
	switch (open_mode) {
	case PICKER_OPEN_HORIZONTAL:
		ui_arrange(&vis->ui, UI_LAYOUT_HORIZONTAL);
		return vis_window_new(vis, path);
	case PICKER_OPEN_VERTICAL:
		ui_arrange(&vis->ui, UI_LAYOUT_VERTICAL);
		return vis_window_new(vis, path);
	case PICKER_OPEN_CURRENT:
	default:
		/* Keep old file alive (hidden buffer) so buffer picker can find it. */
		if (vis->win->file)
			vis->win->file->refcount++;
		return vis_window_change_file(vis->win, path);
	}
}

/* File picker callback: open selected file. */
static void picker_on_file_select(Vis *vis, const PickerItem *item, PickerOpenMode open_mode) {
	const char *path = item ? item->path : NULL;
	if (picker_open_path(vis, path, open_mode))
		picker_jump_to_item(vis, item);
}

/* Check if a file is binary by reading the first 8KB.
 * Returns true if the file contains null bytes or is predominantly non-printable. */
static bool is_binary_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return true; /* can't read = skip */
	unsigned char buf[8192];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n == 0) return false; /* empty file = not binary */
	int non_printable = 0;
	for (size_t i = 0; i < n; i++) {
		if (buf[i] == 0) return true; /* null byte = binary */
		if (buf[i] < 7 || (buf[i] > 14 && buf[i] < 32))
			non_printable++;
	}
	/* If > 30%% non-printable, treat as binary (catches most binaries) */
	return (non_printable * 100 / n) > 30;
}

static bool picker_ignore_entry(const char *name) {
	static const char *ignored[] = {
		".git", "node_modules", "target", "build", "dist", ".cache", "dependency",
	};
	for (size_t i = 0; i < LENGTH(ignored); i++) {
		if (strcmp(name, ignored[i]) == 0)
			return true;
	}
	return false;
}

static bool path_has_child(const char *dir, const char *name) {
	char path[4096];
	struct stat st;
	if (snprintf(path, sizeof(path), "%s/%s", dir, name) >= (int)sizeof(path))
		return false;
	return stat(path, &st) == 0;
}

static bool picker_workspace_root(char *root, size_t len) {
	if (!getcwd(root, len))
		return false;

	for (;;) {
		if (path_has_child(root, ".git") || path_has_child(root, ".jj") || path_has_child(root, ".helix"))
			return true;

		char *slash = strrchr(root, '/');
		if (!slash || slash == root) {
			if (slash == root)
				root[1] = '\0';
			return true;
		}
		*slash = '\0';
	}
}

/* Recursive helper: collect files from directory tree.
 * depth=0 means list files/dirs in given dir. depth>0 recurses into subdirs.
 * prefix is the relative path to prepend (used for recursion). */
static void picker_collect_files(const char *dirpath, const char *prefix,
                                  PickerItem **items, int *count, int *capacity,
                                  int depth) {
	DIR *d = opendir(dirpath);
	if (!d) return;

	struct dirent *entry;
	while ((entry = readdir(d))) {
		if (entry->d_name[0] == '.' || picker_ignore_entry(entry->d_name)) continue;
		/* Build full relative path */
		char relpath[4096];
		if (prefix[0])
			snprintf(relpath, sizeof(relpath), "%s/%s", prefix, entry->d_name);
		else
			snprintf(relpath, sizeof(relpath), "%s", entry->d_name);

		/* Check file type. Do not follow symlinked directories while walking. */
		{
			char fullpath[4096];
			struct stat st;
			snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
			if (lstat(fullpath, &st) != 0)
				continue;
			if (S_ISDIR(st.st_mode) && depth > 0) {
				picker_collect_files(fullpath, relpath, items, count, capacity, depth - 1);
				continue;
			}
			if (!S_ISREG(st.st_mode))
				continue;
		}

		/* Add file to list */
		if (*count >= *capacity) {
			*capacity *= 2;
			PickerItem *new_items = realloc(*items, *capacity * sizeof(PickerItem));
			if (!new_items) {
				closedir(d);
				return;
			}
			*items = new_items;
		}
		char item_path[4096];
		snprintf(item_path, sizeof(item_path), "%s/%s", dirpath, entry->d_name);
		if (!picker_item_set(&(*items)[*count], PICKER_ITEM_FILE, relpath, item_path))
			continue;
		(*count)++;
	}
	closedir(d);
}

/* qsort comparison for file listing:
 * 1. Files in root directory (no '/') come first
 * 2. Within same depth, sort case-insensitively alphabetically */
static int picker_cmp_file(const void *a, const void *b) {
	const PickerItem *ia = a;
	const PickerItem *ib = b;
	const char *pa = ia->label;
	const char *pb = ib->label;

	/* Count path separators (directory depth) */
	int da = 0, db = 0;
	for (const char *s = pa; *s; s++) if (*s == '/') da++;
	for (const char *s = pb; *s; s++) if (*s == '/') db++;
	if (da != db) return da - db;

	/* Same depth: compare basename (after last '/') */
	const char *ba = strrchr(pa, '/'); ba = ba ? ba + 1 : pa;
	const char *bb = strrchr(pb, '/'); bb = bb ? bb + 1 : pb;
	return strcasecmp(ba, bb);
}

/* qsort comparison for buffer listing: sort alphabetically by basename */
static int picker_cmp_buffer(const void *a, const void *b) {
	const PickerItem *ia = a;
	const PickerItem *ib = b;
	const char *pa = ia->label;
	const char *pb = ib->label;
	const char *ba = strrchr(pa, '/'); ba = ba ? ba + 1 : pa;
	const char *bb = strrchr(pb, '/'); bb = bb ? bb + 1 : pb;
	return strcasecmp(ba, bb);
}

static void picker_open_files_at(Vis *vis, const char *root) {
	PickerItem *items = NULL;
	int count = 0;
	int capacity = 128;
	items = calloc(capacity, sizeof(PickerItem));
	if (!items)
		return;

	/* Collect files recursively (depth=2: current dir + 1 level of subdirs) */
	picker_collect_files(root, "", &items, &count, &capacity, 2);

	/* Sort: current-dir files first, then subdir files, all alphabetical */
	qsort(items, count, sizeof(PickerItem), picker_cmp_file);

	picker_open(vis, items, count, picker_on_file_select);
}

/* Open file picker at workspace root, falling back to the current directory. */
void picker_open_files(Vis *vis) {
	char root[4096];
	if (!picker_workspace_root(root, sizeof(root)))
		strncpy(root, ".", sizeof(root) - 1);
	root[sizeof(root) - 1] = '\0';
	picker_open_files_at(vis, root);
}

/* Open file picker rooted at the current working directory. */
void picker_open_files_current(Vis *vis) {
	picker_open_files_at(vis, ".");
}

/* Buffer picker callback: switch to selected buffer */
static void picker_on_buffer_select(Vis *vis, const PickerItem *item, PickerOpenMode open_mode) {
	const char *path = item ? item->path : NULL;
	if (!path || !vis->win) return;
	if (open_mode != PICKER_OPEN_CURRENT) {
		if (picker_open_path(vis, path, open_mode))
			picker_jump_to_item(vis, item);
		return;
	}
	/* Find existing window for this file */
	for (Win *win = vis->windows; win; win = win->next) {
		if (win->file && win->file->name && strcmp(win->file->name, path) == 0) {
			vis->win = win;
			picker_jump_to_item(vis, item);
			return;
		}
	}
	if (picker_open_path(vis, path, open_mode))
		picker_jump_to_item(vis, item);
}

static void picker_on_jumplist_select(Vis *vis, const PickerItem *item, PickerOpenMode open_mode) {
	(void)open_mode;
	picker_jump_to_item(vis, item);
}

void picker_open_jumplist(Vis *vis) {
	Win *win = vis->win;
	if (!win || !win->file)
		return;
	PickerItem *items = calloc(VIS_MARK_SET_LRU_COUNT, sizeof(PickerItem));
	if (!items)
		return;

	int count = 0;
	const char *path = win->file->name;
	const char *name = path ? path : "[No Name]";
	for (size_t i = 0; i < VIS_MARK_SET_LRU_COUNT; i++) {
		SelectionRegionList *regions = &win->mark_set_lru_regions[i];
		if (!regions->count)
			continue;
		Filerange range = view_regions_restore(&win->view, regions->data[0]);
		if (!text_range_valid(range))
			continue;
		int line = (int)text_lineno_by_pos(win->file->text, range.start);
		int column = text_line_char_get(win->file->text, range.start) + 1;
		char label[4096];
		snprintf(label, sizeof(label), "%s:%d:%d", name, line, column);
		if (!picker_item_set(&items[count], PICKER_ITEM_LOCATION, label, path))
			continue;
		items[count].line = line;
		items[count].column = column;
		count++;
	}

	if (count == 0) {
		picker_items_free(items, count);
		return;
	}
	picker_open(vis, items, count, picker_on_jumplist_select);
}

/* Open buffer picker: list open buffers */
void picker_open_buffers(Vis *vis) {
	/* Count non-internal files */
	int count = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (!f->internal && f->name) count++;
	}
	if (count == 0) return;

	PickerItem *items = calloc(count, sizeof(PickerItem));
	if (!items)
		return;
	int i = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (f->internal || !f->name) continue;
		if (picker_item_set(&items[i], PICKER_ITEM_BUFFER, f->name, f->name))
			i++;
	}
	count = i;
	if (count == 0) {
		picker_items_free(items, count);
		return;
	}
	/* Sort alphabetically by filename */
	qsort(items, count, sizeof(PickerItem), picker_cmp_buffer);
	picker_open(vis, items, count, picker_on_buffer_select);
}

void picker_open_changed(Vis *vis) {
	int count = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (!f->internal && f->name && text_modified(f->text))
			count++;
	}
	if (count == 0)
		return;

	PickerItem *items = calloc(count, sizeof(PickerItem));
	if (!items)
		return;
	int i = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (f->internal || !f->name || !text_modified(f->text))
			continue;
		if (picker_item_set(&items[i], PICKER_ITEM_BUFFER, f->name, f->name))
			i++;
	}
	count = i;
	if (count == 0) {
		picker_items_free(items, count);
		return;
	}
	qsort(items, count, sizeof(PickerItem), picker_cmp_buffer);
	picker_open(vis, items, count, picker_on_buffer_select);
}

static KEY_ACTION_FN(ka_picker_files) {
	if (vis->picker.active) return keys;
	picker_open_files(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_files_current) {
	if (vis->picker.active) return keys;
	picker_open_files_current(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_buffers) {
	if (vis->picker.active) return keys;
	picker_open_buffers(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_jumplist) {
	if (vis->picker.active) return keys;
	picker_open_jumplist(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_changed) {
	if (vis->picker.active) return keys;
	picker_open_changed(vis);
	return keys;
}

/* Load preview for a file: first N lines */
static void picker_preview_load(Vis *vis, const char *path, int line) {
	/* Free previous preview */
	for (int i = 0; i < vis->picker_preview.line_count; i++)
		free(vis->picker_preview.lines[i]);
	free(vis->picker_preview.lines);
	free(vis->picker_preview.path);
	vis->picker_preview.lines = NULL;
	vis->picker_preview.line_count = 0;
	vis->picker_preview.line = 0;
	vis->picker_preview.path = NULL;

	if (!path) return;
	if (is_binary_file(path)) {
		vis->picker_preview.lines = calloc(1, sizeof(char*));
		if (vis->picker_preview.lines) {
			vis->picker_preview.lines[0] = strdup("[binary or unreadable file]");
			if (vis->picker_preview.lines[0])
				vis->picker_preview.line_count = 1;
		}
		vis->picker_preview.path = strdup(path);
		vis->picker_preview.line = line;
		return;
	}

	FILE *f = fopen(path, "r");
	if (!f) return;

	int capacity = 32;
	int start_line = line > 16 ? line - 15 : 1;
	int current_line = 1;
	vis->picker_preview.lines = calloc(capacity, sizeof(char*));
	char buf[1024];
	while (current_line < start_line && fgets(buf, sizeof(buf), f))
		current_line++;
	while (vis->picker_preview.line_count < capacity && fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
		if (line > 0) {
			char linebuf[1200];
			snprintf(linebuf, sizeof(linebuf), "%c%5d %s", current_line == line ? '>' : ' ', current_line, buf);
			vis->picker_preview.lines[vis->picker_preview.line_count++] = strdup(linebuf);
		} else {
			vis->picker_preview.lines[vis->picker_preview.line_count++] = strdup(buf);
		}
		current_line++;
	}
	fclose(f);
	vis->picker_preview.path = strdup(path);
	vis->picker_preview.line = line;
}

/* Check if preview needs updating for current selection */
static void picker_preview_update(Vis *vis) {
	if (!vis->picker.active || vis->picker.filtered_count == 0) {
		picker_preview_load(vis, NULL, 0);
		return;
	}
	PickerItem *item = vis->picker.filtered[vis->picker.selected];
	const char *path = picker_item_preview_path(vis->picker.filtered[vis->picker.selected]);
	if (!path) {
		picker_preview_load(vis, NULL, 0);
		return;
	}

	/* Only reload if path changed */
	if (vis->picker_preview.path && strcmp(vis->picker_preview.path, path) == 0 &&
	    vis->picker_preview.line == item->line)
		return;

	picker_preview_load(vis, path, item->line);
}

/* Draw the picker overlay into the UI cells buffer */
void picker_draw(Vis *vis) {
	if (!vis->picker.active) return;
	if (vis->picker.preview_visible)
		picker_preview_update(vis);

	Ui *ui = &vis->ui;
	int width = ui->width;
	int height = ui->height;

	/* Full-screen layout: fill from top to just above status line */
	int picker_height = height - 1; /* leave bottom row for status */
	if (picker_height < 8) return;

	/* Proportional split: file list gets ~35%, minimum 28 columns */
	int list_width = width * 32 / 100;
	if (list_width < 24) list_width = 24;
	if (list_width > width - 25) list_width = width - 25;
	if (!vis->picker.preview_visible)
		list_width = width;
	int divider_x = list_width;

	/* Styles */
	CellStyle bg = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = 0 };
	CellStyle dim = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_DIM };
	CellStyle sel = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_REVERSE };
	CellStyle prompt_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_BOLD };
	CellStyle line_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_DIM };
	CellStyle preview_bg = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = 0 };

	Cell sp = { .data = " ", .len = 1, .width = 1, .style = bg };

	/* Row 0: Filter prompt */
	int row = 0;
	/* Prompt ">" */
	const char *prompt = ">";
	for (int i = 0; prompt[i]; i++) {
		Cell cell = { .data = " ", .len = 1, .width = 1, .style = prompt_style };
		cell.data[0] = prompt[i];
		ui->cells[row * width + i] = cell;
	}
	/* Space after prompt */
	ui->cells[row * width + 1] = sp;
	/* Filter text */
	for (size_t i = 0; i < vis->picker.filter_len && (2 + (int)i) < width; i++) {
		Cell cell = { .data = " ", .len = 1, .width = 1, .style = bg };
		cell.data[0] = vis->picker.filter[i];
		ui->cells[row * width + 2 + i] = cell;
	}
	/* Cursor position hint (underscore at end of filter) */
	if (2 + (int)vis->picker.filter_len < width) {
		ui->cells[row * width + 2 + (int)vis->picker.filter_len] =
			(Cell){ .data = " ", .len = 1, .width = 1, .style = bg };
	}
	/* Fill rest of filter row with spaces */
	for (int x = 2 + (int)vis->picker.filter_len + 1; x < width; x++)
		ui->cells[row * width + x] = sp;

	/* Row 1: Horizontal separator (full width, dim) */
	row = 1;
	for (int x = 0; x < width; x++)
		ui->cells[row * width + x] = (Cell){ .data = "─", .len = 3, .width = 1, .style = line_style };
	/* Divider notch in separator */
	if (vis->picker.preview_visible)
		ui->cells[row * width + divider_x] = (Cell){ .data = "┬", .len = 3, .width = 1, .style = line_style };

	/* Content area: file list (left) + preview (right) */
	int content_start = 2;
	int content_end = picker_height;

	/* Scroll management */
	int visible_rows = content_end - content_start;
	if (vis->picker.selected < vis->picker.scroll_offset)
		vis->picker.scroll_offset = vis->picker.selected;
	if (vis->picker.selected >= vis->picker.scroll_offset + visible_rows)
		vis->picker.scroll_offset = vis->picker.selected - visible_rows + 1;
	if (vis->picker.scroll_offset < 0)
		vis->picker.scroll_offset = 0;

	/* Draw file list rows */
	for (int i = 0; i < visible_rows; i++) {
		int r = content_start + i;
		if (r >= picker_height) break;
		int sel_idx = i + vis->picker.scroll_offset;
		bool is_sel = (sel_idx < vis->picker.filtered_count && sel_idx == vis->picker.selected);

		/* Left panel: file entries */
		if (sel_idx < vis->picker.filtered_count) {
			const char *text = vis->picker.filtered[sel_idx]->label;
			CellStyle s = is_sel ? sel : bg;
			int col = 0;
			for (; text[col] && col < divider_x; col++) {
				Cell cell = { .data = " ", .len = 1, .width = 1, .style = s };
				cell.data[0] = text[col];
				ui->cells[r * width + col] = cell;
			}
			/* Fill remaining list width with selection style if selected */
			for (; col < divider_x; col++)
				ui->cells[r * width + col] = (Cell){ .data = " ", .len = 1, .width = 1, .style = s };
		} else if (sel_idx == 0 && vis->picker.filtered_count == 0) {
			const char *msg = "no matches";
			int col = 0;
			for (int ci = 0; msg[ci] && col < divider_x; ci++, col++) {
				Cell cell = { .data = " ", .len = 1, .width = 1, .style = dim };
				cell.data[0] = msg[ci];
				ui->cells[r * width + col] = cell;
			}
			for (; col < divider_x; col++)
				ui->cells[r * width + col] = sp;
		} else {
			for (int col = 0; col < divider_x; col++)
				ui->cells[r * width + col] = sp;
		}

		if (vis->picker.preview_visible) {
			/* Vertical divider */
			ui->cells[r * width + divider_x] = (Cell){ .data = "│", .len = 3, .width = 1, .style = line_style };

			/* Right panel: preview */
			int px = divider_x + 1;
			if (vis->picker_preview.line_count > 0 && i < vis->picker_preview.line_count) {
				const char *line = vis->picker_preview.lines[i];
				size_t linelen = strlen(line);
				if (linelen > 0 && line[linelen-1] == '\n') linelen--;
				for (size_t c = 0; c < linelen && px < width; c++, px++) {
					Cell ch = { .data = " ", .len = 1, .width = 1, .style = preview_bg };
					ch.data[0] = line[c];
					ui->cells[r * width + px] = ch;
				}
			}
			/* Fill rest of row */
			for (; px < width; px++)
				ui->cells[r * width + px] = sp;
		}
	}

	/* Position cursor in filter line */
	ui->cur_row = 0;
	ui->cur_col = 2 + (int)vis->picker.filter_len;
}
