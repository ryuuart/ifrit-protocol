/** @file
 * WHAT AN AUTHORED EFFECT NEEDS OF THE DEVICE, held to the pixels: a
 * texture that holds data reaching a shader exactly; a surface that adds,
 * and one that writes no depth; the frame's clock playing a flipbook; light
 * brighter than white surviving to the final stage, which leaves a surface
 * drawn as authored exactly as it was authored; a bloom that spreads only
 * what passes its threshold; a surface that fades where it meets the
 * scene; and a body that turns to face the camera.
 *
 * Every case reads its answer off the pixels of one picture, or of two
 * pictures that differ in one statement.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilworld/World.h>
#include <sigilworld/diligent/Import.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** How the camera reads the picture. */
struct Reading {
  float exposure = 1.0f;
  ToneCurve curve = ToneCurve::Linear;
};

/** @p content seen from @p eye, read as @p reading says, with @p passes
 *  after the one that paints it, photographed @p seconds into the scene. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      Reading reading = {}, double seconds = 0.0,
                      std::vector<Pass> passes = {},
                      gm::camera::Camera eye = diligent::levelEye()) {
  content.push_back(
      camera(eye, {.exposure = reading.exposure, .toneCurve = reading.curve})
          .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).runtime(runtime).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  for (Pass& pass : passes) frame.pass(std::move(pass));
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(motion::Duration(seconds));
  scene.render(frame);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);
  draw(scene, canvas);
  return bitmap;
}

/** A display-encoded value as the light it stands for, and back. */
float decoded(float code) {
  return code <= 0.04045f ? code / 12.92f
                          : std::pow((code + 0.055f) / 1.055f, 2.4f);
}
float encoded(float light) {
  const float x = std::clamp(light, 0.0f, 1.0f);
  return x <= 0.0031308f ? x * 12.92f
                         : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

/** A surface drawn as authored, in @p colour, composited by @p blend. */
material::Material flat(material::Color colour,
                        material::BlendMode blend = material::BlendMode::Normal,
                        std::optional<bool> depthWrite = std::nullopt) {
  return material::from(colour).surface(
      {.unlit = true, .blend = blend, .depthWrite = depthWrite});
}

/** A card of @p size facing the camera at depth @p z, wearing @p surface. */
Node card(std::string key, const material::Material& surface, float z = 0,
          float size = 120) {
  return mesh(gm::quad(size, size))
      .key(std::move(key))
      .at({0, 0, z})
      .fill(surface);
}

/** An image of @p width x @p height in @p type and @p alpha, every texel
 *  written by @p texel(x, y, channels). */
template <class Texel>
sk_sp<SkImage> image(int width, int height, SkColorType type, SkAlphaType alpha,
                     Texel texel) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(width, height, type, alpha));
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) texel(x, y, bitmap.getAddr(x, y));
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** A PROBE: a surface drawn as authored whose colour is white where the
 *  Slang @p check — an expression over `read`, what the texture holds at
 *  its first texel — holds, and black where it does not. */
material::Material probe(const std::string& check, material::Texture data) {
  struct Nothing {};
  auto recipe = std::make_shared<const material::Recipe>(
      material::Recipe::of<Nothing>("world.test.probe." + check)
          .slot("data")
          .body(material::Target::Slang,
                "float4 surface(float2 uv) {\n"
                "  let read = data.Sample(float2(0.25, 0.25));\n"
                "  return (" +
                    check +
                    ") ? float4(1.0, 1.0, 1.0, 1.0) : float4(0.0, 0.0, 0.0, "
                    "1.0);\n}\n"));
  material::Material reading(recipe);
  reading.slot("data", std::move(data));
  return material::from(reading).surface({.unlit = true});
}

/** A 2 x 2 sheet of four colours, a cell of four texels each, read row by
 *  row: red, green, blue, white. */
material::Texture sheet() {
  static const uint8_t colours[4][3] = {
      {220, 30, 30}, {30, 200, 40}, {40, 60, 230}, {240, 240, 240}};
  return material::Texture(
      media::PixelSource(image(8, 8, kRGBA_8888_SkColorType,
                               kPremul_SkAlphaType, [](int x, int y, void* at) {
                                 const int cell = (y / 4) * 2 + (x / 4);
                                 auto* texel = static_cast<uint8_t*>(at);
                                 texel[0] = colours[cell][0];
                                 texel[1] = colours[cell][1];
                                 texel[2] = colours[cell][2];
                                 texel[3] = 255;
                               })));
}

