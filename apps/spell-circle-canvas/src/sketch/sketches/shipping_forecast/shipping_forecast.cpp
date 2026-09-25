// shipping_forecast.cpp — THE SHIPPING FORECAST, set as a sheet that
// performs itself: the 0048 bulletin of BBC Radio 4, whose sea areas are
// read in one fixed clockwise order round the British Isles and whose
// every adjective is a defined quantity.
// =============================================================================
// SUBJECT  A broadcast that is really a TYPOGRAPHIC form: a controlled
//          vocabulary in a fixed order, every word of it a number. The
//          areas are a ring, the terms a glossary, the pressure a readout,
//          and the one thing that changes between bulletins is which area
//          is being read — so the sheet has one dominant move, that name
//          arriving in the middle of its own ring, lit again on the ring
//          where it stands, and everything else supports it. A hand reads
//          the ring round in order and rests on that area while the column
//          sets its forecast; under the name, through the ring's porthole,
//          the chart the synopsis describes deepens and drifts on.
//
// FROM THE RECORD
//   * Broadcast by BBC Radio 4 for the Maritime and Coastguard Agency from
//     a Met Office bulletin; the 0048 edition goes out on long wave,
//     198 kHz, which is what the spine says.
//   * Thirty-one sea areas read in one order, broadly clockwise from
//     Viking. The ring carries the first sixteen, each at its own compass
//     bearing from the middle of the islands.
//   * The timing terms are definitions timed from issue — IMMINENT within
//     six hours, SOON six to twelve, LATER beyond twelve — and so are the
//     visibility terms and the pressure tendencies. They are set as
//     glossary entries in a serif italic, not as emphasis.
//   * Wind is a direction and a Beaufort force, so the numerals are the
//     one thing in a paragraph picked out by pattern rather than by name.
//
// THIS STUDY'S OWN: the forecast text, the stations and the 1003 are
// plausible inventions in the real vocabulary; every colour, beat and
// radius, the decision to set the areas on a ring at all, the reading
// hand, and where the chart puts the low and the high.
//
// HOW IT IS BUILT
//   The words are `data/content.json`; the look is `sheet()` — the
//   palette as custom properties on the root, the type as roles and
//   classes; the motion is two stepped clocks and every beat a window,
//   a swell or an envelope shaped from them, so nothing is re-described
//   after setup.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/shipping_forecast/shipping_forecast.cpp \
//       --frame /tmp/shipping_forecast.png --scale 2
//   The whole bulletin:  --at 0.2 --frames 30 --fps 4

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Heading.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <numbers>
#include <ranges>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace arrange = sigil::geometry::arrange;
namespace data = sigil::data;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr SkSize kCanvas{1440, 880};

// The night sea the sheet is set on, the lift a panel washes it with, and
// the accent every light on the sheet is struck in. A gradient takes
// colours rather than custom properties, so these are constants, and the
// sheet's tokens are stated from them.
constexpr material::Color kSea = hexColor(0x06090E);
constexpr material::Color kSeaLift = hexColor(0x0B111A);
constexpr material::Color kAmber = hexColor(0xF0A03C);

// The ring panel: the square it stands in, the baseline the sea areas sit
// on, and the hairline inside the lettering, whose disc is the porthole
// the pressure chart shows through.
constexpr float kRingBox = 660;
constexpr float kRingRadius = 292;
constexpr float kInnerRadius = 238;
constexpr float kPorthole = kInnerRadius - 9;
constexpr SkPoint kEye{kRingBox * 0.5f, kRingBox * 0.5f};

// One pass is a bulletin, and its last seconds are dark, so a loop that
// wraps does so on an unlit sheet.
constexpr float kLoop = 15;
// The grade swell's period; the still is taken at its second peak.
constexpr float kBreathPeriod = 7.2f;
// The gale lamp's period and the share of it the lamp is lit.
constexpr float kLampPeriod = 1.5f;
constexpr float kLampLit = 0.6f;

// THE READING: a hand sweeps the ring in reading order at one pace, rests
// on the area being read for as long as the column sets its forecast, then
// reads on to the last area and goes dark. Every area rises as the hand
// reaches it, so the pace is in degrees per second.
constexpr float kReadingStarts = 0.30f;
constexpr float kReadingPace = 75;
constexpr float kReadingResumes = 4.20f;
// The low deepens and drifts toward Fair Isle across this stretch of the
// bulletin, after the synopsis has named it.
constexpr float kLowFrom = 2.70f;
constexpr float kLowTo = 10.5f;
constexpr float kLowDeepens = 0.84f;

/** A POINT ON THE CHART at @p bearing, degrees clockwise from north, and
 *  @p radius from the middle of the islands. */
