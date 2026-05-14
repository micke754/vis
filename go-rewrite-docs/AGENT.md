# AGENT.md For wisp

## Purpose

This file is the root-agent template for the future `wisp` repository.

When creating the new repository, copy this file to the repository root as `AGENT.md`. Keep the design docs in `go-rewrite-docs/`.

This document tells coding agents how to operate in the repo and which design docs to consult when they get stuck.

## Project Identity

- Project name: `wisp`
- Canonical binary: `wisp`
- Short executable alias: `ws`
- Language: Go
- Initial config: `internal/config/config.go`
- Default verification: `go test ./...`

## Required Reading Order

Before writing code, read:

1. `go-rewrite-docs/README.md`
2. `go-rewrite-docs/10-agent-build-plan.md`
3. `go-rewrite-docs/11-mvp-spec.md`
4. `go-rewrite-docs/12-api-contracts.md`
5. `go-rewrite-docs/13-acceptance-tests.md`

Use the rest of the docs as references while building.

## Doc Routing

If making architecture decisions, read:

- `go-rewrite-docs/03-architecture.md`
- `go-rewrite-docs/09-suckless-comparison.md`

If implementing buffers, selections, undo, repeat, or commands, read:

- `go-rewrite-docs/04-editor-core.md`
- `go-rewrite-docs/12-api-contracts.md`

If implementing terminal UI, overlays, picker, previews, or changed files, read:

- `go-rewrite-docs/05-ui-picker.md`
- `go-rewrite-docs/13-acceptance-tests.md`

If implementing LSP, diagnostics, symbols, references, or AI, read:

- `go-rewrite-docs/06-lsp-ai.md`
- `go-rewrite-docs/12-api-contracts.md`

If writing or fixing tests, read:

- `go-rewrite-docs/07-testing-strategy.md`
- `go-rewrite-docs/13-acceptance-tests.md`

If unsure what to build next, read:

- `go-rewrite-docs/10-agent-build-plan.md`

If scope feels too large, read:

- `go-rewrite-docs/02-prototype-scope.md`
- `go-rewrite-docs/08-migration-and-decision-plan.md`

If considering dependencies, read:

- `go-rewrite-docs/09-suckless-comparison.md`

## Build Order

Follow milestones in `go-rewrite-docs/10-agent-build-plan.md`.

Do not skip ahead to LSP, AI, or plugin-like abstractions before the core command, buffer, UI, and picker milestones are working.

## Dependency Policy

Allowed initial external dependencies:

- `golang.org/x/term`
- `github.com/rivo/uniseg`

Do not add these without explicit human approval:

- Bubbletea
- tcell
- tree-sitter bindings
- OpenAI SDK
- Anthropic SDK
- third-party rope libraries
- fuzzy finder libraries
- TOML, YAML, or JSON config libraries
- assertion libraries

Use the Go standard library by default.

## Configuration Policy

Use `internal/config/config.go` for the prototype.

Do not add TOML, JSON, YAML, Lua, Starlark, or another runtime config format during the MVP unless explicitly requested.

Runtime config will be revisited after LSP and AI flows are proven.

## Implementation Rules

- Build vertical slices.
- Keep `go test ./...` passing before moving to the next milestone.
- Prefer small, direct code over broad framework abstractions.
- Keep editor state mutation on the editor loop.
- Background goroutines must emit typed events, not mutate editor state directly.
- Keybindings invoke typed commands; they are not the command implementation.
- UI renders state; rendering should not own business logic.
- Picker providers generate typed items; picker core should not know git, LSP, or AI details.
- Locations must be file-aware from the beginning.
- Tests should use fake filesystem, fake git, fake LSP, fake AI, and fake terminal where possible.
- Normal unit tests must not require real git, real terminal, real LSP server, real network, or API keys.

## Naming Rules

- Use `wisp` for package/binary naming where a product name is needed.
- Use `cmd/wisp` for the main command.
- Document `ws` as the short alias.
- Do not use `govis`.
- Do not use names tied to AI, Helix, or vis internals.

## Stop Conditions

Pause and ask for direction if any of these happen:

- a new external dependency seems necessary outside the approved list
- the text buffer implementation is blocking milestone progress
- terminal input requires a switch to a full TUI backend
- LSP position conversion is ambiguous or unreliable
- AI design starts requiring a plugin system or complex patch engine
- tests start depending on real workspace state
- a feature requires direct goroutine mutation of editor state
- scope expands toward full vis or Helix parity
- config.go becomes insufficient before LSP and AI are working

## Verification Commands

Run after code changes:

```sh
go test ./...
```

Run for UI-heavy changes:

```sh
go test ./internal/ui ./internal/picker
```

Run optional integration tests only when explicitly configured:

```sh
WISP_INTEGRATION=1 go test ./internal/lsp
WISP_TERMINAL=1 go test ./internal/ui
```

## Git Rules

- Do not commit unless explicitly asked.
- Do not amend commits unless explicitly asked.
- Do not run destructive git commands unless explicitly approved.
- If the worktree has unrelated changes, leave them alone.

## Documentation Rules

- Update `go-rewrite-docs/12-api-contracts.md` when changing major public package contracts.
- Update `go-rewrite-docs/13-acceptance-tests.md` when adding or removing milestone acceptance criteria.
- Update `go-rewrite-docs/10-agent-build-plan.md` if milestone order changes.
- Keep docs practical. Avoid architecture essays in implementation docs.

## Default Bias

The goal is a working prototype that proves or disproves the rewrite direction.

Prefer a small working `wisp` over an elegant unfinished framework.
