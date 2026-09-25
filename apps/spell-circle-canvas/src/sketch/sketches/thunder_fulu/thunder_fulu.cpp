// WU LEI HAO LING — a Thunder-Rite command talisman (fu), written in
// cinnabar on an iron plate five cun tall by three wide. Two long wavy
// strokes, hooked at head and foot, bound the column; between them, in the
// order the brush goes down:
//
//   the HEAD      three hooks for the Three Pure Ones, a stair stepping
//                 down and to the left;
//   the APERTURE  one ring struck in a single revolution, its ends apart;
//   the BODY      three cloud-seal graphs — rain over five, cloud, and
//                 ghost set to the left — real characters whose strokes
//                 wander until they can no longer be read;
//   the GALL      GANG, larger than anything else and left legible, since
//                 its ten strokes are the ten Heavenly Stems;
//   the TAP       the butt of the inverted brush, once;
//   the FOOT      "ji ji ru lu ling", thirty-eight strokes in one breath
//                 without a lift, so the brush runs dry and the iron shows
//                 through in streaks;
//
// and last the master's seal, square, five over thunder cut in relief.
//
// Every stroke is a centreline in data/strokes.json, in plate pixels and in
// drawing order, with the second it starts and ends; the characters' own
// strokes are the makemeahanzi medians. One brush presses along each by one
// width law. Beside the plate stand the chants sung while the strokes go
// down, the tempo each part is written at, and the Dipper the priest treads
// before writing.

// TAGS: Typography/Lettering, Drawing/Brushes

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Brush.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <span>
#include <string>
#include <vector>


namespace data = sigil::data;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace shapers = sigil::geometry::shapers;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using material::skia::Paint;
using sigil::motion::bind;

using namespace sigil::compose;

namespace {

// Cinnabar on beaten iron under an altar lamp. Cinnabar is a body colour
// that sits ON the metal, so it is the only bright thing on the plate, and
// where the brush runs dry the iron shows rather than a paler red.
const material::Color kNight = hexColor(0x08070a);
const material::Color kCinnabar = hexColor(0xcf3018);
const material::Color kCinnabarDry = hexColor(0xa82a14);
const material::Color kCinnabarWet = hexColor(0xf2542a);
const material::Color kSealInk = hexColor(0xc4301a, 0.92f);
const material::Color kGold = hexColor(0xb2914f);
const material::Color kGoldDim = hexColor(0x6d5a33);
const material::Color kChalk = hexColor(0xd8cdb6);
const material::Color kUmber = hexColor(0x9a8a68);

// The sheet, and the plate on it. Five cun by three is the specification,
// so the plate's height is its width in that ratio and nothing rounds it.
constexpr float kWidth = 1900, kHeight = 1220;
constexpr float kPlateLeft = 76, kPlateTop = 92;
constexpr float kPlateWidth = 624, kPlateHeight = kPlateWidth * 5 / 3;
// The spine every part of the talisman is strung on, in plate pixels.
constexpr float kSpine = 306;

// The score, in seconds. The foot is written from 20.0 s and the seal
// pressed at 21.45 s; the plate loops once every 27 s.
constexpr float kSeal = 21.45f, kGall = 15.95f, kGallEach = 0.36f;
constexpr float kHead = 1.90f, kHeadEach = 1.05f;
constexpr float kLoop = 27;

/** THE WIDTH LAW, as a multiple of a stroke's body width over the fraction
 *  of it written: the reversed-tip entry swells to 1.77, the body thins to
 *  0.73 a third of the way along, and the press before the lift swells
 *  back to 1.42 near the end. */
float widthLaw(float along) {
  along = std::clamp(along, 0.0f, 1.0f);
  const float press = along - 0.88f;
  return 1.15f * std::exp(-12.0f * along) + 0.62f + 0.28f * along +
         0.55f * std::exp(-90.0f * press * press);
}

/** ONE WRITTEN STROKE: its centreline sampled evenly, the pressure the law
 *  puts on every sample, the brush that lays it and when it is written. */
struct Written {
  draw::brush::Stroke line;
  draw::brush::Tool tool;
  float start = 0, end = 1;
};

/** The centreline through @p points (x, y pairs), pressed by the width law
 *  times @p weight along its own length. */
draw::brush::Stroke pressed(const data::Json& points, float weight) {
  std::vector<draw::brush::Sample> controls;
  const auto numbers = points.items();
  for (size_t index = 0; index + 1 < numbers.size(); index += 2)
    controls.push_back({{(float)numbers[index].number(),
                         (float)numbers[index + 1].number()}});
  draw::brush::Stroke line = draw::brush::spline(controls, 1.2f, 1.0f);
  float length = 0;
  std::vector<float> reached{0};
  for (size_t index = 1; index < line.size(); ++index) {
    const SkPoint step = line[index].position - line[index - 1].position;
    reached.push_back(length += step.length());
  }
  for (size_t index = 0; index < line.size(); ++index)
    line[index].pressure =
        weight * widthLaw(length > 0 ? reached[index] / length : 0);
  return line;
}

/** THE BRUSH: a bundle of hairs laid side by side across the stroke, its
 *  width the pressure and its envelope flat because the law is already in
 *  the samples. Loaded wet, every hair holds to the end of the stroke; the
 *  foot is written so fast that the load runs out, the hairs part and each
 *  runs dry on its own, and the stroke breaks into streaks along its
 *  length with the iron showing between them. */
draw::brush::Tool brush(float width, material::Color ink, float load) {
  return {.tip = draw::brush::Tip::Fibres,
          .color = ink,
          .width = width,
          .spacing = 0.7f,
          .opacity = 1.0f + load * 0.8f,
          .scatter = 0.0f,
          .density = load,
          .bristles = load < 1 ? 14 : 30,
          .pressure = {.variation = std::nullopt},
          .pressureOpacity = 0.0f,
          .markerTip = false};
}

struct ThunderFulu {
  sketch::kit::Document words;
  std::vector<Written> ink;
  std::vector<path::Polyline> sealGraphs;
  choreograph::Output<float> score{0.0f};
  StyleSheet sheet;
  weave::Type running;
  Paint ironGrain;
  Pattern ironSpeck;

