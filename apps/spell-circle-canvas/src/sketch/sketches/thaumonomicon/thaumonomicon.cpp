// Thaumcraft 4's research browser, open on the Alchemy category: a lattice
// of research plates over a nebula, joined by grey wires that turn once and
// end in an arrow at the research they unlock, inside a wooden frame with
// the category tabs down its left side and the vanilla tooltip of the
// hovered research over everything.
//
// The web is data: `data/research.json` holds every research of the
// category — its title, lattice column and row, plate, icon, save state and
// the research it needs — and `data/icons.json` the sixteen-pixel icons as
// character grids. The plates state those facts and the web's operators
// read them: one `connect::ByLane` per kind of wire and a stamp for the
// corner badges.

// TAGS: Geometry/Diagrams, Interfaces/Game

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace data = sigil::data;
namespace field = sigil::material::field;
namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::material::hexColor;
using sigil::material::Paint;

namespace {

// ---------------------------------------------------------------------------
// The screen. Minecraft draws its interface in GUI pixels, two canvas pixels
// each here, so the 1280 x 800 canvas is a 640 x 400 GUI screen. Every
// length below is written in GUI pixels and scaled once by `gui()`.

constexpr float kPixel = 2.0f;
constexpr float gui(float length) { return length * kPixel; }

constexpr SkSize kCanvas = {gui(640), gui(400)};
constexpr float kInset = 16;  // the viewport stands 16 GUI px in from each edge
constexpr float kCell = 24;   // the lattice pitch
constexpr float kPlate = 26;  // a research plate, a little wider than a cell
constexpr float kIcon = 16;

const material::Color kPlateLight = hexColor(0xE6E0E8);
const material::Color kPlateShade = hexColor(0x9C96A2);
const material::Color kPlateRim = hexColor(0x1C1A20);
const material::Color kWood = hexColor(0xA87450);
const material::Color kWoodEdge = hexColor(0x1A100A);

// The tooltip's four colours are Minecraft's formatting codes: §6 gold for
// the title, §c red and §e yellow for what is missing.
const material::Color kTextGold = hexColor(0xFFAA00);
const material::Color kTextRed = hexColor(0xFF5555);
const material::Color kTextYellow = hexColor(0xFFFF55);

/** ONE RESEARCH, as `data/research.json` states it. */
struct Research {
  std::string key, title, plate, icon, state;
  int column = 0, row = 0, warp = 0;
  bool hidden = false, reverse = false;
  std::vector<std::string> parents, siblings, badges;
};

std::vector<std::string> strings(const data::Json& list) {
  std::vector<std::string> out;
  for (const data::Json& item : list.array()) out.emplace_back(item.string());
  return out;
}

std::vector<Research> readResearch(const data::Json& document) {
  std::vector<Research> web;
  for (const data::Json& entry : document["research"].array())
    web.push_back({.key = std::string(entry["key"].string()),
                   .title = std::string(entry["title"].string()),
                   .plate = std::string(entry["plate"].string("square")),
                   .icon = std::string(entry["icon"].string()),
                   .state = std::string(entry["state"].string()),
                   .column = (int)entry["column"].number(),
                   .row = (int)entry["row"].number(),
                   .warp = (int)entry["warp"].number(),
                   .hidden = entry["hidden"].boolean(),
                   .reverse = entry["reverse"].boolean(),
                   .parents = strings(entry["parents"]),
                   .siblings = strings(entry["siblings"]),
                   .badges = strings(entry["badges"])});
  return web;
}

/** `#RRGGBB` or `#RRGGBBAA`. */
material::Color parseHex(std::string_view hex) {
  const uint32_t word = (uint32_t)std::stoul(std::string(hex.substr(1)), nullptr, 16);
  return hex.size() > 7 ? hexColor(word >> 8u, (float)(word & 255u) / 255.0f)
                        : hexColor(word);
}

/** AN ICON FROM ITS CHARACTER GRID. A locked research's icon is drawn in
 *  one dark grey wherever the art has paint, so `greyed` replaces every
 *  entry's colour and keeps its alpha. */
kit::Sprite readIcon(const data::Json& icon, bool greyed) {
  std::string characters = ".";
  std::vector<material::Color> colours = {{0, 0, 0, 0}};
  for (const auto& [character, hex] : icon["palette"].object()) {
    const material::Color colour = parseHex(hex.string());
    characters += character;
    colours.push_back(greyed ? material::Color{0.18f, 0.18f, 0.18f, colour.a}
                             : colour);
  }
  return kit::pixelMap(strings(icon["rows"]), {characters, colours})
      .value_or(kit::Sprite{});
}

/** WHICH WIRES THERE ARE. Thaumcraft multiplies one white line by the grey
 *  of its kind: a prerequisite already researched, one not yet researched,
 *  and a sibling, which is the one kind with a hue and no arrow. A wire
 *  leaves the research that needs it vertically and turns once toward the
 *  one it needs; a REVERSE research's wires turn the other way round. The
 *  lighter kinds are drawn over the darker. */
struct WireKind {
  const char* lane;
  material::Color tint;
  routers::Bend bend;
  bool arrow;
  int depth;
};
const std::array<WireKind, 5> kWires = {{
    {"sibling", {0.30f, 0.30f, 0.40f, 1}, routers::Bend::VFirst, false, 1},
    {"unknown", {0.20f, 0.20f, 0.20f, 1}, routers::Bend::VFirst, true, 2},
    {"unknown-reversed", {0.20f, 0.20f, 0.20f, 1}, routers::Bend::HFirst, true, 2},
    {"known", {0.60f, 0.60f, 0.60f, 1}, routers::Bend::VFirst, true, 3},
    {"known-reversed", {0.60f, 0.60f, 0.60f, 1}, routers::Bend::HFirst, true, 3},
}};

Operator wires(const WireKind& kind) {
  return Operator(connect::ByLane{
                      .lane = kind.lane,
                      .router = routers::orthogonal(kind.bend, gui(kCell / 2)),
                      .gap = gui(kPlate / 2 + 1),
                      .wire = lines::Line{
                          .width = gui(2.5f),
                          .fill = Fill::color(kind.tint),
                          .startMarker = kind.arrow ? lines::Marker::Arrow
                                                    : lines::Marker::None,
                          .markerSize = gui(8)},
                      .bleed = gui(6)})
      .zIndex(kind.depth);
}

// ---------------------------------------------------------------------------
// The nebula behind the viewport: a dark ground, coloured clouds, a veil of
// fractal noise and a field of stars.

struct Cloud {
  float x, y, radius;  // x and y as fractions of the viewport
  uint32_t colour;
  float alpha;
};
constexpr std::array<Cloud, 8> kClouds = {{
    {0.18f, 0.30f, 260, 0x6A1E78, 0.55f},
    {0.46f, 0.62f, 300, 0x8C2A3A, 0.40f},
    {0.74f, 0.28f, 240, 0x2A3C8C, 0.45f},
    {0.88f, 0.74f, 220, 0x5A6A1E, 0.30f},
    {0.30f, 0.86f, 200, 0x1E6A6A, 0.30f},
    {0.60f, 0.18f, 180, 0x7A2A8C, 0.35f},
    {0.08f, 0.70f, 200, 0x3A1E6A, 0.40f},
    {0.96f, 0.10f, 160, 0x8C4A2A, 0.30f},
}};

Element nebula(SkSize size) {
  std::vector<Element> stars;
  for (int index = 0; index < 240; ++index) {
    const auto unit = [index](int axis) {
      return (float)(sigil::core::noise::lattice(7, index, axis, 3) & 0xFFFFu) /
             65535.0f;
    };
    const float magnitude = unit(2);
    stars.push_back(
        kit::disc({unit(0) * size.width(), unit(1) * size.height()},
                  magnitude > 0.9f ? gui(0.9f) : gui(0.5f))
            .shape(shapes::circle())
            .fill(Fill::color(hexColor(0xF2E8FF, 0.25f + 0.7f * magnitude * magnitude))));
  }
  return box()
      .inset(0)
      .fill(Paint::radialGradient(
          {0.5f, 0.5f}, 1.0f,
          {{0.0f, hexColor(0x1A1030)}, {1.0f, hexColor(0x05030A)}}))
      .children(
          {each(kClouds,
                [size](const Cloud& cloud) {
                  return kit::disc(
                             {cloud.x * size.width(), cloud.y * size.height()},
                             cloud.radius)
                      .fill(Paint::radialGradient(
                          {0.5f, 0.5f}, 1.0f,
                          {{0.0f, hexColor(cloud.colour, cloud.alpha)},
                           {1.0f, hexColor(cloud.colour, 0.0f)}},
                          {.extent = material::RadialExtent::ClosestSide}))
                      .blendMode(material::BlendMode::Screen);
                }),
           box()
               .inset(0)
               .opacity(0.30f)
               .blendMode(material::BlendMode::Overlay)
               .fill(field::noise(0.004f, 4, 3.0f)),
           box().inset(0).children(stars)});
}

// ---------------------------------------------------------------------------
// The frame: four bands of dark grained wood, a square post at each corner.

Paint wood(bool vertical) {
  return material::from(field::grain(0.03f, 2, vertical ? 5.0f : 9.0f, 1.3f,
                                   vertical ? 1.0f / 6.0f : 6.0f)).layer(kWood, {.blend = material::BlendMode::Multiply});
}

Element plank(float x, float y, float width, float height) {
  return kit::at(gui(x), gui(y), gui(width), gui(height))
      .fill(wood(height > width))
      .layerStyle(decorations::doubleBorder(
          decorations::border(gui(1), Fill::color(kWoodEdge)),
          decorations::border(gui(1), Fill::color(hexColor(0xE0B080, 0.18f)), gui(2))));
}

Element frame() {
  constexpr float post = 20;
  std::vector<Element> parts = {plank(0, 0, 640, kInset), plank(0, 400 - kInset, 640, kInset),
                                plank(0, 0, kInset, 400), plank(640 - kInset, 0, kInset, 400)};
  for (const SkPoint corner : {SkPoint{-2, -2}, SkPoint{622, -2}, SkPoint{-2, 382},
                               SkPoint{622, 382}})
    parts.push_back(plank(corner.x(), corner.y(), post, post)
                        .fill(hexColor(0x4A3020)));
  return box().inset(0).children(parts);
}

}  // namespace

