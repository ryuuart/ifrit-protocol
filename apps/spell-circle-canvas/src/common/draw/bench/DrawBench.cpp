/** @file
 * What a frame of the pen costs: ten thousand circles, filled and
 * stroked, ten thousand rects, fourteen thousand segments and points
 * drawn one verb at a time and as one shape, a screen of text, a
 * translucent background laid in place and replayed from a recording,
 * and a stack of layers composited with a few of them redrawn, on raster
 * and on the device.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <sigildraw/Pen.h>
#include <sigildraw/PenTypes.h>
#include <sigildraw/Stack.h>
#include <sigilgpu/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Fonts.h"

namespace {

using namespace sigil::draw;
using sigil::test::fonts;

/** A pen over a raster canvas the size of a modest sketch. */
struct Bench {
  Bench() : surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480))) {}
  explicit Bench(sk_sp<SkSurface> on) : surface(std::move(on)) {}
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

/** The segments a dense flow field lays down each frame, one per pair
 *  of points, at the stroke such a field draws them in. */
void segmentStyle(Pen& pen) {
  pen.randomSeed(1);
  pen.noFill();
  pen.stroke(255, 190);
  pen.strokeWeight(0.55f);
  pen.strokeCap(SQUARE);
}

/** One `line` per segment: a draw, and a paint resolved, per pair. */
void LinesOneAtATime(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    segmentStyle(bench.pen);
    for (int i = 0; i < n; ++i) {
      const float x = bench.pen.random(640);
      const float y = bench.pen.random(480);
      bench.pen.line(x, y, x + 14.0f, y + 6.0f);
    }
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(LinesOneAtATime)->Arg(14000)->Unit(benchmark::kMillisecond);

/** The same segments as one `beginShape(LINES)` run. */
void LinesAsOneShape(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    segmentStyle(bench.pen);
    bench.pen.beginShape(LINES);
    for (int i = 0; i < n; ++i) {
      const float x = bench.pen.random(640);
      const float y = bench.pen.random(480);
      bench.pen.vertex(x, y);
      bench.pen.vertex(x + 14.0f, y + 6.0f);
    }
    bench.pen.endShape();
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(LinesAsOneShape)->Arg(14000)->Unit(benchmark::kMillisecond);

/** One `point` per dot, at a weight that makes each a small disc. */
void PointsOneAtATime(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    segmentStyle(bench.pen);
    bench.pen.strokeWeight(3.0f);
    for (int i = 0; i < n; ++i)
      bench.pen.point(bench.pen.random(640), bench.pen.random(480));
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(PointsOneAtATime)->Arg(14000)->Unit(benchmark::kMillisecond);

/** The same dots as one `beginShape(POINTS)` run. */
void PointsAsOneShape(benchmark::State& state) {
  Bench bench;
  const int n = (int)state.range(0);
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    segmentStyle(bench.pen);
    bench.pen.strokeWeight(3.0f);
    bench.pen.beginShape(POINTS);
    for (int i = 0; i < n; ++i)
      bench.pen.vertex(bench.pen.random(640), bench.pen.random(480));
    bench.pen.endShape();
    bench.pen.end();
  }
  state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(PointsAsOneShape)->Arg(14000)->Unit(benchmark::kMillisecond);

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

/** The same translucent ground replayed from a recording, which carries
 *  the fade as a blender run per pixel rather than the table the pen
 *  applies to pixels it holds. */
void BackgroundReplayed(benchmark::State& state) {
  Bench bench;
  SkPictureRecorder recorder;
  on(*recorder.beginRecording(SkRect::MakeWH(640, 480)), {640, 480},
     [](Pen& pen) { pen.background(0, 20); });
  const sk_sp<SkPicture> fade = recorder.finishRecordingAsPicture();
  for (auto&& _ : state) bench.surface->getCanvas()->drawPicture(fade);
}
BENCHMARK(BackgroundReplayed)->Unit(benchmark::kMicrosecond);

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

/** A frame of a layer stack: N layers at the bench's canvas size, k of
 *  them redrawn this frame, every third one blended and every fifth one
 *  masked by the layer beneath it, then the stack put down. With no layer
 *  redrawn the frame is one image drawn. */
void stackFrames(benchmark::State& state, Bench& bench,
                 const std::function<void()>& finished) {
  const int layers = (int)state.range(0);
  const int dirty = (int)state.range(1);
  Stack stack{640, 480};
  std::vector<Layer*> made;
  bench.frame(0);
  for (int index = 0; index < layers; ++index) {
    Layer& layer = stack.layer("layer " + std::to_string(index));
    Pen& pen = layer.begin(bench.pen);
    pen.noStroke();
    pen.fill(40 + 20 * index, 120, 200, 180);
    pen.circle(320 + 10 * index, 240, 300);
    layer.end();
    if (index % 3 == 2) layer.blendMode(SCREEN);
    if (index % 5 == 4) layer.mask(*made.back());
    made.push_back(&layer);
  }
  stack.composite(bench.pen);
  bench.pen.end();
  finished();
  int count = 0;
  for (auto&& _ : state) {
    bench.frame(++count);
    for (int index = 0; index < dirty; ++index) {
      Layer& layer = *made[(count + index) % layers];
      Pen& pen = layer.begin(bench.pen);
      pen.fill(255, 255, 255, 40);
      pen.circle(pen.random(640), pen.random(480), 40);
      layer.end();
    }
    stack.composite(bench.pen);
    bench.pen.end();
    finished();
  }
  state.SetItemsProcessed(state.iterations() * layers);
}

void StackComposite(benchmark::State& state) {
  Bench bench;
  stackFrames(state, bench, [] {});
}
BENCHMARK(StackComposite)
    ->Args({8, 0})
    ->Args({8, 1})
    ->Args({8, 8})
    ->Args({32, 0})
    ->Args({32, 1})
    ->Unit(benchmark::kMicrosecond);

/** The same frames on the device: every layer and the composite formed
 *  on the device of the pen they are begun under, each frame submitted
 *  and waited for, so a frame's time is the device's work as well as the
 *  recording. Skipped where the machine has no device of its own. */
void StackCompositeDevice(benchmark::State& state) {
  static auto device = sigil::gpu::GpuDevice::createOwned().value_or(nullptr);
  static auto context = device
                            ? sigil::skia::GraphiteContext::create(*device)
                            : std::unique_ptr<sigil::skia::GraphiteContext>();
  if (!context) {
    state.SkipWithError("no device of the machine's own");
    return;
  }
  Bench bench(SkSurfaces::RenderTarget(context->recorder(),
                                       SkImageInfo::MakeN32Premul(640, 480)));
  if (!bench.surface) {
    state.SkipWithError("the device made no surface");
    return;
  }
  stackFrames(state, bench, [] {
    (void)context->submit(*context->recorder(),
                          skgpu::graphite::SyncToCpu::kYes);
  });
}
BENCHMARK(StackCompositeDevice)
    ->Args({8, 0})
    ->Args({8, 1})
    ->Args({8, 8})
    ->Args({32, 0})
    ->Args({32, 1})
    ->Unit(benchmark::kMicrosecond);

}  // namespace
