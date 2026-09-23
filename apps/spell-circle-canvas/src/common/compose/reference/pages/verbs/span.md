---
kind: verb
library: SigilCompose
name: span
qualified: sigil::compose::Text::span
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# span

Restyle the range a selector finds with `compose::Declarations` — the
font fields and the ink they state, laid over the style the range is
set in, the rest standing.

## Description

**The same text declarations the passage was styled in.** A
`Declarations` value carries the element's own font and ink verbs —
`font`, its longhands `fontFamily`, `fontSize`, `fontWeight`,
`fontStyle` and `letterSpacing`, and `ink` — so a range is restyled in
the words the passage itself was:

```cpp
text(copy)
    .span(weave::selectors::regex(u8"[0-9]+"),
          Declarations().fontWeight(700).ink(accent))
    .span(weave::selectors::text(u8"storm"),
          Declarations().fontStyle(-12).letterSpacing(1));
```

**It re-shapes only where a shaping field was declared, and only the
words the range covers.** A face, a size, a weight or a tracking is a
reshape; a colour, a decoration, a pass, a paint or a baseline shift
(`font({.baselineShift = 4})`) alone is a repaint, which never re-shapes
and never relayouts. The shaping cache is
content-addressed, so the rest of the paragraph is reused as it stands.
A size in ems is of the style the range is set in.

**A span differing only in advance-invariant AXES does not re-shape at
all.** A grade thickens a letter without moving the letter after it, so
the coordinate is held on the glyphs at draw time and the layout the
paragraph already has stands to the pen position. It is then a track
carrying a variable-axis effect, and it inherits what that means: the
same size-scaled snapping ladder a driven axis takes, composition with
entrances and loops rather than being hidden by them, and the batched
glyph draw, where a span's band stands at its rest placement while the
letters move. An axis the face moves advances on, or any other
difference, is a reshape, and a later reshaping span over the same text
keeps the earlier one a reshape too, so the later one stands.

**The ink is read three ways.** A colour is the range's colour. A custom
property, `ink(var("accent"))`, is read where the leaf lands. A static
paint collapses to one shader laid in the passage's own coordinates, as
it is — a ramp meant to span the whole leaf belongs on the leaf's own
[`ink`](ink.md), which maps it onto the leaf's box. A live or
geometry-dependent paint has no one shader to give a range and is left
out.

## Several spans

Spans are an ordered list — call the verb as many times as the passage
needs — and a LATER DECLARATION WINS wherever two overlap, so a broad
rule followed by a narrow exception reads in the order it is written.
They apply to every content form alike: plain text, rich spans, and a
paragraph handed over whole, because all three are one materialized
paragraph by the time a span runs.

"Later wins" holds PER DIMENSION. A reshaping span is laid over the
whole style the range is set in, so it carries a paint whether or not
its author stated one; over text an earlier paint-only span coloured,
it applies its other fields and leaves that colour standing. Either
order therefore does the same thing.

Spans run on the PARAGRAPH and resolve their selection as TEXT RANGES,
not glyphs: `weave::selectors::text` and `weave::selectors::regex` go
through weave's query layer, `weave::selectors::word`,
`weave::selectors::words` and `weave::selectors::range` through the
paragraph's own structure, and `weave::selectors::line` through the
layout. `weave::Selector::take` and `weave::Selector::drop` slice GLYPHS
inside a unit, which a text range cannot express, so an
`weave::selectors::each` selector restyles its whole units here and the
slice is ignored with a warning.

A `weave::selectors::line` span addresses THE LAYOUT OF THE TEXT BEFORE
THE SPAN, and costs a second layout pass. It does not chase its own
result: a span on a line that moves the line breaks leaves the
selection where the first breaking put it.

## See also

[`ink`](ink.md), [`font`](font.md), `fx`,
[`variationDrive`](variationDrive.md), `weave::selectors::`.
