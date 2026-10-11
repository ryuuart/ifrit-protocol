/** @file
 * What the device executor answers.
 *
 * WHAT EACH CASE HOLDS IT TO is a claim about the picture that only
 * this code can falsify: a chain cooked on the device is the host's cook
 * to the bit, a readback arrives the frame after, a mask reaches the
 * selection and leaves the ground where it stood, a filter reads between
 * texels or does not, a surface that is its own light stands at its base
 * colour. Every case SKIPS where there is no Vulkan runtime to ask.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilshaders/WorldDiligent.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"
#include "ShaderTable.h"
#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

constexpr glm::ivec2 kExtent{160, 120};

/** A cube, a plate under it and a sun: enough for a depth test and a
 *  lit surface to have something to say. */
Group set() {
  namespace gm = ::sigil::geometry::mesh;
  return group({directionalLight({-0.4f, -0.8f, -0.4f}).key("sun"),
                mesh(gm::quad(300, 300))
                    .key("plate")
                    .at({0, -40, 0})
                    .rotateX(-90.0f)
                    .fill(slangPaint({0.15f, 0.16f, 0.2f, 1.0f}))
                    .collection("ground"),
                mesh(gm::superellipsoid({40, 40, 40}, 2.0f, 24, 16))
                    .key("body")
                    .fill(slangPaint({0.8f, 0.6f, 0.3f, 1.0f}))
                    .collection("glow")})
      .key("set");
}

/** One frame, rendered on @p runtime and photographed from above and in
 *  front. */
SkBitmap photograph(const Frame& frame, const Runtime& runtime) {
  return diligent::photograph(frame, runtime, kExtent);
}

Frame lit() {
  Frame frame(set());
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** A CASE ASKED OF THE DEVICE, which SKIPS where there is no Vulkan
 *  runtime to ask. */
class DeviceFrame : public testing::Test {
 protected:
  void SetUp() override {
    on = diligent::onDevice();
    if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
    runtime = on.runtime;
  }

  diligent::OnDevice on;
  Runtime runtime;
};

}  // namespace

TEST(SurfaceProgram, APipelineComesOffARecipeBody) {
  world::diligent::installSlangCompiler();
  const std::shared_ptr<material::Program> program =
      material::program(slangPaintRecipe(), material::Target::Slang,
                        material::Variant{world::diligent::kVariantLit});
  ASSERT_NE(program, nullptr);
  const auto* slang = program->as<material::slang::SlangProgram>();
  ASSERT_NE(slang, nullptr);
  // Two stages, and the recipe's own parameter reachable at an offset
  // the compiler reported rather than one the renderer guessed.
  EXPECT_FALSE(slang->compiled().vertex.empty());
  EXPECT_FALSE(slang->compiled().fragment.empty());
  ASSERT_NE(slang->compiled().uniform("baseColor"), nullptr);
  EXPECT_EQ(slang->compiled().uniform("baseColor")->bytes, 16u);
  // The lit build carries the shading the unlit one does not.
  EXPECT_NE(slang->compiled().uniform("uLightCount"), nullptr);
  const std::shared_ptr<material::Program> unlit =
      material::program(slangPaintRecipe(), material::Target::Slang);
  ASSERT_NE(unlit, nullptr);
  EXPECT_EQ(unlit->as<material::slang::SlangProgram>()->compiled().uniform(
                "uLightCount"),
            nullptr);
}

TEST(GpuRuntime, ACookedChainMatchesTheHostCook) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  namespace gm = ::sigil::geometry::mesh;
  const std::vector<glm::vec3> path = {
      {-100, 0, 0}, {0, 60, 0}, {100, 0, 0}, {0, -60, 0}};
  const geometry::mesh::pop::Chain chain =
      gm::pop::on(path).count(200).spread(6.0f);

  const auto cooked = [&](const Runtime& runtime) {
    motion::Engine engine;
    Scene scene(engine);
    Frame frame(set());
    frame.extent(kExtent)
        .defaultCamera(diligent::raisedEye())
        .runtime(runtime)
        .pass(computePass("cook").chain(chain).writes("points"))
        .pass(geometryPass("colour").reads("points").writes("colour").stamp(
            gm::superellipsoid({4, 4, 4}, 2.0f, 6, 4)));
    scene.render(frame);
    const geometry::mesh::Cloud* points = scene.targets().points("points");
    return points ? points->positions : std::vector<glm::vec3>{};
  };

  const std::vector<glm::vec3> host =
      gm::pop::cook(chain, gm::pop::Runtime::cpu()).positions;
  const std::vector<glm::vec3> graphics = cooked(on.runtime);
  ASSERT_FALSE(host.empty());
  ASSERT_EQ(host.size(), graphics.size());
  // Every operator in this chain has a kernel, so on the device tier the
  // pass cooked it THERE — and the two answers are still equal to the
  // bit, because the two tiers run one piece of arithmetic.
  for (size_t i = 0; i < host.size(); ++i) EXPECT_EQ(host[i], graphics[i]);
}

