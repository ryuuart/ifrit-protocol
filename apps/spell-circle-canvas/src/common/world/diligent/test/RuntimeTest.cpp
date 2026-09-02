/** @file
 * What the device executor answers.
 *
 * WHAT EACH CASE HOLDS IT TO is a claim about the picture that only
 * this code can falsify: a chain cooked on the device is the host's cook
 * to the bit, a readback arrives the frame after, a mask reaches the
 * selection and leaves the ground where it stood, a filter reads between
 * texels or does not, a surface that is its own light stands at its base
 * colour. A claim a picture makes whichever rasteriser drew it is
 * written once and answered on both tiers, with the tier as the
 * parameter.
 *
 * HOW FAR THE TWO TIERS STAND APART IS NOT ASKED HERE. Two rasterisers
 * are not the same picture bit for bit, the distance between them is a
 * different number per subject, and it moves with the scene rather than
 * with this code — so it is judged over the whole registry, against a
 * committed baseline, by the plate ledger's device tier.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Distance.h"
#include "Gpu.h"
#include "OnDevice.h"
#include "Programs.h"
#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

constexpr SkISize kExtent{160, 120};

/** A cube, a plate under it and a sun: enough for a depth test and a
 *  lit surface to have something to say. */
Element set() {
  namespace gm = ::sigil::geometry::mesh;
  return Element()
      .key("set")
      .child(Element().key("sun").light(light::sun({-0.4f, -0.8f, -0.4f})))
      .child(Element()
                 .key("plate")
                 .at({0, -40, 0})
                 .rotateX(-90.0f)
                 .mesh(gm::quad(300, 300))
                 .fill(slangPaint({0.15f, 0.16f, 0.2f, 1.0f}))
                 .tag("ground"))
      .child(Element()
                 .key("body")
                 .mesh(gm::superellipsoid({40, 40, 40}, 2.0f, 24, 16))
                 .fill(slangPaint({0.8f, 0.6f, 0.3f, 1.0f}))
                 .tag("glow"));
}

/** One frame, rendered on @p runtime and photographed from above and in
 *  front. */
SkBitmap photograph(const Frame& frame, const Runtime& runtime) {
  return diligent::photograph(frame, runtime, kExtent, diligent::raisedEye());
}

Frame lit() {
  Frame frame(set());
  frame.extent(kExtent)
      .camera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** THE TWO TIERS, AS A PARAMETER. A claim a picture has to make
 *  whichever rasteriser drew it is written once and answered twice: on
 *  the host, which needs nothing, and on the device, which SKIPS where
 *  there is no Vulkan runtime to ask. */
enum class Tier { Host, Device };

class EitherTier : public testing::TestWithParam<Tier> {
 protected:
  void SetUp() override {
    if (GetParam() == Tier::Host) return;
    on = diligent::onDevice();
    if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
    runtime = on.runtime;
  }

  diligent::OnDevice<Runtime> on;
  Runtime runtime = Runtime::cpu();
};

std::string tierName(const testing::TestParamInfo<Tier>& info) {
  return info.param == Tier::Host ? "Host" : "Device";
}

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
  EXPECT_NE(slang->compiled().uniform("uShading"), nullptr);
  const std::shared_ptr<material::Program> unlit =
      material::program(slangPaintRecipe(), material::Target::Slang);
  ASSERT_NE(unlit, nullptr);
  EXPECT_EQ(unlit->as<material::slang::SlangProgram>()->compiled().uniform(
                "uShading"),
            nullptr);
}

TEST(GpuRuntime, TheScaffoldAndThePostStagesCompile) {
  EXPECT_FALSE(world::diligent::scaffold(/*lit=*/true).empty());
  EXPECT_FALSE(world::diligent::scaffold(/*lit=*/false).empty());
  EXPECT_FALSE(world::diligent::postPrograms().copy.empty());
  EXPECT_FALSE(world::diligent::postPrograms().blur.empty());
  EXPECT_FALSE(world::diligent::postPrograms().levels.empty());
  EXPECT_FALSE(world::diligent::postPrograms().masked.empty());
}

