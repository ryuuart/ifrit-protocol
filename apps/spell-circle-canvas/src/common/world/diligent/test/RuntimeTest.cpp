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
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilworld/diligent/Device.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
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
  // The chain is cooked on the runtime the PASS carries, which is the
  // host one on both tiers until a point-operator kernel exists for the
  // device — so this is an equality and not a distance.
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
