# SigilWeave — the layout pass

The chapter on the stage that turns a document into placed glyphs: the
entry points, what a caller tells them, how a block is set, what the
breakers weigh, what a frame decides, and what the pass leaves behind.
`README.md` beside the library is the front page; the geometry the pass
pours into is `reference/FLOW.md`.

## The entry point

The layout stage and its result are the engine's main entry point. Call

```cpp
layoutParagraph(fontContext, paragraph, flow, options)
```

to break a `Paragraph` into a chosen flow geometry and get back a
`ParagraphLayout`: positioned runs plus overflow, ellipsis and
placeholder reporting. `ParagraphLayout::draw` or
`ParagraphLayout::drawBatched` paints it to an `SkCanvas`, paint resolved
per span at draw time, or walk `ParagraphLayout::runs` yourself. The
options are `ParagraphLayoutOptions`, and the run and band types are
`PositionedRun`, `LineMetrics` and `ColumnMetrics`.

The draw members are declared beside the result and defined by the paint
feature, so a program that calls them links that feature; everything else
in the header is the layout feature.

`layoutParagraph` lays a paragraph out into a geometry, starting at a
given word. It ensures the paragraph is shaped — cache-hot when little
changed — breaks it into lines with the configured breaker, and returns
positioned runs backed by shared word blobs.

The first word IS THE RESUME POINT, and it is the same number the pass
before it reported as `ParagraphLayout::firstUnplacedWord` — which is
what makes a text fill as many frames as it is given. One paragraph,
shaped once, filled frame after frame: every pass reads the same word
list and the same warm shape cache, and a word index is a sound cursor
because a `Word`'s extent is a fact about the text rather than about any
one layout of it. Blocks are numbered from the START of the text however
far in a pass begins, so `ParagraphLayoutOptions::blocks` addresses the
same block in every frame of a chain.

OVERFLOW IS THE NORMAL CASE HERE and is not a cut: a pass that ran out of
geometry reports where it stopped and draws no marker unless the caller
asked for one. A frame that means to be the last of a chain is the one
that sets the ellipsis.

`layoutSingleLine` lays a paragraph out as one unconstrained horizontal
line whose baseline begins at a given origin. It is the ergonomic path
for labels and captions: callers do not need to construct a one-entry
`LineSetFlow` or precompute the paragraph width.

## ParagraphLayout: what the pass leaves behind

Positioned output of one paragraph layout pass.

IT BORROWS THE PARAGRAPH'S GLYPHS. Every `PositionedRun` points at a
`ShapedWord` the paragraph holds rather than holding one itself, and
every word and interval index a run carries reads a table on one of the
two. A layout is therefore only meaningful while the paragraph it was set
from is alive and unedited, which is what every consumer of a run already
assumes — and every member that reads the text takes that paragraph back
as an argument.

`ParagraphLayout::intervals` is every flow interval the layout consumed,
in the order the geometry handed them over — the numbering
`PositionedRun::intervalIndex` uses. A caller that re-places transformed
runs reads their geometry there rather than rebuilding it and hoping the
two agree. `ParagraphLayout::tangentRotationSteps` is the tangent
snapping the placement used, carried so a re-placement can match it.

`ParagraphLayout::linePitch` is the pitch every line was queried at — the
resolved line height, which is a vertical flow's COLUMN WIDTH. It is
carried because the flow's band is not recoverable from an interval: a
`LineInterval` states where the pen travels, never how wide the band
around it is.

`ParagraphLayout::degradedBlocks` is how many blocks the optimizing
breaker ran out of candidates on and left to the greedy breaker. It is
zero whenever no floor was set, and it is the number a caller watches to
know its floor is too low for the text it is setting.

`ParagraphLayout::reusedBlocks` is how many blocks were set from break
decisions this thread had already made for the same words at the same
measure, under `ParagraphLayoutOptions::live`. It is what says a moving
text is costing only its fill: a frame that reports as many reused blocks
as it holds made no break decision at all.

`ParagraphLayout::initial` is where the initial letter landed, when a
block declared one. Its glyphs are ordinary runs of this layout and draw
with the rest; this is the report, not a second thing to draw.

### Drawing

`ParagraphLayout::draw` draws every run, resolving its ordered paint
layers from the paragraph's current spans — so paint-only tweaks show up
without any relayout. An override paint replaces every span's paint, for
labels drawn in a caller-chosen colour without touching the paragraph.

`ParagraphLayout::drawBatched` draws the same output with minimal draw
calls: horizontal runs are merged into one batched glyph draw per
font-and-paint bucket and configured paint layer, instead of one text
blob per word and layer. A default style is one call per bucket; each
underlay and overlay adds one. Transformed runs fall back to their baked
blobs.

