# Editor Core Design

## Goals

- Keep editing feel fast and modal.
- Make state explicit and testable.
- Support Helix-style selection-first commands from the start.
- Support LSP position conversion without bolting it on later.
- Avoid global mutable state patterns that make features hard to reason about.

## Core Types

```go
type Editor struct {
    Buffers *BufferStore
    Windows *WindowStore
    Current WindowID
    Mode Mode
    Registers Registers
    JumpList JumpList
    CommandHistory CommandHistory
}

type Buffer struct {
    ID BufferID
    URI URI
    Text TextStore
    Version int
    Dirty bool
    Undo *UndoTree
    Language LanguageID
}

type Window struct {
    ID WindowID
    Buffer BufferID
    View ViewState
    Selections selection.Set
}
```

## Text Storage

The prototype can start with a simple piece table. We do not need to over-optimize before measuring.

Required API:

```go
type TextStore interface {
    LenBytes() int
    Insert(pos int, text []byte) error
    Delete(r Range) error
    Slice(r Range) ([]byte, error)
    LineStart(line int) (int, error)
    LineCol(pos int) (LineCol, error)
    Position(lineCol LineCol) (int, error)
    Reader() io.Reader
}
```

Important constraints:

- Positions inside editor core are byte offsets plus explicit encoding helpers.
- LSP positions use UTF-16 line/character by default.
- UI columns use grapheme/display width.
- These are not interchangeable.

## Position Types

Define distinct types to prevent accidental mixing.

```go
type BytePos int
type Line int
type Column int
type UTF16Column int
type ScreenColumn int

type LineCol struct {
    Line Line
    Col Column
}

type Range struct {
    Start BytePos
    End BytePos
}
```

Conversion should live on `Buffer` or a `PositionIndex`.

## Position Index

LSP and rendering both need line/column conversion. Build this early.

```go
type PositionIndex struct {
    lineStarts []BytePos
    version int
}
```

Start simple:

- rebuild line starts after edits for small files
- optimize incrementally later
- cache UTF-16 conversions per line if needed

## Selections

Selections should be owned by windows, not buffers.

```go
type Selection struct {
    Anchor BytePos
    Cursor BytePos
}

type Set struct {
    Items []Selection
    Primary int
}
```

Selection methods:

- normalize overlapping selections
- collapse to cursor
- flip orientation
- map over selections from last to first for edits
- preserve primary selection
- select line
- select word
- split by regex
- keep/remove by regex

Helix-style operations should be native. Do not add Vim selection semantics first and retrofit Helix later.

## Modes

Initial modes:

- normal
- insert
- select
- picker
- prompt

Mode state should be explicit.

```go
type Mode string

const (
    ModeNormal Mode = "normal"
    ModeInsert Mode = "insert"
    ModeSelect Mode = "select"
    ModePicker Mode = "picker"
    ModePrompt Mode = "prompt"
)
```

Do not use mode as a bag for all UI state. Picker mode should be a surface on the surface stack.

## Commands

Commands are the unit of behavior. Keymaps, prompt commands, tests, and future plugins all call commands.

```go
type Context struct {
    Editor *editor.Editor
    Count int
    Register *RegisterID
    Args map[string]any
}
```

Command examples:

- `move.char.next`
- `move.line.down`
- `select.word.next`
- `edit.delete.selection`
- `edit.insert.text`
- `picker.files`
- `picker.changed`
- `lsp.references`
- `ai.complete.selection`

## Motions And Selection Transforms

In vis, movement and selection semantics were intertwined through mode and motion flags. In Go, separate them explicitly.

```go
type Motion func(Buffer, Selection, Count) (BytePos, error)
type SelectionTransform func(Buffer, selection.Set, Count) (selection.Set, error)
```

Normal mode can collapse selections after motions. Select mode can extend selections. Commands should make this behavior explicit.

## Operators

For Helix-like editing, many commands operate directly on selections and do not need a Vim-style pending operator state.

Initial operators:

- delete selections
- change selections
- yank selections
- indent selections
- outdent selections
- replace selections with char
- replace selections with register

The prototype can avoid a full Vim operator-pending mode.

## Repeat

The vis work showed semantic repeat is better than key replay for Helix operations.

Use command replay metadata:

```go
type RepeatRecord struct {
    Command command.ID
    Args command.Args
    Count int
    SelectionTransform *command.Invocation
}
```

Rules:

- Text-changing commands decide whether they are repeatable.
- Selection transform plus operator can be stored together.
- Key replay is only for macros, not `.` repeat.

## Undo And Redo

Use edit transactions.

```go
type EditTransaction struct {
    Buffer BufferID
    BeforeSelections selection.Set
    AfterSelections selection.Set
    Edits []Edit
    Description string
}
```

Each command that changes text should create a transaction. Multi-selection edits are one transaction.

Undo should restore text and selections.

## Registers

Start with simple registers:

- default
- numbered zero register
- named registers later
- clipboard optional in prototype

Register content should include linewise metadata.

```go
type Register struct {
    Text []byte
    Linewise bool
}
```

## Jumplist

Use file-aware locations from day one.

```go
type Jump struct {
    Location Location
    Mode Mode
}
```

Do not store view selection marks as jumplist data. That was the core issue in the vis jumplist picker.

## Buffers And Windows

Separate buffer identity from window/view state.

Buffer owns:

- text
- URI
- dirty state
- language
- LSP version

Window owns:

- buffer reference
- viewport
- selections
- local options

## File Operations

Implement file I/O behind a filesystem interface.

```go
type FileSystem interface {
    ReadFile(ctx context.Context, uri URI) ([]byte, error)
    WriteFile(ctx context.Context, uri URI, data []byte) error
    Stat(ctx context.Context, uri URI) (FileInfo, error)
    Walk(ctx context.Context, root URI, opts WalkOptions) ([]URI, error)
}
```

This enables tests without touching the real workspace.

## Large Files

Do not solve everything first. Define behavior:

- Warn above configurable threshold.
- Disable expensive syntax and LSP for very large files by default.
- Keep line index lazy or chunked later.

## Encoding

Initial assumption:

- files are UTF-8
- invalid bytes are preserved but displayed with replacement markers
- LSP-enabled buffers must provide valid UTF-8 to servers

Do not silently corrupt byte positions when converting to runes or UTF-16.

## Command Test Example

```go
func TestDeleteWord(t *testing.T) {
    e := testutil.NewEditor(t, "hello world\n")
    e.Dispatch(command.Invoke("select.word.next"))
    e.Dispatch(command.Invoke("edit.delete.selection"))
    testutil.AssertBuffer(t, e, "world\n")
}
```

This kind of test should be the default. Terminal-input tests should be additional coverage, not the only path.

## Open Questions

- Piece table or rope for first implementation?
- Full undo tree or linear undo stack first?
- How much multi-cursor support is needed before decision gate?
- Should select mode be a mode or a selection behavior flag?
- How soon do we need macros?

## Recommendation

Use a simple piece table and linear undo stack first. Build the APIs so the internals can be replaced. Spend complexity budget on selection correctness, command dispatch, and testability before optimizing storage.
