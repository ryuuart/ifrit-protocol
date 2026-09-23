/** @file
 * What the cascade says when a statement reaches nothing: a class no rule
 * names, a keyword no fold resolves, a rule stating what a rule cannot
 * hold, and a custom property nobody set. Each is said once per name.
 */

#include <include/core/SkTypes.h>  // SkDebugf

#include <boost/unordered/unordered_flat_set.hpp>
#include <cstdint>
#include <string>
#include <string_view>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

void detail::warnNoSuchClass(std::string_view name, bool anySheetInScope) {
  static thread_local boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(std::string(name)).second) return;
  SkDebugf(
      "[compose] styleClass(\"%.*s\") names a class no rule of the sheets "
      "in force where the element lands speaks about%s — nothing was set, "
      "and the text under it is set in whatever it inherits. Apply a sheet "
      "with applyStyleSheet() on the element or on a node above it, with a "
      "rule naming .%.*s. (warned once)\n",
      (int)name.size(), name.data(),
      anySheetInScope ? "" : " (no sheet is applied on the tree above it)",
      (int)name.size(), name.data());
}

void detail::warnPropertyAnswersNoKeyword(Property property) {
  static thread_local boost::unordered_flat_set<uint8_t> warned;
  if (!warned.insert((uint8_t)property).second) return;
  const std::string_view name = propertyName(property);
  SkDebugf(
      "[compose] inherit/initial/unset was said about %.*s, and nothing "
      "resolves a keyword for that property — it is kept on the "
      "description, which no fold reads. The keyword was REFUSED, so the "
      "node describes as one that never wrote it rather than as one "
      "carrying a statement nothing answers. State the value itself. "
      "(warned once)\n",
      (int)name.size(), name.data());
}

void detail::warnRuleHoldsOnlyStaticValues(Property property) {
  static thread_local boost::unordered_flat_set<uint8_t> warned;
  if (!warned.insert((uint8_t)property).second) return;
  const std::string_view name = propertyName(property);
  SkDebugf(
      "[compose] a rule states %.*s as a live binding, an animation or an "
      "animated paint, and a rule holds static values — the statement was "
      "left out, so the elements it matches keep what they would have "
      "without it. State the value itself in the rule, or the live form on "
      "the element. (warned once)\n",
      (int)name.size(), name.data());
}

void detail::warnRuleCannotState(Property property) {
  static thread_local boost::unordered_flat_set<uint8_t> warned;
  if (!warned.insert((uint8_t)property).second) return;
  const std::string_view name = propertyName(property);
  SkDebugf(
      "[compose] a rule states %.*s, which is kept on the element's own "
      "description and which a rule's layer does not carry — the statement "
      "was left out, so the elements it matches keep what they would have "
      "without it. State it on the element. (warned once)\n",
      (int)name.size(), name.data());
}

void detail::warnNoSuchVar(VarRef reference, bool wantColour) {
  static thread_local boost::unordered_flat_set<uint64_t> warned;
  if (!warned.insert(((uint64_t)reference.id << 1) | (wantColour ? 1u : 0u))
           .second)
    return;
  const std::string_view name = varName(reference);
  SkDebugf(
      "[compose] var(\"%.*s\") read as a %s where no ancestor set one as "
      "such — it resolves to nothing. Set it with Element::var on an "
      "ancestor of the node that reads it, holding a %s. (warned once)\n",
      (int)name.size(), name.data(), wantColour ? "colour" : "length",
      wantColour ? "colour" : "length");
}

}  // namespace sigil::compose
