#pragma once

/** @file
 * @ingroup material-color
 *
 * THE PIECES OF CSS TEXT every reader of a CSS value shares — the
 * gradient, filter and shadow readers among them: a value cut at its
 * top-level commas or blank space, a function's name and its body, and
 * the numbers, percentages, absolute lengths and angles its arguments are
 * written in. Each answers nothing for text it does not read.
 */

#include <sigilcore/compute/Angle.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <string_view>
#include <vector>

namespace sigil::material::css {

inline bool isBlank(char character) {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r' || character == '\f';
}

inline std::string_view trimmed(std::string_view text) {
  while (!text.empty() && isBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && isBlank(text.back())) text.remove_suffix(1);
  return text;
}

/** Whether @p text is @p word, ASCII case aside, as CSS reads a keyword. */
inline bool isWord(std::string_view text, std::string_view word) {
  if (text.size() != word.size()) return false;
  for (size_t index = 0; index < text.size(); ++index)
    if (std::tolower(static_cast<unsigned char>(text[index])) !=
        std::tolower(static_cast<unsigned char>(word[index])))
      return false;
  return true;
}

/** @p text cut at each comma outside brackets, each part trimmed. Nothing
 *  where the brackets do not balance. */
inline std::optional<std::vector<std::string_view>> commaParts(
    std::string_view text) {
  std::vector<std::string_view> parts;
  int depth = 0;
  size_t start = 0;
  for (size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (character == '(') ++depth;
    if (character == ')' && --depth < 0) return std::nullopt;
    if (character == ',' && depth == 0) {
      parts.push_back(trimmed(text.substr(start, index - start)));
      start = index + 1;
    }
  }
  if (depth != 0) return std::nullopt;
  parts.push_back(trimmed(text.substr(start)));
  return parts;
}

/** @p text cut at the blank space outside brackets, as CSS separates the
 *  components of one value: `rgb(0 0 0 / 30%) 4px` is two. A function
 *  ends a component, so `blur(4px)sepia(1)` is two as well. */
inline std::vector<std::string_view> components(std::string_view text) {
  std::vector<std::string_view> found;
  int depth = 0;
  size_t start = 0;
  bool inside = false;
  for (size_t index = 0; index <= text.size(); ++index) {
    const bool end = index == text.size();
    const char character = end ? ' ' : text[index];
    if (depth == 0 && isBlank(character)) {
      if (inside) found.push_back(text.substr(start, index - start));
      inside = false;
      continue;
    }
    if (!inside) {
      start = index;
      inside = true;
    }
    if (character == '(') ++depth;
    if (character == ')' && depth > 0 && --depth == 0) {
      found.push_back(text.substr(start, index + 1 - start));
      inside = false;
    }
  }
  return found;
}

/** A function written `name(body)`: its name and what its brackets hold,
 *  trimmed; nothing where @p text is not one whole function. */
struct Function {
  std::string_view name;
  std::string_view body;
};
inline std::optional<Function> function(std::string_view text) {
  text = trimmed(text);
  const size_t open = text.find('(');
  if (open == std::string_view::npos || open == 0 || text.back() != ')')
    return std::nullopt;
  // The bracket the name opens is the one the text ends with.
  int depth = 0;
  for (size_t index = open; index < text.size(); ++index) {
    if (text[index] == '(') ++depth;
    if (text[index] == ')' && --depth == 0 && index + 1 != text.size())
      return std::nullopt;
  }
  if (depth != 0) return std::nullopt;
  for (size_t index = 0; index < open; ++index)
    if (isBlank(text[index]) || text[index] == ')') return std::nullopt;
  return Function{text.substr(0, open),
                  trimmed(text.substr(open + 1, text.size() - open - 2))};
}

/** A CSS number with the unit straight after it, the unit compared with
 *  ASCII case aside. */
inline std::optional<double> numberWith(std::string_view text,
                                        std::string_view unit) {
  text = trimmed(text);
  if (!text.empty() && text.front() == '+') text.remove_prefix(1);
  if (text.empty()) return std::nullopt;
  double value = 0.0;
  const char* const last = text.data() + text.size();
  const std::from_chars_result read = std::from_chars(text.data(), last, value);
  if (read.ec != std::errc{} || !std::isfinite(value)) return std::nullopt;
  if (!isWord(std::string_view(read.ptr, (size_t)(last - read.ptr)), unit))
    return std::nullopt;
  return value;
}

/** A plain CSS number. */
inline std::optional<double> number(std::string_view text) {
  return numberWith(text, "");
}

/** A percentage, as the fraction it is: `"40%"` is 0.4. */
inline std::optional<float> percentage(std::string_view text) {
  if (const std::optional<double> value = numberWith(text, "%"))
    return (float)(*value / 100.0);
  return std::nullopt;
}

/** An absolute length in pixels: `px`, `pt` at four thirds of a pixel, or
 *  a bare zero, which CSS writes without a unit. A length against the
 *  font, the viewport or a box has nothing to be measured against here. */
inline std::optional<float> pixels(std::string_view text) {
  if (const std::optional<double> value = numberWith(text, "px"))
    return (float)*value;
  if (const std::optional<double> value = numberWith(text, "pt"))
    return (float)(*value * 4.0 / 3.0);
  if (const std::optional<double> value = number(text))
    if (*value == 0.0) return 0.0f;
  return std::nullopt;
}

/** An angle in degrees: one written with CSS's units, or a bare zero. */
inline std::optional<float> degrees(std::string_view text) {
  if (const std::optional<core::angle::Angle> read = core::angle::read(text))
    return read->inDegrees();
  if (const std::optional<double> value = number(text))
    if (*value == 0.0) return 0.0f;
  return std::nullopt;
}

}  // namespace sigil::material::css
