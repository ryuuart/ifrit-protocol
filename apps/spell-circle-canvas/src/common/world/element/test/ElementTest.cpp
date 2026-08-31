/** @file
 * The description value: copy-on-write, the structural prune field by
 * field, the geometry slot's value type standing in for a kind, the lane
 * list, and the cook.
 */

#include <gtest/gtest.h>
#include <sigilworld/element/Lanes.h>
#include <sigilworld/element/Node.h>

#include <glm/vec4.hpp>
#include <memory>

using namespace sigil;
using namespace sigil::world;

namespace {

struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

std::shared_ptr<const material::Recipe> paintRecipe() {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.test.paint"));
  return recipe;
}

material::Material paint(glm::vec4 colour) {
  return material::Material(paintRecipe(), Paint{colour});
}

Mesh triangle(float size) {
  Mesh m;
  m.positions = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}};
  m.normals = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}};
  m.uvs = {{0, 0}, {1, 0}, {0, 1}};
  m.indices = {0, 1, 2};
  return m;
}

}  // namespace

TEST(WorldElement, CopyOnWriteLeavesTheOriginalAlone) {
  Element base;
  base.key("root").translateX(10.0f);
  Element copy = base;
  copy.translateX(20.0f);

  EXPECT_NE(base.node(), copy.node());
  EXPECT_FALSE(propsEqual(*base.node(), *copy.node()));

  Element same = base;
  EXPECT_EQ(base.node(), same.node());
  EXPECT_TRUE(propsEqual(*base.node(), *same.node()));
}

TEST(WorldElement, TwoDescribesOfTheSameNodePrune) {
  const auto describe = [] {
    return Element().key("body").at({1, 2, 3}).rotateY(30.0f).scale(2.0f).tag(
        "lit");
  };
  Element a = describe();
  Element b = describe();
  EXPECT_TRUE(propsEqual(*a.node(), *b.node()));
}

TEST(WorldElement, EveryTransformLaneParticipatesInThePrune) {
  const auto base = [] { return Element().key("body"); };
  struct Case {
    const char* what;
    Element (*apply)(Element);
  };
  const Case cases[] = {
      {"translateX", [](Element e) { return e.translateX(1.0f); }},
      {"translateY", [](Element e) { return e.translateY(1.0f); }},
      {"translateZ", [](Element e) { return e.translateZ(1.0f); }},
      {"rotateX", [](Element e) { return e.rotateX(1.0f); }},
      {"rotateY", [](Element e) { return e.rotateY(1.0f); }},
      {"rotateZ", [](Element e) { return e.rotateZ(1.0f); }},
      {"scaleX", [](Element e) { return e.scaleX(2.0f); }},
      {"scaleY", [](Element e) { return e.scaleY(2.0f); }},
      {"scaleZ", [](Element e) { return e.scaleZ(2.0f); }},
      {"origin", [](Element e) { return e.transformOrigin({1, 1, 1}); }},
      {"axis", [](Element e) { return e.rotate({1, 0, 0}, 15.0f); }},
      {"matrix", [](Element e) { return e.transform(glm::mat4(2.0f)); }},
  };
  for (const Case& c : cases) {
    Element moved = c.apply(base());
    EXPECT_FALSE(propsEqual(*base().node(), *moved.node()))
        << c.what << " does not reach the prune";
  }
}

TEST(WorldElement, TheGeometrySlotsValueTypeIsTheKind) {
  Element mesh = Element().key("g").mesh(triangle(10));
  Element cloud = Element().key("g").cloud(Cloud{}).stamp(triangle(10));
  EXPECT_FALSE(propsEqual(*mesh.node(), *cloud.node()));

  Element sameMesh = Element().key("g").mesh(triangle(10));
  EXPECT_TRUE(propsEqual(*mesh.node(), *sameMesh.node()));

  Element otherMesh = Element().key("g").mesh(triangle(11));
  EXPECT_FALSE(propsEqual(*mesh.node(), *otherMesh.node()));
}

