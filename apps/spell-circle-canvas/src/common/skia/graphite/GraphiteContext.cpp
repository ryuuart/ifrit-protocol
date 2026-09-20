// Graphics-API-independent half of GraphiteContext: the options every
// backend factory funnels through, and the pair the factories wrap. The
// factories themselves are per API — GraphiteContextMetal.mm and
// GraphiteContextVulkan.cpp — and each stands Graphite up on that API's
// handles alone.
//
// `src/core/SkKnownRuntimeEffects.h` is Skia's own private header. It
// ships in this install beside the static archive and declares the size
// of the block of stable names a client's runtime effects are numbered
// out of. THAT NUMBER IS ONLY THE BACKEND'S TO STATE: a copy here would
// go on answering the old size after an upgrade moved it, with no build
// error and no failing case, and a declaration cut at the wrong place
// leaves stored pipeline keys describing programs nobody built.

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Image.h>
#include <gpu/graphite/ImageProvider.h>
#include <gpu/graphite/PrecompileContext.h>
#include <gpu/graphite/Recorder.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>
#include <include/core/SkExecutor.h>
#include <include/core/SkSpan.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <src/core/SkKnownRuntimeEffects.h>

#include <algorithm>
#include <atomic>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <cstdlib>
#include <utility>
#include <vector>

namespace sigil::skia {

namespace {

/** Graphite performs no implicit uploads: any draw sampling a non-Graphite
 *  (raster) SkImage asks the Recorder's client ImageProvider for a texture
 *  version, and DROPS the draw when there is none. This provider promotes
 *  on first use and caches by (image uniqueID, mipmapped) so a raster
 *  atlas or nine-slice uploads once rather than per draw.
 *
 *  Entries pin their textures until the cache reaches kMaxEntries, at
 *  which point it is cleared wholesale — no LRU. That suits hosts holding
 *  a handful of long-lived generated atlases; a host churning thousands of
 *  distinct images would want a real eviction policy instead. */
class CachingImageProvider final : public skgpu::graphite::ImageProvider {
 public:
  sk_sp<SkImage> findOrCreate(skgpu::graphite::Recorder* recorder,
                              const SkImage* image,
                              SkImage::RequiredProperties required) override {
    const uint64_t key = (static_cast<uint64_t>(image->uniqueID()) << 1u) |
                         (required.fMipmapped ? 1u : 0u);
    if (auto it = m_cache.find(key); it != m_cache.end()) return it->second;
    sk_sp<SkImage> texture =
        SkImages::TextureFromImage(recorder, image, required);
    if (!texture && image->colorType() == kRGBA_F32_SkColorType) {
      // F32 sources (an EXR import lands as an F32 SkImage) are not
      // filterable on Apple GPUs, so promotion fails outright. Retry with
      // an F16 copy: float range survives, the source image's own F32
      // pixels are untouched, and the draw actually happens.
      SkBitmap f16;
      if (f16.tryAllocPixels(
              image->imageInfo().makeColorType(kRGBA_F16_SkColorType)) &&
          image->readPixels(nullptr, f16.pixmap(), 0, 0)) {
        f16.setImmutable();
        texture =
            SkImages::TextureFromImage(recorder, f16.asImage().get(), required);
      }
    }
    if (!texture) return nullptr;
    if (m_cache.size() >= kMaxEntries)
      m_cache.clear();  // stale uniqueIDs accumulate as images are replaced
    m_cache.emplace(key, texture);
    return texture;
  }

