// Fallout 2 (Black Isle Studios, 1998), the character screen: a riveted,
// rusted metal plate with black wells cut into it, green bitmap type in the
// wells, engraved gold lettering on the plate, white numerals on counter
// wheels, and a parchment scrap describing the selected skill. The screen is
// 640 x 480 and every coordinate below is the game's own, in its pixels.
//
// `data/character.json` is everything the sheet prints: the S.P.E.C.I.A.L.
// scores and their grades, the status and derived rows, the perks and
// traits, and the eighteen skills with the formula and description each
// card carries. `data/art.json` holds the counter numerals as pixel grids
// and the card's line figure as SVG paths.
//
// The screen moves only when it is used: the selection walks Small Guns,
// Doctor, Melee Weapons, and three points are then spent on Melee Weapons,
// each press rolling the SKILL POINTS counter.
//
// TAGS: Interfaces/Game

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <map>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace field = sigil::material::field;
namespace document = sigil::compose::document;
using sigil::data::Json;
using sigil::material::skia::Paint;
using namespace sigil::compose;

namespace {

/** One screen pixel, in canvas pixels. */
constexpr float kPixel = 2.0f;
constexpr float px(float v) { return v * kPixel; }

Element screen(float x, float y, float width, float height) {
  return kit::at(px(x), px(y), px(width), px(height));
}
Element screen(Element node, float x, float y, float width, float height) {
  return kit::at(std::move(node), px(x), px(y), px(width), px(height));
}

// The palette the screen actually shows.
const material::Color kGreen = hexColor(0x3CF800);
const material::Color kInactive = hexColor(0x183018);
const material::Color kTagged = hexColor(0xA0A0A0);
const material::Color kSelected = hexColor(0xFCFC7C);
const material::Color kGold = hexColor(0x907824);
const material::Color kGoldShadow = hexColor(0x3C2C0C);
const material::Color kNumeral = hexColor(0xF2EEDC);
const material::Color kWell = hexColor(0x040C00);
const material::Color kKeyline = hexColor(0x14100A);
const material::Color kBevelLight = hexColor(0xA08858, 0.55f);
const material::Color kBevelShade = hexColor(0x0C0906, 0.65f);
const material::Color kParchmentGold = hexColor(0xBC9054);

/** The cast plate, the recesses cut into it and the card stuck on it. */
const Paint kPlate = Paint::linearGradient({0, 0}, {0.15f, 1},
                                           {{0.0f, hexColor(0x483828)},
                                            {0.4f, hexColor(0x383020)},
                                            {1.0f, hexColor(0x302820)}});
const material::Material kPlateTooth = field::grain(0.22f, 3, 11.0f, 0.65f);
const Paint kRust =
    Paint::blend({{Paint::solid(hexColor(0x7C581C)), SkBlendMode::kSrcOver},
                  {Paint::recipe(field::grain(0.0075f, 3, 5.0f, 1.35f)),
                   SkBlendMode::kMultiply}});
const Paint kRaised = Paint::linearGradient({0, 0}, {0, 1},
                                            {{0.0f, hexColor(0x483828)},
                                             {0.55f, hexColor(0x383020)},
                                             {1.0f, hexColor(0x302820)}});
const Paint kWheel = Paint::linearGradient({0, 0}, {0, 1},
                                           {{0.0f, hexColor(0x3C3C3C)},
                                            {0.22f, hexColor(0x545454)},
                                            {0.58f, hexColor(0x282828)},
                                            {1.0f, hexColor(0x1C1C1C)}});
const Paint kParchment =
    Paint::blend({{Paint::linearGradient({0.1f, 0}, {0.9f, 1},
                                         {{0.0f, kParchmentGold},
                                          {0.3f, hexColor(0xAC8044)},
                                          {0.66f, hexColor(0x9C7434)},
                                          {1.0f, hexColor(0x8C6428)}}),
                   SkBlendMode::kSrcOver},
                  {Paint::recipe(field::grain(0.013f, 4, 21.0f, 0.62f, 1.4f)),
                   SkBlendMode::kOverlay}});
const Paint kRivet = Paint::radialGradient({0.34f, 0.3f}, 1.15f,
                                           {{0.0f, hexColor(0x6A5838)},
                                            {0.55f, hexColor(0x3A3020)},
                                            {1.0f, hexColor(0x140F08)}});
const Paint kLamp = Paint::radialGradient({0.35f, 0.3f}, 1.1f,
                                          {{0.0f, hexColor(0xFF6A4A)},
                                           {0.45f, hexColor(0xF80000)},
                                           {1.0f, hexColor(0x600000)}});

/** The screen's registers. The green bitmap face is the root; a row names
 *  its state by class, and the engraved gold names its face. */
StyleSheet registers() {
  return StyleSheet{
      rule(".inactive").ink(kInactive), rule(".tagged").ink(kTagged),
      rule(".selected").ink(kSelected),
      rule(".engraved").ink(kGold).font(
          {.face = weave::ports::face({"Impact", "Haettenschweiler"}, 400),
           .track = 0.4f,
           .condense = 0.84f}),
      rule(".card").ink(hexColor(0x000000))};
}

/** Engraved gold: the letters pressed into the plate, their shadow a pixel
 *  down and to the right. */
Element engraved(const std::string& words, float size) {
  return text(words)
      .styleClass("engraved")
      .font({.size = px(size)})
      .layerStyle(LayerStyle::echo({px(1), px(1)}, kGoldShadow));
}

/** A recess cut into the plate: near-black with a green cast, a hard
 *  keyline and a moulded edge lit from the upper left. */
Element well(float x, float y, float width, float height) {
  Element recess = screen(x, y, width, height)
                       .fill(kWell)
                       .borderRadius(Corners{px(3)})
                       .stroke(stroke(px(1), Fill::color(kKeyline),
                                      PathFormat::Align::Inner))
                       .foreground(stroke(px(1.5f),
                                          Fill::color(hexColor(0x5C4C30, 0.85f)),
                                          PathFormat::Align::Outer));
  return kit::bevelled(recess, kit::bevels::plate(kBevelLight, kBevelShade,
                                                  px(1.2f), px(1.6f), true));
}

/** A plaque standing proud of the plate: the plaques, tabs and bars. */
Element raised(float x, float y, float width, float height) {
  Element plaque = screen(x, y, width, height)
                       .fill(kRaised)
                       .borderRadius(Corners{px(2.5f)})
                       .stroke(stroke(px(1), Fill::color(kKeyline),
                                      PathFormat::Align::Inner))
                       .alignItems(Align::Center)
                       .justifyContent(Justify::Center);
  return kit::bevelled(plaque, kit::bevels::plate(kBevelLight, kBevelShade,
                                                  px(1.4f), px(1.8f)));
}

/** A red button lamp, 12 px across. */
Element lamp(float x, float y) {
  return screen(x, y, 12, 12)
      .borderRadius(Corners{px(6)})
      .fill(kLamp)
      .foreground(stroke(px(1.2f), Fill::color(hexColor(0x1A1208)),
                         PathFormat::Align::Outer));
}

/** One printed row: a label, a value, and the class naming its state. */
struct Line {
  std::string label, value, tone;
};

/** Rows at one pitch in a column @p width wide, the first line topping out
 *  at (@p x, @p y); each value's left edge stands @p valueAt right of its
 *  label's. The game left-aligns every value column, so 6% and 71% start
 *  at one x. */
Element rows(float x, float y, float width, float pitch, float valueAt,
             const std::vector<Line>& lines) {
  const float height = pitch * (float)lines.size();
  return screen(x, y - 2, width, height).column().children({each(lines, [&](const Line& line) {
    return box()
        .row()
        .height(px(pitch))
        .flexShrink(0)
        .styleClass(line.tone)
        .children({line.value.empty()
                       ? text(line.label)
                       : text(line.label).width(px(valueAt)).flexShrink(0),
                   text(line.value)});
  })});
}

/** Everything read from `data/`. */
struct Sheet {
  Json character;
  std::map<char, kit::Sprite> numerals;
  std::map<std::string, shapes::Svg, std::less<>> figures;

