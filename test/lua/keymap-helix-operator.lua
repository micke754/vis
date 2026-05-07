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

describe("helix keymap operators", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("d deletes anchored word selection", function()
		reset("This string\n")
		vis:feedkeys("wd")
		assert.are.same("string\n", content())
	end)

	it("c changes anchored word selection", function()
		reset("This string\n")
		vis:feedkeys("wcX<Escape>")
		assert.are.same("Xstring\n", content())
	end)

	it("y yanks anchored word selection", function()
		reset("This string\n")
		vis:feedkeys("wy")
		local values = vis.registers['\"']
		assert.are.same("This ", values[1]:sub(1, 5))
	end)
end)
