#pragma once

/** @file
 * @ingroup skia-graphite
 * A canvas that keeps a scene's painting order on a device whose
 * destination reads cost the depth attachment.
 *
 * workaround: MoltenVK serves the barrier Graphite guards a destination
 * read with by restarting the render pass, and the depth attachment does
 * not survive the restart, so every draw the pass had already executed
 * loses its depth and the reading draw paints over what came after it.
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

/** FORWARDS EVERY DRAW TO THE CANVAS IT WRAPS, and closes the context's
 *  recording after any draw that makes the backend read the destination,
 *  so that draw is the last of its render pass. Cheap where nothing
 *  reads: a scene without an advanced blend mode closes nothing and
 *  costs one virtual call a draw.
 *  @trap The canvas it wraps must be the context's own, and both must
 *  outlive it. */
class PaintOrderCanvas : public SkPaintFilterCanvas {
 public:
  /** Wraps @p target, which must be @p context's own canvas; both must
   *  outlive this one. */
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

  /** The recorder the wrapped canvas draws into, so a helper that
   *  promotes a texture finds it through this canvas too. */
  skgpu::graphite::Recorder* recorder() const override;

 protected:
  /** Closes the recording after a draw whose paint makes the backend
   *  read the destination, and leaves the paint alone. */
  bool onFilter(SkPaint& paint) const override;

  /** Plays a picture back through this canvas rather than handing it to
   *  the one below, so a reading draw inside it is fenced like any
   *  other. */
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override;

  /** @name Layer bookkeeping
   *  A layer whose own paint reads the destination is composited by its
   *  restore, so the restore is the draw that has to end the pass.
   *  These four track which layer that is.
   *  @{ */
  /** Opens a layer entry that reads nothing. */
  void willSave() override;
  /** Opens a layer entry, recording whether @p rec's paint reads the
   *  destination. */
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override;
  /** Takes the innermost layer entry off, remembering whether its
   *  composite reads the destination. */
  void willRestore() override;
  /** Closes the recording when the composite just performed read the
   *  destination. */
  void didRestore() override;
  /** @} */

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
