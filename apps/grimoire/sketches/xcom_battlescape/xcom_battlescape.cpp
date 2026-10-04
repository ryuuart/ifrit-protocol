// X-COM: UFO Defense (1994), the battlescape at dusk: a forest crash site in
// isometric tiles, three units, a movement preview counting down time units,
// and the control panel under it. Everything is a 320 x 200 VGA screen shown
// at four canvas pixels per screen pixel, coloured only through the game's
// 256-entry palette.
//
// The pictures are data. `data/palette.json` is the palette as sixteen ramps
// of sixteen; `data/sprites.json` holds every drawing as a character grid
// whose key names palette entries by `block:step`; `data/font.json` holds the
// 3 x 5 numerals and the small face; `data/scene.json` is the map, the units
// and the walked route.
//
// TAGS: Interfaces/Game

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigildata/decode/Json.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
using sigil::data::Json;
using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

/** One screen pixel, in canvas pixels. Every coordinate below is the game's
 *  own, in its 320 x 200 screen, and is multiplied here and nowhere else. */
constexpr float kPixel = 4.0f;

Element screen(float x, float y, float width, float height) {
  return kit::at(x * kPixel, y * kPixel, width * kPixel, height * kPixel);
}
Element screen(Element node, float x, float y, float width, float height) {
  return kit::at(std::move(node), x * kPixel, y * kPixel, width * kPixel,
                 height * kPixel);
}

/** A palette entry by its ramp and its step down that ramp. The interface
 *  addresses colour this way throughout: sixteen hues, each sixteen steps
 *  from light to dark. */
constexpr int block(int ramp, int step) { return ramp * 16 + step; }

/** The game's shading: add to the step, stay inside the ramp, and fall to
 *  absolute black when the step runs off its end. When @p ramp is given the
 *  drawing's own ramp is replaced by it, which is how one arrow is painted
 *  green, yellow or red. */
constexpr int shaded(int index, int shade, int ramp = -1) {
  const int step = index % 16 + shade;
  if (step > 15) return 15;
  return (ramp < 0 ? index - index % 16 : ramp * 16) + step;
}

/** Screen pixels from the map origin to the top-left of a tile's 32 x 40
 *  sprite cell. North points to the upper right: one step along map x moves
 *  (+16, +8), one along map y moves (-16, +8), one level up moves 24 up. */
constexpr float kOriginX = 144, kOriginY = -92;
struct Tile {
  int x = 0, y = 0;
};
float tileLeft(Tile tile) { return kOriginX + float(tile.x - tile.y) * 16; }
float tileTop(Tile tile, int level = 0) {
  return kOriginY + float(tile.x + tile.y) * 8 - float(level) * 24;
}

/** Everything read from `data/`. */
struct Artefact {
  std::array<material::Color, 256> palette{};
  /** A drawing's grid, the palette entry each of its characters names, and
   *  where its top-left sits inside the sprite cell. */
  struct Drawing {
    std::vector<std::string> rows;
    std::map<char, int> key;
    float left = 0, top = 0;
  };
  std::map<std::string, Drawing, std::less<>> drawings;
  std::map<std::string, std::map<char, std::vector<std::string>>, std::less<>>
      fonts;
  std::vector<std::string> floors, objects;
  std::vector<Tile> soldiers, path;
  std::vector<int> pathTimeUnits;
  Tile alien;
  int globalShade = 0;

  material::Color colour(int index) const {
    return index == 0 ? material::Color{0, 0, 0, 0} : palette[index & 255];
  }

  static int entry(std::string_view blockStep) {
    int ramp = 0, step = 0;
    const size_t colon = blockStep.find(':');
    std::from_chars(blockStep.data(), blockStep.data() + colon, ramp);
    std::from_chars(blockStep.data() + colon + 1,
                    blockStep.data() + blockStep.size(), step);
    return block(ramp, step);
  }

