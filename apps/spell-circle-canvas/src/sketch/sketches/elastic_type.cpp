// elastic_type.cpp — PATTERN: rubber type. Two published keyframe tables,
// transcribed number for number and run per glyph.
// =============================================================================
// THE PATTERN, AND ITS SOURCE
//
// Animate.css (Daniel Eden, 2013) is the CSS animation library that put a
// vocabulary of named motions into everyday web work, and two of its
// entries are the whole elastic-lettering genre:
//
//   rubberBand — a squash-and-stretch on the two scale axes, overshooting
//                and settling over seven stops.
//   jello      — a decaying shear: the same skew, halved and reversed at
//                each step, eight times.
//
// They are the web's restatement of the animator's first principle. SQUASH
// AND STRETCH is the first of the twelve in Thomas and Johnston's THE
// ILLUSION OF LIFE (1981): a body deforms under acceleration and preserves
// its volume while it does, which is why the stretched frame is narrow and
// the squashed frame is wide. `rubberBand`'s table obeys that — every pair
// multiplies out near 1 — and `jello`'s does not, because a shear is not a
// squash.
//
// -----------------------------------------------------------------------------
// THE TABLES, VERBATIM
//
//   rubberBand      scaleX  scaleY          jello     skewX = skewY
//     0%             1.00    1.00             0.0%        0
//    30%             1.25    0.75            11.1%        0
//    40%             0.75    1.25            22.2%      -12.5°
//    50%             1.15    0.85            33.3%       +6.25°
//    65%             0.95    1.05            44.4%       -3.125°
//    75%             1.05    0.95            55.5%       +1.5625°
//   100%             1.00    1.00            66.6%       -0.78125°
//                                            77.7%       +0.390625°
//                                            88.8%       -0.1953125°
//                                           100.0%        0
//
// The table is ALL this file states. `fx::keys` takes the entries and does
// the sampling, so the numbers above and the numbers below are the same
// list read twice rather than a transcription and a sampler that have to
// agree — and the graphs at the bottom plot the EFFECT, not the table, so
// the picture draws whatever the motion is actually doing.
//
// TWO THINGS THE TRANSCRIPTION GETS RIGHT, both of which take a browser to
// notice and both of which change the shape of the motion:
//
//  * `jello` shears on BOTH axes — the published rule is
//    `skewX(a) skewY(a)`, the same angle on each — so the word rocks on a
//    diagonal. The two angles ride one `GlyphModifier` and reach the glyph as a
//    single shear pair.
//  * CSS crosses EACH KEYFRAME SEGMENT with its own timing function, `ease`
//    by default, rather than running one curve across the whole list. That
//    is what `fx::keys` means by a per-segment curve, and `cssEase` below is
//    the curve itself: every segment eases in and out of its own endpoints,
//    which rounds the corners a linear reading leaves sharp.
//
// -----------------------------------------------------------------------------
// WHAT THIS PUTS UNDER LOAD
//
// A non-uniform scale and a shear are the ONE deviation an RSXform cannot
// carry: that transform encodes a rotation, one scale and no shear at all.
// A glyph whose composed deviation uses `scaleX`, `scaleY`, `skewXDeg` or
// `skewYDeg` therefore leaves the shared transform array and draws under its
// own matrix — same passes, same paint, one canvas concat — while its
// neighbours stay batched. Both rows here are a whole line of such glyphs,
// which is the worst case for that split and the reason it is worth having
// a study of.
//
// EDIT THESE FIRST
//   kEachMs   — start-to-start per letter. At 0 the whole word deforms as
//               one body, which is what the CSS class actually does to an
//               element; per letter is what a lettering artist does to a
//               word.
//   kDurMs    — one letter's whole table, start to finish.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/elastic_type.cpp \
//       --frame /tmp/elastic_type.png

// TAGS: Typography/Effects, Motion/Transitions

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kW = 1080.0f;
constexpr float kH = 620.0f;

constexpr SkColor4f kPaper = hexColor(0x101014);
constexpr SkColor4f kInk = hexColor(0xF6F2E9);
constexpr SkColor4f kLabel = hexColor(0x848B99);
constexpr SkColor4f kFaint = hexColor(0x2E3440);
constexpr SkColor4f kX = hexColor(0xFF7A59);  // scaleX / skewX
constexpr SkColor4f kY = hexColor(0x5AC8F5);  // scaleY
constexpr SkColor4f kRest = hexColor(0x4A5262);

