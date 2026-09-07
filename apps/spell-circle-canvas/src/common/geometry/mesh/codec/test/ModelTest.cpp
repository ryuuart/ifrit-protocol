/** @file
 * The Model operations over whatever reader made it: merging parts bakes
 * each one's base colour into a vertex lane, merging clouds concatenates
 * their lanes across parts, the fit transform centres and scales, and a
 * material slot rides the primitive class.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/Mesh.h"
#include "sigilgeometry/mesh/codec/Decode.h"
#include "sigilgeometry/mesh/pop/Pop.h"
#include "support/GeometrySupport.h"

using namespace sigil::geometry;
using namespace sigil::geometry::mesh;

using codec::decode::Model;
using codec::decode::Part;

using sigil::geometry::test::kCubeObj;
using sigil::geometry::test::splitQuad;

// merged() flattens a multi-part model into one Mesh, and a part's material
// base colour is the only thing that would be lost by that — so it is baked
// into the vertex colour lane, per part, as the merge happens.
TEST(Model, MergingBakesEachPartsBaseColourIntoAVertexLane) {
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.baseColor = {1, 0, 0, 1};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.baseColor = {0, 1, 0, 1};
  model.parts = {a, b};
  const Mesh merged = model.merged();
  EXPECT_EQ(merged.vertexCount(), 8u);
  ASSERT_EQ(merged.colors.size(), 8u);
  EXPECT_FLOAT_EQ(merged.colors.front().r, 1);
  EXPECT_FLOAT_EQ(merged.colors.back().g, 1);
}

TEST(Model, MergingACloudConcatenatesItsLanesAcrossParts) {
  // Disjoint lanes across parts: each side's values land at its own
  // offset, and the other side pads with the lane's default.
  Model model;
  Part a;
  a.mesh = mesh::quad(2, 2);
  a.scalarLanes["energy"] = {1, 2, 3, 4};
  Part b;
  b.mesh = mesh::quad(2, 2);
  b.colorLanes["heat"].assign(4, {1, 0, 0, 1});
  model.parts = {a, b};
  const Cloud merged = model.mergedCloud();
  ASSERT_EQ(merged.size(), 8u);
  const std::vector<float>* energy = merged.scalarIf("energy");
  ASSERT_TRUE(energy);
  ASSERT_EQ(energy->size(), 8u);
  EXPECT_FLOAT_EQ((*energy)[0], 1.0f);
  EXPECT_FLOAT_EQ((*energy)[3], 4.0f);
  EXPECT_FLOAT_EQ((*energy)[4], 0.0f);  // b's side pads scalar 0
  EXPECT_FLOAT_EQ((*energy)[7], 0.0f);
  const std::vector<glm::vec4>* heat = merged.colorIf("heat");
  ASSERT_TRUE(heat);
  ASSERT_EQ(heat->size(), 8u);
  EXPECT_FLOAT_EQ((*heat)[0].r, 1.0f);  // a's side pads white
  EXPECT_FLOAT_EQ((*heat)[0].g, 1.0f);
  EXPECT_FLOAT_EQ((*heat)[4].r, 1.0f);  // b's red from offset 4
  EXPECT_FLOAT_EQ((*heat)[4].g, 0.0f);
}

TEST(Model, TheFitTransformCentresAndScalesWhateverItIsGiven) {
  Model model;
  Part part;
  part.mesh = mesh::quad(4, 2);
  part.mesh.transform(glm::translate(glm::mat4(1.0f), {100, 50, 0}));
  model.parts = {part};
  Mesh fitted = model.parts.front().mesh;
  fitted.transform(model.fitTransform(100));
  glm::vec3 lo, hi;
  fitted.bounds(&lo, &hi);
  // fitTransform(n) is uniform: it scales so the LARGEST extent becomes n
  // and recentres on the origin, leaving the aspect ratio alone. A
  // per-axis fit would have stretched the 4x2 quad to 100x100.
  EXPECT_NEAR(hi.x - lo.x, 100, 1e-3);
  EXPECT_NEAR(hi.y - lo.y, 50, 1e-3);
  EXPECT_NEAR(lo.x + hi.x, 0, 1e-3);  // centered
  EXPECT_NEAR(lo.y + hi.y, 0, 1e-3);
}

TEST(Model, AMaterialSlotRidesThePrimitiveClass) {
  // A .geo with a string shop_materialpath per primitive lands as the
  // "Material" prim lane by string-table index; the fetched Avocado (one
  // material) names slot 0 and merged() keeps the lane.
  const char* geo = R"([
    "fileversion","20.5.278","pointcount",4,"vertexcount",6,"primitivecount",2,
    "topology",["pointref",["indices",[0,1,2,0,2,3]]],
    "attributes",["pointattributes",[
      [["scope","public","type","numeric","name","P","options",{}],
       ["size",3,"storage","fpreal32","values",["size",3,"storage","fpreal32",
        "tuples",[[0,0,0],[1,0,0],[1,1,0],[0,1,0]]]]]],
     "primitiveattributes",[
      [["scope","public","type","string","name","shop_materialpath","options",{}],
       ["size",1,"storage","int32","strings",["/mat/steel","/mat/glass"],
        "indices",["size",1,"storage","int32","arrays",[[1,0]]]]]]],
    "primitives",[[["type","Polygon"],["vertex",[0,1,2],"closed",true]],
                  [["type","Polygon"],["vertex",[3,4,5],"closed",true]]]
  ])";
  const std::optional<codec::decode::Model> model =
      codec::decode::model(geo, std::strlen(geo), "slots.geo");
  ASSERT_TRUE(model);
  const std::vector<glm::vec4>* lane =
      model->parts.front().mesh.primIf("Material");
  ASSERT_TRUE(lane);
  ASSERT_EQ(lane->size(), 2u);
  EXPECT_FLOAT_EQ((*lane)[0].x, 1.0f);  // "/mat/glass"
  EXPECT_FLOAT_EQ((*lane)[1].x, 0.0f);  // "/mat/steel"

  std::filesystem::path found;
  for (const std::filesystem::path& candidate :
       {std::filesystem::path("assets/models/Avocado.glb"),
        std::filesystem::path("build/assets/models/Avocado.glb"),
        std::filesystem::path("../build/assets/models/Avocado.glb"),
        std::filesystem::path("../../build/assets/models/Avocado.glb")})
    if (std::filesystem::exists(candidate)) found = candidate;
  if (found.empty()) return;  // the .geo half already stands
  const std::optional<codec::decode::Model> avocado =
      codec::decode::model(found);
  ASSERT_TRUE(avocado);
  EXPECT_EQ(avocado->materialSlotCount(), 1);
  EXPECT_EQ(avocado->parts.front().materialIndex, 0);
  const Mesh merged = avocado->merged();
  const std::vector<glm::vec4>* slots = merged.primIf("Material");
  ASSERT_TRUE(slots);
  EXPECT_EQ(slots->size(), merged.triangleCount());
}
