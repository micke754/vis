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

describe("helix textobjects (mi/ma)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("miw selects inner word", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("miw")
		-- After miw, the word "hello" should be selected
		-- We can test by deleting it
		vis:feedkeys("d")
		assert.are.equal(" world\n", content())
	end)

	it("maw selects around word (includes trailing space)", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("maw")
		vis:feedkeys("d")
		assert.are.equal("world\n", content())
	end)

	it("mi( selects inner parens", function()
		reset("(hello) world\n")
		win.selection.pos = 1
		win.selection.anchored = true
		vis:feedkeys("mi(")
		vis:feedkeys("d")
		assert.are.equal("() world\n", content())
	end)

	it("ma( selects around parens", function()
		reset("(hello) world\n")
		win.selection.pos = 1
		win.selection.anchored = true
		vis:feedkeys("ma(")
		vis:feedkeys("d")
		assert.are.equal(" world\n", content())
	end)
end)
