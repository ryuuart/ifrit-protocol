/** @file
 * decay_step — the arithmetic over a clock reading, plotted.
 *
 * Four of these are one short expression each, and they are in the
 * library because a hand-written copy of one is a second place for two
 * call sites to disagree.
 *
 * `decay(age, tau)` is `exp(-age/tau)`: 1 at the instant a thing happened
 * and falling towards 0 for as long as it is remembered. It is NOT an
 * easing curve, and the difference is the shape of the question — an ease
 * maps a normalised progress, needs a duration, and arrives at exactly 0
 * or 1 at a stated moment; this takes an AGE, has no end, and never quite
 * reaches 0.
 *
 * `quantizeTime(t, hz)` posterises SECONDS at a rate, and `stepIndex` is
 * the same fact as an integer. They are not interchangeable: a value
 * driven by a held clock wants the seconds, and anything that must know
 * WHICH tick it is looking at — reseeding a scramble once per step,
 * advancing a cursor, indexing a table of frames — wants the count.
 * Recovering one from the other means dividing back out by the rate and
 * rounding, which is that second place to disagree.
 *
 * `phase(t, period)` folds seconds into a wrapping [0, 1). A non-positive
 * period answers 0 rather than the NaN a bare `fmod` would produce, and a
 * negative `t` wraps forward.
 *
 * `spring` is the odd one out: a STATE rather than a function, because it
 * carries its own velocity. That is what lets a target move mid-flight and
 * the motion bend into it instead of restarting. It is solved in closed
 * form, so one step of any size is exact — fifty steps of a frame and one
 * step of fifty frames land on the same value.
 *
 * EDIT THESE FIRST
 *   kTau — the decay's time constant, seconds.
 *   kHz — the rate the clock is posterised at.
 *   kPeriod — the phase's loop and the spring's period, seconds.
 */

// TAGS: Motion/Clocks

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Spring.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 770};

constexpr float kSpan = 3.0f;    // seconds across every plot
constexpr float kTau = 0.6f;     // the decay's time constant, seconds
constexpr float kHz = 4.0f;      // the rate the clock is posterised at
constexpr float kPeriod = 0.8f;  // the phase's loop and the spring's period

constexpr material::Color kGrid{0.17f, 0.18f, 0.21f, 1};
constexpr material::Color kSecond{0.46f, 0.72f, 0.92f, 1};
constexpr material::Color kThird{0.86f, 0.46f, 0.36f, 1};

/** THE SHEET THESE PLOTS ARE DRESSED BY: the grid in its own grey, the
 *  zero line a step quieter than a curve, and a name for each series a
 *  multi-curve cell puts beside the first. */
sigil::compose::StyleSheet plotSheet(const sketch::kit::Theme& look) {
  sigil::compose::StyleSheet dressed =
      look.styleSheet() +
      sigil::compose::StyleSheet{
          sigil::compose::rule(".plotAxis")
              .font({.color = material::skia::toSkColor(look.palette.rule)}),
          sigil::compose::rule(".plotRule")
              .font({.color = material::skia::toSkColor(kGrid)}),
          sigil::compose::rule(".ramp").font(
              {.color = material::skia::toSkColor(look.palette.ash)}),
          sigil::compose::rule(".second").font(
              {.color = material::skia::toSkColor(kSecond)}),
          sigil::compose::rule(".third").font(
              {.color = material::skia::toSkColor(kThird)})};
  return dressed;
}

/** One curve of this plate, in the class @p series names or the plate's
 *  first. 420 samples is dense enough that a staircase reads as a
 *  staircase rather than as a ramp with corners. */
sketch::kit::Layer curve(sigil::core::Callable<double(double)> shape,
                         const char* series = "") {
  return sketch::kit::trace(std::move(shape),
                            {.samples = 420, .styleClass = series});
}

/** One plot over `kSpan` seconds, y running 0 at the bottom to @p ceiling
 *  at the top, with @p gridLines equal divisions ruled across it. */
Element plot(const char* key, std::vector<sketch::kit::Layer> curves,
             int gridLines = 0, float ceiling = 1) {
  std::vector<double> ruled;
  for (int i = 1; i < gridLines; ++i)
    ruled.push_back(kSpan * (double)i / (double)gridLines);
  std::vector<sketch::kit::Layer> layers{
      sketch::kit::axis(
          {.of = sketch::kit::Axis::X, .reach = 0, .numbers = false}),
      sketch::kit::rules({.x = std::move(ruled)})};
  for (sketch::kit::Layer& one : curves) layers.push_back(std::move(one));
  return sketch::kit::plot(key,
                           {.x = {.domain = {0, kSpan}},
                            .y = {.domain = {0, ceiling}},
                            .pad = 10},
                           std::move(layers))
      .inset(0);
}

}  // namespace

