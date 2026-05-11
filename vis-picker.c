#include "vis-core.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

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
	picker_bindings_init(vis);

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

	vis->picker.filtered = calloc(count, sizeof(char*));
	vis->picker.filtered_indices = calloc(count, sizeof(int));
	vis->picker.filtered_count = count;

	for (int i = 0; i < count; i++) {
		vis->picker.filtered[i] = items[i];
		vis->picker.filtered_indices[i] = i;
	}

	/* Don't call vis_mode_switch — it invokes enter/leave callbacks.
	   Just set the mode directly, like prompt_restore does. */
	vis->mode = &vis_modes[VIS_MODE_PICKER];
	vis_draw(vis);
}

/* Close the picker, optionally accepting the selection */
static void picker_close(Vis *vis, bool accept) {
	if (!vis->picker.active)
		return;

	const char *selection = NULL;
	if (accept && vis->picker.filtered_count > 0) {
		int idx = vis->picker.filtered_indices[vis->picker.selected];
		selection = vis->picker.items[idx];
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

/* File picker callback: open selected file */
static void picker_on_file_select(Vis *vis, const char *path) {
	if (!path || !vis->win) return;
	vis_window_change_file(vis->win, path);
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

		/* Check if this is a directory (use stat for portability) */
		{
			char fullpath[4096];
			struct stat st;
			snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
			if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode) && depth > 0) {
				picker_collect_files(fullpath, relpath, items, count, capacity, depth);
				continue;
			}
		}

		/* Add file to list */
		if (*count >= *capacity) {
			*capacity *= 2;
			*items = realloc(*items, *capacity * sizeof(char*));
		}
		(*items)[(*count)++] = strdup(relpath);
	}
	closedir(d);
}

/* Open file picker: recursively list files from current directory.
 * Shows both files and directories. Directories are expanded inline
 * (no separate directory entries). Set depth=2 for 2 levels of recursion. */
void picker_open_files(Vis *vis) {
	char **items = NULL;
	int count = 0;
	int capacity = 128;
	items = calloc(capacity, sizeof(char*));

	/* Collect files recursively (depth=2: current dir + 1 level of subdirs) */
	picker_collect_files(".", "", &items, &count, &capacity, 2);

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
	/* Not open - load it into the current window */
	vis_window_change_file(vis->win, path);
}

