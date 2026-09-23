---
kind: verb
library: SigilCompose
name: fontFamily
qualified: sigil::compose::Element::fontFamily
header: sigilcompose/core/verbs/Font.h
group: The cascade
status: stable
---

# fontFamily

The family everything under this node is set in, BY NAME — CSS's
`font-family`. `fontFamily("Georgia")` is the whole call: the composer's
font context finds the family's face when the cascade reaches the node,
at the weight and the style in force there.

## Description

**It is the face field of [`font`](font.md), spelled as a name.** A
family named here and a face stated through `font({.face = …})` are one
statement about one field, so whichever was written LAST stands, on a
node, in a class or in a rule alike.

**The face is chosen at the style in force.** Under an italic, a family
named further down is found at its italic, as CSS finds it; the weight
asked for is the one `fontWeight` states, or the face's own.

**An empty name is the context's default family** — what a node that
names nothing inherits at the root.

**A family the context cannot find leaves the inherited face standing**
and says so once, naming the family. Nothing stands in for it: there is
no fallback list and no generic family.

A span states a family over a range the same way,
`SpanDeclarations().fontFamily("Georgia")`, and the range is set again in
it.

## See also

[`font`](font.md), [`fontStyle`](fontStyle.md), `weave::FontContext`.
