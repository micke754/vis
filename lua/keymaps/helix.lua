local function smart_word_motion(motion, direction)
	return function()
		local mode = vis.mode
		local in_visual = mode == vis.modes.VISUAL or mode == vis.modes.VISUAL_LINE
		local keys = ""
		if in_visual then
			keys = keys .. "<vis-mode-normal>"
			if direction > 0 then
				keys = keys .. "<vis-motion-char-next>"
			elseif direction < 0 then
				keys = keys .. "<vis-motion-char-prev>"
			end
		end
		keys = keys .. "<vis-mode-visual-charwise>" .. motion
		vis:feedkeys(keys)
	end
end

local smart_w = smart_word_motion("<vis-motion-word-start-next>", 1)
local smart_e = smart_word_motion("<vis-motion-word-end-next>", 1)
local smart_b = smart_word_motion("<vis-motion-word-start-prev>", -1)
local smart_W = smart_word_motion("<vis-motion-bigword-start-next>", 1)
local smart_E = smart_word_motion("<vis-motion-bigword-end-next>", 1)
local smart_B = smart_word_motion("<vis-motion-bigword-start-prev>", -1)

local normal = {
	{ "<Escape>", "<vis-mode-normal-escape>", "Return to normal mode" },
	{ ":", "<vis-prompt-show>", "Open command prompt" },

	{ "h", "<vis-motion-char-prev>", "Move left" },
	{ "j", "<vis-motion-line-down>", "Move down" },
	{ "k", "<vis-motion-line-up>", "Move up" },
	{ "l", "<vis-motion-char-next>", "Move right" },
	{ "<Left>", "<vis-motion-char-prev>", "Move left" },
	{ "<Down>", "<vis-motion-line-down>", "Move down" },
	{ "<Up>", "<vis-motion-line-up>", "Move up" },
	{ "<Right>", "<vis-motion-char-next>", "Move right" },
	{ "w", smart_w, "Select next word start" },
	{ "b", smart_b, "Select previous word start" },
	{ "e", smart_e, "Select next word end" },
	{ "W", smart_W, "Select next WORD start" },
	{ "B", smart_B, "Select previous WORD start" },
	{ "E", smart_E, "Select next WORD end" },
	{ "f", "<vis-mode-visual-charwise><vis-motion-to-line-right>", "Select to next char in line" },
	{ "t", "<vis-mode-visual-charwise><vis-motion-till-line-right>", "Select till next char in line" },
	{ "F", "<vis-mode-visual-charwise><vis-motion-to-line-left>", "Select to previous char in line" },
	{ "T", "<vis-mode-visual-charwise><vis-motion-till-line-left>", "Select till previous char in line" },
	{ "gg", "<vis-motion-line-first>", "Go to first line" },
	{ "G", "<vis-motion-line-last>", "Go to last line" },
	{ "ge", "<vis-motion-line-last>", "Go to end of file" },
	{ "gs", "<vis-motion-line-start>", "Go to first non-blank" },
	{ "gl", "<vis-motion-line-end>", "Go to line end" },
	{ "<Home>", "<vis-motion-line-begin>", "Go to line start" },
	{ "<End>", "<vis-motion-line-end>", "Go to line end" },
	{ "<PageUp>", "<vis-window-page-up>", "Page up" },
	{ "<PageDown>", "<vis-window-page-down>", "Page down" },
	{ "<C-b>", "<vis-window-page-up>", "Page up" },
	{ "<C-f>", "<vis-window-page-down>", "Page down" },
	{ "<C-u>", "<vis-window-halfpage-up>", "Half page up" },
	{ "<C-d>", "<vis-window-halfpage-down>", "Half page down" },
	{ "<C-o>", "<vis-jumplist-prev>", "Jump backward" },
	{ "<C-i>", "<vis-jumplist-next>", "Jump forward" },
	{ "<C-s>", "<vis-jumplist-save>", "Save jump position" },
	{ "<C-w>s", "<vis-prompt-show>split<Enter>", "Horizontal split" },
	{ "<C-w>v", "<vis-prompt-show>vsplit<Enter>", "Vertical split" },
	{ "<C-w>h", "<vis-window-prev>", "Focus previous window" },
	{ "<C-w>j", "<vis-window-next>", "Focus next window" },
	{ "<C-w>k", "<vis-window-prev>", "Focus previous window" },
	{ "<C-w>l", "<vis-window-next>", "Focus next window" },
	{ "<C-w><Left>", "<vis-window-prev>", "Focus previous window" },
	{ "<C-w><Down>", "<vis-window-next>", "Focus next window" },
	{ "<C-w><Up>", "<vis-window-prev>", "Focus previous window" },
	{ "<C-w><Right>", "<vis-window-next>", "Focus next window" },
	{ "<C-w>w", "<vis-window-next>", "Focus next window" },
	{ "<C-w><C-w>", "<vis-window-next>", "Focus next window" },
	{ "zt", "<vis-window-redraw-top>", "Scroll cursor line to top" },
	{ "zz", "<vis-window-redraw-center>", "Scroll cursor line to center" },
	{ "zb", "<vis-window-redraw-bottom>", "Scroll cursor line to bottom" },
	{ "<Space>w", "<vis-prompt-show>w<Enter>", "Write file" },
	{ "<Space>q", "<vis-prompt-show>q<Enter>", "Quit window" },
	{ " w", "<vis-prompt-show>w<Enter>", "Write file" },
	{ " q", "<vis-prompt-show>q<Enter>", "Quit window" },

	{ "v", "<vis-mode-visual-charwise>", "Select mode" },
	{ "V", "<vis-mode-visual-linewise>", "Line select mode" },
	{ "x", "<vis-mode-visual-linewise>", "Select current line" },
	{ "X", "<vis-mode-visual-linewise>", "Select current line" },
	{ "i", "<vis-mode-insert>", "Insert mode" },
	{ "a", "<vis-append-char-next>", "Append mode" },
	{ "I", "<vis-insert-line-start>", "Insert at line start" },
	{ "A", "<vis-append-line-end>", "Insert at line end" },
	{ "o", "<vis-open-line-below>", "Open line below" },
	{ "O", "<vis-open-line-above>", "Open line above" },
	{ "r", "<vis-replace-char>", "Replace char" },
	{ "R", "<vis-mode-replace>", "Replace mode" },
	{ "u", "<vis-undo>", "Undo" },
	{ "U", "<vis-redo>", "Redo" },
	{ "y", "<vis-operator-yank>l", "Yank current character" },
	{ "p", "<vis-put-after>", "Paste after" },
	{ "P", "<vis-put-before>", "Paste before" },
	{ "d", "<vis-operator-delete>l", "Delete current character" },
	{ "c", "<vis-operator-change>l", "Change current character" },
	{ ">", "<vis-operator-shift-right>", "Indent" },
	{ "<", "<vis-operator-shift-left>", "Unindent" },
	{ "/", "<vis-search-forward>", "Search forward" },
	{ "?", "<vis-search-backward>", "Search backward" },
	{ "n", "<vis-mode-visual-charwise><vis-motion-search-repeat-forward>", "Select next search match" },
	{ "N", "<vis-mode-visual-charwise><vis-motion-search-repeat-backward>", "Select previous search match" },
	{ "*", "<vis-mode-visual-charwise><vis-motion-search-word-forward>", "Select next word match" },
	{ "\"", "<vis-register>", "Select register" },

	{ ";", "<vis-mode-normal>", "Collapse to cursor" },

	{ "0", "<vis-count>", "Count" },
	{ "1", "<vis-count>", "Count" },
	{ "2", "<vis-count>", "Count" },
	{ "3", "<vis-count>", "Count" },
	{ "4", "<vis-count>", "Count" },
	{ "5", "<vis-count>", "Count" },
	{ "6", "<vis-count>", "Count" },
	{ "7", "<vis-count>", "Count" },
	{ "8", "<vis-count>", "Count" },
	{ "9", "<vis-count>", "Count" },
}

