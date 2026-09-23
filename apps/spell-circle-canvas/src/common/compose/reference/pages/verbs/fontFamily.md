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

**The face is chosen at the weight and the style in force, WHEREVER
EITHER MOVES.** The family is inherited by name, so a `fontWeight` or a
`fontStyle` stated further down finds the family's face at it again, as
CSS matches a face per element: `fontFamily("Georgia")` on a column and
`fontWeight(700)` on a paragraph in it sets the paragraph in Georgia's
bold, exactly as the two stated on one node would. Under an italic, a
family named further down is found at its italic. The weight asked for is
the one in force, or the face's own where none is stated.

**An empty name is the context's default family** — what a node that
names nothing inherits at the root.

**A keyword about the face** — `font()` with the face written as
`inherit` or `initial` — is a statement about this same field, so one in
a stronger layer stands over a family a weaker one named.

**A family the context cannot find leaves the inherited face standing**
and says so once, naming the family. Nothing stands in for it: there is
no fallback list and no generic family.

A span states a family over a range the same way,
`SpanDeclarations().fontFamily("Georgia")`, and the range is set again in
it; so does a rule about the name of a rich run, which the run takes as a
child of its passage would. A span or a run stating only a weight under a
family named above finds that family's face at it.

## See also

[`font`](font.md), [`fontStyle`](fontStyle.md), `weave::FontContext`.
