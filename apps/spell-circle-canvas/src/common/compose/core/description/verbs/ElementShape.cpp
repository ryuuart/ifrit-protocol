/** @file
 * The shape verbs — the corner radii, the silhouette that overrides
 * them, and the clip to either.
 */

#include "description/ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& ShapeVerbs<Derived>::borderRadius(Corners c) {
  declarations()->fields.borderRadius() = c;
  return self();
}

template <class Derived>
Derived& ShapeVerbs<Derived>::shape(Shape path) {
  declarations()->shape() = std::move(path);
  return self();
}

template <class Derived>
Derived& ShapeVerbs<Derived>::overflow(Overflow overflow) {
  // Hidden is Clip: a tree that never scrolls has nothing that reaches
  // what is hidden, so the two cut alike.
  declarations()->fields.overflow() = overflow != Overflow::Visible;
  return self();
}

template class ShapeVerbs<Element>;
template class ShapeVerbs<Text>;
template class ShapeVerbs<Image>;
template class ShapeVerbs<Band>;
template class ShapeVerbs<Rule>;

}  // namespace sigil::compose
