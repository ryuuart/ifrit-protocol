/** @file
 * usd_read_test — the runtime probe every case here skips on, a
 * hand-authored ASCII stage read into a Model (the unweld, the fan, the
 * baked xform, subsets as the "Material" lane, a material's factors and
 * texture bytes, an instancer's sizes), a stage the Writer produced read
 * back — its meshes, its three kinds of emitter, its camera and its dome
 * light — a stage as another tool would author it, and a file that
 * cannot be opened.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <string>
#include <vector>

#include "Fixture.h"

using namespace sigil;
using namespace sigil::usd::test;

namespace {

std::filesystem::path asset(const char* name) {
  return std::filesystem::path(SIGIL_TEST_ASSET_DIR) / name;
}

/** A committed fixture stage read into a Model, with what the read
 *  reported about it. */
struct Stage {
  usd::ReadInfo info;
  std::optional<geometry::mesh::codec::decode::Model> model;
};

Stage readAsset(const char* name) {
  Stage stage;
  std::string error;
  stage.model = usd::readModel(asset(name), &stage.info, &error);
  EXPECT_TRUE(stage.model) << error;
  return stage;
}

const world::light::Light* lightAt(const std::vector<usd::ReadLight>& lights,
                                   const char* path) {
  for (const usd::ReadLight& read : lights)
    if (read.path == path) return &read.light;
  return nullptr;
}

const geometry::mesh::codec::decode::Part* named(
    const geometry::mesh::codec::decode::Model& model, const char* name) {
  for (const geometry::mesh::codec::decode::Part& part : model.parts)
    if (part.name == name) return &part;
  return nullptr;
}

world::light::Light sunLight() {
  return world::light::sun({-0.45f, -0.75f, -0.5f}, {1, 0.9f, 0.8f, 1}, 2.5f);
}

world::light::Light lampLight() {
  return world::light::point({10, 100, -20}, {0.2f, 0.4f, 1, 1}, 3.0f, 250.0f);
}

world::light::Light beamLight() {
  return world::light::spot({0, 80, 0}, {0, -1, 0}, 40.0f, 28.0f,
                            {1, 0.5f, 0, 1}, 4.0f, 500.0f);
}

geometry::mesh::camera::Camera eyeCamera() {
  geometry::mesh::camera::Camera camera;
  camera.eye = {120, 80, 300};
  camera.target = {0, 20, -40};
  camera.fovYDeg = 55;
  camera.zNear = 2;
  camera.zFar = 2000;
  return camera;
}

/** The three kinds of emitter and a camera, authored into @p file. */
void authorEmitters(const std::filesystem::path& file) {
  usd::Writer writer(file);
  ASSERT_EQ(writer.light("sun", sunLight()), "/World/sun");
  ASSERT_EQ(writer.light("lamp", lampLight()), "/World/lamp");
  ASSERT_EQ(writer.light("beam", beamLight()), "/World/beam");
  ASSERT_EQ(writer.camera("eye", eyeCamera()), "/World/eye");
  std::string error;
  ASSERT_TRUE(writer.save(&error)) << error;
}

/** The strength the sky is written with, so what comes back can be
 *  compared against it without building the panorama again. */
constexpr float kSkyIntensity = 1.5f;

world::Environment sunsetSky() {
  world::Environment sky;
  sky.map = material::EnvironmentMap::sunset(64);
  sky.intensity = kSkyIntensity;
  sky.tint = {0.9f, 0.95f, 1.0f};
  sky.diffuse = 0.7f;
  sky.specular = 1.3f;
  sky.roughnessBias = 0.05f;
  sky.backdrop.intensity = 0.8f;
  sky.backdrop.blur = 0.2f;
  sky.backdrop.groundRadius = 400.0f;
  return sky;
}

/** A quarter turn about the vertical, as a frame carries it: the matrix
 *  that takes a world direction INTO the panorama's frame. */
