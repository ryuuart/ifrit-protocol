// Dead Space 2's upgrade bench: the hologram Isaac reads at a bench, a cyan
// sheet of light standing in front of a dark machine room. A chamfered
// panel whose top steps up in the middle carries the weapon's name above a
// header rule; under it, the weapon's nanocircuit — a board of dark empty
// sockets joined by traces, with the upgrades already bought lit in the
// colour of what they raise (red damage, blue capacity, amber reload,
// olive charge). Two smaller boards for the suit and the stasis module sit
// below it, and the bottom band is the specification table — one load bar
// of chevrons per statistic — beside the brass power node counter.
//
// Every word, board, statistic and count stands in data/bench.json. A board
// is a grid of cells (`o` an empty socket, a kind's name a bought node) and
// the traces are runs through cells of that grid.

// TAGS: Geometry/Diagrams, Interfaces/Game

#include <sigilmaterial/filter/Filter.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigildata/decode/Json.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <sstream>
#include <string>
#include <vector>

#include "CrtOverlay.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace path = sigil::geometry::path;
namespace field = sigil::material::field;
namespace weave = sigil::weave;
namespace data = sigil::data;
using material::Filter;
using material::Paint;
using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr float kWidth = 1200, kHeight = 800;

// The panel, in canvas pixels, and the band along its foot.
constexpr float kPanelX = 108, kPanelY = 52, kPanelW = 984, kPanelH = 690;
constexpr float kBandY = 544, kBandH = 148;

const material::Color kVoid = hexColor(0x02060A);
const material::Color kCyan = hexColor(0x8FE0E6);
const material::Color kWhite = hexColor(0xE4F7F8);
const material::Color kBody = hexColor(0x0B1E21);
const material::Color kSocket = hexColor(0x0A1B1E);

material::Color cyan(float alpha) { return material::withAlpha(kCyan, alpha); }

// The panel's outline: 45° cut corners, and a top whose middle stands 22 px
// above the two outer thirds. Its inner contour is the same vocabulary
// turned over — the middle FALLS 40 px, and that fall is the header rule
// the title stands on.
constexpr const char* kPanelOutline =
    "M0 48 L26 22 L110 22 L132 0 L852 0 L874 22 L958 22 L984 48 "
    "L984 664 L958 690 L26 690 L0 664 Z";
constexpr const char* kPanelContour =
    "M0 18 L18 0 L110 0 L150 40 L880 40 L920 0 L942 0 L960 18 "
    "L960 648 L942 666 L18 666 L0 648 Z";
// A load pip: notched on the left, pointed on the right.
constexpr const char* kPip = "M0 0 L30 0 L36 8.5 L30 17 L0 17 L6 8.5 Z";

/** What a bought node raises, and the two colours it is lit in. */
struct Kind {
  material::Color core, rim;
};
Kind kindOf(std::string_view name) {
  if (name == "DMG") return {hexColor(0x6E332F), hexColor(0xE9BCB4)};
  if (name == "CAP") return {hexColor(0x274963), hexColor(0xB2D6EC)};
  if (name == "CHR") return {hexColor(0x4C5E2B), hexColor(0xD6E8AA)};
  if (name == "REL") return {hexColor(0x563F1D), hexColor(0xE2C088)};
  return {kSocket, cyan(0.88f)};
}

/** The bench's registers. Every line is the interface face widened, which
 *  the panel states once; a register says its size, spacing and light. */
StyleSheet registers() {
  const auto bold = weave::ports::face(
      {"Eurostile", "Bank Gothic", "DIN Alternate", "Helvetica Neue"}, 700);
  const auto medium = weave::ports::face(
      {"Eurostile", "Bank Gothic", "DIN Alternate", "Helvetica Neue"}, 500);
  return StyleSheet{
      rule(".weapon").font({.face = bold, .size = 31, .track = 3.1f}).ink(kWhite),
      rule(".caption").font({.face = medium, .size = 10.5f, .track = 2.1f}).ink(cyan(0.5f)),
      rule(".node").font({.face = bold, .size = 11.5f, .track = 1.3f}).ink(cyan(0.8f)),
      rule(".node-small").font({.face = bold, .size = 9, .track = 1}).ink(cyan(0.8f)),
      rule(".board").font({.face = bold, .size = 11, .track = 2}).ink(cyan(0.62f)),
      rule(".count").font({.face = medium, .size = 9.5f, .track = 1.7f}).ink(cyan(0.4f)),
      rule(".head").font({.face = medium, .size = 9, .track = 2}).ink(cyan(0.42f)),
      rule(".spec").font({.face = bold, .size = 14, .track = 1.4f}).ink(cyan(0.95f)),
      rule(".value").font({.face = medium, .size = 13, .track = 0.3f}).ink(kWhite),
      rule(".chip").font({.face = bold, .size = 12, .track = 1.9f}).ink(cyan(0.95f)),
      rule(".numeral").font({.face = bold, .size = 40}).ink(kWhite),
      rule(".hint").font({.face = medium, .size = 12, .track = 0.7f}).ink(cyan(0.78f)),
  };
}

