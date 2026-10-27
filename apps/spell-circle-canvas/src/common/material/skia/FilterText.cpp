/** @file
 * A filter read from its CSS text: CSS's ten filter functions, written
 * one after another as `filter` writes them, chained in that order.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/CssText.h>
#include <sigilmaterial/filter/Filter.h>

#include <optional>
#include <string_view>
#include <vector>

namespace sigil::material {

namespace {

/** An amount a colour function takes: a number or a percentage, never
 *  below zero; @p unwritten where the brackets are empty. */
std::optional<float> amountOf(std::string_view body, float unwritten) {
  if (body.empty()) return unwritten;
  std::optional<float> amount = css::percentage(body);
  if (!amount)
    if (const std::optional<double> number = css::number(body))
      amount = (float)*number;
  if (!amount || *amount < 0.0f) return std::nullopt;
  return amount;
}

/** `drop-shadow(<colour>? && <length>{2,3})`: the offset, then a blur
 *  radius, with the colour before or after them; with none written, the
 *  shadow is in `currentcolor`, which its painter states. */
std::optional<Filter> dropShadowOf(std::string_view body) {
  std::optional<Color> color;
  std::vector<float> lengths;
  bool lengthsEnded = false;
  for (const std::string_view component : css::components(body)) {
    if (const std::optional<float> length = css::pixels(component)) {
      if (lengthsEnded) return std::nullopt;
      lengths.push_back(*length);
      continue;
    }
    if (color) return std::nullopt;
    color = readColor(component);
    if (!color) return std::nullopt;
    lengthsEnded = !lengths.empty();
  }
  if (lengths.size() < 2 || lengths.size() > 3) return std::nullopt;
  ShadowOptions options;
  options.offset = {lengths[0], lengths[1]};
  if (lengths.size() == 3) {
    if (lengths[2] < 0.0f) return std::nullopt;
    options.blur = lengths[2];
  }
  if (!color) return Filter::dropShadowInCurrentColor(options);
  return Filter::dropShadow(*color, options);
}

/** One filter function. */
std::optional<Filter> functionOf(const css::Function& written) {
  const std::string_view name = written.name;
  const std::string_view body = written.body;
  if (css::isWord(name, "blur")) {
    if (body.empty()) return Filter::blur(0.0f);
    const std::optional<float> sigma = css::pixels(body);
    if (!sigma || *sigma < 0.0f) return std::nullopt;
    return Filter::blur(*sigma);
  }
  if (css::isWord(name, "hue-rotate")) {
    if (body.empty()) return Filter::hueRotate(0.0f);
    const std::optional<float> angle = css::degrees(body);
    if (!angle) return std::nullopt;
    return Filter::hueRotate(*angle);
  }
  if (css::isWord(name, "drop-shadow")) return dropShadowOf(body);
  struct Amount {
    std::string_view name;
    Filter (*make)(float);
  };
  static constexpr Amount kAmounts[] = {
      {"brightness", &Filter::brightness}, {"contrast", &Filter::contrast},
      {"saturate", &Filter::saturate},     {"grayscale", &Filter::grayscale},
      {"sepia", &Filter::sepia},           {"invert", &Filter::invert},
      {"opacity", &Filter::opacity},
  };
  for (const Amount& amount : kAmounts) {
    if (!css::isWord(name, amount.name)) continue;
    const std::optional<float> read = amountOf(body, 1.0f);
    if (!read) return std::nullopt;
    return amount.make(*read);
  }
  return std::nullopt;
}

}  // namespace

std::optional<Filter> readFilter(std::string_view text) {
  text = css::trimmed(text);
  if (css::isWord(text, "none")) return Filter{};
  const std::vector<std::string_view> written = css::components(text);
  if (written.empty()) return std::nullopt;
  std::optional<Filter> chain;
  for (const std::string_view part : written) {
    const std::optional<css::Function> function = css::function(part);
    if (!function) return std::nullopt;
    const std::optional<Filter> one = functionOf(*function);
    if (!one) return std::nullopt;
    chain = chain ? chain->then(*one) : *one;
  }
  return chain;
}

}  // namespace sigil::material
