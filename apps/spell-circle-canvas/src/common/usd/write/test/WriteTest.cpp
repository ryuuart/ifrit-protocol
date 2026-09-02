/** @file
 * usd_write_test — what the Writer authors, read back through USD's own
 * API: the file format an extension selects and the stage metrics, the
 * mesh's attributes and its subsets, the material prims and the texture
 * files behind them, the instancer, the lights and the camera, and the
 * prim paths names are sanitized into. Skips when the USD runtime is
 * absent.
 */

#include <gtest/gtest.h>
#include <pxr/base/vt/array.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/tokens.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilusd/write/Writer.h>

#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <utility>

#include "Fixture.h"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace sigil;
using namespace sigil::usd::test;

namespace {

/** The two-slot ring placed twice into @p file, the second placement
 *  wearing the same two materials, with the prim paths the writer chose. */
struct AuthoredRings {
  Torus torus;
  std::string first;
  std::string second;
};

AuthoredRings authorTwoRings(const std::filesystem::path& file) {
  Torus torus = twoSlotTorus();
  usd::Writer writer(file);
  const std::string first = writer.mesh(
      "ring", torus.mesh, glm::translate(glm::mat4(1.0f), {10, 20, 30}),
      {torus.red, torus.textured});
  const std::string second = writer.mesh("ring", torus.mesh, glm::mat4(1.0f),
                                         {torus.red, torus.textured});
  std::string error;
  EXPECT_TRUE(writer.save(&error)) << error;
  return AuthoredRings{std::move(torus), first, second};
}

/** A cloud of fifty stamps along the x axis, each two units across and
 *  facing up. */
geometry::mesh::Cloud sparkCloud() {
  geometry::mesh::Cloud cloud;
  for (int i = 0; i < 50; ++i) cloud.positions.emplace_back((float)i, 0, 0);
  cloud.scalar("size", 2);
  cloud.vector("normal", {0, 1, 0});
  return cloud;
}

material::Material glowSurface() {
  material::kit::SurfaceParams glowParams;
  glowParams.emissive = {1, 0.5f, 0, 1};
  glowParams.emissiveStrength = 2;
  return material::kit::surface(glowParams);
}

}  // namespace

TEST(UsdWrite, WritesCrateBytesAndTheStageMetricsAConsumerReadsThemBy) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("slots.usdc");
  authorTwoRings(file);

  // Crate, not text: the magic bytes.
  {
    std::ifstream in(file, std::ios::binary);
    char magic[8] = {};
    in.read(magic, 8);
    EXPECT_EQ(std::string(magic, 8), std::string("PXR-USDC"));
  }
  UsdStageRefPtr stage = UsdStage::Open(file.string());
  ASSERT_TRUE(stage);
  EXPECT_EQ(UsdGeomGetStageUpAxis(stage), UsdGeomTokens->y);
  EXPECT_DOUBLE_EQ(UsdGeomGetStageMetersPerUnit(stage), 0.01);
}

TEST(UsdWrite, AMeshLandsWithItsPointsAndOneBoundSubsetPerSlot) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("slots.usdc");
  const AuthoredRings rings = authorTwoRings(file);

  UsdStageRefPtr stage = UsdStage::Open(file.string());
  ASSERT_TRUE(stage);
  UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath(rings.first)));
  ASSERT_TRUE(mesh);
  VtVec3fArray points;
  VtIntArray counts;
  mesh.GetPointsAttr().Get(&points);
  mesh.GetFaceVertexCountsAttr().Get(&counts);
  EXPECT_EQ(points.size(), rings.torus.mesh.vertexCount());
  EXPECT_EQ(counts.size(), rings.torus.mesh.triangleCount());
  // Two subsets, each bound to its own material; every face in one.
  const std::vector<UsdGeomSubset> subsets = UsdGeomSubset::GetGeomSubsets(
      mesh, UsdGeomTokens->face, UsdShadeTokens->materialBind);
  ASSERT_EQ(subsets.size(), 2u);
  size_t faces = 0;
  std::vector<std::string> bound;
  for (const UsdGeomSubset& subset : subsets) {
    VtIntArray indices;
    subset.GetIndicesAttr().Get(&indices);
    faces += indices.size();
    bound.push_back(UsdShadeMaterialBindingAPI(subset.GetPrim())
                        .ComputeBoundMaterial()
                        .GetPath()
                        .GetString());
  }
  EXPECT_EQ(faces, rings.torus.mesh.triangleCount());
  EXPECT_NE(bound[0], bound[1]);
  EXPECT_EQ(bound[0].rfind("/World/Materials/", 0), 0u);
}

