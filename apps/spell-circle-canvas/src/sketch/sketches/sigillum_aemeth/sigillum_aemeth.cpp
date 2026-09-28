// John Dee's Sigillum Dei Aemeth, the seal of wax the angels dictated at
// Mortlake in March 1582, as a plate of the cake itself and a margin that
// reads it.
//
// The seal, from the rim inward: the greatest Circle, cut into forty cells,
// each a letter with a number above or below it; the seven "segments of
// circles", plates carrying the forty-nine letters of the angels in seven
// rows, a little cross at every corner; the heptagon, with the seven Names
// of God written along its sides; the heptagram, one band cut in the wax and
// woven over and under itself; the four orders of the Children of Light in
// the star's points; ZABATHIEL on the innermost heptagon; the pentagram of
// the five planetary angels; and the cross of LEVANAEL at the centre.
//
// Every letter stands in data/: the forty cells (ring.csv), the seven Names
// and the cell each begins at (names.csv), the seven rows of the angles
// (angles.csv), the Names of God (god.csv), the Children of Light
// (children.csv), the planets (planets.csv) and the margin's words
// (content.json).

// TAGS: Geometry/Diagrams, Patterns/Ornament

#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilweave/style/Face.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
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
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace field = sigil::material::field;
namespace document = sigil::compose::document;
namespace data = sigil::data;
using sigil::material::Paint;
using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

// Beeswax four centuries old under museum light: one dull olive-brown and
// its depths. On wax a line is not drawn but cut, so every mark is a groove
// with a shadowed floor and a wall that catches the light.
const auto kVitrine = hexColor(0x14161c);
const auto kWaxDeep = hexColor(0x5c4c26);
const auto kWaxMid = hexColor(0x7d6a37);
const auto kWaxLit = hexColor(0x9c8949);
const auto kWaxPale = hexColor(0xb3a267);
const auto kFloor = hexColor(0x2b2210);
const auto kWall = hexColor(0xc4b485);
const auto kEngraving = hexColor(0x241603);
const auto kVellum = hexColor(0xece1c8);
const auto kRubric = hexColor(0xbf634f);

constexpr float kWidth = 2000, kHeight = 1417;
// The greatest Circle, in px, and the cake of wax it is cut into.
constexpr float kRadius = 618;
constexpr float kWax = 1.058f * kRadius;

// The rings, in units of the greatest Circle. Every angle is measured
// clockwise from twelve o'clock, which is how Dee gives each instruction:
// "the begynning of the greatest Circle … and so procede toward thy right
// hand".
constexpr float kBandInner = 0.876f;    // the forty cells lie outside this
constexpr float kCellLetter = 0.912f;   // a cell's letter
constexpr float kNumberAbove = 0.952f;  // a number that steps right
constexpr float kNumberBelow = 0.884f;  // a number that steps left
constexpr float kPlateOuter = 0.868f, kPlateInner = 0.720f;
constexpr float kAngleSides = 0.836f;  // the heptagon the 49 letters ride
constexpr float kHeptagon = 0.777f;    // the heptagon the star is drawn on
constexpr float kNameSides = 0.727f;   // the Names of God ride this one
constexpr float kInnerHeptagon = 0.285f;
constexpr float kPentagram = 0.215f;
// The concentric rules that cut the star's points into cells.
constexpr float kCellRings[] = {0.590f, 0.505f, 0.428f, 0.355f};

constexpr float kSeventh = 360.0f / 7.0f;

/** The seal's own polar frame, in the coordinates of the wax cake's box. */
const path::PolarFrame kSeal{.centre = {kWax, kWax}, .radius = kRadius};

/** A NODE THE SIZE OF A CIRCLE of the seal at @p radius, centred on it:
 *  every figure below is inscribed in one, so a ring's letters, a
 *  polygon's sides and a star's points are all fractions of its box. */
Element circleOf(float radius = 1.0f) {
  return kit::disc(kSeal.centre, radius * kRadius);
}
template <class Node>
Node inCircle(Node node, float radius = 1.0f) {
  const float half = radius * kRadius;
  return kit::at(std::move(node), kWax - half, kWax - half, 2 * half,
                 2 * half);
}

