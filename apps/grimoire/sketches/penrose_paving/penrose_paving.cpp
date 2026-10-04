// A Penrose paving, laid while you watch. Ten thin half-rhombs meet at one
// point; each generation cuts every half into smaller ones by the golden
// ratio, the mason chalks the new joints on the stone, and the setts are
// recut along them — six times, until the plaza is paved in two rhombs, a
// fat one of 72° in a pale limestone and a thin one of 36° in a blue-grey
// slate, that never repeat.
//
// The stone is stone: every sett is cut from one of four beds of its
// quarry, each bed a shade of its own with its grain running its own way,
// and every sett has a chamfered arris and stands in a mortar joint. A low
// sun rakes across the finished plaza from the morning side to the evening
// side and back, so each chamfer turned towards it gleams, each turned away
// darkens, and the joints on the far side of a sett lie in its shadow.
//
// Over the finished paving the matching rules are drawn: a gold arc and a
// blue arc on every sett, which join across every joint into unbroken
// curves only because the tiling obeys the rules — and the rules are what
// forbid it to repeat. Then the paving is gathered back, generation by
// generation, into the sun it was cut from.

// TAGS: Patterns/Tiling

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/path/Segments.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <map>
#include <numbers>
#include <string>
#include <vector>

#include "../cosmati/Stone.h"

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::material::Filter;
using sigil::material::hexColor;