  void read(sketch::SketchContext& context) {
    const auto file = [&](const char* name) {
      return context.assets.hub().load<sigil::data::Json>(
          context.local(std::string("data/") + name));
    };
    if (const auto ramps = file("palette.json"))
      for (size_t ramp = 0; ramp < ramps->size(); ++ramp)
        for (size_t step = 0; step < (*ramps)[ramp].size(); ++step) {
          uint32_t hex = 0;
          const std::string_view text = (*ramps)[ramp][step].string();
          std::from_chars(text.data() + 1, text.data() + text.size(), hex, 16);
          palette[ramp * 16 + step] = hexColor(hex);
        }
    if (const auto sprites = file("sprites.json"))
      for (const auto& [name, drawing] : sprites->object()) {
        Drawing& into = drawings[name];
        into.left = (float)drawing["origin"][0].number();
        into.top = (float)drawing["origin"][1].number();
        for (const auto& [character, blockStep] : drawing["key"].object())
          into.key[character[0]] = entry(blockStep.string());
        for (const Json& row : drawing["rows"].array())
          into.rows.emplace_back(row.string());
      }
    if (const auto faces = file("font.json"))
      for (const auto& [face, glyphs] : faces->object())
        for (const auto& [character, rows] : glyphs.object())
          for (const Json& row : rows.array())
            fonts[face][character[0]].emplace_back(row.string());
    if (const auto scene = file("scene.json")) {
      const auto tile = [](const Json& pair) {
        return Tile{(int)pair[0].number(), (int)pair[1].number()};
      };
      globalShade = (int)(*scene)["globalShade"].number();
      for (const Json& row : (*scene)["floor"].array())
        floors.emplace_back(row.string());
      for (const Json& row : (*scene)["objects"].array())
        objects.emplace_back(row.string());
      for (const Json& soldier : (*scene)["soldiers"].array())
        soldiers.push_back(tile(soldier));
      alien = tile((*scene)["alien"]);
      for (const Json& step : (*scene)["path"].array()) {
        path.push_back(tile(step));
        pathTimeUnits.push_back((int)step[2].number());
      }
    }
  }

  /** @p name coloured through the palette at @p shade, with its ramp
   *  replaced by @p ramp when one is given. */
  kit::Sprite sprite(std::string_view name, int shade = 0,
                     int ramp = -1) const {
    kit::Sprite out;
    const auto found = drawings.find(name);
    if (found == drawings.end()) return out;
    const Drawing& drawing = found->second;
    out.grid = {(int)drawing.rows.front().size(), (int)drawing.rows.size()};
    for (size_t row = 0; row < drawing.rows.size(); ++row)
      for (size_t column = 0; column < drawing.rows[row].size(); ++column) {
        const auto named = drawing.key.find(drawing.rows[row][column]);
        if (named != drawing.key.end())
          out.px((float)column, (float)row,
                 colour(shaded(named->second, shade, ramp)));
      }
    return out;
  }

  /** @p run set in the bitmap face @p face, one pixel between glyphs, in
   *  the entry @p entry. With an @p outline entry every unlit pixel touching
   *  a lit one, diagonals included, is drawn in it: the ring that keeps a
   *  name legible on any ground. */
  kit::Sprite lettering(std::string_view run, std::string_view face, int entry,
                        int outline = -1) const {
    const auto& glyphs = fonts.find(face)->second;
    const int margin = outline < 0 ? 0 : 1;
    std::vector<std::pair<int, int>> lit;
    int pen = margin, height = 0;
    for (char character : run) {
      const auto glyph = glyphs.find(character);
      if (glyph == glyphs.end()) continue;
      const std::vector<std::string>& rows = glyph->second;
      height = std::max(height, (int)rows.size());
      for (size_t row = 0; row < rows.size(); ++row)
        for (size_t column = 0; column < rows[row].size(); ++column)
          if (rows[row][column] == '#')
            lit.emplace_back(pen + (int)column, margin + (int)row);
      pen += (int)rows.front().size() + 1;
    }
    kit::Sprite out;
    out.grid = {pen - 1 + margin, height + 2 * margin};
    if (outline >= 0)
      for (const auto& [x, y] : lit)
        out.rect(float(x - 1), float(y - 1), 3, 3, colour(outline));
    for (const auto& [x, y] : lit) out.px((float)x, (float)y, colour(entry));
    return out;
  }
};

/** A drawing placed with its sprite cell's top-left at (@p x, @p y). */
Element stamp(const Artefact& art, const kit::Sprite& sprite,
              std::string_view name, float x, float y) {
  const Artefact::Drawing& drawing = art.drawings.find(name)->second;
  return screen(kit::pixelSprite(sprite, {.cell = kPixel}), x + drawing.left,
                y + drawing.top, (float)sprite.grid.x, (float)sprite.grid.y);
}

