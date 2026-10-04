/** @file
 * The paint's sources: conic and focal gradients, a buffer that prunes
 * until committed, the two doors a 256-entry palette reaches an effect
 * through, the fit that maps an image onto the box it paints, and a
 * native child receiving its parent's frame.
 */

#include "SkiaPaintTestSupport.h"

TEST(SkiaPaint, AConicWindowPastTheCircleClampsRatherThanWraps) {
  // A hue wheel that is meant to start at red is written with a window
  // from 90 to 450 by everyone who writes it once — and no canvas angle ever
  // reaches past 360, so the run before 90 degrees paints the first
  // stop's flat colour instead of the ramp's tail. The factory says so,
  // once for the process.
  const std::vector<ColorStop> stops{{0.0f, {1, 0, 0, 1}},
                                     {1.0f, {0, 0, 1, 1}}};
  testing::internal::CaptureStderr();
  const Paint past = Paint::conicGradient(
      {50, 50}, stops,
      {.units = GradientUnits::Pixels, .startDegrees = 90, .endDegrees = 450});
  const std::string said = testing::internal::GetCapturedStderr();
  EXPECT_NE(said.find("do not wrap"), std::string::npos) << said;

  const SkBitmap bm = render(skia::staticShader(past), 100, 100);
  // Down and to the right of the centre is 45 degrees on the canvas —
  // before the window opens at 90 — so it paints the first stop flat
  // rather than the ramp the caller thought they had rotated onto it.
  EXPECT_EQ(SkColorGetR(bm.getColor(85, 85)), 255u);
  EXPECT_EQ(SkColorGetB(bm.getColor(85, 85)), 0u);
  // …and a window inside the circle says nothing at all.
  testing::internal::CaptureStderr();
  const Paint inside =
      Paint::conicGradient({50, 50}, stops, {.units = GradientUnits::Pixels});
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
  EXPECT_FALSE(inside == past);
}

TEST(SkiaPaint, AFocusMovesTheHotSpotAndLeavesTheOuterCircle) {
  // What moving the centre cannot do: moving a radial's centre slides the
  // whole ramp, its outer edge included, where a focus keeps the outer circle
  // where it was put and moves only the focus. A displaced highlight on a
  // sphere is the case, and the corner farthest from the displacement is
  // where the difference shows.
  const std::vector<ColorStop> stops{{0.0f, {1, 1, 1, 1}},
                                     {1.0f, {0, 0, 0, 1}}};
  const GradientOptions pixels{.units = GradientUnits::Pixels};
  const SkBitmap centred = render(
      skia::staticShader(Paint::radialGradient({50, 50}, 50, stops, pixels)),
      100, 100);
  const SkBitmap displaced =
      render(skia::staticShader(Paint::radialGradient(
                 {50, 50}, 50, stops,
                 {.units = GradientUnits::Pixels, .focus = glm::vec2{30, 30}})),
             100, 100);
  const SkBitmap slid = render(
      skia::staticShader(Paint::radialGradient({30, 30}, 50, stops, pixels)),
      100, 100);

  // The hot spot moved in both.
  EXPECT_GT(SkColorGetR(displaced.getColor(30, 30)),
            SkColorGetR(centred.getColor(30, 30)));
  // The outer circle did not, with the focus: at the left edge the ramp
  // has all but run out, as it had before the focus moved — where the
  // slid radial, whose whole ramp went with its centre, still has a long
  // way to go there.
  EXPECT_LT(SkColorGetR(displaced.getColor(2, 50)),
            SkColorGetR(slid.getColor(2, 50)));
  EXPECT_NEAR(SkColorGetR(displaced.getColor(2, 50)),
              SkColorGetR(centred.getColor(2, 50)), 24);
}

TEST(SkiaPaint, ABufferPrunesUntilItIsCommitted) {
  // The whole point of a buffer over a custom leaf: the node keeps its
  // picture caching, because the recipe compares by (source, revision).
  // An identical re-describe between commits is the same value, and the
  // first describe after a commit is a different one — exactly once.
  auto pixels = std::make_shared<skia::PixelBuffer>(4, 4);
  pixels->bitmap().eraseColor(SK_ColorRED);
  const Paint described = skia::buffer(pixels);
  EXPECT_TRUE(described == skia::buffer(pixels));

  // Writing without committing publishes nothing: the snapshot the
  // shader holds is the one taken at the last commit, so a describe over
  // an edited-but-uncommitted buffer still prunes.
  pixels->bitmap().eraseColor(SK_ColorBLUE);
  EXPECT_TRUE(described == skia::buffer(pixels));
  EXPECT_EQ(SkColorGetR(render(skia::staticShader(described)).getColor(1, 1)),
            255u);

  pixels->commit();
  const Paint after = skia::buffer(pixels);
  EXPECT_FALSE(described == after);
  EXPECT_TRUE(after == skia::buffer(pixels));
  EXPECT_EQ(SkColorGetB(render(skia::staticShader(after)).getColor(1, 1)),
            255u);

  // A null buffer is a paint that draws nothing rather than a crash.
  EXPECT_EQ(skia::staticShader(skia::buffer(nullptr)), nullptr);
}

