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

describe("helix multicursor", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("C copies cursor to next line", function()
		reset("abc\nabc\n")
		vis:feedkeys("Cd")
		assert.are.same("bc\nbc\n", content())
	end)

	it("comma keeps only primary selection", function()
		reset("abc\nabc\n")
		vis:feedkeys("C,d")
		assert.are.same("abc\nbc\n", content())
	end)

	it("alt comma removes primary selection", function()
		reset("abc\nabc\n")
		vis:feedkeys("C<M-,>d")
		assert.are.same("bc\nabc\n", content())
	end)
end)
