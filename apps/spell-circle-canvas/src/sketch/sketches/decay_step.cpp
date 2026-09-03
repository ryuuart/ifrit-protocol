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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/values/Spring.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <functional>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kSpan = 3.0f;    // seconds across every plot
constexpr float kTau = 0.6f;     // the decay's time constant, seconds
constexpr float kHz = 4.0f;      // the rate the clock is posterised at
constexpr float kPeriod = 0.8f;  // the phase's loop and the spring's period

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kGrid{0.17f, 0.18f, 0.21f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kSecond{0.46f, 0.72f, 0.92f, 1};
constexpr SkColor4f kThird{0.86f, 0.46f, 0.36f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** One curve over `kSpan` seconds, in a plot whose y runs 0 at the bottom
 *  to 1 at the top. The sampler is dense enough that a staircase reads as
 *  a staircase rather than as a ramp with corners. */
using Curve = std::function<float(float)>;

Element plot(const char* key, std::vector<std::pair<Curve, SkColor4f>> curves,
             int gridLines = 0) {
  return custom(key,
                [curves = std::move(curves), gridLines](
                    SkCanvas& canvas, const PaintContext& pc) {
                  constexpr float kPad = 10;
                  const float w = pc.size.width() - 2 * kPad;
                  const float h = pc.size.height() - 2 * kPad;
                  SkPaint paint;
                  paint.setAntiAlias(true);
                  // The grid is the clock's own steps where a cell has
                  // them, so a staircase can be read against the rate that
                  // cut it.
                  paint.setColor4f(kGrid);
                  for (int i = 1; i < gridLines; ++i) {
                    const float x = kPad + w * (float)i / (float)gridLines;
                    canvas.drawRect({x, kPad, x + 1, kPad + h}, paint);
                  }
                  paint.setColor4f(kRule);
                  canvas.drawRect({kPad, kPad + h, kPad + w, kPad + h + 1},
                                  paint);
                  paint.setStyle(SkPaint::kStroke_Style);
                  paint.setStrokeWidth(1.6f);
                  for (const auto& [curve, colour] : curves) {
                    SkPathBuilder path;
                    constexpr int kSamples = 420;
                    for (int i = 0; i <= kSamples; ++i) {
                      const float t = kSpan * (float)i / (float)kSamples;
                      const float y = curve(t);
                      const float px = kPad + w * (t / kSpan);
                      const float py = kPad + h * (1.0f - y);
                      if (i == 0)
                        path.moveTo(px, py);
                      else
                        path.lineTo(px, py);
                    }
                    paint.setColor4f(colour);
                    canvas.drawPath(path.detach(), paint);
                  }
                })
      .absolute()
      .inset(0);
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .width(Dim(kCell))
                       .height(Dim(kPicture))
                       .clip()
                       .fill(Fill::color(kCellGround))
                       .child(std::move(body)));
}

}  // namespace

struct DecayStep final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // the plots are functions of time, not of the clock

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

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE CLOCK ARITHMETIC \xc2\xb7 motion::decay, "
                           "quantizeTime, stepIndex, phase, spring"),
             .subtitle = toU8("dials \xc2\xb7 three seconds across every plot "
                              "\xc2\xb7 the time constant (0.6 s) \xc2\xb7 "
                              "the rate (4 Hz) \xc2\xb7 the period (0.8 s) "
                              "\xc2\xb7 the damping ratios"),
             .footer = toU8("a spring is a STATE and the rest are functions, "
                            "which is the whole difference: an ease needs "
                            "two fixed endpoints and can only restart when "
                            "the target moves, where a spring carries the "
                            "motion it already has into the new one"),
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
                     {cell("motion::decay(age, 0.6)",
                           "exp(-age/tau) \xc2\xb7 1 at the instant it "
                           "happened, and never quite 0 \xc2\xb7 the grid is "
                           "one tau apart, so the curve crosses each line "
                           "lower by the same fraction",
                           plot("decay",
                                {{[](float t) {
                                    return motion::decay(t, kTau);
                                  },
                                  kFigure}},
                                (int)(kSpan / kTau))),
                      cell("quantizeTime(t, 4) / 3",
                           "SECONDS posterised at a rate and held still "
                           "between steps \xc2\xb7 twelve steps across "
                           "three seconds, against the ramp they came from",
                           plot("quantize",
                                {{[](float t) { return t / kSpan; }, kAsh},
                                 {[](float t) {
                                    return motion::quantizeTime(t, kHz) /
                                           kSpan;
                                  },
                                  kFigure}},
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
                                {{[](float t) {
                                    return motion::phase(t, kPeriod);
                                  },
                                  kFigure}})),
                      cell("spring(s, 1, dt, {0.8, damping})",
                           "damping 0.25, 0.6 and 1.2 \xc2\xb7 below one it "
                           "overshoots and rings, at one it arrives as fast "
                           "as it can without crossing, above one it crawls "
                           "in from one side",
                           plot("spring", {{springWalk(0.25f), kThird},
                                           {springWalk(0.6f), kFigure},
                                           {springWalk(1.2f), kSecond}}))},
                 .gap = 12}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(DecayStep, "Kit \xc2\xb7 API",
             "the open-ended settle, the posterised clock as seconds and as "
             "a count, the wrapping phase, and a spring at three damping "
             "ratios, each plotted over the same three seconds")
