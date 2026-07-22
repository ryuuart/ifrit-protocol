#pragma once
#include <memory>

class QRhi;

namespace skgpu::graphite {
class Context;
class Recorder;
struct RecorderOptions;
} // namespace skgpu::graphite

/**
 * Owns the Skia Graphite Context + Recorder used to draw into offscreen
 * textures from SkCanvas. Built on an existing native device/queue so
 * Graphite's GPU work rides the same command queue as the host's own
 * rendering.
 *
 * Two kinds of entry point exist:
 *  - create(QRhi *): Qt applications hand over the QRhi whose native
 *    device/queue Graphite should share. Implemented per graphics API in
 *    the Qt adapter target SpellCircleSkiaQt (SkiaQtInteropMetal.mm on
 *    Apple, SkiaGraphiteContextVulkan.cpp elsewhere) — link that target,
 *    not SpellCircleSkia, from Qt code.
 *  - createMetal(...): Qt-free bring-up from raw Metal handles, used by the
 *    native macOS app. Lives in SpellCircleSkia, which never links Qt.
 */
class SkiaGraphiteContext {
public:
  /** Returns null if @p rhi isn't backed by this build's graphics API or
   *  Context creation fails. Qt adapter — see the class comment. */
  static std::unique_ptr<SkiaGraphiteContext> create(QRhi *rhi);

#ifdef __APPLE__
  /** Qt-free Metal bring-up: @p mtlDevice / @p mtlCommandQueue are
   *  id<MTLDevice> / id<MTLCommandQueue> bridged to void*. Both are
   *  retained for the context's lifetime; the caller keeps its own
   *  references. Returns null if Context creation fails. */
  static std::unique_ptr<SkiaGraphiteContext> createMetal(void *mtlDevice,
                                                          void *mtlCommandQueue);
#endif

  ~SkiaGraphiteContext();

  /** Returns the owned Graphite context. */
  skgpu::graphite::Context *context() const { return m_context.get(); }
  /** Returns the recorder associated with `context()`. */
  skgpu::graphite::Recorder *recorder() const { return m_recorder.get(); }

  /** RecorderOptions every backend factory must pass to makeRecorder():
   *  installs the caching ImageProvider that promotes non-Graphite
   *  (raster) SkImages to textures on first draw. Graphite performs NO
   *  implicit uploads — without a provider it silently drops any draw
   *  that samples a raster image. */
  static skgpu::graphite::RecorderOptions makeRecorderOptions();

private:
  SkiaGraphiteContext(std::unique_ptr<skgpu::graphite::Context> context,
                      std::unique_ptr<skgpu::graphite::Recorder> recorder);

  std::unique_ptr<skgpu::graphite::Context> m_context;
  std::unique_ptr<skgpu::graphite::Recorder> m_recorder;
};
