# Helix keymap profile plan (phase 1)

## Goals

- Add a toggleable keymap profile system in Lua.
- Support `vim` (default) and `helix` profiles.
- Support both config and runtime switching:
  - `vis.options.keymap = "helix"`
  - `:set keymap helix`
- Keep changes lean and upstream-friendly by avoiding editor core changes.

## Scope (phase 1)

- Normal mode mappings.
- Insert mode mappings.
- Runtime profile switching for open windows.
- Auto-apply profile to newly opened windows.

## Out of scope (phase 1)

- Visual/operator-pending parity with Helix.
- Full Helix feature parity for LSP/tree-sitter actions.
- Deep semantic changes in vis core.

## Implementation strategy

- Keep upstream/default vis mappings unchanged.
- Apply keymap profiles as **window-local overlays**.
- For helix profile:
  1. Shadow existing normal+insert mappings with `<vis-nop>` window-local mappings.
  2. Add helix mappings window-locally.
- For vim profile:
  - Remove overlay mappings and expose defaults again.

## Iteration checklist

1. Profile manager module + `:set keymap` option.
2. Helix normal mode baseline.
3. Helix insert mode baseline.
4. Tests for runtime switching.
5. Track behavior differences for phase 2.
