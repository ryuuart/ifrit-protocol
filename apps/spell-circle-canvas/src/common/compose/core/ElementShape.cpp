/** @file
 * The shape verbs — the corner radii, the silhouette that overrides
 * them, and the clip to either.
 */

#include "ComposeInternal.h"

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
  declarations()->fields.overflow() = overflow == Overflow::Clip;
  return self();
}

template class ShapeVerbs<Element>;
template class ShapeVerbs<Text>;
template class ShapeVerbs<Image>;
template class ShapeVerbs<Band>;
template class ShapeVerbs<Rule>;

}  // namespace sigil::compose
