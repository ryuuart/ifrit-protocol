/** @file
 * usd_write_test — what the Writer authors, read back through USD's own
 * API: prim paths and their uniqueness, the mesh's attributes and
 * subsets, the material prims and their texture files, the instancer,
 * the lights, the camera, and the file formats an extension selects.
 * Skips when the USD runtime is absent.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
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
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace sigil;

namespace {

std::filesystem::path scratch(const char* name) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilusd_write_test";
  std::filesystem::create_directories(dir);
  return dir / name;
}

sk_sp<SkImage> solid(SkColor color) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  surface->getCanvas()->clear(color);
  return surface->makeImageSnapshot();
}

#define SKIP_WITHOUT_USD()                                \
  do {                                                    \
    std::string why;                                      \
    if (!usd::runtime::available(&why))                   \
      GTEST_SKIP() << "USD runtime unavailable: " << why; \
  } while (0)

}  // namespace

TEST(UsdWrite, AuthorsAMeshWithSubsetsAndMaterials) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("slots.usdc");
  geometry::mesh::Mesh torus = geometry::mesh::torus(100, 40, 24, 12);
  std::vector<glm::vec4>& lane = torus.prim("Material", {0, 0, 0, 0});
  for (size_t t = 0; t < lane.size(); ++t) lane[t] = {(float)(t % 2), 0, 0, 0};
  material::kit::SurfaceParams redParams;
  redParams.baseColor = {1, 0, 0, 1};
  redParams.roughness = 0.3f;
  const material::Material red = material::kit::surface(redParams);
  material::Material tex = material::kit::surface();
  tex.child(
      material::kit::kBaseColorSlot,
      material::Texture::of(solid(SK_ColorBLUE)).tile(SkTileMode::kRepeat));
  {
    usd::Writer writer(file);
    const std::string path =
        writer.mesh("ring", torus,
                    glm::translate(glm::mat4(1.0f), {10, 20, 30}), {red, tex});
    EXPECT_EQ(path, "/World/ring");
    // The same material again binds the same prim: one image file.
    EXPECT_EQ(writer.mesh("ring", torus, glm::mat4(1.0f), {red, tex}),
              "/World/ring_2");
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
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
  UsdGeomMesh mesh(stage->GetPrimAtPath(SdfPath("/World/ring")));
  ASSERT_TRUE(mesh);
  VtVec3fArray points;
  VtIntArray counts;
  mesh.GetPointsAttr().Get(&points);
  mesh.GetFaceVertexCountsAttr().Get(&counts);
  EXPECT_EQ(points.size(), torus.vertexCount());
  EXPECT_EQ(counts.size(), torus.triangleCount());
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
  EXPECT_EQ(faces, torus.triangleCount());
  EXPECT_NE(bound[0], bound[1]);
  EXPECT_EQ(bound[0].rfind("/World/Materials/", 0), 0u);
  // The second placement shares both material prims.
  UsdGeomMesh again(stage->GetPrimAtPath(SdfPath("/World/ring_2")));
  ASSERT_TRUE(again);
  const std::vector<UsdGeomSubset> subsetsAgain = UsdGeomSubset::GetGeomSubsets(
      again, UsdGeomTokens->face, UsdShadeTokens->materialBind);
  ASSERT_EQ(subsetsAgain.size(), 2u);
  EXPECT_EQ(UsdShadeMaterialBindingAPI(subsetsAgain[0].GetPrim())
                .ComputeBoundMaterial()
                .GetPath()
                .GetString(),
            bound[0]);
  // The texture's PNG was written once, beside the stage.
  const std::filesystem::path textures = file.parent_path() / "slots_textures";
  ASSERT_TRUE(std::filesystem::exists(textures));
  int pngs = 0;
  for (const auto& entry : std::filesystem::directory_iterator(textures))
    pngs += entry.path().extension() == ".png";
  EXPECT_EQ(pngs, 1);
}

TEST(UsdWrite, AuthorsStampsLightsAndCameraAsAscii) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("scene.usda");
  geometry::mesh::Cloud cloud;
  for (int i = 0; i < 50; ++i) cloud.positions.emplace_back((float)i, 0, 0);
  cloud.scalar("size", 2);
  cloud.vector("normal", {0, 1, 0});
  {
    usd::Writer writer(file);
    material::kit::SurfaceParams glowParams;
    glowParams.emissive = {1, 0.5f, 0, 1};
    glowParams.emissiveStrength = 2;
    const material::Material glow = material::kit::surface(glowParams);
    EXPECT_EQ(writer.stamps("sparks", cloud, geometry::mesh::quad(4, 4),
                            glm::mat4(1.0f), glow),
              "/World/sparks");
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
    // Names are sanitized: punctuation and spaces become underscores.
    EXPECT_EQ(writer.mesh("2nd prop!", geometry::mesh::quad(1, 1),
                          glm::mat4(1.0f), material::kit::surface()),
              "/World/_2nd_prop_");
    ASSERT_TRUE(writer.save());
  }
  {
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

TEST(UsdWrite, RefusesAPathItCannotWrite) {
  SKIP_WITHOUT_USD();
  usd::Writer writer("/nonexistent-root/deep/stage.usdc");
  std::string error;
  EXPECT_FALSE(writer.save(&error));
  EXPECT_FALSE(error.empty());
}
