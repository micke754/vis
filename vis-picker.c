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

static void picker_preview_load(Vis *vis, const char *path);

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
		{ "<C-n>",           "vis-picker-down" },
		{ "<Up>",            "vis-picker-up" },
		{ "<C-p>",           "vis-picker-up" },
		{ "<Enter>",         "vis-picker-accept" },
		{ "<C-j>",           "vis-picker-accept" },
		{ "<Escape>",        "vis-picker-cancel" },
		{ "<C-c>",           "vis-picker-cancel" },
		{ "<C-g>",           "vis-picker-cancel" },
		{ "<Backspace>",     "vis-picker-backspace" },
		{ "<C-h>",           "vis-picker-backspace" },
		{ "<C-w>",           "vis-picker-delete-word" },
		{ "<C-u>",           "vis-picker-clear-filter" },
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
void picker_open(Vis *vis, char **items, int count, void (*on_select)(Vis*, const char*)) {
	if (!picker_bindings_init(vis))
		goto err;

	vis->picker.active = true;
	vis->picker.filter[0] = '\0';
	vis->picker.filter_len = 0;
	vis->picker.selected = 0;
	vis->picker.scroll_offset = 0;
	vis->picker.items = items;
	vis->picker.item_count = count;
	vis->picker.on_select = on_select;
	vis->picker.saved_win = vis->win;
	vis->picker.saved_mode = vis->mode;

	vis->picker.filtered = count > 0 ? calloc(count, sizeof(char*)) : NULL;
	vis->picker.filtered_indices = count > 0 ? calloc(count, sizeof(int)) : NULL;
	if (count > 0 && (!vis->picker.filtered || !vis->picker.filtered_indices))
		goto err;
	vis->picker.filtered_count = count;

	for (int i = 0; i < count; i++) {
		vis->picker.filtered[i] = items[i];
		vis->picker.filtered_indices[i] = i;
	}

	/* Don't call vis_mode_switch — it invokes enter/leave callbacks.
	   Just set the mode directly, like prompt_restore does. */
	vis->mode = &vis_modes[VIS_MODE_PICKER];
	vis_draw(vis);
	return;

err:
	for (int i = 0; i < count; i++)
		free(items[i]);
	free(items);
	free(vis->picker.filtered);
	free(vis->picker.filtered_indices);
	memset(&vis->picker, 0, sizeof(vis->picker));
}

/* Close the picker, optionally accepting the selection */
static void picker_close(Vis *vis, bool accept) {
	if (!vis->picker.active)
		return;

	char *selection = NULL;
	if (accept && vis->picker.filtered_count > 0) {
		int idx = vis->picker.filtered_indices[vis->picker.selected];
		selection = strdup(vis->picker.items[idx]);
	}

	void (*on_select)(Vis*, const char*) = vis->picker.on_select;

	for (int i = 0; i < vis->picker.item_count; i++)
		free(vis->picker.items[i]);
	free(vis->picker.items);
	free(vis->picker.filtered);
	free(vis->picker.filtered_indices);

	/* Clean up preview */
	picker_preview_load(vis, NULL);

	vis->picker.active = false;
	vis->picker.items = NULL;
	vis->picker.filtered = NULL;
	vis->picker.filtered_indices = NULL;

	if (vis->picker.saved_win)
		vis->win = vis->picker.saved_win;
	if (vis->picker.saved_mode)
		vis->mode = vis->picker.saved_mode;
	else
		vis->mode = &vis_modes[VIS_MODE_NORMAL];

	if (on_select && selection)
		on_select(vis, selection);

	free(selection);

	vis_draw(vis);
}

/* Called when mode is switched away from PICKER externally
 * (e.g. :set keymap, or any other mode switch bypassing picker_close).
 * Cleans up stale picker state to prevent state leaks between tests. */