`ParagraphLayout::LiveVariations` is a draw-time font-variation override,
valid only for ADVANCE-INVARIANT axes. Every shaped bucket's typeface is
swapped for its varied clone, memoized by the font context, while the
glyph positions computed at shaping time are reused as they are. That is
correct exactly when driving the axis leaves every glyph advance alone —
GRAD behaves that way on faces that have it, whereas `wght` moves
advances on most faces and would leave the glyphs sitting at the wrong
pen positions. Ask `FontContext::axisIsAdvanceInvariant` before animating
an axis here; an axis that fails that test belongs in
`ShapingStyle::variations`, which re-shapes. Transformed and path runs
draw from their baked blobs and ignore the override entirely.

### Derived answers

`ParagraphLayout::placeholderRects` returns rectangles for the
paragraph's inline objects, ready to draw pills and images into.

`ParagraphLayout::lineMetrics` returns per-line geometry derived from the
placed runs, ascending by line index — the building block for selection
bands, line backgrounds, and point-to-line hit-testing that per-span
decorations do not cover.

It is derived, not stored: nothing is recorded during layout and calling
it costs one pass over the runs, metrics resolved per font change. Mixed
fonts on a line report the tallest ascent and deepest descent, matching
how a line box grows — except the initial letter, which is one run
several lines tall and reports its own extent as `PlacedInitial::box`, so
the line it stands on keeps its own band. Straight horizontal lines only:
transformed — path or rotated — and vertical runs are skipped, and lines
whose geometry placed nothing do not appear.

`ParagraphLayout::columnMetrics` returns per-COLUMN geometry for a
vertical layout, ascending by column index — what `lineMetrics` is for a
horizontal one, and the only one of the two that answers in a vertical
paragraph. It too is derived, not stored: one pass over the runs, with
each run's extent down the column taken from the pen it was placed at.
Every vertical form counts — upright, rotated and tate-chu-yoko alike —
because all three consume column pitch. Columns that placed nothing do
not appear, and a horizontal layout returns an empty list.

`ParagraphLayout::glyphOutline` returns the OUTLINE OF EVERY GLYPH this
layout placed, as one path in the layout's own coordinate space.

It is not the ink bounds and not the advance boxes: the actual contours,
at the positions the placement put them, including the per-glyph
transforms a rotated or curved run baked. It is what anything that
dresses letters rather than a box needs — a bevel, a glow, a chrome, a
cut-out — and it is derived, not stored: nothing is recorded during
layout and a caller who never asks pays nothing. Glyphs a face reports no
path for — bitmap and colour glyphs — are absent, because they have no
contour to give.

## PositionedRun: one draw call

What a layout pass leaves behind, run by run: a `PositionedRun` is one
draw call — a shared word blob at an origin, or a fully positioned
RSXform blob — together with where in the paragraph and on the geometry
it came from. Placeholder runs carry no blob at all, just the flow
position where the caller should draw its inline object.

A RUN BORROWS ITS GLYPHS AND OWNS NOTHING BUT ITS PLACEMENT.
`PositionedRun::shaped` points at a `ShapedWord` somebody else holds: the
paragraph, for every run set from a word of the text, and the layout
itself for the few words a layout shapes and retains on its own — a tab
leader, an overflow marker. A run is valid exactly as long as BOTH the
paragraph it was set from and the layout that holds it are alive, and
copying a run out of a layout does not extend that — which is the same
rule `PositionedRun::wordIndex` and `PositionedRun::intervalIndex`
already carry, since they index that paragraph's tables and this layout's
intervals.

`PositionedRun::intervalIndex` says which flow interval the run landed
on. With `PositionedRun::penOffset` it is the whole of what a caller
needs to re-place a transformed run at draw time: the geometry it was
placed on, and where along that geometry its pen started.

`PositionedRun::advance` is THE ADVANCE THIS RUN TOOK WHERE IT LANDED,
which is the shaped advance except on a justified line that spent letter
spacing or a glyph scale: those are the line's answer and not the face's,
so the shaped word cannot report them and anything measuring a line reads
them from here.

`GlyphFit` is HOW A JUSTIFIED LINE RESPACED THE GLYPHS OF A RUN: extra
advance after every glyph, and a horizontal scale on the glyphs
themselves. The identity is neither, which is what every run of a line
fitted on its word gaps alone carries. It is baked into the run's blob,
so a caller that only draws never asks; a caller that reads the glyphs
BACK — a query, a per-glyph effect, a decoration measuring where a run
ends — applies it, or it reads the positions the shaper produced instead
of the ones the line was set at.

`LineMetrics` is the geometry of one laid-out line, derived on demand
from its placed runs. The extent is the advance extent of what actually
landed — selection bands, line backgrounds and line hit-testing live
there — not the flow interval's full measure, which is a question for the
geometry itself.

`ColumnMetrics` is the counterpart for one laid-out COLUMN of a vertical
paragraph. A column has no baseline: its reading axis is y, and the
glyphs of every form — upright, rotated, tate-chu-yoko — centre
themselves ACROSS the column's central axis. So the band is the axis plus
the flow's own column pitch, and the extent is how far down the axis the
placed runs reached.