local visual = {
	{ "<Escape>", "<vis-mode-normal>", "Return to normal mode" },
	{ "h", "<vis-mode-normal><vis-motion-char-prev>", "Move left" },
	{ "j", "<vis-mode-normal><vis-motion-line-down>", "Move down" },
	{ "k", "<vis-mode-normal><vis-motion-line-up>", "Move up" },
	{ "l", "<vis-mode-normal><vis-motion-char-next>", "Move right" },
	{ "<Left>", "<vis-mode-normal><vis-motion-char-prev>", "Move left" },
	{ "<Down>", "<vis-mode-normal><vis-motion-line-down>", "Move down" },
	{ "<Up>", "<vis-mode-normal><vis-motion-line-up>", "Move up" },
	{ "<Right>", "<vis-mode-normal><vis-motion-char-next>", "Move right" },
	{ "w", smart_w, "Select next word start" },
	{ "b", smart_b, "Select previous word start" },
	{ "e", smart_e, "Select next word end" },
	{ "W", smart_W, "Select next WORD start" },
	{ "B", smart_B, "Select previous WORD start" },
	{ "E", smart_E, "Select next WORD end" },
	{ "f", "<vis-motion-to-line-right>", "Select to next char in line" },
	{ "t", "<vis-motion-till-line-right>", "Select till next char in line" },
	{ "F", "<vis-motion-to-line-left>", "Select to previous char in line" },
	{ "T", "<vis-motion-till-line-left>", "Select till previous char in line" },
	{ "gg", "<vis-mode-normal><vis-motion-line-first>", "Go to first line" },
	{ "G", "<vis-mode-normal><vis-motion-line-last>", "Go to last line" },
	{ "ge", "<vis-mode-normal><vis-motion-line-last>", "Go to end of file" },
	{ "gs", "<vis-mode-normal><vis-motion-line-start>", "Go to first non-blank" },
	{ "gl", "<vis-mode-normal><vis-motion-line-end>", "Go to line end" },
	{ "<Home>", "<vis-mode-normal><vis-motion-line-begin>", "Go to line start" },
	{ "<End>", "<vis-mode-normal><vis-motion-line-end>", "Go to line end" },
	{ "n", "<vis-motion-search-repeat-forward>", "Select next search match" },
	{ "N", "<vis-motion-search-repeat-backward>", "Select previous search match" },
	{ "*", "<vis-motion-search-word-forward>", "Select next word match" },
	{ ";", "<vis-mode-normal>", "Collapse to cursor" },
	{ "x", "<vis-mode-normal><vis-mode-visual-linewise>", "Switch to line selection" },
	{ "X", "<vis-mode-normal><vis-mode-visual-linewise>", "Switch to line selection" },
	{ "d", "<vis-operator-delete>", "Delete selection" },
	{ "c", "<vis-operator-change>", "Change selection" },
	{ "y", "<vis-operator-yank>", "Yank selection" },
	{ "p", "<vis-put-after>", "Paste after" },
	{ "P", "<vis-put-before>", "Paste before" },
}

