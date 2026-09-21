# SigilCompose typography

How a passage of type is written, dressed and read back — the chapter
`README.md` sends you to for anything more than `text(utf8, style)`.
Every mechanism here rests on SigilWeave, whose own `FEATURES.md` carries
the control-by-control table and names the verb on this side that reaches
each control.

- [Text fx](reference/TEXT_FX.md) — the multi-track per-glyph seam, its selectors, cascades and presets
- [Text on a path](reference/TEXT_PATH.md)
- [Mixed text](reference/RICH_TEXT.md) — `weave::rich()`, span restyling, and the
  layout setters
- [A passage whose input moves](reference/PARAGRAPHS.md) — `live`, the floor, and what a frame reports
- [Paragraphs, frames and stories](reference/PARAGRAPHS.md)
- [Beside the text](reference/BESIDE_TEXT.md) — `Composer::units`, annotations, and the kit over them
- [Vertical CJK](reference/VERTICAL_TEXT.md)

What a decoration dresses — `Element::decorationOutline`, and the three
mechanisms behind it — stays in the README, because a glyph boundary is
one of three answers and the other two are about shapes and images.

**Where the words live.** The TEXT'S OWN vocabulary is the paragraph
engine's and is spelled `weave::`: the content (`weave::rich`,
`weave::RichText`, `weave::Story`), the granularity (`weave::Unit`,
`weave::Unit::Word`), and selection with every form that names a position
in the text (`weave::Selector`, `weave::selectors::word`, `weave::selectors::regex`,
`weave::selectors::each`). Include them from `<sigilweave/paragraph/RichText.h>`,
`<sigilweave/layout/Story.h>`, `<sigilweave/paragraph/Unit.h>` and
`<sigilweave/query/Selector.h>`. What this library adds is the DRESSING —
the track, the effect, the reading, the path, the unit as the layout
placed it — and the two selector forms whose subject is a description of
this library: `selectors::style`, a run written under a name, and
`selectors::inFrame`, one frame of a chain named by its `Element::key`.

**A leaf is a box, and `Element::padding` insets it.** The paragraph is
laid out in the content box — the leaf's box less its padding — and drawn
there, so the measure the lines break at is the room inside the padding
and the node comes out one padding larger than its lines on both axes. A
fill on the leaf is then the scrim the reading stands ON, which is why a
run needing air around it takes padding directly rather than a box around
it. The first baseline the leaf reports to `Align::Baseline` stands below
the padding with the letters. A run riding a curve is the exception: its
glyphs stand on the baseline `Element::textOnPath` resolves against the
node's box, and nothing insets that.
