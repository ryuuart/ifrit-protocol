/** @file
 * INSTANCED GEOMETRY ON THE DEVICE, held to the pixels: one stamp drawn
 * once per instance, each where its placement puts it; each instance's
 * colour and named scalars reaching the surface it wears, a flipbook
 * among them; and each instance turning to face the camera where it
 * stands.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Instance.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilworld/World.h>

#include <cmath>
#include <cstdint>
#include <memory>
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

/** @p content seen from @p eye, read linearly. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      gm::camera::Camera eye = diligent::levelEye()) {
  content.push_back(
      camera(eye, {.toneCurve = ToneCurve::Linear}).key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

/** A surface drawn as authored: white, multiplied by each instance's
 *  colour on the way in. */
material::Material white() {
  return material::from(material::Color{1, 1, 1, 1}).surface({.unlit = true});
}

/** A surface drawn as authored whose colour is the Slang @p expression. */
material::Material reading(const std::string& name,
                           const std::string& expression) {
  struct Nothing {};
  auto recipe = std::make_shared<const material::Recipe>(
      material::Recipe::of<Nothing>("world.test.instance." + name)
          .body(material::Target::Slang,
                "float4 surface(float2 uv) { return " + expression + "; }\n"));
  return material::from(material::Material(recipe)).surface({.unlit = true});
}

/** @p count copies of a square of @p size, along x @p step apart and
 *  centred on the origin. */
gm::Instances row(int count, float step, float size = 20) {
  gm::Instances out;
  out.stamp = gm::quad(size, size);
  for (int i = 0; i < count; ++i)
    out.positions.emplace_back(((float)i - (float)(count - 1) / 2) * step, 0,
                               0);
  return out;
}

/** The runs of lit pixels along the middle row of @p plate. */
int runsAcross(const SkBitmap& plate) {
  int runs = 0;
  bool inside = false;
  for (int x = 0; x < plate.width(); ++x) {
    const bool lit = SkColorGetR(plate.getColor(x, plate.height() / 2)) > 128 ||
                     SkColorGetB(plate.getColor(x, plate.height() / 2)) > 128;
    if (lit && !inside) ++runs;
    inside = lit;
  }
  return runs;
}

/** A 2 x 2 sheet of four colours, a cell of four texels each, read row by
 *  row: red, green, blue, white. */
material::Texture sheet() {
  static const uint8_t colours[4][3] = {
      {220, 30, 30}, {30, 200, 40}, {40, 60, 230}, {240, 240, 240}};
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::Make(8, 8, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      const int cell = (y / 4) * 2 + (x / 4);
      auto* texel = static_cast<uint8_t*>(bitmap.getAddr(x, y));
      texel[0] = colours[cell][0];
      texel[1] = colours[cell][1];
      texel[2] = colours[cell][2];
      texel[3] = 255;
    }
  bitmap.setImmutable();
  return material::Texture(media::PixelSource(bitmap.asImage()));
}

}  // namespace

TEST(Instancing, EveryInstanceStandsWhereItsPlacementPutsIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate =
      photographed({mesh(row(3, 50)).key("row").fill(white())}, on.runtime);
  EXPECT_EQ(runsAcross(plate), 3) << "three copies, three runs";
  EXPECT_GT(at(plate, 0.5f, 0.5f).fR, 0.9f) << "the middle one at the origin";
  EXPECT_LT(at(plate, 0.5f, 0.2f).fR, 0.05f) << "and nothing above it";

  // Turned and scaled: a copy scaled to nothing is not drawn, and one
  // turned a quarter about y shows its edge.
  gm::Instances placed = row(3, 50);
  placed.scales = {{1, 1, 1}, {0, 0, 0}, {1, 1, 1}};
  EXPECT_EQ(runsAcross(photographed({mesh(placed).key("row").fill(white())},
                                    on.runtime)),
            2);
  placed.scales.clear();
  placed.orientations = {
      glm::quat(1, 0, 0, 0),
      glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)),
      glm::quat(1, 0, 0, 0)};
  EXPECT_EQ(
      runsAcross(photographed({mesh(placed).key("row").fill(white()).backface(
                                  material::Backface::Visible)},
                              on.runtime)),
      2);
}

