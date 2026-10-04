/** @file
 * The blend stack as one paint: the tier its layers give it, the first
 * layer as the accumulation, and framed blends holding their frame while
 * live colours and placement still reach them.
 */

#include "SkiaPaintTestSupport.h"

TEST(SkiaPaint, ChildAndBlendInheritTheirLayersTier) {
  Paint parent =
      skia::sksl(effectFor("uniform shader uSrc;\n"
                           "half4 main(float2 p) { return uSrc.eval(p); }"));
  EXPECT_FALSE(parent.isRunning());
  parent.slot("uSrc", skia::sksl(timeEffect()));
  EXPECT_TRUE(parent.isRunning());

  const Paint stack =
      Paint::blend({{Paint::solid({0, 0, 0, 1}), BlendMode::Source},
                    {skia::sksl(resolutionEffect()), BlendMode::PlusLighter}});
  EXPECT_FALSE(stack.isRunning());
  EXPECT_TRUE(stack.geometryDependent());
}

TEST(SkiaPaint, TheFirstBlendLayerIsTheAccumulationAndItsLayerPropsAreNot) {
  // The first layer has nothing beneath it, so neither of its layer
  // properties is read: not its blend mode, which has no destination,
  // and not its amount, which has nothing to mix back toward. Both folds
  // — the eager flatten a static blend takes and the per-draw one a
  // geometry-dependent layer defers to — are one body, so they cannot
  // disagree about that.
  Paint base = Paint::solid({1, 0, 0, 1});
  base.amount(0.25f);
  const Paint top = Paint::solid({0, 0, 1, 1});
  const SkBitmap thinned = render(skia::staticShader(Paint::blend(
      {{base, BlendMode::Normal}, {top, BlendMode::PlusLighter}})));
  const SkBitmap whole = render(skia::staticShader(
      Paint::blend({{Paint::solid({1, 0, 0, 1}), BlendMode::Normal},
                    {top, BlendMode::PlusLighter}})));
  EXPECT_TRUE(identical(thinned, whole));

  // The SECOND layer's amount is read, and is the whole difference
  // between the two pictures.
  Paint half = top;
  half.amount(0.5f);
  const SkBitmap mixed = render(skia::staticShader(
      Paint::blend({{Paint::solid({1, 0, 0, 1}), BlendMode::Normal},
                    {half, BlendMode::PlusLighter}})));
  EXPECT_FALSE(identical(mixed, whole));
  EXPECT_NEAR(SkColorGetB(mixed.getColor(1, 1)),
              SkColorGetB(whole.getColor(1, 1)) / 2, 2);
}

namespace {

struct HeldBlendCase {
  const char* name;
  BlendMode mode;
  float amount;
};

class HeldBlend : public testing::TestWithParam<HeldBlendCase> {};

Paint boundBlendColor(sigil::motion::Animatable<Color> color) {
  static const auto effect = effectFor(
      "uniform float4 uColor; uniform float uGain;"
      " half4 main(float2 p) { return half4(uColor.rgb * uGain, uColor.a); }");
  Paint paint = skia::sksl(effect, {{"uGain", 1}});
  return paint.bind("uColor", std::move(color));
}

void expectBlendColor(const sk_sp<SkShader>& shader, Color expected) {
  ASSERT_TRUE(shader);
  const SkColor pixel = render(shader).getColor(1, 1);
  EXPECT_NEAR(SkColorGetR(pixel), expected.r * 255, 1);
  EXPECT_NEAR(SkColorGetG(pixel), expected.g * 255, 1);
  EXPECT_NEAR(SkColorGetB(pixel), expected.b * 255, 1);
  EXPECT_NEAR(SkColorGetA(pixel), expected.a * 255, 1);
}

}  // namespace

