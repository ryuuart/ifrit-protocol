/** @file
 * Element's cascade verbs — the inherited font and its colour, the class
 * folded in where the element is written, and the custom properties a
 * node sets for everything under it.
 */

#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Type.h>

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::font(sigil::weave::Type partial) {
  detail::CascadeData& cascade = m_node->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  // Later wins field by field: the partial written last replaces what it
  // names and leaves the rest as an earlier call or a class left it.
  sigil::weave::merge(*cascade.font, partial);
  // A colour written here is the ink, so a property the ink was read from
  // before no longer stands.
  if (partial.color) cascade.inkVar.reset();
  return *this;
}

Element& Element::block(sigil::weave::Block partial) {
  detail::CascadeData& cascade = m_node->cascadeData.ensure();
  if (!cascade.block) cascade.block.emplace();
  sigil::weave::merge(*cascade.block, partial);
  return *this;
}

Element& Element::ink(SkColor4f colour) {
  detail::CascadeData& cascade = m_node->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  cascade.font->color = colour;
  cascade.inkVar.reset();
  return *this;
}

Element& Element::ink(VarRef reference) {
  detail::CascadeData& cascade = m_node->cascadeData.ensure();
  cascade.inkVar = reference;
  if (cascade.font) cascade.font->color.reset();
  return *this;
}

Element& Element::styleClass(std::string_view names) {
  // The names are KEPT, and resolved by the cascade pass against the
  // sheets in force where this node lands: one name, two halves — the
  // text sheet's partial and the block sheet's, whichever carries it —
  // several names folding in the order they are written.
  detail::CascadeData& cascade = m_node->cascadeData.ensure();
  for (size_t at = 0; at < names.size();) {
    const size_t end = std::min(names.find(' ', at), names.size());
    const std::string_view name = names.substr(at, end - at);
    at = end + 1;
    if (!name.empty()) cascade.classes.emplace_back(name);
  }
  return *this;
}

Element& Element::styleSheet(sigil::weave::StyleSheet sheet) {
  m_node->cascadeData.ensure().sheet = std::move(sheet);
  return *this;
}

Element& Element::var(std::string_view name, SkColor4f colour) {
  m_node->cascadeData.ensure().vars.set(compose::var(name), colour);
  return *this;
}

Element& Element::var(std::string_view name, Dimension length) {
  m_node->cascadeData.ensure().vars.set(compose::var(name), length);
  return *this;
}

}  // namespace sigil::compose