TEST(GpuRuntime, AReadbackArrivesTheFrameAfter) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  motion::Engine engine;
  Scene scene(engine);
  int delivered = 0;
  int width = 0;
  Frame frame = lit();
  frame.runtime(on.runtime)
      .readback(readback("colour").then([&](const Readback::Result& result) {
        ++delivered;
        if (result.image) width = result.image.size().x;
      }));

  scene.render(frame);
  // Reading a device's memory costs a wait, so the callback is not run
  // inside the frame that produced the content.
  EXPECT_EQ(delivered, 0);
  scene.render(frame);
  EXPECT_EQ(delivered, 1);
  EXPECT_EQ(width, kExtent.x);
}

TEST(GpuRuntime, AMaskedPassReachesOnlyTheSelection) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  Frame frame(set());
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .runtime(on.runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack))
      .pass(postPass("hot").reads("colour").writes("hot").only("glow").levels(
          {.gain = 4.0f}));
  const SkBitmap masked = photograph(frame, on.runtime);

  Frame plain(set());
  plain.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  const SkBitmap unmasked = photograph(plain, on.runtime);

  // The lift reaches the tagged body and nothing else: the two plates
  // differ somewhere, and the ground — which nothing selected — stands
  // where it stood.
  EXPECT_GT(diligent::worstChannel(masked, unmasked), 16)
      << "the mask lifted nothing at all";
  const int y = kExtent.y - 4;
  const SkColor4f groundMasked = masked.getColor4f(6, y);
  const SkColor4f groundPlain = unmasked.getColor4f(6, y);
  EXPECT_NEAR(groundMasked.fR, groundPlain.fR, 2.0f / 255.0f);
  EXPECT_NEAR(groundMasked.fG, groundPlain.fG, 2.0f / 255.0f);
  EXPECT_NEAR(groundMasked.fB, groundPlain.fB, 2.0f / 255.0f);
}

// ---- the map a body is dressed with ----------------------------------------

namespace {

/** A flat image, as a texture a surface carries in its base-colour
 *  slot. */
material::Texture flatMap(SkColor4f colour, int side = 8) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  bitmap.eraseColor(colour.toSkColor());
  bitmap.setImmutable();
  return material::Texture(bitmap.asImage());
}

/** A white lit surface wearing @p map, so what reaches the pixels is
 *  the map and the shading and nothing else. */
material::Material dressed(material::Texture map) {
  material::Material surface =
      material::surface::program({.baseColor = {1, 1, 1, 1}});
  surface.slot(material::surface::kBaseColorSlot, std::move(map));
  return surface;
}

/** One quad facing the camera, filling most of the frame. */
Frame dressedQuad(material::Material surface) {
  namespace gm = ::sigil::geometry::mesh;
  Frame frame(
      group({directionalLight(
                 {0.0f, 0.0f, -1.0f},
                 {.color = material::Color{1, 1, 1, 1}, .intensity = 1.0f})
                 .key("sun"),
             mesh(gm::quad(200, 150)).key("card").fill(std::move(surface))})
          .key("set"));
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** A source whose pixels stand on a device and NOWHERE ELSE: its frame
 *  carries no image and no binding, so a renderer that cannot bind what
 *  the device holds has nothing at all to draw with. */
struct StandingSource {
  material::DeviceImage where;
  media::Frame frameAt(std::chrono::duration<double>) const {
    media::Frame frame;
    frame.device.kind = media::DeviceFrame::Kind::Texture;
    frame.device.device = where.device;
    frame.device.pointer = where.pointer;
    frame.device.handle = where.handle;
    frame.device.format = where.format;
    frame.device.layout = where.layout;
    frame.device.width = where.width;
    frame.device.height = where.height;
    return frame;
  }
  bool isRunning() const { return false; }
  bool operator==(const StandingSource& other) const {
    return where == other.where;
  }
};

}  // namespace