  void read(sketch::SketchContext& context) {
    const auto file = [&](const char* name) {
      return context.assets.json(context.local(std::string("data/") + name));
    };
    if (const auto found = file("character.json")) character = *found;
    const auto art = file("art.json");
    if (!art) return;
    static const std::array<material::Color, 2> kKey{
        material::Color{0, 0, 0, 0}, kNumeral};
    for (const auto& [digit, grid] : (*art)["digits"].fields()) {
      std::vector<std::string> lines;
      for (const Json& line : grid.items()) lines.emplace_back(line.text());
      if (auto sprite = kit::pixelMap(lines, {".#", kKey}))
        numerals[digit[0]] = *sprite;
    }
    // Each figure is the one body with the skill's prop in its hand.
    const Json& figure = (*art)["figure"];
    const std::string body(figure["body"].text());
    for (const auto& [prop, path] : figure.fields())
      if (prop != "body")
        figures[prop] = shapes::svg((body + " " + std::string(path.text())).c_str(), true);
  }
};

struct Fallout2CharSheet {
  Sheet sheet;
  /** The skill the cursor is on, the points spent on it, and which counter
   *  wheel shows its blank cell as it rolls. */
  struct State {
    int selected = 4, presses = 0;
    bool blankOnes = false;
    bool operator==(const State&) const = default;
  } state;