/** The colour a cell of `sheet()` is, as a display reads it. */
SkColor4f cell(int index) {
  static const uint8_t colours[4][3] = {
      {220, 30, 30}, {30, 200, 40}, {40, 60, 230}, {240, 240, 240}};
  return {colours[index][0] / 255.0f, colours[index][1] / 255.0f,
          colours[index][2] / 255.0f, 1.0f};
}

/** How far apart two colours are, in display levels, at the channel they
 *  disagree about most. */
float levelsApart(SkColor4f a, SkColor4f b) {
  return 255.0f * std::max({std::abs(a.fR - b.fR), std::abs(a.fG - b.fG),
                            std::abs(a.fB - b.fB)});
}

/** @p sheet played with @p options, drawn as authored on a card. */
Node flipbookCard(material::FlipbookOptions options) {
  return card("flipbook",
              material::from(material::flipbook(sheet(), std::move(options)))
                  .surface({.unlit = true}));
}

}  // namespace

// ---- textures that hold data ------------------------------------------

TEST(DataTexture, AFloatTexelReachesAShaderExactly) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const sk_sp<SkImage> floats =
      image(2, 2, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType,
            [](int, int, void* at) {
              auto* texel = static_cast<float*>(at);
              texel[0] = 0.123456f;
              texel[1] = 3.5f;
              texel[2] = -0.25f;
              texel[3] = 0.0f;
            });
  const material::Texture data = material::Texture(media::PixelSource(floats))
                                     .colorSpace(media::ColorSpace::Data)
                                     .sampling(material::Sampling::Nearest);
  const SkBitmap plate = photographed(
      {card("probe", probe("abs(read.r - 0.123456) < 1e-5 && abs(read.g - "
                           "3.5) < 1e-5 && abs(read.b + 0.25) < 1e-5 && "
                           "abs(read.a) < 1e-5",
                           data))},
      on.runtime);
  EXPECT_GT(at(plate, 0.5f, 0.5f).fG, 0.95f)
      << "a float texel, above one, below zero and with no alpha, reaches "
         "the shader as it was written";
}

// A PAGE PAINTED IN HALF FLOATS ON THE DEVICE is worn as half floats: the
// value above one it holds reaches the shader as it was painted, with no
// copy through host memory.
TEST(DataTexture, AHalfFloatPageHoldsAValueAboveOneOnABody) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const diligent::PaintedTexture painted = diligent::paintOnDevice(
      *on.device, 8, core::hardware::TextureFormat::RGBA16Float,
      [](SkCanvas& canvas) { canvas.clear(SkColor4f{3.5f, 0.5f, 0.25f, 1}); });
  if (!painted.native) GTEST_SKIP() << "2D on the 3D device is unavailable";
  const material::Texture page =
      world::diligent::importNative(*on.device, painted.native)
          .colorSpace(media::ColorSpace::Data)
          .sampling(material::Sampling::Nearest);
  ASSERT_TRUE(page.valid());
  const SkBitmap plate = photographed(
      {card("probe", probe("abs(read.r - 3.5) < 1e-2 && abs(read.g - 0.5) < "
                           "1e-2 && abs(read.b - 0.25) < 1e-2",
                           page))},
      on.runtime);
  EXPECT_GT(at(plate, 0.5f, 0.5f).fG, 0.95f)
      << "a half-float page, above one, reaches the shader as it was painted";
  on.device->gpu()->destroy(painted.handle);
}

TEST(DataTexture, AnEightBitTexelWithNoAlphaKeepsItsColour) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto texels = [](int, int, void* at) {
    auto* texel = static_cast<uint8_t*>(at);
    texel[0] = 200;
    texel[1] = 100;
    texel[2] = 50;
    texel[3] = 0;
  };
  const material::Texture data =
      material::Texture(
          media::PixelSource(image(2, 2, kRGBA_8888_SkColorType,
                                   kUnpremul_SkAlphaType, texels)))
          .colorSpace(media::ColorSpace::Data)
          .sampling(material::Sampling::Nearest);
  const std::string check =
      "abs(read.r - 200.0 / 255.0) < 0.002 && abs(read.g - 100.0 / 255.0) < "
      "0.002 && abs(read.b - 50.0 / 255.0) < 0.002 && read.a < 0.002";
  const SkBitmap plate =
      photographed({card("probe", probe(check, data))}, on.runtime);
  EXPECT_GT(at(plate, 0.5f, 0.5f).fG, 0.95f)
      << "a texel whose alpha is zero keeps its red, green and blue";
}

