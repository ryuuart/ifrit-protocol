/** @file
 * A TRIANGLE'S OWN COLOUR ON THE DEVICE, held to the pixels: a body that
 * names a primitive colour lane shows each triangle in its own colour,
 * though the two triangles of a card share two corners; a body that names
 * none, or a lane the mesh does not carry, is untinted; and the same
 * triangles drawn both ways in one scene keep each packing apart.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>

#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** @p content seen square on, read linearly. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime) {
  content.push_back(camera(diligent::levelEye(),
                           {.toneCurve = ToneCurve::Linear})
                        .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

/** A card of two triangles, the first red and the second green on its
 *  "Color" lane. */
gm::Mesh card() {
  gm::Mesh triangles = gm::quad(120, 120);
  std::vector<glm::vec4>& colour = triangles.primitive("Color");
  colour[0] = {1, 0, 0, 1};
  colour[1] = {0, 1, 0, 1};
  return triangles;
}

/** A surface drawn as authored: white, so what shows is the tint. */
material::Material white() {
  return material::surface::unlit({.baseColor = {1, 1, 1, 1}});
}

/** How many pixels of @p plate read as mostly red, mostly green and
 *  white. */
struct Census {
  int red = 0;
  int green = 0;
  int white = 0;
};

Census census(const SkBitmap& plate) {
  Census counted;
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      const SkColor4f pixel = plate.getColor4f(x, y);
      if (pixel.fR > 0.8f && pixel.fG < 0.2f && pixel.fB < 0.2f) ++counted.red;
      if (pixel.fG > 0.8f && pixel.fR < 0.2f && pixel.fB < 0.2f)
        ++counted.green;
      if (pixel.fR > 0.8f && pixel.fG > 0.8f && pixel.fB > 0.8f)
        ++counted.white;
    }
  return counted;
}

}  // namespace

TEST(PrimitiveColor, EachTriangleWearsItsOwnColour) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const Census counted = census(photographed(
      {mesh(card()).key("card").fill(white()).primitiveColor("Color")},
      on.runtime));
  // The card covers well over a thousand pixels, and each half is one of
  // the two triangles.
  EXPECT_GT(counted.red, 1000) << "the first triangle is red";
  EXPECT_GT(counted.green, 1000) << "the second triangle is green";
  EXPECT_EQ(counted.white, 0) << "no corner is left untinted";
}

TEST(PrimitiveColor, ABodyNamingNoLaneOrAMissingOneIsUntinted) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const Census unnamed = census(
      photographed({mesh(card()).key("card").fill(white())}, on.runtime));
  EXPECT_EQ(unnamed.red + unnamed.green, 0);
  EXPECT_GT(unnamed.white, 2000);

  const Census missing = census(photographed(
      {mesh(card()).key("card").fill(white()).primitiveColor("Absent")},
      on.runtime));
  EXPECT_EQ(missing.red + missing.green, 0);
  EXPECT_GT(missing.white, 2000);
}

TEST(PrimitiveColor, OneMeshTintedAndUntintedInOneSceneKeepsBoth) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Equal meshes are one cooked artefact, so the two bodies below ask the
  // device for the same triangles packed two ways in one frame.
  gm::Mesh small = card();
  small.transform(glm::mat4(glm::vec4(0.4f, 0, 0, 0), glm::vec4(0, 0.4f, 0, 0),
                            glm::vec4(0, 0, 1, 0), glm::vec4(0, 0, 0, 1)));
  const Census counted = census(photographed(
      {mesh(small).key("tinted").at({-40, 0, 0}).fill(white()).primitiveColor(
           "Color"),
       mesh(small).key("plain").at({40, 0, 0}).fill(white())},
      on.runtime));
  EXPECT_GT(counted.red, 100);
  EXPECT_GT(counted.green, 100);
  EXPECT_GT(counted.white, 300);
}
