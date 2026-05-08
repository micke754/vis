# Agent Coding Guidelines

## Project direction
- Audience: local fork first. Native correctness over upstream purity when needed.
- Keep Lua for profile/config/mapping glue.
- Use C for editor semantics: motions, selections, operator ranges, repeated chains, cross-line edge cases.
- Preserve `vim` behavior. Gate Helix-specific core behavior behind explicit C state/options.

## Style
- Follow existing vis style. Do not introduce a new house style.
- C:
  - Tabs for indentation.
  - K&R-style function braces as existing files use: `void func(...) {`
  - Keep declarations near use, matching surrounding code.
  - Prefer small helper functions with `static` file scope.
  - Use existing types/helpers: `Filerange`, `text_range_valid`, `view_selections_get`, `view_selections_set`, `text_object_word`, etc.
  - Keep edits minimal and local.
  - Avoid broad refactors unless requested.
  - Do not change Vim/default semantics accidentally.
- Lua:
  - Keep profile files declarative where possible.
  - Use functions only for behavior that cannot be expressed cleanly as mappings.
  - Avoid long `feedkeys` chains when C can implement the semantic directly.
  - Preserve existing module shape: `apply(manager, win)`.

## Architecture preference
- `lua/keymaps/init.lua`: profile selection and lifecycle.
- `lua/keymaps/helix.lua`: Helix mappings and simple glue.
- C core: semantic behavior controlled by explicit flag/option:
  - `selectionsemantics=vim|helix`
  - `vis->selection_semantics`
  - `vis->helix_select`
- Prefer C helpers for directed selections/ranges. Current helper:
  - `view_selections_set_directed(Selection*, Filerange*, cursor_at_start)`
- Avoid adding new Lua feedkey chains for core behavior.

## Regression tests
- Add focused Lua tests under `test/lua/keymap-helix-*.lua` for every manual bug fixed.
- Prefer testing selected ranges through buffer mutations, e.g. `motion + d`, because visual highlight is hard to assert.
- Current focused suites:
  - `keymap-helix-count.lua`
  - `keymap-helix-find.lua`
  - `keymap-helix-line.lua`
  - `keymap-helix-operator.lua`
  - `keymap-helix-paste.lua`
  - `keymap-helix-regression.lua`
  - `keymap-helix-search.lua`
  - `keymap-helix-select.lua`

## Validation
- Always run `make -j2` after C changes.
- Run relevant focused tests, or all Helix suites:
  ```sh
  cd test/lua
  for t in keymap-helix-search.lua keymap-helix-paste.lua keymap-helix-find.lua keymap-helix-count.lua keymap-helix-select.lua keymap-helix-line.lua keymap-helix-regression.lua keymap-helix-operator.lua; do
    LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1
  done
  ```
- Manual smoke tests remain important for visual selection rendering/cursor feel.

## Communication
- State cause, fix, validation.
- Mention if behavior is C-gated or Lua-only.
- Keep responses concise.
