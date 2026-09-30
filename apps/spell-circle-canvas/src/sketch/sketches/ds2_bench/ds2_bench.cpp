/** @file
 * DEAD SPACE 2's upgrade bench (Visceral, 2011): the hologram Isaac reads
 * at a bench, a sheet of blue-white light thrown up from an emitter into a
 * dark machine room. Nothing in the picture is lit but by it: the cone of
 * the throw, the pool it leaves on the floor, the bench it rises from.
 *
 * THE PANEL, as the game draws it: a chamfered sheet whose middle stands
 * above its shoulders and carries the weapon's name in a condensed
 * industrial capital; the weapon's nanocircuit — sockets on a grid joined
 * by twin-rail traces, the nodes already bought lit in the colour of what
 * they raise inside angular bracket frames; the specification table, one
 * chevron load bar per statistic; the power nodes the player holds, the
 * one warm object on the sheet; and the R.I.G. along its foot, health in
 * segments and the stasis module recharging.
 *
 * THIS STUDY'S OWN: the panel is light, so it is added to the room rather
 * than laid over it, its three primaries land a little apart, it is swept
 * in rows, a band of brighter light crosses it and the whole throw
 * flickers with the emitter. And a hand is at it: the cursor crosses the
 * board to an empty socket, the bracket closes on it, the detail card
 * unfolds, the weapon's schematic draws itself in and the node it would
 * add blinks in the load bar — then it all folds away and begins again.
 *
 * ONE CLOCK DRIVES IT. `seconds` is written once a frame; every beat of
 * the hand is a window of one looping cycle over it, stated in seconds in
 * `data/bench.json` beside every word, board and statistic. Everything
 * that stands still — the room, the panel, the card's face — is one image
 * each, and only what moves is drawn every frame, over them.
 */
// TAGS: Geometry/Diagrams, Interfaces/Game

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>

#include <sstream>
#include <string>
#include <vector>

#include "Projection.h"

namespace data = sigil::data;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
using material::Filter;
using material::hexColor;
using namespace sigil::compose;

