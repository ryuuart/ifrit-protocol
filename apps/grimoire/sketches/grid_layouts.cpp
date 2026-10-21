/** @file
 * grid_layouts — four schemes that place a run of cards, given the same
 * twelve cards: three arrangers that place them without a row or a
 * column, and Karl Gerstner's 58-unit programme run through its six
 * configurations.
 *
 * An arranger writes each child record's rect from the container box
 * and the children's measured sizes, in a bounded second pass after
 * Yoga. The three here differ in what they do with that measurement.
 * `Grid` throws the measured size away and SIZES each card to its
 * cell span, so a card is whatever the module is. `Diagonal` keeps every
 * measured size and only moves the cards, each row's left edge riding
 * the same shear line a `skewX` would lean the verticals to. And
 * `BaselineGrid` keeps the sizes too but shifts each card DOWN so its
 * first text baseline lands on the next grid line — the only scheme that
 * reads each child's `baseline`, which is why the twelve cards here are text
 * leaves at three different sizes rather than boxes: a box has no
 * baseline and falls back to its bottom edge.
 *
 * Read the third cell against its own rules. The cards are 12, 16 and
 * 20 px in turn and the rhythm is one distance, so the letters sit on
 * the drawn lines while the plates behind them do not line up at all.
 *
 * The fourth cell is Gerstner's mobile grid for Capital (1962): a measure
 * of 58 units, which is the number that divides into one to six columns
 * while always leaving two units between them — 58, 2 × 28 + 2,
 * 3 × 18 + 2 × 2, 4 × 13 + 3 × 2, 5 × 10 + 4 × 2, 6 × 8 + 5 × 2. A grid
 * is a programme rather than a drawing, so the cell runs it: every few
 * seconds the programme steps to the next configuration and the same
 * twelve cards reflow into it. The step is a snap, because the six are
 * alternative layouts, not frames of one.
 *
 * EDIT THESE FIRST
 *   kColumns, kRows, kGutter — the module the first cell is cut into.
 *   kRhythm — the baseline pitch, px.
 *   kSkewDegrees — the shear the second cell's rows ride.
 *   kProgramme, kHoldSeconds — the six configurations and how long each
 *                              stands before the programme steps.
 */

// TAGS: Geometry/Layout

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <iterator>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1460, 760};
constexpr float kCell = 328;
constexpr float kPicture = 424;
constexpr float kInset = 20;

constexpr int kColumns = 3;  // the module the first cell is cut into
constexpr int kRows = 4;
constexpr float kGutter = 10;
constexpr float kRhythm = 32;        // the baseline pitch, px
constexpr float kSkewDegrees = -12;  // the shear the second cell's rows ride

constexpr material::Color kCard{0.17f, 0.18f, 0.21f, 1};

/** THE PROGRAMME: a measure of 58 units, cut into columns of `width`
 *  units with two units between them. */
constexpr int kMeasureUnits = 58, kGutterUnits = 2;
struct Configuration {
  int columns, width;
  const char* arithmetic;
};
constexpr Configuration kProgramme[] = {
    {1, 58, "58"},
    {2, 28, "2 × 28 + 1 × 2 = 58"},
    {3, 18, "3 × 18 + 2 × 2 = 58"},
    {4, 13, "4 × 13 + 3 × 2 = 58"},
    {5, 10, "5 × 10 + 4 × 2 = 58"},
    {6, 8, "6 × 8 + 5 × 2 = 58"},
};
constexpr int kConfigurations = int(std::size(kProgramme));
/** The configuration the programme opens on: four columns, Capital's
 *  own default. */
constexpr int kOpening = 3;
constexpr double kHoldSeconds = 2.6;

/** The configuration standing at @p seconds. */
int configurationAt(double seconds) {
  return (kOpening + int(seconds / kHoldSeconds)) % kConfigurations;
}

/** One configuration as a grid: its columns a whole number of units
 *  wide, two units between them, and as many rows as the twelve cards
 *  need. */
layouts::Grid programmeGrid(const Configuration& configuration) {
  const float unit = (kCell - 2 * kInset) / kMeasureUnits;
  const int rows = (12 + configuration.columns - 1) / configuration.columns;
  return layouts::Grid{
      .columns = layouts::repeatTrack(configuration.columns,
                                      layouts::px(configuration.width * unit)),
      .rows = layouts::repeatTrack(rows, layouts::fr()),
      .gap = {kGutterUnits * unit, kGutter}};
}

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
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
        .padding(3, 8)
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
  int shown = -1;

  void setup(sketch::Scene2DContext& ctx) {
    // Only the programme moves, a step at a time; the still is taken in
    // its opening configuration.
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    show(ctx, kOpening);
  }

  /** Stepping the programme is a DATA change, so the sheet is described
   *  again and the reconciler moves the cards. */
  void update(double elapsed, sketch::Scene2DContext& ctx) {
    const int configuration = configurationAt(elapsed);
    if (configuration != shown) show(ctx, configuration);
  }

  void show(sketch::Scene2DContext& ctx, int configuration) {
    shown = configuration;
    const sketch::kit::Provide look(sheetTheme());
    const Configuration& running = kProgramme[configuration];
    ctx.composer.render(sketch::kit::page(
        {.title = "Four ways to place twelve cards",
         .subtitle = "The same text leaves at 12, 16 and 20 px · resize the "
                     "cells, shear a stack, align the baselines, or run a "
                     "programme",
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
                        .figure = specimen(layout(layouts::Diagonal{
                            .skewDegrees = kSkewDegrees, .gap = 6})),
                        .note = "Measured sizes stay intact. Each left edge "
                                "follows the same sloping line."},
                       {.title = "SHARE A BASELINE RHYTHM",
                        .control = "32 px pitch · mixed type sizes",
                        .figure = specimen(
                            layout(layouts::BaselineGrid{.rhythm = kRhythm}),
                            true),
                        .note =
                            "Each first baseline moves down to the next rule. "
                            "The spacing absorbs the difference in size."},
                       {.title = "RUN THE 58-UNIT PROGRAMME",
                        .control = running.arithmetic,
                        .figure = specimen(layout(programmeGrid(running))),
                        .note = "One to six columns, always two units apart. "
                                "The programme steps; the cards reflow."}},
                  .measure = 1366,
                  .gap = 18}),
             sigil::compose::kit::readout(
                 std::vector<sigil::compose::kit::Reading>{
                     {.name = "Input", .value = "12 measured text leaves"},
                     {.name = "Grid and programme change",
                      .value = "position + extent"},
                     {.name = "Diagonal / baseline change",
                      .value = "position only"}},
                 {.measure = 501,
                  .gap = sketch::kit::theme().spacing.rowGap,
                  .labelGap = sketch::kit::theme().spacing.labelGap,
                  .divider =
                      Fill::color(sketch::kit::theme().palette.rule)})})));
  }
};

SIGIL_SKETCH(GridLayouts, "Kit · API",
             "the same twelve cards under the modular grid, the sheared "
             "stack, the baseline rhythm and Gerstner's 58-unit programme "
             "stepping through one to six columns, so the one thing each "
             "scheme decides is the only difference between them")
