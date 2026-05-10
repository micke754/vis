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
			range = (i > 0) ? text_range_union(&range, &r) : r;
			pos = textobj->type & TEXTOBJECT_EXTEND_BACKWARD ? range.start : range.end;
		}
		if (text_range_valid(&range))
			view_selections_set_directed(sel, &range, false);
	}
	vis->action.count = VIS_COUNT_UNKNOWN;
	vis_window_invalidate(vis->win);
	vis_draw(vis);
	return true;
}

/* Helix motion/search logic */

static bool helix_find_movement(const Movement *movement) {
	return movement == &vis_motions[VIS_MOVE_TO_RIGHT] ||
	       movement == &vis_motions[VIS_MOVE_TO_LEFT] ||
	       movement == &vis_motions[VIS_MOVE_TILL_RIGHT] ||
	       movement == &vis_motions[VIS_MOVE_TILL_LEFT] ||
	       movement == &vis_motions[VIS_MOVE_TO_LINE_RIGHT] ||
	       movement == &vis_motions[VIS_MOVE_TO_LINE_LEFT] ||
	       movement == &vis_motions[VIS_MOVE_TILL_LINE_RIGHT] ||
	       movement == &vis_motions[VIS_MOVE_TILL_LINE_LEFT];
}

static bool helix_word_movement(const Movement *movement) {
	return movement == &vis_motions[VIS_MOVE_WORD_START_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	       movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV];
}

static Filerange helix_word_at(Text *txt, size_t pos, bool longword) {
	Filerange word = longword ? text_object_longword(txt, pos) : text_object_word(txt, pos);
	if (!text_range_valid(&word) && pos > 0)
		word = longword ? text_object_longword(txt, text_char_prev(txt, pos)) :
		                  text_object_word(txt, text_char_prev(txt, pos));
	return word;
}

static Filerange helix_word_next(Text *txt, size_t pos, bool longword) {
	char ch;
	size_t size = text_size(txt);
	while (pos < size && text_byte_get(txt, pos, &ch) && ch != '\n' && isspace((unsigned char)ch))
		pos = text_char_next(txt, pos);
	return longword ? text_object_longword(txt, pos) : text_object_word(txt, pos);
}

static Filerange helix_word_prev(Text *txt, size_t pos, bool longword) {
	char ch;
	while (pos > 0) {
		pos = text_char_prev(txt, pos);
		if (!text_byte_get(txt, pos, &ch) || ch == '\n' || !isspace((unsigned char)ch))
			break;
	}
	return helix_word_at(txt, pos, longword);
}

static size_t helix_word_include_space(Text *txt, size_t pos) {
	char ch;
	while (pos < text_size(txt) && text_byte_get(txt, pos, &ch) && ch != '\n' && isspace((unsigned char)ch))
		pos = text_char_next(txt, pos);
	return pos;
}

