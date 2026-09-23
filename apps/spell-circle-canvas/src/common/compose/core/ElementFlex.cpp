/** @file
 * The flex verbs — which way a node lays its children out, whether they
 * wrap, how the room left over is shared, and where everything sits on
 * each axis.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& FlexVerbs<Derived>::flexDirection(FlexDirection direction) {
  declarations()->fields.flexDirection() = direction;
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
  declarations()->fields.flexWrap() = wrap;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexGrow(float f) {
  declarations()->fields.flexGrow() = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexShrink(float f) {
  declarations()->fields.flexShrink() = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::flexBasis(Dimension d) {
  declarations()->fields.flexBasis() = d;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignItems(Align a) {
  declarations()->fields.alignItems() = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::alignSelf(Align a) {
  declarations()->fields.alignSelf() = a;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::justifyContent(Justify j) {
  declarations()->fields.justifyContent() = j;
  return self();
}

template class FlexVerbs<Element>;
template class FlexVerbs<Text>;
template class FlexVerbs<Image>;
template class FlexVerbs<Band>;
template class FlexVerbs<Rule>;

}  // namespace sigil::compose
