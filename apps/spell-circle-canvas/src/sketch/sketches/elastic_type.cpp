// elastic_type.cpp — PATTERN: rubber type. Two published keyframe tables,
// transcribed number for number, run per letter and on the whole word.
// =============================================================================
// THE PATTERN, AND ITS SOURCE
//
// Animate.css (Daniel Eden) is the CSS animation library that put a
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
// ILLUSION OF LIFE: a body deforms under acceleration and preserves its
// volume while it does, which is why the stretched frame is narrow and the
// squashed frame is wide. `rubberBand`'s table obeys that — every pair
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
// The table is what this file states. `textFx::keys` takes the entries and
// does the sampling, and everything else on the plate reads that one
// effect: the letters, the whole word, the traces and the ring riding them.
//
// TWO READINGS OF ONE TABLE. The CSS class deforms the ELEMENT: the whole
// word squashes about its own centre as one body, over the library's
// default second. A lettering artist runs the same table per LETTER, each
// on its own beat — the genre's usual look. The plate sets both side by
// side on one clock, so the head of the per-letter word and the one-body
// word are at the same moment of the table.
//
// WHAT THE TRANSCRIPTION GETS RIGHT:
//
//  * `jello` shears on BOTH axes — the published rule is
//    `skewX(a) skewY(a)`, the same angle on each — so the word rocks on a
//    diagonal.
//  * CSS crosses EACH KEYFRAME SEGMENT with its own timing function, `ease`
//    by default, rather than running one curve across the whole list, and
//    `textFx::keys` means exactly that by a per-segment curve. `cssEase`
//    below is `ease` itself, spelled as CSS writes it.
//
// -----------------------------------------------------------------------------
// WHAT THIS PUTS UNDER LOAD
//
// A non-uniform scale and a shear are the one deviation an RSXform cannot
// carry: that transform encodes a rotation, one scale and no shear at all.
// A glyph whose composed deviation uses `scaleX`, `scaleY`, `skewXDeg` or
// `skewYDeg` therefore leaves the shared transform array and draws under its
// own matrix while its neighbours stay batched. Both per-letter words are a
// whole line of such glyphs, which is the worst case for that split.
//
// EDIT THESE FIRST
//   kEachMs      — start-to-start per letter. At 0 every letter keeps the
//                  one-body word's time, but each still deforms about its
//                  own centre.
//   kDurationMs  — one pass of the table, the library's default second.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/elastic_type.cpp --frame /tmp/elastic_type.png

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/bind/Curve.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas{1080, 620};
constexpr float kMargin = 48;
constexpr float kColumnGap = 48;
constexpr float kColumnWidth = (kCanvas.fWidth - 2 * kMargin - kColumnGap) / 2;
constexpr float kLaneGap = 28;
constexpr float kPlotWidth = (kCanvas.fWidth - 2 * kMargin - 2 * kLaneGap) / 3;
constexpr float kPlotHeight = 112;

constexpr float kEachMs = 62;
constexpr float kDurationMs = 1000;
/** One pass of both words, then a rest before the next. */
constexpr float kLoopSeconds = 3.4f;

/** The plate's colours. The sheet states each once as a custom property;
 *  the ground's three stops also shade the root, which a custom property
 *  cannot carry as a gradient. */
constexpr material::Color kPaper = hexColor(0x101014);
constexpr material::Color kPaperLift = hexColor(0x15151B);

// ---------------------------------------------------------------------------
// The tables, and the curve every segment of one is crossed with.

using Table = std::vector<textFx::Key>;

/** CSS's default `animation-timing-function`, `ease`. A keyframe list that
 *  names no timing function is crossed with it one segment at a time. */
choreograph::EaseFn cssEase() {
  return motion::ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f);
}

