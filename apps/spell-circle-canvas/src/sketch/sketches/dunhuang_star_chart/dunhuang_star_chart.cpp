// The Dunhuang star chart, British Library Or.8210/S.3326: the star atlas
// that closes a Tang paper scroll, 2,100 mm long and 244 mm high, read from
// right to left. Twelve hour-angle maps of about 30° of right ascension
// each, every one with its calendar text written in a column on its left,
// then the circumpolar disc, which has none. The stars are dots of one size,
// most of them ringed in black, filled in the colour of the school that
// catalogued them — red for Shi Shen, black for Gan De, white for Wu Xian —
// and joined by thin black lines into asterisms, each named beside itself.
// There is no grid, no equator and no frame round a map.
//
// The plate lays the atlas out as the scroll is usually shown, in two
// lengths one above the other, maps 1 to 7 on the first and maps 8 to 12
// with the disc on the second, and draws it from the sky rather than from a
// tracing:
//
//   data/stars.csv      1,460 stars of Chen Zhuo's catalogue, right
//                       ascension and declination on the equinox of +700,
//                       the epoch the chart was drawn for;
//   data/asterisms.csv  the asterisms: name, the school the chart colours it
//                       in where the published concordances of maps 5 and
//                       13 give one, and its line art as runs of star
//                       numbers, a bar between one run and the next;
//   data/maps.csv       each hour-angle map's centre and the station of
//                       Jupiter its calendar text opens with.
//
// A star whose school the concordances do not give is drawn as a ringed dot
// of a faded ochre, because no colour for it has been read off the scroll.

// TAGS: Data/Astronomy

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/table/Table.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using material::skia::Paint;

namespace {

// ---------------------------------------------------------------------------
// The look: mulberry paper gone to a warm tan, a carbon ink browned at its
// edges, cinnabar, and a lead white oxidised warm.

const material::Color kGround = hexColor(0x16130f);
const material::Color kPaper = hexColor(0xd9c197);
const material::Color kLining = hexColor(0x8f6a3e);
const material::Color kInk = hexColor(0x231b13);
const material::Color kCinnabar = hexColor(0xb23a26);
const material::Color kLeadWhite = hexColor(0xf2e8d0);
const material::Color kUnread = hexColor(0x9a7a48);
const material::Color kCaption = hexColor(0xb8a47e);

// ---------------------------------------------------------------------------
// The scroll's own measure. Lengths are millimetres of paper until
// `kPixelsPerMm` turns them into the plate's pixels.

constexpr float kWidth = 2560, kHeight = 1600;
constexpr float kPixelsPerMm = 2.2f;
constexpr float kDegree = 3.14159265358979f / 180.0f;

constexpr float kScrollHeight = 244.0f;
constexpr float kMiddle = kScrollHeight / 2;
/** One map and its column band: twelve of them and the disc fill the
 *  atlas's 2,100 mm. */
constexpr float kSlot = 158.0f;
/** The map's own width, 48° of right ascension at the chart's 4.56° per
 *  centimetre; the columns stand in what is left of the slot. */
constexpr float kMapWidth = 105.3f;
constexpr float kRightAscensionPerMm = 0.456f;
constexpr float kDeclinationPerMm = 0.528f;
/** The disc: azimuthal, 5.10° of polar distance per centimetre, turned a
 *  little faster than the sky, and centred off the pole. */
constexpr float kDiscCentre = 12 * kSlot + 102.0f;
constexpr float kPolarDistancePerMm = 0.510f;
constexpr float kAzimuthGain = 1.05f;
constexpr float kDiscPole = 87.6f;

/** The two lengths the atlas is shown in: where each starts along the
 *  scroll, where it stands on the plate, and how far left its paper runs. */
struct Length {
  float from;
  float top;
  float left;
};
constexpr float kRightEdge = 2496.0f;
const Length kLengths[2] = {{0.0f, 206.0f, 0.0f},
                            {7 * kSlot, 858.0f, 250.0f}};

float wrap180(float degrees) {
  return degrees - 360.0f * std::floor((degrees + 180.0f) / 360.0f);
}

// ---------------------------------------------------------------------------
// The data.

struct Map {
  int number;
  float rightAscension;
  float declination;
  std::string column;
};

struct Asterism {
  std::string name;
  char school;
  std::vector<std::vector<int>> runs;
  /** The map it is drawn on; none is the disc. */
  std::optional<Map> map;
};

/** WHERE A STAR LANDS ON THE SCROLL, in millimetres: along from the atlas's
 *  right edge, down from the paper's top. A map is a plain cylinder, right
 *  ascension growing to the LEFT because the scroll reads that way; the
 *  disc is a planisphere about its own centre, the angle running clockwise
 *  on the page so the sky still grows leftward along its lower edge. */
SkPoint onScroll(const std::optional<Map>& map, float rightAscension,
                 float declination) {
  if (!map) {
    const float radius = (kDiscPole - declination) / kPolarDistancePerMm;
    const float angle = kAzimuthGain * (rightAscension - 278.0f) * kDegree;
    return {kDiscCentre - radius * std::cos(angle),
            kMiddle + radius * std::sin(angle)};
  }
  const float slot = (float)(map->number - 1) * kSlot;
  return {slot + kMapWidth / 2 +
              wrap180(rightAscension - map->rightAscension) /
                  kRightAscensionPerMm,
          kMiddle + (map->declination - declination) / kDeclinationPerMm};
}

/** Which of the two lengths a place along the scroll falls on. */
int lengthOf(float along) { return along < kLengths[1].from ? 0 : 1; }

/** A place on the scroll in the pixels of the paper of length @p index. */
SkPoint onPaper(SkPoint scroll, int index) {
  const Length& length = kLengths[index];
  return {kRightEdge - length.left - (scroll.fX - length.from) * kPixelsPerMm,
          scroll.fY * kPixelsPerMm};
}

std::vector<std::vector<int>> runsOf(const std::string& line) {
  std::vector<std::vector<int>> runs(1);
  std::istringstream words(line);
  for (std::string word; words >> word;) {
    if (word == "|")
      runs.emplace_back();
    else
      runs.back().push_back(std::stoi(word));
  }
  return runs;
}

struct DunhuangStarChart {
  std::vector<SkPoint> stars;  // right ascension, declination
  std::vector<Map> maps;
  std::vector<Asterism> asterisms;
  sk_sp<SkTypeface> han, latin;

