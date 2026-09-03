/** @file
 * Element's shape verbs — the corner radii, the silhouette, a band's
 * formation, the clip, and what the node's decorations dress.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::corners(Corners c) {
  m_node->corners = c;
  return *this;
}

Element& Element::shape(Shape path) {
  m_node->shapeFn = std::move(path);
  return *this;
}

Element& Element::centered() {
  m_node->deriveData.ensure().bandFormation =
      geometry::path::Formation::Centered;
  return *this;
}

Element& Element::outward() {
  m_node->deriveData.ensure().bandFormation =
      geometry::path::Formation::Outward;
  return *this;
}

Element& Element::inward() {
  m_node->deriveData.ensure().bandFormation = geometry::path::Formation::Inward;
  return *this;
}

Element& Element::clip(bool on) {
  m_node->clipContent = on;
  return *this;
}

Element& Element::boundary(Boundary source) {
  m_node->boundary = source;
  return *this;
}

}  // namespace sigil::compose
