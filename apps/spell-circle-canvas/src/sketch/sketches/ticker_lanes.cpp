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

#include <choreograph/Choreograph.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace ch = choreograph;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 176;

constexpr float kSpan = 3.0f;         // seconds plotted
constexpr float kDt = 1.0f / 120.0f;  // the delta they are stepped at
constexpr double kFixedHz = 5.0;      // the fixed steppable's rate
constexpr int kLevels = 6;            // levels the derivation quantizes to
constexpr float kRamp = 1.4f;         // the timeline motion's duration

constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kSecond{0.46f, 0.72f, 0.92f, 1};

/** One recorded lane: a value per tick, plotted left to right. */
using Trace = std::vector<float>;

Element plot(const char* key, std::vector<std::pair<Trace, SkColor4f>> lanes) {
  return custom(key,
                [lanes = std::move(lanes)](SkCanvas& canvas,
                                           const PaintContext& pc) {
                  constexpr float kPad = 10;
                  const float w = pc.size.width() - 2 * kPad;
                  const float h = pc.size.height() - 2 * kPad;
                  SkPaint paint;
                  paint.setAntiAlias(true);
                  paint.setColor4f(kRule);
                  canvas.drawRect({kPad, kPad + h, kPad + w, kPad + h + 1},
                                  paint);
                  paint.setStyle(SkPaint::kStroke_Style);
                  paint.setStrokeWidth(1.6f);
                  for (const auto& [trace, colour] : lanes) {
                    if (trace.size() < 2) continue;
                    SkPathBuilder path;
                    for (size_t i = 0; i < trace.size(); ++i) {
                      const float x =
                          kPad + w * (float)i / (float)(trace.size() - 1);
                      const float y = kPad + h * (1.0f - trace[i]);
                      if (i == 0)
                        path.moveTo(x, y);
                      else
                        path.lineTo(x, y);
                    }
                    paint.setColor4f(colour);
                    canvas.drawPath(path.detach(), paint);
                  }
                })
      .absolute()
      .inset(0);
}

Element cell(const char* call, const char* note, Element body,
             const std::string& readout) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(body))
          // The readout stands on a scrim of the cell's own ground: a
          // trace runs the whole plate and would otherwise cross it.
          .child(text(toU8(readout), sketch::kit::theme().mono(10, kFigure))
                     .absolute()
                     .left(Dim(8.0f))
                     .top(Dim(6.0f))
                     .padding(4, 2)
                     .fill(Fill::color(kCellGround))));
}

}  // namespace

struct TickerLanes final : sketch::Sketch {
  Trace freeLane, fixedLane, alphaLane, sourceLane, derivedLane, timelineLane;
  std::string readouts[4];

  void setup(sketch::SketchContext& ctx) override {
    // the run has already happened, on its own ticker
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

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

    const int steps = (int)(kSpan / kDt);
    for (int i = 0; i < steps; ++i) {
      stillActive = ticker.tick(kDt);
      freeLane.push_back(source);
      fixedLane.push_back((float)fixedSteps / (float)(kSpan * kFixedHz));
      alphaLane.push_back(alpha);
      sourceLane.push_back(source);
      derivedLane.push_back(derived);
      timelineLane.push_back(ramped);
    }

    readouts[0] = kit::formatted("add \xc2\xb7 %d ticks \xc2\xb7 active %s", steps,
                              stillActive ? "true" : "false");
    readouts[1] = kit::formatted("addFixed %.0f Hz \xc2\xb7 %d steps in %.0f s",
                              kFixedHz, fixedSteps, kSpan);
    readouts[2] =
        kit::formatted("derive \xc2\xb7 quantize(%d) \xc2\xb7 registered %s",
                    kLevels, derived_ok ? "true" : "false");
    readouts[3] = kit::formatted("timeline \xc2\xb7 RampTo over %.1f s", kRamp);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("THE TICKER'S LANES \xc2\xb7 Ticker::add, "
                       "addFixed, derive, timeline"),
         .subtitle = toU8("dials \xc2\xb7 three seconds at a 120 Hz delta "
                          "\xc2\xb7 the fixed rate (5 Hz) \xc2\xb7 the "
                          "derivation's levels (6) \xc2\xb7 the "
                          "timeline motion's duration (1.4 s)"),
         .footer = toU8("a derivation runs in a SECOND PHASE, after the "
                        "timeline and after every steppable, so it "
                        "never reads a stale source and registration "
                        "order does not matter \xe2\x80\x94 which is "
                        "exactly what a hand-rolled shadow copy cannot "
                        "promise")},
        kit::cells(
            {.cells = {cell("ticker.add([](double dt) { … return true; })",
                            "the free steppable, handed the frame's delta "
                            "\xc2\xb7 it answers true forever here, which is "
                            "what keeps active() true forever",
                            plot("free", {{freeLane, kFigure}}), readouts[0]),
                       cell("ticker.addFixed(5, fn, 8, &alpha)",
                            "the count of fixed steps against the render "
                            "interpolant \xc2\xb7 the count comes from total "
                            "elapsed time, so it is exact at any draw rate",
                            plot("fixed",
                                 {{fixedLane, kFigure}, {alphaLane, kSecond}}),
                            readouts[1]),
                       cell("derive(&d, bind(&source).quantize(6))",
                            "the source under the derivation \xc2\xb7 the "
                            "bind() vocabulary reaching an Output instead of "
                            "a property slot",
                            plot("derive",
                                 {{sourceLane, kAsh}, {derivedLane, kFigure}}),
                            readouts[2]),
                       cell("timeline().apply(&v).then<RampTo>(1, 1.4)",
                            "the master timeline \xc2\xb7 a finished motion "
                            "is removed, which is what would let active() "
                            "settle if the steppable above ever retired",
                            plot("timeline", {{timelineLane, kFigure}}),
                            readouts[3])},
             .gap = 14})));
  }
};

SIGIL_SKETCH(TickerLanes, "Kit \xc2\xb7 API",
             "a ticker stepped three seconds at a fixed delta with all four "
             "of its lanes registered, each lane's own answers plotted and "
             "counted")
