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

**`FEATURES.md` is the catalogue.** This page is what the library is, its
seams, how to reach it, and what it will not do. Everything it covers —
the pipeline stage by stage, the header map, the paragraph controls, the
parity table with the compose path for every row, what a frame of the
live composer costs, and the conventions to read before writing against
it — is one file over.

## Getting started

```cpp
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
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
  SingleContourFlow(geometry::path::Contour contour, float start)
      : m_contour(std::move(contour)), m_start(start) {}

  using FlowGeometry::lineIntervals;
  bool lineIntervals(const LineRequest &request,
                     std::vector<LineInterval> &intervals) override {
    if (request.index > 0)
      return false;              // one line only; false = geometry exhausted
    LineInterval interval;
    interval.contour = m_contour;
    interval.contourStart = m_start;   // animate this for a marquee
    interval.length = m_contour.length();
    intervals.push_back(interval);
    return true;
  }

private:
  geometry::path::Contour m_contour;   // geometry::path::Contour::of(path).front()
  float m_start;
};
```

Ready-made geometries cover the common cases: `BlockFlow`,
`ExclusionFlow`, `VerticalBlockFlow`, `LineSetFlow` and `PathFlow`. What
each one is, what a `LineRequest` carries, what a contour interval means,
and what a column costs `ExclusionFlow` are in `FEATURES.md` under the
flow geometries.

## The seams

Five decisions, each owned in one place. Everything else this library
offers is a value handed to one of them, and `FEATURES.md` is the whole
catalogue of those values.

| Seam | What it decides | Reached through |
|---|---|---|
| **Break decision** | where lines end — the breaker, its demerits, which hyphenation points it takes, how far it may spend on justification, and the budget past which it gives up for a frame | `LineBreakStrategy`, `KnuthPlassOptions`, `HyphenationOptions`, `JustificationOptions` |
| **Line placement** | where a line's words sit — the intervals a geometry hands back for a band, the indents that inset them, the tab stops inside them, the edge a character may hang past, and where a frame seats its first baseline | `FlowGeometry`, `IndentOptions`, `TabStop`, `HangingTable`, `FrameOptions` |
| **Strut** | a block's pitch — its leading mode, and every band something set beside the type reserved | `Leading`, `ReservedBand` |
| **Story** | the shaped-once text and where a fill stopped, so the next frame begins there | `Paragraph`, `layoutParagraph`'s resume word, `ParagraphLayout::firstUnplacedWord` |
| **Segmentation** | where a break may happen at all — the language's own tailoring, and a house's prohibitions on top of it | `Paragraph::setLineBreakLocale`, `KinsokuTable`, `Hyphenator` |

Two rules run through all five and are worth stating once.

**The library decides nothing typographic.** No fraction of a base's size
is written anywhere in it: a reading's size is its own style's, a
prohibition set is data, a mojikumi table is data, a hyphenation pattern
table is data. `kit/` ships stock tables and a caller's own is their peer.

**Settled text is the special case.** A page laid out once and then read
is the easy end of what this engine is for; the ordinary end is text
whose measure animates, whose frame grows, whose content changes between
frames. `ParagraphLayoutOptions::live` is how a caller says an input is
moving, and the layout REPORTS what it did about it rather than deciding
whether anything has settled — there is one such proof in a runtime and
this is not it.

## What it covers

`FEATURES.md` is the catalogue, and its parity table is the fastest
answer to "can it do X": one row per control a page-layout application's
panels present, what its status is here, and — for a SigilCompose author
— the verb or field that reaches it. Read it before assuming something
is missing.

The short version: both breakers with the whole setting around them
(hyphenation with per-language pattern tables, three passes of
justification, balanced rag, the keeps enforced at a frame boundary),
paragraph styles per block with four indents and four leading kinds,
threaded frames, tabs with leaders, vertical CJK with the line-edge and
full-width tables a printed page is set under, readings beside the type,
per-glyph choreography over any of it, and a live composer built to run
every frame.

## Targets and dependencies

| Target | Contents | Beyond Skia |
|---|---|---|
| `SigilWeaveUnicode` | the Unicode leaf | ICU and HarfBuzz's ICU bridge, private; no Skia |
| `SigilWeaveStyle` | the style vocabulary, header-only, with `Type` and `textStyle()` — the designated-init aggregate a call site names a style's numbers in | — |
| `SigilWeaveFonts` | the font service and the shaper | HarfBuzz, Boost.Unordered and Boost.ContainerHash — private |
| `SigilWeaveParagraph` | the document model | SigilWeaveUnicode, Boost.Container — private |
| `SigilWeaveLayout` | flows, breakers, placement, metrics | SigilGeometryPath (public: `LineInterval::contour` is a `geometry::path::Contour`); ICU and Boost.Unordered — private |
| `SigilWeaveDecoration` | decoration bands | SigilCoreCompute (the stir the skip-ink cache keys with) — private |
| `SigilWeavePaint` | `draw()` and `drawBatched()`, `paint/Paint.h` | — |
| `SigilWeaveChoreograph` | per-glyph choreography | — |
| `SigilWeaveQuery` | range search and markers | ICU, private |
| `SigilWeaveCache` | the label cache | Boost.Unordered, ICU — private |
| `SigilWeave` | interface over every target above | — |
| `SigilWeavePorts` | `ports::systemFontManager()` — CoreText on Apple; DirectWrite and Fontconfig slot into the same call — and `ports::pickTypeface()`, the first installed family of a fallback chain | Skia platform ports |
| `SigilWeaveKit` | consumer-side discipline: rebuild/layout guards, glyph bucketing, label shorthand, sample content, the line-edge and hyphenation tables (see `kit/README.md`) | SigilWeaveUnicode — private |
| `SigilWeaveQt` | interface target: `QFont` → `SkTypeface`, `QString` ↔ `Paragraph` with no transcoding | Qt6::Gui |

