# TextFlow

A flexible, cache-first text layout engine for Skia, inspired by Chromium's
LayoutNG (shape-by-word caching, paint/shaping style split) and Cheng Lou's
[Pretext](https://github.com/chenglou/pretext) (shape once, position freely,
lines are not rectangles). It shapes with HarfBuzz and analyzes with ICU
directly — no SkShaper, no SkParagraph — so every intermediate result (words,
glyph runs, blobs) is exposed and reusable.

```
Paragraph (UTF-16 + style spans)
  │  ICU: line-break opportunities (UAX#14), bidi levels (skipped when
  │       LTR-only), script itemization, per-codepoint font fallback
  ▼
Word list          — break-delimited segments; content + trailing glue
  │  HarfBuzz via the ShapeCache: content-addressed by
  │  (typeface, size, letter-spacing, script, dir, lang, features, text)
  ▼
ShapedWord (shared, immutable) — glyphs, advances, clusters,
  │                              + lazily built origin-relative SkTextBlob
  ▼
FlowGeometry       — per line: ordered intervals (rect bands, bands minus
  │                  exclusion shapes, arbitrary segments, SkPath contours)
  ▼
Breaker            — greedy or Knuth-Plass, both feeding one placement stage
  ▼
Layout             — PlacedRuns: shared word blob + origin (horizontal), or
                     RSXform-baked blob (rotated / on-path); draw() resolves
                     paint per span at draw time
```

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

Run them yourself:

```
cmake --build build --config Release --target textflow_bench textflow_demo
./build/bin/Release/textflow_bench
./build/bin/Release/textflow_demo        # writes PNGs + per-frame timings
ctest --test-dir build -C Debug -R textflow_test   # or run the binary
```

## API sketch

```cpp
FontContext ctx(SkFontMgr_New_CoreText(nullptr));   // one per thread

ParagraphBuilder b(baseStyle);
b.addText("Glyphs flow ").pushStyle(red).addText("around").pop()
 .addText(" obstacles… 日本語も 한국어도 中文也");
Paragraph para = b.build();

ExclusionFlow flow(SkRect::MakeWH(900, 700));
flow.shapes().push_back({ExclusionFlow::Shape::kCircle, circleBounds, 8});

LayoutOptions opts;                    // greedy + justify, or:
opts.breaker = Breaker::kKnuthPlass;   // optimal paragraph breaking
opts.alignment = Alignment::kJustify;

Layout layout = layoutParagraph(ctx, para, flow, opts);
layout.draw(canvas, para);             // or walk layout.runs yourself

para.replaceText(4, 9, "swift");       // edit: next layout re-shapes 1 word
para.setPaint(0, 6, {SK_ColorRED});    // restyle: re-shapes 0 words
```

Geometries: `BlockFlow` (rect), `ExclusionFlow` (rect minus moving
circles/rects), `LineSetFlow` (explicit intervals — any origin/direction/
count per line), `PathFlow` (each SkPath contour is a line; glyphs follow
the tangent via RSXform runs). Closed contours wrap arc positions, so
animating `LineInterval::contourStart` produces an infinite marquee.
Implement `FlowGeometry` for anything else.

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
  shader (gradients), a mask filter (glyph blur), and a list of drop
  shadows. None of them reshape or relayout anything.
- Knuth-Plass consumes one interval per line slot and always places at least
  one word per interval; filter slivers with `kpMinInterval` when combining
  KP with exclusions.

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
- No vertical text, tabs measure as spaces, one strut line
  height per layout (no per-line growth).
- Re-analysis on edit is full-paragraph (all cache hits except the edit):
  O(n) at ~0.2 µs/word. The next optimization, if ever needed, is windowed
  re-analysis around the edit — the word list and break table are already
  shaped for splicing.
- The shape cache evicts by clearing wholesale past ~130k entries (one cold
  frame), not LRU.
