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
- multicursor paste repeats the last register slot when there are more selections than register values.

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

### Multi-cursor
- Added Helix-style selection management mappings:
  - `C` copy selection/cursor to next line
  - `Alt-C` copy selection/cursor to previous line
  - `,` keep primary selection
  - `Alt-,` remove primary selection
  - `&` align selections
- Added `test/lua/keymap-helix-multicursor.lua`.
- Covered current behavior for:
  - `C`, `2C`, `Alt-C`
  - `,`, `Alt-,`
  - multi-cursor `d`
  - multi-cursor `w d`
  - multi-cursor line `x d`
  - multi-cursor `w y p` / `w y P`
  - multi-selection `*` registers all selections as regex alternatives.
  - select-mode `n/N` extends by adding new match selections.
  - `s` selects regex matches inside selections.
  - `S` splits selections by regex matches.
  - `Y` joins and yanks selections with newline separators.
  - `K` / `Alt-K` keep/remove selections matching regex.
  - `ms`, `md`, `mr` cover add/delete/replace surround pairs for common delimiters.
  - `mi` / `ma` cover inner/outer textobject selections for words, WORDs, paragraphs, and common delimiters.

### Search
- `*` sets search pattern from current selection or word under cursor.
- `*` does not immediately jump.
- Shows concise info message:
  - `Search pattern set: <pattern>`
- `*` searches selected text literally, escaping regex metacharacters.
- `n/N` after `*` selects actual match width.
- `*;n` / `*;N` still select full pattern width after collapse.
- `/` regex repeat also selects actual regex match width.

## Regression suites
Focused Helix tests currently present:
- `test/lua/keymap-helix-count.lua`
- `test/lua/keymap-helix-find.lua`
- `test/lua/keymap-helix-line.lua`
- `test/lua/keymap-helix-multicursor.lua`
- `test/lua/keymap-helix-operator.lua`
- `test/lua/keymap-helix-paste.lua`
- `test/lua/keymap-helix-profile.lua`
- `test/lua/keymap-helix-regression.lua`
- `test/lua/keymap-helix-search.lua`
- `test/lua/keymap-helix-select.lua`

Run all:
```sh
make -j2
cd test/lua
for t in keymap-helix-multicursor.lua keymap-helix-profile.lua keymap-helix-search.lua keymap-helix-paste.lua keymap-helix-find.lua keymap-helix-count.lua keymap-helix-select.lua keymap-helix-line.lua keymap-helix-regression.lua keymap-helix-operator.lua; do
  LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1
done
```

Last validation in this session: all pass.
Manual QC: passed with flying colors. Paste differs from upstream Helix but is accepted/preferred for now.

## Known architectural debt

### Centralized in vis-helix.c
All Helix logic now lives in `vis-helix.c`:
- Key actions (star search, surround, select, yank, rotation, etc.)
- Motion helpers (word range, search repeat, find select, operator context)
- Prompt handlers (select/split/keep/remove regex)
- Textobject selection logic
- `main.c` slimmed by ~300 lines, `vis.c` by ~450, `vis-prompt.c` by ~160.

### Deferred
- Selection-content rotation (`Alt-(`/`Alt-)`).
- Advanced multi-cursor/shell filter commands beyond current daily-driver set.
- Split-window/profile lifecycle segfault.
- Strict upstream paste parity unless explicitly reopened.

## Recommended next work

### Immediate
1. Commit or patch current session before switching machines.
2. Run manual QC checklist from `AGENT-HANDOFF.md`.
3. Add regressions for any manual bugs found before fixing.

### Next implementation phase
1. Architecture cleanup: ✅ complete
   - All Helix logic centralized in `vis-helix.c`.
2. Manual QC:
   - Run full manual test pass (see AGENT-HANDOFF.md).
3. Paste parity only if reopened:
   - current behavior is accepted/preferred.
4. `r` / `R` replace: ✅ complete
   - `r` replaces selection with repeated char (bare cursor: single char).
   - `R` replaces selection with yanked text (bare cursor: no-op).
   - Registered as `vis-helix-replace-char` and `vis-helix-replace-with-yanked`.

## Runtime notes
- Source-built runtime command:
  ```sh
  LD_LIBRARY_PATH=./dependency/install/usr/lib ./vis
  ```
- If using wrapped/package vis, Lua path may differ. Symlink profile files into `~/.config/vis/keymaps/` if needed.

## Important branch note
- Work may be uncommitted. Preserve via commit/patch before changing devices.
