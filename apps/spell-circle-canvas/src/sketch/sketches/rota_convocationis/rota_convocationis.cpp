// ROTA CONVOCATIONIS — an invented conjuring wheel in the anime idiom:
// concentric bands of lettering, each set between a pair of ruled circles,
// round two star compounds and a lettered emblem, with twelve small seals
// riding the rim. Everything is invented in the open: the Latin is this
// study's own, the names are made to be pronounced, and the two rune bands
// borrow Tifinagh and Canadian syllabics for their shapes alone.
//
// Read it as a compass drawing. Every band is a PAIR OF RULES with
// something written or drawn between them, and no rule stands alone. Two
// radii are not free: the {12/3} compound's chords run tangent to the
// envelope circle, and the {12/4} compound's chords to the hub's rule, so
// the figure inside decides where the circle under it goes. Neighbouring
// layers turn opposite ways at different rates, which is what makes the
// wheel read as a train of gears rather than as a picture being spun.
//
// The words are in data/content.json; the rune bands are frozen there as
// dealt, so the plate is the same plate on every machine.

// TAGS: Typography/Lettering, Patterns/Ornament

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <numbers>
#include <string>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
using material::skia::Effect;
using material::skia::Paint;

using namespace sigil::compose;

namespace {

// A magic circle wants a square field: the figure is the picture.
constexpr float kSide = 1280;
constexpr SkPoint kEye{640, 640};
constexpr float kRadius = 545;  // the greatest circle, px
const sigil::geometry::path::PolarFrame kFrame{.centre = kEye,
                                               .radius = kRadius};

// Chalk by candlelight, then one hue of light: the lines are warm bone, the
// names gold, the rune band a cold lilac so it reads as another script.
const material::Color kNight = hexColor(0x0A0812);
const material::Color kNightLift = hexColor(0x171226);
const material::Color kLine = hexColor(0xC9B48C);
const material::Color kHair = hexColor(0x6A5E7C);
const material::Color kGold = hexColor(0xE2B458);
const material::Color kHalo = hexColor(0xFFB13A);
const material::Color kBone = hexColor(0xEDE3CC);
const material::Color kAsh = hexColor(0x8E86A0);
const material::Color kRuneInk = hexColor(0xA697C4);
const material::Color kSealGround = hexColor(0x0D0A16);

// THE RADIUS TABLE, in units of the greatest circle, outside in.
constexpr float rEdge = 1.000f, rEdgeIn = 0.980f;      // 240 teeth
constexpr float rRune = 0.934f;                        // the register
constexpr float rVox = 0.872f;                         // the invocation
constexpr float rTickOut = 0.836f, rTickMid = 0.818f;  // the ladder band
constexpr float rNames = 0.752f;                       // the nine names
constexpr float rArcOut = 0.676f, rArcIn = 0.640f;     // the broken arcs
constexpr float rTexture = 0.598f;                     // the small register
constexpr float rStar = 0.520f;                        // {12/3} vertices
constexpr float rCrescentOut = 0.492f, rCrescentIn = 0.454f;
constexpr float rCageOut = 0.446f, rCageIn = 0.378f;
constexpr float rEnvelope = 0.368f;  // where the {12/3} chords run tangent
constexpr float rInner = 0.348f;     // {12/4} vertices
constexpr float rHub = 0.174f;       // where the {12/4} chords run tangent
constexpr float rEmblem = 0.140f;
constexpr float rHexagram = 0.118f;
constexpr float rKern = 0.104f;
constexpr float rMote = 0.058f;

// A seal's inner edge lands on the register's inner rule and its outer
// edge stands proud of the greatest circle, so one seal is exactly as tall
// as the band group it stands on and breaks the rim on its way out.
constexpr float rSealLip = 1.032f, rSealFoot = 0.910f;
constexpr float rSealRide = (rSealLip + rSealFoot) * 0.5f;
constexpr float kSealRadius = (rSealLip - rSealFoot) * 0.5f * kRadius;
constexpr float kSealBaseline = kSealRadius * 0.78f;
constexpr int kStations = 12;
constexpr float kPitch = 360.0f / kStations;

// THE TURNING LAYERS and their periods in seconds per revolution; a
// negative period turns anticlockwise. No two share a rate, neighbours run
// opposite ways, the geometry takes the fast periods and the lettering the
// slow ones.
enum Turning { kFine, kRegister, kInvocation, kNames, kTexture, kArcs,
               kStar, kInnerStar, kHexagram, kTurnings };
constexpr std::array<float, kTurnings> kPeriod{19, -96, 132, -84, 52,
                                               -44, 31, -17, 62};

/** Every rule of the plate: a circle, its pen width and its ink. A circle
 *  turned is the same circle, so no rule sits in a turning layer. */
struct Rule {
  float radius, width;
  material::Color ink;
};
const Rule kRules[] = {
    {rEdge, 2.1f, kLine},     {rEdgeIn, 0.6f, kHair},  {0.958f, 1.1f, kLine},
    {0.910f, 1.1f, kLine},    {0.898f, 0.5f, kHair},   {0.846f, 1.0f, kLine},
    {rTickMid, 0.5f, kHair},  {0.790f, 1.2f, kLine},   {0.708f, 1.2f, kLine},
    {0.694f, 0.5f, kHair},    {0.622f, 0.9f, kLine},   {0.572f, 0.9f, kLine},
    {0.560f, 0.5f, kHair},    {0.534f, 0.5f, kHair},   {rCageOut, 0.9f, kHair},
    {rCageIn, 0.9f, kHair},   {rEnvelope, 1.0f, kLine}, {0.356f, 0.5f, kHair},
    {rHub, 1.6f, kLine},      {0.160f, 0.6f, kHair},   {0.150f, 0.9f, kLine},
    {rKern, 0.7f, kLine}};

/** A RADIAL LADDER between two radii: @p count marks from @p from degrees
 *  clockwise of twelve o'clock, every @p skip-th left out where a heavier
 *  class already stands. Spokes are ladders too. */
struct Ladder {
  int count, skip;
  float outer, inner, width;
  material::Color ink;
  float from = 0;
};
// The division ladders that stand still. Every count is a multiple of the
// twelve the plate is built on, and the mid class sits half a step off the
// long one so the two interleave instead of doubling up.
const Ladder kLadders[] = {
    {240, 12, rEdge, rEdgeIn, 0.7f, kLine},
    {12, 0, rTickOut, rTickMid - 0.028f, 1.3f, kAsh},
    {24, 2, rTickOut, rTickMid - 0.010f, 0.9f, kLine, kPitch * 0.5f},
    {144, 6, rTickOut, rTickMid, 0.7f, kLine},
    {48, 4, 0.560f, 0.534f, 0.7f, kLine, kPitch * 0.25f},
    {72, 6, rHub, 0.160f, 0.6f, kHair}};

/** THE FOUR BANDS OF SCRIPT, outside in. Each is fitted to its own
 *  circumference, so the word count in the data is what sets the size:
 *  the names are the main register, the invocation and the rune band
 *  flank it at about half, and the texture band runs small enough to stop
 *  being letters and become the grain the disc is made of. */
struct Band {
  const char* words;  // the key in data/content.json
  float radius, track, fill;
  const char* voice;  // the face class; none lets fallback find runes
  material::Color ink;
  Turning turning;
};
const Band kBands[] = {
    {"register", rRune, 2.0f, 0.985f, "", kRuneInk, kRegister},
    {"invocation", rVox, 2.2f, 0.985f, "band", kBone, kInvocation},
    {"names", rNames, 4.2f, 0.985f, "band", kGold, kNames},
    {"texture", rTexture, 0.0f, 0.995f, "", kAsh, kTexture}};

SkPoint polar(float degrees, float radius) {
  return kFrame.at(degrees, radius);
}

/** A node the size of the sheet, where a figure centred on the eye turns
 *  about the eye. */
Element sheet() { return box().inset(0).hitTestable(false); }

Element ladder(const Ladder& ladder) {
  return kit::disc(kEye, ladder.outer * kRadius)
      .shape(shapes::ticks({.divisions = ladder.count,
                            .from = ladder.from,
                            .mark = {ladder.inner / ladder.outer, 1.0f},
                            .longEvery = ladder.skip,
                            .longMark = {1.0f, 1.0f}}))
      .fill(Fill::none())
      .stroke(stroke(ladder.width, Fill::color(ladder.ink)));
}

/** @p count arcs on one circle, each @p span degrees wide and centred on
 *  a station offset by @p from: an arc that stops short of its neighbour
 *  is what makes a ring read as a mechanism rather than another circle. */
Element brokenRing(float radius, int count, float span, float from,
                   float width, material::Color ink) {
  const float px = radius * kRadius;
  return kit::disc(kEye, px + width * 0.5f)
      .shape(shapes::arcs({.divisions = count,
                           .from = from,
                           .mark = {(px - width * 0.5f) / (px + width * 0.5f),
                                    1.0f},
                           .spanDeg = span}))
      .fill(Fill::color(ink));
}

/** @p count small circles standing on a ring: the furniture at every
 *  star vertex and every arc's station. */
Element beads(int count, float radius, float size, float from, float width,
              material::Color ink) {
  return sheet().children(each(count, [=](int index) {
    return kit::ring(polar(from + 360.0f * index / count, radius), size,
                     stroke(width, Fill::color(ink)));
  }));
}

/** A star compound {12/step} whose vertices stand on @p radius. */
Element compound(int step, float radius, float width) {
  return kit::disc(kEye, radius * kRadius)
      .shape(shapes::chords({.sides = kStations, .step = step, .closed = true}))
      .fill(Fill::none())
      .stroke(stroke(width, Fill::color(kLine)));
}

struct RotaConvocationis {
  sketch::kit::Document content;
  StyleSheet registers;
  std::array<ch::Output<float>, kTurnings> phase{};
  std::array<float, std::size(kBands)> bandSize{};
  std::array<float, kStations> sealSize{};

