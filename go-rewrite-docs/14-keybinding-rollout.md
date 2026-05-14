# Keybinding Rollout

## Purpose

This document defines the order in which `wisp` should expose functionality through keybindings after the initial infrastructure exists.

The goal is to make the prototype usable quickly while using keybindings to validate the architecture: typed commands, selection-first editing, picker providers, LSP locations, AI surfaces, and deterministic tests.

## Assumptions

- `wisp` has a working command registry.
- `wisp` has `config.go` keymap tables.
- Commands can be tested directly without terminal input.
- Keymap tests can assert that key sequences dispatch command IDs.
- Picker, LSP, and AI functionality should be surfaced only after their command-level behavior is testable.

## Principles

- Keybindings expose commands; they are not command implementations.
- Add keybindings in stages that match the implementation milestones.
- Do not map a command until it has command-level tests.
- Prefer canonical key notation such as `space f`, `ctrl-n`, `alt-s`, and `escape`.
- Avoid terminal-reserved defaults like `ctrl-s` and `ctrl-v`.
- Keep early bindings close to Helix muscle memory where it helps usability.
- Delay full Helix parity until the editor proves core LSP, picker, and AI workflows.

## Canonical Key Notation

Use lowercase canonical strings in `config.go`.

Examples:

```text
space f
space g
ctrl-n
alt-s
shift-tab
escape
enter
backspace
page-down
```

Do not store separate bindings for literal space and `<Space>`. The parser may accept multiple user-facing forms later, but the internal representation should be one canonical sequence.

## Stage 1: Survival Editing

Expose enough functionality to open a file, edit, save, and exit.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `i` | `mode.insert` | normal | Enter insert mode at cursor. |
| `escape` | `mode.normal` | insert, picker, prompt | Global return/cancel behavior. |
| `h` | `move.char.prev` | normal | Basic left movement. |
| `j` | `move.line.down` | normal | Basic down movement. |
| `k` | `move.line.up` | normal | Basic up movement. |
| `l` | `move.char.next` | normal | Basic right movement. |
| `left` | `move.char.prev` | normal, insert | Arrow fallback. |
| `down` | `move.line.down` | normal, insert | Arrow fallback. |
| `up` | `move.line.up` | normal, insert | Arrow fallback. |
| `right` | `move.char.next` | normal, insert | Arrow fallback. |
| `0` | `move.line.start` | normal | Start of line. |
| `$` | `move.line.end` | normal | End of line. |
| `u` | `edit.undo` | normal | Safe editing loop. |
| `U` | `edit.redo` | normal | Early redo, matches current vis Helix profile. |
| `space w` | `edit.save` | normal | Save active buffer. |
| `space q` | `editor.quit` | normal | Quit editor or current window. |

Insert mode printable input should not be implemented as a keymap entry per character. The insert surface or mode handler should convert printable input events into `edit.insert-text` commands.

Acceptance tests:

- `i` enters insert mode.
- `escape` returns to normal mode.
- `h/j/k/l` dispatch movement commands.
- printable insert input changes buffer text.
- `space w` saves a dirty buffer.
- `space q` exits or requests quit.

## Stage 2: Selection-First Editing

Expose the Helix-like core editing loop.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `x` | `selection.line` | normal | Most important Helix primitive. |
| `X` | `selection.line-current` | normal | Optional variant once `x` is stable. |
| `;` | `selection.collapse` | normal, select | Clears selections to cursors. |
| `d` | `edit.delete-selection` | normal, select | Deletes selected ranges or implicit char. |
| `c` | `edit.change-selection` | normal, select | Delete then enter insert. |
| `y` | `edit.yank-selection` | normal, select | Yank selected ranges. |
| `p` | `edit.paste-after` | normal, select | Paste after selection/cursor. |
| `P` | `edit.paste-before` | normal, select | Paste before selection/cursor. |
| `i` | `mode.insert-before-selection` | normal | On selection, move to range start. |
| `a` | `mode.append-after-selection` | normal | On selection, move to range end. |
| `v` | `mode.select-toggle` | normal | Add after basic selection transforms are stable. |

Behavior requirements:

