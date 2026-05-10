#include "vis-core.h"

/* Helix keymap helpers */

static Filerange helix_surround_inner(Text *txt, size_t pos, char surround)
{
	switch (surround) {
	case '(':
	case ')':
		return text_object_parenthesis(txt, pos);
	case '[':
	case ']':
		return text_object_square_bracket(txt, pos);
	case '{':
	case '}':
		return text_object_curly_bracket(txt, pos);
	case '<':
	case '>':
		return text_object_angle_bracket(txt, pos);
	case '"':
		return text_object_quote(txt, pos);
	case '\'':
		return text_object_single_quote(txt, pos);
	case '`':
		return text_object_backtick(txt, pos);
	}
	return text_range_empty();
}

static KEY_ACTION_FN(ka_helix_surround_add)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || !arg->s || strlen(arg->s) < 2)
		return keys;
	Text *txt = vis_text(vis);
	View *view = vis_view(vis);
	char open[1] = { arg->s[0] }, close[1] = { arg->s[1] };
	Selection *sel = view_selections(view);
	for (Selection *s = sel; s; s = view_selections_next(s))
		sel = s;
	for (; sel; sel = view_selections_prev(sel)) {
		Filerange range = view_selections_get(sel);
		if (!text_range_valid(&range) || range.start == range.end)
			range = text_range_new(view_cursors_pos(sel), text_char_next(txt, view_cursors_pos(sel)));
		if (!text_range_valid(&range))
			continue;
		text_insert(vis, txt, range.end, close, sizeof close);
		text_insert(vis, txt, range.start, open, sizeof open);
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_helix_surround_delete)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || !arg->s || !arg->s[0])
		return keys;
	Text *txt = vis_text(vis);
	View *view = vis_view(vis);
	Selection *sel = view_selections(view);
	for (Selection *s = sel; s; s = view_selections_next(s))
		sel = s;
	for (; sel; sel = view_selections_prev(sel)) {
		Filerange selection = view_selections_get(sel);
		size_t pos = text_range_valid(&selection) ? selection.start : view_cursors_pos(sel);
		Filerange inner = helix_surround_inner(txt, pos, arg->s[0]);
		if (!text_range_valid(&inner) || inner.start == 0 || inner.end >= text_size(txt))
			continue;
		Filerange close = text_range_new(inner.end, text_char_next(txt, inner.end));
		Filerange open = text_range_new(text_char_prev(txt, inner.start), inner.start);
		text_delete_range(txt, &close);
		text_delete_range(txt, &open);
	}
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_helix_yank_joined)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	Text *txt = vis_text(vis);
	View *view = vis_view(vis);
	Buffer joined = {0};
	for (Selection *sel = view_selections(view); sel; sel = view_selections_next(sel)) {
		Filerange range = view_selections_get(sel);
		if (!text_range_valid(&range) || range.start == range.end)
			range = text_range_new(view_cursors_pos(sel), text_char_next(txt, view_cursors_pos(sel)));
		if (!text_range_valid(&range))
			continue;
		if (buffer_length0(&joined))
			buffer_append(&joined, "\n", 1);
		size_t len = text_range_size(&range);
		if (len != SIZE_MAX && buffer_grow(&joined, len))
			joined.len += text_bytes_get(txt, range.start, len, joined.data + joined.len);
	}
	if (buffer_length0(&joined)) {
		register_put(vis, &vis->registers[VIS_REG_DEFAULT], joined.data, joined.len);
		register_put(vis, &vis->registers[VIS_REG_ZERO], joined.data, joined.len);
		vis_info_show(vis, "Joined and yanked selections");
	}
	buffer_release(&joined);
	vis->helix_select = false;
	return keys;
}

static bool helix_search_escape_append(Buffer *buf, const char *text) {
	static const char special[] = ".[]\\()*+?{}|^$";
	for (const char *in = text; *in; in++) {
		if (strchr(special, *in) && !buffer_append(buf, "\\", 1))
			return false;
		if (!buffer_append(buf, in, 1))
			return false;
	}
	return true;
}

static KEY_ACTION_FN(ka_helix_search_word)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	Text *txt = vis_text(vis);
	View *view = vis_view(vis);
	Selection *sel = view_selections_primary_get(view);
	if (!sel)
		return keys;
	Filerange range = sel->anchored ? view_selections_get(sel) : text_object_word(txt, view_cursors_pos(sel));
	if (!text_range_valid(&range))
		return keys;
	char *fragment = text_bytes_alloc0(txt, range.start, text_range_size(&range));
	if (!fragment)
		return keys;
	Buffer pattern = {0};
	if (!helix_search_escape_append(&pattern, fragment)) {
		free(fragment);
		buffer_release(&pattern);
		return keys;
	}
	buffer_terminate(&pattern);
	if (!sel->anchored)
		view_selections_set_directed(sel, &range, false);
	register_put0(vis, &vis->registers[VIS_REG_SEARCH], buffer_content0(&pattern));
	vis->search_direction = arg->i > 0 ? VIS_MOVE_SEARCH_REPEAT_FORWARD : VIS_MOVE_SEARCH_REPEAT_BACKWARD;
	vis_info_show(vis, "Search pattern set: %s", fragment);
	free(fragment);
	buffer_release(&pattern);
	return keys;
}

