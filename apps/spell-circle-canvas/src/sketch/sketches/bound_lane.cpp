// bound_lane.cpp — ONE CHAIN: what a bound lane does to a property
// between the Output and the pixel.
// =============================================================================
// A bare `&output` binding lands on a property RAW. `bind(&output)` puts
// a chain of stages in between, and the stages run in a FIXED ORDER
// whatever order they were called in — normalise, envelope, curve,
// quantize, affine, wrap, wiggle, clamp. None of them reads a clock:
// every one is a pure function of the bound sample, which is why the top
// band can be a set of GRAPHS rather than a description.
//
//   THE CHAIN, GRAPHED. Six panels, `apply(p)` plotted over one period
//   of the lane. The bare ramp; the envelope folding it there and back;
//   an easing curve overshooting past 1; the quantizer's staircase; the
//   wrap folding a fast lane into range; and the wiggle, whose red rails
//   are ±amount — the noise is normalised to [-1,1] BEFORE amount, so
//   the bound holds at any octave count.
//
//   THE WIGGLE IN TWO AXES. `wiggle()` reads no clock either, so a
//   two-lane shake has a LOCUS and it can be drawn. Sharing one seed
//   between x and y collapses that locus to the diagonal y = x: the
//   layer slides instead of shaking. Two seeds is the whole rig, and the
//   two chips beside the plots are the same two BoundFloat values on
//   live properties.
//
//   THE LANE AS A SCHEDULE. Five tracks under `Element::travel`, where
//   the affine stage is what "two laps" means (`.target(0, 2)` on the
//   schedule, and a closed curve wraps) and the clamp is what parks an
//   arrow at the end of an open one. The curve is DRAWN as well as
//   ridden — `track()` hands one `Shape` value to the frame's `.shape()`
//   and to the mark's `.travel({.path=…})` — because a MotionPath
//   resolves against the PARENT's box, which is the box whose outline is
//   on screen.
//
// The three ways things move: this sheet uses door 1 throughout. setup()
// declares once, two ticker steppables walk the two schedules, and the
// runtime re-resolves every bound lane per frame. Nothing re-describes,
// and travel() is paint-only — no track ever relayouts. The graphs are
// static leaves.
//
// EDIT THESE FIRST
//   kSeedY   — set it to kSeedX (= 1) and the locus collapses to the
//              diagonal: one seed drives both axes.
//   kOctaves — the wiggle panel's texture. 1 is drift, 6 is flicker, and
//              neither crosses the rails.
//   kLook    — the lookAhead of tracks 2..5. 0 leaves rotation alone,
//              0.02 banks forward, -0.02 faces BACK down the curve.
//   kLaps    — track 4's `.target(0, kLaps)`. 0.5 is half a lap.
//   kPeriod  — seconds per lap of `phase`; changes speed, not the picture.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kAmount = 60.0f;    // px of shake, and the plot's rails
constexpr float kFrequency = 3.0f;  // cycles per second of `seconds`
constexpr uint32_t kSeedX = 1;
constexpr uint32_t kSeedY = 2;  // <- make this 1 to see the diagonal
constexpr int kOctaves = 3;
constexpr float kFalloff = 0.5f;
constexpr float kWindow = 2.0f;  // seconds of `seconds` on the x axis

constexpr double kPeriod = 6.0;  // seconds per lap of `phase`
constexpr float kLook = 0.02f;   // lookAhead: the auto-orient chord
constexpr float kLaps = 2.0f;    // track 4's .target(0, kLaps)

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
const SkColor4f kRail{0.85f, 0.30f, 0.36f, 0.75f};
const SkColor4f kTrace{0.36f, 0.82f, 0.72f, 1};
const SkColor4f kTraceB{1.00f, 0.72f, 0.28f, 1};
const SkColor4f kCurve{0.32f, 0.46f, 0.62f, 1};

void strokePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color, nullptr);
  canvas.drawPath(path, paint);
}

/** ONE STAGE, GRAPHED: the lane's input on x, `apply()` on y, with @p lo
 *  and @p hi as the plot's own range so a stage that overshoots shows
 *  the overshoot instead of clipping it. This is exactly the value the
 *  runtime hands a bound property. */