namespace {

// --- the drawing ------------------------------------------------------------

constexpr float kWidth = 1600, kHeight = 1200;
constexpr glm::vec2 kCentre{kWidth / 2, kHeight / 2};  // the five-fold point
constexpr float kEdge = 78;       // a finished sett's side, px
constexpr int kGenerations = 6;   // cuts from the sun down to the paving
constexpr float kJoint = 3.0f;    // the mortar joint between two setts
constexpr float kChamfer = 3.2f;  // the bevelled arris round each sett's face
constexpr float kGolden = std::numbers::phi_v<float>;
constexpr float kPi = std::numbers::pi_v<float>;

// --- the construction, in seconds of one loop
// ---------------------------------

constexpr float kFirstCut = 0.8f;  // the sun alone, before the first cut
constexpr float kCutEvery = 1.8f;  // one generation: chalk, then recut
constexpr float kMarking = 0.45f;  // the share of a cut spent chalking it
constexpr float kLaid = kFirstCut + kGenerations * kCutEvery;
constexpr float kGathered = 36.4f;  // the paving begins to go back to the sun
constexpr float kGatherEvery = 0.5f;
constexpr float kLoop = 40.0f;

// --- the stone, the mortar and the lettering ---------------------------------

const material::Color kMortar = hexColor(0x34322E);
const material::Color kSunlight = hexColor(0xFFF1D6);
const material::Color kShade = hexColor(0x10141C);
const material::Color kChalk = hexColor(0xF4F1E8, 0.9f);
const material::Color kGold = hexColor(0xF2BC4E);
const material::Color kBlue = hexColor(0x5CCBEA);
const material::Color kPanel = hexColor(0x15171A, 0.9f);
const material::Color kPanelRule = hexColor(0x8C8577, 0.5f);
const material::Color kLettering = hexColor(0xEDE6D6);
const material::Color kLetteringQuiet = hexColor(0xA59D8E);

// --- the tiling
// ----------------------------------------------------------------

/** HALF A RHOMB, cut along the diagonal between `first` and `second`. A fat
 *  half is 108° at its apex and 36° at the other two corners; a thin half
 *  is 36° at its apex and 72° at the other two. The two halves of one
 *  rhomb are mirror images sharing `first` and `second`. */
struct Half {
  bool fat;
  glm::vec2 apex, first, second;
};

/** One cut: every half becomes two or three halves smaller by the golden
 *  ratio, covering exactly the same ground. A thin half gives one thin and
 *  one fat; a fat half gives two fat and one thin. */
std::vector<Half> subdivide(const std::vector<Half>& halves) {
  std::vector<Half> finer;
  for (const Half& half : halves) {
    if (half.fat) {
      const glm::vec2 alongApex =
          half.first + (half.apex - half.first) / kGolden;
      const glm::vec2 alongBase =
          half.first + (half.second - half.first) / kGolden;
      finer.push_back({true, alongBase, half.second, half.apex});
      finer.push_back({true, alongApex, alongBase, half.first});
      finer.push_back({false, alongBase, alongApex, half.apex});
    } else {
      const glm::vec2 cut = half.apex + (half.first - half.apex) / kGolden;
      finer.push_back({false, half.second, cut, half.first});
      finer.push_back({true, cut, half.second, half.apex});
    }
  }
  return finer;
}

/** The sun: ten thin halves meeting at their 36° apexes, alternately
 *  mirrored, with legs long enough that six cuts leave setts of kEdge. */
std::vector<Half> sun() {
  const float radius = kEdge * std::pow(kGolden, (float)kGenerations);
  std::vector<Half> halves;
  for (int spoke = 0; spoke < 10; ++spoke) {
    auto rim = [&](int step) {
      const float angle = step * kPi / 10;
      return kCentre + radius * glm::vec2{std::cos(angle), std::sin(angle)};
    };
    glm::vec2 first = rim(2 * spoke - 1), second = rim(2 * spoke + 1);
    if (spoke % 2 == 0) std::swap(first, second);
    halves.push_back({false, kCentre, first, second});
  }
  return halves;
}

/** A SETT: a whole rhomb, its corners in order round it. `far` mirrors the
 *  apex through the diagonal, so it is the other half's apex. */
struct Sett {
  bool fat;
  glm::vec2 apex, first, far, second;
  std::array<glm::vec2, 4> corners() const {
    return {apex, first, far, second};
  }
  glm::vec2 centre() const { return (first + second) / 2.0f; }
};

/** A GENERATION: the sun cut @p cuts times, each pair of halves laid as the
 *  one sett they make. A half whose partner lies beyond the sun's rim is
 *  laid whole all the same; that part is off the canvas. */
std::vector<Sett> generation(int cuts) {
  std::vector<Half> halves = sun();
  for (int cut = 0; cut < cuts; ++cut) halves = subdivide(halves);
  std::map<std::pair<long, long>, Sett> setts;  // one per shared diagonal
  for (const Half& half : halves) {
    const Sett sett{half.fat, half.apex, half.first,
                    half.first + half.second - half.apex, half.second};
    float left = INFINITY, right = -INFINITY, top = INFINITY,
          bottom = -INFINITY;
    for (glm::vec2 corner : sett.corners()) {
      left = std::min(left, corner.x), right = std::max(right, corner.x);
      top = std::min(top, corner.y), bottom = std::max(bottom, corner.y);
    }
    if (right < 0 || left > kWidth || bottom < 0 || top > kHeight) continue;
    const glm::vec2 centre = sett.centre();
    setts.emplace(
        std::pair{std::lround(centre.x * 8), std::lround(centre.y * 8)}, sett);
  }
  std::vector<Sett> laid;
  for (const auto& [place, sett] : setts) laid.push_back(sett);
  return laid;
}

// --- cutting a sett from its outline
// --------------------------------------------

using Quad = std::array<glm::vec2, 4>;

glm::vec2 centroid(const Quad& quad) {
  return (quad[0] + quad[1] + quad[2] + quad[3]) / 4.0f;
}

/** The outward normal of the side from corner @p side to the next. */
glm::vec2 outward(const Quad& quad, size_t side) {
  const glm::vec2 along = glm::normalize(quad[(side + 1) % 4] - quad[side]);
  glm::vec2 normal{along.y, -along.x};
  const glm::vec2 middle = (quad[side] + quad[(side + 1) % 4]) / 2.0f;
  if (glm::dot(normal, middle - centroid(quad)) < 0) normal = -normal;
  return normal;
}

/** The same rhomb with every side moved @p distance inward. */
Quad inset(const Quad& quad, float distance) {
  Quad inner;
  for (size_t corner = 0; corner < 4; ++corner) {
    const glm::vec2 before = -outward(quad, (corner + 3) % 4);
    const glm::vec2 after = -outward(quad, corner);
    const glm::vec2 bisector = glm::normalize(before + after);
    inner[corner] =
        quad[corner] + bisector * (distance / glm::dot(bisector, before));
  }
  return inner;
}

/** Closed polygons, or open runs, as one outline. */
path::Outline outlineOf(const std::vector<std::vector<glm::vec2>>& runs,
                        bool closed) {
  std::vector<path::SegmentContour> contours;
  for (const auto& run : runs) {
    path::SegmentContour contour{.closed = closed};
    for (size_t point = 0; point + 1 < run.size(); ++point)
      contour.segments.push_back({.kind = path::SegmentKind::Line,
                                  .points = {run[point], run[point + 1]}});
    if (closed)
      contour.segments.push_back({.kind = path::SegmentKind::Line,
                                  .points = {run.back(), run.front()}});
    contours.push_back(std::move(contour));
  }
  return path::toPath(contours);
}

// --- the stone
// -------------------------------------------------------------------

/** A BED OF A QUARRY: its two tones and the way its grain runs. */
struct Bed {
  material::Color lit, shade;
  float grainDegrees;
};
constexpr size_t kBedsPerQuarry = 4;

/** The fat setts' limestone: cream, buff, a grey bed and a warm one. */
const std::array<Bed, kBedsPerQuarry> kLimestone = {{
    {hexColor(0xE6DECB), hexColor(0xCFC4AC), 24},
    {hexColor(0xDDD3BC), hexColor(0xC2B59A), 71},
    {hexColor(0xD8D5CC), hexColor(0xBDB8AC), 132},
    {hexColor(0xE8D8BC), hexColor(0xCDB894), 163},
}};
/** The thin setts' slate: blue, blue-grey, green and heather beds. */
const std::array<Bed, kBedsPerQuarry> kSlate = {{
    {hexColor(0x677684), hexColor(0x4B5764), 18},
    {hexColor(0x707981), hexColor(0x535B63), 62},
    {hexColor(0x66766F), hexColor(0x4A5752), 118},
    {hexColor(0x74687A), hexColor(0x564C5C), 152},
}};

/** The stone of one bed: limestone fine and clouded, slate split along its
 *  cleavage into long fine grain. */
material::Material quarry(bool fat, size_t bed) {
  const Bed& from = (fat ? kLimestone : kSlate)[bed];
  return cosmati::stone({
      .hi = from.lit,
      .lo = from.shade,
      .bedAngle = from.grainDegrees,
      .bedLength = fat ? 260.0f : 90.0f,
      .bedDepth = fat ? 0.45f : 0.3f,
      .grainScale = fat ? 0.30f : 0.45f,
      .grainContrast = fat ? 0.20f : 0.26f,
      .stretch = fat ? 1.0f : 2.2f,
      .speckle = fat ? 0.30f : 0.12f,
      .speckleCell = fat ? 4.5f : 6.0f,
      .speckleAlpha = fat ? 0.30f : 0.25f,
      .seed = (float)(bed * 7 + (fat ? 3 : 11)),
  });
}

/** Which bed a sett was cut from, fixed by where it lies. */
size_t bedOf(const Sett& sett) {
  const glm::vec2 centre = sett.centre();
  const float scatter =
      std::sin(centre.x * 12.9898f + centre.y * 78.233f) * 43758.5453f;
  return (size_t)((scatter - std::floor(scatter)) * kBedsPerQuarry) %
         kBedsPerQuarry;
}

// --- the sun
// -----------------------------------------------------------------------

/** A LOW SUN: where it stands in plan, how high, where its light pools on
 *  the plaza, and the colour of that light from the pool outward. */
struct Sun {
  glm::vec2 towards;  // unit, in plan, pointing at the sun
  float elevationDegrees;
  glm::vec2 pool;
  std::array<material::Color, 4> light;
};
const Sun kMorning{glm::normalize(glm::vec2{-0.82f, -0.57f}),
                   17,
                   {360, 240},
                   {hexColor(0xFFFCF5), hexColor(0xF1EDE6), hexColor(0xC6C4C4),
                    hexColor(0x9B9CA2)}};
const Sun kEvening{glm::normalize(glm::vec2{0.86f, -0.51f}),
                   14,
                   {1260, 260},
                   {hexColor(0xFFF3DC), hexColor(0xF6E4C6), hexColor(0xD0BCA4),
                    hexColor(0x9C8B82)}};

/** How much brighter (above 0) or darker (below 0) than the flat face a
 *  45° chamfer facing @p normal is under @p sun. */
float chamferLight(const Sun& sun, glm::vec2 normal) {
  const float elevation = sun.elevationDegrees * kPi / 180;
  const float face = std::sin(elevation);
  const float slope = std::numbers::sqrt2_v<float> / 2;
  const float lit =
      slope * (glm::dot(normal, sun.towards) * std::cos(elevation) + face);
  return std::max(lit, 0.0f) / face - 1;
}

// --- a generation laid in stone
// ----------------------------------------------------

/** THE CHAMFERS AND JOINTS, grouped by the way they face. A Penrose tiling's
 *  sides run in five directions, so its outward normals take ten, and each
 *  of the ten catches the sun one way: one figure per direction. */
std::vector<Element> relief(const std::vector<Sett>& setts, const Sun& sun) {
  std::array<std::vector<std::vector<glm::vec2>>, 10> chamfers, joints;
  std::array<glm::vec2, 10> facing{};
  for (const Sett& sett : setts) {
    const Quad face = inset(sett.corners(), kJoint / 2);
    const Quad arris = inset(face, kChamfer);
    for (size_t side = 0; side < 4; ++side) {
      const glm::vec2 normal = outward(face, side);
      const int turn =
          (int)std::lround(std::atan2(normal.y, normal.x) / (kPi / 5));
      const size_t direction = (size_t)((turn % 10 + 10) % 10);
      facing[direction] = normal;
      const size_t next = (side + 1) % 4;
      chamfers[direction].push_back(
          {face[side], face[next], arris[next], arris[side]});
      joints[direction].push_back({face[side], face[next],
                                   face[next] + normal * kJoint,
                                   face[side] + normal * kJoint});
    }
  }
  std::vector<Element> figures;
  for (size_t direction = 0; direction < 10; ++direction) {
    if (chamfers[direction].empty()) continue;
    const float light = chamferLight(sun, facing[direction]);
    const material::Color chamferInk =
        light > 0
            ? material::Color{kSunlight.r, kSunlight.g, kSunlight.b,
                              std::min(light * 0.32f, 0.8f)}
            : material::Color{kShade.r, kShade.g, kShade.b, -light * 0.6f};
    figures.push_back(pathFigure(outlineOf(chamfers[direction], true))
                          .fill(Fill::color(chamferInk)));
    // The joint behind a side turned from the sun lies in that sett's shadow.
    const float away = -glm::dot(facing[direction], sun.towards);
    if (away > 0)
      figures.push_back(
          pathFigure(outlineOf(joints[direction], true))
              .fill(Fill::color({kShade.r, kShade.g, kShade.b, away * 0.75f})));
  }
  return figures;
}

/** ONE GENERATION AS PAVING under @p sun: mortar, the setts of every bed of
 *  both quarries, their chamfers and joints, and the pool of the light. */
Element paving(const std::vector<Sett>& setts, const Sun& sun) {
  std::array<std::array<std::vector<std::vector<glm::vec2>>, kBedsPerQuarry>, 2>
      beds;
  for (const Sett& sett : setts) {
    const Quad face = inset(sett.corners(), kJoint / 2);
    beds[sett.fat][bedOf(sett)].push_back({face.begin(), face.end()});
  }
  std::vector<Element> stone;
  for (bool fat : {false, true})
    for (size_t bed = 0; bed < kBedsPerQuarry; ++bed)
      if (!beds[fat][bed].empty())
        stone.push_back(
            pathFigure(outlineOf(beds[fat][bed], true)).fill(quarry(fat, bed)));
  return stack()
      .inset(0)
      .fill(Fill::color(kMortar))
      .children({
          stone,
          relief(setts, sun),
          box()
              .inset(0)
              .blendMode(material::BlendMode::Multiply)
              .fill(material::radialGradient(
                  sun.pool, 1500,
                  {{0.0f, sun.light[0]},
                   {0.4f, sun.light[1]},
                   {0.8f, sun.light[2]},
                   {1.0f, sun.light[3]}},
                  {.units = material::GradientUnits::Pixels})),
      });
}

/** THE MASON'S CHALK: every side of a generation's setts, marked on the
 *  stone of the generation before. */
Element chalk(const std::vector<Sett>& setts) {
  std::vector<std::vector<glm::vec2>> sides;
  for (const Sett& sett : setts) {
    const Quad corners = sett.corners();
    sides.push_back({corners.begin(), corners.end()});
  }
  return stack().inset(0).children(
      {pathFigure(outlineOf(sides, true), 2)
           .stroke(stroke(1.6f, Fill::color(kChalk)))});
}

/** AN ARC round @p corner, from its side towards @p one to its side towards
 *  @p two, at @p radius. */
std::vector<glm::vec2> arc(glm::vec2 corner, glm::vec2 one, glm::vec2 two,
                           float radius) {
  const float start = std::atan2(one.y - corner.y, one.x - corner.x);
  const float sweep = std::remainder(
      std::atan2(two.y - corner.y, two.x - corner.x) - start, 2 * kPi);
  const int steps = std::max(6, (int)std::ceil(std::abs(sweep) / (kPi / 45)));
  std::vector<glm::vec2> points;
  for (int step = 0; step <= steps; ++step) {
    const float angle = start + sweep * step / steps;
    points.push_back(corner +
                     radius * glm::vec2{std::cos(angle), std::sin(angle)});
  }
  return points;
}

/** THE MATCHING RULES. Every sett carries a gold arc round its `first`
 *  corner and a blue one round its `second`. Blue always has radius
 *  edge/φ³, so it meets across every side it crosses. Gold has radius
 *  edge/φ on a fat sett and edge/φ² on a thin one, and crosses the sides
 *  at a point edge/φ from the corner a fat sett calls `first` and a thin
 *  one calls its apex — a point both setts on a side agree on only when
 *  the side is laid the way the rules allow. */
Element rules(const std::vector<Sett>& setts) {
  std::vector<std::vector<glm::vec2>> gold, blue;
  for (const Sett& sett : setts) {
    const float goldRadius = kEdge / (sett.fat ? kGolden : kGolden * kGolden);
    gold.push_back(arc(sett.first, sett.apex, sett.far, goldRadius));
    blue.push_back(arc(sett.second, sett.apex, sett.far,
                       kEdge / (kGolden * kGolden * kGolden)));
  }
  auto glowing = [](const path::Outline& outline, material::Color colour) {
    return pathFigure(outline, 14)
        .stroke(stroke(3.4f, Fill(material::from(colour).effects(
                                 Filter::shadow(colour, {.blur = 5})))));
  };
  return stack().inset(0).children({
      box().inset(0).fill(hexColor(0x07090D, 0.2f)),
      glowing(outlineOf(gold, false), kGold),
      glowing(outlineOf(blue, false), kBlue),
  });
}

/** A generation in words: its cut, its setts, and their ratio closing on φ. */
Utf8 tally(sketch::kit::Document words, int cuts,
           const std::vector<Sett>& setts) {
  int fat = 0;
  for (const Sett& sett : setts) fat += sett.fat;
  const int thin = (int)setts.size() - fat;
  words.figures({{"cut", std::to_string(cuts)},
                 {"fat", std::to_string(fat)},
                 {"thin", std::to_string(thin)},
                 {"ratio", fat ? kit::formatted("%.3f", (double)fat / thin)
                               : std::string("—")}});
  return words.phrase(cuts == 0 ? "tally.sun" : "tally.cut");
}

}  // namespace

