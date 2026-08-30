# SigilWeave

A C++20 library that turns styled Unicode text into positioned glyph runs
ready to draw on a Skia canvas. It calls HarfBuzz for shaping and ICU for
line-break analysis, script itemization, bidi, case mapping and regular
expressions directly, instead of going through Skia's own SkShaper or
SkParagraph.

Two consequences follow from that, and they are the reason the library
exists.

**Every intermediate value is a public type.** The word list, each shaped
run, each text blob, each placed run — you can inspect them, hold them
across frames, reuse them in a different layout, or walk them per glyph.
Nothing important is sealed inside an opaque paragraph object.

**A line of text is not a rectangle.** Layout consumes an ordered list of
line *intervals*, supplied one line at a time by a geometry interface you
can implement. An interval is a straight segment in any direction, or a span
of an `SkPath` contour. So the same engine fills a block, flows around
arbitrary excluded shapes, runs down vertical CJK columns, or rides the
tangent of a Bezier curve — and none of those is a special mode inside the
breaker.

## Getting started

```cpp
#include <sigilweave/SigilWeave.h>
#include <sigilweave/ports/SystemFontManager.h>

using namespace sigil::weave;

// One context per layout thread. It owns every cache the pipeline leans on.
FontContext fonts(ports::systemFontManager());

TextStyle base;
base.shaping.fontSize = 28.0f;
base.paint.foreground.setColor(SK_ColorWHITE);

TextStyle accent = base;
accent.paint.foreground.setColor(SK_ColorRED);

ParagraphBuilder builder(base);
builder.addText(u8"Glyphs flow ")
    .pushStyle(accent)
    .addText(u8"around")
    .popStyle()
    .addText(u8" obstacles… 日本語も 한국어도 中文也");
Paragraph paragraph = builder.build();

// A rectangle with shapes punched out of it. Shapes are cheap to move:
// geometry is re-queried on every layout pass.
ExclusionFlow flow(SkRect::MakeWH(900, 700));
flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(circleBounds, 8));
flow.shapes().push_back(ExclusionFlow::Shape::fromPath(anyPath, 8));

ParagraphLayoutOptions options;
options.alignment = TextAlignment::kJustify;
options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
options.overflow.maxLines = 4;      // CSS line-clamp, over any geometry
options.overflow.ellipsis = u"…";

ParagraphLayout layout = layoutParagraph(fonts, paragraph, flow, options);
layout.drawBatched(canvas, paragraph);
```

Editing and restyling happen on the paragraph, and the shape cache absorbs
everything that did not actually change:

```cpp
paragraph.replaceText(4, 9, u8"swift");     // one word reaches HarfBuzz
paragraph.setPaint(0, 6, {SK_ColorRED});    // no re-shape, no relayout —
                                            // an existing layout sees it
```

`layout.runs` is a plain vector of `PositionedRun`, so walking the output
yourself is a first-class option; `draw()` and `drawBatched()` are
conveniences over it.

### Writing your own geometry

`FlowGeometry` is one virtual call. Implement it and the whole engine — both
breakers, justification, hyphenation, placement — applies to your shape:

```cpp
class SingleContourFlow : public FlowGeometry {
public:
  SingleContourFlow(geometry::Contour contour, float start)
      : m_contour(std::move(contour)), m_start(start) {}

  bool lineIntervals(int index, float lineHeight, float ascent,
                     std::vector<LineInterval> &intervals) override {
    if (index > 0)
      return false;              // one line only; false = geometry exhausted
    LineInterval interval;
    interval.contour = m_contour;
    interval.contourStart = m_start;   // animate this for a marquee
    interval.length = m_contour.length();
    intervals.push_back(interval);
    return true;
  }

private:
  geometry::Contour m_contour;   // geometry::Contour::of(path).front()
  float m_start;
};
```

A contour interval carries a `geometry::Contour` from SigilGeometryPath —
one sub-path addressed by arc length, built with `geometry::Contour::of(path)`.
The layout reads position and tangent through it, so "distance along" and
"closed wraps around" mean the same thing for text as for every other thing
that walks a path.

