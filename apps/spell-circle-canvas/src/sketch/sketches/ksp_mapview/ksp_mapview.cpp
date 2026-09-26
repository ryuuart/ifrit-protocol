// Kerbal Space Program's map view, one screen of it: Kerbin at the focus of
// the vessel's orbit, the orbit's apoapsis, periapsis and node markers, a
// Mun transfer and an escape trajectory crossing the frame, the manoeuvre
// node's six handles, and the flight interface around the map — the navball
// at the bottom, the staging stack, the altimeter, the mission clock, the
// vessel card and the crew portrait.
//
// The map is data. `data/orbits.json` holds each orbit as its three conic
// elements about Kerbin (semi-latus rectum in canvas pixels, eccentricity,
// the bearing of periapsis), the stretch of each that is drawn, and every
// marker as an orbit and a true anomaly on it. `data/content.json` holds
// the interface's words and the readings its instruments stand at.

// TAGS: Motion/Trajectories, Interfaces/Game, Data/Astronomy

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Conic.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Globe.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <charconv>
#include <cmath>
#include <string>
#include <vector>
#include <sigilmotion/ease/Ease.h>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;
using sigil::material::hexColor;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::data::Json;
using sigil::geometry::path::Conic;
using sigil::material::withAlpha;
using sigil::material::Paint;

namespace {

constexpr material::Color kSpace = hexColor(0x090A0C);
constexpr material::Color kNebula = hexColor(0x6E7288);
constexpr material::Color kAtmosphere = hexColor(0x6FB3D9);
constexpr material::Color kOrbit = hexColor(0x4DD0C8);
constexpr material::Color kPrograde = hexColor(0x4CD964);
constexpr material::Color kNormal = hexColor(0xB87CE0);
constexpr material::Color kRadial = hexColor(0x5AC8E0);
constexpr material::Color kOrange = hexColor(0xDC6F2A);
constexpr material::Color kLcd = hexColor(0x35C93A);
constexpr material::Color kStageTab = hexColor(0xBE5907);
constexpr material::Color kGold = hexColor(0xFCB100);
constexpr material::Color kKeyline = hexColor(0x22282D);

// KSP sets its interface in a plain grotesque and its readouts in a
// monospaced numeral set.
auto sans(int weight = 400) {
  return sigil::weave::ports::face({"Helvetica Neue", "Arial"}, weight);
}

material::Color colour(const Json& hex, float alpha = 1.0f) {
  uint32_t value = 0;
  const std::string_view digits = hex.string();
  std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
  return hexColor(value, alpha);
}
float number(const Json& value) { return (float)value.number(); }

/** The point @p distance from @p centre along @p degrees, clockwise from +x
 *  because the screen's y runs down. */
SkPoint toward(SkPoint centre, float degrees, float distance) {
  const float radians = degrees * 0.0174532925f;
  return {centre.fX + distance * std::cos(radians),
          centre.fY + distance * std::sin(radians)};
}
float bearing(glm::vec2 direction) {
  return std::atan2(direction.y, direction.x) * 57.2957795f;
}

/** Top-lit: the light colour at the top edge, the dark at the bottom. */
Paint ramp(material::Color top, material::Color bottom) {
  return Paint::linearGradient({0, 0}, {0, 1}, {{0.0f, top}, {1.0f, bottom}});
}
/** A sphere lit from the upper left. */
Paint lit(material::Color light, material::Color middle,
          material::Color shadow) {
  return Paint::radialGradient({0.38f, 0.30f}, 1.0f,
                               {{0.0f, light}, {0.5f, middle}, {1.0f, shadow}});
}
PathFormat keyline(material::Color ink = kKeyline, float width = 1.0f) {
  return stroke(width, Fill::color(ink), PathFormat::Align::Inner);
}
/** The metal every interface plate is cut from. */
Element plate(material::Color top, material::Color bottom, float corner = 4) {
  return box().borderRadius({corner}).fill(ramp(top, bottom)).stroke(keyline());
}
Element hazard() {
  return box().fill(hexColor(0xE0B720)).foreground(lines::presets::hatch(
      Fill::color(hexColor(0x141414, 0.9f)), 8.0f, 4.0f, -45.0f));
}

struct KspMapView {
  sketch::kit::Document map, words;
  /** Seconds since the plate opened; the dotted trajectories march on it. */
  sigil::motion::Animatable<float> clock = sigil::motion::animatable(0.0f);

  SkPoint kerbin{500, 330};
  float kerbinRadius = 140;
  static constexpr SkPoint kBall{364, 668};
  static constexpr float kBallRadius = 84, kBezelRadius = 106;

  Conic orbit(std::string_view name) const {
    const Json& elements = map["orbits"][name];
    return {{kerbin.fX, kerbin.fY}, number(elements["semiLatus"]),
            number(elements["eccentricity"]), number(elements["periapsis"])};
  }
  SkPoint on(const Json& place) const {
    const glm::vec2 point =
        orbit(place["orbit"].string()).at(number(place["anomaly"]));
    return {point.x, point.y};
  }
  /** A stretch of a conic as a shape, keyed on the numbers it is drawn from
   *  so an unchanged orbit is never traced again. */
  static Shape trajectory(const Conic& conic,
                          sigil::geometry::path::ConicSpan span) {
    return keyedShape(std::pair(conic, span), [conic, span] {
      return sigil::geometry::path::conicPath(conic, span);
    });
  }
  /** Words riding @p path, as every label on the map does. */
  static Element along(const Json& words, Shape path, float at, float offset) {
    return text(words)
        .textOnPath({.path = std::move(path),
                     .at = at,
                     .align = TextPath::Align::Center,
                     .offset = offset,
                     .autoFlip = true})
        .inset(0);
  }