static bool helix_word_range(Text *txt, Selection *sel, const Movement *movement, int count, bool select_mode, Filerange *range, bool *backward) {
	bool longword = movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT] ||
	                movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT] ||
	                movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV];
	bool prev = movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	            movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV];
	bool end_motion = movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
	                  movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT];
	size_t cursor = view_cursors_pos(sel);
	size_t start = cursor, end = cursor;
	*backward = prev;
	if (count < 1)
		count = 1;

	if (sel->anchored) {
		Filerange current = view_selections_get(sel);
		if (text_range_valid(&current)) {
			if (select_mode)
				cursor = prev ? current.start : current.end;
			else if (end_motion)
				cursor = current.end;
		}
	}

	if (prev) {
		Filerange current = helix_word_at(txt, cursor, longword);
		/* Find the word we're going back to.
		   If cursor is inside a word or on whitespace, skip back past it
		   to the previous word. */
		Filerange word;
		if (text_range_valid(&current) && current.start < cursor && cursor < current.end)
			word = helix_word_prev(txt, current.start, longword);
		else
			word = helix_word_prev(txt, cursor, longword);
		/* For counts > 1, go back additional words */
		for (int i = 1; i < count && text_range_valid(&word) && word.start > 0; i++) {
			Filerange prevword = helix_word_prev(txt, word.start, longword);
			if (!text_range_valid(&prevword) || prevword.start == word.start)
				break;
			word = prevword;
		}
		if (!text_range_valid(&word))
			return false;
		start = word.start;
		end = word.end;
		/* Include trailing whitespace (symmetry with w forward motion) */
		if (!end_motion)
			end = helix_word_include_space(txt, end);
	} else {
		Filerange at = helix_word_at(txt, cursor, longword);
		if (!select_mode && text_range_valid(&at) && cursor == text_char_prev(txt, at.end))
			cursor = at.end;
		Filerange word = helix_word_next(txt, cursor, longword);
		if (!text_range_valid(&word))
			return false;
		start = cursor;
		for (int i = 1; i < count; i++) {
			Filerange next = helix_word_next(txt, word.end, longword);
			if (!text_range_valid(&next))
				break;
			word = next;
		}
		if (!select_mode && count > 1)
			start = word.start;
		end = word.end;
		if (!end_motion)
			end = helix_word_include_space(txt, end);
	}

	if (select_mode && sel->anchored) {
		Filerange current = view_selections_get(sel);
		if (text_range_valid(&current)) {
			if (prev)
				end = current.end;
			else
				start = current.start;
		}
	}
	*range = text_range_new(start, end);
	return text_range_valid(range) && range->start < range->end;
}

static Filerange helix_search_match_range(Vis *vis, Text *txt, size_t pos) {
	if (pos >= text_size(txt))
		return text_range_empty();
	const char *pattern = register_get(vis, &vis->registers[VIS_REG_SEARCH], NULL);
	Regex *regex = vis_regex(vis, pattern);
	if (!regex)
		return text_range_empty();

	RegexMatch match[1];
	Filerange range = text_range_empty();
	if (!text_search_range_forward(txt, pos, text_size(txt) - pos, regex, 1, match, 0) && match[0].start == pos)
		range = match[0];
	text_regex_free(regex);
	return range;
}

static bool helix_search_repeat(Vis *vis, Text *txt, View *view, Selection *sel, const Movement *movement, int count) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || vis->mode->visual ||
	    !(movement == &vis_motions[VIS_MOVE_SEARCH_REPEAT_FORWARD] ||
	      movement == &vis_motions[VIS_MOVE_SEARCH_REPEAT_BACKWARD]))
		return false;
	if (sel != view_selections_primary_get(view))
		return true;

	size_t size = 0;
	if (sel->anchored) {
		Filerange original = view_selections_get(sel);
		if (text_range_valid(&original))
			size = text_range_size(&original);
	}
	if (!size)
		register_get(vis, &vis->registers[VIS_REG_SEARCH], &size);

	size_t pos = view_cursors_pos(sel);
	if (vis->helix_select) {
		for (int i = 0; i < count; i++) {
			size_t oldpos = pos;
			if (movement->vis)
				pos = movement->vis(vis, txt, pos);
			if (pos == EPOS || pos == oldpos)
				break;
		}
		Filerange match = helix_search_match_range(vis, txt, pos);
		if (!text_range_valid(&match) && size) {
			match.start = pos;
			match.end = pos;
			while (size-- && match.end < text_size(txt))
				match.end = text_char_next(txt, match.end);
		}
		if (text_range_valid(&match) && match.start < match.end) {
			Selection *newsel = view_selections_new_force(view, match.start);
			if (newsel) {
				view_selections_set_directed(newsel, &match, false);
				view_selections_primary_set(newsel);
			}
		}
		return true;
	}

	view_selection_clear(sel);
	view_cursors_to(sel, pos);
	for (int i = 0; i < count; i++) {
		if (movement->vis)
			pos = movement->vis(vis, txt, view_cursors_pos(sel));
		if (pos == EPOS || pos == view_cursors_pos(sel))
			break;
		view_cursors_to(sel, pos);
	}

	Filerange match = helix_search_match_range(vis, txt, pos);
	if (!text_range_valid(&match) && size) {
		match.start = pos;
		match.end = pos;
		while (size-- && match.end < text_size(txt))
			match.end = text_char_next(txt, match.end);
	}
	if (text_range_valid(&match) && match.start < match.end)
		view_selections_set_directed(sel, &match, false);
	else
		view_cursors_to(sel, pos);
	return true;
}

