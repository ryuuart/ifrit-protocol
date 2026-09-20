#pragma once

/** @file
 * @ingroup compose-core
 *
 * The node's own outline, as verbs: the corner radii, the silhouette
 * that overrides them, and the clip to either.
 */

#include <include/core/SkPath.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>

#include <utility>

namespace sigil::compose {

/** THE OUTLINE. It is what the node's fill covers, what a clip clips
 *  to, and what every stroke pass and outline-following decoration
 *  traces. A shape is a REGION and a stroke is a mark on its boundary:
 *  filling this is `fill()`, drawing its edge is `stroke()`. */
template <class Derived>
class ShapeVerbs {
 public:
  /** ROUND THE NODE'S CORNERS, per corner, in pixels — CSS
   *  `border-radius`. Square when unstated, and overridden outright by
   *  `shape()`. It is the cheap path: a rounded box clips and strokes
   *  as a round rect where a general shape has to build a path. */
  Derived& corners(Corners c);
  /** THE NODE'S SHAPE: a path generator over its laid-out size, in
   *  local coordinates. Overrides `corners()`. Every `shapes::`
   *  generator is a comparable value, so a shaped node prunes exactly
   *  like an unshaped one; a raw callable never compares equal and its
   *  node re-records on every describe. */
  Derived& shape(Shape path);
  /** THE KEYED SPELLING: the generator plus the value it closes over,
   *  so the node settles. A path already cooked wants
   *  `shape(heldPath(p))` instead. */
  template <typename K, typename F>
    requires core::PrefixCallable<const F&, SkPath(SkSize)>
  Derived& shape(K key, F fn) {
    return shape(Shape(keyedShape(std::move(key), std::move(fn))));
  }
  /** Clip fill, content and children to the node's shape. Decorations
   *  are NOT clipped — they dress the outline, so outer strokes,
   *  shadows and glows keep their reach — and hit-testing still bounds
   *  the subtree. */
  Derived& clip(bool on = true);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
