# Agent Handoff Notes (feature/helix-keymaps)

## Quick start for next Pi agent

1. Check branch and changes:
   - `git status --short --branch`
2. Read these first:
   - `plan.md`
   - `lua/keymaps/init.lua`
   - `lua/keymaps/helix.lua`
   - `test/lua/keymap-profile.lua`
3. Focus next on Phase 2 remaining items in `plan.md`.

## Current behavior intent
- `w/e/b/W/E/B`: selection-first with smart chaining behavior.
- `hjkl` and goto keys: should not keep extending selection after a prior selection-producing motion.
- `;` collapses back toward normal cursor flow.

## Manual smoke checklist
- `This string`: `ee`, `ww`, `bb` then `d` (check scope is correct)
- `w` then `h/j/k/l` (should collapse/stop trailing extension)
- `w` then `gl/gs/ge` (should collapse/stop trailing extension)
- `:set keymap vim` and back to `helix`
- split windows with `<C-w>s` / `<C-w>v` and verify mappings in new window

## Commit strategy recommendation
- Keep Lua-only focused commits.
- One concern per commit (motion family, then tests).
- Rebase-friendly, upstream-friendly patches.