  /** A two-wheel counter at (@p x, @p y): white numerals on 14 x 24 drums.
   *  A change blanks the ones wheel for one beat before the new numeral
   *  shows — a mechanical counter, never a fade. */
  Element counter(float x, float y, int value, bool blankOnes = false) const {
    const std::string digits = (value < 10 ? "0" : "") + std::to_string(value);
    const auto wheel = [&](char digit, bool blank) {
      Element drum = box()
                         .width(px(13))
                         .height(px(24))
                         .fill(kWheel)
                         .borderRadius(Corners{px(2)})
                         .alignItems(Align::Center)
                         .justifyContent(Justify::Center);
      const auto numeral = sheet.numerals.find(digit);
      if (!blank && numeral != sheet.numerals.end())
        drum.children({kit::pixelSprite(numeral->second, {.cell = kPixel})});
      return drum;
    };
    return screen(x - 1, y - 1, 30, 26)
        .fill(hexColor(0x000000))
        .borderRadius(Corners{px(2)})
        .row()
        .padding(px(1))
        .gap(px(1))
        .children({wheel(digits[0], false), wheel(digits[1], blankOnes)});
  }

  /** The S.P.E.C.I.A.L. column: a lit panel riveted at its corners, seven
   *  rows at a pitch of 33 — the engraved abbreviation, its counter, and
   *  the grade on a dark plaque with a gold edge along its foot. */
  Element special() const {
    const float x = 5, y = 34, width = 152, height = 232;
    std::vector<Element> panel;
    panel.push_back(
        screen(x, y, width, height)
            .fill(Paint::linearGradient({0, 0}, {0.2f, 1},
                                        {{0.0f, hexColor(0x54462E)},
                                         {0.35f, hexColor(0x483828)},
                                         {1.0f, hexColor(0x3A3020)}}))
            .borderRadius(Corners{px(4)})
            .stroke(stroke(px(1), Fill::color(hexColor(0x1A1610)),
                           PathFormat::Align::Inner)));
    kit::bevelled(panel.back(),
                  kit::bevels::plate(hexColor(0xB09868, 0.55f),
                                     hexColor(0x080604, 0.7f), px(1.6f), px(2)));
    for (float cornerX : {x + 6, x + width - 6})
      for (float cornerY : {y + 6, y + height - 6})
        panel.push_back(kit::disc({px(cornerX), px(cornerY)}, px(2.6f)).fill(kRivet));
    const auto& stats = sheet.character["special"].items();
    for (size_t row = 0; row < stats.size(); ++row) {
      const Json& stat = stats[row];
      const float top = 37 + 33 * (float)row;
      panel.push_back(screen(engraved(std::string(stat["stat"].text()) + "-", 29),
                             19, top - 5, 40, 30));
      panel.push_back(counter(58, top, (int)stat["value"].number()));
      panel.push_back(
          screen(text(std::string(stat["grade"].text())), 100, top + 4, 58, 17)
              .fill(kWell)
              .borderRadius(Corners{px(1.5f)})
              .paddingLeft(px(3))
              .justifyContent(Justify::Center)
              .background(styles::dropShadow(kParchmentGold, {px(-3), px(2)}, 0))
              .foreground(styles::InnerShadow{hexColor(0x000000, 0.75f),
                                              {0, px(1.2f)}, px(2)}));
    }
    return box().inset(0).children({panel});
  }

