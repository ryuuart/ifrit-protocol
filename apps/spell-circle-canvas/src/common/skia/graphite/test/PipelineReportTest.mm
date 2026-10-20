/** @file
 * The pipelines a Graphite context builds and finds, as its reporter hears
 * them: every build and every find reported, a recorded key set that
 * removes the first build once precompiled, and a key naming the
 * backend's own effects built by a fresh context.
 */

#import <Metal/Metal.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "GraphiteReadback.h"
#include "MetalFixtures.h"

using sigil::skia::GraphiteContext;
using namespace sigil::skia::test;

namespace {

/** EVERY PIPELINE A CONTEXT BUILT OR FOUND while a case was watching.
 *
 *  Reported on whichever thread recorded the draw, and on the pool the
 *  context compiles its pipelines on, so every count is taken under the
 *  lock. */
class PipelineLog final : public GraphiteContext::PipelineReporter {
 public:
  void added(const std::string &, std::uint32_t, bool fromPrecompile, sk_sp<SkData> key) override {
    const std::lock_guard<std::mutex> lock(m_mutex);
    ++m_added;
    if (fromPrecompile) ++m_addedAhead;
    if (key) m_keys.push_back(std::move(key));
  }
  void found(const std::string &, std::uint32_t, bool fromPrecompile) override {
    const std::lock_guard<std::mutex> lock(m_mutex);
    ++m_found;
    if (fromPrecompile) ++m_foundAhead;
  }

  int addedCount() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_added;
  }
  int addedAheadCount() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_addedAhead;
  }
  int foundCount() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_found;
  }
  int foundAheadCount() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_foundAhead;
  }
  std::vector<sk_sp<SkData>> keys() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_keys;
  }
  void clear() {
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_added = m_addedAhead = m_found = m_foundAhead = 0;
    m_keys.clear();
  }

 private:
  mutable std::mutex m_mutex;
  int m_added = 0;
  int m_addedAhead = 0;
  int m_found = 0;
  int m_foundAhead = 0;
  std::vector<sk_sp<SkData>> m_keys;
};

/** A RUNTIME EFFECT, which is what a sketch's effect stages are made of
 *  and what makes each stage its own device program. */
sk_sp<SkRuntimeEffect> tintingEffect() {
  static SkRuntimeEffect::Result compiled =
      SkRuntimeEffect::MakeForShader(SkString("half4 main(float2 position) {"
                                              "  return half4(half(position.x / 32), 0.25, 0.5, 1);"
                                              "}"));
  return compiled.effect;
}

sk_sp<SkShader> tintingShader() {
  sk_sp<SkRuntimeEffect> effect = tintingEffect();
  return effect ? effect->makeShader(nullptr, {}) : nullptr;
}

/** The same scene twice, so the second time asks for the pipelines the
 *  first time built. */
bool drawTheTintedScene(GraphiteContext &ctx) {
  sk_sp<SkShader> shader = tintingShader();
  if (!shader) return false;
  const SkImageInfo info = SkImageInfo::MakeN32Premul(32, 32);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx.recorder(), info);
  if (!surface) return false;
  SkPaint paint;
  paint.setShader(std::move(shader));
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawRect(SkRect::MakeWH(32, 32), paint);
  return submitRecorder(ctx, *ctx.recorder());
}

/** A scene drawn through a blur, which Graphite lowers to its own runtime
 *  effects — made by the backend the first time a draw keys one. */
bool drawTheBlurredScene(GraphiteContext &ctx) {
  const SkImageInfo info = SkImageInfo::MakeN32Premul(64, 64);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(ctx.recorder(), info);
  if (!surface) return false;
  SkPaint paint;
  paint.setColor(SK_ColorWHITE);
  paint.setImageFilter(SkImageFilters::Blur(3, 3, nullptr));
  surface->getCanvas()->clear(SK_ColorBLACK);
  surface->getCanvas()->drawRect(SkRect::MakeXYWH(16, 16, 32, 32), paint);
  return submitRecorder(ctx, *ctx.recorder());
}

/** A context of this case's own: the shared one is built the first
 *  time anything asks for it, which is before a reporter could
 *  have been installed — and a reporter only reaches contexts built
 *  after it. */
std::unique_ptr<GraphiteContext> watchedContext() {
  return GraphiteContext::createMetal((__bridge void *)device(), (__bridge void *)queue());
}

}  // namespace