constexpr float kWordSize = 68.0f;
constexpr float kEachMs = 62.0f;
constexpr float kDurMs = 900.0f;
constexpr double kLoop = 3.4;  // one pass of both rows, then a rest
constexpr float kPlotH = 112.0f;

// ---------------------------------------------------------------------------
// The tables, and the curve every segment of one is crossed with.

using Table = std::vector<fx::Key>;

/** CSS's default `animation-timing-function`: cubic-bezier(0.25, 0.1, 0.25,
 *  1). A keyframe list that names no timing function is crossed with this
 *  ONE SEGMENT AT A TIME, which is why it belongs beside the tables rather
 *  than over them.
 *
 *  A plain function and not a lambda, so the two effects below compare by
 *  their tables and their curve and prune like any other value.
 *
 *  x is monotonic in the parameter for control points inside the unit
 *  square, so bisecting on x lands on the one parameter whose x is the time
 *  asked for, and y at that parameter is the answer. */
float cssEase(float t) {
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  const auto bezier = [](float u, float a, float b) {
    const float v = 1.0f - u;
    return 3.0f * v * v * u * a + 3.0f * v * u * u * b + u * u * u;
  };
  float lo = 0.0f, hi = 1.0f, u = t;
  for (int i = 0; i < 24; ++i) {
    u = 0.5f * (lo + hi);
    (bezier(u, 0.25f, 0.25f) < t ? lo : hi) = u;
  }
  return bezier(u, 0.1f, 1.0f);
}

/** rubberBand: a squash and a stretch on the two scale axes. They are
 *  independent, so every glyph leaves the batched RSXform array and draws
 *  under its own matrix. */
