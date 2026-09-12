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
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/values/Spring.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toUtf8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kSpan = 3.0f;    // seconds across every plot
constexpr float kTau = 0.6f;     // the decay's time constant, seconds
constexpr float kHz = 4.0f;      // the rate the clock is posterised at
constexpr float kPeriod = 0.8f;  // the phase's loop and the spring's period

constexpr SkColor4f kGrid{0.17f, 0.18f, 0.21f, 1};
constexpr SkColor4f kSecond{0.46f, 0.72f, 0.92f, 1};
constexpr SkColor4f kThird{0.86f, 0.46f, 0.36f, 1};

/** One curve over `kSpan` seconds, in a plot whose y runs 0 at the bottom
 *  to 1 at the top. The sampler is dense enough that a staircase reads as
 *  a staircase rather than as a ramp with corners. */
Element plot(const char* key, std::vector<kit::Trace> curves, int gridLines = 0,
             float ceiling = 1) {
  kit::Plot frame{
      .toT = kSpan, .toY = ceiling, .pad = 10, .samples = 420, .rule = kGrid};
  for (int i = 1; i < gridLines; ++i)
    frame.rulesT.push_back(kSpan * (float)i / (float)gridLines);
  return box()
      .inset(0)
      .child(box().left(10).right(10).bottom(9).height(1).fill(
          Fill::color(sketch::kit::theme().palette.rule)))
      .child(kit::curvePlot(key, std::move(curves), std::move(frame)));
}

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toUtf8(call), toUtf8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(body)));
}

}  // namespace

struct DecayStep final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
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

    ctx.composer.render(sketch::kit::page(
        {.title = toUtf8("THE CLOCK ARITHMETIC \xc2\xb7 motion::decay, "
                         "quantizeTime, stepIndex, phase, spring"),
         .subtitle = toUtf8("dials \xc2\xb7 three seconds across every plot "
                            "\xc2\xb7 the time constant (0.6 s) \xc2\xb7 "
                            "the rate (4 Hz) \xc2\xb7 the period (0.8 s) "
                            "\xc2\xb7 the damping ratios"),
         .footer = toUtf8("a spring is a STATE and the rest are functions, "
                          "which is the whole difference: an ease needs "
                          "two fixed endpoints and can only restart when "
                          "the target moves, where a spring carries the "
                          "motion it already has into the new one")},
        kit::cells(
            {.cells =
                 {cell("motion::decay(age, 0.6)",
                       "exp(-age/tau) \xc2\xb7 1 at the instant it "
                       "happened, and never quite 0 \xc2\xb7 the grid is "
                       "one tau apart, so the curve crosses each line "
                       "lower by the same fraction",
                       plot("decay",
                            {{[](float t) { return motion::decay(t, kTau); },
                              look.palette.figure}},
                            (int)(kSpan / kTau))),
                  cell("quantizeTime(t, 4) / 3",
                       "SECONDS posterised at a rate and held still "
                       "between steps \xc2\xb7 twelve steps across "
                       "three seconds, against the ramp they came from",
                       plot("quantize",
                            {{[](float t) { return t / kSpan; },
                              look.palette.ash},
                             {[](float t) {
                                return motion::quantizeTime(t, kHz) / kSpan;
                              },
                              look.palette.figure}},
                            (int)(kSpan * kHz))),
                  cell("stepIndex(t, 4) / 12",
                       "the same clock as an INTEGER COUNT \xc2\xb7 the "
                       "same staircase, and the number a cursor or a "
                       "frame table indexes with",
                       plot("step",
                            {{[](float t) {
                                return (float)motion::stepIndex(t, kHz) /
                                       (kSpan * kHz);
                              },
                              kSecond}},
                            (int)(kSpan * kHz))),
                  cell("motion::phase(t, 0.8)",
                       "seconds folded into a wrapping [0, 1) "
                       "\xc2\xb7 the marching ants, the marquee, the "
                       "scanline creep \xc2\xb7 three and three quarter "
                       "turns in three seconds",
                       plot("phase",
                            {{[](float t) { return motion::phase(t, kPeriod); },
                              look.palette.figure}})),
                  cell("spring(s, 1, dt, {0.8, damping})",
                       "damping 0.25, 0.6 and 1.2 \xc2\xb7 below one it "
                       "overshoots and rings, at one it arrives as fast "
                       "as it can without crossing, above one it crawls "
                       "in from one side",
                       plot("spring",
                            {{springWalk(0.25f), kThird},
                             {springWalk(0.6f), look.palette.figure},
                             {springWalk(1.2f), kSecond}},
                            0, 1.4f))},
             .gap = 12})));
  }
};

SIGIL_SKETCH(DecayStep, "Kit \xc2\xb7 API",
             "the open-ended settle, the posterised clock as seconds and as "
             "a count, the wrapping phase, and a spring at three damping "
             "ratios, each plotted over the same three seconds")
