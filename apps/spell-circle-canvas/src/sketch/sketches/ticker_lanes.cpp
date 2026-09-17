/** @file
 * ticker_lanes — the four ways a value gets stepped, run on a ticker of
 * this sketch's own and plotted.
 *
 * `timeline()` is the master: a Choreograph motion applied to an Output,
 * removed when it finishes, which is what lets `active()` settle to false
 * on its own.
 *
 * `add(fn(dt))` is the free steppable — per-frame work the timeline does
 * not express. It is handed the frame's delta and answers whether it
 * still needs frames. A steppable that always answers true keeps
 * `active()` true forever, and so keeps an event-driven host rendering
 * forever.
 *
 * `addFixed(hz, fn)` runs at exactly its own rate whatever the host draws
 * at, and the step count comes from TOTAL ELAPSED TIME rather than from a
 * running accumulator: `want = floor(total·hz)`, run `want − ran`. A float
 * accumulator compared against a step size drifts, so the same simulated
 * moment lands on either side of a boundary depending on the draw rate;
 * counting from total time is exact at any rate, which is what makes a
 * captured frame reproducible. `maxCatchUp` bounds one frame's backlog,
 * and dropping simulated time is the correct failure.
 *
 * `derive(&dst, chain)` recomputes an Output every tick as the `bind()`
 * vocabulary applied to another Output. Derivations run in a SECOND
 * PHASE, after the timeline and after every steppable, so a derivation
 * never reads a stale source and registration order does not matter — the
 * one-frame lag a hand-rolled shadow copy hides is the failure this
 * contract exists to prevent. It remaps a schedule's VALUE and not TIME,
 * and it is ONE LEVEL ONLY, refused loudly.
 *
 * The ticker here is stepped in this file at a fixed delta, so the traces
 * are the ticker's own answers rather than a drawing of them.
 *
 * EDIT THESE FIRST
 *   kSpan, kDt — the seconds plotted and the delta they are stepped at.
 *   kFixedHz — the fixed steppable's rate.
 *   kLevels — how many levels the derivation quantizes its source to.
 */

// TAGS: Motion/Clocks

#include <choreograph/Choreograph.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;
namespace ch = choreograph;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 720};
constexpr float kCell = 498;
constexpr float kPicture = 160;

constexpr float kSpan = 3.0f;         // seconds plotted
constexpr float kDt = 1.0f / 120.0f;  // the delta they are stepped at
constexpr double kFixedHz = 5.0;      // the fixed steppable's rate
constexpr int kLevels = 6;            // levels the derivation quantizes to
constexpr float kRamp = 1.4f;         // the timeline motion's duration

constexpr SkColor4f kSecond{0.46f, 0.72f, 0.92f, 1};

/** One recorded lane: a value per tick, plotted left to right. */
using Lane = std::vector<double>;

constexpr int kSteps = (int)(kSpan / kDt);

/** THE FRAME EVERY LANE IS PLOTTED IN: the tick index across, the value
 *  up, with room at every edge for the stroke. Nothing below turns a
 *  sample into a pixel. */
const sketch::kit::Plot kField{
    .x = {.domain = {0, kSteps - 1}}, .y = {.domain = {0, 1}}, .pad = 10};

/** ONE LANE AS A LAYER. The record IS the x domain, so the frame walks
 *  the ticker's own answers rather than an expression fitted to them;
 *  @p styleClass is empty for the lane a plot is about and names the
 *  sheet's own entry for a second one beside it. */
sketch::kit::Layer lane(const Lane& recorded, std::string styleClass = {}) {
  return sketch::kit::trace(
      std::span<const double>(recorded),
      {.pen = {.width = 1.6f}, .styleClass = std::move(styleClass)});
}

/** A PLOT OF RECORDED LANES, over the baseline they are read against. */
Element plot(const char* key, std::vector<sketch::kit::Layer> lanes) {
  lanes.insert(lanes.begin(), sketch::kit::rules({.y = {0.0}}));
  return sketch::kit::plot(key, kField, std::move(lanes)).cover();
}

/** The sheet's two classes past the registers and the chart's: the second
 *  lane of a plot that carries two, and the source a derivation is read
 *  against. */
weave::StyleSheet sheetClasses(const sketch::kit::Theme& look) {
  weave::StyleSheet classes = look.styleSheet();
  classes.set("second", {.color = kSecond});
  classes.set("source", {.color = look.palette.ash});
  return classes;
}

}  // namespace

