# Acceptance Tests

## Purpose

This document defines the tests and manual checks that prove the `wisp` prototype is coherent.

The implementation agent should create these tests milestone by milestone. It should not wait until the end.

## Default Verification

Normal verification:

```sh
go test ./...
```

This command must not require:

- real terminal
- real git repository
- real LSP server
- real network
- API keys

Optional integration tests:

```sh
WISP_INTEGRATION=1 go test ./internal/lsp
WISP_TERMINAL=1 go test ./internal/ui
```

## Milestone 0 Tests

### `TestVersionCommand`

Package: `cmd/wisp` or integration-style command test.

Expected:

- `wisp --version` exits successfully
- output contains `wisp`

### `TestHelpCommand`

Expected:

- `wisp --help` exits successfully
- output contains `usage: wisp [file]`
- output mentions alias `ws`

### `TestEditorConstructs`

Package: `internal/editor`

Expected:

- editor can be created from `config.Default`
- command registry is non-nil
- keymap manager is non-nil
- surface stack is non-nil

## Milestone 1 Tests

### `TestBufferInsertDelete`

Package: `internal/buffer`

Scenario:

- create buffer with `hello world\n`
- insert `big ` at byte position 6
- delete `world`

Expected text:

```text
hello big 
```

### `TestBufferLineColumnConversion`

Scenario:

- create buffer with multiple lines
- convert byte positions to line/column
- convert line/column back to byte positions

Expected:

- round trips for ASCII text
- invalid line returns error

### `TestUnicodePositionSeparation`

Scenario:

- buffer contains ASCII, CJK, emoji, and tabs

Expected:

- byte positions are not assumed equal to screen columns
- LSP UTF-16 conversion has dedicated test coverage before LSP milestone is complete

### `TestSelectionCollapse`

Package: `internal/selection`

Expected:

- range selection collapses to cursor
- primary selection remains valid

### `TestDeleteSelectionCommand`

Package: `internal/editor` or `internal/command`

Scenario:

- create editor with `hello world\n`
- select `hello `
- dispatch `edit.delete-selection`

Expected text:

```text
world
```

### `TestUndoRedoRestoresSelections`

Expected:

- delete selection changes text
- undo restores text and selection
- redo reapplies text and selection state

## Milestone 2 Tests

### `TestParseKeySequences`

Package: `internal/keymap`

Inputs:

- `space f`
- `ctrl-n`
- `alt-s`
- `shift-tab`
- `escape`

Expected:

- parser returns canonical key sequences
- invalid key notation returns error

### `TestKeymapResolveExactAndPrefix`

Scenario:

- bind `space f`
- feed `space`
- feed `space f`

Expected:

- `space` resolves as prefix
- `space f` resolves exact command

### `TestInsertModePrintableInput`

Scenario:

- switch to insert mode
- feed printable text

Expected:

- text inserted at cursor

### `TestScreenRenderSnapshot`

Package: `internal/ui`

Scenario:

- render buffer with statusline

Expected:

- screen contains mode, file name, dirty state, and cursor location

No terminal required.

## Milestone 3 Picker Tests

### `TestFilePickerUsesFakeFS`

Expected:

- fake filesystem files appear in picker
- ignored paths are excluded
- no real filesystem access occurs

### `TestBufferPickerListsOpenBuffers`

Expected:

- open buffers appear as picker items
- accepting item switches active buffer

### `TestChangedPickerMergesGitAndDirtyBuffers`

Scenario:

- fake git returns `a.go`
- editor has dirty buffer `b.go`

Expected:

- picker contains both items
- duplicate URI appears once

### `TestChangedPickerEmptyMessage`

Expected:

- no git changes and no dirty buffers does not open empty picker
- editor message is `No changed files`

### `TestPickerFiltering`

Expected:

- typing filter updates filtered item list
- selected index resets safely
- empty filter restores all items

### `TestPickerCancelRestoresMode`

Expected:

- open picker from normal mode
- cancel picker
- mode is normal
- surface stack no longer contains picker

