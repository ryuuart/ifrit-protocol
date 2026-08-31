/** @file
 * The 2D frame loop: what a host pays per frame for a scene that only
 * declares itself once, which is the floor every sketch stands on.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** A grid of filled boxes, described once. Nothing re-describes, so what
 *  is measured is the session's own per-frame cost. */
struct Grid : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(640, 480);
    Element root = stack();
    for (int i = 0; i < 64; ++i) {
      const int column = i % 8;
      const int row = i / 8;
      root =
          root.child(box()
                         .width(60)
                         .height(40)
                         .inset((float)column * 78.0f, (float)row * 58.0f, 0, 0)
                         .fill(Fill::color({0.2f, 0.4f, 0.8f, 1})));
    }
    ctx.composer.render(root);
  }
};

void Frame(benchmark::State& state) {
  std::unique_ptr<Session> session = kindOf<Grid>()->open(fonts(), assets());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  for (int i = 0; i < 8; ++i) session->frame(*surface->getCanvas(), 1.0 / 60.0);
  for (auto&& _ : state) session->frame(*surface->getCanvas(), 1.0 / 60.0);
}
BENCHMARK(Frame)->Unit(benchmark::kMicrosecond);

void Open(benchmark::State& state) {
  const Kind kind = kindOf<Grid>();
  for (auto&& _ : state)
    benchmark::DoNotOptimize(kind->open(fonts(), assets()));
}
BENCHMARK(Open)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
