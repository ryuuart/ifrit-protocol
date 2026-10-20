#pragma once

/** @file
 * Internal to the kernel — what the paint walk carries from a node to the
 * nodes under it: the recording it paints into, the node→root matrix and
 * the box an ink is anchored to, the shared space a node stands in and the
 * plane a hosting node paints its own marks on, the host scale and input
 * every paint context is handed, and the node whose coverage is being
 * traced.
 */

#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>

#include <optional>

#include "paint/RecordingState.h"
#include "sigilcompose/core/Paint.h"

namespace sigil::compose::detail {

struct Instance;

/** THE PAINT WALK'S STATE: one part of the composer's retained state, read
 *  off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct PaintWalk {
  /** Composer::setPointer and setKey: what the host last fed, handed to
   *  every paint context of the frame. */
  SkPoint pointerAt = {0, 0};
  bool pointerPressed = false;
  KeyState keys;
  // The recording the walk is painting into, saved and restored by value
  // by every scope that opens one.
  RecordingState recording;
  // Whether the walk is painting a subtree held at an opacity of 0 only to
  // take the bakes it will blit once it shows (`paint`): every draw lands
  // on a canvas that keeps the matrix and clip and discards the pixels, so
  // nothing painted here may be counted as reaching the canvas.
  bool paintingUnseen = false;
  // Set by a paint that was handed the content scale while a node painted
  // (`PaintContext::contentScaleRead`). What was drawn is then a picture of
  // that one scale: a recording is pinned to it, and a local bake is keyed
  // by the host scale it was taken under.
  bool contentScaleRead = false;
  // The node→root matrix accumulated by paint()'s own recursion — the same
  // walk Query.cpp inverts for hit testing, run forwards. Saved and
  // restored around each paint() frame (RAII, because paint() returns from
  // several places); identity between draws. PaintContext::toRoot is read
  // from here, so every consumer of the material seam sees the SAME matrix
  // the hit test inverts, and a world-space field lands where the hit test
  // says the node is.
  SkMatrix curToRoot = SkMatrix::I();
  // THE BOX THE INK'S PAINT IS ANCHORED TO, accumulated by the same walk:
  // the anchor box's own node→root matrix and its extent. An EMPTY extent
  // is the own-box case — every node maps the paint onto its own box —
  // and is what an ink anchored to a declaring box or to the canvas
  // replaces for the subtree under the node that stated it.
  SkMatrix inkAnchorToRoot = SkMatrix::I();
  SkSize inkAnchorSize = SkSize::MakeEmpty();
  /** The node being painted mapped into that anchor box — what a paint
   *  anchored there samples through. Identity on the own-box case and
   *  wherever the anchor's matrix will not invert, which draws the
   *  anchored paint as an own-box one rather than as nothing. */
  [[nodiscard]] SkMatrix anchorSpace() const {
    if (inkAnchorSize.isEmpty()) return SkMatrix::I();
    SkMatrix fromAnchor;
    if (!inkAnchorToRoot.invert(&fromAnchor)) return SkMatrix::I();
    fromAnchor.preConcat(curToRoot);
    return fromAnchor;
  }
  // The root's LAID-OUT size (canvas px) — differs from `size` under an
  // intrinsic root (snapshot()). Written by paint() at the root frame;
  // PaintContext::rootSize is read from here.
  SkSize rootLayoutSize = SkSize::MakeEmpty();
  /** A SHARED SPACE, open while a hosting node's children are painted or
   *  hit. The canvas stays at the plane the space is drawn on — the
   *  hosting node concatenates nothing for its children — and every node
   *  in the space places itself with `accum · depthMatrixOf` flattened,
   *  relative to that plane. That is what makes the space free of any
   *  inverse: a host turned edge-on has a singular plane of its own and
   *  its children still stand where the space puts them. `rootToPlane` is
   *  the node→root matrix of that plane, which a node in the space builds
   *  its own node→root from. */
  struct Space {
    SkM44 accum;           ///< the plane the space is drawn on → the host
    SkMatrix rootToPlane;  ///< …and that plane's own node→root
  };
  /** The space the node being painted stands in — set by its parent while
   *  that parent hosts one, null otherwise. Saved and restored around each
   *  paint() frame. */
  const Space* curSpace = nullptr;
  /** The plane a HOSTING node's own paint concatenates inside paintContent:
   *  paint() leaves the canvas at the plane the space is drawn on, so the
   *  children can place themselves, and the host's own marks, fill and
   *  content are drawn under this instead. Absent for every other node. */
  std::optional<SkMatrix> curOwnPlane;
  /** …and whether that own plane is drawn at all: a host facing away with
   *  its backface hidden, or turned edge-on, paints nothing of its own and
   *  still paints the children its space holds. */
  bool curOwnHidden = false;
  float hostScale = 1.0f;  // device px per layout px at draw() entry
  /** The node whose coverage is being traced RIGHT NOW, if any.
   *
   *  A coverage boundary is what the node drew, and the node's own marks
   *  are what dress that boundary: drawing them into the trace would make
   *  the boundary a function of itself. So paintContent emits no marks for
   *  this one node while it is set, and asks it for no coverage boundary
   *  either — which is also what keeps the trace from re-entering itself.
   *  Its children, and their marks, are drawn: they are part of what the
   *  node drew, and none of them reads this node's boundary. */
  const Instance* coverageTrace = nullptr;
};

}  // namespace sigil::compose::detail
