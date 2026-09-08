/** @file
 * The picture tier: the node's own paint recorded into a replayable
 * command list behind the kernel's bake seam, what a recording may hold,
 * and the live draw a node that records nothing takes instead.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRect.h>
#include <sigilcore/cache/Bake.h>
#include <sigilmeasure/time/Stopwatch.h>

#include <utility>

#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

// The picture tier, behind the bake seam

void PictureBake::take(PictureBakeTarget& t) const {
  t.painter->recordPicture(*t.inst, t.deviceMatrix, t.deviceClip,
                           t.matrixStable, t.hostScale, t.leafBlend,
                           t.leafOpacity, std::move(*t.scalars));
}
void PictureBake::replay(PictureBakeTarget& t) const {
  t.canvas->drawPicture(t.inst->picture);
  // A held picture replayed into an enclosing recording hands that
  // recording the device blits it holds: the outer picture is now pinned
  // to the same matrix, and remade with this one when it changes.
  t.painter->recordingDeviceBakes += t.inst->pictureDeviceBakes;
  t.painter->recordingDeviceDeferred |= t.inst->pictureDeviceDeferred;
  // …and the traced silhouettes it holds, which pin the outer recording to
  // the scale they were traced at exactly as the blits pin it to a matrix.
  t.painter->recordingCoverageTraces += t.inst->pictureCoverageTraces;
}
void PictureBake::drop(PictureBakeTarget& t) const { t.inst->picture.reset(); }
bool PictureBake::held(const PictureBakeTarget& t) const {
  return (bool)t.inst->picture;
}

void Composer::Impl::recordPicture(Instance& inst, const SkMatrix& deviceMatrix,
                                   const SkIRect& deviceClip, bool matrixStable,
                                   float hostScale, SkBlendMode leafBlend,
                                   float leafOpacity,
                                   Instance::ContentScalars&& scalars) {
  // The same rect the layers and bakes use. Its job HERE is only to be an
  // honest bounds advertisement (SkPicture::cullRect) — this path attaches
  // no BBH, so nothing is culled against it either at record or at
  // playback; see the note on ownPaintBounds for the measurement.
  const SkRect cull = recordBounds(inst);
  SkPictureRecorder recorder;
  SkCanvas* rec = recorder.beginRecording(cull);
  // WHAT A RECORDING MAY HOLD. A picture is a list of draw calls and
  // replays under whatever matrix it meets, so everything inside one must
  // be matrix-independent — with one precise exception. A device-space
  // bake is a blit at an absolute device rect, exact at any angle and
  // wrong under any other matrix. It may be recorded when every recording
  // it stands inside is PINNED: made under a matrix that is stamped on the
  // instance, replayed only under that matrix, and remade the frame it
  // differs. A recording is pinnable when nothing above it moves by
  // declaration — no live transform on this node or an ancestor, no
  // shared space whose view is live — because a pinned recording under
  // motion would be remade every frame, which is the one cost the picture
  // tier exists to avoid. Under a declared motion the recording stays
  // UNPINNED and its contents keep the matrix-independent rule; the
  // coverage trace, an offscreen raster at a scale of its own, is unpinned
  // for the same reason.
  //
  // A pinned recording with no device blit inside is still
  // matrix-independent and is never remade for the matrix alone. So the
  // count is kept, not a flag: it is the number of device blits the
  // recording holds, its own nodes' and those of every held picture
  // replayed into it.
  const bool unpinned = inst.placementUnderMotion;
  // The outermost recording's node is painted every frame, so its verdict
  // on the matrix is a history; a nested one inherits it.
  if (recordingDepth == 0) recordingMatrixStable = matrixStable;
  const SkMatrix outerReplay = recordingReplay;
  const SkMatrix outerReplayInverse = recordingReplayInverse;
  // The ops recorded here reach the device through this node's matrix —
  // already composed out through every enclosing recording.
  recordingReplay = deviceMatrix;
  const bool invertible = recordingReplay.invert(&recordingReplayInverse);
  const uint32_t outerBakes = recordingDeviceBakes;
  const bool outerDeferred = recordingDeviceDeferred;
  const uint32_t outerTraces = recordingCoverageTraces;
  recordingDeviceBakes = 0;
  recordingDeviceDeferred = false;
  recordingCoverageTraces = 0;
  ++recordingDepth;
  // A replay matrix with no inverse has no device rect to land a blit on.
  if (unpinned || !invertible) ++unpinnedRecordingDepth;
  paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
  if (unpinned || !invertible) --unpinnedRecordingDepth;
  --recordingDepth;
  inst.picture = recorder.finishRecordingAsPicture();
  inst.pictureMatrix = deviceMatrix;
  inst.pictureDeviceClip = deviceClip;
  inst.pictureDeviceBakes = recordingDeviceBakes;
  inst.pictureDeviceDeferred = recordingDeviceDeferred;
  inst.pictureCoverageTraces = recordingCoverageTraces;
  inst.pictureHostScale = hostScale;
  // What this recording holds, the enclosing one now holds too.
  recordingDeviceBakes = outerBakes + inst.pictureDeviceBakes;
  recordingDeviceDeferred = outerDeferred || inst.pictureDeviceDeferred;
  recordingCoverageTraces = outerTraces + inst.pictureCoverageTraces;
  recordingReplay = outerReplay;
  recordingReplayInverse = outerReplayInverse;
  inst.bakedLeafOpacity = leafOpacity;  // a settled transition re-bakes
  inst.bakedLeafBlend = leafBlend;      // (the recording froze them in)
  inst.bakedLiveShader =
      inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
  inst.bakedScalars = std::move(scalars);
  inst.paintDirty = false;
  stats.picturesRecorded++;
}

void paintThroughPicture(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkMatrix& totalM = pass.totalM;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;

  // (liveMatOnly bare boxes DO record — the memo's point is replaying
  // the rasterized shader while resolve() stays stable.)
  // (Childless Image leaves deliberately absent: one drawImageRect is
  // cheaper than a nested picture indirection — tile maps stay flat inside
  // their chunk's recording. Cache::Picture opts back in.)
  // The kernel's three-way answer, over this tier's own staleness rule:
  // the node is dirty, a memo's inputs moved, or the leaf paint the
  // recording FROZE IN is not the leaf paint this frame wants.
  PictureBakeTarget target{.painter = &impl,
                           .inst = &inst,
                           .canvas = &canvas,
                           .hostScale = impl.hostScale,
                           .leafBlend = leafBlend,
                           .leafOpacity = leafOpacity,
                           .scalars = &pass.scalarsNow,
                           .deviceMatrix = totalM,
                           .matrixStable = pass.matrixStable,
                           .deviceClip = pass.deviceClip()};
  // …and the pin: a recording holding device blits is exact under the
  // matrix it was made under and is remade under any other; one that
  // deferred a device bake for matrix motion is remade once the matrix
  // has held still for a frame.
  const bool pinMoved = inst.pictureDeviceBakes > 0 &&
                        (totalM != inst.pictureMatrix ||
                         pass.deviceClip() != inst.pictureDeviceClip);
  // …and the scale pin: a recording holding a traced silhouette holds a
  // staircase of whole device pixels, which is a different path once the
  // node is drawn at another scale — a host resized, a plate photographed
  // at its oversample. Nothing else the tier compares moves with it: the
  // node's content is the same content whatever scale it is drawn at, so a
  // recording kept across the change would replay the coarser boundary and
  // the decorations that dress it would never find the finer one.
  const bool scalePinMoved =
      inst.pictureCoverageTraces > 0 && inst.pictureHostScale != impl.hostScale;
  const bool deferredDue = inst.pictureDeviceDeferred && pass.matrixStable;
  if (core::decideBake({.cacheable = true,
                        .held = impl.pictureBake->held(target),
                        .stale = inst.paintDirty || pass.memoStale ||
                                 inst.bakedLeafOpacity != leafOpacity ||
                                 inst.bakedLeafBlend != leafBlend || pinMoved ||
                                 scalePinMoved || deferredDue}) ==
      core::BakeAction::Take)
    impl.pictureBake->take(target);
  if (pass.profile.row != SIZE_MAX)
    impl.profileRows[pass.profile.row].cacheState =
        Composer::CacheState::Picture;
  // The measurement that drives promotion: what the replay of this
  // node's recording cost, which is what the tier is choosing against.
  const measure::Stopwatch replayWatch;
  pass.profDraw("replay", [&] { impl.pictureBake->replay(target); });
  pass.accrue(replayWatch.elapsedMs());
}

void paintLive(PaintPass& pass) {
  Composer::Impl& impl = pass.impl;
  Instance& inst = pass.inst;
  SkCanvas& canvas = pass.canvas;
  const SkBlendMode leafBlend = pass.leafBlend;
  const float leafOpacity = pass.leafOpacity;

  if (!impl.coverageTrace) impl.stats.nodesPainted++;
  // A LEAF never records a picture — one draw call beats a nested
  // recording — so without this it would never be timed at all, and the
  // most expensive single object a scene can hold, a full-canvas box
  // carrying one shader, would be structurally invisible to the promoter.
  // So the live draw is timed too, but ONLY for a node that could
  // actually be promoted: that keeps two clock reads per frame off every
  // ineligible node in the tree, of which there are usually thousands.
  if (!pass.promotable) {
    pass.profDraw("live", [&] {
      impl.paintContent(inst, canvas, impl.hostScale, leafBlend, leafOpacity);
    });
  } else {
    const measure::Stopwatch liveWatch;
    pass.profDraw("live", [&] {
      impl.paintContent(inst, canvas, impl.hostScale, leafBlend, leafOpacity);
    });
    pass.accrue(liveWatch.elapsedMs());
  }
  inst.paintDirty = false;
}

}  // namespace sigil::compose
