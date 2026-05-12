require('keymaps/init')

local win = vis.win
local file = win.file

local function reset(data, pos)
	file:delete(0, file.size)
	file:insert(0, data)
	win.selection.pos = pos or 0
	win.selection.anchored = false
end

describe("helix profile switching", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("vim profile keeps w as cursor movement", function()
		reset("foo bar\n")
		vis:command("set keymap vim")
		vis:feedkeys("w")
		assert.falsy(win.selection.anchored)
		assert.are.same(4, win.selection.pos)
	end)

	it("helix profile makes w select", function()
		reset("foo bar\n")
		vis:command("set keymap helix")
		vis:feedkeys("w")
		assert.truthy(win.selection.anchored)
		assert.are.same(3, win.selection.pos)
	end)

	it("switching vim to helix restores helix selection semantics", function()
		reset("foo bar\n")
		vis:command("set keymap helix")
		vis:command("set keymap vim")
		vis:command("set keymap helix")
		vis:feedkeys("w")
		assert.truthy(win.selection.anchored)
		assert.are.same(3, win.selection.pos)
	end)
end)