struct Thaumonomicon {
  sigil::motion::Animatable<float> veil = sigil::motion::animatable(0.0f);  // how far an unlockable plate is dimmed, 0 to 0.5
  std::map<std::string, kit::Sprite> icons, greyedIcons;

  // -------------------------------------------------------------------------
  // A research plate: the silhouette its research names, lit from above,
  // dimmed by its save state, with its icon on it.

  Element plateFace(Element face, const Research& research) const {
    if (research.plate == "round") return face.shape(shapes::circle());
    if (research.plate == "hex") return face.shape(shapes::polygon(6, 90));
    return face.borderRadius({gui(3)});
  }

  Element plate(const Research& research) const {
    const material::Color light =
        research.hidden ? material::scale(kPlateLight, 0.86f) : kPlateLight;
    const float half = gui(kPlate / 2);
    Element node = kit::at(0, 0, gui(kPlate), gui(kPlate)).key(research.key);
    // Warp stains the page under a research with a violet corona.
    if (research.warp > 0)
      node.children(
          {kit::disc({half, half}, gui(30))
               .fill(Paint::radialGradient(
                   {0.5f, 0.5f}, 1.0f,
                   {{0.0f, hexColor(0xB040FF, 0.28f * (float)research.warp)},
                    {1.0f, hexColor(0x40006A, 0.0f)}},
                   {.extent = material::RadialExtent::ClosestSide}))
               .blendMode(material::BlendMode::Screen)});
    if (research.plate == "spiky")
      node.children({kit::disc({half, half}, gui(21))
                         .shape(shapes::star(8, 0.74f, 0.35f))
                         .fill(Fill::color(material::scale(kPlateShade, 0.8f)))});
    node.children(
        {plateFace(box().inset(0), research)
             .fill(Paint::linearGradient(
                 {0.5f, 0}, {0.5f, 1},
                 {{0.0f, light},
                  {1.0f, material::scale(kPlateShade,
                                         research.hidden ? 0.86f : 1.0f)}}))
             .layerStyle(decorations::doubleBorder(
                 decorations::border(gui(1.5f), Fill::color(kPlateRim)),
                 decorations::border(
                     gui(1), Fill::color(hexColor(0xFFFFFF, 0.5f)), gui(2))))});
    Element dim = plateFace(box().inset(0), research).fill(Fill::color({0, 0, 0, 1}));
    if (research.state == "unlockable")
      node.children({dim.opacity(sigil::motion::bind(veil))});
    else if (research.state == "locked")
      node.children({dim.opacity(0.7f)});
    const auto& art = research.state == "locked" ? greyedIcons : icons;
    if (auto found = art.find(research.icon); found != art.end())
      node.children({kit::pixelSprite(found->second, {.cell = kPixel})
                         .left(gui((kPlate - kIcon) / 2))
                         .top(gui((kPlate - kIcon) / 2))});
    return node;
  }

