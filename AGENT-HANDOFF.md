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
- `w/e/b/W/E/B`: Helix-like selection-first behavior, now mostly C-backed behind `selectionsemantics=helix`.
- Lua profile should provide mappings/config glue, not word-motion feedkey hacks.
- `hjkl` and goto keys: should not keep extending selection after a prior selection-producing motion.
- `;` collapses back toward normal cursor flow.
- Normal search (`/`, `?`, `n`, `N`, `*`) should move/search without forcing selection.
- Visual search (`n`, `N`, `*`) should extend to full common match/word where possible.

## Manual smoke checklist
- `This string`: `ee`, `ww`, `bb` then `d` (check scope is correct)
- `This string-test_test.test`: repeated `w/e/b`
- `separator / skipped`: mid-word `w/e/b` partial selections
- `one-of-a-kind "modal"`: punctuation tokens/runs and WORD behavior
- `w` then `h/j/k/l` (should collapse/stop trailing extension)
- `w` then `gl/gs/ge` (should collapse/stop trailing extension)
- visual `<Space>w`, `<Space>q`, `<`, `>`
- normal `/`, `?`, `n`, `N`, `*` should not force selection
- visual `n`, `N`, `*` should extend to full common match/word where possible
- `:set keymap vim` and back to `helix`
- split windows with `<C-w>s` / `<C-w>v` and verify mappings in new window

## Commit strategy recommendation
- Commit current milestone before moving to `f/F/t/T`.
- Suggested split:
  1. docs/planning (`AGENT.md`, `plan.md`, handoff, Helix digests if desired)
  2. core C semantics (`vis.c`, `vis-motions.c`, `vis-core.h`, `vis.h`, `sam.c`, `vis-cmds.c`, `vis-modes.c`)
  3. Lua profile glue/keymaps/tests (`lua/keymaps/*.lua`, `lua/visrc.lua`, tests)
- One concern per commit when possible.
