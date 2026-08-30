#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/points/Points.h>

#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

#include "sigilusd/Usd.h"

using namespace sigil;

namespace {

std::filesystem::path scratch(const char* name) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "sigilusd_test";
  std::filesystem::create_directories(dir);
  return dir / name;
}

sk_sp<SkImage> solid(SkColor color) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  surface->getCanvas()->clear(color);
  return surface->makeImageSnapshot();
}

}  // namespace

TEST(Usd, RoundTripsAMeshWithSlotsAndMaterials) {
  // A torus wearing two material slots — one textured — goes out as a
  // crate and comes back as a Model: same triangles (unwelded), a
  // "Material" lane naming two slots, both materials found, the
  // texture's bytes read from the file written beside the stage.
  const std::filesystem::path file = scratch("slots.usdc");
  geometry::Mesh torus = geometry::mesh::torus(100, 40, 24, 12);
  std::vector<glm::vec4>& lane = torus.prim("Material", {0, 0, 0, 0});
  for (size_t t = 0; t < lane.size(); ++t) lane[t] = {(float)(t % 2), 0, 0, 0};
  world::Material red;
  red.baseColor = {1, 0, 0, 1};
  red.roughness = 0.3f;
  red.metallic = 0.75f;
  world::Material tex;
  tex.texture = solid(SK_ColorBLUE);
  tex.tile = true;
  {
    usd::Writer writer(file);
    const std::string path =
        writer.mesh("ring", torus,
                    glm::translate(glm::mat4(1.0f), {10, 20, 30}), {red, tex});
    EXPECT_EQ(path, "/World/ring");
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
  ASSERT_TRUE(std::filesystem::exists(file));
  // Crate, not text: the magic bytes.
  {
    std::ifstream in(file, std::ios::binary);
    char magic[8] = {};
    in.read(magic, 8);
    EXPECT_EQ(std::string(magic, 8), std::string("PXR-USDC"));
  }
  usd::ReadInfo info;
  std::string error;
  const std::optional<geometry::import::Model> back =
      usd::readModel(file, &info, &error);
  ASSERT_TRUE(back) << error;
  ASSERT_EQ(back->parts.size(), 1u);
  const geometry::import::Part& part = back->parts.front();
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
  // Materials: subset 0 is red with its scalars; the texture's PNG was
  // written and is readable.
  EXPECT_FLOAT_EQ(part.roughness, 0.3f);
  EXPECT_FLOAT_EQ(part.metallic, 0.75f);
  EXPECT_FLOAT_EQ(part.baseColor.r, 1.0f);
  const std::filesystem::path textures = file.parent_path() / "slots_textures";
  EXPECT_TRUE(std::filesystem::exists(textures));
  int pngs = 0;
  for (const auto& entry : std::filesystem::directory_iterator(textures))
    pngs += entry.path().extension() == ".png";
  EXPECT_EQ(pngs, 1);
}

TEST(Usd, WritesAsciiStampsLightsAndCameraAndReadsInstancers) {
  const std::filesystem::path file = scratch("scene.usda");
  geometry::Cloud cloud;
  for (int i = 0; i < 50; ++i) cloud.positions.push_back({(float)i, 0, 0});
  cloud.scalar("size", 2);
  cloud.vector("normal", {0, 1, 0});
  {
    usd::Writer writer(file);
    world::Material glow;
    glow.emissive = {1, 0.5f, 0, 1};
    glow.emissiveStrength = 2;
    EXPECT_EQ(writer.stamps("sparks", cloud, geometry::mesh::quad(4, 4),
                            glm::mat4(1.0f), glow),
              "/World/sparks");
    world::LightComponent point;
    point.type = world::LightComponent::Type::Point;
    point.position = {0, 100, 0};
    EXPECT_EQ(writer.light("lamp", point), "/World/lamp");
    world::Lighting lighting;
    EXPECT_EQ(writer.sun("sun", lighting), "/World/sun");
    geometry::space::Camera camera;
    camera.eye = {0, 0, 300};
    EXPECT_EQ(writer.camera("eye", camera), "/World/eye");
    // A second prop with the same name gets a unique path.
    EXPECT_EQ(writer.mesh("sparks", geometry::mesh::quad(1, 1), glm::mat4(1.0f),
                          world::Material{}),
              "/World/sparks_2");
    ASSERT_TRUE(writer.save());
  }
  {
    std::ifstream in(file);
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    EXPECT_NE(text.find("#usda"), std::string::npos) << "ascii";
    EXPECT_NE(text.find("PointInstancer"), std::string::npos);
    EXPECT_NE(text.find("SphereLight"), std::string::npos);
    EXPECT_NE(text.find("DistantLight"), std::string::npos);
    EXPECT_NE(text.find("Camera"), std::string::npos);
    EXPECT_NE(text.find("UsdPreviewSurface"), std::string::npos);
    EXPECT_NE(text.find("emissiveColor"), std::string::npos);
  }
  const std::optional<geometry::import::Model> back = usd::readModel(file);
  ASSERT_TRUE(back);
  // The instancer's positions come back as a faceless part with its
  // sizes; the prototype and the second quad as meshes.
  const geometry::import::Part* sparks = nullptr;
  for (const geometry::import::Part& part : back->parts)
    if (part.name == "sparks") sparks = &part;
  ASSERT_TRUE(sparks);
  EXPECT_EQ(sparks->mesh.vertexCount(), 50u);
  EXPECT_EQ(sparks->mesh.triangleCount(), 0u);
  ASSERT_TRUE(sparks->scalarLanes.count("size"));
  EXPECT_FLOAT_EQ(sparks->scalarLanes.at("size")[3], 2.0f);
}

TEST(Usd, RefusesWhatItCannotOpen) {
  std::string error;
  EXPECT_FALSE(usd::readModel(scratch("nope.usdc"), &error));
  EXPECT_FALSE(error.empty());
}
