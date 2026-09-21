/** @file
 * The box verbs — the air inside and outside a node, the size it asks
 * for, the floors and ceilings around that size, what the size measures,
 * and whether the node has a box at all.
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

namespace {

/** A side an `Edges` leaves unnamed is unstated, and for the air around
 *  a node unstated is zero. */
Dimension orZero(Dimension side) {
  return side.unit == Dimension::Unit::Auto ? Dimension(0.0f) : side;
}

}  // namespace

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Edges edges) {
  declarations()->layout.padding = {orZero(edges.left), orZero(edges.top),
                                    orZero(edges.right), orZero(edges.bottom)};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingTop(Dimension length) {
  declarations()->layout.padding.top = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingRight(Dimension length) {
  declarations()->layout.padding.right = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingBottom(Dimension length) {
  declarations()->layout.padding.bottom = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingLeft(Dimension length) {
  declarations()->layout.padding.left = length;
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

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Edges edges) {
  declarations()->layout.margin = {orZero(edges.left), orZero(edges.top),
                                   orZero(edges.right), orZero(edges.bottom)};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginTop(Dimension length) {
  declarations()->layout.margin.top = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginRight(Dimension length) {
  declarations()->layout.margin.right = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginBottom(Dimension length) {
  declarations()->layout.margin.bottom = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginLeft(Dimension length) {
  declarations()->layout.margin.left = length;
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
Derived& BoxVerbs<Derived>::aspectRatio(float r) {
  declarations()->layout.aspect = r;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::boxSizing(BoxSizing sizing) {
  declarations()->layout.boxSizing = sizing;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::display(Display display) {
  declarations()->layout.display = display;
  return self();
}

template class BoxVerbs<Element>;

}  // namespace sigil::compose
