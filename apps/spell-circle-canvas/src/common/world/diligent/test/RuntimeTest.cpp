/** @file
 * What the device executor answers, and how far its answer stands from
 * the host's.
 *
 * TWO RASTERISERS ARE NOT ASKED TO AGREE BIT FOR BIT. The host paints
 * shaded vertices through a triangle sort; the device rasterises the
 * same shading through a depth buffer. They agree about what the scene
 * is, and they differ along every edge — which is why what is asserted
 * here is a per-channel distance and not a hash.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Compile.h"

using namespace sigil;
using namespace sigil::world;

namespace {

/** A DEVICE AND THE RUNTIME ON IT, or the reason there is neither. Every
 *  test that needs one SKIPS rather than fails without a Vulkan runtime,
 *  so a machine with no GPU stays green. */
struct OnDevice {
  std::unique_ptr<world::diligent::Device> device;
  Runtime runtime;
  std::string error;
  explicit operator bool() const { return (bool)device; }
};

OnDevice onDevice() {
  OnDevice out;
  const world::diligent::DeviceConfig config;
  out.device = world::diligent::Device::create(config, &out.error);
  if (out.device) out.runtime = world::diligent::runtime(*out.device);
  return out;
}

struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

constexpr char kPaintSlang[] = R"(
float4 surface(float2 uv) { return baseColor; }
)";

const std::shared_ptr<const material::Recipe>& paintRecipe() {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.test.paint")
              .body(material::Target::Slang, kPaintSlang));
  return recipe;
}

material::Material paint(glm::vec4 colour) {
  return material::Material(paintRecipe(), Paint{colour});
}

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
                 .fill(paint({0.15f, 0.16f, 0.2f, 1.0f}))
                 .tag("ground"))
      .child(Element()
                 .key("body")
                 .mesh(gm::superellipsoid({40, 40, 40}, 2.0f, 24, 16))
                 .fill(paint({0.8f, 0.6f, 0.3f, 1.0f}))
                 .tag("glow"));
}

Camera eye() {
  Camera camera;
  camera.eye = {0, 90, 240};
  camera.target = {0, 0, 0};
  return camera;
}

/** HOW FAR TWO PLATES STAND APART, per colour channel in 0..255: the
 *  mean over every channel of every pixel, the value 99 in a hundred
 *  stay under, and the worst one anywhere.
 *
 *  The mean is what says the two pictures ARE the same picture; the
 *  99th is what says the disagreement is confined; the maximum is an
 *  edge, and an edge is where two rasterisers always differ. */
struct Distance {
  double mean = 0;
  int p99 = 0;
  int max = 0;
};

Distance distanceOf(const SkBitmap& a, const SkBitmap& b) {
  Distance out;
  if (a.width() != b.width() || a.height() != b.height()) {
    out.max = 255;
    return out;
  }
  std::vector<int> histogram(256, 0);
  double total = 0;
  size_t count = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      const SkColor4f left = a.getColor4f(x, y);
      const SkColor4f right = b.getColor4f(x, y);
      const float channels[4][2] = {{left.fR, right.fR},
                                    {left.fG, right.fG},
                                    {left.fB, right.fB},
                                    {left.fA, right.fA}};
      for (const auto& pair : channels) {
        const int diff = (int)std::lround(std::abs(pair[0] - pair[1]) * 255.0f);
        ++histogram[(size_t)std::clamp(diff, 0, 255)];
        total += diff;
        ++count;
        out.max = std::max(out.max, diff);
      }
    }
  }
  out.mean = count ? total / (double)count : 0.0;
  const size_t cut = (size_t)((double)count * 0.99);
  size_t seen = 0;
  for (int value = 0; value < 256; ++value) {
    seen += (size_t)histogram[(size_t)value];
    if (seen >= cut) {
      out.p99 = value;
      break;
    }
  }
  return out;
}

/** One frame, rendered on @p runtime and photographed. */
SkBitmap photograph(const Frame& frame, const Runtime& runtime) {
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame copy = frame;
  copy.runtime(runtime);
  ticker.tick(1.0 / 60.0);
  scene.render(copy);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  scene.draw(canvas, eye());
  return bitmap;
}

Frame lit() {
  Frame frame(set());
  frame.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

}  // namespace