std::vector<std::string> words(std::string_view line) {
  std::istringstream stream{std::string(line)};
  std::vector<std::string> out;
  for (std::string word; stream >> word;) out.push_back(word);
  return out;
}

struct Ds2Bench {
  sketch::kit::Document bench;

  // ------------------------------------------------------------ the room

  /** The machine room behind the glass: out-of-focus wall struts and one
   *  lit doorway, so the translucent panel has something to stand over. */
  Element room() const {
    const auto strut = [](const data::Json& strut) {
      const float alpha = (float)strut[2].number();
      return kit::at((float)strut[0].number(), -40, (float)strut[1].number(),
                     kHeight + 80)
          .fill(
              sigil::material::linearGradient({0, 0}, {0, 1},
                                    {{0.0f, hexColor(0x16262F, alpha * 0.35f)},
                                     {0.38f, hexColor(0x1C303C, alpha)},
                                     {1.0f, hexColor(0x080F16, alpha * 0.2f)}}))
          .filter(Filter::directionalBlur(18, 90, 12));
    };
    return box()
        .inset(0)
        .cache(Cache::Texture)
        .key("room")
        .children({
            each(bench["room"].array(), strut),
            kit::at(250, 118, 176, 470)
                .fill(
                    sigil::material::linearGradient({0, 0}, {0, 1},
                                          {{0.0f, hexColor(0x2A4A52, 0.34f)},
                                           {0.45f, hexColor(0x3E6A6E, 0.26f)},
                                           {1.0f, hexColor(0x0C1A20, 0.09f)}}))
                .filter(Filter::directionalBlur(22, 90, 16)),
        });
  }

  // ----------------------------------------------------------- the panel

  /** The glass: a translucent teal body lit from upper left, a haze of
   *  cyan around a crisp keyline, and inside it the contour whose fall is
   *  the header rule, echoed by a dotted rule just within. */
  static Element panel() {
    return kit::at(kPanelX, kPanelY, kPanelW, kPanelH)
        .shape(shapes::svg(kPanelOutline))
        .fill(sigil::material::from(sigil::material::radialGradient({0.4f, 0.32f}, 1.15f,
                                  {{0.0f, hexColor(0x2A4A4C, 0.74f)},
                                   {0.55f, hexColor(0x16302F, 0.72f)},
                                   {1.0f, material::withAlpha(kBody, 0.70f)}})).effects(sigil::material::Filter::shadow(cyan(0.28f), {.blur = 9})))
        
        .foreground(
            decorations::wash(field::grain(0.9f, 2, 7.0f),
                              sigil::material::BlendMode::Overlay, 0.07f))
        .foreground(
            decorations::border(2.2f, Fill::color(hexColor(0xCFF2F5, 0.95f))))
        .children({
            kit::at(12, 12, kPanelW - 24, kPanelH - 24)
                .shape(shapes::svg(kPanelContour))
                .fill(Fill::none())
                .foreground(decorations::border(1.1f, Fill::color(cyan(0.55f))))
                .foreground(Border{.width = 1,
                                   .fill = Fill::color(cyan(0.26f)),
                                   .inset = 7,
                                   .dash = {2, 6}}),
        });
  }

  /** The weapon's name over the header rule, the repair tier under it at
   *  left, and the suit's integrity as a ring gauge at right. */
  Element header() const {
    const float integrity = (float)bench["integrity"].number(1);
    return box().inset(0).children({
        kit::at(kPanelX, kPanelY + 20, kPanelW, 62)
            .alignItems(Align::Center)
            .justifyContent(Justify::Center)
            .children({text(bench["weapon"].string())
                           .styleClass("weapon")
                           .filter(sigil::material::Filter::glow(cyan(0.5f), 5))}),
        text(bench["repair"].string()).styleClass("caption").left(142).top(117),
        kit::disc({799, 123}, 13)
            .shape(shapes::annulus(0.58f))
            .fill(Fill::color(cyan(0.18f))),
        kit::disc({799, 123}, 13)
            .shape(shapes::sector(-90, 360 * integrity, 0.58f))
            .fill(Fill::color(cyan(0.9f))),
        text("R.I.G. INTEGRITY " + std::to_string((int)(integrity * 100)) + "%")
            .styleClass("caption")
            .left(820)
            .top(117),
    });
  }