Element stage(const BoundFloat& lane, float lo, float hi) {
  return custom([lane, lo, hi](SkCanvas& canvas, const PaintContext& paint) {
           const float pw = paint.size.width(), ph = paint.size.height();
           const auto y = [&](float v) {
             return ph - 6.0f - (v - lo) / (hi - lo) * (ph - 12.0f);
           };
           SkPaint rule;
           rule.setStyle(SkPaint::kStroke_Style);
           rule.setStrokeWidth(1);
           rule.setColor4f(kFrame, nullptr);
           canvas.drawLine(0, y(0.0f), pw, y(0.0f), rule);
           canvas.drawLine(0, y(1.0f), pw, y(1.0f), rule);
           SkPathBuilder trace;
           for (int i = 0; i <= 600; ++i) {
             const float p = (float)i / 600.0f;
             const SkPoint at{pw * p, y(lane.apply(p))};
             i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
           }
           strokePath(canvas, trace.detach(), kTrace, 1.4f);
         })
      .cache(Cache::None);
}

/** THE WIGGLE STAGE, on its own axes: the ±amount rails drawn in red,
 *  because the bound is the claim worth seeing. */
Element wiggleStage(const BoundFloat& lane) {
  return custom([lane](SkCanvas& canvas, const PaintContext& paint) {
           const float pw = paint.size.width(), ph = paint.size.height();
           const float cy = ph * 0.5f;
           const float k = (ph * 0.5f - 6.0f) / kAmount;
           SkPaint rule;
           rule.setStyle(SkPaint::kStroke_Style);
           rule.setStrokeWidth(1);
           rule.setColor4f(kRail, nullptr);
           canvas.drawLine(0, cy - kAmount * k, pw, cy - kAmount * k, rule);
           canvas.drawLine(0, cy + kAmount * k, pw, cy + kAmount * k, rule);
           rule.setColor4f(kFrame, nullptr);
           canvas.drawLine(0, cy, pw, cy, rule);
           SkPathBuilder trace;
           for (int i = 0; i <= 1200; ++i) {
             const float p = kWindow * (float)i / 1200.0f;
             const SkPoint at{pw * (float)i / 1200.0f, cy - lane.apply(p) * k};
             i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
           }
           strokePath(canvas, trace.detach(), kTrace, 1.4f);
         })
      .cache(Cache::None);
}

/** THE 2-D LOCUS: (x(p), y(p)) traced over the window, which is the path
 *  a two-axis shake actually walks. Shared seeds put x == y, so the locus
 *  IS the line y = x — the layer slides on a diagonal and never shakes. */
Element locus(const BoundFloat& wx, const BoundFloat& wy, SkColor4f color) {
  return custom([wx, wy, color](SkCanvas& canvas, const PaintContext& paint) {
           const float w = paint.size.width(), h = paint.size.height();
           const float cx = w * 0.5f, cy = h * 0.5f;
           const float k = std::min(w, h) * 0.5f / (kAmount * 1.15f);
           // The ±amount box: the locus can touch it, never leave it.
           SkPaint box;
           box.setStyle(SkPaint::kStroke_Style);
           box.setStrokeWidth(1);
           box.setColor4f(kRail, nullptr);
           canvas.drawRect(SkRect::MakeLTRB(cx - kAmount * k, cy - kAmount * k,
                                            cx + kAmount * k, cy + kAmount * k),
                           box);
           SkPathBuilder trace;
           for (int i = 0; i <= 900; ++i) {
             const float p = kWindow * (float)i / 900.0f;
             const SkPoint at{cx + wx.apply(p) * k, cy + wy.apply(p) * k};
             i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
           }
           strokePath(canvas, trace.detach(), color, 1.3f);
         })
      .cache(Cache::None);
}

Element panel(float width, float height, const char* title, const char* sub,
              Element inner) {
  inner.inset(0);  // the plot fills its frame
  return box()
      .width(width)
      .column()
      .child(box()
                 .width(width)
                 .height(height)
                 .stroke(stroke(1.0f, Fill::color(kFrame)))
                 .child(std::move(inner)))
      .child(text(toU8(title), type({.size = 13.0f, .color = kInk}))
                 .margin(0, 8, 0, 3))
      .child(text(toU8(sub), type({.size = 11.0f, .color = kDim})));
}

/** The mark: 22 px, and its CENTRE (the default transformOrigin, hence
 *  the point that rides the curve) is what lands on the path. */
