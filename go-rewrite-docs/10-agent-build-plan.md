# Agent Build Plan

## Purpose

This document tells an implementation agent how to build the first `wisp` prototype in a new git-controlled Go repository.

The agent should treat this as the execution plan. If it needs design context, it should consult the other docs in this directory. If instructions conflict, the future root `AGENT.md` wins, then this build plan, then the other design docs.

## Project Identity

- Project name: `wisp`
- Canonical binary: `wisp`
- Short executable alias: `ws`
- Initial module placeholder: `github.com/OWNER/wisp`
- Initial configuration style: `config.go`
- Default verification command: `go test ./...`

## Dependency Decisions

Start with only these external dependencies:

- `golang.org/x/term`
- `github.com/rivo/uniseg`

Do not add these without explicit approval:

- Bubbletea
- tcell
- tree-sitter bindings
- OpenAI or Anthropic SDKs
- third-party rope libraries
- fuzzy finder libraries
- TOML, YAML, or JSON config libraries
- assertion libraries

Use the Go standard library for:

- process spawning
- JSON
- HTTP
- filesystem access
- tests
- logging until structured logging becomes necessary

## Repository Bootstrap

Create this initial layout:

```text
.
AGENT.md
go.mod
cmd/wisp/main.go
internal/ai
internal/buffer
internal/command
internal/config
internal/editor
internal/git
internal/job
internal/keymap
internal/lsp
internal/picker
internal/selection
internal/testutil
internal/ui
internal/workspace
go-rewrite-docs/
```

Copy the docs into `go-rewrite-docs/` and promote `go-rewrite-docs/AGENT.md` to root `AGENT.md`.

## Implementation Rules

- Build vertical slices, not broad frameworks.
- Keep the editor usable at every milestone.
- Do not chase vis parity.
- Do not add plugin support in the prototype.
- Do not add runtime config parsing in the prototype.
- Do not let background goroutines mutate editor state directly.
- Do not let terminal escape sequences leak into editor core packages.
- Do not use feedkey-style strings as the core command API.
- Do not make picker providers depend on real repo state in tests.
- Do not block the editor loop for LSP, AI, git status, or file walking.

## Milestone 0: Skeleton

Goal: create a compilable Go project with package boundaries, `config.go`, and a minimal launch/exit path.

Create:

- `go.mod`
- `cmd/wisp/main.go`
- `internal/config/config.go`
- `internal/editor/editor.go`
- `internal/command/command.go`
- `internal/keymap/keymap.go`
- `internal/ui/terminal.go`
- `internal/testutil/harness.go`

Minimum behavior:

- `go test ./...` passes.
- `go run ./cmd/wisp --version` prints a version string.
- `go run ./cmd/wisp --help` prints minimal usage.
- `go run ./cmd/wisp` can initialize and exit without panic.

Suggested version output:

```text
wisp prototype
```

Suggested help output:

```text
usage: wisp [file]
alias: ws
```

Exit criteria:

- every package compiles
- initial tests pass
- no external dependencies beyond approved list

## Milestone 1: Buffer And Commands

Goal: make state-editing commands work without terminal UI dependency.

Implement:

- `internal/buffer.TextStore`
- `internal/buffer.Buffer`
- line and column conversion
- insert and delete edits
- simple undo and redo
- `internal/selection.Set`
- command registry and dispatch
- editor open buffer from in-memory content

Initial commands:

- `buffer.open`
- `buffer.save`
- `mode.normal`
- `mode.insert`
- `move.char.prev`
- `move.char.next`
- `move.line.up`
- `move.line.down`
- `edit.insert-text`
- `edit.delete-selection`
- `edit.undo`
- `edit.redo`
- `selection.collapse`
- `selection.line`

Minimum behavior:

- tests can create an editor with text
- tests can dispatch commands directly
- editing updates text and dirty state
- undo/redo restores text and selection state

Exit criteria:

- command tests cover insert, delete, movement, line selection, undo, redo
- no terminal required for these tests

## Milestone 2: Keymap And Minimal Terminal UI

Goal: make a small terminal editor usable enough for manual testing.

Implement:

- key parser with canonical keys like `space f`, `ctrl-o`, `alt-s`, `enter`
- `config.go` keymap table
- raw mode terminal backend with `x/term`
- `Screen` and `Cell` render model
- ANSI diff renderer
- statusline
- normal and insert mode input

Initial keybindings:

- `i` -> `mode.insert`
- `escape` -> `mode.normal`
- `h` -> `move.char.prev`
- `j` -> `move.line.down`
- `k` -> `move.line.up`
- `l` -> `move.char.next`
- `x` -> `selection.line`
- `d` -> `edit.delete-selection`
- `u` -> `edit.undo`
- `U` -> `edit.redo`
- `space f` -> `picker.files`
- `space b` -> `picker.buffers`
- `space g` -> `picker.changed`
- `space d` -> `picker.diagnostics`

