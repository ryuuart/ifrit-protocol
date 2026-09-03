/** @file
 * Element's layout verbs — the flex direction, gaps, padding and margin,
 * the dims, the flex factors and alignment, the absolute placement
 * longhand, and the two placement shorthands written over it.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::row() {
  m_node->layout.row = true;
  return *this;
}

Element& Element::column() {
  m_node->layout.row = false;
  return *this;
}

Element& Element::wrapLines(bool on) {
  m_node->layout.wrap = on;
  return *this;
}

Element& Element::gap(float px) {
  m_node->layout.gap = px;
  return *this;
}

Element& Element::padding(float all) {
  m_node->layout.padding = {all, all, all, all};
  return *this;
}

Element& Element::padding(float h, float v) {
  m_node->layout.padding = {h, v, h, v};
  return *this;
}

Element& Element::padding(float l, float t, float r, float b) {
  m_node->layout.padding = {l, t, r, b};
  return *this;
}

Element& Element::margin(float all) {
  m_node->layout.margin = {all, all, all, all};
  return *this;
}

Element& Element::margin(float h, float v) {
  m_node->layout.margin = {h, v, h, v};
  return *this;
}

Element& Element::margin(float l, float t, float r, float b) {
  m_node->layout.margin = {l, t, r, b};
  return *this;
}

Element& Element::width(Dim d) {
  m_node->layout.width = d;
  return *this;
}

Element& Element::height(Dim d) {
  m_node->layout.height = d;
  return *this;
}

Element& Element::minWidth(Dim d) {
  m_node->layout.minWidth = d;
  return *this;
}

Element& Element::maxWidth(Dim d) {
  m_node->layout.maxWidth = d;
  return *this;
}

Element& Element::minHeight(Dim d) {
  m_node->layout.minHeight = d;
  return *this;
}

Element& Element::maxHeight(Dim d) {
  m_node->layout.maxHeight = d;
  return *this;
}

Element& Element::aspect(float r) {
  m_node->layout.aspect = r;
  return *this;
}

Element& Element::grow(float f) {
  m_node->layout.grow = f;
  return *this;
}

Element& Element::shrink(float f) {
  m_node->layout.shrink = f;
  return *this;
}

Element& Element::basis(Dim d) {
  m_node->layout.basis = d;
  return *this;
}

Element& Element::alignItems(Align a) {
  m_node->layout.alignItems = a;
  return *this;
}

Element& Element::alignSelf(Align a) {
  m_node->layout.alignSelf = a;
  return *this;
}

Element& Element::justify(Justify j) {
  m_node->layout.justify = j;
  return *this;
}

Element& Element::absolute() {
  m_node->layout.absolute = true;
  return *this;
}

Element& Element::inset(float all) { return inset(all, all, all, all); }

Element& Element::inset(float l, float t, float r, float b) {
  return inset(Dim(l), Dim(t), Dim(r), Dim(b));
}

Element& Element::inset(Dim l, Dim t, Dim r, Dim b) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets = {l, t, r, b};
  return *this;
}

Element& Element::left(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.left = d;
  return *this;
}

Element& Element::top(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.top = d;
  return *this;
}

Element& Element::right(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.right = d;
  return *this;
}

Element& Element::bottom(Dim d) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets.bottom = d;
  return *this;
}

Element& Element::centerAt(SkPoint p) {
  m_node->layout.absolute = true;
  m_node->layout.centerAt = p;
  return *this;
}

// rect()/at() go through the edge setters rather than writing LayoutProps
// themselves. That is the whole safety argument: they cannot describe a node
// the longhand could not, they touch no field the longhand does not, and
// they cannot drift from it when a setter changes. Keep them that way — the
// setters do more than assign (left/top also raise `absolute` and
// `hasInsets`), so a shortcut that wrote the fields directly would produce a
// node the longhand can never produce.
Element& Element::rect(const SkRect& r) {
  left(Dim(r.fLeft));
  top(Dim(r.fTop));
  width(Dim(r.width()));
  height(Dim(r.height()));
  return *this;
}

Element& Element::at(SkPoint topLeft) {
  left(Dim(topLeft.fX));
  top(Dim(topLeft.fY));
  return *this;
}

}  // namespace sigil::compose