namespace {

constexpr float kWidth = 1200, kHeight = 800;

/** The panel in the room, and every coordinate inside it is its own. */
constexpr float kPanelX = 120, kPanelY = 28, kPanelWidth = 960, kPanelHeight = 530;
/** Where the throw leaves the bench, and where the floor meets the wall. */
constexpr glm::vec2 kLens{600, 694};
constexpr float kHorizon = 610;

const material::Color kVoid = hexColor(0x020508);
const material::Color kLight = hexColor(0x86D9F2);
const material::Color kWhite = hexColor(0xE8FBFF);
const material::Color kBrass = hexColor(0xD9AE3A);

material::Color light(float alpha) { return material::withAlpha(kLight, alpha); }
material::Color white(float alpha) { return material::withAlpha(kWhite, alpha); }

/** The panel's outline: 45° cut corners, and a middle standing 20 px
 *  above the two shoulders. The contour inside it keeps the same steps. */
constexpr const char* kPanelOutline =
    "M0 44 L24 20 L120 20 L140 0 L820 0 L840 20 L936 20 L960 44 "
    "L960 506 L936 530 L24 530 L0 506 Z";
constexpr const char* kPanelContour =
    "M10 50 L30 30 L126 30 L146 10 L814 10 L834 30 L930 30 L950 50 "
    "L950 500 L930 520 L30 520 L10 500 Z";
/** A load pip: notched on the left, pointed on the right. */
constexpr const char* kPip = "M0 0 L26 0 L32 8 L26 16 L0 16 L6 8 Z";
/** A health segment, leaning forward. */
constexpr const char* kSegment = "M4 0 L24 0 L20 12 L0 12 Z";

/** What a bought node raises, as the two lights it is drawn in. */
struct Kind {
  material::Color core, rim;
};
Kind kindOf(std::string_view name) {
  if (name == "DMG") return {hexColor(0xC2452C), hexColor(0xFFB39A)};
  if (name == "CAP") return {hexColor(0x3576B8), hexColor(0xC4E4FF)};
  if (name == "CHR") return {hexColor(0x5E9A2E), hexColor(0xDDF7AE)};
  if (name == "REL") return {hexColor(0xB27A1E), hexColor(0xFFDE96)};
  return {light(0.1f), light(0.9f)};
}

/** THE PANEL'S TYPE: a condensed industrial capital, tracked wide, and a
 *  narrower companion for what it says in passing. */
StyleSheet registers() {
  const char* capital = "DIN Condensed, Avenir Next Condensed, Helvetica Neue";
  const char* companion = "Avenir Next Condensed, DIN Condensed, Helvetica Neue";
  return StyleSheet{
      rule(".title").fontFamily(capital).fontWeight(700).fontSize(42).letterSpacing(11).ink(kWhite),
      rule(".tag").fontFamily(capital).fontWeight(700).fontSize(15).letterSpacing(3).ink(light(0.95f)),
      rule(".caption").fontFamily(companion).fontWeight(600).fontSize(11).letterSpacing(2.6f).ink(light(0.62f)),
      rule(".kind").fontFamily(capital).fontWeight(700).fontSize(12).letterSpacing(1.8f),
      rule(".label").fontFamily(capital).fontWeight(700).fontSize(17).letterSpacing(2.6f).ink(light(0.92f)),
      rule(".value").fontFamily(capital).fontWeight(700).fontSize(19).letterSpacing(1.2f).ink(kWhite),
      rule(".numeral").fontFamily(capital).fontWeight(700).fontSize(36).letterSpacing(2).ink(kWhite),
      rule(".hint").fontFamily(companion).fontWeight(600).fontSize(12).letterSpacing(2.2f).ink(light(0.78f)),
  };
}

std::vector<std::string> words(std::string_view line) {
  std::istringstream stream{std::string(line)};
  std::vector<std::string> out;
  for (std::string word; stream >> word;) out.push_back(word);
  return out;
}

glm::vec2 pair(const data::Json& json) {
  return {(float)json[size_t(0)].number(), (float)json[1].number()};
}

/** @p content set in a pinned box, standing where @p justify puts it. */
Element placed(float x, float y, float width, float height, Element content,
               Justify justify = Justify::Start) {
  return kit::at(x, y, width, height)
      .row()
      .alignItems(Align::Center)
      .justifyContent(justify)
      .children({std::move(content)});
}

/** The same angular bracket the game frames everything with: an L at
 *  each corner of the box and nothing between. */
Element bracketed(float x, float y, float width, float height, float arm,
                  float weight, material::Color colour) {
  return kit::at(x, y, width, height)
      .fill(Fill::none())
      .stroke(spans::corners(arm), stroke(weight, Fill::color(colour)));
}

/** THE SPECIFICATION TABLE'S GRID, which the pips previewed over it are
 *  placed on as well. */
struct Table {
  static constexpr float x = 40, y = 290, width = 500, height = 170;
  static constexpr float firstRow = 326, rowPitch = 31;
  static constexpr float labelWidth = 118, pipsX = 188, pipPitch = 36;
  static glm::vec2 pip(size_t row, size_t index) {
    return {x + pipsX + pipPitch * (float)index, firstRow + rowPitch * (float)row + 4};
  }
};

/** What the throw does to a picture it projects: see `Projection.h`. */
Filter projected() {
  const ds2_bench::ProjectionParameters projection{};
  return Filter::of(ds2_bench::projection(projection), ds2_bench::projectionReach(projection));
}

/** THE CARD the selected socket unfolds, in the panel's coordinates. */
struct Card {
  static constexpr float x = 570, y = 294, width = 364, height = 176;
  static constexpr float schematicX = 2, schematicY = 36;
};

}  // namespace

struct Ds2Bench {
  sketch::kit::Document bench;
  /** The one clock: seconds since the bench came up. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);

  // ------------------------------------------------------------ the clock

  float period() const { return (float)bench["choreography"]["periodSeconds"].number(12); }

  /** Where the hand is inside its loop, on [0, 1). */
  motion::Animatable<float> cycle() const {
    return motion::bind(seconds, {.from = {0, period()}, .wrap = 1.0f});
  }

  /** ONE BEAT OF THE HAND: 0 before it, rising to 1 across its first two
   *  times, holding, and falling back across its last two — read off the
   *  loop, eased, and put onto @p to. */
  motion::Animatable<float> beat(std::string_view name, motion::Range to,
                                 motion::Easing ease = motion::ease::inOutCubic,
                                 motion::Wiggle wiggle = {}) const {
    const data::Json& times = bench["choreography"][name];
    const auto at = [&](size_t index) { return (float)times[index].number() / period(); };
    return motion::bind(cycle(), {.envelope = motion::envelope::trapezoid(at(0), at(1), at(2), at(3)),
                                  .ease = std::move(ease),
                                  .to = to,
                                  .wiggle = wiggle});
  }

  /** THE EMITTER'S SUPPLY: the throw is never quite steady. */
  motion::Animatable<float> flicker() const {
    return motion::bind(seconds, {.to = {1, 1},
                                  .wiggle = {.amount = 0.07f, .frequency = 9, .seed = 11, .octaves = 3}});
  }

