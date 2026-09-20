---
kind: verb
library: SigilCompose
name: spanStyle
qualified: sigil::compose::Element::spanStyle
header: sigilcompose/core/Text.h
group: Content
status: stable
---

# spanStyle

Restyle the range this selector finds — a different face, size, weight
or tracking as well as paint — with a complete `weave::TextStyle`, or
with a partial laid over the style the range is already set in.

## Syntax

```cpp
Element& spanStyle(sigil::weave::Selector where,
                   sigil::weave::TextStyle style);
Element& spanStyle(sigil::weave::Selector where, sigil::weave::Type partial);
```

## Description

**It re-shapes, and only the words the range covers.** The shaping cache
is content-addressed, so the rest of the paragraph is reused as it
stands.

**A style differing only in advance-invariant AXES does not re-shape at
all.** A grade thickens a letter without moving the letter after it, so
the coordinate is held on the glyphs at draw time and the layout the
paragraph already has stands to the pen position. It is then a track
carrying a variable-axis effect, and it inherits what that means: the
same size-scaled snapping ladder a driven axis takes, composition with
entrances and loops rather than being hidden by them, and the batched
glyph draw, where a span style's band stands at its rest placement while
the letters move. An axis the face moves advances on, an axis the
restyle drops, or any other difference is a reshape, and a later
reshaping restyle over the same text keeps the earlier one a reshape
too, so the later one stands.

**A partial that names no shaping field is a repaint.** A colour, a
decoration or a pass never re-shapes; a face, a size, tracking or
features do. A size in ems is of the style the range is set in.

## The two restyles together

Both verbs take an ordered list — call either as many times as the
passage needs — and a LATER DECLARATION WINS wherever two overlap, so a
broad rule followed by a narrow exception reads in the order it is
written. They apply to every content form alike: plain text, rich
spans, and a paragraph handed over whole, because all three are one
materialized paragraph by the time a restyle runs.

They are ordered by WHAT THEY ARE ALLOWED TO DISTURB.
[`spanPaint`](spanPaint.md) repaints and nothing else; this one may
change anything. So "later wins" holds PER DIMENSION where the two
meet: the PAINT of a range is `spanPaint`'s to say, so a `spanStyle`
over text an earlier `spanPaint` coloured applies its other dimensions
and leaves that colour standing. Either order therefore does the same
thing, and neither verb has to know what the other declared.

Both run on the PARAGRAPH and resolve their selection as TEXT RANGES,
not glyphs: `weave::selectors::text` and `weave::selectors::regex` go
through weave's query layer, `weave::selectors::word`,
`weave::selectors::words` and `weave::selectors::range` through the
paragraph's own structure, and `weave::selectors::line` through the
layout. `weave::Selector::take` and `weave::Selector::drop` slice GLYPHS
inside a unit, which a text range cannot express, so an
`weave::selectors::each` selector restyles its whole units here and the
slice is ignored with a warning.

A `weave::selectors::line` restyle addresses THE LAYOUT OF THE TEXT
BEFORE THE RESTYLE, and costs a second layout pass. It does not chase
its own result: a restyle on a line that moves the line breaks leaves
the selection where the first breaking put it.

## See also

[`spanPaint`](spanPaint.md), `fx`,
[`variationDrive`](variationDrive.md), `weave::selectors::`.