 private:
  static constexpr size_t kMaxEntries = 256;
  boost::unordered_flat_map<uint64_t, sk_sp<SkImage>> m_cache;
};

/** Where a failed shader compile is reported, for every context built
 *  after it is set. Atomic because the contexts are built on whichever
 *  thread owns the device, and the handler itself is reached from the
 *  pipeline pool, which is where the compile that failed ran. */
std::atomic<skgpu::ShaderErrorHandler*>& shaderErrorSink() {
  static std::atomic<skgpu::ShaderErrorHandler*> sink{nullptr};
  return sink;
}

/** Where a pipeline Graphite builds or finds is reported, for every
 *  context built after it is set. Atomic for the same reason as the
 *  handler above. An add is reported from the pool thread that built
 *  the pipeline and a find from the thread that recorded the draw, so
 *  the reporter behind it is reached concurrently. */
std::atomic<GraphiteContext::PipelineReporter*>& pipelineSink() {
  static std::atomic<GraphiteContext::PipelineReporter*> sink{nullptr};
  return sink;
}

/** The shape Graphite calls back with. Skia's callback carries a client
 *  context pointer; the sink here is the static above, so the context
 *  stays null and one trampoline serves every context. */
void reportPipeline(skgpu::graphite::ContextOptions::PipelineCallbackContext,
                    skgpu::graphite::ContextOptions::PipelineCacheOp operation,
                    const std::string& label, std::uint32_t uniqueKeyHash,
                    bool fromPrecompile, sk_sp<SkData> pipelineData) {
  GraphiteContext::PipelineReporter* reporter =
      pipelineSink().load(std::memory_order_relaxed);
  if (!reporter) return;
  using Operation = skgpu::graphite::ContextOptions::PipelineCacheOp;
  if (operation == Operation::kAddingPipeline)
    reporter->added(label, uniqueKeyHash, fromPrecompile,
                    std::move(pipelineData));
  else
    reporter->found(label, uniqueKeyHash, fromPrecompile);
}

/** THE THREAD POOL EVERY CONTEXT COMPILES ITS PIPELINES ON.
 *
 *  Skia requires the executor to stay valid for the whole life of every
 *  context given it, and the last context in a process goes during
 *  static teardown — so this is made once, on first ask, and never
 *  released. One pool, not one per context: several contexts in a
 *  process are several windows over one machine, and a pool each would
 *  be that many thread sets competing for the same cores. */
SkExecutor& pipelineExecutor() {
  static SkExecutor* executor = SkExecutor::MakeFIFOThreadPool().release();
  return *executor;
}

/** The runtime effects every context built after them is told about, in
 *  the order they were declared — which is part of the name each one
 *  gets, so the order is the list. Never destroyed: a context refs each
 *  effect, but the span the options carry points here, and the last
 *  context goes during static teardown. */
std::vector<sk_sp<SkRuntimeEffect>>& knownRuntimeEffects() {
  static auto* effects = new std::vector<sk_sp<SkRuntimeEffect>>;
  return *effects;
}

}  // namespace

void GraphiteContext::reportShaderErrorsTo(skgpu::ShaderErrorHandler* handler) {
  shaderErrorSink().store(handler, std::memory_order_relaxed);
}

void GraphiteContext::reportPipelinesTo(PipelineReporter* reporter) {
  pipelineSink().store(reporter, std::memory_order_relaxed);
}

size_t GraphiteContext::runtimeEffectLimit() {
  // Skia numbers a client's declared effects out of one reserved block
  // and gives the rest of them unstable names, so a list longer than
  // the block cannot be declared whole however it is offered.
  constexpr int reservedNames =
      SkKnownRuntimeEffects::kUserDefinedKnownRuntimeEffectsReservedCnt;
  return (size_t)reservedNames;
}

size_t GraphiteContext::registerRuntimeEffects(
    std::span<const sk_sp<SkRuntimeEffect>> effects) {
  const size_t taken = std::min(effects.size(), runtimeEffectLimit());
  knownRuntimeEffects().assign(effects.begin(), effects.begin() + taken);
  return taken;
}

std::unique_ptr<skgpu::graphite::PrecompileContext>
GraphiteContext::makePrecompileContext() const {
  if (!m_context) return nullptr;
  const auto lock = lockContext();
  return m_context->makePrecompileContext();
}

size_t GraphiteContext::precompile(std::span<const sk_sp<SkData>> keys) const {
  if (keys.empty()) return 0;
  const std::unique_ptr<skgpu::graphite::PrecompileContext> precompileContext =
      makePrecompileContext();
  if (!precompileContext) return 0;
  size_t built = 0;
  for (const sk_sp<SkData>& key : keys)
    if (key && precompileContext->precompile(key)) ++built;
  return built;
}

skgpu::graphite::ContextOptions GraphiteContext::makeContextOptions() {
  skgpu::graphite::ContextOptions options;
  // Null leaves Skia's own handler in place, which prints to stderr.
  options.fShaderErrorHandler =
      shaderErrorSink().load(std::memory_order_relaxed);
  // PIPELINES OFF THE THREAD THAT RECORDED THE DRAW. Graphite builds a
  // device program per distinct draw and the recording thread waits on
  // it; with no executor it builds them one after another on that
  // thread, so a scene wearing a chain of runtime shaders pays every
  // one of them inside its first frame.
  options.fExecutor = &pipelineExecutor();
  // Only where someone is listening: an add serialises the pipeline's
  // key for the callback, which is work a context nobody is watching
  // has no reason to do.
  if (pipelineSink().load(std::memory_order_relaxed))
    options.fPipelineCachingCallback = &reportPipeline;
  // A key naming a runtime effect is only serialisable while the effect
  // is one of these, so the declared list travels with the context.
  if (!knownRuntimeEffects().empty())
    options.fUserDefinedKnownRuntimeEffects =
        SkSpan<sk_sp<SkRuntimeEffect>>(knownRuntimeEffects());
  // The glyph-atlas texture budget, overridable from the environment so
  // it can be varied under a benchmark without a rebuild. Unparseable or
  // non-positive values are ignored, so an unset or malformed variable
  // leaves Skia's own default in place and default behaviour never
  // depends on the environment.
  if (const char* bytes = std::getenv("SIGILSKIA_GLYPH_ATLAS_BYTES"))
    if (const long parsed = std::strtol(bytes, nullptr, 10); parsed > 0)
      options.fGlyphCacheTextureMaximumBytes = (size_t)parsed;
  return options;
}

skgpu::graphite::RecorderOptions GraphiteContext::makeRecorderOptions() {
  skgpu::graphite::RecorderOptions options;
  options.fImageProvider = sk_make_sp<CachingImageProvider>();
  // Ordered replay, because unordered replay makes Recorder::snap() drop
  // the glyph, path and clip atlases on every snap — so every glyph
  // re-uploads once per frame. Every host built on this factory snaps and
  // inserts exactly one Recording per frame, on one thread, in order, so
  // the ordering requirement costs nothing and the atlases survive.
  //
  // PRECONDITION, not a tradeoff: a Recording that is snapped and never
  // inserted — including a snap() that returns null — skips an ID and
  // permanently kills this Recorder. Every later insertRecording then
  // fails with kOutOfOrderRecording and nothing renders again, with no
  // error anyone checks. Never snap in order to discard.
  options.fRequireOrderedRecordings = true;
  return options;
}

GraphiteContext::GraphiteContext(
    std::unique_ptr<skgpu::graphite::Context> context,
    std::unique_ptr<skgpu::graphite::Recorder> recorder)
    : m_context(std::move(context)), m_recorder(std::move(recorder)) {}

std::unique_ptr<skgpu::graphite::Recorder> GraphiteContext::makeRecorder()
    const {
  return m_context ? m_context->makeRecorder(makeRecorderOptions()) : nullptr;
}

GraphiteContext::~GraphiteContext() = default;

}  // namespace sigil::skia
