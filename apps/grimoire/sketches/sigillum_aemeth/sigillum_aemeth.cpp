// John Dee's Sigillum Dei Aemeth, the seal the angels dictated at Mortlake
// in March 1582, as the cake of pale beeswax it was made in, lying beside a
// leaf that reads it.
//
// The seal, from the rim inward: the greatest Circle, cut into forty cells,
// each a letter with a number above or below it; the heptagon, a band of
// forty-nine cells carrying the seven Angles and within it a band carrying
// the seven Names of God; the heptagram {7/2}, one band woven over and
// under itself, with the Daughters of Light in its points and the Sons of
// Light and little crosses in the band; two heptagons within it lettered
// with the Daughters of the Daughters and the Sons of the Sons; ZABATHIEL
// round the innermost; the pentagram of the five planetary angels, woven
// like the star; and the cross of LEVANAEL at the centre. Every band reads
// clockwise with the feet of its letters toward the centre, as a seal is
// read by turning it.
//
// The wax is one height field — the face of the cake, the floor of a cut,
// the channel of a band — carved once and lit from the upper left by a
// program over it, so every cut has a lit wall and a shadowed one.
//
// Over it the seal is used as the instrument it is: a lamp walks each of
// the seven Names off the rim, stepping as many cells as the number says,
// right for a number above and left for one below, and the letters it lands
// on are rubricated on the wax and gathered on the leaf.
//
// Every letter stands in data/: the forty cells (ring.csv), the seven Names
// and the cell each begins at (names.csv), the seven Angles (angles.csv),
// the Names of God (god.csv), the Children of Light (children.csv), the
// planetary angels (planets.csv) and the leaf's words (content.json).

// TAGS: Geometry/Diagrams, Patterns/Ornament

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace data = sigil::data;
namespace document = sigil::compose::document;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using material::Filter;
using material::hexColor;
using namespace sigil::compose;

