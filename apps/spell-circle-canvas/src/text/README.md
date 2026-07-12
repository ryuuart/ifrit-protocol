# TextFlow

A flexible, cache-first text layout engine for Skia, inspired by Chromium's
LayoutNG (shape-by-word caching, paint/shaping style split) and Cheng Lou's
[Pretext](https://github.com/chenglou/pretext) (shape once, position freely,
lines are not rectangles). It shapes with HarfBuzz and analyzes with ICU
directly — no SkShaper, no SkParagraph — so every intermediate result (words,
glyph runs, blobs) is exposed and reusable.

```
Paragraph (UTF-16 + style spans, horizontal or vertical-RL writing mode)
  │  ICU: line-break opportunities (UAX#14), bidi levels (skipped when
  │       LTR-only), script itemization, per-codepoint font fallback,
  │       UTR#50 vertical orientation (vertical mode)
  ▼
Word list          — break-delimited segments; content + trailing glue
  │  HarfBuzz via the ShapeCache: content-addressed by
  │  (typeface, size, letter-spacing, script, dir, vertical, lang,
  │   features, text)
  ▼
ShapedWord (shared, immutable) — glyphs, advances, clusters,
  │                              + lazily built origin-relative SkTextBlob
  ▼
FlowGeometry       — per line: ordered intervals (rect bands, bands minus
  │                  exclusion shapes — circles, rects, or any SkPath —
  │                  arbitrary segments, SkPath contours, vertical columns)
  ▼
Breaker            — greedy or Knuth-Plass, both feeding one placement stage
  ▼
Layout             — PlacedRuns: shared word blob + origin (horizontal or
                     vertical columns), or RSXform-baked blob (rotated /
                     on-path); draw() resolves paint per span at draw time
```

On top (all optional, none of it in the core pipeline):

- `Query.h` — find ranges by substring / ICU regex / word, and `MarkerSet`:
  named ranges that follow edits via the Paragraph's bounded revision log
  (DOM-Range style).
- `Labels.h` — the boilerplate a renderer needs for lots of short cached
  labels per frame: `LabelCache` (shaped once per unique text/font/size),
  `RingCache` + `makeRingLabel` (curvature-compensated circular labels),
  `layoutLabel` (single-line placement). Distilled from the SpellCircle
  scene backend, which now consists of little else.