/** A figure in the 3 x 5 numerals, every lit pixel in one entry. */
Element numeral(const Artefact& art, int value, float x, float y, int entry) {
  const kit::Sprite digits =
      art.lettering(std::to_string(value), "digits", entry);
  return screen(kit::pixelSprite(digits, {.cell = kPixel}), x, y,
                (float)digits.grid.x, (float)digits.grid.y);
}

struct XcomBattlescape {
  Artefact art;
  kit::SpriteSheet sheet;
  /** The ground, in the order it is drawn: floors and objects row by row
   *  at level 0, then the raised deck at level 1. Later stamps overlap
   *  earlier ones, which is the whole depth sort. */
  struct Ground {
    std::string cell;
    float x, y;
  };
  std::vector<Ground> ground;
  int tick = -1;
  sigil::motion::Animatable<float> markerOffset =
      sigil::motion::animatable(0.0f);
  sigil::motion::Animatable<Fill> alarm =
      sigil::motion::animatable<Fill>(Fill::none());

  /** Darkness at a tile: the dusk sky lights everything to 15 minus the
   *  global shade, and each soldier carries a light of 15 that loses one
   *  step per tile of rounded distance. The brightest source wins. */
  int shadeAt(Tile tile) const {
    int light = 15 - art.globalShade;
    for (Tile soldier : art.soldiers) {
      const float across = float(tile.x - soldier.x),
                  along = float(tile.y - soldier.y);
      light = std::max(light, 15 - (int)std::lround(std::sqrt(across * across +
                                                              along * along)));
    }
    return std::max(0, 15 - light);
  }

  /** Each drawing at each shade the map asks for, packed onto one sheet
   *  once; the ground is then placements of sheet cells. */
  void layGround() {
    const auto place = [&](const std::string& name, Tile tile, int level) {
      const int shade = shadeAt(tile);
      const std::string cell = name + "@" + std::to_string(shade);
      if (!sheet.find(cell)) sheet.add(cell, art.sprite(name, shade));
      const Artefact::Drawing& drawing = art.drawings.find(name)->second;
      const float x = tileLeft(tile) + drawing.left,
                  y = tileTop(tile, level) + drawing.top;
      if (x < 320 && x + 32 > 0 && y < 144 && y + 40 > 0)
        ground.push_back({cell, x, y});
    };
    const std::map<char, std::string> floor{
        {'g', "grass"}, {'d', "dirt"}, {'h', "hull"}};
    const std::map<char, std::string> object{
        {'b', "bush"}, {'T', "tree"}, {'W', "hull-wall"}};
    const int size = (int)art.floors.size();
    for (int x = 0; x < size; ++x)
      for (int y = 0; y < size; ++y) {
        const char kind = art.floors[y][x];
        if (kind == ' ') continue;  // undiscovered ground is not drawn
        // Two dither variants alternate so the pattern never lines up
        // across the lattice.
        place(floor.at(kind) + "-" + std::to_string((x + y) % 2), {x, y}, 0);
        if (object.contains(art.objects[y][x]))
          place(object.at(art.objects[y][x]), {x, y}, 0);
      }
    for (int x = 0; x < size; ++x)
      for (int y = 0; y < size; ++y)
        if (art.floors[y][x] == 'h') place("hull-deck", {x, y}, 1);
    sheet.bake({.cell = kPixel});
  }

  /** The route's marker ramps, by what the walk would leave: green while a
   *  snap shot is still affordable, yellow once it is not, red past zero. */
  static constexpr int kReserve = 15;
  static int markerRamp(int timeUnitsLeft) {
    return timeUnitsLeft < 0 ? 2 : (timeUnitsLeft >= kReserve ? 3 : 9);
  }

  Element battlefield() const {
    std::vector<Element> units;
    for (Tile soldier : art.soldiers)
      units.push_back(stamp(art, art.sprite("soldier", shadeAt(soldier)),
                            "soldier", tileLeft(soldier), tileTop(soldier)));
    units.push_back(stamp(art, art.sprite("alien", shadeAt(art.alien)), "alien",
                          tileLeft(art.alien), tileTop(art.alien)));

    const Tile selected = art.soldiers.front();
    units.push_back(selectedMarker());

    std::vector<Element> route;
    for (size_t step = 0; step < art.path.size(); ++step) {
      const Tile here = art.path[step];
      const Tile before = step ? art.path[step - 1] : selected;
      const int left = art.pathTimeUnits[step];
      const char* arrow =
          here.x != before.x ? "arrow-down-right" : "arrow-down-left";
      route.push_back(stamp(art, art.sprite(arrow, 0, markerRamp(left)), arrow,
                            tileLeft(here), tileTop(here)));
      route.push_back(numeral(art, std::max(0, left),
                              tileLeft(here) + 16 - (left > 9 ? 5 : 3),
                              tileTop(here) + 29, block(markerRamp(left), 1)));
    }
    const Tile end = art.path.back();
    route.push_back(stamp(art, art.sprite("cursor"), "cursor", tileLeft(end),
                          tileTop(end)));

    return box().inset(0).children(
        {each(ground,
              [this](const Ground& piece) {
                const auto window = sheet.rect(piece.cell);
                return screen(sheet.cell(piece.cell), piece.x, piece.y,
                              window.width() / kPixel,
                              window.height() / kPixel);
              }),
         units, route});
  }

