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
`font-family`, list and all. `fontFamily("Georgia")` is the whole call,
and so is `fontFamily("Inter, Helvetica Neue, sans-serif")`: the
composer's font context finds the first family of the list it has when
the cascade reaches the node, at the weight and the style in force
there.

A font file is a family like any other. In a sketch, the session's
resource hub hands every face it loads to the session's font context,
which adopts it under the family the file names, so loading the file once
is enough for its family to answer by name everywhere:

```cpp
ctx.assets.hub().load<weave::Face>(ctx.local("data/MachineType.ttf"));
compose::text("SIGNAL").fontFamily("Muthur Study");
```

An edited file is read again and the family answers with the new face the
next time it is asked for. A face already held is stated directly with
`fontFamily(face)`, which is the face itself and no lookup at all.

A family's style is named the way its font names it with
`fontFamily(family, styleName)`: `fontFamily("Avenir", "Book")` sets the
run in Avenir's Book, which a weight cannot reach, because Book and Roman
are both an upright 400.

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

**A list is read as CSS reads one.** Names are separated by commas; a
name may be quoted, `"Helvetica Neue"` or `'Helvetica Neue'`, which is
how one holding a comma is written, and an unquoted one is its words
with the space around and between them read as single spaces. The first
name the context finds a family for wins, and the rest are never asked.

**The generic names are the platform's families.** `serif`,
`sans-serif`, `monospace` and `system-ui`, in any case, stand for the
families a browser on the platform draws them in — on macOS Times,
Helvetica, Courier and the system interface face — when the context's
font manager is the system's, which answers them. A list ending in one
always finds a face there.

**The list chooses a face for the run, not for each character.** A
character the chosen face lacks is found through the context's own
fallback, exactly as it would be under a single family; the families
later in the list are not walked glyph by glyph.

**An empty name is the context's default family** — what a node that
names nothing inherits at the root.

**A keyword about the face** — `font()` with the face written as
`inherit` or `initial` — is a statement about this same field, so one in
a stronger layer stands over a family a weaker one named.

**A style named beside the family is the face the font names so.** The
name is compared with ASCII case and spaces aside, and found when the
cascade runs, through the composer's font context, so a family the context
adopted answers by the names its own files give their styles. The face
found is written into the font as `fontFamily(face)` writes one, its own
weight and style standing, and the family stays in force by name: a
`fontWeight` or a `fontStyle` written after it on the node, or anywhere
below it, overrides that trait as CSS would, the named face's other one
standing, and finds the family's face at them — Bold Italic with a weight
of 400 after it is the family's italic at 400 — while one written before
it on the same node gives way to the named face. A name no family of the list has is said once, naming both,
and the family is set at the weight and style in force.

**A list naming no family the context has leaves the inherited face
standing** and says so once, naming the whole list. Nothing else stands
in for it.

A span states a family over a range the same way, with the same string,
`SpanStyle().fontFamily("Didot, Georgia, serif")`, and the range is set
again in it; so does a rule, and a rule about the name of a rich run,
which the run takes as a child of its passage would. A span or a run stating only a weight under a
family named above finds that family's face at it.

## See also

[`font`](font.md), [`fontStyle`](fontStyle.md), `weave::FontContext`,
`weave::FontContext::adopt`, `weave::FontContext::familyTypeface`,
`weave::ports::genericFamilies`.
