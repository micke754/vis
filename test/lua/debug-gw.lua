local vis, win, file

local function setup(text)
  vis:command("e!")
  file = vis.win.file
  file:delete(0, file.size)
  file:insert(0, text)
  vis.win.view.pos = 0
  return vis.win
end

local function dump_labels()
  -- Access the view's jump labels through the C API isn't directly available,
  -- but we can check the rendered behavior by looking at selections after jump
end

-- Test with simple text containing "string"
setup("local string = require('string')\nlocal foo = string.format('hello')\n")
vis:feedkeys("gw")

-- After gw, labels should be set. We can't directly inspect them from Lua,
-- but we can check if vis is in jump label mode
if vis.jump_labels_active then
  print("Jump labels are active")
else
  print("Jump labels NOT active - gw didn't work")
end

-- Press Escape to cancel
vis:feedkeys("<Escape>")

-- Check cursor position is still at start
local pos = vis.win.view.pos
print("Cursor position after cancel: " .. tostring(pos))

vis:command("q!")
