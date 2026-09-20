# SigilWeave — choreography, decoration and query

The chapter on the three optional layers over a finished layout: walking
its glyphs and dressing them one by one, resolving the bands a decoration
draws, and saying which of a passage a caller means. `README.md` beside
the library is the front page.

## Per-glyph choreography

Per-glyph choreography utilities — the "letters leave their lines"
pattern: rain, ripples, marquees, staggered reveals. It is an optional
layer, and nothing in the core pipeline includes it.

The recipe: lay the paragraph out normally, walk every placed glyph with
`forEachPlacedGlyph`, dress it however the effect wants — displaced,
rotated, faded, tinted, drawn through another face, or placed by a full
matrix where an RSXform cannot reach — and accumulate into
`GlyphRSXformBatches`, which is one `SkCanvas::drawGlyphsRSXform` call
per font-and-paint-pass pair instead of thousands of per-glyph draws.

A `PlacedGlyph` carries the identity an effect selects and staggers on —
which glyph of which word, line, style span and sentence it is — beside
the geometry it draws with, and the span's complete `PaintStyle`, so an
animated letter keeps the gradients, strokes and glow passes its span was
styled with.

### PlacedGlyph and the walk

One glyph of a finished layout, as an effect sees it: where it rests,
what it draws with, and where it sits in the text — which glyph of which
word, line, style span and sentence — beside the tangent and pen
coordinate a glyph on a curve was placed with. Every index is a fact
about *this* layout of *this* paragraph; nothing here is stored during
layout.

`forEachPlacedGlyph` visits every placed glyph of a layout in draw order,
each with its rest position, its span's paint, and its position in the
text.

Enumeration order is stable across relayouts as long as the text itself
is unchanged — which is what lets per-glyph particle state keyed by the
ordinal survive a per-frame relayout. The first walk after a text edit
resolves sentence boundaries once, through `Paragraph::sentenceStarts`;
every later walk of unchanged text reuses them.

### GlyphDress

How one glyph is dressed for a batched draw — where it lands, what it is
faded and tinted by, which face it draws with, and the matrix a shear or
a non-uniform scale needs — with the two helpers a dressing reaches for:
the rotation snap that keeps tumbling letters from minting a fresh glyph
mask per frame, and the memoized colour filter that folds a tint, a flash
and a glow into one matrix a batch can key on by pointer.

Everything on a `GlyphDress` is per-GLYPH and nothing on it is per-pass: a
dressed glyph still draws its span's whole `PaintStyle`, one bucket per
pass.

`GlyphDress::alphaScale` multiplies every pass's alpha. Quantize it if an
effect drives it continuously — distinct alphas are distinct buckets.

`GlyphDress::colorMultiplier` multiplies every pass's colour, channel by
channel. A pass painting a flat colour multiplies that colour; a pass
painting a shader, or already carrying a colour filter, gets the
equivalent modulating filter, so a gradient keeps its ramp and takes the
tint over it. Alpha here folds into the alpha scale, and white is no
tint.

`GlyphDress::colorAdd` is added to every pass's colour after the
multiply, clamped at the draw — the flash a multiplier cannot brighten
into. RGB only; the alpha component is never read, coverage being the
alpha scale's lane. Zero is no flash, and keeps the untouched-paint fast
path.

`GlyphDress::colorScreen` is screened over every pass's colour after the
add — c becomes 1 − (1 − c)(1 − screen) — the glow that lifts each
channel by its headroom and never clips. RGB only, as the add is, and
zero is no glow. Screening against a constant is affine per channel, so
all three colour terms ride the one memoized matrix filter together.

`GlyphDress::face` is the face to draw with, or null for the shaped
word's own — a varied clone for a glyph whose effect drives a
variable-font axis. It is part of the bucket key, so two faces are two
buckets.

`GlyphDress::centreOffset`, when non-null, is the glyph-local vector from
the glyph's DRAW ORIGIN to the pose centre — the point the rotation and
the scale turn about. Null keeps the horizontal convention, half the
advance to the right. A vertical column needs it: an upright glyph's
advance runs down the page while the glyph itself is drawn from a
horizontal origin, so half its advance to the RIGHT is half a column
pitch away from anything the eye would call its centre. It is borrowed
for the duration of the call.

`GlyphDress::matrix`, when non-null, draws the glyph under that matrix
instead of an RSXform, which is the only way to place a shear or a
non-uniform scale. It carries the whole placement — centre, rotation and
scale included — so the centre, cosine and sine are unread when it is
set. It too is borrowed for the duration of the call.

### The two helpers

