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

describe("helix paste", function()
	after_each(function()
		vis:command("set keymap vim")
	end)

	it("p inserts after cursor without replacing character", function()
		reset("1 banana 2 3 4\n", 10)
		vis.registers['\"'] = { "banana " }
		vis:feedkeys("p")
		assert.are.same("1 banana 2 banana 3 4\n", content())
	end)

	it("wyp pastes after selected word", function()
		reset("1 banana 2 3 4\n", 2)
		vis:feedkeys("wyp")
		assert.are.same("1 banana banana 2 3 4\n", content())
	end)
end)
