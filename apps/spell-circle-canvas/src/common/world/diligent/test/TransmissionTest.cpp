/** @file
 * TRANSMISSION AND DISTORTION, held to what they mean on the device: a
 * glass slab bends what stands behind it sideways, further through a
 * thicker slab and a higher index and not at all at an index of one; a
 * rough one blurs it; a tinted one colours it more the more of it the
 * light crosses; dispersion parts the colours at an edge; a body that
 * states none of it is drawn as it was; and a distortion moves what is
 * behind by its map.
 *
 * Every scene stands in front of one backdrop — black on the left of the
 * picture, white on the right — so what is behind the glass is an edge
 * whose place and sharpness can be read off one row of pixels.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** @p content in front of the half-black, half-white backdrop, seen
 *  square on and photographed through a camera that reads light as it
 *  is. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      const EnvironmentMap* room = nullptr) {
  const auto unlit = [](float grey) {
    return material::from(material::Color{grey, grey, grey, 1})
        .surface({.unlit = true});
  };
  content.push_back(mesh(gm::quad(200, 300))
                        .key("black")
                        .at({-100, 0, -80})
                        .fill(unlit(0.0f)));
  content.push_back(mesh(gm::quad(200, 300))
                        .key("white")
                        .at({100, 0, -80})
                        .fill(unlit(1.0f)));
  content.push_back(camera(diligent::levelEye(),
                           {.exposure = 1.0f, .toneCurve = ToneCurve::Linear})
                        .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  if (room) frame.environment(*room, {.intensity = 0.6f, .diffuse = 0.08f});
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

/** Where along the middle row, in pixels, the picture first rises past
 *  half grey between columns @p from and @p to: the edge, to a fraction
 *  of a pixel. None when it never does. */
std::optional<float> edgeAt(const SkBitmap& plate, int from = 20, int to = 140,
                            int row = 80) {
  float before = luma(plate.getColor4f(from, row));
  for (int x = from + 1; x < to; ++x) {
    const float here = luma(plate.getColor4f(x, row));
    if (before < 0.5f && here >= 0.5f)
      return (float)(x - 1) + (0.5f - before) / std::max(here - before, 1e-6f);
    before = here;
  }
  return std::nullopt;
}

/** How many columns of the middle row stand between dark and light: the
 *  width of the edge. */
int edgeWidth(const SkBitmap& plate, int from = 20, int to = 140,
              int row = 80) {
  int width = 0;
  for (int x = from; x < to; ++x) {
    const float l = luma(plate.getColor4f(x, row));
    if (l > 0.1f && l < 0.8f) ++width;
  }
  return width;
}

/** Clear glass: white, smooth, transmitting all it does not reflect. */
material::SurfaceOptions glass(float ior, float thickness) {
  return {.roughness = 0.0f,
          .transmission = 1.0f,
          .ior = ior,
          .thickness = thickness};
}

/** A glass card standing in front of the backdrop's edge, turned so the
 *  eye meets it obliquely. */
Node slab(const material::SurfaceOptions& options, float turn = 35) {
  return mesh(gm::quad(90, 90))
      .key("glass")
      .rotateY(turn)
      .fill(material::from(material::Color{1, 1, 1, 1}).surface(options));
}

}  // namespace

TEST(Transmission, AGlassSlabShiftsTheEdgeByItsThicknessAndIndex) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto edge = [&](std::optional<material::SurfaceOptions> options) {
    std::vector<Node> content;
    if (options) content.push_back(slab(*options));
    const std::optional<float> found =
        edgeAt(photographed(content, on.runtime));
    EXPECT_TRUE(found.has_value());
    return found.value_or(0.0f);
  };
  const float bare = edge(std::nullopt);
  const float indexOne = edge(glass(1.0f, 40));
  const float thin = edge(glass(1.5f, 15));
  const float thick = edge(glass(1.5f, 40));
  const float denser = edge(glass(1.9f, 40));
  EXPECT_NEAR(indexOne, bare, 0.5f) << "an index of one bends nothing";
  EXPECT_GT(std::abs(thin - bare), 1.0f) << "glass moves the edge";
  EXPECT_GT(std::abs(thick - bare), std::abs(thin - bare) + 1.0f)
      << "a thicker slab moves it further";
  EXPECT_GT(std::abs(denser - bare), std::abs(thick - bare) + 0.5f)
      << "a higher index moves it further";
}