`quantizeAngle` snaps an angle to a cosine and sine on a 64-step table,
about 5.6 degrees per step and visually indistinguishable for tumbling
letters: continuous per-letter angles would re-rasterize every glyph mask
every frame on the CPU raster backend. The GPU backend does not need
this, but it does not hurt there either.

The overload taking a step count is the same snap on a CALLER-CHOSEN
ladder: that many directions round the circle, computed rather than
tabled because a caller cuts the ladder by rendered glyph size — one step
turns an outline by a fixed angle, a fixed angle displaces a glyph's
extremity by more pixels the larger the glyph is drawn, so a size-blind
ladder that vanishes on a caption ticks visibly on display type. At 64
steps it answers bit for bit what the tabled overload answers. A step
count of zero or less is the exact angle — the continuous opt-out spelled
as a ladder of none.

`tintFilter` returns the colour filter that scales a pass's RED, GREEN
and BLUE by a tint, then adds a flash and screens a glow over the result
— composed over whatever filter the pass already carried, which runs
first, so the modulation applies to the finished colour. Alpha is left
alone: a per-glyph fade rides the paint's own alpha instead.

ALL THREE TERMS RIDE ONE COLOUR MATRIX, because screening against a
constant is affine per channel — c → c(1−s) + s — so multiply, add and
screen fold into one scale-and-bias: c·tint·(1−s) + (add·(1−s) + s). The
matrix filter clamps its output, which is where "added, clamped" happens
on this path. A neutral add and screen contribute a zero bias, leaving
exactly the scale-only matrix a bare tint builds.

It is MEMOIZED, and that is a correctness requirement rather than a
saving: a batch's key is a whole `SkPaint`, and a paint compares its
colour filter by POINTER, so a freshly built filter per glyph would mint
a bucket per glyph and undo the batching entirely. Callers quantize the
tint for the same reason they quantize alpha; the cap is what keeps a
caller that does not from growing the table without bound.

Past the cap the LEAST RECENTLY USED entry goes, one at a time. What that
buys over emptying the table is the case where the cap is reached at all:
a caller whose live tints sit just over the cap would, with a wholesale
drop, lose every filter it is still using — including the ones it asks
for again on the same frame — and rebuild them all, repeatedly. Evicting
the coldest entry instead costs a caller only the tints it has stopped
using, so a working set at the cap keeps its identities stable and its
batching intact.

The composed filter holds a reference to the filter under it, and the
table holds the composed filter, so the address that filter contributes
to the key cannot be recycled underneath a live entry.

### GlyphRSXformBatches

Glyphs grouped by font and paint pass so a frame of thousands of animated
letters collapses into a handful of RSXform draw calls. Reuse one
instance across frames — `GlyphRSXformBatches::clear` keeps the
allocations.

A glyph is added with a whole `PaintStyle` and lands in one batch per
pass it draws: each underlay in order, then the foreground, then each
overlay. Because a batch's key is a complete `SkPaint`, a pass keeps its
gradient, stroke, blend mode and mask filter — animating letters and
styling them are not alternatives.

Batches draw band by band — every underlay batch, then every foreground
batch, then every overlay batch, each band in creation order — so every
underlay lands beneath every foreground even when per-glyph fades split
one style into several buckets. Creation order alone cannot promise that:
the first glyph at a new fade mints its underlay bucket after every
earlier fade's foreground bucket, and a blurred halo reaches past its own
glyph onto its neighbours' strokes.

A `GlyphDress` carries what varies per glyph rather than per pass — the
placement, the fade, the tint, a varied face, and the matrix a shear or a
non-uniform scale needs. The face joins the bucket key; the fade and the
tint change only the resolved paint.

`GlyphRSXformBatches::PassBand` is which stratum of a `PaintStyle` a
bucket's pass came from. The draw walks these in declaration order, so a
bucket's band — not when it was minted — decides what it composites over.

`GlyphRSXformBatches::Batch` is one font-and-pass bucket: parallel glyph
and transform arrays that feed a single RSXform draw. The font is held as
the identity `makeFont` needs rather than as the shaped word it came
from, so words set in the same face and size share one bucket — and so a
bucket kept across frames never outlives a shaped word the cache has
since evicted. The same paint used as one style's underlay and another's
foreground is two buckets, because the two composite differently.

`GlyphRSXformBatches::Batch::matrixGlyphs` holds the glyphs of a bucket
that an RSXform cannot place — a shear, a non-uniform scale — with the
matrix each draws under. They cost one canvas concat and one draw apiece
and are the reason to keep them a separate lane: a glyph whose deviation
IS an RSXform never pays for a neighbour that is not.