  /** Hit points and the seven conditions, dead grey-green until they
   *  apply; the ten derived statistics; level and experience. */
  Element statistics() const {
    std::vector<Line> status{{"Hit Points", std::string(sheet.character["hitPoints"].text()), ""}};
    for (const Json& condition : sheet.character["conditions"].items())
      status.push_back({std::string(condition.text()), "", "inactive"});
    std::vector<Line> derived;
    for (const Json& pair : sheet.character["derived"].items())
      derived.push_back({std::string(pair[0].text()), std::string(pair[1].text()), ""});
    std::vector<Line> level;
    for (const Json& line : sheet.character["level"].items())
      level.push_back({std::string(line.text()), "", ""});
    return box().inset(0).children(
        {well(188, 37, 130, 118), rows(194, 46, 120, 13, 69, status),
         well(188, 171, 130, 143), rows(194, 179, 120, 13, 94, derived),
         well(25, 275, 132, 40), rows(32, 280, 120, 11, 0, level)});
  }

  /** The folder, PERKS tab up: a heading ruled out to both margins, then
   *  its entries, for the perks and then the traits. */
  Element folder() const {
    const auto heading = [](const char* words) {
      return box().row().height(px(11)).flexShrink(0).alignItems(Align::Center).gap(px(4)).children(
          {box().height(px(1)).flexGrow(1).fill(Fill::currentInk()), text(words),
           box().height(px(1)).flexGrow(1).fill(Fill::currentInk())});
    };
    const auto entries = [](const Json& list) {
      return each(list.items(), [](const Json& entry) {
        return text(std::string(entry.text())).height(px(11)).flexShrink(0);
      });
    };
    static constexpr std::array<const char*, 3> kTabs{"PERKS", "KARMA", "KILLS"};
    static constexpr std::array<float, 4> kTabEdges{25, 110, 208, 300};
    std::vector<Element> tabs;
    for (size_t tab = 0; tab < kTabs.size(); ++tab) {
      const bool up = tab == 0;
      tabs.push_back(raised(kTabEdges[tab], up ? 327 : 330,
                            kTabEdges[tab + 1] - kTabEdges[tab], up ? 33 : 29)
                         .opacity(up ? 1.0f : 0.72f)
                         .children({engraved(kTabs[tab], 23)}));
    }
    const auto arrow = [](float y, const char* path) {
      return screen(317, y, 11, 12)
          .fill(kRaised)
          .borderRadius(Corners{px(1)})
          .padding(px(3), px(2))
          .children({box().flexGrow(1).fill(kGold).shape(shapes::svg(path))});
    };
    return box().inset(0).children(
        {tabs, well(25, 359, 291, 112),
         screen(34, 362, 280, 100)
             .column()
             .children({heading("PERKS"), entries(sheet.character["perks"]),
                        heading("TRAITS"), entries(sheet.character["traits"])}),
         arrow(361, "M0 1 L0.5 0 L1 1 Z"), arrow(456, "M0 0 L1 0 L0.5 1 Z")});
  }

