#pragma once
// Internal to IfritWeb — the graphics-API-neutral GPU backend contract.

#include <Ultralight/platform/GPUDriver.h>

#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <functional>
#include <unordered_set>

class SkCanvas;
class SkImage;

namespace skgpu::graphite {
class Recorder;
}

namespace ifrit::web {

/**
 * What the engine needs from a GPU backend, beyond Ultralight's own
 * GPUDriver command execution: publish blits, slot textures, and the
 * Skia interop (paint into / wrap as SkImage). One implementation per
 * graphics API — UltralightMetalDriver today; Vulkan and D3D
 * implementations slot in here when the Windows/Linux ports land, with
 * no changes to WebEngine/WebView/WebImage, which only ever see this
 * interface.
 *
 * "Native texture" handles are opaque void* owned by the backend
 * (id<MTLTexture> on Metal, VkImage-wrapper on Vulkan). Everything runs
 * on the web thread unless noted.
 */
class WebGpuDriver : public ultralight::GPUDriver {
public:
  ~WebGpuDriver() override = default;

  /** Executes the pending Ultralight command list. Returns the ids of
   *  render buffers that were cleared or drawn to (views to publish). */
  virtual std::unordered_set<uint32_t> flush() = 0;

  /** Creates a shader-readable texture for per-view frame publishing.
   *  Returned retained; free with releaseNativeTexture(). */
  virtual void *createPublishTexture(int width, int height) = 0;

  /** Creates a texture usable both as a Skia render target and as a
   *  sampled page image (WebImage slots), CPU-uploadable. Returned
   *  retained; free with releaseNativeTexture(). */
  virtual void *createImageTexture(int width, int height) = 0;

  /** Releases a texture created by this driver (safe from any thread). */
  virtual void releaseNativeTexture(void *texture) = 0;

  /** Copies the top-left region of the texture registered under
   *  @p srcTextureId into @p dstTexture. */
  virtual void copyTexture(uint32_t srcTextureId, void *dstTexture,
                           int width, int height) = 0;

  /** Copies between two native textures (clamped to the smaller size). */
  virtual void copyNativeTexture(void *srcTexture, void *dstTexture,
                                 int width, int height) = 0;

  /** Registers an externally-supplied texture under a fresh Ultralight
   *  texture id so page draw commands can bind it. */
  virtual uint32_t registerExternalTexture(void *texture) = 0;
  virtual void unregisterExternalTexture(uint32_t textureId) = 0;

  /** Copies raster pixels (premultiplied BGRA) into an image texture. */
  virtual void uploadToTexture(void *texture, const void *pixels, int width,
                               int height, size_t rowBytes) = 0;

  /** Runs @p painter with an SkCanvas targeting @p texture through the
   *  driver's own Graphite context, then flushes that work. */
  virtual bool paintTexture(void *texture, int width, int height,
                            const std::function<void(SkCanvas &)> &painter) = 0;

  /** Wraps @p texture as an SkImage for @p recorder's Graphite context.
   *  The image keeps the texture alive. Any thread. */
  virtual sk_sp<SkImage> wrapTexture(skgpu::graphite::Recorder *recorder,
                                     void *texture, int width,
                                     int height) = 0;
};

} // namespace ifrit::web
