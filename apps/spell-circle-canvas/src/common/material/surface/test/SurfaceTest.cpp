/** @file
 * The surface program: both recipes compile and shade through the Skia
 * backend, an authored colour and a map texel are one number, the
 * dressed program takes a decoded set, a stack asks for its operands'
 * samplers and no more, and a material's stated response lowers into the
 * program with its numbers and maps in place.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/Combine.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/mask/Mask.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Draw.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilshaders/MaterialSurface.h>

#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <memory>
#include <string>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
namespace media = sigil::media;

TEST(Surface, BothRecipesCompileAndShade) {
  surface::SurfaceParameters parameters;
  parameters.baseColor = {0.2f, 0.6f, 0.9f, 1};
  parameters.emissive = {1, 0.5f, 0, 1};
  parameters.emissiveStrength = 0.5f;
  for (const Material& m :
       {surface::program(parameters), surface::unlit(parameters)}) {
    EXPECT_TRUE(skia::shader(m, {}));
    // Every declared slot is dressed, so no body evaluates an unbound
    // child.
    EXPECT_EQ(m.slots().size(), m.recipe().slots().size());
  }
  EXPECT_TRUE(surface::isSurface(surface::program(parameters)));
  EXPECT_FALSE(surface::isUnlit(surface::program(parameters)));
  EXPECT_TRUE(surface::isUnlit(surface::unlit(parameters)));
  EXPECT_EQ(surface::program(parameters), surface::program(parameters));
  EXPECT_FALSE(surface::program(parameters) == surface::unlit(parameters));
}

TEST(Surface, NormalCompositionKeepsFlatIdentityAndConventions) {
  const Material flat = Color{0.5f, 0.5f, 1, 1};
  for (bool baseDirectX : {false, true})
    for (bool detailDirectX : {false, true})
      for (bool outputDirectX : {false, true}) {
        const surface::NormalBlendOptions options{baseDirectX, detailDirectX,
                                                  outputDirectX};
        const Material base = Color{0.5f, baseDirectX ? 0.2f : 0.8f, 0.9f, 1};
        const Material detail =
            Color{0.5f, detailDirectX ? 0.2f : 0.8f, 0.9f, 1};
        for (const Material& combined :
             {surface::blendNormals(base, flat, options),
              surface::blendNormals(flat, detail, options)}) {
          const auto bitmap = sigil::material::test::render(combined, 2, 2);
          const SkColor pixel = bitmap.getColor(0, 0);
          EXPECT_NEAR(SkColorGetR(pixel), 128, 1);
          EXPECT_NEAR(SkColorGetG(pixel), outputDirectX ? 51 : 204, 1);
          EXPECT_NEAR(SkColorGetB(pixel), 230, 1);
          EXPECT_EQ(SkColorGetA(pixel), 255u);
        }
      }
}

TEST(Surface, NormalCompositionReorientsDetailIntoTheBaseFrame) {
  const Material combined = surface::blendNormals(Color{0.8f, 0.5f, 0.9f, 1},
                                                  Color{0.5f, 0.8f, 0.9f, 1});
  const auto bitmap = sigil::material::test::render(combined, 2, 2);
  const SkColor pixel = bitmap.getColor(0, 0);
  // A 0.6 tangent component and 0.8 outward component on each input
  // compose to (0.48, 0.6, 0.64), still a unit normal.
  EXPECT_NEAR(SkColorGetR(pixel), 189, 1);
  EXPECT_NEAR(SkColorGetG(pixel), 204, 1);
  EXPECT_NEAR(SkColorGetB(pixel), 209, 1);
  EXPECT_EQ(combined, surface::blendNormals(Color{0.8f, 0.5f, 0.9f, 1},
                                            Color{0.5f, 0.8f, 0.9f, 1}));
}

namespace {

Material heightRamp(glm::vec2 slope, float alpha = 1) {
  struct Ramp {
    glm::vec2 slope;
    float alpha;
  };
  return shader(R"(
half4 main(float2 p) {
  float height = 0.5 + dot(p - float2(8.5), slope);
  return half4(half3(height * alpha), half(alpha));
})",
                Ramp{slope, alpha});
}

void expectHeightNormal(SkColor pixel, float x = 0, float y = 0) {
  const float length = std::sqrt(x * x + y * y + 1);
  EXPECT_NEAR(SkColorGetR(pixel), (x / length * 0.5f + 0.5f) * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), (y / length * 0.5f + 0.5f) * 255, 1);
  EXPECT_NEAR(SkColorGetB(pixel), (1 / length * 0.5f + 0.5f) * 255, 1);
  EXPECT_EQ(SkColorGetA(pixel), 255u);
}

}  // namespace

TEST(Surface, HeightNormalsKeepFlatInputAndInvalidOptionsFlat) {
  const Material flat = Color{0.5f, 0.5f, 1, 1};
  for (float value : {0.0f, 0.35f, 1.0f})
    expectHeightNormal(
        test::render(surface::normalFromHeight(Color{value, value, value, 1}),
                     16, 16)
            .getColor(8, 8));

  constexpr float infinity = std::numeric_limits<float>::infinity();
  constexpr float nan = std::numeric_limits<float>::quiet_NaN();
  const std::array invalid{
      surface::HeightNormalOptions{.depth = 0},
      surface::HeightNormalOptions{.depth = nan},
      surface::HeightNormalOptions{.depth = infinity},
      surface::HeightNormalOptions{.depth = -infinity},
      surface::HeightNormalOptions{.step = 0},
      surface::HeightNormalOptions{.step = -1},
      surface::HeightNormalOptions{.step = nan},
      surface::HeightNormalOptions{.step = infinity},
      surface::HeightNormalOptions{.depth = std::numeric_limits<float>::max(),
                                   .step = 0.25f}};
  for (const auto options : invalid) {
    const Material normal =
        surface::normalFromHeight(heightRamp({0.02f, 0.03f}), options);
    EXPECT_EQ(normal, flat);
    const auto built = skia::shader(skia::paint(normal));
    ASSERT_NE(built, nullptr);
    expectHeightNormal(test::render(built, 16, 16).getColor(8, 8));
  }
  expectHeightNormal(
      test::render(surface::normalFromHeight(
                       heightRamp({0.02f, 0}),
                       {.depth = std::numeric_limits<float>::max()}),
                   16, 16)
          .getColor(8, 8),
      -1.0e10f);
}

TEST(Surface, HeightNormalsPreserveSignedSlopesConventionsAndLogicalStep) {
  const Material height = heightRamp({0.02f, 0.03f});
  for (float depth : {-10.0f, 10.0f})
    for (float step : {0.25f, 1.0f, 4.0f})
      for (bool directX : {false, true}) {
        const auto options = surface::HeightNormalOptions{depth, step, directX};
        const Material normal = surface::normalFromHeight(height, options);
        EXPECT_EQ(normal, surface::normalFromHeight(height, options));
        for (float scale : {0.5f, 2.0f}) {
          const auto built = skia::shader(
              normal, {.resolution = {16, 16}, .contentScale = scale});
          ASSERT_NE(built, nullptr);
          expectHeightNormal(test::render(built, 16, 16).getColor(8, 8),
                             -depth * 0.02f,
                             depth * 0.03f * (directX ? -1 : 1));
        }
      }
  EXPECT_NE(surface::normalFromHeight(height),
            surface::normalFromHeight(height, {.depth = -1}));
  EXPECT_NE(surface::normalFromHeight(height),
            surface::normalFromHeight(height, {.step = 2}));
  EXPECT_NE(surface::normalFromHeight(height),
            surface::normalFromHeight(height, {.directX = true}));
}

TEST(Surface, HeightNormalsFollowLogicalPixelStepsThroughComposedInputs) {
  const glm::vec2 slope{.02f, .03f};
  const Material height = heightRamp(slope);
  const std::array metrics{glm::mat3{1}, glm::mat3{.5f, 0, 0, 0, 2, 0, 0, 0, 1},
                           glm::mat3{0, .5f, 0, -2, 0, 0, 0, 0, 1},
                           glm::mat3{-.5f, .2f, 0, .3f, 1, 0, 0, 0, 1}};
  for (float depth : {-4.0f, 4.0f})
    for (float step : {.25f, 1.0f, 4.0f})
      for (bool directX : {false, true}) {
        const Material normal = surface::normalFromHeight(
            height, {.depth = depth, .step = step, .directX = directX});
        const Material composed = surface::blendNormals(
            normal, Color{.5f, .5f, 1, 1},
            {.baseDirectX = directX, .outputDirectX = directX});
        for (const auto& metric : metrics) {
          FrameData frame{.resolution = {16, 16}};
          frame.localToSample = metric;
          const float x = -depth * glm::dot(slope, glm::vec2(metric[0]));
          const float y = depth * glm::dot(slope, glm::vec2(metric[1])) *
                          (directX ? -1 : 1);
          for (const Material& source : {normal, composed}) {
            for (const auto& built :
                 {skia::shader(source, frame),
                  skia::shader(skia::paint(source), frame)}) {
              ASSERT_TRUE(built);
              expectHeightNormal(test::render(built, 16, 16).getColor(8, 8), x,
                                 y);
            }
          }
        }
      }
}

TEST(Surface,
     HeightNormalMetricsInvalidateHeldPaintWithoutChangingEncodedNormals) {
  const Material derived =
      surface::normalFromHeight(heightRamp({.02f, .03f}), {.depth = 4});
  const Paint held = skia::paint(derived);
  EXPECT_TRUE(held.geometryDependent());
  EXPECT_FALSE(held.isRunning());
  FrameData frame{.resolution = {16, 16}};
  const auto first = skia::shader(held, frame);
  ASSERT_TRUE(first);
  EXPECT_EQ(first, skia::shader(held, frame));
  frame.localToSample[0][0] = .25f;
  const auto changed = skia::shader(held, frame);
  ASSERT_TRUE(changed);
  EXPECT_NE(first, changed);
  EXPECT_EQ(changed, skia::shader(held, frame));
  expectHeightNormal(test::render(changed, 16, 16).getColor(8, 8), -.02f, .12f);
  const Material encoded =
      surface::blendNormals(Color{.8f, .5f, .9f, 1}, Color{.5f, .5f, 1, 1});
  const auto control = skia::shader(encoded, {});
  EXPECT_TRUE(
      test::identical(test::render(control, 16, 16),
                      test::render(skia::shader(encoded, frame), 16, 16)));
}

TEST(Surface, NonAffineAndNonfiniteHeightMetricsHaveFlatNormals) {
  const Material normal =
      surface::normalFromHeight(heightRamp({.02f, .03f}), {.depth = 4});
  for (float invalid : {.01f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    FrameData frame{.resolution = {16, 16}};
    frame.localToSample[0][2] = invalid;
    expectHeightNormal(
        test::render(skia::shader(normal, frame), 16, 16).getColor(8, 8));
  }
  for (float invalid : {std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    FrameData frame{.resolution = {16, 16}};
    frame.localToSample[0][0] = invalid;
    expectHeightNormal(
        test::render(skia::shader(normal, frame), 16, 16).getColor(8, 8));
  }
  for (float translation : {200.f, std::numeric_limits<float>::infinity(),
                            std::numeric_limits<float>::quiet_NaN()}) {
    FrameData frame{.resolution = {16, 16}};
    frame.localToSample[2][0] = translation;
    frame.localToSample[2][1] = translation;
    expectHeightNormal(
        test::render(skia::shader(normal, frame), 16, 16).getColor(8, 8), -.08f,
        .12f);
  }
}

TEST(Surface, HeightNormalsUsePremultipliedHeightAndClampItsRange) {
  for (float alpha : {0.0f, 0.25f, 1.0f})
    expectHeightNormal(
        test::render(surface::normalFromHeight(heightRamp({0.2f, 0}, alpha),
                                               {.depth = 2}),
                     16, 16)
            .getColor(8, 8),
        -0.4f * alpha);
  const auto clamped = test::render(
      surface::normalFromHeight(heightRamp({0.2f, 0}), {.depth = 2}), 16, 16);
  expectHeightNormal(clamped.getColor(0, 8));
  expectHeightNormal(clamped.getColor(15, 8));
  const Material redHeight = shader(R"(
half4 main(float2 p) { return half4(0.5 + 0.2 * (p.x - 8.5), 0, 0, 1); }
)");
  expectHeightNormal(
      test::render(surface::normalFromHeight(redHeight, {.depth = 2}), 16, 16)
          .getColor(8, 8),
      -0.4f * 0.2126f);
}

TEST(Surface, HeightNormalsPreserveTexturePlacementAndPaddedEdges) {
  SkBitmap pixels;
  pixels.allocN32Pixels(8, 8, true);
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      const unsigned height = 40 + 12 * x + 8 * y;
      *pixels.getAddr32(x, y) = SkPreMultiplyARGB(255, height, height, height);
    }
  pixels.setImmutable();
  glm::mat3 placement(1);
  placement[0][0] = 2;
  placement[1][1] = 4;
  placement[2][0] = 4;
  placement[2][1] = 3;
  const Material height = image(Texture(pixels.asImage())
                                    .uv(placement)
                                    .tile(Repeat::Pad)
                                    .sampling(Sampling::Linear));
  const Material normal = surface::normalFromHeight(height, {.depth = 8});
  const auto built = skia::shader(normal, {.resolution = {32, 48}});
  ASSERT_NE(built, nullptr);
  const auto bitmap = test::render(built, 32, 48);
  // Adjacent texels rise by 12 and 8 code values. Their placed distances
  // are two and four logical pixels, respectively.
  constexpr float xSlope = -8.0f * 12 / (255 * 2);
  constexpr float ySlope = 8.0f * 8 / (255 * 4);
  expectHeightNormal(bitmap.getColor(10, 15), xSlope, ySlope);
  expectHeightNormal(bitmap.getColor(1, 15), 0, ySlope);
  expectHeightNormal(bitmap.getColor(24, 15), 0, ySlope);
  expectHeightNormal(bitmap.getColor(10, 2), xSlope, 0);
  expectHeightNormal(bitmap.getColor(10, 40), xSlope, 0);
  expectHeightNormal(bitmap.getColor(1, 40));
}

TEST(Surface, HeightNormalsRetainLiveComposedInputThroughAHeldPaint) {
  const Material live = shader(R"(
half4 main(float2 p) {
  float height = 0.25 + uTime * 0.32 * p.x / max(uResolution.x, 1.0);
  return half4(half3(height), 1);
})");
  const Material height =
      from(Color{0, 0, 0, 1}).layer(live, {.opacity = 0.5f});
  const Material normal = surface::normalFromHeight(height, {.depth = 8});
  const Paint held = skia::paint(normal);
  ASSERT_TRUE(held.isRunning());
  ASSERT_TRUE(held.geometryDependent());
  const std::array frames{FrameData{.seconds = 0, .resolution = {16, 16}},
                          FrameData{.seconds = 2, .resolution = {16, 16}},
                          FrameData{.seconds = 2, .resolution = {32, 16}},
                          FrameData{.seconds = 0, .resolution = {16, 16}}};
  for (const FrameData& frame : frames) {
    const auto built = skia::shader(held, frame);
    ASSERT_NE(built, nullptr);
    expectHeightNormal(
        test::render(built, 16, 16).getColor(8, 8),
        static_cast<float>(-frame.seconds * 1.28 / frame.resolution.x));
  }
}

TEST(Surface, AnAuthoredColourAndAMapTexelAreOneNumber) {
  // A mid-grey as an author types it. A builder takes it and the
  // parameter holds it, because nothing between the two has a space to
  // convert between.
  const int code = 128;
  const float typed = (float)code / 255.0f;
  const surface::SurfaceParameters authored{
      .baseColor = {typed, typed, typed, 1}, .metallic = 0, .roughness = 0.5f};
  EXPECT_FLOAT_EQ(authored.baseColor.r, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.g, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.b, typed);
  EXPECT_FLOAT_EQ(authored.baseColor.a, 1.0f);

  // The body multiplies the base colour by the sample of the map in its
  // slot, so the colour in the parameter and the SAME colour carried by
  // a flat map have to shade to one place. This is the claim for a real
  // image: what a decoded texel carries is the number the image stores,
  // which is the number the author typed.
  surface::SurfaceParameters white;
  white.baseColor = {1, 1, 1, 1};
  Material sampled = surface::unlit(white);
  sampled.slot(surface::kBaseColorSlot,
               Texture(media::PixelSource(test::solid(
                           SkColorSetARGB(255, code, code, code), 4, 4)))
                   .tile(Repeat::Pad));
  const SkColor fromParameter =
      test::shade(surface::unlit(authored), 4, 4).getColor(1, 1);
  const SkColor fromMap = test::shade(sampled, 4, 4).getColor(1, 1);
  EXPECT_NEAR((int)SkColorGetR(fromParameter), (int)SkColorGetR(fromMap), 1);
  EXPECT_NEAR((int)SkColorGetG(fromParameter), (int)SkColorGetG(fromMap), 1);
  EXPECT_NEAR((int)SkColorGetB(fromParameter), (int)SkColorGetB(fromMap), 1);

  // And what the surface shows is the colour that was typed: a recipe
  // that is its own light, over a white map, paints the number and not
  // a transformation of it.
  EXPECT_NEAR((int)SkColorGetR(fromParameter), code, 1);
  EXPECT_NEAR((int)SkColorGetG(fromParameter), code, 1);
  EXPECT_NEAR((int)SkColorGetB(fromParameter), code, 1);
}

TEST(Surface, EveryColourFieldReachesTheUniformAsItWasWritten) {
  surface::SurfaceParameters p{
      .roughness = 0.02f, .transmission = 1, .thickness = 0.35f};
  p.baseColor = {0.9f, 0.4f, 0.1f, 1};
  p.emissive = {0.2f, 0.3f, 0.4f, 1};
  // The Beer-Lambert coefficient is taken per unit of thickness, so a
  // dense channel is a number above one: what would be out of range for
  // a colour is in range for this, and nothing may hold it under one.
  p.absorption = {4.0f, 0.5f, 0.25f, 1};
  const Material m = surface::program(p);
  const Color baseColor = m.get<Color>("baseColor");
  const Color emissive = m.get<Color>("emissive");
  const Color absorption = m.get<Color>("absorption");
  EXPECT_FLOAT_EQ(baseColor.r, 0.9f);
  EXPECT_FLOAT_EQ(baseColor.b, 0.1f);
  EXPECT_FLOAT_EQ(emissive.g, 0.3f);
  EXPECT_FLOAT_EQ(absorption.r, 4.0f);
  EXPECT_FLOAT_EQ(absorption.g, 0.5f);
}

TEST(Surface, DressesADecodedSet) {
  const sk_sp<SkImage> image = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
    s->getCanvas()->clear(SK_ColorGRAY);
    return s->makeImageSnapshot();
  }();
  texture::TextureMaps maps;
  maps.normalDirectX = true;
  maps.maps[texture::Role::BaseColor] = Texture(media::PixelSource(image));
  maps.maps[texture::Role::Packed] = Texture(media::PixelSource(image));
  maps.maps[texture::Role::Emissive] = Texture(media::PixelSource(image));
  const Material m = surface::program(maps);
  // The packed image stands in for all three channel maps, at glTF's
  // order, and the scalars a map multiplies come up off zero.
  EXPECT_FLOAT_EQ(m.get<float>("occlusionChannel"), 0.0f);
  EXPECT_FLOAT_EQ(m.get<float>("roughnessChannel"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallicChannel"), 2.0f);
  EXPECT_FLOAT_EQ(m.get<float>("metallic"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("normalDirectX"), 1.0f);
  EXPECT_FLOAT_EQ(m.get<float>("emissiveStrength"), 1.0f);
  const auto* base =
      dynamic_cast<const Texture*>(m.leaf(surface::kBaseColorSlot));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->source(), media::PixelSource(image));
  // A set with no normal map still leaves the slot dressed flat.
  EXPECT_NE(m.leaf(surface::kNormalSlot), nullptr);
}

TEST(Over, StacksTopOverBaseWhereTheMaskSays) {
  surface::SurfaceParameters red;
  red.baseColor = {1, 0, 0, 1};
  surface::SurfaceParameters blue;
  blue.baseColor = {0, 0, 1, 1};
  const auto shade = [&](float coverage) {
    const Material m =
        over(surface::unlit(red), surface::unlit(blue), maskConstant(coverage));
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
    skia::fill(*s->getCanvas(), SkPath::Rect(SkRect::MakeWH(1, 1)), m);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    s->makeImageSnapshot()->readPixels(nullptr, bm.pixmap(), 0, 0);
    return bm.getColor(0, 0);
  };
  EXPECT_EQ(SkColorGetR(shade(0.0f)), 255u);
  EXPECT_EQ(SkColorGetB(shade(1.0f)), 255u);
  // The stack is one material: the operands are its children.
  const Material stack = over(surface::unlit(red), surface::unlit(blue),
                              maskConstant(1.0f), BlendMode::Multiply);
  EXPECT_EQ(stackDepth(stack), 1);
  EXPECT_EQ(stackDepth(over(stack, surface::unlit(red), maskConstant(1.0f))),
            2);
  EXPECT_EQ(*under(stack), surface::unlit(red));
  EXPECT_TRUE(skia::shader(stack, {}));
}

namespace {

/** A recipe whose parameters are one number nothing reads: what a case that
 *  is about slots or bodies rather than values stands a material on. */
