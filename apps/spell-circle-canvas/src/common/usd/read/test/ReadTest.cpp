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
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilusd/read/Reader.h>
#include <sigilusd/runtime/Runtime.h>
#include <sigilusd/write/Writer.h>
#include <sigilworld/World.h>

#include <cmath>
#include <cstring>
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

const world::PlacedLight* lightAt(const std::vector<usd::ReadLight>& lights,
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

/** @p light standing at @p position, as a frame keeps one. */
world::PlacedLight placed(const world::Light& light,
                          glm::vec3 position = {0, 0, 0}) {
  return {world::held(light.description()), position, ""};
}

world::PlacedLight sunLight() {
  return placed(world::directionalLight(
      {-0.45f, -0.75f, -0.5f},
      {.color = material::Color{1, 0.9f, 0.8f, 1}, .intensity = 2.5f}));
}

world::PlacedLight lampLight() {
  return placed(world::pointLight({.color = material::Color{0.2f, 0.4f, 1, 1},
                                   .intensity = 3.0f,
                                   .range = 250.0f}),
                {10, 100, -20});
}

world::PlacedLight beamLight() {
  world::Light beam = world::spotLight(
      {0, -1, 0}, {.color = material::Color{1, 0.5f, 0, 1},
                   .intensity = 4.0f,
                   .range = 500.0f,
                   .innerAngle = 28.0f,
                   .outerAngle = 40.0f,
                   .castShadow = true,
                   .shadow = {.bias = 2.0f, .radius = 3.0f, .mapSize = 1024}});
  beam.lightLink(world::Collection{.include = {"props"}, .exclude = {"/rig"}});
  return placed(beam, {0, 80, 0});
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

/** A sky whose brightest texel is well above one, which is the case the
 *  writer normalises: it divides the panorama by its peak and multiplies
 *  that peak into the light's strength. */
material::EnvironmentMap overbrightSky() {
  return material::EnvironmentMap::baked(64, [](float u, float v) -> glm::vec3 {
    const float sun = std::exp(-((u - 0.5f) * (u - 0.5f)) / 0.002f) *
                      std::exp(-((v - 0.45f) * (v - 0.45f)) / 0.004f);
    return {0.05f + 2.6f * sun, 0.10f + 1.6f * sun, 0.30f + 0.7f * sun};
  });
}

/** A quarter turn about the vertical, as a frame carries it: the matrix
 *  that takes a world direction INTO the panorama's frame. */
glm::mat3 quarterTurn() {
  return glm::mat3(
      glm::rotate(glm::mat4(1.0f), -1.5707963f, glm::vec3(0, 1, 0)));
}

world::Environment writtenSky() {
  world::Environment sky;
  sky.map = overbrightSky();
  sky.intensity = kSkyIntensity;
  sky.diffuse = 0.7f;
  sky.specular = 1.3f;
  sky.roughnessBias = 0.05f;
  sky.shown = 0.8f;
  sky.blur = 0.2f;
  sky.groundRadius = 400.0f;
  sky.orientation = quarterTurn();
  return sky;
}

void authorSky(const std::filesystem::path& file) {
  usd::Writer writer(file);
  ASSERT_EQ(writer.environmentMap("sky", writtenSky()), "/World/sky");
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
  const std::vector<glm::vec4>* lane = plate->mesh.primitiveIf("Material");
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

// A PNG's first eight bytes are the format's own signature, so a
// prefix compare says the bytes that came back are an image rather than
// a path the reader failed to follow.
constexpr char kPngSignature[] = "\x89PNG\r\n\x1a\n";

bool isPng(const std::vector<std::byte>& bytes) {
  if (bytes.size() < 8) return false;
  return std::memcmp(bytes.data(), kPngSignature, 8) == 0;
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
  EXPECT_TRUE(isPng(ball->textureBytes));
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

TEST(UsdRead, AnInstancerIsInstancedGeometryWithItsColoursAndScalars) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("instances.usda");
  geometry::mesh::Instances written;
  written.stamp = geometry::mesh::quad(4, 4);
  written.positions = {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}};
  written.scales = {{1, 1, 1}, {2, 2, 2}, {0.5f, 0.5f, 0.5f}};
  written.orientations = {
      glm::quat(1, 0, 0, 0),
      glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0)),
      glm::quat(1, 0, 0, 0)};
  written.colors = {{1, 0, 0, 1}, {0, 1, 0, 0.5f}, {0, 0, 1, 1}};
  written.attributes["age"] = {0.1f, 0.5f, 0.9f};
  {
    usd::Writer writer(file);
    writer.instances("sparks", written,
                     glm::translate(glm::mat4(1.0f), {0, 20, 0}),
                     material::surface::program());
    ASSERT_TRUE(writer.save());
  }
  const auto read = usd::readInstances(file);
  ASSERT_TRUE(read);
  ASSERT_EQ(read->size(), 1u);
  const geometry::mesh::Instances& instances = read->front().instances;
  EXPECT_EQ(instances.size(), 3u);
  EXPECT_EQ(instances.positions, written.positions)
      << "in the instancer's own space";
  EXPECT_FLOAT_EQ(read->front().placement[3].y, 20.0f);
  EXPECT_EQ(instances.scales, written.scales);
  ASSERT_EQ(instances.orientations.size(), 3u);
  EXPECT_NEAR(instances.orientations[1].y, written.orientations[1].y, 1e-3f);
  EXPECT_EQ(instances.colors, written.colors);
  ASSERT_NE(instances.attributeIf("age"), nullptr);
  EXPECT_FLOAT_EQ((*instances.attributeIf("age"))[1], 0.5f);
  EXPECT_EQ(instances.stamp.triangleCount(), 2u);
  EXPECT_NEAR(instances.stamp.positions.front().y,
              written.stamp.positions.front().y, 1e-4f)
      << "the stamp is in the instancer's space, not the world's";
  // The same prim is still a faceless part of positions to readModel.
  const auto model = usd::readModel(file);
  ASSERT_TRUE(model);
  const geometry::mesh::codec::decode::Part* sparks = named(*model, "sparks");
  ASSERT_TRUE(sparks);
  EXPECT_EQ(sparks->mesh.vertexCount(), 3u);
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
  const std::vector<glm::vec4>* slots = part.mesh.primitiveIf("Material");
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
  EXPECT_TRUE(isPng(part.textureBytes));
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
  const world::PlacedLight* back = lightAt(*lights, "/World/sun");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->light.kind, world::LightKind::Directional);
  const glm::vec3 aim = glm::normalize(sunLight().light.direction);
  EXPECT_NEAR(back->light.direction.x, aim.x, 1e-5f);
  EXPECT_NEAR(back->light.direction.y, aim.y, 1e-5f);
  EXPECT_NEAR(back->light.direction.z, aim.z, 1e-5f);
  EXPECT_FLOAT_EQ(back->light.intensity.value(), 2.5f);
  EXPECT_NEAR(back->light.color.value().g, 0.9f, 1e-5f);
}

