/** @file
 * The immediate-mode frame loop: what a host pays per frame for a sketch
 * that draws a few hundred shapes, which is the floor a pen sketch
 * stands on — the surface, the blit and the pen's own bookkeeping.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/draw/Draw.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace {

using namespace sigil::sketch;
using namespace sigil::draw;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** Five hundred circles a frame over a translucent ground. */
struct Field : DrawSketch {
  void setup(DrawContext& ctx) override {
    ctx.canvas(640, 480);
    ctx.pen.noStroke();
  }
  void draw(DrawContext& ctx) override {
    Pen& pen = ctx.pen;
    pen.background(0, 20);
    pen.fill(220, 120, 80);
    for (int i = 0; i < 500; ++i)
      pen.circle(pen.random(640), pen.random(480), 10);
  }
};

void DrawFrame(benchmark::State& state) {
  std::unique_ptr<Session> session = kindOf<Field>()->open(fonts(), assets());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  for (int i = 0; i < 8; ++i) session->frame(*surface->getCanvas(), 1.0 / 60.0);
  for (auto&& _ : state) session->frame(*surface->getCanvas(), 1.0 / 60.0);
}
BENCHMARK(DrawFrame)->Unit(benchmark::kMicrosecond);

void DrawOpen(benchmark::State& state) {
  const Kind kind = kindOf<Field>();
  for (auto&& _ : state)
    benchmark::DoNotOptimize(kind->open(fonts(), assets()));
}
BENCHMARK(DrawOpen)->Unit(benchmark::kMicrosecond);

}  // namespace