TEST(WorldElement, StampAndCloudReadInEitherOrder) {
  Cloud points;
  points.positions = {{0, 0, 0}, {5, 0, 0}};
  Element first = Element().stamp(triangle(2)).cloud(points);
  Element second = Element().cloud(points).stamp(triangle(2));
  EXPECT_TRUE(propsEqual(*first.node(), *second.node()));

  const Cooked cooked = cook(first.node()->geometry);
  EXPECT_EQ(cooked.cloud.size(), 2u);
  EXPECT_EQ(cooked.mesh.triangleCount(), 2u);
}

TEST(WorldElement, MaterialsCompareByValue) {
  Element a = Element().fill(paint({1, 0, 0, 1}));
  Element b = Element().fill(paint({1, 0, 0, 1}));
  Element c = Element().fill(paint({0, 1, 0, 1}));
  EXPECT_TRUE(propsEqual(*a.node(), *b.node()));
  EXPECT_FALSE(propsEqual(*a.node(), *c.node()));

  const material::Material slots[] = {paint({1, 0, 0, 1}), paint({0, 0, 1, 1})};
  Element perFace = Element().fill(std::span<const material::Material>(slots));
  EXPECT_FALSE(propsEqual(*a.node(), *perFace.node()));
  EXPECT_EQ(perFace.node()->slots.size(), 2u);
  EXPECT_FALSE(perFace.node()->material.has_value());
}

TEST(WorldElement, TagsLightsCamerasAndCacheReachThePrune) {
  Element base = Element().key("n");
  EXPECT_FALSE(
      propsEqual(*base.node(), *Element().key("n").tag("glow").node()));
  EXPECT_FALSE(propsEqual(*base.node(),
                          *Element().key("n").light(sun({0, -1, 0})).node()));
  EXPECT_FALSE(
      propsEqual(*base.node(), *Element().key("n").camera(Camera{}).node()));
  EXPECT_FALSE(propsEqual(
      *base.node(), *Element().key("n").cache(core::Cache::Never).node()));
}

TEST(WorldElement, AlongAndWindowReachThePrune) {
  Spline3 spline;
  spline.points = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
  Element base = Element().key("n");
  Element rides = Element().key("n").along(spline, 10.0f);
  Element further = Element().key("n").along(spline, 20.0f);
  EXPECT_FALSE(propsEqual(*base.node(), *rides.node()));
  EXPECT_FALSE(propsEqual(*rides.node(), *further.node()));

  Element windowed = Element().key("n").window(0.5f, 0.2f);
  Element widened = Element().key("n").window(0.5f, 0.4f);
  EXPECT_FALSE(propsEqual(*base.node(), *windowed.node()));
  EXPECT_FALSE(propsEqual(*windowed.node(), *widened.node()));
}

TEST(WorldElement, LanesAreOneFixedRowPerSlot) {
  std::vector<Lane> lanes;
  lanesOf(*Element().node(), lanes);
  ASSERT_EQ(lanes.size(), (size_t)kLaneCount);
  for (size_t i = 0; i < lanes.size(); ++i) EXPECT_EQ(lanes[i].slot.index, i);
  EXPECT_NE(lanes[kTranslateX].value, nullptr);
  EXPECT_EQ(lanes[kAlongDistance].value, nullptr);
  EXPECT_EQ(lanes[kWindowHead].value, nullptr);
  EXPECT_FLOAT_EQ(lanes[kScaleX].standing, 1.0f);
  EXPECT_FLOAT_EQ(lanes[kTranslateX].standing, 0.0f);

  Spline3 spline;
  spline.points = {{0, 0, 0}, {10, 0, 0}};
  lanesOf(*Element().along(spline, 3.0f).window(0.5f, 0.25f).node(), lanes);
  EXPECT_NE(lanes[kAlongDistance].value, nullptr);
  EXPECT_NE(lanes[kWindowSpan].value, nullptr);
}

