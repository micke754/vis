require("vis")
require("keymaps").set("helix")

-- === Formatters (vis-format) ===
local format = require("vis-format")
format.options.on_save = true
format.formatters.lua = format.formatters.stylua
format.formatters.python = format.formatters.ruff

local function format_current(win)
	win = win or vis.win
	if not win or not win.file then
		vis:info("format: no file")
		return false
	end
	format.apply(win.file, nil, win.selection.pos)
	return true
end

vis:command_register("Format", function(_, _, win)
	return format_current(win)
end, "Format current buffer")

vis:command_register("format", function(_, _, win)
	return format_current(win)
end, "Format current buffer")

-- === LSP (vis-lspc) ===
local lspc = require("vis-lspc")
lspc.autostart = true

-- Override Python server to use basedpyright
lspc.ls_map.python = {
	name = "basedpyright",
	cmd = "basedpyright-langserver --stdio",
	formatting_options = { tabSize = 4, insertSpaces = true },
}

-- Per-window bindings applied after Helix profile shadows global mappings
local function apply_bindings(win)
	-- Format buffer
	win:map(vis.modes.NORMAL, "+f", function()
		format_current(vis.win)
	end, "Format buffer")

	-- LSP bindings
	win:map(vis.modes.NORMAL, "<F2>", function()
		vis:command("lspc-start-server")
	end, "lspc: start server")

	win:map(vis.modes.NORMAL, "gd", function()
		vis:command("lspc-definition")
	end, "lspc: jump to definition")

	win:map(vis.modes.NORMAL, "gD", function()
		vis:command("lspc-declaration")
	end, "lspc: jump to declaration")

	win:map(vis.modes.NORMAL, "gi", function()
		vis:command("lspc-implementation")
	end, "lspc: jump to implementation")

	win:map(vis.modes.NORMAL, "gr", function()
		vis:command("lspc-references")
	end, "lspc: show references")

	win:map(vis.modes.NORMAL, "K", function()
		vis:command("lspc-hover")
	end, "lspc: hover")

	win:map(vis.modes.NORMAL, " e", function()
		vis:command("lspc-show-diagnostics")
	end, "lspc: show diagnostics")

	win:map(vis.modes.NORMAL, " n", function()
		vis:command("lspc-next-diagnostic")
	end, "lspc: next diagnostic")

	win:map(vis.modes.NORMAL, " N", function()
		vis:command("lspc-prev-diagnostic")
	end, "lspc: prev diagnostic")

	win:map(vis.modes.NORMAL, "<C- >", function()
		vis:command("lspc-completion")
	end, "lspc: completion")

	win:map(vis.modes.INSERT, "<C- >", function()
		vis:command("lspc-completion")
		vis.mode = vis.modes.INSERT
	end, "lspc: completion")
end

-- Apply to existing windows
for win in vis:windows() do
	apply_bindings(win)
end

-- Apply to future windows
vis.events.subscribe(vis.events.WIN_OPEN, function(win)
	apply_bindings(win)
end)

-- Re-apply after keymap START hook reapplies profile to existing windows
vis.events.subscribe(vis.events.START, function()
	for win in vis:windows() do
		apply_bindings(win)
	end
end)