struct PenrosePaving {
  sketch::kit::Document words;
  std::vector<std::vector<Sett>> generations;
  /** The loop's own clock, and how many cuts the paving stands at: whole at
   *  a finished generation, between two while the next is chalked and cut. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);
  motion::Animatable<float> depth = motion::animatable(0.0f);

  /** Shown from the moment a generation's recut begins until the next one
   *  has covered it. */
  motion::Animatable<float> cut(int generation) const {
    const float span = 2;  // from the cut before this one to the cut after
    return motion::bind(depth, {.from = {generation - 1.0f, generation + 1.0f},
                                .clampFrom = true,
                                .envelope = motion::envelope::trapezoid(
                                    kMarking / span, 1 / span, 1, 1)});
  }
  /** Shown while a generation is being marked out, gone as it is recut. */
  motion::Animatable<float> marking(int generation) const {
    return motion::bind(depth, {.from = {generation - 1.0f, (float)generation},
                                .clampFrom = true,
                                .envelope = motion::envelope::trapezoid(
                                    0, kMarking, kMarking, 1)});
  }
  /** A generation's tally, handed to the next halfway through its cut. */
  motion::Animatable<float> tallied(int generation) const {
    return motion::bind(depth, {.from = {generation - 1.0f, generation + 1.0f},
                                .clampFrom = true,
                                .envelope = motion::envelope::trapezoid(
                                    0.225f, 0.275f, 0.725f, 0.775f)});
  }
  /** The rules drawn over the finished paving, and gone before it is
   *  gathered back. */
  motion::Animatable<float> showingRules() const {
    return motion::bind(seconds, {.from = {kLaid + 2, kGathered - 1},
                                  .clampFrom = true,
                                  .envelope = motion::envelope::trapezoid(
                                      0, 0.1f, 0.75f, 0.85f)});
  }

