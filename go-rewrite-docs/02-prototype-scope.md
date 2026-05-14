# Prototype Scope

## Objective

Build `wisp`, a Go editor prototype that validates whether we can move faster on modern editor features than we can inside the current C/Lua codebase.

The prototype should be useful enough to edit small files and demonstrate LSP, diagnostics, pickers, and AI workflows, but it should not attempt to replace vis immediately.

## Primary Questions

- Can we implement IDE-like features without fighting the architecture?
- Can we keep a sharp modal editing feel while adding async systems?
- Can we test picker, LSP, and AI behavior deterministically?
- Can we keep the codebase simple enough that new features do not require cross-language surgery?
- Can we build enough of the editor core in weeks, not months?

## Prototype Acceptance Criteria

The prototype is successful if all of these are true:

- Opening, editing, saving, undoing, and redoing a text file works reliably.
- Normal, insert, select, and picker modes are implemented.
- Keymaps are data-driven and invoke typed commands.
- File, buffer, workspace-changed, diagnostics, symbols, references, and command pickers share one picker implementation.
- A real LSP server can be started, initialized, fed buffer changes, and queried for diagnostics or symbols.
- Multiple LSP clients can exist at once, even if the initial UI only exercises one or two.
- AI/LLM request flow supports prompt construction, API call, streaming or non-streaming response, cancellation, and insertion/preview.
- Unit tests can validate commands, pickers, LSP event handling, and AI request logic without a real terminal.
- Integration tests can run with a fake or minimal LSP server.
- Adding a new picker source requires only provider code and keymap config, not UI rewrites.

## Prototype Non-Goals

- Full vis parity.
- Full Helix parity.
- Full Vim compatibility.
- Full plugin system.
- Full tree-sitter integration.
- Remote editing.
- GUI frontend.
- Perfect large-file performance.
- Compatibility with existing Lua plugins.
- Shipping-quality theme system.
- Every LSP method.

## Initial User Stories

### Basic Editing

- As a user, I can open a file, move around, insert text, delete selections, save, undo, and redo.
- As a user, I can use Helix-like selections where commands operate on selections first.
- As a user, I can switch between normal, insert, select, and picker modes.

### Navigation

- As a user, I can open a file picker from workspace root.
- As a user, I can open a buffer picker for open buffers.
- As a user, I can open a changed-file picker for git changes and dirty buffers.
- As a user, I can open a diagnostics picker and jump to selected diagnostics.
- As a user, I can open symbols and references pickers backed by LSP.

### LSP

- As a user, I can configure `gopls`, `rust-analyzer`, or another language server.
- As a user, I get diagnostics after opening or editing a file.
- As a user, I can jump to definition or references through a location picker.
- As a user, I can see progress and errors when LSP fails.

### AI

- As a user, I can send selected text, current buffer context, or diagnostics to an LLM endpoint.
- As a user, I can preview an AI suggestion before applying it.
- As a user, I can cancel an AI request.
- As a user, I can configure endpoint, model, timeout, and redaction behavior.

## Milestones

### Milestone 0: Skeleton

Build a compilable Go module with package layout, test harness, logging, config loading, and a minimal terminal abstraction.

Deliverables:

- `cmd/wisp/main.go`
- `internal/editor`
- `internal/buffer`
- `internal/ui`
- `internal/keymap`
- `internal/picker`
- `internal/lsp`
- `internal/ai`
- basic unit test setup

Exit criteria:

- `go test ./...` passes.
- A blank editor launches and exits.
- The installed executable can be invoked as `wisp`, with `ws` as the intended short alias.

### Milestone 1: Editor Core

Implement buffers, views, selections, modes, commands, undo, redo, and file open/save.

Deliverables:

- buffer insert/delete
- line/column conversion
- selection model
- command dispatcher
- normal/insert/select modes
- basic keymap

Exit criteria:

- Can edit and save a file.
- Command tests cover movement, insertion, deletion, undo, and selection behavior.

### Milestone 2: UI And Picker

Implement terminal rendering, layout, statusline, command prompt, and generic picker.

Deliverables:

- screen model independent from terminal library
- file picker
- buffer picker
- changed-file picker with injectable git provider
- picker preview
- picker open modes

Exit criteria:

- Picker tests do not depend on real repo state.
- Manual editing is tolerable for small files.

### Milestone 3: LSP

Implement LSP process management and JSON-RPC client.

Deliverables:

- LSP config
- server lifecycle
- initialize/shutdown
- text document sync
- diagnostics
- definition/references/symbols
- diagnostics picker

Exit criteria:

- One real language server works.
- Fake LSP tests cover diagnostics and request/response routing.

### Milestone 4: AI

Implement API-backed AI requests.

Deliverables:

- provider interface
- OpenAI-compatible HTTP adapter
- prompt builders for selection, file, and diagnostics
- cancellation
- preview/apply UI
- redaction hooks

Exit criteria:

- Fake provider tests cover prompt, cancellation, and apply behavior.
- Real endpoint can be manually tested with credentials outside git.

### Milestone 5: Decision Gate

Evaluate whether to continue rewrite, stop, or extract learnings back into vis.

Deliverables:

- velocity report
- defect report
- architecture debt report
- manual usability notes

Exit criteria:

- We can make a clear call on rewrite viability.

## Explicit Cut Lines

If time gets tight, cut in this order:

- Split panes.
- Theme customization.
- Tree-sitter.
- Completion menu polish.
- Complex surround/textobject parity.
- Macro recording.
- Multiple cursors beyond core selection operations.

Do not cut:

- deterministic tests
- typed command dispatch
- picker abstraction
- async event loop
- LSP lifecycle
- cancellation

## Success Metrics

- Time to add a new picker source is less than one day.
- Time to add a new LSP request type is less than one day after the first request is implemented.
- No test requires a real terminal unless marked integration.
- No unit test depends on the real git working tree.
- No feature requires editing more than one language.
- UI overlay state is owned by a typed model and can be inspected in tests.
- Background jobs can be canceled and cannot mutate editor state directly.

## Failure Signals

- Keymap behavior and command behavior drift apart.
- Tests must feed terminal strings for most coverage.
- LSP responses mutate buffers or UI directly from goroutines.
- Picker providers require custom rendering code.
- The buffer model blocks basic LSP position conversion.
- Large architecture decisions are deferred until after many features depend on them.
