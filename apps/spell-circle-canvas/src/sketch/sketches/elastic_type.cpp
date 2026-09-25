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
// THE WORDS AS BODIES
//
// A table deforms something, so each moving word is drawn as a thing with
// substance: a latex face lit from above and a slab of the same word set a
// few pixels behind it for thickness, both carrying the same deformation
// because both are the same word under the same tracks, standing in a pool
// of shadow on a dark ground under a warm lamp. A letter BLUSHES under strain, in
// the plate's own colour key: pulled wide it warms toward the scaleX
// trace's colour, pulled tall it cools toward the scaleY trace's, and
// under jello's lean it warms with the size of the shear. The blush is
// derived from the transcribed table, stop for stop, so it can never fall
// out of step with the shape it colours. The rest pose is a keyline, so
// where a letter has left its outline it has stretched or sheared.
//
// THE BEATS. rubberBand opens the loop. jello answers on the moment
// rubberBand's one-body word lands its first squash — the table's 30%
// stop, 300 ms in — so the two rows read as call and response rather
// than as two clocks started together. The plots' stops carry their
// keyframe offsets as CSS writes them, and the playheads keep each row's
// own moment of its table.
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
//   kJelloEntrySeconds — when jello answers, as seconds into the loop.
//   kBlush       — how far a letter under full strain moves toward its
//                  series colour, 0 to 1.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/elastic_type.cpp --frame /tmp/elastic_type.png

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::material::hexColor;

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
/** jello answers when rubberBand's one-body word lands its first squash,
 *  the table's 30% stop. */
constexpr float kJelloEntrySeconds = 0.3f * kDurationMs / 1000.0f;
/** One pass of both words, then a rest before the next. */
constexpr float kLoopSeconds = 3.4f;
constexpr float kBlush = 0.8f;

/** The plate's colours. The sheet states each once as a custom property;
 *  the ground's stops and the blush a letter wears read them as values,
 *  because a gradient stop and a glyph's colour multiplier cannot read a
 *  custom property. */
constexpr material::Color kPaper = hexColor(0x101014);
constexpr material::Color kPaperLift = hexColor(0x191922);
constexpr material::Color kSeriesX = hexColor(0xFF7A59);
constexpr material::Color kSeriesY = hexColor(0x5AC8F5);
/** The light over the specimens, warm, from above the two rows. */
constexpr material::Color kLamp = {1.0f, 0.86f, 0.68f, 0.075f};

// ---------------------------------------------------------------------------
// The tables, and the curve every segment of one is crossed with.

using Table = std::vector<textFx::Key>;

/** CSS's default `animation-timing-function`, `ease`. A keyframe list that
 *  names no timing function is crossed with it one segment at a time. */
