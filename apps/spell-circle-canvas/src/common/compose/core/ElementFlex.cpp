/** @file
 * The flex verbs — which way a node lays its children out, whether they
 * wrap, how the room left over is shared, and where everything sits on
 * each axis.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& FlexVerbs<Derived>::row() {
  declarations()->layout.row = true;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::column() {
  declarations()->layout.row = false;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::wrapLines(bool on) {
  declarations()->layout.wrap = on;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::grow(float f) {
  declarations()->layout.grow = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::shrink(float f) {
  declarations()->layout.shrink = f;
  return self();
}

template <class Derived>
Derived& FlexVerbs<Derived>::basis(Dimension d) {
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
Derived& FlexVerbs<Derived>::justify(Justify j) {
  declarations()->layout.justify = j;
  return self();
}

template class FlexVerbs<Element>;

}  // namespace sigil::compose