TEST(Transmission, FrostedGlassBlursTheEdge) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::SurfaceOptions clear = glass(1.5f, 0);
  material::SurfaceOptions frosted = clear;
  frosted.roughness = 0.6f;
  const int sharp = edgeWidth(photographed({slab(clear, 0)}, on.runtime));
  const int blurred = edgeWidth(photographed({slab(frosted, 0)}, on.runtime));
  EXPECT_LE(sharp, 3);
  EXPECT_GT(blurred, sharp + 4) << "a rough surface reads a blurrier level";
}

TEST(Transmission, TintedGlassColoursWhatIsBehindMoreThroughAThickerSlab) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto behind = [&](float thickness) {
    material::SurfaceOptions tinted = glass(1.5f, thickness);
    tinted.attenuationColor = {1.0f, 0.4f, 0.4f, 1};
    tinted.attenuationDistance = 20;
    // Face on, over the white half, well away from the edge.
    const SkBitmap plate = photographed(
        {mesh(gm::quad(60, 60))
             .key("glass")
             .at({40, 0, 0})
             .fill(
                 material::from(material::Color{1, 1, 1, 1}).surface(tinted))},
        on.runtime);
    return at(plate, 0.65f, 0.5f);
  };
  const SkColor4f thin = behind(5);
  const SkColor4f thick = behind(40);
  EXPECT_GT(thin.fR, 0.7f) << "red passes the red medium";
  EXPECT_LT(thin.fG, thin.fR) << "the medium takes green out";
  EXPECT_LT(thick.fG, thin.fG - 0.1f) << "a thicker slab takes more of it out";
  EXPECT_NEAR(thick.fR, thin.fR, 0.05f);
}

TEST(Transmission, TransmissionZeroIsTheSurfaceWithoutTheLobe) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::surface::SurfaceParameters plastic;
  plastic.baseColor = {0.7f, 0.3f, 0.2f, 1};
  plastic.roughness = 0.4f;
  const auto plate = [&](const material::Material& worn) {
    return photographed(
        {directionalLight({-0.3f, -0.4f, -1.0f}).key("sun"),
         mesh(gm::quad(90, 90)).key("card").rotateY(20).fill(worn)},
        on.runtime);
  };
  const SkBitmap without = plate(material::surface::program(plastic));
  const SkBitmap carried = plate(material::surface::program(
      plastic, material::surface::Reflection::SplitSum,
      {.transmission = true}));
  float worst = 0;
  for (int y = 0; y < kExtent.y; y += 4)
    for (int x = 0; x < kExtent.x; x += 4) {
      const SkColor4f a = without.getColor4f(x, y);
      const SkColor4f b = carried.getColor4f(x, y);
      worst = std::max({worst, std::abs(a.fR - b.fR), std::abs(a.fG - b.fG),
                        std::abs(a.fB - b.fB)});
    }
  EXPECT_LE(worst, 1.0f / 255.0f);
}

TEST(Transmission, ABodyStatingNoTransmissionIsUntouchedByTheSceneCopy) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const Node lamp = directionalLight({-0.3f, -0.4f, -1.0f}).key("sun");
  const Node matte =
      mesh(gm::quad(50, 50))
          .key("matte")
          .at({-45, 0, 0})
          .fill(material::surface::program(
              {.baseColor = {0.6f, 0.6f, 0.6f, 1}, .roughness = 0.8f}));
  const SkBitmap alone = photographed({lamp, matte}, on.runtime);
  const SkBitmap beside =
      photographed({lamp, matte,
                    mesh(gm::quad(40, 40))
                        .key("glass")
                        .at({50, 0, 0})
                        .fill(material::from(material::Color{1, 1, 1, 1})
                                  .surface(glass(1.5f, 20)))},
                   on.runtime);
  // Over the matte card's footprint and the black backdrop around it,
  // which the glass does not cover, the two pictures are the same bytes.
  for (int y = 50; y < 110; ++y)
    for (int x = 10; x < 70; ++x)
      ASSERT_EQ(alone.getColor(x, y), beside.getColor(x, y)) << x << "," << y;
}

TEST(Transmission, DispersionSeparatesTheChannelsAtAnEdge) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto parted = [&](float dispersion) {
    material::SurfaceOptions options = glass(1.5f, 40);
    options.dispersion = dispersion;
    const SkBitmap plate = photographed({slab(options)}, on.runtime);
    float most = 0;
    for (int x = 20; x < 140; ++x) {
      const SkColor4f c = plate.getColor4f(x, 80);
      most = std::max(most, std::abs(c.fR - c.fB));
    }
    return most;
  };
  EXPECT_LT(parted(0.0f), 0.05f) << "without dispersion the edge is grey";
  EXPECT_GT(parted(8.0f), 0.3f) << "with it the colours part";
}

