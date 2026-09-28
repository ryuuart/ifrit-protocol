/** Modular lettering built from joined cells. A large letter separates into
 *  printing strips above two complete words and their exposed construction
 * grid. */
// TAGS: Studies/Graphic Design, Typography/Motion, Typography/Lettering,
// Layout/Grid, Geometry/Booleans, Motion/Bindings

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace geometry = sigil::geometry;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xE9EADD);
const auto black = material::hexColor(0x172321);
const auto green = material::hexColor(0xC8D745);

// The letter cells belong to this alphabet: stepped joins, square counters
// and a descending g. The contour union removes every internal cell edge.
std::vector<std::string_view> cells(char c) {
  switch (c) {
    case 'v':
      return {"10001", "10001", "10001", "10010", "11100"};
    case 'o':
      return {"01110", "10001", "10001", "10001", "01110"};
    case 'r':
      return {"00111", "01000", "10000", "10000", "10000"};
    case 'm':
      return {"0110110", "1001001", "1001001", "1001001", "1001001"};
    case 'g':
      return {"01110", "10001", "10001", "01111", "00001", "11110"};
    case 'e':
      return {"01110", "10001", "11111", "10000", "01111"};
    case 's':
      return {"01111", "10000", "01110", "00001", "11110"};
    default:
      return {};
  }
}

geometry::path::Outline letter(char c) {
  std::vector<geometry::path::Outline> parts;
  const auto rows = cells(c);
  for (size_t y = 0; y < rows.size(); ++y)
    for (size_t x = 0; x < rows[y].size(); ++x)
      if (rows[y][x] == '1')
        parts.push_back(geometry::path::Outline::rectangle(
            geometry::path::Rect::of({float(x), float(y)}, {1, 1})));
  // A diagonal step shares an optical joint, so its two cells remain one
  // continuous stroke after the corners are rounded.
  for (size_t y = 0; y + 1 < rows.size(); ++y) {
    for (size_t x = 0; x + 1 < rows[y].size(); ++x) {
      if ((rows[y][x] == '1' && rows[y + 1][x + 1] == '1') ||
          (rows[y][x + 1] == '1' && rows[y + 1][x] == '1'))
        parts.push_back(
            geometry::path::Outline::rectangle(geometry::path::Rect::of(
                {float(x) + 0.72f, float(y) + 0.72f}, {0.56f, 0.56f})));
    }
  }
  return geometry::path::operations::roundCorners(
      geometry::path::operations::unite(parts), 0.15f);
}

geometry::path::Outline word(std::string_view letters, float unit) {
  std::vector<geometry::path::Outline> parts;
  float cursor = 0;
  for (char c : letters) {
    parts.push_back(letter(c).transformed(
        geometry::path::Transform::translate({cursor, 0}) *
        geometry::path::Transform::scale({unit, unit})));
    cursor += (float(cells(c).front().size()) + 0.5f) * unit;
  }
  return geometry::path::operations::unite(parts);
}

Text type(std::string words, float size, material::Color ink = black,
          bool heavy = false) {
  return text(std::move(words))
      .font({.face = weave::ports::face(
                 {"Helvetica Neue", "Helvetica", "Arial"}, heavy ? 700 : 400),
             .size = size,
             .track = heavy ? -size * 0.03f : 0.2f})
      .ink(ink);
}

Element figure(const geometry::path::Outline& outline, float x, float y,
               material::Color ink) {
  return kit::at(x, y, outline.bounds().right(), outline.bounds().bottom())
      .shape(heldPath(outline))
      .fill(ink);
}
}  // namespace

