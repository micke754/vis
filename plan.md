# Helix Keymap Work Plan / State (feature/helix-keymaps)

## Branch
- `feature/helix-keymaps`

## Goal
Make vis support a toggleable Helix keymap profile (Lua-configurable and `:set keymap helix`), while staying upstream-friendly (Lua-first, minimal core changes).

## Current State (implemented)

### 1) Profile system
- Added keymap profile manager:
  - `lua/keymaps/init.lua`
- Profiles:
  - `lua/keymaps/vim.lua`
  - `lua/keymaps/helix.lua`
- Runtime switching:
  - `:set keymap helix`
  - `:set keymap vim`
- Config switching:
  - `vis.options.keymap = "helix"`

### 2) Startup integration
- `lua/vis-std.lua` loads keymap profile module.
- `lua/visrc.lua` includes keymap config examples.

### 3) Helix profile behavior implemented so far
- Normal + insert mode baseline mappings.
- Visual/visual-line mappings added for selection-first flow.
- Window mappings added (`<C-w>s`, `<C-w>v`, window navigation variants).
- Arrow keys enabled in normal mode.
- Insert `<C-k>` kill-to-EOL behavior added.

### 4) Selection-first behavior progress
- Implemented nuanced smart motion handlers for:
  - `w/e/b/W/E/B`
- Fixed repeated-word-motion scope issues.
- Fixed starting-character inclusion behavior for word motions.
- Added collapse behavior (`;`) and visual-normal transitions.
- Adjusted `hjkl` + goto keys behavior so they do not continue extending after selection-producing motions.

### 5) Docs/tests present
- Docs:
  - `doc/keymap-helix-plan.md`
  - `doc/keymap-helix-status.md`
- Lua tests:
  - `test/lua/keymap-profile.in`
  - `test/lua/keymap-profile.lua`

## What remains (Phase 2 completion)

1. Validate and refine `f/F/t/T` chain/reset semantics to match target Helix feel.
2. Validate `n/N/*` selection/motion behavior in normal vs select flows.
3. Verify select-mode consistency across `v`, `V`, `x`, `X`, `;` and transitions.
4. Run focused operator matrix (`d/c/y`) over:
   - single-cursor normal motions
   - selection mode motions
   - repeated motion chains
5. Decide and finalize any remaining deltas vs strict Helix behavior (document decisions).

## Recommended next work session
1. Manual test pass for motion/operator matrix.
2. Patch only failing edge cases (small Lua-only commits).
3. Stabilize tests for edge cases found.
4. Then mark Phase 2 complete and move to Phase 3.

## Notes for running on a different machine
- If using packaged vis (e.g., Nix), runtime Lua path may be wrapped.
- Reliable approach used during development:
  - symlink keymap files into `~/.config/vis/keymaps/`
  - use `~/.config/vis/visrc.lua` with:
    - `require('vis')`
    - `require('keymaps/init')`
    - `vis.options.keymap = "helix"`
- `VIS_PATH` one-shot launch may still work depending on wrapper behavior.

## Important
- Work is still uncommitted in this branch state; commit before switching devices if you want a clean handoff.