SkPoint compass(float bearing, float radius) {
  const float radians = (bearing - 90) * std::numbers::pi_v<float> / 180;
  return arrange::onEllipse(kEye, {radius, radius}, radians);
}

/** A SEA AREA AND ITS BEARING, degrees clockwise from north — rounded to
 *  the ring's legibility, since two areas three degrees apart would set
 *  as one word. */
struct Area {
  std::string name;
  float bearing = 0;
};

/** HOW THE SHEET IS SET. The palette is custom properties on the root and
 *  every class below is a whole look. Sizes are pixels: `rem` measures
 *  against the composer's inherited font rather than the root element's,
 *  so a root size stated here would not scale them. */
StyleSheet sheet() {
  const std::string grotesque = "system-ui, Helvetica Neue, sans-serif";
  const std::string book = "Iowan Old Style, Charter, Georgia, serif";
  const std::string mono = "Menlo, SF Mono, Courier New, monospace";
  return StyleSheet{
      rule(":root")
          .var("bone", hexColor(0xE9E5DB))
          .var("slate", hexColor(0x76828F))
          .var("slate-dim", hexColor(0x76828F, 0.86f))
          .var("keyline", hexColor(0x1A2532))
          .var("keyline-deep", hexColor(0x121B26))
          .var("bar", hexColor(0x37475B))
          .var("amber", kAmber)
          .var("amber-wash", material::withAlpha(kAmber, 0.07f))
          .var("isobar", hexColor(0x5E7186, 0.30f))
          .var("isobar-bold", hexColor(0x6C819A, 0.46f))
          .var("amber-ground", hexColor(0x1C1206))
          .var("amber-edge", hexColor(0x4A3411))
          .var("chart", hexColor(0xBFC7D1))
          .fontFamily(grotesque)
          .ink(var("bone")),
      // The masthead and the line naming each panel.
      rule("eyebrow")
          .fontWeight(600)
          .fontSize(11)
          .letterSpacing(3)
          .ink(var("slate-dim")),
      rule("h1").fontWeight(700).fontSize(34).letterSpacing(1),
      rule("caption, footer")
          .fontSize(11.5)
          .letterSpacing(0.5)
          .ink(var("slate-dim")),
      // The running prose: forecast and synopsis, broken as a whole
      // paragraph rather than line by line.
      rule("paragraph")
          .fontSize(19.5)
          .font({.language = "en-GB"})
          .textWrap(TextWrap::Pretty),
      // The wind direction is the one heading inside the sentence.
      rule(".dir").fontWeight(600).letterSpacing(0.6).font({.condense = 0.94f}),
      // A defined term is a citation of the glossary, set as one.
      rule(".term")
          .fontFamily(book)
          .fontStyle(FontStyle::Italic)
          .fontSize(20.5)
          .letterSpacing(0.2)
          .ink(var("amber")),
      rule(".warning").fontWeight(600).fontSize(13.5).letterSpacing(2.8),
      rule(".readout").fontFamily(mono).fontSize(27).letterSpacing(3),
      // A Beaufort numeral names no colour: the cell's ink is the bar's.
      rule(".force").fontWeight(600).fontSize(10.5).letterSpacing(0.4),
      rule(".bands").fontSize(10.5).letterSpacing(0.8),
      rule(".place").fontSize(12.5).letterSpacing(0.8),
      rule(".wind, .station")
          .fontSize(12.5)
          .letterSpacing(1.4)
          .ink(var("slate"))
          .textAlign(weave::TextAlignment::kEnd),
      rule(".wind").fontWeight(600),
      rule(".station").fontFamily(mono).fontSize(12).letterSpacing(0.4),
      rule(".spine")
          .fontWeight(600)
          .fontSize(12.5)
          .letterSpacing(2.6)
          .ink(var("slate-dim"))
          .writingMode(weave::WritingMode::kVerticalRL),
      // The chart: sea areas a shade under the body ink, the one being
      // read in the accent, and the compass points.
      rule(".area")
          .fontWeight(600)
          .fontSize(11.5)
          .letterSpacing(1.1)
          .ink(var("chart")),
      rule(".area.reading").ink(var("amber")),
      rule(".cardinal")
          .fontWeight(600)
          .fontSize(12)
          .letterSpacing(2)
          .ink(var("amber")),
      // The pressure chart under the name: a centre's letter and its
      // millibars, set as a synoptic chart sets them, quieter than the
      // ring.
      rule(".centre")
          .fontWeight(700)
          .fontSize(24)
          .ink(var("isobar-bold")),
      rule(".millibars")
          .fontFamily(mono)
          .fontSize(10.5)
          .letterSpacing(0.6)
          .ink(var("slate"))
          .textAlign(weave::TextAlignment::kCenter),
      rule(".hero")
          .fontWeight(700)
          .fontSize(92)
          .letterSpacing(1.5)
          .textAlign(weave::TextAlignment::kCenter),
  };
}

}  // namespace

