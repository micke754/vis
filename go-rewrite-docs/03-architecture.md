# Architecture

## High-Level Shape

The Go prototype should be a single-process terminal editor with a synchronous editor event loop and asynchronous workers. Background workers never mutate editor state directly. They send typed events to the editor loop.

```text
Terminal Input -> Key Decoder -> Keymap -> Command Dispatcher -> Editor State
                                                            |
Background Events -> Event Queue ----------------------------|
                                                            v
                                                     Render Snapshot -> Terminal
```

## Package Layout

```text
cmd/wisp
internal/editor
internal/buffer
internal/selection
internal/keymap
internal/command
internal/ui
internal/picker
internal/workspace
internal/git
internal/lsp
internal/ai
internal/job
internal/config
internal/testutil
```

## Package Responsibilities

### `internal/editor`

Owns top-level editor state and event loop.

Key types:

```go
type Editor struct {
    Workspace *workspace.Workspace
    Buffers   *BufferStore
    Windows   *WindowStore
    Mode      Mode
    Surface   SurfaceStack
    Commands  *command.Registry
    Keymaps   *keymap.Manager
    Jobs      *job.Manager
    Events    chan Event
    Config    config.Config
}
```

Responsibilities:

- dispatch commands
- apply state changes
- own current mode
- own active window
- own overlay stack
- consume async events
- produce render snapshots

### `internal/buffer`

Owns text storage, edits, undo, redo, snapshots, file I/O, and position conversion.

The initial implementation can use a piece table or gap buffer. The important part is the API boundary, not the final storage structure.

### `internal/selection`

Owns ranges, cursors, multi-selection normalization, and Helix-style selection operations.

### `internal/keymap`

Owns key parsing and key sequence resolution.

Keymaps should be data, not embedded control flow.

### `internal/command`

Owns typed command definitions, arguments, metadata, and dispatch helpers.

Commands should be callable from keymaps, command prompt, tests, and future plugins through one path.

### `internal/ui`

Owns terminal abstraction, screen buffer, layout, rendering, styles, and input decoding.

The editor should render to an intermediate `Screen` model before writing terminal escape sequences.

### `internal/picker`

Owns picker state, filtering, selection, preview routing, and accept/cancel behavior.

Picker providers live outside picker core.

### `internal/workspace`

Owns project root detection, file walking, ignore rules, URI/path normalization, and workspace services.

### `internal/git`

Owns git status parsing and repository operations behind an interface.

### `internal/lsp`

Owns language server configuration, JSON-RPC, process management, document sync, request routing, diagnostics, and capabilities.

### `internal/ai`

Owns API providers, prompt builders, response parsing, cancellation, and apply/preview actions.

### `internal/job`

Owns background task lifecycle, cancellation, progress, logging, and error propagation.

### `internal/config`

Owns static config parsing and validation.

Initial config can be TOML, YAML, or JSON. Pick boring and typed.

### `internal/testutil`

Owns fake terminal, fake filesystem, fake git provider, fake LSP server, fake AI provider, and editor test harness.

## State Ownership Rules

- Only the editor loop mutates editor state.
- Background goroutines emit events.
- Commands return state changes or apply through an editor transaction.
- UI renders snapshots and sends input events.
- Pickers are overlays on the editor state, not independent mini-editors.
- LSP and AI clients never call editor methods directly from worker goroutines.

## Event Model

Use typed events instead of stringly process responses.

```go
type Event interface {
    EventKind() EventKind
}

type KeyEvent struct { Key keymap.Key }
type CommandEvent struct { Command command.Invocation }
type LSPDiagnosticsEvent struct { ClientID lsp.ClientID; URI URI; Diagnostics []Diagnostic }
type LSPResponseEvent struct { RequestID lsp.RequestID; Result any; Err error }
type AIResponseEvent struct { RequestID ai.RequestID; Chunk string; Done bool; Err error }
type JobProgressEvent struct { JobID job.ID; Message string; Percent *int }
type FileChangedEvent struct { URI URI; Kind FileChangeKind }
```

