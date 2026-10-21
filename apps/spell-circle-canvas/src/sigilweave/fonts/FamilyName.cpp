/** @file
 * A face's family name, and the comparison that says two names are one
 * family.
 */

#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FamilyName.h>

#include <algorithm>
#include <cctype>

namespace sigil::weave::fonts {

std::string familyName(const SkTypeface& typeface) {
  SkString name;
  typeface.getFamilyName(&name);
  return {name.c_str(), name.size()};
}

bool sameFamily(std::string_view left, std::string_view right) {
  return std::ranges::equal(
      left, right, [](char leftCharacter, char rightCharacter) {
        return std::tolower(static_cast<unsigned char>(leftCharacter)) ==
               std::tolower(static_cast<unsigned char>(rightCharacter));
      });
}

}  // namespace sigil::weave::fonts