## ParagraphLayoutOptions: what a caller tells the stage

WHAT A CALLER TELLS THE LAYOUT STAGE, in one value: the settings grouped
by the stage that reads them, and the blocks that override them one
paragraph at a time. Every group is a header of its own — breaking,
justification, overflow, tab stops, the frame, mojikumi and the paragraph
style — and this is what a caller hands to `layoutParagraph`.

Every field is defaulted and every nested group is inert unless its stage
runs. Settings that belong to the geometry stay on the geometry —
`ExclusionFlow::setMinimumIntervalWidth`, for instance.

Every member is defaulted, and the common path sets only
`ParagraphLayoutOptions::alignment`. Each nested group is inert unless
its stage runs: justification applies under
`TextAlignment::kJustify`, the Knuth-Plass tuning under
`LineBreakStrategy::kKnuthPlass`, tab stops only when a word carries a
tab, and the path-text options only when runs are transformed.

The top-level alignment, justification, hyphenation and tab stops are the
WHOLE LAYOUT'S answer, and a block that states none of its own is set by
them. `ParagraphLayoutOptions::blocks` overrides them block by block.

`ParagraphLayoutOptions::live` says AN INPUT OF THIS LAYOUT IS MOVING — a
bound measure, an animating frame, a text whose content changes frame to
frame — so this layout is one of a run of them rather than an answer
someone asked for once.

It changes two things and nothing else. The break decisions of a block
set in a UNIFORM measure are kept and reused, keyed on the words and on
the measure taken to the whole pixel below it, so a measure already seen
costs no break decision at all and a measure between two seen ones is set
in the narrower of them. And the block is broken against the measure
alone rather than against the frame's supply of lines, so a frame that
only grows or shrinks in DEPTH changes which lines it holds and never
where they break. A settled layout sets nothing here and is answered
exactly as it has always been answered.

`ParagraphLayoutOptions::nextMeasure` is THE MEASURE THE NEXT FRAME OF
THE CHAIN SETS IN, for the one keep that has to count lines this frame
will not hold. The widow rule asks how many lines the remainder takes,
and the remainder is set in the NEXT frame's measure, which this fill has
no other way to learn: only whoever holds the chain knows what comes
after. 0 says nothing is known and the count is taken at the measure this
frame's last line was set in, which is exact for a chain of equal frames
and off by the difference for one that changes width. Every other keep is
settled from lines this frame placed and never reads it.

`ParagraphLayoutOptions::kinsoku` is which characters may not stand at a
line's edge. A prohibition is settled during SEGMENTATION — the boundary
is simply not opened — so no breaker knows the rule and both of them obey
it. `ParagraphLayoutOptions::hanging` is how far a character may hang
past the measure; empty leaves every line squared on its advances, which
is what a text that says nothing gets.
`ParagraphLayoutOptions::mojikumi` is how much room stands between two
adjacent full-width characters, by the class of each; empty leaves every
gap the width the shaper gave it, and costs nothing, since a layout with
no table asks no question about any gap.

`ParagraphLayoutOptions::tsume` is how much of its own advance a
full-width character gives up so it sets closer to its neighbours — as a
fraction of the em, removed from the gap after every full-width character
the mojikumi table gives no class of its own. 0 leaves the face's own
setting. It is applied where mojikumi is applied and stops where that
stops: at the gaps between words.

`ParagraphLayoutOptions::blocks` is one entry per BLOCK — the text
between two mandatory breaks — in block order. A block past the end of
that list, and every block when it is empty, is set in
`ParagraphLayoutOptions::blockDefault`: the layout-wide fields alone by
default, or whatever the passage inherits when a host has resolved that
for it.

## ParagraphStyle: one block's setting

ONE BLOCK'S SETTING — the paragraph controls a reader sees as a
paragraph: its pitch, its indents, the keeps that hold its lines
together, and the registry that names a set of them — as one comparable
value.

Everything above the overrides is the block's own and has no
layout-wide counterpart. The four optionals are the layout-wide settings
of the same names: present, the block is set that way; absent, the
layout's own answer stands. That is what makes a text with no block
styles lay out exactly as one that never heard of them.

SPACE BEFORE AND AFTER DO NOT COLLAPSE AND ARE NOT SUPPRESSED. The gap
between two blocks is the LARGER of the first's `ParagraphStyle::spaceAfter`
and the second's `ParagraphStyle::spaceBefore`, everywhere, including at
the head of a frame. One rule, no exceptions to hold in the head.

`ParagraphStyle::halfLeading` decides where the room a leading opened
goes: all of it ABOVE the line, which is the setting convention and the
default, or half above and half below, which sits the type optically
centred in its own band.

