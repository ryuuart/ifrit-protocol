/** @file
 * The node kinds over one description: copy-on-write, a group's
 * children as its argument, the structural prune field by field, the
 * geometry slot's value type standing in for a kind, the lane list, and
 * the cook.
 */

#include <gtest/gtest.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Lanes.h>
#include <sigilworld/advanced/Node.h>

#include <array>
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

TEST(WorldNode, EachDescribesTheChildrenARangeOrACountNames) {
  // A ring of N posts is one braced block and not a loop of appends.
  const std::array<float, 3> lanes{{1.0f, 2.0f, 3.0f}};
  const Group rig =
      group({each(lanes, [](float x, size_t i) {
        return group().key("lane" + std::to_string(i)).translateX(x);
      })}).key("rig");
  ASSERT_EQ(rig.node()->children.size(), 3u);
  EXPECT_EQ(rig.node()->children[2].node()->key, "lane2");

  const Group ring = group(each(
      4, [](size_t i) { return group().key("post" + std::to_string(i)); }));
  ASSERT_EQ(ring.node()->children.size(), 4u);
  EXPECT_EQ(ring.node()->children[3].node()->key, "post3");
  EXPECT_TRUE(each(0, [] { return group(); }).empty());
}

TEST(WorldNode, AGroupTakesEveryKindInOneBracedList) {
  geometry::mesh::camera::Camera lens;
  const Group set =
      group({mesh(triangle(1)).key("body"), pointLight().key("lamp"),
             camera(lens).key("eye"), group({mesh(triangle(2))}).key("inner"),
             each(2, [] { return group(); })})
          .key("set");
  ASSERT_EQ(set.node()->children.size(), 6u);
  EXPECT_TRUE(set.node()->children[1].node()->light.has_value());
  EXPECT_TRUE(set.node()->children[2].node()->camera.has_value());
  EXPECT_EQ(set.node()->children[3].node()->children.size(), 1u);
}

TEST(WorldNode, CopyOnWriteLeavesTheOriginalAlone) {
  const Group base = group().key("root").translateX(10.0f);
  Group copy = base;
  copy.translateX(20.0f);

  EXPECT_NE(base.node(), copy.node());
  EXPECT_FALSE(propertiesEqual(*base.node(), *copy.node()));

  const Group same = base;
  EXPECT_EQ(base.node(), same.node());
  EXPECT_TRUE(propertiesEqual(*base.node(), *same.node()));
}

TEST(WorldNode, TwoDescribesOfTheSameNodePrune) {
  const auto describe = [] {
    return mesh(triangle(1))
        .key("body")
        .at({1, 2, 3})
        .rotateY(30.0f)
        .scale(2.0f)
        .collection("lit");
  };
  EXPECT_TRUE(propertiesEqual(*describe().node(), *describe().node()));
}