  motion::Animatable<float> turn(Turning layer) {
    return motion::bind(&phase[layer]).target(0.0f, 360.0f);
  }

  /** The size at which @p run girds a circle of @p radius px, measured
   *  twice because tracking is px and does not scale with the type. */
  float fit(sketch::SketchContext& context, const Text& run, float radius,
            float fill) const {
    float size = 20;
    for (int pass = 0; pass < 2; ++pass) {
      Element probe = run;
      probe.font({.size = size}).applyStyleSheet(registers);
      const SkSize measured = context.measure(probe);
      if (measured.width() > 1)
        size *= 2 * std::numbers::pi_v<float> * radius * fill /
                measured.width();
    }
    return size;
  }

  Text bandRun(const Band& band) const {
    return text(content[band.words].text())
        .styleClass(band.voice)
        .font({.track = band.track});
  }

  /** One band of script: the text node IS the circle its baseline runs
   *  on, and the letters' bodies straddle that circle so they sit between
   *  the band's two rules. */
  Element script(const Band& band, float size) {
    const float px = band.radius * kRadius;
    Text run = bandRun(band);
    run.font({.size = size}).ink(band.ink).rotate(turn(band.turning));
    if (band.turning == kNames) run.filter(styles::textGlow(kHalo, 6.0f));
    return kit::at(std::move(run), kEye.fX - px, kEye.fY - px, 2 * px, 2 * px)
        .textOnPath({.path = shapes::circle(), .offset = -size * 0.34f});
  }

