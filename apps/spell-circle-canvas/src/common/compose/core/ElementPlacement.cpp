/** @file
 * The placement verbs — taking a node out of the flow, the insets and
 * the pins that put it somewhere, the two shorthands written over them,
 * the anchor a node hangs off, and the cell a grid-shaped scheme puts
 * it in.
 */

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& PlacementVerbs<Derived>::absolute() {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::cover() {
  inset(0.0f);
  declarations()->layout.covering = true;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(float all) {
  return inset(all, all, all, all);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(float l, float t, float r, float b) {
  return inset(Dimension(l), Dimension(t), Dimension(r), Dimension(b));
}

template <class Derived>
Derived& PlacementVerbs<Derived>::inset(Dimension l, Dimension t, Dimension r,
                                        Dimension b) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.hasInsets = true;
  node->layout.insets = {l, t, r, b};
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::left(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.hasInsets = true;
  node->layout.insets.left = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::top(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.hasInsets = true;
  node->layout.insets.top = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::right(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.hasInsets = true;
  node->layout.insets.right = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::bottom(Dimension d) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.hasInsets = true;
  node->layout.insets.bottom = d;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::centerAt(SkPoint p) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  node->layout.centerAt = p;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::cells(int column, int row, int columns,
                                        int rows) {
  // A span of zero cells would place the child nowhere and size it to
  // nothing, which reads as "it vanished" rather than as a mistake.
  CellSpan& claim = declarations()->layout.cells;
  claim.column = std::max(column, 0);
  claim.row = std::max(row, 0);
  claim.columns = std::max(columns, 1);
  claim.rows = std::max(rows, 1);
  claim.declared = true;
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::tether(Tether t) {
  detail::ElementNode* node = declarations();
  node->layout.absolute = true;
  node->layout.covering = false;
  detail::DeriveData& derive = node->deriveData.ensure();
  // LAST-WINS, so the previous tether's reads go with it: a box hangs off
  // exactly one anchor at a time, and one re-tethered would otherwise keep
  // waiting on every node it was ever tethered to. The keys that tether
  // named, and no other Bounds read — a spans gate sized from a node's box
  // is a read this one does not own.
  if (derive.tether) {
    const Tether& was = *derive.tether;
    std::erase_if(derive.reads, [&](const sigil::core::Read& read) {
      if (read.facet != sigil::core::Facet::Bounds) return false;
      if (read.key == was.key) return true;
      for (const Tether& fallback : was.fallbacks)
        if (read.key == fallback.key) return true;
      return false;
    });
  }
  // Every place the box may end up is a node whose finished geometry this
  // one waits for, so every one of them is declared — a fallback that
  // named a node nothing waited for would be resolved a pass late, and
  // the box would flick into it a frame after the anchor moved.
  derive.reads.push_back({t.key, sigil::core::Facet::Bounds});
  for (const Tether& fallback : t.fallbacks)
    derive.reads.push_back({fallback.key, sigil::core::Facet::Bounds});
  derive.tether = std::move(t);
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::cells(CellSpan span) {
  return cells(span.column, span.row, span.columns, span.rows);
}

template <class Derived>
Derived& PlacementVerbs<Derived>::area(std::string_view name) {
  // The name alone is the claim: the numbers stay at their defaults until
  // the scheme's picture resolves them, and `declared` is left to that
  // resolution, so a name no picture carries flows exactly as an unspoken
  // child does rather than landing on cell (0, 0).
  declarations()->deriveData.ensure().cellArea = std::string(name);
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::cellAlign(Align across, Align down) {
  // An alignment says where the child sits in whatever cell it gets, and
  // nothing about WHICH cell: `declared` stays as it is, so a child that
  // states only this still flows.
  CellSpan& claim = declarations()->layout.cells;
  claim.across = across;
  claim.down = down;
  claim.alignDeclared = true;
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
Derived& PlacementVerbs<Derived>::rect(const SkRect& r) {
  left(Dimension(r.fLeft));
  top(Dimension(r.fTop));
  self().width(Dimension(r.width()));
  self().height(Dimension(r.height()));
  return self();
}

template <class Derived>
Derived& PlacementVerbs<Derived>::at(SkPoint topLeft) {
  left(Dimension(topLeft.fX));
  top(Dimension(topLeft.fY));
  return self();
}

template class PlacementVerbs<Element>;

}  // namespace sigil::compose