`ParagraphStyle::balanceRaggedLines` sets the block in the NARROWEST
MEASURE THAT STILL TAKES THE SAME NUMBER OF LINES, which is what a ragged
heading of three lines wants: the lines then have nowhere to be long, and
that is an even rag. The optimizing breaker searches for that measure by
bisection and breaks against it; placement still sets the lines in the
measure the geometry gave, so a centred block stays centred on the real
one. It is ignored by the greedy breaker, which takes the first break
that fits.

`ParagraphStyle::initial` is the block's OPENING SET LARGE — sized so its
reference metric spans the lines it is given, seated on the baseline it
sinks to, with the lines under it wrapping the notch it cuts. Zero lines,
the default, declares none and costs nothing.

### Leading

How far apart a block's lines stand — its PITCH, which in a vertical
setting is the width of its columns.

`Leading::face` takes the first span's own line height, which is what a
text that says nothing has always used. `Leading::multiple` scales that.
`Leading::absolute` states it outright in pixels. `Leading::grid` states
a rhythm rather than a pitch: the block's own height rounds UP to a
multiple of it, so blocks set on the same grid share one rhythm however
differently their faces are cut.

### IndentOptions

Where a block's lines start and end across the measure.

`IndentOptions::start` and `IndentOptions::end` inset every line of the
block from the two ends of whatever interval the geometry offered — the
near end being the one the pen enters, so a line and a column read them
the same way round. `IndentOptions::firstLine` and
`IndentOptions::lastLine` are added to the start on the block's first and
last line only; a NEGATIVE first-line indent is the hanging indent a
bullet or a number hangs into.

An indent is arithmetic on the interval the geometry handed back, so it
composes with exclusions and columns without either knowing about it: a
line broken into three intervals by a shape is inset at its outermost
ends and nowhere in the middle.

### KeepOptions

Which of a block's lines refuse to be parted from each other.

Every one of these is a statement about a FRAME BOUNDARY — a widow stands
at the head of the next frame, an orphan at the foot of this one, a
kept-together pair straddles the join — so they are settled where the
boundary is: the fill runs, and lines the block may not leave behind are
taken back out of it and reported as overflow, which is how they reach
the next frame of the chain. No break is re-decided and nothing is
weighed against spacing, so BOTH BREAKERS obey these identically.

A keep never empties a frame. A retraction that would leave the fill with
nothing is dropped: the text would arrive at the next frame in exactly
the state that emptied this one, and the chain would never advance.

`KeepOptions::widowLines` is the one that asks about a frame this fill
cannot see, so it counts the carried lines at the measure THIS frame's
last line was set in. A chain of equal frames — the ordinary one — counts
exactly; a chain that changes width counts the carried lines at the wrong
measure.

## Breaking

WHERE THE LINES BREAK and what decides it: the alignment, the choice of
breaker, the metrics that override the font's, where a word may be
hyphenated, and the tolerances the optimizing breaker weighs.

`HyphenationOptions` is where a word may be broken, and which of those
breaks a line may take. The two halves are decided at different stages
and that is the whole of the split. Where a break MAY fall is
segmentation: a soft hyphen already in the text, plus whatever
`HyphenationOptions::patterns` finds inside a word under
`HyphenationOptions::limits`, and all of that is a fact about the text
that the whole layout shares. Which of those opportunities a line
actually TAKES is a break decision — the three fields after the limits —
so a block may state its own and the breaker reads the block's.

`HyphenationOptions::enabled` set false removes the break opportunity,
not just the hyphen glyph: the halves either side of a soft hyphen fuse
into one unbreakable word during segmentation, so the word wraps or
overflows whole, and the patterns are not consulted at all. Reaching the
paragraph is what makes that happen, through
`Paragraph::setSoftHyphenBreaks`, which `layoutParagraph` sets from here.

`HyphenationOptions::patterns` is compared by identity, because two
hyphenators that are not the same object cannot be shown to answer the
same way.

`HyphenationOptions::zone` is the band at the ragged edge inside which a
line is already square enough, in pixels; 0 lifts it. A line whose last
WHOLE word ends inside the band is left ragged, because a word broken to
reach further is a hyphen the page did not need — so the question is
asked of the line WITHOUT the break, and both breakers ask it the same
way. A word that is the whole line is still broken: there is nothing else
on the line for the zone to measure. It is ragged setting only — a
justified line shows its slack in the gaps rather than at the edge.

`KnuthPlassOptions::candidates` is how many BREAK CANDIDATES the
optimizing breaker may weigh for ONE BLOCK before it gives up and lets
the greedy breaker fill that block instead; 0 lifts the floor, which is
what a layout that says nothing gets. ONE CANDIDATE IS ONE CANDIDATE
LINE: one path in the breaker's active list carried to the break position
under consideration and scored there, which is the innermost step of its
dynamic program.