namespace {

/** ONE FIELD, SAID TWO WAYS, on the kind that carries it. A description
 *  carrying the first differs from the kind's bare node and from one
 *  carrying the second, and compares equal to a second description of
 *  itself — which is what a field being IN the comparison means, as
 *  against being left out of it and never patching again. */
struct Field {
  const char* what;
  Node (*base)();
  Node (*one)();
  Node (*other)();
};

class DescribedField : public testing::TestWithParam<Field> {};

std::string fieldName(const testing::TestParamInfo<Field>& info) {
  return info.param.what;
}

Mesh body() { return mesh(triangle(1)).key("body"); }
Light lamp() { return pointLight().key("body"); }
geometry::mesh::curve::Spline3 rail() {
  geometry::mesh::curve::Spline3 spline;
  spline.points = {{0, 0, 0}, {100, 0, 0}, {100, 100, 0}};
  return spline;
}
Mesh comet(float head, float span) {
  return mesh(geometry::mesh::pop::on(
                  std::vector<glm::vec3>{{0, 0, 0}, {10, 0, 0}, {10, 10, 0}})
                  .count(12)
                  .window(head, span)
                  .instances(triangle(1)))
      .key("body");
}

const Field kFields[] = {
    {"TranslateX", [] -> Node { return body(); },
     [] -> Node { return body().translateX(1.0f); },
     [] -> Node { return body().translateX(2.0f); }},
    {"TranslateY", [] -> Node { return body(); },
     [] -> Node { return body().translateY(1.0f); },
     [] -> Node { return body().translateY(2.0f); }},
    {"TranslateZ", [] -> Node { return body(); },
     [] -> Node { return body().translateZ(1.0f); },
     [] -> Node { return body().translateZ(2.0f); }},
    {"RotateX", [] -> Node { return body(); },
     [] -> Node { return body().rotateX(1.0f); },
     [] -> Node { return body().rotateX(2.0f); }},
    {"RotateY", [] -> Node { return body(); },
     [] -> Node { return body().rotateY(1.0f); },
     [] -> Node { return body().rotateY(2.0f); }},
    {"RotateZ", [] -> Node { return body(); },
     [] -> Node { return body().rotateZ(1.0f); },
     [] -> Node { return body().rotateZ(2.0f); }},
    {"ScaleX", [] -> Node { return body(); },
     [] -> Node { return body().scaleX(2.0f); },
     [] -> Node { return body().scaleX(3.0f); }},
    {"ScaleY", [] -> Node { return body(); },
     [] -> Node { return body().scaleY(2.0f); },
     [] -> Node { return body().scaleY(3.0f); }},
    {"ScaleZ", [] -> Node { return body(); },
     [] -> Node { return body().scaleZ(2.0f); },
     [] -> Node { return body().scaleZ(3.0f); }},
    {"TransformOrigin", [] -> Node { return body(); },
     [] -> Node { return body().transformOrigin({1, 1, 1}); },
     [] -> Node { return body().transformOrigin({2, 2, 2}); }},
    {"AnAxisAndAnAngle", [] -> Node { return body(); },
     [] -> Node { return body().rotate({1, 0, 0}, 15.0f); },
     [] -> Node { return body().rotate({0, 1, 0}, 15.0f); }},
    {"AWholeMatrix", [] -> Node { return body(); },
     [] -> Node { return body().transform(glm::mat4(2.0f)); },
     [] -> Node { return body().transform(glm::mat4(3.0f)); }},
    {"Visibility", [] -> Node { return body(); },
     [] -> Node { return body().visible(false); },
     [] -> Node { return body().visible(false).collection("a"); }},
    {"ACollection", [] -> Node { return body(); },
     [] -> Node { return body().collection("glow"); },
     [] -> Node { return body().collection("dim"); }},
    {"TheCacheWord", [] -> Node { return body(); },
     [] -> Node { return body().cache(tree::Cache::Never); },
     [] -> Node { return body().cache(tree::Cache::Always); }},
    {"ARailAndADistanceAlongIt", [] -> Node { return body(); },
     [] -> Node { return body().along(rail(), 10.0f); },
     [] -> Node { return body().along(rail(), 20.0f); }},
    {"ALightsDirection", [] -> Node { return group().key("body"); },
     [] -> Node { return directionalLight({0, -1, 0}).key("body"); },
     [] -> Node { return directionalLight({0, 1, 0}).key("body"); }},
    {"ALightsColour", [] -> Node { return group().key("body"); },
     [] -> Node {
       return pointLight({.color = material::Color{1, 0, 0, 1}}).key("body");
     },
     [] -> Node {
       return pointLight({.color = material::Color{0, 1, 0, 1}}).key("body");
     }},
    {"ALightsIntensity", [] -> Node { return group().key("body"); },
     [] -> Node { return pointLight({.intensity = 2.0f}).key("body"); },
     [] -> Node { return pointLight({.intensity = 1.5f}).key("body"); }},
    {"ALightsRange", [] -> Node { return group().key("body"); },
     [] -> Node { return pointLight({.range = 100}).key("body"); },
     [] -> Node { return pointLight({.range = 200}).key("body"); }},
    {"ASpotsCone", [] -> Node { return group().key("body"); },
     [] -> Node {
       return spotLight({0, -1, 0}, {.outerAngle = 30}).key("body");
     },
     [] -> Node {
       return spotLight({0, -1, 0}, {.outerAngle = 40}).key("body");
     }},
    {"ALightLink", [] -> Node { return lamp(); },
     [] -> Node { return lamp().lightLink("props"); },
     [] -> Node { return lamp().lightLink("ground"); }},
    {"AShadowLink", [] -> Node { return lamp(); },
     [] -> Node { return lamp().shadowLink("props"); },
     [] -> Node { return lamp().shadowLink("ground"); }},
    {"AViewpoint", [] -> Node { return group().key("body"); },
     [] -> Node {
       return camera(geometry::mesh::camera::Camera{}).key("body");
     },
     [] -> Node {
       geometry::mesh::camera::Camera lens;
       lens.eye = {0, 0, 10};
       return camera(lens).key("body");
     }},
    {"AnExposure",
     [] -> Node {
       return camera(geometry::mesh::camera::Camera{}).key("body");
     },
     [] -> Node {
       return camera(geometry::mesh::camera::Camera{}, {.exposure = 2.0f})
           .key("body");
     },
     [] -> Node {
       return camera(geometry::mesh::camera::Camera{}, {.exposure = 0.5f})
           .key("body");
     }},
    {"AWindowOnTheRail", [] -> Node { return comet(1.0f, 1.0f); },
     [] -> Node { return comet(0.5f, 0.2f); },
     [] -> Node { return comet(0.5f, 0.4f); }},
    {"WhatALightLooksAt", [] -> Node { return lamp(); },
     [] -> Node { return lamp().lookAt("pivot"); },
     [] -> Node { return lamp().lookAt("target"); }},
    {"TheAxisAnAimPoints", [] -> Node { return group().key("body"); },
     [] -> Node {
       Group body = group().key("body");
       body.edit().forward = {0, -1, 0};
       return body;
     },
     [] -> Node {
       Group body = group().key("body");
       body.edit().forward = {1, 0, 0};
       return body;
     }},
};

}  // namespace

