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

describe("helix surround add (ms)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("adds parentheses around selected text", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("vwms(")
		-- vw selects "hello " (word + space)
		assert.are.equal("(hello )world\n", content())
	end)

	it("adds brackets around selected text", function()
		reset("hello world\n")
		win.selection.pos = 0
		vis:feedkeys("vwms[")
		assert.are.equal("[hello ]world\n", content())
	end)
end)

describe("helix surround delete (md)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("deletes surrounding parentheses", function()
		reset("(hello) world\n")
		-- Position inside parens, select the word
		win.selection.pos = 1
		vis:feedkeys("vwmd(")
		assert.are.equal("hello world\n", content())
	end)
end)

describe("helix surround replace (mr)", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("replaces parentheses with brackets", function()
		reset("(hello) world\n")
		win.selection.pos = 1
		vis:feedkeys("vwmr({")
		assert.are.equal("{hello} world\n", content())
	end)
end)
