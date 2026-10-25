/** @file
 * A gradient read from its CSS text: `linear-gradient()`,
 * `radial-gradient()` and `conic-gradient()`, and their `repeating-`
 * forms, onto the CSS-shaped gradient bases.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/CssText.h>
#include <sigilmaterial/paint/Bases.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace sigil::material {

namespace {

/** Which of the three gradients the text names. */
enum class Kind { Linear, Radial, Conic };

/** One stop as written: its colour and, where written, its position — a
 *  fraction of the gradient line, and pixels along it. */
struct WrittenStop {
  Color color;
  bool placed = false;
  float offset = 0.0f;
  float pixels = 0.0f;
};

/** A stop's position: a percentage, a length along the line, or — around
 *  a conic — an angle as its share of the turn. */
std::optional<WrittenStop> positionOf(std::string_view text, Kind kind) {
  WrittenStop at;
  at.placed = true;
  if (const std::optional<float> share = css::percentage(text)) {
    at.offset = *share;
    return at;
  }
  if (kind == Kind::Conic) {
    if (const std::optional<float> angle = css::degrees(text)) {
      at.offset = *angle / 360.0f;
      return at;
    }
    return std::nullopt;
  }
  if (const std::optional<float> length = css::pixels(text)) {
    at.pixels = *length;
    return at;
  }
  return std::nullopt;
}

/** One colour stop, `<colour> [<position> [<position>]]`, appended to
 *  @p stops: two positions are the colour twice, a hard band. A position
 *  alone is a colour hint, which a stop list here cannot hold. */
bool readStop(std::string_view text, Kind kind,
              std::vector<WrittenStop>& stops) {
  const std::vector<std::string_view> parts = css::components(text);
  if (parts.empty() || parts.size() > 3) return false;
  const std::optional<Color> color = readColor(parts.front());
  if (!color) return false;
  if (parts.size() == 1) {
    stops.push_back({.color = *color});
    return true;
  }
  for (size_t index = 1; index < parts.size(); ++index) {
    std::optional<WrittenStop> at = positionOf(parts[index], kind);
    if (!at) return false;
    at->color = *color;
    stops.push_back(*at);
  }
  return true;
}

/** CSS's colour stop fixup: an unplaced first stop is at the start and an
 *  unplaced last at the end; a stop placed before one ahead of it is
 *  moved up to it; and a run of unplaced stops is spread evenly between
 *  the placed ones around it. A position is a fraction of the line plus
 *  pixels along it, so a run between two positions is spread in both, and
 *  a stop is moved up only where both positions are fractions alone,
 *  since comparing a fraction with pixels needs the line's length. */
ColorStops fixedUp(std::vector<WrittenStop> stops) {
  if (!stops.front().placed) stops.front() = {stops.front().color, true, 0, 0};
  if (!stops.back().placed) stops.back() = {stops.back().color, true, 1, 0};
  float highest = stops.front().offset;
  bool highestInPixels = stops.front().pixels != 0.0f;
  for (WrittenStop& stop : stops) {
    if (!stop.placed) continue;
    const bool inPixels = stop.pixels != 0.0f;
    if (!inPixels && !highestInPixels && stop.offset < highest)
      stop.offset = highest;
    highest = stop.offset;
    highestInPixels = inPixels;
  }
  for (size_t index = 1; index + 1 < stops.size(); ++index) {
    if (stops[index].placed) continue;
    size_t next = index;
    while (!stops[next].placed) ++next;
    const WrittenStop& before = stops[index - 1];
    const WrittenStop& after = stops[next];
    const float count = (float)(next - index + 1);
    for (size_t run = index; run < next; ++run) {
      const float share = (float)(run - index + 1) / count;
      stops[run].offset =
          before.offset + (after.offset - before.offset) * share;
      stops[run].pixels =
          before.pixels + (after.pixels - before.pixels) * share;
      stops[run].placed = true;
    }
    index = next;
  }
  std::vector<ColorStop> fixed;
  fixed.reserve(stops.size());
  for (const WrittenStop& stop : stops)
    fixed.push_back({stop.offset, stop.color, stop.pixels});
  return ColorStops(std::move(fixed));
}