struct FormSystem {
  std::array<motion::Animatable<float>, 6> separation;
  motion::Animatable<float> registration = motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1000, 1480);
    ctx.background(paper);
    ctx.captureAt(3.4);
    const auto groot =
        letter('g').transformed(geometry::path::Transform::scale({80, 80}));
    const auto vorm = word("vorm", 39);
    const auto gevers = word("gevers", 28.5f);
    std::vector<Element> fixed{
        kit::at(0, 0, 1000, 1480).fill(paper),
        kit::at(22, 22, 956, 794).fill(black),
        kit::at(type("stedelijk museum\namsterdam", 42, paper, true), 42, 39,
                765, 108),
        kit::at(type("5 april t/m\n23 juni 1968", 21, paper), 767, 52, 208, 65),
        kit::at(type("vormgevers", 18, paper), 43, 173, 600, 33),
        kit::at(type("01 / LETTER IN CONSTRUCTION", 11, paper), 653, 182, 310,
                23),
        figure(vorm, 42, 881, black),
        figure(gevers, 42, 1135, black),
        kit::at(type("vorm / gevers", 15, black, true), 44, 1358, 409, 27),
        kit::at(
            type("een raster wordt een letter\neen letter wordt een beeld", 16),
            478, 1355, 482, 50),
        kit::at(type("AFTER WIM CROUWEL / VORMGEVERS, 1968", 10), 44, 1433, 626,
                22),
        kit::at(type("FORM / SYSTEM", 10), 842, 1433, 130, 22),
    };

    // The doubled rules expose the cell and its print tolerance on both stocks.
    for (int i = 0; i < 36; ++i) {
      const float x = 28 + i * 27.0f;
      fixed.push_back(
          kit::at(x, 23, 0.6f, 1411).fill(material::hexColor(0x8B9887, 0.46f)));
      fixed.push_back(kit::at(x + 3.0f, 23, 0.45f, 1411)
                          .fill(material::hexColor(0x8B9887, 0.19f)));
    }
    for (int i = 0; i < 52; ++i) {
      const float y = 28 + i * 27.0f;
      fixed.push_back(
          kit::at(23, y, 954, 0.6f).fill(material::hexColor(0x8B9887, 0.46f)));
      fixed.push_back(kit::at(23, y + 3, 954, 0.45f)
                          .fill(material::hexColor(0x8B9887, 0.19f)));
    }

    // Repeated contours show the same letter receding into its construction.
    for (int depth = 13; depth >= 1; --depth) {
      auto outline =
          figure(groot, 360 - depth * 12.0f, 267 - depth * 4.8f, {0, 0, 0, 0});
      outline.stroke(stroke(depth == 13 ? 1.3f : 0.75f,
                            Fill::color(material::hexColor(0xCBD6B9, 0.44f))));
      fixed.push_back(std::move(outline));
    }
    for (int marker = 0; marker < 6; ++marker) {
      fixed.push_back(kit::at(type("0" + std::to_string(marker + 1), 10, paper),
                              49, 277 + marker * 80.0f, 34, 20));
      fixed.push_back(kit::at(86, 284 + marker * 80.0f, 167, 0.65f)
                          .fill(material::hexColor(0xD9E2CA, 0.6f)));
      fixed.push_back(kit::at(250, 282 + marker * 80.0f, 4, 4)
                          .shape(geometry::shapes::circle())
                          .fill(green));
    }
    fixed.push_back(kit::at(type("g", 72, paper, true), 58, 703, 78, 94));
    fixed.push_back(kit::at(type("SIX ROWS\nONE CONTINUOUS CONTOUR", 11, paper),
                            105, 749, 280, 35));
    fixed.push_back(kit::at(type("02 / POSITIVE", 10), 43, 848, 353, 20));
    fixed.push_back(kit::at(type("03 / COUNTERFORM", 10), 44, 1100, 600, 20));
    auto sheet = positioned()
                     .width(1000)
                     .height(1480)
                     .children(std::move(fixed))
                     .cache(Cache::Texture);

    std::vector<Element> live{sheet};
    auto secondRegister = figure(vorm, 45, 884, green)
                              .translateX(registration)
                              .opacity(0.72f)
                              .blendMode(material::BlendMode::Multiply);
    live.push_back(std::move(secondRegister));
    for (int row = 0; row < 6; ++row) {
      separation[row] = motion::animatable(0.0f);
      const auto slice = groot.intersected(geometry::path::Outline::rectangle(
          geometry::path::Rect::of({0, row * 80.0f}, {400, 80})));
      live.push_back(kit::at(360, 267, 400, 480)
                         .shape(heldPath(slice))
                         .fill(row == 2 ? green : paper)
                         .translateX(separation[row]));
    }
    // Small register crosses remain tied to the cell intersections.
    for (const auto point : std::array<glm::vec2, 4>{
             glm::vec2{351, 255}, {770, 255}, {351, 761}, {770, 761}}) {
      live.push_back(kit::at(point.x - 7, point.y, 15, 1).fill(green));
      live.push_back(kit::at(point.x, point.y - 7, 1, 15).fill(green));
    }
    live.push_back(
        kit::at(0, 0, 1000, 1480)
            .fill(material::noise(0.65f, {.seed = 17, .grain = true}))
            .cache(Cache::Texture)
            .blendMode(material::BlendMode::SoftLight)
            .opacity(0.13f));
    ctx.composer.render(
        positioned().width(1000).height(1480).children(std::move(live)));
  }

  void update(double elapsed) {
    const float t = float(elapsed);
    const float opening =
        std::pow(0.5f + 0.5f * std::sin(t * 0.65f - 1.2f), 3.0f);
    for (int row = 0; row < 6; ++row)
      separation[row] = opening * ((row % 2 == 0 ? -1 : 1) * (15 + row * 6.0f));
    registration = 2.5f + 3.5f * std::sin(t * 0.65f);
  }
};

SIGIL_SKETCH(
    FormSystem, "Study · Type",
    "After Crouwel's Vormgevers: drawn modular alphabet, rounded "
    "contour unions, exploded letter and overprinted construction grid")
