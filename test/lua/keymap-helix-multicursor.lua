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

	it("counted C copies cursor to following lines", function()
		reset("abc\nabc\nabc\n")
		vis:feedkeys("2Cd")
		assert.are.same("bc\nbc\nbc\n", content())
	end)

	it("Alt-C copies cursor to previous line", function()
		reset("abc\nabc\n", 4)
		vis:feedkeys("<M-C>d")
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

	it("word delete applies to all cursors", function()
		reset("abc\ndef\n")
		vis:feedkeys("Cwd")
		assert.are.same("\n\n", content())
	end)

	it("line delete applies to all cursors", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("Cxd")
		assert.are.same("three\n", content())
	end)

	it("word yank and paste distributes register slots", function()
		reset("abc\ndef\n")
		vis:feedkeys("Cwyp")
		assert.are.same("abcabc\ndefdef\n", content())
	end)

	it("word yank and paste before distributes register slots", function()
		reset("abc\ndef\n")
		vis:feedkeys("CwyP")
		assert.are.same("abcabc\ndefdef\n", content())
	end)
end)
