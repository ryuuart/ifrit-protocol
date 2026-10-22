/** @file
 * A colour read from its CSS text: hex, names, and every functional
 * notation CSS Color 4 defines, with `color-mix()` beside them.
 */

#include <sigilcore/report/Report.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Mix.h>

#include <cctype>
#include <cmath>
#include <mutex>
#include <numbers>
#include <string>
#include <unordered_set>
#include <vector>

#include "Spaces.h"

namespace sigil::material {

namespace {

int hexDigit(char digit) {
  if (digit >= '0' && digit <= '9') return digit - '0';
  if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
  if (digit >= 'A' && digit <= 'F') return digit - 'A' + 10;
  return -1;
}

bool isBlank(char character) {
  return character == ' ' || character == '\t' || character == '\n' ||
         character == '\r' || character == '\f';
}

std::string_view trimmed(std::string_view text) {
  while (!text.empty() && isBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && isBlank(text.back())) text.remove_suffix(1);
  return text;
}

std::optional<Color> readHex(std::string_view hex) {
  if (hex.size() != 3 && hex.size() != 4 && hex.size() != 6 && hex.size() != 8)
    return std::nullopt;
  int digits[8];
  for (size_t index = 0; index < hex.size(); ++index) {
    digits[index] = hexDigit(hex[index]);
    if (digits[index] < 0) return std::nullopt;
  }
  float channels[4] = {0, 0, 0, 1};
  if (hex.size() <= 4) {
    // One digit stands for itself twice, so `f` is `ff` and 17 times it.
    for (size_t index = 0; index < hex.size(); ++index)
      channels[index] = (float)(digits[index] * 17) / 255.0f;
  } else {
    for (size_t index = 0; index < hex.size() / 2; ++index)
      channels[index] =
          (float)(digits[2 * index] * 16 + digits[2 * index + 1]) / 255.0f;
  }
  return Color{channels[0], channels[1], channels[2], channels[3]};
}

/** One component of a functional notation, as written. */
struct Component {
  enum class Kind { Number, Percentage, Angle, None };
  Kind kind = Kind::None;
  /** The number as written; an angle is already in degrees. */
  double value = 0.0;
};

/** A CSS number: a sign, digits with at most one decimal point, and an
 *  exponent. Answers how many characters it took, zero where there is
 *  none. */
size_t readNumber(std::string_view text, double& value) {
  size_t at = 0;
  double sign = 1.0;
  if (at < text.size() && (text[at] == '+' || text[at] == '-'))
    sign = text[at++] == '-' ? -1.0 : 1.0;
  double whole = 0.0;
  size_t digits = 0;
  while (at < text.size() && std::isdigit((unsigned char)text[at])) {
    whole = whole * 10.0 + (text[at++] - '0');
    ++digits;
  }
  int fractionDigits = 0;
  if (at < text.size() && text[at] == '.') {
    ++at;
    while (at < text.size() && std::isdigit((unsigned char)text[at])) {
      whole = whole * 10.0 + (text[at++] - '0');
      ++fractionDigits;
      ++digits;
    }
  }
  if (digits == 0) return 0;
  int exponent = -fractionDigits;
  if (at < text.size() && (text[at] == 'e' || text[at] == 'E')) {
    size_t mark = at + 1;
    int exponentSign = 1;
    if (mark < text.size() && (text[mark] == '+' || text[mark] == '-'))
      exponentSign = text[mark++] == '-' ? -1 : 1;
    if (mark < text.size() && std::isdigit((unsigned char)text[mark])) {
      int written = 0;
      while (mark < text.size() && std::isdigit((unsigned char)text[mark]))
        written = std::min(written * 10 + (text[mark++] - '0'), 1000);
      exponent += exponentSign * written;
      at = mark;
    }
  }
  value = sign * whole * std::pow(10.0, exponent);
  return at;
}

std::optional<Component> readComponent(std::string_view token) {
  if (token == "none") return Component{Component::Kind::None, 0.0};
  double number = 0.0;
  const size_t taken = readNumber(token, number);
  if (taken == 0) return std::nullopt;
  const std::string_view unit = token.substr(taken);
  if (unit.empty()) return Component{Component::Kind::Number, number};
  if (unit == "%") return Component{Component::Kind::Percentage, number};
  if (unit == "deg") return Component{Component::Kind::Angle, number};
  if (unit == "rad")
    return Component{Component::Kind::Angle, number * 180.0 / std::numbers::pi};
  if (unit == "grad") return Component{Component::Kind::Angle, number * 0.9};
  if (unit == "turn") return Component{Component::Kind::Angle, number * 360.0};
  return std::nullopt;
}

/** The arguments of a functional notation: its components, the alpha if
 *  one was written, and whether they were written with commas. */
struct Arguments {
  std::vector<Component> components;
  std::optional<Component> alpha;
  bool legacy = false;
};

std::optional<Arguments> readArguments(std::string_view body) {
  Arguments arguments;
  std::vector<std::string_view> tokens;
  if (body.find(',') != std::string_view::npos) {
    // The legacy syntax: every component between commas, the fourth the
    // alpha, and no `none`.
    arguments.legacy = true;
    while (true) {
      const size_t comma = body.find(',');
      const std::string_view token = trimmed(body.substr(0, comma));
      if (token.empty()) return std::nullopt;
      for (char character : token)
        if (isBlank(character) || character == '/') return std::nullopt;
      tokens.push_back(token);
      if (comma == std::string_view::npos) break;
      body.remove_prefix(comma + 1);
    }
    if (tokens.size() != 3 && tokens.size() != 4) return std::nullopt;
    for (size_t index = 0; index < tokens.size(); ++index) {
      const std::optional<Component> component = readComponent(tokens[index]);
      if (!component || component->kind == Component::Kind::None)
        return std::nullopt;
      if (index < 3)
        arguments.components.push_back(*component);
      else
        arguments.alpha = component;
    }
    return arguments;
  }
  // The modern syntax: components between blank space, then a slash and
  // the alpha.
  bool afterSlash = false;
  size_t slashes = 0;
  size_t at = 0;
  while (at < body.size()) {
    if (isBlank(body[at])) {
      ++at;
      continue;
    }
    if (body[at] == '/') {
      if (++slashes > 1) return std::nullopt;
      afterSlash = true;
      ++at;
      continue;
    }
    size_t end = at;
    while (end < body.size() && !isBlank(body[end]) && body[end] != '/') ++end;
    const std::optional<Component> component =
        readComponent(body.substr(at, end - at));
    if (!component) return std::nullopt;
    if (afterSlash) {
      if (arguments.alpha) return std::nullopt;
      arguments.alpha = component;
    } else {
      arguments.components.push_back(*component);
    }
    at = end;
  }
  if (afterSlash && !arguments.alpha) return std::nullopt;
  return arguments;
}

/** The alpha a component says: a number on 0..1 or a percentage, clamped;
 *  opaque when none was written. */
std::optional<float> alphaOf(const std::optional<Component>& alpha) {
  if (!alpha) return 1.0f;
  switch (alpha->kind) {
    case Component::Kind::Number:
      return (float)std::clamp(alpha->value, 0.0, 1.0);
    case Component::Kind::Percentage:
      return (float)std::clamp(alpha->value / 100.0, 0.0, 1.0);
    case Component::Kind::None:
      return 0.0f;
    case Component::Kind::Angle:
      return std::nullopt;
  }
  return std::nullopt;
}

/** A component that is a number or a percentage, the percentage read
 *  against @p hundredPercent; `none` is zero. */
std::optional<double> scalarOf(const Component& component,
                               double hundredPercent) {
  switch (component.kind) {
    case Component::Kind::Number:
      return component.value;
    case Component::Kind::Percentage:
      return component.value / 100.0 * hundredPercent;
    case Component::Kind::None:
      return 0.0;
    case Component::Kind::Angle:
      return std::nullopt;
  }
  return std::nullopt;
}

/** A hue: a number of degrees or an angle with its unit; `none` is
 *  zero. */
std::optional<double> hueOf(const Component& component) {
  if (component.kind == Component::Kind::Percentage) return std::nullopt;
  return component.value;
}

std::optional<Color> readRgb(const Arguments& arguments) {
  if (arguments.components.size() != 3) return std::nullopt;
  float channels[3];
  for (size_t index = 0; index < 3; ++index) {
    const Component& component = arguments.components[index];
    switch (component.kind) {
      case Component::Kind::Number:
        // Divided as a float, as the hex digits are, so `rgb(110 153 187)`
        // is the same four floats as `#6e99bb`.
        channels[index] =
            (float)std::clamp(component.value, 0.0, 255.0) / 255.0f;
        break;
      case Component::Kind::Percentage:
        channels[index] = (float)std::clamp(component.value / 100.0, 0.0, 1.0);
        break;
      case Component::Kind::None:
        channels[index] = 0.0f;
        break;
      case Component::Kind::Angle:
        return std::nullopt;
    }
  }
  const std::optional<float> alpha = alphaOf(arguments.alpha);
  if (!alpha) return std::nullopt;
  return Color{channels[0], channels[1], channels[2], *alpha};
}

/** `hsl()` and `hwb()`: a hue and two fractions written as percentages,
 *  or as bare numbers on 0..100 in the modern syntax. */
std::optional<Color> readHueFractions(const Arguments& arguments,
                                      ColorSpace space) {
  if (arguments.components.size() != 3) return std::nullopt;
  const std::optional<double> hue = hueOf(arguments.components[0]);
  const std::optional<double> second = scalarOf(arguments.components[1], 100.0);
  const std::optional<double> third = scalarOf(arguments.components[2], 100.0);
  const std::optional<float> alpha = alphaOf(arguments.alpha);
  if (!hue || !second || !third || !alpha) return std::nullopt;
  return spaces::fromSpace(space,
                           {*hue, std::clamp(*second, 0.0, 100.0) / 100.0,
                            std::clamp(*third, 0.0, 100.0) / 100.0},
                           *alpha);
}

/** `lab()`, `oklab()`, `lch()` and `oklch()`: a lightness, then two axes
 *  or a chroma and a hue, each percentage read against the reference
 *  CSS gives it. */
std::optional<Color> readLightness(const Arguments& arguments,
                                   ColorSpace space) {
  if (arguments.legacy || arguments.components.size() != 3) return std::nullopt;
  const bool oklabFamily =
      space == ColorSpace::Oklab || space == ColorSpace::Oklch;
  const double lightnessReference = oklabFamily ? 1.0 : 100.0;
  const bool polar = space == ColorSpace::Lch || space == ColorSpace::Oklch;
  // A percentage of an axis is of 125 in Lab and 0.4 in OKLab; of a
  // chroma, 150 in LCH and 0.4 in OKLCH.
  const double axisReference = oklabFamily ? 0.4 : (polar ? 150.0 : 125.0);
  const std::optional<double> lightness =
      scalarOf(arguments.components[0], lightnessReference);
  const std::optional<double> second =
      scalarOf(arguments.components[1], axisReference);
  const std::optional<double> third =
      polar ? hueOf(arguments.components[2])
            : scalarOf(arguments.components[2], axisReference);
  const std::optional<float> alpha = alphaOf(arguments.alpha);
  if (!lightness || !second || !third || !alpha) return std::nullopt;
  return spaces::fromSpace(space,
                           {std::clamp(*lightness, 0.0, lightnessReference),
                            polar ? std::max(*second, 0.0) : *second, *third},
                           *alpha);
}

/** `color(<space> c1 c2 c3 [/ alpha])` over the RGB and XYZ spaces. */
std::optional<Color> readColorFunction(std::string_view body) {
  body = trimmed(body);
  size_t end = 0;
  while (end < body.size() && !isBlank(body[end]) && body[end] != '/') ++end;
  const std::optional<ColorSpace> space = readColorSpace(body.substr(0, end));
  if (!space) return std::nullopt;
  switch (*space) {
    case ColorSpace::Lab:
    case ColorSpace::Lch:
    case ColorSpace::Oklab:
    case ColorSpace::Oklch:
    case ColorSpace::Hsl:
    case ColorSpace::Hwb:
      return std::nullopt;
    default:
      break;
  }
  const std::optional<Arguments> arguments = readArguments(body.substr(end));
  if (!arguments || arguments->legacy || arguments->components.size() != 3)
    return std::nullopt;
  double values[3];
  for (size_t index = 0; index < 3; ++index) {
    const std::optional<double> value =
        scalarOf(arguments->components[index], 1.0);
    if (!value) return std::nullopt;
    values[index] = *value;
  }
  const std::optional<float> alpha = alphaOf(arguments->alpha);
  if (!alpha) return std::nullopt;
  return spaces::fromSpace(*space, {values[0], values[1], values[2]}, *alpha);
}

/** @p text split at the commas outside any brackets. */
std::vector<std::string_view> topLevelParts(std::string_view text,
                                            char separator) {
  std::vector<std::string_view> parts;
  int depth = 0;
  size_t start = 0;
  for (size_t at = 0; at < text.size(); ++at) {
    const char character = text[at];
    if (character == '(') ++depth;
    if (character == ')') --depth;
    const bool splits =
        depth == 0 &&
        (separator == ' ' ? isBlank(character) : character == separator);
    if (!splits) continue;
    parts.push_back(text.substr(start, at - start));
    start = at + 1;
  }
  parts.push_back(text.substr(start));
  if (separator == ' ')
    std::erase_if(parts, [](std::string_view part) { return part.empty(); });
  return parts;
}

std::optional<Color> readText(std::string_view css);

/** One colour of a `color-mix()`, with the percentage written beside it
 *  before or after. */
struct MixEntry {
  Color color;
  std::optional<double> percentage;
};

std::optional<MixEntry> readMixEntry(std::string_view text) {
  const std::vector<std::string_view> words = topLevelParts(trimmed(text), ' ');
  if (words.empty() || words.size() > 2) return std::nullopt;
  MixEntry entry;
  std::string_view colorText = words[0];
  if (words.size() == 2) {
    std::optional<Component> percentage = readComponent(words[1]);
    colorText = words[0];
    if (!percentage) {
      percentage = readComponent(words[0]);
      colorText = words[1];
    }
    if (!percentage || percentage->kind != Component::Kind::Percentage ||
        percentage->value < 0.0 || percentage->value > 100.0)
      return std::nullopt;
    entry.percentage = percentage->value;
  }
  const std::optional<Color> color = readText(colorText);
  if (!color) return std::nullopt;
  entry.color = *color;
  return entry;
}

std::optional<Color> readColorMix(std::string_view body) {
  const std::vector<std::string_view> parts = topLevelParts(body, ',');
  if (parts.size() != 3) return std::nullopt;
  const std::vector<std::string_view> method =
      topLevelParts(trimmed(parts[0]), ' ');
  if (method.size() != 2 && method.size() != 4) return std::nullopt;
  if (method[0] != "in") return std::nullopt;
  const std::optional<ColorSpace> space = readColorSpace(method[1]);
  if (!space) return std::nullopt;
  HueArc arc = HueArc::Shorter;
  if (method.size() == 4) {
    if (spaces::hueIndex(*space) < 0 || method[3] != "hue") return std::nullopt;
    if (method[2] == "shorter")
      arc = HueArc::Shorter;
    else if (method[2] == "longer")
      arc = HueArc::Longer;
    else if (method[2] == "increasing")
      arc = HueArc::Increasing;
    else if (method[2] == "decreasing")
      arc = HueArc::Decreasing;
    else
      return std::nullopt;
  }
  const std::optional<MixEntry> first = readMixEntry(parts[1]);
  const std::optional<MixEntry> second = readMixEntry(parts[2]);
  if (!first || !second) return std::nullopt;
  // CSS's normalisation: an unwritten share is what the other leaves, two
  // unwritten are half each, and two that sum short of a whole scale up
  // and leave the shortfall as transparency.
  double firstShare = first->percentage.value_or(
      second->percentage ? 100.0 - *second->percentage : 50.0);
  double secondShare = second->percentage.value_or(100.0 - firstShare);
  const double sum = firstShare + secondShare;
  if (sum <= 0.0) return std::nullopt;
  const double alphaScale = std::min(sum / 100.0, 1.0);
  Color mixed = colorMix(first->color, second->color,
                         (float)(secondShare / sum), *space, arc);
  mixed.a = (float)(mixed.a * alphaScale);
  return mixed;
}

/** The reading of already-lowered, already-trimmed text. */
std::optional<Color> readText(std::string_view css) {
  css = trimmed(css);
  if (css.empty()) return std::nullopt;
  if (css.front() == '#') return readHex(css.substr(1));
  const size_t open = css.find('(');
  if (open == std::string_view::npos) {
    if (css == "transparent") return Color{0, 0, 0, 0};
    return namedColor(css);
  }
  if (css.back() != ')') return std::nullopt;
  const std::string_view name = css.substr(0, open);
  const std::string_view body = css.substr(open + 1, css.size() - open - 2);
  if (name == "color-mix") return readColorMix(body);
  if (name == "color") return readColorFunction(body);
  const std::optional<Arguments> arguments = readArguments(body);
  if (!arguments) return std::nullopt;
  if (name == "rgb" || name == "rgba") return readRgb(*arguments);
  if (name == "hsl" || name == "hsla") {
    // The legacy syntax writes saturation and lightness as percentages.
    if (arguments->legacy)
      for (size_t index = 1; index < arguments->components.size(); ++index)
        if (arguments->components[index].kind != Component::Kind::Percentage)
          return std::nullopt;
    return readHueFractions(*arguments, ColorSpace::Hsl);
  }
  if (name == "hwb") {
    if (arguments->legacy) return std::nullopt;
    return readHueFractions(*arguments, ColorSpace::Hwb);
  }
  if (name == "lab") return readLightness(*arguments, ColorSpace::Lab);
  if (name == "lch") return readLightness(*arguments, ColorSpace::Lch);
  if (name == "oklab") return readLightness(*arguments, ColorSpace::Oklab);
  if (name == "oklch") return readLightness(*arguments, ColorSpace::Oklch);
  return std::nullopt;
}

}  // namespace

std::optional<Color> readColor(std::string_view css) {
  // Hex and the names a palette is mostly written in are read without
  // the copy the case-folding needs.
  const std::string_view text = trimmed(css);
  if (!text.empty() && text.front() == '#') return readHex(text.substr(1));
  std::string lowered(text);
  for (char& character : lowered)
    character = (char)std::tolower((unsigned char)character);
  return readText(lowered);
}

Color parseColor(std::string_view css) {
  if (const std::optional<Color> color = readColor(css)) return *color;
  static std::mutex guard;
  static std::unordered_set<std::string> reported;
  bool first = false;
  {
    const std::lock_guard lock(guard);
    first = reported.emplace(css).second;
  }
  if (first)
    core::report("SigilMaterial",
                 "'" + std::string(css) +
                     "' is no CSS colour; it is painted opaque black.");
  return {0, 0, 0, 1};
}

}  // namespace sigil::material
