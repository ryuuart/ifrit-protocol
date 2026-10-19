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
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
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

TEST(PostChain, ABloomKeepsWideningPastAFullSizeBlursReach) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  constexpr float kExposure = 0.1f;
  // A small patch bright enough that its light spread most of the frame
  // wide still reads above the picture's quantisation.
  const std::vector<Node> lamp = {
      mesh(gm::quad(6, 6))
          .key("lamp")
          .fill(material::from(material::Color{0, 0, 0, 1})
                    .surface({.metallic = 1.0f,
                              .emission = {1.0f, 1.0f, 1.0f, 1},
                              .emissionStrength = 4000.0f}))};
  const SkBitmap plain = photographed(lamp, on.runtime, kExposure);
  const Radiance light = lightOf(plain, kExposure);
  const int row = light.height / 2;
  // How far out along the row through the point the bloom adds light:
  // the root of the mean squared distance its added light stands at.
  const auto reach = [&](float radius) {
    const SkBitmap glowing = photographed(
        lamp, on.runtime, kExposure,
        {bloom({.threshold = 1.0f, .intensity = 1.0f, .radius = radius})});
    float sum = 0.0f;
    float moment = 0.0f;
    for (int x = 0; x < light.width; ++x) {
      const float d = (float)(x - light.width / 2);
      if (std::abs(d) < 8.0f) continue;
      const float shown =
          decoded(SkColor4f::FromColor(glowing.getColor(x, row)).fR) /
          kExposure;
      const float added = std::max(shown - light.at(x, row, 0), 0.0f);
      sum += added;
      moment += added * d * d;
    }
    return sum > 0 ? std::sqrt(moment / sum) : 0.0f;
  };
  // Both radii past what one blur at full size could spread.
  const float wide = reach(120.0f);
  const float wider = reach(400.0f);
  EXPECT_GT(wide, 10.0f);
  EXPECT_GT(wider, wide * 1.2f) << wide << " " << wider;
}

// ---- the device's clock --------------------------------------------------

