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

describe("helix keymap regressions", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("normal-mode w selects current word for delete", function()
		reset("This string-test_test.test\n")
		vis:feedkeys("wd")
		assert.are.same("string-test_test.test\n", content())
	end)

	it("w separates punctuation token from following word", function()
		reset("one-of-a-kind\n")
		vis:feedkeys("wwd")
		assert.are.same("oneof-a-kind\n", content())
	end)

	it("ms adds paren surround to selection", function()
		reset("word\n")
		vis:feedkeys("wms(")
		assert.are.same("(word)\n", content())
	end)

	it("ms adds quote surround to multicursor selections", function()
		reset("one\ntwo\n")
		vis:feedkeys("Cwms\"")
		assert.are.same("\"one\"\n\"two\"\n", content())
	end)
end)