namespace {

constexpr float kWidth = 2000, kHeight = 1417;

// THE HEIGHT FIELD the wax is carved in: grey is how high the wax stands.
// The face of the cake, the shallow channel a band is cut as, the floor of
// every cut, and the fall of the cake's edge.
const auto kFace = hexColor(0x9e9e9e);
const auto kChannel = hexColor(0x858585);
const auto kCut = hexColor(0x1e1e1e);
const auto kEdge = hexColor(0x2a2a2a);

// The room and the leaf: a dark case, a vellum leaf in iron-gall ink with
// its rubrics in red, and the lamp's gilt.
const auto kVitrine = hexColor(0x13110d);
const auto kVellum = hexColor(0xeadcc0);
const auto kInk = hexColor(0x33241a);
const auto kFaded = hexColor(0x80684c);
const auto kRubric = hexColor(0x9e3322);
const auto kGilt = hexColor(0xffd57e);
const auto kVermilion = hexColor(0xc8321b);

// THE CAKE and the greatest Circle cut in it, in canvas pixels.
constexpr float kDisc = 642;
constexpr glm::vec2 kCentre{688, 708.5f};
constexpr float kRadius = 604;

/** The seal's polar frame in the carving's own box: every angle is
 *  measured clockwise from twelve o'clock, which is how Dee gives each
 *  instruction — "the begynning of the greatest Circle … and so procede
 *  toward thy right hand". */
const path::PolarFrame kSeal{.centre = {kDisc, kDisc}, .radius = kRadius};

constexpr float kSeventh = 360.0f / 7.0f;
constexpr float kHalfSeventh = kSeventh / 2.0f;
/** A heptagon's side stands this far in from its vertex, as a fraction. */
const float kApothem = std::cos(std::numbers::pi_v<float> / 7.0f);
/** The vertex radius of the heptagon whose sides stand at @p apothem. */
float vertexOf(float apothem) { return apothem / kApothem; }

// THE RADIUS TABLE, in units of the greatest Circle, outside in. The rim's
// cells lie outside kRimInner; a cell's number stands above its letter when
// it steps right and below it when it steps left.
constexpr float kRimInner = 0.880f;
constexpr float kRimLetter = 0.936f, kNumberAbove = 0.977f,
                kNumberBelow = 0.898f;
// The heptagon's vertices stand on the rim's inner circle; its three sides
// bound the Angles' cells and the Names of God.
const float kHeptagonSide = kRimInner * kApothem;
constexpr float kCellsSide = 0.742f, kNamesSide = 0.688f;
// The star's vertices stand on the heptagon's innermost vertices.
const float kStarVertex = vertexOf(kNamesSide);
constexpr float kStarBand = 0.056f;
// The two heptagons within the star stand flat side up, between the
// star's inner edge and the innermost heptagon.
constexpr float kInnerSides[] = {0.448f, 0.384f, 0.320f};
constexpr float kPentagramVertex = 0.300f, kPentagramBand = 0.030f;
constexpr float kZabathiel = 0.268f;

// A little cross pattée, as Dee drew them at the corners of the segments
// and along the star's band.
constexpr const char* kPattee =
    "M8 0H16L13 11L24 8V16L13 13L16 24H8L11 13L0 16V8L11 11Z";
// The cross of LEVANAEL, its foot longer than its arms.
constexpr const char* kLatinCross = "M9 0H15V9H24V15H15V34H9V15H0V9H9Z";

// THE WALK, in seconds: the lamp comes up on a Name's first cell, rests on
// each letter it lands on, and counts its way to the next cell by cell.
constexpr float kLead = 0.6f;     // before the first Name
constexpr float kRise = 0.45f;    // the lamp coming up
constexpr float kDwell = 0.55f;   // resting on a letter
constexpr float kStep = 0.055f;   // counting one cell
constexpr float kHold = 1.1f;     // resting on the letter that ends a Name
constexpr float kBetween = 0.6f;  // dark, between Names
constexpr float kRest = 4.0f;     // every Name gathered, before it begins again
constexpr float kConsumed = 0.45f;  // how much of a walked cell's red stays

/** A run split into its characters, each whole however many bytes it is. */
std::vector<std::string> characters(const std::string& run) {
  std::vector<std::string> split;
  for (size_t at = 0; at < run.size();) {
    size_t length = 1;
    while (at + length < run.size() && (run[at + length] & 0xC0) == 0x80)
      ++length;
    split.push_back(run.substr(at, length));
    at += length;
  }
  return split;
}

struct Cell {
  std::string letter;
  int number;  // positive steps right, negative left, zero ends a Name
};

/** ONE NAME AS THE LAMP WALKS IT: the cells, counted from one as the rim
 *  numbers them, and the moment the lamp lands on each. */
struct Walk {
  std::string name;
  std::vector<int> cells;
  std::vector<float> landed;
  float begins = 0, ends = 0;
};

struct Order {
  std::string name, band;
  std::vector<std::string> names;
};

/** A NODE THE SIZE OF A CIRCLE of the seal at @p radius, centred on it:
 *  a shape's radius fractions are then fractions of that circle. */
Element circleOf(float radius = 1.0f) {
  return kit::disc(kSeal.centre, radius * kRadius);
}

/** @p node centred on @p point, turned @p degrees clockwise from upright. */
Element placed(Element node, glm::vec2 point, float degrees) {
  node.centerAt(point).rotate(degrees);
  return node;
}

/** @p node STANDING ON THE SEAL at @p degrees and @p radius, its foot
 *  toward the centre, which is how every letter on the seal stands. */
Element standing(Element node, float degrees, float radius) {
  return placed(std::move(node), kSeal.at(degrees, radius), degrees);
}

/** A RUN LETTERED ALONG SIDE @p side of the heptagon whose vertices stand
 *  at @p vertexRadius, the first at @p from degrees, its letters' bodies
 *  straddling the side. */
Element alongSide(Text run, float size, int side, float vertexRadius,
                  float from = 0) {
  return kit::at(std::move(run), kSeal.centre.x - kRadius,
                 kSeal.centre.y - kRadius, 2 * kRadius, 2 * kRadius)
      .textOnPath({.path = shapes::chords(
                       {.sides = 7, .radius = vertexRadius, .from = from}),
                   .at = ((float)side + 0.5f) / 7.0f,
                   .align = TextPath::Align::Center,
                   .offset = -size * 0.36f});
}

/** A cut @p width px wide. */
Decoration cut(float width) { return stroke(width, Fill::color(kCut)); }

/** A figure's outline cut into the wax. */
Element outlined(Element node, float width) {
  node.fill(Fill::none()).stroke(cut(width));
  return node;
}

/** A little cross, cut @p size px across. */
Element cross(float size) {
  return box()
      .width(size)
      .height(size)
      .shape(shapes::svg(kPattee))
      .fill(Fill::color(kCut));
}

/** A BAND CUT IN THE WAX: a shallow channel @p width px wide between two
 *  deep cuts, its corners met by @p join. Every layer is opaque, so a
 *  strand passing over another is repainted across it and its cuts run on
 *  unbroken. */
Decoration band(float width, path::Join join = path::Join::Round) {
  const float edge = width * 0.5f - 1.7f;
  const auto rail = [join](float across, float breadth,
                           material::Color height) {
    return lines::Rail{.across = across,
                       .width = breadth,
                       .fill = Fill::color(height),
                       .cap = path::Cap::Butt,
                       .join = join};
  };
  return lines::rails({rail(0, width, kChannel), rail(edge, 3.2f, kCut),
                       rail(-edge, 3.2f, kCut)});
}

/** THE STAR {sides/2} WOVEN IN ITS BAND. The band is cut once round the
 *  whole star, mitred, so its points are sharp on both edges. Over it each
 * chord is laid again across the stretch where it meets the others, one strand
 * per chord in the order the star is drawn without lifting the graver, so that
 * walking it the crossings go over, under, over — the rule that makes an
 * interlace read. A chord of {7/2} or {5/2} meets the others between three and
 * seven tenths of its length. */
Element interlaced(int sides, float vertexRadius, float width) {
  const float degrees = 360.0f / (float)sides;
  std::vector<brush::Strand> strands;
  for (int chord = 0; chord < sides; ++chord) {
    const glm::vec2 from =
        kSeal.at(degrees * (float)(2 * chord % sides), vertexRadius);
    const glm::vec2 to =
        kSeal.at(degrees * (float)(2 * (chord + 1) % sides), vertexRadius);
    strands.push_back({StrandPath::authored(path::toPath(path::Polyline{
                           .points = {from + (to - from) * 0.2f,
                                      from + (to - from) * 0.8f}})),
                       band(width * kRadius)});
  }
  return box().inset(0).children(
      {circleOf()
           .shape(shapes::chords({.sides = sides,
                                  .step = 2,
                                  .radius = vertexRadius,
                                  .closed = true}))
           .fill(Fill::none())
           .stroke(band(width * kRadius, path::Join::Miter)),
       box()
           .inset(0)
           .fill(Fill::none())
           .stroke(Decoration(brush::weave(
               std::move(strands), path::crossing::alternateAlong())))});
}

/** @p fraction of the way along the star's chord whose middle faces
 *  @p degrees. */
glm::vec2 alongChord(float degrees, float fraction) {
  const glm::vec2 from = kSeal.at(degrees - kSeventh, kStarVertex);
  const glm::vec2 to = kSeal.at(degrees + kSeventh, kStarVertex);
  return from + (to - from) * fraction;
}

struct SigillumAemeth {
  std::vector<Cell> ring;
  std::vector<Walk> walks;
  std::vector<std::vector<std::string>> angleRows;
  std::vector<std::string> godNames;
  std::vector<Order> orders;
  std::vector<std::string> initials, tails;
  sketch::kit::Document words;
  StyleSheet registers;
  std::optional<material::Material> relief;
  float loopLength = 60;

