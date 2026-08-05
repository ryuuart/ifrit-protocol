#pragma once
#include <memory>

class QRhi;

namespace skgpu::graphite {
class Context;
class Recorder;
struct RecorderOptions;
struct ContextOptions;
}  // namespace skgpu::graphite

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
  static std::unique_ptr<SkiaGraphiteContext> create(QRhi* rhi);

#ifdef __APPLE__
  /** Qt-free Metal bring-up: @p mtlDevice / @p mtlCommandQueue are
   *  id<MTLDevice> / id<MTLCommandQueue> bridged to void*. Both are
   *  retained for the context's lifetime; the caller keeps its own
   *  references. Returns null if Context creation fails. */
  static std::unique_ptr<SkiaGraphiteContext> createMetal(
      void* mtlDevice, void* mtlCommandQueue);
#endif

  ~SkiaGraphiteContext();

  /** Returns the owned Graphite context. */
  skgpu::graphite::Context* context() const { return m_context.get(); }
  /** Returns the recorder associated with `context()`. */
  skgpu::graphite::Recorder* recorder() const { return m_recorder.get(); }

  /** REQUIRED for every recorder: pass these to makeRecorder().
   *
   *  Two settings here are preconditions, and violating either fails
   *  silently rather than loudly.
   *
   *  1. The caching ImageProvider. Graphite performs NO implicit
   *     uploads: a draw that samples a raster (non-Graphite) SkImage
   *     asks the recorder's provider for a texture version and DROPS
   *     the draw when there is none. A recorder built without these
   *     options renders nothing from any raster image and reports no
   *     error.
   *  2. Ordered recordings. Every recording this recorder snaps must be
   *     inserted, in order. A snap that returns null, or one whose
   *     recording is discarded, skips an ID and permanently kills the
   *     recorder — every later insert fails and nothing ever renders
   *     again. Never snap in order to throw the result away. */
  static skgpu::graphite::RecorderOptions makeRecorderOptions();
  /** One funnel for ContextOptions too (both backends). Reads
   *  SIGILSKIA_GLYPH_ATLAS_BYTES to cap the Graphite glyph-atlas
   *  texture budget; unset leaves Skia's own default in place. */
  static skgpu::graphite::ContextOptions makeContextOptions();

 private:
  SkiaGraphiteContext(std::unique_ptr<skgpu::graphite::Context> context,
                      std::unique_ptr<skgpu::graphite::Recorder> recorder);

  std::unique_ptr<skgpu::graphite::Context> m_context;
  std::unique_ptr<skgpu::graphite::Recorder> m_recorder;
};
