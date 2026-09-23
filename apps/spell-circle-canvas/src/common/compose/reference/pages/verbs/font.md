---
kind: verb
library: SigilCompose
name: font
qualified: sigil::compose::Element::font
header: sigilcompose/core/verbs/Font.h
group: The cascade
status: stable
---

# font

The font everything under this node is set in, as a PARTIAL. It is the
inherited property CSS spells across `font-family`, `font-size`,
`font-weight` and the rest, said once as a value.

## Description

**The fields the partial names override; the rest inherit.**
`font({.size = 22})` is the inherited face and colour at another size.
A text leaf reads its own; a container's reaches every text under it
that does not say otherwise.

**Written twice on one node, the later call wins field by field**, and
so does a class written between them.

**A relative size resolves against the PARENT's font.**
`font({.size = 1.5_em})` is half again the size inherited, not half
again this node's own.

**Its longhands each write one field**: `fontSize`, `fontWeight`,
`letterSpacing`, [`fontFamily`](fontFamily.md) — the face, by its
family's name — and [`fontStyle`](fontStyle.md) — the slant, or the
italic face. A longhand and this verb are one statement, so the later
wins.

**The colour in it IS the [ink](ink.md).** A partial carrying a colour
sets the ink and drops a custom-property reference the ink was read
from before.

## See also

[`ink`](ink.md), [`paragraph`](paragraph.md), `weave::Type`, `styleClass`,
`role`.
