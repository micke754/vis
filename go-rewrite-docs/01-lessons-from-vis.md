# Lessons From The vis Exercise

## Summary

The vis extension work produced useful features and exposed a clear architectural limit. The hard part was not just C. The hard part was adding modern editor concepts across an architecture where behavior, keymaps, UI, async events, and tests are split across C and Lua with a lot of implicit global state.

The Go prototype should preserve the good parts of vis while avoiding the seams that slowed us down.

## What Worked Well

- The core editing model is compact and fast.
- Text, view, selection, and command concepts already exist.
- The terminal UI is small enough to reason about.
- Modal editing maps naturally to a command table.
- Helix-like selections can be layered on top of the core with enough care.
- A single picker overlay can serve many workflows when items are structured.
- The build can be made reproducible and raw `make` can be made workable.

## What Became Expensive

### Feature Wiring Crosses Too Many Layers

A small feature often required changes in several places:

- Lua keymap profile.
- C action registration in `main.c`.
- State in `vis-core.h`.
- Behavior in `vis-picker.c`, `vis-helix.c`, `vis-marks.c`, or `vis.c`.
- UI redraw behavior.
- Lua tests or test fixtures.

This made small features feel large. The changed-file picker looked simple, but quickly touched keymap semantics, git state, open buffer state, picker lifecycle, and test isolation.

### Global Mutable State Is Convenient Until It Is Not

The current design leans heavily on active global state:

- `vis->win` for current window.
- `vis->mode` for current mode.
- `vis->input_queue` for pending keys.
- `vis->picker` and `vis->picker_preview` as singleton UI state.
- `vis->action` for command context.
- window-local mode overlays.

This is workable for synchronous editing. It becomes fragile when background systems need to update diagnostics, completions, progress, and picker sources.

### The C/Lua Boundary Is Not A Stable Product Boundary

Lua is good for configuration and lightweight extension. It is not a clean ownership boundary for features where UI, state, and async behavior must evolve together.

Examples:

- Keymap profiles live in Lua, but new actions require C registration.
- Subprocess responses enter Lua, but UI changes usually require C state.
- Tests can feed keys through Lua, but failures often come from C mode or UI lifecycle.
- Feature names and semantics drift between Lua mappings and C actions.

For LSP and AI, this would likely become worse. A server response would cross process, JSON, Go or shell, Lua event, C state, and UI overlay boundaries.

### Feedkeys Are Useful But Should Not Be The Primary Integration API

The current test and startup workflows often use `vis_keys_feed()` with symbolic key strings. This is good for regression coverage of keymaps. It is a poor foundation for feature integration.

Problems we saw:

- Ambiguous input prefixes can keep the editor waiting.
- Literal space bindings and `<Space>` aliases can behave differently.
- Mode cleanup matters before keymap switches.
- Tests can hang when a picker opens unexpectedly.
- Terminal-reserved keys like `Ctrl-s` and `Ctrl-v` create behavioral traps.

The Go prototype should support key-driven tests, but it should also expose command-level tests that call `editor.Dispatch(Command)` and inspect state directly.

### Picker State Needs To Be A First-Class Domain Object

The picker started as string-only entries and had to become structured:

- label
- path
- detail
- preview path
- line
- column
- kind
- open mode
- selection callback

This direction was correct. It should be the starting point in Go, not a refactor.

The picker should not know whether an item came from files, buffers, diagnostics, references, symbols, git changes, or AI suggestions. It should know how to filter, preview, render, and dispatch typed item actions.

### Location Data Must Be File-Aware From The Start

The jumplist problem came from trying to treat selection marks as location history. That worked within one current text, but failed for cross-file locations.

The prototype should use explicit location types everywhere:

```go
type Location struct {
    URI    URI
    Range  Range
    Source LocationSource
}
```

Jumplist entries, diagnostics, references, grep matches, symbols, and AI citations should all use a common location representation.

### Changed Files Must Mean Repository Changes, Not Only Dirty Buffers

The changed-file picker mismatch was semantic. The user expected git working-tree changes. The implementation listed unsaved open buffers.

The prototype should name sources precisely:

- Dirty buffers: modified in editor and not saved.
- Git changes: staged, unstaged, untracked, renamed, deleted.
- Workspace changes: union of dirty buffers and VCS changes.

The UI can call it “changed files”, but the model should not be ambiguous.

### Async Jobs Need Typed Ownership

The current subprocess model is useful but too generic for LSP and AI:

- process pool is global
- responses are byte chunks
- Lua events carry process names and response types
- lifecycle cancellation is indirect

The Go prototype should model each async system directly:

- `JobManager`
- `LSPClient`
- `AIClient`
- `GitService`
- `FileWatcher`

Each background worker should emit typed events into the main editor event loop.

### Test Isolation Must Be Designed In

A picker test hanging because it opened a real repo-dirty picker is exactly the kind of failure the rewrite must avoid.

The prototype needs injectable services:

- fake filesystem
- fake git status provider
- fake LSP server
- fake AI endpoint
- fake terminal input
- deterministic clock
- deterministic scheduler hooks where needed

Integration tests can use real git and real terminals later, but most tests should not.

## What To Preserve

- Modal command feel.
- Small, direct command implementations.
- Fast startup.
- Terminal-native operation.
- Minimal dependency count for core editing.
- Helix-like selection-first editing.
- Picker as central navigation UI.
- Ability to run without LSP, AI, network, or project metadata.

## What To Avoid

- Split ownership where one language owns keymaps and another owns feature behavior.
- Global singleton overlay state for every UI feature.
- Treating paths, buffers, and locations as interchangeable strings.
- Using shell commands as the main integration point for structured features.
- Tests that depend on the real working tree unless explicitly marked integration.
- Adding a plugin system before the core architecture is stable.
- Recreating every legacy behavior before validating future-facing workflows.

## Rewrite Hindsight Rules

- If a feature needs UI, state, async behavior, and tests, it should live entirely in Go first.
- If a feature returns locations, use the common `Location` type.
- If a feature opens UI, it should produce a `Surface` or `Overlay`, not mutate rendering directly.
- If a feature reads external state, put it behind an interface.
- If a feature can block, it must run in a worker and report typed events.
- If a command changes editor state, it should be testable without terminal escape sequences.
- If a keybinding invokes a command, the keybinding is not the command.
