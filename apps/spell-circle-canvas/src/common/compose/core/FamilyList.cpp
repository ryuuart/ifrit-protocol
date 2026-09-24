/** @file
 * A `font-family` value read into the names it lists.
 */

#include "FamilyList.h"

namespace sigil::compose::detail {

namespace {

bool isSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

/** One entry of the list, its surrounding space already dropped: a quoted
 *  name without its quotes, or an unquoted one with its inner runs of
 *  space read as one. */
std::string nameOf(std::string_view entry) {
  if (entry.size() >= 2 && (entry.front() == '"' || entry.front() == '\'') &&
      entry.back() == entry.front())
    return std::string(entry.substr(1, entry.size() - 2));
  std::string name;
  name.reserve(entry.size());
  bool spaced = false;
  for (const char c : entry) {
    if (isSpace(c)) {
      spaced = true;
      continue;
    }
    if (spaced && !name.empty()) name.push_back(' ');
    spaced = false;
    name.push_back(c);
  }
  return name;
}

std::string_view trimmed(std::string_view text) {
  while (!text.empty() && isSpace(text.front())) text.remove_prefix(1);
  while (!text.empty() && isSpace(text.back())) text.remove_suffix(1);
  return text;
}

}  // namespace

std::vector<std::string> familiesOf(std::string_view list) {
  std::vector<std::string> families;
  char quote = 0;
  size_t start = 0;
  const auto take = [&](size_t end) {
    std::string name = nameOf(trimmed(list.substr(start, end - start)));
    if (!name.empty()) families.push_back(std::move(name));
    start = end + 1;
  };
  for (size_t index = 0; index < list.size(); ++index) {
    const char c = list[index];
    if (quote != 0) {
      if (c == quote) quote = 0;
    } else if (c == '"' || c == '\'') {
      quote = c;
    } else if (c == ',') {
      take(index);
    }
  }
  take(list.size());
  return families;
}

}  // namespace sigil::compose::detail
