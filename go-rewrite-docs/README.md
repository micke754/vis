# Go Editor Prototype Design Docs

## Purpose

These docs define `wisp`, a Go prototype for a Helix-inspired terminal editor that can support first-party LSP, diagnostics, pickers, and AI/LLM workflows without recreating the C/Lua/UI boundary problems we hit while extending vis.

The goal is not to justify a rewrite by vibes. The goal is to build a timeboxed prototype that answers whether Go materially improves development velocity for the features we care about next.

## Recommendation

Build a small Go prototype instead of adding more IDE features directly to the current C/Lua codebase.

The prototype should prove these workflows end to end:

- Open, edit, save, and undo text in a terminal UI.
- Use Helix-like normal, insert, select, and picker modes.
- Open file, buffer, changed-file, diagnostic, symbol, reference, and command pickers from one picker model.
- Start one or more LSP servers and show diagnostics without blocking editing.
- Request AI/LLM suggestions through an API endpoint with cancellation and privacy controls.
- Run deterministic tests without depending on real repo state, real terminal state, or shell-specific behavior.

The canonical binary is `wisp`. The intended short executable alias is `ws`, similar in spirit to `hx` and `vi`.

## Non-Goals

- Do not clone all of vis before proving the prototype.
- Do not implement every Helix command.
- Do not build a plugin system first.
- Do not support every terminal edge case first.
- Do not target GUI, remote collaboration, or tree-sitter-first architecture in the initial prototype.
- Do not preserve C/Lua extension compatibility.

## Design Principles

- One implementation language for editor core, UI state, async jobs, LSP, and AI features.
- Domain objects first: buffers, views, selections, locations, diagnostics, jobs, pickers, commands, keymaps.
- Explicit state transitions instead of feedkey side effects as the main integration mechanism.
- Event loop owns mutation; background goroutines produce typed events.
- UI is a deterministic projection of state.
- Tests drive commands and inspect state, not terminal escape streams by default.
- Shell commands are adapters, not core architecture.
- The picker is infrastructure for IDE features, not a special file browser.
- LSP and AI are first-class systems, not plugins bolted onto subprocess callbacks.

## Document Map

- `01-lessons-from-vis.md`: what the vis exercise taught us and what to avoid.
- `02-prototype-scope.md`: prototype goals, non-goals, milestones, and acceptance criteria.
- `03-architecture.md`: proposed package layout, event loop, state model, and command architecture.
- `04-editor-core.md`: buffer, undo, selections, modes, commands, repeat, and keymap design.
- `05-ui-picker.md`: terminal UI, layout, overlays, picker model, preview, and changed-file handling.
- `06-lsp-ai.md`: LSP manager, diagnostics, completions, code actions, and AI/LLM integrations.
- `07-testing-strategy.md`: deterministic testing model and how to avoid the issues we saw in vis tests.
- `08-migration-and-decision-plan.md`: timeboxed plan, risks, metrics, and decision gates.
- `09-suckless-comparison.md`: comparison with a minimal/suckless Go editor approach and concrete prototype decisions.
- `10-agent-build-plan.md`: milestone-by-milestone implementation plan for an autonomous coding agent.
- `11-mvp-spec.md`: exact prototype behavior, commands, keybindings, and non-goals.
- `12-api-contracts.md`: initial Go interfaces and domain types the prototype should start from.
- `13-acceptance-tests.md`: required tests and verification commands per milestone.
- `AGENT.md`: root-agent template for the future `wisp` repository.

## Core Thesis

The current editor is excellent as a compact modal text editor, but not shaped for modern IDE and AI workflows. A Go prototype is worth building because the next features are dominated by structured concurrency, JSON-RPC, process lifecycle, UI overlays, typed state, and testability.

The rewrite is only justified if the prototype proves we can move faster while keeping the editing feel sharp.