Table rubberTable() {
  return {{0.00f, {}},
          {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
          {0.40f, {.scaleX = 0.75f, .scaleY = 1.25f}},
          {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
          {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
          {0.75f, {.scaleX = 1.05f, .scaleY = 0.95f}},
          {1.00f, {}}};
}

/** jello: a halving, alternating shear, THE SAME ANGLE ON BOTH AXES, which
 *  is what makes the word rock on a diagonal rather than side to side. */
Table jelloTable() {
  const auto shear = [](float at, float deg) {
    return fx::Key{at, {.skewXDeg = deg, .skewYDeg = deg}};
  };
  return {shear(0.000f, 0.0f),        shear(0.111f, 0.0f),
          shear(0.222f, -12.5f),      shear(0.333f, 6.25f),
          shear(0.444f, -3.125f),     shear(0.555f, 1.5625f),
          shear(0.666f, -0.78125f),   shear(0.777f, 0.390625f),
          shear(0.888f, -0.1953125f), shear(1.000f, 0.0f)};
}

/** The deviation at one moment, for the graphs: the effects are pure
 *  functions of local time here, so the picture is read out of the same
 *  value the glyphs are drawn from. */
GlyphModifier at(const TextEffect& effect, float t) {
  GlyphInfo glyph;
  sigil::core::noise::Mix64Stream rng(1);
  return effect(glyph, t, rng);
}

// ---------------------------------------------------------------------------
// The graphs — the EFFECTS, plotted. Pure functions of local time, so they
// are static leaves and nothing here reads a clock.

/** A value the plot rules and names on its own axis. The published tables
 *  are read in these units, so a reader can put a ruler on the trace. */
struct Tick {
  float value;
  const char* label;
};

using Ticks = std::vector<Tick>;

/** THE FOUR CLASSES THE GRAPHS ARE DRESSED BY: the rules a trace is read
 *  against, the rest pose, and one per lane. */
weave::StyleSheet graphSheet() {
  weave::StyleSheet dressed;
  dressed.set("plotRule", {.color = kFaint});
  dressed.set("rest", {.color = kRest});
  dressed.set("x", {.color = kX});
  dressed.set("y", {.color = kY});
  return dressed;
}

/** Where a value sits in a plot box of height @p h, in that box's own
 *  pixels. The graph is drawn through this same frame and `plot` hangs its
 *  labels off it, so the two cannot drift apart. */
float tickY(float value, float lo, float hi, float h) {
  return sketch::kit::Plot{.y = {.domain = {lo, hi}}}.at(0, value, {0, h}).fY;
}

// The two axes, named where they are ruled. The extremes are the published
// tables' own outer values, so a trace that touches a ruled line is a
// transcription a reader can check off the plate.
constexpr float kScaleLo = 0.62f, kScaleHi = 1.38f;
constexpr float kShearLo = -14.0f, kShearHi = 14.0f;

Ticks scaleTicks() {
  return {{1.25f, "1.25"}, {1.00f, "1.00"}, {0.75f, "0.75"}};
}

Ticks shearTicks() {
  return {{12.5f, "+12.5°"}, {0.0f, "0°"}, {-12.5f, "−12.5°"}};
}

/** ONE LANE OF ONE PUBLISHED TABLE: the table, which field of the glyph's
 *  deviation the lane reads, the class it is drawn in, the range its box
 *  spans, the value its rest pose stands at, and the values it is ruled
 *  and named at. Three of these are the whole bottom of the plate. */
struct Lane {
  const char* key;
  const char* title;
  Table (*table)();
  float (*read)(const GlyphModifier&);
  const char* series;
  float lo, hi, rest;
  Ticks (*ticks)();
};

const std::array<Lane, 3> kLanes{{
    {"g-rx", "rubberBand — scaleX 0.75 TO 1.25", rubberTable,
     [](const GlyphModifier& m) { return m.scaleX; }, "x", kScaleLo, kScaleHi,
     1.0f, scaleTicks},
    {"g-ry", "rubberBand — scaleY 0.75 TO 1.25", rubberTable,
     [](const GlyphModifier& m) { return m.scaleY; }, "y", kScaleLo, kScaleHi,
     1.0f, scaleTicks},
    {"g-j", "jello — skewX = skewY ±12.5°, HALVING", jelloTable,
     [](const GlyphModifier& m) { return m.skewXDeg; }, "x", kShearLo, kShearHi,
     0.0f, shearTicks},
}};

/** ONE LANE, FRAMED AND NAMED: the plot inside its keyline, the values it
 *  is ruled at named down its right edge, and its title under it.
 *
 *  The curve is the effect's own law and the dots are the keyframes the
 *  reference publishes — a second reading of the same lane, and therefore a
 *  run of marks rather than a property of the curve. The labels hang off
 *  the same frame the trace is drawn through, so a label names the line
 *  beside it and cannot slide off it. */
Element lanePanel(const Lane& lane) {
  const TextEffect effect = fx::keys(lane.table(), &cssEase);
  const auto curve = [effect, read = lane.read](double t) {
    return (double)read(at(effect, (float)t));
  };
  std::vector<double> ruled;
  for (const Tick& tick : lane.ticks())
    if (tick.value != lane.rest) ruled.push_back(tick.value);
  std::vector<double> published;
  for (const fx::Key& keyframe : lane.table()) published.push_back(keyframe.at);
  const auto dot = [](double, std::size_t) {
    return box()
        .width(5.2f)
        .height(5.2f)
        .borderRadius(Corners{2.6f})
        .fill(Fill::currentInk());
  };
  const float lo = lane.lo, hi = lane.hi, rest = lane.rest;

  return box().column().flexGrow(1).gap(7).children(
      {box()
           .width(pct(100))
           .height(kPlotH)
           .stroke(stroke(1.0f, Fill::color(kFaint)))
           .children(
               {sketch::kit::plot(
                    lane.key, {.y = {.domain = {lo, hi}}},
                    {sketch::kit::rules({.y = std::move(ruled)}),
                     sketch::kit::trace([rest](double) { return (double)rest; },
                                        {.pen = {.width = 1.0f},
                                         .samples = 1,
                                         .styleClass = "rest"}),
                     sketch::kit::trace(curve, {.pen = {.width = 1.6f},
                                                .styleClass = lane.series}),
                     sketch::kit::marks(
                         published, dot,
                         {.x = [](double at) { return at; },
                          .y = [curve](double at) { return curve(at); },
                          .styleClass = lane.series})})
                    .inset(0),
                // Held inside the frame: a value at the very top of the
                // range would hang its label off the plot, and a label
                // outside the box it names is a label for nothing.
                each(lane.ticks(),
                     [lo, hi](const Tick& tick) {
                       return text(tick.label)
                           .font({.size = 9.5f, .track = 0.4f})
                           .absolute()
                           .right(5)
                           .top(std::clamp(
                               tickY(tick.value, lo, hi, kPlotH) - 12.0f, 1.0f,
                               kPlotH - 15.0f));
                     })}),
       text(lane.title).font({.size = 11.0f, .track = 0.8f})});
}

}  // namespace

// ===========================================================================

struct ElasticType {
  choreograph::Output<float> pass{0};  // one wrapping 0→1 over kLoop

  sk_sp<SkTypeface> face, faceLabel;

  /** One specimen row: the word, deformed letter by letter.
   *
   *  The cascade is per CLUSTER rather than per glyph, which for this word
   *  is the same thing and stays right for text where it is not — a letter
   *  and its marks are one body and squash together. */
  [[nodiscard]] Element row(const char* word, const char* caption,
                            TextEffect effect) {
    const sigil::weave::TextStyle set = weave::textStyle(
        {.face = face, .size = kWordSize, .color = kInk, .track = 3.0f});

    // THE GHOST: the same word, same style, no track — the rest position
    // the deviation is measured against. A track's deviation is per glyph
    // and lives only in the draw, so nothing else on the sheet says where
    // the undeformed letter was.
    return box().column().gap(8).children(
        {text(caption), kit::restGhost(text(word, set).key(word).fx(
                                           {.effect = std::move(effect),
                                            .stagger = {.eachMs = kEachMs,
                                                        .durationMs = kDurMs},
                                            .progress = &pass}),
                                       kRest)});
  }

  /** The label type is stated once on the root; a caption restates only what
   *  it changes. */
  [[nodiscard]] Element describe() {
    return box()
        .column()
        .padding(42, 48)
        .gap(26)
        .fill(linearGradient({0, 0}, {0, kH},
                             {kPaper, hexColor(0x15151B), kPaper},
                             {0.0f, 0.55f, 1.0f}))
        .font({.face = faceLabel, .size = 11.5f, .track = 2.4f})
        .ink(kLabel)
        .styleSheet(graphSheet())
        .children(
            {box()
                 .row()
                 .alignItems(Align::End)
                 .children({text("ELASTIC TYPE")
                                .font({.size = 12.5f, .track = 3.4f})
                                .ink(kInk)
                                .flexGrow(1),
                            text("ANIMATE.CSS 2013 · SQUASH AND "
                                 "STRETCH 1981")
                                .ink(kFaint)}),
             kit::line({.fill = Fill::color(kFaint)}),
             text("GREY IS THE REST POSE, SHARING THE LIVE LINE'S "
                  "ORIGIN — WHERE IT SHOWS, THAT LETTER "
                  "IS DEFORMED")
                 .font({.size = 10.5f, .track = 0.6f})
                 .ink(kRest),
             row("RUBBERBAND", "rubberBand · SEVEN STOPS ON TWO SCALE AXES",
                 fx::keys(rubberTable(), &cssEase)),
             row("JELLO",
                 "jello · A HALVING, ALTERNATING SHEAR · "
                 "BOTH AXES",
                 fx::keys(jelloTable(), &cssEase)),
             box().flexGrow(1),
             box().row().gap(28).height(146).children(
                 {each(kLanes, lanePanel)}),
             text("A NON-UNIFORM SCALE AND A SHEAR ARE THE ONE "
                  "DEVIATION AN RSXFORM CANNOT CARRY · EVERY "
                  "GLYPH ON THESE TWO LINES DRAWS UNDER ITS OWN "
                  "MATRIX")
                 .font({.size = 11.0f, .track = 0.6f})
                 .ink(kFaint)});
  }

  void setup(sketch::SketchContext& ctx) {
    // Early in the pass: the head of each word is past its overshoot and
    // settling while the tail is still at rest, so one frame shows the whole
    // table laid out along the line.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                             .captureAt = 1.15,
                             .background = kPaper});

    face = weave::ports::face({"Avenir Next", "Futura", "Helvetica Neue"}, 700);
    faceLabel = sketch::kit::houseFace(sketch::kit::Voice::Interface, 500);

    ctx.ticker.add([this, &ticker = ctx.ticker] {
      const double t = ticker.elapsed();
      pass = motion::phase(t, kLoop);
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    ElasticType, "Study · Type",
    "animate.css rubberBand and jello, transcribed number for number and "
    "run per glyph — with the tables plotted")