- Bare cursor destructive operators use an implicit one-character range.
- Anchored selections operate on selected ranges.
- Multi-selection text edits apply bottom-to-top.
- `i` and `a` must be selection-aware, not aliases for generic insert.
- `c` should create one undo transaction and then enter insert mode.

Acceptance tests:

- `x d` deletes current line.
- `x c` deletes current line and enters insert mode.
- `x y p` duplicates a line or pastes yanked content according to chosen paste semantics.
- `;` collapses selections and leaves normal mode active.
- `i` on a selection moves to selection start before insert.
- `a` on a selection moves to selection end before insert.

## Stage 3: Word, Find, And Goto Motions

Expose enough motion vocabulary to edit at normal speed.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `w` | `selection.word.next` | normal, select | Helix-style word selection transform. |
| `b` | `selection.word.prev` | normal, select | Previous word selection transform. |
| `e` | `selection.word.end-next` | normal, select | Select to word end. |
| `W` | `selection.WORD.next` | normal, select | Whitespace-delimited WORD. |
| `B` | `selection.WORD.prev` | normal, select | Previous WORD. |
| `E` | `selection.WORD.end-next` | normal, select | End of WORD. |
| `f` | `selection.find-char-right` | normal, select | Requires next key input. |
| `t` | `selection.till-char-right` | normal, select | Requires next key input. |
| `F` | `selection.find-char-left` | normal, select | Requires next key input. |
| `T` | `selection.till-char-left` | normal, select | Requires next key input. |
| `gg` | `move.file.start` | normal, select | Start of file. |
| `G` | `move.file.end` | normal, select | End of file. |
| `ge` | `move.file.end` | normal, select | Helix-style end of file alias. |
| `gh` | `move.line.begin` | normal, select | First character in line. |
| `gl` | `move.line.end` | normal, select | End of line. |
| `gs` | `move.line.first-nonblank` | normal, select | First nonblank. |
| `gt` | `move.viewport.top` | normal, select | Top visible line. |
| `gc` | `move.viewport.center` | normal, select | Center visible line. |
| `gb` | `move.viewport.bottom` | normal, select | Bottom visible line. |

Behavior requirements:

- Word motions should be selection transforms in normal mode, not only cursor moves.
- Forward `w` should include trailing whitespace if following the vis Helix semantics.
- Counts should be supported after single-step behavior is tested.
- Find/till commands should keep the pending character input out of the keymap table if possible.
- Select mode should extend existing selections instead of replacing them.

Acceptance tests:

- `wd` deletes the next word plus expected trailing whitespace.
- `bd` deletes/selects previous word according to chosen Helix semantics.
- `2w` selects across two words.
- `fa` selects through the next `a` inclusively.
- `ta` selects until before the next `a`.
- `gg`, `G`, `gh`, `gl`, and `gs` move or extend consistently.

## Stage 4: Picker And Workspace Navigation

Expose the infrastructure that validates the rewrite direction.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `space f` | `picker.files` | normal, select | Workspace-root file picker. |
| `space F` | `picker.files-current` | normal, select | Current-directory file picker. Optional but useful. |
| `space b` | `picker.buffers` | normal, select | Open buffer picker. |
| `space g` | `picker.changed` | normal, select | Git changes plus dirty buffers. |
| `space c` | `picker.commands` | normal, select | Command discovery. |
| `space j` | `picker.jumplist` | normal, select | Only after file-aware jumplist exists. |

Picker mode keys:

| Key | Command | Notes |
|---|---|---|
| `down`, `ctrl-n`, `tab` | `picker.next` | Move selection down. |
| `up`, `ctrl-p`, `shift-tab` | `picker.prev` | Move selection up. |
| `page-down`, `ctrl-d` | `picker.page-next` | Page down. |
| `page-up`, `ctrl-u` | `picker.page-prev` | Page up. |
| `home` | `picker.first` | First item. |
| `end` | `picker.last` | Last item. |
| `enter`, `ctrl-j` | `picker.accept` | Accept item. |
| `escape`, `ctrl-c`, `ctrl-g` | `picker.cancel` | Cancel picker. |
| `backspace`, `ctrl-h` | `picker.backspace` | Delete filter char. |
| `ctrl-w` | `picker.delete-word` | Delete filter word. |
| `ctrl-t` | `picker.toggle-preview` | Toggle preview. |