TEST(DataTexture, AColourTexelIsStillPremultiplied) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto texels = [](int, int, void* at) {
    auto* texel = static_cast<uint8_t*>(at);
    texel[0] = 200;
    texel[1] = 100;
    texel[2] = 50;
    texel[3] = 128;
  };
  const material::Texture colour =
      material::Texture(
          media::PixelSource(image(2, 2, kRGBA_8888_SkColorType,
                                   kUnpremul_SkAlphaType, texels)))
          .sampling(material::Sampling::Nearest);
  // A colour texel reaches a shader with its alpha multiplied in, as it
  // always has.
  const std::string check =
      "abs(read.r - 100.0 / 255.0) < 0.006 && abs(read.g - 50.0 / 255.0) < "
      "0.006 && abs(read.a - 128.0 / 255.0) < 0.006";
  const SkBitmap plate =
      photographed({card("probe", probe(check, colour))}, on.runtime);
  EXPECT_GT(at(plate, 0.5f, 0.5f).fG, 0.95f);
}

TEST(DataTexture, ANormalMapWithAlphaIsReadAsStored) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The same tilted normal, once with its alpha full and once with
  // another value packed into the alpha: a normal map is data by its
  // role, so the alpha says nothing about the other three channels.
  const auto normals = [](uint8_t alpha) {
    return material::Texture(media::PixelSource(
        image(4, 4, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType,
              [alpha](int, int, void* at) {
                auto* texel = static_cast<uint8_t*>(at);
                texel[0] = 210;
                texel[1] = 128;
                texel[2] = 200;
                texel[3] = alpha;
              })));
  };
  const auto lit = [&](uint8_t alpha) {
    material::Material surface = material::surface::program(
        {.baseColor = {0.7f, 0.7f, 0.7f, 1}, .roughness = 0.6f});
    surface.slot(material::surface::kNormalSlot, normals(alpha));
    return photographed(
        {directionalLight({-0.7f, 0.0f, -0.7f}, {.intensity = 3.0f}).key("sun"),
         card("card", surface)},
        on.runtime);
  };
  const SkBitmap full = lit(255);
  const SkBitmap packed = lit(90);
  EXPECT_LE(diligent::worstChannel(full, packed), 1);
}

// ---- how a surface composites -----------------------------------------

TEST(Blending, AnAdditiveCardOverAGreyPlateAdds) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {card("plate", flat({0.4f, 0.4f, 0.4f, 1}), -10, 150),
       card("glow",
            flat({0.2f, 0.1f, 0.0f, 1}, material::BlendMode::PlusLighter))},
      on.runtime);
  const SkColor4f shown = at(plate, 0.5f, 0.5f);
  EXPECT_NEAR(shown.fR, encoded(decoded(0.4f) + decoded(0.2f)), 2.0f / 255);
  EXPECT_NEAR(shown.fG, encoded(decoded(0.4f) + decoded(0.1f)), 2.0f / 255);
  EXPECT_NEAR(shown.fB, 0.4f, 2.0f / 255);
}

TEST(Blending, TwoAdditiveCardsAddInEitherOrder) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const material::Material warm =
      flat({0.5f, 0.2f, 0.1f, 1}, material::BlendMode::PlusLighter);
  const material::Material cool =
      flat({0.1f, 0.2f, 0.5f, 1}, material::BlendMode::PlusLighter);
  const SkBitmap one =
      photographed({card("a", warm, -5), card("b", cool, 5, 80)}, on.runtime);
  const SkBitmap other =
      photographed({card("a", warm, 5), card("b", cool, -5, 80)}, on.runtime);
  // Where both cover, whichever stands nearer.
  EXPECT_EQ(
      diligent::pixelsApart(
          one, other,
          SkIRect::MakeXYWH(kExtent.x / 2 - 15, kExtent.y / 2 - 15, 30, 30), 1),
      0);
}