  /** Every stroke of data/strokes.json as the brush will lay it. The foot
   *  is ONE stroke: its thirty-eight strokes chained by the light hops
   *  between them, because the brush never leaves the plate there. */
  void write(const data::Json& strokes) {
    constexpr float kFootWidth = 5.9f;
    Written foot{.tool = brush(kFootWidth, kCinnabarDry, 0.55f)};
    for (const data::Json& stroke : strokes.items()) {
      const std::string part(stroke["part"].text());
      const float width = (float)stroke["width"].number();
      const float start = (float)stroke["start"].number();
      const float end = (float)stroke["end"].number();
      if (part != "foot") {
        const material::Color loaded = part == "tap" ? kCinnabarWet : kCinnabar;
        ink.push_back({pressed(stroke["points"], 1.0f),
                       brush(width, loaded, 1.0f), start, end});
        continue;
      }
      draw::brush::Stroke next = pressed(stroke["points"], width / kFootWidth);
      if (foot.line.empty()) {
        foot.start = start;
      } else {
        const draw::brush::Stroke hop =
            draw::brush::segment(foot.line.back().position,
                                 next.front().position, 1.2f, 0.16f, 0.16f);
        foot.line.insert(foot.line.end(), hop.begin(), hop.end());
      }
      foot.line.insert(foot.line.end(), next.begin(), next.end());
      foot.end = end;
    }
    ink.push_back(std::move(foot));
  }

  /** THE INK: each stroke laid as far along as the score has written it.
   *  A stroke's randomness is seeded by its place in the order, so a
   *  frame paints the hairs the frame before it did. */
  Element brushwork() const {
    return pen("thunder_fulu.ink", [this](draw::Pen& pen) {
      const float now = score.value();
      for (size_t index = 0; index < ink.size(); ++index) {
        const Written& stroke = ink[index];
        const float written =
            std::clamp((now - stroke.start) / (stroke.end - stroke.start),
                       0.0f, 1.0f);
        const size_t count = (size_t)(written * (float)stroke.line.size());
        if (count < 2) continue;
        pen.randomSeed(1220 + (unsigned)index);
        draw::brush::paint(pen, stroke.tool,
                           std::span(stroke.line).first(count));
      }
    });
  }

