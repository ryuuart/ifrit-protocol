/** @file
 * motion_engine — one ticker of this sketch's own, stepped at a fixed
 * delta and plotted: the four ways it advances a value, then one running
 * flight bent onto a new endpoint three ways.
 *
 * THE FOUR LANES. `animate` is the timeline: a motion applied to an
 * animatable and retired when it finishes, which is what lets
 * `isRunning()` settle to false on its own. `timer(fn(dt))` is the free
 * steppable: per-frame work the timeline does not express, handed the
 * frame's delta; one that always answers true keeps the ticker running
 * forever, and so keeps an event-driven host drawing forever.
 * `timer(fn, {.stepRate})` runs at exactly its own rate whatever the host
 * draws at, and counts its steps from TOTAL ELAPSED TIME rather than from
 * a running accumulator, so the same simulated moment lands on the same
 * side of a step at any draw rate; `catchUp` bounds one frame's backlog,
 * and dropping simulated time is the correct failure. `bind(source,
 * {.quantize})` recomputes a value every tick from another one, in a
 * SECOND PHASE after the timeline and every steppable, so a derivation
 * never reads a stale source and registration order does not matter.
 *
 * THE INTERRUPTED FLIGHT. A LANE is one transitionable float, addressed
 * by WHERE the motion that serves it is held: `Family` is the host's own
 * enumeration of its storages — one fixed slot array, and positional
 * families whose length the description decides — which is why this
 * sketch declares its own and names no host type. `retargetFixed` bends a
 * running row onto the new endpoint, and a row one side lacks ramps from
 * or to the lane's standing value. `retargetPositional` does the same for
 * a family of equal shape, and DROPS the running motions when the shape
 * changes, rather than carrying them onto endpoints that now mean
 * something else.
 *
 * Every trace is what a ticker answered, sample by sample, rather than a
 * drawing of what it would answer.
 *
 * EDIT THESE FIRST
 *   kSpan, kDt — the seconds plotted and the delta they are stepped at.
 *   kFixedHz — the fixed steppable's rate.
 *   kLevels — how many levels the derivation quantizes its source to.
 *   kArrives, kFirst, kSecond — when the second description arrives, and
 *     the two targets of the interrupted flight.
 */

// TAGS: Motion/Clocks, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/advanced/Held.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace motion = sigil::motion;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 990};
constexpr float kMeasure = 1020;
constexpr float kPicture = 150;

constexpr float kSpan = 3.0f;         // seconds plotted
constexpr float kDt = 1.0f / 120.0f;  // the delta they are stepped at
constexpr double kFixedHz = 5.0;      // the fixed steppable's rate
constexpr int kLevels = 6;            // levels the derivation quantizes to
constexpr float kRamp = 1.4f;         // the timeline motion's duration
constexpr float kArrives = 0.55f;     // when the second description arrives
constexpr float kFirst = 0.92f;       // the flight's first target
constexpr float kSecond = 0.24f;      // …and the one it is bent onto
constexpr auto kTransition = std::chrono::milliseconds(900);

constexpr int kSteps = (int)(kSpan / kDt);

constexpr material::Color kSecondInk{0.46f, 0.72f, 0.92f, 1};
constexpr material::Color kArrivalInk{0.19f, 0.20f, 0.24f, 1};

/** One recorded lane: a value per tick, spread across the seconds plotted. */
using Lane = std::vector<double>;

/** ONE RECORDED LANE AS A LAYER in @p styleClass: empty for the lane a plot
 *  is about, `second` for a second lane beside it, `reference` for the one
 *  it is read against. */
sketch::kit::Layer lane(const Lane& recorded, std::string styleClass = {}) {
  return sketch::kit::trace(
      std::span<const double>(recorded),
      {.pen = {.width = 1.6f}, .styleClass = std::move(styleClass)});
}

/** A PLOT OF RECORDED LANES over the seconds plotted, the value up, a
 *  baseline, and a rule at each moment in @p arrivals. */
Element plot(const char* key, std::vector<sketch::kit::Layer> lanes,
             std::vector<double> arrivals = {}) {
  lanes.insert(lanes.begin(), sketch::kit::rules({.y = {0.0}}));
  if (!arrivals.empty())
    lanes.insert(lanes.begin(),
                 sketch::kit::rules(
                     {.x = std::move(arrivals), .styleClass = "arrival"}));
  return sketch::kit::plot(
             key,
             {.x = {.domain = {0, kSpan}}, .y = {.domain = {0, 1}}, .pad = 10},
             std::move(lanes))
      .cover();
}

