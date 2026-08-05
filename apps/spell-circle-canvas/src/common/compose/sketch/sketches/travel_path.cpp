// travel_path.cpp — ONE API: Element::travel({.path, .t, .lookAhead}).
// =============================================================================
// A place to stand, not a picture. Five identical tracks; the only thing
// that differs between them is the MotionPath value printed underneath.
// The curve is DRAWN as well as ridden — `track()` hands the same `Shape`
// value to the frame's `.shape()` and to the mark's `.travel({.path=…})`,
// so the outline you see IS the curve, resolved against the same box.
//
// EDIT THESE FIRST
//   kLook    — the lookAhead of tracks 2..5. 0 leaves rotation alone,
//              0.02 banks forward, -0.02 faces BACK down the curve.
//   kLaps    — track 4's `.target(0, kLaps)`. At 2, its arrow always sits
//              twice as far round as track 2's. 0.5 is half a lap.
//   kPeriod  — seconds per lap of `phase`; changes speed, not the picture.
//
// The three ways things move (hello.cpp): this uses door 1 only. setup()
// DECLARES the five tracks once and a ticker steppable walks `phase`; the
// runtime re-resolves the five `t` lanes every frame. Nothing re-describes,
// and travel() is paint-only — no track ever relayouts.

#include <sigilcompose/Shapes.h>
#include <sigilsketch/Sketch.h>

#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr double kPeriod = 6.0;  // seconds per lap of `phase`
constexpr float kLook = 0.02f;   // lookAhead: the auto-orient chord
constexpr float kLaps = 2.0f;    // track 4's .target(0, kLaps)

sigil::weave::TextStyle type(float size, SkColor4f color) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(true);
  return style;
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kCurve{0.32f, 0.46f, 0.62f, 1};
const SkColor4f kMark{1.00f, 0.72f, 0.28f, 1};

/** The mark: 22 px, and its CENTRE (the default transformOrigin, hence
 *  the point that rides the curve) is what lands on the path. */
Element arrowMark() {
  return box()
      .width(22)
      .height(22)
      .shape(shapes::arrow(0.36f, 0.46f))
      .fill(Fill::color(kMark));
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
      .width(238)
      .column()
      .child(box()
                 .width(190)
                 .height(190)
                 .margin(24, 6, 24, 12)
                 .shape(std::move(curve))
                 .stroke(stroke(1.4f, Fill::color(kCurve)))
                 .child(std::move(mark)))
      .child(text(toU8(caption), type(13, kInk)).margin(0, 0, 0, 4))
      .child(text(toU8(spelling), type(11, kDim)));
}

}  // namespace

struct TravelPath : sigil::compose::sketch::Sketch {
  choreograph::Output<float> phase{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1250, 350);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(kPeriod * 0.31);  // mid-lap: every arrow is off its start

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      phase = (float)std::fmod(t / kPeriod, 1.0);
      return true;
    });

    ctx.composer.render(
        stack()
            .child(text(toU8("Element::travel({.path, .t, .lookAhead}) "
                             "\xc2\xb7 the curve is the shape, the lane is "
                             "the schedule"),
                        type(15, kInk))
                       .left(30)
                       .top(16))

            .child(
                box()
                    .row()
                    .left(20)
                    .top(46)
                    .right(20)
                    .gap(6)

                    // 1 — the bare case. lookAhead defaults to 0, so
                    // orientation is left alone: a dot rides, nothing turns.
                    .child(track(shapes::circle(), {.t = &phase},
                                 box()
                                     .width(18)
                                     .height(18)
                                     .shape(shapes::circle())
                                     .fill(Fill::color(kMark)),
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
                             "\xc2\xb7 translateX/Y are IGNORED while a "
                             "path is engaged"),
                        type(11, kDim))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(TravelPath)