  /** HAMMERED IRON: a plate whose edge is what a hammer leaves rather than
   *  a radius, lit from the upper left, scored by the file and pitted, on
   *  its own shadow. */
  Element iron() const {
    const auto beaten = shapes::shaped(shapes::chamfered(17.0f),
                                       shapers::Jitter{46.0f, 2.6f, 1356});
    return box().inset(0).children(
        {kit::at(-16, -8, kPlateWidth + 46, kPlateHeight + 44)
             .shape(shapes::chamfered(26.0f))
             .fill(Paint::radialGradient({0.5f, 0.5f}, 0.78f,
                                         {{0.0f, hexColor(0x000000, 0.66f)},
                                          {0.72f, hexColor(0x000000, 0.40f)},
                                          {1.0f, hexColor(0x000000, 0.0f)}})),
         box()
             .inset(0)
             .shape(beaten)
             .fill(Paint::linearGradient({0.10f, -0.06f}, {0.96f, 1.0f},
                                         {{0.0f, hexColor(0x736a5b)},
                                          {0.18f, hexColor(0x4f4840)},
                                          {0.46f, hexColor(0x35312c)},
                                          {0.78f, hexColor(0x201e1d)},
                                          {1.0f, hexColor(0x161514)}}))
             .foreground(lines::presets::hatch(
                 Fill::color(hexColor(0xa79a83, 0.075f)), 13.0f, 1.6f, -18.0f))
             .foreground(lines::presets::hatch(
                 Fill::color(hexColor(0x000000, 0.13f)), 31.0f, 3.4f, 24.0f))
             .foreground(Wash{.material = ironGrain,
                              .blend = SkBlendMode::kOverlay,
                              .amount = 0.30f})
             .foreground(Wash{.material = ironSpeck.material(),
                              .blend = SkBlendMode::kMultiply,
                              .amount = 0.85f})
             .stroke(lines::rails(
                 {{.width = 3.0f,
                   .fill = Fill::color(hexColor(0x5d564a, 0.85f))},
                  {.across = -5.5f,
                   .width = 1.1f,
                   .fill = Fill::color(hexColor(0x0a0909, 0.75f))}}))
             .cache(Cache::Texture)});
  }

  /** The five registers, ruled faintly across the plate the way it is
   *  laid out before it is written, each naming what goes on it; and the
   *  spine they are strung on. */
  Element registers() const {
    const material::Color scored = hexColor(0x0e0d0c, 0.28f);
    return box().inset(0).children(
        {kit::at(kit::line({.thickness = 0.8f,
                            .column = true,
                            .fill = Fill::color(scored)}),
                 kSpine, 28, 0.8f, kPlateHeight - 52),
         each(words["registers"].items(), [&](const data::Json& line) {
           return kit::at(18, (float)line["y"].number(), kPlateWidth - 36, 14)
               .children({kit::line({.thickness = 0.7f,
                                     .fill = Fill::color(scored)}),
                          text(line["words"]).styleClass("register")});
         })});
  }

  /** THE MASTER'S SEAL, pressed last: square, five above and thunder
   *  below, each graph stretched to fill its half of the square as seal
   *  script is, and cut in relief so the graphs and the frame print. */
  Element seal() const {
    const auto pressedAt = bind(&score).window(kSeal, kSeal + 0.45f);
    return kit::at(474, 786, 104, 104)
        .rotate(-6.0f)
        .transformOrigin(pct(50), pct(50))
        .opacity(pressedAt)
        .scale(bind(&score).window(kSeal, kSeal + 0.45f).target(1.5f, 1.0f))
        .foreground(decorations::border(5.0f, Fill::color(kSealInk), 2.0f))
        .children(each(sealGraphs, [](const path::Polyline& graph) {
          return box()
              .inset(0)
              .shape(heldPath(path::smoothThrough(graph)))
              .fill(Fill::none())
              .stroke(PathFormat{.width = 4.6f,
                                 .strokeFill = Fill::color(kSealInk),
                                 .cap = path::Cap::Square,
                                 .join = path::Join::Miter});
        }));
  }

