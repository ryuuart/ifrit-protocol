/** @file
 * Element's cascade verbs — the inherited font and its colour, the class
 * folded in where the element is written, and the custom properties a
 * node sets for everything under it.
 */

#include <sigilcore/reconcile/Environment.h>
#include <sigilweave/style/StyleSheet.h>
#include <sigilweave/style/Type.h>

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

Element& Element::styleClass(std::string_view name) {
  // The sheet is read HERE, in the scope the element is written in: a
  // class is lexical, and the description holds the fields it set rather
  // than the name, so it depends on no scope that has since ended.
  const sigil::weave::StyleSheet* sheet =
      core::environment::inherited<sigil::weave::StyleSheet>();
  const sigil::weave::Type* cls = sheet ? sheet->find(name) : nullptr;
  if (!cls) {
    detail::warnNoSuchClass(name, sheet != nullptr);
    return *this;
  }
  return font(*cls);
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
