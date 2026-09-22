/** @file
 * A length written as text, read into a Dimension: the one grammar a
 * rule's declaration and a length handed in from another language are
 * both taken through.
 */

#include <sigilcompose/core/Layout.h>

#include <cctype>
#include <charconv>
#include <string>

namespace sigil::compose {
namespace {

/** @p text with leading and trailing blank space cut off. */
std::string_view trimmed(std::string_view text) {
  const auto blank = [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
  };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}

/** Whether @p text is @p word, letter for letter, ignoring case. */
bool sameWord(std::string_view text, std::string_view word) {
  if (text.size() != word.size()) return false;
  for (size_t i = 0; i < text.size(); ++i)
    if (std::tolower((unsigned char)text[i]) != word[i]) return false;
  return true;
}

/** The Dimension @p amount takes in the unit @p suffix spells, or nothing
 *  for a suffix no unit answers to. A suffix of no letters at all is
 *  pixels, which is what a bare number is everywhere else here. */
std::optional<Dimension> inUnit(float amount, std::string_view suffix) {
  if (suffix.empty() || sameWord(suffix, "px")) return Dimension(amount);
  if (sameWord(suffix, "%")) return pct(amount);
  if (sameWord(suffix, "pw")) return pw(amount);
  if (sameWord(suffix, "ph")) return ph(amount);
  if (sameWord(suffix, "em")) return Dimension(sigil::weave::em(amount));
  if (sameWord(suffix, "rem")) return Dimension(sigil::weave::rem(amount));
  if (sameWord(suffix, "lh")) return Dimension(sigil::weave::lh(amount));
  if (sameWord(suffix, "ch")) return Dimension(sigil::weave::ch(amount));
  if (sameWord(suffix, "pt")) return Dimension(sigil::weave::pt(amount));
  return std::nullopt;
}

/** The name inside `var(...)`, or nothing where @p text is not that call.
 *  A leading `--` is CSS's mark for a custom property and names the same
 *  property as the bare name does. */
std::optional<std::string_view> customProperty(std::string_view text) {
  if (text.size() < 5 || text.back() != ')') return std::nullopt;
  if (!sameWord(text.substr(0, 4), "var(")) return std::nullopt;
  std::string_view name = trimmed(text.substr(4, text.size() - 5));
  if (name.starts_with("--")) name.remove_prefix(2);
  return name.empty() ? std::nullopt : std::optional(name);
}

}  // namespace

std::optional<Dimension> parseDimension(std::string_view text) {
  const std::string_view body = trimmed(text);
  if (body.empty()) return std::nullopt;
  if (sameWord(body, "auto")) return autoDimension();
  if (const std::optional<std::string_view> name = customProperty(body))
    return Dimension(var(*name));
  // from_chars reads the number and stops at the unit, which is the whole
  // of the split: a length is one number and one suffix with nothing
  // between them.
  float amount = 0;
  const char* const first = body.data();
  const char* const last = first + body.size();
  const std::from_chars_result read = std::from_chars(first, last, amount);
  if (read.ec != std::errc{}) return std::nullopt;
  return inUnit(amount, std::string_view(read.ptr, (size_t)(last - read.ptr)));
}

}  // namespace sigil::compose