TEST(Blending, ACardThatWritesNoDepthHidesNothingDrawnAfterIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The second card stands BEHIND the first, but its node stands in front
  // of it, so it is drawn after it: only the first card's depth can hide
  // it.
  gm::Mesh behind = gm::quad(80, 80);
  for (glm::vec3& position : behind.positions) position.z -= 60.0f;
  const auto picture = [&](bool writes) {
    return photographed(
        {card("front", flat({0.3f, 0.0f, 0.0f, 1},
                            material::BlendMode::PlusLighter, writes)),
         mesh(behind)
             .key("behind")
             .at({0, 0, 40})
             .fill(flat({0.0f, 0.3f, 0.0f, 1},
                        material::BlendMode::PlusLighter))},
        on.runtime);
  };
  EXPECT_GT(at(picture(false), 0.5f, 0.5f).fG, 0.25f)
      << "a blended card that writes no depth hides nothing";
  EXPECT_LT(at(picture(true), 0.5f, 0.5f).fG, 0.05f)
      << "the same card writing depth hides what is behind it";
}

TEST(Blending, AnOpaqueBodyStillHidesWhatIsBehindIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {card("front", flat({0.8f, 0.1f, 0.1f, 1}), 10, 80),
       card("behind", flat({0.1f, 0.1f, 0.8f, 1}), -10),
       card("glow",
            flat({0.0f, 0.6f, 0.0f, 1}, material::BlendMode::PlusLighter),
            -20)},
      on.runtime);
  const SkColor4f centre = at(plate, 0.5f, 0.5f);
  EXPECT_NEAR(centre.fR, 0.8f, 2.0f / 255);
  EXPECT_LT(centre.fG, 0.12f) << "nothing behind an opaque body shows";
}

TEST(Blending, ABodyWritingDepthReplacesWhatItCoversAndOneWritingNoneBlends) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A half-transparent card over a blue plate. Writing depth, it is
  // opaque: drawn with blending off, its pixels replace the plate's and no
  // blue reaches the picture through it. Writing none, it is drawn over the
  // plate by its alpha and the blue shows through.
  const auto picture = [&](bool writes) {
    return photographed({card("plate", flat({0.0f, 0.0f, 0.8f, 1}), -10, 150),
                         card("veil",
                              flat({0.0f, 0.8f, 0.0f, 0.5f},
                                   material::BlendMode::Normal, writes),
                              0, 80)},
                        on.runtime);
  };
  const SkColor4f opaque = at(picture(true), 0.5f, 0.5f);
  const SkColor4f translucent = at(picture(false), 0.5f, 0.5f);
  EXPECT_LT(opaque.fB, 1.5f / 255) << "a body writing depth replaces the plate";
  EXPECT_GT(opaque.fG, 0.2f) << "and shows its own colour";
  EXPECT_GT(translucent.fB, 0.3f) << "a body writing no depth blends over it";
  EXPECT_GT(translucent.fG, 0.2f);
}

// ---- the clock and the flipbook ----------------------------------------

TEST(Flipbook, ShowsTheCellTheClockStandsAt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const material::FlipbookOptions options{
      .columns = 2, .rows = 2, .framesPerSecond = 1};
  for (int frame = 0; frame < 4; ++frame) {
    const SkBitmap plate =
        photographed({flipbookCard(options)}, on.runtime, {}, frame + 0.5);
    EXPECT_LE(levelsApart(at(plate, 0.5f, 0.5f), cell(frame)), 1.0f)
        << "frame " << frame;
  }
}

TEST(Flipbook, BlendingFramesHalfwayIsTheMeanOfTwoCells) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed({flipbookCard({.columns = 2,
                                                     .rows = 2,
                                                     .framesPerSecond = 1,
                                                     .blendFrames = true})},
                                      on.runtime, {}, 1.5);
  const SkColor4f a = cell(1), b = cell(2);
  const SkColor4f mean{(a.fR + b.fR) / 2, (a.fG + b.fG) / 2, (a.fB + b.fB) / 2,
                       1};
  EXPECT_LE(levelsApart(at(plate, 0.5f, 0.5f), mean), 1.5f);
}

