# vis Helix Reference

## Purpose

This document analyzes the existing Helix implementation in vis so it can be used as a reference point for the Go rewrite.

The goal is not to copy the C/Lua structure. The goal is to preserve the behavior that worked, avoid the traps we found, and map useful concepts to typed Go commands and state.

## Source Files

| File | Role |
|---|---|
| `lua/keymaps/helix.lua` | Defines Helix profile mappings for normal, visual, visual-line, and insert modes. |
| `lua/keymaps/init.lua` | Applies keymap profiles per window, shadows existing mappings, and toggles selection semantics. |
| `main.c` | Registers key actions such as `vis-helix-line-select` and `vis-picker-files`. |
| `vis-core.h` | Stores Helix state such as `helix_select`, prompt state, repeat state, and picker state. |
| `vis.c` | Main command execution path. Integrates Helix movement, operators, repeat capture, and jumplist triggers. |
| `vis-helix.c` | Implements most C-backed Helix behavior. |
| `vis-picker.c` | Implements picker mode, picker actions, and picker UI. |
| `vis-prompt.c` | Routes prompt input to search, command execution, or Helix regex selection commands. |

## Architecture Summary

The vis implementation has three layers:

| Layer | Description | Go Rewrite Lesson |
|---|---|---|
| Lua keymap profile | Maps key strings to C action names. | Use typed `config.go` keymaps instead of separate scripting layer. |
| Key action registry | Registers C action names and arguments. | Use command registry with metadata and typed invocations. |
| C behavior | Mutates editor, view, selection, picker, prompt, and repeat state. | Keep behavior in Go commands and surfaces with explicit state ownership. |

This split worked, but it made small features cross too many files. In `wisp`, the equivalent should be a single language path:

```text
key sequence -> keymap resolve -> command invocation -> command execute -> editor state update -> render
```

## Keymap Manager

`lua/keymaps/init.lua` maintains a current profile, applies it to every window, and shadows default mappings.

Important details:

- Managed modes are normal, insert, visual, and visual-line.
- Existing mappings are shadowed with `<vis-nop>` so legacy defaults do not leak through.
- Profile application is per-window.
- `WIN_OPEN` reapplies the profile to new windows.
- `:set keymap helix` switches to Helix and sets `selectionsemantics helix`.
- `:set keymap vim` clears overlay mappings and sets `selectionsemantics vim`.

Go rewrite guidance:

- Do not implement overlay shadowing in the prototype.
- There is no legacy Vim keymap to shadow.
- Use one keymap table per mode in `config.go`.
- Window-local keymaps can come later if a concrete use case appears.

## Mode Tables

`helix.lua` defines four mode tables.

| Mode Table | Purpose | Go Equivalent |
|---|---|---|
| `normal` | Main movement, selection, picker, operators, prompts, counts. | `ModeNormal` keymap. |
| `visual` | Existing vis visual mode adapted toward Helix selection behavior. | Prefer explicit `ModeSelect`. |
| `visual_line` | Existing vis visual-line mode adapted for line selections. | Prefer line selection state, not separate mode initially. |
| `insert` | Insert-mode editing and movement. | `ModeInsert` keymap plus printable input handler. |

Go rewrite guidance:

- Use `normal`, `insert`, `picker`, and possibly `select`.
- Avoid inheriting Vim visual mode concepts unless needed.
- Picker and prompt should be surfaces that own input while active.

## Normal Mode Categories

The normal mode table in `helix.lua` includes:

- escape and command prompt
- character and line movement
- word movement
- find/till movement
- goto commands
- viewport movement
- page movement
- jumplist
- window management
- write and quit
- selection mode and line selection
- insert and append
- open line
- repeat
- number increment/decrement
- pickers
- replace and replace-with-yanked
- undo and redo
- yank, paste, delete, change, indent, outdent
- multicursor operations
- select all
- bracket matching
- visible word labels
- search and regex selection transforms
- registers
- count keys

Go rewrite guidance:

- Do not expose all categories at once.
- Use `14-keybinding-rollout.md` ordering.
- Build command-level tests before binding each category.

## Insert Mode Reference

vis insert mode maps:

| Key | Behavior |
|---|---|
| `escape` | Return to normal. |
| `ctrl-h`, `backspace` | Delete previous char. |
| `delete`, `ctrl-d` | Delete next char. |
| `ctrl-w` | Delete previous word. |
| `ctrl-u` | Delete to line start. |
| `ctrl-k` | Delete to line end through key replay. |
| `enter`, `ctrl-j` | Insert newline. |
| `tab` | Insert tab. |
| arrows, home, end, page keys | Move while remaining in insert. |

Go rewrite guidance:

- Implement printable insertion outside static keymap entries.
- Prefer direct commands over key replay for `ctrl-k`.
- Keep movement-in-insert optional but useful.

## Selection State

vis uses two related concepts:

- `selectionsemantics=helix`
- `vis->helix_select`

Important behavior:

- Helix semantics make operators act on selected ranges first.
- `helix_select` controls whether movements extend current selections.
- `v` toggles `helix_select` and anchors selections.
- `;` clears selections and turns off `helix_select`.
- Normal movement can clear anchored selections when not in select mode.

Reference implementation:

- `ka_helix_select_toggle` toggles select mode and anchors selections.
- `ka_helix_collapse` clears each selection to its cursor.
- `vis_do` checks Helix semantics during movement and operator execution.

Go rewrite guidance:

- Prefer explicit `ModeSelect` or selection behavior in command context.
- Do not use hidden global selection flags if command context can be explicit.
- Selection transforms should be pure or mostly pure functions over `selection.Set` and buffer text.

## Line Selection

vis implements `x` and `X` through `ka_helix_line_select`.

Behavior:

- Selects current line by expanding to line boundaries.
- Honors counts.
- If already anchored and using the extending variant, expands from current selection line boundaries.
- Sets selections anchored.
- Clears count after applying.

Go rewrite mapping:

| vis Action | wisp Command |
|---|---|
| `vis-helix-line-select` | `selection.line` |
| `vis-helix-line-select-current` | `selection.line-current` |

Implementation notes:

- Implement `selection.line` early.
- Count support can follow once single-line behavior is stable.
- Tests should cover final newline and last-line edge cases.

## Selection Collapse

vis `;` maps to `ka_helix_collapse`.

Behavior:

- Turns off `helix_select`.
- For every selection, captures cursor position.
- Clears selection.
- Restores cursor to captured position.

Go rewrite mapping:

| vis Action | wisp Command |
|---|---|
| `vis-helix-collapse-selection` | `selection.collapse` |

Implementation notes:

- Collapse should be idempotent.
- Collapse should not alter buffer text.

## Word Motion Semantics

vis special-cases Helix word movement inside `vis_do` and `vis-helix.c`.

Key helpers:

- `helix_word_movement`
- `helix_word_range`
- `helix_word_at`
- `helix_word_next`
- `helix_word_prev`
- `helix_word_include_space`
- `helix_visual_word_prepare`
- `helix_visual_word_adjust`
- `helix_word_after_motion`

Behavior:

- `w`, `b`, `e`, `W`, `B`, `E` become selection transforms in Helix normal mode.
- Forward word-start motion includes trailing whitespace unless it is an end motion.
- Backward movement selects the previous word and can include trailing whitespace for symmetry.
- Anchored selections in select mode extend from existing selection boundaries.
- Counts repeat the word selection transform.
- Visual/select behavior required extra adjustment because cursor position and selected range are not the same thing.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `w` | `selection.word.next` |
| `b` | `selection.word.prev` |
| `e` | `selection.word.end-next` |
| `W` | `selection.WORD.next` |
| `B` | `selection.WORD.prev` |
| `E` | `selection.WORD.end-next` |

Implementation notes:

- Implement as selection transforms, not raw cursor movement.
- Write tests before binding operators like `wd`.
- Decide and document whether `w` includes trailing whitespace. If preserving vis behavior, it should.

## Find And Till Motions

vis maps:

- `f` to next char inclusive
- `t` to next char exclusive/till
- `F` to previous char inclusive
- `T` to previous char exclusive/till

Key helper:

- `helix_find_select`

Behavior:

- Movement range is converted into a selection.
- Inclusive movement includes the target char.
- Backward movement uses directed selection.
- Select mode extends existing selection boundaries.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `f<char>` | `selection.find-char-right` |
| `t<char>` | `selection.till-char-right` |
| `F<char>` | `selection.find-char-left` |
| `T<char>` | `selection.till-char-left` |