/* Open buffer picker: list open buffers */
void picker_open_buffers(Vis *vis) {
	/* Count non-internal files */
	int count = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (!f->internal) count++;
	}
	if (count == 0) return;

	char **items = calloc(count, sizeof(char*));
	int i = 0;
	for (File *f = vis->files; f; f = f->next) {
		if (f->internal) continue;
		items[i++] = strdup(f->name ? f->name : "[unnamed]");
	}
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

	int picker_height = height / 2;
	int picker_width = width * 2 / 3;
	if (picker_width < 40) picker_width = width - 4;
	int picker_x = (width - picker_width) / 2;
	int picker_y = (height - picker_height) / 2;

	CellStyle bg_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = 0 };
	CellStyle border_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_BOLD };
	CellStyle sel_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_REVERSE };
	CellStyle dim_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = CELL_ATTR_DIM };

	Cell space_cell = { .data = " ", .len = 1, .width = 1, .style = bg_style };

	/* Clear picker area */
	for (int y = picker_y; y < picker_y + picker_height && y < height; y++) {
		for (int x = picker_x; x < picker_x + picker_width && x < width; x++) {
			ui->cells[y * width + x] = space_cell;
		}
	}

	/* Top border */
	int row = picker_y;
	if (row >= 0 && row < height) {
		ui->cells[row * width + picker_x] = (Cell){ .data = "┌", .len = 3, .width = 1, .style = border_style };
		ui->cells[row * width + picker_x + picker_width - 1] = (Cell){ .data = "┐", .len = 3, .width = 1, .style = border_style };
		for (int x = picker_x + 1; x < picker_x + picker_width - 1 && x < width; x++)
			ui->cells[row * width + x] = (Cell){ .data = "─", .len = 3, .width = 1, .style = border_style };
	}

	/* Filter prompt line */
	row = picker_y + 1;
	if (row >= 0 && row < height) {
		ui->cells[row * width + picker_x] = (Cell){ .data = "│", .len = 3, .width = 1, .style = border_style };
		ui->cells[row * width + picker_x + picker_width - 1] = (Cell){ .data = "│", .len = 3, .width = 1, .style = border_style };

		int px = picker_x + 1;
		/* "> " prompt */
		const char *prompt = "> ";
		for (int i = 0; prompt[i] && px < picker_x + picker_width - 1; i++, px++) {
			Cell c = { .data = " ", .len = 1, .width = 1, .style = border_style };
			c.data[0] = prompt[i];
			ui->cells[row * width + px] = c;
		}

		/* Filter text (mutable copy to avoid compound literal issue) */
		for (size_t i = 0; i < vis->picker.filter_len && px < picker_x + picker_width - 2; i++, px++) {
			Cell c = { .data = " ", .len = 1, .width = 1, .style = border_style };
			c.data[0] = vis->picker.filter[i];
			ui->cells[row * width + px] = c;
		}
		for (; px < picker_x + picker_width - 1; px++)
			ui->cells[row * width + px] = space_cell;
	}

	/* Items */
	int visible_rows = picker_height - 3;
	if (vis->picker.selected < vis->picker.scroll_offset)
		vis->picker.scroll_offset = vis->picker.selected;
	if (vis->picker.selected >= vis->picker.scroll_offset + visible_rows)
		vis->picker.scroll_offset = vis->picker.selected - visible_rows + 1;

	for (int i = 0; i < visible_rows; i++) {
		row = picker_y + 2 + i;
		if (row < 0 || row >= height) continue;

		ui->cells[row * width + picker_x] = (Cell){ .data = "│", .len = 3, .width = 1, .style = border_style };
		ui->cells[row * width + picker_x + picker_width - 1] = (Cell){ .data = "│", .len = 3, .width = 1, .style = border_style };

		int sel_idx = i + vis->picker.scroll_offset;
		bool is_sel = (sel_idx == vis->picker.selected);
		CellStyle style = is_sel ? sel_style : bg_style;

		if (sel_idx < vis->picker.filtered_count) {
			const char *text = vis->picker.filtered[sel_idx];
			int px = picker_x + 1;
			for (int c = 0; text[c] && px < picker_x + picker_width - 1; c++, px++) {
				Cell ch = { .data = " ", .len = 1, .width = 1, .style = style };
				ch.data[0] = text[c];
				ui->cells[row * width + px] = ch;
			}
			for (; px < picker_x + picker_width - 1; px++)
				ui->cells[row * width + px] = (Cell){ .data = " ", .len = 1, .width = 1, .style = style };
		} else if (i == 0 && vis->picker.filtered_count == 0) {
			const char *msg = "No matches";
			CellStyle style = dim_style;
			int px = picker_x + 1;
			for (int c = 0; msg[c] && px < picker_x + picker_width - 1; c++, px++) {
				Cell ch = { .data = " ", .len = 1, .width = 1, .style = style };
				ch.data[0] = msg[c];
				ui->cells[row * width + px] = ch;
			}
			for (; px < picker_x + picker_width - 1; px++)
				ui->cells[row * width + px] = space_cell;
		} else {
			for (int px = picker_x + 1; px < picker_x + picker_width - 1; px++)
				ui->cells[row * width + px] = space_cell;
		}
	}

	/* Bottom border */
	row = picker_y + picker_height - 1;
	if (row >= 0 && row < height) {
		ui->cells[row * width + picker_x] = (Cell){ .data = "└", .len = 3, .width = 1, .style = border_style };
		ui->cells[row * width + picker_x + picker_width - 1] = (Cell){ .data = "┘", .len = 3, .width = 1, .style = border_style };
		for (int x = picker_x + 1; x < picker_x + picker_width - 1 && x < width; x++)
			ui->cells[row * width + x] = (Cell){ .data = "─", .len = 3, .width = 1, .style = border_style };
	}

	/* Preview pane: right side of picker */
	if (vis->picker_preview.line_count > 0) {
		int preview_x = picker_x + picker_width + 2;
		int preview_width = width - preview_x - 2;
		if (preview_width > 20) {
			/* Preview border left */
			for (int py = picker_y; py < picker_y + picker_height && py < height; py++) {
				ui->cells[py * width + preview_x - 1] = (Cell){ .data = "│", .len = 3, .width = 1, .style = border_style };
			}
			/* Preview content */
			CellStyle preview_style = { .fg = CELL_COLOR_DEFAULT, .bg = CELL_COLOR_DEFAULT, .attr = 0 };
			for (int i = 0; i < vis->picker_preview.line_count && (picker_y + 2 + i) < (picker_y + picker_height - 1); i++) {
				int py = picker_y + 2 + i;
				const char *line = vis->picker_preview.lines[i];
				int px = preview_x;
				for (int c = 0; line[c] && px < preview_x + preview_width && px < width; c++, px++) {
					Cell ch = { .data = " ", .len = 1, .width = 1, .style = preview_style };
					ch.data[0] = line[c];
					ui->cells[py * width + px] = ch;
				}
			}
		}
	}

	/* Position cursor at end of filter text */
	ui->cur_row = picker_y + 1;
	ui->cur_col = picker_x + 3 + (int)vis->picker.filter_len;
}
