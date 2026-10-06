/** @file
 * WHAT A POST PASS MAY CHANGE BY TAKING ITS SPREAD SMALL, held to the
 * pixels: a wide blur, taken at a reduced level and read back, is the
 * Gaussian of the same width taken at full size within a few levels; a
 * bloom of one bright point spreads it as far as the same Gaussians at
 * full size would. And what the device's clock reports: every stage of a
 * timed frame, named, together no longer than the frame took, with no
 * copy of the scene where nothing reads one.
 *
 * Each reference is computed on the host from the unblurred picture the
 * same device drew, recovered to the light it stands for, so the cases
 * compare the device's chain against the exact operation rather than
 * against the device's own former output.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Stats.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** A picture's light, one float a channel, row by row. */
struct Radiance {
  int width = 0;
  int height = 0;
  std::vector<float> rgb;
  float& at(int x, int y, int c) {
    return rgb[((size_t)y * width + x) * 3 + c];
  }
  [[nodiscard]] float at(int x, int y, int c) const {
    return rgb[((size_t)y * width + x) * 3 + c];
  }
};

float decoded(float code) {
  return code <= 0.04045f ? code / 12.92f
                          : std::pow((code + 0.055f) / 1.055f, 2.4f);
}
float encoded(float light) {
  const float x = std::clamp(light, 0.0f, 1.0f);
  return x <= 0.0031308f ? x * 12.92f
                         : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

/** The light @p plate shows, at an exposure of @p exposure and no curve. */
Radiance lightOf(const SkBitmap& plate, float exposure) {
  Radiance light{
      plate.width(), plate.height(),
      std::vector<float>((size_t)plate.width() * plate.height() * 3)};
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      const SkColor4f c = SkColor4f::FromColor(plate.getColor(x, y));
      light.at(x, y, 0) = decoded(c.fR) / exposure;
      light.at(x, y, 1) = decoded(c.fG) / exposure;
      light.at(x, y, 2) = decoded(c.fB) / exposure;
    }
  return light;
}

/** The exact separable Gaussian of @p sigma pixels over @p light, three
 *  sigma wide, normalised over the taps taken, the edge held. */
Radiance gaussian(const Radiance& light, float sigma) {
  const int radius = (int)std::ceil(sigma * 3.0f);
  std::vector<float> weights((size_t)radius + 1);
  for (int i = 0; i <= radius; ++i)
    weights[(size_t)i] = std::exp(-(float)(i * i) / (2.0f * sigma * sigma));
  const auto pass = [&](const Radiance& from, bool across) {
    Radiance into = from;
    for (int y = 0; y < from.height; ++y)
      for (int x = 0; x < from.width; ++x)
        for (int c = 0; c < 3; ++c) {
          float sum = 0.0f;
          float total = 0.0f;
          for (int i = -radius; i <= radius; ++i) {
            const int sx = across ? std::clamp(x + i, 0, from.width - 1) : x;
            const int sy = across ? y : std::clamp(y + i, 0, from.height - 1);
            const float w = weights[(size_t)std::abs(i)];
            sum += from.at(sx, sy, c) * w;
            total += w;
          }
          into.at(x, y, c) = sum / total;
        }
    return into;
  };
  return pass(pass(light, true), false);
}

/** The worst difference, in eight-bit levels of the display encoding,
 *  between @p plate and @p light shown at @p exposure. */
int worstAgainst(const SkBitmap& plate, const Radiance& light, float exposure,
                 int border = 0) {
  int worst = 0;
  for (int y = border; y < plate.height() - border; ++y)
    for (int x = border; x < plate.width() - border; ++x) {
      const SkColor4f c = SkColor4f::FromColor(plate.getColor(x, y));
      const float shown[3] = {c.fR, c.fG, c.fB};
      for (int k = 0; k < 3; ++k) {
        const float expected = encoded(light.at(x, y, k) * exposure);
        worst = std::max(
            worst, (int)std::lround(std::abs(shown[k] - expected) * 255.0f));
      }
    }
  return worst;
}

/** @p content photographed through @p passes at @p exposure, no curve. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      float exposure, std::vector<Pass> passes = {}) {
  content.push_back(
      camera(diligent::levelEye(),
             {.exposure = exposure, .toneCurve = ToneCurve::Linear})
          .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).runtime(runtime).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  for (Pass& pass : passes) frame.pass(std::move(pass));
  motion::Engine engine;
  Scene scene(engine);
  scene.render(frame);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  draw(scene, canvas);
  return bitmap;
}

/** A surface drawn as authored, in @p colour. */
material::Material flat(material::Color colour) {
  return material::from(colour).surface({.unlit = true});
}

/** A plate with edges and corners of several colours, which a blur
 *  softens everywhere. */
