---
kind: verb
library: SigilCompose
name: textIndent
qualified: sigil::compose::Element::textIndent
header: sigilcompose/core/verbs/Cascade.h
group: The cascade
status: stable
---

# textIndent

How far the first line of every block under this node is indented —
CSS's `text-indent`, in any [`Dimension`](../types/Dimension.md).
Negative hangs the first line out into the margin.

## Description

**A bare number is pixels**, and is the `firstLineIndent` field of
[`paragraph`](paragraph.md): `textIndent(24)` and
`paragraph({.firstLineIndent = 24})` are one statement.

**A length in the font is resolved WHERE IT IS STATED.** `em`, `rem`,
`lh`, `ch` and `pt`, a canvas length and a custom property become pixels
against the node that says them, and everything under it inherits the
pixels, as CSS computes the value: a container that says `2_em` at 10 px
indents a 20 px leaf under it by 20 px, not 40.

**A percentage is one of EACH PASSAGE's own measure** — its inline
size, which down a vertical passage is its columns' height. It is
inherited as the percentage, so `textIndent(10_pct)` on a column indents
a 160 px passage by 16 px and a 240 px one beside it by 24 px. A passage
with no bound measure — one on a path, one measured for its intrinsic
width — is not indented by one.

**A rule states every unit as an element does**, resolved at the element
it matches, and so does a rule about a paragraph style's name in
`paragraphStyles`. A `firstLineIndent` written as `inherit` or `initial`
in a stronger layer stands over an indent a weaker one gave in any unit.

**`auto` is no length** and is refused, said once; the indent in force
stands.

**The later of two statements stands**, whichever unit each used.

## See also

[`paragraph`](paragraph.md), [`Dimension`](../types/Dimension.md).