TEST(GpuRuntime, ACookedChainMatchesTheHostCook) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  namespace gm = ::sigil::geometry::mesh;
  const std::vector<glm::vec3> path = {
      {-100, 0, 0}, {0, 60, 0}, {100, 0, 0}, {0, -60, 0}};
  const Chain chain = gm::pop::on(path).count(200).spread(6.0f);

  const auto cooked = [&](const Runtime& runtime) {
    motion::Ticker ticker;
    Scene scene(ticker);
    Frame frame(set());
    frame.extent(kExtent)
        .camera(diligent::raisedEye())
        .runtime(runtime)
        .pass(computePass("cook").chain(chain).writes("points"))
        .pass(geometryPass("colour").reads("points").writes("colour").stamp(
            gm::superellipsoid({4, 4, 4}, 2.0f, 6, 4)));
    scene.render(frame);
    const Cloud* points = scene.targets().points("points");
    return points ? points->positions : std::vector<glm::vec3>{};
  };

  const std::vector<glm::vec3> host = cooked(Runtime::cpu());
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
  motion::Ticker ticker;
  Scene scene(ticker);
  int delivered = 0;
  int width = 0;
  Frame frame = lit();
  frame.runtime(on.runtime)
      .readback(readback("colour").then([&](const Readback::Result& result) {
        ++delivered;
        if (result.image) width = result.image->width();
      }));

  scene.render(frame);
  // Reading a device's memory costs a wait, so the callback is not run
  // inside the frame that produced the content.
  EXPECT_EQ(delivered, 0);
  scene.render(frame);
  EXPECT_EQ(delivered, 1);
  EXPECT_EQ(width, kExtent.width());
}

TEST(GpuRuntime, AMaskedPassReachesOnlyTheSelection) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  Frame frame(set());
  frame.extent(kExtent)
      .camera(diligent::raisedEye())
      .runtime(on.runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack))
      .pass(postPass("hot")
                .reads("colour")
                .writes("hot")
                .only(sel::tag("glow"))
                .levels(4.0f, 0.0f, {1, 1, 1, 1}));
  const SkBitmap masked = photograph(frame, on.runtime);

  Frame plain(set());
  plain.extent(kExtent)
      .camera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  const SkBitmap unmasked = photograph(plain, on.runtime);

  // The lift reaches the tagged body and nothing else: the two plates
  // differ somewhere, and the ground — which nothing selected — stands
  // where it stood.
  EXPECT_GT(diligent::worstChannel(masked, unmasked), 16)
      << "the mask lifted nothing at all";
  const int y = kExtent.height() - 4;
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
  return material::Texture::of(bitmap.asImage());
}

/** A white lit surface wearing @p map, so what reaches the pixels is
 *  the map and the shading and nothing else. */
material::Material dressed(material::Texture map) {
  material::Material surface =
      material::kit::surface({.baseColor = {1, 1, 1, 1}});
  surface.child(material::kit::kBaseColorSlot, std::move(map));
  return surface;
}

/** One quad facing the camera, filling most of the frame. */
Frame dressedQuad(material::Material surface) {
  namespace gm = ::sigil::geometry::mesh;
  Frame frame(Element()
                  .key("set")
                  .child(Element().key("sun").light(
                      light::sun({0.0f, 0.0f, -1.0f}, {1, 1, 1, 1}, 1.0f)))
                  .child(Element()
                             .key("card")
                             .mesh(gm::quad(200, 150))
                             .fill(std::move(surface))));
  frame.extent(kExtent)
      .camera(diligent::raisedEye())
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** A source whose pixels stand on a device and NOWHERE ELSE: `image()`
 *  answers null, so a renderer that cannot bind what the device holds
 *  has nothing at all to draw with. */
struct StandingSource {
  material::DeviceImage where;
  sk_sp<SkImage> image() const { return nullptr; }
  bool animated() const { return false; }
  material::DeviceImage deviceImage() const { return where; }
  bool operator==(const StandingSource& other) const {
    return where == other.where;
  }
};

}  // namespace

TEST_P(EitherTier, TheMapABodyIsDressedWithReachesThePixels) {
  const SkBitmap plate = photograph(
      dressedQuad(dressed(flatMap({0.2f, 0.8f, 0.35f, 1.0f}))), runtime);
  // The map, not the surface: a white surface under a green map is
  // green wherever the card stands.
  const SkColor4f centre =
      plate.getColor4f(kExtent.width() / 2, kExtent.height() / 2);
  EXPECT_GT(centre.fG, centre.fR + 0.15f);
  EXPECT_GT(centre.fG, centre.fB + 0.15f);
}

