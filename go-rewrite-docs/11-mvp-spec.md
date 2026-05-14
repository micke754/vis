# MVP Spec

## Purpose

This document defines the exact minimum prototype for `wisp`. It exists to prevent the implementation agent from expanding scope or inventing incompatible behavior.

## Product Shape

`wisp` is a terminal modal editor prototype with a Helix-inspired editing model and first-class modern tooling hooks.

The short executable alias is `ws`.

The MVP is not a full editor replacement. It is a proving ground for architecture, velocity, and feature integration.

## MVP Must Have

- open one or more files
- edit a small UTF-8 file
- save current buffer
- undo and redo text edits
- normal and insert mode
- selection-first delete for selected line
- file picker
- buffer picker
- changed-file picker backed by injectable git provider
- command picker
- diagnostics picker backed by LSP diagnostics state
- minimal LSP diagnostics flow with fake server tests
- AI request flow with fake provider tests
- deterministic test harness

## MVP Must Not Have

- plugin system
- runtime TOML/JSON/YAML config
- tree-sitter
- full Helix parity
- full Vim parity
- macro recording
- split panes unless cheap after picker work
- terminal mouse support
- remote editing
- persistent sessions
- shipping-quality theme customization
- real AI endpoint required for tests

## CLI Behavior

Required commands:

```sh
wisp --help
wisp --version
wisp [file]
```

Expected help output can be minimal:

```text
usage: wisp [file]
alias: ws
```

The alias `ws` can be documented before it is installed by build tooling. The code should keep naming clear so an alias or symlink is easy to add later.

## Configuration

Use `internal/config/config.go`.

No runtime config parser in the MVP.

Config should include:

- editor defaults
- keymap tables
- language server definitions
- AI provider definitions
- ignore rules for file walking

Initial config should be typed Go data. Do not use map-heavy untyped config where structs are practical.

## Modes

Required modes:

- normal
- insert
- picker
- prompt if needed for command picker or save commands

Select mode is desirable but can be represented by explicit selection commands during early milestones. Do not block LSP and picker work on full select-mode parity.

## Required Commands

### Mode Commands

- `mode.normal`
- `mode.insert`
- `mode.picker` is internal to picker surface and does not need to be user-dispatched

### Movement Commands

- `move.char.prev`
- `move.char.next`
- `move.line.up`
- `move.line.down`
- `move.line.start`
- `move.line.end`

### Edit Commands

- `edit.insert-text`
- `edit.delete-selection`
- `edit.save`
- `edit.undo`
- `edit.redo`

### Selection Commands

- `selection.collapse`
- `selection.line`
- `selection.word` if cheap

### Picker Commands

- `picker.files`
- `picker.buffers`
- `picker.changed`
- `picker.commands`
- `picker.diagnostics`
- `picker.accept`
- `picker.cancel`
- `picker.next`
- `picker.prev`
- `picker.page-next`
- `picker.page-prev`
- `picker.first`
- `picker.last`
- `picker.backspace`
- `picker.delete-word`
- `picker.toggle-preview`

### LSP Commands

- `lsp.start`
- `lsp.stop`
- `lsp.diagnostics`
- `lsp.document-symbols`
- `lsp.references`

### AI Commands

- `ai.complete-selection`
- `ai.cancel`
- `ai.accept`
- `ai.reject`

## Required Keybindings

Normal mode:

```text
i          mode.insert
escape     mode.normal
h          move.char.prev
j          move.line.down
k          move.line.up
l          move.char.next
0          move.line.start
$          move.line.end
x          selection.line
d          edit.delete-selection
u          edit.undo
U          edit.redo
space f    picker.files
space b    picker.buffers
space g    picker.changed
space d    picker.diagnostics
space c    picker.commands
```

Insert mode:

```text
escape     mode.normal
backspace  edit.delete-backward if implemented
enter      insert newline
all printable input inserts text
```

Picker mode:

```text
down        picker.next
ctrl-n      picker.next
tab         picker.next
up          picker.prev
ctrl-p      picker.prev
shift-tab   picker.prev
page-down   picker.page-next
ctrl-d      picker.page-next
page-up     picker.page-prev
ctrl-u      picker.page-prev
home        picker.first
end         picker.last
enter       picker.accept
ctrl-j      picker.accept
escape      picker.cancel
ctrl-c      picker.cancel
ctrl-g      picker.cancel
backspace   picker.backspace
ctrl-h      picker.backspace
ctrl-w      picker.delete-word
ctrl-t      picker.toggle-preview
```

Avoid default `ctrl-s` and `ctrl-v` bindings because terminals commonly reserve them.

## Buffer Behavior

The MVP supports UTF-8 text files.

Buffer state must track:

- URI/path
- text
- dirty flag
- version number
- language ID if known
- undo stack
- redo stack

Saving writes the current buffer to its URI and clears dirty state.

## Selection Behavior

The MVP must support at least one selection per window.

Selections are ranges with anchor and cursor. Commands operate on selections first.

Multi-selection support is desirable but can be limited initially. If implemented, text edits must apply from bottom to top.

## Picker Behavior

Picker is a surface overlay.

Picker must support:

- title
- filter input
- current selection index
- fuzzy or subsequence filtering
- accept and cancel
- preview on/off
- typed item actions

Picker item must support:

- kind
- label
- detail
- optional URI
- optional location
- preview reference
- action command

The picker must not know source-specific behavior like git, LSP, or AI. Providers own item generation.

## Changed Files Behavior

Changed files means the union of:

- git working-tree changes
- dirty editor buffers

Deleted files may be shown later for diff workflows, but the MVP can exclude deleted files from open actions.

If no changed files exist, show `No changed files`.

## LSP MVP Behavior

Required:

- start one configured server for a filetype
- initialize server
- send didOpen
- send didChange with full document text
- receive publishDiagnostics
- store diagnostics by URI and client ID
- diagnostics picker jumps to locations

Initial real language server target:

- `gopls`

All normal tests must use fake LSP server. Real `gopls` tests are optional integration tests.

## AI MVP Behavior

Required:

- fake AI provider for tests
- OpenAI-compatible provider using `net/http`
- no network request unless configured
- prompt builder for selected text or active buffer excerpt
- cancellation with context
- preview surface
- accept inserts or replaces text
- reject leaves buffer unchanged

Ghost text can be added later. The first MVP can use preview/apply only.

## Unicode MVP Behavior

Required separation:

- byte offsets for buffer storage
- line/column for user-visible locations
- UTF-16 positions for LSP
- screen columns for rendering

Use `github.com/rivo/uniseg` unless there is a strong implementation blocker.

## UI MVP Behavior

Required:

- raw terminal mode
- terminal restore on exit
- statusline with mode, file, dirty state, line/column
- render current buffer
- render cursor
- render picker overlay
- render basic messages

No mouse support required.

## Error Messages

Commands should not silently do nothing when user-visible behavior is expected.

Required examples:

- `No changed files`
- `No diagnostics`
- `No LSP server for this buffer`
- `AI provider is not configured`
- `AI request canceled`

## Performance Targets

MVP targets small to medium files.

Reasonable first targets:

- files under 1 MB should feel interactive
- file picker over a few thousand files should remain responsive enough for prototype use
- LSP and AI must never block normal editing while waiting for external responses

## Definition Of Done

The MVP is done when:

- `go test ./...` passes
- manual edit/save smoke test passes
- file picker works
- changed-file picker works with fake git tests and a real git manual smoke test
- fake LSP diagnostics picker works
- real `gopls` diagnostics can be manually tested if installed
- fake AI preview/apply flow works
- docs contain a decision memo on whether to continue