  void read(sketch::SketchContext& ctx) {
    const auto table = [&ctx](const char* name) {
      return ctx.assets.table(ctx.local(std::string("data/") + name));
    };
    if (const auto file = table("stars.csv")) {
      const auto rightAscension = file->column<double>("ra");
      const auto declination = file->column<double>("dec");
      for (size_t i = 0; i < rightAscension.size(); ++i)
        stars.push_back({(float)rightAscension[i], (float)declination[i]});
    }
    if (const auto file = table("maps.csv")) {
      const auto number = file->column<double>("map");
      const auto rightAscension = file->column<double>("ra");
      const auto declination = file->column<double>("dec");
      const auto column = file->column<std::string>("column");
      for (size_t i = 0; i < number.size(); ++i)
        maps.push_back({(int)number[i], (float)rightAscension[i],
                        (float)declination[i], column[i]});
    }
    if (const auto file = table("asterisms.csv")) {
      const auto name = file->column<std::string>("name");
      const auto school = file->column<std::string>("school");
      const auto line = file->column<std::string>("line");
      for (size_t i = 0; i < name.size(); ++i) {
        Asterism asterism{name[i], school[i].empty() ? ' ' : school[i][0],
                          runsOf(line[i]), std::nullopt};
        if (place(asterism)) asterisms.push_back(std::move(asterism));
      }
    }
  }