struct NoParameters {
  float unused = 0;
};

/** A stand-in Slang compiler, so `over()` builds the COMPOSED recipe.
 *  Composition is asked for only where a compiler that needs it is
 *  installed — a language handed one body per material cannot reach a
 *  child material — and a stack built without one carries the plain
 *  three-slot recipe, which never asks what the case below asks. */
std::shared_ptr<Program> slangStandIn(std::shared_ptr<const Recipe> recipe,
                                      Variant variant, std::string&) {
  return std::make_shared<Program>(std::move(recipe), Target::Slang, variant);
}

/** The slots the compiled SkSL program declares, which is one
 *  image sampler each once a GPU backend has inlined it. */
size_t declaredSlots(const Material& m) {
  const auto built = skia::builder(m, {});
  return built ? built->effect()->children().size() : 0u;
}

/** A one-texel texture under its own producer key, so @p key images are
 *  @p key distinct leaves. */
Texture texel(int key) {
  return Texture(media::PixelSource::produce(
      "material.surface.test.texel." + std::to_string(key), [] {
        sk_sp<SkSurface> s =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(1, 1));
        s->getCanvas()->clear(SK_ColorWHITE);
        return s->makeImageSnapshot();
      }));
}

}  // namespace

TEST(Over, AStackAsksForItsOperandsSamplersAndNoMore) {
  registerCompiler(Target::Slang, slangStandIn);

  // An undressed surface fills all seven of its slots so no body ever
  // evaluates an unbound child, and the SkSL body samples two of them.
  // The five it never reads are not declared to that program and so cost
  // it no sampler.
  const Material unlit = surface::unlit();
  EXPECT_EQ(unlit.slots().size(), 7u);
  EXPECT_EQ(declaredSlots(unlit), 2u);
  EXPECT_EQ(skia::samplerCount(unlit), 2);

  const Material stack = over(surface::unlit(), surface::unlit(),
                              maskConstant(0.5f), BlendMode::Normal);
  // The composed recipe declares a slot per operand's own slot, because
  // the language it was composed for reaches no child material.
  EXPECT_GT(stack.recipe().slots().size(), 3u);
  // SkSL samples the operands themselves, so its program declares those
  // three slots and none of the composed ones.
  EXPECT_EQ(declaredSlots(stack), 3u);
  EXPECT_EQ(skia::samplerCount(stack), 2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(stack), skia::kSamplerLimit);

  // The deepest stack anything here builds: a surface under two.
  Material deep = surface::program();
  for (int i = 0; i < 2; ++i)
    deep = over(std::move(deep), surface::unlit(), maskConstant(0.5f));
  EXPECT_EQ(stackDepth(deep), 2);
  EXPECT_EQ(declaredSlots(deep), 3u);
  EXPECT_EQ(skia::samplerCount(deep), skia::samplerCount(surface::program()) +
                                          2 * skia::samplerCount(unlit));
  EXPECT_LE(skia::samplerCount(deep), skia::kSamplerLimit);
  EXPECT_TRUE(skia::shader(deep, {}));
}