TEST_P(DescribedField, ReachesThePruneAndTellsItsTwoValuesApart) {
  const Field& field = GetParam();
  const Node base = field.base();
  const Node one = field.one();
  const Node other = field.other();
  EXPECT_FALSE(propertiesEqual(*base.node(), *one.node()))
      << "does not reach the prune";
  EXPECT_FALSE(propertiesEqual(*base.node(), *other.node()))
      << "does not reach the prune";
  EXPECT_FALSE(propertiesEqual(*one.node(), *other.node()))
      << "compares equal at two different values";
  EXPECT_TRUE(propertiesEqual(*one.node(), *field.one().node()))
      << "compares unequal to a second description of itself";
}

INSTANTIATE_TEST_SUITE_P(EveryFieldADescriptionCarries, DescribedField,
                         testing::ValuesIn(kFields), fieldName);

TEST(WorldNode, BackfaceVisibilityReachesThePrune) {
  const Mesh hidden = body();
  const Mesh shown = body().backface(material::Backface::Visible);
  EXPECT_FALSE(propertiesEqual(*hidden.node(), *shown.node()));
  EXPECT_TRUE(propertiesEqual(
      *shown.node(), *body().backface(material::Backface::Visible).node()));
}

TEST(WorldNode, TheGeometrySlotsValueTypeIsTheKind) {
  const Mesh one = mesh(triangle(10)).key("g");
  geometry::mesh::Instances repeated;
  repeated.stamp = triangle(10);
  const Mesh instanced = mesh(repeated).key("g");
  EXPECT_FALSE(propertiesEqual(*one.node(), *instanced.node()));
  EXPECT_FALSE(propertiesEqual(*one.node(), *null().key("g").node()));
  EXPECT_TRUE(
      propertiesEqual(*one.node(), *mesh(triangle(10)).key("g").node()));
  EXPECT_FALSE(
      propertiesEqual(*one.node(), *mesh(triangle(11)).key("g").node()));
}