TEST_F(DeviceFrame, TheMapABodyIsDressedWithReachesThePixels) {
  const SkBitmap plate = photograph(
      dressedQuad(dressed(flatMap({0.2f, 0.8f, 0.35f, 1.0f}))), runtime);
  // The map, not the surface: a white surface under a green map is
  // green wherever the card stands.
  const SkColor4f centre = plate.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(centre.fG, centre.fR + 0.15f);
  EXPECT_GT(centre.fG, centre.fB + 0.15f);
}

namespace {

/** AN ANIMATED IMAGE OF TWO FRAMES: the first for the first half second,
 *  the second after it. */
struct TwoFrames {
  sk_sp<SkImage> first;
  sk_sp<SkImage> second;
  media::Frame frameAt(std::chrono::duration<double> time) const {
    media::Frame frame;
    frame.image = time.count() < 0.5 ? first : second;
    return frame;
  }
  bool isRunning() const { return true; }
  bool operator==(const TwoFrames& other) const {
    return first == other.first && second == other.second;
  }
};

/** The middle of what @p scene last drew. */
SkColor4f centreOf(Scene& scene) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  draw(scene, canvas);
  return bitmap.getColor4f(kExtent.x / 2, kExtent.y / 2);
}

}  // namespace

TEST_F(DeviceFrame, AnAnimatedMapIsReadAtTheFramesTime) {
  const auto image = [](SkColor4f colour) {
    return flatMap(colour).frameAt().image;
  };
  const material::Texture animated{media::PixelSource(
      TwoFrames{image({0.9f, 0.1f, 0.1f, 1}), image({0.1f, 0.9f, 0.1f, 1})})};
  Frame frame = dressedQuad(dressed(animated));
  frame.runtime(runtime);
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(motion::Duration(0.25));
  scene.render(frame);
  const SkColor4f early = centreOf(scene);
  EXPECT_GT(early.fR, early.fG + 0.15f) << "the first frame, at its time";
  engine.advance(motion::Duration(0.75));
  scene.render(frame);
  const SkColor4f late = centreOf(scene);
  EXPECT_GT(late.fG, late.fR + 0.15f) << "the second frame, at its time";
}

TEST_F(DeviceFrame, EachFaceWearsTheSlotItsLaneNames) {
  namespace gm = ::sigil::geometry::mesh;
  // A square lifted into a block whose caps name slot 0 and whose walls
  // name slot 1, filled with a red and a blue that light does not change.
  const gm::Mesh block = gm::extrude(::sigil::geometry::path::Outline::svg(
                                         "M-50 -50 L50 -50 L50 50 L-50 50 Z"),
                                     {.depth = 60, .faceSets = true});
  const std::vector<material::Material> slots{
      material::surface::unlit({.baseColor = {1, 0, 0, 1}}),
      material::surface::unlit({.baseColor = {0, 0, 1, 1}})};
  Frame frame(group({mesh(block).key("block").fill(slots)}).key("set"));
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  const SkBitmap plate = photograph(frame, runtime);
  const auto seen = [&](glm::vec3 point) {
    const std::optional<glm::vec2> at = diligent::raisedEye().project(
        point, glm::vec2((float)kExtent.x, (float)kExtent.y));
    if (!at) return SkColor4f{0, 0, 0, 0};
    return plate.getColor4f(std::clamp((int)at->x, 0, kExtent.x - 1),
                            std::clamp((int)at->y, 0, kExtent.y - 1));
  };
  // The front cap, square to the eye, and the top wall the raised eye
  // looks down onto.
  const SkColor4f cap = seen({0, 0, 30});
  const SkColor4f wall = seen({0, 50, 0});
  EXPECT_GT(cap.fR, 0.8f);
  EXPECT_LT(cap.fB, 0.2f);
  EXPECT_GT(wall.fB, 0.8f);
  EXPECT_LT(wall.fR, 0.2f);
}

