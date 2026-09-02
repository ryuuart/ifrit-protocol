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

Ready-made geometries cover the common cases: `BlockFlow` (a rectangle),
`ExclusionFlow` (a rectangle minus moving circles, rects, or arbitrary
`SkPath`s with their fill rule honored), `VerticalBlockFlow` (top-to-bottom
columns advancing right to left), `LineSetFlow` (explicit intervals — any
origin, direction, and count per line), and `PathFlow` (each contour of a
path becomes a line). What a `LineRequest` carries, what a contour
interval means, and what a column costs `ExclusionFlow` are in
`FEATURES.md` under the flow geometries.

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
| `SigilWeaveStyle` | the style vocabulary, header-only, with `Type` and `type()` — the designated-init aggregate a call site names a style's numbers in | — |
| `SigilWeaveFonts` | the font service and the shaper | HarfBuzz, abseil — private |
| `SigilWeaveParagraph` | the document model | SigilWeaveUnicode, abseil — private |
| `SigilWeaveLayout` | flows, breakers, placement, metrics | SigilGeometryPath (public: `LineInterval::contour` is a `geometry::path::Contour`); ICU, abseil — private |
| `SigilWeaveDecoration` | decoration bands | SigilCoreCompute (the stir the skip-ink cache keys with) — private |
| `SigilWeavePaint` | `draw()` and `drawBatched()`, `paint/Paint.h` | — |
| `SigilWeaveChoreograph` | per-glyph choreography | — |
| `SigilWeaveQuery` | range search and markers | ICU, private |
| `SigilWeaveCache` | the label cache | abseil, ICU — private |
| `SigilWeave` | interface over every target above | — |
| `SigilWeaveShaders` | `shaders/PaintShaders.h` — water, mesh gradient, sparkle, star nest, clouds, tunnel | SigilMaterialKit, SigilMaterialSkia — private; not in the export set |
| `SigilWeavePorts` | `ports::systemFontManager()` — CoreText today; DirectWrite/Fontconfig slot into the same call — and `ports::pickFace()`, the first installed family of a fallback chain | Skia platform ports |
| `SigilWeaveKit` | consumer-side discipline: rebuild/layout guards, glyph bucketing, label shorthand, sample content, the line-edge and hyphenation tables (see `kit/README.md`) | SigilWeaveUnicode — private |
| `SigilWeaveQt` | interface target: `QFont` → `SkTypeface`, `QString` ↔ `Paragraph` with no transcoding | Qt6::Gui |

Each feature links only the features beneath it — style, then fonts, then
paragraph, then layout, with decoration, paint, choreograph, query and
cache each resting on the one they need — so a consumer of one tier links
that tier alone; `SigilWeave` is for a consumer of the whole engine. Skia
and SigilGeometryPath are PUBLIC dependencies — the path a line of text
follows is a geometry contour, and `ExclusionFlow` flattens its shapes
through the same library; the Unicode leaf, HarfBuzz, ICU and abseil are
PRIVATE and appear in no public header. Pimpls hide the hash maps, and
`Word::segments()` hands out a `std::span` over storage whose container
type only the paragraph feature sees, so the one abseil container inside
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
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug -R weave_ --output-on-failure
```

The tests are one binary per feature, each under its feature's `test/`
and linking that feature and what it rests on, so a test binary is also a
statement of what its feature reaches:

- `weave_unicode_test` — the Unicode leaf, with no fonts at all.
- `weave_style_test` — styles as plain values: fluent sugar, paint-layer
  presets, feature preset tags, the `StyleSet` registry. No fonts either.
- `weave_fonts_test` — the font service on its own: the fallback memo, the
  transient varied clone, and which vertical OpenType features a column
  takes by itself against which a style must name.
- `weave_paragraph_test` — shaping as the paragraph drives it (the shape
  cache under edits and restyles, itemization, complex scripts), the
  document model, and typographic correctness: cluster coverage across
  scripts, ZWNJ joining control, combining-mark attachment (NFC and NFD
  must measure alike), kinsoku prohibitions, NBSP no-break, strut metrics,
  and the options that reach shaping.
- `weave_layout_test` — both breakers, flows and exclusions, overflow and
  clamp, vertical writing, placeholders, relayout locality, the options
  the breakers read, text on a path, justification shrink limits, UAX#9
  visual reordering, edit safety at surrogate boundaries, line metrics,
  and the large-paragraph stress cases.
- `weave_decoration_test` — bands resolved as geometry, without drawing.
- `weave_paint_test` — everything that puts pixels on a surface: paint
  layers and shaders without a relayout, decorations as drawn, selection
  bands, and a large paragraph through the runtime shaders.
- `weave_choreograph_test` — the walk, the batches, and a glyph on a
  contour re-placed from its pen.
- `weave_query_test` — the optional Query layer.
- `weave_kit_test` — the SigilWeaveKit convenience layer.

Fixtures live in `test/support/`: `Fonts.h` holds the one process-wide
`FontContext` every binary shapes with, `Paragraphs.h` and `Layouts.h` the
paragraph and layout fixtures, and each binary that needs more has a
support header that includes exactly the headers its translation units
use. `test/assets/` holds the constructed faces a question needs that no
installed font can answer — `VerticalFeatures.ttf`, where every vertical
feature has its own visible consequence and none share one — each with
the script that generates it beside it; `SIGILWEAVE_TEST_ASSET_DIR` names
the directory to the binaries that read them.

The benchmarks own every performance claim about this library — one
binary per feature, under its feature's `bench/`, so each links only what
it measures: `weave_unicode_bench` (itemize, line breaks and bidi per code
point, on the Unicode leaf alone), `weave_fonts_bench` (`shapeWord` per
word cold and warm), `weave_paragraph_bench` (whole paragraphs shaped cold
against warm), `weave_layout_bench` (`layoutParagraph` per word, greedy
and Knuth-Plass by length, and each kind of per-frame update against the
same warm relayout) and `weave_paint_bench` (`draw` and `drawBatched` per
glyph on a raster surface, with arms that differ in one paint feature).
The corpus and font context they share sit in `bench/support/`. Build
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