Behavior requirements:

- File and buffer items may support split modes later.
- Location items should open current window first unless split semantics are explicitly designed.
- `space g` must not mean only dirty buffers.
- Empty picker sources should show messages like `No changed files` or `No diagnostics`.
- Picker tests must use fake providers.

Acceptance tests:

- `space f` opens file picker with fake filesystem items.
- `space b` opens buffer picker.
- `space g` opens changed picker from fake git plus dirty buffers.
- picker filtering is deterministic.
- picker cancel restores prior mode.
- picker accept dispatches item action.

## Stage 5: Search, Regex, And Multi-Selection

Expose higher-leverage selection transforms.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `/` | `search.forward-prompt` | normal, select | Prompt-backed search. |
| `?` | `search.backward-prompt` | normal, select | Prompt-backed search. |
| `n` | `search.next` | normal, select | Select next match in Helix style. |
| `N` | `search.prev` | normal, select | Select previous match. |
| `*` | `search.selection-or-word` | normal, select | Escape regex specials and save search. |
| `s` | `selection.select-regex-prompt` | normal, select | Select matches inside selections. |
| `S` | `selection.split-regex-prompt` | normal, select | Split selections around matches. |
| `K` | `selection.keep-regex-prompt` | normal, select | Keep matching selections. |
| `alt-K` | `selection.remove-regex-prompt` | normal, select | Remove matching selections. |
| `C` | `selection.add-line-below` | normal, select | Add cursor below. |
| `alt-C` | `selection.add-line-above` | normal, select | Add cursor above. |
| `,` | `selection.keep-primary` | normal, select | Remove all but primary. |
| `alt-,` | `selection.remove-primary` | normal, select | Remove primary selection. |
| `&` | `selection.align` | normal, select | Optional after multicursor edits work. |

Behavior requirements:

- Prompt surface should carry typed continuation commands.
- Empty regex prompt can fall back to search register if desired.
- Invalid regex should show a visible message.
- Regex selection transforms should be unit-testable without terminal prompt input.
- Multi-selection edits must apply bottom-to-top.

Acceptance tests:

- `*` stores selected text or word under cursor as escaped search pattern.
- `n` selects the next match range.
- `s` creates selections for all regex matches in current selections.
- `S` splits a selected range around regex matches.
- `K` keeps matching selections.
- `alt-K` removes matching selections.
- `C` and `alt-C` create new selections without corrupting primary state.

## Stage 6: LSP Surfacing

Expose LSP through location and diagnostics UI.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `space d` | `picker.diagnostics` | normal, select | Diagnostics picker. |
| `gd` | `lsp.definition` | normal, select | Jump direct or picker for multiple locations. |
| `gr` | `lsp.references` | normal, select | References picker. |
| `space s` | `lsp.document-symbols` | normal, select | Symbol picker; avoid conflict with `gs`. |
| `space a` | `picker.code-actions` | normal, select | Code actions if implemented. |

Behavior requirements:

- LSP commands should show `No LSP server for this buffer` when unavailable.
- Location picker items should use common `Location` types.
- Multiple LSP clients should include source/client in item detail where needed.
- LSP responses should arrive as typed events.

Acceptance tests:

- fake diagnostics open diagnostics picker.
- accepting diagnostic jumps to file and range.
- fake references response opens references picker.
- single definition response can jump directly.
- no-server commands show messages.

## Stage 7: AI Surfacing

Expose AI only after cancellation and preview/apply infrastructure works.

| Key | Command | Mode | Notes |
|---|---|---|---|
| `space A` | `picker.ai-actions` | normal, select | Safest first entry point. |
| `space ai` | `ai.complete-selection` | normal, select | If multi-key sequences are supported cleanly. |
| `space af` | `ai.fix-diagnostic` | normal, select | Later, after diagnostics are stable. |
| `escape` | `ai.cancel` | ai-preview | Cancel active AI surface/request. |
| `enter` | `ai.accept` | ai-preview | Apply previewed result. |
| `ctrl-c` | `ai.reject` | ai-preview | Reject result. |