static bool helix_find_select(Vis *vis, Text *txt, Selection *sel, const Movement *movement, size_t start, size_t pos) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || !helix_find_movement(movement))
		return false;

	bool backward = pos < start;
	size_t range_start = start, range_end = pos;
	if (movement->type & INCLUSIVE) {
		if (backward) {
			range_start = pos;
			range_end = text_char_next(txt, start);
		} else {
			range_end = text_char_next(txt, pos);
		}
	}
	Filerange range = text_range_new(range_start, range_end);
	if (vis->helix_select && sel->anchored) {
		Filerange selection = view_selections_get(sel);
		if (text_range_valid(&selection))
			range = backward ? text_range_new(selection.end, range.start) :
			                 text_range_new(selection.start, range.end);
	}
	view_selections_set_directed(sel, &range, backward);
	return true;
}

static bool helix_select_extend(Vis *vis, Text *txt, Selection *sel, const Movement *movement, size_t pos, bool backward) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || !vis->helix_select)
		return false;

	size_t anchor = text_mark_get(txt, sel->anchor);
	Filerange selection = view_selections_get(sel);
	if (text_range_valid(&selection))
		anchor = selection.start <= pos ? selection.start : selection.end;
	size_t end = pos;
	if ((movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
	     movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT]) && anchor < pos)
		end = text_char_next(txt, pos);
	Filerange range = text_range_new(anchor, end);
	view_selections_set_directed(sel, &range, backward);
	return true;
}

static bool helix_put_context(Vis *vis, Text *txt, Selection *sel, const Action *action, OperatorContext *context) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX ||
	    action->op != &vis_operators[VIS_OP_PUT_AFTER])
		return false;

	if (sel->anchored) {
		Filerange selection = view_selections_get(sel);
		if (text_range_valid(&selection)) {
			if (action->arg.i == VIS_OP_PUT_BEFORE || action->arg.i == VIS_OP_PUT_BEFORE_END)
				context->pos = selection.start;
			else
				context->pos = text_char_prev(txt, selection.end);
		}
	}
	size_t slots = vis_register_count(vis, context->reg);
	if (slots > 0 && context->reg_slot >= slots)
		context->reg_slot = slots - 1;
	context->range = text_range_empty();
	return true;
}

static bool helix_operator_context(Vis *vis, Text *txt, Selection *sel, const Action *action, OperatorContext *context) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX || vis->mode->visual || !action->op)
		return false;

	if (helix_put_context(vis, txt, sel, action, context))
		return true;
	if (sel->anchored) {
		context->range = view_selections_get(sel);
	} else {
		context->range.start = context->pos;
		context->range.end = text_char_next(txt, context->pos);
	}
	return true;
}

static Filerange helix_word_object(Text *txt, const Movement *movement, size_t pos) {
	if (movement == &vis_motions[VIS_MOVE_WORD_START_NEXT] ||
	    movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
	    movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	    movement == &vis_motions[VIS_MOVE_WORD_END_PREV])
		return text_object_word(txt, pos);
	if (movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT] ||
	    movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT] ||
	    movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV] ||
	    movement == &vis_motions[VIS_MOVE_LONGWORD_END_PREV])
		return text_object_longword(txt, pos);
	return text_range_empty();
}

static bool helix_word_start_movement(const Movement *movement) {
	return movement == &vis_motions[VIS_MOVE_WORD_START_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT] ||
	       movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	       movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV];
}

typedef struct {
	int count;
	bool initial_partial;
	bool partial_start_next;
	bool backward;
} HelixWordMotionState;