// ---- what a texture and a surface each say about themselves ----------------

namespace {

/** THE GROUND EVERY CARD BELOW STANDS AGAINST: a colour no card wears,
 *  so a card's own pixels are the ones that are not it. */
constexpr SkColor4f kGround{1.0f, 0.0f, 1.0f, 1.0f};

/** A card facing the camera squarely, and a camera square onto it, so
 *  what varies across the card is the surface and nothing else. */
geometry::mesh::camera::Camera squareOn() {
  geometry::mesh::camera::Camera camera;
  camera.eye = {0, 0, 240};
  camera.target = {0, 0, 0};
  return camera;
}

/** One frame holding @p element, cleared to the ground colour. */
Frame squareFrame(Node element) {
  Frame frame(std::move(element));
  frame.extent(kExtent)
      .defaultCamera(squareOn())
      .pass(geometryPass("colour").writes("colour").clear(kGround));
  return frame;
}

/** One frame, rendered on @p runtime and photographed square on. */
SkBitmap photographSquare(const Frame& frame, const Runtime& runtime) {
  return diligent::photograph(frame, runtime, kExtent);
}

/** Is this pixel a card's rather than the ground's? Every card below is
 *  grey or warm and the ground is magenta, so the green channel standing
 *  where the red does is the whole of the question. */
bool onACard(const SkColor4f& pixel) {
  return std::abs(pixel.fR - pixel.fG) < 0.35f;
}

/** THE RUNS OF CARD along the middle row, as pairs of first and last
 *  column — so a test finds its subjects rather than being told where a
 *  projection put them. */
std::vector<std::pair<int, int>> cardsAcrossTheMiddle(const SkBitmap& plate) {
  std::vector<std::pair<int, int>> runs;
  const int y = plate.height() / 2;
  int start = -1;
  for (int x = 0; x < plate.width(); ++x) {
    const bool card = onACard(plate.getColor4f(x, y));
    if (card && start < 0) start = x;
    if (!card && start >= 0) {
      // Two columns in from either edge, so no run keeps the pixels the
      // card's edge antialiases.
      if (x - 1 - start > 4) runs.emplace_back(start + 2, x - 3);
      start = -1;
    }
  }
  if (start >= 0 && plate.width() - 1 - start > 4)
    runs.emplace_back(start + 2, plate.width() - 3);
  return runs;
}

/** A two-texel map: black beside white, so what a filter does between
 *  them is the whole of what the picture shows. */
material::Texture twoTexelMap() {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(2, 1));
  bitmap.eraseColor(SK_ColorBLACK);
  // Black and white are the same bytes whichever way round the channels
  // of this format stand.
  *bitmap.getAddr32(1, 0) = 0xFFFFFFFFu;
  bitmap.setImmutable();
  return material::Texture(bitmap.asImage());
}

/** A card wearing @p map, its own light, so the map alone decides every
 *  pixel of it. */
Frame mappedCard(material::Texture map) {
  namespace gm = ::sigil::geometry::mesh;
  material::Material surface =
      material::surface::unlit({.baseColor = {1, 1, 1, 1}});
  surface.slot(material::surface::kBaseColorSlot, std::move(map));
  return squareFrame(
      group({mesh(gm::quad(200, 140)).key("card").fill(std::move(surface))})
          .key("set"));
}

/** The red channel across the card, at the fractions of its width
 *  @p at names. Empty when the card was not found at all. */
std::vector<float> acrossTheCard(const SkBitmap& plate,
                                 const std::vector<float>& at) {
  const std::vector<std::pair<int, int>> runs = cardsAcrossTheMiddle(plate);
  std::vector<float> out;
  if (runs.size() != 1) return out;
  const int y = plate.height() / 2;
  const float lo = (float)runs.front().first;
  const float span = (float)(runs.front().second - runs.front().first);
  for (float fraction : at)
    out.push_back(
        plate.getColor4f((int)std::lround(lo + span * fraction), y).fR);
  return out;
}

/** The four samples every filter case below reads a card at. */
const std::vector<float>& quarters() {
  static const std::vector<float> at = {0.2f, 0.4f, 0.6f, 0.8f};
  return at;
}

}  // namespace