/** A LETTER CUT IN THE WAX: the dark groove, and a hair below and to the
 *  right of it the lip of wax the graver pushed up, catching the light. */
auto incised(const std::string& words, sigil::material::Color ink = kEngraving) {
  return text(words).ink(sigil::material::from(ink).effects(
      sigil::material::Filter::shadow(hexColor(0xe3d4a2, 0.6f), {.offset = {0.8f, 1.0f}})));
}

/** A RUN LETTERED ALONG SIDE @p side of a heptagon at @p radius — seven
 *  sides chained as one baseline, so a side is addressed by its fraction. */
Element onSide(const std::string& words, int side, float radius) {
  return inCircle(incised(words).textOnPath(
      {.path = shapes::chords({.sides = 7, .radius = radius}),
       .at = ((float)side + 0.5f) / 7.0f,
       .align = TextPath::Align::Center}));
}

/** A cut rule: a dark floor with a lit hairline on the side away from the
 *  light. */
Decoration cutRule(float weight) {
  return lines::rails({{.across = 0, .width = weight, .fill = Fill::color(kFloor)},
                       {.across = weight * 0.5f + 0.8f,
                        .width = 0.9f,
                        .fill = Fill::color(hexColor(0xc4b485, 0.55f))}});
}

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
struct Name {
  std::string name;
  std::vector<int> cells;  // counted from one, as the plate numbers them
};
struct Order {
  std::string name;
  float radius;
  std::vector<std::string> names;
};

struct SigillumAemeth {
  std::vector<Cell> ring;
  std::vector<Name> walked;
  std::vector<std::string> angleRows, godNames, glosses;
  std::vector<Order> orders;
  std::vector<std::string> initials, tails;
  sketch::kit::Document words;
  weave::Type seal, rim, quill, italic, display, mono, serif;

