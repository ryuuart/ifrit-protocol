// Charles Minard's 1869 flow map of Napoleon's Russian campaign. A pale
// buff zone carries the army east from the Niemen to Moscow, its width the
// number of men still marching; a black zone carries the retreat back. The
// strengths are written across the zones, the towns in a fine sloped hand,
// and beneath the map a graph of the cold on the way home hangs from the
// black zone by nine droplines, one per engraved reading.
//
// Everything the plate is drawn from stands in data/: the march by zone,
// the towns, the rivers, the temperatures and the legend paragraph.

// TAGS: Data/Charts

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace path = sigil::geometry::path;
namespace field = sigil::material::field;
using sigil::material::skia::Paint;
using namespace sigil::compose;

namespace {

const auto kPaper = hexColor(0xece3cf);
const auto kInk = hexColor(0x221e1a);
const auto kHairline = hexColor(0x5a4f40);
// Minard calls the advance "le rouge"; the lithograph lays it as a pale
// buff a shade off the paper, so the black retreat reads as the figure.
const auto kAdvance = hexColor(0xdcc093);
const auto kRetreat = hexColor(0x16130f);

constexpr float kWidth = 2560, kHeight = 1600;

// The map is a plain equirectangular sketch: longitude runs across at a
// fixed pitch, latitude up at a little over twice that pitch, which is how
// the plate stretches the narrow band of Russia it crosses.
constexpr float kPixelsPerDegreeLongitude = 158;
constexpr float kPixelsPerDegreeLatitude = kPixelsPerDegreeLongitude * 2.142f;
float mapX(float longitude) {
  return 130 + (longitude - 23.6f) * kPixelsPerDegreeLongitude;
}
float mapY(float latitude) {
  return 270 + (56.0f - latitude) * kPixelsPerDegreeLatitude;
}

// The legend states a millimetre for every ten thousand men; the zones as
// engraved run a little wider than that, and this is the width they have.
float zoneWidth(float men) { return men * 4.69f / 10000; }

// The graph's degrees Réaumur below zero, zero at the top.
constexpr float kGraphTop = 1160, kPixelsPerDegree = 10.5f;
float graphY(float reaumur) { return kGraphTop - reaumur * kPixelsPerDegree; }

struct Station {
  float longitude, latitude, men;
};
struct Zone {
  std::string name;
  std::vector<Station> stations;
};

/** The French thousands point the plate engraves: 422.000. */
std::string french(float men) {
  std::string digits = std::to_string(std::lround(men));
  for (int at = (int)digits.size() - 3; at > 0; at -= 3)
    digits.insert((size_t)at, ".");
  return digits;
}

struct Minard1869 {
  std::vector<Zone> zones;  // march.csv, one entry per group, in file order
  weave::Type script, italic, roman, numerals;

  /** A table beside the sketch, as a list of its rows' named columns. */
  template <class Read>
  static void rows(sketch::SketchContext& context, const char* file,
                   Read read) {
    if (const auto table = context.assets.table(
            context.local(std::string("data/") + file)))
      read(*table);
  }

  /** ONE ZONE: the route through its stations, filled to a width that
   *  steps at each station to the men it carries onward. */
  static Element zone(const Zone& zone, sigil::material::Color colour) {
    path::Polyline route;
    std::vector<float> length{0};
    for (const Station& station : zone.stations) {
      const glm::vec2 point{mapX(station.longitude), mapY(station.latitude)};
      if (!route.points.empty())
        length.push_back(length.back() +
                         glm::distance(route.points.back(), point));
      route.points.push_back(point);
    }
    std::vector<float> stepsAt, widths;
    for (size_t index = 0; index < zone.stations.size(); ++index) {
      if (index > 0) stepsAt.push_back(length[index] / length.back());
      widths.push_back(zoneWidth(zone.stations[index].men));
    }
    brush::Ribbon band =
        brush::ribbon(path::profile::spans(stepsAt, widths), Fill::color(colour));
    band.step = 1.5f;
    return pathFigure(path::toPath(route)).stroke(band);
  }

  /** A strength written across its zone, reading upward, at the middle of
   *  the leg it holds for. On the black zone it stands just below the
   *  band instead, where the ink leaves it legible. */
  Element strength(const Station& from, const Station& to, bool below) const {
    const glm::vec2 start{mapX(from.longitude), mapY(from.latitude)};
    const glm::vec2 end{mapX(to.longitude), mapY(to.latitude)};
    const glm::vec2 along = glm::normalize(end - start);
    float degrees = std::atan2(along.y, along.x) * 180 / 3.14159265f - 90;
    if (degrees > 90) degrees -= 180;
    if (degrees < -90) degrees += 180;
    glm::vec2 centre = (start + end) * 0.5f;
    if (below) {
      const float runLength = 12.0f * (float)french(from.men).size();
      centre.y += zoneWidth(from.men) * 0.5f + runLength * 0.5f + 4;
    }
    return text(french(from.men))
        .font(numerals)
        .centerAt({centre.x, centre.y})
        .rotate(degrees)
        .transformOrigin(pct(50), pct(50));
  }

