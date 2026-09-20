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

// TAGS: Motion/Clocks

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/bind/BoundFloat.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>

namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;

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

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  // A TRACE IS A MEASURED FIGURE, so the curve every plot draws is the
  // palette's figure colour and no plot names one.
  look.palette.figure = {0.36f, 0.82f, 0.72f, 1};
  return look;
}

const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
const SkColor4f kRail{0.85f, 0.30f, 0.36f, 0.75f};
const SkColor4f kTrace{0.36f, 0.82f, 0.72f, 1};
const SkColor4f kTraceB{1.00f, 0.72f, 0.28f, 1};
const SkColor4f kCurve{0.32f, 0.46f, 0.62f, 1};

/** The sheet's one class past the registers and the chart's: the ±amount
 *  rails, which are the bound worth seeing and not a hairline. */
weave::StyleSheet sheetClasses(const sketch::kit::Theme& look) {
  weave::StyleSheet classes = look.styleSheet();
  classes.set("rail", {.color = kRail});
  // The two loci are the same curve in two inks: the split-seed shake and
  // the shared-seed slide, told apart by the class each names.
  classes.set("locus", {.color = kTrace});
  classes.set("locusShared", {.color = kTraceB});
  return classes;
}

void strokePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color, nullptr);
  canvas.drawPath(path, paint);
}

/** ONE STAGE, GRAPHED: the lane's input across, `apply()` up, with @p lo
 *  and @p hi as the frame's own y domain so a stage that overshoots shows
 *  the overshoot instead of clipping it. This is exactly the value the
 *  runtime hands a bound property. */
Element stage(const char* key, const BoundFloat& lane, float lo, float hi) {
  return sketch::kit::plot(
      key, {.x = {.domain = {0, 1}}, .y = {.domain = {lo, hi}}, .pad = 6},
      {sketch::kit::rules({.y = {0.0, 1.0}}),
       sketch::kit::trace([lane](double p) { return lane.apply((float)p); },
                          {.pen = {.width = 1.4f}, .samples = 600})});
}

/** THE WIGGLE STAGE, on its own axes: the ±amount rails in the rail
 *  class, because the bound is the claim worth seeing. */
Element wiggleStage(const char* key, const BoundFloat& lane) {
  return sketch::kit::plot(
      key,
      {.x = {.domain = {0, kWindow}},
       .y = {.domain = {-kAmount, kAmount}},
       .pad = 6},
      {sketch::kit::rules({.y = {-kAmount, kAmount}, .styleClass = "rail"}),
       sketch::kit::rules({.y = {0.0}}),
       sketch::kit::trace([lane](double p) { return lane.apply((float)p); },
                          {.pen = {.width = 1.4f}, .samples = 1200})});
}

/** THE 2-D LOCUS: (x(p), y(p)) traced over the window, which is the path
 *  a two-axis shake actually walks. A trace walks one domain and this
 *  walks a parameter into both, so it is drawn with the pen. Shared seeds
 *  put x == y, so the locus IS the line y = x — the layer slides on a
 *  diagonal and never shakes. */
Element locus(const char* key, const BoundFloat& wx, const BoundFloat& wy,
              const char* ink) {
  // Both axes read the SAME parameter, which is what no trace can be and
  // what `path` is: the ±amount box is two rules across and two down, and
  // the locus can touch it and never leave it.
  constexpr double kRoom = kAmount * 1.15;
  return sketch::kit::plot(
      key, {.x = {.domain = {-kRoom, kRoom}}, .y = {.domain = {-kRoom, kRoom}}},
      {sketch::kit::rules({.x = {-kAmount, kAmount},
                           .y = {-kAmount, kAmount},
                           .styleClass = "rail"}),
       sketch::kit::path(
           [wx, wy](double p) {
             return sketch::kit::Datum{wx.apply((float)p), wy.apply((float)p)};
           },
           {.pen = {.width = 1.3f},
            .samples = 900,
            .over = {0.0, kWindow},
            .styleClass = ink})});
}

/** ONE PANEL, captioned in the one voice every panel here is: the
 *  picture, then what it is, then the call that spelled it, measured to
 *  the cell so a note never widens its own panel. */
sketch::kit::ComparisonCase panel(float width, float height, const char* title,
                                  const char* control, Element inner) {
  inner.inset(0);
  return {.title = title,
          .control = control,
          .figure = sketch::kit::well({.width = width,
                                       .height = height,
                                       .keyline = Fill::color(kFrame)})
                        .children({std::move(inner)})};
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
sketch::kit::ComparisonCase track(Shape curve, MotionPath along, Element mark,
                                  const char* caption, const char* spelling) {
  along.path = curve;
  mark.travel(std::move(along));
  return {
      .title = caption,
      .control = spelling,
      .figure = sketch::kit::well({.width = 228,
                                   .height = 184,
                                   .ground = Fill::none(),
                                   .clip = false,
                                   .content = sketch::kit::Well::Content{}},
                                  box()
                                      .width(156)
                                      .height(156)
                                      .shape(std::move(curve))
                                      .stroke(stroke(1.4f, Fill::color(kCurve)))
                                      .children({std::move(mark)}))};
}

}  // namespace