struct ShippingForecast {
  /** The bulletin: every word the sheet says, read once in setup. */
  sketch::kit::Document bulletin;
  std::vector<Area> areas;
  /** The sea area being read, as the ring names it, and where it stands. */
  std::string reading;
  float readingBearing = 0;

  // The two stepped clocks. `cycle` wraps once per bulletin, so every
  // beat is a window of it and the sheet re-performs on the wrap;
  // `seconds` never wraps, for the swell that outlasts a bulletin.
  motion::Animatable<float> cycle = motion::animatable(0.0f);
  motion::Animatable<float> seconds = motion::animatable(0.0f);

  /** A beat on the bulletin's timeline: 0 before it starts, 1 after it
   *  ends, which is what makes a list of these one schedule. */
  [[nodiscard]] motion::Animatable<float> beat(float from, float to) {
    return motion::bind(cycle, {.from = {from, to}, .clampFrom = true});
  }

  /** The sheet's own envelope: up at the head of the bulletin, held, and
   *  out before the wrap. */
  [[nodiscard]] motion::Animatable<float> envelope() {
    return motion::bind(cycle, {.from = {0, kLoop}, .envelope = motion::envelope::trapezoid(0.04f / kLoop, 0.42f / kLoop, 12.6f / kLoop, 14.2f / kLoop), .ease = motion::ease::inOutQuad});
  }

  /** WHEN THE READING HAND REACHES @p bearing: at one pace from the first
   *  area to the one being read, and from there, after the rest, at the
   *  same pace to the last. */
  [[nodiscard]] float reachedAt(float bearing) const {
    const float first = areas.front().bearing;
    if (bearing <= readingBearing)
      return kReadingStarts + (bearing - first) / kReadingPace;
    return kReadingResumes + (bearing - readingBearing) / kReadingPace;
  }

  // ---------------------------------------------------------------------
  // The dominant move

  /** ONE LINE OF THE AREA'S NAME: letters rising through a clipped line
   *  box, then breathing on GRAD for as long as they are on screen.
   *
   *  The rise is budgeted as a TOTAL, so a longer name shortens each
   *  letter's wait rather than lengthening the reveal. GRAD thickens a
   *  letter without moving the next, so it is driven at draw time over
   *  glyphs shaped once, and a small per-glyph offset rolls the swell
   *  along the line. The ramp is pinned to the line's metric band, so a
   *  letter rises THROUGH it and cools as it lands. The line starts to
   *  rise at @p from on the bulletin's clock. */
  [[nodiscard]] Element heroLine(const data::Json& line, const char* key,
                                 float from) {
    static const material::Paint ramp =
        material::Paint::linearGradient({0.5f, 0.0f}, {0.5f, 1.0f},
                                              {{0.00f, hexColor(0xFFFBF2)},
                                               {0.52f, hexColor(0xE9E5DB)},
                                               {1.00f, hexColor(0xC9A46A)}});
    return box().overflow(Overflow::Clip).width(pct(100)).children({
        text(line)
            .styleClass("hero")
            .key(key)
            .width(pct(100))
            .ink(ramp)
            .textFx({.effect = textFx::rise(92 * 1.24f),
                     .delay = motion::stagger({0ms, 320ms}, {.from = motion::StaggerFrom::First}), .duration = 560ms,
                     .unit = weave::Unit::Glyph,
                     .progress = beat(from, from + 2.0f)})
            .textFx({.effect = textFx::variableAxisSweep("GRAD", 400, 880),
                     .delay = motion::stagger(34ms), .duration = 620ms,
                     .progress = motion::bind(seconds, {.from = {0, kBreathPeriod}, .envelope = motion::envelope::cosine()})}),
    });
  }

  // ---------------------------------------------------------------------
  // The ring

