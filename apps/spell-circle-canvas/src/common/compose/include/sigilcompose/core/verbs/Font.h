#pragma once

/** @file
 * @ingroup compose-core
 *
 * The font and the ink, as verbs: the partial every passage under a node
 * is set in, CSS's longhands for the fields CSS names, and the colour or
 * paint the text and every mark naming no colour is painted in.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/PaintAnchor.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Var.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/Type.h>

#include <optional>

namespace sigil::compose {

/** THE FONT AND THE INK, which flow down the tree from the node that
 *  states them to every passage under it. `font()` is the shorthand over
 *  the whole partial; each longhand writes one field of it, so
 *  `fontSize(18)` and `font({.size = 18})` are one statement. */
template <class Derived>
class FontVerbs {
 public:
  /** THE FONT EVERYTHING UNDER THIS NODE IS SET IN, as a PARTIAL: the
   *  fields @p partial names override the inherited font and the rest
   *  inherit. Written twice, the later call wins field by field. A
   *  relative size resolves against the PARENT's font. */
  Derived& font(sigil::weave::Type partial);
  /** THE FACE — CSS `font-family`, as the face itself. A null face is
   *  the font context's default family. The `face` field of `font()`. */
  Derived& fontFamily(sk_sp<SkTypeface> face);
  /** THE TYPE SIZE — CSS `font-size`. Pixels when bare; `em` and `rem`
   *  resolve against the parent's and the root's size. The `size` field
   *  of `font()`. 16 px when nothing states one. */
  Derived& fontSize(sigil::weave::Length size);
  /** THE WEIGHT — CSS `font-weight`, as the face's `wght` axis: 400
   *  regular, 700 bold. 0 is the face's own weight, stated. The `weight`
   *  field of `font()`. */
  Derived& fontWeight(float weight);
  /** THE LEAN — CSS `font-style`'s oblique, as the face's `slnt` axis in
   *  degrees, NEGATIVE leaning right as OpenType counts it. 0 is
   *  upright. The `slant` field of `font()`. */
  Derived& fontStyle(float slant);
  /** THE TRACKING added after each cluster — CSS `letter-spacing`.
   *  Pixels when bare; `em()` is a fraction of the size the type resolves
   *  to. The `track` field of `font()`. */
  Derived& letterSpacing(sigil::weave::Length tracking);
  /** THE INK: the colour text under this node is set in, and every
   *  mark that names no colour is painted in — CSS's `color`.
   *  `Fill::currentInk()` reads it back. A node whose ink changes under
   *  a `transition()` eases it however the change was written — this
   *  verb, a class, a rule, a custom property. A node under it with no
   *  transition of its own follows the ramp; one with its own eases the
   *  inherited ink over its own duration. */
  Derived& ink(material::Color colour);
  /** The ink read from a custom property in force here. A property
   *  nobody set, or one holding a length, leaves the inherited ink
   *  standing and says so once. */
  Derived& ink(VarRef reference);
  /** THE INK AS A WHOLE PAINT — a ramp, a sprite, a recipe, SkSL —
   *  taking everything `fill` takes. A plain colour behaves as the
   *  colour form above does; any other paint inherits the same way but
   *  SNAPS under a transition rather than easing, as a fill does. @p
   *  anchor is the box the paint's unit square maps onto, own box by
   *  default, which for a text leaf is its text-metric box. @p unit
   *  restarts the paint on each `Glyph`, `Cluster`, `Word`, `Line` or
   *  `Sentence` of a passage, its unit square on that unit's own
   *  text-metric box; absent, the default, is the whole passage.
   *  @trap An empty paint clears an ancestor's ink paint and leaves the
   *  inherited colour standing. A unit is read under `OwnBox` alone, and
   *  `Selection` names no unit: either is dropped with a warning, once,
   *  and the paint is laid whole. */
  Derived& ink(SurfacePaint paint, PaintAnchor anchor = PaintAnchor::OwnBox,
               std::optional<sigil::weave::Unit> unit = std::nullopt);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declare(Property property) {
    return detail::NodeAccess::declare(self(), property);
  }
};

}  // namespace sigil::compose