  /** Where the retreat stands at a longitude: the droplines hang from it. */
  float retreatY(float longitude) const {
    std::vector<Station> route;
    for (const Zone& zone : zones)
      if (zone.name == "retreat east" || zone.name == "retreat west")
        route.insert(route.end(), zone.stations.begin(), zone.stations.end());
    for (size_t index = 1; index < route.size(); ++index) {
      const Station& east = route[index - 1];
      const Station& west = route[index];
      if (longitude <= east.longitude && longitude >= west.longitude) {
        const float share = east.longitude == west.longitude
                                ? 0
                                : (east.longitude - longitude) /
                                      (east.longitude - west.longitude);
        return mapY(east.latitude + (west.latitude - east.latitude) * share);
      }
    }
    return mapY(54.4f);
  }

  static Element rule(float x0, float y0, float x1, float y1, float weight,
                      sigil::material::Color colour) {
    path::Polyline line{.points = {{x0, y0}, {x1, y1}}};
    return pathFigure(path::toPath(line), weight)
        .stroke(stroke(weight, Fill::color(colour)));
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kPaper);
    context.captureAt(20.0);
    context.nonlinearPicture();

    // Four lettering systems: an engraver's round script for the title,
    // a fine sloped italic for places and the legend, spaced roman
    // capitals for MOSCOU alone, and upright numerals across the zones.
    script = {.face = weave::ports::face({"Snell Roundhand", "Apple Chancery"}),
              .size = 34};
    italic = {.face = weave::ports::face({"Baskerville", "Times New Roman"},
                                         SkFontStyle::Italic()),
              .size = 17,
              .track = 0.2f};
    roman = {.face = weave::ports::face({"Baskerville", "Times New Roman"}),
             .size = 24,
             .track = 4};
    numerals = {.face = weave::ports::face({"Baskerville", "Times New Roman"}),
                .size = 19,
                .track = 0.3f};

    rows(context, "march.csv", [this](const auto& table) {
      const auto group = table.template column<std::string>("group");
      const auto longitude = table.template column<double>("lon");
      const auto latitude = table.template column<double>("lat");
      const auto men = table.template column<double>("men");
      for (size_t row = 0; row < group.size(); ++row) {
        if (zones.empty() || zones.back().name != group[row])
          zones.push_back({group[row], {}});
        zones.back().stations.push_back(
            {(float)longitude[row], (float)latitude[row], (float)men[row]});
      }
    });

    std::vector<Element> map, labels, rivers, graph;
    for (const Zone& zone : zones)
      if (zone.name.starts_with("advance"))
        map.push_back(Minard1869::zone(zone, kAdvance));
    for (const Zone& zone : zones)
      if (zone.name.starts_with("retreat"))
        map.push_back(Minard1869::zone(zone, kRetreat));
    for (const Zone& zone : zones) {
      const bool retreat = zone.name.starts_with("retreat");
      for (size_t index = 0; index + 1 < zone.stations.size(); ++index)
        if (index == 0 || zone.stations[index].men != zone.stations[index - 1].men)
          labels.push_back(strength(zone.stations[index],
                                    zone.stations[index + 1], retreat));
    }

    rows(context, "rivers.csv", [&](const auto& table) {
      const auto name = table.template column<std::string>("river");
      const auto longitude = table.template column<double>("lon");
      const auto latitude = table.template column<double>("lat");
      path::Polyline course;
      for (size_t row = 0; row < name.size(); ++row) {
        course.points.push_back(
            {mapX((float)longitude[row]), mapY((float)latitude[row])});
        if (row + 1 == name.size() || name[row + 1] != name[row]) {
          const glm::vec2 mouth = course.points.back();
          rivers.push_back(pathFigure(path::smoothThrough(course), 1)
                               .stroke(stroke(1.1f, Fill::color(kHairline))));
          rivers.push_back(text(name[row])
                               .font({.size = 14})
                               .ink(kHairline)
                               .left(mouth.x - 90)
                               .top(mouth.y + 6));
          course.points.clear();
        }
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
        labels.push_back(
            text(moscow ? std::string("MOSCOU") : name[row])
                .font(moscow ? roman : weave::Type{})
                .left(mapX((float)longitude[row]) + (float)dx[row])
                .top(mapY((float)latitude[row]) + (float)dy[row]));
      }
    });