  /** Where a cell of the board stands on the panel. */
  glm::vec2 cellAt(glm::vec2 cell) const {
    const data::Json& board = bench["board"];
    return pair(board["origin"]) + pair(board["pitch"]) * cell;
  }
  glm::vec2 selected() const { return cellAt(pair(bench["board"]["selected"])); }

  // ------------------------------------------------------------- the room

  /** The machine room the throw stands in: out-of-focus wall struts, the
   *  floor running off to the wall in faint seams, and the bench the
   *  emitter is set in, its top edge catching the hologram's light. */
  Element room() const {
    const auto strut = [](const data::Json& strut) {
      const float alpha = (float)strut[2].number();
      return kit::at((float)strut[0].number(), -40, (float)strut[1].number(), kHorizon + 40)
          .fill(material::linearGradient({0, 0}, {0, 1},
                                         {{0.0f, hexColor(0x13222C, alpha * 0.3f)},
                                          {0.55f, hexColor(0x1A2E3A, alpha)},
                                          {1.0f, hexColor(0x0A141C, alpha * 0.5f)}}))
          .filter(Filter::directionalBlur(18, 90, 12));
    };
    // The floor's seams run to one vanishing point above the horizon.
    const glm::vec2 vanishing{600, 470};
    std::vector<Element> seams;
    for (float foot = -900; foot <= 2100; foot += 190) {
      const float along = (kHorizon - vanishing.y) / (kHeight - vanishing.y);
      path::Polyline seam;
      seam.points = {{vanishing.x + (foot - vanishing.x) * along, kHorizon}, {foot, kHeight}};
      seams.push_back(pathFigure(path::toPath(seam), 2).stroke(stroke(1, Fill::color(light(0.05f)))));
    }
    for (float depth : {6.0f, 16.0f, 32.0f, 56.0f, 92.0f, 140.0f})
      seams.push_back(kit::at(0, kHorizon + depth, kWidth, 1).fill(Fill::color(light(0.035f))));
    return box()
        .inset(0)
        .key("room")
        .cache(Cache::Texture)
        .fill(material::linearGradient({0, 0}, {0, 1},
                                       {{0.0f, hexColor(0x03070B)},
                                        {0.74f, hexColor(0x08131B)},
                                        {0.765f, hexColor(0x060D13)},
                                        {1.0f, hexColor(0x020406)}}))
        .children({
            each(bench["room"].array(), strut),
            kit::at(0, kHorizon - 1, kWidth, 2)
                .fill(material::linearGradient({0, 0}, {1, 0},
                                               {{0.0f, light(0)},
                                                {0.5f, light(0.16f)},
                                                {1.0f, light(0)}})),
            box().inset(0).children(std::move(seams)),
            // The light the throw spills on the wall behind it.
            kit::at(0, 0, kWidth, kHorizon).fill(material::radialGradient(
                {0.5f, 0.55f}, 0.62f, {{0.0f, light(0.07f)}, {1.0f, light(0)}})),
            // The bench: a top the lens is set in, catching the light at
            // its back edge, over a darker front with its seams and lamps.
            kit::at(450, kLens.y - 10, 300, 22)
                .shape(shapes::svg("M22 0 L278 0 L300 22 L0 22 Z"))
                .fill(material::linearGradient({0, 0}, {0, 1},
                                               {{0.0f, hexColor(0x1C3038)}, {1.0f, hexColor(0x0C171C)}}))
                .foreground(stroke(1.2f, Fill::color(light(0.4f)))),
            kit::at(472, kLens.y - 10, 256, 1.5f).fill(Fill::color(white(0.6f))),
            kit::at(450, kLens.y + 12, 300, kHeight - kLens.y - 12)
                .fill(material::linearGradient({0, 0}, {0, 1},
                                               {{0.0f, hexColor(0x0E1A20)}, {1.0f, hexColor(0x030608)}}))
                .children({
                    kit::at(0, 0, 300, 1).fill(Fill::color(light(0.28f))),
                    kit::at(18, 16, 264, 30).fill(Fill::none()).stroke(stroke(1, Fill::color(light(0.12f)))),
                    kit::dot({36, 31}, 2.5f, Fill::color(hexColor(0xFFB060, 0.8f))),
                    kit::dot({48, 31}, 2.5f, Fill::color(light(0.7f))),
                    kit::at(146, 16, 1, 90).fill(Fill::color(light(0.08f))),
                }),
            kit::at(kLens.x - 30, kLens.y - 6, 60, 11)
                .borderRadius({5})
                .fill(hexColor(0x0B161C))
                .foreground(stroke(1, Fill::color(light(0.5f)))),
        });
  }