/** The sheet's classes past the registers and the chart's: a plot's
 *  second lane, the lane it is read against, and the arrival rule. */
StyleSheet sheetClasses() {
  const sketch::kit::Theme& look = sketch::kit::theme();
  return look.styleSheet() +
         StyleSheet{rule(".second").font({.color = kSecondInk}),
                    rule(".reference").font({.color = look.palette.ash}),
                    rule(".arrival").font({.color = kArrivalInk})};
}

/** The host's enumeration of its storages, declared here because a lane
 *  names no host type: one fixed slot array and one positional family. */
enum class Family : uint8_t { Slots, Points };
using Retargeted = motion::Lane<Family>;

/** How the second description reaches the running flight. */
enum class Change { None, Slot, Family, Reshaped };

/** ONE FLIGHT, 0 toward kFirst, with the second description applied at
 *  kArrives in whichever way @p change names, on a ticker stepped at the
 *  sheet's delta. */
Lane flight(Change change) {
  motion::Engine ticker;
  motion::HeldMotions held;
  held.resize(1);
  const motion::Animatable<float> standing = 0.0f;
  const motion::Animatable<float> first = kFirst;
  const motion::Animatable<float> second = kSecond;
  const std::optional<motion::Tween<float>> transition =
      motion::Tween<float>{.duration = kTransition};
  motion::retarget(ticker, held[0], standing, first, transition);

  Lane recorded;
  bool arrived = false;
  for (int step = 0; step < kSteps; ++step) {
    if (!arrived && change != Change::None && step * kDt >= kArrives) {
      arrived = true;
      const Family family =
          change == Change::Slot ? Family::Slots : Family::Points;
      const Retargeted before{&first, {family, 0}, 0.0f};
      const Retargeted after[2] = {{&second, {family, 0}, 0.0f},
                                   {&second, {family, 1}, 0.0f}};
      if (change == Change::Slot)
        motion::retargetFixed<Family>(ticker, held, {&before, 1},
                                      {after, 1}, transition);
      else
        motion::retargetPositional<Family>(
            ticker, held, {&before, 1},
            {after, change == Change::Reshaped ? 2u : 1u}, transition);
    }
    ticker.advance(ticker.elapsed() + motion::Duration(kDt));
    recorded.push_back(motion::valueOf(held.empty() ? nullptr : held[0].get(),
                                       arrived ? second : first));
  }
  return recorded;
}

}  // namespace

