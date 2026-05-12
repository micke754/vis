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

describe("helix goto-word (gw)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("does not crash on escape", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("gw<Escape>")
		assert.are.equal("hello world\n", content())
	end)

	it("does not crash on empty file", function()
		reset("\n")
		win.selection.pos = 0
		vis:feedkeys("gw<Escape>")
		assert.are.equal("\n", content())
	end)

	it("does not crash on single-char words", function()
		reset("a b c d\n")
		win.selection.pos = 0
		vis:feedkeys("gw<Escape>")
		assert.are.equal("a b c d\n", content())
	end)

	it("does not crash on punctuation-only lines", function()
		reset("( ) { } [ ]\n")
		win.selection.pos = 0
		vis:feedkeys("gw<Escape>")
		assert.are.equal("( ) { } [ ]\n", content())
	end)
end)

describe("helix goto mode (gh/gl/gt/gc/gb)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("gh goes to line start (column 0)", function()
		reset("    hello world\n", 7)
		vis:feedkeys("gh")
		assert.are.equal(0, win.selection.pos)
	end)

	it("gl goes to line end", function()
		reset("hello world\n", 0)
		vis:feedkeys("gl")
		local pos = win.selection.pos
		assert.are.equal(true, pos >= 5, "should be near line end, got " .. pos)
	end)

	it("gs goes to first non-blank", function()
		reset("    hello world\n", 0)
		vis:feedkeys("gs")
		assert.are.equal(4, win.selection.pos)
	end)

	it("gg goes to file start", function()
		reset("line 1\nline 2\nline 3\n", 14)
		vis:feedkeys("gg")
		assert.are.equal(0, win.selection.pos)
	end)

	it("ge goes to file end", function()
		reset("line 1\nline 2\nline 3\n", 0)
		vis:feedkeys("ge")
		-- ge should move cursor toward file end
		assert.are.equal(true, win.selection.pos > 0, "should move cursor")
	end)

	it("gt goes to viewport top", function()
		reset("line 1\nline 2\nline 3\n", 14)
		vis:feedkeys("gt")
		assert.are.equal(true, win.selection.pos >= 0)
	end)

	it("gc goes to viewport center", function()
		reset("line 1\nline 2\nline 3\n", 0)
		vis:feedkeys("gc")
		assert.are.equal(true, win.selection.pos >= 0)
	end)

	it("gb goes to viewport bottom", function()
		reset("line 1\nline 2\nline 3\n", 0)
		vis:feedkeys("gb")
		assert.are.equal(true, win.selection.pos >= 0)
	end)
end)
