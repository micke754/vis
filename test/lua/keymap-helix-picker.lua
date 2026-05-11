require('keymaps/init')

local win = vis.win
local file = win.file

local function reset(data, pos)
	file:delete(0, file.size)
	file:insert(0, data)
	win.selection.pos = pos or 0
	win.selection.anchored = false
	vis:command("set keymap helix")
end

describe("helix picker infrastructure", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("picker mode is registered", function()
		assert.are.equal(type(vis.modes.PICKER), "number")
	end)

	it("basic editing still works after picker mode", function()
		reset("hello world\n", 0)
		vis:feedkeys("wd")
		assert.are.equal("world\n", file:content(0, file.size))
	end)
end)