struct MotionEngine {
  void setup(sketch::Scene2DContext& ctx) {
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    // every run has already happened, on tickers of the sketch's own
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // THE FOUR LANES, registered on one ticker and read every step.
    motion::Engine ticker;
    motion::Animatable<float> source = motion::animatable(0.0f),
                              derived = motion::animatable(0.0f),
                              ramped = motion::animatable(0.0f),
                              alpha = motion::animatable(0.0f);
    double elapsed = 0;
    int fixedSteps = 0;
    ticker.timer([&](motion::Duration stepDuration) {
      elapsed += stepDuration.count();
      source = motion::phase(motion::Duration(elapsed), motion::Duration(1.0));
      return true;  // …and so this ticker runs forever
    });
    const auto fixedClock = ticker.timer(
        [&] {
          ++fixedSteps;
          return true;
        },
        {.stepRate = kFixedHz, .catchUp = 8});
    ticker.timer([&, fixedClock] { alpha = fixedClock.betweenSteps(); });
    derived = motion::bind(source, {.quantize = kLevels});
    ticker.animate(ramped, {.to = 1.0f,
                            .duration = motion::Duration(kRamp),
                            .ease = motion::ease::linear});

    Lane free, fixed, between, sourced, quantized, timeline;
    for (int step = 0; step < kSteps; ++step) {
      ticker.advance(motion::Duration((step + 1) * kDt));
      free.push_back(source.value());
      fixed.push_back(fixedSteps / (kSpan * kFixedHz));
      between.push_back(alpha.value());
      sourced.push_back(source.value());
      quantized.push_back(derived.value());
      timeline.push_back(ramped.value());
    }

    // THE INTERRUPTED FLIGHT, run once per way of meeting the arrival.
    const Lane plain = flight(Change::None), slot = flight(Change::Slot),
               family = flight(Change::Family),
               reshaped = flight(Change::Reshaped);
    double disagreement = 0;
    for (size_t step = 0; step < slot.size(); ++step)
      disagreement =
          std::max(disagreement, std::abs(slot[step] - family[step]));

    const auto figure = [](float width, Element chart) {
      return sketch::kit::well({.width = width, .height = kPicture})
          .children({std::move(chart)});
    };
    const float half = (kMeasure - 24) / 2, third = (kMeasure - 36) / 3;
    const auto interrupted = [&](const char* key, const Lane& bent,
                                 const char* styleClass = "") {
      return figure(third, plot(key, {lane(plain, "reference"),
                                      lane(bent, styleClass)},
                                {kArrives}));
    };

    ctx.composer.render(
        sketch::kit::page(
            {.title = "One ticker, stepped by hand",
             .subtitle = kit::formatted(
                 "Every trace is the ticker's own answer · %.3f seconds at "
                 "a 120 Hz delta",
                 elapsed),
             .footer = "Derived values run after the timeline and "
                       "steppables. A finished timeline retires; the free "
                       "steppable here deliberately keeps the ticker "
                       "running."},
            box().column().gap(30).children(
                {sketch::kit::section(
                     {.label = "FOUR WAYS TO ADVANCE A VALUE",
                      .note = "timer, a timer at its own rate, bind, and "
                              "animate on one ticker"},
                     box().column().gap(24).children(
                         {sketch::kit::comparison(
                              {.cases =
                                   {{.title = "01 / EVERY FRAME",
                                     .control = "timer(dt) · a repeating "
                                                "one-second phase",
                                     .figure = figure(
                                         half, plot("free", {lane(free)})),
                                     .note = kit::formatted(
                                         "%d ticks · running %s", kSteps,
                                         ticker.isRunning() ? "true"
                                                            : "false")},
                                    {.title = "02 / FIXED INTERVALS",
                                     .control = "timer at 5 Hz · count and "
                                                "the fraction between steps",
                                     .figure = figure(
                                         half,
                                         plot("fixed",
                                              {lane(fixed),
                                               lane(between, "second")})),
                                     .note = kit::formatted(
                                         "%d steps in %.3f s · blue: the "
                                         "fraction between steps",
                                         fixedSteps, elapsed)}},
                               .measure = kMeasure,
                               .gap = 24}),
                          sketch::kit::comparison(
                              {.cases =
                                   {{.title = "03 / AFTER THE SOURCE",
                                     .control = kit::formatted(
                                         "bind · quantize the phase to %d "
                                         "levels",
                                         kLevels),
                                     .figure = figure(
                                         half,
                                         plot("derive",
                                              {lane(sourced, "reference"),
                                               lane(quantized)})),
                                     .note = "grey: the source phase"},
                                    {.title = "04 / UNTIL COMPLETION",
                                     .control = "animate · ramp 0 → 1, "
                                                "then hold",
                                     .figure = figure(
                                         half,
                                         plot("timeline", {lane(timeline)})),
                                     .note = kit::formatted(
                                         "linear over %.1f s, then retired",
                                         kRamp)}},
                               .measure = kMeasure,
                               .gap = 24})})),
                 sketch::kit::section(
                     {.label = "ONE FLIGHT, A NEW TARGET MID-FLIGHT",
                      .note = kit::formatted(
                          "0 → %.2f, then %.2f at %.2f s, %lld ms each · "
                          "grey: the flight left alone",
                          kFirst, kSecond, kArrives,
                          (long long)kTransition.count())},
                     sketch::kit::comparison(
                         {.cases =
                              {{.title = "FIXED SLOT",
                                .control = "retargetFixed · one row",
                                .figure = interrupted("slot", slot),
                                .note = "Keeps the current value and bends "
                                        "toward the new endpoint."},
                               {.title = "SAME FAMILY",
                                .control = "retargetPositional · 1 → 1",
                                .figure = interrupted("family", family),
                                .note = kit::formatted(
                                    "Equal shape bends as a fixed slot "
                                    "does · largest difference %.6f",
                                    disagreement)},
                               {.title = "CHANGED FAMILY",
                                .control = "retargetPositional · 1 → 2",
                                .figure = interrupted("reshaped", reshaped,
                                                      "second"),
                                .note = "Changing shape drops the running "
                                        "motion; the new value arrives at "
                                        "once."}},
                          .measure = kMeasure,
                          .gap = 18}))}))
            .applyStyleSheet(sheetClasses()));
  }
};

SIGIL_SKETCH(MotionEngine, "Kit · API",
             "a ticker stepped three seconds at a fixed delta with all four "
             "of its lanes plotted, then one flight interrupted three ways — "
             "a fixed row bent onto a new endpoint, a positional family of "
             "equal shape, and one whose shape changed and dropped its "
             "motion")