TEST_F(DeviceFrame, ANearestFilteredMapIsTwoColoursAndOneEdge) {
  const std::vector<float> across = acrossTheCard(
      photographSquare(
          mappedCard(twoTexelMap().sampling(material::Sampling::Nearest)),
          runtime),
      quarters());
  ASSERT_EQ(across.size(), quarters().size());
  // The samples either side of the middle agree exactly, and the pair
  // across it does not: nothing is read BETWEEN two texels.
  EXPECT_FLOAT_EQ(across[0], across[1]);
  EXPECT_FLOAT_EQ(across[2], across[3]);
  EXPECT_GT(std::abs(across[1] - across[2]), 0.5f);
}

TEST_F(DeviceFrame, ALinearFilteredMapIsAGradientBetweenTwoTexels) {
  const std::vector<float> across = acrossTheCard(
      photographSquare(
          mappedCard(twoTexelMap().sampling(material::Sampling::Linear)),
          runtime),
      quarters());
  ASSERT_EQ(across.size(), quarters().size());
  // Every step along the card moves, because every sample is read
  // between the two texels rather than at one of them.
  EXPECT_LT(across[0], across[1]);
  EXPECT_LT(across[1], across[2]);
  EXPECT_LT(across[2], across[3]);
}

TEST_F(DeviceFrame, AMapAskedToRepeatIsAsManyOfItselfAsItWasAsked) {
  // A map asked to repeat, over a card four of itself wide, is four of
  // itself. Clamping instead is not a near miss: past the image's own
  // edge one row of texels is dragged across the whole rest of the face,
  // so a floor whose repeat is what says how large a room is says
  // nothing at all — and every pixel of it still looks like a surface.
  constexpr float kTimes = 4.0f;
  const Frame frame = mappedCard(
      twoTexelMap()
          .sampling(material::Sampling::Nearest)
          .tile(material::Repeat::Repeat)
          .uv(glm::mat3(glm::vec3(1.0f / kTimes, 0, 0),
                        glm::vec3(0, 1.0f / kTimes, 0), glm::vec3(0, 0, 1))));
  // The middle of each half of each repeat, so no sample lands on a
  // seam: eight of them, alternating dark and light all the way across.
  std::vector<float> at(8);
  for (size_t i = 0; i < at.size(); ++i) at[i] = ((float)i + 0.5f) / 8.0f;

  const std::vector<float> across =
      acrossTheCard(photographSquare(frame, runtime), at);
  ASSERT_EQ(across.size(), at.size());
  for (size_t i = 0; i + 1 < across.size(); ++i)
    EXPECT_GT(std::abs(across[i] - across[i + 1]), 0.5f) << "step " << i;
}

namespace {

/** The one colour both cards below wear. */
constexpr glm::vec4 kBodyColour{0.85f, 0.55f, 0.25f, 1.0f};

/** Two cards of that one colour — one that takes light, one that is its
 *  own — under a sun aimed away from both, so what separates them is
 *  only whether the emitter reached them. */
Frame litAndUnlitCards() {
  namespace gm = ::sigil::geometry::mesh;
  const material::surface::SurfaceParameters parameters{
      .baseColor = {kBodyColour.r, kBodyColour.g, kBodyColour.b, 1.0f}};
  return squareFrame(
      group({directionalLight(
                 {0.0f, 0.0f, 1.0f},
                 {.color = material::Color{1, 1, 1, 1}, .intensity = 1.0f})
                 .key("sun"),
             mesh(gm::quad(110, 110))
                 .key("lit")
                 .at({-70, 0, 0})
                 .fill(material::surface::program(parameters)),
             mesh(gm::quad(110, 110))
                 .key("unlit")
                 .at({70, 0, 0})
                 .fill(material::surface::unlit(parameters))})
          .key("set"));
}

/** The two cards' middles: the left run is the lit one and the right is
 *  its own light. */
bool cardCentres(const SkBitmap& plate, int* lit, int* unlit) {
  const std::vector<std::pair<int, int>> runs = cardsAcrossTheMiddle(plate);
  if (runs.size() != 2) return false;
  *lit = (runs[0].first + runs[0].second) / 2;
  *unlit = (runs[1].first + runs[1].second) / 2;
  return true;
}

}  // namespace