  // THE DEMONSTRATION'S LIVE VALUES: seconds into the walk, where the lamp
  // stands on the rim and how bright it is, and how far each cell's letter
  // is rubricated.
  motion::Animatable<float> score = motion::animatable(0.0f);
  motion::Animatable<float> lampTurn = motion::animatable(0.0f);
  motion::Animatable<float> lampLight = motion::animatable(0.0f);
  std::array<motion::Animatable<float>, 40> rubricated{};

  template <class Read>
  static void rows(sketch::SketchContext& context, const char* file,
                   Read read) {
    if (const auto table = context.assets.hub().load<data::Table>(
            context.local(std::string("data/") + file)))
      read(*table);
  }

  /** MICHAEL'S RULE, walked: from the Name's first cell, step as many
   *  cells as the number says, right when it stands above the letter and
   *  left when below, until a letter with no number ends the Name. */
  std::vector<int> walk(int start) const {
    std::vector<int> cells;
    int cell = start - 1;
    for (int step = 0; step < 40; ++step) {
      cells.push_back(cell + 1);
      if (ring[(size_t)cell].number == 0) break;
      cell = ((cell + ring[(size_t)cell].number) % 40 + 40) % 40;
    }
    return cells;
  }

  const Cell& cellAt(int counted) const { return ring[(size_t)counted - 1]; }

  /** A binding of the score that rises from @p low to @p high over the
   *  moment @p from and holds there until the walk begins again. */
  motion::Animatable<float> holdFrom(float from, float low, float high) const {
    const float span = loopLength - from;
    return motion::bind(score, {.from = {from, loopLength},
                                .clampFrom = true,
                                .envelope = motion::envelope::trapezoid(
                                    0, 0.3f / span, 1 - 0.8f / span, 1),
                                .to = {low, high}});
  }

  // -------------------------------------------------------------------------
  // THE CARVING — a height field, lit by the relief program over it

  /** THE CAKE: its face, lumpy where it cooled, falling away at its edge. */
  Element cake() const {
    return kit::disc(kSeal.centre, kDisc)
        .shape(shapes::circle())
        .fill(
            material::from(material::radialGradient(
                               {0.5f, 0.5f}, 1.0f,
                               {{0.0f, kFace},
                                {0.955f, kFace},
                                {0.985f, hexColor(0x6a6a6a)},
                                {1.0f, kEdge}},
                               {.extent = material::RadialExtent::ClosestSide}))
                .layer(
                    material::noise(0.006f, {.octaves = 3, .seed = 1582}),
                    {.blend = material::BlendMode::SoftLight, .opacity = 0.22f})
                .layer(material::noise(
                           0.22f, {.octaves = 2, .seed = 3188, .grain = true}),
                       {.blend = material::BlendMode::SoftLight,
                        .opacity = 0.10f}));
  }

  /** THE GREATEST CIRCLE: forty cells of nine degrees, the first at twelve
   *  o'clock, each a letter with its number above or below it. */
  Element rim() const {
    std::vector<Element> marks;
    for (int cell = 0; cell < 40; ++cell) {
      const Cell& entry = ring[(size_t)cell];
      const float degrees = 9.0f * (float)cell;
      marks.push_back(
          standing(text(entry.letter).styleClass("hand"), degrees, kRimLetter));
      if (entry.number != 0)
        marks.push_back(standing(
            text(std::to_string(std::abs(entry.number))).styleClass("numeral"),
            degrees, entry.number > 0 ? kNumberAbove : kNumberBelow));
    }
    return box().inset(0).children(
        {kit::ring(kSeal.centre, kRadius, cut(4.2f)),
         kit::ring(kSeal.centre, kRimInner * kRadius, cut(3.2f)),
         outlined(
             circleOf().shape(shapes::ticks(
                 {.divisions = 40, .from = 4.5f, .mark = {kRimInner, 1.0f}})),
             2.4f),
         marks});
  }

