# Lua LSP and formatter configuration

This fork currently configures formatters and language servers from
`lua/visrc.lua`.

The local `vis` binary is symlinked from `~/.local/bin/vis` to
`/home/kmichaels/Software/vis/vis`. Because vis prepends the binary-relative
`lua/` directory to `package.path` before `~/.config/vis`, the active startup
config is the repo file:

```text
/home/kmichaels/Software/vis/lua/visrc.lua
```

The user config directory still stores third-party plugins:

```text
~/.config/vis/vis-format
~/.config/vis/vis-lspc
```

## Formatter setup

Formatting is provided by
[`vis-format`](https://github.com/milhnl/vis-format).

The active config does the following:

- Enables format-on-save with `format.options.on_save = true`
- Forces Lua formatting through `stylua`
- Forces Python formatting through `ruff`
- Registers both `:format` and `:Format`
- Binds `+f` in normal mode to format the current buffer

Relevant config:

```lua
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
```

Use any of these to format:

```text
+f
:format
:Format
```

Lua config changes are loaded at editor startup. Rebuilding vis is not required
for formatter configuration changes.

## LSP setup

Language server support is provided by
[`vis-lspc`](https://github.com/fischerling/vis-lspc).

The active config does the following:

- Enables `lspc.autostart = true`
- Uses the default `lua-language-server` config from `vis-lspc`
- Overrides Python to use `basedpyright-langserver --stdio`
- Adds window-local keybindings after the Helix keymap profile is applied

Python server override:

```lua
local lspc = require("vis-lspc")
lspc.autostart = true

lspc.ls_map.python = {
	name = "basedpyright",
	cmd = "basedpyright-langserver --stdio",
	formatting_options = { tabSize = 4, insertSpaces = true },
}
```

Manual startup flow for Python:

```text
:lspc-start-server python
:lspc-open
```

Manual startup flow for Lua:

```text
:lspc-start-server lua
:lspc-open
```

Autostart should make the manual start command unnecessary after the server is
working, but `:lspc-open` is useful when troubleshooting.

## Keybindings

The config applies bindings window-locally because the Helix keymap profile
shadows global mappings. Bindings are applied on existing windows, on
`WIN_OPEN`, and again on `START` after the keymap profile reapplies.

| Key | Action |
| --- | --- |
| `+f` | Format current buffer |
| `<F2>` | Start language server |
| `gd` | Go to definition |
| `gD` | Go to declaration |
| `gi` | Go to implementation |
| `gr` | Show references |
| `K` | Hover |
| `<Space>e` | Show diagnostics for current line |
| `<Space>n` | Next diagnostic |
| `<Space>N` | Previous diagnostic |
| `<C-Space>` | Completion |

`K` overrides the Helix keymap binding for keeping selections matching a regex.
Move hover to another key if that Helix binding is needed.

## Diagnostic highlighting

`vis-lspc` defaults to line diagnostic highlighting:

```lua
highlight_diagnostics = "line"
```

Line mode paints a few cells at the left side of diagnostic lines using styles
such as `fore:yellow,italics,reverse`. This can appear as yellow blocks or
artifacts over the first characters of a line.

Disable inline diagnostic highlighting with:

```lua
lspc.highlight_diagnostics = false
```

Or use exact range highlighting instead:

```lua
lspc.highlight_diagnostics = "range"
```

Diagnostics remain available through:

```text
<Space>e
<Space>n
<Space>N
:lspc-show-diagnostics
```

## Troubleshooting

If `:format` or `:Format` is unknown, the active `visrc.lua` is not the repo
`lua/visrc.lua`, or config loading failed before command registration.

If `:lspc-*` commands are unknown, `vis-lspc` did not load.

If formatting is a no-op, check that the filetype is detected as `lua` or
`python`, and that `stylua` or `ruff` is available on `PATH`.

If Python LSP fails to start, verify that `basedpyright-langserver --stdio` is
the configured command. Running `basedpyright-langserver` without `--stdio`
fails because basedpyright requires an explicit transport.

If config edits do not take effect, restart vis. Lua config changes do not need
`make`, but they are only read when vis starts.
