/** @file
 * Formation and painting cost of one representative mark per subject.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Brush.h>

#include <array>
#include <cmath>
#include <vector>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

void BrushStroke(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  Pen pen;
  Frame frame;
  frame.width = 640;
  frame.height = 480;
  const brush::Tool tool = brush::watercolor({0.12f, 0.36f, 0.72f, 1.0f}, 22.0f);
  const brush::Curl field(91);
  const brush::Stroke stroke =
      brush::trace(SkPoint::Make(30, 240), 560, tool.spacing, 0, field);
  int count = 0;
  for (auto _ : state) {
    frame.frameCount = ++count;
    pen.begin(*surface->getCanvas(), frame);
    pen.randomSeed(12);
    brush::paint(pen, tool, stroke);
    pen.end();
  }
}
BENCHMARK(BrushStroke)->Unit(benchmark::kMillisecond);

void DabSampling(benchmark::State& state) {
  std::array<brush::Input, 128> input;
  for (size_t index = 0; index < input.size(); ++index) {
    const float x = (float)index * 4.0f;
    input[index] = {.position = {x, 120.0f + std::sin(x * 0.03f) * 45.0f},
                    .pressure = 0.35f + 0.65f * (float)index / 127.0f,
                    .tilt = (float)index / 127.0f,
                    .barrelRotation = x * 0.01f,
                    .seconds = (double)index / 120.0,
                    .tiltDirection = x * 0.006f};
  }
  for (auto _ : state) {
    std::vector<brush::Dab> sampled = brush::dabs(input, 0.8f);
    benchmark::DoNotOptimize(sampled);
  }
}
BENCHMARK(DabSampling);

void BrushHatch(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  Pen pen;
  Frame frame{.width = 640, .height = 480};
  brush::Tool tool = brush::pencil({0.14f, 0.21f, 0.31f, 1.0f}, 2.0f);
  const std::array<SkPoint, 6> polygon{
      {{60, 160}, {210, 45}, {510, 75}, {590, 260}, {390, 445}, {95, 400}}};
  int count = 0;
  for (auto _ : state) {
    frame.frameCount = ++count;
    pen.begin(*surface->getCanvas(), frame);
    pen.randomSeed(29);
    brush::hatch(
        pen, tool, polygon,
        {.spacing = 7.0f, .angle = 0.48f, .jitter = 0.12f, .gradient = 0.18f});
    pen.end();
  }
}
BENCHMARK(BrushHatch)->Unit(benchmark::kMillisecond);

void BrushMass(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  Pen pen;
  Frame frame{.width = 640, .height = 480};
  brush::Catalogue catalogue = brush::Catalogue::stock();
  catalogue.scale(3.5f);
  brush::Tool tool = *catalogue.find("crayon");
  tool.color = {0.68f, 0.18f, 0.12f, 1.0f};
  const std::array<SkPoint, 6> polygon{
      {{60, 160}, {210, 45}, {510, 75}, {590, 260}, {390, 445}, {95, 400}}};
  int count = 0;
  for (auto _ : state) {
    frame.frameCount = ++count;
    pen.begin(*surface->getCanvas(), frame);
    pen.randomSeed(31);
    pen.noiseSeed(31);
    brush::mass(pen, tool, polygon,
                {.precision = 0.72f,
                 .strength = 0.72f,
                 .gradient = 0.3f,
                 .outline = true});
    pen.end();
  }
}
BENCHMARK(BrushMass)->Unit(benchmark::kMillisecond);

void BrushWash(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  Pen pen;
  Frame frame{.width = 640, .height = 480};
  const std::array<SkPoint, 6> polygon{
      {{80, 150}, {220, 55}, {490, 85}, {570, 250}, {410, 420}, {110, 390}}};
  int count = 0;
  for (auto _ : state) {
    frame.frameCount = ++count;
    pen.begin(*surface->getCanvas(), frame);
    pen.randomSeed(41);
    pen.noiseSeed(41);
    brush::wash(pen,
                {.color = {0.08f, 0.34f, 0.48f, 1.0f},
                 .opacity = 0.28f,
                 .bleed = 0.35f,
                 .texture = 0.45f,
                 .border = 0.4f,
                 .layers = 18},
                polygon);
    pen.end();
  }
}
BENCHMARK(BrushWash)->Unit(benchmark::kMillisecond);

}  // namespace

BENCHMARK_MAIN();
