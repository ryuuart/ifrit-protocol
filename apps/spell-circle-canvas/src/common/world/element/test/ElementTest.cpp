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
#include <string>

#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

/** ONE TRIANGLE, and it has to stay one: a stamp is stood at every point
 *  of a cloud, so the triangle count of what that cooked to is the
 *  cloud's size times this. */
geometry::mesh::Mesh triangle(float size) {
  geometry::mesh::Mesh m;
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

namespace {

/** ONE FIELD, SAID TWO WAYS. A description carrying the first differs
 *  from one carrying neither and from one carrying the second, and
 *  compares equal to a second description of itself — which is what a
 *  field being IN the comparison means, as against being left out of it
 *  and never patching again. */
struct Field {
  const char* what;
  Element (*one)(Element);
  Element (*other)(Element);
};

class DescribedField : public testing::TestWithParam<Field> {};

std::string fieldName(const testing::TestParamInfo<Field>& info) {
  return info.param.what;
}

const Field kFields[] = {
    {"TranslateX", [](Element e) { return e.translateX(1.0f); },
     [](Element e) { return e.translateX(2.0f); }},
    {"TranslateY", [](Element e) { return e.translateY(1.0f); },
     [](Element e) { return e.translateY(2.0f); }},
    {"TranslateZ", [](Element e) { return e.translateZ(1.0f); },
     [](Element e) { return e.translateZ(2.0f); }},
    {"RotateX", [](Element e) { return e.rotateX(1.0f); },
     [](Element e) { return e.rotateX(2.0f); }},
    {"RotateY", [](Element e) { return e.rotateY(1.0f); },
     [](Element e) { return e.rotateY(2.0f); }},
    {"RotateZ", [](Element e) { return e.rotateZ(1.0f); },
     [](Element e) { return e.rotateZ(2.0f); }},
    {"ScaleX", [](Element e) { return e.scaleX(2.0f); },
     [](Element e) { return e.scaleX(3.0f); }},
    {"ScaleY", [](Element e) { return e.scaleY(2.0f); },
     [](Element e) { return e.scaleY(3.0f); }},
    {"ScaleZ", [](Element e) { return e.scaleZ(2.0f); },
     [](Element e) { return e.scaleZ(3.0f); }},
    {"TransformOrigin", [](Element e) { return e.transformOrigin({1, 1, 1}); },
     [](Element e) { return e.transformOrigin({2, 2, 2}); }},
    {"AnAxisAndAnAngle", [](Element e) { return e.rotate({1, 0, 0}, 15.0f); },
     [](Element e) { return e.rotate({0, 1, 0}, 15.0f); }},
    {"AWholeMatrix", [](Element e) { return e.transform(glm::mat4(2.0f)); },
     [](Element e) { return e.transform(glm::mat4(3.0f)); }},
    {"ATag", [](Element e) { return e.tag("glow"); },
     [](Element e) { return e.tag("dim"); }},
    {"AnEmitter", [](Element e) { return e.light(sun({0, -1, 0})); },
     [](Element e) { return e.light(sun({0, 1, 0})); }},
    {"AViewpoint",
     [](Element e) { return e.camera(geometry::mesh::camera::Camera{}); },
     [](Element e) {
       geometry::mesh::camera::Camera lens;
       lens.eye = {0, 0, 10};
       return e.camera(lens);
     }},
    {"TheCacheWord", [](Element e) { return e.cache(core::Cache::Never); },
     [](Element e) { return e.cache(core::Cache::Always); }},
    {"ARailAndADistanceAlongIt",
     [](Element e) {
       geometry::mesh::curve::Spline3 spline;
       spline.points = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
       return e.along(spline, 10.0f);
     },
     [](Element e) {
       geometry::mesh::curve::Spline3 spline;
       spline.points = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
       return e.along(spline, 20.0f);
     }},
    {"AWindowOnTheRail", [](Element e) { return e.window(0.5f, 0.2f); },
     [](Element e) { return e.window(0.5f, 0.4f); }},
    {"AnIntensityDial",
     [](Element e) { return e.light(point({0, 0, 0})).intensity(2.0f); },
     [](Element e) { return e.light(point({0, 0, 0})).intensity(1.0f); }},
    {"AnEmissionDial",
     [](Element e) {
       return e.light(point({0, 0, 0})).emission(1.0f, 1.0f, 1.0f);
     },
     [](Element e) {
       return e.light(point({0, 0, 0})).emission(1.0f, 1.0f, 0.5f);
     }},
    // A dial that is there and one that is not are different
    // descriptions, because the emitter's own field stands where the
    // dial is absent.
    {"ADialAtAllAgainstNone",
     [](Element e) { return e.light(point({0, 0, 0})).intensity(1.0f); },
     [](Element e) { return e.light(point({0, 0, 0})); }},
};

}  // namespace

TEST_P(DescribedField, ReachesThePruneAndTellsItsTwoValuesApart) {
  const auto base = [] { return Element().key("body"); };
  const Field& field = GetParam();
  const Element one = field.one(base());
  const Element other = field.other(base());
  EXPECT_FALSE(propsEqual(*base().node(), *one.node()))
      << "does not reach the prune";
  EXPECT_FALSE(propsEqual(*base().node(), *other.node()))
      << "does not reach the prune";
  EXPECT_FALSE(propsEqual(*one.node(), *other.node()))
      << "compares equal at two different values";
  EXPECT_TRUE(propsEqual(*one.node(), *field.one(base()).node()))
      << "compares unequal to a second description of itself";
}

INSTANTIATE_TEST_SUITE_P(EveryFieldADescriptionCarries, DescribedField,
                         testing::ValuesIn(kFields), fieldName);

TEST(WorldElement, BackfaceVisibilityReachesThePrune) {
  const Element hidden = Element().key("body");
  const Element visible = Element().key("body").backface(Backface::Visible);
  EXPECT_FALSE(propsEqual(*hidden.node(), *visible.node()));
  EXPECT_TRUE(
      propsEqual(*visible.node(),
                 *Element().key("body").backface(Backface::Visible).node()));
}

TEST(WorldElement, TheGeometrySlotsValueTypeIsTheKind) {
  Element mesh = Element().key("g").mesh(triangle(10));
  Element cloud =
      Element().key("g").cloud(geometry::mesh::Cloud{}).stamp(triangle(10));
  EXPECT_FALSE(propsEqual(*mesh.node(), *cloud.node()));

  Element sameMesh = Element().key("g").mesh(triangle(10));
  EXPECT_TRUE(propsEqual(*mesh.node(), *sameMesh.node()));

  Element otherMesh = Element().key("g").mesh(triangle(11));
  EXPECT_FALSE(propsEqual(*mesh.node(), *otherMesh.node()));
}

TEST(WorldElement, StampAndCloudReadInEitherOrder) {
  geometry::mesh::Cloud points;
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

  geometry::mesh::curve::Spline3 spline;
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