TEST(SlangProgram, APipelineComesOffARecipeBody) {
  world::diligent::installSlangCompiler();
  const std::shared_ptr<material::Program> program =
      material::program(paintRecipe(), material::Target::Slang,
                        material::Variant{world::diligent::kVariantLit});
  ASSERT_NE(program, nullptr);
  const auto* slang = program->as<world::diligent::SlangProgram>();
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
      material::program(paintRecipe(), material::Target::Slang);
  ASSERT_NE(unlit, nullptr);
  EXPECT_EQ(unlit->as<world::diligent::SlangProgram>()->compiled().uniform(
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

TEST(GpuRuntime, OneSceneOnBothTiers) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const SkBitmap host = photograph(lit(), Runtime::cpu());
  const SkBitmap graphics = photograph(lit(), on.runtime);
  const Distance distance = distanceOf(host, graphics);
  // The scene is the same scene: most of the picture agrees closely and
  // what disagrees is confined to the edges two rasterisers antialias
  // differently.
  EXPECT_LT(distance.mean, 12.0) << "mean channel distance";
  EXPECT_LT(distance.p99, 128) << "99th percentile channel distance";
}

TEST(GpuRuntime, ACookedChainMatchesTheHostCook) {
  const OnDevice on = onDevice();
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
        .camera(eye())
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
  const OnDevice on = onDevice();
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
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  Frame frame(set());
  frame.extent(kExtent)
      .camera(eye())
      .runtime(on.runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack))
      .pass(postPass("hot")
                .reads("colour")
                .writes("hot")
                .only(sel::tag("glow"))
                .levels(4.0f, 0.0f, {1, 1, 1, 1}));
  const SkBitmap masked = photograph(frame, on.runtime);

  Frame plain(set());
  plain.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  const SkBitmap unmasked = photograph(plain, on.runtime);

  // The lift reaches the tagged body and nothing else: the two plates
  // differ somewhere, and the ground — which nothing selected — stands
  // where it stood.
  const Distance distance = distanceOf(masked, unmasked);
  EXPECT_GT(distance.max, 16) << "the mask lifted nothing at all";
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
  frame.extent(kExtent).camera(eye()).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
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

TEST(GpuRuntime, TheMapABodyIsDressedWithReachesBothTiers) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const Frame frame = dressedQuad(dressed(flatMap({0.2f, 0.8f, 0.35f, 1.0f})));

  const SkBitmap host = photograph(frame, Runtime::cpu());
  const SkBitmap graphics = photograph(frame, on.runtime);

  // The map, not the surface: a white surface under a green map is
  // green wherever the card stands, on either tier.
  const SkColor4f centre =
      host.getColor4f(kExtent.width() / 2, kExtent.height() / 2);
  EXPECT_GT(centre.fG, centre.fR + 0.15f);
  EXPECT_GT(centre.fG, centre.fB + 0.15f);
  const SkColor4f onDeviceCentre =
      graphics.getColor4f(kExtent.width() / 2, kExtent.height() / 2);
  EXPECT_GT(onDeviceCentre.fG, onDeviceCentre.fR + 0.15f);
  EXPECT_GT(onDeviceCentre.fG, onDeviceCentre.fB + 0.15f);

  // …and the two tiers are the same picture: a flat map has no edges of
  // its own, so what is left is the two rasterisers' own disagreement.
  const Distance distance = distanceOf(host, graphics);
  EXPECT_LT(distance.mean, 6.0);
  EXPECT_LT(distance.p99, 64);
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
  motion::Ticker ticker;
  Scene scene(ticker);
  Frame copy = frame;
  copy.runtime(runtime);
  ticker.tick(1.0 / 60.0);
  scene.render(copy);
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kExtent.width(), kExtent.height()));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  scene.draw(canvas, squareOn());
  return bitmap;
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

}  // namespace

TEST(GpuRuntime, ATexturesFilterIsHonouredOnBothTiers) {
  const std::vector<float> at = {0.2f, 0.4f, 0.6f, 0.8f};
  const Frame nearest =
      mappedCard(twoTexelMap().filter(SkFilterMode::kNearest));
  const Frame linear = mappedCard(twoTexelMap().filter(SkFilterMode::kLinear));

  const std::vector<float> hostNearest =
      acrossTheCard(photographSquare(nearest, Runtime::cpu()), at);
  const std::vector<float> hostLinear =
      acrossTheCard(photographSquare(linear, Runtime::cpu()), at);
  ASSERT_EQ(hostNearest.size(), at.size());
  ASSERT_EQ(hostLinear.size(), at.size());

  // NEAREST is two colours and one edge: the samples either side of the
  // middle agree exactly, and the pair across it does not.
  EXPECT_FLOAT_EQ(hostNearest[0], hostNearest[1]);
  EXPECT_FLOAT_EQ(hostNearest[2], hostNearest[3]);
  EXPECT_GT(std::abs(hostNearest[1] - hostNearest[2]), 0.5f);
  // LINEAR is a gradient: every step along the card moves.
  EXPECT_LT(hostLinear[0], hostLinear[1]);
  EXPECT_LT(hostLinear[1], hostLinear[2]);
  EXPECT_LT(hostLinear[2], hostLinear[3]);

  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const std::vector<float> deviceNearest =
      acrossTheCard(photographSquare(nearest, on.runtime), at);
  const std::vector<float> deviceLinear =
      acrossTheCard(photographSquare(linear, on.runtime), at);
  ASSERT_EQ(deviceNearest.size(), at.size());
  ASSERT_EQ(deviceLinear.size(), at.size());
  EXPECT_FLOAT_EQ(deviceNearest[0], deviceNearest[1]);
  EXPECT_FLOAT_EQ(deviceNearest[2], deviceNearest[3]);
  EXPECT_GT(std::abs(deviceNearest[1] - deviceNearest[2]), 0.5f);
  EXPECT_LT(deviceLinear[0], deviceLinear[1]);
  EXPECT_LT(deviceLinear[1], deviceLinear[2]);
  EXPECT_LT(deviceLinear[2], deviceLinear[3]);
}