  // Space: a near-black ground, a mottled band of the galaxy on a diagonal,
  // and a hashed scatter of stars.
  Element space() const {
    uint32_t seed = 0x2545F491u;
    auto random = [&seed] {
      return (float)(sigil::core::noise::xorshiftNext(seed) & 0xffffffu) /
             (float)0xffffff;
    };
    std::vector<Element> band, stars;
    for (int index = 0; index < 16; ++index) {
      const float along = (float)index / 15.0f;
      const float x = -140 + along * 1360 + (random() - 0.5f) * 190;
      const float y = 40 + along * 190 + (random() - 0.5f) * 230;
      const float width = 180 + random() * 260;
      const float height = width * (0.5f + random() * 0.4f);
      const float alpha = 0.07f + random() * 0.07f, turn = -30 + random() * 60;
      band.push_back(
          kit::at(box()
                      .shape(shapes::blob(41u + 7u * index, 0.30f, 9))
                      .fill(Paint::radialGradient(
                          {0.5f, 0.5f}, 1.0f,
                          {{0.0f, withAlpha(kNebula, alpha)},
                           {0.5f, withAlpha(kNebula, alpha * 0.45f)},
                           {1.0f, withAlpha(kNebula, 0.0f)}}))
                      .rotate(turn),
                  x, y, width, height));
    }
    for (int index = 0; index < 320; ++index) {
      const float x = random() * 1200, y = random() * 800;
      const float radius = 0.5f + random() * random() * 1.3f;
      const float alpha = 0.25f + random() * 0.7f;
      stars.push_back(
          kit::dot({x, y}, radius, Fill::color(hexColor(0xFFFFFF, alpha))));
    }
    return box()
        .inset(0)
        .fill(
            Paint::radialGradient({0.42f, 0.45f}, 1.15f,
                                  {{0.0f, kSpace}, {1.0f, hexColor(0x06070A)}}))
        .children({band, stars});
  }

  // Kerbin: an ocean lit from the upper left, its continents clipped to the
  // disc, a terminator darkening the lower right, and a thin atmosphere.
  Element planet() const {
    auto continent = [](const Json& land) {
      const Json& place = land["box"];
      return kit::at(box()
                         .shape(shapes::blob((uint32_t)land["seed"].number(),
                                             number(land["amplitude"]),
                                             (int)land["lobes"].number()))
                         .fill(colour(land["colour"])),
                     number(place[0]), number(place[1]), number(place[2]),
                     number(place[3]));
    };
    return box().inset(0).children(
        {kit::dot(
             kerbin, kerbinRadius + 16,
             Paint::radialGradient({0.5f, 0.5f}, 0.71f,
                                   {{0.86f, withAlpha(kAtmosphere, 0.0f)},
                                    {0.90f, withAlpha(kAtmosphere, 0.30f)},
                                    {1.0f, withAlpha(kAtmosphere, 0.0f)}})),
         kit::dot(kerbin, kerbinRadius,
                  Paint::radialGradient({0.30f, 0.25f}, 1.0f,
                                        {{0.0f, hexColor(0x2B5C7E)},
                                         {0.30f, hexColor(0x235274)},
                                         {0.66f, hexColor(0x1B4260)},
                                         {1.0f, hexColor(0x12283A)}}))
             .overflow(Overflow::Clip)
             .children({each(map["continents"].array(), continent),
                        box().inset(0).fill(Paint::radialGradient(
                            {0.34f, 0.28f}, 1.02f,
                            {{0.38f, hexColor(0x081420, 0.06f)},
                             {0.70f, hexColor(0x061019, 0.42f)},
                             {1.0f, hexColor(0x03070B, 0.92f)}}))}),
         kit::ring(kerbin, kerbinRadius,
                   stroke(1.6f, Fill::color(withAlpha(kAtmosphere, 0.7f))))});
  }