TEST(Flipbook, LoopsOrHoldsItsLastFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap looped = photographed(
      {flipbookCard({.columns = 2, .rows = 2, .framesPerSecond = 1})},
      on.runtime, {}, 5.5);
  EXPECT_LE(levelsApart(at(looped, 0.5f, 0.5f), cell(1)), 1.0f);
  const SkBitmap held = photographed(
      {flipbookCard(
          {.columns = 2, .rows = 2, .framesPerSecond = 1, .loop = false})},
      on.runtime, {}, 5.5);
  EXPECT_LE(levelsApart(at(held, 0.5f, 0.5f), cell(3)), 1.0f);
}

TEST(Flipbook, ABoundFrameWinsOverTheClock) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate =
      photographed({flipbookCard({.columns = 2,
                                  .rows = 2,
                                  .framesPerSecond = 1,
                                  .frame = motion::Animatable<float>(3.0f)})},
                   on.runtime, {}, 0.5);
  EXPECT_LE(levelsApart(at(plate, 0.5f, 0.5f), cell(3)), 1.0f);
}

// ---- light above white, and the final stage ----------------------------

TEST(FinalStage, LightTwiceAsBrightAsWhiteSurvivesToIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto glowing = [&](float strength) {
    return photographed(
        {card("glow", material::from(material::Color{0, 0, 0, 1})
                          .surface({.metallic = 1.0f,
                                    .emission = {1, 1, 1, 1},
                                    .emissionStrength = strength}))},
        on.runtime, {.exposure = 0.25f});
  };
  // Two units of light read at a quarter are half: a target that held
  // only up to white would have shown a quarter.
  EXPECT_NEAR(at(glowing(2.0f), 0.5f, 0.5f).fR, encoded(0.5f), 2.0f / 255);
  EXPECT_NEAR(at(glowing(1.0f), 0.5f, 0.5f).fR, encoded(0.25f), 2.0f / 255);
}

TEST(FinalStage, ASurfaceDrawnAsAuthoredLeavesItExactly) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const uint8_t authored[3] = {187, 54, 140};
  const material::Color colour{authored[0] / 255.0f, authored[1] / 255.0f,
                               authored[2] / 255.0f, 1};
  for (const Reading reading :
       {Reading{1.0f, ToneCurve::Reinhard}, Reading{1.7f, ToneCurve::Reinhard},
        Reading{0.6f, ToneCurve::Neutral}, Reading{2.0f, ToneCurve::Neutral},
        Reading{1.0f, ToneCurve::Linear}, Reading{0.3f, ToneCurve::Linear}}) {
    const SkBitmap plate =
        photographed({card("card", flat(colour))}, on.runtime, reading);
    const SkColor color = plate.getColor(kExtent.x / 2, kExtent.y / 2);
    EXPECT_EQ(SkColorGetR(color), authored[0]);
    EXPECT_EQ(SkColorGetG(color), authored[1]);
    EXPECT_EQ(SkColorGetB(color), authored[2]);
  }
}

TEST(FinalStage, AnOverlayIsLaidOverThePictureAsAPageIs) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A half-transparent grey panel with a soft edge, as a page paints one.
  const sk_sp<SkImage> panel = [] {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorTRANSPARENT);
    SkPaint paint;
    paint.setColor(SkColorSetARGB(128, 90, 90, 90));
    paint.setAntiAlias(true);
    canvas.drawCircle(kExtent.x * 0.5f, kExtent.y * 0.5f, kExtent.x * 0.3f,
                      paint);
    return bitmap.asImage();
  }();
  // A bright scene, and the same scene with the panel laid over it.
  const auto scene = [&](bool overlaid) {
    Frame frame(group({card("card", flat({0.95f, 0.8f, 0.3f, 1})),
                       camera(diligent::levelEye()).key("camera")})
                    .key("set"));
    frame.extent(kExtent).pass(
        geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    if (overlaid) frame.overlay(material::Texture(media::PixelSource(panel)));
    return diligent::photograph(frame, on.runtime, kExtent);
  };
  SkBitmap composited = scene(false);
  {
    SkCanvas canvas(composited);
    canvas.drawImage(panel, 0, 0);
  }
  EXPECT_LE(diligent::worstChannel(scene(true), composited), 1);
}

// ---- bloom -------------------------------------------------------------