  /** THE HEPTAGON: its outer band of forty-nine cells holding the seven
   *  Angles, a row of seven letters on each side, and within it the band of
   *  the seven Names of God; in the seven segments between the heptagon
   *  and the rim, "at each corner of these segments of circles, to make
   *  little Crosses". */
  Element heptagon() const {
    const float middle = (kHeptagonSide + kCellsSide) * 0.5f;
    const float halfSide = middle * std::tan(std::numbers::pi_v<float> / 7.0f);
    // A point on the line @p apothem in from side @p side's middle, @p along
    // of its half-length clockwise of it.
    const auto onSide = [](int side, float apothem, float along) {
      const float degrees = ((float)side + 0.5f) * kSeventh;
      const float radians = kSeal.screenRadians(degrees);
      const glm::vec2 tangent{-std::sin(radians), std::cos(radians)};
      return kSeal.at(degrees, apothem) + tangent * (along * kRadius);
    };
    path::Outline dividers;
    std::vector<Element> letters, crosses;
    for (int side = 0; side < 7; ++side) {
      const float degrees = ((float)side + 0.5f) * kSeventh;
      for (int cell = 0; cell < 7; ++cell) {
        const float at =
            halfSide * (-1.0f + (2.0f * (float)cell + 1.0f) / 7.0f);
        letters.push_back(placed(
            text(angleRows[(size_t)side][(size_t)cell]).styleClass("angle"),
            onSide(side, middle, at), degrees));
        if (cell == 0) continue;
        const float edge = halfSide * (-1.0f + 2.0f * (float)cell / 7.0f);
        dividers = dividers.joined(path::toPath(path::Polyline{
            .points = {
                onSide(side, kHeptagonSide, edge * kHeptagonSide / middle),
                onSide(side, kCellsSide, edge * kCellsSide / middle)}}));
      }
      for (const auto& [offset, radius] : {std::pair{9.0f, 0.853f},
                                           {kHalfSeventh, 0.836f},
                                           {kSeventh - 9.0f, 0.853f}})
        crosses.push_back(
            standing(cross(19), (float)side * kSeventh + offset, radius));
    }
    return box().inset(0).children(
        {each(std::array{kHeptagonSide, kCellsSide, kNamesSide},
              [](float apothem) {
                return outlined(circleOf().shape(
                                    shapes::chords({.sides = 7,
                                                    .radius = vertexOf(apothem),
                                                    .closed = true})),
                                3.0f);
              }),
         box()
             .inset(0)
             .shape(heldPath(dividers))
             .fill(Fill::none())
             .stroke(cut(2.2f)),
         letters, crosses, each(7, [this](int side) {
           return alongSide(text(godNames[(size_t)side]).styleClass("god"), 25,
                            side, vertexOf((kCellsSide + kNamesSide) * 0.5f));
         })});
  }

  /** THE HEPTAGRAM, woven in its band: a Daughter of Light and a cross in
   *  each point, and a Son of Light in the middle of each strand between
   *  crosses, three to a side. */
  Element heptagram() const {
    const Order* daughters = order("points");
    const Order* sons = order("strands");
    std::vector<Element> lettering;
    for (int point = 0; point < 7; ++point) {
      const float degrees = (float)point * kSeventh;
      if (daughters)
        lettering.push_back(
            standing(text(daughters->names[(size_t)point]).styleClass("point"),
                     degrees, 0.598f));
      lettering.push_back(standing(cross(22), degrees, 0.672f));
      if (sons)
        lettering.push_back(
            placed(text(sons->names[(size_t)point]).styleClass("strand"),
                   alongChord(degrees, 0.5f), degrees));
      for (float fraction : {0.10f, 0.20f, 0.365f, 0.635f, 0.80f, 0.90f})
        lettering.push_back(
            placed(cross(17), alongChord(degrees, fraction), degrees));
    }
    return box().inset(0).children(
        {interlaced(7, kStarVertex, kStarBand), lettering});
  }

  /** THE TWO HEPTAGONS WITHIN THE STAR, flat side up: the Daughters of the
   *  Daughters on the outer and the Sons of the Sons on the inner, each
   *  name between little crosses. */
  Element within() const {
    std::vector<Element> runs;
    for (const auto& [band, outer, inner] :
         {std::tuple{"outer", kInnerSides[0], kInnerSides[1]},
          std::tuple{"inner", kInnerSides[1], kInnerSides[2]}})
      if (const Order* children = order(band))
        for (int side = 0; side < 7; ++side)
          runs.push_back(alongSide(
              text("+++ " + children->names[(size_t)side] + " +++")
                  .styleClass("heptagon"),
              17, side, vertexOf((outer + inner) * 0.5f), -kHalfSeventh));
    return box().inset(0).children(
        {each(kInnerSides,
              [](float apothem) {
                return outlined(circleOf().shape(
                                    shapes::chords({.sides = 7,
                                                    .radius = vertexOf(apothem),
                                                    .from = -kHalfSeventh,
                                                    .closed = true})),
                                2.6f);
              }),
         runs});
  }