Ready-made geometries cover the common cases: `BlockFlow` (a rectangle),
`ExclusionFlow` (a rectangle minus moving circles, rects, or arbitrary
`SkPath`s with their fill rule honored), `VerticalBlockFlow` (top-to-bottom
columns advancing right to left), `LineSetFlow` (explicit intervals — any
origin, direction, and count per line), and `PathFlow` (each contour of a
path becomes a line).

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

## Headers

Include `<sigilweave/SigilWeave.h>` for everything, or the pieces:

| Header | What it is |
|---|---|
| `Style.h` | `TextStyle` = `ShapingStyle` (the shape-cache key) + `PaintStyle` (draw-time), plus `PaintLayer` and `Decoration`. The vocabulary every other header speaks. `StyleSet` is a small ordered registry of named styles, comparable by value, whose lookup always answers — an unregistered name resolves to the set's base entry. |
| `FontContext.h` | The per-thread service object: HarfBuzz faces, fallback memos, varied-typeface clones (retained, or transient for a continuously varying coordinate), the shape cache, observable `Stats`. |
| `Paragraph.h` | The document — UTF-16 text, normalized style spans, inline placeholders, writing mode, the edit log, sentence boundaries, and the analysis entry points. |
| `Flow.h` | `LineInterval`, the `FlowGeometry` interface, and the ready-made geometries. |
| `ParagraphLayout.h` | `layoutParagraph()`, `layoutSingleLine()`, all the options structs, `PositionedRun`, `LineMetrics`. |
| `Shaper.h` | `ShapedWord`, `shapeWord()`, `wordBlob()`, `makeFont()`. Reach for it to inspect or reuse individual glyph runs. |
| `Query.h` | Optional: find ranges by substring, word, or ICU regex; `MarkerSet` tracks named ranges across edits, DOM-Range style. |
| `Choreograph.h` | Optional: `forEachPlacedGlyph()` walks a layout's glyphs as `PlacedGlyph`s — rest pose, span paint, and where each sits in the text — and `GlyphRSXformBatches` collapses thousands of animated letters into a few `drawGlyphsRSXform` calls, each glyph dressed by a `GlyphDress` (placement, fade, tint, face, matrix). |
| `SingleLineParagraphCache.h` | Optional: caches single-style paragraphs by text, typeface, and quantized size, for high-frequency labels. |
| `Features.h` | Named OpenType presets (`Features::tabularNumbers`, `smallCaps`, `stylisticSet(n)`, …) so styles need not hand-spell four-cc tags. |

Separate targets add `unicode/Unicode.h` (the text analysis leaf, below),
`PaintShaders.h` (animated SkSL presets), `ports/SystemFontManager.h` (the
OS font manager), the companion utilities in `kit/`, and a Qt bridge.

### The Unicode leaf

`<sigilweave/unicode/Unicode.h>` (target `SigilWeaveUnicode`, namespace
`sigil::weave::unicode`) is every Unicode question the engine asks,
answered as plain values over UTF-16 text and depending on ICU alone — no
Skia, no other header of this library:

| Function | Answer |
|---|---|
| `toUtf16` / `toUtf8` / `decodeAt` | transcoding and code-point decoding |
| `isWhitespace`, `isHardLineBreak`, `inheritsTypeface`, `mayRequireBidi`, `verticalOrientation` | per-character properties: what separates words, what forces a line end, what takes its neighbour's typeface, what can turn a paragraph bidirectional, how a character stands in a vertical column (UTR#50) |
| `scriptOf`, `scriptShortName`, `isIdeographicScript`, `itemize` | scripts, and the text split into `ScriptRun`s with Common and Inherited characters attached to their neighbours |
| `caseMap` / `caseMapped` | locale-aware upper, lower and first-code-point title case |
| `lineBreaks`, `wordBoundaries`, `sentenceStarts` | UAX#14 break opportunities and UAX#29 word and sentence segmentation, as ascending offsets |
| `bidi` | UAX#9 embedding levels as `BidiRun`s against a chosen `BaseDirection` |

The engine consumes it privately: `Paragraph` runs `lineBreaks`, `itemize`
and `bidi` when it analyzes, `caseMap` just before it shapes a transformed
segment, and `sentenceStarts` on the first walk after an edit. Nothing in
the engine's public headers names one of its types, so a consumer that
wants the analysis without the fonts links the leaf alone. The scratch
objects the analyses reuse (ICU break iterators, the bidi analyzer) are
thread-local, so every function is safe from any thread.

