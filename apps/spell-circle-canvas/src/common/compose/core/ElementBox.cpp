/** @file
 * The box verbs — the air inside and outside a node, the size it asks
 * for, and the floors and ceilings around that size.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& BoxVerbs<Derived>::gap(Dimension length) {
  declarations()->layout.gap = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension all) {
  declarations()->layout.padding = {all, all, all, all};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension h, Dimension v) {
  declarations()->layout.padding = {h, v, h, v};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension l, Dimension t, Dimension r,
                                    Dimension b) {
  declarations()->layout.padding = {l, t, r, b};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension all) {
  declarations()->layout.margin = {all, all, all, all};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension h, Dimension v) {
  declarations()->layout.margin = {h, v, h, v};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension l, Dimension t, Dimension r,
                                   Dimension b) {
  declarations()->layout.margin = {l, t, r, b};
  return self();
}

namespace {

/** A covering node given a size is put back in the flow at that size:
 *  filling the parent's box and holding a box of its own are the two
 *  things it can be, and the size said which. */
void flowFromCover(detail::LayoutProps& layout) {
  if (!layout.covering) return;
  layout.absolute = false;
  layout.hasInsets = false;
  layout.insets = detail::EdgeDims{};
  layout.covering = false;
}

}  // namespace

template <class Derived>
Derived& BoxVerbs<Derived>::width(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.width = d;
  flowFromCover(node->layout);
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::height(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.height = d;
  flowFromCover(node->layout);
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minWidth(Dimension d) {
  declarations()->layout.minWidth = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxWidth(Dimension d) {
  declarations()->layout.maxWidth = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minHeight(Dimension d) {
  declarations()->layout.minHeight = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxHeight(Dimension d) {
  declarations()->layout.maxHeight = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::aspect(float r) {
  declarations()->layout.aspect = r;
  return self();
}

template class BoxVerbs<Element>;

}  // namespace sigil::compose
