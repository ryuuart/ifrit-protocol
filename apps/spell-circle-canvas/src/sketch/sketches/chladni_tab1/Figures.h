#pragma once

/** @file
 * THE TWELVE FIGURES OF THE PLATE, as its words file states them, and the
 * two constructions the entry draws them from: the compass arc the
 * engraver struck for the three figures no eigenmode formula describes,
 * and where each grain of a figure's sand comes to rest.
 */

#include <sigilcompose/core/Instances.h>
#include <sigilcore/compute/Chance.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Color.h>
#include <sigilgeometry/kit/Curves.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Scatter.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace chance = sigil::core::chance;
namespace data = sigil::data;
namespace instancing = sigil::compose::instancing;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;

constexpr float kPi = std::numbers::pi_v<float>;

/** The plate's own convention: 0 degrees is twelve o'clock and bearings
 *  run clockwise, which is how every figure gives the bearing of its
 *  lines. Radius 1 is the rim, so a bearing and a fraction of the disc
 *  name a point. */
constexpr path::PolarFrame kUnit{.centre = {0, 0},
                                 .radius = 1.0f,
                                 .zero = path::Zero::North,
                                 .sense = path::Sense::CW};

glm::vec2 onUnit(float bearing, float radius) {
  const SkPoint point = kUnit.at(bearing, radius);
  return {point.fX, point.fY};
}

/** HOW A FIGURE IS DRAWN. A star is the sand's own heap along the nodal
 *  lines of a symmetric mode; valleys are the same star worn as a blank
 *  channel through a disc of combed sand; arcs are the off-centre
 *  variants, drawn line by line with a compass. */
enum class Kind { Star, Valleys, Arcs };

/** ONE NODAL LINE STRUCK WITH A COMPASS: the part of a circle of
 *  `radius`, its point set at `compass` along `bearing`, that lies inside
 *  the disc. A diameter is the degenerate line the same definition
 *  admits: any path from rim to rim. */
struct Linie {
  bool diameter = false;
  float bearing = 0;
  float compass = 0;
  float radius = 0;
};

/** A REFERENCE LETTER, upright wherever it stands on the rim. */
struct Letter {
  float bearing = 0;
  float radius = 0;
  std::string glyph;
};

struct Figure {
  int number = 0;
  /** Canvas pixels. */
  SkPoint centre{0, 0};
  Kind kind = Kind::Star;
  int points = 0;
  float inner = 0;
  /** How far the star's flanks bow inward, as a fraction of the rim. */
  float waist = 0;
  /** Chladni's count of nodal lines. */
  int lines = 0;
  /** The interval above the fundamental he tunes this figure to, or
   *  zero where he states none. */
  float semitones = 0;
  int grains = 0;
  std::vector<Linie> linien;
  std::vector<Letter> letters;

  /** HOW HIGH THE FIGURE SOUNDS. The stated interval where Chladni gives
   *  one, a rise with the line count where he does not — monotonic in
   *  the plate's reading order either way, so reading order is pitch. */
  float pitch() const {
    return semitones > 0 ? semitones : 20.0f + 2.4f * (float)lines;
  }
};

/** "#e3d7b6" or "#3a3125b8" as a colour. */
sigil::material::Color colourOf(const data::Json& word) {
  return sigil::draw::parseColor(word.text("#000000"));
}

std::vector<Figure> readFigures(const data::Json& entries, float scale) {
  std::vector<Figure> figures;
  for (const data::Json& entry : entries.items()) {
    const std::string_view kind = entry["kind"].text();
    Figure figure{.number = (int)entry["number"].number(),
                  .centre = {(float)entry["centre"][0].number() * scale,
                             (float)entry["centre"][1].number() * scale},
                  .kind = kind == "valleys" ? Kind::Valleys
                          : kind == "arcs"  ? Kind::Arcs
                                            : Kind::Star,
                  .points = (int)entry["points"].number(),
                  .inner = (float)entry["inner"].number(),
                  .waist = (float)entry["waist"].number(),
                  .lines = (int)entry["lines"].number(),
                  .semitones = (float)entry["semitones"].number(),
                  .grains = (int)entry["grains"].number()};
    for (const data::Json& arc : entry["arcs"].items())
      figure.linien.push_back({.diameter = !arc["diameter"].null(),
                               .bearing = (float)arc["bearing"].number(),
                               .compass = (float)arc["compass"].number(),
                               .radius = (float)arc["radius"].number()});
    for (const data::Json& letter : entry["letters"].items())
      figure.letters.push_back({(float)letter[0].number(),
                                (float)letter[1].number(),
                                std::string(letter[2].text())});
    figures.push_back(std::move(figure));
  }
  return figures;
}

