// Charles Joseph Minard's carte figurative of 1869: the French army's
// Russian campaign of 1812 drawn as a band whose width is the number of
// men still marching — pale buff east from the Niemen to Moscow, black
// back again — with the cold of the retreat graphed beneath it.
//
// Here the sheet is a lithograph on aged paper, and the campaign runs as
// time. The buff band grows eastward day by day and each strength is
// written across it as the band passes; the band holds at Moscow through
// the five weeks there; then the black band comes back while the
// thermometer's curve draws itself beneath it, each reading hung from the
// retreat by a line that drops as the cold arrives. The date and the
// army's number run in the corner, and at the Berezina a note says what
// the crossing cost. The finished sheet stands, fades, and begins again.
//
// THE RECORD, in data/: every strength, place, river, reading and line of
// the legend is Minard's. THIS STUDY'S OWN: the day each station is
// reached (the `day` columns, counted from the crossing of the Niemen on
// 24 June 1812, from the campaign's dates where the sheet gives none), the
// timing, the paper and its deckle.
//
// One texture holds the sheet — paper, frame, lettering, rivers, towns,
// the graph's rules — and only what the campaign draws is painted over it.

// TAGS: Data/Charts

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/table/Table.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace draw = sigil::draw;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using material::Color;
using material::hexColor;

namespace {

// The sheet is about twice as wide as it is tall, as Minard's is.
constexpr float kWidth = 2560, kHeight = 1240;

const Color kGround = hexColor(0x2b241d);  // the board the sheet lies on
const Color kPaper = hexColor(0xe9dcc0);
const Color kInk = hexColor(0x201a14);
const Color kHairline = hexColor(0x5b4c3a);
// Minard calls the advance "le rouge"; the lithograph lays it as a pale
// buff a shade off the paper, so the black retreat reads as the figure.
const Color kAdvance = hexColor(0xd6b27c);
const Color kRetreat = hexColor(0x17120e);

// The map is a plain equirectangular sketch, stretched in latitude a
// little as the sheet stretches the narrow band of Russia it crosses.
float mapX(float longitude) { return 150 + (longitude - 23.6f) * 160; }
float mapY(float latitude) { return 300 + (56.0f - latitude) * 232; }

// The legend's millimetre for every ten thousand men, at this sheet's width.
float zoneWidth(float men) { return men * 4.2f / 10000; }

// The graph's degrees Réaumur below zero, zero at the top.
constexpr float kGraphTop = 942;
float graphY(float reaumur) { return kGraphTop - reaumur * 6.6f; }

// THE CAMPAIGN AS TIME. Seconds of the loop against days of the campaign:
// the advance takes twelve seconds, the five weeks in Moscow two and a
// half, the retreat fourteen; the finished sheet then stands until the
// loop fades it and begins again.
struct Knot {
  float seconds, day;
};
constexpr std::array<Knot, 4> kTimeline{{{1.5f, 0}, {13.5f, 82}, {16, 117}, {30, 173}}};
constexpr float kLoopLength = 38, kFadeFrom = 36.8f;
constexpr float kBerezina = 155;  // the day the army stands at Studienska

float campaignDay(float seconds) {
  if (seconds <= kTimeline.front().seconds) return seconds - kTimeline.front().seconds;
  for (size_t index = 1; index < kTimeline.size(); ++index)
    if (seconds < kTimeline[index].seconds) {
      const Knot& from = kTimeline[index - 1];
      const Knot& to = kTimeline[index];
      return from.day + (to.day - from.day) * (seconds - from.seconds) / (to.seconds - from.seconds);
    }
  return kTimeline.back().day;
}

/** A campaign day as Minard dates his readings: the day of the month and
 *  the month, the autumn ones written 7bre, 8bre, 9bre and Xbre. */
std::string frenchDate(float day) {
  struct Month {
    const char* name;
    int length;
  };
  static constexpr std::array<Month, 7> kMonths{{{"Juin", 30},
                                                 {"Juillet", 31},
                                                 {"Août", 31},
                                                 {"7bre", 30},
                                                 {"8bre", 31},
                                                 {"9bre", 30},
                                                 {"Xbre", 31}}};
  int date = 24 + std::max(0, (int)std::floor(day));
  for (const Month& month : kMonths) {
    if (date <= month.length) return std::to_string(date) + " " + month.name + " 1812";
    date -= month.length;
  }
  return "1813";
}

/** The French thousands point the plate engraves: 422.000. */
std::string french(float men) {
  std::string digits = std::to_string(std::lround(men));
  for (int at = (int)digits.size() - 3; at > 0; at -= 3) digits.insert((size_t)at, ".");
  return digits;
}

/** A LINE DRAWN IN TIME: its points on the sheet, the day each is reached,
 *  and how far along it the pen stands on any day between. */
struct Course {
  std::vector<glm::vec2> points;
  std::vector<float> days, along;