Implementation notes:

- Model these as commands that enter a one-key pending input state or accept the next input event.
- Do not encode every possible `f<char>` pair in the keymap.

## Search Semantics

vis maps:

- `/` to search forward prompt
- `?` to search backward prompt
- `n` to repeat forward
- `N` to repeat backward
- `*` to search current selection or word

Reference helpers:

- `ka_helix_search_word`
- `helix_search_escape_append`
- `helix_search_repeat`
- `helix_search_match_range`

Behavior:

- `*` uses selected text if anchored, otherwise word under cursor.
- Regex special characters are escaped before storing the search pattern.
- Pattern is stored in search register.
- `n` and `N` select match ranges in Helix mode, rather than only moving cursor.
- In select mode, search repeat can create or extend selections.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `/` | `search.forward-prompt` |
| `?` | `search.backward-prompt` |
| `n` | `search.next` |
| `N` | `search.prev` |
| `*` | `search.selection-or-word` |

Implementation notes:

- Search state should include pattern, direction, and last match if needed.
- Prompt should invoke typed continuation commands.
- Search commands should return ranges.

## Regex Selection Prompts

vis maps:

- `s` to select regex matches
- `S` to split selections by regex matches
- `K` to keep selections matching regex
- `alt-K` to remove selections matching regex

Reference helpers:

- `ka_helix_regex_prompt`
- `prompt_helix_select_regex`
- `prompt_helix_split_regex`
- `prompt_helix_keep_remove_regex`
- `prompt_helix_pattern_resolve`
- `prompt_helix_regex`

Behavior:

- Prompt mode is opened with `vis->helix_prompt` set to a routing enum.
- Empty prompt falls back to the search register.
- Non-empty prompt saves the regex to the search register.
- Invalid regex reports `Invalid regex`.
- `s` replaces selections with every match inside each selected range.
- `S` replaces selections with non-matching ranges split around regex matches.
- `K` keeps only selections that match.
- `alt-K` removes selections that match.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `s` | `selection.select-regex-prompt` |
| `S` | `selection.split-regex-prompt` |
| `K` | `selection.keep-regex-prompt` |
| `alt-K` | `selection.remove-regex-prompt` |

Implementation notes:

- Avoid a global prompt enum if possible.
- Prompt surface should carry `onSubmit` command invocation and args.
- Regex transforms should be unit-testable by calling commands directly with pattern args.

## Operators

vis integrates Helix operators in `vis_do`.

Key helpers:

- `helix_operator_context`
- `helix_put_context`

Behavior:

- If selection is anchored, operator range is selected range.
- If selection is not anchored, operator range is one character under cursor.
- Paste adjusts position relative to selected range.
- Multi-cursor register slots are selected by selection number.
- After destructive operators, selections generally collapse.
- Yank is special: it does not clear the selection in the same way as delete/change.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `d` | `edit.delete-selection` |
| `c` | `edit.change-selection` |
| `y` | `edit.yank-selection` |
| `p` | `edit.paste-after` |
| `P` | `edit.paste-before` |
| `>` | `edit.indent-selection` |
| `<` | `edit.outdent-selection` |

Implementation notes:

- Do not start with Vim operator-pending mode.
- Implement direct selection-first commands.
- Bottom-to-top edit ordering is mandatory for multi-selection edits.

## Insert And Append

vis maps:

- `i` to `ka_helix_insert`
- `a` to `ka_helix_append`
- `I` to line-start insert
- `A` to line-end append

Behavior:

- `i` on anchored selection clears selection and moves cursor to range start before insert.
- `i` on bare cursor stays at cursor.
- `a` on anchored selection clears selection and moves cursor to range end before insert.
- `a` on bare cursor moves one character right before insert.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `i` | `mode.insert-before-selection` |
| `a` | `mode.append-after-selection` |
| `I` | `mode.insert-line-start` |
| `A` | `mode.append-line-end` |

Implementation notes:

- Do not alias `i` directly to generic `mode.insert`.
- The selection-aware behavior is core Helix feel.

## Open Line

vis maps:

