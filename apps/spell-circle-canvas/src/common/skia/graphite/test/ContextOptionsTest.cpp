/** @file
 * The options every context and every recorder this library builds is
 * given: the two recorder settings whose violation fails silently, the
 * glyph-atlas budget the environment may cap, and where a shader that
 * would not compile is reported. All of it is arithmetic over an options
 * struct — no device, no context, no bring-up.
 */

#include <gpu/ShaderErrorHandler.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Recorder.h>
#include <gtest/gtest.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstddef>
#include <cstdlib>

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

}  // namespace
