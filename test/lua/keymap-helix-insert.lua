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
		vis:feedkeys("vwi<Escape>")
		-- i should collapse to start of selection (pos 0)
		assert.are.equal(0, win.selection.pos)
	end)

	it("a with vw selection goes past the selection", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("vwa<Escape>")
		-- a should put cursor at range.end (right after the selection)
		-- vw on "hello world" from pos 0 selects "hello " (6 chars) or similar
		-- cursor should be at least 5 (past "hello")
		local pos = win.selection.pos
		assert.are.equal(true, pos >= 5, "a should go past selection, got pos=" .. pos)
	end)
end)