The event loop should process events in order and produce invalidation flags for rendering.

## Command Model

Commands should be typed and introspectable.

```go
type Command interface {
    ID() ID
    Execute(ctx Context) error
}

type Invocation struct {
    ID    ID
    Args  Args
    Count int
}
```

Command metadata should include:

- ID
- display name
- help text
- mode constraints
- repeat behavior
- whether it changes text
- whether it is async
- whether it opens UI

This solves a problem we had in vis where keymap strings and C action registrations had to stay manually aligned.

## Transaction Model

Commands that mutate state should do it through a transaction-like API.

```go
type Transaction struct {
    Editor *Editor
    BufferEdits []buffer.Edit
    SelectionEdits []selection.Edit
    ModeChange *Mode
    SurfaceChange *SurfaceChange
}
```

The first prototype does not need a complex transaction engine. It needs a convention that state changes are explicit and test-visible.

## Surface Stack

Use a surface stack for overlays.

```go
type Surface interface {
    HandleEvent(*Editor, Event) Handled
    Render(RenderContext) []DrawOp
    WantsInput() bool
}
```

Examples:

- main editor surface
- command prompt
- picker
- completion menu
- hover popup
- AI preview

The topmost surface handles input first. This avoids singleton fields like `vis->picker` and makes overlay lifecycle testable.

## Workspace Model

Use URIs internally for external integrations and paths for display.

```go
type URI string

type Workspace struct {
    Root URI
    FS FileSystem
    Git git.Provider
    Ignore IgnoreMatcher
}
```

Path rules:

- Normalize once at workspace boundary.
- Store absolute URI in domain objects.
- Display relative paths when possible.
- Never use label strings as paths.

## Dependency Injection

Interfaces should exist where we need test isolation, not everywhere.

Good interface boundaries:

- filesystem
- git provider
- terminal
- LSP process runner
- HTTP client
- clock
- logger

Avoid interface sprawl for internal data structures like selections and commands until there is a concrete need.

## Concurrency Model

Use goroutines for blocking external systems. Do not use goroutines for normal editing operations.

Workers:

- LSP clients
- AI requests
- file watching
- git status refresh
- workspace file indexing

Each worker gets a context and an event sink.

```go
type Worker interface {
    Start(ctx context.Context, sink EventSink) error
    Stop(ctx context.Context) error
}
```

The editor loop owns cancellation when buffers close, workspaces change, or users cancel operations.

## Error Handling

Errors should be visible and structured.

Levels:

- status message for user-facing recoverable errors
- log entry for diagnostics
- command error for tests
- job error for background tasks

No silent returns for user-visible commands like changed-file picker.

## Logging

Add structured logging early.

Minimum fields:

- component
- operation
- buffer URI
- request ID
- job ID
- duration
- error

This will matter immediately for LSP and AI work.

## Configuration

Start with typed static config.

Example:

```toml
[editor]
theme = "default"

[keymap.normal]
"space f" = "picker.files"
"space g" = "picker.changed"

[language.go]
command = "gopls"
filetypes = ["go"]

[ai.openai]
endpoint = "https://api.openai.com/v1/chat/completions"
model = "gpt-4.1-mini"
timeout = "30s"
```

Do not add dynamic scripting until the core is stable.

## Main Loop Sketch

```go
func (e *Editor) Run(ctx context.Context) error {
    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        case ev := <-e.Events:
            e.ApplyEvent(ev)
            if e.Invalidated() {
                e.Render()
            }
        }
    }
}
```

The real loop will also read terminal input and timers, but the important rule remains: all state mutation flows through `ApplyEvent` or command dispatch on the main goroutine.

## Why This Is Different From vis

- Keymaps invoke typed commands, not stringly C action names through Lua.
- Picker state is a surface, not a global singleton.
- Locations are file-aware by default.
- LSP and AI are typed clients, not generic subprocess responses.
- Tests can construct editor state directly and dispatch commands.
- UI rendering can be snapshot-tested before terminal escape output.