  // ---------------------------------------------------------- the boards

  /** One cell of a board: a dark socket, or a bought node lit in its
   *  kind's colours inside a ragged corona, with its kind named at its
   *  upper right. */
  static Element node(SkPoint at, const std::string& cell, bool large) {
    if (cell == "o")
      return kit::disc(sigil::geometry::path::fromSk(at), large ? 10.5f : 6).fill(Fill::color(kSocket))
          .foreground(decorations::border(1.7f, Fill::color(cyan(0.88f))));
    const Kind kind = kindOf(cell);
    const float radius = large ? 14 : 9.5f;
    return box().inset(0).children({
        kit::disc(sigil::geometry::path::fromSk(at),radius + 12)
            .shape(shapes::ticks({.divisions = 24, .mark = {0.72f, 1.0f}}))
            .stroke(stroke(0.9f, Fill::color(cyan(0.22f)))),
        kit::disc(sigil::geometry::path::fromSk(at), radius).fill(sigil::material::from(sigil::material::radialGradient(
                {0.38f, 0.34f}, 0.8f,
                {{0.0f, material::mixToward(kind.core, kind.rim, 0.45f, 1)},
                 {0.6f, kind.core},
                 {1.0f, material::scale(kind.core, 0.7f)}})).effects(sigil::material::Filter::shadow(cyan(0.4f), {.blur = 5})))
            
            .foreground(decorations::border(2.4f, Fill::color(kind.rim))),
        text(cell)
            .styleClass(large ? "node" : "node-small")
            .centerAt({at.fX + (large ? 26.0f : 22.0f),
                       at.fY - (large ? 30.0f : 22.0f)}),
    });
  }

  /** The way current enters a board: an arrow, and on the weapon's board a
   *  housing and a bracket around the first socket. */
  static Element socket(SkPoint at, bool large) {
    const auto arrow = [](float x, float y, float size) {
      return kit::at(x, y, size, size)
          .shape(shapes::polygon(3, 90))
          .fill(Fill::color(cyan(0.8f)));
    };
    if (!large) return arrow(at.fX - 26, at.fY - 8, 16);
    return box().inset(0).children({
        kit::at(at.fX - 68, at.fY - 41, 76, 82)
            .fill(Fill::none())
            .foreground(decorations::border(1.6f, Fill::color(cyan(0.78f)))),
        kit::at(at.fX - 52, at.fY - 22, 16, 44)
            .shape(shapes::svg("M16 0 L0 0 L0 44 L16 44"))
            .fill(Fill::none())
            .stroke(stroke(1.6f, Fill::color(cyan(0.78f)))),
        arrow(at.fX - 100, at.fY - 12, 24),
    });
  }

  /** A board: traces first, lit as twin rails, then its cells over them.
   *  The smaller boards carry a caption over a rule, with the count of
   *  nodes bought against sockets at its right. */
  static Element board(const data::Json& spec) {
    const glm::vec2 origin{(float)spec["origin"][size_t(0)].number(),
                           (float)spec["origin"][1].number()};
    const glm::vec2 pitch{(float)spec["pitch"][size_t(0)].number(),
                          (float)spec["pitch"][1].number()};
    const bool large = spec["large"].boolean();
    const auto cellAt = [&](const data::Json& cell) {
      return origin + pitch * glm::vec2((float)cell[size_t(0)].number(),
                                        (float)cell[1].number());
    };

    std::vector<Element> traces, cells;
    for (const data::Json& run : spec["runs"].array()) {
      path::Polyline line;
      for (const data::Json& cell : run.array()) line.points.push_back(cellAt(cell));
      traces.push_back(pathFigure(path::toPath(line), 4)
                           .stroke(stroke(large ? 5.4f : 4.6f, Fill::color(cyan(0.62f))))
                           .foreground(stroke(large ? 3.0f : 2.4f,
                                              Fill::color(hexColor(0x14302F)))));
    }
    int bought = 0, sockets = 0;
    size_t row = 0;
    for (const data::Json& line : spec["grid"].array()) {
      const std::vector<std::string> tokens = words(line.string());
      for (size_t column = 0; column < tokens.size(); ++column) {
        if (tokens[column] == ".") continue;
        ++sockets;
        bought += tokens[column] != "o";
        const glm::vec2 at = origin + pitch * glm::vec2(column, row);
        cells.push_back(node({at.x, at.y}, tokens[column], large));
      }
      ++row;
    }

    const std::string_view caption = spec["caption"].string();
    return box().inset(0).filter(sigil::material::Filter::glow(cyan(0.3f), 3)).children({
        box().inset(0).children(std::move(traces)),
        socket({origin.x, origin.y + pitch.y}, large),
        box().inset(0).children(std::move(cells)),
        caption.empty()
            ? box()
            : kit::at(origin.x - 34, origin.y - 58, 350, 34)
                  .column()
                  .gap(8)
                  .children({
                      box().row().justifyContent(Justify::SpaceBetween).children({
                          text(caption).styleClass("board"),
                          text(std::to_string(bought) + " / " +
                               std::to_string(sockets) + " NODES")
                              .styleClass("count"),
                      }),
                      kit::line({.fill = Fill::color(cyan(0.28f))}),
                  }),
    });
  }

