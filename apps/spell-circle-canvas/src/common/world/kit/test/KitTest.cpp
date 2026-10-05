/** @file
 * What the presets compose: the tree each returns, the arrangement its
 * numbers put the lights and the camera in, and the one colour this
 * library states.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Node.h>

#include <cmath>
#include <string>
#include <vector>

using namespace sigil;
using namespace sigil::world;

namespace {

const NodeDescription& nodeOf(const Node& node) { return *node.node(); }

/** A lane's constant, for a preset that puts no motion on one. */
float constantOf(const motion::Animatable<float>& lane) {
  const float* constant = lane.constant();
  return constant ? *constant : 0.0f;
}

const NodeDescription* childOf(const Node& node, const std::string& key) {
  for (const Node& child : nodeOf(node).children)
    if (child.node()->key == key) return child.node().get();
  return nullptr;
}

/** The light a keyed child of @p node carries, or null. Reached as a
 *  pointer so a case that asserts on one says so first. */
const LightDescription* lightOf(const Node& node, const std::string& key) {
  const NodeDescription* child = childOf(node, key);
  return child && child->light ? &*child->light : nullptr;
}

/** Where a keyed child of @p node stands, read off its translation. */
glm::vec3 positionOf(const Node& node, const std::string& key) {
  const NodeDescription* child = childOf(node, key);
  if (!child) return glm::vec3(0.0f);
  return {constantOf(child->transform.translateX),
          constantOf(child->transform.translateY),
          constantOf(child->transform.translateZ)};
}

/** …and the surface, on the same terms. */
const material::Material* surfaceOf(const Node& node, const std::string& key) {
  const NodeDescription* child = childOf(node, key);
  return child && child->material ? &*child->material : nullptr;
}

/** …and the ride along a curve. */
const Along* alongOf(const Node& node) {
  const NodeDescription& described = nodeOf(node);
  return described.along ? &*described.along : nullptr;
}

std::vector<std::string> keysOf(const Node& node) {
  std::vector<std::string> keys;
  for (const Node& child : nodeOf(node).children)
    keys.push_back(child.node()->key);
  return keys;
}

TEST(WorldKit, TheRigIsThreeKeyedLights) {
  const Group rig = kit::threePoint();
  EXPECT_EQ(nodeOf(rig).key, "rig");
  EXPECT_EQ(keysOf(rig), (std::vector<std::string>{"key", "fill", "back"}));

  const LightDescription* key = lightOf(rig, "key");
  const LightDescription* fill = lightOf(rig, "fill");
  const LightDescription* back = lightOf(rig, "back");
  ASSERT_NE(key, nullptr);
  ASSERT_NE(fill, nullptr);
  ASSERT_NE(back, nullptr);

  // The key is what the subject is read by, and the other two are what
  // keep it from being read by the key alone.
  EXPECT_GT(key->intensity.value(), fill->intensity.value());
  EXPECT_GT(key->intensity.value(), back->intensity.value());
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
  const Group close = kit::threePoint(small);
  const Group wide = kit::threePoint(large);
  ASSERT_NE(lightOf(close, "key"), nullptr);
  ASSERT_NE(lightOf(wide, "key"), nullptr);
  const glm::vec3 near = positionOf(close, "key");
  const glm::vec3 far = positionOf(wide, "key");
  // One arrangement, two sizes: the far lamp stands a hundred times out.
  EXPECT_NEAR(glm::length(far), glm::length(near) * 100.0f, 1e-2f);

  // Turning the bearing turns the whole rig and nothing else.
  kit::Rig turned;
  turned.bearing = small.bearing + 180.0f;
  turned.extent = small.extent;
  const Group other = kit::threePoint(turned);
  ASSERT_NE(lightOf(other, "key"), nullptr);
  const glm::vec3 opposite = positionOf(other, "key");
  EXPECT_NEAR(opposite.x, -near.x, 1e-3f);
  EXPECT_NEAR(opposite.z, -near.z, 1e-3f);
  EXPECT_NEAR(opposite.y, near.y, 1e-3f);
}

