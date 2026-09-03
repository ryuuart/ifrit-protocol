/** @file
 * live_settling — a passage told that its measure is moving, and what a
 * frame got for it.
 *
 * `Element::live` says AN INPUT OF THIS PASSAGE IS MOVING — a measure
 * that animates, a frame that grows, content that changes from one frame
 * to the next — so this layout is one of a run of them rather than an
 * answer somebody asked for once. NOTHING INFERS IT: a live layout
 * answers the overflow tail differently from a settled one, because it is
 * broken against the MEASURE rather than against the lines the frame has
 * left, so a guess would change the setting of a page that never moves. A
 * passage that moves says so.
 *
 * It buys two things. The break decisions of a block set in a uniform
 * measure are kept and reused, keyed on the words and on the measure
 * taken to the whole pixel below it, so a measure already crossed costs
 * no break decision at all. And a frame that changes only in DEPTH
 * changes which lines it holds and never where they break.
 *
 * `Composer::settling` reports what one frame actually got: `reused` is
 * how many blocks came out of the store, `degraded` how many the budget
 * forced to the greedy breaker. A degrade drops the whole setting — the
 * hyphens, the justification passes, the widow rule — for that frame
 * alone, and the leaf lays out again so the setting comes back the frame
 * the budget is met. It is a REPORT about one input and not a verdict
 * about the node: the runtime holds one proof that a node has settled and
 * folds this into it beside everything else the node reads.
 *
 * The swell is run on a composer of its own, one whole pixel at a time
 * across the range, and the reports are read after it — so the numbers
 * are the numbers a real swell produces and not a description of them.
 * What those numbers are here: at either end of the range the block
 * comes back out of the store, so the report answers `reused` 1 and the
 * frame costs no break decision; the passage that never declared itself
 * live answers `live` false and `reused` 0, having decided its breaks
 * again every frame; and under a floor of one microsecond the block is
 * filled greedily and the report answers `degraded` 1.
 *
 * EDIT THESE FIRST
 *   kNarrow, kWide — the measure the swell runs between, px.
 *   kBudget — the floor under a frame the optimizing breaker cannot meet.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkSurface.h>

#include <cstdio>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 210;

constexpr float kNarrow = 150;   // the measure the swell runs from
constexpr float kWide = 230;     // …and to
constexpr float kBudget = 4000;  // the frame's floor, microseconds
constexpr float kStarved = 1;    // a floor nothing can meet

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBody{0.84f, 0.85f, 0.88f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = 11.5f, .color = kBody});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

const char* kPassage =
    "A measure that animates is one input of a run of layouts rather than "
    "a question somebody asked once, and the block that knows so keeps "
    "the break decisions it has already made.";

/** The passage at one measure. `live` and the budget are what the cells
 *  vary; everything else is one setting. */
Element passage(float measure, bool live, float budget) {
  Element leaf = text(toU8(kPassage), body())
                     .key("para")
                     .width(Dim(measure))
                     .lineBreak(weave::LineBreakStrategy::kKnuthPlass);
  if (live) leaf.live(true, budget);
  return leaf;
}

std::string line(const char* format, auto... args) {
  char buffer[128];
  std::snprintf(buffer, sizeof buffer, format, args...);
  return buffer;
}

}  // namespace

struct LiveSettling final : sketch::Sketch {
  std::string reports[4];

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // the swell has already been run, on its own composer

    // THE SWELL, on a composer of its own: every whole pixel from the
    // narrow measure to the wide one and back, drawn each time, because
    // neither the store nor the report exists until a frame has been
    // drawn. The report read afterwards is the last frame's.
    const auto sweep = [&](bool live, float budget, float endAt) {
      Composer probe(ctx.ticker, *ctx.fonts);
      probe.setSize({kWide + 40, 320});
      sk_sp<SkSurface> scratch = SkSurfaces::Raster(
          SkImageInfo::MakeN32Premul((int)kWide + 40, 320));
      const auto step = [&](float measure) {
        probe.render(box().padding(10).child(passage(measure, live, budget)));
        if (scratch) probe.draw(*scratch->getCanvas());
      };
      for (float w = kNarrow; w <= kWide; w += 1) step(w);
      for (float w = kWide; w >= kNarrow; w -= 1) step(w);
      step(endAt);
      const TextSettling settled = probe.settling("para");
      return line("live %s \xc2\xb7 reused %d \xc2\xb7 degraded %d",
                  settled.live ? "true" : "false", settled.reused,
                  settled.degraded);
    };

    reports[0] = sweep(true, kBudget, kNarrow);
    reports[1] = sweep(true, kBudget, kWide);
    reports[2] = sweep(false, 0, kWide);
    reports[3] = sweep(true, kStarved, kWide);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("A MOVING MEASURE \xc2\xb7 Element::live, "
                           "Composer::settling"),
             .subtitle = toU8("dials \xc2\xb7 the measure the swell runs "
                              "between (150 to 230 px, one pixel at a step) "
                              "\xc2\xb7 the frame's budget (4000 \xc2\xb5s, "
                              "then 1)"),
             .footer = toU8("a settled passage reports nothing and answers "
                            "reused 0 \xe2\x80\x94 it decided its breaks "
                            "once and no later frame asks it again, which "
                            "is why live is DECLARED and never inferred"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("live(true, 4000) \xc2\xb7 at the narrow end",
                           "the swell has crossed this measure before "
                           "\xc2\xb7 the block comes back out of the store, "
                           "so this frame costs no break decision at all",
                           kNarrow, true, kBudget, reports[0]),
                      cell("live(true, 4000) \xc2\xb7 at the wide end",
                           "the other end of the range, reached from "
                           "the narrow one \xc2\xb7 the decisions are keyed "
                           "on the words and on the measure taken to the "
                           "whole pixel below it",
                           kWide, true, kBudget, reports[1]),
                      cell("no live() at all",
                           "the same swell run on a passage that never said "
                           "its input moves \xc2\xb7 it decides its breaks "
                           "again every frame and stores nothing",
                           kWide, false, 0, reports[2]),
                      cell("live(true, 1)",
                           "a floor no optimizing break can meet "
                           "\xc2\xb7 the block is filled greedily for this "
                           "frame and counted, and the setting comes back "
                           "the frame the budget is met",
                           kWide, true, kStarved, reports[3])},
                 .gap = 14}))
            .absolute()
            .inset(0));
  }

  /** One cell: the passage set at its own measure, with the report the
   *  swell produced printed under it. */
  Element cell(const char* call, const char* note, float measure, bool live,
               float budget, const std::string& report) {
    return kit::cell(
        voice(), toU8(call), toU8(note),
        box()
            .width(Dim(kCell))
            .height(Dim(kPicture))
            .clip()
            .fill(Fill::color(kCellGround))
            .padding(12)
            .column()
            .gap(10)
            .child(passage(measure, live, budget))
            .child(text(toU8(report), mono(10, kFigure))));
  }
};

SIGIL_SKETCH(LiveSettling, "Kit \xc2\xb7 API",
             "one passage swelled a pixel at a time between two measures, "
             "with the settling report each run produced printed under the "
             "setting it produced")