  /** THE CENTRE: ZABATHIEL "in his letters into 7 sides of that innermost
   *  Heptagonum"; the pentagram, point up on the first cell, woven, each
   *  planetary angel's initial in its point and the rest of the name in
   *  the angle beside it; and the cross of LEVANAEL, read left, top, right,
   *  foot. */
  Element centre() const {
    const data::Json& zabathiel = words["zabathiel"];
    const data::Json& zabathielDegrees = words["zabathielDegrees"];
    const data::Json& levanael = words["levanael"];
    const float tailRadius = 0.188f;
    return box().inset(0).children(
        {each(7,
              [&](int side) {
                return standing(
                    text(std::string(zabathiel[(size_t)side].string()))
                        .styleClass("zabathiel"),
                    (float)zabathielDegrees[(size_t)side].number(), kZabathiel);
              }),
         interlaced(5, kPentagramVertex, kPentagramBand),
         each(initials.size(),
              [this](int point) {
                return standing(
                    text(initials[(size_t)point]).styleClass("initial"),
                    72.0f * (float)point, 0.196f);
              }),
         each(tails.size(),
              [&](int point) {
                return kit::at(text(tails[(size_t)point]).styleClass("tail"),
                               kSeal.centre.x - tailRadius * kRadius,
                               kSeal.centre.y - tailRadius * kRadius,
                               2 * tailRadius * kRadius,
                               2 * tailRadius * kRadius)
                    .textOnPath(
                        {.path = shapes::circle(),
                         .at = kSeal.fraction(72.0f * (float)point + 36.0f),
                         .align = TextPath::Align::Center,
                         .offset = -4.5f});
              }),
         placed(box()
                    .width(32)
                    .height(46)
                    .shape(shapes::svg(kLatinCross))
                    .fill(Fill::color(kChannel))
                    .stroke(cut(2.4f)),
                kSeal.at(180, 0.012f), 0),
         each(4, [&](int arm) {
           return placed(
               text(std::string(levanael[(size_t)arm].string()))
                   .styleClass("syllable"),
               kSeal.at(90.0f * (float)arm, arm == 2 ? 0.080f : 0.064f), 0);
         })});
  }

  const Order* order(std::string_view band) const {
    for (const Order& candidate : orders)
      if (candidate.band == band && candidate.names.size() >= 7)
        return &candidate;
    return nullptr;
  }

  /** THE SEAL: the carving, baked once as the lit wax it describes. */
  Element carving() const {
    Element carved =
        kit::at(kCentre.x - kDisc, kCentre.y - kDisc, 2 * kDisc, 2 * kDisc)
            .ink(kCut)
            .cache(Cache::Texture)
            .key("carving")
            .children(
                {cake(), rim(), heptagon(), heptagram(), within(), centre()});
    if (relief) carved.filter(Filter::of(*relief, 4.0f));
    return carved;
  }

  // -------------------------------------------------------------------------
  // THE DEMONSTRATION — the lamp on the rim and the red it leaves

  /** THE LAMP: a pool of warm light and a gilt frame round the cell it
   *  stands on, standing at twelve o'clock and turned round the seal's
   *  centre to wherever the walk has brought it. */
  Element lamp() const {
    constexpr float kSize = 132;
    const float reach = kRimLetter * kRadius;
    const glm::vec2 local{kSize * 0.5f, kSize * 0.5f + reach};
    const path::Outline frame =
        shapes::ellipse(
            {.fromDegrees = -94.5f, .sweepDegrees = 9.0f, .inner = kRimInner})
            .outline({2 * kRadius, 2 * kRadius})
            .transformed(path::Transform::translate(
                local - glm::vec2{kRadius, kRadius}));
    return kit::at(kCentre.x - kSize * 0.5f, kCentre.y - reach - kSize * 0.5f,
                   kSize, kSize)
        .hitTestable(false)
        .transformOrigin(Dimension(local.x), Dimension(local.y))
        .rotate(lampTurn)
        .opacity(lampLight)
        .children(
            {box()
                 .inset(0)
                 .cache(Cache::Texture)
                 .key("lamp")
                 .children(
                     {kit::disc({kSize * 0.5f, kSize * 0.5f}, kSize * 0.5f)
                          .fill(material::radialGradient(
                              {0.5f, 0.5f}, 1.0f,
                              {{0.0f, hexColor(0xffcf73, 0.55f)},
                               {0.45f, hexColor(0xffb347, 0.26f)},
                               {1.0f, hexColor(0xff9a30, 0.0f)}},
                              {.extent = material::RadialExtent::ClosestSide})),
                      box()
                          .inset(0)
                          .shape(heldPath(frame))
                          .fill(hexColor(0xffd57e, 0.10f))
                          .stroke(stroke(2.0f, Fill::color(kGilt)))})});
  }

  /** THE RUBRICATION: every rim letter again, its cut filled with
   *  vermilion — shown as the lamp lands on it while its Name is walked,
   *  and faintly held once the Name is gathered. */
  Element rubrication() const {
    return kit::at(kCentre.x - kDisc, kCentre.y - kDisc, 2 * kDisc, 2 * kDisc)
        .hitTestable(false)
        .children(each(40, [this](int cell) {
          return standing(
                     text(ring[(size_t)cell].letter)
                         .styleClass("hand")
                         .ink(kVermilion)
                         .filter(Filter::glow(hexColor(0xffb45a, 0.85f), 4.0f))
                         .cache(Cache::Texture),
                     9.0f * (float)cell, kRimLetter)
              .opacity(rubricated[(size_t)cell]);
        }));
  }