  void add(glm::vec2 point, float day) {
    along.push_back(points.empty() ? 0 : along.back() + glm::distance(points.back(), point));
    points.push_back(point);
    days.push_back(day);
  }
  float length() const { return along.empty() ? 0 : along.back(); }
  /** The fraction of the course drawn by @p day. */
  float drawnBy(float day) const {
    if (points.size() < 2 || day <= days.front()) return 0;
    for (size_t index = 1; index < points.size(); ++index)
      if (day < days[index]) {
        const float span = days[index] - days[index - 1];
        const float share = span > 0 ? (day - days[index - 1]) / span : 1;
        return (along[index - 1] + share * (along[index] - along[index - 1])) / length();
      }
    return 1;
  }
  path::Outline outline() const { return path::toPath(path::Polyline{.points = points}); }
};

/** One zone of the march: its course, the men it carries onward from each
 *  station, and how much of it the campaign has drawn so far. */
struct Zone {
  std::string name;
  Course course;
  std::vector<float> men;
  motion::Animatable<float> drawn = motion::animatable(0.0f);
  bool retreat() const { return name.starts_with("retreat"); }
};

/** How the sheet is lettered: a Didone for the title and the figures, its
 *  italic for the legend, an engraver's round script for the places and
 *  the rivers, and spaced capitals for MOSCOU alone. */
StyleSheet engraving() {
  return StyleSheet{
      rule(":root")
          .var("ink", kInk)
          .var("hairline", kHairline)
          .var("advance", kAdvance)
          .var("retreat", kRetreat)
          .fontFamily("Didot, Bodoni 72, Baskerville, serif")
          .fontStyle(FontStyle::Italic)
          .fontSize(17)
          .ink(var("ink")),
      rule("title").fontStyle(FontStyle::Normal).fontSize(38).letterSpacing(0.6),
      rule("byline").fontSize(20),
      rule("numeral").fontStyle(FontStyle::Normal).fontSize(19).letterSpacing(0.4),
      rule("reading").fontStyle(FontStyle::Normal).fontSize(16),
      rule("place, river, note")
          .fontFamily("Snell Roundhand, Apple Chancery, Baskerville, serif")
          .fontStyle(FontStyle::Normal),
      rule("place").fontSize(23),
      rule("river").fontSize(19).ink(var("hairline")),
      rule("note").fontSize(30),
      rule("capital").fontStyle(FontStyle::Normal).fontSize(28).letterSpacing(7),
      rule("imprint").fontSize(13).ink(var("hairline")),
  };
}

Element engravedLine(glm::vec2 from, glm::vec2 to, float weight, const char* ink = "hairline") {
  return pathFigure(path::toPath(path::Polyline{.points = {from, to}}), weight)
      .stroke(stroke(weight, Fill::var(ink)));
}

/** The paper's edge, torn rather than cut: a rectangle whose sides wander
 *  a few pixels in and out. */
path::Outline deckle(float inset) {
  path::Polyline edge{.closed = true};
  const glm::vec2 corners[] = {{inset, inset},
                               {kWidth - inset, inset},
                               {kWidth - inset, kHeight - inset},
                               {inset, kHeight - inset}};
  uint32_t step = 0;
  for (int side = 0; side < 4; ++side) {
    const glm::vec2 from = corners[side], to = corners[(side + 1) % 4];
    const glm::vec2 inward = glm::normalize(glm::vec2{from.y - to.y, to.x - from.x});
    const int count = (int)(glm::distance(from, to) / 7);
    for (int index = 0; index < count; ++index, ++step) {
      const float wander = 2.2f * sigil::core::noise::hash(1869u, step) +
                           1.6f * std::sin((float)step * 0.07f) + 2.5f;
      edge.points.push_back(glm::mix(from, to, (float)index / (float)count) + inward * wander);
    }
  }
  return path::toPath(edge);
}

}  // namespace

