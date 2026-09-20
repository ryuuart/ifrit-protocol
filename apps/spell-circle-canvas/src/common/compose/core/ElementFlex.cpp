/** @file
 * The flex verbs — which way a node lays its children out, whether they
 * wrap, how the room left over is shared, and where everything sits on
 * each axis.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& FlexVerbs<Derived>::flexDirection(FlexDirection direction) {
  declarations()->layout.direction = direction;
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
  declarations()->layout.wrap = wrap;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexGrow(float f) {
  declarations()->layout.grow = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexShrink(float f) {
  declarations()->layout.shrink = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexBasis(Dimension d) {
  declarations()->layout.basis = d;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignItems(Align a) {
  declarations()->layout.alignItems = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignSelf(Align a) {
  declarations()->layout.alignSelf = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::justifyContent(Justify j) {
  declarations()->layout.justify = j;
  return self();
}

template class FlexVerbs<Element>;

}  // namespace sigil::compose