## Targets and dependencies

| Target | Contents | Beyond Skia |
|---|---|---|
| `SigilWeaveUnicode` | the Unicode leaf: `unicode/Unicode.h` | ICU, private; no Skia |
| `SigilWeave` | the engine | SigilGeometryPath (public: `LineInterval::contour` is a `geometry::Contour`); SigilWeaveUnicode, HarfBuzz, ICU, abseil — all private |
| `SigilWeaveShaders` | `PaintShaders.h` — water, mesh gradient, sparkle, star nest, clouds, tunnel | `SkRuntimeEffect` |
| `SigilWeavePorts` | `ports::systemFontManager()` — CoreText today; DirectWrite/Fontconfig slot into the same call | Skia platform ports |
| `SigilWeaveKit` | consumer-side discipline: rebuild/layout guards, glyph bucketing, label shorthand, sample content (see `kit/README.md`) | — |
| `SigilWeaveQt` | interface target: `QFont` → `SkTypeface`, `QString` ↔ `Paragraph` with no transcoding | Qt6::Gui |

Skia and SigilGeometryPath are PUBLIC dependencies — the path a line of text
follows is a geometry contour, and `ExclusionFlow` flattens its shapes through
the same library; the Unicode leaf, HarfBuzz, ICU and abseil are PRIVATE and
appear in no public header. Pimpls hide the hash maps, and `Word::segments()`
hands out a `std::span` over storage whose container type only the engine
sees, so the one abseil container inside a value type never reaches a
consumer.
The core is Qt-free and carries no SkSL: shader presets are content, not
engine.

Install and export rules are generated, so an installed tree works with:

```cmake
find_package(SigilWeave CONFIG REQUIRED)                # core, Qt-free
find_package(SigilWeave CONFIG REQUIRED COMPONENTS Kit) # + SigilWeaveKit
find_package(SigilWeave CONFIG REQUIRED COMPONENTS Qt)  # + SigilWeaveQt
target_link_libraries(app PRIVATE
  sigil::weave::SigilWeave sigil::weave::SigilWeavePorts)
```

Everything compiles as standard C++20 with extensions disabled. Public APIs
use `std::span` views, concept-constrained callbacks, and
`[[nodiscard("reason")]]` where ignoring a return silently corrupts caller
state.

## What the engine covers

- **Decorations** — underline, strikethrough, overline, highlight, on
  `PaintStyle::decorations`. Thickness and position default to the font's own
  metrics; underlines skip ink around descenders. Span is per-decoration:
  `kDecoratedRange` merges contiguous same-style runs on a line into one band
  that covers the gaps between words (CSS behavior), `kPerWord` draws one
  band per word (squiggles, chips). `Decoration::paint` takes a full `SkPaint`
  applied verbatim, resolved independently of the glyph paint, so a shaded
  band can sit under plain ink.
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
  rotated, tate-chu-yoko). `columnMetrics()` measures the result, and a
  dressed glyph in a column sets `GlyphDress::centreOffset` because half its
  advance is a step down the page rather than across it.
  `FontContext::glyphAdvanceEm()` reports either axis's advance in ems, for a
  caller asking whether two glyphs step the pen alike — the vertical advance
  is a fact Skia's glyph metrics do not carry at all.
- **Font fallback** — per-codepoint, per-language, memoized, with an ASCII
  direct-mapped fast table. The default resolver uses the `SkFontMgr`'s
  platform cascade; supply a `FontContext::FallbackResolver` to encode your
  own family list or script policy.
- **Inline placeholders** — pills, icons, and images woven into the flow. The
  breakers treat each as an unbreakable word; `placeholderRects()` reports
  where they landed.
- **Per-glyph choreography** — `forEachPlacedGlyph()` (`Choreograph.h`) hands
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

**Hyphenation is discretionary only.** There is no dictionary and no Liang
patterns in this library. Soft hyphens (U+00AD) must already be in the text;
feed it through any hyphenator that inserts them. Both breakers then treat
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
straight horizontal runs only — transformed and vertical runs skip them. The
ellipsis marker requires the final interval to be straight, horizontal, and
not a contour. `lineMetrics()` skips transformed and vertical runs, and omits
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

