#pragma once

/** @file
 * Internal to the kernel — the bakes a node holds between paints: its
 * recorded picture and what that recording is pinned to, its pixel bake and
 * the space, clip and matrix it was taken in, whether the node's own
 * transform or an ancestor's is moving, where its paint last landed on the
 * device, and the stamped-brush and per-mark state its decorations keep.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <limits>

#include "cache/MarkStates.h"
#include "cache/StampCache.h"

namespace sigil::compose::detail {

/** THE BAKES A NODE HOLDS: one part of the retained node, read off the
 *  instance by each field's own name. `Instance::markPaintDirtyUp` is what
 *  drops them. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct HeldBakes {
  sk_sp<SkPicture> picture;
  // What the held recording is pinned to. A recording is a list of draw
  // calls and replays under whatever matrix it meets — EXCEPT where it
  // holds a device-space bake, which is a blit at an absolute device rect
  // and is exact only under the device matrix it was recorded under. So
  // the recording counts the device blits inside it (its own nodes' and
  // those of every held picture replayed into it) and keeps that matrix;
  // a count above zero and a different matrix at replay remake it. A
  // recording holding none stays matrix-independent, which is what lets a
  // picture under a live transform replay under the motion.
  //
  // `pictureDeviceDeferred`: a node inside was refused the device bake
  // for matrix motion alone. The recording is retaken once the matrix has
  // held still for a frame, so the node takes the device bake then rather
  // than keeping the resampled local one until its content next changes.
  SkMatrix pictureMatrix = SkMatrix::I();
  // …and the device clip the blits inside it were cut to. An ink clip is
  // a set of device pixels, so a recording holding one is exact for the
  // clip it was made under and stale under a wider one: a window that
  // grew would replay the region it held when it was smaller.
  SkIRect pictureDeviceClip = SkIRect::MakeEmpty();
  uint32_t pictureDeviceBakes = 0;
  bool pictureDeviceDeferred = false;
  // A group inside awaits its settling visit. Unlike a deferred device
  // bake, that visit does not require the ancestor matrix to hold still.
  bool pictureGroupPending = false;
  // …and THE WINDOW OF HOST SCALES the rasters inside it are the same
  // picture over. A traced silhouette is a staircase of whole device
  // pixels and a local texture bake is a texel grid the blit stretches
  // across the node's own units: both are measured off the scale the
  // recording was taken at, and both are answers a recording can hold
  // that are not matrix-independent for a reason no blit explains. A node
  // painted every frame notices and re-traces or re-bakes; a node inside
  // a held recording is never asked again, so the recording carries the
  // window. It is narrowed by its own nodes' rasters and by those of
  // every held picture replayed into it — a point for a trace, the ladder
  // rung's own span for a local bake — so a recording holding neither
  // keeps the whole line and is never remade for the scale alone.
  float pictureScaleLo = 0.0f;
  float pictureScaleHi = std::numeric_limits<float>::infinity();
  sk_sp<SkImage> textureImage;
  float textureScale = 1.0f;
  // The host scale a LOCAL bake was taken under, where a paint in it read
  // the content scale; zero where none did. Such a bake is a picture of
  // that one scale, and is remade when the host leaves it.
  float textureContentScale = 0.0f;
  SkRect textureBakeRect = SkRect::MakeEmpty();  // bake covers paint bounds
  // …and the device clip that bake was CUT TO. Every device bake carries
  // the canvas's own clip into its layer, so what it holds is not the
  // node's paint but the node's paint as that clip left it — and a clip
  // narrows for reasons that have nothing to do with the node's content:
  // an ancestor's layer opens over the node's box while it fades in, a
  // panel reveals, a window grows. A bake taken under the narrow clip and
  // held past it blits the CUT, and the marks the clip removed never come
  // back, because nothing else the staleness rules compare has moved: the
  // paint bounds are the same bounds whatever the clip did to them.
  SkIRect textureBakeClip = SkIRect::MakeEmpty();
  // …AND THE MATRIX IT WAS DRAWN UNDER. The rect a device bake is measured
  // to is whole device pixels, so a matrix that moves BELOW one leaves that
  // rect where it was — and the picture inside it is not the same picture:
  // an edge lands on another part of its pixel, a glyph takes another
  // subpixel phase. A scale settling by a thousandth at the end of an
  // entrance does it, and so does a node drifting a fraction of a pixel. A
  // bake held across that blits the frame it was taken on, because nothing
  // else the staleness rules compare has moved.
  SkMatrix textureBakeMatrix = SkMatrix::I();
  // Which SPACE the held bake lives in, and therefore how it must be
  // blitted: a device-space bake is snapped to whole device pixels and
  // drawn with the matrix reset (a literal copy, at any angle); a local
  // one is drawn through the node's transform and resampled by it. The
  // flag exists so a bake taken in one mode is never blitted in the other
  // — the two store different rectangles in textureBakeRect.
  bool textureDeviceSpace = false;
  // Does the held bake still OWE its layer effect, to be applied at the
  // blit? True only under the moving-effect tier, where the bake holds
  // unfiltered content. The blit dresses itself from the tier the CURRENT
  // frame takes, so a bake made under the other answer would be filtered
  // twice or not at all; a flip re-bakes, exactly as a change of space
  // does.
  bool textureEffectDeferred = false;
  /** THE FILTERED BACKDROP, HELD: the pixels a backdrop filter's pass left
   *  on the device, over the rect its layer covered, and what they were
   *  taken under — everything painted beneath (`PaintWalk::paintedBeneath`),
   *  the device matrix, the device clip, the description, the box and the
   *  outline the pass was cut to. A
   *  draw that finds all of them as they were copies the pixels back
   *  instead of filtering again. */
  struct HeldBackdrop {
    sk_sp<SkImage> pixels;
    SkIRect source = SkIRect::MakeEmpty();  ///< their rect in that image
    SkIRect rect = SkIRect::MakeEmpty();  ///< where they go, in global pixels
    uint64_t beneath = 0;
    SkMatrix matrix = SkMatrix::I();
    SkIRect clip = SkIRect::MakeEmpty();
    const void* description = nullptr;
    SkRect box = SkRect::MakeEmpty();
    SkPath shape;    ///< the outline it was cut to, for a shaped node
    SkRRect corners;  ///< …and for any other
  };
  HeldBackdrop heldBackdrop;
  // Is this node's OWN transform animating? (Geometric slots only — opacity
  // does not move the device rect.) A device-space bake is exact but is
  // pinned to one device rect, so it re-bakes whenever the node moves;
  // the quantized local bake exists precisely so a spinning ornament bakes
  // ONCE and rides its transform. Computed in computeVolatile, which runs
  // every frame whether or not the node is repainted — unlike anything
  // derived from paint history, which a node under a cached parent never
  // accumulates.
  bool transformLive = false;
  // …and is an ANCESTOR's transform declared as animating? A local bake
  // rides every matrix above it exactly as it rides the node's own, and a
  // device bake under a moving ancestor is exact at one sub-pixel position
  // only, so a Cache::Texture node keeps its local bake under either
  // declaration. Written beside `transformLive`, by the same walk.
  bool transformLiveAbove = false;
  // …and does what this node draws LAND SOMEWHERE ELSE next frame?
  // `transformLive` is the node's OWN declared motion; this is that, OR any
  // ancestor's, OR — for text — a live textFx() track whose effect moves glyphs
  // off their pen positions. Text asks it: a run whose device placement
  // creeps needs its glyph origins on the subpixel grid, and a figure
  // rotating above the text, or a slide dragging every letter sideways,
  // makes it creep exactly as a marquee phase does. Every term is read off
  // a declaration, so a run that stops keeps the placement it was moving
  // with instead of taking one last shift as it settles.
  bool placementUnderMotion = false;
  // …and does everything the subtree paints hold still IN ITS OWN FRAME?
  // True where what moves its pixels is its placement alone: this node's
  // own transform or one above it, and a root-anchored paint inside that
  // follows the placement. Nothing below it moves, fades, ticks or reads
  // what lies beneath. Such a node, its own transform moving, is the one a
  // live composer holds as pixels while it moves. Written beside
  // `transformLive`, by the same walk.
  bool stillInOwnFrame = false;
  // THE PIXELS HELD WHILE THE NODE'S OWN TRANSFORM OR OPACITY MOVES, under
  // a live composer. Where the node moves, the subtree painted once in the
  // node's own frame at `movingScale` device pixels a unit, covering
  // `movingBounds`, and composited under each new matrix. Where only its
  // opacity moves, the subtree painted IN PLACE, on the device grid at
  // `movingDeviceRect` under `movingMatrix` and `movingClip`, and
  // composited there at each new opacity: the pixels a layer opened for
  // the fade would hold. Dropped the draw the node holds still, and by
  // every change that stales its paint.
  sk_sp<SkImage> movingImage;
  float movingScale = 0.0f;
  SkRect movingBounds = SkRect::MakeEmpty();
  bool movingInPlace = false;
  SkIRect movingDeviceRect = SkIRect::MakeEmpty();
  SkMatrix movingMatrix = SkMatrix::I();
  SkIRect movingClip = SkIRect::MakeEmpty();
  // The opacity the node composited at in the last draw, and whether it
  // has drawn at all: a moved opacity is one the held pixels can carry.
  float lastOpacity = 1.0f;
  bool opacitySeen = false;
  // …and is the device rect it actually LANDS on holding still? These are
  // NOT the same predicate: a node with no animated property of its own
  // still moves every frame under a resizing window or a pinch zoom, and a
  // device-pinned bake would re-bake each time — losing exactly the
  // one-bake-per-quantized-step reuse the local bake exists to provide.
  //
  // AN INVARIANT OF THIS PAIR, NOT OF ANY ONE CALL SITE: it is written only
  // at recording.depth == 0, where the node is painted every frame and so
  // actually accumulates frame-over-frame history. A node painted into an
  // ancestor's recording is painted once per recording and accumulates
  // none; a rect it observed there would poison the compare and force a
  // spurious re-bake on its next live frame. Inside a recording the
  // "holding still" verdict is the outermost recording's instead
  // (`lastDeviceMatrix` below, read through the painter's
  // recording.matrixStable), which that recording's node keeps every frame
  // for the same reason. The writers, each behind the guard: the
  // Cache::Group device bake and the Cache::Texture device bake, both in
  // paint(). Any new writer must sit behind the same check.
  //
  // The first sighting counts as stable: a node's first frame is otherwise
  // forced down the local path and then re-bakes on its second, which is a
  // re-bake the common case (a host that never changes scale) should never
  // pay.
  SkIRect lastDeviceRect = SkIRect::MakeEmpty();
  bool deviceRectSeen = false;
  // The matrix this node's draws reached the device through last frame,
  // under the same invariant: written only at recording.depth == 0. Read by
  // the node's own recording, to know whether the device bakes it holds
  // are still at the rects they were baked for and whether a node inside
  // it may take one.
  SkMatrix lastDeviceMatrix = SkMatrix::I();
  bool deviceMatrixSeen = false;
  float bakedLeafOpacity = 1.0f;  // frozen into the recording
  SkBlendMode bakedLeafBlend = SkBlendMode::kSrcOver;
  bool paintDirty = true;
  // Whether `paintDirty` was raised on a clean node by a change of bake
  // density alone. What a node covers does not depend on the density, so
  // such a node keeps its coverage trace; every other invalidation lowers
  // this.
  bool paintDirtyOnlyForDensity = false;

  // Stamped-brush bakes live with the NODE (handed to decorations via
  // PaintContext::stamps), so a brush value rebuilt every describe reuses
  // its art's bake instead of re-rastering it.
  StampCache stampCache;
  // What each mark keeps between paints — a lit keyline's prepared
  // surface, a bevel's shoulder — by its place among the node's marks,
  // handed to the mark through PaintContext::markState.
  MarkStates markStates;
};

}  // namespace sigil::compose::detail