  /** THE CORNER BADGES a research states: a gold star for new research at
   *  the plate's upper left, a page for a new page at its lower left. */
  Operator badges() const {
    return Operator(stamp::ByLane{
        .lane = "badges",
        .key = "badges",
        .make = [this](const Scope::Node& node) {
          Element corner = box().inset(0);
          for (const std::string& badge :
               node.attribute<std::vector<std::string>>("badges").value_or(
                   std::vector<std::string>{})) {
            const auto found = icons.find("badge-" + badge);
            if (found == icons.end()) continue;
            corner.children({kit::pixelSprite(found->second, {.cell = kPixel})
                                 .left(gui(-4))
                                 .top(gui(badge == "page" ? 15 : -4))});
          }
          return corner;
        }});
  }

  // -------------------------------------------------------------------------
  // The tooltip: Minecraft's own, a near-black violet card with a blue
  // inner rule, its lines set in a bitmap face with a one-pixel shadow a
  // quarter as bright. Its text starts three pixels right of and above the
  // cursor, and the card flips to the cursor's left when the screen would
  // cut it.

  Element tooltip(weave::FontContext& fonts, const std::vector<std::string>& lines,
                  SkPoint cursor) const {
    const weave::Type face{.face = weave::ports::face({"Menlo", "Monaco", "Courier New"}),
                           .size = 10,
                           .aliased = true};
    const std::array<material::Color, 3> colours = {kTextGold, kTextRed, kTextYellow};
    std::vector<Element> set;
    float widest = 0;
    for (size_t index = 0; index < lines.size(); ++index) {
      const std::u8string run(lines[index].begin(), lines[index].end());
      const kit::Coverage coverage = kit::coverage(run, fonts, face);
      const kit::Mask mask = kit::threshold(coverage);
      widest = std::max(widest, coverage.advance.width());
      // The title stands two pixels clear of the lines under it.
      const float top = 4 + 10 * (float)index + (index > 0 ? 2 : 0);
      set.push_back(kit::masked(mask, {.colour = colours[std::min<size_t>(index, 2)],
                                       .scale = kPixel,
                                       .shadowOffset = {gui(1), gui(1)}})
                        .left(gui(4 + (float)(mask.inkX - coverage.pad.x)))
                        .top(gui(top + (float)(mask.inkY - coverage.pad.y))));
    }
    const float width = widest + 8, height = 10 * (float)lines.size() + 9;
    const float left = cursor.x() + 3 + widest + 4 <= 640 ? cursor.x() - 1
                                                           : cursor.x() - 12 - widest - 4;
    return kit::at(gui(left), gui(cursor.y() - 7), gui(width), gui(height))
        .fill(Fill::color(hexColor(0x100010, 0.94f)))
        .foreground(decorations::border(gui(1), Fill::color(hexColor(0x5000FF, 0.31f)), gui(1)))
        .children(set);
  }

