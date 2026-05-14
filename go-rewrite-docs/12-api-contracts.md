# API Contracts

## Purpose

This document defines the starting Go interfaces and types for `wisp`. The implementation agent should use these as the initial shape unless it finds a concrete blocker.

The contracts are intentionally small. They should guide the first prototype, not freeze the final editor design.

## Naming And Module

Initial module placeholder:

```go
module github.com/OWNER/wisp
```

Primary command package:

```text
cmd/wisp
```

Short alias target:

```text
ws -> wisp
```

## Shared Types

Package: `internal/editor` or `internal/workspace` for URI types.

```go
type URI string

type BytePos int
type Line int
type Column int
type UTF16Column int
type ScreenColumn int

type LineCol struct {
    Line Line
    Col  Column
}

type Range struct {
    Start BytePos
    End   BytePos
}

type Location struct {
    URI   URI
    Range Range
}
```

Rules:

- Use `BytePos` for text storage.
- Use `LineCol` for user-facing location display.
- Use LSP-specific position types inside `internal/lsp` only.
- Use `ScreenColumn` for rendering calculations.

## Config

Package: `internal/config`

```go
type Config struct {
    Editor    EditorConfig
    Keymaps   KeymapConfig
    Languages []LanguageConfig
    AI        AIConfig
    Ignore    []string
}

type EditorConfig struct {
    Theme string
    TabWidth int
}

type KeymapConfig struct {
    Normal map[string]string
    Insert map[string]string
    Picker map[string]string
}

type LanguageConfig struct {
    Name string
    Filetypes []string
    LSP *LSPConfig
}

type LSPConfig struct {
    Command string
    Args []string
    RootPatterns []string
}

type AIConfig struct {
    Provider string
    Endpoint string
    Model string
    APIKeyEnv string
    TimeoutSeconds int
    MaxContextBytes int
}
```

Required file:

```go
var Default = Config{...}
```

No runtime parser in the MVP.

## Buffer

Package: `internal/buffer`

```go
type ID int64

type Buffer struct {
    ID       ID
    URI      editor.URI
    Version  int
    Dirty    bool
    Language string
}
```

```go
type TextStore interface {
    LenBytes() int
    Insert(pos editor.BytePos, text []byte) error
    Delete(r editor.Range) error
    Slice(r editor.Range) ([]byte, error)
    String() string
    LineStart(line editor.Line) (editor.BytePos, error)
    LineCol(pos editor.BytePos) (editor.LineCol, error)
    Position(lineCol editor.LineCol) (editor.BytePos, error)
}
```

Initial concrete implementation:

```go
type PieceTable struct { ... }
```

If a piece table is too much for milestone 1, a simple `[]byte` store is acceptable behind the same interface. Do not expose the concrete storage type outside `internal/buffer`.

## Edits And Undo

Package: `internal/buffer`

```go
type Edit struct {
    Range editor.Range
    Insert []byte
}

type Transaction struct {
    Buffer ID
    Edits []Edit
    Before selection.Set
    After selection.Set
    Description string
}

type UndoStack interface {
    Push(Transaction)
    Undo(*Buffer) (selection.Set, error)
    Redo(*Buffer) (selection.Set, error)
}
```

MVP can use a linear undo stack. Undo tree is not required.

## Selection

Package: `internal/selection`

```go
type Selection struct {
    Anchor editor.BytePos
    Cursor editor.BytePos
}

type Set struct {
    Items []Selection
    Primary int
}
```

Required methods:

```go
func (s Selection) Range() editor.Range
func (s Selection) IsReversed() bool
func (s *Set) Normalize()
func (s *Set) Collapse()
func (s *Set) PrimarySelection() Selection
```

Rules:

- Commands operate on `selection.Set`.
- Text-changing multi-selection edits apply from bottom to top.
- Undo restores selections where practical.

## Editor

Package: `internal/editor`

```go
type Mode string

const (
    ModeNormal Mode = "normal"
    ModeInsert Mode = "insert"
    ModePicker Mode = "picker"
    ModePrompt Mode = "prompt"
)
```

```go
type Editor struct {
    Config config.Config
    Buffers *BufferStore
    Windows *WindowStore
    Current WindowID
    Mode Mode
    Commands *command.Registry
    Keymaps *keymap.Manager
    Surfaces *SurfaceStack
    Jobs *job.Manager
    Events chan Event
    Messages []Message
}
```

