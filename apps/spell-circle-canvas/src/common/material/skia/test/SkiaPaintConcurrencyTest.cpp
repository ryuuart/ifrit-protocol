/** @file
 * One paint resolved from several threads: a shared memo, oversized
 * signed-distance pads and recipe parents and passes all answering what
 * a serial frame answers.
 */

#include "SkiaPaintTestSupport.h"

TEST(SkiaPaint, TwoThreadsResolveOneSharedPaintsMemo) {
  // Copies of a Paint share the state its resolve memo hangs off, and a
  // host may paint two composers on two threads: both reach the memo,
  // and the answer has to be a whole shader either way.
  Paint live = skia::sksl(resolutionEffect());
  EXPECT_TRUE(live.geometryDependent());
  std::vector<std::thread> painters;
  std::atomic<int> built{0};
  for (int t = 0; t < 4; ++t)
    painters.emplace_back([copy = live, t, &built] {
      for (int i = 0; i < 64; ++i) {
        const float side = (float)(8 + ((t + i) % 16));
        if (skia::shader(copy, FrameData{.resolution = {side, side}})) ++built;
      }
    });
  for (std::thread& painter : painters) painter.join();
  EXPECT_EQ(built.load(), 4 * 64);
}

namespace {
class SdfWarningPath : public testing::TestWithParam<bool> {};
}  // namespace

TEST_P(SdfWarningPath, ConcurrentOversizedPadsPreserveFramedPixels) {
  struct Parameters {
    float uPad = 16;
    float uGlowR = 8;
  };
  constexpr const char* body =
      "half4 main(float2 p) { return half4(uResolution.x / 32, "
      "uGlowR / 16, uPad / 32, 1); }";
  Paint paint;
  if (GetParam()) {
    const auto recipe = std::make_shared<const Recipe>(
        Recipe::of<Parameters>("sdf.concurrent-pad")
            .frame(FrameInput::Resolution)
            .body(Target::SkSL, body));
    paint = Paint::recipe(Material(recipe, Parameters{}));
  } else {
    const std::string source = std::string(
                                   "uniform float uPad; uniform float uGlowR; "
                                   "uniform float2 uResolution;") +
                               body;
    paint = skia::sksl(effectFor(source.c_str()))
                .set("uPad", 16.f)
                .set("uGlowR", 8.f);
  }
  ASSERT_TRUE(paint.geometryDependent());
  std::array<SkBitmap, 3> expected;
  for (size_t i = 0; i < expected.size(); ++i)
    expected[i] = render(
        SkShaders::Color(SkColor4f{float(i + 1) / 4, .5f, .5f, 1}, nullptr));
  constexpr size_t threads = 4, frames = 16;
  std::barrier start(static_cast<std::ptrdiff_t>(threads));
  std::atomic<size_t> failures{0};
  std::vector<std::thread> workers;
  for (size_t t = 0; t < threads; ++t)
    workers.emplace_back([&, copy = paint, t] {
      start.arrive_and_wait();
      for (size_t i = 0; i < frames; ++i) {
        const size_t sample = (t + i) % expected.size();
        const float side = 8.f * float(sample + 1);
        const auto built = skia::shader(copy, {.resolution = {side, side}});
        if (!built || !identical(render(built), expected[sample])) ++failures;
      }
    });
  for (auto& worker : workers) worker.join();
  EXPECT_EQ(failures.load(), 0u);
  const FrameData held{.resolution = {8, 8}};
  const auto built = skia::shader(paint, held);
  ASSERT_TRUE(built);
  EXPECT_EQ(skia::shader(paint, held), built);
  EXPECT_TRUE(identical(render(built), expected[0]));
}

INSTANTIATE_TEST_SUITE_P(SkiaPaint, SdfWarningPath, testing::Bool(),
                         [](const testing::TestParamInfo<bool>& parameter) {
                           return parameter.param ? "Recipe" : "Raw";
                         });