  Element plate() const {
    return kit::at(kPlateLeft, kPlateTop, kPlateWidth, kPlateHeight)
        .opacity(bind(&score).window(0.05f, 1.15f))
        .children({iron(), registers(), brushwork(), seal(),
                   // The iron's grain again, faintly over the ink, so the
                   // cinnabar reads as lying on the metal and not over it.
                   box()
                       .inset(0)
                       .shape(shapes::chamfered(17.0f))
                       .fill(ironGrain)
                       .opacity(0.085f)
                       .blendMode(SkBlendMode::kSoftLight)
                       .cache(Cache::Texture)});
  }

  /** A section: its head over a gold rule with a dashed hairline under it,
   *  then what the section says. */
  Element section(const data::Json& heading,
                  std::initializer_list<Children> body, float gap = 7,
                  const char* voice = "heading") const {
    return box().column().gap(8).children(
        {document::h2(heading).styleClass(voice),
         kit::line({.thickness = 1.3f,
                    .fill = Fill::color(hexColor(0xb2914f, 0.48f)),
                    .pair = kit::Line::Companion{
                        .thickness = 0.6f,
                        .gap = 3.4f,
                        .fill = Fill::color(hexColor(0xb2914f, 0.26f)),
                        .dash = {1.3f, 4.2f}}}),
         box().column().gap(gap).children(body)});
  }

  /** A CHANT, sung while the strokes go down: line @p index lights at
   *  @p start plus that many @p each, on the same score the ink is on, so
   *  a phrase and the stroke it belongs to cannot drift apart. */
  std::vector<Element> sung(const data::Json& lines, float start, float each,
                            float hold) const {
    return ::each(lines.items(), [&](const data::Json& line, size_t index) {
      const float at = start + (float)index * each;
      const bool record = line.kind() == data::Json::Kind::Record;
      return text(record ? line["words"] : line)
          .styleClass(std::string(record ? line["style"].text("chant")
                                         : "chant"))
          .opacity(bind(&score).window(at, at + hold).target(0.14f, 0.98f));
    });
  }

  /** THE WIDTH LAW, painted by the brush the plate is written with along
   *  one straight stroke, its three landmarks named where they fall. */
  Element law() const {
    const data::Json& said = words["margin"]["law"];
    constexpr float kLength = 440;
    return section(
        said["heading"],
        {box().width(kLength).height(58).children(
             {pen("thunder_fulu.law",
                  [](draw::Pen& pen) {
                    draw::brush::Stroke line =
                        draw::brush::segment({4, 22}, {kLength - 4, 22}, 1.2f);
                    for (size_t index = 0; index < line.size(); ++index)
                      line[index].pressure = widthLaw(
                          (float)index / (float)(line.size() - 1));
                    pen.randomSeed(46);
                    draw::brush::paint(pen, brush(21, kCinnabar, 1), line);
                  }),
              each(said["marks"].items(),
                   [](const data::Json& mark) {
                     const float along = (float)mark["s"].number();
                     return text(mark["words"])
                         .styleClass("lawMark")
                         .at({4 + along * (kLength - 8) -
                                  (along > 0.5f ? 54.0f : 0.0f),
                              44});
                   })}),
         text(said["note"]).styleClass("gloss")});
  }

  /** The tempo each part of the talisman is written at. The last row is
   *  the foot, lit in cinnabar, because its tempo is the table's point. */
  Element tempo() const {
    const data::Json& said = words["tempo"];
    std::vector<sketch::kit::Row> rows;
    const auto read = said["rows"].items();
    for (size_t index = 0; index < read.size(); ++index) {
      sketch::kit::Row row;
      for (const data::Json& cell : read[index].items())
        row.cells.emplace_back(std::string(cell.text()));
      if (index + 1 == read.size()) row.ink = kCinnabar;
      rows.push_back(std::move(row));
    }
    return section(said["heading"],
                   {sketch::kit::table(std::move(rows),
                                       {.columns = {{.width = 62},
                                                    {.width = 70},
                                                    {.width = 44},
                                                    {.width = 98},
                                                    {}}}),
                    text(said["gloss"]).styleClass("gloss")});
  }

