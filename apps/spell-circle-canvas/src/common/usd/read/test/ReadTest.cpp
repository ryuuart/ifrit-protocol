/** @file
 * usd_read_test — a hand-authored ASCII stage read into a Model (the
 * unweld, the fan, the baked xform, subsets as the "Material" lane, a
 * material's factors and texture bytes, an instancer's sizes), a stage
 * the Writer produced read back, and a file that cannot be opened.
 * Skips when the USD runtime is absent.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>

using namespace sigil;

namespace {

std::filesystem::path asset(const char* name) {
  return std::filesystem::path(IFRIT_USD_TEST_ASSET_DIR) / name;
}

std::filesystem::path scratch(const char* name) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilusd_read_test";
  std::filesystem::create_directories(dir);
  return dir / name;
}

sk_sp<SkImage> solid(SkColor color) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  surface->getCanvas()->clear(color);
  return surface->makeImageSnapshot();
}

const geometry::mesh::codec::decode::Part* named(
    const geometry::mesh::codec::decode::Model& model, const char* name) {
  for (const geometry::mesh::codec::decode::Part& part : model.parts)
    if (part.name == name) return &part;
  return nullptr;
}

#define SKIP_WITHOUT_USD()                                \
  do {                                                    \
    std::string why;                                      \
    if (!usd::runtime::available(&why))                   \
      GTEST_SKIP() << "USD runtime unavailable: " << why; \
  } while (0)

}  // namespace

TEST(UsdRead, ReadsAHandAuthoredStage) {
  SKIP_WITHOUT_USD();
  usd::ReadInfo info;
  std::string error;
  const std::optional<geometry::mesh::codec::decode::Model> model =
      usd::readModel(asset("fixture.usda"), &info, &error);
  ASSERT_TRUE(model) << error;
  ASSERT_EQ(model->parts.size(), 2u);

  // The quad: two faces (a triangle and a quad) fan to three triangles,
  // unwelded to one vertex per face-vertex.
  const geometry::mesh::codec::decode::Part* plate = named(*model, "plate");
  ASSERT_TRUE(plate);
  EXPECT_EQ(plate->mesh.triangleCount(), 3u);
  EXPECT_EQ(plate->mesh.vertexCount(), 7u);
  // The parent Xform's translate is baked into the positions.
  glm::vec3 lo, hi;
  plate->mesh.bounds(&lo, &hi);
  EXPECT_FLOAT_EQ(lo.x, 100.0f);
  EXPECT_FLOAT_EQ(hi.x, 102.0f);
  EXPECT_FLOAT_EQ(lo.z, 50.0f);
  // st's v runs up the image in USD and down here.
  ASSERT_EQ(plate->mesh.uvs.size(), 7u);
  EXPECT_FLOAT_EQ(plate->mesh.uvs[0].y, 1.0f);
  // displayColor per vertex survives.
  ASSERT_EQ(plate->mesh.colors.size(), 7u);
  EXPECT_FLOAT_EQ(plate->mesh.colors[0].r, 1.0f);
  // Two subsets, two materials, in the order met: the triangle's face
  // wears slot 0 and the quad's two triangles slot 1.
  const std::vector<glm::vec4>* lane = plate->mesh.primIf("Material");
  ASSERT_TRUE(lane);
  ASSERT_EQ(lane->size(), 3u);
  EXPECT_FLOAT_EQ((*lane)[0].x, 0.0f);
  EXPECT_FLOAT_EQ((*lane)[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*lane)[2].x, 1.0f);
  ASSERT_EQ(info.materialNames.size(), 2u);
  EXPECT_EQ(info.materialNames[0], "/World/Materials/red");
  EXPECT_EQ(info.materialNames[1], "/World/Materials/checker");
  // The first subset's material fills the factors when the mesh as a
  // whole binds none.
  EXPECT_FLOAT_EQ(plate->baseColor.r, 1.0f);
  EXPECT_FLOAT_EQ(plate->baseColor.g, 0.0f);
  EXPECT_FLOAT_EQ(plate->roughness, 0.25f);
  EXPECT_FLOAT_EQ(plate->metallic, 0.75f);
  EXPECT_FLOAT_EQ(plate->transmission, 0.5f);

  // The textured sphere binds one material over the whole mesh: the
  // PNG beside the stage is read into the part.
  const geometry::mesh::codec::decode::Part* ball = named(*model, "ball");
  ASSERT_TRUE(ball);
  EXPECT_EQ(ball->materialIndex, 1);
  EXPECT_EQ(ball->textureUri, "checker.png");
  EXPECT_FALSE(ball->textureBytes.empty());
  EXPECT_EQ((int)ball->textureBytes[1], 'P');
  ASSERT_TRUE(ball->textures.count("roughness"));
  EXPECT_FALSE(ball->textures.at("roughness").bytes.empty());
}

TEST(UsdRead, ReadsAnInstancerAsAFacelessPart) {
  SKIP_WITHOUT_USD();
  const std::optional<geometry::mesh::codec::decode::Model> model =
      usd::readModel(asset("instancer.usda"));
  ASSERT_TRUE(model);
  const geometry::mesh::codec::decode::Part* sparks = named(*model, "sparks");
  ASSERT_TRUE(sparks);
  EXPECT_EQ(sparks->mesh.vertexCount(), 3u);
  EXPECT_EQ(sparks->mesh.triangleCount(), 0u);
  ASSERT_TRUE(sparks->scalarLanes.count("size"));
  EXPECT_FLOAT_EQ(sparks->scalarLanes.at("size")[2], 3.0f);
  // The instancer's own translate is baked.
  EXPECT_FLOAT_EQ(sparks->mesh.positions[0].y, 10.0f);
  // The prototype mesh under it is a part of its own.
  EXPECT_TRUE(named(*model, "stamp"));
}

TEST(UsdRead, RoundTripsWhatTheWriterAuthors) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("slots.usdc");
  geometry::mesh::Mesh torus = geometry::mesh::torus(100, 40, 24, 12);
  std::vector<glm::vec4>& lane = torus.prim("Material", {0, 0, 0, 0});
  for (size_t t = 0; t < lane.size(); ++t) lane[t] = {(float)(t % 2), 0, 0, 0};
  material::kit::SurfaceParams redParams;
  redParams.baseColor = {1, 0, 0, 1};
  redParams.roughness = 0.3f;
  redParams.metallic = 0.75f;
  const material::Material red = material::kit::surface(redParams);
  material::Material tex = material::kit::surface();
  tex.child(material::kit::kBaseColorSlot,
            material::Texture::of(solid(SK_ColorBLUE)));
  {
    usd::Writer writer(file);
    writer.mesh("ring", torus, glm::translate(glm::mat4(1.0f), {10, 20, 30}),
                {red, tex});
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
  usd::ReadInfo info;
  std::string error;
  const std::optional<geometry::mesh::codec::decode::Model> back =
      usd::readModel(file, &info, &error);
  ASSERT_TRUE(back) << error;
  ASSERT_EQ(back->parts.size(), 1u);
  const geometry::mesh::codec::decode::Part& part = back->parts.front();
  EXPECT_EQ(part.mesh.triangleCount(), torus.triangleCount());
  EXPECT_EQ(part.mesh.vertexCount(), torus.triangleCount() * 3);  // unwelded
  const std::vector<glm::vec4>* slots = part.mesh.primIf("Material");
  ASSERT_TRUE(slots);
  int ones = 0;
  for (const glm::vec4& v : *slots) ones += v.x > 0.5f;
  EXPECT_EQ((size_t)ones, torus.triangleCount() / 2);
  EXPECT_EQ(info.materialNames.size(), 2u);
  // Placement baked: the mesh moved by (10, 20, 30).
  glm::vec3 lo, hi;
  part.mesh.bounds(&lo, &hi);
  EXPECT_NEAR((lo.x + hi.x) * 0.5f, 10.0f, 1.0f);
  EXPECT_NEAR((lo.y + hi.y) * 0.5f, 20.0f, 1.0f);
  EXPECT_FLOAT_EQ(part.roughness, 0.3f);
  EXPECT_FLOAT_EQ(part.metallic, 0.75f);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1.0f);
}

TEST(UsdRead, RefusesWhatItCannotOpen) {
  SKIP_WITHOUT_USD();
  std::string error;
  EXPECT_FALSE(usd::readModel(scratch("nope.usdc"), &error));
  EXPECT_FALSE(error.empty());
}
