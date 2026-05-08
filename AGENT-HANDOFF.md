# Agent Handoff Notes (feature/helix-keymaps)

## Quick start for next agent
1. Check branch/status:
   ```sh
   git status --short --branch
   ```
2. Read first:
   - `plan.md`
   - `AGENT.md`
   - `lua/keymaps/init.lua`
   - `lua/keymaps/helix.lua`
   - `vis.c` Helix helper area around `helix_word_range()`
   - focused tests under `test/lua/keymap-helix-*.lua`
3. Build/run all focused Helix tests:
   ```sh
   make -j2
   cd test/lua
   for t in keymap-helix-search.lua keymap-helix-paste.lua keymap-helix-find.lua keymap-helix-count.lua keymap-helix-select.lua keymap-helix-line.lua keymap-helix-regression.lua keymap-helix-operator.lua; do
     LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1
   done
   ```

## Current behavior implemented
- Helix profile:
  - `:set keymap helix`
  - `:set keymap vim`
  - profile switching toggles C `selectionsemantics`.
- Status line in Helix profile:
  - `NOR`
  - `SEL`
  - `INS`
- True Helix select state:
  - `v` toggles `vis->helix_select`, not Vim visual mode.
  - `<Escape>` exits Helix select state.
- C-backed Helix word semantics:
  - `w/e/b/W/E/B`
  - counts: `2w`, `3e`, `2b`
  - repeated chains: `ww`, `ee`, `bb`
  - select chains: `vww`, `vee`, `vbb`, `vbb;`
  - punctuation tokens/runs like `one-of-a-kind`, `string-test_test.test`
- Operators:
  - `d/c/y/>/<` consume anchored Helix selections in normal mode.
  - bare `d/c/y` operate on implicit one-character cursor selection.
  - `y` preserves current selection for `p/P`.
- Line selection:
  - `x`
  - `X`
  - `xx`
  - `2x`
- Paste:
  - `p/P` no longer replace implicit char/selection in Helix normal mode.
  - `wyp`, `wyP` tested.
- Find motions:
  - `f/t/F/T` no longer enter Vim visual mode in Helix normal mode.
  - mapped to cross-line C motions (`to/till`, not line-confined mappings).
- Search:
  - `*` registers current selection/word as search pattern and shows `Search pattern set: ...`.
  - `*` does not immediately jump.
  - `n/N` after `*` selects match width, even after `;` collapse, based on search-register pattern length.

## Important files touched
- Core:
  - `vis.c`
  - `vis-motions.c`
  - `vis-operators.c`
  - `vis-core.h`
  - `vis-cmds.c`
  - `vis-lua.c`
  - `view.c`
  - `view.h`
  - `main.c`
- Lua:
  - `lua/vis-std.lua`
  - `lua/keymaps/init.lua`
  - `lua/keymaps/helix.lua`
- Tests:
  - `test/lua/keymap-helix-count.lua`
  - `test/lua/keymap-helix-find.lua`
  - `test/lua/keymap-helix-line.lua`
  - `test/lua/keymap-helix-operator.lua`
  - `test/lua/keymap-helix-paste.lua`
  - `test/lua/keymap-helix-regression.lua`
  - `test/lua/keymap-helix-search.lua`
  - `test/lua/keymap-helix-select.lua`

## Known architectural debt / cleanup
- `vis.c` now has an early `helix_word_range()` path for Helix normal/select word motions.
- Older Helix word special-case code still remains later in generic motion handling. It is mostly bypassed for normal/select word motions but may still affect Vim visual fallback paths. Do not delete casually.
- Word/token logic exists both in:
  - `vis-motions.c` Helix boundary helpers
  - `vis.c` `helix_word_*` range helpers
- Desired cleanup:
  1. centralize Helix word token/range helpers,
  2. remove dead special cases once tests/manual smoke are stable,
  3. keep `view_selections_set_directed()` for direction-safe selections.

## Manual QC checklist before next big work
- Statusline: `NOR`, `SEL`, `INS`.
- Word motions:
  - `w/e/b`
  - `2w/3e/2b`
  - `ww/ee/bb`
  - `vww/vee/vbb/vbb;`
  - `W/E/B`
- Punctuation:
  - `one-of-a-kind "modal"`
  - `string-test_test.test`
- Operators:
  - `wd`, `wc`, `wy`, `wyp`, `wyP`
- Lines:
  - `x`, `xx`, `2x`, `X`, then `d/c/y`
- Find:
  - `f`, `t`, `F`, `T`
  - select mode variants: `vf`, `vt`, `vF`, `vT`
- Search:
  - `*`, message appears, no immediate jump
  - `*n`, `*N`
  - `*;n`, `*;N`
  - selected pattern containing whitespace
- Profile switching:
  - `:set keymap vim`
  - `:set keymap helix`
- Split windows:
  - `<C-w>s`, `<C-w>v`; mappings should apply in new window.

## Recommended next tasks
1. Run manual QC checklist and add any newly found bug as regression first.
2. Cleanup word-motion architecture if QC is good:
   - centralize Helix word helpers,
   - remove old special-case block in `vis.c` carefully.
3. Then continue remaining Helix parity:
   - paste replacement semantics over multiple selections,
   - search/select mode edge cases,
   - multi-cursor Helix behavior.

## Notes for moving machines
- Commit or create a patch before switching devices.
- If using wrapped/package vis, Lua runtime path may differ. Reliable dev command from source tree:
  ```sh
  LD_LIBRARY_PATH=./dependency/install/usr/lib ./vis
  ```
- Focused tests require source-built binary/libs as above.