static bool helix_visual_word_adjust(Vis *vis, Text *txt, Selection *sel, const Movement *movement,
                                     size_t pos, int count, bool initial_partial, bool backward, Filerange *range) {
	if (!vis->mode->visual || vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return false;

	*range = view_selections_get(sel);
	Filerange word = helix_word_object(txt, movement, pos);
	if (text_range_valid(&word) && !initial_partial && count == 1) {
		if (helix_word_start_movement(movement)) {
			char ch;
			while (word.end < text_size(txt) && text_byte_get(txt, word.end, &ch) &&
			       ch != '\n' && isspace((unsigned char)ch))
				word.end = text_char_next(txt, word.end);
		}
		*range = word;
	}

	if (helix_select_extend(vis, txt, sel, movement, pos, backward))
		*range = view_selections_get(sel);
	return true;
}

static void helix_visual_word_prepare(Vis *vis, Text *txt, Selection *sel, const Movement *movement,
                                      HelixWordMotionState *state, size_t *pos, size_t *start) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return;

	bool word_prev = movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	                 movement == &vis_motions[VIS_MOVE_WORD_END_PREV] ||
	                 movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV] ||
	                 movement == &vis_motions[VIS_MOVE_LONGWORD_END_PREV];
	bool word_next = movement == &vis_motions[VIS_MOVE_WORD_START_NEXT] ||
	                 movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
	                 movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT] ||
	                 movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT];
	if (!word_prev && !word_next)
		return;

	state->backward = word_prev;
	size_t cursor = *pos;
	size_t anchor = text_mark_get(txt, sel->anchor);
	if (vis->helix_select && sel->anchored) {
		Filerange selection = view_selections_get(sel);
		if (text_range_valid(&selection)) {
			if (word_prev && selection.start < cursor)
				cursor = selection.start;
			if (!(selection.start == cursor && selection.end == text_char_next(txt, cursor)))
				anchor = word_prev ? selection.end : selection.start;
		}
	}
	if (anchor == cursor) {
		Filerange word = helix_word_object(txt, movement, cursor);
		if (text_range_valid(&word) && word.start < cursor && cursor < word.end) {
			size_t word_last = text_char_prev(txt, word.end);
			state->initial_partial = true;
			if (movement == &vis_motions[VIS_MOVE_WORD_START_NEXT] ||
			    movement == &vis_motions[VIS_MOVE_LONGWORD_START_NEXT]) {
				if (cursor != word_last)
					state->partial_start_next = true;
				else
					state->initial_partial = false;
			} else if (movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
			          movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT]) {
				if (cursor == word_last)
					state->initial_partial = false;
			} else if (movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
			         movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV]) {
				cursor = word.start;
				state->count = 0;
			}
		}
	} else if (word_prev && (anchor < cursor || (vis->helix_select && anchor > cursor))) {
		if (vis->helix_select && anchor > cursor && state->count > 0) {
			Filerange word = helix_word_object(txt, movement, cursor);
			if (text_range_valid(&word) && word.start == cursor) {
				size_t prev = cursor;
				char ch;
				while (prev > 0) {
					prev = text_char_prev(txt, prev);
					if (!text_byte_get(txt, prev, &ch) || ch == '\n' || !isspace((unsigned char)ch))
						break;
				}
				Filerange prevword = helix_word_object(txt, movement, prev);
				if (text_range_valid(&prevword)) {
					cursor = prevword.start;
					state->count--;
				} else {
					cursor = text_char_prev(txt, cursor);
				}
			} else {
				cursor = text_char_prev(txt, cursor);
			}
		} else {
			cursor = text_char_prev(txt, cursor);
		}
	} else if (word_next && anchor > cursor) {
		cursor = text_char_next(txt, cursor);
	}
	if (vis->helix_select && word_next && anchor != cursor) {
		Filerange word = helix_word_object(txt, movement, cursor);
		if (text_range_valid(&word)) {
			size_t word_last = text_char_prev(txt, word.end);
			if (word.start == cursor || cursor == word_last) {
				if (movement == &vis_motions[VIS_MOVE_WORD_END_NEXT] ||
				    movement == &vis_motions[VIS_MOVE_LONGWORD_END_NEXT] ||
				    cursor == word_last)
					cursor = word.end;
				else
					cursor = word_last;
			}
		}
		char ch;
		while (cursor < text_size(txt) && text_byte_get(txt, cursor, &ch) &&
		       ch != '\n' && isspace((unsigned char)ch))
			cursor = text_char_next(txt, cursor);
	}
	*pos = cursor;
	*start = cursor;
}

