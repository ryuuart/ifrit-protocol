/** @file
 * The box verbs — the air inside and outside a node, the size it asks
 * for, the floors and ceilings around that size, what the size measures,
 * and whether the node has a box at all.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& BoxVerbs<Derived>::gap(Dimension length) {
  declare(Property::Gap)->layout.gap = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension all) {
  declare({Property::PaddingTop, Property::PaddingRight,
           Property::PaddingBottom, Property::PaddingLeft})
      ->layout.padding = {all, all, all, all};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension vertical, Dimension horizontal) {
  declare({Property::PaddingTop, Property::PaddingRight,
           Property::PaddingBottom, Property::PaddingLeft})
      ->layout.padding = {horizontal, vertical, horizontal, vertical};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension top, Dimension horizontal,
                                    Dimension bottom) {
  declare({Property::PaddingTop, Property::PaddingRight,
           Property::PaddingBottom, Property::PaddingLeft})
      ->layout.padding = {horizontal, top, horizontal, bottom};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::padding(Dimension top, Dimension right,
                                    Dimension bottom, Dimension left) {
  declare({Property::PaddingTop, Property::PaddingRight,
           Property::PaddingBottom, Property::PaddingLeft})
      ->layout.padding = {left, top, right, bottom};
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
  declare({Property::PaddingTop, Property::PaddingRight,
           Property::PaddingBottom, Property::PaddingLeft})
      ->layout.padding = {orZero(edges.left), orZero(edges.top),
                          orZero(edges.right), orZero(edges.bottom)};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingTop(Dimension length) {
  declare(Property::PaddingTop)->layout.padding.top = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingRight(Dimension length) {
  declare(Property::PaddingRight)->layout.padding.right = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingBottom(Dimension length) {
  declare(Property::PaddingBottom)->layout.padding.bottom = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::paddingLeft(Dimension length) {
  declare(Property::PaddingLeft)->layout.padding.left = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension all) {
  declare({Property::MarginTop, Property::MarginRight, Property::MarginBottom,
           Property::MarginLeft})
      ->layout.margin = {all, all, all, all};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension vertical, Dimension horizontal) {
  declare({Property::MarginTop, Property::MarginRight, Property::MarginBottom,
           Property::MarginLeft})
      ->layout.margin = {horizontal, vertical, horizontal, vertical};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension top, Dimension horizontal,
                                   Dimension bottom) {
  declare({Property::MarginTop, Property::MarginRight, Property::MarginBottom,
           Property::MarginLeft})
      ->layout.margin = {horizontal, top, horizontal, bottom};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Dimension top, Dimension right,
                                   Dimension bottom, Dimension left) {
  declare({Property::MarginTop, Property::MarginRight, Property::MarginBottom,
           Property::MarginLeft})
      ->layout.margin = {left, top, right, bottom};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::margin(Edges edges) {
  declare({Property::MarginTop, Property::MarginRight, Property::MarginBottom,
           Property::MarginLeft})
      ->layout.margin = {orZero(edges.left), orZero(edges.top),
                         orZero(edges.right), orZero(edges.bottom)};
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginTop(Dimension length) {
  declare(Property::MarginTop)->layout.margin.top = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginRight(Dimension length) {
  declare(Property::MarginRight)->layout.margin.right = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginBottom(Dimension length) {
  declare(Property::MarginBottom)->layout.margin.bottom = length;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::marginLeft(Dimension length) {
  declare(Property::MarginLeft)->layout.margin.left = length;
  return self();
}

namespace {

/** A covering node given a size is put back in the flow at that size:
 *  filling the parent's box and holding a box of its own are the two
 *  things it can be, and the size said which. The placement `cover()`
 *  declared is UNDECLARED with it — the node states nothing about where
 *  it sits any more, and a mask that still said so would make it unequal
 *  to a node that never covered. */
void flowFromCover(detail::ElementNode* node) {
  detail::LayoutProps& layout = node->layout;
  if (!layout.covering) return;
  layout.absolute = false;
  layout.hasInsets = false;
  layout.insets = detail::EdgeDims{};
  layout.covering = false;
  for (Property side : {Property::Absolute, Property::Left, Property::Top,
                        Property::Right, Property::Bottom})
    node->declared.clear(side);
}

}  // namespace

template <class Derived>
Derived& BoxVerbs<Derived>::width(Dimension d) {
  detail::ElementNode* node = declare(Property::Width);
  node->layout.width = d;
  flowFromCover(node);
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::height(Dimension d) {
  detail::ElementNode* node = declare(Property::Height);
  node->layout.height = d;
  flowFromCover(node);
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minWidth(Dimension d) {
  declare(Property::MinWidth)->layout.minWidth = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxWidth(Dimension d) {
  declare(Property::MaxWidth)->layout.maxWidth = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::minHeight(Dimension d) {
  declare(Property::MinHeight)->layout.minHeight = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::maxHeight(Dimension d) {
  declare(Property::MaxHeight)->layout.maxHeight = d;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::aspectRatio(float r) {
  declare(Property::AspectRatio)->layout.aspect = r;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::boxSizing(BoxSizing sizing) {
  declare(Property::BoxSizing)->layout.boxSizing = sizing;
  return self();
}

template <class Derived>
Derived& BoxVerbs<Derived>::display(Display display) {
  declare(Property::Display)->layout.display = display;
  return self();
}

template class BoxVerbs<Element>;
template class BoxVerbs<Text>;
template class BoxVerbs<Image>;
template class BoxVerbs<Band>;
template class BoxVerbs<Rule>;

}  // namespace sigil::compose
