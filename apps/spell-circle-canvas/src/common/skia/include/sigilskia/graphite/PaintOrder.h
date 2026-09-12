#pragma once

/** @file
 * A canvas that keeps a scene's painting order on a device whose
 * destination reads cost the depth attachment.
 *
 * Graphite paints out of order on purpose: a draw that does not read the
 * destination carries a depth value, the backend may execute it whenever
 * it likes, and the depth test rejects whatever the original order put
 * beneath it. A draw that DOES read the destination — any blend mode past
 * Skia's coefficient modes, or a blender of its own — cannot be reordered
 * that freely, so Graphite makes it depend on the draws it overlaps and
 * relies on the depth test to reject it where a later draw has already
 * landed.
 *
 * workaround: on the Vulkan backend such a read is an input attachment and
 * Graphite guards it with a barrier inside the render pass; MoltenVK
 * serves that barrier by restarting the pass, and the depth attachment
 * does not survive the restart. Every draw the pass had already executed
 * then loses its depth, and the reading draw paints over content that was
 * described after it — a solid box over a dense window comes back with
 * holes in it, each hole exactly the shape of something drawn earlier.
 *
 * The fence is to close the recording after a reading draw, so that draw
 * is the last of its render pass and the ones described after it begin a
 * pass of their own. Draw a scene through this canvas and the picture is
 * the CPU's; the cost is one render pass per reading draw.
 */

#include <include/utils/SkPaintFilterCanvas.h>

#include <vector>

class SkCanvas;
class SkMatrix;
class SkPaint;
class SkPicture;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::skia {

class GraphiteContext;

/**
 * Forwards every draw to the canvas it wraps, and closes @p context's
 * recording after any draw that makes the backend read the destination.
 * Cheap where nothing reads: a scene without an advanced blend mode
 * closes nothing and costs one virtual call a draw.
 *
 * The canvas it wraps must be @p context's own, and both must outlive it.
 */
class PaintOrderCanvas : public SkPaintFilterCanvas {
 public:
  PaintOrderCanvas(GraphiteContext& context, SkCanvas* target);

  /** Whether @p context is a backend whose destination reads cost the
   *  depth attachment. A canvas over a backend that answers false
   *  forwards every draw and closes nothing, so a host wraps
   *  unconditionally and pays only where the fence is real. */
  static bool needed(const GraphiteContext& context);

  /** How many recordings this canvas has closed since it was made. What
   *  a test asserts about, and what says whether a scene pays anything
   *  for the fence. */
  int fences() const { return m_fences; }

  skgpu::graphite::Recorder* recorder() const override;

 protected:
  bool onFilter(SkPaint& paint) const override;

  /** Plays a picture back through this canvas rather than handing it to
   *  the one below, so a reading draw inside it is fenced like any
   *  other. */
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override;

  void willSave() override;
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override;
  void willRestore() override;
  void didRestore() override;

 private:
  /** Closes the recording, so everything described so far is one render
   *  pass and everything after it starts another. */
  void fence() const;

  GraphiteContext* m_context;
  SkCanvas* m_target;
  /** False on a backend that reads the destination coherently, where
   *  every draw is forwarded and nothing is ever closed. */
  bool m_fencing;
  /** One entry a save, true where the layer's own paint reads the
   *  destination — a layer like that is composited by the restore, which
   *  is the draw that has to end the pass. */
  std::vector<bool> m_layerReadsDestination;
  bool m_restoreReadsDestination = false;
  /** A reading draw has landed and the next draw must begin a new pass. */
  mutable bool m_pending = false;
  mutable int m_fences = 0;
};

}  // namespace sigil::skia