  // The trajectories. The vessel's orbit is the one closed conic, drawn on
  // as the plate opens and glowing; the others are the stretches of larger
  // conics that cross the frame, dotted and marching.
  Element trajectories() const {
    auto crossing = [this](const Json& part) {
      const Shape path =
          trajectory(orbit(part["orbit"].string()),
                     {number(part["from"]), number(part["to"]), 260});
      const material::Color ink = colour(part["colour"], number(part["alpha"]));
      PathFormat pen = stroke(number(part["width"]), Fill::color(ink));
      if (const float march = number(part["march"]); march > 0) {
        pen.dashIntervals = {0.01f, 6.0f};
        pen.cap = sigil::geometry::path::Cap::Round;
        pen.dashPhaseBinding = sigil::motion::bind(clock, {.to = {0.0f, -march}});
      }
      Element drawn = box().inset(0).shape(path).stroke(pen);
      if (!part["label"].null())
        drawn.children({along(part["label"], path, number(part["labelAt"]),
                              number(part["labelOffset"]))
                            .ink(withAlpha(ink, 0.85f))});
      return drawn;
    };
    const Shape vessel = trajectory(orbit("vessel"), {0, 360, 360});
    return box().inset(0).font({.size = 8.5f, .track = 1.3f}).children(
        {each(map["trajectories"].array(), crossing),
         box().inset(0).shape(vessel).stroke(
             spans::upTo(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 900ms, .ease = sigil::motion::ease::outQuad})),
             brush::presets::filament(withAlpha(kOrbit, 0.30f),
                                      hexColor(0xDCF7F5), 0.26f)),
         along(map["vessel"]["label"], vessel,
               number(map["vessel"]["labelAt"]), 8)
             .font({.size = 9.5f, .track = 1.6f})
             .ink(withAlpha(kOrbit, 0.9f))});
  }

  // What stands on the trajectories: the Ap, Pe, AN and DN diamonds, the Mun
  // on its orbit, and the craft ahead of the node.
  Element markers() const {
    auto marker = [this](const Json& mark) {
      const SkPoint at = on(mark);
      const material::Color ink = colour(mark["colour"]);
      Element diamond = kit::disc(at, 4.5f).shape(shapes::polygon(4));
      if (mark["filled"].boolean()) diamond.fill(ink);
      else diamond.stroke(stroke(1.2f, Fill::color(ink)));
      return box().inset(0).ink(ink).children(
          {diamond, text(mark["label"]).centerAt(
                        {at.fX + number(mark["lift"][0]),
                         at.fY + number(mark["lift"][1])})});
    };
    auto body = [this](const Json& moon) {
      const SkPoint at = on(moon);
      const float radius = number(moon["radius"]);
      return box().inset(0).children(
          {kit::centred(text(moon["glyph"])
                            .font({.face = sans(), .size = radius * 0.9f})
                            .ink(hexColor(0x0F1316)))
               .width(radius * 2)
               .height(radius * 2)
               .centerAt(at)
               .shape(shapes::circle())
               .fill(lit(hexColor(0xB8C0C6), hexColor(0x66707A),
                         hexColor(0x2C3238)))
               .stroke(stroke(1.0f, Fill::color(hexColor(0x161A1E)))),
           text(moon["label"])
               .ink(hexColor(0xD8CE7A))
               .left(at.fX + radius + 5)
               .top(at.fY + 2)});
    };
    const Json& craft = map["craft"];
    return box()
        .inset(0)
        .font({.face = sans(700), .size = 9, .track = 0.6f})
        .children({each(map["markers"].array(), marker),
                   each(map["bodies"].array(), body),
                   kit::disc(on(craft), 6.5f)
                       .shape(shapes::polygon(3, 90))
                       .fill(hexColor(0xE8F2F4))
                       .rotate(bearing(orbit(craft["orbit"].string())
                                           .alongAt(number(craft["anomaly"]))))});
  }

  // The manoeuvre node: a hub on the orbit and six handles. Prograde and
  // retrograde lie along the direction of travel, radial out and in along
  // the line from Kerbin; the normal pair has no direction in the plane of
  // the map, so it takes the bisector of the wide side between them.
  Element manoeuvre() const {
    const Json& node = map["node"];
    const Conic path = orbit(node["orbit"].string());
    const float anomaly = number(node["anomaly"]);
    const SkPoint hub = on(node);
    const float prograde = bearing(path.alongAt(anomaly));
    const float radialOut = bearing(path.outwardAt(anomaly));
    const float normal =
        prograde + std::fmod(radialOut - prograde + 720.0f, 360.0f) * 0.5f;

    struct Handle {
      float bearing, length;
      material::Color ink;
      bool solid;
    };
    const std::array<Handle, 4> arms{{{prograde, 54, kPrograde, true},
                                      {radialOut, 42, kRadial, true},
                                      {prograde + 180, 54, kPrograde, false},
                                      {radialOut + 180, 42, kRadial, false}}};
    auto arm = [hub](const Handle& handle) {
      Element paddle =
          box()
              .width(handle.length)
              .height(20)
              .shape(shapes::arrow(0.225f, 14.0f / handle.length, 0.75f))
              .centerAt(toward(hub, handle.bearing, handle.length * 0.5f + 9))
              .rotate(handle.bearing);
      if (handle.solid) return paddle.fill(handle.ink);
      return paddle.fill(withAlpha(handle.ink, 0.22f))
          .stroke(stroke(1.8f, Fill::color(handle.ink)));
    };
    auto outOfPlane = [hub](float direction, bool solid) {
      const material::Color ink = withAlpha(kNormal, solid ? 1.0f : 0.62f);
      return box().inset(0).children(
          {box()
               .width(20)
               .height(2)
               .centerAt(toward(hub, direction, 21))
               .rotate(direction)
               .fill(withAlpha(ink, 0.75f)),
           kit::disc(toward(hub, direction, 40), 9)
               .shape(solid ? shapes::ring(2.6f, 3.0f) : shapes::ring(2.2f))
               .fill(ink)});
    };
    return box().inset(0).children(
        {each(arms, arm), outOfPlane(normal, true),
         outOfPlane(normal + 180, false),
         kit::dot(hub, 20,
                  Paint::radialGradient({0.5f, 0.5f}, 0.71f,
                                        {{0.3f, withAlpha(kPrograde, 0.55f)},
                                         {1.0f, withAlpha(kPrograde, 0.0f)}})),
         kit::dot(hub, 9.5f, Fill::color(hexColor(0x12181C, 0.8f)))
             .stroke(stroke(1.6f, Fill::color(hexColor(0xF0F4F5))))});
  }

  // The navball: a sky-and-ground sphere with its pitch ladder in a silver
  // bezel, the throttle and g-force tapes curved round the bezel, the speed
  // and heading windows above and below, and RCS and SAS on its shoulders.
  static Element bezel(float radius = kBezelRadius) {
    return kit::disc(kBall, radius);
  }

  Element navball() const {
    const Json& ball = words["navball"];
    const Json& attitude = ball["attitude"];
    const float yaw = number(attitude["yaw"]), pitch = number(attitude["pitch"]),
                roll = number(attitude["roll"]);

    // The ladder: rungs either side of the horizon at the height a sphere
    // puts them, turned by the roll and slid by the pitch the sphere under
    // them is drawn at.
    const float middle = kBallRadius;
    std::vector<Element> ladder;
    for (const Json& rung : ball["ladder"].array())
      for (float side : {1.0f, -1.0f}) {
        const float degrees = number(rung);
        const float y =
            middle - side * kBallRadius * std::sin(degrees * 0.0174532925f);
        const float half =
            kBallRadius * (std::fmod(degrees, 30.0f) == 0 ? 0.44f : 0.26f);
        const Utf8 figure = std::to_string((int)degrees);
        ladder.insert(ladder.end(),
                      {kit::at(middle - half, y - 0.9f, half * 2, 1.8f)
                           .fill(hexColor(0xEAF4F8, 0.88f)),
                       text(figure).left(middle - half - 13).top(y - 5),
                       text(figure).left(middle + half + 3).top(y - 5)});
      }
    ladder.push_back(kit::at(middle - kBallRadius * 0.92f, middle - 1.4f,
                             kBallRadius * 1.84f, 2.8f)
                         .fill(hexColor(0xFFFFFF)));

    // A tape is a band of the bezel's annulus, read against eleven ticks
    // with every fifth one long, by a thin wedge of a needle.
    auto band = [](const Json& band) {
      return bezel()
          .shape(shapes::sector(number(band["start"]), number(band["sweep"]),
                                0.845f))
          .fill(colour(band["colour"]))
          .stroke(stroke(0.9f, Fill::color(hexColor(0x2A3034))));
    };
    auto tick = [](int index) {
      const int step = index % 11;
      return bezel()
          .shape(shapes::sector(-0.42f, 0.84f, step % 5 ? 0.86f : 0.79f))
          .fill(hexColor(0xC6CFD3, 0.85f))
          .rotate((index < 11 ? 150.0f : -30.0f) + 6.0f * step);
    };
    auto needle = [](float degrees) {
      return bezel()
          .shape(shapes::sector(-3.0f, 6.0f, 0.80f))
          .fill(hexColor(0xF2F4F5))
          .rotate(degrees);
    };
    auto scale = [](const Json& mark) {
      return bezel(kBezelRadius * 0.94f).children(
          {along(mark["words"], shapes::circle(), number(mark["at"]),
                 number(mark["offset"]))
               .font({.size = number(mark["size"]), .track = 1.1f})
               .ink(colour(mark["colour"]))});
    };
    auto toggle = [](const char* label, material::Color ink, float x) {
      return kit::at(kit::centred(text(label))
                         .borderRadius({3})
                         .fill(ramp(material::lighten(ink, 0.14f), ink))
                         .stroke(keyline(hexColor(0xE8EDEF, 0.5f))),
                     x, kBall.fY - kBezelRadius + 4, 40, 20);
    };
    auto window = [](float centre, float y, float width, float height) {
      return kit::at(kit::centred()
                         .borderRadius({4})
                         .fill(hexColor(0x1C1E20))
                         .stroke(stroke(1.2f, Fill::color(hexColor(0x9AA2A6)))),
                     centre - width * 0.5f, y, width, height);
    };

    return box().inset(0).children(
        {bezel()
             .shape(shapes::circle())
             .fill(Paint::linearGradient({0.15f, 0}, {0.85f, 1},
                                         {{0.0f, hexColor(0xC8CDD0)},
                                          {0.45f, hexColor(0x8B9296)},
                                          {1.0f, hexColor(0x5A6165)}}))
             .stroke(stroke(1.2f, Fill::color(hexColor(0x2A3034)))),
         kit::dot(kBall, kBallRadius + 5, Fill::color(hexColor(0x171B1E))),
         kit::disc(kBall, kBallRadius)
             .fill(material::kit::globe({.sky = hexColor(0x1180AC),
                                         .skyPole = hexColor(0x8ED4E8),
                                         .ground = hexColor(0x8B5A2E),
                                         .groundPole = hexColor(0x5A3A1E),
                                         .yaw = yaw,
                                         .pitch = pitch,
                                         .roll = roll,
                                         .minorWeight = 0.22f})),
         kit::disc(kBall, kBallRadius)
             .shape(shapes::circle())
             .overflow(Overflow::Clip)
             .children({box()
                            .inset(0)
                            .rotate(-roll * 57.2957795f)
                            .translateY(pitch * kBallRadius)
                            .font({.face = sans(700), .size = 7})
                            .ink(hexColor(0xEAF4F8, 0.9f))
                            .children(ladder)}),
         each(ball["bands"].array(), band), each(22, tick),
         needle(207.0f - 59.0f * number(ball["throttle"])),
         needle(28.0f - 61.0f * number(ball["gforce"])),
         each(ball["scale"].array(), scale),
         // the heading letters inside the bezel turn with the ball
         kit::disc(kBall, kBallRadius * 0.83f)
             .rotate(-yaw * 14.3239449f)
             .font({.face = sans(700), .size = 9, .track = 0.6f})
             .ink(hexColor(0xEAF4F8, 0.85f))
             .children({each(ball["compass"].array(),
                             [](const Json& letter, std::size_t index) {
                               return along(letter, shapes::circle(),
                                            0.75f + 0.25f * index, 2);
                             })}),
         // the gold level mark holds still while the ball turns under it
         kit::at(kBall.fX - 46, kBall.fY - 13, 92, 26)
             .shape(shapes::chevron(0.20f, 0.34f, 0.16f, 0.20f))
             .fill(kGold),
         window(kBall.fX, kBall.fY - kBezelRadius - 6, 136, 38)
             .column()
             .children({text(ball["mode"]).styleClass("lcd").font({.size = 11}),
                        text(ball["speed"])
                            .styleClass("lcd")
                            .ink(hexColor(0xDCEEF2))}),
         window(kBall.fX, kBall.fY + kBezelRadius - 8, 104, 24)
             .row()
             .gap(6)
             .children(
                 {text("HDG").font({.size = 10}).ink(hexColor(0xA9B4B8)),
                  text(ball["heading"]).styleClass("lcd").font({.size = 12})}),
         kit::disc({kBall.fX, kBall.fY - kBezelRadius - 4}, 8)
             .height(10)
             .shape(shapes::polygon(3, 180))
             .fill(hexColor(0xD7DDE0)),
         box()
             .inset(0)
             .font({.face = sans(700), .size = 10})
             .children({toggle("RCS", hexColor(0x73AC43),
                               kBall.fX - kBezelRadius - 6),
                        toggle("SAS", hexColor(0x77A9B0),
                               kBall.fX + kBezelRadius - 34)}),
         // the node's Δv, as an arc outside the bezel and a tag beside it
         bezel(kBezelRadius + 16)
             .shape(shapes::arc(-72, 144))
             .stroke(spans::upTo(0.5f), brush::presets::filament(
                                            withAlpha(hexColor(0x7FE33F), 0.5f),
                                            hexColor(0xEBFFDA), 0.5f)),
         kit::at(
             plate(hexColor(0xA0A6AA), hexColor(0x6E767B), 3)
                 .row()
                 .alignItems(Align::Center)
                 .justifyContent(Justify::Center)
                 .gap(5)
                 .children({text(words["burn"]["tag"]).ink(hexColor(0x14181A)),
                            kit::centred(text("×").font({.face = sans(700)}))
                                .width(13)
                                .height(13)
                                .borderRadius({2})
                                .fill(kStageTab)}),
             kBall.fX + kBezelRadius + 18, kBall.fY - kBezelRadius - 2, 92,
             20)});
  }

  // The staging stack down the left edge: each stage's striped tab, then its
  // parts, a fuel tank's gauge carrying its name inside the tank; under it
  // the STAGE button and the roll and yaw tapes.
  Element staging() const {
    auto part = [](const Json& entry) {
      Element row = box().row().gap(10).alignItems(Align::Center).children(
          {plate(hexColor(0x767F86), hexColor(0x333A3F), 2)
               .width(27)
               .height(27)
               .children({kit::centred(text(entry["glyph"]).font({.size = 14}))
                              .inset(0),
                          text(entry["count"])
                              .font({.face = sans(700), .size = 8})
                              .ink(hexColor(0xF6D488))
                              .right(1)
                              .bottom(0)})});
      if (!entry["fuel"].null())
        row.children(
            {box()
                 .width(96)
                 .height(13)
                 .fill(hexColor(0x14181B))
                 .stroke(keyline(hexColor(0x3B4147)))
                 .children({kit::at(1, 1, 94 * number(entry["fuel"]), 11)
                                .fill(ramp(hexColor(0x7A8214), hexColor(0x666C0A))),
                            kit::centred(text("LiquidFuel").font({.size = 9}))
                                .inset(0)})});
      return row;
    };
    auto stage = [&part](const Json& stage) {
      return box().column().gap(6).children(
          {kit::centred(text(stage["number"]).font({.face = sans(700),
                                                    .size = 13}))
               .width(58)
               .height(25)
               .borderRadius({2})
               .fill(kStageTab)
               .foreground(lines::presets::hatch(
                   Fill::color(hexColor(0x101010, 0.45f)), 8.0f, 3.4f, -45.0f)),
           box().column().gap(4).paddingLeft(4).children(
               {each(stage["parts"].array(), part)})});
    };
    auto digit = [](const Json& figure) {
      return kit::centred(text(figure).styleClass("lcd").ink(hexColor(0x16181A)))
          .width(13)
          .height(17)
          .fill(ramp(hexColor(0xF2F2F2), hexColor(0xBFBFBF)));
    };
    // A tape is a ruled track and an orange caret riding it.
    auto tape = [](const char* label, float reading, float y) {
      return kit::at(
          box()
              .borderRadius({2})
              .fill(hexColor(0x1B1F22))
              .stroke(keyline(hexColor(0x454C51)))
              .children(
                  {kit::at(kit::ladder({.count = 16,
                                        .pitch = 6,
                                        .thickness = 0.8f,
                                        .column = true,
                                        .fill = Fill::color(hexColor(0x6C767C))}),
                           4, 4, 98, 8),
                   text(label).ink(hexColor(0xC7D0D5)).left(4).top(1),
                   kit::at(6 + 84 * reading, 0, 9, 8)
                       .shape(shapes::polygon(3, 180))
                       .fill(kStageTab)}),
          168, y, 106, 16);
    };
    const Json& panel = words["stage"];
    return box()
        .inset(0)
        .ink(hexColor(0xF0F3F0))
        .children(
            {kit::at(0, 512, 528, 288)
                 .fill(Paint::linearGradient(
                     {0, 0}, {0, 1},
                     {{0.0f, hexColor(0x0A0C10, 0.30f)},
                      {0.35f, hexColor(0x0A0C10, 0.62f)},
                      {1.0f, hexColor(0x0A0C10, 0.74f)}})),
             kit::at(14, 528, 150, 220)
                 .column()
                 .gap(8)
                 .children({each(words["stages"].array(), stage)}),
             kit::at(8, 756, 152, 38)
                 .column()
                 .borderRadius({3})
                 .overflow(Overflow::Clip)
                 .fill(hexColor(0x2A2E31))
                 .stroke(keyline(hexColor(0x4A5157)))
                 .children(
                     {hazard().height(9),
                      box()
                          .row()
                          .flexGrow(1)
                          .gap(5)
                          .padding(0, 6)
                          .alignItems(Align::Center)
                          .children({box()
                                         .width(15)
                                         .height(15)
                                         .shape(shapes::circle())
                                         .fill(lit(hexColor(0xE6FDD1),
                                                   hexColor(0x4CAF50),
                                                   hexColor(0x2E6E33))),
                                     text("STAGE").font(
                                         {.face = sans(700), .size = 9}),
                                     box().flexGrow(1),
                                     box().row().gap(2).children({each(
                                         panel["digits"].array(), digit)})})}),
             box()
                 .inset(0)
                 .font({.face = sans(700), .size = 8})
                 .children({tape("ROLL", number(panel["roll"]), 756),
                            tape("YAW", number(panel["yaw"]), 778)})});
  }

  // The altimeter at the top: a hazard cheek, the odometer ending in a red
  // kilometre cell, the blue atmosphere tape, and the vertical-speed dial.
  Element altimeter() const {
    const Json& gauge = words["altimeter"];
    auto wheel = [](const Json& figure, float x, bool kilometres) {
      return kit::at(
          kit::centred(text(figure).styleClass("lcd").font({.size = 20}).ink(
                           kilometres ? hexColor(0xFFFFFF) : hexColor(0x101214)))
              .fill(kilometres ? ramp(hexColor(0xE05B4A), hexColor(0x8E2A20))
                               : ramp(hexColor(0xFFFFFF), hexColor(0xBFBFBF)))
              .stroke(keyline(hexColor(0x50585E))),
          x, 8, 26, 34);
    };
    const SkPoint dial{300, 42};
    // the dial's name over two lines, and the two bounds it reads between
    const std::array<SkPoint, 4> dialWords{{{dial.fX + 15, dial.fY - 6},
                                            {dial.fX + 15, dial.fY + 3},
                                            {dial.fX - 5, dial.fY - 24},
                                            {dial.fX - 5, dial.fY + 24}}};
    return kit::at(
        plate(hexColor(0xA8AFB4), hexColor(0x4E565C))
            .overflow(Overflow::Clip)
            .children(
                {kit::at(hazard(), 0, 0, 11, 82),
                 each(gauge["digits"].array(),
                      [&wheel](const Json& figure, std::size_t index) {
                        return wheel(figure, 18.0f + 28.0f * index, false);
                      }),
                 wheel(gauge["suffix"], 18.0f + 28.0f * 6, true),
                 kit::at(18, 48, 238, 22)
                     .fill(Paint::linearGradient({0, 0}, {0, 1},
                                                 {{0.0f, hexColor(0x2E6E9E)},
                                                  {0.5f, hexColor(0x4E9CC8)},
                                                  {1.0f, hexColor(0x1E4E72)}}))
                     .stroke(keyline(hexColor(0x18333F)))
                     .children(
                         {kit::at(kit::ladder({.count = 46,
                                               .pitch = 5,
                                               .thickness = 0.9f,
                                               .column = true,
                                               .fill = Fill::color(
                                                   hexColor(0xE8F4FA, 0.85f))}),
                                  2, 8, 234, 12),
                          text(gauge["tape"])
                              .font(
                                  {.face = sans(700), .size = 8, .track = 1.4f})
                              .ink(hexColor(0xEAF4FA))
                              .left(6)
                              .top(1),
                          kit::at(30 + 190 * number(gauge["depth"]), 0, 9, 8)
                              .shape(shapes::polygon(3, 180))
                              .fill(hexColor(0xFFFFFF))}),
                 kit::disc(dial, 37)
                     .shape(shapes::circle())
                     .fill(Paint::radialGradient({0.4f, 0.32f}, 1.0f,
                                                 {{0.0f, hexColor(0xF2F4F5)},
                                                  {0.7f, hexColor(0xD3D8DB)},
                                                  {1.0f, hexColor(0x9AA2A7)}}))
                     .stroke(stroke(1.4f, Fill::color(hexColor(0x33393E)))),
                 each(13,
                      [dial](int index) {
                        return kit::disc(dial, 34)
                            .shape(shapes::sector(-1.1f, 2.2f,
                                                  index % 3 ? 0.82f : 0.72f))
                            .fill(hexColor(0x3A4046))
                            .rotate(-125.0f + 20.8f * index);
                      }),
                 box()
                     .inset(0)
                     .ink(hexColor(0x4A5157))
                     .children({each(
                         gauge["dial"].array(),
                         [&dialWords](const Json& word, std::size_t index) {
                           return text(word)
                               .font({.face = sans(index < 2 ? 700 : 400),
                                      .size = index < 2 ? 6.5f : 6.0f})
                               .centerAt(dialWords[index]);
                         })}),
                 kit::disc(dial, 31)
                     .shape(shapes::sector(-2.2f, 4.4f))
                     .fill(kGold)
                     .rotate(-118.0f + 236.0f * number(gauge["verticalSpeed"])),
                 kit::dot(dial, 3.5f, Fill::color(hexColor(0x33393E)))}),
        430, 6, 356, 82);
  }

  // The vessel card: an orange title, a tab, and strips of readings whose
  // names are set in the card ink and whose figures are orange.
  Element vesselCard() const {
    const Json& card = words["info"];
    auto readings = [](const Json& rows) {
      std::vector<kit::Reading> out;
      for (const Json& row : rows.array())
        out.push_back({.name = row["name"], .value = row["value"]});
      return out;
    };
    // A strip runs the card's full width, out through its side padding.
    auto strip = [](material::Color ground, float height) {
      return box()
          .height(height)
          .justifyContent(Justify::Center)
          .margin(0, -8)
          .padding(0, 8)
          .fill(ground);
    };
    auto head = [&strip](const Json& label) {
      if (label.string().empty()) return box();
      return strip(hexColor(0xF4F4F5), 19).children({kit::section(label)});
    };
    auto title = [&strip](const Json& label, material::Color ground,
                          float size) {
      return strip(ground, size + 12).children(
          {text(label).font({.face = sans(700), .size = size}).ink(
              hexColor(0xF4F4F5))});
    };
    return kit::at(906, 40, 240, 352)
        .ink(hexColor(0x2A2A2C))
        .fill(hexColor(0xE4E5E7))
        .overflow(Overflow::Clip)
        .padding(0, 8)
        .gap(1)
        .children(
            {title(card["vessel"], kOrange, 14),
             title(card["tab"], hexColor(0x3A3A3E), 11),
             head(card["classification"]),
             box()
                 .row()
                 .gap(8)
                 .alignItems(Align::Center)
                 .children({box()
                                .width(34)
                                .height(40)
                                .shape(shapes::polygon(7, 12))
                                .fill(Paint::linearGradient(
                                    {0, 0}, {1, 1},
                                    {{0.0f, hexColor(0xF7F7F8)},
                                     {1.0f, hexColor(0xB9BCC1)}}))
                                .stroke(stroke(
                                    1.0f, Fill::color(hexColor(0x8A8E93)))),
                            kit::readout(readings(card["ship"]),
                                         {.measure = 182, .gap = 5})
                                .flexGrow(1)}),
             each(card["sections"].array(),
                  [&head, &readings](const Json& section) {
                    return box().children(
                        {head(section["head"]),
                         kit::readout(readings(section["rows"]),
                                      {.measure = 224, .gap = 5})});
                  }),
             box().flexGrow(1),
             box().height(6).margin(0, -8).fill(hexColor(0x9DA1A6))});
  }

  // The mission clock and its buttons, the toolbar down the right edge and
  // the Δv card.
  Element chrome() const {
    const Json& burn = words["burn"];
    auto button = [](const Json& glyph, std::size_t index) {
      return kit::at(kit::centred(text(glyph).font({.size = 13}))
                         .borderRadius({5})
                         .fill(ramp(hexColor(0x6E7B85), hexColor(0x3E4750)))
                         .stroke(keyline()),
                     1156, 34.0f + 46.0f * index, 38, 38);
    };
    auto clockIcon = [](const Json& glyph, std::size_t index) {
      return kit::at(kit::centred(text(glyph).font({.size = 10}))
                         .borderRadius({3})
                         .fill(hexColor(0x474F57)),
                     272.0f + 28.0f * index, 16, 24, 24);
    };
    return box().inset(0).ink(hexColor(0xD3DBE0)).children(
        {kit::at(kit::centred(text(words["clock"]).styleClass("lcd")), 18, 14,
                 200, 28)
             .borderRadius({4})
             .fill(hexColor(0x26282C, 0.94f))
             .stroke(stroke(1.0f, Fill::color(hexColor(0x4A5157)))),
         kit::at(kit::centred(text("MET").font({.face = sans(700)})), 222, 14,
                 40, 28)
             .borderRadius({4})
             .fill(ramp(hexColor(0x707E89), hexColor(0x3E4750))),
         box().inset(0).ink(hexColor(0x8CE07A)).children(
             {each(words["clockIcons"].array(), clockIcon)}),
         each(words["toolbar"].array(), button),
         kit::at(646, 566, 190, 88)
             .column()
             .padding(8, 9)
             .gap(3)
             .fill(hexColor(0x12181C, 0.86f))
             .stroke(stroke(1.0f, Fill::color(hexColor(0x3A4148))))
             .font({.size = 10.5f})
             .ink(hexColor(0xE3D24A))
             .children(
                 {box().row().gap(6).alignItems(Align::Baseline).children(
                      {text("Δv").ink(hexColor(0x9AA4AA)),
                       text(burn["deltaV"]).styleClass("lcd").font({.size = 16}),
                       text("m/s").font({.size = 10}).ink(withAlpha(kLcd, 0.8f))}),
                  text(burn["lines"][0]), text(burn["lines"][1]),
                  kit::line({.fill = Fill::color(hexColor(0x2C3238))})
                      .marginTop(2)})});
  }

  // The crew portrait: the composition of the game's plate — a helmet, a
  // green face with two wide eyes under the visor, the suit's shoulders and
  // a name bar — not the character art.
  Element crew() const {
    auto eye = [](float x) {
      return box().inset(0).children(
          {kit::dot({x, 70.5f}, 7.5f, Fill::color(hexColor(0xF4F4F0))),
           kit::dot({x, 71.5f}, 3.2f, Fill::color(hexColor(0x141414)))});
    };
    return kit::at(986, 594, 178, 186)
        .borderRadius({3})
        .fill(ramp(hexColor(0x7F878C), hexColor(0x454D53)))
        .stroke(keyline())
        .overflow(Overflow::Clip)
        .children(
            {kit::at(5, 5, 168, 152)
                 .fill(Paint::radialGradient(
                     {0.5f, 0.35f}, 1.1f,
                     {{0.0f, hexColor(0x3E4A52)}, {1.0f, hexColor(0x1A2126)}})),
             kit::at(40, 124, 104, 40)
                 .borderRadius({26})
                 .fill(ramp(hexColor(0xE7E8E4), hexColor(0x9AA0A2))),
             kit::dot({92, 80}, 46,
                      lit(hexColor(0xFFFFFF), hexColor(0xD3D8DB),
                          hexColor(0x7C858B))),
             kit::dot({92, 80}, 32,
                      lit(hexColor(0x9FC45C), hexColor(0x7FA446),
                          hexColor(0x5F8330))),
             eye(81), eye(103),
             kit::disc({92, 91}, 12)
                 .shape(shapes::sector(20, 140))
                 .fill(hexColor(0x2E3A18)),
             kit::disc({92, 80}, 38)
                 .shape(shapes::sector(150, 240))
                 .fill(
                     Paint::linearGradient({0, 0}, {1, 1},
                                           {{0.0f, hexColor(0xBFE0D8, 0.34f)},
                                            {0.55f, hexColor(0x6E9A94, 0.10f)},
                                            {1.0f, hexColor(0x2E4A46, 0.26f)}}))
                 .stroke(stroke(1.4f, Fill::color(hexColor(0xE8ECEA)))),
             kit::at(kit::centred(text(words["crew"]).ink(hexColor(0x14181A))),
                     5, 160, 168, 21)
                 .borderRadius({2})
                 .fill(ramp(hexColor(0x9AA2A7), hexColor(0x666E74)))});
  }

  void setup(sketch::SketchContext& context) {
    sketch::kit::stage(
        context, {.size = {1200, 800}, .captureAt = 6.0, .background = kSpace});
    map = sketch::kit::Document(context, "data/orbits.json");
    words = sketch::kit::Document(context, "data/content.json");
    kerbin = {number(map["kerbin"]["centre"][0]),
              number(map["kerbin"]["centre"][1])};
    kerbinRadius = number(map["kerbin"]["radius"]);
    context.ticker.add([this, seconds = 0.0](double step) mutable {
      seconds += step;
      clock = (float)seconds;
      return true;
    });

    // The card's row names, figures and strip heads, and the green
    // monospaced readout every instrument window uses.
    const StyleSheet look{
        rule("caption, .caption").font({.face = sans(), .size = 11}),
        rule(".readout").font({.face = sans(700), .size = 11}).ink(kOrange),
        rule("h2").font({.face = sans(700), .size = 11, .track = 0.2f})
            .ink(kOrange),
        rule(".lcd")
            .font({.face = sketch::kit::houseFace(sketch::kit::Voice::Terminal),
                   .size = 13})
            .ink(kLcd)};

    context.composer.render(
        box()
            .inset(0)
            .font({.face = sans(), .size = 11})
            .ink(hexColor(0xFFFFFF))
            .applyStyleSheet(look)
            .children({space(), planet(), trajectories(), markers(),
                       manoeuvre(), staging(), navball(), altimeter(),
                       vesselCard(), chrome(), crew(),
                       // the lens's falloff toward the corners, over everything
                       box().inset(0).fill(Paint::radialGradient(
                           {0.5f, 0.5f}, 1.0f,
                           {{0.50f, hexColor(0x000000, 0.0f)},
                            {1.0f, hexColor(0x000000, 0.30f)}}))}));
  }
};

}  // namespace

SIGIL_SKETCH(KspMapView, "Study · Game UI",
             "Kerbal Space Program's map view — three conics about Kerbin, "
             "their markers, the manoeuvre node and the navball")
