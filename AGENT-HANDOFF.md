# Helix Mode — Agent Handoff

## Build & Test
```sh
make -j2
cd test/lua && for t in keymap-helix-*.lua; do LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1; done
```

## Key Files
- `vis-helix.c` — all Helix C logic (~1350 lines)
- `vis-core.h` — `HelixRepeatKind`, `helix_select`, `selection_semantics`, `jump_labels_*`
- `vis.c` — key processing (jump labels, Helix word pipeline, `vis_do` repeat clearing)
- `main.c` — action table entries
- `lua/keymaps/helix.lua` — key mappings, default profile activation
- `view.c/view.h` — `JumpLabel`, `view_jump_labels_set/clear`
- `test/lua/keymap-helix-*.lua` — 17 regression suites

## Architecture
- Single-TU build via `#include` chain in `vis.c`
- Helix behavior gated behind `vis->selection_semantics == VIS_SELECTION_SEMANTICS_HELIX`
- `vis->helix_select` for select/extend mode
- `vis->helix_repeat` for semantic dot-repeat (Phase 1: r/R only)
- All Helix key actions are `KEY_ACTION_FN` in `vis-helix.c`
- Helper functions extracted for repeat: `helix_replace_char_apply`, `helix_replace_with_yanked_apply`

## `.` Repeat Architecture
- `HelixRepeatKind`: `NONE`, `REPLACE_CHAR`, `REPLACE_WITH_YANKED`
- `vis->helix_repeat.kind` + `.data[]` + `.len` stores the repeat command
- `ka_helix_repeat` dispatches on kind, calls helper, falls back to `vis_repeat()` for NONE
- Standard vis operators clear `.kind = NONE` via `vis_do()`
- Helix actions clear `action_prev` via `action_reset()`

## Phase 2 Notes (selection + operator repeat)
When implementing, add to `HelixRepeatKind`:
```
HELIX_REPEAT_SELECTION_OPERATOR
```
With fields for selection kind (word-next, word-prev, etc.) and operator kind (delete, change, yank, shift).
`wd` records `selection=WORD_NEXT, operator=DELETE`. `.` replays: apply word-next selection, then delete it.

## Phase 3 Notes (surround/textobject repeat)
Add kinds: `HELIX_REPEAT_SURROUND_ADD`, `HELIX_REPEAT_SURROUND_DELETE`, `HELIX_REPEAT_SURROUND_REPLACE`.
Store paired chars in `.data[]`.

## Manual QA Checklist
See bottom of this file for the full checklist. Key areas to test after changes:
- Word motions with punctuation and counts
- `wd` / `wc` / `wy` operator chains
- `r` and `R` with multicursor
- `.` repeat after `r`, after standard operators (vis fallback)
- `gw` jump labels don't corrupt TUI on Escape/arrows
- `mm` with multiple cursors
- Goto mode: `gt/gc/gb/gh/gl/gs/gg/ge`

## Full Manual QA Checklist
- [ ] w/e/b — word motions
- [ ] 2w, 3e, 2b — counts
- [ ] wd, wc, wy — operator chains
- [ ] bare d/c/y — implicit 1-char
- [ ] v — select mode toggle
- [ ] vww, vbb — select extend
- [ ] ; — collapse
- [ ] x/X — line select
- [ ] f/t/F/T — find select
- [ ] * / n / N — search
- [ ] s/S/K/Alt-K — regex prompts
- [ ] C/Alt-C/,/Alt-, — multicursor
- [ ] ms/md/mr — surround
- [ ] mi/ma — textobjects
- [ ] r/R — replace char/yanked
- [ ] . — repeat (r, R, standard ops)
- [ ] mm — match bracket
- [ ] gw — goto word
- [ ] gt/gc/gb/gh/gl/gs/gg/ge — goto mode
- [ ] % — select all
- [ ] Ctrl-a/Ctrl-x — increment/decrement (not yet implemented)
- [ ] :set keymap vim / helix — profile switch
- [ ] Statusline: NOR/SEL/INS

## Picker Feature (Phase 1-3)

### Architecture
- `VIS_MODE_PICKER` — new vis mode with `input` callback for filter keystrokes
- `vis->picker` — state struct: filter string, items array, filtered/sorted results, selection index, scroll offset, callbacks
- `vis->picker_preview` — preview state: lines array, line count, cached path
- `vis-picker.c` — all picker logic (~580 lines)
- `ui_terminal.c` — `picker_draw()` called from `ui_draw()` after window rendering, before `blit()`
- Drawing via Cell buffer overlay (no raw curses calls)

### Key bindings (picker mode)
- `j`/`<Down>`/`<C-n>` — next item
- `k`/`<Up>`/`<C-p>` — previous item
- `<Enter>`/`<C-j>` — accept selection
- `<Escape>`/`<C-c>`/`<C-g>` — cancel
- `<Backspace>`/`<C-h>` — delete filter char
- `<C-w>` — delete filter word
- `<C-u>` — clear filter

### Helix keymaps
- `<Space>f` — file picker (current directory)
- `<Space>b` — buffer picker (open files)

### Fuzzy matching
- fzy-style scoring: consecutive bonus, word boundary bonus, start bonus, case match bonus, gap penalty
- Results sorted by score descending; ties broken by original order
- `strstr` fallback for empty filter (show all)

### Preview pane
- Shows first 32 lines of selected file on right side of picker
- Only loads when selection changes
- Caches path to avoid redundant file reads
- Cleaned up on picker close