static size_t helix_word_after_motion(Vis *vis, Text *txt, const Movement *movement, size_t start, size_t pos, bool partial_start_next) {
	if (partial_start_next && pos > start)
		pos = text_char_prev(txt, pos);
	if (vis->selection_semantics == VIS_SELECTION_SEMANTICS_HELIX &&
	    (movement == &vis_motions[VIS_MOVE_WORD_START_PREV] ||
	     movement == &vis_motions[VIS_MOVE_LONGWORD_START_PREV])) {
		Filerange word = helix_word_object(txt, movement, pos);
		if (text_range_valid(&word) && word.start < pos && pos < word.end)
			pos = word.start;
	}
	return pos;
}

/* Helix prompt helpers */

static Regex *prompt_helix_regex(Vis *vis, const char *pattern) {
	Regex *regex = text_regex_new();
	if (!regex)
		return NULL;
	int cflags = REG_EXTENDED|REG_NEWLINE|(REG_ICASE*vis->ignorecase);
	if (text_regex_compile(regex, pattern, cflags) != 0) {
		text_regex_free(regex);
		vis_info_show(vis, "Invalid regex: `%s'", pattern);
		return NULL;
	}
	return regex;
}

static char *prompt_helix_pattern_resolve(Vis *vis, const char *pattern, bool save) {
	if (!pattern)
		return NULL;
	/* strip trailing whitespace/newline */
	size_t len = strlen(pattern);
	while (len > 0 && (pattern[len-1] == '\n' || pattern[len-1] == ' ' || pattern[len-1] == '\t'))
		len--;
	if (len == 0) {
		const char *fallback = register_get(vis, &vis->registers[VIS_REG_SEARCH], NULL);
		return fallback ? strdup(fallback) : NULL;
	}
	char *copy = malloc(len + 1);
	if (!copy)
		return NULL;
	memcpy(copy, pattern, len);
	copy[len] = '\0';
	if (save)
		register_put0(vis, &vis->registers[VIS_REG_SEARCH], copy);
	return copy;
}

static bool prompt_helix_select_regex(Vis *vis, const char *pattern) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return false;
	Win *win = vis->win;
	if (!win)
		return false;
	Text *txt = win->file->text;
	char *resolved = prompt_helix_pattern_resolve(vis, pattern, true);
	if (!resolved)
		return true;
	Regex *regex = prompt_helix_regex(vis, resolved);
	if (!regex) {
		free(resolved);
		return true;
	}
	FilerangeList ranges = {0};
	for (Selection *sel = view_selections(&win->view); sel; sel = view_selections_next(sel)) {
		Filerange selection = view_selections_get(sel);
		if (!text_range_valid(&selection) || selection.start == selection.end)
			selection = text_range_new(view_cursors_pos(sel), text_char_next(txt, view_cursors_pos(sel)));
		if (!text_range_valid(&selection))
			continue;
		size_t start = selection.start;
		while (start < selection.end) {
			RegexMatch match[1];
			if (text_search_range_forward(txt, start, selection.end - start, regex, 1, match, 0) != 0)
				break;
			if (match[0].start == EPOS || match[0].end == EPOS || match[0].start >= selection.end)
				break;
			if (match[0].end > selection.end)
				match[0].end = selection.end;
			if (!(match[0].start == selection.end && match[0].end == selection.end))
				*da_push(vis, &ranges) = match[0];
			start = match[0].end > match[0].start ? match[0].end : text_char_next(txt, match[0].start);
		}
	}
	text_regex_free(regex);
	free(resolved);
	if (ranges.count)
		view_selections_set_all(&win->view, ranges, true);
	else
		vis_info_show(vis, "nothing selected");
	free(ranges.data);
	return true;
}

