#include <sigildraw/Pen.h>
#include <sigildraw/brush/Deposit.h>
#include <sigildraw/brush/Stroke.h>
#include <sigildraw/brush/Tool.h>
#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Paint.h>

#include <array>
#include <memory>

#include "support/Host.h"

namespace {

namespace brush = sigil::draw::brush;

struct PaintParameters {
  float strength = 0.75f;
  std::array<float, 1> wetness{0.25f};
};

TEST(MaterialInput, BrushPixelsAndUniformsReachACachedShaderConsumer) {
  Host host(48, 32);
  auto pixels = std::make_shared<material::skia::PixelBuffer>(48, 32);
  pixels->canvas().clear(SK_ColorBLACK);
  pixels->commit();

  material::Material shader = material::shader(
      R"(
half4 main(float2 p) {
  float coverage = painted.eval(p).r;
  return half4(coverage * strength, coverage * wetness[0], 0, 1);
}
)",
      PaintParameters{}, {.textures = {{"painted", {}}}});
  const auto publish = [&] {
    shader.slot("painted",
                material::skia::base(material::skia::buffer(pixels)));
  };
  const auto tree = [&] {
    return box().width(48).height(32).fill(shader).cache(Cache::Picture);
  };
  const auto paint = [&](float y) {
    auto tool = brush::marker({1, 1, 1, 1}, 8);
    tool.opacity = 1;
    tool.pressure = {1, 1, 1};
    tool.pressureSize = 0;
    tool.pressureOpacity = 0;
    tool.markerTip = false;
    sigil::draw::on(pixels->canvas(), {48, 32}, [&](sigil::draw::Pen& pen) {
      brush::paint(pen, tool, brush::segment({8, y}, {40, y}));
    });
  };

  publish();
  host.composer.render(tree());
  host.frame();
  ASSERT_EQ(host.pixel(24, 10), SK_ColorBLACK);

  paint(10);
  publish();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(24, 10), SK_ColorBLACK)
      << "an uncommitted edit keeps the published pixels";

  pixels->commit();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(24, 10), SK_ColorBLACK)
      << "a held shader input keeps its earlier snapshot";

  publish();
  host.composer.render(tree());
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 10)), 191, 2);
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 64, 2);
  EXPECT_EQ(host.pixel(24, 20), SK_ColorBLACK);

  publish();
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 10)), 191, 2);

  paint(24);
  pixels->commit();
  publish();
  host.composer.render(tree());
  host.frame();
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 10)), 191, 2);
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 24)), 191, 2);
  EXPECT_EQ(host.pixel(24, 18), SK_ColorBLACK);

  shader.set("strength", 0.5f);
  host.composer.render(tree());
  host.frame();
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 10)), 128, 2);
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 24)), 128, 2);

  // The clocked program exercises live publication after the static
  // program has proved that unchanged pixel snapshots prune.
  shader = material::shader(
      R"(
half4 main(float2 p) {
  float coverage = painted.eval(p).r;
  return half4(coverage * strength, coverage * wetness[0],
               coverage * min(uTime * 0.25, 1), 1);
}
)",
      PaintParameters{.strength = 0.5f}, {.textures = {{"painted", {}}}});
  publish();
  auto wetness = std::make_shared<material::UniformBlock>(1);
  wetness->values()[0] = 0.25f;
  wetness->commit();
  shader.bind("wetness", wetness);
  host.composer.render(tree());
  host.frame();
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 64, 2);

  wetness->values()[0] = 0.8f;
  shader.set("strength", 0.25f);
  host.composer.render(tree());
  host.frame(1);
  EXPECT_NEAR(SkColorGetR(host.pixel(24, 10)), 64, 2);
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 64, 2)
      << "a changed scalar does not publish the draft array";
  EXPECT_NEAR(SkColorGetB(host.pixel(24, 10)), 64, 2);
  host.frame(1);
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 64, 2)
      << "a changed frame clock does not publish the draft array";
  EXPECT_NEAR(SkColorGetB(host.pixel(24, 10)), 128, 2);
  wetness->commit();
  host.frame();
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 204, 2)
      << "a bound uniform publishes without rebuilding the tree";
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 24)), 204, 2);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_NEAR(SkColorGetG(host.pixel(24, 10)), 204, 2);

  pixels->canvas().clear(SK_ColorBLACK);
  pixels->commit();
  publish();
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(24, 10), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(24, 24), SK_ColorBLACK);
}

}  // namespace