struct Minard1869 {
  std::vector<Zone> zones;  // march.csv, one per group, in file order
  Course cold;              // the thermometer's curve, reading by reading
  motion::Animatable<float> coldDrawn = motion::animatable(0.0f);
  /** THE ONE CLOCK: seconds into the loop, and the campaign day they are. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);
  motion::Animatable<float> day = motion::animatable(-1.0f);
  weave::Type almanacDate, almanacCount;

  template <class Read>
  static void rows(sketch::SketchContext& context, const char* file, Read read) {
    if (const auto table = context.assets.hub().load<sigil::data::Table>(
            context.local(std::string("data/") + file)))
      read(*table);
  }

  /** On from @p from, fully by @p to, in campaign days. */
  motion::Animatable<float> after(float from, float to) const {
    return motion::bind(day, {.from = {from, std::max(to, from + 0.01f)}, .clampFrom = true});
  }

  /** Where the retreat stands at a longitude: the droplines hang from it. */
  float retreatY(float longitude) const {
    std::vector<glm::vec2> route;
    for (const Zone& zone : zones)
      if (zone.name == "retreat east" || zone.name == "retreat west")
        route.insert(route.end(), zone.course.points.begin(), zone.course.points.end());
    const float x = mapX(longitude);
    for (size_t index = 1; index < route.size(); ++index) {
      const glm::vec2 east = route[index - 1], west = route[index];
      if (x <= east.x && x >= west.x)
        return east.x == west.x ? east.y : glm::mix(east.y, west.y, (east.x - x) / (east.x - west.x));
    }
    return mapY(54.4f);
  }

  /** The men at the head of the army on @p today: the main column east,
   *  then the retreat home. */
  float headCount(float today) const {
    float men = zones.empty() ? 0 : zones.front().men.front();
    for (const Zone& zone : zones)
      if (zone.name == "advance trunk" || zone.name == "retreat east" || zone.name == "retreat west")
        for (size_t index = 0; index < zone.men.size(); ++index)
          if (zone.course.days[index] <= today) men = zone.men[index];
    return men;
  }

  // ---- The sheet: everything that stands still, in one texture ----------

  Element paper() const {
    return box().inset(0).fill(kGround).children({
        pathFigure(deckle(14))
            .fill(material::grained(kPaper, 0.10f, 0.06f))
            .filter(material::Filter::dropShadow(hexColor(0x000000, 0.55f),
                                                 {.blur = 14, .offset = {0, 6}})),
        // Age: a slow mottle and a browning toward the edges, both laid
        // into the paper by multiplying.
        kit::at(box()
                    .fill(material::noise(0.004f, {.octaves = 3, .seed = 12, .grain = true,
                                                   .contrast = 0.5f}))
                    .blendMode(material::BlendMode::Multiply)
                    .opacity(0.16f),
                18, 18, kWidth - 36, kHeight - 36),
        kit::at(box()
                    .fill(material::radialGradient({(kWidth - 36) / 2, (kHeight - 36) / 2}, kWidth * 0.62f,
                                                   {{0.55f, hexColor(0xffffff)},
                                                    {1.00f, hexColor(0xc9a877)}},
                                                   {.units = material::GradientUnits::Pixels}))
                    .blendMode(material::BlendMode::Multiply)
                    .opacity(0.7f),
                18, 18, kWidth - 36, kHeight - 36),
        // The plate's tone: where the stone was inked, a breath darker.
        kit::at(box().fill(hexColor(0xe2cfa9)).blendMode(material::BlendMode::Multiply).opacity(0.35f),
                40, 40, kWidth - 80, kHeight - 80),
    });
  }

