# Migration And Decision Plan

## Purpose

This document defines how to evaluate the Go prototype without accidentally committing to a rewrite too early.

The decision should be based on demonstrated development velocity, architecture quality, and user experience, not just frustration with the current codebase.

## Current Assessment

The current editor is strong for compact modal editing. It is weak for modern IDE and AI workflows because those require structured async systems, typed domain models, and testable UI overlays.

The C/Lua split is especially concerning for LSP and AI because those features naturally cut across:

- process lifecycle
- JSON-RPC or HTTP
- background events
- editor state
- UI surfaces
- keymaps
- tests

Adding a Go sidecar would improve process and protocol handling, but the UI and editor-state boundary would remain hard. Every meaningful feature would still need a C/Lua integration path.

That is why a Go prototype is the right next experiment.

## Decision Gates

### Gate 1: Skeleton And Core Editing

Question: Can we rebuild enough editing core quickly without drowning in fundamentals?

Pass criteria:

- normal and insert mode work
- file open/save works
- basic selections work
- command tests are easy to write
- code remains small and understandable

Fail criteria:

- text storage dominates the project
- mode/keymap design already feels tangled
- tests require terminal hacks

### Gate 2: Picker And Workspace

Question: Can we build picker-based workflows cleanly?

Pass criteria:

- file, buffer, changed-file, and command pickers share one model
- changed-file picker uses fake git in tests and real git manually
- picker preview is testable
- adding a provider is straightforward

Fail criteria:

- picker becomes a special case for each source
- tests depend on real workspace state
- overlay lifecycle becomes implicit

### Gate 3: LSP

Question: Does Go materially improve LSP implementation velocity?

Pass criteria:

- one real language server works
- fake LSP tests are deterministic
- diagnostics flow into UI without blocking
- references/symbols use picker naturally
- multiple clients are plausible without redesign

Fail criteria:

- JSON-RPC or process lifecycle consumes excessive effort
- state mutation from goroutines becomes unsafe or confusing
- LSP position conversion is unreliable

### Gate 4: AI

Question: Can AI workflows reuse the same architecture instead of adding a new tangle?

Pass criteria:

- provider interface is simple
- cancellation works
- preview/apply flow is explicit
- prompts are testable
- no network request happens without config

Fail criteria:

- AI needs bespoke UI that bypasses surfaces
- prompt construction is untestable
- applying edits is unsafe or unclear

## Timebox

Recommended timebox: 3 to 5 focused weeks.

Suggested split:

- Week 1: skeleton, buffer, commands, keymap, tests
- Week 2: UI, picker, workspace, git provider
- Week 3: LSP lifecycle, diagnostics, location picker
- Week 4: symbols/references, AI provider, preview/apply
- Week 5: hardening, evaluation, decision memo

If this cannot show meaningful progress by week 3, stop and reassess.

## What To Do With vis During Prototype

Do not continue major IDE work in vis while the prototype is being evaluated.

Allowed vis work:

- bug fixes
- build fixes
- small keymap fixes
- documentation updates

Avoid vis work:

- LSP implementation
- AI integration
- large picker rewrites
- async subsystem expansion

The prototype needs a clean comparison against the friction we already observed.

## Migration Paths If Prototype Succeeds

### Path A: Full Rewrite

Continue the Go editor until it replaces current daily usage.

Pros:

- clean architecture
- one language
- first-class LSP and AI
- easier testing

Cons:

- must rebuild mature editor features
- risk of long parity gap
- users lose Lua plugin compatibility

### Path B: New Editor With Narrow Scope

Ship a new Go editor focused on Helix-like editing plus IDE/AI workflows, without chasing full vis compatibility.

Pros:

- avoids parity trap
- optimizes for target experience
- faster to make coherent

Cons:

- not a drop-in replacement
- some vis features remain missing

### Path C: Use Prototype As Design Source, Keep vis Minimal

Use the Go prototype to validate architecture, then selectively backport tiny concepts to vis.

Pros:

- lower disruption
- preserves current editor

Cons:

- original boundary problem remains
- modern features still hard

## Migration Paths If Prototype Fails

If the prototype fails, that is useful information.

Possible outcomes:

- Text editor core is too costly to rebuild.
- Go terminal UI is not better enough.
- LSP complexity dominates regardless of language.
- The desired product should be built on top of an existing editor instead.

Fallback options:

- keep vis as lightweight modal editor
- use Helix or Neovim for IDE-heavy workflows
- build only standalone LSP/AI tools that communicate through files or CLI

## Risk Register

### Text Engine Risk

Risk: rebuilding buffer, undo, and selections takes too long.

Mitigation: start simple, defer large-file optimization, and evaluate early.

### Terminal UI Risk

Risk: terminal input/rendering edge cases consume time.

Mitigation: use a mature terminal library and keep a fake terminal for tests.

### Parity Trap Risk

Risk: trying to replicate all of vis delays future-facing features.

Mitigation: define prototype non-goals and enforce them.

### LSP Complexity Risk

Risk: LSP is complex even in Go.

Mitigation: implement minimal request set first and use fake server tests.

### AI Product Risk

Risk: AI features are easy to demo and hard to make safe/useful.

Mitigation: require preview/apply, cancellation, redaction, and no-network defaults.

### Architecture Drift Risk

Risk: prototype shortcuts become permanent architecture.

Mitigation: write decision records for shortcuts and revisit at gates.

## Staff-Level Recommendation

Proceed with the Go prototype, but keep it disciplined.

The evidence from the vis work says future features will be slow and brittle if built directly into the current C/Lua architecture. A Go sidecar would improve protocol handling but not solve UI and state ownership. A full rewrite is risky, but a prototype is the right way to reduce uncertainty.

The prototype should focus on the exact areas that hurt:

- typed picker sources
- file-aware locations
- deterministic tests
- async LSP events
- AI request lifecycle
- unified command/keymap model

If those feel materially better in Go, continuing the rewrite is justified. If they do not, we should stop before rebuilding too much editor surface area.

## Decision Memo Template

At the end of the prototype, write a short decision memo.

Required sections:

- What was built
- What was not built
- Time spent by milestone
- Features that were easier than in vis
- Features that were still hard
- Test quality assessment
- Performance notes
- UX notes
- Architecture debt introduced
- Recommendation: continue, stop, or pivot

## Final Bias

Bias toward proving the new architecture with real workflows, not toward preserving old implementation choices.

The lesson from this exercise is not “C is bad”. The lesson is that modern IDE and AI features need explicit domain models, typed async events, and testable UI state. Go is a strong fit for that if we keep the scope sharp.
