/** @file
 * The colour feature under load: the OKLab round trip per colour, the
 * ramp read in each space it can be walked in, and the table chosen from
 * a picture's worth of pixels.
 */

#include <benchmark/benchmark.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Dither.h>
#include <sigilmaterial/color/Extract.h>
#include <sigilmaterial/color/Ramp.h>

#include <vector>

using namespace sigil::material;

namespace {

void OklabRoundTrip(benchmark::State& state) {
  Color c{0.3f, 0.6f, 0.9f, 1};
  for ([[maybe_unused]] auto iteration : state) {
    c = fromOklab(toOklab(c));
    benchmark::DoNotOptimize(c);
  }
}
BENCHMARK(OklabRoundTrip);

/** One colour out of a ramp, per space. The arms differ by the walk
 *  between two stops alone — the domain, the bracket and the easing are
 *  the same work in all four — so the spread between them is the price
 *  of asking for a perceptual ramp rather than the sRGB one a gradient
 *  draws. */
void RampSample(benchmark::State& state) {
  Ramp ramp{.stops = {{0.0f, {0.05f, 0.02f, 0.2f, 1}},
                      {0.5f, {0.8f, 0.2f, 0.3f, 1}},
                      {1.0f, {1.0f, 0.95f, 0.6f, 1}}},
            .space = (RampSpace)state.range(0)};
  float t = 0.0f;
  for ([[maybe_unused]] auto iteration : state) {
    t = t < 1.0f ? t + 0.001f : 0.0f;
    benchmark::DoNotOptimize(ramp.at(t));
  }
}
BENCHMARK(RampSample)
    ->Arg((int)RampSpace::Srgb)
    ->Arg((int)RampSpace::Linear)
    ->Arg((int)RampSpace::Oklab)
    ->Arg((int)RampSpace::Oklch);

/** The threshold read at a pixel, which is the per-pixel cost of a
 *  dithered pass over a whole picture. */
void DitherThreshold(benchmark::State& state) {
  const Dither dither{.kind = (DitherKind)state.range(0)};
  int x = 0, y = 0;
  for ([[maybe_unused]] auto iteration : state) {
    x = (x + 1) & 1023;
    y = x == 0 ? (y + 1) & 1023 : y;
    benchmark::DoNotOptimize(dither.threshold(x, y));
  }
}
BENCHMARK(DitherThreshold)
    ->Arg((int)DitherKind::Ordered)
    ->Arg((int)DitherKind::Noise);

/** A table chosen from a thumbnail's worth of pixels — 128 x 128, which
 *  is the size the image crossing reads a picture down to. The argument
 *  is the number of entries, since the walk is one pass over every pixel
 *  per entry per iteration. */
void PaletteExtraction(benchmark::State& state) {
  std::vector<Color> pixels;
  pixels.reserve(128 * 128);
  for (int i = 0; i < 128 * 128; ++i) {
    const float u = (float)(i % 128) / 127.0f, v = (float)(i / 128) / 127.0f;
    pixels.push_back({u, v, 1.0f - u * v, 1.0f});
  }
  const PaletteOptions options{.entries = (int)state.range(0)};
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(palette(pixels, options));
}
BENCHMARK(PaletteExtraction)->Arg(4)->Arg(16);

}  // namespace