`GlyphRSXformBatches::recentBatch` is where the last pass landed.
Neighbouring glyphs repeat a pass, and a full paint is dearer to compare
than a colour, so the scan starts where it last succeeded. It is
bounds-checked, because callers own the batch list.

`GlyphRSXformBatches::subpixel` says THE GLYPHS ADDED HERE MOVE BETWEEN
FRAMES, so their origins are placed on Skia's SUBPIXEL PHASE GRID instead
of on whole pixels.

A glyph mask is rasterized for a quantized origin. Left on whole pixels,
a run creeping along by a fraction of a pixel per frame does not creep at
all: each letter stands still until its own origin crosses a pixel
boundary and then HOPS a whole pixel, at its own moment, which is exactly
the unsteadiness a turning ring shows. On the phase grid the same creep
advances a quarter pixel at a time, and the hop is a quarter of what it
was.

IT IS OFF BY DEFAULT because the grid is the second factor in a product.
Every mask is a glyph, rotation and phase triple: the phases multiply
what a rotation ladder has already multiplied, on both axes for an
off-axis run. A run at REST gains nothing from it — its letters are not
creeping anywhere — and would pay the multiplied population for a
placement no one can see move, which is why settled type keeps
whole-pixel origins.

A MOVING run's arithmetic is the other way round. Its masks were never
going to be reused: the rotation it needs this frame is a different
rotation next frame, so the population it mints is per-frame either way,
and the phase grid only refines a mask it was going to rasterize
regardless. This is the same trade the rotation ladder makes and not a
competing one — the ladder still bounds the ROTATIONS, and dropping it in
exchange costs several times what the grid does.

`GlyphRSXformBatches::batchForPass` returns the batch for one shaped
word's font and one resolved pass. The face overrides the shaped word's
own typeface when it is non-null — the varied clone a driven
variable-font axis asks for — and is part of the key, so the same word
set at two axis coordinates is two buckets.

`GlyphRSXformBatches::addGlyph` appends one glyph, once per pass of the
style, anchored at its advance centre and rotated by a cosine and sine,
which is the placement convention the effects use. The alpha scale
multiplies every pass's alpha, which is how a per-glyph fade stays
batched: the style itself is untouched and only the resolved paint
differs. Quantize it if the effect drives it continuously — distinct
alphas are distinct buckets. Passes with nothing to draw are skipped, so
a fully faded glyph costs no bucket at all. The dressed overload places,
fades, tints and faces the glyph as a `GlyphDress` says; the tint and the
fade never touch the style itself, which is what keeps a coloured, faded
letter in the same handful of buckets as its neighbours. The overloads
taking a `PlacedGlyph` take the font, advance and span paint from the
walk, and the one that also takes a glyph id overrides the walk's own,
which is how a code-point substitution draws a different letter at the
original's pen position.

`GlyphRSXformBatches::clear` clears glyph data while retaining batch
allocations for the next frame. A frame that minted a pathological number
of buckets releases them instead, because a retained bucket also retains
its paint — and with it every shader, filter and blender that paint holds
a reference to.

`GlyphRSXformBatches::draw` draws every batch — underlay buckets, then
foreground buckets, then overlay buckets, each band in creation order —
and returns the number of glyph draws it issued, one per glyph per pass.
The band walk is what keeps a blurred halo beneath a neighbouring
letter's stroke when per-glyph fades have split the style across several
buckets.

## Resolving a decoration

A decoration resolved against a run: the band an underline, strikethrough,
overline or highlight occupies once the font's metrics have filled in
what the style left at zero, the paint that band draws with, and the
spans along the run's own axis its band actually covers once skip-ink has
cut it around the glyphs' descenders. It is deterministic geometry over a
`PositionedRun`, exposed so a test can check the band without drawing it;
the draws in the paint feature run over these same functions.

A resolved band is concrete geometry — the near edge measured across the
run's own axis, in pixels — plus a colour. Along a line that axis is the
baseline and the band grows down; down a column it is the column axis and
the band grows right.

Resolving one takes explicit values first; zeros fall back to the face's
underline and strikeout metrics, a mid-x-height strikethrough, or the
ascent line for overlines, with a one-pixel thickness floor throughout.

Resolving ALONG A COLUMN answers for a run set down one instead. A column
has no baseline — an upright glyph's em box is centred on the column axis
— so the face's underline and strikeout metrics have nothing to measure
from and the em box does the measuring instead: an underline stands clear
of the box on the RIGHT of the column, which is the side a vertical
setting reads its emphasis line on, an overline on the left, a
strikethrough down the axis itself, and a highlight across the whole box.
`Decoration::offset` still overrides, and is then a signed distance
ACROSS the column, positive to the right.

