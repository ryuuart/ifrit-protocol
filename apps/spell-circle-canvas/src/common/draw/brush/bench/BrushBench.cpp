/** @file
 * Formation and painting cost of one representative mark per subject.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <sigildraw/Draw.h>
#include <sigildraw/brush/Brush.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

namespace brush = sigil::draw::brush;
using namespace sigil::draw;

/** A soft round tip drawn once: what an imported brush's artwork is,
 *  without a file in the benchmark. */
sk_sp<SkImage> softTip(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side, true);
  const float centre = (float)side * 0.5f;
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x) {
      const float distance =
          std::hypot((float)x + 0.5f - centre, (float)y + 0.5f - centre) /
          centre;
      const float coverage = std::clamp(1.0f - distance, 0.0f, 1.0f);
      const uint32_t alpha = (uint32_t)(coverage * coverage * 255.0f);
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(alpha, 255, 255, 255);
    }
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

/** A tiled noise the grain is. */
sk_sp<SkImage> noiseTile(int side) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(side, side, true);
  uint32_t state = 0x9e3779b9u;
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x) {
      state = state * 1664525u + 1013904223u;
      const uint32_t level = 96u + (state >> 24) % 160u;
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(255, level, level, level);
    }
  bitmap.setImmutable();
  return SkImages::RasterFromBitmap(bitmap);
}

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

/** One stroke of an imported brush: an image stamped per dab, with and
 *  without the grain that is the second image over it. */
void ShapeStroke(benchmark::State& state) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(640, 480));
  Pen pen;
  Frame frame;
  frame.width = 640;
  frame.height = 480;
  brush::Tool tool = brush::marker({0.1f, 0.1f, 0.12f, 1.0f}, 28.0f);
  tool.tip = brush::Tip::Image;
  tool.shape = brush::Shape{.image = softTip(64),
                            .mask = brush::ImageMask::Alpha,
                            .spacing = 0.12f,
                            .scatter = 0.08f,
                            .angleJitter = 0.4f};
  if (state.range(0) != 0)
    tool.grain = brush::Grain{
        .image = noiseTile(128),
        .space = state.range(0) == 1 ? brush::GrainSpace::Stroke
                                     : brush::GrainSpace::Dab,
        .scale = 1.5f};
  const brush::Stroke stroke = brush::segment({30, 240}, {610, 300},
                                              brush::spacingOf(tool));
  int count = 0;
  for (auto _ : state) {
    frame.frameCount = ++count;
    pen.begin(*surface->getCanvas(), frame);
    pen.randomSeed(12);
    brush::paint(pen, tool, stroke);
    pen.end();
  }
}
BENCHMARK(ShapeStroke)->Arg(0)->Arg(1)->Arg(2)->Unit(benchmark::kMillisecond);

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
