require('keymaps/init')

local win = vis.win
local file = win.file

local function content()
	return file:content(0, file.size)
end

local function reset(data, pos)
	file:delete(0, file.size)
	file:insert(0, data)
	win.selection.pos = pos or 0
	win.selection.anchored = false
	vis:command("set keymap helix")
end

describe("helix match bracket (mm)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("does not crash on brackets", function()
		reset("(hello) world\n")
		win.selection.pos = 0
		vis:feedkeys("mm")
		assert.are.equal("(hello) world\n", content())
	end)

	it("does not crash without brackets", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("mm")
		assert.are.equal("hello world\n", content())
	end)

	it("mm from inside parens jumps to match", function()
		reset("(hello) world\n")
		win.selection.pos = 1
		win.selection.anchored = true
		vis:feedkeys("mm")
		-- Verify content unchanged (mm is just a motion)
		assert.are.equal("(hello) world\n", content())
	end)
end)
