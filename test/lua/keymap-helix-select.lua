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

describe("helix select mode", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("d deletes character under cursor", function()
		reset("This\n")
		vis:feedkeys("d")
		assert.are.same("his\n", content())
	end)

	it("semicolon collapses selection before delete", function()
		reset("This string\n")
		vis:feedkeys("w;d")
		assert.are.same("Thisstring\n", content())
	end)

	it("v toggles helix select state", function()
		reset("This\n")
		vis:feedkeys("v")
		assert.truthy(vis.helix_select)
		vis:feedkeys("<Escape>")
		assert.falsy(vis.helix_select)
	end)

	it("select mode repeated w traverses punctuation tokens", function()
		reset("one-of-a-kind\n")
		vis:feedkeys("vwwd")
		assert.are.same("of-a-kind\n", content())
	end)

	it("v2w delete extends selection", function()
		reset("Remove the FOO BAR distracting words\n", 11)
		vis:feedkeys("v2wd")
		assert.are.same("Remove the distracting words\n", content())
	end)
end)
