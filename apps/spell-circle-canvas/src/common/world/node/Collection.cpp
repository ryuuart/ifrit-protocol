/** @file
 * Collection terms and the match.
 */

#include <sigilworld/node/Collection.h>

#include <algorithm>

namespace sigil::world {

bool answers(const Member& member, std::string_view term) {
  if (!term.empty() && term.front() == '/') {
    const std::string_view key = term.substr(1);
    if (member.key == key) return true;
    return std::find(member.ancestors.begin(), member.ancestors.end(), key) !=
           member.ancestors.end();
  }
  return std::find(member.collections.begin(), member.collections.end(),
                   term) != member.collections.end();
}

bool Collection::contains(const Member& member) const {
  const auto named = [&member](const std::string& term) {
    return answers(member, term);
  };
  if (!include.empty() && std::none_of(include.begin(), include.end(), named))
    return false;
  return std::none_of(exclude.begin(), exclude.end(), named);
}

}  // namespace sigil::world