  template <class Read>
  static void rows(sketch::SketchContext& context, const char* file,
                   Read read) {
    if (const auto table = context.assets.hub().load<sigil::data::Table>(
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

  // -------------------------------------------------------------------------
  // THE SEAL

  Element wax() const {
    return kit::disc(kSeal.centre, kWax).shape(shapes::circle()).fill(sigil::material::from(sigil::material::radialGradient({0.42f, 0.36f}, 1.05f,
                                                {{0.0f, kWaxPale},
                                                 {0.45f, kWaxLit},
                                                 {0.82f, kWaxMid},
                                                 {1.0f, kWaxDeep}})).layer(field::grain(1.6f, 4, 1582.0f, 0.34f), {.blend = sigil::material::BlendMode::Overlay}).effects(sigil::material::Filter::shadow(hexColor(0x05070a, 0.7f), {.blur = 18, .offset = {6, 12}}).then(sigil::material::Filter::bevel({.depth = 5, .size = 9, .highlight = hexColor(0xf1e2b0, 0.45f), .shadow = hexColor(0x1c1406, 0.6f)})).then(sigil::material::Filter::shadow(hexColor(0x2b2210, 0.35f), {.blur = 14, .inside = true}))))
        .key("wax");
  }

  /** THE GREATEST CIRCLE: forty cells of nine degrees, the first at twelve
   *  o'clock. Each letter stands at its cell's centre facing out; its
   *  number stands above it or below, by which way it steps. */
  Element circumference() const {
    std::vector<Element> letters, above, below;
    for (int cell = 0; cell < 40; ++cell) {
      const Cell& entry = ring[(size_t)cell];
      letters.push_back(incised(entry.letter).attribute("cell", cell));
      if (entry.number != 0)
        (entry.number > 0 ? above : below)
            .push_back(incised(std::to_string(std::abs(entry.number)), hexColor(0x4a3210))
                           .attribute("cell", cell));
    }
    const auto around = [](float radius) {
      return layouts::Radial{.radiusFraction = radius,
                             .lane = "cell",
                             .divisions = 40,
                             .facing = true};
    };
    return box().inset(0).children(
        {kit::ring(kSeal.centre, kRadius,
                   kit::groove(kRadius, 5.6f, kFloor, kWall)),
         kit::ring(kSeal.centre, kBandInner * kRadius,
                   kit::groove(kBandInner * kRadius, 3.4f, kFloor, kWall)),
         circleOf()
             .shape(shapes::ticks({.divisions = 40,
                                   .from = 4.5f,
                                   .mark = {kBandInner + 0.012f, 0.988f}}))
             .fill(Fill::none())
             .stroke(stroke(1.9f, Fill::color(kEngraving))),
         circleOf().operators({around(kCellLetter)}).font(rim).children(letters),
         circleOf()
             .operators({around(kNumberAbove)})
             .font({.face = rim.face, .size = 19})
             .ink(hexColor(0x4a3210))
             .children(above),
         circleOf()
             .operators({around(kNumberBelow)})
             .font({.face = rim.face, .size = 19})
             .ink(hexColor(0x4a3210))
             .children(below)});
  }

  /** THE SEVEN ANGLES: seven plates cut as segments of circles, a row of
   *  seven letters on each, "and at each corner of these segments of
   *  circles, to make little Crosses". */
  Element angles() const {
    const float halfPlate = kSeventh * 0.5f - 1.1f;
    std::vector<Element> crosses;
    for (int plate = 0; plate < 7; ++plate)
      for (float side : {-halfPlate, halfPlate})
        for (float radius : {kPlateInner, kPlateOuter}) {
          const float degrees = ((float)plate + 0.5f) * kSeventh + side;
          crosses.push_back(kit::disc(kSeal.at(degrees, radius), 6.5f)
                                .shape(shapes::svg(
                                    "M4 0H8V4H12V8H8V12H4V8H0V4H4Z"))
                                .fill(Fill::color(kEngraving))
                                .rotate(degrees));
        }
    return box().inset(0).font(seal).font({.track = 21}).children(
        {circleOf()
             .shape(shapes::arcs({.divisions = 7,
                                  .from = kSeventh * 0.5f,
                                  .mark = {kPlateInner, kPlateOuter},
                                  .spanDeg = 2 * halfPlate}))
             .fill(Fill::color(hexColor(0xd9bd88, 0.22f)))
             .stroke(cutRule(1.8f)),
         crosses, each(7, [this](int row) {
           return onSide(angleRows[(size_t)row], row, kAngleSides);
         })});
  }

  /** THE HEPTAGON, and the seven Names of God written along its sides
   *  with a quill, each over its Latin reading; within it, the concentric
   *  rules that cut the star's points into cells. */
  Element heptagon() const {
    return box().inset(0).children(
        {each(kCellRings,
              [](float radius) {
                return kit::ring(kSeal.centre, radius * kRadius,
                                 kit::groove(radius * kRadius, 1.6f, kFloor, kWall));
              }),
         circleOf()
             .shape(shapes::chords({.sides = 7, .radius = kHeptagon}))
             .fill(Fill::none())
             .stroke(cutRule(2.6f)),
         circleOf()
             .shape(shapes::chords({.sides = 7, .radius = kNameSides - 0.043f}))
             .fill(Fill::none())
             .stroke(cutRule(1.4f)),
         box().inset(0).font(quill).children(each(7, [this](int side) {
           return onSide(godNames[(size_t)side], side, kNameSides);
         })),
         box()
             .inset(0)
             .font({.face = italic.face, .size = 13.5f, .track = 0})
             .ink(hexColor(0x53380f, 0.88f))
             .children(each(7, [this](int side) {
               return onSide(glosses[(size_t)side], side, kNameSides - 0.056f);
             }))});
  }

  /** THE HEPTAGRAM {7/2}: one band cut in the wax, vertex to every second
   *  vertex. Its seven strands cross seven times, and it alternates as you
   *  TRAVEL it — over, under, over along each strand — which is the rule
   *  that makes an interlace read. */
  Element heptagram() const {
    const path::PolarFrame local{.centre = {kRadius, kRadius},
                                 .radius = kRadius};
    const float band = 0.038f * kRadius;
    // The floor, the lit wall and the shadowed one, all opaque: a strand
    // passing over is repainted across the one beneath it.
    const Decoration cut = lines::rails(
        {{.across = 0, .width = band, .fill = Fill::color(hexColor(0x3e3116))},
         {.across = band * 0.5f - 1.6f,
          .width = 3.2f,
          .fill = Fill::color(kWall)},
         {.across = 1.6f - band * 0.5f,
          .width = 3.2f,
          .fill = Fill::color(hexColor(0x140f06))}});
    std::vector<brush::Strand> strands;
    for (int strand = 0; strand < 7; ++strand) {
      const glm::vec2 from = local.at(kSeventh * (float)(2 * strand % 7), kHeptagon);
      const glm::vec2 to =
          local.at(kSeventh * (float)(2 * (strand + 1) % 7), kHeptagon);
      strands.push_back({StrandPath::authored(path::toPath(
                             path::Polyline{.points = {from, to}})),
                         cut});
    }
    return circleOf().fill(Fill::none()).stroke(Decoration(
        brush::weave(std::move(strands), path::crossing::alternateAlong())));
  }

  /** WITHIN THE STAR: the four orders of the Children of Light, each name
   *  on the ray of a point beside the tablet its order wears, and
   *  ZABATHIEL distributed "in his letters into 7 sides of that innermost
   *  Heptagonum". */
  Element children() const {
    // A forehead's arc-segment, a round gold plate on the breast, a
    // four-square ivory and a three-cornered green.
    const Shape tablets[4] = {shapes::sector(200, 140, 0.45f), shapes::circle(),
                              shapes::polygon(4, 45), shapes::polygon(3, 180)};
    const data::Json& zabathiel = words["zabathiel"];
    return box().inset(0).font(seal).children(
        {each(orders,
              [&tablets](const Order& order, size_t index) {
                return circleOf()
                    .operators({layouts::Radial{.radiusFraction = order.radius,
                                                .facing = true}})
                    // each name keeps its own width rather than the ring's
                    .alignItems(Align::Start)
                    .font({.size = 19.0f - (float)index * 0.8f})
                    .children(each(order.names, [&](const std::string& name) {
                      return box().row().gap(5).alignItems(Align::Center).children(
                          {box()
                               .width(15)
                               .height(15)
                               .shape(tablets[index])
                               .fill(Fill::color(hexColor(0xe4cd9e, 0.6f)))
                               .stroke(stroke(1.5f, Fill::color(kEngraving))),
                           incised(name)});
                    }));
              }),
         circleOf()
             .shape(shapes::chords({.sides = 7, .radius = kInnerHeptagon}))
             .fill(Fill::none())
             .stroke(cutRule(2.2f)),
         box().inset(0).font({.size = 18.5f}).children(
             each(7, [&zabathiel](int side) {
               return onSide(std::string(zabathiel[(size_t)side].string()), side,
                             kInnerHeptagon - 0.028f);
             }))});
  }

  /** THE PENTAGRAM, point up on the first cell — "Set Z, of Zedekieil
   *  within the angle which standeth up toward the begynning of the
   *  greatest Circle" — each initial in its angle and the rest of the
   *  name running round outside it; and the cross of LEVANAEL, its
   *  syllables read left, top, right, foot. */
  Element centre() const {
    const data::Json& levanael = words["levanael"];
    const float armDegrees[4] = {0, 90, 180, 270};
    const float armRadius[4] = {0.052f, 0.075f, 0.075f, 0.075f};
    return box().inset(0).font(seal).children(
        {circleOf(kPentagram)
             .shape(shapes::star(5, 0.382f))
             .fill(Fill::color(hexColor(0xe6cf9e, 0.18f)))
             .stroke(cutRule(3.0f)),
         circleOf()
             .operators({layouts::Radial{.radiusFraction = 0.176f, .facing = true}})
             .font({.size = 32})
             .children(each(initials, [](const std::string& initial) {
               return incised(initial);
             })),
         box().inset(0).font({.face = quill.face, .size = 15}).children(
             each(5, [this](int point) {
               return inCircle(
                   text(tails[(size_t)point])
                       .textOnPath({.path = shapes::circle(),
                                    .at = kSeal.fraction((float)point * 72 + 38),
                                    .align = TextPath::Align::Center}),
                   0.161f);
             })),
         circleOf(0.085f)
             .shape(shapes::svg("M44 0H56V34H90V46H56V100H44V46H10V34H44Z"))
             .fill(Fill::color(hexColor(0xe9d4a4, 0.34f)))
             .stroke(stroke(2.4f, Fill::color(kEngraving))),
         box().inset(0).font({.size = 15}).children(
             each(4, [&](int arm) {
               return incised(std::string(levanael[(size_t)arm].string()))
                   .centerAt(kSeal.at(armDegrees[arm], armRadius[arm]));
             }))});
  }

  // -------------------------------------------------------------------------
  // THE MARGIN

  Element margin() const {
    const auto doubleRule = [](float weight) {
      return kit::line({.length = Dimension(575),
                        .thickness = weight,
                        .fill = Fill::color(hexColor(0xc7ab74, 0.7f)),
                        .pair = {{.thickness = 0.7f,
                                  .gap = 2.8f,
                                  .fill = Fill::color(hexColor(0xc7ab74, 0.4f)),
                                  .dash = {1.6f, 4.4f}}}});
    };
    const Shape tablets[4] = {shapes::sector(-100, 200, 0.55f), shapes::circle(),
                              shapes::polygon(4, 45), shapes::polygon(3)};
    const sigil::material::Color tabletColours[4] = {
        hexColor(0xb9c6da), hexColor(0xe6bf63), hexColor(0xf7f1e2),
        hexColor(0x9dbfa2)};
    return kit::at(1383, 56, 575, 1320)
        .column()
        .gap(7)
        .font(mono)
        .ink(hexColor(0x8d7a58))
        .applyStyleSheet(StyleSheet{
            rule("h1").font({.face = display.face, .size = 40, .color = kVellum,
                             .track = 2}),
            rule("lead").font({.face = italic.face, .size = 16,
                               .color = hexColor(0xc7ab74)}),
            rule("h2").font({.size = 12.5f, .color = kRubric, .track = 1.4f}),
            rule(".serif").font(serif),
            rule(".italic").font({.face = italic.face, .size = 14}),
            rule(".name").font({.face = display.face, .size = 25,
                                .color = kVellum, .track = 1}),
            rule(".chain").font({.color = hexColor(0x71b2cf)}),
            rule(".basket").font({.face = seal.face, .size = 24,
                                  .color = kVellum})})
        .children(
            {document::h1(words.phrase("title")),
             document::lead(words.phrase("subtitle")),
             text(words.phrase("provenance")).styleClass("serif"),
             doubleRule(2.2f),
             document::h2(words.phrase("namesHeading")),
             text(words.phrase("rule")).styleClass("italic").width(575),
             box().column().children(each(walked, [](const Name& name, size_t index) {
               std::string chain;
               for (int cell : name.cells)
                 chain += (chain.empty() ? "" : "·") + std::to_string(cell);
               return box().row().height(40).alignItems(Align::Baseline).children(
                   {text(std::to_string(index + 1) + ".").width(26),
                    text(name.name).styleClass("name").width(170),
                    text(chain).styleClass("chain")});
             })),
             text(words.phrase("consumed")),
             document::h2(words.phrase("basketsHeading")),
             box().column().children(each(angleRows, [](const std::string& row) {
               return box().row().children(
                   each(characters(row), [](const std::string& letter) {
                     return text(letter)
                         .styleClass("basket")
                         .width(44)
                         .paragraph({.alignment = weave::TextAlignment::kCenter});
                   }));
             })),
             text(words.phrase("archangels")).styleClass("italic"),
             text(words.phrase("crossNote")).styleClass("italic"),
             document::h2(words.phrase("ordersHeading")),
             box().column().gap(8).children(each(
                 words.run("orders"),
                 [&](const sketch::kit::Document::Line& line, size_t index) {
                   return box().row().gap(10).alignItems(Align::Center).children(
                       {box()
                            .width(14)
                            .height(14)
                            .shape(tablets[index])
                            .fill(Fill::color(tabletColours[index])),
                        text(line.words).styleClass("serif")});
                 })),
             box().flexGrow(1),
             doubleRule(1.6f),
             text(words.phrase("seal")).styleClass("italic").width(575),
             text(words.phrase("imprint")).font({.size = 10})});
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& context) {
    sketch::kit::stage(context, {.size = {kWidth, kHeight},
                                 .captureAt = 0.05,
                                 .background = kVitrine});

    // One chain of faces per lettering system: the first installed wins.
    const auto book = sketch::kit::houseFace(sketch::kit::Voice::Book);
    const auto bookItalic = sketch::kit::houseFace(
        sketch::kit::Voice::Book, 400, sigil::weave::FaceSlant::Italic);
    seal = {.face = weave::ports::face({"Herculanum", "Optima", "Baskerville"}),
            .size = 30,
            .color = kEngraving};
    rim = {.face = weave::ports::face({"Trattatello", "Hoefler Text", "Baskerville"},
                                      sigil::weave::FaceStyle{.slant = sigil::weave::FaceSlant::Italic}),
           .size = 37,
           .color = kEngraving};
    quill = {.face = bookItalic, .size = 30, .color = kEngraving, .track = 16};
    italic = {.face = bookItalic};
    display = {.face = weave::ports::face(
                   {"Luminari", "Herculanum", "Optima", "Baskerville"})};
    mono = {.face = sketch::kit::houseFace(sketch::kit::Voice::Terminal),
            .size = 12.5f};
    serif = {.face = book, .size = 13};

    rows(context, "ring.csv", [this](const auto& table) {
      const auto letter = table.template column<std::string>("letter");
      const auto number = table.template column<double>("number");
      for (size_t row = 0; row < letter.size(); ++row)
        ring.push_back({letter[row], (int)number[row]});
    });
    rows(context, "angles.csv", [this](const auto& table) {
      for (const std::string& row : table.template column<std::string>("letters"))
        angleRows.push_back(row);
    });
    rows(context, "god.csv", [this](const auto& table) {
      const auto plate = table.template column<std::string>("plate");
      const auto gloss = table.template column<std::string>("gloss");
      for (size_t row = 0; row < plate.size(); ++row) {
        godNames.push_back(plate[row]);
        glosses.push_back(gloss[row]);
      }
    });
    rows(context, "children.csv", [this](const auto& table) {
      const auto name = table.template column<std::string>("order");
      const auto radius = table.template column<double>("radius");
      const auto names = table.template column<std::string>("names");
      for (size_t row = 0; row < name.size(); ++row) {
        Order order{name[row], (float)radius[row], {}};
        std::istringstream split(names[row]);
        for (std::string word; split >> word;) order.names.push_back(word);
        orders.push_back(std::move(order));
      }
    });
    rows(context, "planets.csv", [this](const auto& table) {
      for (const std::string& initial : table.template column<std::string>("initial"))
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
          walked.push_back({name[row], walk((int)start[row])});
          consumed.insert(walked.back().cells.begin(), walked.back().cells.end());
        }
      });

    words = sketch::kit::Document(context, "data/content.json");
    words.figures({{"used", std::to_string(consumed.size())},
                   {"unvisited", std::to_string(40 - consumed.size())}});

    context.composer.render(box().inset(0).children(
        {kit::disc({50 + kWax, 50 + kWax}, kWax)
             .ink(kEngraving)
             .children({wax(), circumference(), angles(), heptagon(),
                        heptagram(), children(), centre()}),
         margin()}));
  }
};

}  // namespace

SIGIL_SKETCH(SigillumAemeth, "Study · Esoteric",
             "Dee's Sigillum Dei Aemeth (1582) — solved from the "
             "angels' own jump rule, 33 of 40 cells")