Element arrowMark() {
  return box()
      .width(22)
      .height(22)
      .shape(shapes::arrow(0.36f, 0.46f))
      .fill(Fill::color(kTraceB));
}

/** One track. `curve` is used TWICE — stroked as the frame's own
 *  silhouette, and handed to the mark as its motion path — because a
 *  MotionPath resolves against the PARENT's box, which is precisely the
 *  box whose outline you are looking at. */
Element track(Shape curve, MotionPath along, Element mark, const char* caption,
              const char* spelling) {
  along.path = curve;
  mark.travel(std::move(along));
  return box()
      .width(232)
      .column()
      .child(box()
                 .width(176)
                 .height(176)
                 .margin(22, 4, 22, 10)
                 .shape(std::move(curve))
                 .stroke(stroke(1.4f, Fill::color(kCurve)))
                 .child(std::move(mark)))
      .child(text(toU8(caption), type({.size = 13.0f, .color = kInk}))
                 .margin(0, 0, 0, 4))
      .child(text(toU8(spelling), type({.size = 11.0f, .color = kDim})));
}

}  // namespace

struct BoundLane : sketch::Sketch {
  choreograph::Output<float> seconds{0};
  choreograph::Output<float> phase{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1280, 900);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(6.0);

    // `seconds` is the SCHEDULE the shake is phased off. It ramps forever
    // rather than wrapping, so `frequency` reads as plain Hz and the noise
    // never steps at a seam.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      seconds = (float)t;
      return true;
    });
    // `phase` is the other kind of schedule: a lap, wrapped by hand here
    // because the tracks want it in [0,1) as their input, not as their
    // output.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      phase = (float)std::fmod(t / kPeriod, 1.0);
      return true;
    });

    // The chips' own lanes and the plots' are the SAME values: build
    // them once and hand the BoundFloat to both.
    const Bound shakeX = wiggle(&seconds, kAmount, kFrequency, kSeedX);
    const Bound shakeY = wiggle(&seconds, kAmount, kFrequency, kSeedY);
    const Bound sameY = wiggle(&seconds, kAmount, kFrequency, kSeedX);

    // A live chip: two lanes, noise around REST. `wiggle(&out, …)` is
    // `bind(&out).scale(0).wiggle(…)` — without the scale(0) the property
    // would track `seconds` itself and drift off the canvas. `left/top` are
    // the REST position; the wiggle is a paint-only transform on top of it.
    const auto chip = [](const Bound& x, const Bound& y, SkColor4f color,
                         float left) {
      return box()
          .width(26)
          .height(26)
          .corners({5})
          .fill(Fill::color(color))
          .left(left)
          .top(96)
          .translateX(x)
          .translateY(y);
    };

    ctx.composer.render(
        stack()
            .child(text(toU8("bind(&output) \xc2\xb7 normalise \xe2\x86\x92 "
                             "envelope \xe2\x86\x92 curve \xe2\x86\x92 "
                             "quantize \xe2\x86\x92 affine \xe2\x86\x92 wrap "
                             "\xe2\x86\x92 wiggle \xe2\x86\x92 clamp, in that "
                             "order whatever order they were written in"),
                        type({.size = 15.0f, .color = kInk}))
                       .left(30)
                       .top(16))

            // ---- the chain, one panel per stage -------------------------
            .child(box()
                       .row()
                       .left(30)
                       .top(52)
                       .gap(12)
                       .child(panel(190, 128, "bare", "bind(&phase)",
                                    stage(bind(&phase).value(), -0.15f, 1.15f)))
                       .child(panel(190, 128, "envelope", ".pingPong()",
                                    stage(bind(&phase).pingPong().value(),
                                          -0.15f, 1.15f)))
                       .child(panel(
                           190, 128, "curve", ".map(ease::outBack())",
                           stage(bind(&phase).map(ease::outBack()).value(),
                                 -0.15f, 1.15f)))
                       .child(panel(190, 128, "quantize", ".quantize(8)",
                                    stage(bind(&phase).quantize(8).value(),
                                          -0.15f, 1.15f)))
                       .child(panel(
                           190, 128, "wrap", ".scale(3).wrap(1)",
                           stage(bind(&phase).scale(3.0f).wrap(1.0f).value(),
                                 -0.15f, 1.15f)))
                       .child(panel(
                           190, 128, "wiggle \xc2\xb7 3 octaves",
                           "rails are \xc2\xb1"
                           "amount",
                           wiggleStage(wiggle(&seconds, kAmount, kFrequency,
                                              kSeedX, kOctaves, kFalloff)
                                           .value()))))

            // ---- the wiggle in two axes, plotted and live ---------------
            .child(
                box()
                    .row()
                    .left(30)
                    .top(258)
                    .gap(24)
                    .child(panel(230, 230, "SHARED SEED \xc2\xb7 broken",
                                 "x and y both seed 1 \xe2\x86\x92 y = x",
                                 locus(shakeX.value(), sameY.value(), kTraceB)))
                    .child(panel(230, 230, "SEEDS 1 / 2 \xc2\xb7 a shake",
                                 "two independent lanes",
                                 locus(shakeX.value(), shakeY.value(), kTrace)))
                    .child(
                        panel(230, 230, "the same lanes, LIVE",
                              "amber = shared seed, teal = 1 / 2",
                              stack()
                                  .child(chip(shakeX, sameY, kTraceB, 62))
                                  .child(chip(shakeX, shakeY, kTrace, 142)))))

            .child(text(toU8("THE ORDER IS THE POINT. wrap folds the affine "
                             "value, so a wrapped phase still wiggles across "
                             "the seam; wiggle adds in the property's own "
                             "units, so its amount is pixels here and laps "
                             "below; clamp is always last, whenever it was "
                             "written."),
                        type({.size = 12.0f, .color = kDim}))
                       .left(806)
                       .top(268)
                       .width(444))

            // ---- the lane as a schedule, under travel() -----------------
            .child(
                box()
                    .row()
                    .left(24)
                    .top(578)
                    .gap(8)

                    // 1 — the bare case. lookAhead defaults to 0, so
                    // orientation is left alone: a dot rides, nothing turns.
                    .child(track(shapes::circle(), {.t = &phase},
                                 box()
                                     .width(18)
                                     .height(18)
                                     .shape(shapes::circle())
                                     .fill(Fill::color(kTraceB)),
                                 "1 \xc2\xb7 t only", ".t = &phase"))

                    // 2 — lookAhead engages auto-orient: the angle of the
                    // chord ahead is ADDED to rotate() (which is 0 here).
                    .child(track(shapes::circle(),
                                 {.t = &phase, .lookAhead = kLook}, arrowMark(),
                                 "2 \xc2\xb7 + lookAhead", ".lookAhead = 0.02"))

                    // 3 — …and rotate() still composes on top of the bank.
                    // Same flight as 2; the arrow also spins as it goes.
                    .child(track(
                        shapes::circle(), {.t = &phase, .lookAhead = kLook},
                        arrowMark().rotate(bind(&phase).target(0.0f, 720.0f)),
                        "3 \xc2\xb7 + rotate()", "rotate() ADDS to it"))

                    // 4 — the lane is the SCHEDULE, so "two laps" is one
                    // affine verb on it. A closed curve wraps; no API.
                    .child(track(shapes::circle(),
                                 {.t = bind(&phase).target(0.0f, kLaps),
                                  .lookAhead = kLook},
                                 arrowMark(), "4 \xc2\xb7 two laps",
                                 ".target(0, 2) wraps"))

                    // 5 — an OPEN curve CLAMPS at its ends and holds the
                    // last good chord there, so a parked arrow still points
                    // down the final leg instead of reading atan2(0, 0).
                    .child(track(shapes::arc(140.0f, 260.0f),
                                 {.t = bind(&phase).target(-0.3f, 1.3f),
                                  .lookAhead = kLook},
                                 arrowMark(), "5 \xc2\xb7 open curve",
                                 ".target(-0.3, 1.3) clamps")))

            .child(text(toU8("outline and motion path are one Shape value "
                             "\xc2\xb7 translateX/Y are IGNORED while a path "
                             "is engaged"),
                        type({.size = 11.0f, .color = kDim}))
                       .left(30)
                       .bottom(12)));
  }
};

SIGIL_SKETCH(BoundLane, "Kit \xc2\xb7 API",
             "one BoundFloat chain, stage by stage \xe2\x80\x94 each one "
             "graphed because none reads a clock, then the wiggle's locus "
             "and the schedule under travel()")