  /** THE CHART: hairlines, a tick at every area's bearing, the four
   *  cardinals, the sixteen areas on the circle, and the name being read
   *  in the middle.
   *
   *  A bearing becomes a fraction of `shapes::circle()`, which starts due
   *  east and runs clockwise, by the quarter turn between the two
   *  conventions. Glyph-up points radially outward everywhere, as on a
   *  compass ring, so nothing is flipped. Neighbours sit on alternate
   *  radii, as a chart moves a label off its neighbour's ring rather than
   *  off its own bearing, and they beat in reading order. */
  [[nodiscard]] Element ringPanel() {
    const data::Json& ring = bulletin["ring"];
    const float reached = reachedAt(readingBearing);
    return box().width(kRingBox).height(kRingBox).flexShrink(0).children({
        // The ground under the lettering never moves, so it is painted once.
        box()
            .cover()
            .cache(Cache::Texture)
            .key("ring-ground")
            .children({
                box().cover().fill(material::Paint::radialGradient(
                    {0.5f, 0.5f}, 0.94f,
                    {{0.0f, kSeaLift},
                     {0.62f, hexColor(0x090E15)},
                     {1.0f, material::withAlpha(kSea, 0)}},
                    {.extent = material::RadialExtent::ClosestSide})),
                kit::ring(kEye, kRingRadius + 21,
                          stroke(1, Fill::var("keyline"))),
                kit::ring(kEye, kInnerRadius, stroke(1, Fill::var("keyline"))),
                kit::ring(kEye, kPorthole,
                          stroke(1, Fill::var("keyline-deep"))),
            }),
        pressureChart().opacity(beat(0.40f, 1.60f)),
        readingHand(),
        // The area being read keeps its slice of the ring lit once the hand
        // has reached it, from the hairline out to the ticks.
        kit::disc(kEye, kRingRadius + 21)
            .key("reading-slice")
            .shape(sigil::geometry::shapes::sector(
                readingBearing - 90 - 7, 14, kInnerRadius / (kRingRadius + 21)))
            .fill(Fill::var("amber-wash"))
            .opacity(beat(reached, reached + 0.6f)),
        each(areas,
             [&](const Area& area, size_t index) {
               const bool read = area.name == reading;
               const float at = reachedAt(area.bearing);
               return box()
                   .key("tick" + std::to_string(index))
                   .width(read ? 1.5f : 1)
                   .height(read ? 18 : 9)
                   .rotate(area.bearing)
                   .centerAt(compass(area.bearing, kRingRadius + (read ? 32 : 28)))
                   .fill(Fill::var(read ? "amber" : "slate-dim"))
                   .opacity(beat(at - 0.08f, at + 0.30f));
             }),
        each(bulletin["cardinals"].items(),
             [&](const data::Json& letter, size_t quarter) {
               return text(letter)
                   .styleClass("cardinal")
                   .key("cardinal" + std::to_string(quarter))
                   .centerAt(compass(90.0f * quarter, kRingRadius + 46))
                   .opacity(beat(0.10f, 1.20f));
             }),
        each(areas,
             [&](const Area& area, size_t index) {
               const float radius = index % 2 == 0 ? kRingRadius : kRingRadius - 31;
               const float at = reachedAt(area.bearing) - 0.06f;
               return text(area.name)
                   .styleClass(area.name == reading ? "area reading" : "area")
                   .key("area" + std::to_string(index))
                   .inset(kRingBox * 0.5f - radius)
                   .textOnPath({.path = sigil::geometry::shapes::circle(),
                                .at = std::fmod(area.bearing / 360 + 0.75f, 1.0f),
                                .align = TextPath::Align::Center,
                                .offset = 7,
                                .autoFlip = false})
                   .textFx({.effect = textFx::rise(13),
                            .delay = motion::stagger(20ms), .duration = 420ms,
                            .progress = beat(at, at + 0.62f)});
             }),
        // A lamp comes up behind the name as it arrives, spreading from
        // the middle, and breathes with the grade the name breathes on.
        kit::disc(kEye, kPorthole)
            .key("lamp")
            .fill(material::Paint::radialGradient(
                {kPorthole, kPorthole}, kPorthole,
                {material::withAlpha(kAmber, 0.15f),
                 material::withAlpha(kAmber, 0.05f),
                 material::withAlpha(kAmber, 0)},
                {.units = material::GradientUnits::Pixels}))
            .scale(motion::bind(cycle, {.from = {reached, reached + 1.4f}, .clampFrom = true, .ease = motion::ease::outCubic}))
            .opacity(motion::bind(seconds, {.from = {0, kBreathPeriod}, .envelope = motion::envelope::cosine(), .to = {0.55f, 1.0f}})),
        box()
            .column()
            .width(2 * kInnerRadius - 40)
            .centerAt({kEye.x(), kEye.y() - 6})
            .key("hero")
            .children({heroLine(ring["hero"][0], "hero-1", reached),
                       heroLine(ring["hero"][1], "hero-2", reached + 0.22f)}),
        document::eyebrow(ring["cap"])
            .key("ring-cap")
            .opacity(beat(reached + 1.75f, reached + 2.40f))
            .centerAt({kEye.x(), kEye.y() + 118}),
    });
  }