TEST(WorldNode, InstancedGeometryIsDrawnAsItIsAndCooksNothing) {
  geometry::mesh::Cloud cloud;
  cloud.positions = {{0, 0, 0}, {5, 0, 0}};
  const Mesh body = mesh(geometry::mesh::points::instances(cloud, triangle(2)));
  const auto* instances =
      std::get_if<geometry::mesh::Instances>(&body.node()->geometry);
  ASSERT_NE(instances, nullptr);
  EXPECT_EQ(instances->size(), 2u);
  EXPECT_EQ(instances->stamp.triangleCount(), 1u);
  EXPECT_TRUE(cook(body.node()->geometry).mesh.indices.empty());
}

namespace {

/** A consumer's own solid value: a square card of a side, counting how
 *  many times it was formed. */
struct CountedCard {
  float side = 1;
  std::shared_ptr<int> formed = std::make_shared<int>(0);
  bool operator==(const CountedCard& other) const { return side == other.side; }
  [[nodiscard]] geometry::mesh::Mesh mesh() const {
    ++*formed;
    return geometry::mesh::quad(side, side);
  }
};

}  // namespace

TEST(WorldNode, ASolidValueIsHeldAsAValueAndFormedOnlyByTheCook) {
  static_assert(geometry::mesh::Solid<CountedCard>);
  const CountedCard card{.side = 12};
  // Described twice, as two frames of a sketch describe it: the slot holds
  // the value, nothing is formed, and the two descriptions are one body to
  // the prune and one artefact to the content-keyed store.
  const Mesh first = mesh(card).key("card");
  const Mesh second = mesh(card).key("card");
  EXPECT_EQ(*card.formed, 0) << "describing a solid forms nothing";
  EXPECT_TRUE(propertiesEqual(*first.node(), *second.node()));
  EXPECT_EQ(first.node()->geometry, second.node()->geometry);
  EXPECT_EQ(signature(first.node()->geometry),
            signature(second.node()->geometry));
  // An equal value built afresh is the same body; another side is not.
  EXPECT_TRUE(propertiesEqual(
      *first.node(), *mesh(CountedCard{.side = 12}).key("card").node()));
  EXPECT_FALSE(propertiesEqual(
      *first.node(), *mesh(CountedCard{.side = 13}).key("card").node()));
  // The cook is what forms it, once per cook, into what the value says.
  EXPECT_EQ(cook(first.node()->geometry).mesh, geometry::mesh::quad(12, 12));
  EXPECT_EQ(*card.formed, 1);
}

TEST(WorldNode, ANullIsAPlacementAndNothingElse) {
  const Null pivot = null().key("pivot").at({1, 2, 3}).collection("marks");
  const NodeDescription& node = *pivot.node();
  EXPECT_EQ(node.key, "pivot");
  EXPECT_TRUE(std::holds_alternative<std::monostate>(node.geometry));
  EXPECT_FALSE(node.light.has_value());
  EXPECT_FALSE(node.camera.has_value());
  EXPECT_TRUE(node.children.empty());
  EXPECT_EQ(node.collections, std::vector<std::string>{"marks"});
}

TEST(WorldNode, MaterialsCompareByValue) {
  const Mesh a = body().fill(paint({1, 0, 0, 1}));
  const Mesh b = body().fill(paint({1, 0, 0, 1}));
  const Mesh c = body().fill(paint({0, 1, 0, 1}));
  EXPECT_TRUE(propertiesEqual(*a.node(), *b.node()));
  EXPECT_FALSE(propertiesEqual(*a.node(), *c.node()));

  const material::Material slots[] = {paint({1, 0, 0, 1}), paint({0, 0, 1, 1})};
  const Mesh perFace = body().fill(std::span<const material::Material>(slots));
  EXPECT_FALSE(propertiesEqual(*a.node(), *perFace.node()));
  EXPECT_EQ(perFace.node()->slots.size(), 2u);
  EXPECT_FALSE(perFace.node()->material.has_value());
}