  /** BU GANG TA DOU: the priest's walk over the nine stations of the
   *  Dipper, on the real sky — the stars' positions projected about the
   *  asterism's own centre and scaled so the seven span one unit across.
   *  Zuo Fu stands off Kai Yang by hand, as every tread plate sets it;
   *  You Bi cannot be seen, so its station is drawn open. */
  Element tread() const {
    const data::Json& said = words["tread"];
    constexpr float kSpan = 530, kLeft = 0, kTop = 44;
    const auto stations = said["stations"].items();
    const auto station = [&](size_t index) {
      return SkPoint{kLeft + (float)stations[index]["x"].number() * kSpan,
                     kTop + (float)stations[index]["y"].number() * kSpan};
    };
    path::Polyline walk;
    for (size_t index = 0; index < stations.size(); ++index)
      walk.points.push_back({station(index).fX, station(index).fY});
    const auto seen = bind(&score).window(12.7f, 13.6f);
    return section(
        said["heading"],
        {box().width(kSpan + 20).height(kSpan * 0.53f + 84).children(
             {box()
                  .inset(0)
                  .shape(heldPath(path::toPath(walk)))
                  .fill(Fill::none())
                  .stroke(lines::rails(
                      {{.width = 2.6f,
                        .fill = Fill::color(hexColor(0x8b6f36, 0.50f))},
                       {.width = 1.0f,
                        .fill = Fill::color(hexColor(0xd8bd7c, 0.75f)),
                        .dash = {2.0f, 7.0f}}}))
                  .opacity(seen),
              each(stations,
                   [&](const data::Json& star, size_t index) {
                     const bool unseen = star["unseen"].boolean(false);
                     const SkPoint at = station(index);
                     const float lit = 13.1f + (float)index * 0.16f;
                     return box()
                         .inset(0)
                         .opacity(bind(&score).window(lit, lit + 0.45f))
                         .children(
                             {kit::disc(at, 12)
                                  .shape(shapes::star(6, 0.30f))
                                  .fill(unseen ? Fill::none()
                                               : Fill::color(hexColor(
                                                     0xe4c98a, 0.92f)))
                                  .stroke(PathFormat{
                                      .width = 1.0f,
                                      .strokeFill = Fill::color(hexColor(
                                          0xe4c98a, unseen ? 0.7f : 0.4f)),
                                      .dashIntervals =
                                          unseen ? std::vector<float>{2, 2.6f}
                                                 : std::vector<float>{}}),
                              text(kit::formatted(
                                       "%d %s", (int)index + 1,
                                       std::string(star["name"].text())
                                           .c_str()))
                                  .styleClass("station")
                                  .at({at.fX - 40 +
                                           (float)star["nameShift"].number(),
                                       at.fY - 30}),
                              text(star["star"])
                                  .styleClass("bayer")
                                  .at({at.fX - 18 +
                                           (float)star["starShift"].number(),
                                       at.fY + 12})});
                   })}),
         each(said["lines"].items(),
              [](const data::Json& line) {
                return text(line).styleClass("colophon");
              })});
  }