  /** The arc layer: two broken rings half a pitch apart, the spokes that
   *  tie them, the nodes between them and a dashed ring outside the pair,
   *  all turning as one. */
  Element arcs() {
    return sheet().rotate(turn(kArcs)).children(
        {brokenRing(rArcOut, kStations, kPitch * 0.80f, 0, 1.7f, kLine),
         brokenRing(rArcIn, kStations, kPitch * 0.62f, kPitch * 0.5f, 1.7f,
                    kLine),
         brokenRing(0.684f, 60, 6.0f * 0.44f, 0, 1.0f, kLine),
         ladder({kStations, 0, rArcOut, rArcIn, 0.9f, kLine, kPitch * 0.5f}),
         beads(kStations, (rArcIn + rArcOut) * 0.5f, 5, 0, 1.0f, kLine)});
  }

  /** The outer figure: the {12/3} compound — three squares — with a
   *  circle at every vertex, spokes down to the envelope its chords are
   *  tangent to, the cage floor under it, and three crescents laid across
   *  it: the one three-fold mark on a plate that counts twelve. */
  Element star() {
    const float crescentSpan = 66;
    return sheet().rotate(turn(kStar)).children(
        {compound(3, rStar, 1.5f),
         ladder({kStations, 0, rStar, rEnvelope, 0.8f, kHair}),
         beads(kStations, rStar, 9, 0, 1.0f, kLine),
         ladder({72, 0, rCageOut - 0.004f, rCageIn + 0.004f, 0.6f, kHair,
                 kPitch / 12}),
         beads(24, (rCageIn + rCageOut) * 0.5f, 2.6f, kPitch * 0.25f, 0.9f,
               kLine),
         kit::disc(kEye, rCrescentOut * kRadius)
             .shape(shapes::arcs({.divisions = 3,
                                  .from = kPitch * 2.5f,
                                  .mark = {rCrescentIn / rCrescentOut, 1},
                                  .spanDeg = crescentSpan}))
             .fill(Fill::none())
             .stroke(stroke(1.3f, Fill::color(kLine))),
         // Each crescent's rungs are stubs off its inner arc, so the mark
         // reads as a bracket and not as a grid.
         each(3, [&](int index) {
           return kit::disc(kEye, rCrescentOut * kRadius)
               .shape(shapes::ticks(
                   {.divisions = 6,
                    .from = kPitch * 2.5f + 120.0f * index - crescentSpan / 2,
                    .sweep = crescentSpan,
                    .mark = {rCrescentIn / rCrescentOut,
                             (rCrescentIn + 0.45f * (rCrescentOut -
                                                     rCrescentIn)) /
                                 rCrescentOut},
                    .longEvery = 6,
                    .longMark = {1, 1}}))
               .fill(Fill::none())
               .stroke(stroke(1.0f, Fill::color(kLine)));
         })});
  }