  /** AN ASTERISM IS DRAWN WHOLE ON ONE MAP, the one whose centre its mean
   *  right ascension is nearest, or on the disc when it stands north of
   *  +50°. Below −45° the sky never cleared the horizon at Chang'an. */
  bool place(Asterism& asterism) const {
    float east = 0, north = 0, declination = 0;
    int count = 0;
    for (const auto& run : asterism.runs)
      for (int star : run) {
        if (star < 0 || (size_t)star >= stars.size()) return false;
        east += std::cos(stars[(size_t)star].fX * kDegree);
        north += std::sin(stars[(size_t)star].fX * kDegree);
        declination += stars[(size_t)star].fY;
        ++count;
      }
    if (count == 0 || maps.size() != 12) return false;
    declination /= (float)count;
    if (declination >= 50.0f) return true;
    if (declination < -45.0f) return false;
    const float rightAscension = std::atan2(north, east) / kDegree;
    const int ladder = (int)std::lround(wrap180(rightAscension - 308.0f) / 30.0f);
    asterism.map = maps[(size_t)((ladder + 12) % 12)];
    return true;
  }

  /** Where along the scroll an asterism's map, or the disc, begins. */
  static float slotOf(const Asterism& asterism) {
    return asterism.map ? (float)(asterism.map->number - 1) * kSlot
                        : kDiscCentre;
  }

  /** A star of @p asterism on the paper of the length its map is on, so a
   *  map near the break is drawn whole on one side of it. */
  SkPoint where(const Asterism& asterism, int star) const {
    const SkPoint sky = stars[(size_t)star];
    return onPaper(onScroll(asterism.map, sky.fX, sky.fY),
                   lengthOf(slotOf(asterism)));
  }

  // =========================================================================

  /** One star: a dot of one size, ringed in black unless it is the hazy
   *  red of Fa, the dagger below Orion's belt. */
  static Element star(SkPoint at, char school, float radius = 3.3f) {
    const material::Color fill = school == 'R' || school == 'H' ? kCinnabar
                                 : school == 'B'                ? kInk
                                 : school == 'W'                ? kLeadWhite
                                                                : kUnread;
    Element dot = kit::dot(at, radius, Fill::color(school == 'H'
                                                     ? material::withAlpha(fill, 0.6f)
                                                     : fill));
    if (school != 'H')
      dot.stroke(stroke(1.1f, Fill::color(kInk), PathFormat::Align::Inner));
    return dot;
  }

  /** The asterism on one length of paper: its lines, then its dots over
   *  them, then its name written down beside its first star. */
  Element figure(const Asterism& asterism) const {
    std::vector<Element> parts;
    for (const auto& run : asterism.runs) {
      std::vector<SkPoint> stops;
      for (int star : run) stops.push_back(where(asterism, star));
      if (stops.size() > 1)
        parts.push_back(connect::wire(
            stops, routers::polyline(), 0, 0, 2, "",
            {.mark = stroke(0.9f, Fill::color(material::withAlpha(kInk, 0.85f)))}));
    }
    for (const auto& run : asterism.runs)
      for (int star : run)
        parts.push_back(this->star(where(asterism, star), asterism.school));
    const SkPoint first = where(asterism, asterism.runs.front().front());
    parts.push_back(text(asterism.name)
                        .styleClass("name")
                        .writingMode(weave::WritingMode::kVerticalRL)
                        .left(first.fX + 5)
                        .top(first.fY + 4));
    return box().inset(0).children(std::move(parts));
  }

  /** A map's calendar text: one column, down from near the paper's top,
   *  in the first column place left of the map. */
  static Element column(const Map& map) {
    const float along = (float)(map.number - 1) * kSlot;
    const SkPoint top = onPaper({along + kMapWidth + 8.0f, 14.0f}, lengthOf(along));
    return text(map.column)
        .styleClass("column")
        .writingMode(weave::WritingMode::kVerticalRL)
        .left(top.fX - 12)
        .top(top.fY);
  }