  Element cartouche() const {
    const weave::Face engraved =
        weave::ports::face({"Optima", "Gill Sans", "Avenir Next"}, 500);
    std::vector<Element> tallies;
    for (int cuts = 0; cuts <= kGenerations; ++cuts)
      tallies.push_back(
          kit::at(text(tally(words, cuts, generations[(size_t)cuts])), 80, 1100,
                  700, 18)
              .font({.face = engraved,
                     .size = 12.5f,
                     .color = kLettering,
                     .track = 1.6f})
              .opacity(tallied(cuts)));
    return stack().inset(0).children({
        kit::at(
            box()
                .fill(material::from(kPanel).effects(Filter::shadow(
                    hexColor(0x000000, 0.55f), {.blur = 22, .offset = {0, 6}})))
                .stroke(stroke(1, Fill::color(kPanelRule),
                               PathFormat::Align::Inner)),
            56, 1010, 760, 150)
            .column()
            .padding(24)
            .gap(10)
            .cache(Cache::Texture)
            .key("cartouche")
            .children({text(words.phrase("title"))
                           .font({.face = engraved,
                                  .size = 24,
                                  .color = kLettering,
                                  .track = 6}),
                       text(words.phrase("tiling"))
                           .font({.face = engraved,
                                  .size = 11.5f,
                                  .color = kLetteringQuiet,
                                  .track = 1.8f})}),
        tallies,
        kit::at(text(words.phrase("rules")), 80, 1127, 720, 16)
            .font({.face = engraved,
                   .size = 11.5f,
                   .color = kGold,
                   .track = 1.6f})
            .opacity(showingRules()),
    });
  }

