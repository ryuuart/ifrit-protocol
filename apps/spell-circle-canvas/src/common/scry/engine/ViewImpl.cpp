/** @file
 * The web-thread side of a view: a repaint snapshotted into a pooled
 * immutable buffer on CPU engines, blitted into the spare publish
 * texture and swapped in on GPU ones, the frame callback fired, and the
 * page's load and console events forwarded.
 */

#include "ViewImpl.h"

#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>

#include <cstring>

#include "EngineImpl.h"
#include "Utf8.h"

namespace sigil::scry {

bool WebView::Impl::publishIfDirty() {
  auto* surface = static_cast<SkiaSurface*>(view->surface());
  if (!surface || surface->dirty_bounds().IsEmpty()) return false;

  ultralight::IntRect dirty = surface->dirty_bounds();
  SkIRect dirtyBounds =
      SkIRect::MakeLTRB(dirty.left, dirty.top, dirty.right, dirty.bottom);

  // Snapshot into an immutable buffer so consumers on other threads can
  // hold the SkImage for as long as they like while Ultralight keeps
  // painting into the live surface. Buffers are pooled: once every
  // consumer releases a frame, its allocation is reused instead of
  // paying a fresh multi-megabyte alloc (and its page faults) per
  // publish.
  const SkBitmap& src = surface->bitmap();
  const size_t byteSize = src.computeByteSize();
  sk_sp<SkData> buffer;
  for (auto it = publishPool.begin(); it != publishPool.end(); ++it) {
    if ((*it)->size() == byteSize && (*it)->unique()) {
      // Take the pool's ref so the buffer is uniquely ours while writing
      // (SkData::writable_data requires it).
      buffer = std::move(*it);
      publishPool.erase(it);
      break;
    }
  }
  std::erase_if(publishPool, [byteSize](const sk_sp<SkData>& pooled) {
    return pooled->size() != byteSize && pooled->unique();  // post-resize
  });
  if (!buffer) buffer = SkData::MakeUninitialized(byteSize);
  std::memcpy(buffer->writable_data(), src.getPixels(), byteSize);

  sk_sp<SkImage> image =
      SkImages::RasterFromData(src.info(), buffer, src.rowBytes());
  if (publishPool.size() < 2) publishPool.push_back(std::move(buffer));
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    latestImage = image;
    lastDirtyBounds = dirtyBounds;
  }
  uint64_t newVersion = ++version;
  surface->ClearDirtyBounds();

  if (frameCallback)
    frameCallback({std::move(image),
                   {},
                   src.width(),
                   src.height(),
                   dirtyBounds,
                   newVersion});
  return true;
}

bool WebView::Impl::publishGpuIfDirty(
    GpuDriver& driver, const std::unordered_set<uint32_t>& dirtyRenderBuffers) {
  ultralight::RenderTarget target = view->render_target();
  if (target.is_empty || !dirtyRenderBuffers.count(target.render_buffer_id))
    return false;

  const int frameWidth = static_cast<int>(target.width);
  const int frameHeight = static_cast<int>(target.height);
  if (gpuTextureWidth != frameWidth || gpuTextureHeight != frameHeight) {
    releaseGpuTextures();
    publishedGpuTexture = driver.createPublishTexture(frameWidth, frameHeight);
    spareGpuTexture = driver.createPublishTexture(frameWidth, frameHeight);
    gpuTextureWidth = frameWidth;
    gpuTextureHeight = frameHeight;
  }

  driver.copyTexture(target.texture_id, spareGpuTexture, frameWidth,
                     frameHeight);
  SkIRect dirtyBounds = SkIRect::MakeWH(frameWidth, frameHeight);
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    std::swap(publishedGpuTexture, spareGpuTexture);
    lastDirtyBounds = dirtyBounds;
  }
  uint64_t newVersion = ++version;

  if (frameCallback)
    frameCallback({nullptr, publishedGpuTexture, frameWidth, frameHeight,
                   dirtyBounds, newVersion});
  return true;
}

void WebView::Impl::releaseGpuTextures() {
  std::lock_guard<std::mutex> lock(frameMutex);
  if (GpuDriver* driver = engine->gpuDriver()) {
    driver->releaseTexture(publishedGpuTexture);
    driver->releaseTexture(spareGpuTexture);
  }
  publishedGpuTexture = {};
  spareGpuTexture = {};
  gpuTextureWidth = 0;
  gpuTextureHeight = 0;
  cachedWrap = nullptr;  // the wrap itself keeps its texture alive
  cachedWrapVersion = 0;
  cachedWrapRecorder = nullptr;
}

void WebView::Impl::OnFinishLoading(ultralight::View*, uint64_t,
                                    bool isMainFrame,
                                    const ultralight::String&) {
  if (isMainFrame && loadCallback) loadCallback();
}

void WebView::Impl::OnAddConsoleMessage(
    ultralight::View*, const ultralight::ConsoleMessage& message) {
  LogLevel level = LogLevel::Info;
  if (message.level() == ultralight::MessageLevel::kMessageLevel_Error)
    level = LogLevel::Error;
  else if (message.level() == ultralight::MessageLevel::kMessageLevel_Warning)
    level = LogLevel::Warning;
  engine->logger()->log(level, "console: " + toUtf8(message.message()));
}

}  // namespace sigil::scry