TEST(SkiaPaint, ABufferPaintKeepsItsCapturedPixelsWhenFittedOrPanned) {
  auto pixels = std::make_shared<skia::PixelBuffer>(4, 4);
  pixels->bitmap().eraseColor(SK_ColorRED);
  pixels->commit();
  auto pan = sigil::motion::animatable(1.0f);
  auto describe = [&] {
    Paint plain =
        skia::buffer(pixels, Repeat::None, Repeat::None, SkMatrix::I(),
                     SkSamplingOptions(SkFilterMode::kNearest));
    Paint fitted = plain;
    fitted.fit(Fit::Stretch);
    Paint panned = plain;
    panned.offset(pan, std::nullopt);
    Paint fittedAndPanned = fitted;
    fittedAndPanned.offset(pan, std::nullopt);
    return std::array{plain, fitted, panned, fittedAndPanned};
  };
  const FrameData frame{.resolution = {8, 8}};
  auto expectColor = [&](const std::array<Paint, 4>& paints, SkColor color) {
    const std::array names{"native", "fitted", "panned", "fitted and panned"};
    for (size_t i = 0; i < paints.size(); ++i) {
      SCOPED_TRACE(names[i]);
      EXPECT_EQ(render(skia::shader(paints[i], frame), 8, 8).getColor(3, 1),
                color);
    }
  };
  const auto held = describe();
  expectColor(held, SK_ColorRED);
  EXPECT_EQ(render(skia::shader(held[1], frame), 8, 8).getColor(7, 7),
            SK_ColorRED);
  EXPECT_EQ(render(skia::shader(held[0], frame), 8, 8).getColor(7, 7),
            SK_ColorTRANSPARENT);
  EXPECT_EQ(render(skia::shader(held[2], frame), 8, 8).getColor(0, 1),
            SK_ColorTRANSPARENT);

  pixels->bitmap().eraseColor(SK_ColorBLUE);
  EXPECT_EQ(held, describe());
  expectColor(held, SK_ColorRED);

  pixels->commit();
  pan = 2.0f;
  // Resolving a fit or a changed pan cannot publish a newer revision.
  expectColor(held, SK_ColorRED);
  EXPECT_EQ(render(skia::shader(held[2]), 8, 8).getColor(3, 1), SK_ColorRED);
  const auto fresh = describe();
  EXPECT_NE(held, fresh);
  expectColor(fresh, SK_ColorBLUE);

  pixels->bitmap().eraseColor(SK_ColorGREEN);
  EXPECT_EQ(fresh, describe());
  expectColor(fresh, SK_ColorBLUE);
  pixels->commit();
  const auto latest = describe();
  EXPECT_NE(fresh, latest);
  expectColor(latest, SK_ColorGREEN);
  expectColor(fresh, SK_ColorBLUE);
  expectColor(held, SK_ColorRED);
}

// ---------------------------------------------------------------------------
// A FIXED PALETTE THROUGH AN EFFECT. An indexed picture — a 1994 sprite
// sheet, a datashader's category ramp — is one channel of indices and one
// 256-entry table; both doors an effect has for that table are here, so a
// consumer never has to bake one sprite per palette.

