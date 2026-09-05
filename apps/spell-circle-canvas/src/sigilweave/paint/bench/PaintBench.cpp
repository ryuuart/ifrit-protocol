/** @file
 * weave_paint_bench — drawing a finished layout per glyph on a CPU
 * raster surface (the production path is the GPU, but the quantity here
 * is the CPU-side submission work before any glyph reaches a backend):
 * draw against drawBatched, and arms that differ in exactly one paint
 * feature, so the feature's cost is the difference between two runs.
 * Run a Release build; Debug numbers say nothing.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <cmath>

#include "support/Corpus.h"
#include "support/Layouts.h"
#include <sigilweave/kit/PaintLayers.h>

using namespace sigil::weave;

namespace {
/// A text-paint preset shaded by SigilMaterial's Skia backend.
sk_sp<SkShader> shade(const sigil::material::Material& m) {
  sigil::material::skia::install();
  return sigil::material::skia::shader(m, {});
}
}  // namespace
using namespace sigil::weave::bench;

namespace {

void countGlyphs(benchmark::State& state, const ParagraphLayout& layout) {
  state.counters["glyphs/s"] =
      benchmark::Counter((double)glyphCount(layout),
                         benchmark::Counter::kIsIterationInvariantRate);
}

struct Scene {
  Paragraph paragraph;
  ParagraphLayout layout;
  sk_sp<SkSurface> surface;
};

/** A mixed-script paragraph of `words` in `style`, laid out in a column
 *  and given a raster surface tall enough to hold it. */
Scene scene(int words, const TextStyle& style) {
  Scene s;
  s.paragraph.appendText(makeText(words, /*mixed=*/true), style);
  BlockFlow flow(SkRect::MakeWH(700, 40000));
  s.layout = layoutParagraph(sigil::test::fonts(), s.paragraph, flow);
  const int height =
      (int)std::ceil((float)s.layout.lineCount * s.layout.linePitch) + 40;
  s.surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(720, height));
  return s;
}

/** The same quantity of type set DOWN COLUMNS: upright CJK in a vertical
 *  block flow, on a surface wide enough for the columns it fills. */
Scene columnScene(int words, const TextStyle& style) {
  Scene s;
  s.paragraph.appendText(makeColumnText(words), style);
  s.paragraph.setWritingMode(WritingMode::kVerticalRL);
  ParagraphLayoutOptions options;
  options.lineMetrics.height = 26;  // column pitch
  VerticalBlockFlow flow(SkRect::MakeWH(1400, 680));
  s.layout = layoutParagraph(sigil::test::fonts(), s.paragraph, flow, options);
  s.surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1400, 700));
  return s;
}

