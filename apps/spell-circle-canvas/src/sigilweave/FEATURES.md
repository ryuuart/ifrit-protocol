# SigilWeave features

The whole of what the engine does, section by section — the reference
`README.md` sends you to. The README is what the library is and how to
reach it; this is what it covers.

- [The flow geometries](#the-flow-geometries) — what a `LineRequest` means, and the ready-made ones
- [The pipeline](#the-pipeline) — the stages `layoutParagraph()` runs
- [The features and their headers](#the-features-and-their-headers)
- [Paragraphs: what a BLOCK is set like](#paragraphs-what-a-block-is-set-like)
- [What the engine covers](#what-the-engine-covers)
- [What this covers, control by control](#what-this-covers-control-by-control) — the parity table, with the compose path for every row
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
`ExclusionFlow` (a rectangle minus moving circles, rects, or arbitrary
`SkPath`s with their fill rule honored), `VerticalBlockFlow` (top-to-bottom
columns advancing right to left), `LineSetFlow` (explicit intervals — any
origin, direction, and count per line), and `PathFlow` (each contour of a
path becomes a line).

`ExclusionFlow` takes a `FlowAxis`, and that is the whole of what a column
costs it: `FlowAxis::kColumns` makes each band a top-to-bottom column
advancing right to left from the bounds' right edge, and reads every
shape's extent DOWN the column instead of across the line. A column is a
line turned a quarter turn — a shape shortens one, or splits it in two,
exactly as it shortens or splits the other — so the band scan, the fill
rule, the flattening cache and the sliver threshold are one implementation
read through two coordinates. Pair `kColumns` with
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
4. **Geometry flattening.** Every line's intervals are flattened, lazily,
   into a single indexed `IntervalSequence`. Both breakers consume geometry
   *only* through it, so a break decision and the placement that follows can
   never disagree about which interval is which.
5. **Line breaking.** Greedy or Knuth-Plass (see below).
6. **Lazy shaping.** Breakers call `ensureShapedTo()` just ahead of their own
   frontier, so a paragraph far larger than its geometry only ever sends the
   words that can actually land through HarfBuzz. Words past the last
   interval are never shaped at all.
7. **Shaping.** `shapeWord()` goes through the content-addressed shape cache.
   The cache is probed with a borrowed view of the key, so a warm
   re-analysis allocates nothing; an owning key is materialized only on a
   miss.
8. **Placement.** Words are reordered per UAX#9 rule L2 (reverse maximal runs
   of each level, highest first), then positioned inside their interval with
   the requested alignment.
9. **Blob emission.** One of four shapes per run: a straight horizontal run
   reuses the word's shared origin-relative blob translated to its origin; an
   upright vertical run does the same down a column; tate-chu-yoko reuses it
   centred across the column axis; anything rotated or on a contour bakes
   per-glyph `SkRSXform`s into a fresh blob.
10. **Ellipsis.** When set and the layout overflowed, the final placed line
    is trimmed until a shaped marker fits.
11. **Draw.** `draw()` emits one blob per word; `drawBatched()` merges
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
pulls the earlier ones in. `<sigilweave/SigilWeave.h>` is a transitional
umbrella over every engine feature, for code written against the flat
header tree; new code includes the feature headers it uses.

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
  draw time through `paint/Paint.h`'s resolver), `style/Decoration.h`
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
  from `ports::pickTypeface()`, which walks the system font manager.
- **`style/Features.h`** — named OpenType presets
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

**`layout`** — `SigilWeaveLayout`, with `SigilGeometryPath` public because
a contour interval carries a `geometry::path::Contour`:

- **`layout/Flow.h`** — `LineInterval`, the `FlowGeometry` interface, and
  the ready-made geometries.
- **`layout/LayoutOptions.h`** — `ParagraphLayoutOptions` and every
  options struct it groups: alignment, break strategy, line metrics,
  hyphenation, justification, Knuth-Plass, overflow, tab stops, path text.
- **`layout/PositionedRun.h`** — `PositionedRun`, one draw call, and the
  `LineMetrics` and `ColumnMetrics` bands derived from placed runs.
- **`layout/ParagraphLayout.h`** — `ParagraphLayout`, `layoutParagraph()`
  and `layoutSingleLine()`. Includes the three above.

**`decoration`** — `SigilWeaveDecoration`:

- **`decoration/Decoration.h`** — a decoration resolved against a run's
  metrics: `detail::resolveDecorationBand()`, `decorationBandPaint()`,
  `decorationSegments()`. Skip-ink intercepts are memoized on the blob's
  id and the band window, folded into a key with SigilCoreCompute's stir
  so a hash is one body wherever it is accumulated.
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
  the shaders feature's `PaintShaders::installMaterialResolver()` installs
  SigilMaterial's Skia backend there.

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

**`query`** — `SigilWeaveQuery`, optional: `query/Query.h` finds ranges by
substring, word, or ICU regex; `MarkerSet` tracks named ranges across
edits, DOM-Range style.

**`cache`** — `SigilWeaveCache`, optional:
`cache/SingleLineParagraphCache.h` caches single-style paragraphs by text,
typeface, and quantized size, for high-frequency labels.

Separate from the engine: **`shaders`** (`shaders/PaintShaders.h`,
animated paint presets — SigilMaterial's text paint recipes shaded for a
run's bounds and the clock), **`ports`** (`ports/SystemFontManager.h`, the OS
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
  each a complete `SkPaint` plus an offset; `PaintLayer::dropShadow`, `glow`,
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
  spelled in `style/Features.h`. A named feature is not gated on the
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
- **Tab stops, overflow ellipsis, line clamp** — see the options structs.
  The clamp counts COLUMNS in a vertical flow, and the marker stands for
  the text that was cut, so it is set the way that text was set: upright
  after upright glyphs — the face's own `vert` form when it has one — and
  turned with the column after a rotated run.

## What this covers, control by control

The list a page-layout application's paragraph and character panels
present, and where each one stands here. It is the FEATURE list: what a
typesetter reaches for, not what a file format carries.

**From compose** is how a SigilCompose author reaches the same control,
because a control nobody can spell is a control nobody has. Every row
either names a verb, a field on a value a verb takes, or a kit item —
and the ones that name nothing say so.

| Control | Status | Where | From compose |
| --- | --- | --- | --- |
| Alignment: left / centre / right / justify variants | done, per block | `ParagraphStyle::alignment` | `Element::textAlign`; per block through `Element::paragraphs` |
| Justification: word spacing min/desired/max | done | `JustificationOptions::wordSpacing`, `spaceStretch`, `spaceShrink` | `Element::justification` |
| Justification: letter spacing min/desired/max | done | `JustificationOptions::letterSpacing*` | `Element::justification` |
| Justification: glyph scaling min/desired/max | done | `JustificationOptions::glyphScale*` | `Element::justification` |
| Justification: single-word rule | done | `JustificationOptions::singleWord` | `Element::justification` |
| Left / right / first-line / last-line indent | done | `IndentOptions` | `ParagraphStyle::indent`, through `Element::paragraphs` |
| Space before / after | done, larger-of | `ParagraphStyle::spaceBefore`, `spaceAfter` | through `Element::paragraphs` |
| Leading: auto, multiple, absolute, baseline grid | done | `Leading` | `ParagraphStyle::leading`, through `Element::paragraphs` |
| Leading: all above the line, or half above and half below | done | `ParagraphStyle::halfLeading` | through `Element::paragraphs` |
| Keep: widows, orphans, with next, all lines together, start in next frame | done — enforced at the frame boundary by retracting lines into the next fill, under both breakers | `KeepOptions` | `ParagraphStyle::keep`, through `Element::paragraphs` |
| Hyphenation: pattern dictionary | done, for any language that has a pattern table — the engine matches letters of any script and a table declares the language it answers for; the kit carries English and a caller loads the rest | `HyphenationOptions::patterns`, `kit::PatternHyphenator`, `kit::englishHyphenationPatterns` | `Element::hyphenation` |
| Hyphenation: minimum word, letters before / after, capitalised words | done | `HyphenationLimits` | `Element::hyphenation` |
| Hyphenation: consecutive limit, last word of a block | done | `HyphenationOptions` | `Element::hyphenation`; per block through `Element::paragraphs` |
| Hyphenation: zone | done, both breakers | `HyphenationOptions::zone` | `Element::hyphenation` |
| Composer: single-line vs paragraph | done | `LineBreakStrategy` | `Element::lineBreak` |
| Composer: balance ragged lines | done — a bisection for the narrowest measure that keeps the line count, with the last line scored like every other. The narrowing is a FRACTION of each interval's own length, so a block an exclusion cut into unequal lines gives up the same proportion of every one of them; over a uniform measure the two searches coincide. ONE APPROXIMATION remains: the bisection stops after a fixed number of steps rather than at the exact fraction where the count turns over | `ParagraphStyle::balanceRaggedLines` | through `Element::paragraphs` |
| Composer: the live composer, and the budget under it | done — a moving input is declared, break decisions are kept per thread and reused at a measure already crossed, and a block the budget cannot finish is filled greedily for that frame and counted | `ParagraphLayoutOptions::live`, `KnuthPlassOptions::budgetMicroseconds`, `ParagraphLayout::reusedBlocks`, `degradedBlocks` | `Element::live`, reported by `Composer::settling` |
| Optical margin alignment (hanging punctuation) | done | `HangingTable`, `kit::hanging` | `Element::hanging` |
| Drop caps: lines × characters | done as compose kit — a text initial or caller-built ornament is an ordinary exclusion, and a declared silhouette shapes the opening lines | `kit::dropCap` | `kit::dropCap` |
| Drop caps: nested style | done as compose kit — the run stated in the text's own terms (words, a character count, or through a delimiter) and applied as a span restyle | `kit::NestedStyle`, `kit::nestedRun`, `kit::dropCap` | `kit::nestedRun` with `Element::spanStyle` |
| Bullets and numbering | done as compose kit | `kit::bullets` | `kit::bullets` |
| Tabs: position, leaders, alignment on a character | done | `TabStop` | `Element::tabStops`; per block through `Element::paragraphs` |
| Paragraph rules above / below, shading | done as compose kit | `kit::rules` | `kit::rules` |
| Paragraph border | **not started** | — | — |
| Nested styles, GREP styles, line styles | exists | `sel::regex`, `sel::line`, `sel::style`, span restyling | `Element::spanStyle`, `Element::spanPaint` over the same selectors |
| Named character styles | exists — a registry whose lookup always answers | `StyleSet` | `rich().add(text, name)` against `env::Provide<weave::StyleSet>` |
| Named paragraph styles | exists — the same registry shape for blocks | `ParagraphStyleSet` | `Element::paragraphs(names)` against `env::Provide<weave::ParagraphStyleSet>`; a name no set carries warns |
| Character: size, tracking, horizontal scale | exists | `ShapingStyle` | the `TextStyle` a `text()` or `rich()` run carries; `Element::spanStyle` |
| Character: metric kerning | exists (HarfBuzz) | shaping | the same style |
| Character: optical kerning | done, as a STATED APPROXIMATION: every adjacent pair of a word is measured — the narrowest distance between the left glyph's right edge and the right glyph's left, in bands off the outlines — and closed to the distance the FACE'S OWN even pair leaves, with the face's kerning table switched off because the two are answers to one question. A designer kerns by judging the white as an area and as a rhythm; this measures a distance, so a pair a designer would have opened for legibility comes out tighter. The library decides nothing about how tight type should be — the reference is the face's own — and no pair moves further than a stated bound | `ShapingStyle::opticalKerning` | the same field on the style a run carries; `Element::spanStyle` |
| Character: baseline shift | done | `PaintStyle::baselineShift` | the same field; `Element::spanPaint` |
| Character: skew | **not started** | — | — |
| OpenType features, small caps, figures, sets | exists | `style/Features.h` | `ShapingStyle::fontFeatures` on the style a run carries |
| Underline / strikethrough / overline / highlight options | exists | `Decoration` | `PaintStyle::decorations`; `Element::spanPaint` |
| Frame: columns, gutter | done as compose kit — a Western column is a FRAME | `kit::columns` | `kit::columns` |
| Frame: balance columns | **not started** | — | — |
| Frame: inset | exists | compose padding | `Element::padding` |
| Frame: vertical justification | done | `FrameOptions::distribute` | `Element::distribute` |
| Frame: first-baseline offset | done | `FrameOptions::firstBaseline` | `Element::firstBaseline` |
| Frame: auto-size | exists | compose measure | a leaf given no width measures its own content |
| Threading (in and out ports) | done | `layoutParagraph`'s resume word; the chain also states the next frame's measure through `ParagraphLayoutOptions::nextMeasure` | `Story`, `frame`, `Element::key` and `Element::thread` |
| Story-wide addressing | done — a story's words, characters, sentences and named runs are the story's already, and the LINE is what a frame chain renumbers | — | `sel::line` addresses the story, `sel::inFrame` is the frame-local address beside it, and a cascade's beats span the chain on one master progress |
| Text wrap: bounding box, object shape, offsets | exists | `ExclusionFlow`, compose `flowAround` | `Element::flowAround` |
| Text wrap: jump object, wrap to one side | **not started** | — | — |
| Anchored objects: inline | exists | `Placeholder`, `RichText::slot` | `rich().slot(name, size)` with `slot(name)` |
| Anchored objects: above line | done, for a READING — a band reserved above the line and filled with set text | compose `Element::annotate` | `Element::annotate` |
| Anchored objects: custom position | done as compose kit — an object tied to a text unit and placed at an offset the caller states, with the x and y references named separately (the unit, its line, or the frame) | compose `kit::annotate` with `kit::Anchored` | `kit::annotate` with `kit::Anchored` |
| Room reserved beside every line | done — a layout input, in the strut before anything is broken | `ReservedBand`, `ParagraphStyle::reserved` | `Element::reserve`; `Element::annotate` reserves its own on top |
| Type on a path: orient, flip, start / end, align | exists | `PathFlow`, compose `onPath` | `Element::onPath` |
| Type on a path: effects (skew, stair, gravity) | **not started** | — | — |
| Vertical writing (CJK columns) | exists | `WritingMode::kVerticalRL` on the Paragraph | `Element::writingMode` |
| CJK: tate-chu-yoko | exists | `VerticalForm::kTateChuYoko` | the same field on a run's style; `Element::spanStyle` |
| CJK: ruby — mono, group, jukugo | done | `layout/Beside.h`; compose `Annotation`, `kit::ruby` | `Element::annotate`, `kit::ruby` |
| CJK: kenten | done | `kit::kenten` | `Element::annotate`, `kit::kenten` |
| CJK: kinsoku | done — ICU's own strict/loose tailoring under a locale, plus a table over the segmentation for a house's own additions; the stock table is derived from the line-break class each character carries, narrowed to the full-width cell | `Paragraph::setLineBreakLocale`, `KinsokuTable`, `kit::kinsoku` | `Element::lineBreakLocale`, `Element::kinsoku` |
| CJK: burasagari | done — the hanging table, along the column | `HangingTable` | `Element::hanging` |
| CJK: mojikumi (per-class spacing) | done, as a table over the gaps between words — the class of each character is the table's, whether a character is full-width at all is Unicode's | `MojikumiTable`, `ParagraphLayoutOptions::mojikumi` | `Element::mojikumi` |
| CJK: tsume | done, as a fraction closed at every gap between two plain full-width characters. LIMIT: two characters shaped inside one word are set by the face and the shaper, and no fraction here moves them | `ParagraphLayoutOptions::tsume` | `Element::mojikumi`'s second argument |
| CJK: warichu | done — the note is cut where its two lines come closest in length and stacked inside the slot the base reserved | `warichuSplit`, `layoutWarichu` | **weave only.** The slot has to be sized from the note's own split, which is a question for a font context, and a compose description is written before there is one — so no verb reserves it yet. A caller holding a `FontContext` calls the two functions directly |
| Baseline grid | done | `Leading::grid` | `Leading::grid` through `Element::paragraphs` |
| Frame grid (CJK cell grid) | **not started** | — | — |
| What a decoration dresses | done — the node's shape, its glyph contours, or the silhouette it actually drew | — (a compose seam; weave supplies the glyph outlines) | `Element::boundary` |
| Footnotes and endnotes | out of scope | — | — |
| Tables | out of scope — a layout, not a text | — | — |
| Text variables, cross-references, conditional text | out of scope — data, not typography | — | — |

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

**The budget the arms hold.** `weave_layout_bench` carries one arm per
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
are outstanding, because a `ShapedWord` owns its own data. (The tint-filter
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
bounds the rotations, and dropping it in exchange costs several times what
the grid does.

**A `GlyphDress` carries what varies per glyph** rather than per pass — the
placement, the fade, three colour terms (a `colorMul` tint, a `colorAdd`
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
exclusion flows, animate through a shape's `pathOffset`: path flattening is
cached by the path's generation ID, so translating is free while assigning a
rebuilt `SkPath` changes the ID and re-flattens.

**Lazy shaping is ascending and idempotent only.** `ensureShapedTo()` with a
decreasing word count is not supported.
