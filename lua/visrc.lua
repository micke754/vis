-- load standard vis module, providing parts of the Lua API
require("vis")

-- Select keymap profile. Custom `:set` options are not exposed through
-- `vis.options`, so use the keymap module directly from Lua config.
local keymaps = require("keymaps")
keymaps.apply("helix")
-- keymaps.apply("vim")

vis.events.subscribe(vis.events.INIT, function()
	-- Your global configuration options
end)

vis.events.subscribe(vis.events.WIN_OPEN, function(win) -- luacheck: no unused args
	-- Your per window configuration options e.g.
	-- vis:command('set number')
end)