  // ------------------------------------------------------------ the band

  /** A region of the band: a dark chamfered card inside four corner
   *  brackets that stand off it. */
  static Element bracketed(float x, float width, Element card) {
    return kit::at(x - 8, kBandY - 8, width + 16, kBandH + 16)
        .stroke(spans::corners(24), stroke(1.5f, Fill::color(cyan(0.72f))))
        .padding(8)
        .children({card.flexGrow(1)
                       .shape(shapes::chamfered(12))
                       .fill(Fill::color(hexColor(0x102A2A, 0.62f)))});
  }

  /** One statistic: its name, a marker in its kind's colours, the load bar
   *  of pips it has filled out of its total, and its value past a rule. */
  static Element statistic(const data::Json& stat) {
    const Kind kind = kindOf(stat["kind"].string());
    const int filled = (int)stat["filled"].number();
    const material::Color metal =
        material::mixToward(hexColor(0xC8DADA), kind.rim, 0.35f, 1);
    const auto pip = [&](size_t index) {
      Element pip = box().width(36).height(17).shape(shapes::svg(kPip));
      if ((int)index >= filled)
        return pip.fill(Fill::none()).stroke(stroke(1.2f, Fill::color(cyan(0.3f))));
      return pip
          .fill(sigil::material::linearGradient({0, 0}, {0, 1},
                                      {{0.0f, metal},
                                       {0.45f, material::scale(metal, 0.74f)},
                                       {0.55f, material::scale(metal, 0.6f)},
                                       {1.0f, material::scale(metal, 0.88f)}}))
          .stroke(stroke(1, Fill::color(cyan(0.5f))));
    };
    return box()
        .row()
        .alignItems(Align::Center)
        .height(24)
        .children({
            box()
                .width(160)
                .alignItems(Align::End)
                .children({text(stat["label"].string()).styleClass("spec")}),
            box()
                .width(9)
                .height(9)
                .margin(0, 13)
                .shape(shapes::circle())
                .fill(sigil::material::radialGradient(
                    {0.5f, 0.5f}, 0.6f, {{0.0f, kind.rim}, {1.0f, kind.core}})),
            box().row().gap(6).children(
                {each((size_t)stat["total"].number(), pip)}),
            box().flexGrow(1),
            box().width(98).paddingLeft(12).children(
                {text(stat["value"].string()).styleClass("value")}),
        });
  }

  /** The specification table: three column heads over a rule, one row
   *  per statistic, and the value column parted off by a rule. */
  Element specification() const {
    return bracketed(
        140, 680,
        box().column().padding(12, 20).gap(3).children({
            box().row().height(14).children({
                box().width(160).alignItems(Align::End).children(
                    {text("SPECIFICATION").styleClass("head")}),
                box().width(35),
                text("NANOCIRCUIT LOAD").styleClass("head"),
                box().flexGrow(1),
                box().width(98).paddingLeft(12).children(
                    {text("VALUE").styleClass("head")}),
            }),
            kit::line({.fill = Fill::color(cyan(0.26f))}).marginBottom(4),
            each(bench["stats"].array(), statistic),
            kit::at(680 - 118, 14, 1, kBandH - 28)
                .fill(Fill::color(cyan(0.26f))),
        }));
  }