TEST(Over, ATreeOverTheSamplerBudgetIsRefusedRatherThanDrawn) {
  // A device rejects a fragment program past its sampler indices after
  // Skia has accepted it, so the draw paints nothing and names nobody.
  // Refused here, the material that asked is the one reported.
  const int tooMany = skia::kSamplerLimit + 1;
  Recipe recipe = Recipe::of<NoParameters>("surface.test.overBudget");
  std::string body = "half4 main(float2 p) { return ";
  for (int i = 0; i < tooMany; ++i) {
    const std::string slot = "uMap" + std::to_string(i);
    recipe.slot(slot);
    body += (i ? " + " : "");
    body += slot + ".eval(p)";
  }
  recipe.body(Target::SkSL, body + "; }");
  Material m(std::make_shared<const Recipe>(std::move(recipe)), NoParameters{});
  for (int i = 0; i < tooMany; ++i)
    m.slot("uMap" + std::to_string(i), texel(i));
  EXPECT_EQ(skia::samplerCount(m), tooMany);
  EXPECT_FALSE(skia::shader(m, {}));
}

// ---- the lowering ----------------------------------------------------------

TEST(Surface, LowersAColourAndItsResponseIntoTheProgram) {
  const Material lowered =
      surface::lower(from(Color{0.2f, 0.4f, 0.6f, 1})
                         .surface({.metallic = 0.3f, .roughness = 0.85f}));
  ASSERT_TRUE(surface::isSurface(lowered));
  EXPECT_FALSE(surface::isUnlit(lowered));
  EXPECT_EQ(lowered.get<Color>("baseColor"), (Color{0.2f, 0.4f, 0.6f, 1}));
  EXPECT_FLOAT_EQ(lowered.get<float>("metallic"), 0.3f);
  EXPECT_FLOAT_EQ(lowered.get<float>("roughness"), 0.85f);
  // A surface program is already what it states, so it lowers to itself.
  EXPECT_EQ(surface::lower(lowered), lowered);
}

