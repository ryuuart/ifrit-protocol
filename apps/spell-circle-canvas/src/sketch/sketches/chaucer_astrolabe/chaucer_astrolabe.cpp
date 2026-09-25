// The astrolabe of Chaucer's Treatise, as the English instrument of 1326
// shows it: the fore side with its limb, the plate for Oxford and the
// pierced rete over it, and the back with its altitude scale, calendar and
// shadow square. The rete stands where Chaucer set it on 12 March 1391 —
// the sun in 1° of Aries on the almucantar of 25° 30′ — and then turns
// through the day, because that is what the instrument is for.
//
// Every curve on the plate is a circle, and every circle is the sphere
// projected from the south celestial pole onto the plane of the equator:
// the tropics, the horizon and its almucantars, the azimuths, the ecliptic.
// The twelve stars and the lettering stand in data/.

// TAGS: Data/Astronomy

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Projection.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

namespace data = sigil::data;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::material::hexColor;
using material::Paint;
using sigil::motion::bind;

namespace {

// ---------------------------------------------------------------------------
// The look: latten under museum light, on vellum, in a dark case.

const material::Color kVellum = hexColor(0xefe6d2);
const material::Color kCard = hexColor(0xe8dcc2, 0.62f);
const material::Color kInk = hexColor(0x241c15);
const material::Color kRubric = hexColor(0x8c2f22);
const material::Color kCase = hexColor(0x1d222d);
const material::Color kCut = hexColor(0x33240c);  // an engraved line
const material::Color kCutLight = hexColor(0xf2dfa0);
const material::Color kEdge = hexColor(0x2a1d08);  // the metal's own edge
const material::Color kGilt = hexColor(0xffdc8b);

// ---------------------------------------------------------------------------
// The instrument's frame. One unit is the radius of the Tropic of
// Capricorn, which is the edge of the plate; the limb and the mater stand
// outside it.

constexpr float kWidth = 2400, kHeight = 1600;
constexpr SkPoint kCentre{622, 870};
constexpr float kR = 470;
constexpr float kMater = 1.155f * kR;
constexpr float kDegree = 3.14159265358979f / 180.0f;

/** Plate units to the plate box's own pixels: x right, y UP. */
const path::Grid kPlateBox{.scale = kR, .yScale = -1.0f, .origin = {kR, kR}};
SkPoint onPlate(glm::vec2 units) { return kPlateBox.at({units.x, units.y}); }
/** Where an angle (degrees counterclockwise from +x) falls along
 *  `shapes::circle()`, which runs clockwise on the page from +x. */
float alongCircle(float angle) {
  return std::fmod(1.0f - angle / 360.0f + 2.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// The two numbers the plate is cut from, both Chaucer's: the obliquity of
// the ecliptic (I.17) and the latitude of Oxford (I.14).

constexpr float kObliquity = 23.0f + 50.0f / 60.0f;
constexpr float kLatitude = 51.0f + 50.0f / 60.0f;

/** The sphere seen from the south pole onto the plane of the equator,
 *  scaled so the Tropic of Capricorn is one unit; right ascension is read
 *  as an ordinary angle from +x. */
const float kEquator = std::tan((90.0f - kObliquity) * 0.5f * kDegree);
const path::Projection kPlate{.scheme = path::Scheme::Stereographic,
                              .centre = {.lonDeg = 0, .latDeg = 90},
                              .scale = kEquator * 0.5f,
                              .rollDeg = 90.0f};
const path::Spherical kZenith{.lonDeg = 90.0f, .latDeg = kLatitude};
const path::Spherical kEclipticPole{.lonDeg = 270.0f,
                                    .latDeg = 90.0f - kObliquity};

/** A declination lands at radius R_eq·tan((90−δ)/2). */
float radiusOf(float declination) {
  return kPlate.radiusAt(90.0f - declination);
}
/** A point of the sky by right ascension and declination. */
glm::vec2 sky(float rightAscension, float declination) {
  return kPlate.at({.lonDeg = rightAscension, .latDeg = declination});
}
/** The same point by hour angle: noon at the top, east on the LEFT,
 *  because an astrolabe shows the sky from outside the sphere. */
glm::vec2 byHour(float declination, float hourAngle) {
  return sky(90.0f - hourAngle, declination);
}
/** The almucantar of altitude @p altitude; zero is the horizon. */
path::PlaneCircle almucantar(float altitude) {
  return *kPlate.circleOf(kZenith, 90.0f - altitude);
}
const path::PlaneCircle kEcliptic = *kPlate.circleOf(kEclipticPole, 90.0f);

/** An azimuth, counted from the prime vertical: every one of them passes
 *  through the zenith and the nadir. */
path::PlaneCircle azimuth(float fromPrimeVertical) {
  return *kPlate.circleOf(
      path::offsetFrom(kZenith, 180.0f - fromPrimeVertical, 90.0f), 90.0f);
}

/** How far west of the meridian a body of declination @p declination sets. */
float setting(float declination) {
  return std::acos(std::clamp(-std::tan(kLatitude * kDegree) *
                                  std::tan(declination * kDegree),
                              -1.0f, 1.0f)) /
         kDegree;
}
/** The k-th line of the unequal hours, struck as the makers struck it:
 *  the arc of each tropic and of the equator below the horizon is divided
 *  into twelve, and one circle swung through the three k-th divisions. The
 *  sixth has no circle — its three points stand on one line, because
 *  midnight is midnight at every declination. */
std::optional<path::PlaneCircle> unequalHour(int k) {
  const auto division = [k](float declination) {
    const float night = 360.0f - 2.0f * setting(declination);
    return byHour(declination, setting(declination) + k * night / 12.0f);
  };
  return path::circleThrough(division(kObliquity), division(0.0f),
                             division(-kObliquity));
}

/** The sun at ecliptic longitude @p longitude, carried onto the equator by
 *  one turn of the sphere about the line of the equinoxes. */
const path::Rotation kFromEcliptic = path::Rotation::aboutX(kObliquity);
float sunDeclination(float longitude) {
  return kFromEcliptic({longitude, 0}).latDeg;
}
float sunRightAscension(float longitude) {
  return kFromEcliptic({longitude, 0}).lonDeg;
}
/** Where longitude @p longitude falls on the ecliptic ring, as an angle
 *  about the ring's OWN centre: the signs are unequal there, Capricorn more
 *  than twice the width of Cancer. */
float eclipticAngle(float longitude) {
  const glm::vec2 point =
      sky(sunRightAscension(longitude), sunDeclination(longitude));
  return std::atan2(point.y - kEcliptic.centre.y, point.x) / kDegree;
}

// Chaucer's reading. The sun in 1° of Aries, measured 25° 30′ high on the
// back; the rete is turned until the sun's degree lies on that almucantar,
// east of the meridian, and the label laid over it points into the hour.
constexpr float kSunLongitude = 1.0f;
constexpr float kAltitude = 25.5f;
const float kSunDeclination = sunDeclination(kSunLongitude);
const float kChaucerHourAngle =
    -std::acos((std::sin(kAltitude * kDegree) -
                std::sin(kLatitude * kDegree) *
                    std::sin(kSunDeclination * kDegree)) /
               (std::cos(kLatitude * kDegree) *
                std::cos(kSunDeclination * kDegree))) /
    kDegree;
/** The still names Chaucer's moment. */
constexpr double kStill = 22.1;

// ---------------------------------------------------------------------------
// Drawing words this plate repeats.

/** One sheet of latten at @p level between its shadow and its light, the
 *  sheen laid corner to corner across the whole canvas so every face of
 *  the instrument is lit by the same light. */
Paint brass(float level) {
  return Paint::recipe(material::kit::latten(
                           {.shadow = hexColor(0x5c462f),
                            .body = hexColor(0xa18643),
                            .light = kGilt,
                            .from = {kCentre.fX - kMater, kCentre.fY + kMater},
                            .to = {kCentre.fX + kMater, kCentre.fY - kMater},
                            .level = level,
                            .sheen = 0.10f,
                            .tooth = 0.07f,
                            .toothScale = 0.55f}))
      .worldSpace();
}

/** An engraved circle: a V-cut, dark on its shadowed wall and light on its
 *  lit one, @p depth saying how strongly it reads. */
Element engraved(SkPoint centre, float radius, float width, float depth) {
  return kit::ring(centre, radius,
                   kit::groove(radius, width, material::withAlpha(kCut, depth),
                               material::withAlpha(kCutLight, depth * 0.5f)));
}
Element engraved(const path::PlaneCircle& circle, float width, float depth) {
  return engraved(onPlate(circle.centre), circle.radius * kR, width, depth);
}

/** A mark along a radius of @p centre, from @p inner to @p outer px, at
 *  @p angle degrees counterclockwise from +x. */
Element spoke(SkPoint centre, float angle, float inner, float outer,
              float thickness, SurfacePaint paint) {
  return kit::at(centre.fX + inner, centre.fY - thickness * 0.5f,
                 outer - inner, thickness)
      .fill(std::move(paint))
      .transformOrigin(Dimension(-inner), pct(50))
      .rotate(-angle);
}

/** Lettering run round a circle of @p radius about @p centre, centred at
 *  arc fraction @p along of `shapes::circle()`. */
Element around(const Utf8& words, SkPoint centre, float radius, float along,
               TextPath::Orient orient = TextPath::Orient::Tangent) {
  return text(words)
      .width(2 * radius)
      .height(2 * radius)
      .centerAt(centre)
      .textOnPath(TextPath{.path = shapes::circle(),
                           .at = along,
                           .align = TextPath::Align::Center,
                           .autoFlip = false,
                           .orient = orient});
}

/** A card of the commentary: its head from the document, a rule, and the
 *  content flowing under it. */
Element card(const data::Json& page, Element content) {
  return box()
      .padding(12, 16, 14, 16)
      .gap(4)
      .fill(Fill::color(kCard))
      .stroke(stroke(1.0f, Fill::color(material::withAlpha(kInk, 0.24f)),
                     PathFormat::Align::Inner))
      .children({text(page["title"]).styleClass("title"),
                 text(page["subtitle"]).styleClass("subtitle"),
                 kit::line({.fill = Fill::color(material::withAlpha(kInk, 0.28f))})
                     .margin(6, 0, 10, 0),
                 std::move(content)});
}

struct ChaucerAstrolabe {
  sketch::kit::Document words, tables;
  sk_sp<SkTypeface> engraver, copperplate, book, italic, mono;
  sigil::motion::Animatable<float> hourAngle = sigil::motion::animatable<float>(kChaucerHourAngle);
  double elapsed = 0;

  // =========================================================================
  // THE MATER AND ITS LIMB: the degrees outside, the 24 hour letters inside.

  Element mater() const {
    const float throne = 0.20f * kR;
    const SkPoint top{kCentre.fX, kCentre.fY - kMater};
    return box().inset(0).children({
        // the throne and its shackle: the instrument hangs plumb from it
        kit::disc({top.fX, top.fY - throne - 0.055f * kR}, 0.075f * kR)
            .shape(shapes::annulus(0.62f))
            .fill(brass(0.78f)),
        kit::at(top.fX - 0.16f * kR, top.fY - throne, 0.32f * kR,
                throne + 0.05f * kR)
            .shape(shapes::blob(3u, 0.10f, 9))
            .fill(brass(0.74f))
            .foreground(styles::BevelEmboss{.depth = 2,
                                            .size = 4,
                                            .angleDeg = 125,
                                            .highlight = hexColor(0xfff0c4, 0.6f),
                                            .shadow = material::withAlpha(kEdge, 0.6f)}),
        kit::dot(kCentre, kMater, brass(0.50f))
            .background(shadow(hexColor(0x05070c, 0.62f), {8, 12}, 26))
            .foreground(styles::BevelEmboss{.depth = 3,
                                            .size = 6,
                                            .angleDeg = 125,
                                            .highlight = hexColor(0xfff0c4, 0.5f),
                                            .shadow = material::withAlpha(kEdge, 0.6f)}),
        // the three rules of the limb
        engraved(kCentre, 1.155f * kR - 2, 3.0f, 0.75f),
        engraved(kCentre, 1.082f * kR, 2.0f, 0.75f),
        engraved(kCentre, 1.005f * kR, 2.0f, 0.75f),
        // the degrees, hung from the outer rule: every degree, every fifth
        // longer, every thirtieth longest and numbered
        kit::disc(kCentre, kMater)
            .shape(shapes::ticks({.divisions = 360,
                                  .mark = {0.986f, 0.996f},
                                  .longEvery = 5,
                                  .longMark = {0.976f, 0.996f}}))
            .fill(Fill::none())
            .stroke(stroke(1.0f, Fill::color(material::withAlpha(kCut, 0.85f)))),
        kit::disc(kCentre, kMater)
            .shape(shapes::ticks({.divisions = 12, .mark = {0.966f, 0.996f}}))
            .fill(Fill::none())
            .stroke(stroke(1.6f, Fill::color(kCut))),
        each(12,
             [](std::size_t index) {
               const int degrees = (int)index * 30;
               return around(Utf8(std::to_string(degrees == 0 ? 360 : degrees)),
                             kCentre, 1.104f * kR,
                             alongCircle(90.0f - (float)degrees),
                             TextPath::Orient::Tangent)
                   .styleClass("degree");
             }),
        // the hours: a division between each pair of letters
        kit::disc(kCentre, 1.082f * kR)
            .shape(shapes::ticks({.divisions = 24,
                                  .from = 7.5f,
                                  .mark = {1.005f / 1.082f, 1.0f}}))
            .fill(Fill::none())
            .stroke(stroke(1.2f, Fill::color(material::withAlpha(kCut, 0.8f)))),
        // A stands on the first hour after noon and they run clockwise, so X,
        // the twenty-first, stands at 9 in the morning
        each(tables["letters"].items(),
             [](const data::Json& letter, std::size_t index) {
               return around(Utf8(letter), kCentre, 1.044f * kR,
                             alongCircle(90.0f - 15.0f * (float)(index + 1)),
                             TextPath::Orient::Radial)
                   .styleClass("letter");
             }),
    });
  }

  // =========================================================================
  // THE PLATE for the latitude of Oxford.

  Element plate() const {
    const path::PlaneCircle horizon = almucantar(0.0f);
    const SkPoint skyCorner{onPlate(horizon.centre).fX - horizon.radius * kR,
                            onPlate(horizon.centre).fY - horizon.radius * kR};
    const auto inSky = [skyCorner](SkPoint point) {
      return SkPoint{point.fX - skyCorner.fX, point.fY - skyCorner.fY};
    };
    const std::array<float, 3> tropics{1.0f, kEquator, kEquator * kEquator};
    return kit::at(kCentre.fX - kR, kCentre.fY - kR, 2 * kR, 2 * kR)
        .shape(shapes::circle())
        .overflow(Overflow::Clip)
        .fill(brass(0.46f))
        .children({
            // the unequal hours stand below the horizon only: they are cut
            // first, and the sky is the plate again laid over them
            each(11,
                 [](std::size_t index) -> Element {
                   const int k = (int)index + 1;
                   if (const auto line = unequalHour(k)) return engraved(*line, 1.4f, 0.55f);
                   return kit::at(kR - 0.8f, kR, 1.6f, kR)
                       .fill(Fill::color(material::withAlpha(kCut, 0.5f)));
                 }),
            kit::dot(onPlate(horizon.centre), horizon.radius * kR, brass(0.46f)),
            // the twilight: the sun 18° below the horizon
            kit::ring(onPlate(almucantar(-18.0f).centre),
                      almucantar(-18.0f).radius * kR,
                      PathFormat{.width = 1.4f,
                                 .strokeFill = Fill::color(material::withAlpha(kCut, 0.42f)),
                                 .dashIntervals = {3.0f, 5.0f}}),
            // the almucantars, "compowned by two and two"
            each(44,
                 [](std::size_t index) {
                   const bool tenth = (index + 1) % 5 == 0;
                   return engraved(almucantar(2.0f * (float)(index + 1)),
                                   tenth ? 1.8f : 1.4f, tenth ? 0.85f : 0.65f);
                 }),
            // the azimuths, every 15°, only in the visible sky
            kit::disc(onPlate(horizon.centre), horizon.radius * kR)
                .shape(shapes::circle())
                .overflow(Overflow::Clip)
                .children({each(11,
                                [&inSky](std::size_t index) {
                                  const path::PlaneCircle line =
                                      azimuth(15.0f * ((float)index - 5.0f));
                                  return engraved(inSky(onPlate(line.centre)),
                                                  line.radius * kR,
                                                  index == 5 ? 1.7f : 1.3f,
                                                  index == 5 ? 0.6f : 0.45f);
                                })}),
            // the meridian is straight: it passes through the south pole,
            // the eye of the projection
            kit::at(kR - 1, 0, 2, 2 * kR)
                .fill(Fill::color(material::withAlpha(kCut, 0.55f))),
            engraved(horizon, 3.2f, 0.9f),
            each(tropics,
                 [](float radius) {
                   return engraved(onPlate({0, 0}), radius * kR,
                                   radius == 1.0f ? 3.0f : 2.4f, 0.8f);
                 }),
            // Chaucer's almucantar, the one the sun was set on, in rubric
            kit::ring(onPlate(almucantar(kAltitude).centre),
                      almucantar(kAltitude).radius * kR,
                      stroke(1.6f, Fill::color(material::withAlpha(kRubric, 0.75f)))),
            kit::dot(onPlate(kPlate.at(kZenith)), 3.6f,
                     Fill::color(material::withAlpha(kCut, 0.85f))),
        })
        .foreground(styles::InnerShadow{material::withAlpha(kEdge, 0.55f), {0, 3}, 9});
  }

  // =========================================================================
  // THE RETE: one pierced sheet — the outer ring, the cross, the ecliptic
  // ring and a pointer for every star, whose TIP is the star.

  /** Where a star's pointer springs from: the nearest point of the metal
   *  it grows out of — the outer ring, the ecliptic ring or an arm. */
  static glm::vec2 root(glm::vec2 star) {
    constexpr float ring = 0.979f;
    const glm::vec2 fromEcliptic = star - kEcliptic.centre;
    const std::array<glm::vec2, 4> candidates{
        star * (ring / std::max(glm::length(star), 1e-4f)),
        kEcliptic.centre +
            fromEcliptic * (kEcliptic.radius / std::max(glm::length(fromEcliptic), 1e-4f)),
        glm::vec2{std::clamp(star.x, -ring, ring), 0.0f},
        glm::vec2{0.0f, std::clamp(star.y, -ring, ring)}};
    glm::vec2 best = candidates[0];
    for (const glm::vec2& candidate : candidates)
      if (glm::distance(candidate, star) < glm::distance(best, star)) best = candidate;
    return best;
  }

  /** A Gothic pointer: a blade from its root, bowed to one side, tapering
   *  to the star. A star that sits on its host gets a blade laid across it. */
  static Element pointer(glm::vec2 star, std::size_t index) {
    glm::vec2 from = root(star);
    glm::vec2 run = star - from;
    if (glm::length(run) < 0.07f) {
      const glm::vec2 outward =
          glm::length(star) > 1e-4f ? glm::normalize(star) : glm::vec2{0, 1};
      from = star - outward * 0.09f;
      run = star - from;
    }
    const float side = index % 2 == 0 ? 1.0f : -1.0f;
    const glm::vec2 bow =
        (from + star) * 0.5f + glm::vec2{-run.y, run.x} * (0.07f * side);
    const std::array<glm::vec2, 3> spine{
        path::fromSk(onPlate(from)), path::fromSk(onPlate(bow)),
        path::fromSk(onPlate(star))};
    return pathFigure(path::smoothThrough(spine), 12)
        .fill(Fill::none())
        .foreground(brush::presets::taper(0.030f * kR + 2.5f, 2.5f,
                                          Fill::color(material::withAlpha(kEdge, 0.8f))))
        .foreground(brush::presets::taper(0.030f * kR, 1.0f, brass(0.72f)));
  }

  /** A foil of @p lobes rings about @p centre. */
  static Element foil(SkPoint centre, int lobes, float radius) {
    return kit::disc(centre, radius).children({each(lobes, [=](std::size_t index) {
      const float angle = 90.0f + 360.0f * (float)index / (float)lobes;
      const SkPoint lobe{radius + 0.5f * radius * std::cos(angle * kDegree),
                         radius - 0.5f * radius * std::sin(angle * kDegree)};
      return kit::disc(lobe, 0.52f * radius)
          .shape(shapes::ring(0.16f * radius))
          .fill(brass(0.68f));
    })});
  }

  Element rete() const {
    const SkPoint pin = onPlate({0, 0});
    const SkPoint eclipticCentre = onPlate(kEcliptic.centre);
    const float eclipticRadius = kEcliptic.radius * kR;
    const float band = 0.085f * kR;
    const float bar = 0.040f * kR;
    const data::Json& stars = tables["stars"];
    const auto starAt = [](const data::Json& star) {
      return sky((float)star["ra"].number(), (float)star["dec"].number());
    };
    const glm::vec2 sun =
        sky(sunRightAscension(kSunLongitude), kSunDeclination);

    Element sheet =
        kit::at(kCentre.fX - kR, kCentre.fY - kR, 2 * kR, 2 * kR)
            .shape(shapes::circle())
            .overflow(Overflow::Clip)
            // the rete turns so the sun's right ascension stands at the
            // hour angle the label reads
            .transformOrigin(pct(50), pct(50))
            .rotate(sigil::motion::bind(hourAngle, {.to = {sunRightAscension(kSunLongitude) - 90.0f, sunRightAscension(kSunLongitude) - 90.0f + 1.0f}}))
            .children({
                each(stars.items(),
                     [&](const data::Json& star, std::size_t index) {
                       return pointer(starAt(star), index);
                     }),
                // the ecliptic ring, clipped where it runs past Capricorn,
                // so it fuses with the outer ring there as on the object
                kit::disc(eclipticCentre, eclipticRadius + band * 0.5f)
                    .shape(shapes::ring(band))
                    .fill(brass(0.64f)),
                kit::disc(pin, kR)
                    .shape(shapes::ring(0.042f * kR))
                    .fill(brass(0.66f)),
                kit::at(pin.fX - kR, pin.fY - bar * 0.5f, 2 * kR, bar).fill(brass(0.66f)),
                kit::at(pin.fX - bar * 0.5f, pin.fY - kR, bar, 2 * kR).fill(brass(0.66f)),
                foil(onPlate({0, 0.72f}), 4, 0.10f * kR),
                foil(onPlate({0, -0.66f}), 3, 0.09f * kR),
                kit::disc(pin, 0.11f * kR)
                    .shape(shapes::star(12, 0.52f, 0.16f))
                    .fill(brass(0.70f)),
                // the almury, the tooth at the head of Capricorn
                kit::disc(onPlate({0, -1.0f + 0.055f}), 0.048f * kR)
                    .shape(shapes::polygon(3, 180))
                    .fill(brass(0.70f)),
                // the ecliptic itself, down the middle of its ring, and its
                // degrees at the projection's own unequal spacing
                engraved(eclipticCentre, eclipticRadius, 1.8f, 0.7f),
                each(360,
                     [&](std::size_t degree) {
                       const bool sign = degree % 30 == 0, fifth = degree % 5 == 0;
                       const float reach = sign ? band * 0.5f : (fifth ? 0.030f : 0.018f) * kR;
                       return spoke(eclipticCentre, eclipticAngle((float)degree),
                                    eclipticRadius - (sign ? band * 0.5f : reach),
                                    sign ? eclipticRadius + band * 0.5f : eclipticRadius,
                                    sign ? 1.4f : 0.8f,
                                    Fill::color(material::withAlpha(kCut, 0.7f)));
                     }),
                each(tables["signs"].items(),
                     [&](const data::Json& name, std::size_t index) {
                       float from = eclipticAngle(30.0f * (float)index);
                       float to = eclipticAngle(30.0f * (float)(index + 1));
                       if (to < from) to += 360.0f;
                       return around(Utf8(name), eclipticCentre, eclipticRadius + 4,
                                     alongCircle((from + to) * 0.5f))
                           .styleClass("sign");
                     }),
                // the stars' names, engraved along the outer ring
                each(stars.items(),
                     [&](const data::Json& star) {
                       const glm::vec2 at = starAt(star);
                       const float angle = std::atan2(at.y, at.x) / kDegree;
                       return around(Utf8(star["name"]), pin, kR * 0.979f,
                                     alongCircle(angle))
                           .styleClass("starName");
                     }),
                each(stars.items(),
                     [&](const data::Json& star) {
                       return kit::dot(onPlate(starAt(star)), (float)star["tip"].number(5.5), Fill::color(kGilt))
                           .stroke(stroke(1.5f, Fill::color(material::withAlpha(kEdge, 0.85f))));
                     }),
                // the sun, set in its degree of the ecliptic
                kit::disc(onPlate(sun), 22.0f)
                    .shape(shapes::star(12, 0.40f, 0.16f))
                    .fill(Fill::color(hexColor(0xfff6dc)))
                    .stroke(stroke(1.4f, Fill::color(material::withAlpha(kEdge, 0.75f)))),
            });
    // the sheet is raised off the plate: it casts, and its edges catch
    return box()
        .inset(0)
        .decorationOutline(Boundary::Coverage)
        .background(shadow(material::withAlpha(kEdge, 0.55f), {7, 9}, 8))
        .foreground(styles::BevelEmboss{.depth = 2,
                                        .size = 3,
                                        .angleDeg = 125,
                                        .highlight = hexColor(0xffe9b0, 0.55f),
                                        .shadow = material::withAlpha(kEdge, 0.55f)})
        .children({std::move(sheet)});
  }

  /** The label, pivoted on the pin, lies over the sun and reads the hour
   *  in the bordure. */
  Element label() const {
    return box().inset(0).children({
        kit::at(kCentre.fX, kCentre.fY - 4.5f, kMater * 1.02f, 9)
            .transformOrigin(Dimension(0), pct(50))
            .rotate(sigil::motion::bind(hourAngle, {.to = {-90.0f, -89.0f}}))
            .fill(brass(0.80f))
            .stroke(stroke(1.2f, Fill::color(material::withAlpha(kEdge, 0.7f))))
            .background(shadow(material::withAlpha(kEdge, 0.5f), {4, 5}, 7)),
        kit::dot(kCentre, 0.040f * kR, brass(0.82f))
            .background(shadow(material::withAlpha(kEdge, 0.5f), {2, 3}, 5))
            .foreground(styles::BevelEmboss{.depth = 2,
                                            .size = 2,
                                            .angleDeg = 125,
                                            .highlight = hexColor(0xfff0c4, 0.7f),
                                            .shadow = material::withAlpha(kEdge, 0.6f)}),
    });
  }

  // =========================================================================
  // THE BACK: the altitude scale round the edge, the calendar and the
  // zodiac inside it, the shadow square below, and the rule on its pivot.

  Element back(float radius) const {
    const SkPoint centre{radius, radius};
    const float square = radius * 0.50f;
    const Fill cut = Fill::color(material::withAlpha(kCut, 0.65f));
    const auto calendarAngle = [](float day) {  // January at the top, clockwise
      return 90.0f - day / 365.0f * 360.0f;
    };
    std::array<float, 13> monthStarts{};
    for (std::size_t month = 0; month < 12; ++month)
      monthStarts[month + 1] =
          monthStarts[month] + (float)tables["months"][month]["days"].number();
    // the sun enters Aries on the 12th of March in Chaucer's calendar
    constexpr float kAries = 31 + 28 + 11.5f;
    return box().width(2 * radius).height(2 * radius).styleClass("engrave").children({
        kit::dot(centre, radius, brass(0.46f))
            .foreground(styles::BevelEmboss{.depth = 2,
                                            .size = 4,
                                            .angleDeg = 125,
                                            .highlight = hexColor(0xffe9b0, 0.45f),
                                            .shadow = material::withAlpha(kEdge, 0.5f)}),
        // four quadrants of ninety degrees, every second degree ruled
        kit::disc(centre, radius)
            .shape(shapes::ticks({.divisions = 180,
                                  .mark = {0.93f, 0.96f},
                                  .longEvery = 5,
                                  .longMark = {0.90f, 0.96f}}))
            .fill(Fill::none())
            .stroke(stroke(0.8f, cut)),
        each(std::array{0.96f, 0.90f, 0.855f, 0.78f, 0.70f},
             [&](float ring) { return engraved(centre, ring * radius, 1.2f, 0.6f); }),
        // the calendar: months of their own lengths
        each(12,
             [&](std::size_t month) {
               return spoke(centre, calendarAngle(monthStarts[month]), 0.78f * radius,
                            0.855f * radius, 1.0f, cut);
             }),
        each(tables["months"].items(),
             [&](const data::Json& month, std::size_t index) {
               const float middle =
                   calendarAngle((monthStarts[index] + monthStarts[index + 1]) * 0.5f);
               return text(month["name"]).centerAt(
                   {centre.fX + 0.817f * radius * std::cos(middle * kDegree),
                    centre.fY - 0.817f * radius * std::sin(middle * kDegree)});
             }),
        // the zodiac beside it, twelve equal signs from Aries
        each(12,
             [&](std::size_t sign) {
               return spoke(centre, calendarAngle(kAries + 365.0f / 12.0f * (float)sign),
                            0.70f * radius, 0.78f * radius, 1.0f, cut);
             }),
        each(tables["signs"].items(),
             [&](const data::Json& name, std::size_t index) {
               const float middle =
                   calendarAngle(kAries + 365.0f / 12.0f * ((float)index + 0.5f));
               return text(Utf8(std::string(name.text().substr(0, 3))))
                   .centerAt({centre.fX + 0.74f * radius * std::cos(middle * kDegree),
                              centre.fY - 0.74f * radius * std::sin(middle * kDegree)});
             }),
        // the shadow square: twelve parts on each side
        kit::at(centre.fX - square, centre.fY, 2 * square, square)
            .fill(Fill::none())
            .stroke(stroke(1.4f, cut)),
        kit::at(centre.fX - 0.5f, centre.fY, 1, square).fill(cut),
        each(11,
             [&](std::size_t index) {
               const float step = square * (float)(index + 1) / 12.0f;
               const float reach = square * ((index + 1) % 3 == 0 ? 0.16f : 0.09f);
               return box().inset(0).children({
                   kit::at(centre.fX - square, centre.fY + step, reach, 0.8f).fill(cut),
                   kit::at(centre.fX + square - reach, centre.fY + step, reach, 0.8f).fill(cut),
                   kit::at(centre.fX - square + step - 0.4f, centre.fY + square - reach,
                           0.8f, reach).fill(cut),
                   kit::at(centre.fX + step - 0.4f, centre.fY + square - reach, 0.8f, reach)
                       .fill(cut),
               });
             }),
        text(words["back"]["umbraRecta"])
            .centerAt({centre.fX - square * 0.52f, centre.fY + square * 0.55f}),
        text(words["back"]["umbraVersa"])
            .centerAt({centre.fX + square * 0.52f, centre.fY + square * 0.55f}),
        // the rule, sighted on the sun at 25° 30′
        kit::at(centre.fX - 0.97f * radius, centre.fY - 5, 1.94f * radius, 10)
            .transformOrigin(pct(50), pct(50))
            .rotate(-kAltitude)
            .fill(brass(0.78f))
            .stroke(stroke(1.0f, Fill::color(material::withAlpha(kEdge, 0.6f))))
            .background(shadow(material::withAlpha(kEdge, 0.45f), {2, 3}, 5)),
        kit::dot(centre, 8, brass(0.82f)),
    });
  }

  // =========================================================================
  // THE COMMENTARY

  Element masthead() const {
    const data::Json& page = words["masthead"];
    return kit::at(64, 40, kWidth - 128, 84).gap(10).children({
        text(page["title"]).styleClass("masthead"),
        text(page["subtitle"]).styleClass("mastheadSubtitle"),
        kit::line({.fill = Fill::color(material::withAlpha(kInk, 0.3f))}).margin(6, 0, 0, 0),
    });
  }

  Element provenance() const {
    const data::Json& page = words["provenance"];
    return card(page, box().gap(3).children({each(page["rows"].items(), [](const data::Json& row) {
                    return box().row().children({text(row["key"]).width(96).styleClass("head"),
                                                 text(row["value"]).flexGrow(1).flexShrink(1).width(0)});
                  })}));
  }

  Element starTable() const {
    const data::Json& page = words["stars"];
    const auto cells = [](std::array<Utf8, 5> columns, const char* style) {
      constexpr std::array<float, 5> kWidths{230, 230, 170, 170, 170};
      return box().row().children({each(5, [&](std::size_t column) {
        Element cell = text(columns[column]).width(kWidths[column]).styleClass(style);
        if (column >= 2) cell.paragraph({.alignment = weave::TextAlignment::kEnd});
        return cell;
      })});
    };
    const data::Json& heads = page["heads"];
    return card(page,
                box().gap(14).children({
                    cells({Utf8(heads[0]), Utf8(heads[1]), Utf8(heads[2]), Utf8(heads[3]),
                           Utf8(heads[4])},
                          "head"),
                    each(tables["stars"].items(),
                         [&](const data::Json& star) {
                           const float declination = (float)star["dec"].number();
                           return cells({Utf8(star["name"]), Utf8(star["modern"]),
                                         Utf8(kit::formatted("%.2f°", star["ra"].number())),
                                         Utf8(kit::formatted("%+.2f°", declination)),
                                         Utf8(kit::formatted("%.3f", radiusOf(declination)))},
                                        "tableFigure");
                         }),
                }));
  }

  Element chaucer() const {
    const data::Json& page = words["chaucer"];
    return card(page, box().gap(5).children({
                          document::paragraph(page["quote"].text()).styleClass("quote"),
                          each(words.run(page["reading"]), sketch::kit::lineOf),
                      }));
  }

  Element commentary() const {
    return kit::at(1220, 148, 1116, 1408).gap(18).children({
        box().row().gap(18).children({
            card(words["back"], back(318)).width(668),
            box().width(0).flexGrow(1).gap(18).children({chaucer(), provenance().flexGrow(1)}),
        }),
        starTable().flexGrow(1),
    });
  }

  // =========================================================================

  StyleSheet registers() const {
    const auto type = [](sk_sp<SkTypeface> face, float size, material::Color color,
                         float track = 0) {
      return weave::Type{.face = std::move(face), .size = size, .color = color, .track = track};
    };
    return StyleSheet{
        rule(".masthead").font(type(engraver, 34, kInk, 2.4f)),
        rule(".title").font(type(copperplate, 15, kRubric, 1.9f)),
        rule(".subtitle").font(type(italic, 15, hexColor(0x6b5a44))),
        rule(".head").font(type(copperplate, 11, hexColor(0x7b6a54), 1.0f)),
        rule(".figure").font(type(mono, 14, kInk)),
        rule(".tableFigure").font(type(mono, 17, kInk)),
        rule(".mastheadSubtitle").font(type(italic, 20, kRubric)),
        rule(".gloss").font(type(italic, 15, kRubric)),
        rule(".quote").font(type(italic, 17, kInk)),
        rule(".caption").font(type(italic, 17, hexColor(0xd8c79c))),
        rule(".engrave").font(type(copperplate, 12, material::withAlpha(kCut, 0.85f))),
        rule(".degree").font(type(copperplate, 13, material::withAlpha(kCut, 0.92f), 0.6f)),
        rule(".letter").font(type(copperplate, 19, kCut)),
        rule(".sign").font(type(engraver, 17, material::withAlpha(kCut, 0.88f), 1.0f)),
        rule(".starName").font(type(engraver, 12, material::withAlpha(kCut, 0.85f), 0.4f)),
    };
  }

  void setup(sketch::SketchContext& context) {
    sketch::kit::stage(context, {.size = {kWidth, kHeight},
                                 .captureAt = kStill,
                                 .background = kVellum,
                                 .plateOnly = true});
    // Three lettering systems: a chiselled majuscule on the rete, an
    // engraver's copperplate on the limb, a book face for the commentary.
    engraver = weave::ports::face({"Herculanum", "Optima", "Baskerville"});
    copperplate = weave::ports::face({"Copperplate", "Optima", "Baskerville"});
    book = weave::ports::face({"Baskerville", "Hoefler Text", "Georgia"});
    italic = weave::ports::face({"Baskerville", "Hoefler Text", "Georgia"}, 400,
                                SkFontStyle::kItalic_Slant);
    mono = weave::ports::face({"SF Mono", "Menlo", "Courier"});

    tables = sketch::kit::Document(context, "data/sky.json");
    words = sketch::kit::Document(context, "data/content.json");
    const float hours = 12.0f + kChaucerHourAngle / 15.0f;
    words.figures(
        {{"declination", Utf8(kit::formatted("%+.2f°", kSunDeclination))},
         {"hourAngle", Utf8(kit::formatted("%.2f° east", -kChaucerHourAngle))},
         {"time", Utf8(kit::formatted("%d:%02d", (int)hours,
                                      (int)std::lround((hours - std::floor(hours)) * 60)))}});

    // The day turns at fifteen degrees a second, from Chaucer's moment.
    context.ticker.add([this](double step) {
      elapsed += step;
      const float turned = kChaucerHourAngle + 15.0f * (float)(elapsed - kStill);
      hourAngle = std::fmod(std::fmod(turned + 180.0f, 360.0f) + 360.0f, 360.0f) - 180.0f;
    });

    context.composer.render(
        box()
            .inset(0)
            .fill(Fill::color(kVellum))
            .font({.face = book, .size = 15, .color = kInk})
            .applyStyleSheet(registers())
            .children({
                kit::at(56, 140, 1132, 1416)
                    .borderRadius({3})
                    .fill(Paint::radialGradient(
                        {0.50f, 0.46f}, 1.05f,
                        {{0.0f, hexColor(0x33405a)},
                         {0.62f, kCase},
                         {1.0f, hexColor(0x080a10)}},
                        {.extent = material::RadialExtent::ClosestSide})),
                masthead(),
                mater(),
                plate(),
                rete(),
                label(),
                text(words["front"])
                    .styleClass("caption")
                    .paragraph({.alignment = weave::TextAlignment::kCenter})
                    .left(56)
                    .width(1132)
                    .top(1494),
                commentary(),
            }));
  }
};

}  // namespace

SIGIL_SKETCH(ChaucerAstrolabe, "Study · Science",
             "A planispheric astrolabe for Oxford 51° 50′ "
             "— an instrument that tells the time")
