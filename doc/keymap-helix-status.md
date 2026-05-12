# Helix keymap status

## Phase 1 / early Phase 2 status

- [x] Added profile manager (`lua/keymaps/init.lua`)
- [x] Added profile modules (`vim`, `helix`)
- [x] Added `:set keymap <vim|helix>` option
- [x] Added startup integration (`lua/vis-std.lua`)
- [x] Added config toggle example (`lua/visrc.lua`)
- [x] Added normal mode arrow-key motion mappings
- [x] Added initial Helix-style window split mappings (`<C-w>s`, `<C-w>v`, `<C-w>w`)
- [x] Added dedicated Lua tests for keymap switching
- [x] Added Lua tests for insert `Ctrl-k` and visual-mode entry (`v`)
- [x] Added Lua tests for visual `d/c` behavior and split-window mapping application

## Known differences vs Helix (current)

- `f/F/t/T` are line-local in vis action mappings used by phase 1.
- `d/c/y` semantics are approximations based on vis cursor/selection model.
- Minor modes are still partial (`Space` and `Ctrl-w` have initial coverage; `g/m/z` parity remains incomplete).
- Select mode parity is in progress (`v`/`V` entry plus baseline visual actions wired; full Helix select behavior remains incomplete).
- `c` and `y` should target active selections once select-mode parity exists (current behavior is fallback-on-cursor).

## Notes

- Current design uses window-local overlay mappings to keep upstream defaults intact.
- Runtime switching is supported and should not require restart.
