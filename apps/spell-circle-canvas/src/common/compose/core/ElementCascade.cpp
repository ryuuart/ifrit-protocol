/** @file
 * The cascade verbs — the inherited font and its colour, the block,
 * the custom properties a node sets for everything under it, and how
 * image leaves under it sample.
 */

#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Type.h>

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& CascadeVerbs<Derived>::font(sigil::weave::Type partial) {
  detail::CascadeData& cascade = declarations()->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  // Later wins field by field: the partial written last replaces what it
  // names and leaves the rest as an earlier call or a class left it.
  sigil::weave::merge(*cascade.font, partial);
  // A colour written here is the ink, so a property the ink was read from
  // before no longer stands.
  if (partial.color) cascade.inkVar.reset();
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::block(sigil::weave::Block partial) {
  detail::CascadeData& cascade = declarations()->cascadeData.ensure();
  if (!cascade.block) cascade.block.emplace();
  sigil::weave::merge(*cascade.block, partial);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::ink(SkColor4f colour) {
  detail::CascadeData& cascade = declarations()->cascadeData.ensure();
  if (!cascade.font) cascade.font.emplace();
  cascade.font->color = colour;
  cascade.inkVar.reset();
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::ink(VarRef reference) {
  detail::CascadeData& cascade = declarations()->cascadeData.ensure();
  cascade.inkVar = reference;
  if (cascade.font) cascade.font->color.reset();
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::var(std::string_view name, SkColor4f colour) {
  declarations()->cascadeData.ensure().vars.set(compose::var(name), colour);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::var(std::string_view name, Dimension length) {
  declarations()->cascadeData.ensure().vars.set(compose::var(name), length);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::varDefaults(VarTable defaults) {
  declarations()->cascadeData.ensure().varDefaults = std::move(defaults);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::imageRendering(SkSamplingOptions options) {
  declarations()->cascadeData.ensure().sampling = options;
  return self();
}

template class CascadeVerbs<Element>;
template class CascadeVerbs<Text>;
template class CascadeVerbs<Image>;
template class CascadeVerbs<Band>;

// The cascade a node NAMES rather than states: the sheets its classes
// resolve through and the sheets it applies to its subtree, the role
// that stands under them, and the classes themselves. They are the
// node's identity in the cascade, not a property a rule could restate,
// so they stand with the rest of what a node IS.

template <class Derived>
Derived& StructureVerbs<Derived>::styleClass(std::string_view names) {
  // The names are KEPT, and resolved by the cascade pass against the
  // sheets in force where this node lands: one name, two halves — the
  // text sheet's partial and the block sheet's, whichever carries it —
  // several names folding in the order they are written.
  detail::CascadeData& cascade = declarations()->cascadeData.ensure();
  for (size_t at = 0; at < names.size();) {
    const size_t end = std::min(names.find(' ', at), names.size());
    const std::string_view name = names.substr(at, end - at);
    at = end + 1;
    if (!name.empty()) cascade.classes.emplace_back(name);
  }
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::styleSheet(sigil::weave::StyleSheet sheet) {
  declarations()->cascadeData.ensure().sheet = std::move(sheet);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::applyStyleSheet(StyleSheet sheet) {
  // Applications ADD rather than replace, so a node may stand under a
  // house sheet and a local one at once, and the order they were
  // applied in is the last tiebreak between two rules of equal weight.
  declarations()->cascadeData.ensure().appliedSheets.push_back(
      std::move(sheet));
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::role(sigil::weave::Rule defaults) {
  declarations()->cascadeData.ensure().role = std::move(defaults);
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::role(std::string name) {
  return role(sigil::weave::Rule(std::move(name)));
}

// The five members of the structure family this file defines, named one
// by one for each kind of node: the family's other members are
// instantiated where they are defined, beside the rest of a node's
// identity.
#define SIGIL_COMPOSE_CASCADE_NAMES(Node)                                    \
  template Node& StructureVerbs<Node>::styleClass(std::string_view);         \
  template Node& StructureVerbs<Node>::styleSheet(sigil::weave::StyleSheet); \
  template Node& StructureVerbs<Node>::applyStyleSheet(StyleSheet);          \
  template Node& StructureVerbs<Node>::role(sigil::weave::Rule);             \
  template Node& StructureVerbs<Node>::role(std::string);
SIGIL_COMPOSE_CASCADE_NAMES(Element)
SIGIL_COMPOSE_CASCADE_NAMES(Text)
SIGIL_COMPOSE_CASCADE_NAMES(Image)
SIGIL_COMPOSE_CASCADE_NAMES(Band)
#undef SIGIL_COMPOSE_CASCADE_NAMES

}  // namespace sigil::compose