  /** One length of the scroll: paper under a darker lining along both
   *  edges, the columns, and every asterism whose map falls on it. */
  Element length(int index) const {
    const Length& length = kLengths[index];
    const float height = kScrollHeight * kPixelsPerMm;
    const auto onThis = [index](float along) { return lengthOf(along) == index; };
    std::vector<Element> figures;
    for (const Asterism& asterism : asterisms) {
      if (onThis(slotOf(asterism))) figures.push_back(figure(asterism));
    }
    std::vector<Map> columns;
    for (const Map& map : maps)
      if (onThis((float)(map.number - 1) * kSlot)) columns.push_back(map);
    return kit::at(length.left, length.top, kWidth - length.left, height)
        .fill(Paint::recipe(material::kit::board({.paint = kPaper,
                                                  .tooth = 0.07f,
                                                  .toothScale = 0.08f,
                                                  .stretch = 2.5f,
                                                  .wear = 0.16f,
                                                  .wearScale = 0.003f,
                                                  .seed = (float)index})))
        .overflow(Overflow::Clip)
        .background(shadow(hexColor(0x000000, 0.55f), {0, 10}, 24))
        .children({box().inset(0).fill(linearGradient(
                       {0, 0}, {0, height},
                       {material::withAlpha(kLining, 0.55f),
                        material::withAlpha(kLining, 0.0f),
                        material::withAlpha(kLining, 0.0f),
                        material::withAlpha(kLining, 0.55f)},
                       {0.0f, 0.06f, 0.94f, 1.0f})),
                   each(columns, column), box().inset(0).children(std::move(figures))});
  }

  /** The three schools as a key, on a slip of the same paper so the black
   *  and the white dots read as they do on the scroll. */
  Element key() const {
    const struct {
      char school;
      const char* name;
      const char* reading;
    } schools[] = {{'R', "石氏", "Shi Shen"},
                   {'B', "甘氏", "Gan De"},
                   {'W', "巫咸", "Wu Xian"},
                   {' ', "", "school not read"}};
    return kit::at(40, 1010, 180, 170)
        .padding(18)
        .column()
        .gap(14)
        .fill(Fill::color(kPaper))
        .styleClass("key")
        .children(
        each(schools, [](const auto& entry) {
          return box().row().gap(10).alignItems(Align::Center).children(
              {box().width(12).height(12).children(
                   {star({6, 6}, entry.school, 5.5f)}),
               text(entry.name).styleClass("keyName"),
               text(entry.reading).styleClass("caption")});
        }));
  }

  Element describe() const {
    return box()
        .inset(0)
        .fill(Fill::color(kGround))
        .applyStyleSheet(StyleSheet{
            rule(".column").font(
                weave::Type{.face = han, .size = 19.0f, .color = kInk}),
            rule(".name").font(weave::Type{
                .face = han, .size = 10.5f,
                .color = material::withAlpha(kInk, 0.9f)}),
            rule(".keyName").font(
                weave::Type{.face = han, .size = 15.0f, .color = kCaption}),
            rule(".caption").font(
                weave::Type{.face = latin, .size = 13.0f, .color = kCaption}),
            rule(".key .caption, .key .keyName").font(weave::Type{.color = kInk}),
            rule(".title").font(weave::Type{
                .face = latin, .size = 30.0f, .color = hexColor(0xe7d6b0),
                .track = 1.5f})})
        .children(
            {kit::at(64, 64, 1600, 120).column().gap(10).children(
                 {text("THE DUNHUANG STAR CHART").styleClass("title"),
                  text("British Library Or.8210/S.3326 · Tang, +649–684 · "
                       "ink and colour on paper, 2,100 × 244 mm · read right "
                       "to left, maps 1–7 above, 8–12 and the circumpolar "
                       "disc below")
                      .styleClass("caption")}),
             length(0), length(1), key(),
             text("1,460 stars of Chen Zhuo's catalogue placed at the epoch "
                  "+700 through the chart's own projection: 4.56° of right "
                  "ascension and 5.28° of declination to the centimetre, "
                  "the disc 5.10° to the centimetre about a centre 2.4° off "
                  "the pole")
                 .styleClass("caption")
                 .left(64)
                 .top(1500)});
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 0.05,
                             .background = kGround});
    han = weave::ports::face({"Songti TC", "Songti SC", "Noto Serif TC",
                              "STSong", "PingFang SC"});
    latin = weave::ports::face({"Baskerville", "Optima", "Georgia"});
    read(ctx);
    ctx.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(DunhuangStarChart, "Study · Esoteric",
             "The Dunhuang star chart (c. 649–684): twelve hour maps and the "
             "polar disc, 1,460 real stars in the three schools' colours")