It is COUNTED AND NOT TIMED. How many candidates a block weighs is a fact
about its words and its measure, so the same block at the same measure
meets or misses the floor every time — the same answer on a loaded
machine as on an idle one, and a capture of it is a function of the
declaration alone.

It is a DEGRADE AND NOT A POLICY. The composer is meant to run on moving
text — that is what it is for — and this is the floor under a frame that
meets a block it cannot compose inside it: one frame set greedily,
counted in `ParagraphLayout::degradedBlocks`, rather than a frame that
arrives late. A layout that reports degrades every frame is asking for a
higher floor or a shorter block.

## Justification

HOW A JUSTIFIED LINE IS FITTED: the three passes a line spends its slack
in, and the limits each of them works between.

A justified line is fitted in three passes, each spending only what the
one before it could not: the WORD GAPS move first, from their desired
width towards the near limit; then LETTER SPACING is added between the
glyphs; then the glyphs themselves are SCALED across. Shrinking runs the
same order. A pass whose limits equal its desired value contributes
nothing and costs nothing — which is why a caller who sets none of them
gets word spacing alone, as this stage has always done.

Every pass's DESIRED value widens the line before any of them is fitted,
and its two limits bound what it may add on top of that.

THE GAPS ARE BOUNDED BY `JustificationOptions::spaceStretch` ONLY WHERE A
LATER PASS CAN SPEND WHAT THEY MAY NOT — where the letter or glyph limits
leave room past what those passes were asked for. With both shut, a bound
on the gaps would open a hole at the right margin that nothing in the
line is allowed to close, and a hole is worse than a wide gap. What a
later pass then FAILS to spend — because it reached its own limit — goes
back to the gaps for the same reason: the bound stood on the claim that a
later pass takes what the gaps drop, and where that claim fails the bound
goes with it. So a justified line reaches its measure whatever the limits
are, and the limits decide only how much of the fit stands between the
words and how much between the letters.

ROOM ABOVE A DESIRED VALUE IS ROOM THE FIT SPENDS. A glyph scale of 0.92
with the limits left at 1 is a scale of 1 on every line that needed
widening, because the pass reaches through its range before the gaps take
anything back. A value meant to HOLD says so with its limits: pin them
either side of it and it is what every justified line is set at.

`JustificationOptions::expandIdeographicGaps` is on because CJK text has
no spaces, so eligible zero-width ideographic gaps may be expanded up to
`JustificationOptions::maxIdeographicExpansion` times the font size per
gap.

`JustificationOptions::letterSpacing` is applied to every justified line
whatever its fit; its two limits bound what the pass may add on top, and
the pass may always undo its own desired value where the line will not
take it. All three zero leaves the pass out.
`JustificationOptions::glyphScale` and its limits work the same way, all
three at 1 leaving the pass out — scaling letters is the last thing a
page should do and the defaults never do it.

`JustificationOptions::SingleWord` is for a line holding ONE word, which
has no gaps to spend: `kAlign` leaves it at the block's alignment,
`kJustify` stretches it across the measure with letter spacing alone.

## The frame

HOW A FRAME SEATS WHAT IT HOLDS — where the first baseline sits and what
becomes of the room left over — the band reserved beside every line for
something set alongside the type, and the snapping text on a path is
drawn with.

`FrameOptions` carries the two decisions a frame makes that no line makes
for itself.

WHERE THE FIRST BASELINE SITS is otherwise the first line's own ascent,
so two frames of different type start their text at different heights;
naming a cap height, an x-height or a fixed offset instead pins the first
line to something the page can be ruled against.

WHAT BECOMES OF THE ROOM LEFT OVER is otherwise nothing: the lines stack
from the top and the remainder is air underneath. Centring or seating the
text against the far edge translates the whole block; justifying it
spreads the remainder BETWEEN the lines, as extra leading, which is what
a column of a magazine does to reach its foot.

Both need to know how deep the frame is, which a geometry knows and the
layout does not, so `FrameOptions::extent` states it: 0 leaves both
decisions alone. Neither applies to a flow whose intervals ride a contour
— a loop has no near edge to measure from.

`ReservedBand` is SPACE RESERVED BESIDE EVERY LINE, over and above the
leading — the band something set alongside the type occupies: a reading
over a base, a row of emphasis dots, a note in the gutter.

It is a LAYOUT INPUT and that is the whole point of it. The band is
stated before the text is laid out, from the annotation's own metrics,
never from where the base's glyphs turned out to land — so the base is
broken and placed once, with the room already in its strut, and the
annotation is then placed on the result. Nothing chases anything.

`ReservedBand::before` is above a line and to the RIGHT of a column,
`ReservedBand::after` below a line and to the LEFT of one: the sides each
writing mode reads its furniture on. Both open the pitch; the before band
also moves the baseline down inside the band, so the type stays where the
reader expects it and the room appears where the reading goes.

`PathTextOptions::tangentRotationSteps` makes animated path tangents snap
to that many directions, to avoid creating a fresh glyph-atlas strike for
every tiny rotation change. Zero preserves exact rotations for static
artwork.

