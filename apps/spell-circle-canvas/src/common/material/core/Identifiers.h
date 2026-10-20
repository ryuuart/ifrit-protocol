#pragma once

/** @file
 * Reading a shader body for the names it spells: a name counts only as a
 * whole identifier, so `low` inside `lowEdge` is a different name.
 */

#include <string_view>

namespace sigil::material::detail {

/** Whether @p character can stand inside an identifier. */
inline bool identifierCharacter(char character) {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '_';
}

/** Whether @p body spells @p name as a WHOLE IDENTIFIER. */
inline bool spellsIdentifier(std::string_view body, std::string_view name) {
  for (size_t at = body.find(name); at != std::string_view::npos;
       at = body.find(name, at + 1)) {
    if (at > 0 && identifierCharacter(body[at - 1])) continue;
    const size_t end = at + name.size();
    if (end < body.size() && identifierCharacter(body[end])) continue;
    return true;
  }
  return false;
}

}  // namespace sigil::material::detail
