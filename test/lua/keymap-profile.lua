require('keymaps/init')

local function current()
	return vis.win, vis.win.file
end

local function reset(data, pos)
	local win, file = current()
	file:delete(0, file.size)
	file:insert(0, data)
	win.selection.pos = pos or 0
	win.selection.anchored = false
	vis:command("set keymap vim")
end

describe("keymap profile", function()
	before_each(function()
		reset("abc def\n", 0)
	end)

	after_each(function()
		vis:command("set keymap vim")
	end)

	it("supports :set keymap helix in normal mode", function()
		vis:command("set keymap helix")
		vis:feedkeys("w")
		local win = vis.win
		assert.truthy(win.selection.anchored)
		assert.are.same(3, win.selection.pos)
	end)

	it("restores vim defaults when switching back", function()
		vis:command("set keymap helix")
		vis:command("set keymap vim")
		vis:feedkeys("w")
		local win = vis.win
		assert.falsy(win.selection.anchored)
		assert.are.same(4, win.selection.pos)
	end)

	it("applies profile mappings to newly split windows", function()
		vis:command("set keymap helix")
		vis:feedkeys("<C-w>v")
		vis:feedkeys("w")
		local win = vis.win
		assert.truthy(win.selection.anchored)
		assert.are.same(3, win.selection.pos)
	end)
end)
