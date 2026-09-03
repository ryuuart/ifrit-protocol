/** @file
 * What a frame of the pen costs: ten thousand circles, filled and
 * stroked, ten thousand rects, a screen of text, and a background.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Draw.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

namespace {

using namespace sigil::draw;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

/** A pen over a raster canvas the size of a modest sketch. */
struct Bench {
  Bench() : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480))) {}
  void frame(int count) {
    Frame f;
    f.width = 640;
    f.height = 480;
    f.seconds = count / 60.0;
    f.deltaSeconds = 1.0 / 60.0;
    f.frameCount = count;
    f.fonts = &fonts();
    pen.begin(*surface->getCanvas(), f);
  }
  sk_sp<SkSurface> surface;
  Pen pen;
};

void Circles(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    bench.pen.randomSeed(1);
    bench.pen.noStroke();
    bench.pen.fill(200, 80, 120);
    for (int i = 0; i < n; ++i)
      bench.pen.circle(bench.pen.random(640), bench.pen.random(480), 8);
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(Circles)->Arg(10000)->Unit(benchmark::kMillisecond);

void CirclesStroked(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    bench.pen.randomSeed(1);
    bench.pen.stroke(20);
    bench.pen.strokeWeight(2);
    bench.pen.fill(200, 80, 120);
    for (int i = 0; i < n; ++i)
      bench.pen.circle(bench.pen.random(640), bench.pen.random(480), 8);
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(CirclesStroked)->Arg(10000)->Unit(benchmark::kMillisecond);

void Rects(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    bench.pen.randomSeed(1);
    bench.pen.noStroke();
    bench.pen.rectMode(CENTER);
    for (int i = 0; i < n; ++i) {
      bench.pen.fill(bench.pen.random(255), 120, 200);
      bench.pen.rect(bench.pen.random(640), bench.pen.random(480), 10, 6);
    }
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(Rects)->Arg(10000)->Unit(benchmark::kMillisecond);

void Text(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    bench.pen.fill(255);
    bench.pen.textSize(14);
    for (int i = 0; i < n; ++i)
      bench.pen.text("frame rate steady", 20.0f + (float)(i % 4) * 150.0f,
                     20.0f + (float)(i / 4) * 18.0f);
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(Text)->Arg(100)->Unit(benchmark::kMillisecond);

void Background(benchmark::State& state) {
  Bench bench;
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    bench.pen.background(0, 20);
    bench.pen.end();
  }
}
BENCHMARK(Background)->Unit(benchmark::kMicrosecond);

void Noise(benchmark::State& state) {
  Pen pen;
  float acc = 0.0f;
  for (auto&& _ : state) {
    for (int i = 0; i < 1000; ++i)
      acc += pen.noise((float)i * 0.01f, (float)i * 0.003f);
  }
  benchmark::DoNotOptimize(acc);
  state.SetItemsProcessed(state.iterations() * 1000);
}
BENCHMARK(Noise)->Unit(benchmark::kMicrosecond);

}  // namespace

BENCHMARK_MAIN();