static bool prompt_helix_keep_remove_regex(Vis *vis, const char *pattern, bool remove) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return false;
	Win *win = vis->win;
	if (!win)
		return false;
	Text *txt = win->file->text;
	char *resolved = prompt_helix_pattern_resolve(vis, pattern, true);
	if (!resolved)
		return true;
	Regex *regex = prompt_helix_regex(vis, resolved);
	if (!regex) {
		free(resolved);
		return true;
	}
	FilerangeList ranges = {0};
	for (Selection *sel = view_selections(&win->view); sel; sel = view_selections_next(sel)) {
		Filerange selection = view_selections_get(sel);
		if (!text_range_valid(&selection) || selection.start == selection.end)
			selection = text_range_new(view_cursors_pos(sel), text_char_next(txt, view_cursors_pos(sel)));
		bool matches = false;
		if (text_range_valid(&selection) && selection.start < selection.end) {
			RegexMatch match[1];
			matches = text_search_range_forward(txt, selection.start, selection.end - selection.start, regex, 1, match, 0) == 0 &&
			          match[0].start != EPOS && match[0].end != EPOS && match[0].start < selection.end;
		}
		if (matches != remove)
			*da_push(vis, &ranges) = selection;
	}
	text_regex_free(regex);
	free(resolved);
	if (ranges.count)
		view_selections_set_all(&win->view, ranges, true);
	else
		vis_info_show(vis, "no selections remaining");
	free(ranges.data);
	return true;
}

static bool prompt_helix_split_regex(Vis *vis, const char *pattern) {
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return false;
	Win *win = vis->win;
	if (!win)
		return false;
	Text *txt = win->file->text;
	char *resolved = prompt_helix_pattern_resolve(vis, pattern, true);
	if (!resolved)
		return true;
	Regex *regex = prompt_helix_regex(vis, resolved);
	if (!regex) {
		free(resolved);
		return true;
	}
	FilerangeList ranges = {0};
	for (Selection *sel = view_selections(&win->view); sel; sel = view_selections_next(sel)) {
		Filerange selection = view_selections_get(sel);
		if (!text_range_valid(&selection) || selection.start == selection.end) {
			*da_push(vis, &ranges) = text_range_new(view_cursors_pos(sel), view_cursors_pos(sel));
			continue;
		}
		size_t start = selection.start;
		while (start < selection.end) {
			RegexMatch match[1];
			if (text_search_range_forward(txt, start, selection.end - start, regex, 1, match, 0) != 0 ||
			    match[0].start == EPOS || match[0].end == EPOS || match[0].start >= selection.end)
				break;
			if (start < match[0].start)
				*da_push(vis, &ranges) = text_range_new(start, match[0].start);
			start = match[0].end > match[0].start ? match[0].end : text_char_next(txt, match[0].start);
		}
		if (start < selection.end)
			*da_push(vis, &ranges) = text_range_new(start, selection.end);
	}
	text_regex_free(regex);
	free(resolved);
	if (ranges.count)
		view_selections_set_all(&win->view, ranges, true);
	free(ranges.data);
	return true;
}


static KEY_ACTION_FN(ka_helix_match_bracket)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	Text *txt = vis_text(vis);
	View *view = vis_view(vis);
	Selection *sel = view_selections_primary_get(view);
	if (!sel)
		return keys;
	size_t pos = view_cursors_pos(sel);
	size_t match = text_bracket_match_symbol(txt, pos, "(){}[]<>'\"`", NULL);
	if (match == pos || match == EPOS) {
		Iterator it = text_iterator_get(txt, pos);
		char ch;
		while (text_iterator_byte_get(&it, &ch)) {
			switch (ch) {
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case '<':
			case '>':
			case '"':
			case '\'':
			case '`':
				match = text_bracket_match_symbol(txt, it.pos, "(){}[]<>'\"`", NULL);
				if (match != it.pos && match != EPOS)
					goto found;
				break;
			}
			text_iterator_byte_prev(&it, NULL);
		}
	}