  /** THE READING HAND: a hairline from the middle to the ticks with a
   *  wake of light behind it, turning clockwise in reading order. It is
   *  two turns nested, one to the area being read and one on from it to
   *  the last, so the rest between them is where neither is moving. A
   *  sweep's ramp begins due east, which is where the hand stands before
   *  it is turned. */
  [[nodiscard]] Element readingHand() {
    const float first = areas.front().bearing;
    const float last = areas.back().bearing;
    const float finished = reachedAt(last);
    const float radius = kRingRadius + 21;
    return kit::disc(kEye, radius)
        .key("reading-hand")
        .rotate(motion::bind(cycle, {.from = {kReadingStarts, reachedAt(readingBearing)}, .clampFrom = true, .to = {first - 90, readingBearing - 90}}))
        .opacity(motion::bind(cycle, {.from = {0, kLoop}, .envelope = motion::envelope::trapezoid((kReadingStarts - 0.20f) / kLoop, (kReadingStarts + 0.20f) / kLoop, finished / kLoop, (finished + 0.80f) / kLoop)}))
        .children({
            // The wake is the last seventh of a sweep ramp, so the node is
            // shaped to that slice and paints nothing where the ramp is clear.
            box()
                .cover()
                .shape(sigil::geometry::shapes::sector(-51.5f, 51.5f))
                .rotate(motion::bind(cycle, {.from = {kReadingResumes, finished}, .clampFrom = true, .to = {0.0f, last - readingBearing}}))
                .fill(material::Paint::conicGradient(
                    {radius, radius},
                    {{0.0f, material::withAlpha(kAmber, 0)},
                     {0.86f, material::withAlpha(kAmber, 0)},
                     {1.0f, material::withAlpha(kAmber, 0.16f)}},
                    {.units = material::GradientUnits::Pixels}))
                .children({
                    box()
                        .left(radius)
                        .top(radius - 0.5f)
                        .width(radius)
                        .height(1)
                        .fill(material::Paint::linearGradient(
                            {0, 0.5f}, {1, 0.5f},
                            {{0.0f, material::withAlpha(kAmber, 0)},
                             {1.0f, material::withAlpha(kAmber, 0.7f)}})),
                }),
        });
  }

  /** THE PRESSURE CHART the synopsis describes, seen through the porthole
   *  under the name: the Rockall low as nested isobars, each opening a
   *  little further toward the islands as the gradient slackens away from
   *  the centre, and the Atlantic high to the south-west.
   *
   *  Across the bulletin the low drifts toward Fair Isle and deepens —
   *  its isobars draw in about the centre and its figure turns from what
   *  it is now to what it is expected to be — while the high fades. The
   *  isobars are described once and ride the bound drift and scale; a
   *  texture under a scale that changes every frame would be re-rastered
   *  every frame, so they stay live paint. */
  [[nodiscard]] Element pressureChart() {
    const data::Json& chart = bulletin["ring"]["chart"];
    const data::Json& low = chart["low"];
    const data::Json& high = chart["high"];
    const float distance = (float)low["distance"].number();
    const SkPoint lowAt = compass((float)low["bearing"].number(), distance);
    const SkPoint lowBound = compass((float)low["toward"].number(), distance * 0.86f);
    const SkPoint highAt =
        compass((float)high["bearing"].number(), (float)high["distance"].number());
    const auto drift = [&](float span) {
      return motion::bind(cycle, {.from = {kLowFrom, kLowTo}, .clampFrom = true, .ease = motion::ease::inOutSine, .to = {0.0f, span}});
    };
    const auto isobar = [](SkPoint centre, float across, float tilt, bool bold) {
      return box()
          .width(across * 2)
          .height(across * 1.56f)
          .centerAt(centre)
          .rotate(tilt)
          .shape(sigil::geometry::shapes::circle())
          .fill(Fill::none())
          .stroke(stroke(1, Fill::var(bold ? "isobar-bold" : "isobar")));
    };
    const auto figure = [](SkPoint at, const data::Json& words) {
      return text(words).styleClass("millibars").width(60).centerAt(at);
    };
    std::vector<Element> lowRings, highRings;
    for (int step = 0; step < 8; ++step) {
      const float across = 26 + step * 34 + step * step * 2.5f;
      const SkPoint centre{lowAt.x() + step * 7.0f, lowAt.y() + step * 5.5f};
      lowRings.push_back(isobar(centre, across, -26 + step * 2.0f, step % 4 == 3));
    }
    for (int step = 0; step < 3; ++step)
      highRings.push_back(isobar(highAt, 64 + step * 52, 18, false));
    const SkPoint below{0, 24};
    return box()
        .cover()
        .shape(sigil::geometry::shapes::Circle{.inset = kEye.x() - kPorthole})
        .overflow(Overflow::Clip)
        .key("pressure-chart")
        .children({
            box().cover().key("high").opacity(drift(1).invert().target(0.45f, 1)).children({
                box().cover().key("high-isobars").children(std::move(highRings)),
                text(high["mark"]).styleClass("centre").centerAt(highAt),
                figure(highAt + below, high["reading"]),
            }),
            box()
                .cover()
                .key("low")
                .translateX(drift(lowBound.x() - lowAt.x()))
                .translateY(drift(lowBound.y() - lowAt.y()))
                .children({
                    box()
                        .cover()
                        .key("low-isobars")
                        .transformOrigin(Dimension(lowAt.x()), Dimension(lowAt.y()))
                        .scale(drift(kLowDeepens - 1).offset(1))
                        .children(std::move(lowRings)),
                    text(low["mark"]).styleClass("centre").centerAt(lowAt),
                    figure(lowAt + below, low["now"])
                        .opacity(motion::bind(cycle, {.from = {6.0f, 7.0f}, .clampFrom = true, .to = {1.0f, 0.0f}})),
                    figure(lowAt + below, low["later"])
                        .opacity(motion::bind(cycle, {.from = {6.0f, 7.0f}, .clampFrom = true})),
                }),
        });
  }

