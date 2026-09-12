#pragma once

/** @file
 * A custom property's NAME as a value: `var("accent")` interns the name
 * once and answers the reference a length, a fill or an ink is written
 * with, to be resolved down the tree against the nearest ancestor that set
 * the property.
 */

#include <cstdint>
#include <string_view>

namespace sigil::compose {

/** A REFERENCE TO A CUSTOM PROPERTY, by name — what `var("accent")`
 *  answers, and what a `Dimension`, a `Fill` or `Element::ink` carries in
 *  place of a value until the tree resolves it against the nearest
 *  ancestor whose `Element::var` set that name.
 *
 *  Two references to one name are equal. The id is the name's slot in a
 *  process-wide table, so comparing two is an integer compare and a
 *  description never holds the string; `varName` reads it back for a
 *  diagnostic. Zero names nothing. */
struct VarRef {
  uint32_t id = 0;
  bool operator==(const VarRef&) const = default;
};

/** The reference for @p name, interning it on first sight. */
[[nodiscard]] VarRef var(std::string_view name);

/** The name behind @p ref, or empty for an id nobody interned. */
[[nodiscard]] std::string_view varName(VarRef ref);

}  // namespace sigil::compose