  /** A LIGHT THE THROW CASTS at @p place: kept as one image under @p key,
   *  added to whatever is behind it, and flickering with the emitter. */
  Element cast(Element place, std::string key, std::vector<Element> content) const {
    return place.blendMode(material::BlendMode::PlusLighter)
        .opacity(flicker())
        .children({box().inset(0).key(std::move(key)).cache(Cache::Texture).children(std::move(content))});
  }

  /** THE THROW'S LIGHT on the floor: a pool about the bench, and the
   *  panel's own glow lying along the floor under its foot. */
  Element floorLight() const {
    const auto glow = [](float x, float y, float width, float height, float alpha, float blur) {
      return kit::at(x, y, width, height)
          .shape(shapes::circle())
          .fill(Fill::color(light(alpha)))
          .filter(Filter::blur(blur));
    };
    return cast(kit::at(0, kHorizon - 30, kWidth, kHeight - kHorizon + 30), "floor-light",
                {glow(220, 40, 760, 150, 0.16f, 44), glow(150, 22, 900, 34, 0.22f, 14)});
  }

  /** THE CONE OF THE THROW: light widening from the lens to the panel's
   *  whole foot, carried on into its face, with rays through the haze. */
  Element cone() const {
    constexpr float left = 140, top = 250, width = 920, height = kLens.y - 250;
    const glm::vec2 apex{kLens.x - left, height};
    std::vector<Element> rays;
    for (float foot : {60.0f, 190.0f, 330.0f, 410.0f, 520.0f, 640.0f, 760.0f, 880.0f}) {
      path::Polyline ray;
      ray.points = {apex, {foot, 0}};
      rays.push_back(pathFigure(path::toPath(ray), 4).stroke(stroke(3, Fill::color(light(0.05f)))));
    }
    return cast(kit::at(left, top, width, height), "cone",
                {box().inset(0).filter(Filter::blur(3)).children({
                    box()
                        .inset(0)
                        .shape(shapes::svg("M0 0 L920 0 L468 444 L452 444 Z"))
                        .fill(material::linearGradient({0, 0}, {0, 1},
                                                       {{0.0f, light(0)},
                                                        {0.6f, light(0.05f)},
                                                        {0.9f, light(0.14f)},
                                                        {1.0f, white(0.4f)}})),
                    box().inset(0).children(std::move(rays)),
                })});
  }

  /** The lens itself, burning at the heart of the throw. */
  Element lens() const {
    return cast(kit::disc(kLens, 46), "lens",
                {box().inset(0).fill(material::radialGradient(
                    {0.5f, 0.5f}, 0.5f,
                    {{0.0f, white(0.95f)}, {0.12f, light(0.6f)}, {0.4f, light(0.12f)}, {1.0f, light(0)}}))});
  }

  // ------------------------------------------------------------ the panel

  /** The sheet: a faint body of light, a rim burning brighter than it,
   *  the inner contour and its dotted echo, and the header rule. */
  static Element sheet() {
    return box().inset(0).children({
        box()
            .inset(0)
            .shape(shapes::svg(kPanelOutline))
            .fill(material::radialGradient({0.5f, 0.42f}, 0.75f,
                                           {{0.0f, light(0.15f)},
                                            {0.7f, light(0.08f)},
                                            {1.0f, light(0.05f)}})),
        box()
            .inset(0)
            .shape(shapes::svg(kPanelOutline))
            .fill(Fill::none())
            .foreground(decorations::border(2, Fill::color(white(0.92f))))
            .filter(Filter::glow(light(0.8f), 7)),
        box()
            .inset(0)
            .shape(shapes::svg(kPanelContour))
            .fill(Fill::none())
            .foreground(decorations::border(1, Fill::color(light(0.45f))))
            .foreground(Border{.width = 1, .fill = Fill::color(light(0.22f)), .inset = 6, .dash = {2, 6}}),
        kit::at(30, 80, 900, 1).fill(material::linearGradient(
            {0, 0}, {1, 0}, {{0.0f, light(0.1f)}, {0.5f, light(0.55f)}, {1.0f, light(0.1f)}})),
        kit::at(30, 484, 900, 1).fill(Fill::color(light(0.3f))),
    });
  }