  /** The eighteen skills at a pitch of 11: tagged skills grey, the
   *  selected one yellow, the rest green; the +/- slider stands against
   *  the selected row. */
  Element skills() const {
    std::vector<Line> lines;
    const auto& list = sheet.character["skills"].items();
    for (size_t index = 0; index < list.size(); ++index) {
      const Json& skill = list[index];
      const bool selected = (int)index == state.selected;
      const int value = (int)skill["value"].number() + (selected ? 2 * state.presses : 0);
      lines.push_back({std::string(skill["name"].text()), std::to_string(value) + "%",
                       selected ? "selected" : (skill["tagged"].boolean() ? "tagged" : "")});
    }
    const auto button = [](const char* sign) {
      return box().width(px(12)).height(px(9)).fill(hexColor(0x3A3020))
          .borderRadius(Corners{px(1.5f)})
          .stroke(stroke(px(0.8f), Fill::color(hexColor(0x8A7448, 0.7f)),
                         PathFormat::Align::Inner))
          .alignItems(Align::Center).justifyContent(Justify::Center)
          .children({text(sign).font({.size = px(7)}).ink(kGold)});
    };
    const float sliderTop = (float)state.selected * 11 + 27 - 7;
    const int points = (int)sheet.character["skillPoints"].number();
    return box().inset(0).children(
        {screen(engraved("SKILLS", 24), 380, 1, 60, 24), well(368, 23, 249, 202),
         rows(380, 27, 233, 11, 193, lines),
         screen(598, sliderTop, 36, 24)
             .fill(kRaised)
             .borderRadius(Corners{px(2)})
             .row()
             .padding(px(2))
             .gap(px(2))
             .alignItems(Align::Center)
             .children({box().width(px(16)).height(px(12)).fill(hexColor(0x141008)),
                        box().column().gap(px(2)).children({button("+"), button("-")})}),
         raised(336, 226, 292, 30),
         screen(engraved("SKILL POINTS", 24), 400, 229, 110, 26),
         well(520, 226, 34, 28),
         counter(522, 228, points - state.presses, state.blankOnes)});
  }

  /** The description card: the skill's name in the heavy face with its
   *  formula on the same baseline, a two-pixel rule, and the description
   *  wrapped greedily short of the figure, ragged right, at the body's
   *  pitch of 11. */
  Element card() const {
    const Json& skill = sheet.character["skills"][(size_t)state.selected];
    const auto figure = sheet.figures.find(skill["figure"].text("club"));
    Element drawing = screen(180, 42, 64, 124);
    if (figure != sheet.figures.end())
      drawing.shape(figure->second).fill(Fill::none()).stroke(stroke(px(1.1f)));
    return screen(345, 267, 277, 170)
        .fill(kParchment)
        .borderRadius(Corners{px(1)})
        .overflow(Overflow::Clip)
        .stroke(stroke(px(1.5f), Fill::color(hexColor(0x2A1C08, 0.75f)),
                       PathFormat::Align::Inner))
        .styleClass("card")
        .column()
        .padding(px(2), px(3))
        .children({box()
                       .row()
                       .gap(px(8))
                       .alignItems(Align::Baseline)
                       .children({engraved(std::string(skill["name"].text()), 26)
                                      .ink(hexColor(0x000000))
                                      .layerStyle(LayerStyle{}),
                                  text(std::string(skill["formula"].text()))}),
                   kit::line({.length = Dimension(px(265)), .thickness = px(2)})
                       .marginTop(px(2)),
                   text(std::string(skill["blurb"].text()))
                       .width(px(160))
                       .marginTop(px(12))
                       .paragraph({.leading = weave::Leading::absolute(px(11))}),
                   drawing});
  }

  /** PRINT, DONE and CANCEL along the foot, each behind a red lamp. */
  Element buttons() const {
    static constexpr std::array<std::pair<const char*, float>, 3> kButtons{
        {{"PRINT", 344}, {"DONE", 457}, {"CANCEL", 553}}};
    return box().inset(0).children({each(kButtons, [](const auto& button) {
      return box().inset(0).children(
          {lamp(button.second, 455),
           screen(engraved(button.first, 22), button.second + 20, 450, 70, 24)});
    })});
  }