namespace {

/** Renders @p frame until the scene reports a timed frame read, at most
 *  a dozen times, each followed by a readback that waits for the device. */
const SceneStats& timedUntilRead(Scene& scene, Frame& frame,
                                 double* longestMilliseconds) {
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
    *longestMilliseconds = std::max(*longestMilliseconds, ms);
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
  EXPECT_GT(stats.deviceMilliseconds(), 0.0);
  EXPECT_LE(stats.deviceMilliseconds(), longest);
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

TEST(PostChain, ASmoothPaneDerivesNoSmallerLevelsOfTheScene) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto stagesOf = [&](float roughness) {
    std::vector<Node> content = patchwork();
    content.push_back(mesh(gm::quad(60, 60))
                          .key("pane")
                          .at({0, 0, 20})
                          .fill(material::from(material::Color{1, 1, 1, 1})
                                    .surface({.roughness = roughness,
                                              .transmission = 1.0f,
                                              .ior = 1.5f})));
    content.push_back(camera(diligent::levelEye()).key("camera"));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent(kExtent)
        .runtime(on.runtime)
        .timeStages(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    motion::Engine engine;
    Scene scene(engine);
    double longest = 0.0;
    return SceneStats(timedUntilRead(scene, frame, &longest));
  };
  const SceneStats smooth = stagesOf(0.0f);
  if (smooth.deviceTiming == DeviceTiming::Unavailable)
    GTEST_SKIP() << "the device writes no timestamps";
  ASSERT_EQ(smooth.deviceTiming, DeviceTiming::Measured);
  EXPECT_TRUE(hasStage(smooth, "colour.colour-copy"));
  EXPECT_FALSE(hasStage(smooth, "colour.mips"))
      << "a smooth pane reads level 0";
  const SceneStats rough = stagesOf(0.6f);
  EXPECT_TRUE(hasStage(rough, "colour.mips")) << "a rough one reads below it";
}

TEST(PostChain, TheLaidPhaseCostsWhatIsSeenNotWhatIsDrawn) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The device time of the phase that lays the opaque bodies over, at a
  // full-size frame: the least of a few timed frames, so a frame the
  // device was slow to start does not decide it.
  // With @p castingElsewhere, a second light in the frame casts but is
  // linked to no card, so the cards' programs are built with the shadow
  // terms while no shadow falls on them.
  const auto laid = [&](int cards, bool shadowed, bool castingElsewhere) {
    std::vector<Node> content;
    for (int i = 0; i < cards; ++i)
      content.push_back(
          mesh(gm::quad(400, 400))
              .key("card" + std::to_string(i))
              .collection("cards")
              .at({0, 0, -2.0f * (float)i})
              .fill(material::surface::program(
                  {.baseColor = {0.5f, 0.5f, 0.5f, 1}, .roughness = 0.5f})));
    content.push_back(
        directionalLight({-0.2f, -0.3f, -1}, {.castShadow = shadowed})
            .key("sun"));
    if (castingElsewhere)
      content.push_back(
          directionalLight({0.2f, -0.3f, -1}, {.castShadow = true})
              .key("elsewhere")
              .lightLink(Collection{.exclude = {"cards"}})
              .shadowLink(Collection{.exclude = {"cards"}}));
    content.push_back(camera(diligent::levelEye()).key("camera"));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent({1440, 960})
        .runtime(on.runtime)
        .timeStages(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    motion::Engine engine;
    Scene scene(engine);
    double least = 1e9;
    bool measured = false;
    for (int k = 0; k < 10; ++k) {
      double longest = 0.0;
      const SceneStats stats = timedUntilRead(scene, frame, &longest);
      measured = measured || stats.deviceTiming == DeviceTiming::Measured;
      for (const DeviceStage& stage : stats.deviceStages)
        if (stage.stage == "colour.laid") least = std::min(least, stage.ms);
    }
    return measured ? least : -1.0;
  };
  // The first scene timed meets a device still raising its clocks, so it
  // is timed once to be thrown away.
  if (laid(1, false, false) < 0)
    GTEST_SKIP() << "the device writes no timestamps";
  const double one = laid(1, false, false);
  const double sixteen = laid(16, false, false);
  const double withTerms = laid(1, false, true);
  const double shadowed = laid(1, true, false);
  RecordProperty("oneCardMilliseconds", std::to_string(one));
  RecordProperty("sixteenCardsMilliseconds", std::to_string(sixteen));
  RecordProperty("oneCardWithShadowTermsMilliseconds",
                 std::to_string(withTerms));
  RecordProperty("oneShadowedCardMilliseconds", std::to_string(shadowed));
  // Sixteen cards over one another cost about what the one in front does:
  // the device does not shade what is hidden.
  EXPECT_LT(sixteen, one * 2.5) << one << " " << sixteen;
  // A card no light casts on, in a frame where nothing casts, is built
  // without the shadow terms and pays nothing for them.
  EXPECT_LT(one, withTerms) << one << " " << withTerms;
  // A casting light's filter — four comparing gathers a tap block — is
  // less than the cost of the lit pixel it is compiled into again.
  EXPECT_LT(shadowed, withTerms * 2.0) << withTerms << " " << shadowed;
}

TEST(PostChain, TheSeenPhaseOfAPaneCostsASmallMultipleOfALaidCard) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The device time of one phase at a full-size frame — the least of a few
  // timed frames — for an opaque card with, when @p pane is stated, a pane
  // covering the whole frame in front of it, under one light that casts
  // shadows when @p shadowed says.
  const auto timed = [&](std::optional<material::SurfaceOptions> pane,
                         const char* stage, bool shadowed) {
    std::vector<Node> content;
    content.push_back(
        mesh(gm::quad(400, 400))
            .key("card")
            .at({0, 0, -20})
            .fill(material::surface::program(
                {.baseColor = {0.5f, 0.5f, 0.5f, 1}, .roughness = 0.5f})));
    if (pane)
      content.push_back(
          mesh(gm::quad(400, 400))
              .key("pane")
              .fill(
                  material::from(material::Color{1, 1, 1, 1}).surface(*pane)));
    content.push_back(
        directionalLight({-0.2f, -0.3f, -1}, {.castShadow = shadowed})
            .key("sun"));
    content.push_back(camera(diligent::levelEye()).key("camera"));
    Frame frame(group(std::move(content)).key("set"));
    frame.extent({1440, 960})
        .runtime(on.runtime)
        .timeStages(true)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    motion::Engine engine;
    Scene scene(engine);
    double least = 1e9;
    bool measured = false;
    for (int k = 0; k < 10; ++k) {
      double longest = 0.0;
      const SceneStats stats = timedUntilRead(scene, frame, &longest);
      measured = measured || stats.deviceTiming == DeviceTiming::Measured;
      for (const DeviceStage& each : stats.deviceStages)
        if (each.stage == stage) least = std::min(least, each.ms);
    }
    return measured ? least : -1.0;
  };
  // The first scene timed meets a device still raising its clocks, so it
  // is timed once to be thrown away.
  if (timed(std::nullopt, "colour.laid", false) < 0)
    GTEST_SKIP() << "the device writes no timestamps";
  // Clear glass with a volume behind its face, so the eye's ray is carried
  // through it to the scene behind.
  const material::SurfaceOptions lens{
      .roughness = 0.0f, .transmission = 1.0f, .ior = 1.5f, .thickness = 10.0f};
  const double card = timed(std::nullopt, "colour.laid", false);
  const double pane = timed(lens, "colour.seen", false);
  const double shadowedCard = timed(std::nullopt, "colour.laid", true);
  const double shadowedPane = timed(lens, "colour.seen", true);
  RecordProperty("cardMilliseconds", std::to_string(card));
  RecordProperty("paneMilliseconds", std::to_string(pane));
  RecordProperty("shadowedCardMilliseconds", std::to_string(shadowedCard));
  RecordProperty("shadowedPaneMilliseconds", std::to_string(shadowedPane));
  // A pixel seen through — its lit face, the ray carried through the
  // volume and the scene behind read where it lands — costs a small
  // multiple of an opaque lit one.
  EXPECT_LT(pane, card * 3.0) << card << " " << pane;
}