- `o` to open line below
- `O` to open line above

Reference helper:

- `ka_helix_openline`

Behavior:

- Clears selections first in Helix mode.
- Switches to insert mode.
- For below, moves to line end and feeds enter.
- For above, moves to line start or first nonblank depending on autoindent behavior, feeds enter, then moves up.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `o` | `edit.open-line-below` |
| `O` | `edit.open-line-above` |

Implementation notes:

- Implement directly, not via key replay.
- Make autoindent explicit later.

## Replace Commands

vis maps:

- `r` to replace selected content with a character
- `R` to replace selected content with yanked/register content

Reference helpers:

- `helix_replace_char_apply`
- `ka_helix_replace_char`
- `helix_replace_with_yanked_apply`
- `ka_helix_replace_with_yanked`

Behavior:

- `r` captures the next key as replacement character.
- Escape cancels replacement.
- Anchored selections are replaced over their selected ranges.
- Bare cursor replaces one character.
- Replacement preserves selected grapheme count by repeating replacement character.
- `R` only acts on anchored selections.
- `R` uses register slots by selection number when multiple cursors exist.
- Both store semantic repeat records.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `r<char>` | `edit.replace-selection-with-char` |
| `R` | `edit.replace-selection-with-register` |

Implementation notes:

- Requires next-key capture.
- Requires repeat metadata.
- Should wait until registers and repeat infrastructure exist.

## Repeat

vis maps `.` to `ka_helix_repeat`.

Reference state:

- `HELIX_REPEAT_NONE`
- `HELIX_REPEAT_REPLACE_CHAR`
- `HELIX_REPEAT_REPLACE_WITH_YANKED`
- `HELIX_REPEAT_SELECTION_OPERATOR`

Behavior:

- `r` repeats by storing replacement character bytes.
- `R` repeats by storing replace-with-yanked kind.
- Word-selection plus operator can store selection transform, operator, and count.
- Falls back to standard repeat when no Helix semantic repeat exists.

Go rewrite guidance:

- Implement repeat as semantic command records.
- Do not replay raw key strings for `.`.
- Repeat records should store command ID, args, count, and optional selection transform.

## Surround

vis maps surround prefixes in Lua:

- `ms<char>` surround add
- `md<char>` surround delete
- `mr<from><to>` surround replace

Reference helpers:

- `ka_helix_surround_add`
- `ka_helix_surround_delete`
- `helix_surround_inner`
- Lua-generated `surround_replace` combinations

Behavior:

- Add surrounds every selection, iterating from last to first.
- Empty/bare cursor selection becomes one character.
- Delete finds inner text object for delimiter pair and deletes surrounding chars.
- Replace is generated as delete plus add in Lua by concatenating command strings.

Go rewrite mapping:

| Prefix | wisp Command Pattern |
|---|---|
| `ms<char>` | `surround.add` with delimiter arg |
| `md<char>` | `surround.delete` with delimiter arg |
| `mr<from><to>` | `surround.replace` with from/to args |

Implementation notes:

- Do not generate command strings by concatenation.
- Use pending-key command state with typed args.
- Add after textobjects are stable.

## Textobjects

vis maps:

- `mi` prefix for inner textobjects
- `ma` prefix for outer textobjects

Textobjects include:

- word
- WORD
- paragraph
- parentheses
- brackets
- braces
- angle brackets
- quotes
- single quotes
- backticks

Reference helper:

- `helix_text_object`

Behavior:

- Finds textobject around cursor or selection.
- Counts can extend object selection.
- Outer delimited objects include delimiters.
- Direction and extension rules depend on textobject flags.

Go rewrite mapping:

| Prefix | wisp Command Pattern |
|---|---|
| `mi<object>` | `selection.textobject-inner` |
| `ma<object>` | `selection.textobject-outer` |

Implementation notes:

- Start with word and bracket pairs only.
- Add textobject flags only when needed.

## Multicursor And Selection Management

vis maps:

- `C` add selection below
- `alt-C` add selection above
- `,` keep primary selection
- `alt-,` remove primary selection
- `&` align selections
- `(` and `)` rotate primary selection
- `alt-(` and `alt-)` rotate selection contents
- `alt-s` split selections on newlines

Reference helpers:

- `ka_helix_rotate_selection`
- `ka_helix_select_split_lines`
- existing selection actions in `main.c`

Behavior:

- Primary selection is distinct and can be rotated.
- Selections can be split by lines.
- Selection content rotation is separate from primary rotation.

Go rewrite guidance:

- Implement primary selection as explicit index in `selection.Set`.
- Add line splitting before content rotation.
- Keep multi-selection mutations pure and testable.

## Select All

vis maps `%` to `ka_helix_select_all`.

Behavior:

- Sets one anchored selection from byte 0 to text size.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `%` | `selection.file` |

Implementation notes:

- Easy to add, but not critical before picker/LSP workflows.

## Match Bracket

vis maps `mm` to `ka_helix_match_bracket`.

Behavior:

- Tries to match bracket at cursor.
- If not at bracket, scans backward for a bracket and tries match.
- Clears selection and moves cursor to match if found.
- Supports `(){}[]<>` plus quotes and backticks.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `mm` | `move.match-bracket` |

Implementation notes:

- Add after bracket matching utility exists.
- Quotes are harder than bracket pairs; they can come later.

## Visible Word Labels

vis maps `gw` to `ka_helix_goto_word`.

Behavior:

- Collects visible word starts in viewport.
- Ignores tiny or non-alphanumeric words.
- Assigns two-letter labels from `aa` through `zz`.
- Stores labels in view state.
- Sets `jump_labels_active` so the next two keys select a label.
- Key processing in `vis_keys_process` handles active label input before normal keymap resolution.
- On match, selects the target word for every selection.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `gw` | `ui.goto-word-labels` |

Implementation notes:

- This should be an overlay or surface, not a global key-processing special case.
- Add only after render overlays and pending input states are stable.

## Viewport Goto

vis maps:

- `gt` to top of viewport
- `gc` to center of viewport
- `gb` to bottom of viewport

Reference helper:

- `ka_helix_goto_viewport`

Behavior:

- Top, center, bottom target resolves to first nonblank of target visible line.
- In select mode, extends selection.
- Otherwise moves cursor and clears anchor.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `gt` | `move.viewport.top` |
| `gc` | `move.viewport.center` |
| `gb` | `move.viewport.bottom` |

## Number Increment And Decrement

vis maps:

- `ctrl-a` increment number
- `ctrl-x` decrement number

Reference helpers:

- `helix_find_number`
- `ka_helix_increment`

Behavior:

- Finds number under or near cursor.
- Supports negative numbers and decimal parse but writes integer result.
- Count controls delta.
- Updates each selection.
- Clears selection and moves cursor to end of new number.

Go rewrite mapping:

| Key | wisp Command |
|---|---|
| `ctrl-a` | `edit.increment-number` |
| `ctrl-x` | `edit.decrement-number` |

Implementation notes:

- `ctrl-a` may conflict less often than `ctrl-s`, but keep terminal behavior in mind.
- Add after core edit operations are stable.

## Picker Reference

vis picker evolved from string-only items to structured items.

Important behavior:

- Picker mode owns filter, selected index, scroll offset, preview visibility, items, filtered items, and callback.
- Picker item contains label, path, detail, preview path, line, column, and kind.
- File picker and buffer picker support current/split/vsplit open modes.
- Location pickers should be current-window only unless explicitly designed.
- Preview cache must include selected item and location data.
- `ctrl-s` and `ctrl-v` were avoided for split/vsplit due terminal conflicts; `alt-s` and `alt-v` were used instead.

Go rewrite guidance:

- Start with typed picker items and providers.
- Picker state should be a surface, not global editor fields.
- Accept action should be a typed command invocation.
- Provider tests must not use real repo state.

## Changed Files Reference

Original issue:

- The initial changed-file picker listed dirty open buffers only.
- User expected unstaged git working-tree changes.

Go rewrite guidance:

- `picker.changed` should mean git working-tree changes plus dirty buffers.
- The git provider must be injectable.
- Empty result should show `No changed files`.

## Jumplist Reference

Original issue:

- vis jumplist storage was based on selection regions restored against the current view.
- This failed for cross-file picker previews and jumps.

Go rewrite guidance:

- Store jumplist entries as file-aware `Location` values from the start.
- Do not derive cross-file locations from view-local marks.

## Terminal Key Pitfalls

Observed pitfalls:

- `ctrl-s` can freeze terminal flow control.
- `ctrl-v` can be terminal literal-next.
- `ctrl-i` can be indistinguishable from tab.
- literal space bindings and `<Space>` aliases caused confusion in vis tests.

Go rewrite guidance:

- Canonicalize key notation to `space x` style.
- Avoid `ctrl-s` and `ctrl-v` default bindings.
- Treat advanced terminal protocols as later enhancements.

## Testing Lessons

vis tests proved valuable but were vulnerable to real environment state.

Observed issues:

- Picker tests can hang if they open a real picker with real repo state.
- Switching keymaps while picker is open requires cleanup.
- Lua `WIN_OPEN` events can accidentally rerun tests without guards.
- Feedkey tests cover keymaps but hide command intent.

Go rewrite guidance:

- Prefer direct command tests.
- Add keymap tests separately.
- Use fake picker providers.
- Enforce event drain timeouts.
- Assert no unexpected open surfaces after tests.

## Mapping Table

| vis Key | vis Action | wisp Command |
|---|---|---|
| `i` | `vis-helix-insert` | `mode.insert-before-selection` |
| `a` | `vis-helix-append` | `mode.append-after-selection` |
| `x` | `vis-helix-line-select` | `selection.line` |
| `X` | `vis-helix-line-select-current` | `selection.line-current` |
| `;` | `vis-helix-collapse-selection` | `selection.collapse` |
| `v` | `vis-helix-select-toggle` | `mode.select-toggle` |
| `w` | `vis-motion-word-start-next` plus Helix special case | `selection.word.next` |
| `b` | `vis-motion-word-start-prev` plus Helix special case | `selection.word.prev` |
| `e` | `vis-motion-word-end-next` plus Helix special case | `selection.word.end-next` |
| `f` | `vis-motion-to-right` | `selection.find-char-right` |
| `t` | `vis-motion-till-right` | `selection.till-char-right` |
| `d` | `vis-operator-delete` with Helix context | `edit.delete-selection` |
| `c` | `vis-operator-change` with Helix context | `edit.change-selection` |
| `y` | `vis-operator-yank` with Helix context | `edit.yank-selection` |
| `p` | `vis-put-after` with Helix put context | `edit.paste-after` |
| `P` | `vis-put-before` with Helix put context | `edit.paste-before` |
| `r` | `vis-helix-replace-char` | `edit.replace-selection-with-char` |
| `R` | `vis-helix-replace-with-yanked` | `edit.replace-selection-with-register` |
| `.` | `vis-helix-repeat` | `edit.repeat` |
| `s` | `vis-helix-select-regex-prompt` | `selection.select-regex-prompt` |
| `S` | `vis-helix-split-regex-prompt` | `selection.split-regex-prompt` |
| `K` | `vis-helix-keep-regex-prompt` | `selection.keep-regex-prompt` |
| `alt-K` | `vis-helix-remove-regex-prompt` | `selection.remove-regex-prompt` |
| `space f` | `vis-picker-files` | `picker.files` |
| `space b` | `vis-picker-buffers` | `picker.buffers` |
| `space g` | `vis-picker-changed` | `picker.changed` |
| `space j` | `vis-picker-jumplist` | `picker.jumplist` |
| `mm` | `vis-helix-match-bracket` | `move.match-bracket` |
| `gw` | `vis-helix-goto-word` | `ui.goto-word-labels` |
| `%` | `vis-helix-select-all` | `selection.file` |
| `ctrl-a` | `vis-helix-increment` | `edit.increment-number` |
| `ctrl-x` | `vis-helix-decrement` | `edit.decrement-number` |

## Final Guidance

Use the vis implementation as a behavior reference, not an architecture template.

Preserve:

- selection-first editing
- selection-aware insert and append
- word movement as selection transforms
- semantic repeat
- typed picker items
- file-aware locations
- visible user messages for no-op picker states

Avoid:

- C/Lua split ownership
- key replay as primary implementation mechanism
- global prompt routing enums when typed surfaces can carry continuations
- view-local marks for cross-file locations
- tests coupled to real workspace state