  // -------------------------------------------------------------------------
  // THE LEAF

  /** ONE CELL OF A NAME, as the rim carries it: its letter, the number
   *  above or below it, and which cell of the forty it is. */
  Element tile(int counted, float landed) const {
    const Cell& cell = cellAt(counted);
    const std::string number =
        cell.number ? std::to_string(std::abs(cell.number)) : "";
    return box()
        .column()
        .width(33)
        .alignItems(Align::Center)
        .opacity(holdFrom(landed, 0.2f, 1.0f))
        .children(
            {text(cell.number > 0 ? number : "").styleClass("step").height(14),
             text(cell.letter).styleClass("letter").height(32),
             text(cell.number < 0 ? number : "").styleClass("step").height(14),
             text(std::to_string(counted)).styleClass("cell")});
  }

  /** ONE NAME ON THE LEAF: the hand pointing while it is walked, its cells
   *  lighting as the lamp lands on them, and the Name once it is whole. */
  Element walkRow(const Walk& walk, size_t index) const {
    return box()
        .row()
        .alignItems(Align::Center)
        .gap(2)
        .children(
            {text("☞")
                 .styleClass("hand-pointing")
                 .width(24)
                 .opacity(motion::bind(score,
                                       {.from = {walk.begins, walk.ends + 0.4f},
                                        .clampFrom = true,
                                        .envelope = motion::envelope::trapezoid(
                                            0, 0.05f, 0.95f, 1)})),
             text(std::to_string(index + 1)).styleClass("index").width(18),
             each(walk.cells.size(),
                  [&](int step) {
                    return tile(walk.cells[(size_t)step],
                                walk.landed[(size_t)step]);
                  }),
             box().flexGrow(1),
             text(walk.name).styleClass("name").opacity(
                 holdFrom(walk.ends - 0.3f, 0, 1))});
  }

  Element doubleRule() const {
    return box().column().gap(3).children(
        {kit::line({.thickness = 1.4f, .fill = Fill::color(kRubric)}),
         kit::line({.thickness = 0.6f, .fill = Fill::color(kRubric)})});
  }

  Element leaf() const {
    return kit::at(1438, 104, 476, 1210)
        .column()
        .gap(14)
        .children(
            {box()
                 .column()
                 .gap(7)
                 .cache(Cache::Texture)
                 .key("leaf.head")
                 .children({document::h1(words.phrase("title")),
                            document::lead(words.phrase("subtitle")),
                            text(words.phrase("provenance")).styleClass("note"),
                            doubleRule(),
                            document::h2(words.phrase("namesHeading")),
                            text(words.phrase("rule")).styleClass("plain")}),
             box().column().gap(9).children(each(
                 walks, [this](const Walk& walk,
                               size_t index) { return walkRow(walk, index); })),
             box()
                 .column()
                 .gap(9)
                 .flexGrow(1)
                 .cache(Cache::Texture)
                 .key("leaf.foot")
                 .children(
                     {text(words.phrase("consumed")).styleClass("note"),
                      doubleRule(), document::h2(words.phrase("anglesHeading")),
                      box()
                          .column()
                          .alignItems(Align::Center)
                          .children(each(
                              angleRows,
                              [](const std::vector<std::string>& row) {
                                return box().row().children(
                                    each(row, [](const std::string& letter) {
                                      return text(letter)
                                          .styleClass("basket")
                                          .width(46)
                                          .paragraph({.alignment =
                                                          weave::TextAlignment::
                                                              kCenter});
                                    }));
                              })),
                      text(words.phrase("archangels")).styleClass("note"),
                      text(words.phrase("crossNote")).styleClass("note"),
                      document::h2(words.phrase("ordersHeading")),
                      box().column().gap(3).children(
                          each(words.run("orders"),
                               [](const sketch::kit::Document::Line& line) {
                                 return text(line.words).styleClass("plain");
                               })),
                      box().flexGrow(1), doubleRule(),
                      text(words.phrase("seal")).styleClass("quote"),
                      text(words.phrase("imprint")).styleClass("note")})});
  }

  // -------------------------------------------------------------------------