static KEY_ACTION_FN(ka_helix_rotate_selection)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	View *view = vis_view(vis);
	if (view->selection_count < 2)
		return keys;
	Selection *sel = view_selections_primary_get(view);
	Selection *next = arg->i < 0 ? view_selections_prev(sel) : view_selections_next(sel);
	if (!next) {
		next = view_selections(view);
		if (arg->i < 0)
			for (Selection *s = next; s; s = view_selections_next(s))
				next = s;
	}
	view_selections_primary_set(next);
	return keys;
}

static KEY_ACTION_FN(ka_helix_select_all)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	View *view = vis_view(vis);
	FilerangeList ranges = {0};
	*da_push(vis, &ranges) = text_range_new(0, text_size(vis_text(vis)));
	view_selections_set_all(view, ranges, true);
	free(ranges.data);
	return keys;
}

static KEY_ACTION_FN(ka_helix_select_split_lines)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	View *view = vis_view(vis);
	Text *txt = vis_text(vis);
	FilerangeList ranges = {0};
	for (Selection *sel = view_selections(view); sel; sel = view_selections_next(sel)) {
		Filerange range = view_selections_get(sel);
		if (!text_range_valid(&range)) {
			*da_push(vis, &ranges) = text_range_new(view_cursors_pos(sel), view_cursors_pos(sel));
			continue;
		}
		if (range.start == range.end) {
			*da_push(vis, &ranges) = range;
			continue;
		}
		size_t start = range.start;
		while (start < range.end) {
			size_t line_end = text_line_end(txt, start);
			if (line_end > range.end)
				line_end = range.end;
			*da_push(vis, &ranges) = text_range_new(start, line_end);
			start = text_line_next(txt, start);
			if (start <= line_end)
				break;
		}
	}
	if (ranges.count)
		view_selections_set_all(view, ranges, true);
	free(ranges.data);
	return keys;
}

static KEY_ACTION_FN(ka_helix_collapse)
{
	if (vis->selection_semantics == VIS_SELECTION_SEMANTICS_HELIX) {
		vis->helix_select = false;
		for (Selection *s = view_selections(vis_view(vis)); s; s = view_selections_next(s)) {
			size_t pos = view_cursors_pos(s);
			view_selection_clear(s);
			view_cursors_to(s, pos);
		}
	}
	return keys;
}

static KEY_ACTION_FN(ka_helix_line_select)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	Text *txt = vis_text(vis);
	int count = VIS_COUNT_DEFAULT(vis->action.count, 1);
	for (Selection *s = view_selections(vis_view(vis)); s; s = view_selections_next(s)) {
		size_t pos = view_cursors_pos(s);
		Filerange range;
		if (arg->b && s->anchored) {
			range = view_selections_get(s);
			range.start = text_line_begin(txt, range.start);
			range.end = text_line_begin(txt, range.end);
			if (range.end <= range.start)
				range.end = text_line_next(txt, range.start);
		} else {
			range.start = text_line_begin(txt, pos);
			range.end = range.start;
		}
		for (int i = 0; i < count; i++)
			range.end = text_line_next(txt, range.end);
		view_selections_set(s, &range);
		s->anchored = true;
	}
	vis->action.count = VIS_COUNT_UNKNOWN;
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_helix_select_toggle)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	vis->helix_select = !vis->helix_select;
	if (vis->helix_select) {
		Text *txt = vis_text(vis);
		for (Selection *s = view_selections(vis_view(vis)); s; s = view_selections_next(s)) {
			size_t pos = view_cursors_pos(s);
			char ch;
			if (pos > text_line_begin(txt, pos) &&
			    (!text_byte_get(txt, pos, &ch) || ch == '\n'))
				view_cursors_to(s, text_char_prev(txt, pos));
			s->anchored = true;
		}
	}
	vis_draw(vis);
	return keys;
}


static KEY_ACTION_FN(ka_helix_regex_prompt)
{
	vis->helix_prompt = arg->i;
	vis_prompt_show(vis, "");
	return keys;
}

static bool helix_text_object(Vis *vis, const Arg *arg)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return false;
	const TextObject *textobj = NULL;
	if (arg->i < LENGTH(vis_textobjects))
		textobj = vis_textobjects + arg->i;
	else if ((VisDACount)arg->i - LENGTH(vis_textobjects) < vis->textobjects.count)
		textobj = vis->textobjects.data + arg->i - LENGTH(vis_textobjects);
	if (!textobj)
		return false;
	Text *txt = vis_text(vis);
	Win *win = vis->win;
	int count = VIS_COUNT_DEFAULT(vis->action.count, 1);
	for (Selection *sel = view_selections(vis_view(vis)); sel; sel = view_selections_next(sel)) {
		size_t pos = view_cursors_pos(sel);
		Filerange range = sel->anchored ? view_selections_get(sel) : text_range_new(pos, pos);
		for (int i = 0; i < count; i++) {
			Filerange r = text_range_empty();
			if (textobj->txt)
				r = textobj->txt(txt, pos);
			else if (textobj->vis)
				r = textobj->vis(vis, txt, pos);
			else if (textobj->user)
				r = textobj->user(vis, win, textobj->data, pos);
			if (!text_range_valid(&r))
				break;
			if ((textobj->type & TEXTOBJECT_DELIMITED_OUTER) && r.start > 0 && r.end < text_size(txt)) {
				r.start--;
				r.end++;
			}
			range = (sel->anchored || i > 0) ? text_range_union(&range, &r) : r;
			pos = textobj->type & TEXTOBJECT_EXTEND_BACKWARD ? range.start : range.end;
		}
		if (text_range_valid(&range))
			view_selections_set_directed(sel, &range, false);
	}
	vis->action.count = VIS_COUNT_UNKNOWN;
	vis_draw(vis);
	return true;
}