glm::mat3 quarterTurn() {
  return glm::mat3(
      glm::rotate(glm::mat4(1.0f), -1.5707963f, glm::vec3(0, 1, 0)));
}

void authorSky(const std::filesystem::path& file) {
  usd::Writer writer(file);
  ASSERT_EQ(writer.environmentMap("sky", sunsetSky(), quarterTurn()),
            "/World/sky");
  std::string error;
  ASSERT_TRUE(writer.save(&error)) << error;
}

}  // namespace

TEST(UsdRuntime, ReportsAvailabilityWithAReason) {
  std::string why;
  const bool ok = usd::available(&why);
  if (!ok) GTEST_SKIP() << "USD runtime unavailable: " << why;
  EXPECT_TRUE(why.empty()) << "no reason when nothing is missing";
  // Idempotent: the plugin registry is discovered once per process.
  EXPECT_TRUE(usd::available());
}

TEST(UsdRead, UnweldsFansAndBakesTheXformIntoTheMeshItReads) {
  SKIP_WITHOUT_USD();
  const Stage stage = readAsset("fixture.usda");
  ASSERT_TRUE(stage.model);
  ASSERT_EQ(stage.model->parts.size(), 2u);

  // The quad: two faces (a triangle and a quad) fan to three triangles,
  // unwelded to one vertex per face-vertex.
  const geometry::mesh::codec::decode::Part* plate =
      named(*stage.model, "plate");
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
}

TEST(UsdRead, SubsetsBecomeTheMaterialLaneAndTheFirstOneFillsTheFactors) {
  SKIP_WITHOUT_USD();
  const Stage stage = readAsset("fixture.usda");
  ASSERT_TRUE(stage.model);
  const geometry::mesh::codec::decode::Part* plate =
      named(*stage.model, "plate");
  ASSERT_TRUE(plate);

  // Two subsets, two materials, in the order met: the triangle's face
  // wears slot 0 and the quad's two triangles slot 1.
  const std::vector<glm::vec4>* lane = plate->mesh.primIf("Material");
  ASSERT_TRUE(lane);
  ASSERT_EQ(lane->size(), 3u);
  EXPECT_FLOAT_EQ((*lane)[0].x, 0.0f);
  EXPECT_FLOAT_EQ((*lane)[1].x, 1.0f);
  EXPECT_FLOAT_EQ((*lane)[2].x, 1.0f);
  ASSERT_EQ(stage.info.materialNames.size(), 2u);
  EXPECT_EQ(stage.info.materialNames[0], "/World/Materials/red");
  EXPECT_EQ(stage.info.materialNames[1], "/World/Materials/checker");
  // The first subset's material fills the factors when the mesh as a
  // whole binds none.
  EXPECT_FLOAT_EQ(plate->baseColor.r, 1.0f);
  EXPECT_FLOAT_EQ(plate->baseColor.g, 0.0f);
  EXPECT_FLOAT_EQ(plate->roughness, 0.25f);
  EXPECT_FLOAT_EQ(plate->metallic, 0.75f);
  EXPECT_FLOAT_EQ(plate->transmission, 0.5f);
}

