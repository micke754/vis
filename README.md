# vis helix

A personal fork of [vis](https://github.com/martanne/vis).

This is a toy editor project that keeps the small C core and structural regex ideas from vis while borrowing selection-first editing from Helix. It is for personal use, not a polished distribution.

## Build

### Nix

Run from the source tree:

```sh
nix run
```

Build the package:

```sh
nix build
```

Enter a development shell:

```sh
nix develop
./configure
make -j2
```

The flake targets Linux and macOS, including x86_64 Linux and Apple Silicon Macs.

### Make

Install the build dependencies:

```text
libtermkey
ncurses
Lua 5.2 or newer
LPeg
TRE, optional
```

Then build:

```sh
./configure
make -j2
sudo make install
```

## Run

```sh
./vis path/to/file
```

Open a directory to start in the file picker:

```sh
./vis .
```

## Keymaps

Helix mode is the default.

```text
:set keymap vim
:set keymap helix
```

## Useful Features

```text
selection first motions
multiple selections
word and line selection
search selection
file and buffer picker
goto word labels
surround commands
textobjects
semantic repeat for selected Helix actions
Lua formatter and LSP setup
```

Lua formatter and LSP notes are in [`doc/lua-lsp-formatters.md`](doc/lua-lsp-formatters.md).

## Tests

Build first:

```sh
make -j2
```

Run the Helix keymap tests:

```sh
cd test/lua
for t in keymap-helix-*.lua; do LD_LIBRARY_PATH=../../dependency/install/usr/lib ./test.sh "$t" || exit 1; done
```

## Branches

`master` mirrors upstream vis.

`main` is this fork's default branch.

Feature branches should come from `main` and merge back by PR.

## Upstream

Original project:

https://github.com/martanne/vis