TEST(Instancing, EachInstanceCarriesItsColourToTheSurface) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::Instances two = row(2, 60, 30);
  two.colors = {{1, 0, 0, 1}, {0, 0, 1, 1}};
  // The program returns the instance's colour itself, so what shows is
  // what reached the shader.
  const SkBitmap plate = photographed(
      {mesh(two).key("pair").fill(reading("colour", "instanceColor()"))},
      on.runtime);
  const SkColor4f left = at(plate, 0.31f, 0.5f);
  const SkColor4f right = at(plate, 0.69f, 0.5f);
  EXPECT_GT(left.fR, 0.9f);
  EXPECT_LT(left.fB, 0.05f);
  EXPECT_GT(right.fB, 0.9f);
  EXPECT_LT(right.fR, 0.05f);
}

TEST(Instancing, ANamedScalarReachesTheSurfaceByItsName) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::Instances two = row(2, 60, 30);
  two.attributes["age"] = {0.25f, 1.0f};
  two.attributes["phase"] = {1.0f, 0.0f};
  const float age = material::instanceAttributeKey("age");
  const std::string read = "instanceAttribute(" + std::to_string(age) + ")";
  const SkBitmap plate = photographed(
      {mesh(two).key("pair").fill(reading(
          "age", "float4(" + read + ", " + read + ", " + read + ", 1.0)"))},
      on.runtime);
  // Drawn as authored and read linearly, a grey of a quarter is a quarter.
  EXPECT_NEAR(at(plate, 0.31f, 0.5f).fR, 0.25f, 2.0f / 255);
  EXPECT_NEAR(at(plate, 0.69f, 0.5f).fR, 1.0f, 2.0f / 255);
}

TEST(Instancing, AFlipbookShowsEachInstanceItsOwnFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::Instances two = row(2, 60, 30);
  two.attributes["cell"] = {1.0f, 2.0f};
  const material::Material book =
      material::from(material::flipbook(sheet(), {.columns = 2,
                                                  .rows = 2,
                                                  .framesPerSecond = 0,
                                                  .frameAttribute = "cell"}))
          .surface({.unlit = true});
  const SkBitmap plate =
      photographed({mesh(two).key("pair").fill(book)}, on.runtime);
  const SkColor4f left = at(plate, 0.31f, 0.5f);
  const SkColor4f right = at(plate, 0.69f, 0.5f);
  EXPECT_GT(left.fG, left.fR) << "the second cell, green";
  EXPECT_GT(left.fG, left.fB);
  EXPECT_GT(right.fB, right.fR) << "the third cell, blue";
  EXPECT_GT(right.fB, right.fG);
}

TEST(Instancing, EachInstanceFacesTheCameraWhereItStands) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::camera::Camera side = diligent::levelEye();
  side.eye = glm::normalize(glm::vec3(200.0f * std::sin(0.9f), 70.0f,
                                      200.0f * std::cos(0.9f))) *
             200.0f;
  const auto covered = [](const SkBitmap& plate) {
    int count = 0;
    for (int y = 0; y < plate.height(); ++y)
      for (int x = 0; x < plate.width(); ++x)
        if (SkColorGetR(plate.getColor(x, y)) > 128) ++count;
    return count;
  };
  const auto sprites = [](Facing facing) {
    return std::vector<Node>{
        mesh(row(2, 40, 30)).key("sprites").facing(facing).fill(white())};
  };
  const int ahead = covered(photographed(sprites(Facing::Camera), on.runtime));
  const int aside =
      covered(photographed(sprites(Facing::Camera), on.runtime, side));
  EXPECT_GT(ahead, 500);
  EXPECT_NEAR(aside, ahead, ahead / 10) << "square on from both places";
  const int unturned =
      covered(photographed(sprites(Facing::None), on.runtime, side));
  EXPECT_LT(unturned, ahead * 9 / 10)
      << "and foreshortened when they do not turn";
}
