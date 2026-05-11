# Helix Mode Work Plan (feature/helix-mode)

## Goal
Native-feeling Helix editing mode in vis, toggled via `:set keymap helix` / `vim`.

## Architecture Principles
- C owns semantics (motions, selections, operators, ranges, repeat)
- Lua owns profile/mapping glue
- Vim behavior unchanged when `selectionsemantics=vim`
- Semantic repeat (`. `) — helpers callable by `.` without key replay

## Completed

### Core state & profile
- `:set keymap helix` / `:set keymap vim` toggle
- `vis->selection_semantics`, `vis->helix_select`
- Statusline: NOR / SEL / INS
- Helix is default keymap; no visrc.lua needed

### C-backed Helix semantics
- Word motions: `w/e/b/W/E/B` with counts, select mode
- Operators: `d/c/y/>/<` consume anchored selections
- Select mode: `v` toggles, `<Escape>` exits, `;` collapses
- Line selection: `x/X` with counts
- Paste: `p/P` with multicursor slot cycling
- Find: `f/t/F/T` select-mode aware
- Search: `*` sets pattern, `n/N` select match width
- Regex prompts: `s/S/K/Alt-K` with search-register fallback
- Multicursor: `C/Alt-C/,/Alt-,/Y/&`
- `Alt-s` split selections on newlines
- `%` select entire file
- `(`/`)` rotate primary selection
- Surround: `ms/md/mr` for all bracket types
- Textobjects: `mi/ma` for word/WORD/paragraph/quotes/brackets
- `mm` match bracket (multicursor-safe)
- `gw` goto-word with 2-char jump labels
- `r` replace char, `R` replace with yanked (multicursor-safe)
- `i`/`a` Helix insert/append (collapse to selection edges)
- `gw` key label special-key fix (consumes full key tokens)
- Goto mode: `gg/ge/gh/gl/gs/gt/gc/gb`
- `mm` multicursor fix (iterates all selections)
- `b` motion: selects destination word + trailing space only

### `.` repeat (Phase 1)
- Semantic repeat: `HelixRepeatKind` enum in `vis-core.h`
- `r` and `R` refactored into `helix_replace_char_apply` / `helix_replace_with_yanked_apply`
- `.` calls `ka_helix_repeat` which dispatches on `kind`:
  - `REPLACE_CHAR` → replays `r` with saved char
  - `REPLACE_WITH_YANKED` → replays `R`
  - `NONE` → falls back to `vis_repeat()` for standard ops
- Standard vis operators clear `helix_repeat.kind = NONE`
- Helix actions clear `action_prev`

## In Progress

### `.` repeat Phase 2 — selection + operator
- `wd.` should replay "select next word, delete"
- Requires recording selection transform + operator
- Storage: `HELIX_REPEAT_SELECTION_OPERATOR` with:
  - `selection`: enum (word-next, word-prev, word-end, line, etc.)
  - `operator`: enum (delete, change, yank, shift-left, shift-right)
  - `count`: int
- `.` replays: apply selection transform at current position, then operator

### `.` repeat Phase 3 — surround / textobjects
- `ms<char>`, `md<char>`, `mr<from><to>` repeat
- `mi<object>`, `ma<object>` + operator repeat
- Storage: `HELIX_REPEAT_SURROUND_ADD`, etc. with paired chars

## Completed (since last plan update)

- `b` trailing-whitespace — already fixed in earlier commit
- `Ctrl-a` / `Ctrl-x` — increment/decrement number under cursor
- `A` / `I` — verified working with Helix semantics
- `o` / `O` — clears selections, then opens line
- `. ` repeat Phase 1 — semantic repeat for `r` and `R`

## In Progress

### File picker (complete ✅)
- VIS_MODE_PICKER added to mode enum
- Overlay UI drawn via Cell buffer after window rendering
- Fzy-style fuzzy matching with consecutive/word-boundary/start bonuses
- Real-time filter (printable chars append, Backspace/Ctrl-w/Ctrl-u edit)
- Arrow keys / Ctrl-n/Ctrl-p / j/k navigation
- Enter to select, Escape to cancel
- File picker (`<Space>f`): recursive directory listing (depth=2)
- Buffer picker (`<Space>b`): list open files, switches or opens in current window
- Preview pane: shows first 32 lines of selected file
- Literal-space bindings (` f` / ` b`) for physical keypress fixed
- `vis_picker_leave` cleanup callback for external mode switches
- Tests: mode registration, `<Space>f` alias, literal ` f`, regression

## Next Priorities (ordered)

### 1. Picker polish (current)
- [x] Fix visual artifacts: redraw AFTER on_select callback
- [x] File picker: use `vis_window_change_file` instead of `vis_window_new` (prevent splits)
- [x] Buffer picker: use `vis_window_change_file` fallback instead of `vis_window_new`
- [x] Recursive directory search (depth=2) with `stat()` for portability
- [ ] Handle absolute/relative path resolution in buffer picker lookup
- [ ] Show file icons or type indicators

### 2. Keymap profile segfault
- [ ] Investigate crash in `keymap-profile.lua` test
- [ ] Fix split-window profile lifecycle (WIN_OPEN handler)

### 3. Backlog
- `. ` repeat Phase 2: selection+operator (`wd.`)
- `. ` repeat Phase 3: surround/textobjects
- View mode: `V` line-select, `Ctrl-v` block-select
- Selection-content rotation (`Alt-(`/`Alt-)`)

## Test Suites
All in `test/lua/`:
```
keymap-helix-{count,find,goto,insert,line,match,multicursor,operator,paste,profile,regression,repeat,replace,search,select,surround,textobj}.lua
```
Run: `cd test/lua && for t in keymap-helix-*.lua; do LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh $t || exit 1; done`

## Build & Run
```sh
make -j2
LD_LIBRARY_PATH=./dependency/install/usr/lib ./vis
```