## The initial letter

THE INITIAL LETTER: a block's opening set large enough to span several
lines, with the lines beneath it wrapping the notch it cuts.

The two numbers a dropped or raised initial is made of — the size that
makes its cap height span N lines, and which line's baseline it sits on —
exist only where the block's pitch and the cap face's own metrics are, so
they are answered here rather than guessed by a caller. Declare one on
`ParagraphStyle::initial`; the layout sizes it, cuts the notch out of the
bands it covers — the bands of the block after it too, when its own block
has fewer lines than it sinks — shapes its glyphs, and reports where it
put them in `ParagraphLayout::initial`.

A COLUMN'S INITIAL is set down the column, like the text around it: the
cap hangs from the head of the column it opens, the notch it cuts is its
own vertical advance, and it sinks across the columns rather than down
them.

`InitialLetter` is A BLOCK'S OPENING, SET LARGE — how tall, how far down,
how much of the text, against which metric, and how close the following
lines come.

THE SIZING RULE, which is the whole reason this is a layout value and not
a font size someone picked: the initial's top reference point is aligned
with the FIRST LINE'S top reference point, and its baseline is aligned
with the baseline of the line it sinks to. Those two alignments fix the
distance the initial's reference metric must span, and the font size
follows from the face's own ratio for that metric. A letter chosen by eye
is wrong per typeface, because ascent, descent and cap height differ
between faces at one size; a letter sized by the rule is right in every
face.

`InitialLetter::Align` is WHICH REFERENCE METRIC the two alignments are
made on. Latin setting aligns cap heights, Han setting aligns em boxes,
and a hanging script aligns the ascent its characters hang from.
`InitialLetter::Wrap` is how the following lines meet the initial: the
notch is the initial's advance box, or the outline of its own glyphs, so
a line may tuck under the diagonal of an A.

`InitialLetter::sink` is how many lines BELOW THE FIRST BASELINE the
initial's own baseline sits: 1 puts it on the second line's baseline and
0 leaves it on the first, which is a raised initial and as high as one
goes — a negative sink is read as none. Unset drops it by the line count
rounded down less one, which lands the baseline on the last line the
initial spans: the dropped cap.

`InitialLetter::graphemes` is how many GRAPHEME CLUSTERS of the block's
opening the initial takes. A cluster is what a reader calls a letter, so
an accented capital and a digraph count as one and two.

`InitialLetter::style` is what the initial is set in: a PARTIAL over the
style the block's opening carries — a display face, a colour — with the
rest the opening's own, at the derived size. Empty sets it in the
opening's style outright.

`initialLetterSize` is THE SIZE AN INITIAL LETTER IS SET AT, from the
rule rather than by eye. The first-line reference is the FIRST LINE'S own
reference metric — its cap height under the alphabetic align, its em box
under the ideographic one, its ascent under the hanging one. The
initial's reference metric must reach from the first line's reference
point down to the baseline that many lines later, so it spans the line
count less one times the pitch, plus the first line's reference, and the
size that gives the face that span is what comes back. It is public
because a caller drawing its own ornament in the initial's place wants
the same number.

`PlacedInitial` is WHERE THE LAYOUT PUT THE INITIAL, and what it took to
put it there. The initial's glyphs are ordinary runs of the layout — they
are in `ParagraphLayout::runs` and draw with everything else — so this is
the report a caller reads to rule a page against the initial, not a
second thing to draw.

## Tab stops

THE STOPS A TAB ADVANCES TO: where the pen goes, what it aligns there,
and what fills the gap behind it.

`TabStop::Align::kStart` puts the text after the stop, `kEnd` ends it
there, `kCenter` straddles it, and `kCharacter` lines the FIRST
`TabStop::alignOn` in the following text up on it — which is the decimal
column a table of figures wants, and falls back to `kEnd` for a cell that
holds no such character.

`TabStop::leader` is set repeatedly across the gap the stop opened,
clipped to it, in the style of the text ahead of the tab: a run of dots
between a heading and its page number is one string here rather than
typed content.

`TabStopOptions` is tab-character handling for straight horizontal flows.
A word whose trailing whitespace contains a tab advances the pen to the
next stop instead of its measured glue: first through the explicit stops,
ascending, in pixels from each line interval's start, then repeating
every `TabStopOptions::interval` pixels past the last explicit stop. With
no stop ahead — or no configuration at all, which is the default — tabs
keep their shaped space-equivalent width.

Both breakers resolve stops identically: greedy fits against tab-resolved
widths as it goes, and Knuth-Plass scores every candidate line at its
tab-resolved width. Stops are line-local — alignment other than the start
shifts the resolved line as a whole. Tab gaps are rigid under
justification, and gaps at or before a line's last tab never stretch or
shrink, because the following stop would swallow the adjustment and unpin
the column; only the gaps past the last tab absorb slack. The scope is
straight horizontal intervals, left-to-right lines.

