#pragma once
/** @file
 * @ingroup weave-geometry
 *
 * THE ESCAPE HATCH TO SKIA: the one header of this library that names the
 * renderer its values stand in for. Nothing else under `sigilweave/` that
 * lays text out includes it; a caller that holds Skia's own values — a
 * path to flow text around, a typeface, a colour — and wants to hand them
 * to the engine, or wants an answer as the Skia value it was computed as,
 * includes this header by name.
 *
 * The painter seam is not here: `paint`, `drawBatched` and the testing
 * plates draw ON a Skia canvas, which is what they are for, and say so in
 * their own headers. What stands here is each entrance whose Weave form
 * differs from its Skia form only in the type it takes or answers.
 */
#include <include/core/SkFontStyle.h>
#include <include/core/SkPath.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>

#include <memory>

#include "sigilweave/style/Face.h"

namespace sigil::weave {

/** @name Faces
 *  A Skia typeface as a `Face`, and back: the same typeface, shared, not
 *  copied. With this header included a Skia typeface stands wherever a
 *  face is taken, and a face converts wherever a Skia typeface is.
 *  @{ */
template <>
struct FaceAdapter<sk_sp<SkTypeface>> {
  static Face wrap(sk_sp<SkTypeface> native);
  static sk_sp<SkTypeface> unwrap(const Face& face);
  static SkTypeface* borrow(const Face& face);
};

/** @p face as Skia's typeface; null for no face. */
inline sk_sp<SkTypeface> toSk(const Face& face) {
  return FaceAdapter<sk_sp<SkTypeface>>::unwrap(face);
}
/** The Skia typeface @p face holds, BORROWED: no reference is taken, so
 *  it is valid exactly as long as the face stands. For a question asked of
 *  the typeface on a warm path, where a reference would be counted up and
 *  down for nothing. Null for no face. */
inline SkTypeface* borrowSk(const Face& face) {
  return FaceAdapter<sk_sp<SkTypeface>>::borrow(face);
}
/** Skia's typeface as a `Face`; no face for null. */
inline Face fromSk(sk_sp<SkTypeface> typeface) {
  return FaceAdapter<sk_sp<SkTypeface>>::wrap(std::move(typeface));
}
/** The weight, width and slant as Skia's font style. */
SkFontStyle toSk(const FaceStyle& style);
/** Skia's font style as a weight, width and slant. */
FaceStyle fromSk(const SkFontStyle& style);
/** @} */

}  // namespace sigil::weave

namespace sigil::weave {
class FlowShape;
}  // namespace sigil::weave

namespace sigil::weave::flowshape {

/** A filled Skia path as a flow shape, on the terms `path(Outline)` reads
 *  an outline: an inverse fill is read as its own non-inverse self. */
[[nodiscard]] std::shared_ptr<FlowShape> path(const SkPath& path);

}  // namespace sigil::weave::flowshape
