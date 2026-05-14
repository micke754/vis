# Suckless Comparison

## Purpose

This document compares a minimal suckless-style Go terminal editor approach with our proposed Go prototype architecture.

The external analysis is useful because it pushes us toward simplicity: own the core, avoid heavy frameworks, use standard library facilities where practical, and keep the terminal editor understandable. We should adopt that pressure without losing the lessons from our vis work: modern LSP and AI workflows need typed domain models, typed async events, explicit UI state, and deterministic tests.

The target is not dependency-zero minimalism. The target is a small editor architecture that stays fast to develop as features grow.

## Summary Position

Use a suckless core, but not a suckless architecture ceiling.

We should own these pieces:

- text buffer
- selections
- command system
- keymap model
- picker
- render model
- LSP client shape
- AI request lifecycle

We should allow small dependencies where correctness or terminal behavior would otherwise consume disproportionate time.

Initial likely dependencies:

- `golang.org/x/term` for raw terminal mode and size handling.
- `github.com/rivo/uniseg` or `github.com/mattn/go-runewidth` for Unicode display width and grapheme behavior.

Initial explicit non-dependencies:

- no TUI framework by default
- no TOML/YAML/JSON config parser initially
- no OpenAI or Anthropic SDK initially
- no tree-sitter initially
- no third-party rope initially
- no `fzf` wrapper for core picker behavior

## Configuration Decision

Start with a `config.go` file.

This follows the suckless idea that early configuration can be source-level and recompiled quickly. It keeps the prototype simple while we are still changing command names, keymap shapes, LSP config, and AI config frequently.

Initial config should be plain Go data:

```go
package config

var Default = Config{
    Editor: EditorConfig{
        Theme: "default",
    },
    Keymaps: Keymaps{
        Normal: map[string]string{
            "space f": "picker.files",
            "space g": "picker.changed",
            "space d": "picker.diagnostics",
        },
    },
    Languages: []LanguageConfig{
        {
            Name: "go",
            Filetypes: []string{"go"},
            LSP: LSPConfig{
                Command: "gopls",
            },
        },
    },
}
```

This should be treated as a prototype choice, not a permanent product decision.

Revisit runtime config after the prototype proves the architecture. TOML, JSON, YAML, or a Lua/Starlark-like system should be evaluated later against real needs:

- user friendliness
- typed validation
- comments
- language server config shape
- AI provider config shape
- test fixture ergonomics
- no-toolchain user installs

## Text Buffer

### External Suggestion

Use a gap buffer or piece table from scratch with Go slices. Avoid editing immutable strings on every keystroke.

### Our Position

Agree.

Own the buffer. Start with a simple piece table or gap buffer and keep the API replaceable. Do not import a rope before we have measured actual prototype needs.

Important additions from our design:

- use explicit byte positions internally
- add line/column and UTF-16 conversion early for LSP
- keep visual column calculation separate from byte/rune/LSP positions
- make undo transactions restore selections, not only text

### Decision

Implement our own text store with an API boundary. Prefer piece table first unless a gap buffer proves faster to implement.

## Terminal Control And Input

### External Suggestion

Use `golang.org/x/term` for raw mode and write ANSI escape sequences directly. Avoid heavy TUI frameworks unless needed.

### Our Position

Mostly agree.

Use `x/term` initially, but hide raw terminal operations behind a `Terminal` interface. The editor must not depend directly on ANSI sequences outside the UI backend.

This keeps us free to switch to `tcell` later if keyboard input or terminal compatibility burns too much time.

### Decision

Start with a minimal ANSI backend behind an interface:

```go
type Terminal interface {
    Init() error
    Close() error
    Size() (width int, height int)
    ReadEvent(ctx context.Context) (InputEvent, error)
    Draw(Screen) error
}
```

Do not start with Bubbletea. Do not let a TUI framework own editor state.

## Rendering Engine

### External Suggestion

Use double buffering with a 2D cell array. Diff old and new screens, build output with `bytes.Buffer`, and avoid heavy `fmt.Sprintf` in hot render paths.

### Our Position

Agree.

This matches our proposed `Screen` model exactly.

Important additions from our design:

- render to a deterministic intermediate screen
- snapshot-test the screen model
- keep overlays as surfaces composed into the screen
- make render output a projection of state, not a place that mutates editor behavior

### Decision

Implement `Screen`, `Cell`, and ANSI diff rendering in-house.

## Configuration

### External Suggestion

Use `config.go` like `dwm`; users edit source and recompile.

### Our Position

Agree for the prototype, with a clear revisit point.

The prototype benefits from compile-time config because command IDs, keymap structure, LSP config, and AI config will change quickly. A runtime config format too early adds schema churn and parsing decisions before the product shape is stable.

But source-only config is likely not acceptable long term for users who do not want a Go toolchain or source edits.

### Decision

Use `config.go` for the prototype. Revisit runtime config after the LSP and AI flows are working.

## LSP And Syntax Highlighting

### External Suggestion

Use `os/exec` and `encoding/json` for LSP. For syntax highlighting, rely on LSP semantic tokens. Avoid tree-sitter because CGO hurts pure-Go distribution.

### Our Position

Partially agree.

Agree:

- use `os/exec` for language server processes
- use standard JSON support or a small JSON-RPC layer
- avoid tree-sitter in the initial prototype
- avoid CGO initially

