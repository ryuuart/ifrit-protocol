# SigilWeave features

The whole of what the engine does, section by section — the reference
`README.md` sends you to. The README is what the library is and how to
reach it; this is what it covers.

- [The flow geometries](#the-flow-geometries) — what a `LineRequest` means, and the ready-made ones
- [The pipeline](#the-pipeline) — the stages `layoutParagraph()` runs
- [The features and their headers](#the-features-and-their-headers)
- [Paragraphs: what a BLOCK is set like](#paragraphs-what-a-block-is-set-like)
- [What the engine covers](#what-the-engine-covers)
- [What this covers, control by control](#what-this-covers-control-by-control) — where the parity table is, and what it holds
- [What a frame costs: the live composer](#what-a-frame-costs-the-live-composer)
- [The hard parts](#the-hard-parts)
- [Conventions and gotchas](#conventions-and-gotchas) — read this before writing against the library

## The flow geometries

The one virtual a caller implements is `FlowGeometry::lineIntervals`, and
the README's quickstart shows the shape of one. What its argument means:

A `LineRequest` is the band being asked for, and the field that matters is
`bandStart`: how far along the stacking axis the band's near edge sits,
measured from the flow's own start edge. Bands do NOT stand at
`index · lineHeight` — that holds only while one pitch serves the whole
text, and a text whose blocks lead differently, or which puts air between
them, stacks at distances the layout accumulates. So the layout carries
that cursor and a geometry answers what is available in the band that
starts there. `blockIndex` and `lineInBlock` come along for a geometry that
wants them — a frame grid, a well cut for one block — and every geometry
above ignores them.

A contour interval carries a `geometry::path::Contour` from SigilGeometryPath —
one sub-path addressed by arc length, built with `geometry::path::Contour::of(path)`.
The layout reads position and tangent through it, so "distance along" and
"closed wraps around" mean the same thing for text as for every other thing
that walks a path.

Ready-made geometries cover the common cases: `BlockFlow` (a rectangle),
`ExclusionFlow` (a rectangle minus moving `Silhouette`s), `VerticalBlockFlow`
(top-to-bottom columns advancing right to left), `LineSetFlow` (explicit
intervals — any origin, direction, and count per line), and `PathFlow` (each
contour of a path becomes a line).

A `Silhouette` is the second virtual, and it has one question too: which
stretches of a band, along the flow axis, does this shape occupy. The stock
ones — `silhouette::rectangle`, `circle`, `ellipse`, `path` (fill rule
honoured, so holes stay open to text) and `coverage` (an image's alpha above
a threshold) — are peers of one a caller writes, and there is no kind to
switch on. `Exclusion` pairs a silhouette with a margin and an offset: the
margin is a DISC, the set of points within that distance of the shape, so a
diagonal edge stands the text off by exactly what was asked and a corner
comes out round; the offset is rigid motion and costs the silhouette
nothing.

`ExclusionFlow` takes a `FlowAxis`, and that is the whole of what a column
costs it: `FlowAxis::kColumns` makes each band a top-to-bottom column
advancing right to left from the bounds' right edge, and reads every
silhouette's extent DOWN the column instead of across the line. A column is
a line turned a quarter turn — a shape shortens one, or splits it in two,
exactly as it shortens or splits the other — so the band scan, the fill
rule, the disc offset and the sliver threshold are one implementation read
through two coordinates. Pair `kColumns` with
`Paragraph::setWritingMode(WritingMode::kVerticalRL)`, exactly as
`VerticalBlockFlow` is paired.

## The pipeline

`layoutParagraph()` is the entry point and runs these stages in order.

1. **Line clamp.** When `overflow.maxLines` is set, the caller's geometry is
   wrapped by one that stops handing out lines at the limit. Every geometry
   and both breakers get clamping for free.
2. **Segmentation only.** `Paragraph::ensureAnalyzed()` runs the ICU passes —
   UAX#14 break opportunities, script itemization, bidi, per-codepoint
   fallback resolution — and builds the `Word` list. No glyphs yet. The bidi
   pass is skipped entirely unless the codepoint walk saw something that can
   force right-to-left, which is the overwhelmingly common case.
3. **Strut and line metrics.** Line height and ascent come from the first
   span's font unless `lineMetrics` overrides them.
4. **The initial letter.** When a block declares `ParagraphStyle::initial`,
   its size is derived from that block's pitch and the cap face's own
   metrics, and the caller's geometry is wrapped by one that takes the notch
   off the head of every band the initial covers. It happens here because
   the notch is part of the geometry every line is then broken against. One
   per pass, and a block a frame before this one already opened is resumed
   rather than opened again.
5. **Geometry flattening.** Every line's intervals are flattened, lazily,
   into a single indexed `IntervalSequence`. Both breakers consume geometry
   *only* through it, so a break decision and the placement that follows can
   never disagree about which interval is which.
6. **Line breaking.** Greedy or Knuth-Plass (see below).
7. **Lazy shaping.** Breakers call `ensureShapedTo()` just ahead of their own
   frontier, so a paragraph far larger than its geometry only ever sends the
   words that can actually land through HarfBuzz. Words past the last
   interval are never shaped at all.
8. **Shaping.** `shapeWord()` goes through the content-addressed shape cache.
   The cache is probed with a borrowed view of the key, so a warm
   re-analysis allocates nothing; an owning key is materialized only on a
   miss.
9. **Placement.** Words are reordered per UAX#9 rule L2 (reverse maximal runs
   of each level, highest first), then positioned inside their interval with
   the requested alignment.
10. **Blob emission.** One of four shapes per run: a straight horizontal run
    reuses the word's shared origin-relative blob translated to its origin; an
    upright vertical run does the same down a column; tate-chu-yoko reuses it
    centred across the column axis; anything rotated or on a contour bakes
    per-glyph `SkRSXform`s into a fresh blob.
11. **Ellipsis.** When set and the layout overflowed, the final placed line
    is trimmed until a shaped marker fits.
12. **The initial's own glyphs.** The fill started past the graphemes the
    initial took, so they are shaped and placed last, into the notch that was
    cut for them. They are ordinary runs of the layout, and
    `ParagraphLayout::initial` reports where they landed.
13. **Draw.** `draw()` emits one blob per word; `drawBatched()` merges
    horizontal runs into one `drawGlyphs` call per (font, paint) bucket.
    Both resolve paint per span at draw time.

## The features and their headers

Each feature is a directory holding its sources, its `CMakeLists.txt`, its
`test/` and (where it has one) its `bench/`; its public headers sit under
`include/sigilweave/<feature>/`. A fixture several features shape with
belongs to none of them, so the shared ones sit at the library root:
`test/support/`, `test/assets/` and `bench/support/`. Internal headers never leave the feature
directory. The features form a dependency chain — each links those it
needs, and each header includes those it needs, so including a later one
pulls the earlier ones in. `<sigilweave/SigilWeave.h>` is the umbrella
over every engine feature; a translation unit that uses one feature
includes that feature's header.

**`unicode`** — `SigilWeaveUnicode`, the leaf: `unicode/Unicode.h`, every
Unicode question the engine asks answered as plain values over UTF-16
text (its own section below).

**`style`** — `SigilWeaveStyle`, header-only over Skia's paint types:

- **`style/Style.h`** — the umbrella over the vocabulary every other
  header speaks, one header per subject beneath it: `style/ShapingStyle.h`
  (`ShapingStyle`, the shape-cache key, with `FontFeature`,
  `FontVariation`, `TextTransform`, `VerticalForm` and
  `opticalKerning`), `style/PaintLayer.h`
  (`PaintLayer` — a pass's `SkPaint`, its offset, and optionally a
  SigilMaterial instance it shades with, held by pointer and resolved at
  draw time through `paint/Paint.h`'s resolver, plus `blurred()` for
  attaching a blur mask to one; the three arrangements everyone writes
  are `kit/PaintLayers.h`), `style/Decoration.h`
  (`Decoration`), `style/PaintStyle.h` (`PaintStyle`, draw-time),
  `style/TextStyle.h` (`TextStyle` = the two halves) and
  `style/StyleSet.h` (`StyleSet`, a small ordered registry of named
  styles, comparable by value, whose lookup always answers — an
  unregistered name resolves to the set's base entry).
- **`style/Type.h`** — `Type` and `textStyle()`: the parameters of a style as a
  designated-init aggregate (face, size, colour, tracking, condensation,
  weight, slant, aliasing, the 8-bit colour ladder, extra axes) and the
  `TextStyle` they build. It decides nothing — there is no type scale and
  no opinion about which face stands in for which. The face itself comes
  from `ports::pickTypeface()`, which walks the system font manager, or
  from `ports::face()`, which keeps that answer once per chain and style —
  a face is compared by pointer, so one holder is what lets two asks for
  one family compare equal.
- **`kit/Features.h`** — named OpenType presets
  (`features::tabularNumbers`, `smallCaps`, `stylisticSet(n)`, …) so
  styles need not hand-spell four-cc tags, including the ones a COLUMN
  asks for: `verticalRotatedForms`, `verticalAlternates`,
  `proportionalVerticalMetrics`, `halfWidthVerticalMetrics`,
  `verticalKana`, `verticalKerning`, and `verticalFormsOff` to decline
  the vertical forms shaping takes by itself.

**`fonts`** — `SigilWeaveFonts`, HarfBuzz and Boost private:

- **`fonts/FontContext.h`** — the per-thread service object: HarfBuzz
  faces, fallback memos, varied-typeface clones (retained, or transient for
  a continuously varying coordinate), the shape cache, observable `Stats`.
- **`fonts/Shaper.h`** — `ShapedWord`, `shapeWord()`, `wordBlob()`,
  `makeFont()`. Reach for it to inspect or reuse individual glyph runs.

**`paragraph`** — `SigilWeaveParagraph`, the Unicode leaf private:

- **`paragraph/Word.h`** — the atomic layout unit: `Word`, its
  `WordSegment`s in a `WordSegmentList`, and the `SegmentForm` a vertical
  column places a segment in.
- **`paragraph/Paragraph.h`** — the document: UTF-16 text, normalized
  `StyleSpan`s, `CharRange`, inline `Placeholder`s, writing mode, the edit
  log, sentence boundaries, and the analysis entry points;
  `ParagraphBuilder` for the push/pop idiom.
- **`paragraph/RichText.h`** — the same content said as a VALUE:
  `RichText`, `rich()`, runs of text with the styles or style NAMES they
  are set in, and `RichText::slot` for a box reserved in the flow.
- **`paragraph/Unit.h`** — `Unit`: the granularity a passage is
  addressed by.

**`layout`** — `SigilWeaveLayout`, with `SigilGeometryPath` public because
a contour interval carries a `geometry::path::Contour`:

- **`layout/Flow.h`** — `LineInterval`, the `FlowGeometry` interface, and
  the ready-made geometries.
- **`layout/LayoutOptions.h`** — `ParagraphLayoutOptions`, the whole
  layout's answer, which includes the group headers beside it:
  `layout/Breaking.h` (`TextAlignment`, `LineBreakStrategy`,
  `LineMetricsOptions`, `HyphenationOptions`, `KnuthPlassOptions`),
  `layout/Justification.h`, `layout/Overflow.h`, `layout/TabStops.h`,
  `layout/Frame.h` (`FrameOptions`, `ReservedBand`, `PathTextOptions`),
  `layout/Mojikumi.h` and `layout/ParagraphStyle.h` (`Leading`,
  `IndentOptions`, `KeepOptions`, `ParagraphStyle`, `ParagraphStyleSet`).
- **`layout/PositionedRun.h`** — `PositionedRun`, one draw call, and the
  `LineMetrics` and `ColumnMetrics` bands derived from placed runs. A run
  BORROWS its glyphs: `shaped` is a `const ShapedWord*` into the paragraph
  the layout was set from, exactly as `wordIndex` is an index into that
  paragraph's word list.
- **`layout/InitialLetter.h`** — `InitialLetter`, the block's opening set
  large enough to span several lines; `initialLetterSize()`, the size the
  rule derives; and `PlacedInitial`, what the layout reports about where it
  put one.
- **`layout/ParagraphLayout.h`** — `ParagraphLayout`, `layoutParagraph()`
  and `layoutSingleLine()`. Includes the four above.
- **`layout/Beside.h`** — setting a run beside another's extent:
  `bandBeside()`, `layoutBeside()` and `shareOfReading()`, the three
  questions a reading over or beside a base is made of.
- **`layout/Story.h`** — `Story`: a `RichText` and the `ParagraphStyle`s
  its blocks are set under, the value a chain of frames is filled from.

**`decoration`** — `SigilWeaveDecoration`:

- **`decoration/Decoration.h`** — a decoration resolved against a run's
  metrics: `detail::resolveDecorationBand()`, `decorationBandPaint()`,
  `decorationSegments()`. Skip-ink intercepts are memoized on the blob's
  id and the band window, folded into a key with Boost's stir: the table
  lives inside one run and no bucket of it is ever seen from outside the
  process, so the fold does not have to be one whose answer is pinned.
- **`decoration/DecorationRects.h`** — the walk that turns a layout's
  decorations into rectangles with their paint, `detail::forEachDecorationRect()`,
  run by both draws.

**`paint`** — `SigilWeavePaint`: `ParagraphLayout::draw()` and
`drawBatched()`. They are declared on `ParagraphLayout` in
`layout/ParagraphLayout.h` and defined here, so a program that draws links
this archive.

- **`paint/Paint.h`** — the feature's face: `paint::draw()` and
  `paint::drawBatched()`, the same draws as free functions over a layout,
  and `paint::setMaterialResolver()`, the seam a pass carrying a
  SigilMaterial instance is shaded through. The archive links no renderer;
  a host that wants materials shaded installs SigilMaterial's Skia backend
  there, with the pass's bounds as the material's resolution.

**`choreograph`** — `SigilWeaveChoreograph`, optional:

- **`choreograph/PlacedGlyph.h`** — `PlacedGlyph` and
  `forEachPlacedGlyph()`, which walks a layout's glyphs as rest pose, span
  paint, and where each sits in the text.
- **`choreograph/GlyphDress.h`** — `GlyphDress` (placement, fade, tint,
  face, matrix), `quantizeAngle()` and the memoized `tintFilter()`.
- **`choreograph/GlyphBatches.h`** — `GlyphRSXformBatches`, which
  collapses thousands of animated letters into a few `drawGlyphsRSXform`
  calls.
- **`choreograph/Choreograph.h`** — the three above.

**Text service — `layout/TextContext.h`:** `TextContext` owns or borrows a
font service and manages paragraph reuse internally. Configure
`TextContextOptions::paragraphCacheEntries` (zero disables retention), then
measure or lay out text with its full style. `TextLayout` retains the text
its runs borrow, so eviction and later calls cannot invalidate a result.
The service is part of `SigilWeaveLayout`; drawing links `SigilWeavePaint`.

**`query`** — `SigilWeaveQuery`, optional: `query/Query.h` finds ranges by
substring, word, or ICU regex; `MarkerSet` tracks named ranges across
edits, DOM-Range style. `query/Selector.h` is the same question written
down rather than asked: `Selector` and the `selectors::` vocabulary.

Separate from the engine: **`ports`** (`ports/SystemFontManager.h`, the OS
font manager), **`kit`** (`kit/`, the companion utilities, with its own
README) and **`qt`** (`qt/SigilWeaveQt.h`, the Qt bridge).

### The Unicode leaf

`<sigilweave/unicode/Unicode.h>` (target `SigilWeaveUnicode`, namespace
`sigil::weave::unicode`) is every Unicode question the engine asks,
answered as plain values over UTF-16 text — no Skia, no other header of
this library. ICU answers all of it but one: the tag a shaper is told a
run's script in is HarfBuzz's own translation of an ICU script code, so
the leaf links HarfBuzz's ICU bridge as well and no feature above it opens
either header:

| Function | Answer |
|---|---|
| `toUtf16` / `toUtf8` / `decodeAt` | transcoding and code-point decoding |
| `isWhitespace`, `isHardLineBreak`, `inheritsTypeface`, `mayRequireBidi`, `isLetter`, `isUpperCase`, `isFullWidth`, `verticalOrientation` | per-character properties, each one ICU's own answer for that character: what separates words, what forces a line end, what takes its neighbour's typeface, what can turn a paragraph bidirectional, what is a letter and what is an upper-case one, what stands in a full-width cell (East Asian Width), how a character stands in a vertical column (UTR#50) |
| `scriptOf`, `scriptShortName`, `itemize`, `shaperScript` | scripts: a character's own, its four-letter ISO 15924 code, the text split into `ScriptRun`s with Common and Inherited characters attached to their neighbours, and the tag a shaper takes for a run |
| `lineStartProhibited`, `lineEndProhibited` | the code points whose UAX#14 line-break class says a line may not begin, or may not end, with them — the property listing a prohibition table is derived from, not a decision about a line |
| `caseMap` / `caseMapped`, `lowerCased` | locale-aware upper, lower and first-code-point title case over text, and the simple one-code-point lower-case mapping a table is matched under |
| `lineBreaks`, `graphemeBoundaries`, `wordBoundaries`, `sentenceStarts` | UAX#14 break opportunities — each one a `LineBreak`, an offset and whether the text DEMANDS the break there — under an optional locale tailoring, and UAX#29 grapheme, word and sentence segmentation as ascending offsets |
| `bidi` | UAX#9 embedding levels as `BidiRun`s against a chosen `BaseDirection` |

The engine consumes it privately: `Paragraph` runs `lineBreaks`, `itemize`
and `bidi` when it analyzes, `caseMap` just before it shapes a transformed
segment, and `sentenceStarts` on the first walk after an edit. Nothing in
the engine's public headers names one of its types, so a consumer that
wants the analysis without the fonts links the leaf alone. The scratch
objects the analyses reuse (ICU break iterators, the bidi analyzer) are
thread-local, so every function is safe from any thread.

## Paragraphs: what a BLOCK is set like

A `Paragraph` is one styled string, and a hard break inside it separates
**blocks** — the paragraphs a reader sees. A block is styled by telling the
layout stage about it, not by carrying anything on the text:

```cpp
ParagraphStyle body;
body.leading = Leading::multiple(1.45f);
body.spaceAfter = 10.0f;
body.indent.firstLine = 18.0f;

ParagraphStyle heading;
heading.leading = Leading::grid(24.0f);   // shares a rhythm with the body
heading.spaceAfter = 16.0f;
heading.alignment = TextAlignment::kCenter;
heading.keep.withNext = true;

ParagraphLayoutOptions options;
options.blocks = {heading, body, body};   // one entry per block, in order
```

`ParagraphLayoutOptions`'s own `alignment`, `justification`, `hyphenation`
and `tabStops` are the WHOLE LAYOUT'S answer; a block states its own by
setting the matching optional and inherits it otherwise. A layout with no
`blocks` lays out exactly as one that never heard of them.

**Pitch is per block.** `Leading::face()` takes that block's first span's
own line height — which is what a text that says nothing has always used —
and `multiple`, `absolute` and `grid` state it otherwise. The extra a
leading opens goes ABOVE the line, where leading has always gone.
`Leading::grid(24)` rounds the block's own height up to a whole number of
grid steps and snaps each band's near edge to one, so two blocks on the
same grid share one rhythm however differently their faces are cut. In a
vertical setting the pitch is the width of the block's columns, so two
blocks of different pitch are two column sets one after the other.

**One spacing rule.** The gap between two blocks is the LARGER of the
first's `spaceAfter` and the second's `spaceBefore`. Everywhere, including
at the head of the flow, with no exception to remember: neither CSS
collapsing nor a suppression at the top of a frame.

**Indents are geometry.** `IndentOptions` insets the intervals a geometry
handed back — `start` and `end` on every line, `firstLine` and `lastLine`
added to `start` on the first and last. A negative `firstLine` is the
hanging indent a bullet or a number hangs into. Because it is arithmetic on
the interval, an indent composes with exclusions and columns without either
knowing about it: a line an exclusion cut into three is inset at its
outermost ends and nowhere in the middle.

**A block's opening set large is the block's own property.**
`ParagraphStyle::initial` declares an `InitialLetter`: how many lines its
reference metric spans, how many grapheme clusters of the opening it takes,
which metric that is (`InitialLetter::Align` — the cap height, the em box,
or the ascent a script hangs from), how many lines below the first baseline
its own baseline sinks, and how far the following lines stand off it.
Nothing there is a font size, because the SIZE IS DERIVED: the initial's
top reference point aligns with the first line's and its baseline with the
baseline of the line it sinks to, so its reference metric must span
`(lines - 1)` pitches plus the first line's own, and `initialLetterSize()`
answers what size gives the face that span. A letter chosen by eye is wrong
per typeface — ascent, descent and cap height differ between faces at one
size — and a letter sized by the rule is right in every one. The notch the
following lines wrap is pen travel taken off the head of a band, which is
the one thing every geometry answers in, so a block minus exclusions, a
column and a line riding a contour all wrap an initial with nothing written
for any of them; `InitialLetter::Wrap` says whether that notch is the
initial's advance box or the outline of its own glyphs, so a line can tuck
under the diagonal of an A. A block with fewer lines than the initial
sinks hands the rest of the cut to the block after it, and a column's
initial is set down the column and hangs from its head. The initial's glyphs are ordinary runs of the
layout and draw with everything else — `ParagraphLayout::initial` is the
report a caller rules a page against, not a second thing to draw. One
initial per layout pass: a block a frame before this one already opened is
resumed rather than opened again, and the initial belongs to the frame the
block began in.

**Keeps are settled at the frame boundary.** `KeepOptions` — widows,
orphans, keep-with-next, all-lines-together, start-in-next-frame — is a
statement about a JOIN between two frames, so the fill runs and then the
lines a block may not leave behind are taken back out of it and reported as
overflow, which is how they reach the next frame of the chain. No break is
re-decided and nothing is weighed against spacing, so both breakers obey
them identically. A retraction that would leave the frame empty is dropped:
the text would arrive at the next frame in exactly the state that emptied
this one. `widowLines` counts the lines the next frame would get, and the
next frame's MEASURE is a fact only whoever holds the chain knows —
`ParagraphLayoutOptions::nextMeasure` is where they state it, and a fill
told nothing counts at the measure this frame's last line was set in,
which is the same number for a chain of equal frames. The count is a
greedy fit either way, so a remainder whose hyphens or demerits would
have bought it a line comes out a line long.

**Justification spends in three passes.** The word gaps move first, from
`wordSpacing` towards `spaceStretch` / `spaceShrink`; what they could not
spend goes into letter spacing, bounded by
`letterSpacingMinimum`/`Maximum` as fractions of the em; what is still left
scales the glyphs across, bounded by `glyphScaleMinimum`/`Maximum`. A pass
whose limits equal its desired value contributes nothing and costs nothing,
which is why a caller who sets none of them gets word spacing alone and the
shared word blobs that go with it. The gaps are bounded at their stretch
limit only where a later pass has room to spend what they drop, and
whatever that pass then FAILS to spend — because it reached its own limit —
goes back to the gaps: a justified line reaches its measure whatever the
limits are, and the limits decide only how much of the fit stands between
the words and how much between the letters. Room above a desired value is
room the fit spends, so a value meant to hold is pinned either side of it.
`singleWord` decides what a line holding
one word does: align, or stretch across the measure on letter spacing.
Respacing and scaling are a straight-horizontal answer — a column and a
curve place per glyph already.

**Tab stops align their cell.** A `TabStop` states a position, what it pins
there (`kStart`, `kCenter`, `kEnd`, or `kCharacter` on a named character —
the decimal column a table of figures wants), and an optional `leader`
string set repeatedly across the gap it opened, butted against the stop so
the dots meet the figure. The BREAKERS fit against the start rule, because
every other alignment renders the same cell nearer its stop: a line that
fits under one fits under all.

**A frame seats what it holds.** `FrameOptions::firstBaseline` names where
baseline 0 sits below the flow's near edge — the first line's ascent, its
cap height, its x-height, its whole pitch, or a fixed offset — and every
later baseline follows at its own block's pitch, so seating a passage is one
number applied once. `distribute` spends the room left over: nothing,
centred, against the far edge, or spread between the lines as extra
leading. Both need `extent`, how deep the frame is, which a geometry knows
and the layout does not; 0 leaves both alone. Neither applies to a flow
whose intervals ride a contour, nor to runs whose glyphs are baked per
glyph.

**A reading beside the type is the engine's placement.** `layout/Beside.h`
answers the three questions setting one is made of: `bandBeside` is the
room a reading of a given type needs beside a line, which is what a block
reserves BEFORE anything is broken; `layoutBeside` sets the reading on one
line — or one column — centred on the extent its base occupied and clear
of its band, on the side that writing mode reads its furniture on; and
`shareOfReading` cuts a reading in the proportion a broken base's pieces
carry. None of them knows what a ruby IS, or which unit somebody
annotated, or how big a reading should be beside its base: a reading's
size is its own style's, and there is no fraction of anything in that
header.

**A script's own prohibitions come from the segmentation.**
`Paragraph::setLineBreakLocale` names the tailoring the line iterator runs
under — `"ja@lb=strict"` is the strict Japanese rule set a printed page is
set under, `"zh@lb=loose"` the loose Chinese one — and a tailored
prohibition is a boundary that never opens, so nothing downstream learns a
rule. A table is what a HOUSE adds on top of that.

**The room between two full-width characters is a table.**
`ParagraphLayoutOptions::mojikumi` gives each class of character its
members and each ORDERED PAIR of classes the room between them, as a
fraction of the em — negative closes the gap up, which is what nearly every
entry of a real table does, since an opening bracket carries its ink in its
right half and a closing bracket in its left. `tsume` closes the gap
between two plain full-width characters by a fraction of the em on top of
that. Both are applied where two characters meet ACROSS A BREAK
OPPORTUNITY, which in a text set in full-width characters is nearly every
gap it has; two characters shaped inside one word are set by the face and
by the shaper, and no table moves them. Which characters are of which class
is a house's decision and is therefore data; whether a character stands in
a full-width cell at all is the character's own property and the engine
answers it.

**A line's two edges are tables.** `ParagraphLayoutOptions::kinsoku` says
which characters may not open or close a line, and the prohibition is
settled during SEGMENTATION — the boundary is simply never opened — so
neither breaker learns a rule and both obey it. `hanging` says how far a
character may stand OUTSIDE the measure, as a fraction of its own advance:
a line that begins on a quote or ends in a comma then squares optically
rather than on its advances, which is optical margin alignment down a page
and burasagari down a column. Both are DATA; the kit ships stock tables
(`kit/LineTables.h`) and a caller's own is a peer of them. The stock
prohibition set is DERIVED rather than typed: the line-break class each
character carries says what may not open or close a line, and the set is
narrowed to the characters standing in a full-width cell, which is the
punctuation of the ideographic grid and exactly what the convention is
about.

**Hyphenation is two decisions in two places.** WHERE a word may break is
segmentation, so the whole layout shares it: the soft hyphens the author
typed, plus whatever a `Hyphenator` finds inside a word under
`HyphenationOptions::limits` (minimum word length, letters before and
after, capitalised words). WHICH of those a line takes is a break decision,
so a block owns it: `consecutiveLimit`, `zone`, `lastWordOfBlock`. The
`zone` is the band at the ragged edge inside which a line is already square
enough for the eye — asked of the line WITHOUT the break, so a word broken
to reach past a word that already ends inside the band is a hyphen neither
breaker takes — and a word that is the whole line is still broken, having
nothing else on the line to be measured against. The kit carries the
pattern engine and one pattern table (see below); the engine decides
nothing about where a word breaks, because that is a fact about its
language. Liang's method matches LETTERS, of any script, so a table
published for any language loads into it and English is one language among
them rather than the only one.

## What the engine covers

- **Decorations** — underline, strikethrough, overline, highlight, on
  `PaintStyle::decorations`. Thickness and position default to the font's own
  metrics; underlines skip ink around descenders. Span is per-decoration:
  `kDecoratedRange` merges contiguous same-style runs on a line into one band
  that covers the gaps between words (CSS behavior), `kPerWord` draws one
  band per word (squiggles, chips). `Decoration::paint` takes a full `SkPaint`
  applied verbatim, resolved independently of the glyph paint, so a shaded
  band can sit under plain ink. **Down a column the band turns with the
  type**: an underline runs beside the column on its right — the side a
  vertical setting reads its emphasis line on — an overline on its left, a
  strikethrough down the column axis, and a highlight across the whole em
  box; `Decoration::offset` is then a signed distance ACROSS the column.
  Ink skipping is a line's alone: intercepts are cut out of a horizontal
  band window, so a column's band is continuous. `Decoration::side` picks
  which side of the run's own axis an underline or an overline anchors on
  — `Side::kOpposite` is the other one's anchor, so a column's underline
  moves to the left and a line's above the type. A strikethrough and a
  highlight cross the type rather than standing beside it and have no
  second side; nor does a decoration with an explicit `offset`, which
  names the near edge outright.
- **Paint layers** — ordered underlays and overlays around the foreground,
  each a complete `SkPaint` plus an offset; `kit::dropShadow`, `glow`,
  and `outline` are presets over that. Each layer costs one more draw per
  bucket.
- **Variable fonts** — `shaping.variations = {{"wght", 700}}`, or the fluent
  `style.weight(650)`. `FontContext` memoizes the varied clone, so HarfBuzz
  and Skia agree on the design position and the varied face has a stable
  cache identity. An advance-invariant axis can instead be driven at *draw*
  time through `ParagraphLayout::LiveVariations`, with no re-shape.
- **OpenType features and text transform** — per-span features (part of the
  cache key) and locale-aware ICU case mapping applied just before shaping.
  The stored text, edit ranges, and query results stay untransformed.
- **Spacing** — `letterSpacing` (tracking, JIS aki in vertical text),
  `wordSpacing` (added to inter-word glue after measurement), `scaleX`
  (horizontal condensation of glyph shapes *and* advances, for faces with no
  `wdth` axis).
- **Vertical CJK** — `WritingMode::kVerticalRL` with per-character UTR#50
  orientation, `vert` forms, and per-span `VerticalForm` overrides (upright,
  rotated, tate-chu-yoko). Shaping a run top-to-bottom applies the face's
  `vert` substitutions and reads its vertical metrics on its own; everything
  else a column may want from the face — the wider `vrt2` rotation set,
  punctuation recentred (`valt`) or fitted (`vpal`, `vhal`), kana cut for a
  column (`vkna`), vertical kerning (`vkrn`) — is a feature a style names,
  spelled in `kit/Features.h`. A named feature is not gated on the
  direction: it runs whichever way the run is set, so those belong on the
  styles a passage sets vertically. `columnMetrics()` measures the result, and a
  dressed glyph in a column sets `GlyphDress::centreOffset` because half its
  advance is a step down the page rather than across it.
  `FontContext::glyphAdvanceEm()` reports either axis's advance in ems, for a
  caller asking whether two glyphs step the pen alike — the vertical advance
  is a fact Skia's glyph metrics do not carry at all. A column takes the
  furniture a line takes: an exclusion cuts it (`FlowAxis::kColumns`), and
  a clamp ends it in the overflow marker, at the column's foot.
- **Font fallback** — per-codepoint, per-language, memoized, with an ASCII
  direct-mapped fast table. The default resolver uses the `SkFontMgr`'s
  platform cascade; supply a `FontContext::FallbackResolver` to encode your
  own family list or script policy.
- **Inline placeholders** — pills, icons, and images woven into the flow. The
  breakers treat each as an unbreakable word; `placeholderRects()` reports
  where they landed.
- **Per-glyph choreography** — `forEachPlacedGlyph()` (`choreograph/PlacedGlyph.h`) hands
  every glyph of a finished layout to a visitor as one `PlacedGlyph`: the
  shaped run it came from, its glyph ID and advance, the absolute rest
  position the layout placed it at, its span's whole `PaintStyle`, and the
  identity an effect selects on — position in the walk, index within the
  shaped run, UTF-16 cluster, the same cluster as a text offset, and word,
  line, style-span and sentence indices. A glyph the layout TURNED — one on
  a contour, one on a rotated interval — carries the tangent it faces and
  the interval and pen coordinate it was placed at, so it can be re-placed
  at draw time from the same geometry. Displace, rotate and fade from there,
  accumulate into `GlyphRSXformBatches`, and draw.
- **Line metrics** — `lineMetrics()` derives per-line baseline, ascent and
  descent band, advance extent, and character range from the placed runs.
  Selection bands and point-to-line hit-testing are `lineMetrics()[i].rect()`
  plus ordinary canvas drawing; nothing is stored during layout and callers
  who never ask pay nothing. `columnMetrics()` is the same query for the
  other writing mode: a column has no baseline, so it reports the axis, the
  flow's pitch (also carried on `ParagraphLayout::linePitch`) and how far
  down the axis the runs reached. Exactly one of the two answers in any
  given layout.
- **Tab stops, overflow ellipsis, line clamp** — `TabStopOptions`,
  `OverflowOptions::ellipsis` and `OverflowOptions::maxLines`.
  The clamp counts COLUMNS in a vertical flow, and the marker stands for
  the text that was cut, so it is set the way that text was set: upright
  after upright glyphs — the face's own `vert` form when it has one — and
  turned with the column after a rotated run.

## What this covers, control by control

The control-by-control parity table — every control a page-layout
application's paragraph and character panels present, where it stands
here, and how a SigilCompose author spells it — is `PARITY.md` beside
this file.

## What a frame costs: the live composer

**Settled text is the special case here, not the moving kind.** A page that
is laid out once and then read is the easy end of what this engine is for;
the ordinary end is text whose measure animates, whose frame grows, whose
CONTENT changes from one frame to the next — a scramble, a decode, a
counter, a feed. So the optimizing breaker is designed to run every frame
rather than to be avoided while something moves, and
`ParagraphLayoutOptions::live` is how a caller says an input is moving.

**What is precomputed, and where it lives.** Everything that depends on the
TEXT and not on the frame is settled when a word is shaped and kept with
the paragraph: the word's advance, its trailing glue, the shaped hyphen a
discretionary break would render. Shaping is content-addressed and
incremental, so a frame that changes one word in twenty re-shapes those
words and reads the rest out of the cache. The composer builds nothing per
frame that a word already knows.

**What a frame does.** The break decisions, and then the fill. The
decisions are a dynamic program over contiguous arrays the thread owns
rather than allocates, with the active paths windowed — a path whose line
is already overfull is retired where it is found, a uniform measure merges
every path that reached one breakpoint into one, and a bounded window is
the floor under a geometry that neither of those bounds. A block set in a
uniform measure is broken against that measure alone and never walks the
geometry while deciding, so the lines are asked for only as they are
placed.

**What a frame does not do twice.** Break decisions are kept per thread,
keyed on the paragraph, its word revision, the block, the setting, and the
measure taken to the whole pixel below it. A measure already seen is
answered from that store and the frame costs its fill alone
(`ParagraphLayout::reusedBlocks` says so); a frame that changes only in
DEPTH changes which lines it holds and never where they break; a change of
content misses, because the word revision moved. The store holds the most
recently answered blocks and forgets the rest, so an animating width keeps
the pixels it has just crossed.

**Who decides that a text is settled.** Not this library. A layout is TOLD
that an input is moving and REPORTS what it did about it — how many blocks
it answered from break decisions it already had, how many it had to hand to
the greedy breaker — and a host folds those facts into its own proof that a
node is holding still, beside every other input that node has. There is one
such proof in a runtime and this is not it: a second answer to "has this
settled" is a second answer that can disagree.

SigilCompose is the worked example. `Element::live` is the declaration,
`Composer::settling` hands the two numbers back, and the one bit its
caching proof reads off them is whether the passage still composed this
frame — a passage answered entirely from the store is set exactly as the
frame before it, and one that still decided a break can be set
differently next frame with no number on the node moving, which is what
no value memo can see.

**A degrade is provisional, not a decision.** The block was filled
greedily for that frame alone and the setting the caller asked for is
still what the passage wants, so a host holding the layout must not treat
it as the answer for that measure: the next frame asks again, and
everything is back the frame the budget is met. SigilCompose's text leaf
does exactly that — it drops the measure the degraded layout was held
for, so the leaf lays out again.

**The floor under a frame that cannot be composed in time.**
`KnuthPlassOptions::budgetMicroseconds` is a degrade and not a policy: a
block the composer cannot finish inside it is filled greedily for that
frame and counted in `ParagraphLayout::degradedBlocks`. A degrade drops
the whole setting and not the breaker alone — the hyphens, the
justification passes past the word gaps, and the widow rule (the one keep
that has to count lines the frame cannot see) go with it, while the keeps
that cost nothing are enforced as always — and everything is back the next
frame the budget is met. A layout that reports degrades every frame is
asking for a longer budget or a shorter block, not for a different
breaker.

**The budget the arms hold.** `weave_bench` carries one arm per
mechanism — the paragraph controls, hyphenation, the justification ranges,
a reserved band, a 600-word story re-filled through a chain of six frames,
the live composer at an animating measure, at a measure it has already
seen, and under one word in twenty churning every frame. The behavioural
constant they are held to: a 600-word story's re-fill, and one frame of the
live composer on it, each stay a small fraction of a 60 Hz frame on one
thread, leaving the frame to the drawing. The numbers themselves live in
the ledger, never here.

## The hard parts

These are the places where the implementation is not the obvious one, and
where a change is most likely to break something quietly.

**Breaking against a list of intervals, not a width.** Classical line
breaking asks "does the next word fit in the measure?". Here each line may
offer several intervals of different lengths — the gaps a set of exclusion
shapes leaves behind — and the answer depends on which one the pen is in.
The greedy breaker therefore has to survive a word that fits in *no*
interval: it records the widest interval it skipped over, and when the
geometry runs out (or it has skipped too many) it backs up to that interval
and forces the word there. Without that, a long word either drops the rest
of the paragraph or jams itself into whatever narrow sliver the skip run
happened to stop on, visibly overflowing into an exclusion shape.

**Knuth-Plass, made to always terminate.** Three departures from the
textbook algorithm:

- *Badness saturates.* A stretch-free underfull line is terrible but must
  stay finite. Let badness reach infinity and the squared demerits overflow
  and poison every surviving path, which loses whole paragraphs on narrow,
  hyphen-heavy measures.
- *A lifeline break.* When no feasible break survives at some boundary, the
  least-bad candidate is force-accepted, uniformly penalized so any feasible
  path still beats it. A loose line is preferred to an overfull one
  regardless of demerits: loose merely looks bad, overfull leaks past the
  measure.
- *An emergency rerun.* If that lifeline ever had to accept an overfull
  line, the entire pass is redone with each line's own width added to its
  stretchability (TeX's `\emergencystretch`), which turns loose lines into
  real break nodes. Overfull is then forced only when a single box is wider
  than its line.

On uniform geometry the breaker also merges paths that reached the same
breakpoint on different line numbers — their futures are identical — which
is what keeps the active list bounded by the measure instead of growing with
the paragraph.

**Justification has three kinds of gap.** Rigid, space, and ideographic.
CJK has no spaces at all, so zero-width ideographic break opportunities are
the only thing that can absorb slack, and they expand up to a per-gap cap
expressed as a fraction of the font size. Shrink is clamped at the glue's
shrink limit. Gaps at or before a line's last tab are rigid: stretching them
would move the following tab stop and unpin the column, so only the gaps
past the last tab absorb slack.

**The break inside a word is discretionary either way.** It reaches the
breakers as a soft hyphen (U+00AD): the ones the author typed, and the ones
a `Hyphenator` proposed during segmentation. Both breakers then treat
them as break opportunities that are invisible unless a line actually breaks
there, in which case a styled hyphen is rendered, and Knuth-Plass charges the
configured penalty per hyphenated line. `hyphenation.enabled = false` removes
the opportunity rather than just the glyph: the two halves fuse into one
unbreakable word during segmentation — `Paragraph::setSoftHyphenBreaks` is
that switch, and `layoutParagraph` throws it from the option — so the word
wraps or overflows whole, the way `hyphens: none` does. It changes the word
list, so it re-runs the analysis, and the fused word is its own
content-addressed shaping entry.

**Text on a path.** Each glyph is anchored by its *advance center* on the
baseline point, not by its origin — with the offsets HarfBuzz applied on top
of the pen position backed out first, or accented glyphs drift off the curve.
Closed contours wrap their arc positions, so animating an interval's
`contourStart` gives an infinite marquee around the loop; an interval that is
closed in geometry without being *flagged* closed says so with
`LineInterval::wrapContour`, and a negative `advanceScale` walks the contour
backwards so a run can read right way up along the lower half of a ring.
Tangents are quantized to a fixed number of directions by default, because
every distinct rotation mints a fresh glyph-atlas strike, and continuously
varying per-glyph rotations turn animated curved text into a per-frame
mask-rasterization storm. Set `pathText.tangentRotationSteps = 0` for exact
rotations on static artwork.

`LineInterval::placeAt` is that mapping, and it is public: a pen coordinate
on the interval, plus a phase, gives the baseline point and the unit tangent.
The layout bakes its blobs through it, so a caller that re-places those
glyphs at draw time — to run a marquee, or to compose per-glyph effects on
top of curved lettering — reads the same function the blob was built from and
the two cannot disagree. It reports whether the pen fell outside an open
contour, so a caller may drop a glyph that ran off the end rather than pile
it on the last point.

**A transformed run is not opaque to choreography.** The layout keeps
the intervals it consumed (`ParagraphLayout::intervals`) and each run reports
which one it landed on and where its pen started, so `forEachPlacedGlyph`
gives a glyph on a curve its true `rest` position, the `tangent` it was
turned to, and the `pen`/`intervalIndex` pair that re-places it. Every
per-glyph dressing — a fade, a tint, a driven variable-font axis, a
substituted code point — therefore reaches curved lettering exactly as it
reaches straight lettering. What still draws from baked blobs, and still
ignores the override, is `ParagraphLayout::LiveVariations`.

## Conventions and gotchas

Read this section before writing against the library. Most of it is not
discoverable from a signature.

**Threading.** A `FontContext` is single-threaded by contract and contains no
locks. The shape cache and the HarfBuzz buffer are reused scratch, not
per-call state. Create one per layout thread; parallelism belongs above the
library, one paragraph per task with zero shared state. Several hot paths
also use `thread_local` scratch (the ICU break iterators and bidi analyzer
among them), so a context must not migrate between threads mid-use.

**Typeface lifetime.** Every cache keys off `SkTypeface::uniqueID()`.
Typefaces must outlive the context, or be consistently owned by it.

**Shape-cache eviction is a wholesale clear**, not LRU: past its cap the
shape cache empties in one go and re-fills, costing one cold frame. The
per-typeface, fallback, and varied-typeface maps are never pruned at all —
`purgeAllCaches()` is the manual reset for a long-lived process whose
typeface population churns. It is safe to call while shaped-word references
are outstanding, because a `ShapedWord` owns its own data. The full purge
also releases optical-kerning profiles and per-face reference gaps; a shape-
only purge retains those measurements. The font statistics count their
queries independently of shaping calls. (The tint-filter
table behind `GlyphRSXformBatches` is the one LRU: past its cap it drops
its coldest entry rather than everything, so a working set sitting at the
cap keeps the filter identities its batching depends on.)

**A varied clone from `variedTypeface()` is retained forever.** The memo is
keyed on the coordinate's exact bytes, has no cap and no eviction, and
`purgeAllCaches()` is the only thing that empties it. That is right for a
coordinate drawn from a bounded set and wrong for one that varies
continuously, which would add a permanently held clone per frame for the
life of the process. `variedTypefaceTransient()` is the entry point for the
latter: it builds the clone and retains nothing, so the cost is constant
per frame instead of growing, and the face has no stable identity — which
rules it out of `ShapingStyle::variations` and suits a draw-time drive,
where the identity is only a batch key inside one frame.

**All range APIs are UTF-16 code-unit offsets, end-exclusive.** UTF-8 entry
points take `std::u8string_view` specifically, so the encoding contract rides
the type — use `u8` literals or `std::u8string`.

**Coordinates are Skia's: y grows down.** A decoration's `offset` is the
band's *top edge relative to the baseline*, positive meaning below it. Ascent
and descent are reported as positive magnitudes. The horizontal fast path
tests for a direction of exactly (1, 0) and the vertical one for exactly
(0, 1); anything else takes the transformed path.

**On contour intervals, length, fitting and alignment stay in unscaled
advance units.** Only the pen-to-arc mapping is scaled by `advanceScale`. To
offer a whole contour, set `length = arcLength / advanceScale`.

**Rendering must match shaping.** Build draw fonts with `makeFont()` — it
sets the unhinted, linear-metrics, size-gated-subpixel configuration the
shaper measured against — or glyphs drift off their shaped positions. Related:
Skia takes glyph edging from the *font*, never the paint, so
`paint.setAntiAlias(false)` is silently ignored for text. Ask for hard edges
with `ShapingStyle::aliased` instead.

**A per-glyph walk is stable, and its batches are keyed by paint.**
`forEachPlacedGlyph()` enumerates in draw order, and that order does not
change across relayouts while the text is unchanged — which is what lets an
effect key particle state on a glyph's position in the walk. Sentence indices
come from an ICU pass over the text that runs on the first walk after an edit
and is reused by every walk after it; a paint edit does not invalidate it.
`GlyphRSXformBatches` buckets on (typeface, size, condensation, edging,
resolved paint pass, pass band), and a glyph is added once per pass of its
`PaintStyle` — each underlay in order, then the foreground, then each
overlay — so an animated letter keeps its gradients, strokes and mask
filters, and each pass costs one more `drawGlyphsRSXform` call. Buckets
draw band by band — every underlay bucket, then every foreground bucket,
then every overlay bucket, each band in creation order — so every underlay
lands beneath every foreground even when per-glyph fades split one style
into several buckets; a blurred halo reaches past its own glyph, so
creation order alone would lay a late-fading letter's halo over its
neighbour's stroke. A per-glyph fade rides `alphaScale` instead of a
per-glyph style; quantize it when an effect drives it continuously, because
distinct alphas are distinct buckets.
Batched glyphs draw with their rotations quantized: a continuous per-letter
angle mints a fresh glyph-atlas strike per letter per frame.

**`GlyphRSXformBatches::subpixel` is the caller's declaration that the
glyphs it is adding MOVE between frames**, and it decides whether their
origins land on Skia's subpixel phase grid or on whole pixels. It is off by
default, because the phases are the second factor in a product: every mask
is a (glyph, rotation, phase) triple, and the phases multiply what a
rotation ladder has already multiplied, on both axes for an off-axis run. A
run at REST gains nothing — its letters are not creeping anywhere — and
would pay that multiplied population for a placement no one can see move. A
MOVING run's arithmetic runs the other way: its masks were never going to be
re-used, since the rotation it needs this frame is a different rotation next
frame, so the phase grid only refines a mask it was going to rasterize
regardless. Left on whole pixels, a run creeping by a fraction of a pixel
per frame does not creep at all — each letter stands still until its own
origin crosses a pixel boundary and then hops a whole one. This is the same
trade the rotation ladder makes and not a competing one: the ladder still
bounds the rotations, and dropping it in exchange multiplies the mask
population by the rotation count.

**A `GlyphDress` carries what varies per glyph** rather than per pass — the
placement, the fade, three colour terms (a `colorMultiplier` tint, a `colorAdd`
flash added after it, and a `colorScreen` glow screened over both — the two
brightening terms a multiplier cannot say), a `face` override for a glyph
drawn through a varied clone, and a `matrix` for the placements an RSXform
cannot express (a shear, a non-uniform scale). The face joins the bucket
key; the fade and the colour terms change only each pass's resolved paint,
and on a shader pass all three terms fold into one memoized modulating
colour filter — screening against a constant is affine per channel — because
a batch's key is a whole `SkPaint` and `SkPaint` compares its colour filter
by pointer. A
matrix glyph draws in its own bucket's lane, after that bucket's RSXform
glyphs — same font, same paint, same place in the pass order, at the cost of
one canvas concat and one draw each.

**Shaping style versus paint style.** Any change to a shaping field re-shapes
the words it covers. Paint changes never re-shape and never relayout, and
they are visible to an *already-computed* `ParagraphLayout`, because `draw()`
resolves paint per span at draw time. `wordSpacing` is the odd one out: it
lives in the shaping style and is compared for restyle detection, but it is
not part of the shape-cache key — it is applied to whitespace after
measurement, so changing it re-derives words at pure cache-hit cost.

**Variable-font variation lists are order-sensitive for memo identity.** A
permuted list resolves to an equivalent face but occupies a second memo
entry, so keep the order stable across call sites. For draw-time animation
only advance-invariant axes are safe; ask
`FontContext::axisIsAdvanceInvariant()` before driving one through
`LiveVariations`. An axis that fails that test belongs in
`ShapingStyle::variations`, which re-shapes.

**Placeholders match records by occurrence order** of the object-replacement
character (U+FFFC) in the text, so a direct text edit must not add or remove
one.

**Two `[[nodiscard]]` returns mean "rebuild your ranges".**
`Paragraph::editsSince()` and `MarkerSet::synchronize()` both return false
when the bounded edit log no longer reaches back to the caller's revision.
Ignoring that silently corrupts tracked ranges. The log is halved when it
fills rather than trimmed one entry at a time, so the lookback you can count
on is half the cap, not the cap.

**The `languageTag` handed to a custom fallback resolver is a borrowed view**,
valid only for that call, and it is *not* guaranteed to be NUL-terminated.
Copy it before handing it to any C API; never pass its `.data()` through
directly.

**Several things silently no-op outside their scope.** Decorations render on
straight runs, set either way; a TRANSFORMED run (on a path, on a rotated
interval) skips them, and a column's band never skips ink. The
ellipsis marker requires the final interval to be straight and not a
contour — a line takes it at its end and a column at its foot, but a loop
has no end to put one at. `lineMetrics()` skips transformed and vertical runs, and omits
lines whose geometry placed nothing — `columnMetrics()` is what answers
there. Tab stops are line-local and scoped to
straight horizontal left-to-right intervals.

**Geometry is re-queried on every layout pass and never cached between
passes**, so an implementation may depend freely on animated state. For
exclusion flows, animate through `Exclusion::offset`: a silhouette caches
what answering costs it — a flattening, a grown outline, a distance field
— and
rigid motion reuses all of it, while a rebuilt shape (a morphing `SkPath`,
a new video frame) re-measures from scratch.

**Lazy shaping is ascending and idempotent only.** `ensureShapedTo()` with a
decreasing word count is not supported.
