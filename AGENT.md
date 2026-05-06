# Agent Coding Guidelines

## Project direction
- Audience: local fork first. Native correctness over upstream purity when needed.
- Keep Lua for profile/config/mapping glue.
- Use C for editor semantics: motions, selections, operator ranges, repeated chains, cross-line edge cases.
- Preserve `vim` behavior. Gate Helix-specific core behavior behind an explicit C state/option.

## Style
- Follow existing vis style. Do not introduce a new house style.
- C:
  - Tabs for indentation.
  - K&R-style function braces as existing files use:
    - `void func(...) {`
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
- C core: semantic behavior controlled by an explicit flag/option, e.g. Helix selection semantics.

## Validation
- Always run `make -j2` after C changes.
- Prefer focused tests in `test/lua/keymap-profile.lua` for behavior visible through keys.
- Manual smoke tests remain important for visual selection rendering.

## Communication
- State cause, fix, validation.
- Mention if behavior is C-gated or Lua-only.
- Keep responses concise.