`Decoration::side` chooses between the two anchors an underline and an
overline are: the opposite side reads the other one's metric, in either
writing mode. A strikethrough and a highlight cross the type rather than
standing beside it and have no second side; nor does a decoration with an
explicit offset, which names the near edge outright.

The paint a band draws with is the fill concern, separate from the band
geometry: the decoration's own paint override verbatim when present,
otherwise an anti-aliased fill of the band's resolved colour.

The spans a decoration actually draws for a run are one span covering the
run's advance, minus glyph-ink intercepts — grown by one thickness of
standoff — when the decoration skips ink. A COLUMN RUN ALWAYS ANSWERS ONE
SPAN: intercepts are cut out of a horizontal band, which a column's band
is not, so a vertical underline draws through its glyphs' ink rather than
around it. Transformed and placeholder runs answer nothing.

### The walk over a layout

The decoration walk over a layout's runs emits every band rectangle a
paragraph's decorations draw, through a callback with its paint already
resolved, so a draw only has to put the rectangle on the canvas. Runs on
one line that share a style and a font merge into one band that also
covers the glue between words, and an underline that skips ink is cut
around every member run's glyph intercepts, which are memoized per blob.
Both draws of a layout — the immediate one and the batched one — run over
this same walk, once for the highlights that sit beneath the glyphs and
once for everything that sits above them.

`DecorationPhase` is which decoration kinds an emission pass covers:
highlights paint beneath every glyph pass, everything else above them.

## Selecting text as a value

SELECTING TEXT AS A VALUE: `Selector`, which says which of a passage a
caller means, and the `selectors` vocabulary that builds one.

The other half of this feature answers a question NOW —
`findAllOccurrences` hands back the ranges a needle matches in the
paragraph it was given. A selector is the same question written down and
not yet asked: a small comparable value that can ride in a larger one, be
compared frame to frame, and be resolved again after the text changed or
the lines re-broke. Every form of it names a position in the text or a
granularity to slice; none of them holds a paragraph.

RESOLVING one is the caller's, and deliberately so. What a selection
means as GLYPHS depends on a layout — which line a word landed on, which
cluster a mark belongs to — and this library hands its layout out rather
than owning a canonical one. So a selector is a value here and a set of
glyphs wherever the glyphs are.

`Selector` is WHICH OF A PASSAGE A CALLER MEANS, as a comparable value.
It is built from `selectors`, combined with `|` for union, `&` for
intersection and `!` for complement. A default-constructed selector
addresses EVERYTHING, which is what a caller who names nothing gets.

It is cheap to copy and compares by state, so resolving one can be cached
against the content, layout and selector it was resolved for: a regular
expression over a paragraph is matched when the text changes or reflows
rather than once per frame.

`Selector::take` keeps a number of glyphs within EACH unit of an
each-selector, from wherever `Selector::drop` left off. The two on their
own partition every unit exactly: no glyph is in both, none is in
neither.

`Selector::Kind` is the forms a selector can take. It is public because
resolving one is the caller's: a resolver reads the state and answers for
its own glyphs. `Selector::Kind::Named` and `Selector::Kind::Scope` are
the two a CALLER defines. Everything above them addresses the text itself
— words, lines, sentences, characters, patterns — and any resolver over a
paragraph answers them the same way. Those two address something the
caller named: a set of spans registered under a name, and a whole passage
identified by one. This library ships no builder for either, because what
a name addresses is the caller's to say; `Selector::of` is how a caller
spells its own form, and the combinators then treat it like any other.

### Asking now

`findAllOccurrences` returns every non-overlapping occurrence of a
needle. The scoped overload answers inside the clamped scope, as if that
window were the whole string — a match never extends past either edge.
This is the cost control for large documents: scope the query to what the
layout actually placed and the search is bounded by the geometry, not the
text.

`findRegexMatches` returns every match of an ICU regular expression, with
full Unicode semantics and a UTF-8 pattern, or nothing at all when the
pattern does not compile.

`wordRanges` returns content-only ranges for the paragraph's analyzed
words — the line-break segments the layout itself uses, trailing
whitespace excluded. It shapes on demand, cache-hot.

`MarkerSet` is named range sets that follow edits. Ranges are adjusted by
replaying the paragraph's recorded edit operations: text inserted or
removed before a range shifts it; a replacement overlapping a range is
absorbed into it, the range growing to cover the inserted text; and
ranges that collapse to empty are dropped. An insertion exactly at a
range's start joins the range; one exactly at its exclusive end does not.

## See also

`reference/LAYOUT.md` for the pass these layers stand on, and
`reference/TYPE.md` for the decorations a style declares.
