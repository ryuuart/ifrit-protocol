/** @file
 * The shape verbs — the corner radii, the silhouette that overrides
 * them, and the clip to either.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& ShapeVerbs<Derived>::corners(Corners c) {
  declarations()->corners = c;
  return self();
}

template <class Derived>
Derived& ShapeVerbs<Derived>::shape(Shape path) {
  declarations()->shapeFn = std::move(path);
  return self();
}

template <class Derived>
Derived& ShapeVerbs<Derived>::clip(bool on) {
  declarations()->clipContent = on;
  return self();
}

template class ShapeVerbs<Element>;

}  // namespace sigil::compose
