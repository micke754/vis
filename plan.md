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

### Picker parity branch
- Goal: bring the picker closer to pragmatic Helix parity and prepare it for first-party C LSP integration.
- Primary constraint: keep the picker usable and stable; if a task gets deep, record status here and move on.
- Out of scope for this branch: C LSP client, repeat Phase 2/3, tree-sitter, and large UI rewrites.

#### Target scope
- [x] Refactor picker from string-only entries to structured items (`label`, `path`, `line`, `column`, `kind`, `detail`, future payload).
- [x] Preserve current file and buffer picker behavior during the refactor.
- [x] Add generic picker action handling so future diagnostics/symbol/reference pickers can open locations without changing picker core.
- [x] Add Helix-like navigation polish: Tab/Shift-Tab, PageUp/PageDown, Home/End, Ctrl-u/Ctrl-d.
- [x] Add preview toggle (`Ctrl-t`).
- [x] Split file pickers into workspace-root picker and current-directory picker.
- [x] Add workspace root detection (`.git`, `.jj`, `.helix`, fallback cwd).
- [x] Add basic ignore rules for large/noisy directories (`.git`, `node_modules`, `target`, `build`, `dist`, `.cache`, `dependency`).
- [ ] Improve preview: binary/large-file fallback and selected-line centering done; syntax highlighting last.
- [x] Add jumplist picker if current state is accessible cleanly. Current-window jump only; no split/vsplit for jumplist items.
- [ ] Add changed-file picker if dirty file state is accessible cleanly. Current-window switch/open only.
- [ ] Add tests for lifecycle, filtering, navigation bindings, preview toggle, and preserved file/buffer behavior.

#### LSP preparation
- Picker item model should be able to represent future LSP diagnostics, document symbols, workspace symbols, references, and global search matches.
- LSP branch should be able to feed structured items into picker without another picker refactor.
- Formatter/LSP implementation remains a follow-up branch; current Lua plugin setup stays as bridge only.
- Status 2026-05-12: structured item model, workspace/current-directory file picker split, preview toggle, binary preview fallback, Helix-like navigation keys, and current/split/vsplit open modes are implemented. Split opens use `Alt-s` / `Alt-v` because terminals commonly reserve `Ctrl-s` / `Ctrl-v`. Syntax-highlighted preview is not done yet.
- Next implementation order: changed-file picker if clean, then syntax-highlighted preview attempt. If blocked, record status here and move on.

## Backlog

### `.` repeat Phase 2 — selection + operator follow-up
- Word-selection + operator repeat is implemented for `d/c/y/>/<`.
- `wd.` replays "select next word, delete" at the current position.
- Remaining Phase 2 scope: non-word selection transforms (`x`, find/search, line selections) if needed.
- Deferred for now; not a high-use feature.

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
- Status: deferred. Needs explicit command recording for multi-key surround/textobject actions, not key replay.

## Completed (since last plan update)

- `b` trailing-whitespace — already fixed in earlier commit
- `Ctrl-a` / `Ctrl-x` — increment/decrement number under cursor
- `A` / `I` — verified working with Helix semantics
- `o` / `O` — clears selections, then opens line
- `. ` repeat Phase 1 — semantic repeat for `r` and `R`

## Picker Feature (complete, hardening in progress)
- VIS_MODE_PICKER added to mode enum with `.leave = vis_picker_leave`
- Overlay UI drawn via Cell buffer after window rendering
- Fzy-style fuzzy matching with consecutive/word-boundary/start bonuses
- Real-time filter (printable chars append, Backspace/Ctrl-w/Ctrl-u edit)
- Arrow keys / Ctrl-n/Ctrl-p / j/k navigation; Enter/Escape accept/cancel
- File picker (`<Space>f`): recursive directory listing (depth=2) via opendir/readdir + stat()
- Buffer picker (`<Space>b`): lists open files, switches or opens in current window
- Preview pane: shows first 32 lines of selected file, cached by path
- Fixed: literal-space bindings (` f` / ` b`) for physical keypress
- Fixed: use-after-free of selection string (now strdup'd before item cleanup)
- Fixed: vis_window_change_file replaces current buffer (no unwanted splits)
- Fixed: redraw AFTER on_select callback (no visual artifacts)
- `vis_picker_leave` cleanup callback for external mode switches
- Tests: mode registration, `<Space>f` alias, literal ` f`, regression

## Next Priorities (ordered)

### 1. Picker polish ✅ COMPLETE / hardening active
- [x] Fix visual artifacts: redraw AFTER on_select callback
- [x] File picker: use `vis_window_change_file` instead of `vis_window_new` (prevent splits)
- [x] Buffer picker: use `vis_window_change_file` fallback instead of `vis_window_new`
- [x] Recursive directory search (depth=2) with `stat()` for portability
- [x] Fix use-after-free: strdup selection before freeing items (was opening blank files)
- [x] Fix depth decrement in recursive call (prevents infinite walk)
- [x] Audit and fix remaining picker lifecycle/memory/file-walk risks

### 2. Keymap profile segfault
- [x] Investigate crash/hang in `keymap-profile.lua` test
  - Root cause: Lua test harness reran the same test file on split-window `WIN_OPEN` and called `vis:exit()` from inside that nested event.
  - Fixed with one-shot test execution guard in `test/lua/visrc.lua`.
- [x] Refresh stale `keymap-profile.lua` assertions for current profile semantics

### 3. Backlog
- `. ` repeat Phase 2 follow-up: non-word selection transforms (`x`, find/search, line selections)
- `. ` repeat Phase 3: surround/textobjects (deferred; needs command recording for multi-key actions)
- View mode: `V` line-select (mapped), `Ctrl-v` block-select (deferred: no block selection mode in current core)
- Selection-content rotation (`Alt-(`/`Alt-)`) — mapped to existing selection content rotation actions

## Test Suites
All in `test/lua/`:
```
keymap-helix-{count,find,goto,insert,line,match,multicursor,operator,paste,profile,regression,repeat,replace,search,select,surround,textobj}.lua
```
Run: `cd test/lua && for t in keymap-helix-*.lua; do ./test.sh $t || exit 1; done`

## Build & Run
```sh
./configure --enable-lpeg-static
make -j2
./vis
```

- `configure` adds rpaths for absolute dependency library directories so raw `make` binaries can find TRE/Lua/termkey without `LD_LIBRARY_PATH`.
- Static LPeg is preferred; if no compatible static archive is found, bundled LPeg sources are compiled into `vis`.
