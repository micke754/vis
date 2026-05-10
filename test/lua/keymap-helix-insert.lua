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

describe("helix insert/append (i/a)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("i with bare cursor stays at current position", function()
		reset("hello world\n")
		win.selection.pos = 3
		win.selection.anchored = false
		vis:feedkeys("i<Escape>")
		assert.are.equal(3, win.selection.pos)
	end)

	it("a with bare cursor moves one char right", function()
		reset("hello world\n")
		win.selection.pos = 0
		win.selection.anchored = false
		vis:feedkeys("a<Escape>")
		assert.are.equal(1, win.selection.pos)
	end)

	it("i with vw selection collapses to start", function()
		reset("hello world\n")
		win.selection.pos = 0
		-- vw selects word at cursor position
		vis:feedkeys("vwi<Escape>")
		-- i should collapse to start of word (pos 0)
		assert.are.equal(0, win.selection.pos)
	end)

	it("a with vw selection collapses to end", function()
		reset("hello world\n")
		win.selection.pos = 0
		-- vw selects "hello " (word + trailing space, 6 chars, ending at pos 6)
		vis:feedkeys("vwa<Escape>")
		-- a should put cursor at end of selection (pos 5 for "hello ")
		assert.are.equal(5, win.selection.pos)
	end)
end)