## Boundaries

- **No SkShaper, no SkParagraph.** HarfBuzz and ICU are called directly.
- **Product-specific geometry lives with the consumer.** The core exposes the
  reusable pieces — `SingleLineParagraphCache` and `layoutSingleLine()` — and
  nothing above them. Measurement and curvature compensation for a particular
  application's labels belong in that application. No product symbol appears
  in this library or its tests.
- **Shape-by-word.** Words are shaped independently, so cross-word kerning and
  ligatures at word boundaries are dropped — the same trade browser engines
  make. Within a word, including CJK runs between break opportunities, shaping
  is fully contextual.
- **Whole-paragraph transforms are canvas transforms.** Rotating, scaling, or
  skewing a paragraph is `canvas->concat()` before `draw()`. Keeping it out of
  the API means layout coordinates stay in one predictable space, and
  compositing effects over finished text are `SkCanvas::saveLayer()` around
  the draw.
- **Paragraph-wide shaders need no library support.** Skia shaders are
  canvas-space, so one shader set as a span's foreground already flows
  seamlessly across every line.
- **Ruby and kenten are not core features.** They are a few lines each over
  the layout's placed runs; the CJK demo and gallery scenes show how.
- **Bidi is per-word.** Levels are computed and UAX#9 L2 visual reordering is
  applied per word; glue between reordered runs is approximated, and
  multi-segment RTL words keep logical segment order.

## Build, test, and see it

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug -R weave_ --output-on-failure
```

The tests are one binary per feature, following the library's layering so
each compiles and links only what its feature needs:

- `weave_unicode_test` — the Unicode leaf, with no fonts at all.
- `weave_shaping_test` — the shaper, the paragraph model, and typographic
  correctness: cluster coverage across scripts, ZWNJ joining control,
  combining-mark attachment (NFC and NFD must measure alike), kinsoku
  prohibitions, NBSP no-break, justification shrink limits, UAX#9 visual
  reordering, strut metrics, and edit safety at surrogate boundaries.
- `weave_layout_test` — line breaking (greedy and Knuth-Plass), flows and
  exclusions, overflow and clamp, vertical writing, and the large-paragraph
  stress cases.
- `weave_query_test` — the optional Query layer.
- `weave_choreograph_test` — per-glyph choreography and its rendering.
- `weave_kit_test` — the SigilWeaveKit convenience layer.

Fixtures live in `test/support/`: `Fonts.h` holds the one process-wide
`FontContext` every binary shapes with, and each binary has a support header
that includes exactly the headers its translation units use.

`weave_bench` owns every performance claim about this library. Build it
Release and run it rather than trusting a number written down anywhere:

```sh
cmake --build build --config Release --target weave_bench weave_demo
./build/bin/Release/weave_bench
./build/bin/Release/weave_demo   # writes weave_demo_out/*.png in the cwd
```

`weave_demo` renders headless PNG panels of the library-only surfaces:
extreme geometries, typographic options, mixed-script and feature panels,
CJK and vertical text, `SkPath` exclusions, CJK fallback, and a panel
covering decorations, text transform, word spacing, variable axes, tab stops,
and line clamp.

`WeaveGallery` (`examples/gallery/`) is the interactive home for the animated
scenes — exclusions and morphing paths, greedy versus Knuth-Plass, an
infinite path loop, letter rain, a click-to-ripple pool, vertical CJK with
ruby, kenten and tate-chu-yoko, a mixed-script wall, effects and shaders,
regex markers, inline slots, overflow and clamp, an aliased-type terminal.
Scenes self-register with declarative
parameters, so the sidebar builds their controls automatically. It renders
through a `QQuickRhiItem` — Skia Graphite on Qt's own Metal queue, with a CPU
raster fallback and a live GPU/CPU switch — and displays a
reshaped-words-per-frame counter, which sits at zero while everything moves
when the shape cache is doing its job. Judge any of it on a Release build;
Skia's Debug recording path is dramatically slower on glyph-heavy scenes.
