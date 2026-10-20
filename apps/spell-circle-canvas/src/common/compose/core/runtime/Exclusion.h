#pragma once

/** @file
 * Internal to the kernel — one resolved `contentFlowAround` target, the
 * value a text node's flow subtracts from the room its lines run in.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

namespace sigil::compose::detail {

/** ONE RESOLVED `contentFlowAround` TARGET, in the text node's own space.
 *
 *  A target that resolves a SILHOUETTE of its own — a `shape()`, a routed
 *  connector or rail — is subtracted by that outline, concavities and holes
 *  included, so text runs into the notch of a star and through the hole of
 *  a ring. A target that resolves none is subtracted by its BOX, which is
 *  the whole of what it occupies. `circle` is the analytic case: a round
 *  silhouette costs one square root per line band instead of a walk of a
 *  flattened outline, and the answer is the same one.
 *
 *  Both forms are one value so the compare that decides whether the text
 *  must be laid out again stays a plain vector compare. */
struct Exclusion {
  SkRect bounds = SkRect::MakeEmpty();  ///< the target's box (or the circle's)
  SkPath path;          ///< the silhouette; empty when the target has none
  bool circle = false;  ///< `bounds`'s inscribed circle IS the silhouette
  bool operator==(const Exclusion& other) const {
    return bounds == other.bounds && circle == other.circle &&
           path == other.path;
  }
};

}  // namespace sigil::compose::detail