TEST(UsdRead, APrimBindingOneMaterialCarriesTheImagesBesideTheStage) {
  SKIP_WITHOUT_USD();
  const Stage stage = readAsset("fixture.usda");
  ASSERT_TRUE(stage.model);
  // The textured sphere binds one material over the whole mesh: the
  // PNG beside the stage is read into the part.
  const geometry::mesh::codec::decode::Part* ball = named(*stage.model, "ball");
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
  const Torus torus = twoSlotTorus();
  {
    usd::Writer writer(file);
    writer.mesh("ring", torus.mesh,
                glm::translate(glm::mat4(1.0f), {10, 20, 30}),
                {torus.red, torus.textured});
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
  EXPECT_EQ(part.mesh.triangleCount(), torus.mesh.triangleCount());
  EXPECT_EQ(part.mesh.vertexCount(),
            torus.mesh.triangleCount() * 3);  // unwelded
  const std::vector<glm::vec4>* slots = part.mesh.primIf("Material");
  ASSERT_TRUE(slots);
  int ones = 0;
  for (const glm::vec4& v : *slots) ones += v.x > 0.5f;
  EXPECT_EQ((size_t)ones, torus.mesh.triangleCount() / 2);
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

TEST(UsdRead, APackageIsOneFileAndTheModelComesBackOutOfIt) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("packed.usdz");
  const std::filesystem::path textures = file.parent_path() / "packed_textures";
  std::error_code ec;
  std::filesystem::remove_all(textures, ec);
  std::filesystem::remove(file, ec);
  const Torus torus = twoSlotTorus();
  {
    usd::Writer writer(file);
    // The textured material alone, so what comes back names the image
    // and the package is the only place the bytes can have come from.
    writer.mesh("ring", torus.mesh, glm::mat4(1.0f), torus.textured);
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
  // A zip, not a layer: the archive's own magic. A `.usdz` reaching
  // SdfLayer::Export instead would land a crate under this name.
  {
    std::ifstream in(file, std::ios::binary);
    char magic[4] = {};
    in.read(magic, 4);
    EXPECT_EQ(std::string(magic, 4), std::string("PK\x03\x04", 4));
  }
  // ONE FILE. The texture the material wears is inside the package, so
  // nothing of the stage is left standing beside it.
  EXPECT_FALSE(std::filesystem::exists(textures));
  EXPECT_FALSE(std::filesystem::exists(file.parent_path() / "packed.usdc"));

  usd::ReadInfo info;
  std::string error;
  const std::optional<geometry::mesh::codec::decode::Model> back =
      usd::readModel(file, &info, &error);
  ASSERT_TRUE(back) << error;
  ASSERT_EQ(back->parts.size(), 1u);
  const geometry::mesh::codec::decode::Part& part = back->parts.front();
  EXPECT_EQ(part.mesh.triangleCount(), torus.mesh.triangleCount());
  EXPECT_EQ(info.materialNames.size(), 1u);
  // The image the material wears, read out of the package: the file it
  // was written from is gone, so these bytes came from inside.
  EXPECT_FALSE(part.textureUri.empty());
  ASSERT_GE(part.textureBytes.size(), 8u);
  EXPECT_EQ((unsigned char)part.textureBytes[1], 'P');
  EXPECT_EQ((unsigned char)part.textureBytes[2], 'N');
  EXPECT_EQ((unsigned char)part.textureBytes[3], 'G');
}

TEST(UsdRead, ASunComesBackAimedWhereItWasPointedAndStandingNowhere) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("emitters.usdc");
  authorEmitters(file);
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;
  ASSERT_EQ(lights->size(), 3u);

  // The sun keeps its direction (normalized on the way out) and nothing
  // else: a distant light stands nowhere.
  const world::light::Light* back = lightAt(*lights, "/World/sun");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->kind, world::light::Kind::Sun);
  const glm::vec3 aim = glm::normalize(sunLight().direction);
  EXPECT_NEAR(back->direction.x, aim.x, 1e-5f);
  EXPECT_NEAR(back->direction.y, aim.y, 1e-5f);
  EXPECT_NEAR(back->direction.z, aim.z, 1e-5f);
  EXPECT_FLOAT_EQ(back->intensity, 2.5f);
  EXPECT_FLOAT_EQ(back->color.g, 0.9f);
}

