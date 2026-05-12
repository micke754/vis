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

describe("helix line selection", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("x deletes current line", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("xd")
		assert.are.same("two\nthree\n", content())
	end)

	it("xx deletes current and next line", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("xxd")
		assert.are.same("three\n", content())
	end)

	it("2x deletes two lines", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("2xd")
		assert.are.same("three\n", content())
	end)

	it("X selects only current line", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("xXd")
		assert.are.same("two\nthree\n", content())
	end)
end)