TEST(UsdRead, APointLightComesBackWhereItStoodAndAsFarAsItReached) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("emitters.usdc");
  authorEmitters(file);
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;

  const world::PlacedLight* back = lightAt(*lights, "/World/lamp");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->light.kind, world::LightKind::Point);
  EXPECT_NEAR(back->position.x, 10.0f, 1e-4f);
  EXPECT_NEAR(back->position.y, 100.0f, 1e-4f);
  EXPECT_NEAR(back->position.z, -20.0f, 1e-4f);
  EXPECT_FLOAT_EQ(back->light.range, 250.0f);
  EXPECT_NEAR(back->light.intensity.value(), 3.0f, 1e-5f);
  EXPECT_NEAR(back->light.color.value().b, 1.0f, 1e-5f);
  // An unlinked light reaches everything, and comes back so.
  EXPECT_TRUE(back->light.lightLink.everything());
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
  const world::PlacedLight* back = lightAt(*lights, "/World/beam");
  ASSERT_TRUE(back);
  EXPECT_EQ(back->light.kind, world::LightKind::Spot);
  EXPECT_NEAR(back->light.direction.y, -1.0f, 1e-5f);
  EXPECT_NEAR(back->position.y, 80.0f, 1e-4f);
  EXPECT_FLOAT_EQ(back->light.outerAngle, 40.0f);
  const float softness = 1.0f - 28.0f / 40.0f;
  EXPECT_FLOAT_EQ(back->light.innerAngle, 40.0f * (1.0f - softness));
  EXPECT_FLOAT_EQ(back->light.range, 500.0f);
  // Its light link comes back term for term.
  EXPECT_EQ(back->light.lightLink, beamLight().light.lightLink);
  // …and so does its shadow: UsdLux's switch, and how it is made.
  EXPECT_TRUE(back->light.castShadow);
  EXPECT_EQ(back->light.shadow, beamLight().light.shadow);
  // A light that does not cast says so, and comes back casting nothing.
  const world::PlacedLight* sun = lightAt(*lights, "/World/sun");
  ASSERT_TRUE(sun);
  EXPECT_FALSE(sun->light.castShadow);
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

  // The sky has values above one, so the writer divided the panorama by
  // its peak and multiplied that peak into the light's strength: what
  // comes back is brighter than what went out by exactly that factor,
  // and the radiance the set is lit at is the same either way.
  EXPECT_GT(back.environment.intensity, kSkyIntensity);
  EXPECT_FLOAT_EQ(back.environment.diffuse, 0.7f);
  EXPECT_FLOAT_EQ(back.environment.specular, 1.3f);
  EXPECT_FLOAT_EQ(back.environment.roughnessBias, 0.05f);
  EXPECT_FLOAT_EQ(back.environment.shown, 0.8f);
  EXPECT_FLOAT_EQ(back.environment.blur, 0.2f);
  EXPECT_FLOAT_EQ(back.environment.groundRadius, 400.0f);
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
      EXPECT_NEAR(read->front().environment.orientation[c][r],
                  orientation[c][r], 1e-4f)
          << c << "," << r;
}

