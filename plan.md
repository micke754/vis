# Helix Keymap Work Plan / State (feature/helix-keymaps)

## Branch
- `feature/helix-keymaps`

## Goal
Make vis support a toggleable Helix keymap/profile (`:set keymap helix` / `vim`) with native-feeling Helix behavior.

Project priority has shifted for this fork:
- Correctness and feel for the local Helix workflow first.
- Lua remains the profile/config/mapping layer.
- C should own hard editor semantics when Lua becomes brittle.
- Preserve Vim behavior by gating Helix-specific C behavior behind explicit state/options.

## Current State (implemented)

### Direction update
- We are no longer forcing all Helix behavior into Lua.
- Use C for selection/motion/operator semantics where it is cleaner:
  - `w/e/b/W/E/B` range behavior
  - repeated motion chains
  - cross-line selection scope
  - operator range correctness
- Use Lua for:
  - profile registration and switching
  - key mappings
  - simple mode glue


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

### A) Add C-side Helix behavior gate
1. Add explicit core state for selection semantics, e.g. Vim vs Helix.
2. Default must remain Vim-compatible.
3. `:set keymap helix` should enable Helix semantics.
4. `:set keymap vim` should restore Vim semantics.
5. Keep Lua profile switching as the public interface.

### B) Move hard word-motion semantics into gated C
1. Keep/finalize cross-line `e` / `E` scope reset behind Helix semantics.
2. Audit `w/e/b/W/E/B` repeated chains under Helix semantics.
3. Simplify Lua smart motion wrappers once C owns the behavior.
4. Ensure operators use the corrected range:
   - `d`
   - `c`
   - `y`

### C) Remaining motion families
1. Validate and refine `f/F/t/T` chain/reset semantics to match target Helix feel.
2. Validate `n/N/*` selection/motion behavior in normal vs select flows.
3. Verify select-mode consistency across `v`, `V`, `x`, `X`, `;` and transitions.

### D) Validation matrix
1. Manual and/or tests over:
   - single-cursor normal motions
   - selection mode motions
   - repeated motion chains
   - cross-line motion chains
2. Decide and document any remaining deltas vs strict Helix behavior.

## Recommended next work session
1. Add C-side Helix selection semantics flag/option.
2. Gate the current cross-line `e` / `E` fix behind that flag.
3. Wire `:set keymap helix` / `vim` to toggle the C flag.
4. Rebuild and manually validate the known screenshot scenario.
5. Add/repair focused tests once current test harness segfault is isolated.
6. Continue moving brittle Lua smart-motion behavior into gated C.

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