  /** THE CASE, drawn once: dark cloth under a light from the upper left,
   *  the cake's shadow on it, and the vellum leaf on its own shadow. */
  Element ground() const {
    const float left = 1392, top = 62, width = 568, height = 1296;
    return box()
        .inset(0)
        .cache(Cache::Texture)
        .key("ground")
        .fill(material::from(
                  material::radialGradient({0.22f, 0.12f}, 1.25f,
                                           {{0.0f, hexColor(0x2a241c)},
                                            {0.55f, hexColor(0x17140f)},
                                            {1.0f, kVitrine}}))
                  .layer(material::noise(
                             0.9f, {.octaves = 2, .seed = 7, .grain = true}),
                         {.blend = material::BlendMode::SoftLight,
                          .opacity = 0.18f}))
        .children(
            {kit::disc(kCentre + glm::vec2{14, 22}, kDisc + 6)
                 .shape(shapes::circle())
                 .fill(hexColor(0x000000, 0.72f))
                 .filter(Filter::blur(22)),
             kit::at(left + 12, top + 18, width, height)
                 .fill(hexColor(0x000000, 0.6f))
                 .filter(Filter::blur(16)),
             kit::at(left, top, width, height)
                 .fill(material::from(material::linearGradient(
                                          {0.0f, 0.0f}, {1.0f, 1.0f},
                                          {{0.0f, hexColor(0xf1e6cd)},
                                           {0.6f, kVellum},
                                           {1.0f, hexColor(0xd6c29c)}}))
                           .layer(material::noise(0.012f,
                                                  {.octaves = 4, .seed = 30}),
                                  {.blend = material::BlendMode::SoftLight,
                                   .opacity = 0.35f})
                           .layer(material::noise(0.6f, {.octaves = 2,
                                                         .seed = 31,
                                                         .grain = true}),
                                  {.blend = material::BlendMode::SoftLight,
                                   .opacity = 0.2f}))
                 .foreground(decorations::border(
                     1.0f, Fill::color(hexColor(0x8a7450, 0.5f))))});
  }

  StyleSheet sheet() const {
    const weave::Face hand =
        weave::ports::face({"Baskerville", "Hoefler Text"}, 600);
    const weave::Face capitals =
        weave::ports::face({"Herculanum", "Optima", "Baskerville"});
    const weave::Face italic =
        weave::ports::face({"Baskerville", "Hoefler Text"},
                           weave::FaceStyle{.slant = weave::FaceSlant::Italic});
    const weave::Face book = sketch::kit::houseFace(sketch::kit::Voice::Book);
    const weave::Face mono =
        sketch::kit::houseFace(sketch::kit::Voice::Terminal);
    return StyleSheet{
        // The carving: every letter is a cut, so each takes the cut's ink.
        rule(".hand").font({.face = hand, .size = 29}),
        rule(".numeral").font({.face = hand, .size = 15}),
        rule(".angle").font({.face = hand, .size = 27}),
        rule(".god").font({.face = capitals, .size = 25, .track = 9}),
        rule(".point").font({.face = capitals, .size = 24, .track = 1}),
        rule(".strand").font({.face = capitals, .size = 21, .track = 2}),
        rule(".heptagon").font({.face = capitals, .size = 17, .track = 1}),
        rule(".zabathiel").font({.face = capitals, .size = 24}),
        rule(".initial").font({.face = capitals, .size = 22}),
        rule(".tail").font({.face = hand, .size = 15}),
        rule(".syllable").font({.face = capitals, .size = 15}),
        // The leaf.
        rule("h1").font(
            {.face = capitals, .size = 36, .color = kInk, .track = 2.5f}),
        rule("lead").font({.face = italic, .size = 18, .color = kInk}),
        rule("h2").font({.face = italic, .size = 19, .color = kRubric}),
        rule(".plain").font({.face = book, .size = 13.5f, .color = kInk}),
        rule(".note").font({.face = italic, .size = 14, .color = kFaded}),
        rule(".quote").font({.face = italic, .size = 16, .color = kInk}),
        rule(".hand-pointing")
            .font({.face = book, .size = 18, .color = kRubric}),
        rule(".index").font({.face = italic, .size = 15, .color = kRubric}),
        rule(".letter").font({.face = hand, .size = 26, .color = kInk}),
        rule(".step").font({.face = book, .size = 12, .color = kRubric}),
        rule(".cell").font({.face = mono, .size = 9.5f, .color = kFaded}),
        rule(".name").font(
            {.face = capitals, .size = 23, .color = kInk, .track = 0.5f}),
        rule(".basket").font({.face = hand, .size = 23, .color = kInk})};
  }

  /** WHEN THE LAMP LANDS on each cell of each Name, one Name after another,
   *  and how long the whole demonstration runs before it begins again. */
  void schedule() {
    float now = kLead;
    for (Walk& walk : walks) {
      walk.begins = now;
      walk.landed = {now + kRise};
      for (size_t step = 0; step + 1 < walk.cells.size(); ++step)
        walk.landed.push_back(
            walk.landed.back() + kDwell +
            kStep * (float)std::abs(cellAt(walk.cells[step]).number));
      walk.ends = walk.landed.back() + kHold;
      now = walk.ends + kBetween;
    }
    loopLength = now + kRest;
  }

