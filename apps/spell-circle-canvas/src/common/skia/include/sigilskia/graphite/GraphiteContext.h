#pragma once
#include <sigilcore/hardware/GpuDevice.h>

#include <memory>
#include <mutex>

namespace skgpu::graphite {
class Context;
class Recorder;
struct RecorderOptions;
struct ContextOptions;
}  // namespace skgpu::graphite

namespace sigil::skia {

/**
 * Owns the Skia Graphite Context + Recorder used to draw into offscreen
 * textures from SkCanvas. Built on an existing native device/queue so
 * Graphite's GPU work rides the same command queue as the host's own
 * rendering: submissions and the host's later passes execute in
 * submission order on that one queue, which is what lets a submit stay
 * asynchronous.
 *
 * Metal and Vulkan are parallel bring-up paths with one factory each,
 * both Qt-free. A Qt host reaches them through the adapters in
 * <sigilskia/qt/QtInterop.h>, which unwrap a QRhi's native handles and
 * forward here.
 *
 * Threads: a Recorder belongs to one thread; the Context tolerates use
 * from several threads but never at once. `recorder()` is the thread
 * that made the context; another thread takes its own from
 * `makeRecorder()` and records on it alone; and every call on
 * `context()` — inserting, submitting, reading back, retiring finished
 * work — from any thread holds `lockContext()` once more than one thread
 * can reach the context.
 */
class GraphiteContext {
 public:
  /** Graphite on the device and queue behind @p device, whichever API it
   *  is: the one entry point a host holding a device needs. Returns
   *  null when the device's API has no bring-up in this build or
   *  Context creation fails. */
  static std::unique_ptr<GraphiteContext> create(
      core::hardware::GpuDevice& device);

#ifdef __APPLE__
  /** Metal bring-up: @p mtlDevice / @p mtlCommandQueue are id<MTLDevice> /
   *  id<MTLCommandQueue> bridged to void*. Both are retained for the
   *  context's lifetime; the caller keeps its own references and stays
   *  the owner. Returns null if Context creation fails. */
  static std::unique_ptr<GraphiteContext> createMetal(void* mtlDevice,
                                                      void* mtlCommandQueue);
#endif

  /** Vulkan bring-up on handles someone else owns and keeps alive for
   *  the context's lifetime. Returns null when a handle is missing, when
   *  this build's Skia carries no Vulkan backend, or when Context
   *  creation fails. */
  static std::unique_ptr<GraphiteContext> createVulkan(
      const core::hardware::VulkanHandles& handles);

  ~GraphiteContext();

  /** Returns the owned Graphite context. */
  skgpu::graphite::Context* context() const { return m_context.get(); }
  /** Returns the recorder associated with `context()`, for the thread
   *  that made the context. */
  skgpu::graphite::Recorder* recorder() const { return m_recorder.get(); }

  /** A recorder of its own for another thread, built with the same
   *  options as `recorder()` — the image provider and ordered replay —
   *  so it carries the same preconditions. Recordings it snaps are
   *  inserted into `context()` under `lockContext()`. */
  std::unique_ptr<skgpu::graphite::Recorder> makeRecorder() const;

  /** Holds the context for the calling thread; every `context()` call
   *  from a thread other than the one that made it happens under this,
   *  and so does the making thread's own once another thread exists. */
  std::unique_lock<std::mutex> lockContext() const {
    return std::unique_lock<std::mutex>(m_contextMutex);
  }

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
  /** Wraps a context and the one recorder made from it; the backend
   *  factories build the pair and hand it here. A context is reached
   *  only through a factory, and only ever by pointer. */
  GraphiteContext(std::unique_ptr<skgpu::graphite::Context> context,
                  std::unique_ptr<skgpu::graphite::Recorder> recorder);

  std::unique_ptr<skgpu::graphite::Context> m_context;
  std::unique_ptr<skgpu::graphite::Recorder> m_recorder;
  mutable std::mutex m_contextMutex;
};

}  // namespace sigil::skia
