/** @file
 * The tiles under load: the bake per generator and pitch, a tile rebuilt
 * while an equal one is held, and the repeating texture's shader per call.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Painted.h>
#include <sigilmaterial/skia/Texture.h>

using namespace sigil::material;

namespace {

void TileBakeHalftone(benchmark::State& state) {
  const float pitch = (float)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile t =
        pattern::halftone(pitch, pitch * 0.35f, {0.1f, 0.1f, 0.12f, 1});
    benchmark::DoNotOptimize(skia::image(t.texture()));
  }
}
BENCHMARK(TileBakeHalftone)->Arg(4)->Arg(16)->Arg(64);

void TileBakeSpeckle(benchmark::State& state) {
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile t = pattern::speckle(64, count, 0.5f, 2, {{1, 1, 1, 1}});
    benchmark::DoNotOptimize(skia::image(t.texture()));
  }
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(TileBakeSpeckle)->Arg(50)->Arg(500);

/** A tile rebuilt from the same numbers while one is held — a pattern
 *  re-described on every render. Arg 0 makes it from a bare program,
 *  which bakes every time; arg 1 from the same program and its recipe,
 *  which finds the held bake. */
void TileRebuiltWhileHeld(benchmark::State& state) {
  const bool fromRecipe = state.range(0) == 1;
  const auto build = [&] {
    pattern::Program dots =
        skia::painted([](SkCanvas& canvas, SkSize, uint32_t) {
          SkPaint paint;
          paint.setAntiAlias(true);
          for (int index = 0; index < 500; ++index)
            canvas.drawCircle((float)(index * 37 % 64), (float)(index * 11 % 64),
                              1.5f, paint);
        });
    return fromRecipe ? pattern::Tile::of({64, 64}, std::move(dots),
                                          {"dots", {64, 500, 1.5}})
                      : pattern::Tile::of({64, 64}, std::move(dots));
  };
  const pattern::Tile held = build();
  held.texture();
  for ([[maybe_unused]] auto iteration : state) {
    pattern::Tile rebuilt = build();
    benchmark::DoNotOptimize(skia::image(rebuilt.texture()));
  }
}
BENCHMARK(TileRebuiltWhileHeld)->Arg(0)->Arg(1);

void TileTexture(benchmark::State& state) {
  const pattern::Tile t = pattern::checker(8, {0, 0, 0, 1}, {1, 1, 1, 1});
  t.texture();
  for ([[maybe_unused]] auto iteration : state) {
    sk_sp<SkShader> s = skia::shader(t.texture());
    benchmark::DoNotOptimize(s);
  }
}
BENCHMARK(TileTexture);

}  // namespace