motion::Easing cssEase() {
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

/** How far one stop of a table pulls a letter, from −1 to 1: positive
 *  toward the colour its first series is drawn in, negative toward the
 *  second's. */
using Strain = float (*)(const GlyphModifier&);

/** rubberBand pulls a letter wide when scaleX leads and tall when scaleY
 *  does; the table's widest gap between the two is half a unit. */
float widening(const GlyphModifier& stop) {
  return (stop.scaleX - stop.scaleY) / 0.5f;
}

/** jello pulls a letter by the size of its lean, whichever way it leans. */
float leaning(const GlyphModifier& stop) {
  return std::abs(stop.skewXDeg) / 12.5f;
}

/** THE BLUSH: a colour table read off @p table stop for stop, so it is
 *  crossed with the same curve at the same moments and cannot drift from
 *  the shape it colours. A letter under full strain moves `kBlush` of the
 *  way toward its series colour, mixed in linear light, and at rest it
 *  wears no tint at all. */
TextEffect blush(const Table& table, Strain strain, material::Color positive,
                 material::Color negative) {
  constexpr material::Color untinted{1, 1, 1, 1};
  Table tints;
  for (const textFx::Key& stop : table) {
    const float amount = std::clamp(strain(stop.modifier), -1.0f, 1.0f);
    tints.push_back(
        {stop.at,
         {.colorMultiplier = material::mixLinear(
              untinted, amount >= 0 ? positive : negative,
              kBlush * std::abs(amount))}});
  }
  return textFx::keys(std::move(tints), cssEase());
}

const TextEffect& rubberBandBlush() {
  static const TextEffect effect =
      blush(rubberBandTable(), widening, kSeriesX, kSeriesY);
  return effect;
}

const TextEffect& jelloBlush() {
  static const TextEffect effect =
      blush(jelloTable(), leaning, kSeriesX, kSeriesX);
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
 *  the lane reads, the series class it is drawn in, its scale, and when in
 *  the loop its row starts. */
struct Lane {
  const char* key;
  const char* title;
  Table (*table)();
  float (*value)(float);
  const char* series;
  const LaneScale& scale;
  float startSeconds;
};

const std::array<Lane, 3> kLanes{{
    {"lane-scale-x", "rubberBand — scaleX 0.75 to 1.25", rubberBandTable,
     rubberBandScaleX, "x", kScaleFactor, 0},
    {"lane-scale-y", "rubberBand — scaleY 0.75 to 1.25", rubberBandTable,
     rubberBandScaleY, "y", kScaleFactor, 0},
    {"lane-shear", "jello — skewX = skewY ±12.5°, halving", jelloTable,
     jelloShear, "x", kShearDegrees, kJelloEntrySeconds},
}};

/** A keyframe offset as a stylesheet writes it: whole percents where the
 *  table has them, one decimal where it does not. */
std::string percent(double at) {
  const double value = at * 100.0;
  return std::abs(value - std::round(value)) < 0.05
             ? kit::formatted("%.0f%%", value)
             : kit::formatted("%.1f%%", value);
}

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
  // The frame's own edges are 0% and 100%, so only the stops between them
  // are numbered.
  const std::vector<double> inner(stops.begin() + 1, stops.end() - 1);
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
                        {sketch::kit::rules({.x = inner,
                                             .pen = {.width = 1,
                                                     .dashIntervals = {2, 3}},
                                             .styleClass = "keyframe"}),
                         sketch::kit::rules({.y = std::move(ruled)}),
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
                                            .tickLine = number}),
                         sketch::kit::axis(
                             {.of = sketch::kit::Axis::X,
                              .ticks = inner,
                              .line = false,
                              .reach = 0.0f,
                              .gap = 5.0f,
                              .tickLine = [](double at) {
                                return text(percent(at)).styleClass("offset");
                              }})})
                        .inset(0)}),
           document::caption(lane.title).marginTop(14)});
}

// ---------------------------------------------------------------------------
// The specimens.

/** The word in the specimen face, at rest. */
Text specimen(std::string_view word) { return text(word).role("specimen"); }

/** THE MOVING WORD AS A BODY over its own rest pose, all at one origin:
 *  the rest pose's keyline, the slab that gives the body thickness, and
 *  its face. @p moving builds the word as it deforms; it is called once
 *  per layer, because a layer is a node of its own and the two must carry
 *  the same tracks and the same transforms to move as one. The rest pose
 *  is the plain word rather than `Text::atRest`, because the one-body word
 *  deforms as a node and a copy of it would deform with it. @p key names
 *  the face, and every other layer is keyed after it.
 *
 *  The slab carries the pool of shadow the body stands in, attached under
 *  the word's own extent: a child of the slab, so a word that squashes as
 *  one body spreads its shadow with it, while letters that move one by
 *  one stand over a floor that stays put. A glyph-outline decoration is
 *  cut from the glyphs at rest, so no shadow here follows a letter a track
 *  has deformed. */
