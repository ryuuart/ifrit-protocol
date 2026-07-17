# TextFlow

A flexible, cache-first text layout engine for Skia, inspired by Chromium's
LayoutNG (shape-by-word caching, paint/shaping style split) and Cheng Lou's
[Pretext](https://github.com/chenglou/pretext) (shape once, position freely,
lines are not rectangles). It shapes with HarfBuzz and analyzes with ICU
directly — no SkShaper, no SkParagraph — so every intermediate result (words,
glyph runs, blobs) is exposed and reusable.

- [Architecture](#architecture)
- [Library targets & consumption](#library-targets--consumption)
- [Typographic features](#typographic-features)
- [Effects: what belongs where](#effects-what-belongs-where)
- [Why it's fast](#why-its-fast)
- [Measured performance](#measured-performance)
- [API sketch](#api-sketch)
- [Behavior notes & accuracy trade-offs](#behavior-notes--accuracy-trade-offs)
- [Tuning knobs](#tuning-knobs)
- [Threading model](#threading-model-and-why-theres-no-onetbb)
- [Known limitations](#known-limitations)

## Architecture

```
Paragraph (UTF-16 + style spans, horizontal or vertical-RL writing mode)
  │  ICU: line-break opportunities (UAX#14), bidi levels (skipped when
  │       LTR-only), script itemization, per-codepoint font fallback,
  │       locale-aware text-transform case mapping, UTR#50 vertical
  │       orientation (vertical mode)
  ▼
Word list          — break-delimited segments; content + trailing glue
  │  HarfBuzz via the ShapeCache: content-addressed by
  │  (typeface, fontSize, letterSpacing, script, direction, vertical,
  │   languageTag, fontFeatures, text). Variable-font axes and text
  │   transforms enter *indirectly* — via the memoized varied typeface's
  │   uniqueID and via the transformed key text — so the key itself never
  │   grows.
  ▼
ShapedWord (shared, immutable) — glyphs, advances, clusters,
  │                              + lazily built origin-relative SkTextBlob
  ▼
FlowGeometry       — per line: ordered intervals (rect bands, bands minus
  │                  exclusion shapes — circles, rects, or any SkPath —
  │                  arbitrary segments, SkPath contours, vertical columns);
  │                  OverflowOptions::maxLines clamps any geometry
  ▼
LineBreakStrategy  — greedy or Knuth-Plass, both feeding one placement stage
  │                  (tab stops resolve in the greedy pass)
  ▼
ParagraphLayout    — PositionedRuns: shared word blob + origin (horizontal or
                     vertical columns), or RSXform-baked blob (rotated /
                     on-path); draw() resolves paint per span at draw time
                     and emits line-spanning decoration bands
```

Optional layers on top (nothing in the core pipeline depends on them):

- `Query.h` — find ranges by substring / ICU regex / word, and `MarkerSet`:
  named ranges that follow edits via the Paragraph's bounded revision log
  (DOM-Range style).
- `SingleLineParagraphCache.h` + `layoutSingleLine()` — reusable support for
  high-frequency labels and captions, shaped once per unique text/font/size.
  SpellCircle-specific ring measurement and curvature compensation live with
  that renderer in `src/qml/SpellCircle/SceneLabels.*`, not in this library.
- `Choreograph.h` — per-glyph animation: `forEachPlacedGlyph` (walk a
  layout's glyphs with rest positions; stable order across relayouts) and
  `GlyphRSXformBatches` (thousands of animated letters → a handful of
  drawGlyphsRSXform calls). Used by the gallery's rain/ripple scenes.
- `Features.h` — named OpenType presets (`Features::tabularNumbers`,
  `smallCaps`, `stylisticSet(n)`, …) so styles don't hand-spell four-cc
  tags.

## Library targets & consumption

The build splits along dependency lines so consumers link only what they
use:

| Target | Contents | Extra deps |
|---|---|---|
| `TextFlow` | the engine (everything above) | Skia (public); HarfBuzz, ICU, abseil (private) |
| `TextFlowShaders` | `PaintShaders.h` — animated SkSL presets (water, mesh gradient, sparkle, star nest, clouds, tunnel) | Skia + SkRuntimeEffect |
| `TextFlowPorts` | `ports/SystemFontManager.h` — the OS font manager (CoreText today; DirectWrite/Fontconfig slot in here) | Skia platform ports |
| `TextFlowQt` | header-only Qt bridging (QFont → SkTypeface, QString ↔ Paragraph zero-copy) | Qt6::Gui |

The core is deliberately Qt-free and contains no SkSL/runtime-effect code —
presets are content, not engine. No abseil type appears in any public
header (`Word::segments` uses the library's own `InlineVector`; internal
caches hide behind pimpls), so consumers never inherit that dependency
surface.

Install rules are in place for standalone consumption (the eventual private
vcpkg registry port drives exactly these):

```cmake
find_package(TextFlow CONFIG REQUIRED)                # core, Qt-free
find_package(TextFlow CONFIG REQUIRED COMPONENTS Qt)  # + textflow::TextFlowQt
target_link_libraries(app PRIVATE textflow::TextFlow textflow::TextFlowPorts)
```

**C++ baseline:** everything compiles as standard C++20 with extensions
disabled. Public APIs use non-owning `std::span` views, concept-constrained
callbacks, and diagnostic `[[nodiscard("reason")]]` annotations. C++26
contracts are deliberately not enabled; until they're a portable baseline,
contracts are expressed with strong types, scoped enums, constrained
templates, and documented preconditions.

## Typographic features

The Chrome-parity surface, and where each feature lives in the pipeline:

- **Text decorations** — underline / strikethrough / overline / highlight
  on `PaintStyle::decorations`. Thickness and position default to the
  font's own metrics (1px floor when a face reports none); color defaults
  to the foreground; underlines skip ink around descenders via blob
  intercepts. **Span is a per-decoration choice**: the default
  `Span::kDecoratedRange` merges contiguous same-style runs on a line into
  one continuous band covering the glue between words (CSS-style — an
  underlined sentence is one line; skip-ink breaks come only from glyph
  ink), while `Span::kPerWord` draws one band per word (spell-check
  squiggles, word chips). `Kind::kHighlight` is the background member: a
  full-text-height band drawn beneath every glyph pass — with range
  spanning it reads as one highlighter stroke across words *and* gaps
  (default color: the foreground at ~25% alpha, so text stays legible).
  All of it is paint-side: adding or recoloring never reshapes or
  relayouts.
- **Text transform** — `ShapingStyle::textTransform` (uppercase, lowercase,
  capitalize) applies locale-aware ICU case mapping just before shaping
  (Turkish dotless-i via `languageTag`, full ß→SS). The stored text, edit
  ranges, and queries stay untransformed; the transformed text is itself
  the shape-cache key, so `"HELLO"` typed and `"hello"`+uppercase share one
  entry.
- **Spacing** — `letterSpacing` (tracking, part of the shape key; JIS aki
  in vertical text) and `wordSpacing` (added to inter-word glue after
  measurement; restyles at pure cache-hit cost, never below zero).
- **Variable fonts** — `ShapingStyle::variations = {{"wght", 700}}`. The
  FontContext memoizes the varied `SkTypeface` clone per (base, axis list),
  so identical positions share one instance — and therefore one shape-cache
  identity — while HarfBuzz mirrors the same design position Skia
  rasterizes. No hand-built `SkFontArguments` clones needed.
- **OpenType features** — per-span `ShapingStyle::fontFeatures`, part of the
  shape-cache key (liga/dlig/smcp/lnum/frac variants of the same text
  coexist in the cache), with `Features::` presets for the common ones.
- **Line clamp** — `OverflowOptions::maxLines` (CSS line-clamp) wraps any
  `FlowGeometry`, so it works identically under greedy, Knuth-Plass, and
  exclusion flows; combine with `overflow.ellipsis` for a shaped marker on
  the clamped line.
- **Tab stops** — `ParagraphLayoutOptions::tabStops` (explicit positions +
  repeating interval). Tab glue advances to the next stop and is rigid under
  justification. Unconfigured, tabs measure as spaces.
- **Overflow ellipsis** — `overflow.ellipsis` trims the final placed line
  until a shaped marker fits (CSS text-overflow, but the marker is a real
  glyph run in the tail's style).
- **Inline placeholders** — pills/icons/images woven into the flow; the
  breaker treats each slot as an unbreakable word and the layout reports
  its rect.
- **Vertical CJK** — `WritingMode::kVerticalRL` with per-character UTR#50
  orientation, 'vert' forms, and per-span `VerticalForm` overrides
  (upright / rotated / tate-chu-yoko).
- **Line metrics** — `ParagraphLayout::lineMetrics()` reports per-line
  baseline, ascent/descent band, advance extent, and character range,
  derived on demand from the placed runs (see the effects section for the
  design rationale).
- **Font fallback** — per-codepoint, per-language, memoized. The default
  resolver uses the supplied `SkFontMgr`'s platform cascade; pass a
  `FontContext::FallbackResolver` to encode an application-owned family
  list or script policy. (The `languageTag` view handed to resolvers is
  not guaranteed NUL-terminated — copy before calling C APIs.)

## Effects: what belongs where

A deliberate split, worth knowing before reaching for a feature request:

- **Span-level appearance is the library's job.** `PaintStyle` carries the
  foreground, ordered underlay/overlay passes, and decorations per style
  span; the layout resolves it at draw time. Anything that needs *run or
  line geometry* — continuous decoration bands, skip-ink, per-span shader
  fills — must live here, because only the layout knows where lines and
  words landed.
- **Paragraph-wide surface effects need no library support.** Skia shaders
  are canvas-space: a single `PaintShaders::meshGradient(bounds, t)` set as
  one span's foreground already flows seamlessly across every line of a
  2,000-word paragraph (the gallery's stress wall is exactly this). For
  compositing effects over the finished text — blur it, fade it, mask it —
  use `SkCanvas::saveLayer` around `draw()`.
- **Paragraph-level geometric transforms are user-level on purpose.**
  Rotating, scaling, or skewing a whole paragraph is
  `canvas->concat(matrix)` before `draw()`/`drawBatched()`; it composes
  with every effect, costs nothing in the engine, and keeping it out of the
  API means the layout's coordinates stay in one predictable space (shatai
  obliquing in vertical text is the documented example: a canvas skew).
- **Per-glyph transforms are a walk, not a mode.** Choreograph exposes each
  glyph's rest position; rain/ripple/marquee effects rebuild RSXform
  batches per frame while the layout itself stays untouched.

- **Line geometry is a query, not an effect system.**
  `ParagraphLayout::lineMetrics(paragraph)` derives per-line geometry from
  the placed runs on demand — baseline, tallest ascent / deepest descent
  (mixed fonts grow the band, placeholders too), advance extent, and the
  line's character range. Selection bands, line backgrounds, and
  point-to-line hit-testing are `lineMetrics()[i].rect()` plus ordinary
  canvas drawing; nothing is stored during layout and callers who never
  ask pay nothing.

## Why it's fast

- **The shape cache is the incremental engine.** Editing one word of a
  paragraph re-runs analysis, but every unchanged word (and the glue
  between words) resolves to a cache hit — only the edited word reaches
  HarfBuzz. Keys are content-addressed, so repeated words across the whole
  session share one entry.
- **Blobs are shaped once and repositioned forever.** Each ShapedWord
  lazily builds one origin-relative SkTextBlob shared by every layout and
  frame (the Pretext trick). A moving exclusion shape re-runs placement
  arithmetic only: zero shaping, zero blob building.
- **Paint is not shaping.** `TextStyle` splits into `ShapingStyle` (cache
  key) and `PaintStyle` (resolved at draw time), so recoloring, decorating,
  or re-shadering words never reshapes and doesn't even require a relayout
  if span boundaries are unchanged.
- **Language is shaping, not bidi.** ICU derives bidi levels from Unicode
  properties separately. The BCP-47 `languageTag` stays in the shape key
  because it can select a different fallback face and because HarfBuzz uses
  it for OpenType language systems and localized (`locl`) substitutions.
- **Shaping is lazy and geometry-bounded.** Analysis splits into whole-text
  itemization (cheap) and per-word HarfBuzz shaping that the breakers pull
  just ahead of their frontier — words past the geometry are never shaped
  at all. A 30,000-word paragraph in a 15-line box shapes ~600 words.
- Zero-allocation cache probes (heterogeneous lookup), an ASCII
  direct-mapped fallback table, memoized script/language tables, glue
  memoization, and inline word-segment storage keep warm re-analysis lean.
  For long-lived processes whose typeface population churns,
  `FontContext::purgeAllCaches()` resets everything (the per-typeface maps
  are otherwise never pruned).

## Measured performance

M-series Mac, Release, google benchmark / demo:

| Scenario | Time |
|---|---|
| Warm relayout, 500 mixed Latin/CJK words | ~16 µs |
| Warm relayout, 2000 words | ~69 µs |
| Moving exclusion shapes through 300 words, per frame | ~10 µs |
| Same sweep with SkPath exclusions (star + holed donut) | ~17 µs |
| One-word edit in 500-word paragraph (full pipeline) | ~110 µs |
| Paint-only or size restyle of a word, 500 words | ~112 µs |
| Knuth-Plass justify, 500 words, warm | ~120 µs |
| Cold shape of 100 mixed words (empty cache) | ~50 µs |
| Warm relayout, 500 words across 3 typefaces + CJK fallback | ~18 µs |
| Replace the *entire* paragraph text (warm variants) | ~130 µs |
| Replace the entire paragraph with never-seen text (true cold) | ~530 µs |
| Batched CPU-raster draw, 300 mixed words, foreground only | ~206 µs |
| Same draw with shadow + glow + outline + foreground (two blurs) | ~1.18 ms |
| Fully placed 2000-word CPU-raster draw, foreground only | ~0.84 ms |
| Same 2000 words, mesh shader + tiled stars + glow + outline | ~28 ms |
| Cross-line span restyle (spans merge, steady state) | ~118 µs |
| Knuth-Plass with soft hyphens, 300 words @180px | ~32 µs |
| Live in SpellCircle: reflow around 12 streamed shapes @4K | ~170 µs/frame |
| Letter rain: 1000-word paragraph, breathing measure, 4657 letters | 41 µs relayout + 180 µs letters/frame |
| Pool ripple: 1000-word paragraph, rings displacing every letter | 37 µs relayout + 228 µs wave/frame |
| Infinite-loop path marquee (relayout around a closed contour) | ~15 µs/frame |
| Babel confetti: 2000 tokens across ~12 scripts, rotated intervals | ~540 µs/frame |

Run them yourself:

```
cmake --build build --config Release --target textflow_bench textflow_demo
./build/bin/Release/textflow_bench
./build/bin/Release/textflow_demo   # headless PNG panels (see below)
ctest --test-dir build -C Debug -R textflow_test
```

**`textflow_demo`** writes headless PNG panels for the library-only
surfaces: extreme geometries, typographic options, babel/features panels,
CJK/vertical, SkPath exclusions, CJK fallback, and a `new_features.png`
panel covering decorations, text-transform, word spacing, variable axes,
tab stops, and line clamp (the visual-regression image for those features).

**`TextFlowGallery`** (`src/gallery/`) is the interactive home for the
animated scenes: exclusions & morphing SkPath shapes,
greedy-vs-Knuth-Plass, infinite loop, letter rain, click-to-ripple pool,
vertical CJK with ruby/kenten/tate-chu-yoko, a Unicode-singularity wall,
the merged **Effects & shaders** scene (layer showcase / loud SkSL shaders
/ a fully placed 2,000-word stress wall with live pass toggles), regex
markers with live decoration toggles, inline slots, and overflow/clamp.
Scenes self-register with declarative parameters, so the sidebar builds
their controls automatically (or loads a scene-owned QML file). It renders
through a QQuickRhiItem — Skia Graphite on Qt's own Metal queue, with a CPU
raster fallback and a live GPU/CPU A/B switch — and reports fps, layout µs,
and a **reshaped-words-per-frame** counter (watch it sit at 0 while
everything moves: that's the shape cache doing its job). Judge performance
on the Release build; Skia's Debug recording path alone is ~20× slower on
glyph-heavy scenes.

The test suite (123 cases across per-module TUs) includes a Blink-inspired
typographic-correctness section: complete cluster coverage across scripts,
ZWNJ joining control, combining-mark attachment (NFC ≡ NFD widths), kinsoku
prohibitions, NBSP no-break, justification shrink limits, UAX#9 visual
reordering, strut metrics, and edit safety at surrogate boundaries.

## API sketch

UTF-8 entry points use `std::u8string_view`; use `u8` literals or
`std::u8string` so the encoding contract is carried by the type.

```cpp
// One context per thread. TextFlowPorts supplies the OS font manager.
FontContext fontContext(textflow::ports::systemFontManager());

ParagraphBuilder builder(baseStyle);
builder.addText(u8"Glyphs flow ")
    .pushStyle(red)
    .addText(u8"around")
    .popStyle()
    .addText(u8" obstacles… 日本語も 한국어도 中文也");
Paragraph paragraph = builder.build();

ExclusionFlow flow(SkRect::MakeWH(900, 700));
flow.shapes().push_back(ExclusionFlow::Shape::fromCircle(circleBounds, 8));
flow.shapes().push_back(ExclusionFlow::Shape::fromPath(anySkPath, 8));
// kPath honors the fill rule: even-odd holes and concave gaps stay open to
// text. Animate with shape.pathOffset (free — the flattening is cached by
// the path's generation ID); rebuilding the SkPath re-flattens.

ParagraphLayoutOptions options;
options.alignment = TextAlignment::kJustify;
options.lineBreakStrategy = LineBreakStrategy::kKnuthPlass;
options.overflow.maxLines = 4;          // CSS line-clamp, any geometry
options.overflow.ellipsis = u"…";
options.tabStops.positions = {120, 320}; // greedy breaker, LTR lines

ParagraphLayout layout =
    layoutParagraph(fontContext, paragraph, flow, options);
layout.draw(canvas, paragraph);        // or walk layout.runs yourself
layout.drawBatched(canvas, paragraph); // few drawGlyphs calls instead of
                                       // one drawTextBlob per word

paragraph.replaceText(4, 9, u8"swift");
paragraph.setPaint(0, 6, {SK_ColorRED}); // skips ICU re-analysis too

// Typography knobs on the shaping side (these re-shape the covered words):
TextStyle style = baseStyle;
style.shaping.variations = {{"wght", 650}, {"wdth", 85}}; // variable axes
style.shaping.textTransform = TextTransform::kUppercase;  // locale-aware
style.shaping.wordSpacing = 4.0f;                         // extra glue px
style.shaping.fontFeatures = {Features::tabularNumbers};

// Draw-time appearance (never reshapes, never relayouts):
PaintStyle effects(SK_ColorWHITE);
effects
    .addUnderlay(PaintLayer::dropShadow(0x77000000, {4, 5}, 3.0f))
    .addUnderlay(PaintLayer::glow(0x6688AAFF, 6.0f))
    .addUnderlay(PaintLayer::outline(SK_ColorBLACK, 4.0f))
    .addDecoration({})   // metric underline, skip-ink, spans word gaps
    .addDecoration({.kind = Decoration::Kind::kStrikethrough,
                    .color = SK_ColorRED})
    .addDecoration({.kind = Decoration::Kind::kHighlight,   // marker stroke
                    .color = 0x66FFD54A})                   // behind the text
    .addDecoration({.span = Decoration::Span::kPerWord});   // squiggle-style
                                                            // per-word bands
// Shaders are canvas-space: one shader spans the whole paragraph.
// (PaintShaders presets live in the separate TextFlowShaders library.)
effects.foreground.setShader(
    PaintShaders::meshGradient(canvasBounds, elapsedSeconds));
paragraph.setPaint(0, 6, effects); // existing ParagraphLayout sees it

// Whole-paragraph transforms are canvas transforms — user level:
canvas->save();
canvas->rotate(-4.0f, origin.x(), origin.y());
layout.draw(canvas, paragraph);
canvas->restore();

// Query layer (optional): HTML/JS-flavoured selection + styling.
MarkerSet markers(paragraph);
markers.setRanges("keywords", findAllOccurrences(paragraph, u8"glyph"));
paragraph.replaceText(0, 0, u8"New intro. ");
markers.applyPaint(paragraph, "keywords", {accent});

// Large documents: scope queries to what the layout actually placed —
// search cost follows the geometry, not the text.
uint32_t placedTextEnd =
    layout.overflowed()
        ? paragraph.words()[layout.firstUnplacedWord].textBegin
        : uint32_t(paragraph.text().size());
findRegexMatches(paragraph, u8"\\p{Lu}\\w+", {0, placedTextEnd});

// Vertical CJK (書字方向): columns top-to-bottom, right-to-left.
paragraph.setWritingMode(WritingMode::kVerticalRL);
VerticalBlockFlow columns(SkRect::MakeWH(600, 800));

// Inline placeholders: pills/icons/images woven into the flow.
paragraph.appendPlaceholder({90, 22, /*baselineDrop=*/5}, style);
for (const auto &placeholder : layout.placeholderRects(paragraph))
  drawMyPill(canvas, placeholder.rect, placeholder.index);

// Line metrics (derived on demand): selection bands, line backgrounds,
// point-to-line hit tests.
for (const LineMetrics &line : layout.lineMetrics(paragraph))
  if (selection.intersects(line.textBegin, line.textEnd))
    canvas->drawRect(line.rect(), selectionPaint);  // then layout.draw(...)
```

Geometries: `BlockFlow` (rect), `ExclusionFlow` (rect minus moving shapes),
`VerticalBlockFlow` (top-to-bottom columns advancing right to left),
`LineSetFlow` (explicit intervals — any origin/direction/count per line),
`PathFlow` (each SkPath contour is a line; glyphs follow the tangent via
RSXform runs). Closed contours wrap arc positions, so animating
`LineInterval::contourStart` produces an infinite marquee;
`LineInterval::advanceScale` compensates curvature when glyph centers ride
a different radius than the measured contour. Implement `FlowGeometry` for
anything else.

## Behavior notes & accuracy trade-offs

- **Shape-by-word (default, fast):** words are shaped independently, so
  cross-word kerning/ligatures at word boundaries are dropped — the same
  trade Chrome makes. Within a word (including CJK runs between break
  opportunities) shaping is fully contextual.
- Justification stretches spaces (shrink is clamped at the glue's shrink
  limit, TeX-style); for spaceless CJK it optionally expands inter-cluster
  gaps. InDesign-style endings via `justification.lastLineAlignment` /
  `justifyLastLine`. Tab gaps are rigid under justification.
- Soft hyphens (U+00AD) are discretionary breaks in both breakers:
  invisible unless a line actually breaks there, in which case a styled
  hyphen is rendered; Knuth-Plass charges `hyphenation.penalty` per
  hyphenated line. Feed text through any hyphenator that inserts U+00AD.
- Lines never render past the measure: shrink is only counted where
  placement will shrink, final lines get TeX's `\parfillskip`, and when no
  break fits the tolerance the pass reruns with `\emergencystretch` rather
  than force an overfull line.
- Knuth-Plass is linear in *placed* words on uniform-width flows
  (`FlowGeometry::uniformIntervals`): paths reaching the same breakpoint
  merge, TeX's one-measure model, so a fully-placed 10k-word paragraph
  relayouts in ~2 ms. Filter exclusion slivers with
  `knuthPlass.minimumIntervalWidth`.
- Layout cost is bounded by the *geometry*, not the text: both strategies
  stop dead when the flow runs out of intervals
  (`ParagraphLayout::overflowed()` / `firstUnplacedWord`), and shaping is
  lazy behind the breaker frontier.
- Vertical mode resolves each character per UTR#50 (CJK upright with 'vert'
  forms, Latin rotated — CSS `text-orientation: mixed`), overridable per
  span. Kinsoku prohibitions come from ICU's UAX#14 rules. Ruby (furigana)
  and kenten are deliberately *not* core features — they're a dozen lines
  each over the layout's placed runs (see the CJK demo and gallery scenes).
- Script coverage is fallback-driven and tested: Arabic joining and
  lam-alef ligation, Devanagari conjuncts, supplementary-plane Cuneiform,
  emoji ZWJ/modifier/flag sequences as single grapheme clusters.
- Rendering hygiene for animation: subpixel positioning is size-gated
  (<48px) and on-path rotations quantize to 512 steps, so Skia's glyph
  atlas keeps hitting while text moves.
- For per-glyph choreography walk the layout with `forEachPlacedGlyph` and
  feed `SkCanvas::drawGlyphsRSXform`; on the CPU raster backend quantize
  rotation angles so Skia's glyph-mask cache can hit.

## Tuning knobs

Everything that trades fidelity against speed, in one place:

| Knob | Where | Default | Effect |
|---|---|---|---|
| `lineBreakStrategy` | ParagraphLayoutOptions | greedy | greedy or optimal Knuth-Plass breaking |
| `hyphenation.enabled` / `.penalty` | ParagraphLayoutOptions | on / 50 | discretionary soft-hyphen breaks |
| `justification.*` | ParagraphLayoutOptions | see header | space elasticity, CJK gap expansion, last-line alignment |
| `knuthPlass.tolerance` / `.minimumIntervalWidth` | ParagraphLayoutOptions | 4000 / 0 | badness limit; sliver filtering |
| `overflow.ellipsis` / `.maxLines` | ParagraphLayoutOptions | off | shaped overflow marker; line clamp over any geometry |
| `tabStops.positions` / `.interval` | ParagraphLayoutOptions | off | tab-stop columns (greedy, LTR); unset → tabs measure as spaces |
| `pathText.tangentRotationSteps` | ParagraphLayoutOptions | 512 | quantizes animated path tangents; 0 = exact |
| `advanceScale` | LineInterval | 1 | curvature compensation for offset baselines on contours |
| `letterSpacing` / `wordSpacing` | ShapingStyle | 0 / 0 | tracking (shape key) / inter-word glue (restyle-only) |
| `variations` | ShapingStyle | none | variable-font axes via the memoized clone cache |
| `textTransform` | ShapingStyle | none | pre-shaping locale-aware case mapping |
| `verticalForm` | ShapingStyle | auto | per-span upright / rotated / tate-chu-yoko in vertical mode |
| paint layers / decorations | PaintStyle | none | ordered extra glyph passes and decoration bands (range or per-word span; highlights beneath the glyphs); each layer adds one draw |
| shader program / layer count | PaintStyle + TextFlowShaders | solid foreground | runtime shader pixel cost and pass count are independent; prefer `drawBatched` |
| subpixel gate | Shaper.cpp (`makeFont`) | <48px | subpixel positioning only where visible |
| shape-cache cap | FontContextImpl.h | ~130k entries | wholesale clear past the cap (one cold frame), no LRU |
| `purgeAllCaches()` | FontContext | manual | drops every cache incl. per-typeface HarfBuzz records |
| `Paragraph` edit history | Paragraph.cpp | 256 ops | how far behind a MarkerSet may fall before re-querying |
| `draw` vs `drawBatched` | ParagraphLayout | — | per-word blobs vs one drawGlyphs per (font, paint) bucket |

## Threading model (and why there's no oneTBB)

A `FontContext` — shape cache, HarfBuzz buffer, ICU iterators, UBiDi — is
**single-threaded by contract**; every scratch object is reused rather than
locked. Parallelism happens *above* the library: one FontContext per
thread, one paragraph per task, embarrassingly parallel with zero shared
state (the app runs its context on the Qt render thread; the tests/bench/
demo each own theirs).

Intra-paragraph parallelism (e.g. oneTBB over words) was considered and
rejected: the only path over ~100 µs is *cold* shaping of never-seen text,
paid once per novel word per session; the warm paths this library is built
around are 10–150 µs, well below the cost of waking a thread pool. A
parallel shape pass would also force locks or sharding onto the shape
cache — the single hottest structure — to speed up the coldest path. If a
workload ever needs faster cold starts, the right shape is N contexts
shaping N paragraphs, then merging layouts — no library changes needed.

## Known limitations

- **Decorations:** straight horizontal runs only — transformed (on-path /
  rotated) and vertical runs skip them. Range-spanning bands merge across
  word gaps within a same-style, same-metrics group; a bidi reorder or
  fallback-font switch mid-range starts a new band (per-word spans are
  unaffected). Skip-ink intercepts are computed per draw (uncached).
  Highlights are per-decorated-range bands; for full-line/selection
  geometry use `ParagraphLayout::lineMetrics()`, which reports the advance
  extent of what actually placed (lines whose geometry placed nothing do
  not appear, and straight horizontal lines only — query the FlowGeometry
  itself for raw interval geometry).
- **Tab stops:** greedy breaker, LTR, straight horizontal lines;
  Knuth-Plass treats tabs as ordinary glue. Alignments other than kStart
  may shift a tabbed line as a whole.
- **Text transform:** length-changing mappings (ß→SS) make per-character
  hit-testing inside that word approximate; break analysis runs on the
  untransformed text (matching browser engines).
- Bidi: levels are computed and per-line visual reordering (UAX#9 L2) is
  applied per word, but glue between reordered runs is approximated and
  multi-segment RTL words keep logical segment order. Latin+CJK (the
  priority) is unaffected.
- Restyling a range that cuts *inside* a word splits that word into
  separately shaped fragments (one-time reshape of the boundary words);
  word-aligned ranges cost zero reshapes. Adjacent identical spans
  re-merge automatically.
- No automatic hyphenation dictionary — soft hyphens must be present in
  the text (preprocess with a Liang-pattern hyphenator if needed).
- Placeholders: slots match records by U+FFFC occurrence order; a line
  never grows to fit a slot taller than the leading; straight intervals
  only.
- Vertical mode: rotated (Latin) runs put their baseline on the column
  axis rather than optically centering; RTL scripts inside vertical
  columns keep logical order; one strut line height per layout.
- Re-analysis on edit is full-paragraph (all cache hits except the edit):
  O(n) at ~0.2 µs/word. The next optimization, if ever needed, is windowed
  re-analysis around the edit.
- The shape cache evicts by clearing wholesale past ~130k entries (one
  cold frame), not LRU; the per-typeface/fallback maps are unbounded but
  naturally small — `purgeAllCaches()` resets them manually.