TEST(UsdWrite, TheSameMaterialBindsOnePrimAndWritesOneImageFile) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("slots.usdc");
  const AuthoredRings rings = authorTwoRings(file);

  UsdStageRefPtr stage = UsdStage::Open(file.string());
  ASSERT_TRUE(stage);
  const auto boundMaterialOfFirstSubset = [&](const std::string& path) {
    UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath(path)));
    EXPECT_TRUE(mesh);
    const std::vector<UsdGeomSubset> subsets = UsdGeomSubset::GetGeomSubsets(
        mesh, UsdGeomTokens->face, UsdShadeTokens->materialBind);
    EXPECT_EQ(subsets.size(), 2u);
    if (subsets.empty()) return std::string();
    return UsdShadeMaterialBindingAPI(subsets[0].GetPrim())
        .ComputeBoundMaterial()
        .GetPath()
        .GetString();
  };
  // The second placement of the same two materials binds the same prims.
  EXPECT_EQ(boundMaterialOfFirstSubset(rings.second),
            boundMaterialOfFirstSubset(rings.first));

  // And the image they share was written once, beside the stage.
  const std::filesystem::path textures = file.parent_path() / "slots_textures";
  ASSERT_TRUE(std::filesystem::exists(textures));
  int pngs = 0;
  for (const auto& entry : std::filesystem::directory_iterator(textures))
    pngs += entry.path().extension() == ".png";
  EXPECT_EQ(pngs, 1);
}

TEST(UsdWrite, WritesStampsAsAPointInstancerOverOnePrototype) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("stamps.usda");
  {
    usd::Writer writer(file);
    EXPECT_EQ(writer.stamps("sparks", sparkCloud(), geometry::mesh::quad(4, 4),
                            glm::mat4(1.0f), glowSurface()),
              "/World/sparks");
    ASSERT_TRUE(writer.save());
  }
  UsdStageRefPtr stage = UsdStage::Open(file.string());
  ASSERT_TRUE(stage);
  UsdGeomPointInstancer instancer(
      stage->GetPrimAtPath(SdfPath("/World/sparks")));
  ASSERT_TRUE(instancer);
  VtVec3fArray positions, scales;
  VtQuathArray orientations;
  instancer.GetPositionsAttr().Get(&positions);
  instancer.GetScalesAttr().Get(&scales);
  instancer.GetOrientationsAttr().Get(&orientations);
  EXPECT_EQ(positions.size(), 50u);
  ASSERT_EQ(scales.size(), 50u);
  EXPECT_FLOAT_EQ(scales[7][0], 2.0f);
  EXPECT_EQ(orientations.size(), 50u);
  SdfPathVector prototypes;
  instancer.GetPrototypesRel().GetTargets(&prototypes);
  ASSERT_EQ(prototypes.size(), 1u);
  EXPECT_TRUE(UsdGeomMesh(stage->GetPrimAtPath(prototypes[0])));
}

TEST(UsdWrite, WritesAsciiWhenTheExtensionAsksForIt) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("scene.usda");
  {
    usd::Writer writer(file);
    EXPECT_EQ(writer.light("lamp", world::light::point({0, 100, 0})),
              "/World/lamp");
    EXPECT_EQ(writer.light("beam", world::light::spot({0, 80, 0}, {0, -1, 0},
                                                      40.0f, 28.0f)),
              "/World/beam");
    EXPECT_EQ(writer.light("sun", world::light::sun({-0.45f, -0.75f, -0.5f})),
              "/World/sun");
    geometry::mesh::camera::Camera camera;
    camera.eye = {0, 0, 300};
    EXPECT_EQ(writer.camera("eye", camera), "/World/eye");
    writer.mesh("glow", geometry::mesh::quad(1, 1), glm::mat4(1.0f),
                glowSurface());
    ASSERT_TRUE(writer.save());
  }
  // Every kind the writer authors, spelled out in the text.
  std::ifstream in(file);
  std::string text((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  EXPECT_NE(text.find("#usda"), std::string::npos) << "ascii";
  EXPECT_NE(text.find("SphereLight"), std::string::npos);
  EXPECT_NE(text.find("shaping:cone:angle"), std::string::npos);
  EXPECT_NE(text.find("DistantLight"), std::string::npos);
  EXPECT_NE(text.find("Camera"), std::string::npos);
  EXPECT_NE(text.find("UsdPreviewSurface"), std::string::npos);
  EXPECT_NE(text.find("emissiveColor"), std::string::npos);
}

TEST(UsdWrite, SanitizesANameIntoAPrimPathAndKeepsEveryPathUnique) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("names.usda");
  usd::Writer writer(file);
  const geometry::mesh::Mesh plate = geometry::mesh::quad(1, 1);
  EXPECT_EQ(
      writer.mesh("prop", plate, glm::mat4(1.0f), material::kit::surface()),
      "/World/prop");
  // Punctuation and spaces become underscores, and a leading digit gains
  // a leading underscore.
  EXPECT_EQ(writer.mesh("2nd prop!", plate, glm::mat4(1.0f),
                        material::kit::surface()),
            "/World/_2nd_prop_");
  // A name already taken gets a suffix rather than overwriting the prim.
  EXPECT_EQ(
      writer.mesh("prop", plate, glm::mat4(1.0f), material::kit::surface()),
      "/World/prop_2");
}

TEST(UsdWrite, RefusesAPathItCannotWrite) {
  SKIP_WITHOUT_USD();
  usd::Writer writer("/nonexistent-root/deep/stage.usdc");
  std::string error;
  EXPECT_FALSE(writer.save(&error));
  EXPECT_FALSE(error.empty());
}