template <class Moving>
Element relief(std::string_view word, const std::string& key,
               const Moving& moving) {
  return box()
      .width(kColumnWidth)
      .children({specimen(word).styleClass("rest").key(key + "-rest"),
                 moving()
                     .styleClass("slab")
                     .key(key + "-slab")
                     .absolute()
                     .left(0)
                     .top(0)
                     .textAttach(sigil::weave::Selector{},
                                 box()
                                     .styleClass("floor")
                                     .left(pct(-6))
                                     .top(pct(70))
                                     .width(pct(112))
                                     .height(pct(36))),
                 moving().key(key).absolute().left(0).top(0)});
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
          .var("x", kSeriesX)
          .var("y", kSeriesY)
          .var("slab", hexColor(0x4A2C26))
          .fontFamily(".SF NS, SF Pro Text, Helvetica Neue, system-ui")
          .fontWeight(500)
          .fontSize(11.5f)
          .letterSpacing(0.2f)
          .ink(var("label")),
      rule("h1")
          .fontFamily("Avenir Next, Futura, Helvetica Neue, sans-serif")
          .fontWeight(700)
          .fontSize(28)
          .letterSpacing(0.4f)
          .ink(var("ink")),
      rule("lead").fontSize(12).letterSpacing(0.3f),
      rule("h2").fontSize(13).ink(var("ink")),
      rule("eyebrow").letterSpacing(2.4f),
      rule("caption").fontSize(11).letterSpacing(0.8f),
      // The face is latex under a lamp: lit at the cap line, deepening
      // toward the baseline, the ramp laid afresh on each line so a word
      // that stretches carries its light with it.
      rule("specimen")
          .fontFamily("Avenir Next, Futura, Helvetica Neue, sans-serif")
          .fontWeight(700)
          .fontSize(60)
          .letterSpacing(3)
          .ink(material::Paint::linearGradient(
                   {0, 0}, {0, 1},
                   {{0.0f, hexColor(0xFFFBF3)},
                    {0.55f, hexColor(0xF3EADB)},
                    {1.0f, hexColor(0xD9C8AE)}}),
               PaintBox::Line),
      // The rest pose is only its outline, so the body over it reads as
      // having left it wherever the outline shows.
      rule("specimen.rest")
          .ink(material::Color{0, 0, 0, 0})
          .textStroke(1.1f, Fill::var("rest")),
      rule("specimen.slab").ink(var("slab")).translateX(2.5f).translateY(4),
      // The pool is an ellipse because the unit square it is laid over is
      // as wide as the word and far shallower than it.
      rule(".floor").fill(material::Paint::radialGradient(
          {0.5f, 0.5f}, 1.0f,
          {{0.0f, material::Color{0, 0, 0, 0.8f}},
           {0.55f, material::Color{0, 0, 0, 0.3f}},
           {1.0f, material::Color{0, 0, 0, 0}}},
          {.extent = material::RadialExtent::ClosestSide})),
      rule(".offset").fontSize(9).letterSpacing(0.3f).ink(var("label")),
      rule(".keyframe").ink(var("faint")),
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

/** THE GROUND THE WORDS STAND ON: dark paper, a lamp's warm pool over the
 *  two rows of specimens, and the corners falling away. None of it moves,
 *  so it is one texture. */
Element ground() {
  return box()
      .absolute()
      .inset(0)
      .cache(Cache::Texture)
      .key("ground")
      // The paper asks for a grain, but `kit::grained` puts no grain on a
      // near-black ground: it folds its noise in by soft light, which moves
      // a colour this dark by under one level, so this fill reads as plain
      // `kPaper` until the kit's grain holds its strength on dark grounds.
      .fill(kit::grained(kPaper, 0.07f, 0.9f))
      .children(
          {box().absolute().inset(0).fill(material::Paint::linearGradient(
               {0, 0}, {0, kCanvas.fHeight},
               {{0.15f, material::Color{0, 0, 0, 0}},
                {0.5f, kPaperLift},
                {0.95f, material::Color{0, 0, 0, 0}}},
               {.units = material::GradientUnits::Pixels})),
           box().absolute().inset(0).fill(material::Paint::radialGradient(
               {kCanvas.fWidth * 0.5f, 250}, 560,
               {{0.0f, kLamp},
                {1.0f, material::Color{kLamp.r, kLamp.g, kLamp.b, 0}}},
               {.units = material::GradientUnits::Pixels})),
           box().absolute().inset(0).fill(
               kit::vignette(kCanvas, {0, 0, 0, 0.5f}, 0.5f))});
}

}  // namespace

// ===========================================================================

struct ElasticType {
  /** Seconds into one pass, wrapping: the one value that moves. Every
   *  word, cursor and ring is a binding over it. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);

  /** @p lengthSeconds of the loop from @p startSeconds, as 0 → 1, held at
   *  0 before and at 1 after. */
  [[nodiscard]] motion::Bound playing(float startSeconds,
                                      float lengthSeconds) const {
    return motion::bind(seconds, {.from = {startSeconds, startSeconds + lengthSeconds}, .clampFrom = true});
  }

  /** @p value of a table over a one-body word's pass from @p startSeconds. */
  [[nodiscard]] motion::Bound oneBody(float (*value)(float),
                                      float startSeconds) const {
    return playing(startSeconds, kDurationMs / 1000.0f).map(value);
  }

  /** ONE EFFECT'S ROW: its name, then the word per letter beside the word
   *  as one body, both starting at @p startSeconds. @p deform lays the
   *  table on the one-body word as the node's own transform; every letter
   *  of both words wears @p tint over the same moments. */
  template <class Deform>
  [[nodiscard]] Element effectRow(std::string_view word, Text eyebrow,
                                  const TextEffect& effect,
                                  const TextEffect& tint, float startSeconds,
                                  const Deform& deform) const {
    const motion::Spread cascade{.eachMs = kEachMs, .durationMs = kDurationMs};
    const motion::Bound letters = playing(
        startSeconds, cascade.spanMs((uint32_t)word.size()) / 1000.0f);
    const motion::Bound whole = playing(startSeconds, kDurationMs / 1000.0f);
    const auto perLetter = [&] {
      return specimen(word)
          .textFx({.effect = effect, .stagger = cascade, .progress = letters})
          .textFx({.effect = tint, .stagger = cascade, .progress = letters});
    };
    const auto oneBodyWord = [&] {
      return deform(specimen(word).textFx(
          {.effect = tint,
           .delay = motion::stagger(0ms), .duration = std::chrono::duration<double, std::milli>(kDurationMs),
           .progress = whole}));
    };
    return box().column().gap(6).children(
        {eyebrow.span(sigil::weave::selectors::word(0),
                      SpanStyle()
                          .fontFamily("SF Mono, Menlo, monospace")
                          .letterSpacing(0)
                          .ink(var("ink"))),
         box()
             .row()
             .gap(kColumnGap)
             .children({relief(word, std::string(word), perLetter),
                        relief(word, std::string(word) + "-body",
                               oneBodyWord)})});
  }