  Element printed(sketch::SketchContext& context) const {
    std::vector<Element> ink;
    // The printed frame: a heavy rule outside a fine one.
    ink.push_back(kit::at(box().foreground(decorations::border(2.6f, Fill::var("ink"))),
                          40, 40, kWidth - 80, kHeight - 80));
    ink.push_back(kit::at(box().foreground(decorations::border(0.8f, Fill::var("ink"))),
                          50, 50, kWidth - 100, kHeight - 100));

    std::vector<Element> legend;
    rows(context, "legend.csv", [&](const auto& table) {
      for (const std::string& line : table.template column<std::string>("text"))
        legend.push_back(text(line));
    });
    ink.push_back(box().column().alignItems(Align::Center).left(0).width(kWidth).top(72).gap(6).children({
        text("Carte Figurative des pertes successives en hommes de l'Armée "
             "Française dans la campagne de Russie 1812–1813.")
            .role("title"),
        text("Dressée par M. Minard, Inspecteur Général des Ponts et Chaussées "
             "en retraite.          Paris, le 20 Novembre 1869.")
            .role("byline"),
        box().column().alignItems(Align::Center).gap(2).marginTop(6).children(std::move(legend)),
    }));

    rows(context, "rivers.csv", [&](const auto& table) {
      const auto name = table.template column<std::string>("river");
      const auto longitude = table.template column<double>("lon");
      const auto latitude = table.template column<double>("lat");
      path::Polyline course;
      for (size_t row = 0; row < name.size(); ++row) {
        course.points.push_back({mapX((float)longitude[row]), mapY((float)latitude[row])});
        if (row + 1 < name.size() && name[row + 1] == name[row]) continue;
        const glm::vec2 mouth = course.points.back();
        ink.push_back(pathFigure(path::smoothThrough(course), 1)
                          .stroke(stroke(1.2f, Fill::var("hairline"))));
        ink.push_back(text(name[row]).role("river").left(mouth.x - 70).top(mouth.y + 4));
        course.points.clear();
      }
    });

    rows(context, "cities.csv", [&](const auto& table) {
      const auto name = table.template column<std::string>("name");
      const auto longitude = table.template column<double>("lon");
      const auto latitude = table.template column<double>("lat");
      const auto dx = table.template column<double>("dx");
      const auto dy = table.template column<double>("dy");
      for (size_t row = 0; row < name.size(); ++row) {
        const bool moscow = name[row] == "Moscou";
        ink.push_back(text(moscow ? std::string("MOSCOU") : name[row])
                          .role(moscow ? "capital" : "place")
                          .left(mapX((float)longitude[row]) + (float)dx[row])
                          .top(mapY((float)latitude[row]) + (float)dy[row]));
      }
    });

    // The scale of distance: common French leagues, graduated every five
    // to twenty-five and numbered again only at fifty.
    {
      constexpr float kPixelsPerLeague = 6.0f;
      const glm::vec2 origin{mapX(34.9f), mapY(54.05f)};
      ink.push_back(engravedLine(origin, origin + glm::vec2{50 * kPixelsPerLeague, 0}, 1.2f, "ink"));
      for (int league : {0, 5, 10, 15, 20, 25, 50}) {
        const glm::vec2 tick = origin + glm::vec2{(float)league * kPixelsPerLeague, 0};
        ink.push_back(engravedLine(tick - glm::vec2{0, 7}, tick, 1.0f, "ink"));
        ink.push_back(text(std::to_string(league)).role("reading").fontSize(12).centerAt({tick.x, tick.y + 12}));
      }
      ink.push_back(text("Lieues communes de France (Carte de M. de Fezensac)")
                        .fontSize(15)
                        .left(origin.x)
                        .top(origin.y - 30));
    }

    // The graph's rules, a heavier one every ten degrees.
    for (int degrees = 0; degrees <= 30; degrees += 5) {
      const float y = graphY((float)-degrees);
      ink.push_back(engravedLine({mapX(24.4f), y}, {mapX(38.0f), y}, degrees % 10 ? 0.5f : 0.9f));
      ink.push_back(text(degrees == 30 ? "-30 degrés" : degrees == 0 ? "Zéro" : "-" + std::to_string(degrees))
                        .role("reading")
                        .fontSize(14)
                        .left(mapX(38.0f) + 10)
                        .top(y - 9));
    }
    ink.push_back(text("TABLEAU GRAPHIQUE de la température en degrés du thermomètre "
                       "de Réaumur au dessous de zéro.")
                      .fontSize(19)
                      .left(mapX(26.2f))
                      .top(kGraphTop - 62));
    ink.push_back(text("Les Cosaques passent au galop\nle Niémen gelé.").fontSize(15).left(74).top(graphY(-21)));
    ink.push_back(text("Autog. par Regnier, 8. Pas. Sᵗᵉ Marie Sᵗ Gᵃᵉᵐ à Paris.")
                      .role("imprint")
                      .left(60)
                      .top(kHeight - 42));
    ink.push_back(text("Imp. Lith. Regnier et Dourdet").role("imprint").left(kWidth - 250).top(kHeight - 42));

    // The ink sat a moment in the paper before it dried.
    return box().inset(0).filter(material::Filter::blur(0.45f)).children(std::move(ink));
  }