TEST(Transmission, DistortionMovesWhatIsBehindByItsMapAndNotAtZeroStrength) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A map that says "across, a half" everywhere.
  const sk_sp<SkImage> push = [] {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    surface->getCanvas()->clear(SkColorSetARGB(255, 191, 128, 128));
    return surface->makeImageSnapshot();
  }();
  const auto edge = [&](float strength) {
    const SkBitmap plate = photographed(
        {mesh(gm::quad(100, 100))
             .key("haze")
             .fill(material::from(material::Color{1, 1, 1, 0})
                       .surface({.distortion =
                                     material::image(media::PixelSource(push)),
                                 .distortionStrength = strength}))},
        on.runtime);
    return edgeAt(plate).value_or(0.0f);
  };
  const float bare = edgeAt(photographed({}, on.runtime)).value_or(0.0f);
  EXPECT_NEAR(edge(0.0f), bare, 0.5f) << "no strength moves nothing";
  // What is seen at a pixel is what stands half the strength to its
  // right, so the edge moves left.
  EXPECT_LT(edge(20.0f), bare - 3.0f);
}

TEST(Transmission, ACurvedBodyAtIndexOneShowsWhatIsBehindAsAFlatOneDoes) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A dim room with one bright window, which a surface that reflected
  // anything would show over what is behind it, most at its rim.
  const EnvironmentMap room =
      kit::environmentMap({.zenith = {0.09f, 0.11f, 0.14f},
                           .nadir = {0.02f, 0.02f, 0.025f},
                           .softBoxes = {{.centre = {0.0f, 0.34f},
                                          .size = {0.12f, 0.08f},
                                          .radiance = {3.0f, 3.6f, 4.6f}}}});
  const auto clear =
      material::from(material::Color{1, 1, 1, 1}).surface(glass(1.0f, 0.0f));
  const SkBitmap bare = photographed({}, on.runtime, &room);
  // A ball standing on the backdrop's edge, over black and white alike,
  // and a card where it stands: through either the picture is the one
  // with neither, to its last level.
  const SkBitmap ball =
      photographed({mesh(gm::superellipsoid({30, 30, 30}, 2.0f, 64, 48))
                        .key("ball")
                        .fill(clear)},
                   on.runtime, &room);
  const SkBitmap card = photographed(
      {mesh(gm::quad(60, 60)).key("card").fill(clear)}, on.runtime, &room);
  EXPECT_LE(diligent::worstChannel(card, bare), 1)
      << "an index of one reflects nothing";
  EXPECT_LE(diligent::worstChannel(ball, bare), 1)
      << "curvature darkens and blurs nothing at index one";
}

TEST(Transmission, GlassBehindGlassShowsTheTintOfBoth) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::SurfaceOptions tinted = glass(1.5f, 5);
  tinted.attenuationColor = {0.3f, 1.0f, 0.3f, 1};
  tinted.attenuationDistance = 5;
  const auto pane = [](std::string key, float depth,
                       const material::SurfaceOptions& options) {
    return mesh(gm::quad(60, 60))
        .key(std::move(key))
        .at({40, 0, depth})
        .fill(material::from(material::Color{1, 1, 1, 1}).surface(options));
  };
  // Face on, over the white half: the green pane behind, a clear one in
  // front of it.
  const SkBitmap plate = photographed(
      {pane("green", -30, tinted), pane("clear", 20, glass(1.5f, 5))},
      on.runtime);
  const SkColor4f through = at(plate, 0.75f, 0.5f);
  EXPECT_GT(through.fG, 0.6f);
  EXPECT_LT(through.fR, through.fG - 0.3f)
      << "the clear pane shows the green one behind it";
}

TEST(Transmission,
     ABallLensTurnsWhatStandsFarBehindItRoundThroughBothSurfaces) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A glass ball as thick as it is wide, on the backdrop's edge: bent in
  // at its front and again out at its back, the rays cross before they
  // reach the backdrop, so the white half shows on the left of the ball
  // and the black half on its right.
  const SkBitmap plate =
      photographed({mesh(gm::superellipsoid({20, 20, 20}, 2.0f, 96, 64))
                        .key("lens")
                        .fill(material::from(material::Color{1, 1, 1, 1})
                                  .surface(glass(1.5f, 40)))},
                   on.runtime);
  const float left = luma(plate.getColor4f(72, 80));
  const float right = luma(plate.getColor4f(88, 80));
  EXPECT_GT(left, 0.6f);
  EXPECT_LT(right, 0.3f);
}