## Japanese composition

JAPANESE COMPOSITION: the class a character is set by, and the table that
says how much space stands between two of them.

`MojikumiClass` is WHICH KIND OF CHARACTER A MOJIKUMI RULE IS ABOUT.
Japanese setting spaces full-width characters by the CLASS of the two
either side of a gap rather than by the characters themselves: an opening
bracket carries its ink in its right half and a closing bracket in its
left, so two brackets back to back leave a full em of white between two
marks that are each half air, and a page sets them closer.

WHICH characters are of which class is a decision, so it is data — a
house sets its own — with one exception the engine answers for itself:
whether a character stands in a full-width cell at all is a property of
the character, not an opinion about it.

`MojikumiTable` is HOW MUCH ROOM STANDS BETWEEN TWO ADJACENT FULL-WIDTH
CHARACTERS. `MojikumiTable::members` names the characters of each class,
one character per entry, exactly as a kinsoku table names its
prohibitions; a full-width character no entry names is an ideograph, and
everything else is other. `MojikumiTable::room` is then read by the class
of the character BEFORE the gap and the class of the one after it, as a
fraction of the em: negative closes the gap up, which is what nearly
every entry of a real table does.

It is applied where the two characters are adjacent across a BREAK
OPPORTUNITY, which between full-width characters is nearly every gap
there is; two characters shaped inside one word are set by the face and
by the shaper, and no table moves them.

`MojikumiTable::classOf` is the class the table gives a character, or
other when it names none. A full-width character the table does not name
is an ideograph, which the caller decides by asking the character and not
this table.

## Block: the block partial

`Block` is a block's setting as a PARTIAL: every field optional, so a
call site states the one thing it changes and says nothing about the
rest. Beside it are the merges — `merge` folds one partial into another,
`overlay` resolves one against a whole `ParagraphStyle` — and the style a
partial alone names, `toParagraphStyle`. `Block` is to `ParagraphStyle`
what `Type` is to `TextStyle`.

It carries the settings a block takes from the passage it stands in when
it says nothing of its own: the pitch and where its room goes, the
alignment, the justification and its last line, the hyphenation, the tab
stops, the first- and last-line indents, the widow and orphan counts,
balanced ragging, the breaking strategy, the writing mode, the locale the
lines break under, and the line tables a house sets CJK text by. What a
block keeps to itself — its air before and after, its reservation, its
keeps with the next block, its initial letter, its every-line insets —
stays on the whole `ParagraphStyle`, as a margin is a box's own and not
its children's.

A field left unset is the field inherited; a field stated is the block's
own. `overlay` is one step of that, onto a whole style, and
`toParagraphStyle` is what a partial names with nothing above it: the
layout's own answer for every field it leaves unset.

`Block::lastLineAlignment` and `Block::justifyLastLine` are stated apart
from the rest of the justification so a passage can name the last line
without restating everything else about it.

`apply` writes THE LAYOUT-WIDE FIELDS a block in force sets — the
alignment, the breaking strategy, the hyphenation, the justification and
its last line, the tab stops and the line tables — each where the partial
states it, the rest as the options already hold them. The per-block
fields are `overlay`'s and `toParagraphStyle`'s; the writing mode and the
locale are the paragraph's and a consumer sets them there. `overlay`
itself passes the writing mode and the locale through untouched, for the
same reason.

## Rule and StyleSheet: the classes a tree states

`Rule` is ONE CLASS OF A SHEET: a name, and what it states — a partial
over the type and a partial over the block, either or both. It is spelled
as a literal by the half it names, `{"note", {.size = 11}}` or
`{"lead", {.firstLineIndent = 24}}`, or with the verbs:

```cpp
rule("body").font({.size = 19.5f}).block({.leading = Leading::multiple(1.35f)})
```

What a rule leaves unsaid is what the node inherits.

`StyleSheet` is THE CLASSES A TREE STATES, as one value: rules in the
order they were written, comparable by value, and a base style for the
runs of a rich text that name nothing.

A name stated again ADDS to its rule — the later fields standing, the
rest as they were — so a class can be spelled once per half, and a sheet
stated nearer the leaf changes only what it names in the sheet it stands
over. Lookup is a linear scan: a sheet names a handful of classes, and a
scan of a handful beats a hash of one.

`StyleSheet::types` is the type half as a `TypeSheet`, which the
paragraph layer shapes rich runs through; it never sees a block.

## Story: content plus its block styles

`Story` is a text and the block styles it is set under, filled into as
many frames as it is given.

A story is CONTENT PLUS ITS BLOCK STYLES and nothing else: it holds no
layout, no cursor and no frame. Every frame of a chain lays the same
story out and resumes at the word the frame before it stopped on, so the
cut between two frames moves as either one's measure moves and nobody has
to decide where it falls.

