#pragma once
#include <include/core/SkRefCnt.h>
#include <sigilcore/hardware/GpuDevice.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>

class SkData;
class SkRuntimeEffect;

namespace skgpu {
class ShaderErrorHandler;
}  // namespace skgpu

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
   *  texture budget; unset leaves Skia's own default in place.
   *
   *  Every context is given one process-wide thread pool to build its
   *  device pipelines on. Without it Graphite compiles each one
   *  serially on the thread that recorded the draw, which for a scene
   *  wearing a chain of runtime shaders is a dozen compiles inside its
   *  first frame. The pool is never released: Skia requires it to
   *  outlive every context built over it, and the last context goes
   *  during static teardown. */
  static skgpu::graphite::ContextOptions makeContextOptions();

  /** WHERE A SHADER THAT WOULD NOT COMPILE IS REPORTED. Graphite builds
   *  the fragment program for a draw at record time and compiles it on
   *  the device; a program that fails there is dropped, the draw paints
   *  nothing, and the frame after it tries again. With no handler
   *  installed Skia prints the generated shader and the compiler's
   *  errors to stderr and the process carries on, so a body that
   *  compiles as its own SkSL program and not once Graphite has inlined
   *  it into a pipeline is a scrolling log rather than something a
   *  caller can act on. A handler set here is given to every context
   *  this factory builds afterwards, which is what makes such a failure
   *  observable — so set it BEFORE the context is created. Process-wide;
   *  the caller keeps ownership and must outlive the contexts. Null
   *  restores Skia's own reporting.
   *
   *  The compile that fails runs on the pool every context builds its
   *  pipelines on, so the handler is called FROM THAT POOL and from
   *  several of its threads at once when a scene's stages fail
   *  together: a handler that collects must guard what it collects
   *  into, and one that counts must count atomically. */
  static void reportShaderErrorsTo(skgpu::ShaderErrorHandler* handler);

  /** WHAT GRAPHITE DID WITH A PIPELINE, as it did it.
   *
   *  A backend builds one device program per distinct draw and the
   *  thread that recorded the draw waits for it, so a scene wearing a
   *  chain of runtime shaders pays one program per stage the first time
   *  it is drawn. Which pipelines a scene needs, and which of them were
   *  already standing, is the whole of what a warm-up can act on, and
   *  nothing else reports it. */
  class PipelineReporter {
   public:
    virtual ~PipelineReporter() = default;
    /** A pipeline Graphite has just built. @p key is what a later run
     *  rebuilds it from through `precompile`, and is null for a
     *  pipeline this backend cannot serialise. @p fromPrecompile says
     *  it was built ahead of a draw rather than for one. */
    virtual void added(const std::string& label, std::uint32_t uniqueHash,
                       bool fromPrecompile, sk_sp<SkData> key) = 0;
    /** A pipeline a draw asked for and found already built. */
    virtual void found(const std::string& label, std::uint32_t uniqueHash,
                       bool fromPrecompile) = 0;
  };

  /** Given to every context this factory builds afterwards, so set it
   *  BEFORE the context is created. Process-wide; the caller keeps
   *  ownership and must outlive the contexts. Null reports nothing.
   *
   *  THE REPORTER MUST BE THREAD-SAFE. A pipeline is built on the pool
   *  every context compiles on, and `added` is called from the thread
   *  that built it — several at once for a scene whose stages are
   *  built beside each other. `found` arrives on whichever thread
   *  asked for the pipeline, which is the thread that recorded the
   *  draw, and may run at the same time as an `added` for another one. */
  static void reportPipelinesTo(PipelineReporter* reporter);

  /** THE RUNTIME EFFECTS A SERIALISED PIPELINE KEY MAY NAME.
   *
   *  A pipeline's key describes the whole inlined paint tree, and a
   *  runtime effect in that tree has no name a later run would
   *  recognise unless it was declared here: without the declaration the
   *  key for such a pipeline is absent, and precompiling a recorded set
   *  skips exactly the stages a chain of effects is made of. The effects
   *  are copied and given to every context built afterwards, so declare
   *  them BEFORE the first one. Process-wide, and the list REPLACES
   *  whatever stood before it: a caller that keeps recorded keys on
   *  disk must throw them away whenever this list changes, because the
   *  same effect at a different place in it is a different name. */
  static void registerRuntimeEffects(
      std::span<const sk_sp<SkRuntimeEffect>> effects);

  /** REBUILDS THE PIPELINES @p keys NAMES, on the calling thread.
   *
   *  A key this backend or this version of Skia cannot read is skipped;
   *  the answer is how many pipelines were built. The helper it runs
   *  through may be made here and used on another thread, which is the
   *  point: a warm-up run where frames are drawn would be the stall it
   *  exists to remove. */
  [[nodiscard]] size_t precompile(std::span<const sk_sp<SkData>> keys) const;

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