  // ---- The campaign: what the days draw over the sheet --------------------

  /** ONE ZONE: its course filled to a width that steps at each station to
   *  the men it carries onward, drawn as far as the campaign has reached.
   *  The width is keyed in pixels of the course, so it stays where it
   *  belongs as the band lengthens. */
  static Element band(const Zone& zone) {
    std::vector<path::Stop> widths;
    float widest = 0;
    for (size_t index = 0; index < zone.men.size(); ++index) {
      widths.push_back({zone.course.along[index], zoneWidth(zone.men[index])});
      widest = std::max(widest, zoneWidth(zone.men[index]));
    }
    brush::Ribbon ribbon{.fill = Fill::var(zone.retreat() ? "retreat" : "advance")};
    ribbon.width = path::Profile(widths, {.between = path::Between::Step, .inPixels = true});
    ribbon.join = path::Join::Miter;
    ribbon.miterLimit = 1.6f;
    ribbon.step = 1.5f;
    return pathFigure(zone.course.outline(), widest).stroke(spans::upTo(zone.drawn), ribbon);
  }

  /** A strength written across its zone, reading upward, at the middle of
   *  the leg it holds for, once the band has passed there. On the black
   *  zone it stands just below the band, where the ink leaves it legible. */
  Element strength(const Zone& zone, size_t index) const {
    const glm::vec2 start = zone.course.points[index], end = zone.course.points[index + 1];
    const glm::vec2 along = glm::normalize(end - start);
    float degrees = std::atan2(along.y, along.x) * 180 / std::numbers::pi_v<float> - 90;
    if (degrees > 90) degrees -= 180;
    if (degrees < -90) degrees += 180;
    glm::vec2 centre = (start + end) * 0.5f;
    const std::string words = french(zone.men[index]);
    if (zone.retreat()) centre.y += zoneWidth(zone.men[index]) * 0.5f + 6.5f * (float)words.size() + 6;
    const float passed = (zone.course.days[index] + zone.course.days[index + 1]) / 2;
    return text(words)
        .role("numeral")
        .centerAt(centre)
        .rotate(degrees)
        .transformOrigin(pct(50), pct(50))
        .opacity(after(passed, passed + 2));
  }

  /** The thermometer: the curve as the readings come in, each hung from
   *  the retreat by a line that drops to it on the day it was taken. */
  Element thermometer(sketch::SketchContext& context) {
    std::vector<Element> marks;
    rows(context, "temperatures.csv", [&](const auto& table) {
      const auto label = table.template column<std::string>("label");
      const auto longitude = table.template column<double>("lon");
      const auto reaumur = table.template column<double>("reaumur");
      const auto taken = table.template column<double>("day");
      for (size_t row = 0; row < label.size(); ++row) {
        const float x = mapX((float)longitude[row]), y = graphY((float)reaumur[row]);
        const float when = (float)taken[row];
        cold.add({x, y}, when);
        marks.push_back(engravedLine({x, retreatY((float)longitude[row])}, {x, y}, 0.9f)
                            .mask(by::edge(90, after(when - 1, when + 2))));
        // A reading stands to the right of its dropline and below the
        // curve; the two at zero stand above the line, ending at the drop.
        const bool zero = reaumur[row] == 0;
        marks.push_back(text(label[row])
                            .role("reading")
                            .left(zero ? x - 118 : x + 7)
                            .top(zero ? y - 24 : y + 7)
                            .opacity(after(when + 1, when + 3)));
      }
    });
    marks.push_back(pathFigure(cold.outline(), 2).stroke(spans::upTo(coldDrawn), stroke(2.4f, Fill::var("ink"))));
    return box().inset(0).children(std::move(marks));
  }

