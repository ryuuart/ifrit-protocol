// An author's explicit `Cache::Texture` bake against the same drawing
// unbaked, at one moment and a fractional capture density: a turning node
// takes no device-aligned bake, so its pixels are resampled onto the
// capture's grid, and the difference that resampling leaves stays within
// the bound stated here.
//
// The bound stands beside the bake rather than with the plate tiers: the
// device tier holds each plate to its own earlier bytes and the raster
// smoke reads one rasteriser against the other, and neither ever draws a
// drawing both with and without its bakes. What a bake may change is a
// fact about the bake.

#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "support/Host.h"
#include "support/Rows.h"

namespace {

namespace brush = sigil::draw::brush;
using sigil::draw::Pen;

/** The density a plate is captured at in these cases: a view that puts no
 *  texel of a canvas unit on a whole device pixel. */
constexpr float kCaptureDensity = 1.6667f;

/** HOW FAR A BAKED DRAWING MAY READ FROM ITSELF UNBAKED, per colour
 *  channel in 0..255 over the whole capture: the mean absolute
 *  difference, and the value ninety-nine channels in a hundred stay
 *  under. A bake resamples its texels once more than the drawing it holds
 *  is sampled, which softens an edge by up to a texel and leaves every
 *  interior as it was, so the mean stays small however many edges the
 *  drawing has and the tail is the edges themselves. */
constexpr float kBakedMeanBound = 2.0f;
constexpr int kBakedP99Bound = 48;

/** How one capture reads against another, per colour channel. */
struct ChannelDistance {
  double mean = 0;
  int p99 = 0;
  int max = 0;
};

ChannelDistance distanceBetween(const Host& reference, const Host& candidate) {
  const SkBitmap referencePixels = reference.pixels(),
                 candidatePixels = candidate.pixels();
  std::vector<int> differences;
  differences.reserve((size_t)referencePixels.width() *
                      (size_t)referencePixels.height() * 3);
  for (int y = 0; y < referencePixels.height(); ++y)
    for (int x = 0; x < referencePixels.width(); ++x) {
      const SkColor left = referencePixels.getColor(x, y),
                    right = candidatePixels.getColor(x, y);
      differences.push_back(
          std::abs((int)SkColorGetR(left) - (int)SkColorGetR(right)));
      differences.push_back(
          std::abs((int)SkColorGetG(left) - (int)SkColorGetG(right)));
      differences.push_back(
          std::abs((int)SkColorGetB(left) - (int)SkColorGetB(right)));
    }
  ChannelDistance distance;
  if (differences.empty()) return distance;
  double sum = 0;
  for (const int difference : differences) sum += difference;
  distance.mean = sum / (double)differences.size();
  std::sort(differences.begin(), differences.end());
  distance.p99 = differences[(differences.size() * 99) / 100];
  distance.max = differences.back();
  return distance;
}

/** Captures @p host's tree as a plate is captured: bakes taken at the
 *  capture density and the canvas drawn under it. */
void capture(Host& host, double seconds = 0.0) {
  if (seconds > 0)
    host.engine.advance(host.engine.elapsed() +
                        sigil::motion::Duration(seconds));
  SkCanvas* canvas = host.surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  canvas->save();
  canvas->scale(kCaptureDensity, kCaptureDensity);
  host.composer.draw(*canvas);
  canvas->restore();
}

/** Twelve words on a circle, turned about its centre by @p turn of a
 *  revolution, held under @p mode. */
Element turningRing(Cache mode, const sigil::motion::Animatable<float>& turn) {
  const float side = 360.0f, radius = 150.0f;
  Element ring = box()
                     .absolute()
                     .left(12)
                     .top(12)
                     .width(side)
                     .height(side)
                     .cache(mode)
                     .transformOrigin(percent(50), percent(50))
                     .rotate(sigil::motion::bind(turn, {.to = {0.0f, 360.0f}}));
  const char8_t* words[] = {u8"ANIMA", u8"LUMEN", u8"ORDO",  u8"SIGNUM",
                            u8"VOX",   u8"NOMEN", u8"CIRCU", u8"TERRA",
                            u8"AQUA",  u8"IGNIS", u8"AER",   u8"SAL"};
  for (int index = 0; index < 12; ++index) {
    const float angle = (float)index * (float)(2 * M_PI) / 12.0f;
    ring.children({text(words[index], whiteStyle(15))
                       .absolute()
                       .left(side * 0.5f + radius * std::cos(angle) - 32.0f)
                       .top(side * 0.5f + radius * std::sin(angle) - 10.0f)
                       .width(64)});
  }
  return box().cache(Cache::None).children({std::move(ring)});
}

/** A fibre stroke dealt from one seed, laid once and held under @p mode. */
Element fibreStroke(Cache mode) {
  return box()
      .cache(Cache::None)
      .children({pen(
                     "fibre",
                     [](Pen& pen) {
                       brush::Tool fibres =
                           brush::watercolor({0.92f, 0.38f, 0.22f, 1}, 26);
                       fibres.blend = sigil::draw::BLEND;
                       fibres.opacity = 0.6f;
                       pen.randomSeed(1220);
                       brush::paint(pen, fibres,
                                    brush::segment({24, 40}, {330, 150}));
                       brush::paint(pen, fibres,
                                    brush::segment({40, 170}, {300, 60}));
                     },
                     mode)
                     .absolute()
                     .left(10.5f)
                     .top(14.25f)
                     .width(360)
                     .height(200)});
}

/** The ring held at one turn, where no texel of its bake lands on a whole
 *  device pixel. */
Element ringAtOneTurn(Cache mode) {
  static const sigil::motion::Animatable<float> turn =
      sigil::motion::animatable(0.137f);
  return turningRing(mode, turn);
}

/** A drawing taken with `Cache::Texture` and read against itself drawn
 *  under @p unbaked, on a canvas of @p width by @p height units. */
struct ResampledDrawing {
  const char* name;
  float width, height;
  Element (*drawing)(Cache mode);
  Cache unbaked;
};

}  // namespace

// An explicit bake of a turning drawing, captured at a density that puts no
// texel on a whole pixel, reads from the drawing unbaked within the stated
// mean and ninety-ninth percentile per channel.
TEST(BakeResampling, ATurningBakeReadsAsItsDrawingWithinTheStatedBound) {
  const ResampledDrawing rows[] = {
      {"ATurningRing", 390, 390, ringAtOneTurn, Cache::Auto},
      {"ASeededFibreStroke", 390, 230, fibreStroke, Cache::None},
  };
  sigil::compose::test::forEachRow(rows, [](const ResampledDrawing& row) {
    const int width = (int)std::ceil(row.width * kCaptureDensity),
              height = (int)std::ceil(row.height * kCaptureDensity);
    Host baked(width, height), drawn(width, height);
    for (Host* host : {&baked, &drawn}) {
      host->composer.setSize({row.width, row.height});
      host->composer.setBakeDensity(kCaptureDensity);
    }
    baked.composer.render(row.drawing(Cache::Texture));
    drawn.composer.render(row.drawing(row.unbaked));
    capture(baked);
    capture(drawn);
    ASSERT_GT(baked.composer.stats().texturesBaked, 0u)
        << "the drawing took no bake";
    const ChannelDistance distance = distanceBetween(drawn, baked);
    EXPECT_LE(distance.mean, kBakedMeanBound)
        << "the baked drawing read " << distance.mean
        << " from itself on average";
    EXPECT_LE(distance.p99, kBakedP99Bound)
        << "the baked drawing's edges read " << distance.p99
        << " from themselves at the ninety-ninth percentile";
  });
}