/** THE LINE AS THE ENGRAVER DREW IT, in unit-disc coordinates: an arc of
 *  the compass circle from one rim crossing to the other, or a diameter.
 *  Solving |P| = 1 against |P − O| = radius gives both crossings; of the
 *  two sweeps between them, the one inside the disc is the one whose
 *  midpoint is nearer the centre. */
struct Arc {
  bool straight = false;
  glm::vec2 compass{0, 0};
  float radius = 0;
  float start = 0;
  float sweep = 0;
  /** A diameter's two ends. */
  glm::vec2 from{0, -1}, to{0, 1};

  glm::vec2 at(float along) const {
    if (straight) return from + (to - from) * along;
    const float angle = start + sweep * along;
    return compass + radius * glm::vec2{std::cos(angle), std::sin(angle)};
  }
  /** The unit direction of travel at @p along. */
  glm::vec2 heading(float along) const {
    if (straight) return glm::normalize(to - from);
    const float angle = start + sweep * along;
    const float sense = sweep < 0 ? -1.0f : 1.0f;
    return sense * glm::vec2{-std::sin(angle), std::cos(angle)};
  }
  float length() const {
    return straight ? glm::length(to - from) : std::abs(sweep) * radius;
  }
};

Arc arcOf(const Linie& linie) {
  if (linie.diameter) {
    const glm::vec2 end = onUnit(linie.bearing, 1.0f);
    return {.straight = true, .from = end, .to = -end};
  }
  const glm::vec2 compass = onUnit(linie.bearing, linie.compass);
  const float reach = glm::length(compass);
  // The chord through both crossings stands `along` from the centre on
  // the line to the compass point, and reaches `half` either side of it.
  const float along =
      (1.0f + reach * reach - linie.radius * linie.radius) * 0.5f / reach;
  const float half = std::sqrt(std::max(1.0f - along * along, 0.0f));
  const glm::vec2 toward = compass / reach;
  const glm::vec2 across{-toward.y, toward.x};
  const glm::vec2 first = along * toward + half * across;
  const glm::vec2 second = along * toward - half * across;
  const float start = std::atan2(first.y - compass.y, first.x - compass.x);
  float sweep = std::atan2(second.y - compass.y, second.x - compass.x) - start;
  sweep = std::remainder(sweep, 2 * kPi);
  Arc arc{.compass = compass,
          .radius = linie.radius,
          .start = start,
          .sweep = sweep};
  if (glm::length(arc.at(0.5f)) > 1.0f)
    arc.sweep += sweep > 0 ? -2 * kPi : 2 * kPi;
  return arc;
}

/** The arc as the node's outline, keyed so a node drawn from it prunes. */
shapes::KeyedParametric outlineOf(const Figure& figure, size_t index) {
  const Arc arc = arcOf(figure.linien[index]);
  return shapes::parametric(
      "chladni." + std::to_string(figure.number) + "." + std::to_string(index),
      [arc](float along) {
        const glm::vec2 point = arc.at(along);
        return SkPoint{point.x, point.y};
      },
      0.0f, 1.0f, arc.straight ? 2 : 72);
}

/** The figure's star as rings, in unit-disc coordinates, its tips at
 *  @p reach of the rim. It is flattened at a box of its own size in
 *  pixels, fine enough that a grain inside the rings is inside the ink. */
std::vector<path::Polyline> starRings(const Figure& figure, float reach) {
  constexpr float kBox = 400;
  const SkPath star = shapes::star(figure.points, figure.inner,
                                   figure.waist)(SkSize{kBox, kBox});
  std::vector<path::Polyline> rings = path::Region::of(star).rings;
  for (path::Polyline& ring : rings)
    for (glm::vec2& point : ring.points)
      point = (point - kBox * 0.5f) / (kBox * 0.5f) * reach;
  return rings;
}

/** THE MARKS THE SAND IS STAMPED WITH, as the atlas registers them: a
 *  grain, a chip, a round grain and a long combed stroke. */
enum Mark : int { Grain = 0, Chip = 1, Round = 2, Comb = 3 };

/** WHERE EVERY GRAIN OF ONE FIGURE LANDS AND WHEN, written into @p pool
 *  as flights in pixels about the figure's centre, with the phase each
 *  grain shivers at into @p shiver.
 *
 *  Every grain starts in an even scatter over the disc and flies to its
 *  place on its own start and duration: inside the star where the sand
 *  heaps, in the valleys between its arms where the sand is combed
 *  outward, or beside a compass line. Where Chladni states an interval
 *  it also sets how fast the sand finds the line — a higher pitch
 *  bounces it into place sooner. The pool's pixels put the figure's
 *  centre at @p origin. */
