/** @file
 * grid_layouts — the three schemes that place a run of cards without a
 * row or a column, given the same twelve cards.
 *
 * A `LayoutScheme` returns one rect per child from the container size
 * and the children's MEASURED sizes, in a bounded second pass after
 * Yoga. The three here differ in what they do with that measurement.
 * `ModularGrid` throws the measured size away and SIZES each card to its
 * cell span, so a card is whatever the module is. `Diagonal` keeps every
 * measured size and only moves the cards, each row's left edge riding
 * the same shear line a `skewX` would lean the verticals to. And
 * `BaselineGrid` keeps the sizes too but shifts each card DOWN so its
 * first text baseline lands on the next grid line — the only scheme that
 * reads `childBaselines`, which is why the twelve cards here are text
 * leaves at three different sizes rather than boxes: a box has no
 * baseline and falls back to its bottom edge.
 *
 * Read the third cell against its own rules. The cards are 11, 14 and
 * 18 px in turn and the rhythm is one distance, so the letters sit on
 * the drawn lines while the plates behind them do not line up at all.
 *
 * EDIT THESE FIRST
 *   kColumns, kRows, kGutter — the module the first cell is cut into.
 *   kRhythm — the baseline pitch, px.
 *   kSkewDeg — the shear the second cell's rows ride.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 622};
constexpr float kCell = 332;
constexpr float kPicture = 430;
constexpr float kInset = 12;

constexpr int kColumns = 3;  // the module the first cell is cut into
constexpr int kRows = 4;
constexpr float kGutter = 10;
constexpr float kRhythm = 32;    // the baseline pitch, px
constexpr float kSkewDeg = -12;  // the shear the second cell's rows ride

constexpr SkColor4f kCard{0.17f, 0.18f, 0.21f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  look.type.captionNote = {.size = 10.5f, .track = 0.2f};
  look.spacing.captionGap = 8;
  return look;
}

/** The twelve cards, identical in every cell. Each is a TEXT leaf, so
 *  every scheme is handed a real first baseline; the three sizes cycle so
 *  the baseline rhythm has something to correct. */
std::vector<Element> cards() {
  static constexpr float kSizes[3] = {11, 14, 18};
  std::vector<Element> made;
  made.reserve(12);
  for (int i = 0; i < 12; ++i) {
    const std::string digits = (i < 9 ? "0" : "") + std::to_string(i + 1);
    made.push_back(
        text(toU8(digits), sketch::kit::theme().mono(kSizes[i % 3], kFigure))
            .padding(8, 4, 8, 4)
            .fill(Fill::color(kCard)));
  }
  return made;
}

/** The rhythm the third cell snaps to, drawn so the reader can see which
 *  line each card's letters landed on. */
Element rhythmLines() {
  return custom("grid_layouts.rhythm",
                [](SkCanvas& canvas, const PaintContext& pc) {
                  SkPaint paint;
                  paint.setColor4f(kRule);
                  for (float y = kRhythm; y < pc.size.height(); y += kRhythm)
                    canvas.drawRect({0, y, pc.size.width(), y + 1}, paint);
                })
      .absolute()
      .inset(0);
}

Element cell(const char* call, const char* note, Element placed,
             bool ruled = false) {
  Element plate = sketch::kit::well({.width = kCell, .height = kPicture});
  if (ruled) plate.child(rhythmLines());
  plate.child(placed.absolute().inset(kInset).children(cards()));
  return sketch::kit::caption(kCell, toU8(call), toU8(note), std::move(plate));
}

}  // namespace

struct GridLayouts final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("GRID LAYOUTS \xc2\xb7 layout(layouts::"
                       "ModularGrid | Diagonal | BaselineGrid)"),
         .subtitle = toU8("dials \xc2\xb7 the module (3 columns "
                          "\xc3\x97 4 rows, 10 px gutter) \xc2\xb7 the "
                          "baseline rhythm (32 px) \xc2\xb7 the shear "
                          "(\xe2\x88\x92"
                          "12\xc2\xb0) \xc2\xb7 the same "
                          "twelve cards in all three"),
         .footer = toU8("a scheme is arithmetic over LayoutInput, so "
                        "each of these caches like any other static "
                        "subtree \xe2\x80\x94 and only BaselineGrid "
                        "reads childBaselines, which a box does not "
                        "have")},
        kit::cells(
            {.cells = {cell("layouts::ModularGrid{3, 4, 10}",
                            "the card is SIZED to its cell \xc2\xb7 twelve "
                            "with no spans auto-flow one module each, "
                            "left to right then down",
                            layout(layouts::ModularGrid{.columns = kColumns,
                                                        .rows = kRows,
                                                        .gutter = kGutter})),
                       cell("layouts::Diagonal{-12, 6}",
                            "measured sizes kept \xc2\xb7 x tracks the shear "
                            "line at each row's y, and the run is shifted so "
                            "nothing lands at negative x",
                            layout(layouts::Diagonal{.skewDeg = kSkewDeg,
                                                     .gap = 6})),
                       cell("layouts::BaselineGrid{32}",
                            "each card falls to the next 32 px line by its "
                            "own FIRST BASELINE \xc2\xb7 three type sizes, "
                            "one rhythm",
                            layout(layouts::BaselineGrid{.rhythm = kRhythm}),
                            true)},
             .gap = 16})));
  }
};

SIGIL_SKETCH(GridLayouts, "Kit \xc2\xb7 API",
             "the same twelve cards under the modular grid, the sheared "
             "stack and the baseline rhythm, so the one thing each scheme "
             "decides is the only difference between them")