void vis_picker_leave(Vis *vis, Mode *old_mode) {
	(void)old_mode;
	if (!vis->picker.active)
		return;
	picker_close(vis, false);
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
			vis->picker.filtered[i] = vis->picker.items[i];
			vis->picker.filtered_indices[i] = i;
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
		double s = picker_fuzzy_score(vis->picker.items[i], vis->picker.filter);
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
		vis->picker.filtered[i] = vis->picker.items[idx];
		vis->picker.filtered_indices[i] = idx;
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

static KEY_ACTION_FN(ka_picker_accept) {
	picker_close(vis, true);
	return keys;
}

static KEY_ACTION_FN(ka_picker_cancel) {
	picker_close(vis, false);
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

/* File picker callback: open selected file.
 * Increments old file refcount before replacement so it stays
 * in vis->files for the buffer picker (<Space>b) to find. */
static void picker_on_file_select(Vis *vis, const char *path) {
	if (!path || !vis->win) return;
	/* Keep old file alive (hidden buffer) so buffer picker can find it */
	if (vis->win->file)
		vis->win->file->refcount++;
	vis_window_change_file(vis->win, path);
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

/* Recursive helper: collect files from directory tree.
 * depth=0 means list files/dirs in given dir. depth>0 recurses into subdirs.
 * prefix is the relative path to prepend (used for recursion). */
static void picker_collect_files(const char *dirpath, const char *prefix,
                                  char ***items, int *count, int *capacity,
                                  int depth) {
	DIR *d = opendir(dirpath);
	if (!d) return;

	struct dirent *entry;
	while ((entry = readdir(d))) {
		if (entry->d_name[0] == '.') continue;
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

		/* Skip binary files (executables, object files, etc.) */
		{
			char fullpath[4096];
			snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
			if (is_binary_file(fullpath))
				continue;
		}

		/* Add file to list */
		if (*count >= *capacity) {
			*capacity *= 2;
			char **new_items = realloc(*items, *capacity * sizeof(char*));
			if (!new_items) {
				closedir(d);
				return;
			}
			*items = new_items;
		}
		char *item = strdup(relpath);
		if (!item)
			continue;
		(*items)[(*count)++] = item;
	}
	closedir(d);
}

/* qsort comparison for file listing:
 * 1. Files in root directory (no '/') come first
 * 2. Within same depth, sort case-insensitively alphabetically */
static int picker_cmp_file(const void *a, const void *b) {
	const char *pa = *(const char **)a;
	const char *pb = *(const char **)b;

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
	const char *pa = *(const char **)a;
	const char *pb = *(const char **)b;
	const char *ba = strrchr(pa, '/'); ba = ba ? ba + 1 : pa;
	const char *bb = strrchr(pb, '/'); bb = bb ? bb + 1 : pb;
	return strcasecmp(ba, bb);
}

/* Open file picker: recursively list files from current directory.
 * Shows both files and directories. Directories are expanded inline
 * (no separate directory entries). Set depth=2 for 2 levels of recursion. */
void picker_open_files(Vis *vis) {
	char **items = NULL;
	int count = 0;
	int capacity = 128;
	items = calloc(capacity, sizeof(char*));
	if (!items)
		return;

	/* Collect files recursively (depth=2: current dir + 1 level of subdirs) */
	picker_collect_files(".", "", &items, &count, &capacity, 2);

	/* Sort: current-dir files first, then subdir files, all alphabetical */
	qsort(items, count, sizeof(char*), picker_cmp_file);

	picker_open(vis, items, count, picker_on_file_select);
}

/* Buffer picker callback: switch to selected buffer */
static void picker_on_buffer_select(Vis *vis, const char *path) {
	if (!path || !vis->win) return;
	/* Find existing window for this file */
	for (Win *win = vis->windows; win; win = win->next) {
		if (win->file && win->file->name && strcmp(win->file->name, path) == 0) {
			vis->win = win;
			return;
		}
	}
	/* Not open in any window - load into current window.
	 * Keep old file alive so it stays in the buffer list. */
	if (vis->win->file)
		vis->win->file->refcount++;
	vis_window_change_file(vis->win, path);
}

/* Open buffer picker: list open buffers */
void picker_open_buffers(Vis *vis) {
	/* Count non-internal files */
	int count = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (!f->internal && f->name) count++;
	}
	if (count == 0) return;

	char **items = calloc(count, sizeof(char*));
	if (!items)
		return;
	int i = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (f->internal || !f->name) continue;
		items[i] = strdup(f->name);
		if (items[i])
			i++;
	}
	count = i;
	if (count == 0) {
		free(items);
		return;
	}
	/* Sort alphabetically by filename */
	qsort(items, count, sizeof(char*), picker_cmp_buffer);
	picker_open(vis, items, count, picker_on_buffer_select);
}

static KEY_ACTION_FN(ka_picker_files) {
	if (vis->picker.active) return keys;
	picker_open_files(vis);
	return keys;
}

static KEY_ACTION_FN(ka_picker_buffers) {
	if (vis->picker.active) return keys;
	picker_open_buffers(vis);
	return keys;
}

/* Load preview for a file: first N lines */
static void picker_preview_load(Vis *vis, const char *path) {
	/* Free previous preview */
	for (int i = 0; i < vis->picker_preview.line_count; i++)
		free(vis->picker_preview.lines[i]);
	free(vis->picker_preview.lines);
	free(vis->picker_preview.path);
	vis->picker_preview.lines = NULL;
	vis->picker_preview.line_count = 0;
	vis->picker_preview.path = NULL;

	if (!path) return;

	FILE *f = fopen(path, "r");
	if (!f) return;

	int capacity = 32;
	vis->picker_preview.lines = calloc(capacity, sizeof(char*));
	char buf[1024];
	while (vis->picker_preview.line_count < capacity && fgets(buf, sizeof(buf), f)) {
		size_t len = strlen(buf);
		if (len > 0 && buf[len-1] == '\n') buf[--len] = '\0';
		vis->picker_preview.lines[vis->picker_preview.line_count++] = strdup(buf);
	}
	fclose(f);
	vis->picker_preview.path = strdup(path);
}

/* Check if preview needs updating for current selection */
static void picker_preview_update(Vis *vis) {
	if (!vis->picker.active || vis->picker.filtered_count == 0) {
		picker_preview_load(vis, NULL);
		return;
	}
	int idx = vis->picker.filtered_indices[vis->picker.selected];
	const char *path = vis->picker.items[idx];

	/* Only reload if path changed */
	if (vis->picker_preview.path && strcmp(vis->picker_preview.path, path) == 0)
		return;

	picker_preview_load(vis, path);
}

/* Draw the picker overlay into the UI cells buffer */
void picker_draw(Vis *vis) {
	if (!vis->picker.active) return;
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
			const char *text = vis->picker.filtered[sel_idx];
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

	/* Position cursor in filter line */
	ui->cur_row = 0;
	ui->cur_col = 2 + (int)vis->picker.filter_len;
}
