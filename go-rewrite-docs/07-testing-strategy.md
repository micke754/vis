# Testing Strategy

## Goals

- Make editor features testable without a real terminal, real git repo, real LSP server, or real network.
- Catch mode, picker, and async regressions before manual testing.
- Avoid hangs caused by ambiguous key input or unexpected overlays.
- Keep integration tests explicit and isolated.

## Lessons From Current Tests

The current Lua tests are valuable but have limits:

- Many tests rely on `feedkeys`, which exercises keymaps but obscures command intent.
- Picker tests can accidentally interact with real workspace state.
- Mode cleanup matters and can cause cascading failures.
- Opening a real picker in tests can hang when the test expected no overlay.
- Lua event handlers can rerun tests from window-open events unless guarded.

The Go prototype should test commands and state directly first, then add keymap coverage separately.

## Test Layers

### Unit Tests

Fast tests for pure or mostly pure components.

Targets:

- buffer edits
- line/column conversion
- selection transforms
- fuzzy matching
- key sequence parsing
- command dispatch
- git status parsing
- LSP message encoding
- prompt building

### State Tests

Construct an editor in memory, dispatch commands, inspect state.

Example:

```go
func TestChangedPickerUsesGitAndDirtyBuffers(t *testing.T) {
    e := testutil.NewEditor(t)
    e.Workspace.Git = git.FakeProvider{
        Changes: []git.Change{{URI: "file:///repo/a.go", Kind: git.Modified}},
    }
    e.OpenBuffer("file:///repo/b.go", "dirty")
    e.Buffer("file:///repo/b.go").Dirty = true

    err := e.Dispatch(command.Invoke("picker.changed"))
    require.NoError(t, err)

    p := testutil.ActivePicker(t, e)
    assert.Equal(t, []string{"a.go", "b.go"}, p.Labels())
}
```

### Keymap Tests

Feed canonical keys and assert dispatched commands or final state.

Keymap tests should be fewer than command tests.

Targets:

- `space f` opens file picker
- `space g` opens changed picker
- escape closes overlays
- normal/insert mode transitions
- terminal-reserved defaults are avoided

### Render Snapshot Tests

Render state to `Screen` and compare text/styled cells.

Targets:

- statusline
- picker list
- picker preview
- diagnostics underline or gutter marker
- AI preview surface

Snapshots should be stable and small.

### Async Tests

Use fake workers and controlled event delivery.

Targets:

- LSP diagnostics event updates store
- LSP references response opens picker
- AI streaming response updates preview
- cancellation stops job and leaves state consistent

### Integration Tests

Run real terminal, real git, or real language servers only when explicitly enabled.

Examples:

- `WISP_INTEGRATION=1 go test ./internal/lsp -run TestGoplsIntegration`
- `WISP_TERMINAL=1 go test ./internal/ui -run TestTerminalInput`

Integration tests should never be required for normal `go test ./...`.

## Test Harness

Create a test harness early.

```go
type Harness struct {
    Editor *editor.Editor
    FS *fstest.MapFS
    Git *git.FakeProvider
    Terminal *ui.FakeTerminal
    LSP *lsp.FakeServer
    AI *ai.FakeProvider
}
```

Helpers:

- `NewEditor(t, content string)`
- `OpenBuffer(t, uri, content)`
- `Dispatch(t, commandID, args)`
- `FeedKeys(t, keys...)`
- `AssertBuffer(t, uri, content)`
- `AssertMode(t, mode)`
- `ActivePicker(t)`
- `DrainEvents(t)`
- `CancelJob(t, jobID)`

## Fake Services

### Fake Filesystem

Use `testing/fstest` or a small URI-aware fake filesystem.

Needs:

- read file
- write file
- stat
- walk
- file change events later

### Fake Git Provider

No shelling out in unit tests.

```go
type FakeProvider struct {
    Changes []Change
    Err error
}
```

### Fake LSP Server

Support scripted responses.

```go
server.On("textDocument/references", func(req Request) Response {
    return Response{Result: []Location{...}}
})
```

### Fake AI Provider

Support streaming and cancellation.

```go
provider.Stream([]string{"hello", " world"})
provider.BlockUntilCanceled()
```

## Avoiding Hangs

Rules:

- Every test that feeds keys gets a step limit.
- Every event drain gets a timeout.
- Every async job gets a context deadline.
- Tests fail if an unexpected overlay remains open.
- Tests fail if there are leaked jobs after cleanup.

## Command Coverage Matrix

Maintain a small matrix for core behavior.

Columns:

- command ID
- unit tested
- keymap tested
- render tested if UI command
- async tested if background command

This prevents the C-action/Lua-keymap drift we saw in vis.

## Picker Test Matrix

Every picker provider should test:

- item generation
- empty state message
- filtering
- preview
- accept action
- cancel behavior
- no dependency on real workspace

Provider-specific tests:

- files provider respects ignore rules
- changed provider merges git and dirty buffers
- diagnostics provider groups by severity/source
- symbols provider handles nested symbols
- references provider dedupes locations
- command provider lists keybindings

## LSP Test Matrix

- server starts and initializes
- server startup error is reported
- didOpen sends correct text and version
- didChange increments version
- diagnostics update store
- references response opens location picker
- shutdown timeout kills process
- multi-server diagnostics do not overwrite each other

## AI Test Matrix

- no provider configured returns visible error
- prompt builder includes selected text when requested
- prompt builder respects max context bytes
- redaction removes configured secrets
- streaming updates preview
- cancellation stops provider
- accept inserts or replaces expected range
- reject leaves buffer unchanged

## Golden Files

Use golden files sparingly.

Good uses:

- screen snapshots
- prompt output
- serialized config validation errors

Bad uses:

- entire editor state dumps that change constantly
- large terminal escape outputs

## CI Defaults

Default CI should run:

- `go test ./...`
- race tests for async packages if runtime is acceptable
- static analysis with `go vet`
- formatting check

Optional CI job:

- real LSP smoke test
- terminal integration smoke test

## Manual Test Checklist

Manual tests should be short and scripted.

- open file
- edit and save
- undo/redo
- open file picker
- open changed picker with fake or temp git repo
- start LSP and show diagnostics
- open diagnostics picker and jump
- make AI request with fake endpoint
- cancel AI request

## Recommendation

Write test harness and fake services before LSP. If we defer testability, we will recreate the same friction we hit in vis.