TEST(WorldNode, AColourFillIsTheStockSurfaceProgramOfThatColour) {
  // `fill(colour)` and the program spelled out describe one surface, which
  // is what lets a scene say a colour and be lit.
  const material::Color colour{0.6f, 0.3f, 0.2f, 1.0f};
  const Mesh short_ = body().fill(colour);
  const Mesh spelled =
      body().fill(material::surface::program({.baseColor = colour}));
  EXPECT_TRUE(propertiesEqual(*short_.node(), *spelled.node()));
}

TEST(WorldNode, LanesAreOneFixedRowPerSlot) {
  std::vector<Lane> lanes;
  lanesOf(*group().node(), lanes);
  ASSERT_EQ(lanes.size(), (size_t)kLaneCount);
  for (size_t i = 0; i < lanes.size(); ++i) EXPECT_EQ(lanes[i].slot.index, i);
  EXPECT_NE(lanes[kTranslateX].value, nullptr);
  EXPECT_EQ(lanes[kAlongDistance].value, nullptr);
  EXPECT_FLOAT_EQ(lanes[kScaleX].standing, 1.0f);
  EXPECT_FLOAT_EQ(lanes[kTranslateX].standing, 0.0f);

  lanesOf(*comet(0.5f, 0.25f).along(rail(), 3.0f).node(), lanes);
  EXPECT_NE(lanes[kAlongDistance].value, nullptr);
}

TEST(WorldNode, LocalMatrixPlacesScalesAndTurns) {
  geometry::mesh::Transform values;
  values.translate = {10, 0, 0};
  values.scale = {2, 2, 2};
  const glm::vec4 placed = values.matrix() * glm::vec4(1, 0, 0, 1);
  EXPECT_FLOAT_EQ(placed.x, 12.0f);

  geometry::mesh::Transform turned;
  turned.rotateDegrees = {0, 90, 0};
  const glm::vec4 spun = turned.matrix() * glm::vec4(1, 0, 0, 1);
  EXPECT_NEAR(spun.x, 0.0f, 1e-5f);
  EXPECT_NEAR(spun.z, -1.0f, 1e-5f);
}

TEST(WorldNode, SignatureBucketsEqualValuesTogether) {
  const Geometry a = triangle(10);
  const Geometry b = triangle(10);
  geometry::mesh::Instances repeated;
  repeated.stamp = triangle(10);
  const Geometry c = repeated;
  EXPECT_EQ(signature(a), signature(b));
  EXPECT_NE(signature(a), signature(c));
}

TEST(WorldNode, MemoSkipsTheDescribeItsPropsDidNotChange) {
  int described = 0;
  const auto build = [&described](int value) {
    return memo(value, [&described](const int& v) {
      ++described;
      return group().key("memoized").translateX((float)v);
    });
  };
  const Node first = build(3);
  ASSERT_TRUE(first.node()->memo.has_value());
  EXPECT_EQ(described, 0);  // the describe is deferred until it is resolved

  const Memo& shell = *first.node()->memo;
  const Node same = build(3);
  const Node other = build(4);
  ASSERT_TRUE(same.node()->memo.has_value());
  ASSERT_TRUE(other.node()->memo.has_value());
  EXPECT_TRUE(shell.equal(shell.properties, same.node()->memo->properties));
  EXPECT_FALSE(shell.equal(shell.properties, other.node()->memo->properties));
  const Node produced = shell.invoke(shell.properties);
  EXPECT_EQ(described, 1);
  EXPECT_EQ(produced.node()->key, "memoized");
}