```go
type Event interface {
    Kind() EventKind
}

type EventKind string

type Message struct {
    Text string
    Level MessageLevel
}
```

Editor must expose:

```go
func New(cfg config.Config, deps Deps) (*Editor, error)
func (e *Editor) Dispatch(ctx context.Context, inv command.Invocation) error
func (e *Editor) ApplyEvent(ctx context.Context, ev Event) error
func (e *Editor) ActiveBuffer() (*buffer.Buffer, error)
func (e *Editor) ActiveWindow() (*Window, error)
func (e *Editor) ShowMessage(text string)
```

## Window And View

Package: `internal/editor`

```go
type WindowID int64

type Window struct {
    ID WindowID
    Buffer buffer.ID
    View ViewState
    Selections selection.Set
}

type ViewState struct {
    Top editor.BytePos
    Width int
    Height int
}
```

Splits are not required for MVP.

## Commands

Package: `internal/command`

```go
type ID string

type Invocation struct {
    ID ID
    Args Args
    Count int
}

type Args map[string]any
```

```go
type Command interface {
    ID() ID
    Metadata() Metadata
    Execute(context.Context, Context) error
}

type Context struct {
    Editor *editor.Editor
    Invocation Invocation
}
```

```go
type Metadata struct {
    Name string
    Help string
    Modes []editor.Mode
    ChangesText bool
    OpensSurface bool
    Async bool
    Repeatable bool
}
```

```go
type Registry struct { ... }

func (r *Registry) Register(cmd Command) error
func (r *Registry) Invoke(ctx context.Context, e *editor.Editor, inv Invocation) error
func (r *Registry) List() []Metadata
```

Do not use stringly feedkey commands as the core API.

## Keymap

Package: `internal/keymap`

```go
type Key string
type Sequence []Key

type Binding struct {
    Sequence Sequence
    Command command.Invocation
    Help string
}
```

```go
type Manager struct { ... }

func ParseSequence(input string) (Sequence, error)
func (m *Manager) Bind(mode editor.Mode, seq Sequence, inv command.Invocation, help string) error
func (m *Manager) Resolve(mode editor.Mode, pending Sequence) (ResolveResult, error)
```

```go
type ResolveResult struct {
    Match *command.Invocation
    Prefix bool
}
```

Canonical key strings:

- `space`
- `escape`
- `enter`
- `backspace`
- `ctrl-n`
- `alt-s`
- `shift-tab`

The parser may accept `<Space>` later, but config should use canonical lowercase strings.

## UI

Package: `internal/ui`

```go
type Terminal interface {
    Init() error
    Close() error
    Size() (width int, height int)
    ReadEvent(ctx context.Context) (InputEvent, error)
    Draw(Screen) error
}

type InputEvent struct {
    Key keymap.Key
    Text string
}
```

```go
type Screen struct {
    Width int
    Height int
    Cells []Cell
}

type Cell struct {
    Rune rune
    Style Style
}

type Style struct {
    Fore Color
    Back Color
    Bold bool
    Dim bool
    Reverse bool
}
```

Terminal backend:

```go
type ANSI struct { ... }
```

Rules:

- Editor core renders to `Screen` or draw operations, not ANSI strings.
- ANSI diffing stays inside `internal/ui`.

## Surfaces

Package: `internal/editor` or `internal/ui`

```go
type Surface interface {
    Name() string
    HandleInput(context.Context, *Editor, ui.InputEvent) (Handled, error)
    Render(*Editor, *ui.Screen) error
}

type Handled bool

type SurfaceStack struct { ... }
```

Surfaces:

- main editor surface
- picker surface
- AI preview surface
- prompt surface later

## Picker

Package: `internal/picker`

```go
type Kind string

const (
    KindFile Kind = "file"
    KindBuffer Kind = "buffer"
    KindLocation Kind = "location"
    KindDiagnostic Kind = "diagnostic"
    KindCommand Kind = "command"
    KindText Kind = "text"
    KindAI Kind = "ai"
)
```

```go
type Item struct {
    ID string
    Kind Kind
    Label string
    Detail string
    URI *editor.URI
    Location *editor.Location
    Preview PreviewRef
    Action command.Invocation
}

type PreviewRef struct {
    URI *editor.URI
    Location *editor.Location
    Text string
}
```