TEST(SkiaPaint, ConcurrentRecipeParentsAndPassesMatchSerialFrames) {
  struct Empty {};
  struct Gain {
    float uGain;
  };
  const auto childRecipe = std::make_shared<const Recipe>(
      Recipe::of<Empty>("paint.concurrent-child")
          .frame(FrameInput::Time)
          .frame(FrameInput::Resolution)
          .body(Target::SkSL,
                "half4 main(float2 p) { return half4("
                "uTime / 16, uResolution.x / 64, p.x / 16, 1); }"));
  const auto middleRecipe = std::make_shared<const Recipe>(
      Recipe::of<Empty>("paint.concurrent-middle")
          .slot("source")
          .body(Target::SkSL,
                "half4 main(float2 p) { return source.eval(p); }"));
  Material sharedChild(middleRecipe);
  sharedChild.slot("source", Material(childRecipe));
  constexpr size_t kThreads = 4, kFrames = 64;
  const auto frameAt = [](size_t thread, size_t index) {
    return FrameData{
        .seconds = static_cast<double>(1 + (thread * 5 + index) % 15),
        .resolution = {static_cast<float>(8 + (thread * 7 + index) % 32), 16}};
  };

  for (bool pass : {false, true}) {
    SCOPED_TRACE(pass ? "pass" : "ordinary paint");
    const std::string body =
        pass ? "half4 main(float2 p) { return source.eval(p) * half(uGain) * "
               "half(uUnitPhase[kUnitCount - 1].x) * uContent.eval(p); }"
             : "half4 main(float2 p) { return source.eval(p) * half(uGain); }";
    const auto parentRecipe = std::make_shared<const Recipe>(
        Recipe::of<Gain>(pass ? "paint.concurrent-pass"
                              : "paint.concurrent-parent")
            .slot("source")
            .body(Target::SkSL, body));
    std::array<Paint, 2> parents;
    for (size_t i = 0; i < parents.size(); ++i) {
      Material parent(parentRecipe, Gain{i == 0 ? 1.f : .75f});
      // Each parent is lowered separately. Their middle values still
      // share the same child slot, beyond either Paint's own memo lock.
      parent.slot("source", sharedChild);
      parents[i] = Paint::recipe(parent);
    }
    const auto resolve = [&](const Paint& paint, size_t thread, size_t index) {
      const FrameData frame = frameAt(thread, index);
      if (!pass) return skia::shader(paint, frame);
      const std::array<float, 12> rects{0, 0, 4, 4, 0, 0, 4, 4, 0, 0, 4, 4};
      const float phase = (index % 2) == 0 ? 1.f : .5f;
      const std::array<float, 6> phases{phase, 0, phase, 0, phase, 0};
      skia::PassInputs input;
      input.content = SkShaders::Color(SK_ColorWHITE);
      input.rects = rects.data();
      input.phases = phases.data();
      input.units = 1 + static_cast<uint32_t>(thread % 3);
      return skia::resolvePass(paint, input, frame);
    };
    std::array<std::array<SkBitmap, kFrames>, kThreads> expected;
    for (size_t t = 0; t < kThreads; ++t)
      for (size_t i = 0; i < kFrames; ++i) {
        const sk_sp<SkShader> shader = resolve(parents[t % 2], t, i);
        ASSERT_TRUE(shader);
        expected[t][i] = render(shader);
        ASSERT_GT(SkColorGetA(expected[t][i].getColor(1, 1)), 0u);
      }
    ASSERT_FALSE(identical(expected[0][0], expected[0][1]));
    std::barrier start(static_cast<std::ptrdiff_t>(kThreads));
    std::atomic<size_t> failures{0};
    std::vector<std::thread> painters;
    for (size_t t = 0; t < kThreads; ++t)
      painters.emplace_back([&, copy = parents[t % 2], t] {
        start.arrive_and_wait();
        for (size_t i = 0; i < kFrames; ++i) {
          const sk_sp<SkShader> shader = resolve(copy, t, i);
          if (!shader || !identical(render(shader), expected[t][i])) ++failures;
        }
      });
    for (std::thread& painter : painters) painter.join();
    EXPECT_EQ(failures.load(), 0u);
  }
}