TEST_P(HeldBlend, ReusesItsHeldFrameAndObservesLiveColorChanges) {
  const auto& parameters = GetParam();
  const Color base{.25f, .5f, .75f, 1};
  auto color = sigil::motion::animatable(Color{.75f, .25f, .5f, 1});
  Paint top = boundBlendColor(color);
  top.amount(parameters.amount);
  const Paint paint = Paint::blend(
      {{Paint::solid(base), BlendMode::Normal}, {top, parameters.mode}});
  const Paint same = Paint::blend(
      {{Paint::solid(base), BlendMode::Normal}, {top, parameters.mode}});
  EXPECT_EQ(paint, same);
  const FrameData frame{.resolution = {8, 8}};
  const auto expected = [&](Color value) {
    if (parameters.mode == BlendMode::Multiply) {
      value.r *= base.r;
      value.g *= base.g;
      value.b *= base.b;
    }
    const float amount = parameters.amount;
    return Color{base.r * (1 - amount) + value.r * amount,
                 base.g * (1 - amount) + value.g * amount,
                 base.b * (1 - amount) + value.b * amount, 1};
  };
  const auto first = skia::shader(paint, frame);
  ASSERT_TRUE(first);
  expectBlendColor(first, expected(color.value()));
  EXPECT_EQ(first, skia::shader(paint, frame));
  const auto snapshot = skia::staticShader(paint);

  color = Color{0, .75f, .25f, 1};
  const auto changed = skia::shader(paint, frame);
  ASSERT_TRUE(changed);
  EXPECT_NE(changed, first);
  expectBlendColor(changed, expected(color.value()));
  EXPECT_EQ(changed, skia::shader(paint, frame));
  expectBlendColor(skia::shader(paint), expected(color.value()));
  EXPECT_EQ(changed, skia::shader(paint, frame));
  EXPECT_EQ(snapshot, skia::staticShader(paint));
}

INSTANTIATE_TEST_SUITE_P(
    SkiaPaint, HeldBlend,
    testing::Values(HeldBlendCase{"NormalHalf", BlendMode::Normal, .5f},
                    HeldBlendCase{"NormalWhole", BlendMode::Normal, 1},
                    HeldBlendCase{"MultiplyHalf", BlendMode::Multiply, .5f},
                    HeldBlendCase{"MultiplyWhole", BlendMode::Multiply, 1}),
    [](const testing::TestParamInfo<HeldBlendCase>& parameter) {
      return parameter.param.name;
    });

TEST(SkiaPaint, NestedFramedBlendsHoldAndRetainIndependentChildCopies) {
  auto color = sigil::motion::animatable(Color{1, 0, 0, 1});
  Paint original = boundBlendColor(color);
  const auto describe = [](Paint top) {
    top.amount(.5f);
    Paint inner = Paint::blend({{Paint::solid({0, 0, 1, 1}), BlendMode::Normal},
                                {std::move(top), BlendMode::Normal}});
    inner.amount(.5f);
    return Paint::blend({{Paint::solid({0, 1, 0, 1}), BlendMode::Normal},
                         {std::move(inner), BlendMode::Normal}});
  };
  const Paint paint = describe(original);
  const FrameData frame{.resolution = {8, 8}};
  const auto held = skia::shader(paint, frame);
  ASSERT_TRUE(held);
  expectBlendColor(held, {.25f, .5f, .25f, 1});
  EXPECT_EQ(held, skia::shader(paint, frame));
  Paint changedChild = original;
  changedChild.set("uGain", .5f);
  const Paint changedPaint = describe(changedChild);
  EXPECT_NE(paint, changedPaint);
  expectBlendColor(skia::shader(changedPaint, frame), {.125f, .5f, .25f, 1});
  EXPECT_EQ(held, skia::shader(paint, frame));
  expectBlendColor(held, {.25f, .5f, .25f, 1});

  color = Color{0, 1, 0, 1};
  const auto moved = skia::shader(paint, frame);
  EXPECT_NE(moved, held);
  expectBlendColor(moved, {0, .75f, .25f, 1});
  EXPECT_EQ(moved, skia::shader(paint, frame));
  expectBlendColor(skia::shader(changedPaint, frame), {0, .625f, .25f, 1});
}

TEST(SkiaPaint, FramedBlendMemoSeparatesOuterWorldSpaceAndPlacement) {
  Paint top = skia::sksl(resolutionEffect());
  top.amount(.5f);
  const Paint local =
      Paint::blend({{Paint::solid({0, 0, 0, 1}), BlendMode::Normal},
                    {top, BlendMode::Normal}});
  Paint anchored = local;
  anchored.worldSpace();
  EXPECT_NE(local, anchored);
  FrameData frame{.resolution = {8, 8},
                  .world = glm::mat3{1, 0, 0, 0, 1, 0, 2, 0, 1}};
  const auto localShader = skia::shader(local, frame);
  expectBlendColor(localShader, {1.5f / 16, 0, 0, 1});
  EXPECT_EQ(localShader, skia::shader(local, frame));
  const auto placed = skia::shader(anchored, frame);
  expectBlendColor(placed, {3.5f / 16, 0, 0, 1});
  EXPECT_EQ(placed, skia::shader(anchored, frame));
  frame.world[2].x = 4;
  const auto moved = skia::shader(anchored, frame);
  EXPECT_NE(moved, placed);
  expectBlendColor(moved, {5.5f / 16, 0, 0, 1});
  EXPECT_EQ(moved, skia::shader(anchored, frame));
  expectBlendColor(skia::shader(local, frame), {1.5f / 16, 0, 0, 1});
}