std::vector<Node> patchwork() {
  return {mesh(gm::quad(60, 60))
              .key("a")
              .at({-30, 20, 0})
              .fill(flat({0.9f, 0.3f, 0.1f, 1})),
          mesh(gm::quad(40, 80))
              .key("b")
              .at({35, -10, 1})
              .fill(flat({0.1f, 0.7f, 0.9f, 1})),
          mesh(gm::quad(8, 90))
              .key("c")
              .at({0, 0, 2})
              .fill(flat({1.0f, 1.0f, 1.0f, 1}))};
}

/** One point brighter than white, and the camera exposed so it is not
 *  clipped. */
std::vector<Node> spark() {
  return {mesh(gm::quad(2, 2))
              .key("spark")
              .fill(material::from(material::Color{0, 0, 0, 1})
                        .surface({.metallic = 1.0f,
                                  .emission = {1.0f, 0.8f, 0.6f, 1},
                                  .emissionStrength = 6.0f}))};
}

}  // namespace

// ---- a blur taken small --------------------------------------------------

TEST(PostChain, AWideBlurTakenSmallIsTheFullSizeGaussianWithinAFewLevels) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Twelve pixels: wide enough to be taken at a quarter of the size.
  constexpr float kSigma = 12.0f;
  const SkBitmap plain = photographed(patchwork(), on.runtime, 1.0f);
  const SkBitmap blurred = photographed(
      patchwork(), on.runtime, 1.0f,
      {postPass("soft").reads("colour").writes("soft").blur(kSigma)});
  const Radiance exact = gaussian(lightOf(plain, 1.0f), kSigma);
  // THE RULE: a blur's footprint is honoured at the level it is taken
  // at, so the picture may differ from the full-size Gaussian by what the
  // smaller level cannot resolve — a few levels at an edge, and nothing a
  // reader would see.
  //
  // Away from the picture's edge that is within a few levels; within one
  // texel of the level of the edge, where the level holds its edge texel
  // over four pixels where the full-size blur holds one, a few more.
  EXPECT_LE(worstAgainst(blurred, exact, 1.0f, /*border=*/4), 5);
  EXPECT_LE(worstAgainst(blurred, exact, 1.0f), 12);
  // …and it is a blur: the hard edges are gone.
  EXPECT_GT(diligent::worstChannel(plain, blurred), 40);
}

TEST(PostChain, ANarrowBlurIsStillTakenAtFullSize) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr float kSigma = 2.0f;
  const SkBitmap plain = photographed(patchwork(), on.runtime, 1.0f);
  const SkBitmap blurred = photographed(
      patchwork(), on.runtime, 1.0f,
      {postPass("soft").reads("colour").writes("soft").blur(kSigma)});
  EXPECT_LE(worstAgainst(blurred, gaussian(lightOf(plain, 1.0f), kSigma), 1.0f),
            2);
}

// ---- a bloom taken small -------------------------------------------------

TEST(PostChain, ABloomOfOneBrightPointSpreadsAsFarAsAtFullSize) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr float kExposure = 0.1f;
  constexpr Bloom kGlow{.threshold = 1.0f, .intensity = 1.0f, .radius = 48};
  const SkBitmap plain = photographed(spark(), on.runtime, kExposure);
  const SkBitmap glowing =
      photographed(spark(), on.runtime, kExposure, {bloom(kGlow)});

  // THE BLOOM AT FULL SIZE, on the host: what passes the threshold,
  // spread to a ninth of the radius, that spread again by a third of it,
  // both added at half the intensity.
  const Radiance light = lightOf(plain, kExposure);
  Radiance bright = light;
  for (int y = 0; y < light.height; ++y)
    for (int x = 0; x < light.width; ++x) {
      const float l = 0.2126f * light.at(x, y, 0) +
                      0.7152f * light.at(x, y, 1) + 0.0722f * light.at(x, y, 2);
      const float kept = l > 0.0f ? std::max(l - kGlow.threshold, 0.0f) / l : 0;
      for (int c = 0; c < 3; ++c) bright.at(x, y, c) *= kept;
    }
  const Radiance near = gaussian(bright, kGlow.radius / 9.0f);
  const Radiance far = gaussian(near, kGlow.radius / 3.5f);
  Radiance expected = light;
  for (size_t i = 0; i < expected.rgb.size(); ++i)
    expected.rgb[i] += 0.5f * kGlow.intensity * (near.rgb[i] + far.rgb[i]);

  // The tail along the row through the point: how far out it still adds
  // light, against how far out the exact bloom does.
  const int row = light.height / 2;
  const auto reach = [&](auto shown) {
    float sum = 0.0f;
    float moment = 0.0f;
    for (int x = 0; x < light.width; ++x) {
      const float d = (float)(x - light.width / 2);
      if (std::abs(d) < 3.0f) continue;
      const float v = shown(x);
      sum += v;
      moment += v * d * d;
    }
    return sum > 0 ? std::sqrt(moment / sum) : 0.0f;
  };
  const float exactReach = reach(
      [&](int x) { return expected.at(x, row, 0) - light.at(x, row, 0); });
  const float deviceReach = reach([&](int x) {
    const float shown =
        decoded(SkColor4f::FromColor(glowing.getColor(x, row)).fR) / kExposure;
    return std::max(shown - light.at(x, row, 0), 0.0f);
  });
  ASSERT_GT(exactReach, 5.0f);
  EXPECT_NEAR(deviceReach, exactReach, exactReach * 0.15f);
  EXPECT_LE(worstAgainst(glowing, expected, kExposure), 8);
}