void seedSand(const Figure& figure, float radius, float bowAt, SkPoint origin,
              instancing::Pool& pool, std::vector<float>& shiver) {
  chance::Stream draw =
      chance::Stream::pcg(1787u + (uint64_t)figure.number * 61u);
  const float speed = std::clamp(24.0f / figure.pitch(), 0.68f, 1.25f);

  std::vector<glm::vec2> rests;
  std::vector<float> headings;
  if (figure.kind == Kind::Arcs) {
    std::vector<Arc> arcs;
    float total = 0;
    for (const Linie& linie : figure.linien) {
      arcs.push_back(arcOf(linie));
      total += arcs.back().length();
    }
    for (int grain = 0; grain < figure.grains; ++grain) {
      float pick = draw.unit() * total;
      size_t which = 0;
      while (which + 1 < arcs.size() && pick > arcs[which].length())
        pick -= arcs[which++].length();
      const Arc& arc = arcs[which];
      const float along = std::clamp(pick / arc.length(), 0.0f, 1.0f);
      const glm::vec2 heading = arc.heading(along);
      const float aside = (draw.unit() + draw.unit() - 1.0f) * 1.9f / radius;
      rests.push_back(arc.at(along) + aside * glm::vec2{-heading.y, heading.x});
      headings.push_back(std::atan2(heading.y, heading.x));
    }
  } else {
    path::Region region;
    if (figure.kind == Kind::Star) {
      region.rings = starRings(figure, 0.985f);
    } else {
      // The disc and the star read by the even-odd rule: the valleys.
      region = path::Region::disc({0, 0}, 0.965f);
      for (path::Polyline& ring : starRings(figure, 1.0f))
        region.rings.push_back(std::move(ring));
    }
    rests = path::sample(region, path::uniform(figure.grains * 2,
                                               17u + (uint64_t)figure.number));
    // Combed sand gathers thickest at the rim, where the plate moves
    // least, and thins toward the channel: a grain is kept with a chance
    // that grows with its distance out.
    if (figure.kind == Kind::Valleys)
      std::erase_if(rests, [&](glm::vec2 rest) {
        return draw.unit() >
               0.15f + 0.85f * glm::length(rest) * glm::length(rest);
      });
    if (rests.size() > (size_t)figure.grains)
      rests.resize((size_t)figure.grains);
    for (const glm::vec2& rest : rests)
      headings.push_back(figure.kind == Kind::Valleys
                             ? std::atan2(rest.y, rest.x)  // combed outward
                             : draw.range(0, 2 * kPi));
  }

  pool.resize(rests.size());
  shiver.assign(rests.size(), 0.0f);
  auto flights = pool.flights();
  auto marks = pool.frames();
  for (size_t index = 0; index < rests.size(); ++index) {
    const glm::vec2 scattered =
        onUnit(draw.range(0, 360), 0.965f * std::sqrt(draw.unit()));
    instancing::Pool::Flight grain{
        .from = {origin.fX + scattered.x * radius,
                 origin.fY + scattered.y * radius},
        .to = {origin.fX + rests[index].x * radius,
               origin.fY + rests[index].y * radius},
        .rotateFrom = draw.range(0, 2 * kPi),
        .start = bowAt + draw.range(0, 0.24f),
        .duration = (0.80f + draw.range(0, 0.42f)) * speed};
    grain.alphaFrom = grain.alphaTo = draw.range(0.62f, 1.0f);
    switch (figure.kind) {
      case Kind::Star:
        grain.rotateTo = headings[index];
        marks[index] = draw.unit() < 0.55f ? Grain : Round;
        grain.scaleFrom = grain.scaleTo = draw.range(0.55f, 0.94f);
        break;
      case Kind::Valleys:
        grain.rotateTo = headings[index] + draw.range(-0.09f, 0.09f);
        marks[index] = Comb;
        grain.scaleFrom = grain.scaleTo = draw.range(0.44f, 0.88f);
        break;
      case Kind::Arcs:
        grain.rotateTo = headings[index] + draw.range(-0.10f, 0.10f);
        marks[index] = draw.unit() < 0.7f ? Chip : Round;
        grain.scaleFrom = grain.scaleTo = draw.range(0.45f, 0.78f);
        break;
    }
    flights[index] = grain;
    shiver[index] = draw.range(0, 2 * kPi);
  }
}

}  // namespace
