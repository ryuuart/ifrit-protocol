/** @file
 * The flex verbs — which way a node lays its children out, whether they
 * wrap, how the room left over is shared, and where everything sits on
 * each axis.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& FlexVerbs<Derived>::flexDirection(FlexDirection direction) {
  declare(Property::FlexDirection)->layout.direction = direction;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::row() {
  return flexDirection(FlexDirection::Row);
}

template <class Derived>
Derived& FlexVerbs<Derived>::column() {
  return flexDirection(FlexDirection::Column);
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexWrap(FlexWrap wrap) {
  declare(Property::FlexWrap)->layout.wrap = wrap;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexGrow(float f) {
  declare(Property::FlexGrow)->layout.grow = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexShrink(float f) {
  declare(Property::FlexShrink)->layout.shrink = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexBasis(Dimension d) {
  declare(Property::FlexBasis)->layout.basis = d;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignItems(Align a) {
  declare(Property::AlignItems)->layout.alignItems = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignSelf(Align a) {
  declare(Property::AlignSelf)->layout.alignSelf = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::justifyContent(Justify j) {
  declare(Property::JustifyContent)->layout.justify = j;
  return self();
}

template class FlexVerbs<Element>;
template class FlexVerbs<Text>;
template class FlexVerbs<Image>;
template class FlexVerbs<Band>;

}  // namespace sigil::compose