TEST(WorldKit, TheTurntableRidesOneClosedRail) {
  kit::Turntable table;
  const geometry::mesh::curve::Spline3 track = kit::rail(table);
  EXPECT_TRUE(track.closed);
  EXPECT_EQ((int)track.points.size(), table.stations);

  const Camera start = kit::turntable(table, motion::Duration{});
  EXPECT_EQ(nodeOf(start).key, "camera");
  EXPECT_TRUE(nodeOf(start).camera.has_value());
  ASSERT_NE(alongOf(start), nullptr);
  EXPECT_FLOAT_EQ(constantOf(alongOf(start)->distance), 0.0f);

  // A whole turn is where it started, and half of one is halfway along.
  const Camera round = kit::turntable(table, table.period);
  ASSERT_NE(alongOf(round), nullptr);
  EXPECT_NEAR(constantOf(alongOf(round)->distance), 0.0f, 1e-3f);
  const Camera half = kit::turntable(table, table.period * 0.5);
  ASSERT_NE(alongOf(half), nullptr);
  EXPECT_NEAR(constantOf(alongOf(half)->distance), track.length() * 0.5f,
              1e-2f);

  // A parked table stands at its first station whatever the clock says.
  kit::Turntable parked = table;
  parked.period = motion::Duration{};
  const Camera still = kit::turntable(parked, motion::Duration(4.0));
  ASSERT_NE(alongOf(still), nullptr);
  EXPECT_FLOAT_EQ(constantOf(alongOf(still)->distance), 0.0f);
}

TEST(WorldKit, TheSetIsAGroundARigACameraAndTheSubject) {
  const Group set = kit::litSet(mesh(geometry::mesh::quad(10, 10)).key("body"));
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
  own.surface = ::sigil::material::surface::program(
      {.baseColor = {0.9f, 0.1f, 0.1f, 1.0f}});
  const Group painted = kit::litSet(group().key("body"), own);
  ASSERT_NE(surfaceOf(painted, "ground"), nullptr);
  EXPECT_FLOAT_EQ(surfaceOf(painted, "ground")->get<glm::vec4>("baseColor").r,
                  0.9f);

  // …and a set with no ground has none.
  kit::Set bare;
  bare.ground = 0.0f;
  EXPECT_EQ(keysOf(kit::litSet(group().key("body"), bare)),
            (std::vector<std::string>{"rig", "camera", "subject"}));
}

TEST(WorldKit, ARigWithNoExtentStandsAtTheSubject) {
  // Every distance in the arrangement is a multiple of the extent, so a
  // subject nothing across puts all three lamps where it stands. The
  // tree is still three keyed lights: a degenerate number narrows the
  // arrangement rather than dropping a light out of it.
  kit::Rig flat;
  flat.extent = 0.0f;
  const Group rig = kit::threePoint(flat);
  EXPECT_EQ(keysOf(rig), (std::vector<std::string>{"key", "fill", "back"}));
  for (const std::string& key : {"key", "fill", "back"}) {
    ASSERT_NE(lightOf(rig, key), nullptr) << key;
    EXPECT_NEAR(glm::length(positionOf(rig, key)), 0.0f, 1e-4f) << key;
  }
}

TEST(WorldKit, ARailIsAClosedLoopHoweverFewStationsItIsAskedFor) {
  // A rail is a closed loop drawn through its stations, and fewer than
  // three points is not a loop — so the rail stands at three rather than
  // handing back a curve a camera cannot ride.
  kit::Turntable few;
  few.stations = 1;
  const geometry::mesh::curve::Spline3 track = kit::rail(few);
  EXPECT_TRUE(track.closed);
  EXPECT_EQ((int)track.points.size(), 3);
  EXPECT_GT(track.length(), 0.0f);

  kit::Wave fewWave;
  fewWave.knots = 2;
  EXPECT_EQ((int)kit::wave(fewWave).points.size(), 3);
  EXPECT_TRUE(kit::wave(fewWave).closed);
  kit::Winding fewWinding;
  fewWinding.knots = 0;
  EXPECT_EQ((int)kit::winding(fewWinding).points.size(), 3);
  EXPECT_TRUE(kit::winding(fewWinding).closed);
}