  /** The selected soldier's marker bobs through three whole-pixel heights. */
  Element selectedMarker() const {
    const Tile selected = art.soldiers.front();
    return stamp(art, art.sprite("selected"), "selected", tileLeft(selected),
                 tileTop(selected) - 4)
        .translateY(markerOffset);
  }

  /** A gauge as the game draws it: an outline one longer than the maximum,
   *  its middle row cleared so the lattice shows through, and the value
   *  filled along that row one pixel per point. */
  Element gauge(float y, int value, int maximum, int entry) const {
    const float length = float(maximum + 1);
    const material::Color outline = art.colour(entry + 4);
    return box().inset(0).children({
        screen(170, y, length, 1).fill(outline),
        screen(170, y + 2, length, 1).fill(outline),
        screen(170 + (float)maximum, y + 1, 1, 1).fill(outline),
        screen(170, y + 1, (float)value, 1).fill(art.colour(entry)),
    });
  }

  /** A readout recess: seven rows stepping one entry darker each. */
  Element recess(float x, float y, int firstEntry) const {
    return box().inset(0).children({each(7, [&](size_t row) {
      return screen(x, y + (float)row, 17, 1)
          .fill(art.colour(firstEntry + (int)row));
    })});
  }

  /** A stone pillar 48 wide holding a hand slot: the stone lightens in
   *  steps toward the slot, whose 2-pixel lavender frame is shadowed on the
   *  top and left and lit on the bottom and right, so it reads as sunk. */
  Element pillar(float x) const {
    return box().inset(0).children({
        screen(x, 144, 48, 56).fill(art.colour(block(5, 8))),
        screen(x + 2, 144, 44, 56).fill(art.colour(block(5, 5))),
        screen(x + 4, 145, 40, 54).fill(art.colour(block(5, 3))),
        screen(x + 6, 146, 36, 52).fill(art.colour(block(14, 6))),
        screen(x + 6, 146, 34, 50).fill(art.colour(block(14, 11))),
        screen(x + 8, 148, 32, 48).fill(art.colour(block(0, 15))),
    });
  }

  /** A reserve switch: a lit rim on the top and left, a shadow on the
   *  bottom and right, and its glyph pressed into the face. */
  Element reserveSwitch(float x, float y, int ramp,
                        std::string_view glyph) const {
    return box().inset(0).children({
        screen(x, y, 28, 11).fill(art.colour(block(ramp, 3))),
        screen(x + 1, y + 1, 27, 10).fill(art.colour(block(ramp, 8))),
        screen(x + 1, y + 1, 26, 9).fill(art.colour(block(ramp, 5))),
        stamp(art, art.sprite(glyph), glyph, x, y),
    });
  }