struct TickerLanes {
  Lane freeLane, fixedLane, alphaLane, sourceLane, derivedLane, timelineLane;
  std::string readouts[4];

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // the run has already happened, on its own ticker
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Theme& look = sketch::kit::theme();

    // A TICKER OF THIS SKETCH'S OWN, stepped at a fixed delta: everything
    // below is what it answered, sample by sample, rather than a drawing
    // of what it would answer.
    motion::Ticker ticker;
    ch::Output<float> source{0.0f}, derived{0.0f}, ramped{0.0f}, alpha{0.0f};
    double elapsed = 0;
    int fixedSteps = 0;
    bool stillActive = false;

    ticker.add([&](double dt) {
      elapsed += dt;
      source = motion::phase(elapsed, 1.0);
      return true;  // …and so this ticker is active forever
    });
    ticker.addFixed(
        kFixedHz,
        [&] {
          ++fixedSteps;
          return true;
        },
        8, &alpha);
    const bool derived_ok =
        ticker.derive(&derived, motion::bind(&source).quantize(kLevels));
    ticker.timeline().apply(&ramped).then<ch::RampTo>(1.0f, kRamp);

    for (int i = 0; i < kSteps; ++i) {
      stillActive = ticker.tick(kDt);
      freeLane.push_back(source);
      fixedLane.push_back((float)fixedSteps / (float)(kSpan * kFixedHz));
      alphaLane.push_back(alpha);
      sourceLane.push_back(source);
      derivedLane.push_back(derived);
      timelineLane.push_back(ramped);
    }

    readouts[0] = kit::formatted("add · %d ticks · active %s", kSteps,
                                 stillActive ? "true" : "false");
    readouts[1] = kit::formatted("addFixed %.0f Hz · %d steps in %.3f s",
                                 kFixedHz, fixedSteps, elapsed);
    readouts[2] = kit::formatted("derive · quantize(%d) · registered %s",
                                 kLevels, derived_ok ? "true" : "false");
    readouts[3] = kit::formatted("timeline · RampTo over %.1f s", kRamp);

    const auto figure = [](Element chart) {
      return sketch::kit::well({.width = kCell, .height = kPicture})
          .children({std::move(chart)});
    };
    ctx.composer.render(
        sketch::kit::page(
            {.title = "Four ways to advance a value",
             .subtitle =
                 kit::formatted("One ticker, %.3f seconds at a 120 Hz delta · "
                                "the same clock across every chart",
                                elapsed),
             .footer = "Derived values run after the timeline and steppables. "
                       "A finished timeline retires; the free steppable here "
                       "deliberately keeps the ticker active."},
            box().column().gap(26).children(
                {sketch::kit::comparison(
                     {.cases =
                          {{.title = "01 / EVERY FRAME",
                            .control = "add(dt) · a repeating one-second phase",
                            .figure = figure(plot("free", {lane(freeLane)})),
                            .note = readouts[0]},
                           {.title = "02 / FIXED INTERVALS",
                            .control = "addFixed(5 Hz) · count + interpolation",
                            .figure = figure(plot(
                                "fixed",
                                {lane(fixedLane), lane(alphaLane, "second")})),
                            .note =
                                readouts[1] + " · blue: alpha between steps"}},
                      .measure = 1020,
                      .gap = 24}),
                 sketch::kit::comparison(
                     {.cases =
                          {{.title = "03 / AFTER THE SOURCE",
                            .control =
                                "derive · quantize the phase to six levels",
                            .figure = figure(
                                plot("derive", {lane(sourceLane, "source"),
                                                lane(derivedLane)})),
                            .note = readouts[2] + " · grey: source phase"},
                           {.title = "04 / UNTIL COMPLETION",
                            .control = "timeline · ramp 0 → 1, then hold",
                            .figure =
                                figure(plot("timeline", {lane(timelineLane)})),
                            .note = readouts[3]}},
                      .measure = 1020,
                      .gap = 24})}))
            .styleSheet(sheetClasses(look)));
  }
};

SIGIL_SKETCH(TickerLanes, "Kit · API",
             "a ticker stepped three seconds at a fixed delta with all four "
             "of its lanes registered, each lane's own answers plotted and "
             "counted")