/** `to <side or corner>`, the words after `to` in either order. */
std::optional<Toward> towardOf(const std::vector<std::string_view>& words) {
  if (words.size() < 2 || words.size() > 3 || !css::isWord(words[0], "to"))
    return std::nullopt;
  bool top = false, bottom = false, left = false, right = false;
  for (size_t index = 1; index < words.size(); ++index) {
    const std::string_view word = words[index];
    bool* side = css::isWord(word, "top")      ? &top
                 : css::isWord(word, "bottom") ? &bottom
                 : css::isWord(word, "left")   ? &left
                 : css::isWord(word, "right")  ? &right
                                               : nullptr;
    if (side == nullptr || *side) return std::nullopt;
    *side = true;
  }
  if ((top && bottom) || (left && right)) return std::nullopt;
  if (top)
    return left ? Toward::TopLeft : right ? Toward::TopRight : Toward::Top;
  if (bottom)
    return left    ? Toward::BottomLeft
           : right ? Toward::BottomRight
                   : Toward::Bottom;
  return left ? Toward::Left : Toward::Right;
}

/** One coordinate of `at <position>`, as a fraction of the box. */
std::optional<float> coordinateOf(std::string_view word, bool horizontal) {
  if (const std::optional<float> share = css::percentage(word)) return *share;
  if (css::isWord(word, "center")) return 0.5f;
  if (horizontal && css::isWord(word, "left")) return 0.0f;
  if (horizontal && css::isWord(word, "right")) return 1.0f;
  if (!horizontal && css::isWord(word, "top")) return 0.0f;
  if (!horizontal && css::isWord(word, "bottom")) return 1.0f;
  if (const std::optional<double> zero = css::number(word))
    if (*zero == 0.0) return 0.0f;
  return std::nullopt;
}

/** CSS's `<position>` of one or two words, as fractions of the box: one
 *  word is that side or that coordinate with the other centred, and two
 *  keywords may come in either order. A length needs the box's size,
 *  which a gradient read on its own does not know. */
std::optional<glm::vec2> positionOf(
    const std::vector<std::string_view>& words) {
  const auto vertical = [](std::string_view word) {
    return css::isWord(word, "top") || css::isWord(word, "bottom");
  };
  const auto horizontal = [](std::string_view word) {
    return css::isWord(word, "left") || css::isWord(word, "right");
  };
  if (words.size() == 1) {
    if (vertical(words[0])) {
      const std::optional<float> y = coordinateOf(words[0], false);
      return glm::vec2{0.5f, *y};
    }
    const std::optional<float> x = coordinateOf(words[0], true);
    if (!x) return std::nullopt;
    return glm::vec2{*x, 0.5f};
  }
  if (words.size() != 2) return std::nullopt;
  std::string_view first = words[0], second = words[1];
  // Two keywords in either order: `top left` is `left top`.
  if (vertical(first) || horizontal(second)) std::swap(first, second);
  const std::optional<float> x = coordinateOf(first, true);
  const std::optional<float> y = coordinateOf(second, false);
  if (!x || !y) return std::nullopt;
  return glm::vec2{*x, *y};
}

/** The words before the stops of a radial gradient: its ending shape and
 *  extent in either order, then `at` and a position. A size written as a
 *  length is a size the ending shapes here do not take. */
std::optional<RadialEndingShape> radialPreludeOf(
    const std::vector<std::string_view>& words) {
  RadialEndingShape shape;
  bool namedShape = false, namedExtent = false;
  size_t index = 0;
  for (; index < words.size() && !css::isWord(words[index], "at"); ++index) {
    const std::string_view word = words[index];
    if (!namedShape &&
        (css::isWord(word, "circle") || css::isWord(word, "ellipse"))) {
      shape.shape = css::isWord(word, "circle") ? RadialShape::Circle
                                                : RadialShape::Ellipse;
      namedShape = true;
    } else if (!namedExtent && css::isWord(word, "closest-side")) {
      shape.extent = RadialExtent::ClosestSide;
      namedExtent = true;
    } else if (!namedExtent && css::isWord(word, "closest-corner")) {
      shape.extent = RadialExtent::ClosestCorner;
      namedExtent = true;
    } else if (!namedExtent && css::isWord(word, "farthest-side")) {
      shape.extent = RadialExtent::FarthestSide;
      namedExtent = true;
    } else if (!namedExtent && css::isWord(word, "farthest-corner")) {
      shape.extent = RadialExtent::FarthestCorner;
      namedExtent = true;
    } else {
      return std::nullopt;
    }
  }
  if (index < words.size()) {
    const std::optional<glm::vec2> at = positionOf(
        std::vector<std::string_view>(words.begin() + index + 1, words.end()));
    if (!at) return std::nullopt;
    shape.at = *at;
  }
  return shape;
}

