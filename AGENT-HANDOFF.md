# Helix Keymap Manual QA Checklist

## Word motions (normal mode)
- [ ] w  — select next word start
- [ ] b  — select prev word start
- [ ] e  — select next word end
- [ ] W/B/E  — bigword variants
- [ ] 2w, 3e, 2b  — counts
- [ ] ww, ee, bb  — repeated chains
- [ ] w on punctuation: "one-of-a-kind"
- [ ] w on single-char punctuation: "a.b"

## Operators (normal mode)
- [ ] wd  — delete selected word
- [ ] wc  — change selected word
- [ ] wy  — yank selected word
- [ ] wyp / wyP  — paste after/before
- [ ] bare d/c/y  — implicit 1-char cursor
- [ ] > / <  — indent/unindent
- [ ] w;d  — collapse then delete

## Select mode (v)
- [ ] v  — toggle select mode
- [ ] vww, vee, vbb  — extend
- [ ] vwb  — mixed directions
- [ ] ;  — collapse to cursor
- [ ] <Escape>  — exit select mode
- [ ] v2wd  — count + select + delete

## Line selection
- [ ] x  — select current line
- [ ] X  — extend
- [ ] xx, 2x, xd, xxd, xXd

## Find motions
- [ ] f/t  — select to/till char
- [ ] F/T  — backward
- [ ] vf/vt/vF/vT  — select mode

## Search
- [ ] *  — set pattern, no jump
- [ ] * ; n / * ; N  — collapse + match
- [ ] /<Enter>  — repeat forward
- [ ] ?<Enter>  — repeat backward
- [ ] * then / then <Enter>  — shared register
- [ ] vw * n / * N  — select mode extends

## Regex prompts (s/S/K/Alt-K)
- [ ] x s <Enter>  — empty = use * pattern
- [ ] x S <Enter>  — empty = use * pattern
- [ ] K / Alt-K  — keep/remove
- [ ] pattern from s persists across operations

## Multicursor
- [ ] C / 2C / Alt-C  — copy cursors
- [ ] , / Alt-,  — keep/remove
- [ ] ( / )  — rotate primary
- [ ] C w d, C x d  — multi delete
- [ ] C w y p / P  — multi paste
- [ ] C w Y p  — joined yank
- [ ] C w *  — star + multi

## Surround (m)
- [ ] w ms ( / " / [ / { / < / ' / `  — add
- [ ] w md ( / "  — delete
- [ ] w mr ( {  — replace
- [ ] C w ms "  — multi surround

## Textobjects (mi/ma)
- [ ] miw / maw  — word
- [ ] miW / maW  — WORD
- [ ] mi( / ma(  — parens
- [ ] mi" / ma"  — quotes
- [ ] mi[ / mi{ / mi<  — brackets

## Replace
- [ ] r  — replace selection with char
- [ ] R  — replace selection with yanked text
- [ ] vwr  — select word, replace with char
- [ ] vw y R  — select word, yank, select other word, replace with yanked
- [ ] ;R  — bare cursor R (no-op)

## Misc
- [ ] %  — select all
- [ ] Alt-s  — split on newlines
- [ ] Y  — joined yank
- [ ] u / U  — undo/redo
- [ ] :set keymap vim  — switch, w is cursor
- [ ] :set keymap helix  — switch back, w selects
- [ ] Statusline: NOR / SEL / INS

## Profile
- [ ] No visrc.lua → helix default
- [ ] require("keymaps").set("vim") → vim
- [ ] :set keymap helix → vim → helix