struct DecayStep {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // the plots are functions of time, not of the clock
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    const sketch::kit::Theme& look = sketch::kit::theme();

    // The spring is a STATE, so its curve is a walk rather than a
    // sampling: it is stepped at a fixed dt and remembers its velocity.
    const auto springWalk = [](float damping) {
      return [damping](float t) {
        motion::Spring s{0.0f, 0.0f};
        constexpr float kDt = 1.0f / 240.0f;
        for (float u = 0; u < t; u += kDt)
          s = motion::spring(s, 1.0f, kDt,
                             {.periodSeconds = kPeriod, .damping = damping});
        return s.value * 0.9f;
      };
    };

    const auto figure = [](float width, float height, Element chart) {
      return sketch::kit::well({.width = width, .height = height})
          .children({std::move(chart)});
    };
    ctx.composer.render(
        sketch::kit::page(
            {.title = "Reading a clock, remembering a motion",
             .subtitle = "Three seconds across every plot · the top row remaps "
                         "time; the lower row compares ways to settle",
             .footer = "Clock functions are sampled directly. Each spring is "
                       "stepped at 240 Hz and keeps its velocity; the "
                       "displayed target is 0.9."},
            box().column().gap(28).children(
                {sketch::kit::comparison(
                     {.cases =
                          {{.title = "HOLD THE TIME",
                            .control = "quantizeTime(t, 4 Hz) / 3",
                            .figure = figure(
                                328, 150,
                                plot("quantize",
                                     {curve([](double t) { return t / kSpan; },
                                            "ramp"),
                                      curve([](double t) {
                                        return motion::quantizeTime((float)t,
                                                                    kHz) /
                                               kSpan;
                                      })},
                                     12)),
                            .note = "Seconds held between steps. Grey is the "
                                    "continuous clock."},
                           {.title = "COUNT THE STEPS",
                            .control = "stepIndex(t, 4 Hz) / 12",
                            .figure = figure(328, 150,
                                             plot("step",
                                                  {curve(
                                                      [](double t) {
                                                        return (double)motion::
                                                                   stepIndex(
                                                                       (float)t,
                                                                       kHz) /
                                                               (kSpan * kHz);
                                                      },
                                                      "second")},
                                                  12)),
                            .note = "The same boundaries, returned as an "
                                    "integer for a frame or cursor."},
                           {.title = "REPEAT A PHASE",
                            .control = "phase(t, 0.8 s)",
                            .figure = figure(328, 150,
                                             plot("phase", {curve([](double t) {
                                                    return motion::phase(
                                                        (float)t, kPeriod);
                                                  })})),
                            .note = "Wrap to zero every 0.8 seconds. The value "
                                    "remains in [0, 1)."}},
                      .measure = 1020,
                      .gap = 18}),
                 sketch::kit::comparison(
                     {.cases =
                          {{.title = "FORGET AN EVENT",
                            .control = "decay(age, τ = 0.6 s)",
                            .figure = figure(498, 200,
                                             plot("decay", {curve([](double t) {
                                                    return motion::decay(
                                                        (float)t, kTau);
                                                  })},
                                                  5)),
                            .note = "Each grid interval is one time constant: "
                                    "36.8% remains after τ. No fixed end."},
                           {.title = "CARRY THE VELOCITY",
                            .control =
                                "spring · period 0.8 s · three damping ratios",
                            .figure =
                                figure(498, 200,
                                       plot("spring",
                                            {curve(springWalk(0.25f), "third"),
                                             curve(springWalk(0.6f)),
                                             curve(springWalk(1.2f), "second")},
                                            0, 1.4f)),
                            .note = "Coral 0.25: ringing · gold 0.6: smaller "
                                    "overshoot · blue 1.2: no crossing"}},
                      .measure = 1020,
                      .gap = 24})}))
            .applyStyleSheet(plotSheet(look)));
  }
};

SIGIL_SKETCH(DecayStep, "Kit · API",
             "the open-ended settle, the posterised clock as seconds and as "
             "a count, the wrapping phase, and a spring at three damping "
             "ratios, each plotted over the same three seconds")