  // ---------------------------------------------------------------------
  // The left column

  /** THE GALE WARNING: the strip slides in and crossfades into an elastic
   *  settle over the last fifth of the slide, so it arrives and
   *  compresses in one gesture. The dot and the words are one statement,
   *  so the strip's ink is the accent and the dot names no colour. The
   *  dot is the warning's lamp: it blinks for as long as the warning
   *  stands, with a halo that goes out with it. */
  [[nodiscard]] Element galeStrip() {
    return box()
        .row()
        .alignItems(Align::Center)
        .gap(12)
        .padding(10, 13)
        .borderRadius({3})
        .fill(Fill::var("amber-ground"))
        .stroke(stroke(1, Fill::var("amber-edge")))
        .ink(var("amber"))
        .opacity(beat(0.10f, 0.70f))
        .children({
            box()
                .width(7)
                .height(7)
                .borderRadius({4})
                .fill(Fill::currentInk())
                .opacity(motion::bind(seconds, {.from = {0, kLampPeriod}, .envelope = motion::envelope::square(kLampLit), .to = {0.28f, 1.0f}}))
                .children({
                    kit::disc({3.5f, 3.5f}, 13)
                        .fill(material::Paint::radialGradient(
                            {0.5f, 0.5f}, 1.0f,
                            {{0.0f, material::withAlpha(kAmber, 0.45f)},
                             {1.0f, material::withAlpha(kAmber, 0)}},
                            {.extent = material::RadialExtent::ClosestSide})),
                }),
            text(bulletin["gale"])
                .styleClass("warning")
                .key("gale")
                .textFx({.effect = textFx::sequence(
                             textFx::slide(-46).until(0.46f).crossfade(0.20f),
                             textFx::pop(0.86f, 2.6f)),
                         .delay = motion::stagger({0ms, 520ms}), .duration = 620ms,
                         .progress = beat(0.25f, 1.85f)}),
        });
  }

  /** A PANEL of the column: the line naming it, then what it names. */
  [[nodiscard]] Element panel(const data::Json& eyebrow, float from,
                              std::initializer_list<Children> body) {
    return box().column().gap(9).children({
        document::eyebrow(eyebrow).opacity(beat(from, from + 0.55f)),
    }).children(body);
  }

  /** THE AREA FORECAST: one paragraph, three faces, four tracks.
   *
   *  The first letter of every word lifts further than the rest of its
   *  word, and a grade passes over the initials the grotesque can carry
   *  it on and settles part of the way back, as a reader's stress passes
   *  along a line and leaves the initials marked — the glossary terms,
   *  set in the serif, are left out by the name they were written in.
   *  The Beaufort numerals are the quantities the bulletin exists for, so
   *  their grade rises with the same stress and stays at its height; they
   *  are found by pattern, repainted and re-graded at draw time, never
   *  re-shaped. Every cascade is numbered over the PARAGRAPH
   *  (`beats::Text`), so every glyph of word ten is on beat ten whichever
   *  track holds it. */
  [[nodiscard]] Element forecast() {
    const data::Json& page = bulletin["forecast"];
    const weave::Selector initials =
        weave::selectors::each(weave::Unit::Word).take(1);
    const weave::Selector figures = weave::selectors::regex(u8"[0-9]+");
    const auto words = [&](const weave::Selector& where, TextEffect effect,
                           float durationMs, float from, float to) {
      return Track{.where = where,
                   .effect = std::move(effect),
                   .delay = motion::stagger(46ms), .duration = std::chrono::duration<double, std::milli>(durationMs),
                   .unit = weave::Unit::Word,
                   .beatsOver = beats::Text,
                   .progress = beat(from, to)};
    };
    return panel(page["eyebrow"], 1.50f, {
        document::paragraph(bulletin.passage(page["runs"]))
            .key("forecast")
            .width(pct(100))
            .span(figures, SpanStyle().ink(var("amber")))
            .textFx(words(initials, textFx::rise(16), 460, 1.75f, 4.10f))
            .textFx(words(initials & !selectors::style("term") & !figures,
                          textFx::sequence(
                              textFx::variableAxisSweep("GRAD", 400, 900)
                                  .until(0.45f),
                              textFx::variableAxisSweep("GRAD", 900, 640)),
                          620, 1.75f, 4.10f))
            .textFx(words(figures, textFx::variableAxisSweep("GRAD", 400, 900),
                          460, 1.75f, 4.10f))
            .textFx(words(weave::selectors::each(weave::Unit::Word).drop(1),
                          textFx::rise(9), 500, 1.83f, 4.30f)),
    });
  }