  /** THE BRASS NODE the player spends. */
  static Element powerNode() {
    return box().width(46).height(34).children({
        kit::at(1, 11, 44, 20)
            .borderRadius({10})
            .fill(material::linearGradient({0, 0}, {0, 1},
                                           {{0.0f, kBrass}, {1.0f, hexColor(0x6A5114)}})),
        kit::at(1, 2, 44, 19)
            .shape(shapes::squircle(2))
            .fill(material::linearGradient({0, 0}, {1, 1},
                                           {{0.0f, hexColor(0xF4D77A)}, {1.0f, hexColor(0x9A7A22)}}))
            .stroke(stroke(1, Fill::color(hexColor(0xFFE9A6, 0.8f)))),
        kit::at(15, 7, 16, 9).shape(shapes::squircle(2)).fill(Fill::none())
            .stroke(stroke(1.2f, Fill::color(hexColor(0x6A5114)))),
    });
  }

  /** The weapon's name in the raised middle, the bench's tag under the
   *  left shoulder, and the power nodes held under the right. */
  Element header() const {
    return box().inset(0).children({
        placed(146, 14, 668, 58, text(bench["weapon"].string()).styleClass("title"), Justify::Center),
        placed(40, 38, 320, 18,
               box().row().gap(14).alignItems(Align::Center).children({
                   text(bench["bench"].string()).styleClass("tag"),
                   text(bench["repair"].string()).styleClass("caption"),
               })),
        placed(700, 30, 230, 44,
               box().row().gap(12).alignItems(Align::Center).children({
                   text("POWER NODES").styleClass("caption"),
                   powerNode(),
                   text("0" + std::to_string((int)bench["nodes"].number())).styleClass("numeral"),
               }),
               Justify::End),
    });
  }

  /** One cell of the board: an empty socket, or a bought node lit in its
   *  kind inside a bracket frame, its kind named above it. */
  static Element cell(glm::vec2 at, const std::string& name) {
    if (name == "o")
      return kit::at(at.x - 9, at.y - 9, 18, 18)
          .shape(shapes::chamfered(4))
          .fill(Fill::color(light(0.1f)))
          .foreground(decorations::border(1.5f, Fill::color(light(0.85f))));
    const Kind kind = kindOf(name);
    return box().inset(0).children({
        bracketed(at.x - 20, at.y - 20, 40, 40, 8, 1.3f, light(0.75f)),
        kit::at(at.x - 12, at.y - 12, 24, 24)
            .shape(shapes::chamfered(5))
            .fill(material::radialGradient({0.4f, 0.35f}, 0.8f,
                                           {{0.0f, kind.rim}, {0.55f, kind.core},
                                            {1.0f, material::scale(kind.core, 0.6f)}}))
            .foreground(decorations::border(1.8f, Fill::color(kind.rim)))
            .filter(Filter::glow(material::withAlpha(kind.core, 0.8f), 6)),
        text(name).styleClass("kind").ink(kind.rim).centerAt({at.x + 30, at.y - 22}),
    });
  }

  /** THE NANOCIRCUIT: traces first, lit as twin rails, the way current
   *  enters at the left, then the cells over them. */
  Element board() const {
    const data::Json& board = bench["board"];
    std::vector<Element> traces, cells;
    for (const data::Json& run : board["runs"].array()) {
      path::Polyline line;
      for (const data::Json& at : run.array()) line.points.push_back(cellAt(pair(at)));
      traces.push_back(pathFigure(path::toPath(line), 4)
                           .stroke(stroke(5.5f, Fill::color(light(0.6f))))
                           .foreground(stroke(2.6f, Fill::color(kVoid))));
    }
    size_t row = 0;
    for (const data::Json& line : board["grid"].array()) {
      const std::vector<std::string> tokens = words(line.string());
      for (size_t column = 0; column < tokens.size(); ++column)
        if (tokens[column] != ".")
          cells.push_back(cell(cellAt({(float)column, (float)row}), tokens[column]));
      ++row;
    }
    const glm::vec2 entry = cellAt({0, 1});
    return box().inset(0).children({
        box().inset(0).children(std::move(traces)),
        bracketed(entry.x - 76, entry.y - 38, 64, 76, 12, 1.5f, light(0.8f)),
        kit::at(entry.x - 62, entry.y - 20, 14, 40)
            .shape(shapes::svg("M14 0 L0 0 L0 40 L14 40"))
            .fill(Fill::none())
            .stroke(stroke(1.5f, Fill::color(light(0.8f)))),
        kit::at(entry.x - 108, entry.y - 11, 22, 22)
            .shape(shapes::polygon(3, 90))
            .fill(Fill::color(light(0.85f))),
        box().inset(0).children(std::move(cells)),
    });
  }

