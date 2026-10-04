// Fallout 2 (Black Isle Studios, 1998), the character screen: a worn,
// riveted plate of brushed steel with phosphor displays sunk into it, green
// readouts glowing behind scan-lined glass, brass lettering stamped into
// the metal, white numerals on counter drums, and a parchment card that
// shows the selected entry with the Vault Boy acting it out. The screen is
// 640 x 480 and every coordinate below is the game's own, in its pixels.
//
// `data/character.json` is everything the sheet prints: the S.P.E.C.I.A.L.
// scores with their grades and cards, the status and derived rows, the
// perks and traits, and the eighteen skills with the formula and card each
// carries. `data/art.json` holds the counter numerals as pixel grids and
// the Vault Boy as SVG line art: one body, and the arms and prop of each
// pose.
//
// The sheet is used, not animated: the cursor visits Strength, then walks
// the skill list to Small Guns, Doctor and Melee Weapons; three points go
// into Melee Weapons, each press lighting the + key and rolling the SKILL
// POINTS drum; at the end CANCEL lights and the points come back.
//
// TAGS: Interfaces/Game

#include <sigilcompose/brush/Decorations.h>
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
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace field = sigil::material::field;
namespace document = sigil::compose::document;
using sigil::data::Json;
using sigil::material::BlendMode;
using sigil::material::Filter;
using sigil::material::hexColor;
using namespace sigil::compose;