TEST(UsdRead, AnAmbientAndAHemisphereLightComeBackAsUntexturedDomes) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("domes.usdc");
  {
    usd::Writer writer(file);
    ASSERT_EQ(
        writer.light("fill", placed(world::ambientLight({.intensity = 0.4f}))),
        "/World/fill");
    ASSERT_EQ(
        writer.light("sky",
                     placed(world::hemisphereLight(
                         {.color = material::Color{0.6f, 0.7f, 1, 1},
                          .groundColor = material::Color{0.3f, 0.2f, 0.1f, 1},
                          .intensity = 0.8f}))),
        "/World/sky");
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;
  ASSERT_EQ(lights->size(), 2u);
  const world::PlacedLight* fill = lightAt(*lights, "/World/fill");
  ASSERT_TRUE(fill);
  EXPECT_EQ(fill->light.kind, world::LightKind::Ambient);
  EXPECT_FLOAT_EQ(fill->light.intensity.value(), 0.4f);
  const world::PlacedLight* sky = lightAt(*lights, "/World/sky");
  ASSERT_TRUE(sky);
  EXPECT_EQ(sky->light.kind, world::LightKind::Hemisphere);
  EXPECT_FLOAT_EQ(sky->light.intensity.value(), 0.8f);
  EXPECT_NEAR(sky->light.color.value().g, 0.7f, 1e-5f);
  EXPECT_NEAR(sky->light.groundColor.value().r, 0.3f, 1e-5f);
  EXPECT_NEAR(sky->light.direction.y, 1.0f, 1e-5f);
  // Neither is an environment: a dome with no picture is light, not a sky.
  const std::optional<std::vector<usd::ReadEnvironment>> skies =
      usd::readEnvironments(file, &error);
  ASSERT_TRUE(skies) << error;
  EXPECT_TRUE(skies->empty());
}