  /** THE BAROMETER decodes into place. A substitution draws its letter at
   *  the original's pen position, so it is honoured only where the two
   *  advances agree — which is why the readout is monospaced and its
   *  charset one width. The decode is HELD, so a character waiting its
   *  turn is absent rather than a wrong letter. */
  [[nodiscard]] Element barometer() {
    const data::Json& page = bulletin["barometer"];
    return panel(page["eyebrow"], 2.10f, {
        text(page["reading"])
            .styleClass("readout")
            .key("barometer")
            .textFx({.effect = textFx::hold(textFx::scramble(
                         U"0123456789ABCDEFGHJKLMNPRSTUVWXYZ", 16)),
                     .delay = motion::stagger(26ms, {.from = motion::StaggerFrom::First}), .duration = 520ms,
                     .progress = beat(2.25f, 4.10f)}),
        document::caption(page["note"]).opacity(beat(3.30f, 3.90f)),
    });
  }

  /** THE GENERAL SYNOPSIS beats over LINES of the current layout, the
   *  nearest a laid-out paragraph comes to its clauses. Its millibar
   *  readings are picked out twice, and neither moves a letter: first in
   *  GRAD, while they still wear the passage's paint and differ from it
   *  in that axis alone, then in the accent — declared the other way
   *  round, the grade's range would differ in paint too, re-shape, and
   *  put the base paint back over the accent. */
  [[nodiscard]] Element synopsis() {
    const data::Json& page = bulletin["synopsis"];
    const weave::Selector figures = weave::selectors::regex(u8"[0-9]+");
    return panel(page["eyebrow"], 2.60f, {
        document::paragraph(bulletin.passage(page["runs"]))
            .key("synopsis")
            .width(pct(100))
            .span(figures, SpanStyle().font(
                               {.variations = {weave::FontVariation("GRAD", 800)}}))
            .span(figures, SpanStyle().ink(var("amber")))
            .textFx({.effect = textFx::slide(-22),
                     .delay = motion::stagger(150ms), .duration = 620ms,
                     .unit = weave::Unit::Line,
                     .progress = beat(2.70f, 4.60f)}),
    });
  }

  /** THE BEAUFORT SCALE, which is why the paragraph has numerals at all:
   *  the forces this bulletin quotes carry the accent, bar and numeral
   *  alike, and a force it does not quote has a darker bar than numeral.
   *  A force it quotes only as OCCASIONAL is drawn in outline, since the
   *  bulletin says it may not blow at all. */
  [[nodiscard]] Element beaufort() {
    const data::Json& page = bulletin["beaufort"];
    return panel(page["eyebrow"], 3.20f, {
        box().row().gap(6).height(56).alignItems(Align::End).children(
            each(std::views::iota(0, 13),
                 [](int force) {
                   const bool quoted = force >= 5 && force <= 8;
                   const bool occasional = force == 8;
                   Element bar = box()
                                     .width(pct(100))
                                     .height(6 + force * 2.6f)
                                     .fill(Fill::var(occasional ? "amber-ground"
                                                     : quoted   ? "amber"
                                                                : "bar"));
                   if (occasional) bar.stroke(stroke(1, Fill::var("amber")));
                   return box()
                       .flexGrow(1)
                       .column()
                       .gap(6)
                       .alignItems(Align::Center)
                       .key("force" + std::to_string(force))
                       .ink(var(quoted ? "amber" : "slate-dim"))
                       .children({
                           bar,
                           text(std::to_string(force)).styleClass("force"),
                       });
                 })),
        document::caption(page["names"]).styleClass("bands"),
    }).opacity(beat(3.20f, 3.80f));
  }