TEST_F(DeviceFrame, AnUnlitSurfaceIsItsOwnLight) {
  const SkBitmap plate = photographSquare(litAndUnlitCards(), runtime);
  int litAt = 0, unlitAt = 0;
  ASSERT_TRUE(cardCentres(plate, &litAt, &unlitAt));
  const int y = kExtent.y / 2;
  const SkColor4f unlit = plate.getColor4f(unlitAt, y);
  const SkColor4f lit = plate.getColor4f(litAt, y);
  // The unlit card IS its base colour, and the lit one — with the sun on
  // its far side — is only what the ambient leaves of it.
  EXPECT_NEAR(unlit.fR, kBodyColour.r, 3.0f / 255.0f);
  EXPECT_NEAR(unlit.fG, kBodyColour.g, 3.0f / 255.0f);
  EXPECT_NEAR(unlit.fB, kBodyColour.b, 3.0f / 255.0f);
  EXPECT_LT(lit.fR, unlit.fR * 0.5f);
}

TEST(GpuRuntime, AMapAlreadyOnThisDeviceIsBoundWhereItStands) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;

  // 2D paints into a texture on the one shared device…
  const diligent::PaintedTexture painted =
      diligent::paintOnDevice(*on.device, SkColor4f{0.15f, 0.35f, 0.95f, 1.0f});
  if (!painted.native) GTEST_SKIP() << "the device was not adopted";

  material::DeviceImage where;
  where.device = on.device->gpu();
  where.pointer = painted.native.mtlTexture;
  where.handle = painted.native.vkImage;
  where.format = painted.native.vkFormat;
  where.layout = painted.native.vkLayout;
  where.width = painted.native.width;
  where.height = painted.native.height;

  // …and 3D binds those very pixels. The source yields no host image at
  // all, so a picture carrying the colour proves the binding rather than
  // a copy.
  const SkBitmap graphics =
      photograph(dressedQuad(dressed(material::Texture(StandingSource{where}))),
                 on.runtime);
  const SkColor4f centre = graphics.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(centre.fB, centre.fR + 0.15f);
  EXPECT_GT(centre.fB, centre.fG + 0.15f);
  on.device->gpu()->destroy(painted.handle);
}

TEST_F(DeviceFrame, AVariantReDrawStandsUnderThePassesLights) {
  // A variant surface is a colour laid over the bodies a selector names,
  // and it is shaded by the pass's own lights the way any body is: the
  // side of the sphere facing away from the sun is darker in the overlay
  // too. An overlay drawn flat would be one white from edge to edge.
  Frame frame(set());
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .pass(geometryPass("colour")
                .writes("colour")
                .clear(SkColors::kBlack)
                .only("glow")
                .variant(
                    material::surface::program({.baseColor = {1, 1, 1, 1}})));
  const SkBitmap plate = photograph(frame, runtime);
  // The sphere stands at the centre of the frame; the sun travels toward
  // -x, so its +x side is the lit one.
  const int y = kExtent.y / 2;
  const SkColor4f shaded = plate.getColor4f(kExtent.x / 2 - 14, y);
  const SkColor4f sunlit = plate.getColor4f(kExtent.x / 2 + 14, y);
  // The overlay is the variant's white and not the body's own amber…
  EXPECT_NEAR(sunlit.fG, sunlit.fR, 0.05f);
  EXPECT_NEAR(sunlit.fB, sunlit.fR, 0.05f);
  EXPECT_NEAR(shaded.fG, shaded.fR, 0.05f);
  EXPECT_NEAR(shaded.fB, shaded.fR, 0.05f);
  // …and it is lit: darker where the sun does not reach.
  const auto luma = [](SkColor4f c) {
    return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f;
  };
  EXPECT_LT(luma(shaded), luma(sunlit) - 0.1f);
}

