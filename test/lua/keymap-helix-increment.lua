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

describe("helix increment/decrement", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("Ctrl-a increments number under cursor", function()
		reset("x = 42\n", 5)
		vis:feedkeys("<C-a>")
		assert.are.equal("x = 43\n", content())
	end)

	it("Ctrl-a works on number at start of line", function()
		reset("10 + 20\n", 0)
		vis:feedkeys("<C-a>")
		assert.are.equal("11 + 20\n", content())
	end)

	it("Ctrl-x decrements number under cursor", function()
		reset("x = 42\n", 5)
		vis:feedkeys("<C-x>")
		assert.are.equal("x = 41\n", content())
	end)

	it("Ctrl-a decrements to negative", function()
		reset("x = 0\n", 4)
		vis:feedkeys("<C-x>")
		assert.are.equal("x = -1\n", content())
	end)

	it("2<C-a> increments by 2", function()
		reset("x = 10\n", 4)
		vis:feedkeys("2<C-a>")
		assert.are.equal("x = 12\n", content())
	end)

	it("Ctrl-a with no number does nothing", function()
		reset("hello\n", 0)
		vis:feedkeys("<C-a>")
		assert.are.equal("hello\n", content())
	end)
end)
