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
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilworld/World.h>

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
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime) {
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
