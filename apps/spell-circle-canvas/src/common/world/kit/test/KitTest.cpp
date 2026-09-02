/** @file
 * What the presets compose: the tree each returns, the arrangement its
 * numbers put the lights and the camera in, and the one colour this
 * library states.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilworld/element/Node.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

const ElementNode& nodeOf(const Element& element) { return *element.node(); }

/** A lane's constant, for a preset that puts no motion on one. */
float constantOf(const motion::Animatable<float>& lane) {
  const float* plain = lane.plain();
  return plain ? *plain : 0.0f;
}

const ElementNode* childOf(const Element& element, const std::string& key) {
  for (const Element& child : nodeOf(element).children)
    if (child.node()->key == key) return child.node().get();
  return nullptr;
}

/** The emitter a keyed child of @p element carries, or null. Reached
 *  as a pointer so a case that asserts on one says so first. */
const Light* lightOf(const Element& element, const std::string& key) {
  const ElementNode* node = childOf(element, key);
  return node && node->light ? &*node->light : nullptr;
}

/** …and the surface, on the same terms. */
const material::Material* surfaceOf(const Element& element,
                                    const std::string& key) {
  const ElementNode* node = childOf(element, key);
  return node && node->material ? &*node->material : nullptr;
}

/** …and the ride along a curve. */
const Along* alongOf(const Element& element) {
  const ElementNode& node = nodeOf(element);
  return node.along ? &*node.along : nullptr;
}

std::vector<std::string> keysOf(const Element& element) {
  std::vector<std::string> keys;
  for (const Element& child : nodeOf(element).children)
    keys.push_back(child.node()->key);
  return keys;
}

TEST(WorldKit, TheRigIsThreeKeyedEmitters) {
  const Element rig = kit::threePoint();
  EXPECT_EQ(nodeOf(rig).key, "rig");
  EXPECT_EQ(keysOf(rig), (std::vector<std::string>{"key", "fill", "back"}));

  const Light* key = lightOf(rig, "key");
  const Light* fill = lightOf(rig, "fill");
  const Light* back = lightOf(rig, "back");
  ASSERT_NE(key, nullptr);
  ASSERT_NE(fill, nullptr);
  ASSERT_NE(back, nullptr);

  // The key is what the subject is read by, and the other two are what
  // keep it from being read by the key alone.
  EXPECT_GT(key->intensity, fill->intensity);
  EXPECT_GT(key->intensity, back->intensity);
  // …and none of them carries a surface, a mesh or a viewpoint: a rig
  // lights a set, it does not furnish one.
  EXPECT_EQ(surfaceOf(rig, "key"), nullptr);
  EXPECT_FALSE(childOf(rig, "key")->camera.has_value());
}

TEST(WorldKit, TheRigIsStatedInTheSubjectsOwnExtents) {
  kit::Rig small;
  small.extent = 10.0f;
  kit::Rig large;
  large.extent = 1000.0f;
  const Element close = kit::threePoint(small);
  const Element wide = kit::threePoint(large);
  ASSERT_NE(lightOf(close, "key"), nullptr);
  ASSERT_NE(lightOf(wide, "key"), nullptr);
  const glm::vec3 near = lightOf(close, "key")->position;
  const glm::vec3 far = lightOf(wide, "key")->position;
  // One arrangement, two sizes: the far lamp stands a hundred times out.
  EXPECT_NEAR(glm::length(far), glm::length(near) * 100.0f, 1e-2f);

  // Turning the bearing turns the whole rig and nothing else.
  kit::Rig turned;
  turned.bearing = small.bearing + 180.0f;
  turned.extent = small.extent;
  const Element other = kit::threePoint(turned);
  ASSERT_NE(lightOf(other, "key"), nullptr);
  const glm::vec3 opposite = lightOf(other, "key")->position;
  EXPECT_NEAR(opposite.x, -near.x, 1e-3f);
  EXPECT_NEAR(opposite.z, -near.z, 1e-3f);
  EXPECT_NEAR(opposite.y, near.y, 1e-3f);
}

TEST(WorldKit, TheTurntableRidesOneClosedRail) {
  kit::Turntable table;
  const Spline3 track = kit::rail(table);
  EXPECT_TRUE(track.closed);
  EXPECT_EQ((int)track.points.size(), table.stations);

  const Element start = kit::turntable(table, 0.0f);
  EXPECT_EQ(nodeOf(start).key, "camera");
  EXPECT_TRUE(nodeOf(start).camera.has_value());
  ASSERT_NE(alongOf(start), nullptr);
  EXPECT_FLOAT_EQ(constantOf(alongOf(start)->distance), 0.0f);

  // A whole turn is where it started, and half of one is halfway along.
  const Element round = kit::turntable(table, table.period);
  ASSERT_NE(alongOf(round), nullptr);
  EXPECT_NEAR(constantOf(alongOf(round)->distance), 0.0f, 1e-3f);
  const Element half = kit::turntable(table, table.period * 0.5f);
  ASSERT_NE(alongOf(half), nullptr);
  EXPECT_NEAR(constantOf(alongOf(half)->distance), track.length() * 0.5f,
              1e-2f);

  // A parked table stands at its first station whatever the clock says.
  kit::Turntable parked = table;
  parked.period = 0.0f;
  const Element still = kit::turntable(parked, 4.0f);
  ASSERT_NE(alongOf(still), nullptr);
  EXPECT_FLOAT_EQ(constantOf(alongOf(still)->distance), 0.0f);
}

TEST(WorldKit, TheSetIsAGroundARigACameraAndTheSubject) {
  const Element set =
      kit::litSet(Element().key("body").mesh(geometry::mesh::quad(10, 10)));
  EXPECT_EQ(nodeOf(set).key, "set");
  EXPECT_EQ(keysOf(set),
            (std::vector<std::string>{"ground", "rig", "camera", "subject"}));

  const material::Material* ground = surfaceOf(set, "ground");
  ASSERT_NE(ground, nullptr);
  // ONE COLOUR, and it is the ground's: nothing else here states a look.
  const glm::vec4 grey = ground->get<glm::vec4>("baseColor");
  EXPECT_NEAR(grey.r, grey.g, 1e-3f);
  EXPECT_GT(grey.r, 0.0f);
  EXPECT_LT(grey.r, 0.5f);

  // A ground the caller made is the ground.
  kit::Set own;
  own.surface =
      ::sigil::material::kit::surface({.baseColor = {0.9f, 0.1f, 0.1f, 1.0f}});
  const Element painted = kit::litSet(Element().key("body"), own);
  ASSERT_NE(surfaceOf(painted, "ground"), nullptr);
  EXPECT_FLOAT_EQ(surfaceOf(painted, "ground")->get<glm::vec4>("baseColor").r,
                  0.9f);

  // …and a set with no ground has none.
  kit::Set bare;
  bare.ground = 0.0f;
  EXPECT_EQ(keysOf(kit::litSet(Element().key("body"), bare)),
            (std::vector<std::string>{"rig", "camera", "subject"}));
}

}  // namespace
