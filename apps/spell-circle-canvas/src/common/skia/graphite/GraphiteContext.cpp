// Graphics-API-independent half of GraphiteContext: the options every
// backend factory funnels through, and the pair the factories wrap. The
// factories themselves are per API — GraphiteContextMetal.mm and
// GraphiteContextVulkan.cpp — and each stands Graphite up on that API's
// handles alone.

#include <gpu/graphite/Context.h>
#include <gpu/graphite/ContextOptions.h>
#include <gpu/graphite/Image.h>
#include <gpu/graphite/ImageProvider.h>
#include <gpu/graphite/Recorder.h>
#include <include/core/SkBitmap.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <atomic>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <cstdlib>

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
 *  thread owns the device. */
std::atomic<skgpu::ShaderErrorHandler*>& shaderErrorSink() {
  static std::atomic<skgpu::ShaderErrorHandler*> sink{nullptr};
  return sink;
}

}  // namespace

void GraphiteContext::reportShaderErrorsTo(skgpu::ShaderErrorHandler* handler) {
  shaderErrorSink().store(handler, std::memory_order_relaxed);
}

skgpu::graphite::ContextOptions GraphiteContext::makeContextOptions() {
  skgpu::graphite::ContextOptions options;
  // Null leaves Skia's own handler in place, which prints to stderr.
  options.fShaderErrorHandler =
      shaderErrorSink().load(std::memory_order_relaxed);
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