// ---- the device's clock --------------------------------------------------

namespace {

/** Renders @p frame until the scene reports a timed frame read, at most
 *  a dozen times, each followed by a readback that waits for the device. */
const SceneStats& timedUntilRead(Scene& scene, Frame& frame,
                                 double* longestMs) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(bitmap);
  for (int i = 0; i < 12; ++i) {
    const auto began = std::chrono::steady_clock::now();
    scene.render(frame);
    draw(scene, canvas);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - began)
                          .count();
    *longestMs = std::max(*longestMs, ms);
    if (scene.stats().deviceTiming != DeviceTiming::Pending) break;
  }
  return scene.stats();
}

bool hasStage(const SceneStats& stats, const std::string& name) {
  return std::any_of(
      stats.deviceStages.begin(), stats.deviceStages.end(),
      [&](const DeviceStage& stage) { return stage.stage == name; });
}

}  // namespace

TEST(PostChain, ATimedFrameNamesEveryStageAndTheyFitInsideTheFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  std::vector<Node> content = patchwork();
  content.push_back(camera(diligent::levelEye()).key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent)
      .runtime(on.runtime)
      .timeStages(true)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack))
      .pass(postPass("soft").reads("colour").writes("soft").blur(12.0f));
  motion::Engine engine;
  Scene scene(engine);
  double longest = 0.0;
  const SceneStats& stats = timedUntilRead(scene, frame, &longest);
  if (stats.deviceTiming == DeviceTiming::Unavailable)
    GTEST_SKIP() << "the device writes no timestamps";
  ASSERT_EQ(stats.deviceTiming, DeviceTiming::Measured);
  EXPECT_TRUE(hasStage(stats, "colour.shadow"));
  EXPECT_TRUE(hasStage(stats, "colour.laid"));
  EXPECT_TRUE(hasStage(stats, "soft"));
  EXPECT_TRUE(hasStage(stats, "readback"));
  for (const DeviceStage& stage : stats.deviceStages)
    EXPECT_GE(stage.ms, 0.0) << stage.stage;
  EXPECT_GE(stats.deviceStagesFrame, 0);
  // THE STAGES ARE THE FRAME: together more than nothing, and no more
  // than the longest frame took on the host, which waited for the device
  // to finish it before reading it back.
  EXPECT_GT(stats.deviceMs(), 0.0);
  EXPECT_LE(stats.deviceMs(), longest);
}

TEST(PostChain, AnUntimedFrameReportsNoStages) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  std::vector<Node> content = patchwork();
  content.push_back(camera(diligent::levelEye()).key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent)
      .runtime(on.runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  motion::Engine engine;
  Scene scene(engine);
  scene.render(frame);
  scene.render(frame);
  EXPECT_EQ(scene.stats().deviceTiming, DeviceTiming::Off);
  EXPECT_TRUE(scene.stats().deviceStages.empty());
}

TEST(PostChain, AFrameNothingReadsTheSceneFromCopiesNothing) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto stagesOf = [&](std::vector<Node> content) {
    content.push_back(camera(diligent::levelEye()).key("camera"));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent(kExtent)
        .runtime(on.runtime)
        .timeStages(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    motion::Engine engine;
    Scene scene(engine);
    double longest = 0.0;
    // A STILL FRAME, drawn again and again with nothing in it changed.
    return SceneStats(timedUntilRead(scene, frame, &longest));
  };
  const SceneStats still = stagesOf(patchwork());
  if (still.deviceTiming == DeviceTiming::Unavailable)
    GTEST_SKIP() << "the device writes no timestamps";
  ASSERT_EQ(still.deviceTiming, DeviceTiming::Measured);
  EXPECT_FALSE(hasStage(still, "colour.colour-copy"));
  EXPECT_FALSE(hasStage(still, "colour.mips"));
  EXPECT_FALSE(hasStage(still, "colour.depth-copy"));

  // A card that fades into the scene reads the depth, and only it.
  std::vector<Node> fading = patchwork();
  fading.push_back(
      mesh(gm::quad(60, 60))
          .key("haze")
          .at({0, 0, 10})
          .fill(material::from(material::Color{0.9f, 0.1f, 0.1f, 1})
                    .surface({.unlit = true, .depthFade = 30.0f})));
  const SceneStats faded = stagesOf(std::move(fading));
  ASSERT_EQ(faded.deviceTiming, DeviceTiming::Measured);
  std::string named;
  for (const DeviceStage& stage : faded.deviceStages)
    named += stage.stage + " ";
  EXPECT_TRUE(hasStage(faded, "colour.depth-copy")) << named;
  EXPECT_FALSE(hasStage(faded, "colour.mips"));
}