TEST(WorldElement, AnEmitterLaneStandsWhereTheEmitterStands) {
  std::vector<Lane> lanes;
  // No emitter and no dials: the rows are there, empty, at full
  // strength in white.
  lanesOf(*Element().node(), lanes);
  EXPECT_EQ(lanes[kIntensity].value, nullptr);
  EXPECT_FLOAT_EQ(lanes[kIntensity].standing, 1.0f);
  EXPECT_FLOAT_EQ(lanes[kEmissionGreen].standing, 1.0f);

  // An emitter with no dials: each row stands at the emitter's own
  // field, so a dropped dial ramps back to the light rather than to one.
  const Light lamp = point({0, 0, 0}, {0.2f, 0.4f, 0.8f, 1.0f}, 0.6f);
  lanesOf(*Element().light(lamp).node(), lanes);
  EXPECT_EQ(lanes[kIntensity].value, nullptr);
  EXPECT_FLOAT_EQ(lanes[kIntensity].standing, 0.6f);
  EXPECT_FLOAT_EQ(lanes[kEmissionRed].standing, 0.2f);
  EXPECT_FLOAT_EQ(lanes[kEmissionBlue].standing, 0.8f);

  // …and the dials the tree DID put there carry values.
  lanesOf(*Element().light(lamp).intensity(2.0f).node(), lanes);
  EXPECT_NE(lanes[kIntensity].value, nullptr);
  EXPECT_EQ(lanes[kEmissionRed].value, nullptr);
  lanesOf(*Element().light(lamp).emission(1.0f, 0.5f, 0.25f).node(), lanes);
  EXPECT_NE(lanes[kEmissionGreen].value, nullptr);
  EXPECT_EQ(lanes[kIntensity].value, nullptr);
}

TEST(WorldElement, TheEmitterDialsTakePartInTheStructuralPrune) {
  const Light lamp = point({0, 0, 0});
  EXPECT_TRUE(propsEqual(*Element().light(lamp).intensity(2.0f).node(),
                         *Element().light(lamp).intensity(2.0f).node()));
  EXPECT_FALSE(propsEqual(*Element().light(lamp).intensity(2.0f).node(),
                          *Element().light(lamp).intensity(1.0f).node()));
  // A dial that is there and one that is not are different descriptions,
  // because the emitter's own field stands where the dial is absent.
  EXPECT_FALSE(propsEqual(*Element().light(lamp).intensity(1.0f).node(),
                          *Element().light(lamp).node()));
  EXPECT_FALSE(
      propsEqual(*Element().light(lamp).emission(1.0f, 1.0f, 1.0f).node(),
                 *Element().light(lamp).emission(1.0f, 1.0f, 0.5f).node()));
}

TEST(WorldElement, LocalMatrixPlacesScalesAndTurns) {
  TransformValues values;
  values.translate = {10, 0, 0};
  values.scale = {2, 2, 2};
  const glm::vec4 placed = localMatrix(values) * glm::vec4(1, 0, 0, 1);
  EXPECT_FLOAT_EQ(placed.x, 12.0f);

  TransformValues turned;
  turned.rotateDegrees = {0, 90, 0};
  const glm::vec4 spun = localMatrix(turned) * glm::vec4(1, 0, 0, 1);
  EXPECT_NEAR(spun.x, 0.0f, 1e-5f);
  EXPECT_NEAR(spun.z, -1.0f, 1e-5f);
}

TEST(WorldElement, SignatureBucketsEqualValuesTogether) {
  const Geometry a = triangle(10);
  const Geometry b = triangle(10);
  const Geometry c = Stamped{{}, triangle(10)};
  EXPECT_EQ(signature(a), signature(b));
  EXPECT_NE(signature(a), signature(c));
}

TEST(WorldElement, MemoSkipsTheDescribeItsPropsDidNotChange) {
  int described = 0;
  const auto build = [&described](int value) {
    return memo(value, [&described](const int& v) {
      ++described;
      return Element().key("memoized").translateX((float)v);
    });
  };
  Element first = build(3);
  ASSERT_TRUE(first.node()->memo.has_value());
  EXPECT_EQ(described, 0);  // the describe is deferred until it is resolved

  const Memo& shell = *first.node()->memo;
  const Element same = build(3);
  const Element other = build(4);
  ASSERT_TRUE(same.node()->memo.has_value());
  ASSERT_TRUE(other.node()->memo.has_value());
  EXPECT_TRUE(shell.equal(shell.props, same.node()->memo->props));
  EXPECT_FALSE(shell.equal(shell.props, other.node()->memo->props));
  Element produced = shell.invoke(shell.props);
  EXPECT_EQ(described, 1);
  EXPECT_EQ(produced.node()->key, "memoized");
}
