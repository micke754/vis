# UI And Picker Design

## Goals

- Make terminal rendering deterministic and testable.
- Treat overlays as first-class surfaces.
- Use one generic picker for files, buffers, changed files, diagnostics, symbols, references, commands, and AI actions.
- Avoid coupling picker tests to real filesystem or git state.

## Terminal Abstraction

Use a terminal interface rather than writing directly from editor logic.

```go
type Terminal interface {
    Init() error
    Close() error
    Size() (width int, height int)
    ReadEvent(ctx context.Context) (InputEvent, error)
    Draw(Screen) error
}
```

Initial candidates:

- `tcell`
- `bubbletea` lower-level pieces
- custom minimal terminal layer

Recommendation: start with `tcell` or a small wrapper around it. Do not let framework architecture own editor state.

## Screen Model

Render to an intermediate screen before terminal output.

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
```

Benefits:

- snapshot tests
- no terminal required for render tests
- easy overlay composition
- easier debugging of UI regressions

## Layout

Initial layout:

- one editor pane
- statusline
- command/prompt line
- overlay area for picker/completion/hover

Add splits after core workflows work.

## Surface Stack

Use a stack for UI surfaces.

```go
type Surface interface {
    Name() string
    HandleInput(*editor.Editor, InputEvent) (Handled, error)
    Render(*editor.Editor, Screen) error
}
```

Surface examples:

- main editor
- picker
- prompt
- completion menu
- hover
- AI preview

Input goes to the top surface first. If unhandled, it can fall through to lower surfaces only when explicitly allowed.

This avoids global fields like `picker.active` and makes lifecycle explicit.

## Picker Model

```go
type Picker struct {
    Title string
    Items []Item
    Filter string
    Filtered []int
    Selected int
    Preview PreviewState
    Accept AcceptHandler
    Source SourceID
}
```

```go
type Item struct {
    ID string
    Kind ItemKind
    Label string
    Detail string
    Location *Location
    URI *URI
    Preview PreviewRef
    Data any
}
```

Item kinds:

- file
- buffer
- location
- diagnostic
- symbol
- command
- text
- ai-suggestion

## Picker Providers

Providers build picker items. Picker core does not know source-specific logic.

```go
type Provider interface {
    ID() SourceID
    Items(ctx context.Context, e *editor.Editor) ([]picker.Item, error)
}
```

Providers:

- `FilesProvider`
- `BuffersProvider`
- `ChangedFilesProvider`
- `DiagnosticsProvider`
- `SymbolsProvider`
- `ReferencesProvider`
- `CommandsProvider`
- `AISuggestionsProvider`

## Picker Actions

Accepting an item should dispatch a typed command.

```go
type AcceptAction struct {
    Command command.ID
    Args command.Args
}
```

Examples:

- file item -> `buffer.open`
- buffer item -> `window.show-buffer`
- diagnostic item -> `location.open`
- symbol item -> `location.open`
- command item -> selected command
- AI suggestion -> `ai.preview-suggestion`

## Open Modes

Open modes should be explicit and validated per item kind.

```go
type OpenMode int

const (
    OpenCurrent OpenMode = iota
    OpenHorizontalSplit
    OpenVerticalSplit
    OpenPreview
)
```

File and buffer items can support split modes. Location and diagnostics can initially be current-window only. This avoids the ambiguity we had with jumplist and location pickers.

## Filtering

Use a simple fuzzy matcher first.

Requirements:

- stable order when scores tie
- deterministic scores for tests
- match label and optional detail
- highlight matched characters later

Do not optimize until large workspaces force it.

## Preview

Preview should be separate from picker item construction.

```go
type Previewer interface {
    Preview(ctx context.Context, item picker.Item, size PreviewSize) (Preview, error)
}
```

Preview types:

- file excerpt
- location-centered excerpt
- diagnostic detail
- symbol outline
- command help
- AI response preview

Location previews should center around the location line and highlight target range.

## Changed-File Picker

The changed-file picker should be a workspace provider that merges two sources:

- VCS changes from `git.Provider`.
- Dirty buffers from `BufferStore`.

```go
type ChangedFilesProvider struct {
    Git git.Provider
    Buffers *editor.BufferStore
}
```

It should return workspace changes, not just dirty editor buffers.

Git status should be injectable.

```go
type Provider interface {
    Status(ctx context.Context, root URI) ([]Change, error)
}

type Change struct {
    URI URI
    Kind ChangeKind
    Staged bool
    Untracked bool
    RenamedFrom *URI
}
```

Deleted files should either be excluded from open-file picker actions or shown with a special action for diff view later.

## Diagnostics Picker

Diagnostics should use the same picker model.

Diagnostic item fields:

- label: message summary
- detail: severity, source, relative path, line/column
- location: diagnostic range
- preview: location-centered file excerpt plus message

## Command Picker

The command picker should come early. It makes command discovery testable and helps avoid hardcoding too many key paths.

Command item fields:

- label: command name
- detail: help text and keybinding if any
- action: dispatch command

## AI UI

AI interactions should use picker and preview surfaces instead of inventing bespoke UI.

Initial flows:

- selected text -> AI request -> preview result -> accept insert/replace
- diagnostics -> AI request -> preview fix -> apply patch
- command picker item -> AI action

AI preview should be cancellable and should show progress.

## Status And Messages

Every user-invoked command should produce visible feedback when it cannot act.

Examples:

- `No changed files`
- `No diagnostics`
- `No LSP server for this buffer`
- `AI request canceled`
- `AI provider is not configured`

Silent no-ops caused confusion in the vis picker work.

## Keybinding Strategy

Normalize key notation internally.

Examples:

- `space g`
- `ctrl-o`
- `alt-s`
- `enter`

Do not treat literal space bindings and `<Space>` aliases as separate concepts internally. Parsing can accept both, but storage should use one canonical representation.

## Terminal-Reserved Keys

Avoid default bindings that conflict with common terminal behavior.

Known traps:

- `Ctrl-s` can freeze terminal flow control.
- `Ctrl-v` can be terminal literal-next.
- `Ctrl-i` and `Tab` may be indistinguishable.
- `Escape` and `Alt` sequences can be timing-sensitive.

Use alt bindings where practical and make defaults configurable.

## UI Tests

Test at three levels:

- picker state tests for filtering/navigation/actions
- render snapshot tests for overlay layout
- terminal integration tests for actual input decoding

Most tests should use the first two.

## Initial Picker Acceptance Tests

- file picker lists fake filesystem files
- buffer picker lists open buffers
- changed picker merges fake git changes and dirty buffers
- diagnostics picker opens selected location
- preview centers around selected location
- filter updates selected item and scroll offset
- cancel pops picker surface and restores previous mode
- accept dispatches expected command

## Recommendation

Build the picker earlier than LSP. LSP, git changes, symbols, references, and AI suggestions all need a high-quality picker. The picker is the prototype’s integration point for modern features.
