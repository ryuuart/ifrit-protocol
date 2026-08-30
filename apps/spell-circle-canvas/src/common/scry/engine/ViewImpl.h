#pragma once

/** @file
 * WebView::Impl — the ultralight::View, the latest published frame
 * (a raster image on CPU engines, ping-pong publish textures on GPU
 * ones), the per-version wrap cache, the recycled publish buffers, and
 * the page's load and console listeners. Internal: it names Ultralight
 * types, and Ultralight is private to the library.
 */

#include <Ultralight/Ultralight.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <sigilskia/device/Handle.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "sigilscry/engine/WebEngine.h"
#include "sigilscry/engine/WebView.h"

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::scry {

class GpuDriver;

class WebView::Impl final : public ultralight::LoadListener,
                            public ultralight::ViewListener {
 public:
  ~Impl() override = default;

  WebEngine::Impl* engine = nullptr;
  ultralight::RefPtr<ultralight::View> view;  // web thread only
  std::atomic<int> width{0};
  std::atomic<int> height{0};

  // Latest published frame, readable from any thread. CPU engines fill
  // latestImage; GPU engines ping-pong two driver-owned textures and
  // expose publishedGpuTexture.
  mutable std::mutex frameMutex;
  sk_sp<SkImage> latestImage;
  sigil::skia::TextureHandle publishedGpuTexture;
  sigil::skia::TextureHandle spareGpuTexture;
  int gpuTextureWidth = 0;
  int gpuTextureHeight = 0;
  SkIRect lastDirtyBounds = SkIRect::MakeEmpty();
  std::atomic<uint64_t> version{0};

  // Per-version cache of the Graphite wrap handed out by frame(): keeps
  // the SkImage identity stable across draws of one frame (Skia caches
  // key on it) and makes repeat acquisition free. Guarded by frameMutex;
  // the wrap itself happens on the recorder owner's thread.
  mutable sk_sp<SkImage> cachedWrap;
  mutable uint64_t cachedWrapVersion = 0;
  mutable skgpu::graphite::Recorder* cachedWrapRecorder = nullptr;

  // CPU publish buffers, recycled once consumers release the SkImages
  // that reference them (web thread only).
  std::vector<sk_sp<SkData>> publishPool;

  // Web-thread-only state (set via posted tasks, invoked on the web thread).
  std::function<void(const WebView::Frame&)> frameCallback;
  std::function<void()> loadCallback;

  /** Web thread: snapshots the surface into an immutable SkImage if the
   *  page repainted since the last publish. Returns true on publish. */
  bool publishIfDirty();

  /** Web thread: blit-copies the view's render target into the spare
   *  publish texture and swaps it in, when this view's render buffer is
   *  in @p dirtyRenderBuffers. Returns true on publish. */
  bool publishGpuIfDirty(
      GpuDriver& driver,
      const std::unordered_set<uint32_t>& dirtyRenderBuffers);

  /** Web thread: releases the ping-pong publish textures. */
  void releaseGpuTextures();

  // ultralight::LoadListener
  void OnFinishLoading(ultralight::View* caller, uint64_t frameId,
                       bool isMainFrame,
                       const ultralight::String& url) override;

  // ultralight::ViewListener
  void OnAddConsoleMessage(ultralight::View* caller,
                           const ultralight::ConsoleMessage& message) override;
};

}  // namespace sigil::scry