  /** The inner figure: the {12/4} compound — four triangles — whose
   *  chords run tangent to the hub's rule, turning against the squares. */
  Element innerStar() {
    return sheet().rotate(turn(kInnerStar)).children(
        {compound(4, rInner, 1.2f),
         ladder({kStations, 0, rInner, rHub, 0.7f, kHair, kPitch * 0.5f})});
  }

  /** The thresholds, named on six of the compound's twelve chords. They
   *  ride the same chords as open contours, so chord k's midpoint is at
   *  (k + 0.5) / 12 of one arc-length coordinate. They do not turn: a
   *  caption is read, and the mechanism is what moves. */
  Element thresholds() {
    const float px = rStar * kRadius;
    return sheet().children(each(content["thresholds"].items(),
                [&](const sigil::data::Json& name, size_t index) {
                  return kit::at(text(name.text())
                                     .styleClass("label")
                                     .font({.size = 11, .track = 2})
                                     .ink(kAsh),
                                 kEye.fX - px, kEye.fY - px, 2 * px, 2 * px)
                      .textOnPath({.path = shapes::chords({.sides = kStations,
                                                           .step = 3,
                                                           .inset = 74}),
                                   .at = (index * 2 + 0.5f) / kStations,
                                   .align = TextPath::Align::Center,
                                   .offset = 5,
                                   .autoFlip = true});
                }));
  }