TEST(UsdRead, APointLightComesBackWhereItStoodAndAsFarAsItReached) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("emitters.usdc");
  authorEmitters(file);
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;

  const world::light::Light* back = lightAt(*lights, "/World/lamp");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->kind, world::light::Kind::Point);
  EXPECT_NEAR(back->position.x, 10.0f, 1e-4f);
  EXPECT_NEAR(back->position.y, 100.0f, 1e-4f);
  EXPECT_NEAR(back->position.z, -20.0f, 1e-4f);
  EXPECT_FLOAT_EQ(back->range, 250.0f);
  EXPECT_FLOAT_EQ(back->intensity, 3.0f);
  EXPECT_FLOAT_EQ(back->color.b, 1.0f);
}

TEST(UsdRead, ASpotComesBackWithItsConeAndTheInnerEdgeTheSoftnessGivesIt) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("emitters.usdc");
  authorEmitters(file);
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;

  // The inner edge travels as the fraction of the cone the falloff eats,
  // so it comes back through that arithmetic rather than as itself.
  const world::light::Light* back = lightAt(*lights, "/World/beam");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->kind, world::light::Kind::Spot);
  EXPECT_NEAR(back->direction.y, -1.0f, 1e-5f);
  EXPECT_NEAR(back->position.y, 80.0f, 1e-4f);
  EXPECT_FLOAT_EQ(back->outerDeg, 40.0f);
  const float softness = 1.0f - 28.0f / 40.0f;
  EXPECT_FLOAT_EQ(back->innerDeg, 40.0f * (1.0f - softness));
  EXPECT_FLOAT_EQ(back->range, 500.0f);
}

TEST(UsdRead, ACameraComesBackSeeingWhatItSaw) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("emitters.usdc");
  authorEmitters(file);
  std::string error;
  const std::optional<std::vector<usd::ReadCamera>> cameras =
      usd::readCameras(file, &error);
  ASSERT_TRUE(cameras) << error;
  ASSERT_EQ(cameras->size(), 1u);
  EXPECT_EQ(cameras->front().path, "/World/eye");

  const geometry::mesh::camera::Camera camera = eyeCamera();
  const geometry::mesh::camera::Camera& lens = cameras->front().camera;
  EXPECT_NEAR(lens.eye.x, camera.eye.x, 1e-2f);
  EXPECT_NEAR(lens.eye.y, camera.eye.y, 1e-2f);
  EXPECT_NEAR(lens.eye.z, camera.eye.z, 1e-2f);
  // The focus distance carries where on the view ray the target sits.
  EXPECT_NEAR(lens.target.x, camera.target.x, 1e-2f);
  EXPECT_NEAR(lens.target.y, camera.target.y, 1e-2f);
  EXPECT_NEAR(lens.target.z, camera.target.z, 1e-2f);
  EXPECT_NEAR(lens.fovYDeg, camera.fovYDeg, 1e-3f);
  EXPECT_FLOAT_EQ(lens.zNear, camera.zNear);
  EXPECT_FLOAT_EQ(lens.zFar, camera.zFar);
  // up is orthonormalized against the view direction on the way out, so
  // the view itself is what round-trips.
  const glm::mat4 wanted = camera.view();
  const glm::mat4 got = lens.view();
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) EXPECT_NEAR(got[i][j], wanted[i][j], 1e-3f);
}

TEST(UsdRead, ADomeLightsPanoramaIsAFileBesideTheStageAndItsDialsComeBack) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("sky.usda");
  authorSky(file);
  std::string error;
  const std::optional<std::vector<usd::ReadEnvironment>> read =
      usd::readEnvironments(file, &error);
  ASSERT_TRUE(read) << error;
  ASSERT_EQ(read->size(), 1u);
  const usd::ReadEnvironment& back = read->front();
  EXPECT_EQ(back.path, "/World/sky");
  // The panorama is a file beside the stage; this library decodes none.
  EXPECT_NE(back.texture.find("_environment.png"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(file.parent_path() / back.texture));
  EXPECT_FALSE(back.environment.valid());

  // A sunset has values above one, so the writer divided the panorama by
  // its peak and multiplied that peak into the light's strength: what
  // comes back is brighter than what went out by exactly that factor,
  // and the radiance the set is lit at is the same either way.
  EXPECT_GT(back.environment.intensity, kSkyIntensity);
  EXPECT_FLOAT_EQ(back.environment.tint.x, 0.9f);
  EXPECT_FLOAT_EQ(back.environment.diffuse, 0.7f);
  EXPECT_FLOAT_EQ(back.environment.specular, 1.3f);
  EXPECT_FLOAT_EQ(back.environment.roughnessBias, 0.05f);
  EXPECT_FLOAT_EQ(back.environment.backdrop.intensity, 0.8f);
  EXPECT_FLOAT_EQ(back.environment.backdrop.blur, 0.2f);
  EXPECT_FLOAT_EQ(back.environment.backdrop.groundRadius, 400.0f);
}