    // The graph: a rule every five degrees, the curve through the nine
    // readings, and a dropline from the black zone to each reading.
    for (int degrees = 0; degrees <= 30; degrees += 5) {
      const float y = graphY((float)-degrees);
      graph.push_back(rule(mapX(24.4f), y, 2430, y, degrees % 10 ? 0.5f : 0.9f,
                           kHairline));
      graph.push_back(text(degrees == 30 ? "-30 degrés"
                           : degrees == 0 ? "Zéro"
                                          : "-" + std::to_string(degrees))
                          .font({.face = numerals.face, .size = 14})
                          .left(2440)
                          .top(y - 9));
    }
    rows(context, "temperatures.csv", [&](const auto& table) {
      const auto label = table.template column<std::string>("label");
      const auto longitude = table.template column<double>("lon");
      const auto reaumur = table.template column<double>("reaumur");
      path::Polyline curve;
      for (size_t row = 0; row < label.size(); ++row) {
        const float x = mapX((float)longitude[row]);
        const float y = graphY((float)reaumur[row]);
        curve.points.push_back({x, y});
        graph.push_back(rule(x, retreatY((float)longitude[row]), x, y, 0.8f,
                             kHairline));
        // A reading stands to the right of its dropline and below the
        // curve; the two at zero stand above the line, ending at the drop.
        const bool zero = reaumur[row] == 0;
        graph.push_back(text(label[row])
                            .font({.face = numerals.face, .size = 15})
                            .left(zero ? x - 112 : x + 6)
                            .top(zero ? y - 24 : y + 6));
      }
      graph.push_back(pathFigure(path::toPath(curve), 2)
                          .stroke(stroke(2.2f, Fill::color(kInk))));
    });

    // The plate's scale of distance: common French leagues, graduated
    // every five to twenty-five and numbered again only at fifty.
    std::vector<Element> scale;
    {
      constexpr float kPixelsPerLeague = 6.0f;
      const float x = mapX(33.4f), y = mapY(54.25f);
      scale.push_back(rule(x, y, x + 50 * kPixelsPerLeague, y, 1.2f, kInk));
      for (int league : {0, 5, 10, 15, 20, 25, 50}) {
        const float tick = x + (float)league * kPixelsPerLeague;
        scale.push_back(rule(tick, y - 7, tick, y, 1.0f, kInk));
        scale.push_back(text(std::to_string(league))
                            .font({.face = numerals.face, .size = 12})
                            .centerAt({tick, y + 12}));
      }
      scale.push_back(text("Lieues communes de France (Carte de M. de Fezensac)")
                          .font({.size = 14})
                          .left(x)
                          .top(y - 30));
    }

    std::vector<Element> legend;
    rows(context, "legend.csv", [&](const auto& table) {
      for (const std::string& line : table.template column<std::string>("text"))
        legend.push_back(text(line));
    });

    context.composer.render(
        box()
            .inset(0)
            .fill(Paint::solid(kPaper))
            .font(italic)
            .ink(kInk)
            .children({
                // The sheet's tooth, laid under everything printed.
                box()
                    .inset(0)
                    .fill(Paint::recipe(field::grain(0.5f, 3, 1869.0f)))
                    .blendMode(SkBlendMode::kMultiply)
                    .opacity(0.08f),
                // The printed frame: a heavy rule outside a fine one.
                kit::at(box().foreground(
                            decorations::border(2.4f, Fill::color(kInk))),
                        34, 34, kWidth - 68, kHeight - 68),
                kit::at(box().foreground(
                            decorations::border(0.8f, Fill::color(kInk))),
                        44, 44, kWidth - 88, kHeight - 88),
                box()
                    .column()
                    .alignItems(Align::Center)
                    .left(0)
                    .width(kWidth)
                    .top(64)
                    .gap(8)
                    .children({
                        text("Carte Figurative des pertes successives en "
                             "hommes de l'Armée Française dans la campagne de "
                             "Russie 1812–1813.")
                            .font(script),
                        text("Dressée par M. Minard, Inspecteur Général des "
                             "Ponts et Chaussées en retraite.            "
                             "Paris, le 20 Novembre 1869.")
                            .font({.size = 19}),
                        box()
                            .column()
                            .alignItems(Align::Center)
                            .gap(3)
                            .font({.size = 17})
                            .children(std::move(legend)),
                    }),
                box().inset(0).children(std::move(rivers)),
                box().inset(0).children(std::move(map)),
                box().inset(0).children(std::move(labels)),
                box().inset(0).children(std::move(scale)),
                text("TABLEAU GRAPHIQUE de la température en degrés du "
                     "thermomètre de Réaumur au dessous de zéro.")
                    .font({.size = 18})
                    .left(mapX(26.5f))
                    .top(kGraphTop - 58),
                box().inset(0).children(std::move(graph)),
                text("Les Cosaques passent au galop\nle Niémen gelé.")
                    .font({.size = 15})
                    .left(80)
                    .top(graphY(-22)),
                text("Autog. par Regnier, 8. Pas. Sᵗᵉ Marie Sᵗ Gᵃᵉᵐ à Paris.")
                    .font({.size = 12})
                    .left(56)
                    .top(kHeight - 70),
                text("Imp. Lith. Regnier et Dourdet")
                    .font({.size = 12})
                    .left(kWidth - 280)
                    .top(kHeight - 70),
            }));
  }
};

}  // namespace

SIGIL_SKETCH(Minard1869, "Study · Science",
             "Minard's 1869 flow map of the Russian campaign — the army's "
             "strength as the width of its march, the cold beneath")
