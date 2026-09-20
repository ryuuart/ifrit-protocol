#pragma once

/** @file
 * @ingroup skia-graphite
 * Skia's Graphite backend brought up on a device someone else owns, and
 * the recorders drawing threads take out of it.
 */

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
class PrecompileContext;
class Recorder;
struct RecorderOptions;
struct ContextOptions;
}  // namespace skgpu::graphite

/** SKIA ON A DEVICE SOMEBODY ELSE OWNS. Graphite brought up against a
 *  graphics device and queue a host already has, surfaces and images
 *  over textures that host lends, the pixel reads an upload takes, and
 *  the canvas draws Graphite leaves unimplemented put back. Reach for
 *  it when Skia has to draw into a window, a texture or a queue this
 *  code did not create. Nothing here owns a device. */
namespace sigil::skia {

/** THE GRAPHITE CONTEXT AND RECORDER A HOST'S OWN DEVICE AND QUEUE CARRY,
 *  for drawing into offscreen textures from SkCanvas. Graphite's work
 *  rides that one queue in submission order, which is what lets a submit
 *  stay asynchronous. Metal and Vulkan are parallel bring-up paths with
 *  one factory each, both Qt-free.
 *  @trap A Recorder belongs to one thread and the Context to one thread
 *  at a time: every call on `context()` — inserting, submitting, reading
 *  back, retiring finished work — holds `lockContext()` once a second
 *  thread can reach it. */
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

  /** THE OPTIONS EVERY RECORDER MUST BE MADE WITH — pass them to
   *  `makeRecorder()`: the caching image provider, and ordered replay.
   *  @silent a recorder built without them draws any raster image,
   *  because Graphite uploads nothing implicitly and DROPS the draw.
   *  @trap Every recording such a recorder snaps must be inserted, in
   *  order: a skipped ID kills the recorder for the rest of the run. */
  static skgpu::graphite::RecorderOptions makeRecorderOptions();
  /** ONE FUNNEL FOR ContextOptions TOO, both backends: the glyph-atlas
   *  texture budget SIGILSKIA_GLYPH_ATLAS_BYTES caps, unset leaving
   *  Skia's own, and the one process-wide pool every context builds its
   *  device pipelines on. The pool is never released, because Skia
   *  requires it to outlive every context built over it. */
  static skgpu::graphite::ContextOptions makeContextOptions();

  /** WHERE A SHADER THAT WOULD NOT COMPILE IS REPORTED. Process-wide,
   *  and given to every context this factory builds AFTERWARDS, so set
   *  it before the context is created; the caller keeps ownership and
   *  must outlive the contexts, and null restores Skia's own reporting
   *  to stderr.
   *  @trap The handler is called from the pipeline pool, several of its
   *  threads at once, so what it collects into must be guarded. */
  static void reportShaderErrorsTo(skgpu::ShaderErrorHandler* handler);

  /** WHAT GRAPHITE DID WITH A PIPELINE, as it did it. One device
   *  program is built per distinct draw while the thread that recorded
   *  the draw waits for it; which pipelines a scene needs, and which
   *  were already standing, is what a warm-up acts on. */
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

  /** Given to every context this factory builds AFTERWARDS, so set it
   *  before the context is created. Process-wide; the caller keeps
   *  ownership and must outlive the contexts, and null reports nothing.
   *  @trap THE REPORTER MUST BE THREAD-SAFE: `added` arrives from the
   *  pool that built the pipeline, several at once, and `found` from
   *  whichever thread recorded the draw. */
  static void reportPipelinesTo(PipelineReporter* reporter);

  /** THE RUNTIME EFFECTS A SERIALISED PIPELINE KEY MAY NAME — undeclared,
   *  the key for a pipeline holding one is absent altogether. Copied
   *  into every context built AFTERWARDS, so declare them before the
   *  first; the list REPLACES the one before it and is CUT at
   *  `runtimeEffectLimit()`, so the answer, how many were taken, is what
   *  a caller keys stored keys on.
   *  @trap An effect's place in the list is part of its name. */
  static size_t registerRuntimeEffects(
      std::span<const sk_sp<SkRuntimeEffect>> effects);

  /** HOW MANY EFFECTS A DECLARATION MAY CARRY: the size of the
   *  backend's reserved block of client names, past which an effect
   *  cannot be given one. */
  static size_t runtimeEffectLimit();

  /** THE HELPER A WARM-UP BUILDS PIPELINES THROUGH, made where the
   *  context is and usable on ANOTHER THREAD — which is the point: a
   *  warm-up run where frames are drawn would be the stall it exists to
   *  remove. It holds the context's shared half rather than the
   *  context, so the thread that draws keeps drawing while it stands.
   *  Null when there is no context. */
  [[nodiscard]] std::unique_ptr<skgpu::graphite::PrecompileContext>
  makePrecompileContext() const;

  /** REBUILDS THE PIPELINES @p keys NAMES, on the calling thread, and
   *  answers how many were built; a key this backend or this version of
   *  Skia cannot read is skipped. It goes through a helper of its own,
   *  so a caller with several kinds of warming to do takes one
   *  `makePrecompileContext()` and spends it on all of them. */
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
