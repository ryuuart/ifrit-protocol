/** @file
 * grid_layouts — the three schemes that place a run of cards without a
 * row or a column, given the same twelve cards.
 *
 * A `LayoutScheme` returns one rect per child from the container size
 * and the children's MEASURED sizes, in a bounded second pass after
 * Yoga. The three here differ in what they do with that measurement.
 * `Grid` throws the measured size away and SIZES each card to its
 * cell span, so a card is whatever the module is. `Diagonal` keeps every
 * measured size and only moves the cards, each row's left edge riding
 * the same shear line a `skewX` would lean the verticals to. And
 * `BaselineGrid` keeps the sizes too but shifts each card DOWN so its
 * first text baseline lands on the next grid line — the only scheme that
 * reads `childBaselines`, which is why the twelve cards here are text
 * leaves at three different sizes rather than boxes: a box has no
 * baseline and falls back to its bottom edge.
 *
 * Read the third cell against its own rules. The cards are 12, 16 and
 * 20 px in turn and the rhythm is one distance, so the letters sit on
 * the drawn lines while the plates behind them do not line up at all.
 *
 * EDIT THESE FIRST
 *   kColumns, kRows, kGutter — the module the first cell is cut into.
 *   kRhythm — the baseline pitch, px.
 *   kSkewDeg — the shear the second cell's rows ride.
 */

// TAGS: Geometry/Layout

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 760};
constexpr float kCell = 328;
constexpr float kPicture = 424;
constexpr float kInset = 20;

constexpr int kColumns = 3;  // the module the first cell is cut into
constexpr int kRows = 4;
constexpr float kGutter = 10;
constexpr float kRhythm = 32;    // the baseline pitch, px
constexpr float kSkewDeg = -12;  // the shear the second cell's rows ride

constexpr SkColor4f kCard{0.17f, 0.18f, 0.21f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  look.spacing.captionGap = 8;
  return look;
}

/** The twelve cards, identical in every cell. Each is a TEXT leaf, so
 *  every scheme is handed a real first baseline; the three sizes cycle so
 *  the baseline rhythm has something to correct. */
std::vector<Element> cards() {
  static constexpr float kSizes[3] = {12, 16, 20};
  const sketch::kit::Theme& look = sketch::kit::theme();
  return each(12, [&look](int i) {
    return text((i < 9 ? "0" : "") + std::to_string(i + 1) + " Aa",
                look.mono(kSizes[i % 3], look.palette.figure))
        .padding({.top = 3, .right = 8, .bottom = 3, .left = 8})
        .fill(Fill::color(kCard));
  });
}

/** The rhythm the third cell snaps to, drawn so the reader can see which
 *  line each card's letters landed on. */
Element rhythmLines() {
  return kit::ladder({.count = 13,
                      .pitch = kRhythm,
                      .fill = Fill::color(sketch::kit::theme().palette.rule)})
      .absolute()
      .left(kInset)
      .top(kInset)
      .width(kCell - 2 * kInset);
}

Element specimen(Element placed, bool ruled = false) {
  return sketch::kit::well({.width = kCell, .height = kPicture, .padding = 0})
      .children({ruled ? rhythmLines() : box(),
                 placed.inset(kInset).children(cards())});
}

}  // namespace

struct GridLayouts {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "Three ways to place twelve cards",
         .subtitle = "The same text leaves at 12, 16 and 20 px · resize the "
                     "cells, shear a stack, or align the baselines",
         .footer = "The baseline rules share the layout's origin. Letters land "
                   "on the rules; the card edges need not align."},
        box().column().gap(26).children(
            {sketch::kit::comparison(
                 {.cases =
                      {{.title = "FILL THE MODULE",
                        .control = "3 columns × 4 rows · 10 px gutters",
                        .figure = specimen(layout(layouts::Grid{
                            .columns =
                                layouts::repeatTrack(kColumns, layouts::fr()),
                            .rows = layouts::repeatTrack(kRows, layouts::fr()),
                            .gap = {kGutter, kGutter}})),
                        .note = "Every card takes its cell's width and height. "
                                "Type size stays unchanged."},
                       {.title = "FOLLOW A SHEAR",
                        .control = "−12° · 6 px between cards",
                        .figure = specimen(layout(
                            layouts::Diagonal{.skewDeg = kSkewDeg, .gap = 6})),
                        .note = "Measured sizes stay intact. Each left edge "
                                "follows the same sloping line."},
                       {.title = "SHARE A BASELINE RHYTHM",
                        .control = "32 px pitch · mixed type sizes",
                        .figure = specimen(
                            layout(layouts::BaselineGrid{.rhythm = kRhythm}),
                            true),
                        .note =
                            "Each first baseline moves down to the next rule. "
                            "The spacing absorbs the difference in size."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::readout(
                 {{.name = "Input", .value = "12 measured text leaves"},
                  {.name = "Grid changes", .value = "position + extent"},
                  {.name = "Diagonal / baseline change",
                   .value = "position only"}},
                 {.measure = 501, .ruled = true})})));
  }
};

SIGIL_SKETCH(GridLayouts, "Kit · API",
             "the same twelve cards under the modular grid, the sheared "
             "stack and the baseline rhythm, so the one thing each scheme "
             "decides is the only difference between them")
