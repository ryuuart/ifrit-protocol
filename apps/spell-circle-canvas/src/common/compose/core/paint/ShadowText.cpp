/** @file
 * CSS's `box-shadow` and `text-shadow` read from their text: a list of
 * shadows, each an offset, a blur radius, a spread for a box, a colour
 * and, for a box, `inset`, in any order CSS allows.
 */

#include <sigilcompose/core/Shadow.h>
#include <sigilcompose/core/verbs/Font.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/CssText.h>

#include <optional>
#include <string_view>
#include <vector>

namespace sigil::compose {

namespace {

namespace css = material::css;

/** One shadow as written: its lengths, in order, its colour where one is
 *  named, and whether it is inset. */
struct WrittenShadow {
  std::vector<float> lengths;
  std::optional<material::Color> color;
  bool inset = false;
};

/** One entry of a shadow list: the lengths together, the colour and
 *  `inset` before or after them, each once. `currentcolor` is the ink in
 *  force, as an unnamed colour is. */
std::optional<WrittenShadow> shadowOf(std::string_view text, bool mayInset,
                                      size_t mostLengths) {
  WrittenShadow shadow;
  bool lengthsEnded = false, namedColor = false;
  for (const std::string_view component : css::components(text)) {
    if (const std::optional<float> length = css::pixels(component)) {
      if (lengthsEnded || shadow.lengths.size() == mostLengths)
        return std::nullopt;
      shadow.lengths.push_back(*length);
      continue;
    }
    lengthsEnded = !shadow.lengths.empty();
    if (mayInset && css::isWord(component, "inset")) {
      if (shadow.inset) return std::nullopt;
      shadow.inset = true;
      continue;
    }
    if (namedColor) return std::nullopt;
    namedColor = true;
    if (css::isWord(component, "currentcolor")) continue;
    const std::optional<material::Color> color = material::readColor(component);
    if (!color) return std::nullopt;
    shadow.color = *color;
  }
  if (shadow.lengths.size() < 2) return std::nullopt;
  // A blur radius is never negative; a spread may be.
  if (shadow.lengths.size() > 2 && shadow.lengths[2] < 0.0f)
    return std::nullopt;
  return shadow;
}

/** Every entry of a shadow list, or nothing where one does not read.
 *  `none` is the empty list. */
std::optional<std::vector<WrittenShadow>> listOf(std::string_view text,
                                                 bool mayInset,
                                                 size_t mostLengths) {
  text = css::trimmed(text);
  if (css::isWord(text, "none")) return std::vector<WrittenShadow>{};
  const std::optional<std::vector<std::string_view>> parts =
      css::commaParts(text);
  if (!parts) return std::nullopt;
  std::vector<WrittenShadow> list;
  for (const std::string_view part : *parts) {
    std::optional<WrittenShadow> shadow = shadowOf(part, mayInset, mostLengths);
    if (!shadow) return std::nullopt;
    list.push_back(std::move(*shadow));
  }
  return list;
}

}  // namespace

std::optional<std::vector<Shadow>> readShadow(std::string_view text) {
  const std::optional<std::vector<WrittenShadow>> written =
      listOf(text, /*mayInset=*/true, /*mostLengths=*/4);
  if (!written) return std::nullopt;
  std::vector<Shadow> shadows;
  for (const WrittenShadow& one : *written) {
    Shadow shadow;
    shadow.offset = {one.lengths[0], one.lengths[1]};
    if (one.lengths.size() > 2) shadow.blur = one.lengths[2];
    if (one.lengths.size() > 3) shadow.spread = one.lengths[3];
    if (one.color) shadow.color = Fill::color(*one.color);
    shadow.inset = one.inset;
    shadows.push_back(std::move(shadow));
  }
  return shadows;
}

std::optional<std::vector<TextShadow>> readTextShadow(std::string_view text) {
  const std::optional<std::vector<WrittenShadow>> written =
      listOf(text, /*mayInset=*/false, /*mostLengths=*/3);
  if (!written) return std::nullopt;
  std::vector<TextShadow> shadows;
  for (const WrittenShadow& one : *written) {
    TextShadow shadow;
    shadow.offset = {one.lengths[0], one.lengths[1]};
    if (one.lengths.size() > 2) shadow.blur = one.lengths[2];
    if (one.color) shadow.color = ColorTerm(*one.color);
    shadows.push_back(shadow);
  }
  return shadows;
}

}  // namespace sigil::compose