TEST(Bloom, SpreadsLightFromAnEmissiveBodyOntoItsNeighbours) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const std::vector<Node> ember = {
      card("ember",
           material::from(material::Color{0, 0, 0, 1})
               .surface({.metallic = 1.0f,
                         .emission = {1.0f, 0.5f, 0.2f, 1},
                         .emissionStrength = 6.0f}),
           0, 30)};
  const SkBitmap plain = photographed(ember, on.runtime, {.exposure = 0.5f});
  const SkBitmap glowing = photographed(
      ember, on.runtime, {.exposure = 0.5f}, 0.0,
      {bloom({.threshold = 1.0f, .intensity = 1.0f, .radius = 24})});
  // Beside the ember, outside it: dark without the bloom, lit with it.
  const float beside = 0.5f + 25.0f / (float)kExtent.x;
  EXPECT_LT(at(plain, beside, 0.5f).fR, 0.02f);
  EXPECT_GT(at(glowing, beside, 0.5f).fR, at(plain, beside, 0.5f).fR + 0.05f);
}

TEST(Bloom, AddsNothingWhereNothingPassesTheThreshold) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const std::vector<Node> dim = {
      card("dim", flat({0.6f, 0.5f, 0.4f, 1})),
      card("ember",
           material::from(material::Color{0, 0, 0, 1})
               .surface({.metallic = 1.0f,
                         .emission = {1, 1, 1, 1},
                         .emissionStrength = 0.4f}),
           5, 30)};
  const SkBitmap plain = photographed(dim, on.runtime);
  const SkBitmap bloomed =
      photographed(dim, on.runtime, {}, 0.0, {bloom({.threshold = 1.0f})});
  EXPECT_EQ(diligent::worstChannel(plain, bloomed), 0);
}

// ---- soft edges where a surface meets the scene ------------------------

TEST(DepthFade, AFadingCardIsGoneWhereItMeetsTheSceneAndWholeAwayFromIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // An opaque wall turned sixty degrees through the card's own plane: at
  // the middle of the picture the two meet, to one side the wall falls
  // away behind the card and to the other it stands in front of it.
  const SkBitmap plate = photographed(
      {mesh(gm::quad(240, 240))
           .key("wall")
           .rotateY(60)
           .fill(flat({0.1f, 0.1f, 0.8f, 1})),
       card("haze", material::from(material::Color{0.9f, 0.1f, 0.1f, 1})
                        .surface({.unlit = true, .depthFade = 30.0f}))},
      on.runtime);
  const SkColor4f where = at(plate, 0.5f, 0.5f);
  const SkColor4f left = at(plate, 0.2f, 0.5f);
  const SkColor4f right = at(plate, 0.8f, 0.5f);
  EXPECT_LT(where.fR, 0.2f) << "no hard line where the card meets the wall";
  EXPECT_GT(std::max(left.fR, right.fR), 0.85f)
      << "the card is whole where the wall stands well behind it";
}

// ---- facing the camera ----------------------------------------------------

TEST(Facing, ACardFacingTheCameraIsTheSameShapeFromTwoPlaces) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::camera::Camera side = diligent::levelEye();
  side.eye = {200.0f * std::sin(0.9f), 70.0f, 200.0f * std::cos(0.9f)};
  side.eye = glm::normalize(side.eye) * 200.0f;
  const auto covered = [](const SkBitmap& plate) {
    int count = 0;
    for (int y = 0; y < plate.height(); ++y)
      for (int x = 0; x < plate.width(); ++x)
        if (SkColorGetR(plate.getColor(x, y)) > 128) ++count;
    return count;
  };
  const auto sprite = [](Facing facing) {
    return std::vector<Node>{mesh(gm::quad(60, 60))
                                 .key("sprite")
                                 .facing(facing)
                                 .fill(flat({1, 1, 1, 1}))};
  };
  const int ahead =
      covered(photographed(sprite(Facing::Camera), on.runtime, {}, 0, {}));
  const int aside = covered(
      photographed(sprite(Facing::Camera), on.runtime, {}, 0, {}, side));
  EXPECT_GT(ahead, 1000);
  EXPECT_NEAR(aside, ahead, ahead / 50) << "square on from both places";
  const int unturned =
      covered(photographed(sprite(Facing::None), on.runtime, {}, 0, {}, side));
  EXPECT_LT(unturned, ahead * 9 / 10)
      << "and foreshortened when it does not turn";
}