  void setup(sketch::SketchContext& context) {
    // Captured in the evening light with the rules drawn on the finished
    // paving; the loop begins at the sun, so the moment is late in it.
    sketch::kit::stage(
        context,
        {.size = {kWidth, kHeight}, .captureAt = 22, .background = kMortar});
    words = sketch::kit::Document(context, "data/content.json");
    for (int cuts = 0; cuts <= kGenerations; ++cuts)
      generations.push_back(generation(cuts));
    const std::vector<Sett>& laid = generations.back();

    // Each generation, and the rules, stand still once laid, so each is one
    // image; only which of them shows, and how much, moves.
    std::vector<Element> layers;
    for (int cuts = 0; cuts <= kGenerations; ++cuts) {
      layers.push_back(paving(generations[(size_t)cuts], kMorning)
                           .key("generation." + std::to_string(cuts))
                           .cache(Cache::Texture)
                           .opacity(cut(cuts)));
      if (cuts > 0)
        layers.push_back(chalk(generations[(size_t)cuts])
                             .key("chalk." + std::to_string(cuts))
                             .cache(Cache::Texture)
                             .opacity(marking(cuts)));
    }
    context.composer.render(
        stack()
            .fill(Fill::color(kMortar))
            .children({
                layers,
                // The finished paving in the evening, over the same paving in
                // the morning: the sun swings across it by the one fading into
                // the other.
                paving(laid, kEvening)
                    .key("generation.evening")
                    .cache(Cache::Texture)
                    .opacity(motion::bind(
                        seconds, {.from = {kLaid, kGathered},
                                  .clampFrom = true,
                                  .envelope = motion::envelope::cosine()})),
                rules(laid)
                    .key("rules")
                    .cache(Cache::Texture)
                    .opacity(showingRules()),
                cartouche(),
            }));
  }

  void update(double elapsed, sketch::SketchContext&) {
    const float now = std::fmod((float)elapsed, kLoop);
    seconds = now;
    // Each cut moves in the first four fifths of its time and rests after.
    auto cuts = [](float progress) {
      const float whole = std::floor(progress);
      const float within = std::clamp((progress - whole) / 0.8f, 0.0f, 1.0f);
      return std::min(whole + within * within * (3 - 2 * within),
                      (float)kGenerations);
    };
    depth = now < kGathered
                ? cuts(std::max(now - kFirstCut, 0.0f) / kCutEvery)
                : kGenerations - cuts((now - kGathered) / kGatherEvery);
  }
};

SIGIL_SKETCH(PenrosePaving, "Study · Pattern",
             "A Penrose paving laid by deflation — limestone and slate rhombs "
             "cut from a sun, raked by a low sun, their matching rules drawn")