TEST(GpuRuntime, ATexturesWrapIsHonouredOnBothTiers) {
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

  const std::vector<float> host =
      acrossTheCard(photographSquare(frame, Runtime::cpu()), at);
  ASSERT_EQ(host.size(), at.size());
  for (size_t i = 0; i + 1 < host.size(); ++i)
    EXPECT_GT(std::abs(host[i] - host[i + 1]), 0.5f) << "host step " << i;

  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const std::vector<float> graphics =
      acrossTheCard(photographSquare(frame, on.runtime), at);
  ASSERT_EQ(graphics.size(), at.size());
  for (size_t i = 0; i + 1 < graphics.size(); ++i)
    EXPECT_GT(std::abs(graphics[i] - graphics[i + 1]), 0.5f)
        << "device step " << i;
}

TEST(SlangProgram, TheKitsOwnSurfacesCompileTheirBodies) {
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
    const auto* slang = resolved.program->as<world::diligent::SlangProgram>();
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

TEST(GpuRuntime, AnUnlitSurfaceIsItsOwnLightOnBothTiers) {
  const Frame frame = litAndUnlitCards();
  const int y = kExtent.height() / 2;

  const SkBitmap host = photographSquare(frame, Runtime::cpu());
  int litAt = 0, unlitAt = 0;
  ASSERT_TRUE(cardCentres(host, &litAt, &unlitAt));
  const SkColor4f hostUnlit = host.getColor4f(unlitAt, y);
  const SkColor4f hostLit = host.getColor4f(litAt, y);
  // The unlit card IS its base colour, and the lit one — with the sun on
  // its far side — is only what the ambient leaves of it.
  EXPECT_NEAR(hostUnlit.fR, kBodyColour.r, 3.0f / 255.0f);
  EXPECT_NEAR(hostUnlit.fG, kBodyColour.g, 3.0f / 255.0f);
  EXPECT_NEAR(hostUnlit.fB, kBodyColour.b, 3.0f / 255.0f);
  EXPECT_LT(hostLit.fR, hostUnlit.fR * 0.5f);

  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  const SkBitmap graphics = photographSquare(frame, on.runtime);
  ASSERT_TRUE(cardCentres(graphics, &litAt, &unlitAt));
  const SkColor4f deviceUnlit = graphics.getColor4f(unlitAt, y);
  const SkColor4f deviceLit = graphics.getColor4f(litAt, y);
  EXPECT_NEAR(deviceUnlit.fR, kBodyColour.r, 3.0f / 255.0f);
  EXPECT_NEAR(deviceUnlit.fG, kBodyColour.g, 3.0f / 255.0f);
  EXPECT_NEAR(deviceUnlit.fB, kBodyColour.b, 3.0f / 255.0f);
  EXPECT_LT(deviceLit.fR, deviceUnlit.fR * 0.5f);
}

TEST(GpuRuntime, AMapAlreadyOnThisDeviceIsBoundWhereItStands) {
  const OnDevice on = onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  core::hardware::GpuDevice* gpu = on.device->gpu();
  skia::GraphiteContext* graphite = on.device->graphite();
  if (!gpu || !graphite) GTEST_SKIP() << "the device was not adopted";

  // 2D paints into a texture on the one shared device…
  core::hardware::TextureDesc desc;
  desc.width = desc.height = 16;
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  const core::hardware::TextureHandle handle = gpu->createTexture(desc);
  ASSERT_TRUE((bool)handle);
  {
    const world::diligent::Device::QueueLock lock(*on.device);
    skia::OffscreenSurface surface(*graphite, *gpu, handle);
    ASSERT_NE(surface.canvas(), nullptr);
    surface.canvas()->clear(SkColor4f{0.15f, 0.35f, 0.95f, 1.0f});
    surface.submit();
  }
  const core::hardware::NativeTexture native = gpu->exportNative(handle);
  ASSERT_TRUE((bool)native);

  material::DeviceImage where;
  where.device = gpu;
  where.pointer = native.mtlTexture;
  where.handle = native.vkImage;
  where.format = native.vkFormat;
  where.layout = native.vkLayout;
  where.width = native.width;
  where.height = native.height;

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
  gpu->destroy(handle);
}
