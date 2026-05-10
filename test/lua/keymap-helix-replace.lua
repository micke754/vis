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

describe("helix replace char (r)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("replaces single char with bare cursor", function()
		reset("Hello World\n")
		win.selection.pos = 0
		vis:feedkeys("rx")
		assert.are.equal("xello World\n", content())
	end)

	it("replaces single char at middle position", function()
		reset("abcdef\n")
		win.selection.pos = 3  -- on 'd'
		vis:feedkeys("rz")
		assert.are.equal("abczef\n", content())
	end)

	it("replaces anchored word selection by repeating char", function()
		reset("Hello World\n")
		win.selection.pos = 0
		-- vw selects first word (includes trailing space in Helix)
		vis:feedkeys("vw")
		vis:feedkeys("rx")
		-- vw in Helix selects the word + trailing space
		-- So "Hello " (6 chars) becomes "xxxxxx"
		local c = content()
		assert.are.equal("xxxxxxWorld\n", c)
	end)

	it("ignores Escape", function()
		reset("Hello\n")
		win.selection.pos = 0
		vis:feedkeys("r<Escape>")
		assert.are.equal("Hello\n", content())
	end)
end)

describe("helix replace with yanked (R)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("replaces selection with yanked text", function()
		reset("abcdef ghij\n")
		-- Select "abcdef " (first word + space) and yank it
		win.selection.pos = 0
		vis:feedkeys("vw")
		vis:feedkeys("y")
		-- Select "ghij" and replace with yanked
		vis:feedkeys("w")
		vis:feedkeys("R")
		local c = content()
		-- "ghij" replaced by "abcdef "
		assert.are.equal("abcdef abcdef \n", c)
	end)

	it("does nothing with bare cursor (no selection)", function()
		reset("Hello\n")
		win.selection.pos = 0
		win.selection.anchored = false
		-- Yank something first
		vis:feedkeys("vw")
		vis:feedkeys("y")
		-- Collapse to cursor (bare cursor)
		vis:feedkeys(";")
		vis:feedkeys("R")
		-- Bare cursor: R should do nothing
		assert.are.equal("Hello\n", content())
	end)
end)