Disagree:

- do not rely entirely on LSP semantic tokens for highlighting

Reasons:

- many files will not have an active language server
- language servers can be slow or absent
- semantic tokens often arrive after initial render
- some languages or filetypes may not support semantic tokens well

Fallback highlighting can be minimal. It can even start as no highlighting. The key design point is that rendering should not require LSP to produce usable text.

### Decision

Build LSP as a first-class async subsystem. Treat semantic tokens as one optional source of styling, not the only source.

## Multiple Cursors

### External Suggestion

Maintain a slice of cursors/selections and apply edits from bottom to top, then sort and merge overlapping cursors.

### Our Position

Agree.

This matches what we learned from Helix-style editing. Multi-selection edits must avoid invalidating later positions.

Important additions from our design:

- selections are ranges, not just cursors
- selection orientation matters
- primary selection matters
- undo should restore selection state
- commands should operate on selections directly

### Decision

Make bottom-to-top edit application a core invariant for multi-selection text changes.

## UI Picker

### External Suggestion

Use `io/fs` to gather files and write a simple subsequence matcher. Avoid wrapping `fzf`. Keep matching simple to avoid blocking the main thread.

### Our Position

Mostly agree.

Use a custom picker and custom matcher. Avoid `fzf` for core picker behavior.

But our picker must be more than a file finder. The picker is the shared UI for files, buffers, git changes, diagnostics, symbols, references, commands, jobs, and AI suggestions.

Important additions from our design:

- typed picker items
- provider-based picker sources
- common `Location` model
- preview abstraction
- deterministic provider tests
- async-capable item sources when needed

### Decision

Build the picker in-house with a simple matcher, but design it around providers and typed actions from day one.

## LLM And AI Ghost Text

### External Suggestion

Use `net/http` directly. Render ghost text as gray visual overlay rather than inserting into the buffer. Debounce requests and cancel in-flight HTTP calls with `context.WithCancel`.

### Our Position

Agree.

This aligns strongly with our goals.

Important additions from our design:

- provider interface for OpenAI-compatible endpoints and future local providers
- no network request unless configured
- explicit prompt builders
- preview/apply flow for edits
- redaction hooks
- job lifecycle and cancellation
- tests with fake AI provider

### Decision

Use `net/http` directly. No official SDK initially. AI suggestions should be overlays or previews until explicitly accepted.

## Unicode And Text Measurement

### External Suggestion

Decouple byte position, rune position, and visual width. Consider a minimal character-width table, or import `runewidth`.

### Our Position

Agree with decoupling. Prefer a small dependency for Unicode correctness.

Terminal cursor bugs are distracting and expensive. We should not hand-roll grapheme and width handling unless we have to.

Candidate dependencies:

- `github.com/rivo/uniseg` for grapheme clusters and width behavior
- `github.com/mattn/go-runewidth` for width behavior

### Decision

Use one small Unicode measurement dependency unless we find it too heavy. Lean toward `uniseg` for correctness.

## Dependency Policy

### Allowed Early

- `golang.org/x/term`
- `github.com/rivo/uniseg` or `github.com/mattn/go-runewidth`

### Avoid Early

- Bubbletea
- tcell, unless terminal work becomes a distraction
- tree-sitter bindings
- OpenAI/Anthropic SDKs
- third-party rope libraries
- fuzzy finder libraries
- TOML/YAML config libraries
- assertion libraries unless stdlib tests become too noisy

### Revisit Later

- runtime config parser
- tree-sitter or pure-Go parsing/highlighting options
- richer terminal input backend
- plugin scripting language
- telemetry-free crash/error reporting package

## Architecture Comparison

| Area | Suckless Approach | Prototype Approach |
|---|---|---|
| Buffer | own gap buffer or piece table | same, with explicit LSP/display position APIs |
| Terminal | x/term plus raw ANSI | same behind `Terminal` interface |
| Rendering | double-buffer ANSI diffs | same with snapshot-testable `Screen` |
| Config | `config.go` only | `config.go` for prototype, runtime config later |
| LSP | os/exec plus JSON | same, with typed manager/events/jobs |
| Syntax | semantic tokens only | semantic tokens optional, fallback allowed |
| Picker | simple file finder | generic provider-based picker |
| AI | net/http, ghost text, debounce | same, plus jobs, previews, redaction, fake tests |
| Unicode | minimal table or runewidth | use small dependency if needed |
| Tests | not central in summary | central design constraint |

## What We Should Not Compromise

Do not compromise these for minimalism:

- typed command dispatch
- typed async events
- file-aware locations
- deterministic tests
- picker provider abstraction
- cancellation for LSP and AI
- terminal backend abstraction
- byte/rune/LSP/display position separation

These are the specific areas where the vis exercise showed pain.

## Final Decision

Adopt the suckless implementation instincts, but keep our richer architecture boundaries.

The prototype should feel like a small editor, not a framework. But it must be built around the future workflows we actually want: multiple LSPs, diagnostics, pickers, and native AI/LLM completions.

The practical starting stack is:

- Go standard library for process, JSON, HTTP, filesystem, and tests.
- `golang.org/x/term` for terminal raw mode.
- `github.com/rivo/uniseg` or `github.com/mattn/go-runewidth` for Unicode display handling.
- `config.go` for initial configuration.

Everything else earns its way in later.
