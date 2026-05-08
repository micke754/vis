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

describe("helix counts", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("2w in normal mode selects destination word", function()
		reset("one two three four\n")
		vis:feedkeys("2wd")
		assert.are.same("one three four\n", content())
	end)

	it("3e in normal mode selects destination word", function()
		reset("one two three four\n")
		vis:feedkeys("3ed")
		assert.are.same("one two  four\n", content())
	end)

	it("repeated e advances from selected word end", function()
		reset("This string\n")
		vis:feedkeys("eed")
		assert.are.same("This \n", content())
	end)

	it("2b in normal mode selects destination word backward", function()
		reset("one two three four\n", 13)
		vis:feedkeys("2bd")
		assert.are.same("one  three four\n", content())
	end)

	it("2b from inside single word selects back to word start", function()
		reset("testing\n", 6)
		vis:feedkeys("2bd")
		assert.are.same("\n", content())
	end)
end)