  /** The plate's frame and the three raised seams between its panels. */
  Element chrome() const {
    const auto seam = [](float x, float y, float width, float height,
                         bool down) {
      return screen(x, y, width, height)
          .fill(Paint::linearGradient(
              {0, 0}, down ? SkPoint{0, 1} : SkPoint{1, 0},
              {{0.0f, hexColor(0x554430)}, {1.0f, hexColor(0x241D12)}}));
    };
    static constexpr std::array<std::pair<const char*, std::array<float, 2>>, 3> kPlaques{
        {{"name", {14, 140}}, {"age", {155, 82}}, {"sex", {238, 76}}}};
    return box().inset(0).children(
        {seam(328, 0, 4, 480, false), seam(165, 30, 3, 240, false),
         seam(5, 318, 320, 3, true), each(kPlaques, [&](const auto& plaque) {
           return raised(plaque.second[0], 0, plaque.second[1], 26)
               .children({engraved(std::string(sheet.character[plaque.first].text()), 26)});
         })});
  }

  Element describe() const {
    Element face = screen(0, 0, 640, 480)
                       .fill(kPlate)
                       .overflow(Overflow::Clip)
                       .font({.face = weave::ports::face({"Verdana", "DejaVu Sans"}, 700),
                              .size = px(9),
                              .condense = 0.88f})
                       .ink(kGreen)
                       .applyStyleSheet(registers())
                       .foreground(stroke(px(3), Fill::color(hexColor(0x1E1810)),
                                          PathFormat::Align::Inner));
    face.children(
        {box().inset(0).fill(kPlateTooth).blendMode(SkBlendMode::kOverlay).opacity(0.3f),
         box().inset(0).fill(kRust).blendMode(SkBlendMode::kSoftLight).opacity(0.55f),
         chrome(), special(), statistics(), folder(), skills(), card(), buttons()});
    return box().inset(0).children(
        {face,
         screen(0, 480, 640, 100)
             .fill(hexColor(0x0B0D08))
             .column()
             .padding(20, 28)
             .gap(8)
             .ink(hexColor(0xB5B5A2))
             .children({document::h1("FALLOUT 2 · THE CHARACTER SHEET")
                            .font({.size = 21, .color = hexColor(0xC0A44E), .track = 1.2f}),
                        document::caption("Black Isle Studios, 1998 · 640×480 in "
                                          "indexed colour, rebuilt at twice its size")
                            .font({.size = 14})})});
  }

  /** The moment the screen is showing at @p elapsed seconds into its
   *  nine-second loop: 1.3 s on each of the first two skills, then Melee
   *  Weapons, with a point spent at 3.4 s and every 0.6 s after. */
  static State at(double elapsed) {
    static constexpr std::array<int, 3> kWalk{0, 7, 4};
    static constexpr double kDwell = 1.3, kFirstPress = 3.4, kPressGap = 0.6;
    static constexpr double kBlank = 0.123;
    const double time = std::fmod(elapsed, 9.0);
    State now;
    now.selected = kWalk[std::min<size_t>(2, (size_t)(time / kDwell))];
    if (now.selected != kWalk[2]) return now;
    for (int press = 0; press < 3; ++press)
      if (time >= kFirstPress + kPressGap * press) now.presses = press + 1;
    if (now.presses == 0) return now;
    const double since = time - (kFirstPress + kPressGap * (now.presses - 1));
    now.blankOnes = since < kBlank;
    return now;
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(px(640), px(580));
    context.background(hexColor(0x050604));
    context.captureAt(6.0);
    context.oversample(2);
    sheet.read(context);
    state = at(0);
    context.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& context) {
    const State now = at(elapsed);
    if (now == state) return;
    state = now;
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(Fallout2CharSheet, "Study · Game UI",
             "Fallout 2's character screen (1998) at 2×: riveted plate, "
             "counter numerals, skills and the parchment card")