local visual_line = {
	{ "<Escape>", "<vis-mode-normal>", "Return to normal mode" },
	{ "j", "<vis-motion-line-down>", "Extend selection down" },
	{ "k", "<vis-motion-line-up>", "Extend selection up" },
	{ "<Down>", "<vis-motion-line-down>", "Extend selection down" },
	{ "<Up>", "<vis-motion-line-up>", "Extend selection up" },
	{ "x", "<vis-motion-line-down>", "Extend selection down" },
	{ "X", "<vis-motion-line-up>", "Extend selection up" },
	{ "d", "<vis-operator-delete>", "Delete selection" },
	{ "c", "<vis-operator-change>", "Change selection" },
	{ "y", "<vis-operator-yank>", "Yank selection" },
	{ "p", "<vis-put-after>", "Paste after" },
	{ "P", "<vis-put-before>", "Paste before" },
}

local insert = {
	{ "<Escape>", "<vis-mode-normal>", "Return to normal mode" },
	{ "<C-h>", "<vis-delete-char-prev>", "Delete previous char" },
	{ "<Backspace>", "<vis-delete-char-prev>", "Delete previous char" },
	{ "<Delete>", "<vis-delete-char-next>", "Delete next char" },
	{ "<C-d>", "<vis-delete-char-next>", "Delete next char" },
	{ "<C-w>", "<vis-delete-word-prev>", "Delete previous word" },
	{ "<C-u>", "<vis-delete-line-begin>", "Delete to line start" },
	{ "<C-k>", "<Escape><vis-operator-delete><vis-motion-line-end><vis-mode-insert>", "Delete to line end" },
	{ "<Enter>", "<vis-insert-newline>", "Insert line break" },
	{ "<C-j>", "<vis-insert-newline>", "Insert line break" },
	{ "<Tab>", "<vis-insert-tab>", "Insert tab" },
	{ "<Left>", "<vis-motion-char-prev>", "Move left" },
	{ "<Right>", "<vis-motion-char-next>", "Move right" },
	{ "<Up>", "<vis-motion-line-up>", "Move up" },
	{ "<Down>", "<vis-motion-line-down>", "Move down" },
	{ "<Home>", "<vis-motion-line-start>", "Move to line start" },
	{ "<End>", "<vis-motion-line-end>", "Move to line end" },
	{ "<PageUp>", "<vis-window-page-up>", "Page up" },
	{ "<PageDown>", "<vis-window-page-down>", "Page down" },
}

local function map_all(manager, win, mode, mappings)
	for _, mapping in ipairs(mappings) do
		local key, rhs, help = mapping[1], mapping[2], mapping[3]
		if not manager.bind(win, mode, key, rhs, help) then
			return false
		end
	end
	return true
end

return {
	apply = function(manager, win)
		if not manager.shadow_defaults(win) then
			return false
		end
		return map_all(manager, win, vis.modes.NORMAL, normal) and
		       map_all(manager, win, vis.modes.VISUAL, visual) and
		       map_all(manager, win, vis.modes.VISUAL_LINE, visual_line) and
		       map_all(manager, win, vis.modes.INSERT, insert)
	end,
}