  /** The power nodes the player holds: a chip naming them, the brass
   *  node itself — the one warm object on an all-cyan screen — and the
   *  count. */
  Element counter() const {
    return bracketed(
        852, 208,
        box()
            .column()
            .alignItems(Align::Center)
            .padding(11, 16)
            .gap(2)
            .children({
                kit::centred(text("NODES").styleClass("chip"))
                    .width(112)
                    .height(21)
                    .shape(shapes::chamfered(6))
                    .stroke(stroke(1, Fill::color(cyan(0.45f)))),
                box().width(66).height(46).marginTop(6).children({
                    kit::at(2, 14, 62, 28)
                        .borderRadius({14})
                        .fill(sigil::material::linearGradient(
                            {0, 0}, {0, 1},
                            {{0.0f, hexColor(0xC9A227)},
                             {0.45f, hexColor(0x7E6318)},
                             {1.0f, hexColor(0x5E4914)}})),
                    kit::at(2, 2, 62, 27)
                        .shape(shapes::squircle(2))
                        .fill(
                            sigil::material::linearGradient({0, 0}, {1, 1},
                                                  {{0.0f, hexColor(0xE8C860)},
                                                   {0.4f, hexColor(0xD3AA33)},
                                                   {1.0f, hexColor(0x8E6F1E)}}))
                        .stroke(
                            stroke(1, Fill::color(hexColor(0xF3DC94, 0.75f)))),
                    kit::at(22, 8, 24, 13)
                        .shape(shapes::squircle(2))
                        .fill(Fill::none())
                        .stroke(stroke(1.3f,
                                       Fill::color(hexColor(0x74590F, 0.9f)))),
                }),
                text(std::to_string((int)bench["nodes"].number()))
                    .styleClass("numeral"),
            }));
  }

  /** The controller hints on the panel's bottom rail, under a rule, and
   *  the two empty sockets the bezel carries in its bottom corners. */
  Element hints() const {
    const auto hint = [](const data::Json& words) {
      return text(words.string()).styleClass("hint");
    };
    const auto navigate =
        box().width(15).height(15).shape(shapes::circle()).fill(Fill::none())
            .foreground(decorations::border(1.3f, Fill::color(cyan(0.82f))))
            .alignItems(Align::Center).justifyContent(Justify::Center)
            .children({box().width(6).height(6).shape(shapes::circle())
                           .fill(Fill::color(cyan(0.6f)))});
    const auto bezelSocket = [](float x) {
      return kit::at(x, 719, 12, 12)
          .fill(Fill::none())
          .foreground(decorations::border(1.2f, Fill::color(cyan(0.45f))));
    };
    return box().inset(0).children({
        kit::at(kPanelX + 32, 706, kPanelW - 64, 1).fill(Fill::color(cyan(0.36f))),
        kit::at(kPanelX, 714, kPanelW, 26)
            .row()
            .alignItems(Align::Center)
            .justifyContent(Justify::Center)
            .gap(56)
            .children({
                box().row().alignItems(Align::Center).gap(8).children(
                    {navigate, hint(bench["hints"][size_t(0)])}),
                hint(bench["hints"][1]),
                hint(bench["hints"][2]),
            }),
        bezelSocket(kPanelX + 34),
        bezelSocket(kPanelX + kPanelW - 46),
    });
  }

  /** Nothing on the bench moves, so the whole of it is kept as one
   *  image and drawn again from that. */
  Element describe() const {
    return box()
        .inset(0)
        .cache(Cache::Texture)
        .key("ds2_bench.bench")
        .fill(sigil::material::radialGradient({0.5f, 0.5f}, 0.9f,
                                    {{0.0f, hexColor(0x09131B)},
                                     {0.6f, hexColor(0x050B11)},
                                     {1.0f, hexColor(0x020406)}}))
        .applyStyleSheet(registers())
        .children({
            room(),
            panel(),
            header(),
            each(bench["circuits"].array(), board),
            specification(),
            counter(),
            hints(),
            // The tube the hologram is seen through: fine scanlines and a
            // falloff into the corners, laid over everything.
            box().inset(0).fill(ds2_bench::crtOverlay({.uScanPitch = 3,
                                                   .uScanStrength = 0.09f,
                                                   .uVigInner = 1.0f,
                                                   .uVigOuter = 1.9f,
                                                   .uVigStrength = 0.6f,
                                                   .uSqueeze = 0.7f})),
        });
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kVoid);
    context.captureAt(2.5);
    bench = sketch::kit::Document(context, "data/bench.json");
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(Ds2Bench, "Study · Game UI",
             "Dead Space 2's Nanocircuit bench (2011) — routers, rails, "
             "a diegetic hologram")