namespace {

/** One screen pixel, in canvas pixels. */
constexpr float kPixel = 2.0f;
constexpr float px(float length) { return length * kPixel; }

Element screen(float x, float y, float width, float height) {
  return kit::at(px(x), px(y), px(width), px(height));
}
Element screen(Element node, float x, float y, float width, float height) {
  return kit::at(std::move(node), px(x), px(y), px(width), px(height));
}

// ---------------------------------------------------------------------------
// THE METAL

/** Brushed steel gone brown: a warm cast lit from the upper left, long
 *  horizontal grain from the brushing, blotches of rust, and the plate
 *  darkening toward its rim where hands never polished it. */
const material::Material kSteel =
    material::from(
        material::linearGradient(
            {0, 0}, {0.3f, 1},
            {hexColor(0x5E4E38), hexColor(0x4A3E2C), hexColor(0x2E261C)}))
        .layer(field::grain(0.004f, 3, 7, 0.9f, 28),
               {.blend = BlendMode::Overlay, .opacity = 0.3f})
        .layer(field::grain(0.005f, 2, 9, 1.2f, 36),
               {.blend = BlendMode::Overlay, .opacity = 0.35f})
        .layer(material::from(hexColor(0x8A5A22))
                   .layer(field::grain(0.009f, 4, 3, 1.6f),
                          {.blend = BlendMode::Multiply}),
               {.blend = BlendMode::SoftLight, .opacity = 0.8f})
        .layer(material::radialGradient({0.45f, 0.4f}, 1,
                                        {{0.0f, hexColor(0x000000, 0.0f)},
                                         {0.7f, hexColor(0x000000, 0.08f)},
                                         {1.0f, hexColor(0x000000, 0.5f)}}));

/** A plate screwed over the ground: the same steel, a shade lighter. */
const material::Material kRaisedSteel =
    material::from(
        material::linearGradient(
            {0, 0}, {0.1f, 1},
            {hexColor(0x6A5A40), hexColor(0x54462F), hexColor(0x3C3224)}))
        .layer(field::grain(0.005f, 3, 5, 0.9f, 18),
               {.blend = BlendMode::Overlay, .opacity = 0.45f});

/** The brass the lettering is stamped in, catching light on its top face
 *  and shadowed a pixel down-right into the plate. */
const material::Material kBrass =
    material::from(
        material::linearGradient(
            {0, 0}, {0, 1},
            {hexColor(0xE0C070), hexColor(0xB08C3C), hexColor(0x6C5220)}))
        .effects(
            Filter::shadow(hexColor(0x140C04, 0.9f), {.offset = {px(1), px(1)}})
                .then(Filter::bevel({.depth = px(0.6f),
                                     .size = px(0.8f),
                                     .highlight = hexColor(0xFFF0C0, 0.55f),
                                     .shadow = hexColor(0x2A1C08, 0.6f)})));

/** A screw head, and a lamp: domes lit from the upper left. */
const material::Material kScrew =
    material::radialGradient({0.34f, 0.3f}, 1.1f,
                             {hexColor(0xC8B894), hexColor(0x7A6A4C),
                              hexColor(0x3A3020), hexColor(0x16110A)});
const material::Material kLamp = material::radialGradient(
    {0.35f, 0.3f}, 1.1f,
    {hexColor(0xC04030), hexColor(0x801008), hexColor(0x300402)});
/** A lamp's light when it is pressed. */
const material::Material kLampLight =
    material::radialGradient({0.5f, 0.5f}, 1,
                             {{0.0f, hexColor(0xFFE0B0)},
                              {0.12f, hexColor(0xFF6040)},
                              {0.3f, hexColor(0xF01808, 0.75f)},
                              {1.0f, hexColor(0xF01808, 0.0f)}},
                             {.extent = material::RadialExtent::ClosestSide});

/** A counter's drum, rounded away from the eye at top and bottom. */
const material::Material kDrumShade =
    material::linearGradient({0, 0}, {0, 1},
                             {{0.0f, hexColor(0x000000, 0.85f)},
                              {0.28f, hexColor(0x000000, 0.0f)},
                              {0.62f, hexColor(0x000000, 0.0f)},
                              {1.0f, hexColor(0x000000, 0.9f)}});

// ---------------------------------------------------------------------------
// THE PHOSPHOR

const material::Color kPhosphor = hexColor(0x3CF800);

/** A display's glass: near-black with a green cast, brightest where the
 *  beam spends most of its time. */
const material::Material kScreenGround = material::radialGradient(
    {0.45f, 0.4f}, 1,
    {hexColor(0x0C1C06), hexColor(0x061004), hexColor(0x020502)});

/** The raster the tube draws in: every other line a little darker. */
const material::Material kScanLines = material::linearGradient(
    {0, 0}, {0, px(1)},
    {{0.5f, hexColor(0x000000, 0.0f)}, {0.5f, hexColor(0x000000, 0.34f)}},
    {.units = material::GradientUnits::Pixels,
     .repeat = material::Repeat::Repeat});

/** The curve of the glass catching the room's light from the upper left. */
const material::Material kSheen =
    material::linearGradient({0, 0}, {0.7f, 1},
                             {{0.0f, hexColor(0xE8FFE0, 0.10f)},
                              {0.35f, hexColor(0xE8FFE0, 0.02f)},
                              {0.36f, hexColor(0xE8FFE0, 0.0f)}});

/** The light of the lit text, spilling onto the glass around it. */
const Filter kGlow = Filter::glow(hexColor(0x3CF800, 0.55f), px(1.6f));

/** The parchment scrap the card is written on, browned at its edges. */
const material::Material kParchment =
    material::from(
        material::linearGradient({0.1f, 0}, {0.9f, 1},
                                 {hexColor(0xD8B478), hexColor(0xC49C5C),
                                  hexColor(0xAC8040), hexColor(0x8C6428)}))
        .layer(field::grain(0.013f, 4, 21, 0.62f, 1.4f),
               {.blend = BlendMode::Overlay})
        .layer(material::radialGradient({0.5f, 0.5f}, 1,
                                        {{0.0f, hexColor(0x3A2008, 0.0f)},
                                         {0.75f, hexColor(0x3A2008, 0.05f)},
                                         {1.0f, hexColor(0x3A2008, 0.45f)}}));

/** THE SCREEN'S REGISTERS. The readouts are a chunky monospace glowing
 *  green; a row names its state by class; the plate's lettering is a
 *  condensed grotesque stamped in brass; the card is typewritten under a
 *  heavy slab-serif title. */
StyleSheet registers() {
  return StyleSheet{
      rule(":root")
          .var("phosphor", kPhosphor)
          .var("phosphor-dead", hexColor(0x2C5A1E))
          .var("tagged", hexColor(0xC8D8C0))
          .var("selected", hexColor(0xFCFC7C))
          .var("card-ink", hexColor(0x1C1206))
          .font({.face = weave::ports::face({"Menlo", "Andale Mono"}, 700),
                 .size = px(8.4f),
                 .condense = 0.86f})
          .ink(var("phosphor")),
      rule(".dead").ink(var("phosphor-dead")),
      rule(".tagged").ink(var("tagged")),
      rule(".selected").ink(var("selected")),
      rule(".stamped")
          .font({.face = weave::ports::face({"Impact", "DIN Condensed"}, 400),
                 .track = px(0.3f),
                 .condense = 0.88f})
          .ink(kBrass),
      rule(".card")
          .font({.face = weave::ports::face(
                     {"American Typewriter", "Courier New"}, 600),
                 .size = px(7.6f),
                 .condense = 1.0f})
          .ink(var("card-ink")),
      // Steel standing proud of the plate: panels, plaques, keys, each
      // keylined where it meets the plate.
      rule(".raised")
          .fill(kRaisedSteel)
          .borderRadius(Corners{px(2)})
          .stroke(stroke(px(1), Fill::color(hexColor(0x100C08)),
                         PathFormat::Align::Outer)),
      rule(".card-title")
          .font(
              {.face = weave::ports::face({"Superclarendon", "Rockwell"}, 900),
               .size = px(19),
               .track = px(-0.2f)})};
}

/** Lettering stamped into the plate, @p size screen pixels tall. */
Element stamped(const std::string& words, float size) {
  return text(words).styleClass("stamped").font({.size = px(size)});
}

/** The same lettering centred in the rect at (@p x, @p y). */
Element stamped(const std::string& words, float size, float x, float y,
                float width, float height) {
  return screen(x, y, width, height)
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({stamped(words, size)});
}

/** A screw at (@p x, @p y), its slot turned by @p turn degrees. */
Element screw(float x, float y, float turn) {
  return screen(x - 3, y - 3, 6, 6)
      .borderRadius(Corners{px(3)})
      .fill(kScrew)
      .foreground(stroke(px(0.6f), Fill::color(hexColor(0x0C0804, 0.8f)),
                         PathFormat::Align::Outer))
      .children(
          {box()
               .inset(0)
               .shape(shapes::svg("M0.5 2.6 L5.5 2.6 L5.5 3.4 L0.5 3.4 Z"))
               .fill(hexColor(0x100C06, 0.85f))
               .rotate(turn)
               .transformOrigin(pct(50), pct(50))});
}

/** A plate standing proud of the ground, screwed at its corners. */
Element raised(float x, float y, float width, float height,
               bool screwed = true) {
  Element plate = screen(x, y, width, height).styleClass("raised");
  kit::bevelled(
      plate, kit::bevels::plate(hexColor(0xE8D4A0, 0.5f),
                                hexColor(0x080604, 0.75f), px(1.4f), px(1.6f)));
  if (!screwed) return plate;
  return box().inset(0).children({plate, screw(x + 5, y + 5, 20),
                                  screw(x + width - 5, y + 5, 75),
                                  screw(x + 5, y + height - 5, -40),
                                  screw(x + width - 5, y + height - 5, 110)});
}

/** A display's recess in the plate at (@p x, @p y): the dark glass, its
 *  keyline, and the moulded edge of the hole it is sunk in. */
Element recess(float x, float y, float width, float height) {
  Element hole = screen(x, y, width, height)
                     .fill(kScreenGround)
                     .borderRadius(Corners{px(3)})
                     .stroke(stroke(px(1.2f), Fill::color(hexColor(0x0A0804)),
                                    PathFormat::Align::Outer));
  return kit::bevelled(hole, kit::bevels::plate(hexColor(0xE8D4A0, 0.45f),
                                                hexColor(0x000000, 0.8f),
                                                px(1.4f), px(2), true));
}

/** What a display shows at (@p x, @p y): @p content lit on the glass,
 *  glowing, and nothing outside the glass. */
Element readout(float x, float y, float width, float height, Element content) {
  return screen(x, y, width, height)
      .borderRadius(Corners{px(3)})
      .overflow(Overflow::Clip)
      .children({content.filter(kGlow)});
}

/** The front of a display at (@p x, @p y): the raster the tube draws in,
 *  over whatever it shows, and the glass's sheen over that. */
Element glass(float x, float y, float width, float height) {
  return screen(x, y, width, height)
      .borderRadius(Corners{px(3)})
      .fill(kScanLines)
      .hitTestable(false)
      .children({box().inset(0).borderRadius(Corners{px(3)}).fill(kSheen)});
}

/** A whole display: the recess, what it shows, the glass. */
Element display(float x, float y, float width, float height, Element content) {
  return box().inset(0).children(
      {recess(x, y, width, height),
       readout(x, y, width, height, std::move(content)),
       glass(x, y, width, height)});
}

/** One printed row: a label, a value, and the class naming its state. */
struct Line {
  std::string label, value, tone;
};

/** Rows at one @p pitch in a column @p width wide, standing at (@p x, @p y)
 *  inside their display; each value's left edge stands @p valueAt right of
 *  its label's, as the game left-aligns every value column. */
Element rows(float x, float y, float width, float pitch, float valueAt,
             const std::vector<Line>& lines) {
  return screen(x, y, width, pitch * (float)lines.size())
      .column()
      .children({each(lines, [&](const Line& line) {
        return box()
            .row()
            .height(px(pitch))
            .flexShrink(0)
            .alignItems(Align::Center)
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
  std::map<std::string, shapes::Fitted, std::less<>> figures;

  void read(sketch::SketchContext& context) {
    const auto file = [&](const char* name) {
      return context.assets.hub().load<Json>(
          context.local(std::string("data/") + name));
    };
    if (const auto found = file("character.json")) character = *found;
    const auto art = file("art.json");
    if (!art) return;
    static const std::array<material::Color, 2> kKey{
        material::Color{0, 0, 0, 0}, hexColor(0xF4F0E0)};
    for (const auto& [digit, grid] : (*art)["digits"].object()) {
      std::vector<std::string> lines;
      for (const Json& line : grid.array()) lines.emplace_back(line.string());
      if (auto sprite = kit::pixelMap(lines, {".#", kKey}))
        numerals[digit[0]] = *sprite;
    }
    // Each pose is the one body with that pose's arms and prop.
    const Json& figure = (*art)["figure"];
    const std::string body(figure["body"].string());
    for (const auto& [pose, path] : figure.object())
      if (pose != "body")
        figures[pose] =
            shapes::svg(body + " " + std::string(path.string()), true);
  }
};

// ---------------------------------------------------------------------------
// THE CHOREOGRAPHY

constexpr double kLoop = 10.0;
constexpr int kSkillPitch = 11;

/** What the cursor is on: a S.P.E.C.I.A.L. row or a skill. */
struct Selection {
  bool stat = false;
  int index = 0;
  bool operator==(const Selection&) const = default;
};

/** The cursor's visits: where, from when. */
constexpr std::array<std::pair<double, Selection>, 4> kVisits{{
    {0.0, {.stat = true, .index = 0}},
    {1.6, {.index = 0}},
    {3.0, {.index = 7}},
    {4.2, {.index = 4}},
}};
/** The points put into the last skill visited, and CANCEL giving them back. */
constexpr std::array<double, 3> kPresses{4.8, 5.3, 5.8};
constexpr double kCancel = 9.1;

/** 0 before, 1 after @p over seconds, eased between. */
float rise(double since, double over) {
  const float fraction = std::clamp(float(since / over), 0.0f, 1.0f);
  return fraction * fraction * (3 - 2 * fraction);
}

struct Fallout2CharSheet {
  Sheet sheet;

  /** What the sheet shows, which changes only when it is used. */
  struct State {
    Selection selection{.stat = true};
    int presses = 0;
    bool operator==(const State&) const = default;
  } state;

  // What moves between those changes, written each frame.
  motion::Animatable<float> cursorTop = motion::animatable(0.0f);
  motion::Animatable<float> cursorShown = motion::animatable(0.0f);
  motion::Animatable<float> cardShown = motion::animatable(1.0f);
  motion::Animatable<float> keyLit = motion::animatable(0.0f);
  motion::Animatable<float> drumTurn = motion::animatable(0.0f);
  motion::Animatable<float> cancelLit = motion::animatable(0.0f);
  motion::Animatable<float> beamTop = motion::animatable(0.0f);

  // -------------------------------------------------------------------------
  // THE COUNTERS

  /** A column of the ten numerals, 0 at the top, each on a 24-pixel drum
   *  face; standing it at -24 × n shows n through the window. */
  Element numeralStrip() const {
    return box().column().children({each(10, [this](int digit) {
      Element face = box()
                         .width(px(13))
                         .height(px(24))
                         .flexShrink(0)
                         .alignItems(Align::Center)
                         .justifyContent(Justify::Center);
      const auto numeral = sheet.numerals.find(char('0' + digit));
      if (numeral != sheet.numerals.end())
        face.children({kit::pixelSprite(numeral->second, {.cell = kPixel})});
      return face;
    })});
  }

  /** A two-drum counter at (@p x, @p y): white numerals on black drums in
   *  a black window. @p tens and @p ones are where each drum's strip
   *  stands; a drum between two numerals shows both, rolling. */
  template <class Turn>
  Element counter(float x, float y, float tens, Turn ones) const {
    const auto drum = [this](auto turn) {
      return box()
          .width(px(13))
          .height(px(24))
          .fill(hexColor(0x141414))
          .borderRadius(Corners{px(1.5f)})
          .overflow(Overflow::Clip)
          .children({numeralStrip().cache(Cache::Texture).translateY(turn),
                     box().inset(0).fill(kDrumShade)});
    };
    return screen(x - 1, y - 1, 30, 26)
        .fill(hexColor(0x000000))
        .borderRadius(Corners{px(2)})
        .row()
        .padding(px(1))
        .gap(px(1))
        .children({drum(tens), drum(ones)});
  }

  // -------------------------------------------------------------------------
  // THE PLATE: everything that stands still

  /** The ground, its seams, the name plaques and the page's caption. */
  Element chrome() const {
    const auto seam = [](float x, float y, float width, float height) {
      std::vector<Element> parts{raised(x, y, width, height, false)};
      const bool across = width > height;
      const float length = across ? width : height;
      for (float along = 12; along < length - 6; along += 36)
        parts.push_back(across ? screw(x + along, y + height / 2, along * 7)
                               : screw(x + width / 2, y + along, along * 7));
      return box().inset(0).children(parts);
    };
    static constexpr std::array<std::pair<const char*, std::array<float, 2>>, 3>
        kPlaques{{{"name", {12, 140}}, {"age", {155, 82}}, {"sex", {240, 78}}}};
    return box().inset(0).children(
        {seam(326, 0, 8, 480), seam(163, 30, 7, 244), seam(5, 316, 320, 7),
         each(kPlaques, [&](const auto& plaque) {
           return box().inset(0).children(
               {raised(plaque.second[0], 1, plaque.second[1], 26, false),
                stamped(std::string(sheet.character[plaque.first].string()), 25,
                        plaque.second[0], 1, plaque.second[1], 26)});
         })});
  }

  /** The S.P.E.C.I.A.L. column: a raised panel, seven rows at a pitch of
   *  33 — the stamped abbreviation, its counter, and its grade on a small
   *  display. */
  Element special() const {
    std::vector<Element> panel{raised(5, 33, 152, 234)};
    const auto& stats = sheet.character["special"].array();
    for (size_t row = 0; row < stats.size(); ++row) {
      const Json& stat = stats[row];
      const float top = 38 + 33 * (float)row;
      const int value = (int)stat["value"].number();
      panel.push_back(
          screen(stamped(std::string(stat["stat"].string()) + "-", 29), 17,
                 top - 4, 42, 30));
      panel.push_back(counter(58, top, px(-24) * float(value / 10),
                              px(-24) * float(value % 10)));
      panel.push_back(display(
          96, top + 3, 56, 19,
          screen(text(std::string(stat["grade"].string())), 4, 0, 52, 19)
              .alignItems(Align::Center)));
    }
    return box().inset(0).children({panel});
  }

  /** Hit points and the seven conditions, dead until they apply; the ten
   *  derived statistics; level and experience. */
  Element statistics() const {
    std::vector<Line> status{
        {"Hit Points", std::string(sheet.character["hitPoints"].string()), ""}};
    for (const Json& condition : sheet.character["conditions"].array())
      status.push_back({std::string(condition.string()), "", "dead"});
    std::vector<Line> derived;
    for (const Json& pair : sheet.character["derived"].array())
      derived.push_back(
          {std::string(pair[0].string()), std::string(pair[1].string()), ""});
    std::vector<Line> level;
    for (const Json& line : sheet.character["level"].array())
      level.push_back({std::string(line.string()), "", ""});
    return box().inset(0).children(
        {display(178, 34, 142, 122, rows(8, 8, 130, 13, 76, status)),
         display(178, 166, 142, 144, rows(8, 7, 130, 13, 94, derived)),
         display(20, 274, 137, 38, rows(7, 3, 128, 11, 0, level))});
  }

  /** The folder, PERKS tab up: a heading ruled out to both margins, then
   *  its entries, for the perks and then the traits. */
  Element folder() const {
    const auto heading = [](const char* words) {
      return box()
          .row()
          .height(px(12))
          .flexShrink(0)
          .alignItems(Align::Center)
          .gap(px(4))
          .children({box().height(px(1)).flexGrow(1).fill(Fill::currentInk()),
                     text(words),
                     box().height(px(1)).flexGrow(1).fill(Fill::currentInk())});
    };
    const auto entries = [](const Json& list) {
      return each(list.array(), [](const Json& entry) {
        return text(std::string(entry.string())).height(px(12)).flexShrink(0);
      });
    };
    static constexpr std::array<const char*, 3> kTabs{"PERKS", "KARMA",
                                                      "KILLS"};
    static constexpr std::array<float, 4> kTabEdges{22, 108, 204, 298};
    std::vector<Element> tabs;
    for (size_t tab = 0; tab < kTabs.size(); ++tab) {
      const bool up = tab == 0;
      const float left = kTabEdges[tab],
                  width = kTabEdges[tab + 1] - kTabEdges[tab] - 2;
      tabs.push_back(
          box()
              .inset(0)
              .opacity(up ? 1.0f : 0.62f)
              .children(
                  {raised(left, up ? 326 : 330, width, up ? 32 : 28, false),
                   stamped(kTabs[tab], 23, left, up ? 326 : 330, width,
                           up ? 32 : 28)}));
    }
    const auto arrow = [](float y, const char* path) {
      return screen(318, y, 11, 12)
          .styleClass("raised")
          .padding(px(3), px(2))
          .children({box()
                         .flexGrow(1)
                         .fill(hexColor(0xC8A450))
                         .shape(shapes::svg(path))});
    };
    return box().inset(0).children(
        {tabs,
         display(
             20, 358, 296, 114,
             screen(10, 4, 276, 104)
                 .column()
                 .children({heading("PERKS"), entries(sheet.character["perks"]),
                            heading("TRAITS"),
                            entries(sheet.character["traits"])})),
         arrow(360, "M0 1 L0.5 0 L1 1 Z"), arrow(458, "M0 0 L1 0 L0.5 1 Z")});
  }

  /** The skills' fittings: the heading, the SKILL POINTS bar and the empty
   *  card the entry is written on. */
  Element skillFittings() const {
    return box().inset(0).children(
        {stamped("SKILLS", 22, 372, 0, 72, 22), recess(366, 22, 252, 204),
         raised(338, 228, 290, 30),
         stamped("SKILL POINTS", 24, 392, 229, 124, 28),
         screen(345, 266, 279, 172)
             .fill(kParchment)
             .borderRadius(Corners{px(1.5f)})
             .stroke(stroke(px(1.2f), Fill::color(hexColor(0x2A1C08, 0.8f)),
                            PathFormat::Align::Inner))
             .background(Shadow{.ink = Fill::color(hexColor(0x000000, 0.7f)),
                                .offset = {px(1.5f), px(2)},
                                .blur = px(3)})});
  }

  /** PRINT, DONE and CANCEL along the foot, each behind a red lamp. */
  Element buttons() const {
    return box().inset(0).children({each(kButtons, [](const auto& button) {
      return box().inset(0).children(
          {screen(button.second - 2, 453, 16, 16)
               .borderRadius(Corners{px(8)})
               .fill(hexColor(0x16100A))
               .padding(px(2))
               .children({box()
                              .flexGrow(1)
                              .borderRadius(Corners{px(6)})
                              .fill(kLamp)}),
           screen(button.second + 18, 449, 80, 26)
               .children({stamped(button.first, 23)})});
    })});
  }
  static constexpr std::array<std::pair<const char*, float>, 3> kButtons{
      {{"PRINT", 344}, {"DONE", 457}, {"CANCEL", 553}}};

  /** The caption under the screen. */
  Element caption() const {
    return screen(0, 480, 640, 100)
        .fill(hexColor(0x0B0D08))
        .column()
        .padding(20, 28)
        .gap(8)
        .ink(hexColor(0xB5B5A2))
        .children(
            {document::h1("FALLOUT 2 · THE CHARACTER SHEET")
                 .styleClass("card-title")
                 .font(
                     {.size = 21, .color = hexColor(0xC8A450), .track = 1.4f}),
             document::caption("Black Isle Studios, 1998 · 640×480 in indexed "
                               "colour, rebuilt at twice its size")
                 .font({.size = 13,
                        .color = hexColor(0x6E8C60),
                        .condense = 1.0f})});
  }

  /** The whole plate, kept as one image: nothing on it moves. */
  Element plate() const {
    return box()
        .inset(0)
        .children(
            {screen(0, 0, 640, 480)
                 .fill(kSteel)
                 .overflow(Overflow::Clip)
                 .foreground(stroke(px(3), Fill::color(hexColor(0x16120C)),
                                    PathFormat::Align::Inner))
                 .children({chrome(), special(), statistics(), folder(),
                            skillFittings(), buttons()}),
             caption()})
        .cache(Cache::Texture)
        .key("fallout2_charsheet.plate");
  }

  // -------------------------------------------------------------------------
  // WHAT CHANGES WHEN THE SHEET IS USED

  int skillValue(size_t index) const {
    const Json& skill = sheet.character["skills"][index];
    const bool spending =
        !state.selection.stat && (int)index == state.selection.index;
    return (int)skill["value"].number() + (spending ? 2 * state.presses : 0);
  }

  /** The eighteen skills at a pitch of 11: tagged skills pale, the
   *  selected one yellow, the rest green. */
  Element skillList() const {
    std::vector<Line> lines;
    const auto& list = sheet.character["skills"].array();
    for (size_t index = 0; index < list.size(); ++index) {
      const bool selected =
          !state.selection.stat && (int)index == state.selection.index;
      lines.push_back(
          {std::string(list[index]["name"].string()),
           std::to_string(skillValue(index)) + "%",
           selected ? "selected"
                    : (list[index]["tagged"].boolean() ? "tagged" : "")});
    }
    return readout(366, 22, 252, 204, rows(12, 3, 232, kSkillPitch, 190, lines))
        .cache(Cache::Texture)
        .key("fallout2_charsheet.skills");
  }

  /** The cursor over the skill list: a bar of light the height of a row,
   *  and the +/- slider riding beside it. */
  Element cursor() const {
    const auto key = [](const char* sign) {
      return box()
          .width(px(12))
          .height(px(9))
          .styleClass("raised")
          .alignItems(Align::Center)
          .justifyContent(Justify::Center)
          .children({text(sign).font({.size = px(8)}).ink(hexColor(0xE0C070))});
    };
    return box()
        .inset(0)
        .hitTestable(false)
        .opacity(cursorShown)
        .children(
            {screen(368, 25, 248, kSkillPitch)
                 .translateY(cursorTop)
                 .fill(material::linearGradient(
                     {0, 0}, {1, 0},
                     {{0.0f, hexColor(0x80FF40, 0.0f)},
                      {0.08f, hexColor(0x80FF40, 0.16f)},
                      {0.92f, hexColor(0x80FF40, 0.16f)},
                      {1.0f, hexColor(0x80FF40, 0.0f)}})),
             screen(600, 20, 36, 24)
                 .translateY(cursorTop)
                 .children(
                     {box()
                          .inset(0)
                          .styleClass("raised")
                          .row()
                          .padding(px(2))
                          .gap(px(2))
                          .alignItems(Align::Center)
                          .children({box().width(px(16)).height(px(12)).fill(
                                         hexColor(0x061004)),
                                     box().column().gap(px(2)).children(
                                         {key("+"), key("-")})})
                          .cache(Cache::Texture),
                      // The + key lights under the finger.
                      screen(20, 2, 12, 9)
                          .borderRadius(Corners{px(1.5f)})
                          .fill(hexColor(0xFFE070, 0.55f))
                          .opacity(keyLit)})});
  }

  /** The beam's slow roll down the skill display. */
  Element beam() const {
    return screen(366, 22, 252, 204)
        .borderRadius(Corners{px(3)})
        .overflow(Overflow::Clip)
        .hitTestable(false)
        .children({screen(0, -40, 252, 40)
                       .translateY(beamTop)
                       .fill(material::linearGradient(
                           {0, 0}, {0, 1},
                           {{0.0f, hexColor(0x60FF30, 0.0f)},
                            {0.8f, hexColor(0x60FF30, 0.05f)},
                            {1.0f, hexColor(0x60FF30, 0.0f)}}))});
  }

  /** A frame of light around the S.P.E.C.I.A.L. row the cursor is on. */
  Element statCursor() const {
    if (!state.selection.stat) return box();
    const float top = 38 + 33 * (float)state.selection.index;
    return screen(12, top - 3, 142, 30)
        .borderRadius(Corners{px(3)})
        .stroke(stroke(px(1.2f), Fill::color(hexColor(0xFCFC7C, 0.8f))))
        .hitTestable(false)
        .opacity(cardShown);
  }

  /** The card: the entry's name in the slab serif with its formula (or,
   *  for a statistic, its score) on the same baseline, a two-pixel rule,
   *  the description ragged right short of the figure, and the Vault Boy
   *  in the pose the entry asks for. */
  Element card() const {
    const Json& entry =
        state.selection.stat
            ? sheet.character["special"][(size_t)state.selection.index]
            : sheet.character["skills"][(size_t)state.selection.index];
    const std::string title(entry["name"].string());
    const std::string aside =
        state.selection.stat ? std::to_string((int)entry["value"].number()) +
                                   " · " + std::string(entry["grade"].string())
                             : std::string(entry["formula"].string());
    const auto figure = sheet.figures.find(entry["figure"].string("thumbs"));
    Element drawing = screen(176, 36, 96, 128);
    if (figure != sheet.figures.end())
      drawing.shape(figure->second).fill(Fill::none()).stroke([] {
        PathFormat line = stroke(px(1.1f), Fill::color(hexColor(0x1C1206)));
        line.cap = sigil::geometry::path::Cap::Round;
        line.join = sigil::geometry::path::Join::Round;
        return line;
      }());
    return screen(345, 266, 279, 172)
        .styleClass("card")
        .column()
        .padding(px(4), px(6))
        .opacity(cardShown)
        .children(
            {box()
                 .row()
                 .gap(px(8))
                 .alignItems(Align::Baseline)
                 .children({text(title).styleClass("card-title"),
                            text(aside).font({.size = px(8.4f)})}),
             kit::line({.length = Dimension(px(265)), .thickness = px(2)})
                 .marginTop(px(1)),
             text(std::string(entry["blurb"].string()))
                 .width(px(172))
                 .marginTop(px(10))
                 .paragraph({.leading = weave::Leading::absolute(px(11))}),
             drawing})
        .cache(Cache::Texture)
        .key("fallout2_charsheet.card");
  }

  /** The SKILL POINTS drums, the ones drum rolling as points are spent. */
  Element skillPoints() const {
    return box().inset(0).children({screen(520, 229, 34, 28)
                                        .fill(hexColor(0x16100A))
                                        .borderRadius(Corners{px(2)}),
                                    counter(523, 231, 0.0f, drumTurn)});
  }

  /** CANCEL's lamp, lit as it is pressed. */
  Element cancelLamp() const {
    return screen(kButtons[2].second - 10, 445, 32, 32)
        .hitTestable(false)
        .opacity(cancelLit)
        .fill(kLampLight);
  }

  Element describe() const {
    return box()
        .inset(0)
        .applyStyleSheet(registers())
        .children({plate(), statCursor(), skillList(), cursor(), beam(),
                   glass(366, 22, 252, 204)
                       .cache(Cache::Texture)
                       .key("fallout2_charsheet.glass"),
                   card(), skillPoints(), cancelLamp()});
  }

  // -------------------------------------------------------------------------
  // THE MOMENT

  static State at(double time) {
    State now;
    for (const auto& [from, selection] : kVisits)
      if (time >= from) now.selection = selection;
    if (time < kCancel)
      for (double press : kPresses)
        if (time >= press) ++now.presses;
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
    const double time = std::fmod(elapsed, kLoop);

    // The cursor slides from the row it left to the row it is sent to.
    float row = 0, shown = 0, arrived = 0;
    for (size_t visit = 0; visit < kVisits.size(); ++visit) {
      const auto& [from, selection] = kVisits[visit];
      if (time < from) break;
      arrived = (float)from;
      if (selection.stat) {
        shown = 0;
        continue;
      }
      const auto& previous = kVisits[visit == 0 ? 0 : visit - 1].second;
      const float leaving =
          previous.stat ? (float)selection.index : (float)previous.index;
      row = leaving +
            ((float)selection.index - leaving) * rise(time - from, 0.22);
      shown = previous.stat ? rise(time - from, 0.2) : 1.0f;
    }
    cursorTop = px((float)kSkillPitch * row);
    cursorShown = shown;
    cardShown = rise(time - arrived, 0.3);

    // Each press lights the + key and turns the drum on by one numeral;
    // CANCEL turns it back.
    const int points = (int)sheet.character["skillPoints"].number();
    float spent = 0, lit = 0;
    for (double press : kPresses) {
      if (time < press || time >= kCancel) continue;
      spent += rise(time - press, 0.16);
      lit = std::max(lit, 1.0f - rise(time - press, 0.3));
    }
    if (time >= kCancel) spent = 3.0f * (1.0f - rise(time - kCancel, 0.35));
    keyLit = lit;
    drumTurn = px(-24) * ((float)points - spent);
    cancelLit = time < kCancel ? 0.0f : 1.0f - rise(time - kCancel - 0.25, 0.5);

    // The beam takes four seconds to cross the display.
    beamTop = px(244) * float(std::fmod(elapsed, 4.0) / 4.0);

    const State now = at(time);
    if (now == state) return;
    state = now;
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(Fallout2CharSheet, "Study · Game UI",
             "Fallout 2's character screen (1998) at 2×: brushed steel, "
             "phosphor readouts, counter drums and the Vault Boy's card")
