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

describe("helix find motions", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("f selects through found character", function()
		reset("abc-def\n")
		vis:feedkeys("f-d")
		assert.are.same("def\n", content())
	end)

	it("t selects until found character", function()
		reset("abc-def\n")
		vis:feedkeys("t-d")
		assert.are.same("-def\n", content())
	end)

	it("F selects backward through found character", function()
		reset("abc-def\n", 6)
		vis:feedkeys("F-d")
		assert.are.same("abcf\n", content())
	end)

	it("select mode f extends through found character", function()
		reset("abc-def\n")
		vis:feedkeys("vf-d")
		assert.are.same("def\n", content())
	end)

	it("select mode t extends until found character", function()
		reset("abc-def\n")
		vis:feedkeys("vt-d")
		assert.are.same("-def\n", content())
	end)

	it("select mode F extends backward through found character", function()
		reset("abc-def\n", 6)
		vis:feedkeys("vF-d")
		assert.are.same("abc\n", content())
	end)

	it("select mode T extends backward until found character", function()
		reset("abc-def\n", 6)
		vis:feedkeys("vT-d")
		assert.are.same("abc-\n", content())
	end)
end)