TEST(UsdRead, ReadsALightAndACameraAnotherToolAuthored) {
  SKIP_WITHOUT_USD();
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(asset("foreign.usda"), &error);
  ASSERT_TRUE(lights) << error;
  ASSERT_EQ(lights->size(), 1u);
  EXPECT_EQ(lights->front().path, "/World/rig/key");
  const world::LightDescription& key = lights->front().light.light;
  const glm::vec3 position = lights->front().light.position;
  // A shaping cone and no sigil data: a spot whose range is the default,
  // standing where its parent Xform puts it and aimed by its own
  // rotation.
  EXPECT_EQ(key.kind, world::LightKind::Spot);
  EXPECT_NEAR(position.y, 200.0f, 1e-4f);
  EXPECT_NEAR(key.direction.y, -1.0f, 1e-5f);
  EXPECT_FLOAT_EQ(key.outerAngle, 30.0f);
  EXPECT_FLOAT_EQ(key.innerAngle, 30.0f * (1.0f - 0.25f));
  // Its surface radiance of three over a sphere of radius one delivers
  // three pi at a distance of one, and its linear blue is blue.
  EXPECT_NEAR(key.intensity.value(), 3.0f * 3.14159265f, 1e-4f);
  EXPECT_NEAR(key.color.value().b, 1.0f, 1e-5f);
  EXPECT_FLOAT_EQ(key.range, world::LightDescription{}.range);

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

TEST(UsdRead, SkipsAMeshWhoseTopologyDoesNotAddUp) {
  SKIP_WITHOUT_USD();
  // Two meshes no reader can fan: one names a point that is not there,
  // the other's counts account for more face-vertices than it has. Both
  // are skipped like a mesh with no points at all, and the sound one
  // beside them still comes through.
  const std::filesystem::path file = scratch("broken.usda");
  {
    std::ofstream out(file);
    out << "#usda 1.0\n(\n    defaultPrim = \"World\"\n)\n\n"
           "def Xform \"World\"\n{\n"
           "    def Mesh \"past_the_points\"\n    {\n"
           "        int[] faceVertexCounts = [3]\n"
           "        int[] faceVertexIndices = [0, 1, 9]\n"
           "        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]\n"
           "    }\n"
           "    def Mesh \"counts_that_lie\"\n    {\n"
           "        int[] faceVertexCounts = [3, 3]\n"
           "        int[] faceVertexIndices = [0, 1, 2]\n"
           "        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]\n"
           "    }\n"
           "    def Mesh \"sound\"\n    {\n"
           "        int[] faceVertexCounts = [3]\n"
           "        int[] faceVertexIndices = [0, 1, 2]\n"
           "        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]\n"
           "    }\n}\n";
  }
  std::string error;
  const std::optional<geometry::mesh::codec::decode::Model> model =
      usd::readModel(file, nullptr, &error);
  ASSERT_TRUE(model) << error;
  EXPECT_FALSE(named(*model, "past_the_points"));
  EXPECT_FALSE(named(*model, "counts_that_lie"));
  const geometry::mesh::codec::decode::Part* sound = named(*model, "sound");
  ASSERT_TRUE(sound);
  EXPECT_EQ(sound->mesh.triangleCount(), 1u);
}

TEST(UsdRead, RefusesWhatItCannotOpen) {
  SKIP_WITHOUT_USD();
  std::string error;
  EXPECT_FALSE(usd::readModel(scratch("nope.usdc"), &error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(usd::readLights(scratch("nope.usdc")));
  EXPECT_FALSE(usd::readCameras(scratch("nope.usdc")));
  EXPECT_FALSE(usd::readInstances(scratch("nope.usdc")));
}

TEST(UsdRead, LightsWithASizeComeBackAsTheirShapeWithTheirSizeAndStrength) {
  SKIP_WITHOUT_USD();
  const std::filesystem::path file = scratch("sized.usdc");
  {
    usd::Writer writer(file);
    world::PlacedLight panel;
    panel.light.kind = world::LightKind::Rect;
    panel.light.direction = {0, -1, 0};
    panel.light.width = 4;
    panel.light.height = 2;
    panel.light.intensity = 5.0f;
    panel.position = {1, 10, 2};
    panel.across = {0, 0, 1};
    world::PlacedLight ball;
    ball.light.kind = world::LightKind::Sphere;
    ball.light.radius = 0.5f;
    ball.light.intensity = 30.0f;
    ball.position = {0, 3, 0};
    world::PlacedLight disk;
    disk.light.kind = world::LightKind::Disk;
    disk.light.direction = {1, 0, 0};
    disk.light.radius = 2;
    disk.light.intensity = 12.0f;
    ASSERT_EQ(writer.light("panel", panel), "/World/panel");
    ASSERT_EQ(writer.light("ball", ball), "/World/ball");
    ASSERT_EQ(writer.light("disk", disk), "/World/disk");
    std::string error;
    ASSERT_TRUE(writer.save(&error)) << error;
  }
  std::string error;
  const std::optional<std::vector<usd::ReadLight>> lights =
      usd::readLights(file, &error);
  ASSERT_TRUE(lights) << error;

  const world::PlacedLight* panel = lightAt(*lights, "/World/panel");
  ASSERT_TRUE(panel);
  EXPECT_EQ(panel->light.kind, world::LightKind::Rect);
  EXPECT_FLOAT_EQ(panel->light.width, 4);
  EXPECT_FLOAT_EQ(panel->light.height, 2);
  EXPECT_NEAR(panel->light.intensity.value(), 5.0f, 1e-5f);
  EXPECT_NEAR(panel->light.direction.y, -1.0f, 1e-5f);
  EXPECT_NEAR(std::abs(panel->across.z), 1.0f, 1e-5f);
  EXPECT_NEAR(panel->position.y, 10.0f, 1e-4f);

  const world::PlacedLight* ball = lightAt(*lights, "/World/ball");
  ASSERT_TRUE(ball);
  EXPECT_EQ(ball->light.kind, world::LightKind::Sphere);
  EXPECT_FLOAT_EQ(ball->light.radius, 0.5f);
  EXPECT_NEAR(ball->light.intensity.value(), 30.0f, 1e-3f);

  const world::PlacedLight* disk = lightAt(*lights, "/World/disk");
  ASSERT_TRUE(disk);
  EXPECT_EQ(disk->light.kind, world::LightKind::Disk);
  EXPECT_FLOAT_EQ(disk->light.radius, 2);
  EXPECT_NEAR(disk->light.intensity.value(), 12.0f, 1e-3f);
  EXPECT_NEAR(disk->light.direction.x, 1.0f, 1e-5f);
}