Minimum behavior:

- open a file
- move cursor
- insert text
- save file
- exit cleanly

Exit criteria:

- render snapshot tests pass
- keymap tests pass
- manual smoke test works on a small file

## Milestone 3: Generic Picker

Goal: implement the picker as shared infrastructure.

Implement:

- `internal/picker.Item`
- `internal/picker.Provider`
- picker surface
- filtering
- navigation
- preview routing
- accept and cancel
- file provider
- buffer provider
- changed-file provider
- command provider

Initial picker keys:

- `down`, `ctrl-n`, `tab` -> next item
- `up`, `ctrl-p`, `shift-tab` -> previous item
- `page-down`, `ctrl-d` -> page down
- `page-up`, `ctrl-u` -> page up
- `home` -> first item
- `end` -> last item
- `enter`, `ctrl-j` -> accept
- `escape`, `ctrl-c`, `ctrl-g` -> cancel
- `backspace`, `ctrl-h` -> delete filter char
- `ctrl-w` -> delete filter word
- `ctrl-t` -> toggle preview

Minimum behavior:

- `space f` opens file picker
- `space b` opens buffer picker
- `space g` opens changed-file picker using fake git in tests
- picker item accept opens file or switches buffer
- picker cancel restores prior mode
- empty providers show status messages like `No changed files`

Exit criteria:

- picker provider tests pass
- picker state tests pass
- picker render tests pass
- no picker unit test depends on real git or real filesystem

## Milestone 4: LSP Diagnostics Vertical Slice

Goal: prove typed async LSP integration.

Implement:

- LSP process runner behind an interface
- JSON-RPC message codec
- client lifecycle
- initialize and shutdown
- didOpen and didChange full document sync
- diagnostics storage
- diagnostics picker provider
- fake LSP server for tests

Initial real server target:

- `gopls`

Minimum behavior:

- opening a Go file can start `gopls` if available
- diagnostics events update editor state
- `space d` opens diagnostics picker
- accepting a diagnostic jumps to its location

Exit criteria:

- fake LSP tests pass
- real `gopls` smoke test is available behind `WISP_INTEGRATION=1`
- diagnostics never mutate editor state from background goroutines

## Milestone 5: Symbols And References

Goal: prove location picker reuse.

Implement:

- document symbols request
- references request
- definition request if cheap
- symbol picker provider
- references picker provider

Minimum behavior:

- command can request symbols from active LSP client
- command can request references from active cursor location
- one result jumps directly or many results open picker

Exit criteria:

- fake LSP tests cover symbols and references
- location conversion tests cover UTF-16 to byte offsets

## Milestone 6: AI Vertical Slice

Goal: prove AI request lifecycle, not product-perfect AI UX.

Implement:

- AI provider interface
- fake AI provider
- OpenAI-compatible HTTP provider using `net/http`
- request cancellation
- prompt builder for selection and current file
- AI preview surface
- accept as insert or replace selection

Minimum behavior:

- configured fake provider can stream a response into preview
- user can cancel request
- user can accept response into buffer
- no real network call happens without explicit config

Exit criteria:

- fake provider tests pass
- cancellation tests pass
- prompt builder tests pass
- manual real endpoint test is documented but optional

## Milestone 7: Decision Gate

Goal: decide whether to continue the rewrite.

Produce:

- brief decision memo
- what was built
- what was easier than vis
- what stayed hard
- test quality assessment
- architecture debt list
- recommendation to continue, stop, or pivot

Exit criteria:

- all normal tests pass
- manual smoke checklist completed
- no unresolved architectural blockers hidden in TODOs

## Stop Conditions

Pause and ask for human direction if any of these happen:

- a new dependency seems necessary outside the approved list
- buffer storage starts dominating milestone 1
- terminal input requires a major backend switch
- config.go becomes obviously insufficient before LSP works
- LSP position conversion is ambiguous or failing on Unicode
- background goroutines need direct editor mutation
- a feature requires building plugin architecture
- tests require real terminal or real git for normal `go test ./...`

## Verification Commands

Always run:

```sh
go test ./...
```

Run when UI changes:

```sh
go test ./internal/ui ./internal/picker
```

Run optional integration tests only when explicitly requested or when environment is configured:

```sh
WISP_INTEGRATION=1 go test ./internal/lsp
WISP_TERMINAL=1 go test ./internal/ui
```

## Implementation Bias

Prefer small working slices over perfect abstractions. The design docs define the direction, but a working prototype is the purpose of this repository.