### `TestPickerAcceptDispatchesAction`

Expected:

- accepting file item dispatches `buffer.open`
- accepting diagnostic item dispatches `location.open`

### `TestPickerPreviewLocationCentered`

Expected:

- preview for line 50 includes surrounding lines
- target line is marked or styled in screen model

## Milestone 4 LSP Tests

### `TestJSONRPCEncodeDecode`

Package: `internal/lsp`

Expected:

- request with Content-Length encodes correctly
- decoder handles one response
- decoder handles partial reads

### `TestLSPInitializeFakeServer`

Expected:

- client sends initialize
- fake server responds
- client records capabilities

### `TestLSPDidOpenAndDidChange`

Expected:

- opening buffer sends didOpen with text
- editing buffer increments version
- didChange sends full document text for MVP

### `TestDiagnosticsUpdatedEvent`

Expected:

- fake server publishes diagnostics
- editor receives typed event
- diagnostics store updates on editor loop

### `TestDiagnosticsPicker`

Expected:

- diagnostics provider returns picker items
- item detail includes source, severity, path, line, column
- accepting item jumps to location

### `TestNoLSPServerMessage`

Expected:

- requesting diagnostics or symbols without server shows `No LSP server for this buffer`

## Milestone 5 Symbol And Reference Tests

### `TestDocumentSymbolsPicker`

Expected:

- fake LSP returns document symbols
- symbols provider opens picker
- accepting symbol jumps to location

### `TestReferencesPicker`

Expected:

- fake LSP returns multiple references
- references picker lists them
- accepting item jumps to file and range

### `TestSingleDefinitionJumpsDirectly`

Expected:

- one location response jumps directly without picker if command chooses direct behavior

## Milestone 6 AI Tests

### `TestAIProviderNotConfigured`

Expected:

- AI command without provider shows `AI provider is not configured`
- no network call occurs

### `TestPromptBuilderSelectionContext`

Expected:

- selected text is included
- file path is included
- context byte limit is respected

### `TestAIRedaction`

Expected:

- configured redaction pattern removes secret-like text from prompt

### `TestAIStreamingPreview`

Expected:

- fake provider streams chunks
- preview surface updates
- buffer remains unchanged before accept

### `TestAICancel`

Expected:

- cancel command cancels request context
- job status is canceled
- message is `AI request canceled`

### `TestAIAcceptReplaceSelection`

Expected:

- selected text replaced with fake AI result
- undo restores original text

### `TestAIRejectLeavesBufferUnchanged`

Expected:

- rejecting preview closes surface
- buffer text is unchanged

## Integration Tests

### Real Git Smoke Test

Optional manual or integration test.

Setup:

- create temp git repo
- create tracked modified file
- create untracked file

Expected:

- changed provider lists both

### Real Terminal Smoke Test

Optional with `WISP_TERMINAL=1`.

Expected:

- raw mode enters and exits safely
- terminal is restored after exit

### Real gopls Smoke Test

Optional with `WISP_INTEGRATION=1` and `gopls` installed.

Expected:

- opening Go file starts server
- diagnostics or symbols request returns without panic

### Real AI Endpoint Smoke Test

Manual only unless explicitly configured.

Expected:

- API key comes from environment
- request can be canceled
- response previews before applying

## Hangs And Leaks

Every test harness should enforce:

- event drain timeout
- no leaked jobs after test cleanup
- no unexpected open surfaces unless test asserts them
- context deadline for fake LSP and AI operations

## Final MVP Manual Checklist

- `go test ./...` passes
- `go run ./cmd/wisp --help` works
- `go run ./cmd/wisp --version` works
- open small file
- enter insert mode and type
- return to normal mode
- save file
- undo and redo
- open file picker with `space f`
- open buffer picker with `space b`
- open changed-file picker with `space g`
- open command picker with `space c`
- fake or real diagnostics picker with `space d`
- fake AI preview/apply flow works

## Acceptance Philosophy

These tests are not busywork. They are the mechanism that prevents the Go rewrite from recreating the fragility we saw in the C/Lua codebase.