  Element describe() const {
    const data::Json& margin = words["margin"];
    const data::Json& chant = words["chant"];
    const auto column = [](float left, float width) {
      return kit::at(left, kPlateTop + 30, width, kPlateHeight - 40)
          .column()
          .justifyContent(Justify::SpaceBetween);
    };
    return box()
        .inset(0)
        .font(running)
        .applyStyleSheet(sheet)
        .children(
            {kit::at(76, 34, 1500, 50).column().gap(6).children(
                 {text(words["title"]).styleClass("title"),
                  text(words["subtitle"]).styleClass("subtitle")}),
             plate(),
             column(764, 440).children(
                 {section(margin["head"]["heading"],
                          {sung(margin["head"]["lines"], kHead, kHeadEach,
                                0.3f)},
                          11),
                  law(),
                  section(margin["gall"]["heading"],
                          {box().column().gap(5).children(
                               // the six phrases finish as the tenth stroke
                               // lands, so each takes ten strokes over six
                               sung(margin["gall"]["lines"], kGall,
                                    10 * kGallEach / 6, 0.28f)),
                           text(margin["gall"]["note"]).styleClass("note")}),
                  tempo()}),
             column(1264, 590).children(
                 {tread(),
                  section(chant["heading"],
                          {box().column().gap(9).children(
                               sung(chant["lines"], kGall, 0.7f, 0.4f)),
                           text(chant["gloss"]).styleClass("gloss")},
                          7, "rite")}),
             text(words["footer"])
                 .styleClass("note")
                 .at({76, kHeight - 34})
                 .width(1600)});
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kNight);
    // After the seal lands: the whole talisman, written and sealed.
    context.captureAt(22.4);
    words = sketch::kit::Document(context, "data/content.json");
    if (const auto strokes =
            context.assets.json(context.local("data/strokes.json"))) {
      write((*strokes)["strokes"]);
      for (const data::Json& graph : (*strokes)["seal"].items()) {
        path::Polyline line;
        const auto numbers = graph.items();
        for (size_t index = 0; index + 1 < numbers.size(); index += 2)
          line.points.push_back({(float)numbers[index].number(),
                                 (float)numbers[index + 1].number()});
        sealGraphs.push_back(std::move(line));
      }
    }

    ironGrain =
        Paint::recipe(material::field::grain(2.2f, 4, 1356.0f, 0.55f, 2.6f));
    ironSpeck = material::pattern::speckle(
        420, 26, 0.7f, 2.6f,
        {hexColor(0x7c7263, 0.10f), hexColor(0x000000, 0.16f)});
    ironSpeck.seed(1220);

    // The terminal face wherever a line names none; a display face in
    // gold for the heads; the book italic for what is sung and glossed.
    const auto terminal = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
    const auto italic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                               SkFontStyle::kItalic_Slant);
    const auto display = weave::ports::face({"Optima", "Baskerville"}, 700);
    running = {.face = terminal, .size = 10.5f, .color = kUmber};
    sheet = StyleSheet{
        rule(".title").font(
            {.face = display, .size = 22, .color = kChalk, .track = 2.6f}),
        rule(".subtitle").font({.size = 10.5f, .color = kGoldDim}),
        rule(".heading").font(
            {.face = display, .size = 11.5f, .color = kGold, .track = 1.1f}),
        rule(".rite").font(
            {.face = display, .size = 17, .color = kGold, .track = 1.4f}),
        rule(".chant").font({.face = italic, .size = 11.5f, .color = kChalk}),
        rule(".lands").font(
            {.face = italic, .size = 11.5f, .color = hexColor(0xe07a52)}),
        rule(".gloss").font(
            {.face = italic, .size = 10.5f, .color = hexColor(0x7d6f52)}),
        rule(".colophon").font(
            {.face = italic, .size = 10.5f, .color = hexColor(0x8d7f60)}),
        rule(".note").font({.size = 9.5f, .color = hexColor(0x5d5341)}),
        rule(".lawMark").font({.size = 8.5f, .color = hexColor(0xa89778)}),
        rule(".register").font(
            {.size = 8.5f, .color = hexColor(0x0b0a09, 0.60f)}),
        rule(".station").font(
            {.size = 9.5f, .color = hexColor(0xa48c5c, 0.9f)}),
        rule(".bayer").font({.face = italic,
                             .size = 9.0f,
                             .color = hexColor(0x6f6047, 0.85f)})};
    context.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext&) {
    score = (float)std::fmod(elapsed, (double)kLoop);
  }
};

}  // namespace

SIGIL_SKETCH(ThunderFulu, "Study · Esoteric",
             "A Thunder-Rite talisman, WRITTEN — real stroke medians, "
             "and the foot at 7.1× the body's tempo")