/** rubberBand: a squash and a stretch on the two scale axes. */
Table rubberBandTable() {
  return {{0.00f, {}},
          {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
          {0.40f, {.scaleX = 0.75f, .scaleY = 1.25f}},
          {0.50f, {.scaleX = 1.15f, .scaleY = 0.85f}},
          {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
          {0.75f, {.scaleX = 1.05f, .scaleY = 0.95f}},
          {1.00f, {}}};
}

/** jello: a halving, alternating shear, the same angle on both axes, which
 *  is what makes the word rock on a diagonal rather than side to side.
 *
 *  A glyph's two skew angles, like a node's, are ONE shear pair, where
 *  CSS's `skewX(a) skewY(a)` multiplies two shears and so also widens the
 *  word by the product of the two tangents; the word here leans without
 *  that widening. */
Table jelloTable() {
  const auto shear = [](float at, float degrees) {
    return textFx::Key{at, {.skewXDeg = degrees, .skewYDeg = degrees}};
  };
  return {shear(0.000f, 0.0f),        shear(0.111f, 0.0f),
          shear(0.222f, -12.5f),      shear(0.333f, 6.25f),
          shear(0.444f, -3.125f),     shear(0.555f, 1.5625f),
          shear(0.666f, -0.78125f),   shear(0.777f, 0.390625f),
          shear(0.888f, -0.1953125f), shear(1.000f, 0.0f)};
}

const TextEffect& rubberBand() {
  static const TextEffect effect = textFx::keys(rubberBandTable(), cssEase());
  return effect;
}

const TextEffect& jello() {
  static const TextEffect effect = textFx::keys(jelloTable(), cssEase());
  return effect;
}

/** The deviation a table stands at, at local time @p t. The tables are pure
 *  functions of local time, so the whole word, the traces and the ring read
 *  the value the letters are drawn from. */
GlyphModifier deviation(const TextEffect& effect, float t) {
  sigil::core::noise::Mix64Stream stream(1);
  return effect(GlyphInfo{}, t, stream);
}

// Plain functions, so a binding that maps through one compares by it.
float rubberBandScaleX(float t) { return deviation(rubberBand(), t).scaleX; }
float rubberBandScaleY(float t) { return deviation(rubberBand(), t).scaleY; }
float jelloShear(float t) { return deviation(jello(), t).skewXDeg; }

// ---------------------------------------------------------------------------
// The plots: one lane of one table each.

/** A value a lane is ruled at, and how its number reads. */
struct Tick {
  double value;
  const char* reading;
};

/** WHAT A LANE'S BOX SPANS: its range, where the rest pose stands, and the
 *  values it is ruled at. The extremes ruled are the tables' own outer
 *  values, so a trace that touches a ruled line is a transcription a
 *  reader can check. */
struct LaneScale {
  float low, high, rest;
  std::array<Tick, 3> ticks;
};

constexpr LaneScale kScaleFactor{
    0.62f, 1.38f, 1.0f, {{{1.25, "1.25"}, {1.00, "1.00"}, {0.75, "0.75"}}}};
// Each box runs far enough past its outer ruled values that a reading,
// centred on its rule, keeps its ground clear of the frame's keyline.
constexpr LaneScale kShearDegrees{
    -17.0f, 17.0f, 0.0f, {{{12.5, "+12.5°"}, {0.0, "0°"}, {-12.5, "−12.5°"}}}};

/** ONE LANE OF ONE PUBLISHED TABLE: the table, the field of its deviation
 *  the lane reads, the series class it is drawn in, and its scale. */
struct Lane {
  const char* key;
  const char* title;
  Table (*table)();
  float (*value)(float);
  const char* series;
  const LaneScale& scale;
};

const std::array<Lane, 3> kLanes{{
    {"lane-scale-x", "rubberBand — scaleX 0.75 to 1.25", rubberBandTable,
     rubberBandScaleX, "x", kScaleFactor},
    {"lane-scale-y", "rubberBand — scaleY 0.75 to 1.25", rubberBandTable,
     rubberBandScaleY, "y", kScaleFactor},
    {"lane-shear", "jello — skewX = skewY ±12.5°, halving", jelloTable,
     jelloShear, "x", kShearDegrees},
}};

/** ONE LANE, FRAMED AND NAMED: the effect's own curve, the stops the
 *  reference publishes as dots on it — a second reading of the same lane —
 *  the rest pose and the ruled values, numbered where they are ruled. */
Element lanePanel(const Lane& lane) {
  std::vector<double> ruled;
  std::vector<double> numbered;
  for (const Tick& tick : lane.scale.ticks) {
    numbered.push_back(tick.value);
    if (tick.value != lane.scale.rest) ruled.push_back(tick.value);
  }
  std::vector<double> stops;
  for (const textFx::Key& key : lane.table()) stops.push_back(key.at);
  const auto value = [read = lane.value](double t) {
    return (double)read((float)t);
  };
  const auto number = [ticks = lane.scale.ticks](double at) {
    const auto tick = std::ranges::find(ticks, at, &Tick::value);
    return text(tick->reading).styleClass("reading");
  };

  return box()
      .column()
      .gap(7)
      .width(kPlotWidth)
      .children(
          // A rule cannot state a stroke, so the frame's keyline is the node's.
          {box()
               .styleClass("frame")
               .stroke(stroke(1, Fill::var("faint")))
               .children(
                   {sketch::kit::plot(
                        lane.key,
                        {.y = {.domain = {lane.scale.low, lane.scale.high}}},
                        {sketch::kit::rules({.y = std::move(ruled)}),
                         sketch::kit::rules(
                             {.y = {lane.scale.rest}, .styleClass = "rest"}),
                         sketch::kit::trace(value, {.pen = {.width = 1.6f},
                                                    .styleClass = lane.series}),
                         sketch::kit::marks(
                             stops, [] { return box().styleClass("stop"); },
                             {.x = [](double at) { return at; },
                              .y = value,
                              .styleClass = lane.series}),
                         sketch::kit::axis({.of = sketch::kit::Axis::Y,
                                            .at = 1.0,
                                            .ticks = std::move(numbered),
                                            .line = false,
                                            .reach = 0.0f,
                                            .gap = 6.0f,
                                            .tickLine = number})})
                        .inset(0)}),
           document::caption(lane.title)});
}

// ---------------------------------------------------------------------------
// The specimens.

/** The word in the specimen face, at rest. */
Text specimen(std::string_view word) { return text(word).role("specimen"); }

/** The moving word over its own rest pose, both at one origin — where the
 *  grey shows, the live word has stretched or sheared. The rest pose is the
 *  plain word rather than `Text::atRest`, because the one-body word deforms
 *  as a node and a copy of it would deform with it. @p key names the
 *  moving word, and its rest pose is keyed after it. */
Element overRest(std::string_view word, const std::string& key, Text moving) {
  return box()
      .width(kColumnWidth)
      .children({specimen(word).styleClass("rest").key(key + "-rest"),
                 moving.key(key).absolute().left(0).top(0)});
}

/** HOW THE PLATE IS SET: the colours as custom properties, one interface
 *  face over everything read, the specimen face for the two words, and a
 *  class per series the plots and the ring are drawn in. */
StyleSheet sheet() {
  return StyleSheet{
      rule(":root")
          .var("paper", kPaper)
          .var("ink", hexColor(0xF6F2E9))
          .var("label", hexColor(0x848B99))
          .var("faint", hexColor(0x2E3440))
          .var("rest", hexColor(0x4A5262))
          .var("x", hexColor(0xFF7A59))
          .var("y", hexColor(0x5AC8F5))
          .fontFamily(".SF NS, SF Pro Text, Helvetica Neue, system-ui")
          .fontWeight(500)
          .fontSize(11.5f)
          .letterSpacing(0.2f)
          .ink(var("label")),
      rule("h1").fontSize(26).ink(var("ink")),
      rule("lead").fontSize(12).letterSpacing(0.3f),
      rule("h2").fontSize(13).ink(var("ink")),
      rule("eyebrow").letterSpacing(2.4f),
      rule("caption").fontSize(11).letterSpacing(0.8f),
      rule("specimen")
          .fontFamily("Avenir Next, Futura, Helvetica Neue, sans-serif")
          .fontWeight(700)
          .fontSize(60)
          .letterSpacing(3)
          .ink(var("ink")),
      rule("rule, .plotRule").ink(var("faint")),
      rule(".plotTick").ink(var("label")),
      rule(".rest").ink(var("rest")),
      rule(".x").ink(var("x")),
      rule(".y").ink(var("y")),
      rule(".frame").width(pct(100)).height(kPlotHeight),
      // A number is knocked out of the hairline it names.
      rule(".reading")
          .fontSize(9.5f)
          .letterSpacing(0.4f)
          .padding(0, 4)
          .fill(Fill::var("paper")),
      rule(".stop")
          .width(5.2f)
          .height(5.2f)
          .borderRadius(Corners{2.6f})
          .fill(Fill::currentInk()),
      rule(".cursor")
          .width(1)
          .height(kPlotHeight)
          .opacity(0.35f)
          .fill(Fill::currentInk()),
      rule(".ring")
          .width(9)
          .height(9)
          .borderRadius(Corners{4.5f})
          .fill(Fill::var("paper")),
  };
}

}  // namespace

// ===========================================================================

struct ElasticType {
  /** Seconds into one pass, wrapping: the one value that moves. Every
   *  word, cursor and ring is a binding over it. */
  choreograph::Output<float> seconds{0};

  /** The first @p length seconds of the pass, as 0 → 1, held at 1 after. */
  [[nodiscard]] motion::Bound playing(float lengthSeconds) const {
    return motion::bind(&seconds).window(0, lengthSeconds);
  }

  /** The word, each letter running @p effect on its own beat. */
  [[nodiscard]] Text perLetter(std::string_view word,
                               const TextEffect& effect) const {
    const motion::Spread cascade{.eachMs = kEachMs, .durationMs = kDurationMs};
    return specimen(word).textFx(
        {.effect = effect,
         .stagger = cascade,
         .progress = playing(cascade.spanMs((uint32_t)word.size()) / 1000.0f)});
  }

  /** @p value of a table over the one-body word's pass. */
  [[nodiscard]] motion::Bound oneBody(float (*value)(float)) const {
    return playing(kDurationMs / 1000.0f).map(value);
  }

  /** One effect's row: its name, then the word per letter beside the word
   *  as one body. */
  [[nodiscard]] Element effectRow(std::string_view word, const char* eyebrow,
                                  const TextEffect& effect, Text body) const {
    return box().column().gap(6).children(
        {document::eyebrow(eyebrow),
         box()
             .row()
             .gap(kColumnGap)
             .children(
                 {overRest(word, std::string(word), perLetter(word, effect)),
                  overRest(word, std::string(word) + "-body", body)})});
  }

  /** Where the one-body word stands on @p lane's curve: a cursor at its
   *  moment of the table and a ring at its value, over the plot and never
   *  inside its texture. */
  [[nodiscard]] Element playhead(const Lane& lane) const {
    const float perUnit = -kPlotHeight / (lane.scale.high - lane.scale.low);
    const motion::Bound across =
        playing(kDurationMs / 1000.0f).target(0, kPlotWidth);
    return box()
        .width(kPlotWidth)
        .height(kPlotHeight)
        .styleClass(lane.series)
        .children({box().styleClass("cursor").absolute().translateX(across),
                   // A rule cannot state a stroke, so the ring's is the node's.
                   box()
                       .styleClass("ring")
                       .stroke(stroke(1.5f))
                       .absolute()
                       .left(-4.5f)
                       .top(-4.5f)
                       .translateX(across)
                       .translateY(oneBody(lane.value)
                                       .scale(perUnit)
                                       .offset(-perUnit * lane.scale.high))});
  }

  [[nodiscard]] Element describe() const {
    return box()
        .column()
        .padding(40, kMargin)
        .gap(18)
        .fill(linearGradient({0, 0}, {0, kCanvas.fHeight},
                             {kPaper, kPaperLift, kPaper}, {0.0f, 0.55f, 1.0f}))
        .applyStyleSheet(sheet())
        .children(
            {box().column().gap(5).children(
                 {document::h1("Elastic type"),
                  document::lead("Animate.css · rubberBand and jello")}),
             document::rule(),
             box()
                 .row()
                 .gap(kColumnGap)
                 .children(
                     {document::h2(kit::formatted("Per letter · each glyph on "
                                                  "its own beat, %.0f ms apart",
                                                  kEachMs))
                          .width(kColumnWidth),
                      document::h2("One body · the class on the element, as a "
                                   "browser runs it")
                          .width(kColumnWidth)}),
             effectRow("RUBBERBAND",
                       "rubberBand · SEVEN STOPS ON TWO SCALE AXES",
                       rubberBand(),
                       specimen("RUBBERBAND")
                           .scaleX(oneBody(rubberBandScaleX))
                           .scaleY(oneBody(rubberBandScaleY))),
             effectRow("JELLO",
                       "jello · A HALVING, ALTERNATING SHEAR · BOTH AXES",
                       jello(),
                       specimen("JELLO")
                           .skewX(oneBody(jelloShear))
                           .skewY(oneBody(jelloShear))),
             box().flexGrow(1),
             // The plots never move, so they are held as one texture; the
             // cursors ride over them on their own row.
             box().children(
                 {box()
                      .row()
                      .gap(kLaneGap)
                      .cache(Cache::Texture)
                      .key("lanes")
                      .children({each(kLanes, lanePanel)}),
                  box().absolute().left(0).top(0).row().gap(kLaneGap).children(
                      {each(kLanes,
                            [this](const Lane& lane) {
                              return playhead(lane);
                            })})}),
             document::footer(
                 "Grey is each word at rest. Every segment of a table is "
                 "crossed with CSS ease; the plots draw the values that deform "
                 "the type, and the ring rides the one-body word.")});
  }

  void setup(sketch::SketchContext& ctx) {
    // Early in the pass: the one-body RUBBERBAND stands at the table's
    // stretch while the per-letter word lays the squash that led to it
    // along the line, head to tail.
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = 0.40, .background = kPaper});
    ctx.ticker.add([this](double, double elapsed) {
      seconds = motion::phase(elapsed, kLoopSeconds) * kLoopSeconds;
    });
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    ElasticType, "Study · Type",
    "animate.css rubberBand and jello, transcribed number for number and "
    "run per letter and on the whole word — with the tables plotted")