  void setup(sketch::SketchContext& context) {
    rows(context, "ring.csv", [this](const auto& table) {
      const auto letter = table.template column<std::string>("letter");
      const auto number = table.template column<double>("number");
      for (size_t row = 0; row < letter.size(); ++row)
        ring.push_back({letter[row], (int)number[row]});
    });
    rows(context, "angles.csv", [this](const auto& table) {
      for (const std::string& row :
           table.template column<std::string>("letters"))
        angleRows.push_back(characters(row));
    });
    rows(context, "god.csv", [this](const auto& table) {
      for (const std::string& name :
           table.template column<std::string>("plate"))
        godNames.push_back(name);
    });
    rows(context, "children.csv", [this](const auto& table) {
      const auto name = table.template column<std::string>("order");
      const auto band = table.template column<std::string>("band");
      const auto names = table.template column<std::string>("names");
      for (size_t row = 0; row < name.size(); ++row) {
        Order order{name[row], band[row], {}};
        std::istringstream split(names[row]);
        for (std::string word; split >> word;) order.names.push_back(word);
        orders.push_back(std::move(order));
      }
    });
    rows(context, "planets.csv", [this](const auto& table) {
      for (const std::string& initial :
           table.template column<std::string>("initial"))
        initials.push_back(initial);
      for (const std::string& tail : table.template column<std::string>("tail"))
        tails.push_back(tail);
    });
    std::set<int> consumed;
    if (ring.size() == 40)
      rows(context, "names.csv", [&](const auto& table) {
        const auto name = table.template column<std::string>("name");
        const auto start = table.template column<double>("start");
        for (size_t row = 0; row < name.size(); ++row) {
          walks.push_back({.name = name[row], .cells = walk((int)start[row])});
          consumed.insert(walks.back().cells.begin(), walks.back().cells.end());
        }
      });
    if (angleRows.size() < 7 || godNames.size() < 7) return;
    for (const auto& row : angleRows)
      if (row.size() < 7) return;
    schedule();

    // Captured while the fifth Name is being walked: four gathered on the
    // leaf, the lamp on the rim, the rest still dim.
    sketch::kit::stage(
        context,
        {.size = {kWidth, kHeight},
         .captureAt = walks.size() > 4 ? walks[4].landed[2] + 0.2f : 1.0,
         .background = kVitrine});

    words = sketch::kit::Document(context, "data/content.json");
    words.figures({{"used", std::to_string(consumed.size())},
                   {"unvisited", std::to_string(40 - consumed.size())}});

    struct Relief {
      glm::vec4 uWax{0.84f, 0.73f, 0.52f, 1};
      glm::vec4 uGrime{0.24f, 0.17f, 0.10f, 1};
      glm::vec4 uLight{-0.55f, -0.66f, 0.60f, 0};
      glm::vec4 uLightColour{1.0f, 0.95f, 0.86f, 1};
      glm::vec4 uCake{kDisc, kDisc, kDisc, 0};
      float uDepth = 6.0f;
      float uAmbient = 0.40f;
      float uKey = 0.75f;
      float uSheen = 0.16f;
      float uFloor = 0.16f;
      float uFace = 0.58f;
      float uFalloff = 0.20f;
    };
    relief = material::shader(context.assets.hub(),
                              context.local("data/relief.sksl"), Relief{},
                              {.textures = {{"content", {}}}});
    registers = sheet();
    for (auto& level : rubricated) level = motion::animatable(0.0f);

    context.composer.render(box().inset(0).applyStyleSheet(registers).children(
        {ground(), carving(), rubrication(), lamp(), leaf()}));
  }

  void update(double elapsed, sketch::SketchContext&) {
    if (walks.empty()) return;
    const float now = std::fmod((float)elapsed, loopLength);
    score = now;
    // 0 before, 1 after, eased between.
    const auto rise = [](float fraction) {
      fraction = std::clamp(fraction, 0.0f, 1.0f);
      return fraction * fraction * (3 - 2 * fraction);
    };
    // At the end of the demonstration everything goes dark together.
    const float fading = 1 - rise((now - (loopLength - 0.8f)) / 0.8f);
    std::array<float, 40> shown{};
    lampLight = 0.0f;
    for (const Walk& walk : walks) {
      const bool walked = now >= walk.ends;
      for (size_t step = 0; step < walk.cells.size(); ++step) {
        float& level = shown[(size_t)walk.cells[step] - 1];
        if (walked)
          level = std::max(
              level, kConsumed + (1 - kConsumed) *
                                     (1 - rise((now - walk.ends) / 0.6f)));
        else if (now >= walk.landed[step])
          level = std::max(level, rise((now - walk.landed[step]) / 0.25f));
      }
      if (now < walk.begins || now >= walk.ends + kBetween) continue;
      lampLight = rise((now - walk.begins) / kRise) *
                  (1 - rise((now - walk.ends) / 0.4f));
      // Where the lamp stands: on a letter while it rests there, and while
      // it counts, stepping cell by cell toward the next.
      float cell = (float)(walk.cells.front() - 1);
      for (size_t step = 0; step + 1 < walk.cells.size(); ++step) {
        const float departs = walk.landed[step] + kDwell;
        if (now < departs) break;
        const int number = cellAt(walk.cells[step]).number;
        const float counted =
            std::min((now - departs) / kStep, (float)std::abs(number));
        const float whole = std::floor(counted);
        cell = (float)(walk.cells[step] - 1) +
               std::copysign(whole + rise(counted - whole), (float)number);
      }
      lampTurn = 9.0f * cell;
    }
    for (size_t cell = 0; cell < 40; ++cell)
      rubricated[cell] = shown[cell] * fading;
  }
};

}  // namespace

SIGIL_SKETCH(SigillumAemeth, "Study · Esoteric",
             "Dee's Sigillum Dei Aemeth (1582) carved in pale wax — a lamp "
             "walks the seven Names off the rim by the angels' own rule")