found:
	if (match == pos || match == EPOS)
		return keys;
	/* Collapse to match position, clear selection */
	view_selection_clear(sel);
	view_cursors_to(sel, match);
	vis_draw(vis);
	return keys;
}

static KEY_ACTION_FN(ka_helix_goto_word)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	Text *txt = vis_text(vis);
	Win *win = vis->win;
	if (!txt || !win)
		return keys;
	Filerange vp = VIEW_VIEWPORT_GET(win->view);
	if (!text_range_valid(&vp))
		return keys;

	const char *alphabet = "abcdefghijklmnopqrstuvwxyz";
	int alen = strlen(alphabet);
	int limit = alen * alen;

	/* Collect visible word starts */
	JumpLabel *labels = NULL;
	int count = 0;
	size_t pos = vp.start;
	while (pos < vp.end && count < limit) {
		Filerange word = text_object_word(txt, pos);
		if (!text_range_valid(&word) || word.start == word.end) {
			pos = text_char_next(txt, pos);
			continue;
		}
		size_t size = text_range_size(&word);
		if (size < 2) {
			pos = word.end;
			continue;
		}
		/* Skip non-alphanumeric ranges (spaces, punctuation, etc.) */
		char c;
		if (!text_byte_get(txt, word.start, &c) || (!isalnum((unsigned char)c) && c != '_')) {
			pos = word.end;
			continue;
		}
		/* Allocate/grow labels array */
		labels = realloc(labels, (count + 1) * sizeof(JumpLabel));
		if (!labels)
			break;
		labels[count].pos = word.start;
		int outer = count / alen;
		int inner = count % alen;
		labels[count].text[0] = alphabet[outer];
		labels[count].text[1] = alphabet[inner];
		labels[count].text[2] = '\0';
		count++;
		pos = word.end;
	}
	if (count == 0) {
		free(labels);
		return keys;
	}
	view_jump_labels_set(&win->view, labels, count);
	vis_window_invalidate(win);
	free(labels);
	vis->jump_labels_active = true;
	vis->jump_label_input_count = 0;
	vis_redraw(vis);
	return keys;
}

/* Helix r: replace selection content with a repeated character */
static KEY_ACTION_FN(ka_helix_replace_char)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;
	if (!keys[0]) {
		vis_keymap_disable(vis);
		return NULL;
	}

	const char *next = vis_keys_next(vis, keys);
	if (!next)
		return NULL;

	char replacement[4+1];
	if (!vis_keys_utf8(vis, keys, replacement))
		return next;

	if (replacement[0] == 0x1b) /* <Escape> */
		return next;

	size_t replen = strlen(replacement);
	Win *win = vis->win;
	if (!win)
		return next;
	Text *txt = vis_text(vis);
	View *view = &win->view;

	/* For each selection, replace its content with the typed character
	   repeated to match the selection's grapheme count.
	   If the cursor is not anchored (bare cursor), replace the single
	   character under cursor.
	   Process in reverse order to avoid position shifts. */
	Selection *last_r = view_selections(view);
	while (last_r && last_r->next)
		last_r = last_r->next;
	for (Selection *sel = last_r; sel; sel = sel->prev) {
		size_t pos = view_cursors_pos(sel);
		if (pos == EPOS)
			continue;

		Filerange range;
		if (sel->anchored) {
			range = view_selections_get(sel);
		} else {
			range.start = pos;
			range.end = text_char_next(txt, pos);
		}

		if (!text_range_valid(&range))
			continue;

		/* Count graphemes in the selection */
		size_t grapheme_count = 0;
		Iterator it = text_iterator_get(txt, range.start);
		while (it.pos < range.end && text_iterator_char_next(&it, NULL))
			grapheme_count++;

		/* Delete selection content */
		text_delete_range(txt, &range);

		/* Insert repeated character */
		size_t insert_pos = range.start;
		for (size_t i = 0; i < grapheme_count; i++)
			text_insert(vis, txt, insert_pos + i * replen, replacement, replen);

		/* Move cursor to start of replaced area */
		view_selection_clear(sel);
		view_cursors_to(sel, insert_pos);
	}

	vis_window_invalidate(win);
	vis_file_snapshot(vis, win->file);
	vis_draw(vis);
	return next;
}