```go
type Provider interface {
    ID() string
    Title() string
    Items(context.Context, *editor.Editor) ([]Item, error)
    EmptyMessage() string
}
```

```go
type State struct {
    Title string
    Items []Item
    Filter string
    Filtered []int
    Selected int
    PreviewVisible bool
}
```

Picker core must not call git, LSP, or AI directly. Providers do that.

## Workspace And Git

Package: `internal/workspace`

```go
type FileSystem interface {
    ReadFile(ctx context.Context, uri editor.URI) ([]byte, error)
    WriteFile(ctx context.Context, uri editor.URI, data []byte) error
    Stat(ctx context.Context, uri editor.URI) (FileInfo, error)
    Walk(ctx context.Context, root editor.URI, opts WalkOptions) ([]editor.URI, error)
}
```

Package: `internal/git`

```go
type Provider interface {
    Status(ctx context.Context, root editor.URI) ([]Change, error)
}

type Change struct {
    URI editor.URI
    Kind ChangeKind
    Staged bool
    Untracked bool
    RenamedFrom *editor.URI
}

type ChangeKind string
```

Real implementation may use `os/exec git status --porcelain -z`. Tests must use fake provider.

## Jobs

Package: `internal/job`

```go
type ID int64
type Kind string
type Status string

type Job struct {
    ID ID
    Kind Kind
    Title string
    Status Status
    Cancel context.CancelFunc
    Started time.Time
}

type Manager struct { ... }
```

```go
func (m *Manager) Start(ctx context.Context, kind Kind, title string, run func(context.Context) error) ID
func (m *Manager) Cancel(id ID) error
func (m *Manager) List() []Job
```

Background jobs emit typed editor events. They do not mutate editor state directly.

## LSP

Package: `internal/lsp`

```go
type ClientID int64
type RequestID int64

type Manager struct { ... }

type Client struct { ... }
```

```go
type ProcessRunner interface {
    Start(ctx context.Context, command string, args []string) (Process, error)
}

type Process interface {
    Stdin() io.WriteCloser
    Stdout() io.Reader
    Stderr() io.Reader
    Kill() error
    Wait() error
}
```

```go
type Diagnostic struct {
    URI editor.URI
    Range editor.Range
    Severity Severity
    Source string
    Code string
    Message string
    ClientID ClientID
}
```

Events:

```go
type DiagnosticsUpdated struct {
    ClientID ClientID
    URI editor.URI
    Version *int
    Diagnostics []Diagnostic
}
```

Required request methods for MVP:

- initialize
- shutdown
- textDocument/didOpen
- textDocument/didChange
- textDocument/didSave
- textDocument/didClose
- textDocument/documentSymbol
- textDocument/references

## AI

Package: `internal/ai`

```go
type RequestID int64

type Provider interface {
    Complete(ctx context.Context, req Request) (<-chan Event, error)
}

type Request struct {
    Model string
    Messages []Message
    Context ContextBundle
    Stream bool
}

type Message struct {
    Role string
    Content string
}
```

```go
type Event struct {
    RequestID RequestID
    Text string
    Done bool
    Err error
}

type ContextBundle struct {
    ActiveFile FileContext
    Selection *SelectionContext
    Diagnostics []lsp.Diagnostic
    UserInstruction string
}
```

Required providers:

- fake provider for tests
- OpenAI-compatible HTTP provider using `net/http`

Do not add official SDKs in MVP.

## Test Utilities

Package: `internal/testutil`

```go
type Harness struct {
    Editor *editor.Editor
    FS *FakeFS
    Git *git.FakeProvider
    Terminal *ui.FakeTerminal
    LSP *lsp.FakeServer
    AI *ai.FakeProvider
}
```

Required helpers:

```go
func NewHarness(t *testing.T) *Harness
func (h *Harness) OpenBuffer(uri string, content string)
func (h *Harness) Dispatch(id command.ID, args command.Args)
func (h *Harness) FeedKeys(keys ...string)
func (h *Harness) AssertBuffer(uri string, want string)
func (h *Harness) ActivePicker() *picker.State
func (h *Harness) DrainEvents()
```

## API Change Policy During Prototype

These contracts may evolve, but changes should follow rules:

- update this doc when changing public package contracts
- keep tests passing before moving to next milestone
- prefer removing concepts over adding new abstractions
- do not add interfaces unless tests or external systems need them
- do not add generic plugin APIs during MVP
