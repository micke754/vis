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

describe("helix search", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("star sets search pattern without moving", function()
		reset("foo bar foo\n")
		vis:feedkeys("*")
		assert.are.same(2, win.selection.pos)
	end)

	it("n after star selects next match using selected text width", function()
		reset("foo bar foo\n")
		vis:feedkeys("*nd")
		assert.are.same("foo bar \n", content())
	end)

	it("n after star and collapse selects search pattern width", function()
		reset("foo bar foo\n")
		vis:feedkeys("*;nd")
		assert.are.same("foo bar \n", content())
	end)

	it("N after star and collapse selects previous match", function()
		reset("foo bar foo\n", 8)
		vis:feedkeys("*;Nd")
		assert.are.same(" bar foo\n", content())
	end)

	it("search pattern width includes whitespace", function()
		reset("foo bar foo bar\n")
		vis:feedkeys("w*;nd")
		assert.are.same("foo bar bar\n", content())
	end)

	it("select mode star registers selection without jumping", function()
		reset("foo bar foo baz\n")
		vis:feedkeys("vw*")
		assert.are.same(3, win.selection.pos)
	end)

	it("select mode n adds next match selection", function()
		reset("foo bar foo baz\n")
		vis:feedkeys("vw*n,d")
		assert.are.same("foo bar baz\n", content())
	end)

	it("select mode N adds previous match selection", function()
		reset("foo bar foo baz\n", 8)
		vis:feedkeys("vw*N,d")
		assert.are.same("bar foo baz\n", content())
	end)

	it("empty s prompt uses last star pattern", function()
		reset("foo bar foo baz\n")
		vis:feedkeys("*s<Enter>d")
		assert.are.same(" bar foo baz\n", content())
	end)

	it("empty slash prompt searches next star pattern", function()
		reset("foo bar foo baz\n")
		vis:feedkeys("*/<Enter>")
		assert.are.same(10, win.selection.pos)
	end)

	it("empty question prompt searches prev star pattern", function()
		reset("foo bar foo baz\n", 8)
		vis:feedkeys("*?<Enter>")
		assert.are.same(2, win.selection.pos)
	end)

	it("star searches literal punctuation", function()
		reset("a.c a-c a.c\n")
		vis:feedkeys("vt *nd")
		assert.are.same(" a-c \n", content())
	end)

	it("slash repeat selects actual regex match width", function()
		reset("foo fxo foo\n")
		vis:feedkeys("/f.o<Enter>nd")
		assert.are.same("foo fxo \n", content())
	end)
end)