/* Helix R: replace selection content with yanked text */
static KEY_ACTION_FN(ka_helix_replace_with_yanked)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;

	Win *win = vis->win;
	if (!win)
		return keys;
	Text *txt = vis_text(vis);
	View *view = &win->view;

	Register *reg = &vis->registers[VIS_REG_DEFAULT];
	size_t reg_count = vis_register_count(vis, reg);
	if (reg_count == 0)
		return keys;

	bool multiple_cursors = view->selection_count > 1;

	/* Process in reverse order to avoid position shifts from text modifications */
	Selection *last_R = view_selections(view);
	while (last_R && last_R->next)
		last_R = last_R->next;
	for (Selection *sel = last_R; sel; sel = sel->prev) {
		size_t pos = view_cursors_pos(sel);
		if (pos == EPOS)
			continue;

		Filerange range;
		if (sel->anchored) {
			range = view_selections_get(sel);
		} else {
			/* Bare cursor: no-op for R, consistent with Helix */
			continue;
		}

		if (!text_range_valid(&range))
			continue;

		/* Determine register slot */
		size_t slot = multiple_cursors ? view_selections_number(sel) : 0;
		if (slot >= reg_count)
			slot = reg_count - 1;

		size_t len;
		const char *data = register_slot_get(vis, reg, slot, &len);
		if (!data || len == 0)
			continue;

		/* Normalize line endings */
		size_t insert_pos = range.start;

		/* Delete selection, then insert register content */
		text_delete_range(txt, &range);
		text_insert(vis, txt, insert_pos, data, len);

		/* Move cursor to end of inserted text */
		view_selection_clear(sel);
		view_cursors_to(sel, insert_pos);
	}

	vis_window_invalidate(win);
	vis_file_snapshot(vis, win->file);
	vis_draw(vis);
	return keys;
}

/* Helix i: insert before selection start */
static KEY_ACTION_FN(ka_helix_insert)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;

	Win *win = vis->win;
	if (!win)
		return keys;
	View *view = &win->view;

	for (Selection *sel = view_selections(view); sel; sel = view_selections_next(sel)) {
		if (sel->anchored) {
			Filerange range = view_selections_get(sel);
			if (text_range_valid(&range)) {
				view_selection_clear(sel);
				view_cursors_to(sel, range.start);
			}
		}
		/* bare cursor: stay at current position */
	}

	vis_operator(vis, VIS_OP_MODESWITCH, VIS_MODE_INSERT);
	vis_motion(vis, VIS_MOVE_NOP);
	return keys;
}

/* Helix a: insert after selection end */
static KEY_ACTION_FN(ka_helix_append)
{
	if (vis->selection_semantics != VIS_SELECTION_SEMANTICS_HELIX)
		return keys;

	Win *win = vis->win;
	if (!win)
		return keys;
	Text *txt = vis_text(vis);
	View *view = &win->view;

	for (Selection *sel = view_selections(view); sel; sel = view_selections_next(sel)) {
		if (sel->anchored) {
			Filerange range = view_selections_get(sel);
			if (text_range_valid(&range)) {
				view_selection_clear(sel);
				view_cursors_to(sel, range.end);
			}
		} else {
			/* bare cursor: move one char right (Vim-like append) */
			size_t pos = view_cursors_pos(sel);
			view_cursors_to(sel, text_char_next(txt, pos));
		}
	}

	vis_operator(vis, VIS_OP_MODESWITCH, VIS_MODE_INSERT);
	vis_motion(vis, VIS_MOVE_NOP);
	return keys;
}
