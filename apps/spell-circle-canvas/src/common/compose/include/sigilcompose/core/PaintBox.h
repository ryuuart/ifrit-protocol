#pragma once

/** @file
 * @ingroup compose-core
 *
 * Which rectangle a surface paint's unit square is stretched over.
 */

#include <cstdint>

namespace sigil::compose {

/** WHICH RECTANGLE A PAINT'S UNIT SQUARE IS STRETCHED OVER: a box of the
 *  element, a box of the tree, or each unit of a passage. `fill` and
 *  `ink` take it, and so do a rule's and a span's `ink`; the default is
 *  `Element`.
 *  @trap `fill` refuses the five text units, and `ink` on a node that is
 *  no passage ignores them; either says so once and paints as `Element`.
 *  A fill does not inherit, so `Subtree` on a fill is `Element`. */
enum class PaintBox : uint8_t {
  /** The element's own box — for a passage, its text box: x across the
   *  widest line, y from the first line's cap top to the last line's
   *  baseline, which is where a ramp has to land for chrome type to
   *  read. Under an inherited ink, each thing painted is stretched over
   *  its own. */
  Element,
  /** Inside the element's border — CSS's padding-box origin. */
  Padding,
  /** Inside the element's border and padding — CSS's content-box
   *  origin. */
  Content,
  /** The box of the element that STATED the paint; everything under it
   *  shows its own slice of the one paint. */
  Subtree,
  /** The whole canvas; elements anywhere in the tree show slices of one
   *  paint, and moving one of them moves which slice it shows. */
  Canvas,
  /** Each glyph of a passage, the paint restarting on every one. */
  Glyph,
  /** Each cluster: a base and the marks set on it take one square. */
  Cluster,
  /** Each word. */
  Word,
  /** Each line. */
  Line,
  /** Each sentence. */
  Sentence,
};

}  // namespace sigil::compose