TEST(GpuRuntime, AMapTOOSMALLForAChainAsksForNoneAtAll) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const std::shared_ptr<diligent::Gpu> gpu = diligent::makeGpu(*on.device);

  // A map wider than one texel wears the whole chain and is described
  // as generating it, because a surface smaller on screen than its map
  // is in texels aliases without one…
  Diligent::ITexture* many = gpu->sample(flatMap({0.2f, 0.8f, 0.35f, 1.0f}, 8));
  ASSERT_NE(many, nullptr);
  EXPECT_EQ(many->GetDesc().MipLevels, 4u);
  EXPECT_TRUE(many->GetDesc().MiscFlags &
              Diligent::MISC_TEXTURE_FLAG_GENERATE_MIPS);

  // …and a ONE-TEXEL map — how a constant slot such as an emissive tint
  // is spelled — has one level, since halving a single texel arrives
  // nowhere, and must therefore not be described as generating a chain:
  // a device handed a view with one level in it and told to fill the
  // levels below has nowhere to put them, and refuses.
  Diligent::ITexture* one = gpu->sample(flatMap({0.9f, 0.2f, 0.2f, 1.0f}, 1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->GetDesc().MipLevels, 1u);
  EXPECT_FALSE(one->GetDesc().MiscFlags &
               Diligent::MISC_TEXTURE_FLAG_GENERATE_MIPS);
}

// ---- what a texture and a surface each say about themselves ----------------