  Element panel() const {
    // The fourteen buttons read in columns: unit up and down, level up and
    // down, map and kneel, inventory and centre, next unit and next stop,
    // layers and options, end turn and abort.
    static constexpr std::array<const char*, 14> kButtons{
        "unit-up", "unit-down", "map-up",   "map-down",  "show-map",
        "kneel",   "inventory", "centre",   "next-unit", "next-stop",
        "layers",  "options",   "end-turn", "abort"};
    // Each gauge's readout: position, value, maximum, and its declared
    // entry; a readout lights one entry past the one it declares.
    struct Stat {
      float x, y, gaugeY;
      int value, maximum, entry;
    };
    static constexpr std::array<Stat, 4> kStats{{
        {136, 186, 185, 58, 60, 64},     // time units
        {154, 186, 189, 56, 65, 16},     // energy
        {136, 194, 193, 36, 36, 32},     // health
        {154, 194, 197, 100, 100, 192},  // morale
    }};

    const kit::Sprite plate = art.sprite("plate");
    const kit::Sprite name =
        art.lettering("Anders Holmgren", "small", block(8, 2), block(8, 11));

    return box().inset(0).children({
        // The button bank stands on dark stone; the soldier's half is black.
        screen(48, 144, 224, 32).fill(art.colour(block(5, 13))),
        screen(48, 176, 224, 24).fill(art.colour(block(0, 15))),
        pillar(0),
        pillar(272),
        each(kButtons,
             [&](const char* button, size_t index) {
               const std::string glyph = std::string("button-") + button;
               return screen(stack().children(
                                 {kit::pixelSprite(plate, {.cell = kPixel}),
                                  stamp(art, art.sprite(glyph), glyph, 0, 0)}),
                             48 + float(index / 2) * 32,
                             144 + float(index % 2) * 16, 32, 16);
             }),
        // Time-unit reserve: none is chosen and lit green; snap, aimed and
        // automatic fire wait in red.
        reserveSwitch(49, 177, 4, "reserve-none"),
        reserveSwitch(78, 177, 2, "reserve-snap"),
        reserveSwitch(49, 189, 2, "reserve-aimed"),
        reserveSwitch(78, 189, 2, "reserve-auto"),
        stamp(art, art.sprite("rank-squaddie"), "rank-squaddie", 107, 177),
        screen(kit::pixelSprite(name, {.cell = kPixel}), 134, 176,
               (float)name.grid.x, (float)name.grid.y),
        recess(134, 185, block(3, 7)),
        recess(152, 185, block(1, 5)),
        recess(134, 193, block(2, 5)),
        recess(152, 193, block(12, 5)),
        // The lattice behind the gauges: a line on every odd row and every
        // fifth column.
        each(20,
             [&](size_t column) {
               return screen(176 + float(column) * 5, 185, 1, 15)
                   .fill(art.colour(block(8, 10)));
             }),
        each(8,
             [&](size_t row) {
               return screen(170, 185 + float(row) * 2, 102, 1)
                   .fill(art.colour(block(8, 10)));
             }),
        each(kStats,
             [&](const Stat& stat) {
               return box().inset(0).children(
                   {gauge(stat.gaugeY, stat.value, stat.maximum, stat.entry),
                    numeral(art, stat.value, stat.x, stat.y, stat.entry + 1)});
             }),
        stamp(art, art.sprite("rifle"), "rifle", 280, 148),
        numeral(art, 14, 280, 148, 3),  // rounds left in the clip
        numeral(art, 1, 232, 150, 15),  // the level being shown
    });
  }

  /** The spotted-alien buttons at the right edge, stacked upward; their
   *  red walks up its ramp one step a tick and back two at a time. */
  Element spotted() const {
    return box().inset(0).children({each(3, [&](size_t index) {
      const float top = 128 - 13 * (float)index;
      return box().inset(0).children(
          {screen(300, top, 15, 12).fill(art.colour(block(0, 15))),
           screen(301, top + 1, 13, 10).fill(alarm),
           numeral(art, (int)index + 1, 306, top + 4, 17)});
    })});
  }

  Element describe() const {
    return box().inset(0).children({battlefield(), panel(), spotted()});
  }

  void refresh() {
    static constexpr std::array<int, 8> kBob{0, 1, 2, 1, 0, 1, 2, 1};
    markerOffset = (float)kBob[tick % kBob.size()] * kPixel;
    const int phase = tick % 18;
    const int entry = phase <= 12 ? 32 + phase : 44 - 2 * (phase - 12);
    alarm = Fill::color(art.colour(entry));
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(320 * kPixel, 200 * kPixel);
    context.background(hexColor(0x000000));
    // One screen pixel is four canvas pixels, so eight device pixels in
    // every row and column: a whole-number downsample lays the plate over
    // a capture of the game.
    context.oversample(2);
    context.captureAt(4.0);
    art.read(context);
    layGround();
    tick = 0;
    refresh();
    context.composer.render(describe());
  }

  /** The screen's clock ticks ten times a second and nothing on it moves
   *  between ticks. */
  void update(double elapsed, sketch::SketchContext&) {
    const int now = (int)(elapsed * 10);
    if (now == tick) return;
    tick = now;
    refresh();
  }
};

}  // namespace

SIGIL_SKETCH(XcomBattlescape, "Study · Game UI",
             "X-COM: UFO Defense (1994) battlescape at 4× — every colour "
             "from the game's palette")
