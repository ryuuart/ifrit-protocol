#pragma once

/** @file
 * @ingroup skia-graphite
 * A canvas that keeps a scene's painting order on a device whose
 * destination reads cost the depth attachment, and keeps what a pass drew
 * when the next pass on the same device begins.
 *
 * workaround: MoltenVK serves the barrier Graphite guards a destination
 * read with by restarting the render pass, and the depth attachment does
 * not survive the restart, so every draw the pass had already executed
 * loses its depth and the reading draw paints over what came after it.
 *
 * workaround: MoltenVK resolves a multisample attachment over the whole
 * texture whatever area the render pass names, while Graphite reloads
 * the multisample attachment only inside the area its draws cover; a pass
 * that resumes on a device therefore resolves undefined samples over
 * everything outside that area. Graphite loads the whole attachment when
 * a pass covers the whole device, so the first draw of a resumed pass is
 * preceded by one that does and changes nothing.
 */

#include <include/utils/SkPaintFilterCanvas.h>

#include <cstdint>
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
 *  so that draw is the last of its render pass. Each device a draw lands
 *  in after a recording has closed — this canvas's fence, a surface's
 *  submit, a readback — or after a backdrop has been read from it is
 *  first covered edge to edge by a transparent draw, so the pass resumed
 *  there reloads the whole multisample attachment. Cheap where nothing
 *  reads: a scene without an advanced blend mode closes nothing and pays
 *  one cover a frame and one virtual call a draw.
 *  @trap A pass ended where this library cannot see it — a recording
 *  closed by a caller's own snap rather than through this library, or a
 *  pass Graphite ends inside one recording when the surface is
 *  snapshotted while it is still being drawn — leaves its device
 *  uncovered.
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
   *  read the destination, covers the device the draw lands in when its
   *  pass is resuming, and leaves the paint alone. */
  bool onFilter(SkPaint& paint) const override;

  /** Plays a picture back through this canvas rather than handing it to
   *  the one below, so a reading draw inside it is fenced like any
   *  other. */
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override;

  /** @name Layer bookkeeping
   *  A layer whose own paint reads the destination is composited by its
   *  restore, so the restore is the draw that has to end the pass; a
   *  layer is a device of its own, covered apart from the one beneath.
   *  These four track which layer is which.
   *  @{ */
  /** Opens a save entry that is no layer and reads nothing. */
  void willSave() override;
  /** Opens a layer entry, recording whether @p rec's paint reads the
   *  destination, and marks the device beneath for a cover when @p rec
   *  reads a backdrop from it. */
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override;
  /** Takes the innermost save entry off, remembering whether it closes a
   *  layer and whether its composite reads the destination. */
  void willRestore() override;
  /** Covers the device a closed layer was composited into when its pass
   *  is resuming, and closes the recording when the composite just
   *  performed read the destination. */
  void didRestore() override;
  /** @} */

 private:
  /** Closes the recording, so everything described so far is one render
   *  pass and everything after it starts another. */
  void fence() const;
  /** Draws a transparent rectangle over the whole of the device draws
   *  land in, unless it has been covered since the recorder last closed a
   *  recording. */
  void cover() const;
  /** How many recordings this library has closed on the context's
   *  recorder; a device last covered at another count is resuming a
   *  pass. */
  uint32_t recordingsClosed() const;

  /** A count no recorder reaches, so a device marked with it is covered
   *  by the next draw that lands in it. */
  static constexpr uint32_t kNeverCovered = UINT32_MAX;

  /** One save the scene has opened and not yet closed. */
  struct Save {
    /** The save opened a layer, a device of its own. */
    bool layer;
    /** The layer's own paint reads the destination, so its composite is
     *  the draw that has to end the pass. */
    bool readsDestination;
  };

  GraphiteContext* m_context;
  SkCanvas* m_target;
  /** False on a backend that reads the destination coherently, where
   *  every draw is forwarded and nothing is ever closed. */
  bool m_fencing;
  /** One entry a save the scene has open, innermost last. */
  std::vector<Save> m_saves;
  /** One entry a device draws can land in — the canvas's own first, then
   *  each open layer — holding the recordings-closed count at which it
   *  was last covered. */
  mutable std::vector<uint32_t> m_coveredAt;
  bool m_restoreReadsDestination = false;
  /** The restore under way closes a layer. */
  bool m_restoreCloses = false;
  /** A reading draw has landed and the next draw must begin a new pass. */
  mutable bool m_pending = false;
  mutable int m_fences = 0;
};

}  // namespace sigil::skia
