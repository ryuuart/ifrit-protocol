/** @file
 * The placement verbs — taking a node out of the flow, the insets and
 * the pins that put it somewhere, the two shorthands written over them,
 * the anchor a node hangs off, and the cell a grid-shaped scheme puts
 * it in.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& PlacementVerbs<Derived>::absolute() {
  declarations()->fields.absolute().absolute = true;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::cover() {
  declarations()->fields.cover();
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Dimension all) {
  return inset(all, all, all, all);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Dimension vertical,
                                        Dimension horizontal) {
  return inset(vertical, horizontal, vertical, horizontal);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Dimension top, Dimension horizontal,
                                        Dimension bottom) {
  return inset(top, horizontal, bottom, horizontal);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Dimension top, Dimension right,
                                        Dimension bottom, Dimension left) {
  detail::DeclaredFields& fields = declarations()->fields;
  const detail::DeclaredFields::Absolute placed = fields.absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  fields.top() = top;
  fields.right() = right;
  fields.bottom() = bottom;
  fields.left() = left;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Edges edges) {
  return inset(edges.top, edges.right, edges.bottom, edges.left);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::left(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  const detail::DeclaredFields::Absolute placed = fields.absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  fields.left() = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::top(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  const detail::DeclaredFields::Absolute placed = fields.absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  fields.top() = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::right(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  const detail::DeclaredFields::Absolute placed = fields.absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  fields.right() = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::bottom(Dimension d) {
  detail::DeclaredFields& fields = declarations()->fields;
  const detail::DeclaredFields::Absolute placed = fields.absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  fields.bottom() = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::centerAt(SkPoint p) {
  detail::DeclaredFields& fields = declarations()->fields;
  fields.absolute().absolute = true;
  fields.centerAt() = p;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::gridCells(int column, int row, int columns,
                                            int rows) {
  // A span of zero cells would place the child nowhere and size it to
  // nothing, which reads as "it vanished" rather than as a mistake.
  const detail::DeclaredFields::GridCells claim =
      declarations()->fields.gridCells();
  claim.column = std::max(column, 0);
  claim.row = std::max(row, 0);
  claim.columns = std::max(columns, 1);
  claim.rows = std::max(rows, 1);
  claim.declared = true;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::gridCells(CellSpan span) {
  return gridCells(span.column, span.row, span.columns, span.rows);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::gridArea(std::string_view name) {
  // The name alone is the claim: the numbers stay at their defaults until
  // the scheme's picture resolves them, and `declared` is left to that
  // resolution, so a name no picture carries flows exactly as an unspoken
  // child does rather than landing on cell (0, 0).
  declarations()->gridArea() = std::string(name);
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::gridCellAlign(Align across, Align down) {
  // An alignment says where the child sits in whatever cell it gets, and
  // nothing about WHICH cell: `declared` stays as it is, so a child that
  // states only this still flows.
  const detail::DeclaredFields::GridCellAlign claim =
      declarations()->fields.gridCellAlign();
  claim.across = across;
  claim.down = down;
  claim.declared = true;
  return self();
}

// rect()/at() go through the edge setters rather than writing LayoutProps
// themselves. That is the whole safety argument: they cannot describe a node
// the longhand could not, they touch no field the longhand does not, and
// they cannot drift from it when a setter changes. Keep them that way — the
// setters do more than assign (left/top also raise `absolute` and
// `hasInsets`), so a shortcut that wrote the fields directly would produce a
// node the longhand can never produce.
template <class Derived>
Derived& PlacementVerbs<Derived>::rect(Dimension x, Dimension y,
                                       Dimension width, Dimension height) {
  left(x);
  top(y);
  self().width(width);
  self().height(height);
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::rect(const SkRect& r) {
  return rect(Dimension(r.fLeft), Dimension(r.fTop), Dimension(r.width()),
              Dimension(r.height()));
}

template <class Derived>
Derived& PlacementVerbs<Derived>::at(Dimension x, Dimension y) {
  left(x);
  top(y);
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::at(SkPoint topLeft) {
  return at(Dimension(topLeft.fX), Dimension(topLeft.fY));
}

template class PlacementVerbs<Element>;
template class PlacementVerbs<Text>;
template class PlacementVerbs<Image>;
template class PlacementVerbs<Band>;
template class PlacementVerbs<Rule>;

}  // namespace sigil::compose