  /** The emblem: a disc of light, the hexagram and its beads turning slowly
   *  inside it, and three letterforms set large at the centre, standing
   *  still — the brightest stable thing on the plate. */
  Element emblem() {
    const float px = rHexagram * kRadius;
    return sheet().children(
        {kit::disc(kEye, rEmblem * kRadius * 1.6f)
             .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                   {{0.0f, hexColor(0x3A240C, 0.9f)},
                                    {0.55f, hexColor(0x241608, 0.6f)},
                                    {1.0f, hexColor(0x100A04, 0.0f)}})),
         sheet().rotate(turn(kHexagram)).children(
             {kit::disc(kEye, px)
                  .shape(shapes::chords({.sides = 6, .step = 2,
                                         .closed = true}))
                  .fill(Fill::none())
                  .stroke(stroke(1.1f, Fill::color(kLine))),
              beads(6, rKern, 4, kPitch, 1.0f, kLine),
              beads(6, rMote, 2.2f, kPitch * 0.5f, 0.9f, kHair)}),
         text(content["monogram"].text())
             .font({.size = 52, .track = 6})
             .ink(kBone)
             .filter(styles::textGlow(kHalo, 7.0f))
             .centerAt(kEye)});
  }

  /** ONE SEAL: a small magic circle of its own standing on the rim — an
   *  opaque ground, two rules, an order-sided polygon, two words of the
   *  register round it and its ordinal at the centre. The ground occludes
   *  the bands beneath, because a seal sits on the plate. */
  Element seal(const sigil::data::Json& seal, size_t index) {
    const float inner = kSealBaseline - 7;
    const SkPoint centre{kSealRadius, kSealRadius};
    return kit::disc(polar((index + 0.5f) * kPitch, rSealRide), kSealRadius)
        .borderRadius({kSealRadius})
        .fill(Fill::color(kSealGround))
        .overlay(styles::innerGlow(hexColor(0xE79A32, 0.30f), 8.0f))
        .foreground(decorations::border(1.2f, Fill::color(kLine)))
        .children(
            {kit::ring(centre, inner, stroke(0.7f, Fill::color(kHair))),
             kit::disc(centre, kSealRadius - 16)
                 .shape(shapes::polygon((int)seal["sides"].number()))
                 .fill(Fill::none())
                 .stroke(stroke(0.9f, Fill::color(kHair))),
             kit::at(text(seal["words"].text())
                         .styleClass("band")
                         .font({.size = sealSize[index], .track = 1.4f})
                         .ink(kBone),
                     kSealRadius - kSealBaseline, kSealRadius - kSealBaseline,
                     2 * kSealBaseline, 2 * kSealBaseline)
                 .textOnPath({.path = shapes::circle(),
                              .at = float(index) / kStations,
                              .offset = -sealSize[index] * 0.34f}),
             text(seal["ordinal"].text())
                 .styleClass("mono")
                 .font({.size = 12, .track = 1})
                 .ink(kGold)
                 .filter(styles::textGlow(kHalo, 3.0f))
                 .centerAt(centre)});
  }

  /** The one off-order mark: a medallion straddling the outermost rule at
   *  twelve o'clock, between the first seal and the last, carrying a
   *  single letterform. A perfectly regular figure reads as a pattern; one
   *  exception makes it a drawing. */
  Element spur() {
    const SkPoint centre = polar(0, rEdge);
    const float size = 23;
    return sheet().children(
        {kit::dot(centre, size, Fill::color(kSealGround)),
         kit::ring(centre, size, stroke(1.2f, Fill::color(kLine))),
         kit::ring(centre, size * 0.6f, stroke(1.2f, Fill::color(kLine))),
         text(content["spur"].text()).font({.size = 19}).ink(kBone)
             .centerAt(centre)});
  }

  Element colophon() {
    return kit::at(0, kSide - 72, kSide, 50)
        .column()
        .alignItems(Align::Center)
        .gap(7)
        .styleClass("label")
        .ink(kAsh)
        .children({text(content["title"].text()).font({.size = 12,
                                                       .track = 5.2f}),
                   text(content["colophon"].text()).font({.size = 11,
                                                          .track = 0.3f})});
  }

  Element describe() {
    return box()
        .inset(0)
        .fill(Paint::glowUnit({0.5f, 0.5f}, 0.9f,
                              {{0.0f, kNightLift}, {1.0f, kNight}}))
        .applyStyleSheet(registers)
        .children(
            {// The line work, lit: one soft halo over every rule and figure,
             // so the circle reads as charged rather than merely drawn.
             sheet()
                 .filter(Effect::glow(hexColor(0xE79A32, 0.45f), 3.0f))
                 .children(
                     {each(kRules,
                           [](const Rule& rule) {
                             return kit::ring(kEye, rule.radius * kRadius,
                                              stroke(rule.width,
                                                     Fill::color(rule.ink)));
                           }),
                      each(kLadders, ladder),
                      // The fast layer: a hairline ladder in the tick band's
                      // inner half.
                      sheet().rotate(turn(kFine)).children({ladder(
                          {288, 6, rTickMid - 0.003f, 0.800f, 0.6f, kLine})}),
                      arcs(), star(), innerStar()}),
             each(kBands,
                  [&](const Band& band, size_t index) {
                    return script(band, bandSize[index]);
                  }),
             thresholds(), emblem(),
             each(content["seals"].items(),
                  [&](const sigil::data::Json& row, size_t index) {
                    return seal(row, index);
                  }),
             spur(), colophon()});
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kSide, kSide);
    context.background(kNight);
    // Far enough into the turning that every layer is visibly off its
    // stations, and the neighbours visibly apart.
    context.captureAt(6.0);
    content = sketch::kit::Document(context, "data/content.json");

    // THE THREE VOICES THAT NAME A FACE. The rune bands name none: the
    // invented alphabets are not in the interface family, and a leaf that
    // names no face lets the shaper's fallback find the letterforms.
    registers = StyleSheet{
        rule(".band").font({.face = sketch::kit::houseFace(
                                sketch::kit::Voice::Interface, 600)}),
        rule(".label").font({.face = sketch::kit::houseFace(
                                 sketch::kit::Voice::Interface, 500)}),
        rule(".mono").font(
            {.face = weave::ports::face({"Menlo", "SF Mono", "Courier New"},
                                        500)})};

    for (size_t index = 0; index < std::size(kBands); ++index)
      bandSize[index] = fit(context, bandRun(kBands[index]),
                            kBands[index].radius * kRadius,
                            kBands[index].fill);
    const auto seals = content["seals"].items();
    for (size_t index = 0; index < seals.size() && index < kStations; ++index)
      sealSize[index] = fit(context,
                            text(seals[index]["words"].text())
                                .styleClass("band")
                                .font({.track = 1.4f}),
                            kSealBaseline, 0.97f);
    context.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext&) {
    for (int layer = 0; layer < kTurnings; ++layer) {
      const double turns = elapsed / kPeriod[layer];
      phase[layer] = float(turns - std::floor(turns));
    }
  }
};

}  // namespace

SIGIL_SKETCH(RotaConvocationis, "Study · Type",
             "An invented conjuring wheel in the anime idiom — four bands "
             "of script, two star compounds and twelve seals, neighbours "
             "turning against each other")