TEST(Transmission,
     ADistortionFollowsTheSurfaceAndBendsWhatATransmittingBodyPasses) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A map that says "a half, up the map's green" everywhere — on a dome
  // whose v runs from its crown out to its rim, a push along its radius.
  const sk_sp<SkImage> push = [] {
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    surface->getCanvas()->clear(SkColorSetARGB(255, 128, 191, 128));
    return surface->makeImageSnapshot();
  }();
  std::vector<glm::vec2> profile;
  for (int step = 0; step <= 24; ++step) {
    const float angle = 1.5707963f * (float)step / 24.0f;
    profile.push_back({30.0f * std::sin(angle), 10.0f * std::cos(angle)});
  }
  const gm::Mesh dome = gm::revolve(profile, {.segments = 64});
  const float bare = edgeAt(photographed({}, on.runtime)).value_or(0.0f);
  // How far the dome standing at @p across moves the backdrop's edge.
  const auto shift = [&](float across, bool transmits) {
    material::SurfaceOptions options{
        .distortion = material::image(media::PixelSource(push)),
        .distortionStrength = 16.0f};
    if (transmits) {
      options.transmission = 1.0f;
      options.ior = 1.0f;
      options.thickness = 0.0f;
      options.roughness = 0.0f;
    }
    const SkBitmap plate = photographed(
        {mesh(dome)
             .key("dome")
             .at({across, 0, 0})
             .rotateX(90)
             .fill(material::from(
                       material::Color{1, 1, 1, transmits ? 1.0f : 0.0f})
                       .surface(options))},
        on.runtime);
    return edgeAt(plate).value_or(bare) - bare;
  };
  for (const bool transmits : {false, true}) {
    // The edge under the dome's right side and under its left side moves
    // the opposite ways: the push is along the radius, not across the
    // picture.
    const float right = shift(-22, transmits);
    const float left = shift(22, transmits);
    EXPECT_GT(std::abs(right), 2.0f) << transmits;
    EXPECT_GT(std::abs(left), 2.0f) << transmits;
    EXPECT_LT(right * left, 0.0f) << transmits;
  }
}

TEST(Transmission, EachInstanceStatesItsOwnThickness) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::SurfaceOptions options = glass(1.5f, 1.0f);
  options.thicknessAttribute = "depth";
  const auto clear =
      material::from(material::Color{1, 1, 1, 1}).surface(options);
  const float bare = edgeAt(photographed({}, on.runtime)).value_or(0.0f);
  // One copy of a glass card, turned to the eye, whose instance says how
  // deep the glass is.
  const auto shift = [&](float depth) {
    gm::Instances copy;
    copy.stamp = gm::quad(90, 90);
    copy.positions.push_back({0, 0, 0});
    copy.attributes["depth"] = {depth};
    const SkBitmap plate = photographed(
        {mesh(copy).key("glass").rotateY(35).fill(clear)}, on.runtime);
    return std::abs(edgeAt(plate).value_or(bare) - bare);
  };
  const float thin = shift(5);
  const float thick = shift(40);
  EXPECT_GT(thin, 0.5f);
  EXPECT_GT(thick, thin + 3.0f) << "the deeper copy bends the edge further";
}

TEST(Transmission, AStackWhoseTopTransmitsSeesTheSceneBehindIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  material::surface::SurfaceParameters clear;
  clear.baseColor = {1, 1, 1, 1};
  clear.roughness = 0.0f;
  clear.transmission = 1.0f;
  clear.ior = 1.0f;
  const material::Material stack = material::over(
      material::surface::program(
          {.baseColor = {0.1f, 0.1f, 0.1f, 1}, .roughness = 0.8f}),
      material::surface::program(clear), material::maskConstant(1.0f));
  // Over the white half, the stack is the glass on top of it, which shows
  // the white standing behind.
  const SkBitmap plate = photographed(
      {mesh(gm::quad(60, 60)).key("stack").at({40, 0, 0}).fill(stack)},
      on.runtime);
  EXPECT_GT(luma(at(plate, 0.75f, 0.5f)), 0.8f);
}
