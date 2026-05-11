-- Helix picker: test both <Space>f (alias notation) and literal " f" (actual keypress)
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

describe("helix picker", function()
	after_each(function()
		-- Close picker BEFORE switching keymap (switching keymap changes mode)
		if vis.mode == vis.modes.PICKER then
			vis:feedkeys("<Escape>")
		end
		vis:command("set keymap vim")
	end)

	it("picker mode is registered", function()
		assert.are.equal(type(vis.modes.PICKER), "number")
	end)

	it("<Space>f (alias notation) opens picker", function()
		reset("hello world\n", 0)
		vis:feedkeys("<Space>f")
		assert.are.equal(vis.modes.PICKER, vis.mode)
	end)

	it("literal space+f opens picker", function()
		reset("hello world\n", 0)
		vis:feedkeys(" f")
		assert.are.equal(vis.modes.PICKER, vis.mode)
	end)

	it("basic editing still works after picker", function()
		reset("hello world\n", 0)
		-- Open and close picker via feedkeys
		vis:feedkeys(" f")
		if vis.mode == vis.modes.PICKER then
			vis:feedkeys("<Escape>")
		end
		vis:feedkeys("wd")
		assert.are.equal("world\n", file:content(0, file.size))
	end)
end)