namespace {

/** THE GROUND EVERY CARD BELOW STANDS AGAINST: a colour no card wears,
 *  so a card's own pixels are the ones that are not it. */
constexpr SkColor4f kGround{1.0f, 0.0f, 1.0f, 1.0f};

/** A card facing the camera squarely, and a camera square onto it, so
 *  what varies across the card is the surface and nothing else. */
Camera squareOn() {
  Camera camera;
  camera.eye = {0, 0, 240};
  camera.target = {0, 0, 0};
  return camera;
}

/** One frame holding @p element, cleared to the ground colour. */
Frame squareFrame(Element element) {
  Frame frame(std::move(element));
  frame.extent(kExtent)
      .camera(squareOn())
      .pass(geometryPass("colour").writes("colour").clear(kGround));
  return frame;
}

/** One frame, rendered on @p runtime and photographed square on. */
SkBitmap photographSquare(const Frame& frame, const Runtime& runtime) {
  return diligent::photograph(frame, runtime, kExtent, squareOn());
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
      // Two columns in from either edge, so no run keeps the pixels two
      // rasterisers antialias differently.
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
  return material::Texture::of(bitmap.asImage());
}

/** A card wearing @p map, its own light, so the map alone decides every
 *  pixel of it. */
Frame mappedCard(material::Texture map) {
  namespace gm = ::sigil::geometry::mesh;
  material::Material surface =
      material::kit::unlit({.baseColor = {1, 1, 1, 1}});
  surface.child(material::kit::kBaseColorSlot, std::move(map));
  return squareFrame(Element().key("set").child(
      Element().key("card").mesh(gm::quad(200, 140)).fill(std::move(surface))));
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

TEST_P(EitherTier, ANearestFilteredMapIsTwoColoursAndOneEdge) {
  const std::vector<float> across = acrossTheCard(
      photographSquare(mappedCard(twoTexelMap().filter(SkFilterMode::kNearest)),
                       runtime),
      quarters());
  ASSERT_EQ(across.size(), quarters().size());
  // The samples either side of the middle agree exactly, and the pair
  // across it does not: nothing is read BETWEEN two texels.
  EXPECT_FLOAT_EQ(across[0], across[1]);
  EXPECT_FLOAT_EQ(across[2], across[3]);
  EXPECT_GT(std::abs(across[1] - across[2]), 0.5f);
}

TEST_P(EitherTier, ALinearFilteredMapIsAGradientBetweenTwoTexels) {
  const std::vector<float> across = acrossTheCard(
      photographSquare(mappedCard(twoTexelMap().filter(SkFilterMode::kLinear)),
                       runtime),
      quarters());
  ASSERT_EQ(across.size(), quarters().size());
  // Every step along the card moves, because every sample is read
  // between the two texels rather than at one of them.
  EXPECT_LT(across[0], across[1]);
  EXPECT_LT(across[1], across[2]);
  EXPECT_LT(across[2], across[3]);
}

TEST_P(EitherTier, AMapAskedToRepeatIsAsManyOfItselfAsItWasAsked) {
  // A map asked to repeat, over a card four of itself wide, is four of
  // itself. Clamping instead is not a near miss: past the image's own
  // edge one row of texels is dragged across the whole rest of the face,
  // so a floor whose repeat is what says how large a room is says
  // nothing at all — and every pixel of it still looks like a surface.
  constexpr float kTimes = 4.0f;
  const Frame frame =
      mappedCard(twoTexelMap()
                     .filter(SkFilterMode::kNearest)
                     .tile(SkTileMode::kRepeat)
                     .uv(SkMatrix::Scale(1.0f / kTimes, 1.0f / kTimes)));
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

TEST(SurfaceProgram, TheKitsOwnSurfacesCompileTheirBodies) {
  // WHAT MAKES A SURFACE A SURFACE rather than a colour is that its own
  // body is compiled and run, and every set in this repository wears one
  // of these two. A recipe whose body will not compile is reported once
  // and then painted in the colour the frame extracted — which is the
  // same reading a tier with no compiler makes, so the picture that
  // comes back looks like a picture rather than like a failure.
  world::diligent::installSlangCompiler();
  const material::kit::SurfaceParams params{.baseColor = {0.6f, 0.4f, 0.2f, 1},
                                            .roughness = 0.4f};
  for (const material::Material& surface :
       {material::kit::surface(params), material::kit::unlit(params)}) {
    const material::Material::Resolved resolved =
        surface.resolve(material::Target::Slang, material::FrameData{},
                        material::Variant{world::diligent::kVariantLit});
    ASSERT_NE(resolved.program, nullptr) << surface.recipe().name();
    const auto* slang = resolved.program->as<material::slang::SlangProgram>();
    ASSERT_NE(slang, nullptr) << surface.recipe().name();
    EXPECT_FALSE(slang->compiled().empty()) << surface.recipe().name();
  }
}

namespace {

/** The one colour both cards below wear. */
constexpr glm::vec4 kBodyColour{0.85f, 0.55f, 0.25f, 1.0f};

/** Two cards of that one colour — one that takes light, one that is its
 *  own — under a sun aimed away from both, so what separates them is
 *  only whether the emitter reached them. */
Frame litAndUnlitCards() {
  namespace gm = ::sigil::geometry::mesh;
  const material::kit::SurfaceParams params{
      .baseColor = {kBodyColour.r, kBodyColour.g, kBodyColour.b, 1.0f}};
  return squareFrame(Element()
                         .key("set")
                         // Travelling away from the camera, so it lands on the
                         // far side of both cards and neither is diffusely lit.
                         .child(Element().key("sun").light(light::sun(
                             {0.0f, 0.0f, 1.0f}, {1, 1, 1, 1}, 1.0f)))
                         .child(Element()
                                    .key("lit")
                                    .at({-70, 0, 0})
                                    .mesh(gm::quad(110, 110))
                                    .fill(material::kit::surface(params)))
                         .child(Element()
                                    .key("unlit")
                                    .at({70, 0, 0})
                                    .mesh(gm::quad(110, 110))
                                    .fill(material::kit::unlit(params))));
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

TEST_P(EitherTier, AnUnlitSurfaceIsItsOwnLight) {
  const SkBitmap plate = photographSquare(litAndUnlitCards(), runtime);
  int litAt = 0, unlitAt = 0;
  ASSERT_TRUE(cardCentres(plate, &litAt, &unlitAt));
  const int y = kExtent.height() / 2;
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
  const SkColor4f centre =
      graphics.getColor4f(kExtent.width() / 2, kExtent.height() / 2);
  EXPECT_GT(centre.fB, centre.fR + 0.15f);
  EXPECT_GT(centre.fB, centre.fG + 0.15f);
  on.device->gpu()->destroy(painted.handle);
}

INSTANTIATE_TEST_SUITE_P(Tiers, EitherTier,
                         testing::Values(Tier::Host, Tier::Device), tierName);