TEST(UsdRead, ADomeLightsOrientationTakesAWorldDirectionIntoTheSameFrame) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("sky.usda");
  authorSky(file);
  std::string error;
  const std::optional<std::vector<usd::ReadEnvironment>> read =
      usd::readEnvironments(file, &error);
  ASSERT_TRUE(read) << error;
  ASSERT_EQ(read->size(), 1u);

  const glm::mat3 orientation = quarterTurn();
  for (int c = 0; c < 3; ++c)
    for (int r = 0; r < 3; ++r)
      EXPECT_NEAR(read->front().orientation[c][r], orientation[c][r], 1e-4f)
          << c << "," << r;
}

TEST(UsdRead, ReadsALightAndACameraAnotherToolAuthored) {
  SKIP_WITHOUT_USD();
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(asset("foreign.usda"), &error);
  ASSERT_TRUE(lights) << error;
  ASSERT_EQ(lights->size(), 1u);
  EXPECT_EQ(lights->front().path, "/World/rig/key");
  const world::light::Light& key = lights->front().light;
  // A shaping cone and no sigil data: a spot whose range is the default,
  // standing where its parent Xform puts it and aimed by its own
  // rotation.
  EXPECT_EQ(key.kind, world::light::Kind::Spot);
  EXPECT_NEAR(key.position.y, 200.0f, 1e-4f);
  EXPECT_NEAR(key.direction.y, -1.0f, 1e-5f);
  EXPECT_FLOAT_EQ(key.outerDeg, 30.0f);
  EXPECT_FLOAT_EQ(key.innerDeg, 30.0f * (1.0f - 0.25f));
  EXPECT_FLOAT_EQ(key.intensity, 3.0f);
  EXPECT_FLOAT_EQ(key.color.b, 1.0f);
  EXPECT_FLOAT_EQ(key.range, world::light::Light{}.range);

  const std::optional<std::vector<usd::ReadCamera>> cameras =
      usd::readCameras(asset("foreign.usda"), &error);
  ASSERT_TRUE(cameras) << error;
  ASSERT_EQ(cameras->size(), 1u);
  const geometry::mesh::camera::Camera& lens = cameras->front().camera;
  EXPECT_NEAR(lens.eye.y, 200.0f, 1e-4f);
  EXPECT_NEAR(lens.eye.z, 100.0f, 1e-4f);
  // No focus distance authored: the target sits one unit down the view.
  EXPECT_NEAR(lens.target.z, 99.0f, 1e-4f);
  EXPECT_NEAR(lens.fovYDeg, std::atan(12.0f / 50.0f) * 360.0f / (float)M_PI,
              1e-3f);
  EXPECT_FLOAT_EQ(lens.zNear, 1.0f);
  EXPECT_FLOAT_EQ(lens.zFar, 1000.0f);
}

TEST(UsdRead, RefusesWhatItCannotOpen) {
  SKIP_WITHOUT_USD();
  std::string error;
  EXPECT_FALSE(usd::readModel(scratch("nope.usdc"), &error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(usd::readLights(scratch("nope.usdc")));
  EXPECT_FALSE(usd::readCameras(scratch("nope.usdc")));
}
