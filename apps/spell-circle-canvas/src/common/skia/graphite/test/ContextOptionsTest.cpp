/** @file
 * The options every context and every recorder this library builds is
 * given: the two recorder settings whose violation fails silently, the
 * glyph-atlas budget the environment may cap, where a shader that would
 * not compile is reported, and what every context builds its device
 * pipelines on and reports them to. All of it is arithmetic over an
 * options struct — no device, no context, no bring-up.
 */

#include <gpu/ShaderErrorHandler.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gtest/gtest.h>
#include <include/core/SkData.h>
#include <include/core/SkExecutor.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

using sigil::skia::GraphiteContext;

namespace {

constexpr const char* kBudget = "SIGILSKIA_GLYPH_ATLAS_BYTES";

/** Skia's own budget, which is what an absent or unusable variable must
 *  leave in place. */
size_t skiaBudget() {
  return skgpu::graphite::ContextOptions{}.fGlyphCacheTextureMaximumBytes;
}

size_t budgetWith(const char* value) {
  if (value)
    ::setenv(kBudget, value, 1);
  else
    ::unsetenv(kBudget);
  const size_t bytes =
      GraphiteContext::makeContextOptions().fGlyphCacheTextureMaximumBytes;
  ::unsetenv(kBudget);
  return bytes;
}

/** A handler that only has to be addressable: what is asserted is which
 *  handler the options carry, not what it is told. */
class SilentShaderErrors final : public skgpu::ShaderErrorHandler {
 public:
  void compileError(const char*, const char*) override {}
};

/** A reporter that only has to be addressable, for the same reason. */
class SilentPipelines final : public GraphiteContext::PipelineReporter {
 public:
  void added(const std::string&, std::uint32_t, bool, sk_sp<SkData>) override {}
  void found(const std::string&, std::uint32_t, bool) override {}
};

TEST(SkiaGraphiteOptions, EveryRecorderIsGivenAProviderAndOrderedReplay) {
  // Both are preconditions rather than preferences: without the provider
  // a draw sampling a raster image is dropped and nothing says so, and
  // without ordered replay every snap drops the atlases.
  const skgpu::graphite::RecorderOptions options =
      GraphiteContext::makeRecorderOptions();
  EXPECT_NE(options.fImageProvider, nullptr);
  EXPECT_TRUE(options.fRequireOrderedRecordings);
}

TEST(SkiaGraphiteOptions, TheGlyphAtlasBudgetComesFromTheEnvironment) {
  EXPECT_EQ(budgetWith("1048576"), size_t{1048576});
  // An unset, unreadable or non-positive value leaves Skia's own budget,
  // so what the library does by default never depends on the environment.
  EXPECT_EQ(budgetWith(nullptr), skiaBudget());
  EXPECT_EQ(budgetWith("not a number"), skiaBudget());
  EXPECT_EQ(budgetWith("0"), skiaBudget());
  EXPECT_EQ(budgetWith("-4096"), skiaBudget());
}

TEST(SkiaGraphiteOptions, TheShaderErrorHandlerIsGivenToContextsBuiltAfterIt) {
  SilentShaderErrors handler;
  EXPECT_EQ(GraphiteContext::makeContextOptions().fShaderErrorHandler, nullptr);

  GraphiteContext::reportShaderErrorsTo(&handler);
  EXPECT_EQ(GraphiteContext::makeContextOptions().fShaderErrorHandler,
            &handler);

  // Null restores Skia's own reporting, which is what a caller whose
  // handler is going away has to be able to do.
  GraphiteContext::reportShaderErrorsTo(nullptr);
  EXPECT_EQ(GraphiteContext::makeContextOptions().fShaderErrorHandler, nullptr);
}

TEST(SkiaGraphiteOptions, ThePipelineReporterIsGivenToContextsBuiltAfterIt) {
  SilentPipelines reporter;
  // Serialising a pipeline's key is work, so a context nobody is
  // watching carries no callback at all.
  EXPECT_EQ(GraphiteContext::makeContextOptions().fPipelineCachingCallback,
            nullptr);

  GraphiteContext::reportPipelinesTo(&reporter);
  EXPECT_NE(GraphiteContext::makeContextOptions().fPipelineCachingCallback,
            nullptr);

  GraphiteContext::reportPipelinesTo(nullptr);
  EXPECT_EQ(GraphiteContext::makeContextOptions().fPipelineCachingCallback,
            nullptr);
}

TEST(SkiaGraphiteOptions, DeclaredRuntimeEffectsTravelWithEveryLaterContext) {
  EXPECT_TRUE(GraphiteContext::makeContextOptions()
                  .fUserDefinedKnownRuntimeEffects.empty());

  SkRuntimeEffect::Result compiled = SkRuntimeEffect::MakeForShader(
      SkString("half4 main(float2 position) { return half4(1); }"));
  ASSERT_NE(compiled.effect, nullptr);
  const std::array<sk_sp<SkRuntimeEffect>, 1> declared{compiled.effect};
  GraphiteContext::registerRuntimeEffects(declared);
  const auto known =
      GraphiteContext::makeContextOptions().fUserDefinedKnownRuntimeEffects;
  ASSERT_EQ(known.size(), size_t{1});
  EXPECT_EQ(known[0], compiled.effect);

  // The list replaces rather than grows, so a caller withdrawing its
  // effects leaves nothing declared behind it.
  GraphiteContext::registerRuntimeEffects({});
  EXPECT_TRUE(GraphiteContext::makeContextOptions()
                  .fUserDefinedKnownRuntimeEffects.empty());
}

TEST(SkiaGraphiteOptions, EveryContextIsGivenAnExecutorForPipelineCompilation) {
  // Without one, Graphite builds each device pipeline serially on the
  // thread that recorded the draw — which for a scene wearing a chain
  // of runtime shaders is every one of them inside its first frame.
  SkExecutor* executor = GraphiteContext::makeContextOptions().fExecutor;
  EXPECT_NE(executor, nullptr);
  // One pool per process, not one per context: several contexts are
  // several windows over one machine.
  EXPECT_EQ(GraphiteContext::makeContextOptions().fExecutor, executor);
}

}  // namespace
