#include "vis-core.h"
#include <dirent.h>
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

	vis_draw(vis);

	if (on_select && selection)
		on_select(vis, selection);
}

/* Refilter items based on current filter string */
void picker_refilter(Vis *vis) {
	vis->picker.filtered_count = 0;
	for (int i = 0; i < vis->picker.item_count; i++) {
		if (vis->picker.filter_len == 0 ||
		    strstr(vis->picker.items[i], vis->picker.filter)) {
			vis->picker.filtered[vis->picker.filtered_count] = vis->picker.items[i];
			vis->picker.filtered_indices[vis->picker.filtered_count] = i;
			vis->picker.filtered_count++;
		}
	}
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
	if (!path) return;
	vis_window_new(vis, path);
}

/* Open file picker */
void picker_open_files(Vis *vis) {
	DIR *d = opendir(".");
	if (!d) return;

	char **items = NULL;
	int count = 0;
	int capacity = 128;
	items = calloc(capacity, sizeof(char*));

	struct dirent *entry;
	while ((entry = readdir(d))) {
		if (entry->d_name[0] == '.') continue;
		if (count >= capacity) {
			capacity *= 2;
			items = realloc(items, capacity * sizeof(char*));
		}
		items[count++] = strdup(entry->d_name);
	}
	closedir(d);

	picker_open(vis, items, count, picker_on_file_select);
}

static KEY_ACTION_FN(ka_picker_files) {
	if (vis->picker.active) return keys;
	picker_open_files(vis);
	return keys;
}

/* Draw the picker overlay into the UI cells buffer */
void picker_draw(Vis *vis) {
	if (!vis->picker.active) return;

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

	/* Position cursor at end of filter text */
	ui->cur_row = picker_y + 1;
	ui->cur_col = picker_x + 3 + (int)vis->picker.filter_len;
}