struct BoundLane {
  choreograph::Output<float> seconds{0};
  choreograph::Output<float> phase{0};

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1280, 1030}, .captureAt = 6.0});

    // `seconds` is the SCHEDULE the shake is phased off. It ramps forever
    // rather than wrapping, so `frequency` reads as plain Hz and the noise
    // never steps at a seam.
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      seconds = (float)t;
    });
    // `phase` is the other kind of schedule: a lap, wrapped by hand here
    // because the tracks want it in [0,1) as their input, not as their
    // output.
    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      phase = (float)std::fmod(t / kPeriod, 1.0);
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

    Element chain = sketch::kit::comparison(
        {.cases =
             {panel(190, 128, "INPUT", "bind(phase)",
                    stage("lane.bare", bind(&phase).value(), -0.15f, 1.15f)),
              panel(190, 128, "ENVELOPE", "pingPong()",
                    stage("lane.pingPong", bind(&phase).pingPong().value(),
                          -0.15f, 1.15f)),
              panel(
                  190, 128, "CURVE", "map(outBack)",
                  stage("lane.curve", bind(&phase).map(ease::outBack()).value(),
                        -0.15f, 1.15f)),
              panel(190, 128, "STEPS", "quantize(8)",
                    stage("lane.quantize", bind(&phase).quantize(8).value(),
                          -0.15f, 1.15f)),
              panel(190, 128, "REPEAT", "scale(3).wrap(1)",
                    stage("lane.wrap",
                          bind(&phase).scale(3.0f).wrap(1.0f).value(), -0.15f,
                          1.15f)),
              panel(190, 128, "NOISE", "3 octaves · rails ±60",
                    wiggleStage("lane.wiggle",
                                wiggle(&seconds, kAmount, kFrequency, kSeedX,
                                       kOctaves, kFalloff)
                                    .value()))},
         .measure = 1200,
         .gap = 12});

    Element locusRow = sketch::kit::comparison(
        {.cases =
             {panel(282, 220, "SHARED SEED", "x: seed 1 / y: seed 1",
                    locus("locus.shared", shakeX.value(), sameY.value(),
                          "locusShared")),
              panel(282, 220, "INDEPENDENT SEEDS", "x: seed 1 / y: seed 2",
                    locus("locus.split", shakeX.value(), shakeY.value(),
                          "locus")),
              panel(282, 220, "THE SAME LANES, LIVE",
                    "amber: shared / teal: split",
                    stack().children({chip(shakeX, sameY, kTraceB, 81),
                                      chip(shakeX, shakeY, kTrace, 167)})),
              {.title = "WHY TWO SEEDS?",
               .figure = box().width(282).column().gap(14).children(
                   {document::caption(
                        "Equal seeds make x = y, so the point can only slide "
                        "along a diagonal. Independent seeds let it explore "
                        "the plane.")
                        .width(282),
                    sketch::kit::readout(
                        {{.name = "Amplitude", .value = "±60 px"},
                         {.name = "Frequency", .value = "3 Hz"},
                         {.name = "Plotted window", .value = "2 seconds"}},
                        {.measure = 282, .ruled = true}),
                    document::caption(
                        "The red rails bound the displacement. Noise is "
                        "added in the property's own units; clamp runs last.")
                        .width(282)})}},
         .measure = 1200,
         .gap = 24});

    Element tracks = sketch::kit::comparison(
        {.cases =
             {// 1 — the bare case. lookAhead defaults to 0, so orientation
              // is left alone: a dot rides, nothing turns.
              track(shapes::circle(), {.t = &phase},
                    box()
                        .width(18)
                        .height(18)
                        .shape(shapes::circle())
                        .fill(Fill::color(kTraceB)),
                    "POSITION ONLY", ".t = &phase"),
              // 2 — lookAhead engages auto-orient: the angle of the chord
              // ahead is ADDED to rotate() (which is 0 here).
              track(shapes::circle(), {.t = &phase, .lookAhead = kLook},
                    arrowMark(), "FOLLOW THE TANGENT", ".lookAhead = 0.02"),
              // 3 — …and rotate() still composes on top of the bank. Same
              // flight as 2; the arrow also spins as it goes.
              track(shapes::circle(), {.t = &phase, .lookAhead = kLook},
                    arrowMark().rotate(bind(&phase).target(0.0f, 720.0f)),
                    "ADD A SPIN", "rotate() ADDS to it"),
              // 4 — the lane is the SCHEDULE, so "two laps" is one affine
              // verb on it. A closed curve wraps; no API.
              track(shapes::circle(),
                    {.t = bind(&phase).target(0.0f, kLaps), .lookAhead = kLook},
                    arrowMark(), "TWO LAPS", ".target(0, 2) wraps"),
              // 5 — an OPEN curve CLAMPS at its ends and holds the last good
              // chord there, so a parked arrow still points down the final
              // leg instead of reading atan2(0, 0).
              track(shapes::arc(140.0f, 260.0f),
                    {.t = bind(&phase).target(-0.3f, 1.3f), .lookAhead = kLook},
                    arrowMark(), "OPEN PATH", ".target(-0.3, 1.3) clamps")},
         .measure = 1200,
         .gap = 15});

    ctx.composer.render(
        sketch::kit::page(
            {.title = "One lane, from signal to movement",
             .subtitle = "Read a value, reshape it, then give it a place to go "
                         "· the same binding vocabulary in three settings",
             .footer = "Stage order is fixed: normalise → envelope → curve → "
                       "quantize → affine → wrap → wiggle → clamp. A path "
                       "controls position while travel is engaged."},
            box().column().gap(24).children(
                {document::h2("01 / RESHAPE A NORMALIZED INPUT"),
                 std::move(chain), document::h2("02 / BUILD A TWO-AXIS SHAKE"),
                 std::move(locusRow),
                 document::h2("03 / FOLLOW A PATH · ONE PHASE, FIVE READINGS"),
                 std::move(tracks)}))
            .styleSheet(sheetClasses(sketch::kit::theme())));
  }
};

SIGIL_SKETCH(BoundLane, "Specimen",
             "one BoundFloat chain, stage by stage — each one "
             "graphed because none reads a clock, then the wiggle's locus "
             "and the schedule under travel()")