  // -------------------------------------------------------------------------
  // The category tabs down the left edge, each a rune cut in its aspect's
  // colour on a wooden boss, and the search glass on the lower left post.

  Element tabs() const {
    static constexpr std::array<const char*, 7> kCategories = {
        "fundamentals", "auromancy", "alchemy", "artifice",
        "infusion", "golemancy", "eldritch"};
    Element rail = box().inset(0);
    for (size_t index = 0; index < kCategories.size(); ++index) {
      const bool open = std::string_view(kCategories[index]) == "alchemy";
      const auto found = icons.find(std::string("tab-") + kCategories[index]);
      if (found == icons.end()) continue;
      rail.children({kit::at(gui(open ? 3 : 1), gui(28 + 26 * (float)index), gui(22), gui(22))
                         .borderRadius({gui(3)})
                         .fill(wood(false))
                         .opacity(open ? 1.0f : 0.8f)
                         .layerStyle(decorations::doubleBorder(
                             decorations::border(gui(1), Fill::color(kWoodEdge)),
                             decorations::border(gui(1),
                                                 Fill::color(open ? hexColor(0x9FFFFF, 0.8f)
                                                                  : hexColor(0xE0B080, 0.25f)),
                                                 gui(1))))
                         .children({kit::pixelSprite(found->second, {.cell = kPixel})
                                        .left(gui(3))
                                        .top(gui(3))})});
    }
    if (const auto search = icons.find("search"); search != icons.end())
      rail.children({kit::pixelSprite(search->second, {.cell = kPixel})
                         .left(gui(1))
                         .top(gui(383))});
    return rail;
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kCanvas,
                             .captureAt = 0.3,
                             .background = hexColor(0x0B0806),
                             .oversample = 2});

    const auto icon_document = ctx.assets.json(ctx.local("data/icons.json"));
    for (const auto& [name, icon] : icon_document->object()) {
      icons[name] = readIcon(icon, false);
      greyedIcons[name] = readIcon(icon, true);
    }
    const auto document = ctx.assets.json(ctx.local("data/research.json"));
    const std::vector<Research> web = readResearch(*document);
    const std::string hovered((*document)["hovered"].string());

    // The view is centred on the middle of the web's own bounds.
    int left = 0, right = 0, top = 0, bottom = 0;
    for (const Research& research : web) {
      left = std::min(left, research.column), right = std::max(right, research.column);
      top = std::min(top, research.row), bottom = std::max(bottom, research.row);
    }
    const auto centreOf = [&](const Research& research) {
      return SkPoint{gui(320 + ((float)research.column - (float)(left + right) / 2) * kCell),
                     gui(200 + ((float)research.row - (float)(top + bottom) / 2) * kCell)};
    };
    const auto find = [&](std::string_view key) -> const Research* {
      for (const Research& research : web)
        if (research.key == key) return &research;
      return nullptr;
    };

    // Thaumcraft's pulse: every unlockable plate brightens and dims together
    // on a 600 ms sine between half and full brightness.
    ctx.engine.add([this, &ticker = ctx.engine] {
      veil = (float)(0.25 - 0.25 * std::sin(std::fmod(ticker.elapsed(), 0.6) / 0.6 * 6.2831853));
    });

    // Each plate states the research it needs under the lane of its wire:
    // known or unknown by whether that research is done, reversed when the
    // plate says so. A prerequisite that lists this research as a sibling
    // draws the sibling wire in its place.
    std::vector<Element> plates;
    Element hover = box();
    for (const Research& research : web) {
      std::map<std::string, std::vector<std::string>> lanes;
      for (const std::string& key : research.parents) {
        const Research* parent = find(key);
        if (!parent || std::ranges::find(parent->siblings, research.key) != parent->siblings.end()) continue;
        lanes[std::string(parent->state == "complete" ? "known" : "unknown") +
              (research.reverse ? "-reversed" : "")]
            .push_back(key);
      }
      if (!research.siblings.empty()) lanes["sibling"] = research.siblings;
      const SkPoint centre = centreOf(research);
      Element node = plate(research).centerAt(centre).zIndex(10);
      for (const auto& [lane, keys] : lanes) node.attribute(lane, keys);
      if (!research.badges.empty()) node.attribute("badges", research.badges);
      // The hovered research's tooltip names it and what it still needs.
      if (research.key == hovered && ctx.fonts) {
        std::vector<std::string> lines = {research.title, "Missing required research:"};
        for (const std::string& key : research.parents)
          if (const Research* parent = find(key); parent && parent->state != "complete")
            lines.push_back(" - " + parent->title);
        // The cursor rests two pixels right of and four below the centre.
        hover = tooltip(*ctx.fonts, lines,
                        {centre.x() / kPixel + 2, centre.y() / kPixel + 4});
      }
      plates.push_back(node);
    }

    std::vector<Operator> operators;
    for (const WireKind& kind : kWires) operators.push_back(wires(kind));
    operators.push_back(badges());

    const SkSize viewport = {gui(640 - 2 * kInset), gui(400 - 2 * kInset)};
    ctx.composer.render(box().inset(0).children({
        kit::at(gui(kInset), gui(kInset), viewport.width(), viewport.height())
            .overflow(Overflow::Clip)
            .cache(Cache::Texture)
            .children({nebula(viewport)}),
        box().inset(0).operators(std::move(operators)).children(plates),
        frame(),
        tabs(),
        hover,
    }));
  }
};

SIGIL_SKETCH(Thaumonomicon, "Study · Game UI",
             "Thaumcraft 4's research browser, Alchemy open — a web of "
             "research plates read from data, wired by what each one needs")