Behavior requirements:

- No real network request unless AI provider is configured.
- AI suggestions should be previews or ghost text, not buffer mutations before accept.
- Fast typing should debounce requests and cancel stale requests.
- Every AI request should have a job ID and cancellation context.

Acceptance tests:

- AI provider missing shows message.
- fake AI provider streams preview text.
- cancel stops provider and closes preview or marks request canceled.
- accept applies result in one undoable transaction.
- reject leaves buffer unchanged.

## Stage 8: Advanced Helix Parity

Add after core workflow and future-facing features are proven.

| Key | Command | Notes |
|---|---|---|
| `r` | `edit.replace-selection-with-char` | Needs next-key capture and semantic repeat. |
| `R` | `edit.replace-selection-with-register` | Requires register slot behavior. |
| `.` | `edit.repeat` | Must be semantic repeat, not key replay. |
| `%` | `selection.file` | Select whole file. |
| `mm` | `move.match-bracket` | Bracket matching. |
| `mi` | textobject inner prefix | Textobjects. |
| `ma` | textobject outer prefix | Textobjects. |
| `ms` | surround add prefix | Surround add. |
| `md` | surround delete prefix | Surround delete. |
| `mr` | surround replace prefix | Surround replace. |
| `ctrl-a` | `edit.increment-number` | Avoid if terminal conflict appears. |
| `ctrl-x` | `edit.decrement-number` | Usually safe. |
| `gw` | `ui.goto-word-labels` | Visible word labels. |

Behavior requirements:

- `.` repeat should replay semantic command records.
- `r` should repeat the replacement character across selected graphemes.
- Surround and textobject prefixes should be data-driven in config.go.
- `gw` requires an overlay state and two-key label input.

Acceptance tests:

- `rX` replaces selected graphemes with `X`.
- `.` repeats `rX` without replaying raw keys.
- `R` replaces selections with register content.
- `mm` jumps to matching bracket or stays put if none.
- `ms)` surrounds selection with parentheses.
- `md)` deletes surrounding parentheses.

## Suggested `config.go` Shape

Use staged keymaps in config so incomplete commands are not exposed by accident.

```go
var Default = Config{
    Keymaps: KeymapConfig{
        Normal: map[string]string{
            "i": "mode.insert-before-selection",
            "a": "mode.append-after-selection",
            "escape": "mode.normal",
            "h": "move.char.prev",
            "j": "move.line.down",
            "k": "move.line.up",
            "l": "move.char.next",
            "x": "selection.line",
            ";": "selection.collapse",
            "d": "edit.delete-selection",
            "c": "edit.change-selection",
            "u": "edit.undo",
            "U": "edit.redo",
            "space f": "picker.files",
            "space b": "picker.buffers",
            "space g": "picker.changed",
            "space c": "picker.commands",
            "space d": "picker.diagnostics",
        },
        Picker: map[string]string{
            "down": "picker.next",
            "ctrl-n": "picker.next",
            "tab": "picker.next",
            "up": "picker.prev",
            "ctrl-p": "picker.prev",
            "enter": "picker.accept",
            "escape": "picker.cancel",
            "backspace": "picker.backspace",
            "ctrl-t": "picker.toggle-preview",
        },
    },
}
```

The exact map value can be a command ID string early. Later it can become a typed invocation structure when arguments are needed.

## Do Not Bind Yet

Do not bind these until their behavior is implemented and tested:

- `ctrl-s`
- `ctrl-v`
- block selection keys
- macro recording
- plugin commands
- tree-sitter commands
- direct AI network commands without preview/cancel support

## Rollout Checklist

Before adding any keybinding:

- command exists
- command has direct tests
- command has a visible error path if it can no-op
- keymap resolve test exists
- mode interaction is clear
- docs mention any terminal caveat

## Final Recommendation

Expose keybindings in the same order as the prototype risk:

1. editing loop
2. selection-first operators
3. word and find motions
4. picker and workspace navigation
5. search and multi-selection transforms
6. LSP pickers and jumps
7. AI actions through preview/cancel/apply
8. advanced Helix parity

This order makes the editor useful quickly while proving the architecture that motivated the rewrite.