  /** ONE STATISTIC: its name, a marker in its kind, the chevron load bar
   *  filled so far, and its value. */
  static Element statistic(const data::Json& stat, size_t row) {
    const Kind kind = kindOf(stat["kind"].string());
    const int filled = (int)stat["filled"].number();
    const float top = Table::firstRow + Table::rowPitch * (float)row;
    std::vector<Element> pips;
    for (size_t index = 0; index < (size_t)stat["total"].number(); ++index) {
      const glm::vec2 at = Table::pip(row, index);
      Element pip = kit::at(at.x, at.y, 32, 16).shape(shapes::svg(kPip));
      pips.push_back((int)index < filled
                         ? pip.fill(material::linearGradient({0, 0}, {0, 1},
                                                             {{0.0f, kWhite},
                                                              {1.0f, material::mixToward(kLight, kind.rim, 0.3f, 1)}}))
                         : pip.fill(Fill::none()).stroke(stroke(1.1f, Fill::color(light(0.35f)))));
    }
    return box().inset(0).children({
        placed(Table::x, top, Table::labelWidth, 24, text(stat["label"].string()).styleClass("label"), Justify::End),
        kit::dot({Table::x + Table::labelWidth + 18, top + 12}, 4, Fill::color(kind.rim)),
        box().inset(0).children(std::move(pips)),
        placed(Table::x + Table::width - 60, top, 60, 24,
               box().row().gap(4).alignItems(Align::Baseline).children({
                   text(stat["value"].string()).styleClass("value"),
                   text("PTS").styleClass("caption"),
               }),
               Justify::End),
    });
  }

  /** THE SPECIFICATION TABLE, in its brackets, under its column heads. */
  Element table() const {
    std::vector<Element> rows;
    size_t row = 0;
    for (const data::Json& stat : bench["stats"].array()) rows.push_back(statistic(stat, row++));
    return box().inset(0).children({
        bracketed(Table::x - 10, Table::y - 4, Table::width + 20, Table::height, 18, 1.4f, light(0.7f)),
        placed(Table::x, Table::y + 4, Table::labelWidth, 16, text("SPECIFICATION").styleClass("caption"), Justify::End),
        placed(Table::x + Table::pipsX, Table::y + 4, 200, 16, text("NANOCIRCUIT LOAD").styleClass("caption")),
        placed(Table::x + Table::width - 60, Table::y + 4, 60, 16, text("VALUE").styleClass("caption"), Justify::End),
        kit::at(Table::x, Table::y + 26, Table::width, 1).fill(Fill::color(light(0.28f))),
        box().inset(0).children(std::move(rows)),
    });
  }

  /** THE R.I.G. along the panel's foot, as it stands — every segment's
   *  frame, the healthy ones lit, the stasis module's empty bar — and
   *  the controller's hints at the right. */
  Element rig() const {
    const data::Json& rig = bench["rig"];
    const int lit = (int)rig["lit"].number();
    std::vector<Element> segments;
    for (int index = 0; index < (int)rig["segments"].number(); ++index) {
      Element segment = kit::at(104 + 27.0f * (float)index, 494, 24, 12).shape(shapes::svg(kSegment));
      segments.push_back(index < lit - 1
                             ? segment.fill(Fill::color(hexColor(0xBDF5E4, 0.9f)))
                             : segment.fill(Fill::none()).stroke(stroke(1, Fill::color(light(0.45f)))));
    }
    std::vector<Element> hints;
    for (const data::Json& hint : bench["hints"].array())
      hints.push_back(text(hint.string()).styleClass("hint"));
    return box().inset(0).children({
        placed(40, 490, 60, 20, text("R.I.G.").styleClass("tag")),
        box().inset(0).children(std::move(segments)),
        placed(398, 490, 70, 20, text("STASIS").styleClass("tag")),
        kit::at(470, 495, 170, 10).fill(Fill::none()).stroke(stroke(1, Fill::color(light(0.55f)))),
        placed(660, 490, 270, 20, box().row().gap(26).children(std::move(hints)), Justify::End),
    });
  }

  /** EVERYTHING ON THE PANEL THAT STANDS STILL, as one image the throw
   *  projects. */
  Element panelArt() const {
    return box()
        .inset(0)
        .key("panel")
        .cache(Cache::Texture)
        .filter(projected())
        .children({sheet(), header(), board(), table(), rig()});
  }

  // ---------------------------------------------------------- what moves

