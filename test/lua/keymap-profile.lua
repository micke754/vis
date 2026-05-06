require('keymaps/init')

local win = vis.win
local file = win.file

local function content()
	return file:content(0, file.size)
end

local function reset(data)
	file:delete(0, file.size)
	file:insert(0, data)
	win.selection.pos = 0
end

describe("keymap profile", function()
	before_each(function()
		reset("abc\n")
		vis:command("set keymap vim")
	end)

	after_each(function()
		vis:command("set keymap vim")
	end)

	it("supports :set keymap helix in normal mode", function()
		vis:command("set keymap helix")
		vis:feedkeys("d")
		assert.are.same("bc\n", content())
	end)

	it("supports vis.options.keymap config style", function()
		vis.options.keymap = "helix"
		vis:feedkeys("d")
		assert.are.same("bc\n", content())
	end)

	it("restores vim defaults when switching back", function()
		vis:command("set keymap helix")
		vis:command("set keymap vim")
		vis:feedkeys("d<Escape>")
		assert.are.same("abc\n", content())
	end)

	it("supports insert mode Ctrl-k kill to line end", function()
		vis:command("set keymap helix")
		vis:feedkeys("i<C-k><Escape>")
		assert.are.same("\n", content())
	end)

	it("maps v to visual mode in helix profile", function()
		vis:command("set keymap helix")
		vis:feedkeys("v")
		assert.are.same(vis.modes.VISUAL, vis.mode)
	end)

	it("does not extend selection for hjkl in normal mode", function()
		vis:command("set keymap helix")
		vis:feedkeys("ll")
		assert.are.same(vis.modes.NORMAL, vis.mode)
		vis:feedkeys("d")
		assert.are.same("ab\n", content())
	end)

	it("collapses word-selection before hjkl motion", function()
		vis:command("set keymap helix")
		reset("This string\n")
		vis:feedkeys("wl")
		assert.are.same(vis.modes.NORMAL, vis.mode)
	end)

	it("collapses word-selection before goto motion", function()
		vis:command("set keymap helix")
		reset("This string\n")
		vis:feedkeys("wgl")
		assert.are.same(vis.modes.NORMAL, vis.mode)
	end)

	it("deletes visual selection with d", function()
		vis:command("set keymap helix")
		vis:feedkeys("vlld")
		assert.are.same("\n", content())
	end)

	it("includes starting character for word-forward selection", function()
		vis:command("set keymap helix")
		reset("This is a test string\n")
		vis:feedkeys("wd")
		assert.are.same("is a test string\n", content())
	end)

	it("does not retain previous terminal character on repeated e", function()
		vis:command("set keymap helix")
		reset("This string\n")
		vis:feedkeys("eed")
		assert.are.same("This\n", content())
	end)

	it("resets selection scope on cross-line repeated e", function()
		vis:command("set keymap helix")
		reset("This is a test string\nThis string\n")
		vis:feedkeys("eed")
		assert.are.same("This is a test string\n string\n", content())
	end)

	it("includes starting character for word-backward selection", function()
		vis:command("set keymap helix")
		reset("This is a test string\n")
		vis:feedkeys("Gbbd")
		assert.are.same("This is a \n", content())
	end)

	it("does not retain previous terminal character on repeated b", function()
		vis:command("set keymap helix")
		reset("This string\n")
		vis:feedkeys("Gbbd")
		assert.are.same("string\n", content())
	end)

	it("resets selection scope on repeated word motions", function()
		vis:command("set keymap helix")
		reset("abc def ghi\n")
		vis:feedkeys("wwd")
		assert.are.same("abc ghi\n", content())
	end)

	it("changes visual selection with c", function()
		vis:command("set keymap helix")
		vis:feedkeys("vllcZ<Escape>")
		assert.are.same("Z\n", content())
	end)

	it("applies profile mappings to newly split windows", function()
		vis:command("set keymap helix")
		vis:feedkeys("<C-w>v")
		vis:feedkeys("d")
		assert.are.same("bc\n", content())
	end)
end)
