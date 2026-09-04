#pragma once

/** @file
 * The graphics-API-neutral GPU backend contract: what the engine needs
 * from a GPU driver beyond Ultralight's own command execution. Internal:
 * it names Ultralight types, and Ultralight is private to the library.
 */

#include <Ultralight/platform/GPUDriver.h>
#include <include/core/SkRefCnt.h>
#include <sigilcore/hardware/Handle.h>

#include <boost/unordered/unordered_flat_set.hpp>
#include <cstdint>
#include <functional>

class SkCanvas;
class SkImage;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::scry {

/**
 * What the engine needs from a GPU backend, beyond Ultralight's own
 * GPUDriver command execution: publish blits, slot textures, and the
 * Skia interop (paint into / wrap as SkImage). One implementation per
 * graphics API — Metal today; a Vulkan implementation slots in here
 * with no changes to the engine, which only ever sees this interface.
 *
 * Textures are named by the engine's GpuDevice: the backend creates
 * them natively, imports them borrowed, and hands out the handles, so
 * everything above the driver — and the host — speaks handles and
 * never a native object. Everything runs on the web thread unless
 * noted.
 */
class GpuDriver : public ultralight::GPUDriver {
 public:
  ~GpuDriver() override = default;

  /** Executes the pending Ultralight command list. Returns the ids of
   *  render buffers that were cleared or drawn to (views to publish). */
  virtual boost::unordered_flat_set<uint32_t> flush() = 0;

  /** Creates a shader-readable texture for per-view frame publishing,
   *  owned by the driver; free with releaseTexture(). */
  virtual sigil::core::hardware::TextureHandle createPublishTexture(
      int width, int height) = 0;

  /** Creates a texture usable both as a Skia render target and as a
   *  sampled page image (WebImage slots), CPU-uploadable, owned by the
   *  driver; free with releaseTexture(). */
  virtual sigil::core::hardware::TextureHandle createImageTexture(
      int width, int height) = 0;

  /** Forgets the handle on the device and drops the driver's own
   *  reference; a wrap that still holds the texture keeps it alive.
   *  Safe from any thread. */
  virtual void releaseTexture(sigil::core::hardware::TextureHandle texture) = 0;

  /** Copies the top-left region of the texture registered under
   *  @p srcTextureId into @p dst. */
  virtual void copyTexture(uint32_t srcTextureId,
                           sigil::core::hardware::TextureHandle dst, int width,
                           int height) = 0;

  /** Copies between two device textures (clamped to the smaller size).
   *  False when either handle is stale. */
  virtual bool copyDeviceTexture(sigil::core::hardware::TextureHandle src,
                                 sigil::core::hardware::TextureHandle dst,
                                 int width, int height) = 0;

  /** Registers a device texture under a fresh Ultralight texture id so
   *  page draw commands can bind it. */
  virtual uint32_t registerExternalTexture(
      sigil::core::hardware::TextureHandle texture) = 0;
  virtual void unregisterExternalTexture(uint32_t textureId) = 0;

  /** Copies raster pixels (premultiplied BGRA) into an image texture. */
  virtual void uploadToTexture(sigil::core::hardware::TextureHandle texture,
                               const void* pixels, int width, int height,
                               size_t rowBytes) = 0;

  /** Runs @p painter with an SkCanvas targeting @p texture on the web
   *  thread's recorder over the shared Graphite context, then inserts
   *  and submits that work under the context's lock. Returns false if
   *  the frame did not render (no surface, empty recording, or a failed
   *  Graphite insert). */
  virtual bool paintTexture(sigil::core::hardware::TextureHandle texture,
                            int width, int height,
                            const std::function<void(SkCanvas&)>& painter) = 0;

  /** Wraps @p texture as an SkImage for @p recorder. The image keeps the
   *  native texture alive past the handle. Any thread. */
  virtual sk_sp<SkImage> wrapTexture(
      skgpu::graphite::Recorder* recorder,
      sigil::core::hardware::TextureHandle texture, int width, int height) = 0;
};

}  // namespace sigil::scry