TEST_F(DeviceFrame, APartOfThePictureIsThatPartOfTheWhole) {
  // A frame whose targets are a part of a larger picture is formed
  // through the whole picture's projection carried off-centre, so each
  // pixel of the part is the pixel the whole picture holds there — and
  // the draw puts the part where it stands in the whole.
  constexpr glm::ivec2 kWhole{kExtent.x * 4, kExtent.y * 4};
  constexpr glm::ivec2 kCorner{280, 150};
  Frame whole = lit();
  whole.extent(kWhole);
  Frame part = lit();
  part.viewOffset({kWhole, kCorner});
  const SkBitmap all = diligent::photograph(whole, runtime, kWhole);
  const SkBitmap piece = diligent::photograph(part, runtime, kWhole);

  const SkIRect region =
      SkIRect::MakeXYWH(kCorner.x, kCorner.y, kExtent.x, kExtent.y);
  // The part looks at the body, so a part that drew nothing would not
  // pass for the wrong reason.
  EXPECT_NE(piece.getColor(kWhole.x / 2, kWhole.y / 2), SK_ColorBLACK);
  EXPECT_LE(diligent::pixelsApart(all, piece, region, 2),
            kExtent.x * kExtent.y / 500);
  // Nothing outside the part was formed, so nothing outside it lands:
  // the ground there is the canvas's own, where the whole picture shows
  // the plate the body stands on.
  EXPECT_EQ(piece.getColor(kCorner.x - 4, kWhole.y / 2), SK_ColorBLACK);
}

// ---- the embedded shader table --------------------------------------------

// ---- what a pass draws -----------------------------------------------------

namespace {

/** How many pixels of one half of @p plate hold anything brighter than
 *  the black every photograph stands on. */
int litIn(const SkBitmap& plate, bool leftHalf) {
  int count = 0;
  const int mid = plate.width() / 2;
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      if (leftHalf != (x < mid)) continue;
      const SkColor4f c = plate.getColor4f(x, y);
      if (std::max({c.fR, c.fG, c.fB}) > 4.0f / 255.0f) ++count;
    }
  return count;
}

/** One plain card on the left and one tagged "glow" on the right, square
 *  on to the camera under a sun behind it. */
Group twoCards() {
  namespace gm = ::sigil::geometry::mesh;
  return group({directionalLight({0.0f, 0.0f, -1.0f}).key("sun"),
                mesh(gm::quad(80, 80))
                    .key("left")
                    .at({-60, 0, 0})
                    .fill(slangPaint({0.8f, 0.6f, 0.3f, 1.0f}))
                    .collection("plain"),
                mesh(gm::quad(80, 80))
                    .key("right")
                    .at({60, 0, 0})
                    .fill(slangPaint({0.3f, 0.6f, 0.8f, 1.0f}))
                    .collection("glow")})
      .key("set");
}

}  // namespace

TEST_F(DeviceFrame, ALightLinkLightsOnlyTheBodiesItNames) {
  // Two cards square on to the camera under a sun that reaches them both;
  // linked to the "plain" collection, it reaches the left card alone and
  // the right one is left with the ambient a body with no light keeps.
  const auto photographWith = [&](std::optional<Collection> link) {
    Light sun = directionalLight({0.0f, 0.0f, -1.0f}).key("sun");
    if (link) sun.lightLink(*link);
    const Group set = group({sun,
                             mesh(::sigil::geometry::mesh::quad(80, 80))
                                 .key("left")
                                 .at({-60, 0, 0})
                                 .fill(slangPaint({0.8f, 0.8f, 0.8f, 1.0f}))
                                 .collection("plain"),
                             mesh(::sigil::geometry::mesh::quad(80, 80))
                                 .key("right")
                                 .at({60, 0, 0})
                                 .fill(slangPaint({0.8f, 0.8f, 0.8f, 1.0f}))
                                 .collection("glow")})
                          .key("set");
    return photograph(squareFrame(set), runtime);
  };
  const auto middleOf = [](const SkBitmap& plate, size_t card) {
    const std::vector<std::pair<int, int>> runs = cardsAcrossTheMiddle(plate);
    if (runs.size() != 2) return SkColor4f{-1, -1, -1, -1};
    return plate.getColor4f((runs[card].first + runs[card].second) / 2,
                            plate.height() / 2);
  };
  const SkBitmap everything = photographWith(std::nullopt);
  const SkBitmap linked = photographWith(Collection{.include = {"plain"}});
  ASSERT_GE(middleOf(everything, 0).fR, 0.0f);
  ASSERT_GE(middleOf(linked, 1).fR, 0.0f);
  // The card the link names is lit as it was…
  EXPECT_NEAR(middleOf(linked, 0).fR, middleOf(everything, 0).fR, 1.0f / 255);
  // …and the one it leaves out is not.
  EXPECT_LT(middleOf(linked, 1).fR, middleOf(everything, 1).fR - 0.1f);
}

