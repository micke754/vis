#include "vis-core.h"

static DA_COMPARE_FN(ranges_comparator)
{
	const Filerange *r1 = va, *r2 = vb;
	if (!text_range_valid(*r1))
		return text_range_valid(*r2) ? 1 : 0;
	if (!text_range_valid(*r2))
		return -1;
	return (r1->start < r2->start || (r1->start == r2->start && r1->end < r2->end)) ? -1 : 1;
}

void vis_mark_normalize(FilerangeList *ranges)
{
	for (VisDACount i = 0; i < ranges->count; i++)
		if (text_range_size(ranges->data[i]) == 0)
			da_unordered_remove(ranges, i);

	if (ranges->count) {
		da_sort(ranges, ranges_comparator);
		Filerange prev = text_range_empty();
		for (VisDACount i = 0; i < ranges->count; i++) {
			Filerange r = ranges->data[i];
			if (text_range_overlap(prev, r)) {
				prev = text_range_union(prev, r);
				da_ordered_remove(ranges, i);
			} else {
				prev = r;
				i++;
			}
		}
	}
}

static SelectionRegionList *mark_from(Vis *vis, enum VisMark id)
{
	if (vis->win) {
		if (id == VIS_MARK_SELECTION)
			return &vis->win->saved_selections;
		File *file = vis->win->file;
		if (id < LENGTH(file->marks))
			return file->marks + id;
	}
	return 0;
}

enum VisMark vis_mark_used(Vis *vis) {
	return vis->action.mark;
}

void vis_mark(Vis *vis, enum VisMark mark) {
	if (mark < LENGTH(vis->win->file->marks))
		vis->action.mark = mark;
}

static FilerangeList mark_get(Vis *vis, Win *win, SelectionRegionList *mark)
{
	FilerangeList result = {0};
	if (mark) {
		da_reserve(vis, &result, mark->count);
		for (VisDACount i = 0; i < mark->count; i++) {
			Filerange r = view_regions_restore(&win->view, mark->data[i]);
			if (text_range_valid(r))
				*da_push(vis, &result) = r;
		}
		vis_mark_normalize(&result);
	}
	return result;
}

FilerangeList vis_mark_get(Vis *vis, Win *win, enum VisMark id)
{
	return mark_get(vis, win, mark_from(vis, id));
}

static void mark_set(Vis *vis, Win *win, SelectionRegionList *mark, FilerangeList ranges)
{
	if (mark) {
		mark->count = 0;
		for (VisDACount i = 0; i < ranges.count; i++) {
			SelectionRegion ss;
			if (view_regions_save(&win->view, ranges.data[i], &ss))
				*da_push(vis, mark) = ss;
		}
	}
}

void vis_mark_set(Vis *vis, Win *win, enum VisMark id, FilerangeList ranges)
{
	mark_set(vis, win, mark_from(vis, id), ranges);
}

static void jumplist_entry_clear(JumpListEntry *entry)
{
	free(entry->path);
	memset(entry, 0, sizeof(*entry));
}

static bool jumplist_path_equal(const char *a, const char *b)
{
	if (!a || !b)
		return a == b;
	return strcmp(a, b) == 0;
}

static bool jumplist_entry_set(JumpListEntry *entry, const char *path, size_t pos, int line, int column, enum VisMode mode)
{
	char *path_copy = path ? strdup(path) : NULL;
	if (path && !path_copy)
		return false;
	jumplist_entry_clear(entry);
	entry->path = path_copy;
	entry->pos = pos;
	entry->line = line;
	entry->column = column;
	entry->mode = mode;
	return true;
}

static bool jumplist_jump(Vis *vis, const JumpListEntry *entry)
{
	Win *win = vis->win;
	if (!win || !entry)
		return false;

	if (entry->path && (!win->file || !win->file->name || strcmp(win->file->name, entry->path) != 0)) {
		/* Keep the previous file available as a hidden buffer, matching picker file switches. */
		if (win->file)
			win->file->refcount++;
		if (!vis_window_change_file(win, entry->path))
			return false;
	}
	if (!win->file)
		return false;

	size_t pos = entry->pos;
	if (pos == EPOS || pos >= text_size(win->file->text))
		pos = text_pos_by_lineno(win->file->text, entry->line);
	if (pos == EPOS)
		return false;
	if (entry->column > 0) {
		size_t colpos = text_line_char_set(win->file->text, text_line_begin(win->file->text, pos), entry->column - 1);
		if (colpos != EPOS)
			pos = colpos;
	}

	vis_mode_switch(vis, entry->mode);
	view_selections_clear_all(&win->view);
	view_cursors_to(win->view.selection, pos);
	view_selection_clear(win->view.selection);
	return true;
}