Each feature links only the features beneath it — style, then fonts, then
paragraph, then layout, with decoration, paint, choreograph, query and
cache each resting on the one they need — so a consumer of one tier links
that tier alone; `SigilWeave` is for a consumer of the whole engine. Skia
and SigilGeometryPath are PUBLIC dependencies — the path a line of text
follows is a geometry contour, and `ExclusionFlow` flattens its shapes
through the same library; the Unicode leaf, HarfBuzz, ICU and Boost are
PRIVATE and appear in no public header. Pimpls hide the hash maps, and
`Word::segments()` hands out a `std::span` over storage whose container
type only the paragraph feature sees, so the one Boost container inside
a value type never reaches a consumer. The engine is Qt-free and carries
no SkSL: shader presets are content, not engine.

Install and export rules are generated, so an installed tree works with:

```cmake
find_package(SigilWeave CONFIG REQUIRED)                # engine, Qt-free
find_package(SigilWeave CONFIG REQUIRED COMPONENTS Kit) # + SigilWeaveKit
find_package(SigilWeave CONFIG REQUIRED COMPONENTS Qt)  # + SigilWeaveQt
target_link_libraries(app PRIVATE
  sigil::weave::SigilWeave sigil::weave::SigilWeavePorts)
```

Everything compiles as standard C++20 with extensions disabled. Public APIs
use `std::span` views, concept-constrained callbacks, and
`[[nodiscard("reason")]]` where ignoring a return silently corrupts caller
state.

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
- **Drawing a reading is not the engine's.** `layout/Beside.h` answers the
  three questions setting one is made of — the band a reading of a given
  type needs beside a line, where it stands against the base it reads, and
  how a base broken across two lines shares it — and knows nothing about
  what a ruby IS, which unit somebody annotated, or how big a reading
  should be. Turning those answers into glyphs is the caller's.
- **Bidi is per-word.** Levels are computed and UAX#9 L2 visual reordering is
  applied per word; glue between reordered runs is approximated, and
  multi-segment RTL words keep logical segment order.

## Build, test, and see it

From `apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Release
cmake --build build --config Release
ctest --test-dir build -C Release -R weave_ --output-on-failure
```

The tests are one binary per feature, each under its feature's `test/`
and linking that feature and what it rests on, so a test binary is also a
statement of what its feature reaches. A binary exists only where it links
a **strictly smaller** set of targets than its neighbours, or where what a
runner must supply to run it differs — two binaries over one closure with
one answer to that question are one binary.

A case asserts one behaviour the library promises through its public
headers to a caller who has read only this page, and its name is that
promise written as a sentence, so a failure reads as the claim that broke.
It pins only what editing this library could falsify — never an
anti-aliased byte, a fitted tolerance, a count a font or a locale could
move, or elapsed time. Nothing here reads a clock: what a thing costs is
the benchmarks' to judge, and what a page looks like is the plate
ledger's. A claim made N times with one thing varying is one `TEST_P`
whose parameter is that thing, with its rows named — both breakers over
one breaking claim, every anchor of a decoration band, every preset text
paint. One subject to a file, named for the subject: a case is found by
opening the file its subject names.

- `weave_unicode_test` — the Unicode leaf, with no fonts at all.
- `weave_style_test` — styles as plain values: fluent sugar, paint-layer
  presets, the `StyleSet` registry. No fonts either, and no run-time case
  for the feature preset tags: they are `static_assert`ed beside their own
  declarations, where only editing them can falsify them.
- `weave_fonts_test` — the font service asked about faces this machine
  happens to have: the fallback memo keyed by language, the transient
  varied clone, and the pair a shaper sets by measuring outlines.
- `weave_vertical_features_test` — which vertical OpenType features a
  column takes by itself and which a style must name, asked of the
  constructed face committed under `test/assets/`.
- `weave_paragraph_test` — shaping as the paragraph drives it (the shape
  cache under edits and restyles, itemization, complex scripts), the
  document model, and typographic correctness: cluster coverage across
  scripts, ZWNJ joining control, combining-mark attachment (NFC and NFD
  must measure alike), NBSP no-break, strut metrics, and the options that
  reach shaping.
