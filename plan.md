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
- Word motions are now mostly C-backed under Helix selection semantics:
  - `w/e/b/W/E/B`
- Removed old Lua `smart_word_motion()` feedkey wrappers.
- Added Helix-style word categories:
  - whitespace
  - EOL
  - word (`alnum` + `_`)
  - punctuation runs
- Fixed repeated-word-motion scope issues.
- Fixed starting-character inclusion behavior for word motions.
- Fixed punctuation handling (`-`, `.`, punctuation runs, `_` as word char).
- Fixed mid-word partial selections for `w/e/b`.
- Fixed horizontal whitespace inclusion for `w`/`b` selections.
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
Status: mostly complete.

Done:
1. Cross-line `e` / `E` scope reset is behind Helix semantics.
2. `w/e/b/W/E/B` now use Helix-gated C motion/range logic.
3. Lua smart motion wrappers were removed.
4. Common edge cases fixed:
   - punctuation tokens/runs
   - `_` word membership
   - first `w` selecting current word
   - repeated `e`
   - mid-word partial selections
   - horizontal whitespace inclusion for `w`/`b`

Remaining:
1. Audit full operator matrix using corrected ranges:
   - `d`
   - `c`
   - `y`
2. Continue checking line boundary / EOF cases.
3. Longer-term: replace Vim visual-mode piggybacking with true Helix normal/select mode semantics.

### C) Remaining motion families
1. Validate and refine `f/F/t/T` chain/reset semantics to match target Helix feel.
2. Search behavior:
   - Normal mode `/`, `?`, `n`, `N`, `*` should move/search without entering selection.
   - Helix profile sets `ignorecase` on.
   - Visual-mode `n`, `N`, `*` now attempts to extend to full match/word.
   - Known limitation: because current implementation piggybacks on Vim visual mode, some search-selection edge cases remain until a true Helix normal/select mode exists.
3. Verify select-mode consistency across `v`, `V`, `x`, `X`, `;` and transitions.

### D) Validation matrix
1. Manual and/or tests over:
   - single-cursor normal motions
   - selection mode motions
   - repeated motion chains
   - cross-line motion chains
2. Decide and document any remaining deltas vs strict Helix behavior.

## Recommended next work session
1. Commit current word-motion/search/keymap milestone.
2. Run focused manual smoke:
   - `w/e/b/W/E/B` over punctuation, whitespace, line boundary, EOF.
   - visual `<Space>w`, `<Space>q`, `<`, `>`.
   - normal `/`, `?`, `n`, `N`, `*` are motion-only.
   - visual `n`, `N`, `*` extends to full common matches.
3. Run operator matrix over word selections:
   - `wd`, `ed`, `bd`, `wwd`, `eed`, `bbd`
   - `c`/`y` equivalents.
4. Move to `f/F/t/T` chain/reset semantics.
5. Longer-term: design true Helix normal/select mode to remove remaining Vim visual-mode limitations.

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