/** The words before the stops of a conic gradient: `from <angle>`, then
 *  `at` and a position, either one alone. */
std::optional<ConicStart> conicPreludeOf(
    const std::vector<std::string_view>& words) {
  ConicStart start;
  size_t index = 0;
  if (index < words.size() && css::isWord(words[index], "from")) {
    if (index + 1 >= words.size()) return std::nullopt;
    const std::optional<float> angle = css::degrees(words[index + 1]);
    if (!angle) return std::nullopt;
    start.fromDegrees = *angle;
    index += 2;
  }
  if (index < words.size()) {
    if (!css::isWord(words[index], "at")) return std::nullopt;
    const std::optional<glm::vec2> at = positionOf(
        std::vector<std::string_view>(words.begin() + index + 1, words.end()));
    if (!at) return std::nullopt;
    start.at = *at;
  }
  return start;
}

/** Whether the first part of a gradient's body is what CSS writes before
 *  the stops rather than a stop itself. */
bool isPrelude(std::string_view part, Kind kind) {
  const std::vector<std::string_view> words = css::components(part);
  if (words.empty()) return false;
  const std::string_view first = words.front();
  switch (kind) {
    case Kind::Linear:
      return css::isWord(first, "to") || css::degrees(first).has_value();
    case Kind::Radial:
      return css::isWord(first, "at") || css::isWord(first, "circle") ||
             css::isWord(first, "ellipse") ||
             css::isWord(first, "closest-side") ||
             css::isWord(first, "closest-corner") ||
             css::isWord(first, "farthest-side") ||
             css::isWord(first, "farthest-corner");
    case Kind::Conic:
      return css::isWord(first, "from") || css::isWord(first, "at");
  }
  return false;
}

}  // namespace

std::optional<Material> readGradient(std::string_view text) {
  const std::optional<css::Function> written = css::function(text);
  if (!written) return std::nullopt;
  std::string_view name = written->name;
  GradientOptions options;
  constexpr std::string_view kRepeating = "repeating-";
  if (name.size() > kRepeating.size() &&
      css::isWord(name.substr(0, kRepeating.size()), kRepeating)) {
    options.repeat = Repeat::Repeat;
    name.remove_prefix(kRepeating.size());
  }
  Kind kind;
  if (css::isWord(name, "linear-gradient"))
    kind = Kind::Linear;
  else if (css::isWord(name, "radial-gradient"))
    kind = Kind::Radial;
  else if (css::isWord(name, "conic-gradient"))
    kind = Kind::Conic;
  else
    return std::nullopt;
  const std::optional<std::vector<std::string_view>> parts =
      css::commaParts(written->body);
  if (!parts || parts->empty()) return std::nullopt;
  size_t first = 0;
  std::vector<std::string_view> prelude;
  if (isPrelude(parts->front(), kind)) {
    prelude = css::components(parts->front());
    first = 1;
  }
  std::vector<WrittenStop> stops;
  for (size_t index = first; index < parts->size(); ++index)
    if (!readStop((*parts)[index], kind, stops)) return std::nullopt;
  if (stops.size() < 2) return std::nullopt;
  ColorStops fixed = fixedUp(std::move(stops));
  switch (kind) {
    case Kind::Linear: {
      LinearDirection direction;
      if (!prelude.empty()) {
        if (const std::optional<Toward> toward = towardOf(prelude))
          direction = *toward;
        else if (prelude.size() == 1 && css::degrees(prelude[0]))
          direction = *css::degrees(prelude[0]);
        else
          return std::nullopt;
      }
      return linearGradient(std::move(fixed), direction, options);
    }
    case Kind::Radial: {
      RadialEndingShape shape;
      if (!prelude.empty()) {
        const std::optional<RadialEndingShape> read = radialPreludeOf(prelude);
        if (!read) return std::nullopt;
        shape = *read;
      }
      return radialGradient(std::move(fixed), shape, options);
    }
    case Kind::Conic: {
      for (const ColorStop& stop : fixed.stops())
        if (stop.pixels != 0.0f) return std::nullopt;
      ConicStart start;
      if (!prelude.empty()) {
        const std::optional<ConicStart> read = conicPreludeOf(prelude);
        if (!read) return std::nullopt;
        start = *read;
      }
      return conicGradient(std::move(fixed), start, options);
    }
  }
  return std::nullopt;
}

}  // namespace sigil::material