TEST(WorldKit, AWaveAlternatesBetweenTwoRadiiAndTwoHeightsRoundItsCentre) {
  kit::Wave shape;
  shape.at = {10.0f, 20.0f, 30.0f};
  shape.radius = 200.0f;
  shape.inner = 120.0f;
  shape.high = 70.0f;
  shape.low = -50.0f;
  const geometry::mesh::curve::Spline3 loop = kit::wave(shape);
  EXPECT_TRUE(loop.closed);
  EXPECT_EQ(loop.type, geometry::mesh::curve::Spline3::Type::CatmullRom);
  ASSERT_EQ((int)loop.points.size(), shape.knots);
  for (int i = 0; i < shape.knots; ++i) {
    const glm::vec3 station = loop.points[(size_t)i] - shape.at;
    const bool outer = i % 2 == 0;
    // Even stations stand out and high, odd ones in and low, each at
    // its share of the turn.
    EXPECT_NEAR(std::hypot(station.x, station.z),
                outer ? shape.radius : shape.inner, 1e-3f)
        << i;
    EXPECT_FLOAT_EQ(station.y, outer ? shape.high : shape.low) << i;
    const float angle = std::atan2(station.z, station.x);
    const float expected = (float)i * 6.283185307179586f / (float)shape.knots;
    EXPECT_NEAR(std::remainder(angle - expected, 6.283185307179586f), 0.0f,
                1e-4f)
        << i;
  }
}

TEST(WorldKit, AWindingStaysOnItsShellAndCrossesItsOwnPlane) {
  kit::Winding shape;
  shape.at = {5.0f, -10.0f, 15.0f};
  const geometry::mesh::curve::Spline3 loop = kit::winding(shape);
  EXPECT_TRUE(loop.closed);
  ASSERT_EQ((int)loop.points.size(), shape.knots);
  // Every station is on the ellipsoid the shell names, round the centre.
  for (const glm::vec3& point : loop.points) {
    const glm::vec3 q = (point - shape.at) / shape.shell;
    EXPECT_NEAR(glm::dot(q, q), 1.0f, 1e-4f);
  }
  // It climbs and dives `wraps` times a lap: the height changes sign
  // twice per wrap, counted round the loop and across the seam.
  int crossings = 0;
  float turned = 0.0f;
  for (size_t i = 0; i < loop.points.size(); ++i) {
    const glm::vec3 here = (loop.points[i] - shape.at) / shape.shell;
    const glm::vec3 before =
        (loop.points[(i + loop.points.size() - 1) % loop.points.size()] -
         shape.at) /
        shape.shell;
    crossings += (here.y >= 0.0f) != (before.y >= 0.0f);
    // …while the azimuth makes `turns` laps of its own, from +x toward −z.
    turned += std::remainder(
        std::atan2(here.z, here.x) - std::atan2(before.z, before.x),
        6.283185307179586f);
  }
  EXPECT_EQ(crossings, 2 * (int)shape.wraps);
  EXPECT_NEAR(turned / 6.283185307179586f, -shape.turns, 1e-3f);
}

