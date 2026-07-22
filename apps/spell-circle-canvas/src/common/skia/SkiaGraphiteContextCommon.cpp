// Graphics-API-independent half of SkiaGraphiteContext. The static
// create() factory is per-API: exactly one of SkiaGraphiteContextMetal.mm
// or SkiaGraphiteContextVulkan.cpp is compiled into a given build (see
// CMakeLists.txt), each defining create() for the QRhi backend it serves
// and returning null for every other backend.

#include "SkiaGraphiteContext.h"

#include <include/core/SkBitmap.h>

#include <gpu/graphite/Context.h>
#include <gpu/graphite/Image.h>
#include <gpu/graphite/ImageProvider.h>
#include <gpu/graphite/Recorder.h>

#include <cstdint>
#include <unordered_map>

namespace {

/** Graphite performs no implicit uploads: any draw sampling a non-Graphite
 *  (raster) SkImage asks the Recorder's client ImageProvider for a texture
 *  version, and DROPS the draw when there is none. This provider promotes
 *  on first use and caches by (image uniqueID, mipmapped) so a raster
 *  atlas/nine-slice uploads once, not per draw. The cache pins textures
 *  until the crude full-evict at capacity — sized for the hosts here (a
 *  handful of long-lived generated atlases), revisit if a host starts
 *  churning thousands of distinct images. */
class CachingImageProvider final : public skgpu::graphite::ImageProvider {
public:
  sk_sp<SkImage> findOrCreate(skgpu::graphite::Recorder *recorder,
                              const SkImage *image,
                              SkImage::RequiredProperties required) override {
    const uint64_t key = (static_cast<uint64_t>(image->uniqueID()) << 1) |
                         (required.fMipmapped ? 1u : 0u);
    if (auto it = m_cache.find(key); it != m_cache.end())
      return it->second;
    sk_sp<SkImage> texture =
        SkImages::TextureFromImage(recorder, image, required);
    if (!texture && image->colorType() == kRGBA_F32_SkColorType) {
      // F32 sources (EXR imports land as F32 SkImages) are not filterable
      // on Apple GPUs, so promotion fails outright. Retry with an F16 copy:
      // float range survives, the loader's F32 processing semantics are
      // untouched, and the draw actually happens.
      SkBitmap f16;
      if (f16.tryAllocPixels(
              image->imageInfo().makeColorType(kRGBA_F16_SkColorType)) &&
          image->readPixels(nullptr, f16.pixmap(), 0, 0)) {
        f16.setImmutable();
        texture =
            SkImages::TextureFromImage(recorder, f16.asImage().get(), required);
      }
    }
    if (!texture)
      return nullptr;
    if (m_cache.size() >= kMaxEntries)
      m_cache.clear(); // stale uniqueIDs accumulate across hot reloads
    m_cache.emplace(key, texture);
    return texture;
  }

private:
  static constexpr size_t kMaxEntries = 256;
  std::unordered_map<uint64_t, sk_sp<SkImage>> m_cache;
};

} // namespace

skgpu::graphite::RecorderOptions SkiaGraphiteContext::makeRecorderOptions() {
  skgpu::graphite::RecorderOptions options;
  options.fImageProvider = sk_make_sp<CachingImageProvider>();
  return options;
}

SkiaGraphiteContext::SkiaGraphiteContext(
    std::unique_ptr<skgpu::graphite::Context> context,
    std::unique_ptr<skgpu::graphite::Recorder> recorder)
    : m_context(std::move(context)), m_recorder(std::move(recorder)) {}

SkiaGraphiteContext::~SkiaGraphiteContext() = default;
