/** Sixteen letterforms occupy one modular grid. Each row gives its word
 *  a different deformation while the cell boundaries stay registered. */
// TAGS: Studies/Graphic Design, Typography/Motion, Layout/Grid, Motion/Bindings

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
const auto paper = material::hexColor(0xEEEADB);
const auto blue = material::hexColor(0x173ACB);
const auto acid = material::hexColor(0xE4F95A);
const auto black = material::hexColor(0x172220);

Text type(std::string value, float size, material::Color ink = black,
          bool heavy = false) {
  return text(std::move(value))
      .font({.face = weave::ports::face({"Helvetica Neue", "Arial"},
                                        heavy ? 800 : 400),
             .size = size,
             .track = heavy ? -size * 0.035f : 0.0f})
      .ink(ink);
}
}  // namespace

struct FormSystem {
  std::array<motion::Animatable<float>, 16> horizontal;
  std::array<motion::Animatable<float>, 16> vertical;
  std::array<motion::Animatable<float>, 16> turn;

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 1120);
    ctx.background(paper);
    ctx.captureAt(2.7);
    std::vector<Element> letters;
    const std::array<std::string, 4> words{"FORM", "FOLD", "FLOW", "FLUX"};
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        const int index = row * 4 + column;
        horizontal[index] = motion::animatable(1.0f);
        vertical[index] = motion::animatable(1.0f);
        turn[index] = motion::animatable(0.0f);
        const bool light = (row + column) % 3 == 0;
        auto glyph =
            type(words[row].substr(column, 1), 214, light ? blue : acid, true)
                .scaleX(horizontal[index])
                .scaleY(vertical[index])
                .rotate(turn[index])
                .translateY(-24);
        letters.push_back(kit::centred(std::move(glyph))
                              .gridCells(column, row)
                              .fill(light ? acid : blue)
                              .foreground(stroke(1, Fill::color(paper))));
      }
    }
    const auto fraction = layouts::Track::fr();
    auto matrix =
        layout(
            layouts::Grid{.columns = {fraction, fraction, fraction, fraction},
                          .rows = {fraction, fraction, fraction, fraction}})
            .width(1320)
            .height(844)
            .children(std::move(letters));
    std::vector<Element> poster{
        kit::at(type("FORM / SYSTEM", 65, black, true), 57, 27, 930, 93),
        kit::at(
            type("A MODULAR TYPE SCORE\nSIXTEEN CELLS / FOUR MOVEMENTS", 14),
            1090, 54, 301, 44),
        kit::at(std::move(matrix), 60, 156, 1320, 844),
        kit::at(type("01   EXPAND", 15, black, true), 61, 1030, 320, 26),
        kit::at(type("02   TURN", 15, black, true), 391, 1030, 320, 26),
        kit::at(type("03   COMPRESS", 15, black, true), 721, 1030, 320, 26),
        kit::at(type("04   COUNTERPOINT", 15, black, true), 1051, 1030, 331,
                26),
        kit::at(
            type("THE GRID IS THE SCORE. THE LETTER IS THE INSTRUMENT.", 12),
            61, 1082, 1020, 21),
        kit::at(type("FS / 16", 12), 1311, 1082, 100, 21)};
    for (auto& element : poster) element.absolute();
    ctx.composer.render(
        box().width(1440).height(1120).children(std::move(poster)));
  }

  void update(double elapsed) {
    for (int row = 0; row < 4; ++row) {
      for (int column = 0; column < 4; ++column) {
        const int index = row * 4 + column;
        const float wave =
            std::sin(float(elapsed) * 1.047198f - column * 0.72f - row * 0.4f);
        horizontal[index] = row == 0   ? 0.88f + wave * 0.16f
                            : row == 3 ? 0.85f - wave * 0.15f
                                       : 1.0f;
        vertical[index] = row == 2   ? 0.82f + wave * 0.17f
                          : row == 3 ? 0.85f + wave * 0.15f
                                     : 0.94f;
        turn[index] = row == 1 ? wave * 15.0f : row == 3 ? -wave * 8.0f : 0.0f;
      }
    }
  }
};

SIGIL_SKETCH(FormSystem, "Study · Type",
             "A kinetic modular type poster: FORM, FOLD, FLOW and FLUX in "
             "sixteen coupled cells")
