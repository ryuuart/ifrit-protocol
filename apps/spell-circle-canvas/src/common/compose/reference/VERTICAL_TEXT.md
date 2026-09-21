# Vertical CJK

A chapter of [TYPOGRAPHY.md](../TYPOGRAPHY.md), the type chapter of
[SigilCompose](../README.md).

`block({.writingMode = …})` sets the passage running down the page.
`sigil::weave::WritingMode::kVerticalRL` is the CJK book layout: characters
top to bottom, columns advancing RIGHT TO LEFT from the node's right edge.
It is one field of the block lane, set on the passage or on any node above
it and inherited by every text leaf under that — plain text, `weave::rich()`
spans, and the paragraph overload alike, where a mode nobody names leaves
the paragraph's own mode standing.

```cpp
text(weave::rich(mincho)
         .add(u8"平成")
         .add(u8"31", tateChuYoko)
         .add(u8"年、縦組みに対応した。"))
    .width(260).height(300)
    .block({.writingMode = sigil::weave::WritingMode::kVerticalRL})
    .fx({.effect = fx::rise(24)});
```

**BOTH AXES ARE MEASURES.** A horizontal passage reads its width as the
measure and grows down; a vertical one reads its HEIGHT as how far a column
runs before the next one starts, and grows LEFT. So a vertical leaf's
intrinsic size swaps: one column of type measures tall and one column pitch
wide, and giving the node no height gives it one endless column. It has no
baseline — the reading axis is y and a column's glyphs centre themselves
across the axis rather than standing on one — so `Align::Baseline` gets its
first character's own baseline, which lines a column's opening character up
with a horizontal neighbour's first line.

**Per character the orientation is UTR#50's**: ideographs stand upright and
take their `vert` forms, Latin lies on its side. A run that wants otherwise
says so in its own style — `sigil::weave::VerticalForm` is `kAuto`,
`kUpright`, `kRotated` or `kTateChuYoko` — set on a `weave::rich()` run's
`sigil::weave::TextStyle` or through `spanStyle`. It is a SHAPING field, so
it re-shapes the words it covers and nothing else; there is no separate verb
because there is no separate concept. 縦中横 is the one to know: a short run
shaped horizontally and set upright across the column, which is how two-digit
numbers read in vertical prose.

**What else the face keeps for a column it hands over only when asked.**
Setting a run down the page applies the `vert` forms by itself; the wider
`vrt2` rotation set, punctuation recentred (`valt`) or fitted to its ink
(`vpal`, `vhal`), kana cut for a column (`vkna`) and vertical kerning
(`vkrn`) are named features, spelled as
`sigil::weave::features::verticalRotatedForms` and its siblings and set on
`shaping.fontFeatures` like any other. They are part of shaping identity, so
naming one re-shapes the runs it covers — and they are NOT gated on the
writing direction, so a style carrying them and set along a line takes them
there too.

**The engine runs in columns.** `weave::Unit::Line` IS A COLUMN here, so a
track with `.unit = weave::Unit::Line` beats column by column and
`weave::selectors::line(0)` addresses the rightmost one; `weave::Unit::Cluster`
runs down a column in reading order. `spanPaint`, `spanStyle`, the block's
alignment (start is the top of the column), `maxTextLines` (which clamps
COLUMNS) with `textOverflow` at the clamped column's foot, `contentFlowAround`,
the block's last line and breaking strategy, `textStroke`, `variationDrive` and
`feed()`'s text tier all work as they do across a line. `mark()` anchors as it
does anywhere — its rect is the union of the advance boxes its selector
addressed, and in a column those stack downward, so a phrase's mark is a tall
box standing in that phrase's column. `Element::textFill` maps its unit square
onto the COLUMN BLOCK rather than onto a cap band — a column's glyphs centre
across its axis instead of standing on a baseline, so there is no cap band to
hang a ramp on — which means a gradient authored in [0,1]² crosses the type
reading DOWN the page.

**Track deviations apply in the frame the layout placed the glyph in**, the
same rule a path baseline follows — and in a column the placed frame is the
glyph's own vertical pose. An UPRIGHT glyph is not turned, so its frame is
the canvas frame: `fx::rise` lifts it up the page. A ROTATED one is turned
to the column, so its frame is turned with it and a rise lifts it off its own
baseline, across the column. A glyph's pivot moves too: an upright glyph
turns and scales about the point on the COLUMN AXIS its pen reached, not
about a point half a column pitch to its right.

**`contentFlowAround`, the initial letter and `textOverflow` follow the type
down the page.** An exclusion cuts a COLUMN exactly as it cuts a line: the
column a target crosses hands back a head above it and a foot below it, and
the same silhouette is subtracted — a `shape()` outline, an analytic circle,
the target's traced coverage, or the box a target that declared none stands
in — with the margin the same disc standoff in all of them. An initial
letter cuts its notch out of the head of the columns it stands in, for the
same reason and by the same means: the notch is pen travel taken off a band,
and a column is a band. And a clamped column ends in its marker, at the
column's FOOT, measured against the column's length so the cut moves up to
make room for it. The marker stands for the text it cut and is set the way
that text was set: upright after upright glyphs, in the face's own vertical
form when it has one, and turned with the column after a rotated Latin run.

**What does not follow the type down the page.** `textOnPath` ignores the
writing mode entirely — a path run's baseline is its own geometry and has
no columns to advance — and setting both warns once and keeps the path. A
decoration on a span DOES follow the type down the page — an underline runs beside the
column on its right, an overline on its left, a strikethrough down the
column axis and a highlight across the whole column pitch — but it never
skips ink there, because ink intercepts are cut out of a horizontal band
window that a column's band is not. Which side an underline or an overline
takes is `side` on the decoration: the default puts a column's underline on
the right, the side a vertical setting reads its emphasis line on, and
`Decoration::Side::kOpposite` is the other placement (left of the column,
above the line).

**A BAND UNDER A TRACK STANDS AT REST**, in either writing mode. A track
draws its own glyphs in batched buckets and a bucket carries glyphs alone,
so the band is drawn beside them from the layout the letters left at rest:
the letters travel on their schedule and the band does not travel with
them. That is the same stand `mark()` takes — a rect resolved from the
layout cannot chase a paint-time pose — and it is the honest one for a
band, which dresses a whole run rather than one letter. Type on a path
carries no band either way: a turned run's band would have to follow the
curve it rides.

**Ruby and kenten are `Element::textAnnotation`**, in a column exactly as along
a line: the band a reading needs goes into the base's strut before the
base is broken, and the reading is then placed on the result, on the side
the writing mode reads its furniture on — above a line, to the RIGHT of a
column. `kit::ruby` and `kit::kenten` are the two stock spellings, and
mono, group and jukugo ruby are the unit choice and nothing else —
`weave::Unit::Selection` is the group one, and a group reading whose base
runs off the foot of a column is shared between the two columns rather
than read twice. See [BESIDE_TEXT.md](BESIDE_TEXT.md).