TEST(SigilSkiaGraphite, TheReporterIsToldOfEveryPipelineBuiltAndEveryOneFound) {
  SKIP_WITHOUT_METAL();
  // WHAT THIS STANDS FOR is the reporter's wiring, not a stall removed:
  // that a second draw of one scene reuses the programs of the first is
  // Graphite's own cache and was true before anything here was written.
  // What is new is that both halves are visible — a warm-up can only act
  // on which pipelines a scene asked for and which of them it found.
  PipelineLog log;
  GraphiteContext::reportPipelinesTo(&log);
  std::unique_ptr<GraphiteContext> ctx = watchedContext();
  ASSERT_NE(ctx, nullptr);

  ASSERT_TRUE(drawTheTintedScene(*ctx));
  EXPECT_GT(log.addedCount(), 0) << "no build was reported at all";
  EXPECT_EQ(log.addedAheadCount(), 0)
      << "nothing was precompiled, so nothing was built ahead of a draw";

  log.clear();
  ASSERT_TRUE(drawTheTintedScene(*ctx));
  EXPECT_EQ(log.addedCount(), 0);
  EXPECT_GT(log.foundCount(), 0) << "no find was reported at all";

  ctx.reset();
  GraphiteContext::reportPipelinesTo(nullptr);
}

TEST(SigilSkiaGraphite, PrecompilingARecordedKeySetRemovesTheFirstBuild) {
  SKIP_WITHOUT_METAL();
  PipelineLog log;
  GraphiteContext::reportPipelinesTo(&log);
  // DECLARED BEFORE THE CONTEXT, because a key naming an undeclared
  // runtime effect is not serialisable at all — which would leave a
  // recorded set holding everything except the stages a chain of
  // effects is made of.
  const std::array<sk_sp<SkRuntimeEffect>, 1> declared{tintingEffect()};
  ASSERT_NE(declared[0], nullptr);
  GraphiteContext::registerRuntimeEffects(declared);

  std::vector<sk_sp<SkData>> keys;
  {
    std::unique_ptr<GraphiteContext> first = watchedContext();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(drawTheTintedScene(*first));
    keys = log.keys();
  }
  if (keys.empty()) {
    GraphiteContext::reportPipelinesTo(nullptr);
    GraphiteContext::registerRuntimeEffects({});
    GTEST_SKIP() << "this backend serialises no pipeline key";
  }

  // A LAUNCH THAT KNOWS WHAT THE LAST ONE NEEDED. The keys are rebuilt
  // before anything draws, so the draw that follows finds its programs
  // rather than waiting for them.
  log.clear();
  std::unique_ptr<GraphiteContext> second = watchedContext();
  ASSERT_NE(second, nullptr);
  EXPECT_GT(second->precompile(keys), size_t{0});
  EXPECT_GT(log.addedAheadCount(), 0);

  const int builtAhead = log.addedCount();
  ASSERT_TRUE(drawTheTintedScene(*second));
  EXPECT_EQ(log.addedCount(), builtAhead) << "the draw built a pipeline anyway";
  EXPECT_GT(log.foundAheadCount(), 0);

  second.reset();
  GraphiteContext::reportPipelinesTo(nullptr);
  GraphiteContext::registerRuntimeEffects({});
}

TEST(SigilSkiaGraphite, AKeyNamingTheBackendsOwnEffectsIsBuiltByAFreshContext) {
  // A key recorded by a run whose draws made the backend's own blur
  // effects, replayed by a context where no draw has made them yet —
  // every second launch of a sketch that blurs. The replay builds the
  // program rather than taking the process down.
  SKIP_WITHOUT_METAL();
  PipelineLog log;
  GraphiteContext::reportPipelinesTo(&log);
  std::vector<sk_sp<SkData>> keys;
  {
    std::unique_ptr<GraphiteContext> first = watchedContext();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(drawTheBlurredScene(*first));
    keys = log.keys();
  }
  if (keys.empty()) {
    GraphiteContext::reportPipelinesTo(nullptr);
    GTEST_SKIP() << "this backend serialises no pipeline key";
  }
  log.clear();
  std::unique_ptr<GraphiteContext> second = watchedContext();
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->precompile(keys), keys.size());
  const int builtAhead = log.addedCount();
  ASSERT_TRUE(drawTheBlurredScene(*second));
  EXPECT_EQ(log.addedCount(), builtAhead) << "the draw built a pipeline anyway";
  second.reset();
  GraphiteContext::reportPipelinesTo(nullptr);
}