static bool jumplist_save_current(Vis *vis, bool reset_cursor)
{
	Win *win = vis->win;
	if (!win || !win->file)
		return false;

	Selection *sel = view_selections_primary_get(&win->view);
	if (!sel)
		return false;
	size_t pos = view_cursors_pos(sel);
	if (pos == EPOS)
		return false;
	int line = (int)text_lineno_by_pos(win->file->text, pos);
	int column = text_line_char_get(win->file->text, pos) + 1;
	const char *path = win->file->name;

	if (win->jumplist_count) {
		JumpListEntry *last = &win->jumplist[win->jumplist_count - 1];
		if (jumplist_path_equal(last->path, path) && last->line == line && last->column == column) {
			last->pos = pos;
			last->mode = vis->mode->id;
			if (reset_cursor)
				win->jumplist_cursor = win->jumplist_count;
			return false;
		}
	}

	if (win->jumplist_count == VIS_JUMPLIST_COUNT) {
		jumplist_entry_clear(&win->jumplist[0]);
		memmove(&win->jumplist[0], &win->jumplist[1], sizeof(win->jumplist[0]) * (VIS_JUMPLIST_COUNT - 1));
		memset(&win->jumplist[VIS_JUMPLIST_COUNT - 1], 0, sizeof(win->jumplist[VIS_JUMPLIST_COUNT - 1]));
		win->jumplist_count--;
		if (win->jumplist_cursor > 0)
			win->jumplist_cursor--;
	}

	if (!jumplist_entry_set(&win->jumplist[win->jumplist_count], path, pos, line, column, vis->mode->id))
		return false;
	win->jumplist_count++;
	if (reset_cursor)
		win->jumplist_cursor = win->jumplist_count;
	return true;
}

void vis_jumplist(Vis *vis, int advance)
{
	Win *win = vis->win;
	if (!win || !win->file)
		return;

	if (advance) {
		if (!win->jumplist_count)
			return;

		if (advance < 0) {
			if (win->jumplist_cursor == win->jumplist_count) {
				bool appended = jumplist_save_current(vis, false);
				if (appended && win->jumplist_count > 1)
					win->jumplist_cursor = win->jumplist_count - 2;
				else if (!appended && win->jumplist_count > 1)
					win->jumplist_cursor = win->jumplist_count - 2;
				else
					win->jumplist_cursor = 0;
			} else if (win->jumplist_cursor == 0) {
				win->jumplist_cursor = win->jumplist_count - 1;
			} else if (win->jumplist_cursor > win->jumplist_count) {
				win->jumplist_cursor = win->jumplist_count - 1;
			} else {
				win->jumplist_cursor--;
			}
		} else {
			if (win->jumplist_cursor >= win->jumplist_count || win->jumplist_cursor + 1 >= win->jumplist_count) {
				win->jumplist_cursor = win->jumplist_count;
				return;
			}
			win->jumplist_cursor++;
		}

		jumplist_jump(vis, &win->jumplist[win->jumplist_cursor]);
		return;
	}

	jumplist_save_current(vis, true);
}

enum VisMark vis_mark_from(Vis *vis, char mark) {
	if (mark >= 'a' && mark <= 'z')
		return VIS_MARK_a + mark - 'a';
	for (size_t i = 0; i < LENGTH(vis_marks); i++) {
		if (vis_marks[i].name == mark)
			return i;
	}
	return VIS_MARK_INVALID;
}

char vis_mark_to(Vis *vis, enum VisMark mark) {
	if (VIS_MARK_a <= mark && mark <= VIS_MARK_z)
		return 'a' + mark - VIS_MARK_a;

	if (mark < LENGTH(vis_marks))
		return vis_marks[mark].name;

	return '\0';
}

const MarkDef vis_marks[] = {
	[VIS_MARK_DEFAULT]        = { '\'', VIS_HELP("Default mark")    },
	[VIS_MARK_SELECTION]      = { '^',  VIS_HELP("Last selections") },
};
