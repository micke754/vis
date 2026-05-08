# Helix Keymap Work Plan / State (feature/helix-keymaps)

## Goal
Make vis support a toggleable Helix keymap/profile (`:set keymap helix` / `vim`) with native-feeling Helix behavior.

Priorities for this fork:
- Correctness and feel for local Helix workflow first.
- Lua remains profile/config/mapping glue.
- C owns hard editor semantics: motions, selections, operators, ranges.
- Preserve Vim behavior by gating Helix-specific C behavior behind explicit state/options.

## Current state summary

### Profile/system
- Runtime profile switching:
  - `:set keymap helix`
  - `:set keymap vim`
- Helix profile toggles C semantics:
  - `selectionsemantics=helix`
- Vim profile restores:
  - `selectionsemantics=vim`
- Helix statusline via Lua/C state:
  - `NOR`
  - `SEL`
  - `INS`

### Core state/options
- `vis->selection_semantics`
- `vis->helix_select`
- Lua exposed:
  - `vis.helix_keymap`
  - `vis.helix_select`

### Selection helpers
- Added directed selection helper:
  - `view_selections_set_directed(Selection*, const Filerange*, bool cursor_at_start)`
- This avoids fragile ordering around:
  - `view_selections_set`
  - `sel->anchored = true`
  - `view_selections_flip`

### Word motions
C-backed under Helix semantics:
- `w/e/b/W/E/B`
- counts:
  - `2w`
  - `3e`
  - `2b`
- repeated chains:
  - `ww`
  - `ee`
  - `bb`
- select mode:
  - `vww`
  - `vee`
  - `vbb`
  - `vbb;`
- punctuation and longword behavior covered for known cases.

### Operators
- Normal Helix selections are consumed directly by:
  - `d`
  - `c`
  - `y`
  - `>`
  - `<`
- Bare selection-actions use implicit one-character cursor selection.
- `y` preserves selection so `p/P` can use its edge.

### Select mode
- `v` toggles `vis->helix_select`; does not enter Vim visual mode.
- `<Escape>` exits Helix select mode.
- `;` collapses selection to current cursor/head endpoint.

### Line selection
C-backed:
- `x`
- `X`
- `xx`
- `2x`

### Paste
Implemented/tested basics:
- `p` inserts after cursor/selection edge without replacing implicit char.
- `P` inserts before cursor/selection edge.
- `wyp`
- `wyP`

### Find motions
Helix normal mappings no longer enter Vim visual mode:
- `f`
- `t`
- `F`
- `T`

Current mappings use cross-line motions:
- `<vis-motion-to-right>`
- `<vis-motion-till-right>`
- `<vis-motion-to-left>`
- `<vis-motion-till-left>`

### Search
- `*` sets search pattern from current selection or word under cursor.
- `*` does not immediately jump.
- Shows concise info message:
  - `Search pattern set: <pattern>`
- `n/N` after `*` selects match width.
- `*;n` / `*;N` still select full pattern width by using search-register length.

## Regression suites
Focused Helix tests currently present:
- `test/lua/keymap-helix-count.lua`
- `test/lua/keymap-helix-find.lua`
- `test/lua/keymap-helix-line.lua`
- `test/lua/keymap-helix-operator.lua`
- `test/lua/keymap-helix-paste.lua`
- `test/lua/keymap-helix-regression.lua`
- `test/lua/keymap-helix-search.lua`
- `test/lua/keymap-helix-select.lua`

Run all:
```sh
make -j2
cd test/lua
for t in keymap-helix-search.lua keymap-helix-paste.lua keymap-helix-find.lua keymap-helix-count.lua keymap-helix-select.lua keymap-helix-line.lua keymap-helix-regression.lua keymap-helix-operator.lua; do
  LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1
done
```

Last validation in this session: all pass.
Manual QC: passed with flying colors. Paste differs from upstream Helix but is accepted/preferred for now.

## Known architectural debt

### 1. Helix word logic is split
Current places:
- `vis-motions.c`: Helix char categories/boundaries and motion functions.
- `vis.c`: `helix_word_range()` and helper functions.

Desired future cleanup:
- centralize token/range logic into one C API.

### 2. Old Helix word special-case code still exists
- `vis.c` now has an early Helix word path for normal/select word motions.
- Old special cases in generic motion handling remain mostly bypassed for these paths.
- Do not delete casually; first verify visual/Vim fallback impact.

### 3. Search/select behavior still partial
- Normal `*`, `n/N` behavior improved.
- Select-mode and multi-selection search semantics still need deeper Helix parity.

### 4. Paste semantics beyond basics
- Basic `p/P`, `wyp`, `wyP` work.
- Manual QC passed.
- Current behavior is intentionally not identical to upstream Helix; user likes it and wants to leave it for now.
- Revisit later only if explicitly desired: multi-selection paste, replacement semantics, register slot behavior.

## Recommended next work

### Immediate
1. Commit or patch current session before switching machines.
2. Run manual QC checklist from `AGENT-HANDOFF.md`.
3. Add regressions for any manual bugs found before fixing.

### Next implementation phase
1. Word architecture cleanup:
   - move/centralize Helix token helpers,
   - remove dead special cases carefully,
   - keep all focused tests green.
2. Search/select-mode parity:
   - `n/N` in `SEL`, selected patterns with whitespace, multi-selection.
3. Multi-cursor Helix behavior.
4. Paste parity only if reopened:
   - current behavior is accepted/preferred despite differing from upstream Helix,
   - multi-selection behavior,
   - replacement semantics if selection active,
   - register slot behavior.

## Runtime notes
- Source-built runtime command:
  ```sh
  LD_LIBRARY_PATH=./dependency/install/usr/lib ./vis
  ```
- If using wrapped/package vis, Lua path may differ. Symlink profile files into `~/.config/vis/keymaps/` if needed.

## Important branch note
- Work may be uncommitted. Preserve via commit/patch before changing devices.