TEST_F(DeviceFrame, AVisibleBackfaceKeepsAPlaneUnderAnOrbit) {
  namespace gm = ::sigil::geometry::mesh;
  geometry::mesh::camera::Camera behind = squareOn();
  behind.eye.z = -240;
  const auto fromBehind = [&](material::Backface backface) {
    Frame frame(group({directionalLight({0.0f, 0.0f, 1.0f}).key("sun"),
                       mesh(gm::quad(120, 120))
                           .key("card")
                           .fill(slangPaint({0.9f, 0.6f, 0.2f, 1.0f}))
                           .backface(backface)})
                    .key("set"));
    frame.extent(kExtent).defaultCamera(behind).pass(
        geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    const SkBitmap plate = photograph(frame, runtime);
    return litIn(plate, true) + litIn(plate, false);
  };
  EXPECT_EQ(fromBehind(material::Backface::Hidden), 0);
  EXPECT_GT(fromBehind(material::Backface::Visible), 0);
}

TEST_F(DeviceFrame, ACulledGeometryPassDrawsOnlyItsSelection) {
  Frame frame(twoCards());
  frame.extent(kExtent)
      .defaultCamera(squareOn())
      .pass(geometryPass("glow").only("glow").writes("colour").clear(
          SkColors::kBlack));
  const SkBitmap plate = photograph(frame, runtime);
  EXPECT_EQ(litIn(plate, /*leftHalf=*/true), 0);
  EXPECT_GT(litIn(plate, /*leftHalf=*/false), 0);
}

TEST_F(DeviceFrame, APreviousReadIsWhatStoodAtTheEndOfTheFrameBefore) {
  Frame frame(twoCards());
  frame.extent(kExtent)
      .defaultCamera(squareOn())
      .runtime(runtime)
      .pass(geometryPass("main").writes("colour").clear(SkColors::kBlack))
      .pass(postPass("trail")
                .reads("colour")
                .previous("trail")
                .writes("trail")
                .composite({.mode = material::BlendMode::PlusLighter,
                            .opacity = 1.0f}))
      .present("trail");
  motion::Engine engine;
  Scene scene(engine);
  const auto picture = [&] {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorBLACK);
    draw(scene, canvas);
    return bitmap;
  };

  // The first frame has no previous, so the trail is only the picture;
  // the second adds last frame's trail to it, and a card only brightens.
  scene.render(frame);
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  const SkBitmap once = picture();
  scene.render(frame);
  const SkBitmap twice = picture();
  const SkColor4f first = once.getColor4f(kExtent.x / 4, kExtent.y / 2);
  const SkColor4f second = twice.getColor4f(kExtent.x / 4, kExtent.y / 2);
  EXPECT_GT(first.fR, 0.05f) << "the card is not where the case looks";
  EXPECT_GT(second.fR, first.fR + 0.05f);
}

TEST_F(DeviceFrame, ADeclaredBodyRunsInPlaceOfItsStage) {
  int ran = 0;
  size_t seen = 0;
  Frame frame(twoCards());
  frame.extent(kExtent)
      .defaultCamera(squareOn())
      .runtime(runtime)
      .pass(geometryPass("hand").writes("colour").body([&](const View& view) {
        ++ran;
        seen = view.draws.size();
      }));
  motion::Engine engine;
  Scene scene(engine);
  scene.render(frame);
  ASSERT_TRUE(scene.error().empty()) << scene.error();
  EXPECT_EQ(ran, 1);
  EXPECT_EQ(seen, 2u);
}

TEST(Programs, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(diligent::shaderSources(),
                                                 SIGIL_WORLD_SHADER_DIR);
}

TEST(GpuRuntime, ItsFixedProgramsAreMadeWhenTheDeviceComesUp) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The runtime is made; nothing is drawn. The programs every frame uses
  // are already there.
  EXPECT_TRUE(world::diligent::fixedProgramsReady());
}