  /** COASTAL STATIONS: the still part of the sheet — the table and its
   *  rules arrive together, its rows one after another, and then stay. */
  [[nodiscard]] Element stations() {
    const data::Json& page = bulletin["stations"];
    static const char* const kColumnClasses[] = {"place", "wind", "station"};
    std::vector<std::vector<Utf8>> cells;
    for (const data::Json& row : page["rows"].items())
      cells.push_back({row["place"], row["wind"], row["baro"]});
    const std::vector<std::span<const Utf8>> rows(cells.begin(), cells.end());
    return kit::table(
        rows,
        {.columns = {{page["eyebrow"], 316}, {{}, 64}, {{}, 156}},
         .rowGap = 7,
         .divider = Fill::var("keyline"),
         .headRuled = true,
         .headLine = [](const Utf8& words) { return document::eyebrow(words); },
         .cellLine = [&](const Utf8& words, const kit::Table&, size_t column,
                         size_t row) -> Element {
           return text(words)
               .styleClass(kColumnClasses[column])
               .opacity(beat(2.80f + row * 0.14f, 3.40f + row * 0.14f));
         }})
        .opacity(beat(2.66f, 3.16f));
  }

  // ---------------------------------------------------------------------

  /** THE SPINE runs down the gutter as a vertical-rl column, where Latin
   *  lies on its side; a track deviates in the frame the layout placed
   *  the glyph in, so its lift runs across the column. */
  [[nodiscard]] Element spine() {
    return text(bulletin["spine"])
        .styleClass("spine")
        .key("spine")
        .left(40)
        .top(196)
        .width(28)
        .height(560)
        .textFx({.effect = textFx::rise(11),
                 .delay = motion::stagger({0ms, 780ms}, {.from = motion::StaggerFrom::First}), .duration = 420ms,
                 .progress = beat(0.45f, 2.70f)});
  }

  [[nodiscard]] Element masthead() {
    const data::Json& page = bulletin["header"];
    std::vector<sketch::kit::Line> slugs;
    for (const data::Json& slug : page["slugs"].items()) {
      const float from = 0.55f + slugs.size() * 0.16f;
      slugs.push_back({.words = slug, .opacity = beat(from, from + 0.60f)});
    }
    return sketch::kit::titleCard(
        {.eyebrow = {.words = page["eyebrow"], .opacity = beat(0.05f, 0.55f)},
         .title = {.words = page["title"],
                   .textFx = Track{.effect = textFx::rise(16),
                                   .delay = motion::stagger({0ms, 420ms}), .duration = 520ms,
                                   .progress = beat(0.15f, 1.30f)}},
         .notes = std::move(slugs),
         .align = Align::Stretch,
         .key = "head"});
  }

  [[nodiscard]] Element describe() {
    return stack()
        .applyStyleSheet(sheet())
        .fill(material::Paint::linearGradient(
            {0, 0}, {0, kCanvas.height()},
            {{0.0f, kSea}, {0.55f, kSeaLift}, {1.0f, hexColor(0x05080C)}},
            {.units = material::GradientUnits::Pixels}))
        .children({
            // The sea falls to near black at the corners. It carries no
            // grain: `kit::grained` lays its noise on as soft light, which
            // moves a ground this dark by less than one level.
            box().cover().fill(
                kit::vignette(kCanvas, hexColor(0x020304, 0.9f), 0.3f)),
            spine().opacity(envelope()),
            box()
                .column()
                .inset(40, 44, 38, 112)
                .gap(26)
                .opacity(envelope())
                .children({
                    masthead(),
                    kit::line({.fill = Fill::var("keyline")}),
                    box().row().gap(48).flexGrow(1).children({
                        box()
                            .width(556)
                            .flexShrink(0)
                            .column()
                            .justifyContent(Justify::SpaceBetween)
                            .children({galeStrip(), forecast(), barometer(),
                                       synopsis(), beaufort(), stations()}),
                        kit::centred().flexGrow(1).children({ringPanel()}),
                    }),
                    document::footer(bulletin["foot"])
                        .key("foot")
                        .opacity(beat(3.10f, 3.75f)),
                }),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    // The still is the swell's second peak: the barometer has decoded and
    // the forecast's initials have landed, and the grade is at its height.
    sketch::kit::stage(ctx, {.size = kCanvas,
                             .captureAt = 10.8,
                             .background = kSea});

    bulletin = sketch::kit::Document(ctx, "data/content.json");
    for (const data::Json& area : bulletin["areas"].items())
      areas.push_back({std::string(area["name"].text()),
                       (float)area["bearing"].number()});
    const data::Json& hero = bulletin["ring"]["hero"];
    reading = std::string(hero[0].text()) + " " + std::string(hero[1].text());
    for (const Area& area : areas)
      if (area.name == reading) readingBearing = area.bearing;

    ctx.engine.add([this, &ticker = ctx.engine] {
      const double elapsed = ticker.elapsed();
      cycle = motion::phase(elapsed, kLoop) * kLoop;
      seconds = (float)elapsed;
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(ShippingForecast, "Study · Type",
             "BBC Radio 4's 0048 bulletin as a sheet that performs itself "
             "— a fixed vocabulary, a ring of sea areas, one dominant "
             "move")
