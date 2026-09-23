/** @file
 * The box verbs — the air inside and outside a node, the size it asks
 * for, the floors and ceilings around that size, what the size measures,
 * and whether the node has a box at all.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& BoxVerbs<Derived>::gap(Dimension length) {
  declarations()->fields.gap() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension all) {
  return padding(all, all, all, all);
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension vertical, Dimension horizontal) {
  return padding(vertical, horizontal, vertical, horizontal);
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension top, Dimension horizontal,
                                    Dimension bottom) {
  return padding(top, horizontal, bottom, horizontal);
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension top, Dimension right,
                                    Dimension bottom, Dimension left) {
  detail::DeclaredFields& fields = declarations()->fields;
  fields.paddingTop() = top;
  fields.paddingRight() = right;
  fields.paddingBottom() = bottom;
  fields.paddingLeft() = left;
  return self();
}

namespace {

/** A side an `Edges` leaves unnamed is unstated, and for the air around
 *  a node unstated is zero. Auto is not a length the air can take, so a
 *  side stated as `autoDimension()` is zero here too. */
Dimension orZero(Dimension side) {
  return side.unit == Dimension::Unit::Auto ? Dimension(0.0f) : side;
}

}  // namespace

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Edges edges) {
  return padding(orZero(edges.top), orZero(edges.right), orZero(edges.bottom),
                 orZero(edges.left));
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingTop(Dimension length) {
  declarations()->fields.paddingTop() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingRight(Dimension length) {
  declarations()->fields.paddingRight() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingBottom(Dimension length) {
  declarations()->fields.paddingBottom() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingLeft(Dimension length) {
  declarations()->fields.paddingLeft() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension all) {
  return margin(all, all, all, all);
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension vertical, Dimension horizontal) {
  return margin(vertical, horizontal, vertical, horizontal);
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension top, Dimension horizontal,
                                   Dimension bottom) {
  return margin(top, horizontal, bottom, horizontal);
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension top, Dimension right,
                                   Dimension bottom, Dimension left) {
  detail::DeclaredFields& fields = declarations()->fields;
  fields.marginTop() = top;
  fields.marginRight() = right;
  fields.marginBottom() = bottom;
  fields.marginLeft() = left;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Edges edges) {
  return margin(orZero(edges.top), orZero(edges.right), orZero(edges.bottom),
                orZero(edges.left));
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginTop(Dimension length) {
  declarations()->fields.marginTop() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginRight(Dimension length) {
  declarations()->fields.marginRight() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginBottom(Dimension length) {
  declarations()->fields.marginBottom() = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginLeft(Dimension length) {
  declarations()->fields.marginLeft() = length;
  return self();
}

// A covering node given a size is put back in the flow at that size:
// filling the parent's box and holding a box of its own are the two things
// it can be, and the size said which.

template <class Derived>
Derived& BoxVerbs<Derived>::width(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  fields.width() = d;
  fields.leaveCover();
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::height(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  fields.height() = d;
  fields.leaveCover();
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minWidth(Dimension d) {
  declarations()->fields.minWidth() = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxWidth(Dimension d) {
  declarations()->fields.maxWidth() = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minHeight(Dimension d) {
  declarations()->fields.minHeight() = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxHeight(Dimension d) {
  declarations()->fields.maxHeight() = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::aspectRatio(float r) {
  declarations()->fields.aspectRatio() = r;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::boxSizing(BoxSizing sizing) {
  declarations()->fields.boxSizing() = sizing;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::display(Display display) {
  declarations()->fields.display() = display;
  return self();
}

template class BoxVerbs<Element>;
template class BoxVerbs<Text>;
template class BoxVerbs<Image>;
template class BoxVerbs<Band>;
template class BoxVerbs<Rule>;

}  // namespace sigil::compose