  /** THE SCAN: a band of brighter light crossing the sheet top to foot,
   *  lighting the sheet and nothing beside it. */
  Element scan() const {
    const float scanSeconds = (float)bench["choreography"]["scanSeconds"].number(4);
    return box()
        .inset(0)
        .shape(shapes::svg(kPanelOutline))
        .overflow(Overflow::Clip)
        .children({kit::at(0, -80, kPanelWidth, 80)
                       .translateY(motion::bind(seconds, {.from = {0, scanSeconds},
                                                          .to = {0, kPanelHeight + 80},
                                                          .wrap = kPanelHeight + 80}))
                       .children({box().inset(0).key("scan").cache(Cache::Texture).fill(material::linearGradient(
                           {0, 0}, {0, 1},
                           {{0.0f, light(0)}, {0.42f, light(0.06f)}, {0.5f, white(0.16f)},
                            {0.56f, light(0.05f)}, {1.0f, light(0)}}))})});
  }

  /** The empty socket the hand is going to: it breathes, asking. */
  Element beckon() const {
    return kit::dot(selected(), 6, Fill::color(kWhite))
        .opacity(motion::bind(seconds, {.from = {0, 1.4f},
                                        .envelope = motion::envelope::cosine(),
                                        .to = {0.1f, 0.85f}}));
  }

  /** THE BRACKET closing on the chosen socket as the cursor lands. */
  Element bracket() const {
    const glm::vec2 at = selected();
    return kit::at(at.x - 24, at.y - 24, 48, 48)
        .scale(beat("brackets", {1.9f, 1.0f}, motion::ease::outCubic))
        .opacity(beat("brackets", {0, 1}, motion::ease::outCubic))
        .children({box().inset(0).key("bracket").cache(Cache::Texture).filter(Filter::glow(light(0.9f), 4))
                       .children({bracketed(4, 4, 40, 40, 10, 2, kWhite)})});
  }

  /** THE CURSOR: a lit pointer, crossing from where the hand rested to the
   *  chosen socket and back, never quite still in a hand. */
  Element cursor() const {
    const glm::vec2 from = pair(bench["choreography"]["cursorFrom"]);
    const glm::vec2 to = selected() + glm::vec2(16, 16);
    const auto axis = [&](float start, float end, uint32_t seed) {
      return beat("cursor", {start, end}, motion::ease::inOutCubic,
                  {.amount = 1.5f, .frequency = 5, .seed = seed, .octaves = 2});
    };
    return kit::at(-14, -14, 28, 28)
        .translateX(axis(from.x, to.x, 3))
        .translateY(axis(from.y, to.y, 5))
        .children({box().inset(0).key("cursor").cache(Cache::Texture).filter(Filter::glow(light(0.9f), 3))
                       .children({
                           kit::at(6, 6, 16, 16).shape(shapes::svg("M0 0 L16 6 L9 9 L6 16 Z"))
                               .fill(Fill::color(kWhite)),
                       })});
  }

  /** THE CARD'S FACE, which stands still once it is open. */
  Element cardFace() const {
    const data::Json& offer = bench["offer"];
    const Kind kind = kindOf(offer["kind"].string());
    std::vector<Element> lines;
    for (const data::Json& line : offer["lines"].array())
      lines.push_back(box().row().gap(10).alignItems(Align::Baseline).children({
          text(line[size_t(0)].string()).styleClass("caption"),
          text(line[1].string()).styleClass("label"),
      }));
    return box()
        .inset(0)
        .key("card")
        .cache(Cache::Texture)
        .filter(projected())
        .children({
            box().inset(0).shape(shapes::chamfered(10))
                .fill(material::linearGradient({0, 0}, {0, 1}, {{0.0f, light(0.16f)}, {1.0f, light(0.07f)}}))
                .foreground(decorations::border(1.3f, Fill::color(light(0.7f)))),
            bracketed(-8, -8, Card::width + 16, Card::height + 16, 20, 1.8f, white(0.9f)),
            placed(16, 8, Card::width - 32, 24,
                   box().row().flexGrow(1).justifyContent(Justify::SpaceBetween).alignItems(Align::Center).children({
                       text(offer["node"].string()).styleClass("tag"),
                       text(offer["raise"].string()).styleClass("label").ink(kind.rim),
                   })),
            kit::at(14, 36, Card::width - 28, 1).fill(Fill::color(light(0.35f))),
            placed(16, 150, Card::width - 32, 22,
                   box().row().flexGrow(1).justifyContent(Justify::SpaceBetween).alignItems(Align::Center).children(
                       std::move(lines))),
        });
  }