namespace {

/** Entry i is (i/255, 1 - i/255, 0, 1): the index and its colour are the
 *  same fact stated twice, so a wrong lookup is visible in the pixel. */
std::vector<SkColor4f> paletteTable() {
  std::vector<SkColor4f> pal((size_t)256);
  for (int i = 0; i < 256; ++i)
    pal[(size_t)i] = {(float)i / 255.0f, 1.0f - (float)i / 255.0f, 0.0f, 1.0f};
  return pal;
}

/** The same table as the 256 x 1 unpremultiplied image a slot takes. */
sk_sp<SkImage> paletteImage(const std::vector<SkColor4f>& pal) {
  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(256, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  for (int i = 0; i < 256; ++i) {
    const SkColor4f c = pal[(size_t)i];
    auto b = [](float v) {
      return (uint32_t)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
    };
    *bm.getAddr32(i, 0) =
        b(c.fR) | (b(c.fG) << 8) | (b(c.fB) << 16) | (b(c.fA) << 24);
  }
  bm.setImmutable();
  return bm.asImage();
}

}  // namespace

TEST(SkiaPaint, APaletteReachesAnEffectAsOneChildImage) {
  const std::vector<SkColor4f> pal = paletteTable();
  // The lookup a fixed-palette picture needs is DYNAMIC — the index is a
  // pixel value, not a literal — and a sampled 256 x 1 strip is the form
  // that takes: nearest, at the texel centre, so entry 200 is entry 200
  // and not a blend of two unrelated ones.
  Paint lut = skia::sksl(
      effectFor("uniform shader uPalette;\n"
                "uniform float uIndex;\n"
                "half4 main(float2 p) {\n"
                "  return uPalette.eval(float2(uIndex + 0.5, 0.5));\n"
                "}"));
  lut.set("uIndex", 200.0f);
  lut.slot("uPalette", skia::image(paletteImage(pal), Repeat::Pad, Repeat::Pad,
                                   SkMatrix::I(),
                                   SkSamplingOptions(SkFilterMode::kNearest)));
  // One child, one uniform: the whole table is in the shader and nothing
  // was baked per entry.
  sk_sp<SkShader> shader = skia::staticShader(lut);
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  const SkColor got = bm.getColor(1, 1);
  EXPECT_EQ(SkColorGetR(got), 200u);
  EXPECT_EQ(SkColorGetG(got), 55u);
  EXPECT_EQ(SkColorGetB(got), 0u);
  EXPECT_EQ(SkColorGetA(got), 255u);
}

TEST(SkiaPaint, APaletteReachesAnEffectAsOneUniformArray) {
  const std::vector<SkColor4f> pal = paletteTable();
  std::vector<float> flat;
  flat.reserve(pal.size() * 4);
  for (const SkColor4f& c : pal) {
    flat.push_back(c.fR);
    flat.push_back(c.fG);
    flat.push_back(c.fB);
    flat.push_back(c.fA);
  }
  ASSERT_EQ(flat.size(), 1024u);

  // The array door: 1024 floats fill `float4 uPalette[256]`, because the
  // builder matches the DECLARED TOTAL float count and nothing finer.
  Paint lut = skia::sksl(
      effectFor("uniform float4 uPalette[256];\n"
                "half4 main(float2 p) { return half4(uPalette[200]); }"));
  lut.set("uPalette", flat);
  sk_sp<SkShader> shader = skia::staticShader(lut);
  ASSERT_NE(shader, nullptr);
  const SkBitmap bm = render(shader);
  const SkColor got = bm.getColor(1, 1);
  EXPECT_EQ(SkColorGetR(got), 200u);
  EXPECT_EQ(SkColorGetG(got), 55u);

  // A count that is not the declaration's is refused whole rather than
  // written partly: the paint keeps the table it had.
  Paint partial = lut;
  partial.set("uPalette", std::vector<float>(8, 1.0f));
  const SkBitmap same = render(skia::staticShader(partial));
  EXPECT_TRUE(identical(bm, same));
}

// ---- a fitted image -------------------------------------------------------

namespace {

/** Two pixels side by side, red then blue — a source whose halves are
 *  told apart wherever it lands. */
sk_sp<SkImage> twoHalves() {
  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  *bm.getAddr32(0, 0) = 0xFF0000FFu;  // ABGR: opaque red
  *bm.getAddr32(1, 0) = 0xFFFF0000u;  // opaque blue
  bm.setImmutable();
  return bm.asImage();
}

/** The image's POINTER is its identity in a recipe, so every case here
 *  fits the same one — two paints over two copies of the same pixels are
 *  deliberately not equal. */
sk_sp<SkImage> theHalves() {
  static const sk_sp<SkImage> image = twoHalves();
  return image;
}

Paint fitted(Fit how) {
  Paint p = skia::image(theHalves(), Repeat::None, Repeat::None, SkMatrix::I(),
                        SkSamplingOptions(SkFilterMode::kNearest));
  p.fit(how);
  return p;
}

}  // namespace

TEST(SkiaPaint, AFittedImageIsMappedOntoTheBoxItIsPainting) {
  // Native is the absence of the question: two source pixels at the
  // origin, and the rest of the box is not the image's business.
  const Paint native = fitted(Fit::Native);
  EXPECT_FALSE(native.geometryDependent());
  EXPECT_EQ(
      SkColorGetA(render(skia::staticShader(native), 40, 10).getColor(20, 5)),
      0u);

  // Stretch fills both axes: the halves land either side of the middle and
  // the whole box is covered.
  const Paint stretch = fitted(Fit::Stretch);
  EXPECT_TRUE(stretch.geometryDependent());
  const SkBitmap wide =
      render(skia::shader(stretch, FrameData{.resolution = {40, 10}}), 40, 10);
  EXPECT_EQ(wide.getColor(5, 5), SK_ColorRED);
  EXPECT_EQ(wide.getColor(35, 5), SK_ColorBLUE);
  EXPECT_EQ(wide.getColor(5, 9), SK_ColorRED);

  // Contain keeps the aspect and leaves the margin: a 2:1 source in a
  // square box is half the height of it, centred, and the box's own top
  // is not the image.
  const SkBitmap inside = render(
      skia::shader(fitted(Fit::Contain), FrameData{.resolution = {40, 40}}), 40,
      40);
  EXPECT_EQ(SkColorGetA(inside.getColor(5, 5)), 0u);
  EXPECT_EQ(inside.getColor(5, 20), SK_ColorRED);
  EXPECT_EQ(inside.getColor(35, 20), SK_ColorBLUE);

  // Cover keeps the aspect the other way: nothing of the box is left, and
  // what does not fit is off the edges.
  const SkBitmap over = render(
      skia::shader(fitted(Fit::Cover), FrameData{.resolution = {40, 40}}), 40,
      40);
  EXPECT_EQ(over.getColor(5, 2), SK_ColorRED);
  EXPECT_EQ(over.getColor(5, 38), SK_ColorRED);
  EXPECT_EQ(over.getColor(35, 20), SK_ColorBLUE);
}

TEST(SkiaPaint, AFitIsPartOfTheRecipeAndDegradesWhereThereIsNoBox) {
  EXPECT_FALSE(fitted(Fit::Cover) == fitted(Fit::Contain));
  EXPECT_FALSE(fitted(Fit::Cover) == fitted(Fit::Native));
  EXPECT_TRUE(fitted(Fit::Cover) == fitted(Fit::Cover));

  // Asked with no box in reach — a standalone decoration, a measurement —
  // it answers the unfitted mapping rather than nothing at all, the same
  // degradation a world-space material makes outside a composer.
  const SkBitmap loose = render(skia::shader(fitted(Fit::Cover)), 40, 40);
  EXPECT_EQ(SkColorGetA(loose.getColor(20, 20)), 0u);
  EXPECT_EQ(loose.getColor(0, 0), SK_ColorRED);
}

namespace {
enum class SlotFrameInput { Resolution, World, Time };
struct SlotFrameCase {
  const char* name;
  SlotFrameInput input;
  float firstRed;
  float secondRed;
};
class SkiaPaintSlotFrame : public testing::TestWithParam<SlotFrameCase> {};
}  // namespace

TEST_P(SkiaPaintSlotFrame, NativePaintChildReceivesItsParentsChangingFrame) {
  const SlotFrameCase& test = GetParam();
  sk_sp<SkRuntimeEffect> effect;
  switch (test.input) {
    case SlotFrameInput::Resolution:
      effect = resolutionEffect();
      break;
    case SlotFrameInput::World:
      effect = worldEffect();
      break;
    case SlotFrameInput::Time:
      effect = timeEffect();
      break;
  }
  ASSERT_TRUE(effect);
  struct NoParameters {};
  const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParameters>("paint.native-child")
          .slot("source")
          .body(Target::SkSL,
                "half4 main(float2 p) { return source.eval(p); }"));
  const Paint paint =
      Paint::recipe(Material(recipe)).slot("source", skia::sksl(effect));
  EXPECT_EQ(paint.isRunning(), test.input == SlotFrameInput::Time);
  EXPECT_EQ(paint.geometryDependent(), test.input != SlotFrameInput::Time);
  FrameData firstFrame{.seconds = .25, .resolution = {8, 8}};
  FrameData secondFrame{.seconds = .75, .resolution = {16, 16}};
  secondFrame.world[2][0] = 64;
  const auto initial = render(skia::shader(paint, firstFrame), 8, 8);
  const auto changed = render(skia::shader(paint, secondFrame), 8, 8);
  const auto restored = render(skia::shader(paint, firstFrame), 8, 8);
  EXPECT_NEAR(initial.getColor4f(4, 4).fR, test.firstRed, 1.f / 255);
  EXPECT_NEAR(changed.getColor4f(4, 4).fR, test.secondRed, 1.f / 255);
  EXPECT_TRUE(identical(initial, restored));
  EXPECT_FALSE(identical(initial, changed));
}

INSTANTIATE_TEST_SUITE_P(
    Inputs, SkiaPaintSlotFrame,
    testing::Values(SlotFrameCase{"Resolution", SlotFrameInput::Resolution,
                                  4.5f / 8, 4.5f / 16},
                    SlotFrameCase{"Placement", SlotFrameInput::World,
                                  4.5f / 128, 68.5f / 128},
                    SlotFrameCase{"Time", SlotFrameInput::Time, .25f, .75f}),
    [](const testing::TestParamInfo<SlotFrameCase>& test) {
      return test.param.name;
    });