TEST(WorldKit, TheKeysStrengthAndColourAreBindableAndTheOthersAreTheirOwn) {
  motion::Animatable<float> strength = motion::animatable(0.5f);
  motion::Animatable<material::Color> tint =
      motion::animatable(material::Color{1, 0.5f, 0.25f, 1});
  const kit::Rig stated{.extent = 100.0f,
                        .intensity = strength,
                        .color = tint,
                        .fillIntensity = 0.8f,
                        .fillColor = material::Color{0.6f, 0.7f, 1, 1}};
  const Group rig = kit::threePoint(stated);
  ASSERT_NE(lightOf(rig, "key"), nullptr);
  ASSERT_NE(lightOf(rig, "fill"), nullptr);
  ASSERT_NE(lightOf(rig, "back"), nullptr);
  // Each lamp is stated by what it delivers at the subject: its
  // intensity is that strength times its distance squared.
  const float distance = 2.2f * stated.extent;
  const float square = distance * distance;
  EXPECT_NEAR(lightOf(rig, "key")->intensity.value(), 0.5f * square,
              1e-2f * square);
  EXPECT_NEAR(lightOf(rig, "fill")->intensity.value(), 0.8f * square,
              1e-2f * square);
  EXPECT_NEAR(lightOf(rig, "back")->intensity.value(),
              stated.backIntensity * square, 1e-2f * square);
  // The key reads the live values; the fill and the back keep their own
  // stated strength and colour whatever the key does.
  strength = 2.0f;
  tint = material::Color{0.2f, 0.2f, 1, 1};
  EXPECT_NEAR(lightOf(rig, "key")->intensity.value(), 2.0f * square,
              1e-2f * square);
  EXPECT_EQ(lightOf(rig, "key")->color.value(),
            (material::Color{0.2f, 0.2f, 1, 1}));
  EXPECT_NEAR(lightOf(rig, "fill")->intensity.value(), 0.8f * square,
              1e-2f * square);
  EXPECT_EQ(lightOf(rig, "fill")->color.value(),
            (material::Color{0.6f, 0.7f, 1, 1}));
  EXPECT_EQ(lightOf(rig, "back")->color.value(), (material::Color{1, 1, 1, 1}));
}

TEST(WorldKit, AnEnvironmentMapIsAGradientAndSoftBoxes) {
  // A black map: nothing here states a look.
  EXPECT_TRUE(kit::environmentMap({}).valid());
  const kit::EnvironmentMapOptions room{
      .width = 64,
      .zenith = {1.0f, 1.0f, 1.0f},
      .nadir = {0.0f, 0.0f, 0.0f},
      .softBoxes = {{.centre = {0.25f, 0.5f},
                     .size = {0.05f, 0.1f},
                     .radiance = {10.0f, 0.0f, 0.0f}}}};
  const EnvironmentMap map = kit::environmentMap(room);
  ASSERT_TRUE(map.valid());
  EXPECT_EQ(map.size(), glm::ivec2(64, 32));
  // The box shows red where it stands and leaves the rest grey.
  EXPECT_GT(map.average().r, map.average().g);
}

}  // namespace

TEST(WorldKit, AVisibleLightIsTheLightBesideAnUnlitMeshOfItsShape) {
  const Group panel = kit::visibleLight(
      rectLight({0, -1, 0},
                {.width = 4,
                 .height = 2,
                 .color = ::sigil::material::Color{1, 0.5f, 0.25f, 1}})
          .key("panel"));
  EXPECT_EQ(keysOf(panel), (std::vector<std::string>{"panel", "emitter"}));
  const NodeDescription* emitter = childOf(panel, "emitter");
  ASSERT_NE(emitter, nullptr);
  EXPECT_FALSE(emitter->castShadow);
  ASSERT_NE(surfaceOf(panel, "emitter"), nullptr);
  // The panel's face, +z, is turned onto the way the light shines.
  const glm::mat4 turned = emitter->transform.matrix.value_or(glm::mat4(1));
  EXPECT_NEAR(turned[2][1], -1.0f, 1e-5f);
  EXPECT_NEAR(turned[0][0], 1.0f, 1e-5f);

  EXPECT_EQ(keysOf(kit::visibleLight(sphereLight({.radius = 1}).key("ball"))),
            (std::vector<std::string>{"ball", "emitter"}));
  EXPECT_EQ(keysOf(kit::visibleLight(diskLight({0, 0, -1}).key("disk"))),
            (std::vector<std::string>{"disk", "emitter"}));
  // A light with no size has nothing to show.
  EXPECT_EQ(keysOf(kit::visibleLight(pointLight().key("lamp"))),
            (std::vector<std::string>{"lamp"}));
}
