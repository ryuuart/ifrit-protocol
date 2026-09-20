# SigilWeave — the flow geometry

The chapter on the shape a paragraph is poured into. `README.md` beside
the library is the front page; the pass that consumes a geometry is
`reference/LAYOUT.md`.

## Text is never bound to a rectangle

A "line" is an ordered list of `LineInterval` values — straight segments
in any direction, or spans of an `SkPath` contour — supplied one line at
a time by a `FlowGeometry`. The ready-made geometries cover the common
cases:

- `BlockFlow` — a single rectangle.
- `ExclusionFlow` — a rectangle minus moving flow shapes (a rect, a
  circle, an ellipse, any filled `SkPath`, an image's own alpha, or one a
  caller writes), in lines or in columns.
- `VerticalBlockFlow` — top-to-bottom CJK columns advancing right to
  left.
- `LineSetFlow` — an explicit set of intervals, any origin and direction.
- `PathFlow` — each `SkPath` contour becomes a line; glyphs ride the
  tangent through RSXform runs.

A contour is the geometry library's `geometry::path::Contour` — one
sub-path addressed by arc length — so "distance along" and "closed wraps
around" mean the same thing here as anywhere else a path is walked.

Implement `FlowGeometry` yourself for anything else, and pass the chosen
geometry to `layoutParagraph`.

`FlowAxis` is which way a flow's lines run, and therefore which way its
bands stack: horizontal lines stacking down the page, or top-to-bottom
columns advancing right to left. It is the writing mode said in the
geometry's own terms — a geometry never sees a paragraph — and a geometry
that offers both takes one of these.

## LineInterval

One stretch of pen travel a line of text may occupy. In its straight
form the pen starts at `LineInterval::origin`, a baseline point, and
travels along the unit vector `LineInterval::direction` for at most
`LineInterval::length`. In its path form — when `LineInterval::contour`
is valid — the pen instead travels the contour's arc length starting at
`LineInterval::contourStart`, glyphs are rotated to the local tangent,
and the origin and direction are ignored. A default-constructed contour
is "no contour" and leaves the interval straight.

`LineInterval::wrapContour` is for contour intervals only: WRAP at the
contour's ends rather than stop at them, so the pen may run round the
loop forever. A contour the path flagged closed wraps on its own; this is
for one that is closed in GEOMETRY without being flagged. A 359.9-degree
arc is a common spelling of a ring — losing half a centred caption off it
over a tenth of a degree is not a behaviour anyone wants.

`LineInterval::advanceScale` is arc length consumed per unit of glyph
advance, contour intervals only. It compensates curvature when the
glyphs' optical centres ride at a different radius than the measured
baseline contour — text on the outside of a small circle reads too loose
because the centres sit on a larger ring than the baseline, and a scale
of the baseline radius over the centre radius restores optical spacing.
The length, the fitting and the alignment arithmetic all stay in unscaled
advance units — set the length to the arc length over the scale to offer
the whole contour — and only the pen-to-arc mapping is scaled.

NEGATIVE walks the contour BACKWARDS: the pen still travels forward
through the text, but its arc position decreases and every glyph faces
the other way. That is how a run reads right way up along the lower half
of a ring — the run turns round once, rather than each letter turning
over and reversing the reading order.

`LineInterval::placeAt` maps a PEN COORDINATE on the interval — travel in
advance units from where the pen enters it — to the baseline point it
lands on and the unit direction it is turned to.

THIS IS THE PLACEMENT THE LAYOUT ITSELF BAKES. It is public so that a
caller re-placing a transformed run at draw time reads the same function
the blob was built from, and the two can never disagree about where a
glyph on a curve belongs. Anchor the glyph's ADVANCE CENTRE at the
returned point — the pen coordinate for a glyph is the pen at its start
plus half its advance — or accented glyphs drift off the curve.

The phase shifts every glyph along the contour by the same arc length,
which is how a marquee runs without laying the paragraph out again. A
contour that WRAPS — flagged closed, or opted in — takes the phase
forever and the pen may sit anywhere; one that does not clamps to its
ends. The rotation steps snap the direction to that many directions, 0
keeping it exact: every distinct rotation mints a glyph-atlas strike, so
an animated curve that does not snap re-rasterizes every glyph every
frame.

It returns false when the pen fell OUTSIDE a non-wrapping contour and the
result was clamped to its end, so a caller that would rather drop a glyph
than pile it on the last point can. A straight interval and a wrapping
contour always return true.

## LineRequest

ONE BAND ASKED OF A GEOMETRY, and everything about it the band's number
alone does not say.

`LineRequest::bandStart` is the whole of why this is a value rather than
three arguments. Bands do not stack at index times line height: that is
true only while every line of a passage is the same height. A text whose
blocks lead differently, or which puts air between them, stacks its bands
at distances the LAYOUT accumulates and a geometry could not work out
from a line number. So the layout carries that cursor and the geometry
answers what is available in the band that starts there.

The block context — `LineRequest::blockIndex` and
`LineRequest::lineInBlock` — is for a geometry that wants it: a frame
grid, a well cut for one block, a drop cap's notch. A geometry that does
not care ignores it, which is every geometry the library ships.

## FlowGeometry