void BM_Draw(benchmark::State& state) {
  Scene s = scene((int)state.range(0), basicStyle());
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.draw(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_Draw)->Arg(300)->Arg(2000)->Unit(benchmark::kMicrosecond);

void BM_DrawBatched(benchmark::State& state) {
  Scene s = scene((int)state.range(0), basicStyle());
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched)->Arg(300)->Arg(2000)->Unit(benchmark::kMicrosecond);

// Identical corpus, flow, surface and draw path to BM_DrawBatched/300 —
// the ONLY difference is one default underline decoration (skipInk on).
// The difference between the two runs is the whole per-frame price of a
// skip-ink underline: font metrics once per decoration group, plus the
// glyph-ink intercepts of every run in the group.
void BM_DrawBatched_SkipInkUnderline_300w(benchmark::State& state) {
  TextStyle underlined = basicStyle();
  underlined.paint.addDecoration(Decoration{});
  Scene s = scene(300, underlined);
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_SkipInkUnderline_300w)->Unit(benchmark::kMicrosecond);

// The control that makes the arm above attributable: the same decoration
// with skip-ink OFF. Plain-vs-none is the price of drawing a band at all;
// skipInk-vs-plain is an upper bound on the intercept cost, because ink
// skipping also replaces one wide band rect per group with several
// segment fills.
void BM_DrawBatched_PlainUnderline_300w(benchmark::State& state) {
  TextStyle underlined = basicStyle();
  Decoration decoration;
  decoration.skipInk = false;
  underlined.paint.addDecoration(decoration);
  Scene s = scene(300, underlined);
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_PlainUnderline_300w)->Unit(benchmark::kMicrosecond);

// The column pair, read the way the underline pair above is read: the
// first arm is a plain vertical setting drawn batched, the second the same
// setting with one band beside every column. A column's band is one rect
// per group with no intercepts to cut, so the difference is the band walk
// and the fills alone.
void BM_DrawBatched_Column_600w(benchmark::State& state) {
  Scene s = columnScene(600, basicStyle());
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_Column_600w)->Unit(benchmark::kMicrosecond);

void BM_DrawBatched_ColumnSideline_600w(benchmark::State& state) {
  TextStyle sidelined = basicStyle();
  sidelined.paint.addDecoration(Decoration{});
  Scene s = columnScene(600, sidelined);
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_ColumnSideline_600w)->Unit(benchmark::kMicrosecond);

// Four ordered glyph passes: blurred shadow, blurred glow, outline, fill.
// Draw submission scales with pass count; blur-mask work depends on the
// backend and the font size.
void BM_DrawBatched_4PassEffects_300w(benchmark::State& state) {
  TextStyle layered = basicStyle();
  layered.paint.addUnderlay(sigil::weave::kit::dropShadow(0x66000000, {2, 2}, 2.0f))
      .addUnderlay(sigil::weave::kit::glow(0x440000FF, 3.0f))
      .addUnderlay(sigil::weave::kit::outline(SK_ColorBLACK, 1.5f));
  Scene s = scene(300, layered);
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(SK_ColorWHITE);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_4PassEffects_300w)->Unit(benchmark::kMicrosecond);

/** Two thousand words at 8 px, justified into a 1180 x 880 panel, with or
 *  without the shader-heavy paint stack (glow, outline, mesh-gradient
 *  fill, sparkle overlay). */
Scene wall(bool effects) {
  TextStyle textStyle = basicStyle();
  textStyle.shaping.fontSize = 8.0f;
  const SkRect bounds = SkRect::MakeXYWH(10, 10, 1180, 880);
  if (effects) {
    textStyle.paint.addUnderlay(sigil::weave::kit::glow(0x772A77FF, 1.8f))
        .addUnderlay(sigil::weave::kit::outline(0xFF061229, 0.7f));
    textStyle.paint.foreground.setShader(
        shade(sigil::material::kit::meshGradient(bounds, 1.25f)));
    SkPaint stars;
    stars.setAntiAlias(true);
    stars.setShader(shade(sigil::material::kit::sparkle(bounds, 1.25f)));
    stars.setBlendMode(SkBlendMode::kScreen);
    textStyle.paint.addOverlay(PaintLayer(std::move(stars)));
  }
  Scene s;
  s.paragraph.appendText(makeText(2000, /*mixed=*/true), textStyle);
  BlockFlow flow(bounds);
  ParagraphLayoutOptions options;
  options.alignment = TextAlignment::kJustify;
  options.lineMetrics.height = 10.0f;
  s.layout = layoutParagraph(sigil::test::fonts(), s.paragraph, flow, options);
  s.surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1200, 900));
  return s;
}

void BM_DrawBatched_Wall_2000w(benchmark::State& state) {
  Scene s = wall(state.range(0) != 0);
  state.SetLabel(state.range(0) ? "shader effects" : "plain");
  for ([[maybe_unused]] auto iteration : state) {
    s.surface->getCanvas()->clear(0xFF050A18);
    s.layout.drawBatched(s.surface->getCanvas(), s.paragraph);
    benchmark::DoNotOptimize(s.surface.get());
  }
  countGlyphs(state, s.layout);
}
BENCHMARK(BM_DrawBatched_Wall_2000w)
    ->Arg(0)
    ->Arg(1)
    ->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
