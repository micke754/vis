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

describe("helix insert/append", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("i inserts before selection start", function()
		reset("hello world\n", 0)
		vis:feedkeys("wi")
		-- w selects "hello " (0-6), i collapses to start (0)
		assert.are.equal(0, win.selection.pos)
	end)

	it("a appends after selection end", function()
		reset("hello world\n", 0)
		vis:feedkeys("wa")
		-- w selects "hello " (0-6), a collapses to end (6)
		assert.are.equal(6, win.selection.pos)
	end)

	it("I goes to first non-blank and enters insert", function()
		reset("    hello\n", 6)
		vis:feedkeys("I<Escape>")
		-- I goes to first non-blank (pos 4)
		assert.are.equal(4, win.selection.pos)
	end)

	it("A goes to line end and enters insert", function()
		reset("hello\n", 0)
		vis:feedkeys("A<Escape>")
		-- A goes to end of line
		assert.are.equal(true, win.selection.pos >= 4, "should be near line end")
	end)
end)