  /** THE BEREZINA: a ring round the crossing and, beside it, what it cost. */
  Element berezina() const {
    const glm::vec2 crossing{mapX(28.42f), mapY(54.24f)};
    path::Polyline ring{.closed = true};
    for (int step = 0; step < 72; ++step) {
      const float angle = (float)step / 72 * 2 * std::numbers::pi_v<float>;
      ring.points.push_back(crossing + 58.0f * glm::vec2{std::cos(angle), std::sin(angle) * 0.8f});
    }
    const glm::vec2 note{mapX(30.9f), mapY(53.98f)};
    return box().inset(0).opacity(after(kBerezina, kBerezina + 3)).children({
        pathFigure(path::toPath(ring), 2).stroke(stroke(1.2f, Fill::var("ink"))),
        engravedLine(crossing + glm::vec2{56, 22}, note + glm::vec2{-12, 26}, 0.9f, "ink"),
        box().column().left(note.x).top(note.y).gap(2).children({
            text("Passage de la Bérézina").role("note"),
            text("du 26 au 28 9bre 1812").role("numeral").fontSize(16),
            text("50.000 hommes y arrivent, 28.000 la passent.").fontSize(17),
        }),
    });
  }

  /** The running date and the army's number, written each frame. */
  Element almanac() const {
    return pen("almanac",
               [this](draw::Pen& pen) {
                 const float today = day.value();
                 pen.noStroke();
                 pen.fill(kInk);
                 pen.textAlign(draw::LEFT, draw::TOP);
                 pen.textFont(almanacDate);
                 pen.text(frenchDate(today), 0, 0);
                 pen.textFont(almanacCount);
                 pen.text(french(headCount(today)) + " hommes", 0, 46);
               })
        .left(80)
        .top(150)
        .width(460)
        .height(96);
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kGround);
    // Taken with the retreat home, the graph drawn and the Berezina named.
    context.captureAt(33);

    almanacDate = {.face = weave::ports::face({"Didot", "Bodoni 72", "Baskerville"},
                                              weave::FaceStyle{.slant = weave::FaceSlant::Italic}),
                   .size = 34};
    almanacCount = {.face = weave::ports::face({"Didot", "Bodoni 72", "Baskerville"}), .size = 24, .track = 1};

    rows(context, "march.csv", [this](const auto& table) {
      const auto group = table.template column<std::string>("group");
      const auto longitude = table.template column<double>("lon");
      const auto latitude = table.template column<double>("lat");
      const auto men = table.template column<double>("men");
      const auto reached = table.template column<double>("day");
      for (size_t row = 0; row < group.size(); ++row) {
        if (zones.empty() || zones.back().name != group[row]) zones.push_back({.name = group[row]});
        zones.back().course.add({mapX((float)longitude[row]), mapY((float)latitude[row])}, (float)reached[row]);
        zones.back().men.push_back((float)men[row]);
      }
    });

    std::vector<Element> advance, retreat, strengths;
    for (const Zone& zone : zones) {
      (zone.retreat() ? retreat : advance).push_back(band(zone));
      for (size_t index = 0; index + 1 < zone.men.size(); ++index)
        if (index == 0 || zone.men[index] != zone.men[index - 1]) strengths.push_back(strength(zone, index));
    }

    context.composer.render(
        box().inset(0).applyStyleSheet(engraving()).children({
            box().inset(0).key("sheet").cache(Cache::Texture).children({paper(), printed(context)}),
            box()
                .inset(0)
                .opacity(motion::bind(seconds, {.from = {kFadeFrom, kLoopLength},
                                                .clampFrom = true,
                                                .reverse = true}))
                .children({
                    box().inset(0).children(std::move(advance)),
                    box().inset(0).children(std::move(retreat)),
                    box().inset(0).children(std::move(strengths)),
                    thermometer(context),
                    berezina(),
                    almanac(),
                }),
        }));
  }

  void update(double elapsed, sketch::SketchContext&) {
    seconds = (float)std::fmod(elapsed, (double)kLoopLength);
    const float today = campaignDay(seconds.value());
    day = today;
    for (Zone& zone : zones) zone.drawn = zone.course.drawnBy(today);
    coldDrawn = cold.drawnBy(today);
  }
};

SIGIL_SKETCH(Minard1869, "Study · Science",
             "Minard's 1869 flow map of the Russian campaign, run as time — "
             "the army's strength as the width of its march, the cold beneath")