TEST(Surface, LowersUnlitAndNoResponseToTheUnlitProgram) {
  const Color red{1, 0, 0, 1};
  const Material asked = surface::lower(from(red).surface({.unlit = true}));
  EXPECT_TRUE(surface::isUnlit(asked));
  EXPECT_EQ(asked.get<Color>("baseColor"), red);
  const Material flat = surface::lower(Material(red));
  EXPECT_TRUE(surface::isUnlit(flat));
  EXPECT_EQ(flat.get<Color>("baseColor"), red);
}

TEST(Surface, LowersAnImageChannelIntoItsMapSlot) {
  const sk_sp<SkImage> grey = test::solid(SK_ColorGRAY, 2, 2);
  const Material lowered = surface::lower(
      from(Color{1, 1, 1, 1})
          .surface({.roughness = image(media::PixelSource(grey))}));
  // The map multiplies the factor, so the factor lets it through whole.
  EXPECT_FLOAT_EQ(lowered.get<float>("roughness"), 1.0f);
  const Texture* map = surface::map(lowered, surface::kRoughnessSlot);
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(map->source(), media::PixelSource(grey));
  // An image base is the base colour's map in the same way.
  const Material based =
      surface::lower(from(image(media::PixelSource(grey))).surface({}));
  const Texture* base = surface::map(based, surface::kBaseColorSlot);
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->source(), media::PixelSource(grey));
}

// ---- the embedded shader table --------------------------------------------

TEST(SurfaceShaderTable, HoldsEveryFileTheShaderDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      surface::shaderSources(), SIGIL_MATERIAL_SURFACE_SHADER_DIR);
}
