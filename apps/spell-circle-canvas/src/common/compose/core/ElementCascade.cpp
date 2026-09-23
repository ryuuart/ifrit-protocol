/** @file
 * The cascade verbs — the block and its longhands, the custom properties
 * a node sets for everything under it, how image leaves under it sample,
 * and the three keywords — with the identity a node names in the cascade.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilweave/layout/Block.h>
#include <sigilweave/style/Type.h>

#include <algorithm>

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& CascadeVerbs<Derived>::block(sigil::weave::Block partial) {
  detail::CascadeData& cascade = declare(Property::Block)->cascadeData.ensure();
  if (!cascade.block) cascade.block.emplace();
  sigil::weave::merge(*cascade.block, partial);
  return self();
}

// The block's longhands: each is `block()` with one field, so the two
// spellings fold into one partial and the later statement wins.

template <class Derived>
Derived& CascadeVerbs<Derived>::lineHeight(sigil::weave::Leading leading) {
  return block({.leading = leading});
}

template <class Derived>
Derived& CascadeVerbs<Derived>::textAlign(
    sigil::weave::TextAlignment alignment) {
  return block({.alignment = alignment});
}

template <class Derived>
Derived& CascadeVerbs<Derived>::textIndent(float px) {
  return block({.firstLineIndent = px});
}

template <class Derived>
Derived& CascadeVerbs<Derived>::writingMode(sigil::weave::WritingMode mode) {
  return block({.writingMode = mode});
}

template <class Derived>
Derived& CascadeVerbs<Derived>::hyphens(
    sigil::weave::HyphenationOptions hyphenation) {
  return block({.hyphenation = std::move(hyphenation)});
}

template <class Derived>
Derived& CascadeVerbs<Derived>::var(std::string_view name,
                                    material::Color colour) {
  declare(Property::CustomProperties)
      ->cascadeData.ensure()
      .vars.set(compose::var(name), colour);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::var(std::string_view name, Dimension length) {
  declare(Property::CustomProperties)
      ->cascadeData.ensure()
      .vars.set(compose::var(name), length);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::varDefaults(VarTable defaults) {
  declare(Property::CustomProperties)->cascadeData.ensure().varDefaults =
      std::move(defaults);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::imageRendering(SkSamplingOptions options) {
  declare(Property::ImageRendering)->cascadeData.ensure().sampling = options;
  return self();
}

// The three wide keywords. Each is a DECLARATION of the property it
// names — the bit is set beside the entry — so it covers a rule and an
// inherited value exactly as a stated number would, and two descriptions
// that differ only in a keyword are unequal.

template <class Derived>
Derived& CascadeVerbs<Derived>::inherit(Property property) {
  detail::markKeyword(declarations(), property, sigil::weave::Keyword::Inherit);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::initial(Property property) {
  detail::markKeyword(declarations(), property, sigil::weave::Keyword::Initial);
  return self();
}

template <class Derived>
Derived& CascadeVerbs<Derived>::unset(Property property) {
  detail::markKeyword(declarations(), property, sigil::weave::Keyword::Unset);
  return self();
}

template class CascadeVerbs<Element>;
template class CascadeVerbs<Text>;
template class CascadeVerbs<Image>;
template class CascadeVerbs<Band>;
template class CascadeVerbs<Rule>;

// The cascade a node NAMES rather than states: the sheets it applies to
// its subtree, its role, and its classes. They are the
// node's identity in the cascade, not a property a rule could restate,
// so they stand with the rest of what a node IS.

template <class Derived>
Derived& StructureVerbs<Derived>::styleClass(std::string_view names) {
  // The names are KEPT, and matched by the cascade pass against the rules
  // of the sheets in force where this node lands.
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
Derived& StructureVerbs<Derived>::applyStyleSheet(StyleSheet sheet) {
  // Applications ADD rather than replace, so a node may stand under a
  // house sheet and a local one at once, and the order they were
  // applied in is the last tiebreak between two rules of equal weight.
  declarations()->cascadeData.ensure().appliedSheets.push_back(
      std::move(sheet));
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::role(std::string name,
                                       sigil::weave::Type font,
                                       sigil::weave::Block block) {
  declarations()->cascadeData.ensure().role = detail::RoleDefaults{
      std::move(name), std::move(font), std::move(block)};
  return self();
}

template <class Derived>
Derived& StructureVerbs<Derived>::role(std::string name,
                                       sigil::weave::Block block) {
  return role(std::move(name), sigil::weave::Type{}, std::move(block));
}

template <class Derived>
Derived& StructureVerbs<Derived>::role(std::string name) {
  return role(std::move(name), sigil::weave::Type{}, sigil::weave::Block{});
}

// The members of the structure family this file defines, named one
// by one for each kind of node: the family's other members are
// instantiated where they are defined, beside the rest of a node's
// identity.
#define SIGIL_COMPOSE_CASCADE_NAMES(Node)                                    \
  template Node& StructureVerbs<Node>::styleClass(std::string_view);         \
  template Node& StructureVerbs<Node>::applyStyleSheet(StyleSheet);          \
  template Node& StructureVerbs<Node>::role(                                 \
      std::string, sigil::weave::Type, sigil::weave::Block);                 \
  template Node& StructureVerbs<Node>::role(std::string, sigil::weave::Block); \
  template Node& StructureVerbs<Node>::role(std::string);
SIGIL_COMPOSE_CASCADE_NAMES(Element)
SIGIL_COMPOSE_CASCADE_NAMES(Text)
SIGIL_COMPOSE_CASCADE_NAMES(Image)
SIGIL_COMPOSE_CASCADE_NAMES(Band)
#undef SIGIL_COMPOSE_CASCADE_NAMES

}  // namespace sigil::compose