- `weave_layout_test` — everything that places runs and reads where they
  landed, one subject to a file: both breakers (`LayoutTest`,
  `KnuthPlassTest`) and the live composer over them
  (`LiveComposerTest`), the flows (`FlowTest`), overflow and clamp
  (`OverflowTest`), vertical writing (`VerticalTest`), placeholders,
  relayout locality (`IncrementalTest`), text set on a geometry of its own
  (`PathTextTest`), how a justified line is fitted
  (`JustificationTest`), and each paragraph control — `LeadingTest`,
  `TabStopTest`, `FrameTest`, `HyphenationTest`, `LineEdgesTest`,
  `BesideTest`, `MojikumiTest`, `BalanceTest`.
- `weave_decoration_test` — bands resolved as geometry, without drawing:
  what a face's metrics fill in, where each kind and side anchors its
  band, and the walk both draws run over turning a paragraph's
  decorations into rectangles.
- `weave_cache_test` — the single-line paragraph cache: what its key
  discriminates, the size step two nearby sizes fall inside, and the two
  promises its node-based storage makes about a reference it handed out.
- `weave_paint_test` — what reaches the canvas: paint layers and shaders
  without a relayout, selection bands, a pass shaded through the material
  resolver a host installs, the preset text paints resolving, and the
  decoration ink. Where a band LANDS is geometry and belongs to the
  decoration feature; only whether its ink arrives is asked here.
- `weave_choreograph_test` — the walk over a finished layout with the
  glyph on a contour it re-places from its pen (`ChoreographTest`), and
  the buckets a paint-complete batched draw collapses into
  (`GlyphBatchesTest`).
- `weave_query_test` — the optional Query layer.
- `weave_kit_test` — the SigilWeaveKit convenience layer, including the
  pattern hyphenator every table question is asked of.

| label | binaries | what a runner must supply |
|---|---|---|
| `fonts` | `weave_paragraph_test` | installed faces broad enough for an unstyled paragraph of mixed scripts and emoji to resolve — the machine's own fallback is what those cases are about |
| — | every other binary | nothing: they read the Unicode leaf, plain values, or a committed instrument |

A case that skips is not coverage on the machine it skipped on, so a case
whose claim is about a script, an axis or a feature names the instrument
that carries it and skips on nothing; what is left behind the label is the
handful whose claim IS the machine's font set. `ctest -L fonts` selects
them, so a runner that knows its own font set can require what the rest of
the tree lets pass.

Fixtures live in `test/support/`, and nothing is written twice: `Faces.h`
holds this library's own committed face, `Paragraphs.h` builds paragraphs
and the deterministic texts drawn from a word pool, `Layouts.h` takes the
readings off a finished layout — which runs placed glyphs, where each line
ended, how wide it is, how many glyphs it placed, whether every run stayed
inside an interval its band offered, and the two-word setting a decoration
band is read across — `LayoutSupport.h` carries the breaker parameter a
breaking claim is held to both ways, `Paints.h` a shader whose colour says
where it was sampled, `Pixels.h` scans a rendered surface, and
`Readings.h` the spread of a set of measurements. The font context itself
is the whole test tree's, `sigil::test::fonts()` from `src/test/Fonts.h`,
so one process shapes through one cache. `Layouts.h` calls no GoogleTest
assertion, so the benchmarks include it and count what the tests count.
Each binary that needs more has a support header that includes exactly
the headers its translation units use. `test/assets/` holds the face only
this library asks for — `VerticalFeatures.ttf`, where every vertical
feature has its own visible consequence and none share one — with the
script that generates it beside it, reached through
`SIGIL_TEST_ASSET_DIR`. The faces more than one library asks for are the
tree's, under `src/test/assets/` and reached as
`sigil::test::instrument::sans()` and its siblings: a ligature, an
advance-moving axis beside an advance-holding one, an A/V pair for an
optical kerner, zero-advance combining marks, and the coverage for
Arabic, Devanagari and a supplementary-plane script.

The benchmarks own every performance claim about this library — one
binary per feature, under its feature's `bench/`, so each links only what
it measures: `weave_unicode_bench` (itemize, line breaks and bidi per code
point, on the Unicode leaf alone), `weave_fonts_bench` (`shapeWord` per
word cold and warm), `weave_paragraph_bench` (whole paragraphs shaped cold
against warm), `weave_layout_bench` (`layoutParagraph` per word, greedy
and Knuth-Plass by length, and each kind of per-frame update against the
same warm relayout) and `weave_paint_bench` (`draw` and `drawBatched` per
glyph on a raster surface, with arms that differ in one paint feature).
The corpus they share sits in `bench/support/`, over the same font
context and the same layout readings the tests use. Build
them Release through the `benches` target and run them through
`scripts/bench_ledger.py` rather than trusting a number written down
anywhere:

```sh
cmake --build build --config Release --target benches weave_demo
python3 scripts/bench_ledger.py --benches weave_layout_bench
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