Supplies the intervals available to each successive line.
Implementations are queried per layout pass — they may depend on animated
state like moving exclusion shapes — and the layout never caches geometry
between passes.

`FlowGeometry::lineIntervals` returns the intervals available in a
request's band, and returns false when the geometry is exhausted (the
band lies past its end); an empty interval list with a true return means
"this band has no room, try the next one". The overload taking an index
and a line height is sugar for a caller with no block model: bands
stacked at index times line height, which is where a passage of one pitch
puts them.

`FlowGeometry::uniformIntervals` is true when every line yields one
interval of the same width — TeX's model, which `BlockFlow` and its
relatives satisfy. Knuth-Plass uses it to merge paths that reached the
same breakpoint on different line numbers: their futures are identical,
so only the best survives and the active list stays bounded by the line
width instead of growing with the paragraph.

## The shapes text stands off

`Span` is ONE STRETCH OF A BAND a shape occupies, measured along the
flow's own axis — the units a line's pen travels in, and a column's.
`Band` is ONE BAND, measured ACROSS the flow: where a line's own depth
begins and ends, which is where a column's width begins and ends when the
flow is turned a quarter turn.

`FlowShape` is A SHAPE TEXT STANDS OFF. One question: which
stretches of a band this shape occupies, along the flow axis — the same
shape of answer for a rectangle, a photograph's alpha and anything a
caller writes, which is why there is no kind to switch on.

THE MARGIN IS A DISC AND NOT A SQUARE. It asks for the set of points
within that distance of the shape: a diagonal edge stands the text off by
exactly the margin and a corner comes out rounded. Every implementation
owes that meaning, because a caller asking two shapes for six pixels of
standoff is asking one question.

A flow shape CACHES what answering costs it — a flattening, a raster, a
distance field — so one belongs to one flow at a time, as an `SkPath`'s
own caches do.

`Exclusion` is ONE AREA TEXT FLOWS AROUND: a shape, how far the text
stands off it, and where it has moved to since. `Exclusion::offset` is
rigid motion and costs the shape nothing — the marquee, the drifting
figure, the parallax photograph — where a rebuilt shape re-answers from
scratch.

The stock flow shapes are in `flowshape`, and a caller with a shape none
of them describes implements `FlowShape` itself and stands beside them.

`flowshape::rectangle` is an axis-aligned rectangle, whose margin rounds
the corners exactly as a disc offset does. `flowshape::circle` is the
circle INSCRIBED in a box, answered analytically: one square root a band,
and the margin is simply a larger radius. `flowshape::ellipse` is the
oval inscribed in a box — the circle when it is round, and otherwise the
oval's own path, because a disc offset of an ellipse is not an ellipse
and only the path answer stays exact.

`flowshape::path` is any filled `SkPath` — several contours, curves,
winding or even-odd fill, so holes and concavities stay available to
text. It is flattened once and kept. An inverse fill type is read as its
own non-inverse self: a flow shape is the region the path encloses. The
answer is read off a flattened outline exactly, at any margin: with a
standoff the outline is the path unioned with itself stroked at twice the
margin, round join and round cap, which is what a disc rolled around the
shape sweeps.

`flowshape::coverage` is AN IMAGE'S OWN ALPHA, resolved inside a box in
flow coordinates: a pixel is inside where its alpha is greater than the
threshold, a fraction of full opacity. It is the answer for a photograph,
a rendered node, a video frame — a flow shape that is neither an outline
nor a glyph run. The tolerance is the dial: a soft edge admits words further
in as it rises. A new frame re-thresholds and re-measures; a still one
costs that once.

## ExclusionFlow

A rectangle with exclusions punched out — CSS float and `shape-outside`
style. Each band subtracts every intersecting flow shape's extent ACROSS
the band, so a line, or a column, shortens or splits into several
intervals around them. Exclusions are cheap to move: geometry is
re-evaluated per layout pass.

A COLUMN IS A LINE TURNED A QUARTER TURN, and `FlowAxis` is the whole of
the difference: `FlowAxis::kColumns` makes each band a top-to-bottom
column, the columns advancing right to left from the bounds' right edge,
and reads every flow shape's extent down the column instead of across the
line. Pair it with `Paragraph::setWritingMode`, exactly as
`VerticalBlockFlow` is paired.

`ExclusionFlow::setMinimumIntervalWidth` drops exclusion-created slivers
— intervals shorter than that much pen travel — that would otherwise
appear between shapes.

## The other stock geometries

`BlockFlow` is the classic paragraph block: horizontal lines filling a
rectangle.

`VerticalBlockFlow` is the vertical-RL block of CJK book layout: each
"line" is a top-to-bottom column, columns advancing right to left. The
line height is the column pitch; the interval origin sits on the column's
central axis, which is what vertical-shaped glyphs centre themselves on,
and the ascent is unused. Pair it with `Paragraph::setWritingMode`.

`LineSetFlow` is fully explicit geometry: the caller supplies every
line's intervals — arbitrary positions, directions and counts. Use it
when the text should land on shapes the block geometries cannot express:
scattered labels, hand-placed captions, one interval per animated slot.

`PathFlow` makes each contour of each path one line, and the glyphs
follow the curve.

## See also

`reference/LAYOUT.md` for the pass that asks a geometry for its bands.
