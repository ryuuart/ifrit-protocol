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
  const SkBitmap a = reference.pixels(), b = candidate.pixels();
  std::vector<int> differences;
  differences.reserve((size_t)a.width() * (size_t)a.height() * 3);
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) {
      const SkColor left = a.getColor(x, y), right = b.getColor(x, y);
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
                     .transformOrigin(pct(50), pct(50))
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
  return box().cache(Cache::None).children(
      {pen("fibre",
           [](Pen& pen) {
             brush::Tool fibres = brush::watercolor({0.92f, 0.38f, 0.22f, 1}, 26);
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

}  // namespace

TEST(BakeResampling, ATurningRingsBakeReadsAsTheRingWithinTheStatedBound) {
  const int width = (int)std::ceil(390 * kCaptureDensity),
            height = (int)std::ceil(390 * kCaptureDensity);
  sigil::motion::Animatable<float> turn = sigil::motion::animatable(0.137f);
  Host baked(width, height), drawn(width, height);
  for (Host* host : {&baked, &drawn}) {
    host->composer.setSize({390, 390});
    host->composer.setBakeDensity(kCaptureDensity);
  }
  baked.composer.render(turningRing(Cache::Texture, turn));
  drawn.composer.render(turningRing(Cache::Auto, turn));
  capture(baked);
  capture(drawn);
  ASSERT_GT(baked.composer.stats().texturesBaked, 0u) << "the ring took no bake";
  const ChannelDistance distance = distanceBetween(drawn, baked);
  EXPECT_LE(distance.mean, kBakedMeanBound)
      << "the baked ring read " << distance.mean << " from itself on average";
  EXPECT_LE(distance.p99, kBakedP99Bound)
      << "the baked ring's edges read " << distance.p99
      << " from themselves at the ninety-ninth percentile";
}

TEST(BakeResampling, ASeededFibreStrokesBakeReadsAsTheStrokeWithinTheStatedBound) {
  const int width = (int)std::ceil(390 * kCaptureDensity),
            height = (int)std::ceil(230 * kCaptureDensity);
  Host baked(width, height), drawn(width, height);
  for (Host* host : {&baked, &drawn}) {
    host->composer.setSize({390, 230});
    host->composer.setBakeDensity(kCaptureDensity);
  }
  baked.composer.render(fibreStroke(Cache::Texture));
  drawn.composer.render(fibreStroke(Cache::None));
  capture(baked);
  capture(drawn);
  ASSERT_GT(baked.composer.stats().texturesBaked, 0u)
      << "the stroke took no bake";
  const ChannelDistance distance = distanceBetween(drawn, baked);
  EXPECT_LE(distance.mean, kBakedMeanBound)
      << "the baked stroke read " << distance.mean << " from itself on average";
  EXPECT_LE(distance.p99, kBakedP99Bound)
      << "the baked stroke's edges read " << distance.p99
      << " from themselves at the ninety-ninth percentile";
}
