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
 * how many blocks came out of the store, `degraded` how many the floor
 * forced to the greedy breaker. That floor is a COUNT OF BREAK
 * CANDIDATES — how many lines the optimizing breaker may score for one
 * block before it gives that block to the greedy one — so what a frame
 * reports is a fact about the words and the measure rather than about the
 * machine. A degrade drops the whole setting — the hyphens, the
 * justification passes, the widow rule — for that frame alone, and the
 * leaf lays out again so the setting comes back the frame the floor is
 * met. It is a REPORT about one input and not a verdict about the node:
 * the runtime holds one proof that a node has settled and folds this into
 * it beside everything else the node reads.
 *
 * The swell is run on a composer of its own, one whole pixel at a time
 * across the range, and the reports are read after it — so the numbers
 * are the numbers a real swell produces and not a description of them.
 * What those numbers are here: at either end of the range the block
 * comes back out of the store, so the report answers `reused` 1 and the
 * frame costs no break decision; the passage that never declared itself
 * live answers `live` false and `reused` 0, having decided its breaks
 * again every frame; and under a floor of one candidate the block is
 * filled greedily and the report answers `degraded` 1.
 *
 * EDIT THESE FIRST
 *   kNarrow, kWide — the measure the swell runs between, px.
 *   kFloor — how many break candidates a frame of this block may weigh.
 */

// TAGS: Typography/Paragraph

#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 210;

constexpr float kNarrow = 150;  // the measure the swell runs from
constexpr float kWide = 230;    // …and to
constexpr int kFloor = 4000;    // the frame's floor, break candidates
constexpr int kStarved = 1;     // a floor nothing can meet

constexpr SkColor4f kBody{0.84f, 0.85f, 0.88f, 1};

weave::TextStyle body() {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = 11.5f, .color = kBody});
}

const char* kPassage =
    "A measure that animates is one input of a run of layouts rather than "
    "a question somebody asked once, and the block that knows so keeps "
    "the break decisions it has already made.";

/** The passage at one measure. `live` and the floor are what the cells
 *  vary; everything else is one setting. */
Element passage(float measure, bool live, int candidates) {
  Element leaf =
      text(kPassage, body())
          .key("para")
          .width(measure)
          .block({.lineBreak = weave::LineBreakStrategy::kKnuthPlass});
  if (live) leaf.live(true, candidates);
  return leaf;
}

/** THE FOUR RUNS: what each swell declares and where it ends — which is
 *  also the measure the cell beside it is set at, so the reading and the
 *  setting under it cannot drift apart. */
struct Run {
  const char* call;
  const char* note;
  float measure;
  bool live;
  int candidates;
};

constexpr Run kRuns[] = {
    {"live(true, 4000) · at the narrow end",
     "the swell has crossed this measure before · the block comes back out "
     "of the store, so this frame costs no break decision at all",
     kNarrow, true, kFloor},
    {"live(true, 4000) · at the wide end",
     "the other end of the range, reached from the narrow one · the "
     "decisions are keyed on the words and on the measure taken to the "
     "whole pixel below it",
     kWide, true, kFloor},
    {"no live() at all",
     "the same swell run on a passage that never said its input moves · it "
     "decides its breaks again every frame and stores nothing",
     kWide, false, 0},
    {"live(true, 1)",
     "a floor of one break candidate, which no block of more than one "
     "break position can meet · the block is filled greedily for this "
     "frame and counted, and the setting comes back the frame the floor is "
     "met",
     kWide, true, kStarved},
};

}  // namespace

struct LiveSettling {
  void setup(sketch::SketchContext& ctx) {
    // the swell has already been run, on its own composer
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE SWELL, on a composer of its own: every whole pixel from the
    // narrow measure to the wide one and back, drawn each time, because
    // neither the store nor the report exists until a frame has been
    // drawn. The report read afterwards is the last frame's.
    const auto sweep = [&](bool live, int candidates, float endAt) {
      Composer probe(ctx.ticker, *ctx.fonts);
      probe.setSize({kWide + 40, 320});
      sk_sp<SkSurface> scratch =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul((int)kWide + 40, 320));
      const auto step = [&](float measure) {
        probe.render(
            box().padding(10).children({passage(measure, live, candidates)}));
        if (scratch) probe.draw(*scratch->getCanvas());
      };
      for (float w = kNarrow; w <= kWide; w += 1) step(w);
      for (float w = kWide; w >= kNarrow; w -= 1) step(w);
      step(endAt);
      const TextSettling settled = probe.settling("para");
      return kit::formatted("live %s · reused %d · degraded %d",
                            settled.live ? "true" : "false", settled.reused,
                            settled.degraded);
    };

    // Each run's swell is made where its cell is, so a run states its
    // declaration once and the report under it is that run's own.
    const auto study = [&](const Run& run) {
      return cell(run.call, run.note, run.measure, run.live, run.candidates,
                  sweep(run.live, run.candidates, run.measure));
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "A MOVING MEASURE · Element::live, "
                  "Composer::settling",
         .subtitle = "dials · the measure the swell runs "
                     "between (150 to 230 px, one pixel at a step) "
                     "· the frame's floor in break candidates "
                     "(4000, then 1)",
         .footer = "a settled passage reports nothing and answers "
                   "reused 0 — it decided its breaks "
                   "once and no later frame asks it again, which "
                   "is why live is DECLARED and never inferred"},
        kit::cells({.cells = each(kRuns, study), .gap = 14})));
  }

  /** One cell: the passage set at its own measure, with the report the
   *  swell produced printed under it. */
  Element cell(const char* call, const char* note, float measure, bool live,
               int candidates, const std::string& report) {
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return sketch::kit::caption(
        kCell, call, note,
        sketch::kit::well({.width = kCell, .height = kPicture, .padding = 12})
            .column()
            .gap(10)
            .children({passage(measure, live, candidates),
                       text(report).styleClass("readout")}));
  }
};

SIGIL_SKETCH(LiveSettling, "Kit · API",
             "one passage swelled a pixel at a time between two measures, "
             "with the settling report each run produced printed under the "
             "setting it produced")