  /** THE SCHEMATIC, drawing itself in: a wide faint pass under a fine
   *  bright one, both claiming the same run of the drawing. */
  Element schematic() const {
    std::string drawing;
    for (const data::Json& contour : bench["offer"]["schematic"].array()) {
      drawing += contour.string();
      drawing += ' ';
    }
    const auto pass = [&](float weight, material::Color colour) {
      return box()
          .inset(0)
          .shape(shapes::svg(drawing))
          .fill(Fill::none())
          .stroke(spans::upTo(beat("draw", {0, 1}, motion::ease::inOutQuad)),
                  stroke(weight, Fill::color(colour)));
    };
    return kit::at(Card::schematicX, Card::schematicY, 360, 108).children({
        pass(4, light(0.3f)),
        pass(1.3f, kWhite),
    });
  }

  /** THE DETAIL CARD, unfolding from a line across into its whole face. */
  Element card() const {
    return kit::at(Card::x, Card::y, Card::width, Card::height)
        .scaleX(beat("widen", {0.02f, 1}))
        .scaleY(beat("open", {0.02f, 1}))
        .opacity(beat("widen", {0, 1}))
        .children({cardFace(), schematic()});
  }

  /** THE NODE THE CARD OFFERS, blinking in its statistic's load bar. */
  Element preview() const {
    const data::Json& offer = bench["offer"];
    const size_t row = (size_t)offer["stat"].number();
    const size_t index = (size_t)bench["stats"][row]["filled"].number();
    const glm::vec2 at = Table::pip(row, index);
    return kit::at(at.x, at.y, 32, 16)
        .opacity(beat("preview", {0, 1}))
        .children({box().inset(0).shape(shapes::svg(kPip)).fill(Fill::color(kWhite))
                       .opacity(motion::bind(seconds, {.from = {0, 0.8f},
                                                       .envelope = motion::envelope::square(0.5f),
                                                       .to = {0.2f, 0.9f}}))});
  }

  /** THE R.I.G. LIVE: the last healthy segment failing and catching, and
   *  the stasis module refilling. */
  Element vitals() const {
    const data::Json& rig = bench["rig"];
    const int last = (int)rig["lit"].number() - 1;
    const float stasisSeconds = (float)bench["choreography"]["stasisSeconds"].number(6);
    const float stasisFrom = (float)rig["stasis"][size_t(0)].number();
    const float stasisTo = (float)rig["stasis"][1].number();
    return box().inset(0).children({
        kit::at(104 + 27.0f * (float)last, 494, 24, 12)
            .shape(shapes::svg(kSegment))
            .fill(Fill::color(hexColor(0xFFE6A0)))
            .opacity(motion::bind(seconds, {.to = {0.6f, 0.6f},
                                            .wiggle = {.amount = 0.4f, .frequency = 3, .seed = 29, .octaves = 2},
                                            .clamp = {0.1f, 1.0f}})),
        kit::at(472, 497, 166, 6)
            .fill(material::linearGradient({0, 0}, {1, 0}, {{0.0f, light(0.6f)}, {1.0f, kWhite}}))
            .transformOrigin(pct(0), pct(50))
            .scaleX(motion::bind(motion::bind(seconds, {.from = {0, stasisSeconds}, .wrap = 1.0f}),
                                 {.ease = motion::ease::outCubic, .to = {stasisFrom, stasisTo}})),
    });
  }

  /** THE HOLOGRAM: the panel's image and everything that moves over it,
   *  added to the room as light, flickering with the emitter. */
  Element hologram() const {
    return kit::at(kPanelX, kPanelY, kPanelWidth, kPanelHeight)
        .blendMode(material::BlendMode::PlusLighter)
        .opacity(flicker())
        .children({panelArt(), scan(), beckon(), vitals(), preview(), bracket(), card(), cursor()});
  }

  Element describe() const {
    return box()
        .inset(0)
        .fill(kVoid)
        .applyStyleSheet(registers())
        .children({room(), floorLight(), cone(), lens(), hologram()});
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kVoid);
    // The schematic has just finished drawing in.
    context.captureAt(2.5);
    context.engine.timer([this](motion::Duration, motion::Duration elapsed) {
      seconds = (float)elapsed.count();
      return true;
    });
    bench = sketch::kit::Document(context, "data/bench.json");
    context.composer.render(describe());
  }
};

SIGIL_SKETCH(Ds2Bench, "Study · Game UI",
             "Dead Space 2's Nanocircuit bench (2011) — a diegetic hologram "
             "thrown into a dark room, and a hand at it")
