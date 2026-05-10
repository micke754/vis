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