```cpp
Story article(rich(body).add(u8"…"));
article.paragraphs({heading, para, para});
```

THE BLOCKS ARE NUMBERED FROM THE STORY'S START, so the third block is set
the same way whichever frame it happens to land in. Pitch, writing mode
and block styles are the story's, and a frame cannot override them: a
frame that wants a different pitch is a different story. What a frame
decides is its own geometry — its box, its exclusions, a shape it flows
around — and whether it is the last one, which is the only one an
ellipsis belongs on. Overflow on any other frame is the normal case and
is what the next frame is for.

It is a VALUE, for the reason `RichText` is: two stories describing the
same runs under the same block styles are equal, so a caller that
rebuilds its story can ask whether anything actually changed.

## Setting a run beside another's extent

SETTING A RUN BESIDE ANOTHER'S EXTENT — the placement every reading over
or beside a base is made of: furigana over a compound, emphasis marks
down a column, a gloss under a phrase.

Three questions, and they are all the engine's rather than a caller's.
How much room does a reading of this type need beside a line — the number
that goes into a block's strut BEFORE the base is broken, which is what
makes a reservation a layout input rather than a cycle. Where does the
reading stand against the extent its base occupied. And when a base
breaks across a line or a column, which part of the reading goes with
which part of the base.

Nothing here knows what a ruby IS, or which unit a caller annotated, or
how big a reading should be relative to its base — a reading's size is
its own style's, and there is no fraction of anything in it.

`Beside` is WHERE A READING STANDS against the extent its base occupied.
`Beside::Side::Before` is above a line and to the RIGHT of a column,
`After` below a line and to the LEFT of one — the sides each writing mode
reads its furniture on. The reading is centred on the base's own extent
along the reading direction and stands `Beside::gap` clear of its band
across it.

`bandBeside` is the band a reading set in a style needs beside a line,
the gap included. This is the number a block reserves in
`ParagraphLayoutOptions::reserved` before anything is broken: it is the
reading's OWN strut, so it depends on the reading's type and on nothing
about the base — which is the whole reason a reservation costs no round
of convergence.

`layoutBeside` lays a reading out beside the base's extent and returns
where it landed: one line — or one column, in a vertical setting — at the
reading's own natural width, centred on the base and standing clear of
it. The reading's writing mode is set from the placement before it is
laid out, so a column's reading runs down the column beside it.

`shareOfReading` is the part of a reading that belongs to a piece of a
base carrying so much of the base's advance where the rest carries the
remainder. A base that breaks across a line or a column splits its
reading with it, in proportion to the advance either side — which is the
only proportion a reading has to go by, since the reading's own
characters need not correspond to the base's one for one. The cut lands
on a UTF-16 boundary and never inside a surrogate pair.

`WarichuSplit` is WHERE A NOTE SET IN TWO LINES INSIDE ONE LINE OF ITS
BASE IS CUT, and what room it then needs — warichu, the aside a text sets
small and doubled inside the line it interrupts rather than beside it.
The cut is the break opportunity that leaves the two lines CLOSEST IN
ADVANCE, because two lines of one length is what makes the note read as
one object rather than as a line with something under it. The note's size
is its own style's, as everything beside a base is: nothing here halves
anything.

`warichuSplit` returns the cut, the advance and the band a two-line note
needs. A note of one word is one line, and says so with its cut word past
its last word.

`layoutWarichu` lays a note out as two lines inside a slot and returns
where it landed. The slot is the inline box the note occupies in the
base's flow — the room a placeholder reserved for it — and the two lines
stack across it, down the box in a horizontal setting and across it in a
vertical one, in the note's own writing mode. A slot narrower than the
split's advance sets the note anyway: the note is the caller's to size.

## TextContext: single-style text

Single-style text measured and laid out through a per-thread service.
Paragraph reuse is configured on the service; callers supply content,
style and geometry and receive a result that owns its text.

`TextContext` is that per-thread service, with internally owned paragraph
reuse. One instance serves any number of labels or single-style passages.
Exact text and shaping style identify an entry; paint changes reuse its
analysis. Geometry is queried on every layout call. A result still held
by a caller is isolated before another call changes its paragraph.

Editable or mixed-style documents use `Paragraph` and `layoutParagraph`
directly. This context owns no canonical layout of such a document.

`TextLayout` is a layout and the immutable text it was laid out from.
Copies share the text, and eviction, subsequent layout calls and
destruction of the context cannot invalidate a result. Link the paint
feature to use its draw members, as with `ParagraphLayout`.

`TextContextOptions::paragraphCacheEntries` is the maximum retained
paragraphs; zero disables retention, and least recently used entries are
released first. Results held by a caller remain alive independently of
that limit.

## See also

`reference/FLOW.md` for the geometry, `reference/PARAGRAPHS.md` for the
document, and `reference/TYPE.md` for the styles its spans carry.