- `Choreograph.h` — per-glyph animation: `forEachPlacedGlyph` (walk a
  layout's glyphs with rest positions; stable order across relayouts) and
  `GlyphRSXformBatches` (thousands of animated letters → a handful of
  drawGlyphsRSXform calls). Used by the rain/ripple demos and the gallery.
- `textflowqt/TextFlowQt.h` — Qt bridging: QFont → SkTypeface,
  QString ↔ Paragraph/Query **zero-copy** (both are UTF-16), QColor/QRectF/
  QPointF converters. The core library stays Qt-free.

## Why it's fast

- **The shape cache is the incremental engine.** Editing one word of a
  paragraph re-runs analysis, but every unchanged word (and the glue between
  words) resolves to a cache hit — only the edited word reaches HarfBuzz.
  This is Chrome's ShapeCache idea taken further: keys are content-addressed,
  so repeated words across the whole session share one entry.
- **Blobs are shaped once and repositioned forever.** Each ShapedWord lazily
  builds one origin-relative SkTextBlob shared by every layout and frame
  (the Pretext trick). A moving exclusion shape re-runs placement arithmetic
  only: zero shaping, zero blob building.
- **Paint is not shaping.** `TextStyle` splits into `ShapingStyle` (cache
  key) and `PaintStyle` (resolved at draw time), so recoloring words never
  reshapes and doesn't even require a relayout if span boundaries are
  unchanged.
- Zero-allocation cache probes (heterogeneous absl lookup), an ASCII
  direct-mapped fallback table, a memoized script-tag table, glue
  memoization, and inline word-segment storage keep warm re-analysis lean.

## Measured (M-series Mac, Release, google benchmark / demo)

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
./build/bin/Release/textflow_demo        # writes PNGs + per-frame timings
ctest --test-dir build -C Debug -R textflow_test   # or run the binary
```

There is also an interactive gallery (`TextFlowGallery`, in `src/gallery/`):
a standalone Qt Quick app with a scene switcher (exclusions & SkPath
shapes — including a spiky ring that is a brand-new path every frame,
its points and even-odd hole morphing live — greedy-vs-Knuth-Plass,
infinite loop, letter rain, click-to-ripple pool, vertical CJK with
ruby/kenten/tate-chu-yoko, regex markers, inline slots & pills), live text
editing, and font/size/alignment/breaker controls plus a variable-font
Weight slider (`wght` 100–900, "auto" = the font's own position). It renders through a QQuickRhiItem: **Skia Graphite on Qt's own
Metal queue** — the same GPU path as the SpellCircle app — with a CPU
raster + texture-upload fallback, and a **GPU switch** in the panel that
flips between the two live for A/B comparison. The status line reports the
backend, fps, layout µs, record/submit ms, run count, and a
**reshaped-words-per-frame** counter (watch it sit at 0 while everything
moves — that's the shape cache doing its job).
`./build/bin/Release/TextFlowGallery [--scene N]`. Demos default to the
installed Noto Sans / Noto Serif variable fonts. Judge performance on the
Release build: Skia's Debug recording path alone is ~20× slower on the
glyph-heavy rain/ripple scenes (94 ms vs 3.7 ms record for ~2,400
letters).

The test binary includes a Blink-inspired typographic-correctness suite
(`Correctness.*`): complete cluster coverage across scripts, ZWNJ joining
control, combining-mark attachment (NFC ≡ NFD widths), kinsoku
prohibitions, NBSP no-break, tab measurement, justification shrink limits,
UAX#9 visual reordering, strut metrics, and edit safety at surrogate
boundaries.

## API sketch

```cpp
FontContext ctx(SkFontMgr_New_CoreText(nullptr));   // one per thread

ParagraphBuilder b(baseStyle);
b.addText("Glyphs flow ").pushStyle(red).addText("around").pop()
 .addText(" obstacles… 日本語も 한국어도 中文也");
Paragraph para = b.build();

ExclusionFlow flow(SkRect::MakeWH(900, 700));
flow.shapes().push_back(ExclusionFlow::Shape::Circle(circleBounds, 8));
flow.shapes().push_back(ExclusionFlow::Shape::Path(anySkPath, 8));
// kPath honors the fill rule: even-odd holes and concave gaps stay open to
// text. Animate with shape.pathOffset (free — the flattening is cached by
// the path's generation ID); rebuilding the SkPath re-flattens.

LayoutOptions opts;                    // greedy + justify, or:
opts.breaker = Breaker::kKnuthPlass;   // optimal paragraph breaking
opts.alignment = Alignment::kJustify;

Layout layout = layoutParagraph(ctx, para, flow, opts);
layout.draw(canvas, para);             // or walk layout.runs yourself
layout.drawBatched(canvas, para);      // few drawGlyphs calls instead of
                                       // one drawTextBlob per word — use on
                                       // GPU canvases with many runs

para.replaceText(4, 9, "swift");       // edit: next layout re-shapes 1 word
para.setPaint(0, 6, {SK_ColorRED});    // restyle: re-shapes 0 words

// Query layer (optional): HTML/JS-flavoured selection + styling.
for (CharRange r : findRegex(para, "\\p{Lu}\\w+").value_or({}))
  para.setPaint(r.start, r.end, {accent});
MarkerSet marks(para);
marks.set("keywords", findAll(para, "glyph"));
para.replaceText(0, 0, "New intro. ");  // markers shift with the edit
marks.applyPaint(para, "keywords", {accent}); // syncs, then restyles

// Vertical CJK (書字方向): columns top-to-bottom, right-to-left.
para.setWritingMode(WritingMode::kVerticalRL);
VerticalBlockFlow columns(SkRect::MakeWH(600, 800));
// Per-span overrides: VerticalForm::kUpright / kRotated / kTateChuYoko
// (縦中横 — digits set horizontally across the column).

// Inline placeholders: pills/icons/images woven into the flow. The breaker
// treats each slot as an unbreakable word; the layout reports its rect.
para.appendPlaceholder({90, 22, /*baselineDrop=*/5}, style);
for (const auto &placed : layout.placeholderRects(para))
  drawMyPill(canvas, placed.rect, placed.index);
para.setPlaceholder(0, {120, 22, 5}); // live resize: relayout, 0 reshapes
```

Geometries: `BlockFlow` (rect), `ExclusionFlow` (rect minus moving shapes —
circles, rects, or arbitrary/compound SkPaths with their fill rule
honored), `VerticalBlockFlow` (top-to-bottom columns advancing right to
left), `LineSetFlow` (explicit intervals — any origin/direction/count per
line), `PathFlow` (each SkPath contour is a line; glyphs follow the tangent
via RSXform runs). Closed contours wrap arc positions, so animating
`LineInterval::contourStart` produces an infinite marquee; and
`LineInterval::advanceScale` compensates curvature when glyph centers ride
a different radius than the measured baseline contour (small circular
labels read too loose without it). Implement `FlowGeometry` for anything
else.

Vertical mode resolves each character's orientation per UTR#50 (CJK
upright with the font's 'vert' forms, Latin rotated 90° clockwise — CSS
`text-orientation: mixed`), overridable per span via
`ShapingStyle::verticalForm`. Kinsoku line-break prohibitions come from
ICU's UAX#14 rules; JIS aki is `letterSpacing`; shatai is a canvas skew.
Ruby (furigana) and kenten (emphasis dots) are deliberately *not* core
features — they're a dozen lines each over the layout's placed runs; see
the demo's CJK scene and the gallery's Vertical scene.

For per-glyph choreography (rain, ripples, screensavers) walk the layout
with its glyph rest positions (`forEachPlacedGlyph` in the demo) and feed
`SkCanvas::drawGlyphsRSXform` — Scene F does this over a 1000-word
paragraph that is *also* re-laid-out every frame (breathing measure, live
edits): ~40 µs of relayout plus ~200 µs of per-letter math for ~4700
letters. On the CPU raster backend, quantize rotation angles (the demo
uses 64 steps) so Skia's glyph-mask cache can hit; the GPU path doesn't
care.

## Accuracy / performance dial

- **Shape-by-word (default, fast):** words are shaped independently, so
  cross-word kerning/ligatures at word boundaries are dropped — the same
  trade Chrome makes. Within a word (including CJK runs between break
  opportunities) shaping is fully contextual.
- Justification stretches spaces (shrink is clamped at the glue's shrink
  limit, TeX-style); for spaceless CJK it optionally expands inter-cluster
  gaps (`ideographicJustify`, capped by `maxIdeographicExpansion`).
- InDesign-style paragraph endings: `alignment = kJustify` plus
  `lastLineAlignment` (left/center/right) or `justifyLastLine` for full.
- Soft hyphens (U+00AD) are discretionary breaks in both breakers: invisible
  unless a line actually breaks there, in which case a styled hyphen is
  rendered; Knuth-Plass charges `hyphenPenalty` per hyphenated line. Feed
  text through any hyphenator (pattern dictionaries, ICU-based, manual) that
  inserts U+00AD — the engine handles the rest.
- Paint effects are per-span and draw-time only: `PaintStyle` carries a
  shader (gradients), a mask filter (glyph blur), a blend mode (e.g.
  kDstOut punch-outs), and a list of drop shadows. None of them reshape or
  relayout anything. `draw`/`drawBatched` take an optional paint override
  for caller-colored labels.
- OpenType features are per-span (`ShapingStyle::features`) and part of the
  shape-cache key: liga/dlig/smcp/lnum/frac variants of the same text
  coexist in the cache (see the demo's features panel).
- Script coverage is fallback-driven and tested: Arabic joining and
  lam-alef ligation, Devanagari conjuncts, supplementary-plane Cuneiform,
  emoji ZWJ/modifier/flag sequences as single grapheme clusters.
- Variable fonts shape at their design position: `faceFor` mirrors each
  SkTypeface's variation coordinates into HarfBuzz
  (`hb_font_set_variations`), so an instance made with
  `SkTypeface::makeClone(SkFontArguments)` shapes exactly what Skia
  rasterizes. Clone once and reuse — each clone carries its own uniqueID,
  hence its own shape-cache keys (the gallery's Weight slider caches one
  clone per family+weight for this reason).
- Rendering hygiene for animation: subpixel positioning is size-gated
  (<48px) and on-path rotations quantize to 512 steps, so Skia's glyph
  atlas keeps hitting while text moves; without both, animated 4K text
  re-rasterizes masks every frame.
- Knuth-Plass consumes one interval per line slot and always places at least
  one word per interval; filter slivers with `kpMinInterval` when combining
  KP with exclusions.
- Lines never render past the measure. The breakers only count shrink where
  placement will actually shrink (justified lines that aren't a demoted last
  line), final lines get TeX's \parfillskip (a loose last line is free), and
  when no break fits the tolerance the pass reruns with \emergencystretch
  (each line's own width added to its stretchability) rather than force an
  overfull line — one is only ever forced when a single unbreakable box is
  wider than every interval. Justified CJK shrinks ideographic gaps by up to
  0.03 em at render to mirror the breaker's model.

## Tuning knobs

Everything that trades fidelity against speed, in one place:

| Knob | Where | Default | Effect |
|---|---|---|---|
| `breaker` | LayoutOptions | greedy | Knuth-Plass is optimal but O(n·active); greedy is the fast path |
| `hyphenate`, `hyphenPenalty` | LayoutOptions | on, 50 | soft-hyphen breaks; raise the penalty to discourage them |
| `ideographicJustify`, `maxIdeographicExpansion` | LayoutOptions | on, 0.5em | CJK inter-cluster expansion when justifying |
| `glueStretch` / `glueShrink`, `kpTolerance` | LayoutOptions | 0.5/0.333, 4000 | TeX glue elasticity and badness tolerance |
| `kpMinInterval` | LayoutOptions | 0 | drop sliver intervals before Knuth-Plass sees them |
| `rotationSteps` | LayoutOptions | 512 | rotated/on-path glyph tangents snap to N directions so the glyph atlas hits during animation; 0 = exact rotations for static art |
| `advanceScale` | LineInterval | 1 | curvature compensation for offset baselines on contours |
| `verticalForm` | ShapingStyle | auto | per-span upright / rotated / tate-chu-yoko override in vertical mode |
| `letterSpacing` | ShapingStyle | 0 | tracking (JIS aki in vertical text); part of the shape key |
| subpixel gate | Shaper.cpp (`makeFont`) | <48px | subpixel positioning only where visible; above it every animated glyph would rasterize per-phase atlas entries |
| shape-cache cap | FontContextImpl.h | ~130k entries | wholesale clear past the cap (one cold frame), no LRU bookkeeping |
| `Paragraph` edit history | Paragraph.cpp | 256 ops | how far behind a MarkerSet may fall before it must re-query |
| bidi skip | automatic | — | the UBiDi pass runs only when a codepoint could be RTL |
| `draw` vs `drawBatched` | Layout | — | per-word blobs vs one drawGlyphs per (font, paint) bucket — use batched on GPU canvases with many runs |

## Threading model (and why there's no oneTBB)

A `FontContext` — shape cache, HarfBuzz buffer, ICU iterators, UBiDi — is
**single-threaded by contract**; every scratch object is reused rather than
locked. Parallelism happens *above* the library: one FontContext per
thread, one paragraph per task, embarrassingly parallel with zero shared
state (the app runs its context on the Qt render thread; the tests/bench/
demo each own theirs).

Intra-paragraph parallelism (e.g. oneTBB over words) was considered and
rejected: the only path over ~100 µs is *cold* shaping of never-seen text,
which is paid once per novel word per session; the warm paths this library
is built around are 10–150 µs, well below the cost of waking a thread pool.
A parallel shape pass would also force locks or sharding onto the shape
cache — the single hottest structure — to speed up the coldest path.
If a workload ever needs faster cold starts (megabytes of fresh text), the
right shape is N contexts shaping N paragraphs, then merging layouts —
which needs no library changes at all.

## Known limitations (v1)

- Bidi: levels are computed and per-line visual reordering (UAX#9 L2) is
  applied per word, but glue between reordered runs is approximated and
  multi-segment RTL words keep logical segment order. Latin+CJK (the
  priority) is unaffected.
- Restyling a range that cuts *inside* a word splits that word into
  separately shaped fragments (one-time reshape of the boundary words, like
  Chrome's run splitting); word-aligned ranges cost zero reshapes. Adjacent
  spans with identical styles re-merge automatically.
- No automatic hyphenation dictionary — soft hyphens must be present in the
  text (preprocess with a Liang-pattern hyphenator if needed).
- Placeholders: slots match their records by U+FFFC occurrence order (avoid
  replaceText edits that add/remove U+FFFC directly); a line never grows to
  fit a slot taller than the leading; breaks are allowed between a slot and
  adjacent punctuation (browser behaviour for replaced elements); straight
  intervals only (no slots on path contours).
- Vertical mode: rotated (Latin) runs put their baseline on the column axis
  rather than optically centering, RTL scripts inside vertical columns keep
  logical order, and ruby/kenten are external utilities by design. Tabs
  measure as spaces; one strut line height per layout (no per-line growth).
- Re-analysis on edit is full-paragraph (all cache hits except the edit):
  O(n) at ~0.2 µs/word. The next optimization, if ever needed, is windowed
  re-analysis around the edit — the word list and break table are already
  shaped for splicing.
- The shape cache evicts by clearing wholesale past ~130k entries (one cold
  frame), not LRU.
