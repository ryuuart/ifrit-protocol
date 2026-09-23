---
kind: verb
library: SigilCompose
name: fontStyle
qualified: sigil::compose::Element::fontStyle
header: sigilcompose/core/verbs/Font.h
group: The cascade
status: stable
---

# fontStyle

How the type under this node leans — CSS's `font-style`, as a
[`FontStyle`](../types/FontStyle.md): `FontStyle::Normal`,
`FontStyle::Italic`, or `FontStyle::oblique(degrees)`.

## Description

**A bare number is an oblique of that many degrees, POSITIVE LEANING
RIGHT**, as CSS counts it: `fontStyle(14)` and
`fontStyle(FontStyle::oblique(14))` are one statement, and
`FontStyle::oblique()` with no angle is CSS's 14. The angle is the
`slant` field of [`font`](font.md) NEGATED, because the face's `slnt`
axis counts a lean to the right as negative: `fontStyle(12)` and
`font({.slant = -12})` describe the same node.

**An oblique leans only a face with a `slnt` axis.** A face without one
stands upright under it; nothing is sheared, so an italic falling back
to the oblique on such a face stands upright too, and the warning is the
one sign of it.

**`Italic` is a different face, not a lean.** The family's italic face
where it has one, else its `ital` axis set to 1, else — the family having
neither — an oblique of 14 degrees, which the composer says once, naming
the family. The italic is inherited beside the face, so a family named
further down is found at its italic, and `FontStyle::Normal` under an
italic stands the type upright again.

**A style written after another replaces it**: an oblique after an
italic is the oblique alone. A rule, a span and a rule about a rich run's
name state the style the same way.

**A face loaded from a file is its own only face.** Its italic is its
`ital` axis or the lean, never an installed family's face that happens
to share its family's name.

## See also

[`font`](font.md), [`fontFamily`](fontFamily.md), `fontWeight`.
