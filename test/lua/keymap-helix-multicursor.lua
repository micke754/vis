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

	it("parentheses rotate primary selection", function()
		reset("abc\ndef\n")
		vis:feedkeys("C(,d")
		assert.are.same("bc\ndef\n", content())
	end)

	it("Alt parentheses rotate selection contents", function()
		reset("abc\ndef\n")
		vis:feedkeys("Cw<M-)>,")
		assert.are.same("def\nabc\n", content())
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

	it("star uses primary selection only", function()
		reset("abc\ndef\n")
		vis:feedkeys("Cw*")
		assert.are.same("def\0", vis.registers['/'][1])
	end)

	it("Alt-s splits selection on newlines", function()
		reset("one\ntwo\nthree\n")
		vis:feedkeys("3x<M-s>,d")
		assert.are.same("two\nthree\n", content())
	end)

	it("percent selects entire file", function()
		reset("one\ntwo\n")
		vis:feedkeys("%d")
		assert.are.same("", content())
	end)

	it("Y joins and yanks selections", function()
		reset("abc\ndef\n")
		vis:feedkeys("CwYp")
		assert.are.same("abcabc\ndef\ndefabc\ndef\n", content())
	end)

	it("s selects regex matches inside selection", function()
		reset("one two one\n")
		vis:feedkeys("xsone<Enter>d")
		assert.are.same(" two \n", content())
	end)

	it("S splits selection by regex matches", function()
		reset("one,two,three\n")
		vis:feedkeys("xS,<Enter>,d")
		assert.are.same(",two,three\n", content())
	end)

	it("K keeps selections matching regex", function()
		reset("foo\nbar\n")
		vis:feedkeys("CwKfoo<Enter>d")
		assert.are.same("\nbar\n", content())
	end)

	it("Alt-K removes selections matching regex", function()
		reset("foo\nbar\n")
		vis:feedkeys("Cw<M-K>foo<Enter>d")
		assert.are.same("foo\n\n", content())
	end)
end)