  /** Where the one-body word stands on @p lane's curve: a cursor at its
   *  moment of the table and a ring at its value, over the plot and never
   *  inside its texture. */
  [[nodiscard]] Element playhead(const Lane& lane) const {
    const float perUnit = -kPlotHeight / (lane.scale.high - lane.scale.low);
    const motion::Bound across =
        playing(lane.startSeconds, kDurationMs / 1000.0f)
            .target(0, kPlotWidth);
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
                       .translateY(oneBody(lane.value, lane.startSeconds)
                                       .scale(perUnit)
                                       .offset(-perUnit * lane.scale.high))});
  }

  [[nodiscard]] Element describe() const {
    const auto squashAndStretch = [this](Text word) {
      return word.scaleX(oneBody(rubberBandScaleX, 0))
          .scaleY(oneBody(rubberBandScaleY, 0));
    };
    const auto shear = [this](Text word) {
      return word.skewX(oneBody(jelloShear, kJelloEntrySeconds))
          .skewY(oneBody(jelloShear, kJelloEntrySeconds));
    };
    return box()
        .width(kCanvas.fWidth)
        .height(kCanvas.fHeight)
        .applyStyleSheet(sheet())
        .children(
            {ground(),
             box()
                 .column()
                 .flexGrow(1)
                 .padding(40, kMargin)
                 .gap(18)
                 .children(
                     {box().column().gap(5).children(
                          {document::h1("Elastic type"),
                           document::lead(
                               "Animate.css · rubberBand, then jello")}),
                      document::rule(),
                      box()
                          .row()
                          .gap(kColumnGap)
                          .children(
                              {document::h2(kit::formatted(
                                                "Per letter · each glyph on "
                                                "its own beat, %.0f ms apart",
                                                kEachMs))
                                   .width(kColumnWidth),
                               document::h2("One body · the class on the "
                                            "element, as a browser runs it")
                                   .width(kColumnWidth)}),
                      effectRow("RUBBERBAND",
                                document::eyebrow("rubberBand · SEVEN STOPS "
                                                  "ON TWO SCALE AXES"),
                                rubberBand(), rubberBandBlush(), 0,
                                squashAndStretch),
                      effectRow("JELLO",
                                document::eyebrow(
                                    "jello · ANSWERS THE FIRST SQUASH · A "
                                    "HALVING SHEAR ON BOTH AXES"),
                                jello(), jelloBlush(), kJelloEntrySeconds,
                                shear),
                      box().flexGrow(1),
                      // The plots never move, so they are held as one
                      // texture; the cursors ride over them on their own
                      // row.
                      box().children(
                          {box()
                               .row()
                               .gap(kLaneGap)
                               .cache(Cache::Texture)
                               .key("lanes")
                               .children({each(kLanes, lanePanel)}),
                           box()
                               .absolute()
                               .left(0)
                               .top(0)
                               .row()
                               .gap(kLaneGap)
                               .children({each(kLanes,
                                               [this](const Lane& lane) {
                                                 return playhead(lane);
                                               })})}),
                      document::footer(
                          "The keyline is each word at rest. A letter warms "
                          "as a table pulls it wide and cools as it pulls it "
                          "tall, in the colours its traces are drawn in; "
                          "every segment is crossed with CSS ease.")})});
  }

  void setup(sketch::SketchContext& ctx) {
    // Early in the pass: the one-body RUBBERBAND stands at the table's
    // stretch while the per-letter word lays the squash that led to it
    // along the line, head to tail, and jello has just answered.
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = 0.52, .background = kPaper});
    ctx.engine.add([this](double, double elapsed) {
      seconds = motion::phase(elapsed, kLoopSeconds) * kLoopSeconds;
    });
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    ElasticType, "Study · Type",
    "animate.css rubberBand and jello, transcribed number for number and "
    "run per letter and on the whole word — with the tables plotted")
