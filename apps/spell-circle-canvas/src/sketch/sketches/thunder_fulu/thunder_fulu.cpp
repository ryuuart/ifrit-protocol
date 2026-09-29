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

#include <sigildata/decode/Json.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilweave/style/Face.h>
#include <sigilgeometry/advanced/Skia.h>
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
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
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
using material::Paint;
using sigil::motion::bind;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

// Cinnabar on beaten iron under an altar lamp. Cinnabar is a body colour
// that sits ON the metal, so it is the only bright thing on the plate, and
// where the brush runs dry the iron shows rather than a paler red.
const material::Color kNight = hexColor(0x08070a);
const material::Color kCinnabar = hexColor(0xcf3018);
const material::Color kCinnabarDry = hexColor(0xa82a14);
// Fresh paint is glossier and brighter than it dries, and catches the lamp
// in a line; laid thick it throws a shadow and its edge takes the light;
// the foot's thinning load leaves a stain rather than a body.
const material::Color kCinnabarWet = hexColor(0xf2542a, 0.42f);
const material::Color kGlint = hexColor(0xffd6b0, 0.62f);
const material::Color kLitEdge = hexColor(0xf08a5c, 0.85f);
const material::Color kCastShadow = hexColor(0x050403, 0.55f);
const material::Color kStain = hexColor(0xb8321c);
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
// How long the seal takes to come down onto the plate.
constexpr float kSealFall = 0.26f;

/** How the plate gives under the seal, as a share of the deepest it goes
 *  over the time it takes to settle: struck down at once, then ringing
 *  back up with less each time. */
float recoil(float settled) {
  return std::sin(settled * 3.14159265f * 3.0f) * std::exp(-4.2f * settled) *
         (1.0f - settled);
}

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

/** ONE WRITTEN PIECE: a run of a stroke's centreline sampled evenly, the
 *  pressure the law puts on every sample, the brush that lays it, when it
 *  is written and the rectangle its ink stays inside. A loaded brush lays
 *  a body of paint under its hairs; whether the piece opens or closes its
 *  stroke says where the round of the brush's tip is left. */
struct Written {
  draw::brush::Stroke line;
  draw::brush::Tool tool;
  float start = 0, end = 1;
  float left = 0, top = 0, width = 0, height = 0;
  /** The full width of the paint the brush leaves at pressure one. */
  float thickness = 0;
  bool loaded = true, opens = false, closes = false;
  /** Samples at the head of the line that belong to the piece before:
   *  the body of paint is laid over them too, so it covers the lit edge
   *  and the shadow a piece throws back over the join, and nothing else
   *  is. */
  size_t lead = 0;

  /** How many samples are laid once @p fraction of the piece is
   *  written. */
  size_t reached(float fraction) const {
    return lead + (size_t)(fraction * (float)(line.size() - lead));
  }
};

/** How far back into the piece before a body of paint is laid. */
constexpr size_t kLead = 4;

/** The most samples one piece holds. A frame repaints only the pieces
 *  being written, each from its own start, so a stroke is cut into pieces
 *  short enough that the one in progress is cheap to lay again; a finished
 *  piece is kept as pixels. */
constexpr size_t kPieceSamples = 96;

// The altar lamp stands to the upper left of the plate. Cinnabar laid
// thick stands proud of the iron: it throws a shadow away from the lamp
// and its edge toward the lamp catches the light. Fresh paint is glossy
// and dries matte over these seconds.
constexpr SkPoint kShadowFall = {1.4f, 1.8f};
constexpr SkPoint kLampSide = {-0.75f, -0.85f};
constexpr float kDrying = 3.2f;

/** The centreline through @p points (x, y pairs), pressed by the width law
 *  times @p weight along its own length. */
draw::brush::Stroke pressed(const data::Json& points, float weight) {
  std::vector<draw::brush::Sample> controls;
  const auto numbers = points.array();
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
 *  the samples. Over a loaded stroke the hairs are the furrows the brush
 *  combs into its own body of paint, and part where the load thins; the
 *  foot is written so fast that the load runs out, the hairs part and each
 *  runs dry on its own, and the stroke breaks into streaks along its
 *  length with the iron showing between them. */
draw::brush::Tool brush(float width, material::Color ink, float load,
                        int bristles) {
  return {.tip = draw::brush::Tip::Fibres,
          .color = ink,
          .width = width,
          .spacing = 0.7f,
          .opacity = 2.2f,
          .scatter = 0.0f,
          .density = load,
          .bristles = bristles,
          .pressure = {.variation = std::nullopt},
          .pressureOpacity = 0.0f,
          .markerTip = false};
}

/** The hairs a loaded brush combs through a body of paint @p thickness
 *  wide: darker furrows, kept inside its edges. */
draw::brush::Tool combed(float thickness) {
  return brush(thickness * 0.84f, kCinnabarDry, 0.72f, 7);
}

/** THE BODY OF PAINT a loaded brush leaves along @p samples: the region
 *  its width sweeps, @p width times the pressure at every station. */
SkPath bodyOf(std::span<const draw::brush::Sample> samples, float width) {
  path::Polyline spine;
  for (const draw::brush::Sample& sample : samples)
    spine.points.push_back({sample.position.fX, sample.position.fY});
  const auto across = [&](const path::SweepStation& station) {
    // The samples are evenly spaced, so a fraction of the length is a
    // fraction of the samples.
    const size_t at = (size_t)std::lround(station.fraction *
                                          (float)(samples.size() - 1));
    return samples[std::min(at, samples.size() - 1)].pressure * width;
  };
  return path::toSk(path::sweptRegion(
      path::toPath(spine), across,
      {.stepPx = 1.5f, .join = path::SweepJoin::Round}));
}

struct ThunderFulu {
  sketch::kit::Document words;
  std::vector<Written> ink;
  /** When the writing of each part of the talisman begins, by the part's
   *  name in data/strokes.json. */
  std::map<std::string, float> begun;
  std::vector<path::Polyline> sealGraphs;
  sigil::motion::Animatable<float> score = sigil::motion::animatable(0.0f);
  StyleSheet sheet;
  weave::Type running;
  Paint ironGrain;
  Pattern ironSpeck;
  std::optional<material::Material> dents;

  /** Adds @p line, written from @p start to @p end, as pieces of at most
   *  kPieceSamples samples; each piece shares one sample with the next so
   *  the ink runs on through the join, and is written over its own share
   *  of the time. */
  void lay(const draw::brush::Stroke& line, const draw::brush::Tool& tool,
           float thickness, bool loaded, float start, float end) {
    const size_t count = line.size();
    for (size_t first = 0; first + 1 < count; first += kPieceSamples) {
      const size_t last = std::min(count, first + kPieceSamples + 1);
      const size_t lead = std::min(first, kLead);
      Written piece{.line = {line.begin() + (long)(first - lead),
                             line.begin() + (long)last},
                    .tool = tool,
                    .start = start + (end - start) * (float)first / (float)count,
                    .end = start + (end - start) * (float)last / (float)count,
                    .thickness = thickness,
                    .loaded = loaded,
                    .opens = loaded && first == 0,
                    .closes = loaded && last == count,
                    .lead = lead};
      float left = piece.line.front().position.fX, right = left;
      float top = piece.line.front().position.fY, bottom = top;
      float pressure = 0;
      for (const draw::brush::Sample& sample : piece.line) {
        left = std::min(left, sample.position.fX);
        right = std::max(right, sample.position.fX);
        top = std::min(top, sample.position.fY);
        bottom = std::max(bottom, sample.position.fY);
        pressure = std::max(pressure, sample.pressure);
      }
      // Half the widest body, its shadow, and the antialiasing past it.
      const float reach = thickness * pressure * 0.5f + 6.0f;
      piece.left = std::floor(left - reach);
      piece.top = std::floor(top - reach);
      piece.width = std::ceil(right + reach) - piece.left;
      piece.height = std::ceil(bottom + reach) - piece.top;
      ink.push_back(std::move(piece));
    }
  }

  /** Every stroke of data/strokes.json as the brush will lay it. The foot
   *  is written without a lift: each of its thirty-eight strokes begins
   *  with the light hop from where the one before it ended, because the
   *  brush never leaves the plate there, and the load it carries thins
   *  from the first of them to the last. */
  void write(const data::Json& strokes) {
    constexpr float kFootWidth = 5.9f, kFootStrokes = 38;
    float footWritten = 0;
    std::optional<SkPoint> footReached;
    for (const data::Json& stroke : strokes.array()) {
      const std::string part(stroke["part"].string());
      const float width = (float)stroke["width"].number();
      const float start = (float)stroke["start"].number();
      const float end = (float)stroke["end"].number();
      begun.try_emplace(part, start);
      if (part != "foot") {
        lay(pressed(stroke["points"], 1.0f),
            combed(width), width, true, start, end);
        continue;
      }
      draw::brush::Stroke line = pressed(stroke["points"], width / kFootWidth);
      if (footReached) {
        draw::brush::Stroke hop = draw::brush::segment(
            *footReached, line.front().position, 1.2f, 0.16f, 0.16f);
        hop.insert(hop.end(), line.begin(), line.end());
        line = std::move(hop);
      }
      footReached = line.back().position;
      const float load = 0.78f - 0.40f * (footWritten++ / (kFootStrokes - 1));
      lay(line, brush(kFootWidth, kCinnabar, load, 5), kFootWidth * 1.2f,
          false, start, end);
    }
  }

  /** The body of paint piece @p written leaves over its first @p count
   *  samples, painted at @p offset in @p color: the swept region, and the
   *  round of the tip where the stroke begins and where it lifts. */
  static void body(draw::Pen& pen, const Written& written, size_t count,
                   SkPoint offset, material::Color color, bool overLead) {
    const size_t from = overLead ? 0 : written.lead;
    if (count < from + 2) return;
    const auto samples = std::span(written.line).subspan(from, count - from);
    const float width = written.thickness;
    pen.push();
    pen.translate(offset.fX, offset.fY);
    pen.noStroke();
    pen.fill(color);
    pen.shape(bodyOf(samples, width));
    const auto tip = [&](const draw::brush::Sample& sample) {
      pen.circle(sample.position.fX, sample.position.fY,
                 sample.pressure * width * 0.92f);
    };
    if (written.opens) tip(samples.front());
    if (written.closes && count == written.line.size()) tip(samples.back());
    pen.pop();
  }

  /** Piece @p written as it dries, over its first @p count samples. A loaded
   *  piece is its body of paint, standing on its shadow with its lit edge
   *  showing, and the brush's hairs combed through it; the foot is the
   *  hairs over a stain. */
  static void dry(draw::Pen& pen, const Written& written, size_t count,
                  unsigned seed) {
    const auto samples =
        std::span(written.line).subspan(written.lead, count - written.lead);
    if (written.loaded) {
      body(pen, written, count, kShadowFall, kCastShadow, false);
      body(pen, written, count, kLampSide, kLitEdge, false);
      body(pen, written, count, {0, 0}, kCinnabar, true);
    } else {
      // Too little is left on the brush to stand proud: a thin stain the
      // iron shows through, thinner as the load runs out.
      material::Color stain = kStain;
      stain.a = written.tool.density * 0.45f;
      body(pen, written, count, {0, 0}, stain, false);
    }
    pen.randomSeed(seed);
    draw::brush::paint(pen, written.tool, samples);
  }

  /** The gloss of piece @p written while it is wet, over its first @p count
   *  samples: the body brighter, and the lamp caught in a line along the
   *  crown of the paint on the side it stands. */
  static void wet(draw::Pen& pen, const Written& written, size_t count) {
    if (!written.loaded) return;
    body(pen, written, count, {0, 0}, kCinnabarWet, false);
    const float width = written.thickness;
    pen.push();
    pen.noFill();
    pen.stroke(kGlint);
    pen.strokeWeight(width * 0.16f);
    pen.strokeCap(draw::ROUND);
    pen.strokeJoin(draw::ROUND);
    pen.translate(kLampSide.fX * width * 0.22f, kLampSide.fY * width * 0.22f);
    pen.beginShape();
    for (const draw::brush::Sample& sample :
         std::span(written.line).subspan(written.lead, count - written.lead))
      pen.vertex(sample.position.fX, sample.position.fY);
    pen.endShape();
    pen.pop();
  }

  /** One piece of the ink, laid once and kept as pixels, shown from the
   *  moment it is written; its gloss is kept beside it and fades as it
   *  dries. */
  Element piece(size_t index) const {
    const Written& written = ink[index];
    const std::string key = "thunder_fulu.piece." + std::to_string(index);
    const auto kept = [this, index, &written](bool gloss) {
      return [this, index, gloss, left = written.left,
              top = written.top](draw::Pen& pen) {
        pen.translate(-left, -top);
        const Written& written = ink[index];
        const size_t count = written.line.size();
        gloss ? wet(pen, written, count)
              : dry(pen, written, count, 1220 + (unsigned)index);
      };
    };
    std::vector<Element> coats{
        pen(key, kept(false), Cache::Texture)};
    if (written.loaded)
      coats.push_back(
          pen(key + ".wet", kept(true), Cache::Texture)
              .opacity(bind(score, {.from = {written.end, written.end + kDrying},
                                    .clampFrom = true,
                                    .ease = sigil::motion::ease::inQuad,
                                    .to = {1.0f, 0.0f}})));
    return kit::at(written.left, written.top, written.width, written.height)
        .opacity(bind(score, {.from = {written.end, written.end + 0.001f},
                              .clampFrom = true}))
        .children(coats);
  }

  /** THE INK: the finished pieces as they are kept, and over them the
   *  pieces being written, each laid as far along as the score has
   *  reached with the seed its finished pixels are laid with, so a piece
   *  ends the frame before it is kept as the picture it is kept as. */
  Element brushwork() const {
    return box().inset(0).children(
        {box().inset(0).children(
             each(ink.size(), [this](size_t index) { return piece(index); })),
         pen("thunder_fulu.ink", [this](draw::Pen& pen) {
           const float now = score.value();
           for (size_t index = 0; index < ink.size(); ++index) {
             const Written& stroke = ink[index];
             if (now <= stroke.start || now >= stroke.end) continue;
             const float written = (now - stroke.start) / (stroke.end - stroke.start);
             const size_t count = stroke.reached(written);
             if (count < stroke.lead + 2) continue;
             dry(pen, stroke, count, 1220 + (unsigned)index);
             wet(pen, stroke, count);
           }
         })});
  }

  /** THE PLATE'S FACE: iron darkening away from the lamp, dented all over
   *  by the hammer that beat it flat, each dent a shallow bowl that turns
   *  its far wall to the lamp and its near wall away. The lamp is low, warm
   *  and to the upper left, as the ink's own shadows say. */
  material::Material beatenIron() const {
    return material::from(
               material::linearGradient({0.10f, -0.06f}, {0.96f, 1.0f},
                                        {{0.0f, hexColor(0x8a7f6c)},
                                         {0.18f, hexColor(0x5e564c)},
                                         {0.46f, hexColor(0x3f3a34)},
                                         {0.78f, hexColor(0x272423)},
                                         {1.0f, hexColor(0x1a1918)}}))
        .surface({.metallic = 0.55f,
                  .roughness = 0.46f,
                  .normal = dents,
                  .lighting = material::studio({.direction = 128,
                                                .elevation = 34,
                                                .color = hexColor(0xffd2a0),
                                                .intensity = 1.05f,
                                                .ambient = 0.40f})});
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
             .fill(sigil::material::radialGradient({0.5f, 0.5f}, 0.78f,
                                         {{0.0f, hexColor(0x000000, 0.66f)},
                                          {0.72f, hexColor(0x000000, 0.40f)},
                                          {1.0f, hexColor(0x000000, 0.0f)}})),
         box()
             .inset(0)
             .shape(beaten)
             .fill(beatenIron())
             .foreground(lines::presets::hatch(
                 Fill::color(hexColor(0xa79a83, 0.075f)), 13.0f, 1.6f, -18.0f))
             .foreground(lines::presets::hatch(
                 Fill::color(hexColor(0x000000, 0.13f)), 31.0f, 3.4f, 24.0f))
             .foreground(Wash{.material = sigil::material::skia::base(ironGrain),
                              .blend = material::BlendMode::Overlay,
                              .amount = 0.30f})
             .foreground(Wash{.material = ironSpeck.material(),
                              .blend = material::BlendMode::Multiply,
                              .amount = 0.85f})
             .stroke(lines::rails(
                 {{.width = 3.0f,
                   .fill = Fill::color(hexColor(0x5d564a, 0.85f))},
                  {.across = -5.5f,
                   .width = 1.1f,
                   .fill = Fill::color(hexColor(0x0a0909, 0.75f))}}))
             // The lamp's pool: warm where it stands, falling off down the
             // plate.
             .foreground(Wash{
                 .material = sigil::material::radialGradient(
                     {0.26f, 0.14f}, 0.95f,
                     {{0.0f, hexColor(0xffb070, 0.34f)},
                      {0.45f, hexColor(0xc07038, 0.12f)},
                      {1.0f, hexColor(0x000000, 0.0f)}}),
                 .blend = material::BlendMode::Screen,
                 .amount = 1.0f})});
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
         each(words["registers"].array(), [&](const data::Json& line) {
           return kit::at(18, (float)line["y"].number(), kPlateWidth - 36, 14)
               .children({kit::line({.thickness = 0.7f,
                                     .fill = Fill::color(scored)}),
                          text(line["words"]).styleClass("register")});
         })});
  }

  /** THE MASTER'S SEAL, pressed last: square, five above and thunder
   *  below, each graph stretched to fill its half of the square as seal
   *  script is, and cut in relief so the graphs and the frame print. */
  /** How far the plate has given under the seal. */
  sigil::motion::Animatable<float> recoiling() const {
    return bind(score, {.from = {kSeal + kSealFall, kSeal + kSealFall + 0.7f},
                        .clampFrom = true,
                        .ease = recoil,
                        .to = {0.0f, 3.2f}});
  }

  Element seal() const {
    return kit::at(474, 786, 104, 104)
        .cache(Cache::Texture)
        .rotate(-6.0f)
        .transformOrigin(pct(50), pct(50))
        .opacity(bind(score, {.from = {kSeal, kSeal + 0.08f}, .clampFrom = true}))
        // The seal is brought down from above the plate and lands hard:
        // it gathers speed all the way to the iron.
        .scale(bind(score, {.from = {kSeal, kSeal + kSealFall},
                            .clampFrom = true,
                            .ease = sigil::motion::ease::inCubic,
                            .to = {1.6f, 1.0f}}))
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

  /** THE PLATE: iron and its ruled registers kept as pixels, the ink and
   *  the seal over them. The plate gives under the seal as it lands, and
   *  at the end of the rite the ink is washed off for the next writing. */
  Element plate() const {
    return kit::at(kPlateLeft, kPlateTop, kPlateWidth, kPlateHeight)
        .translateY(recoiling())
        .children(
            {box()
                 .inset(0)
                 .children({iron(), registers()})
                 .cache(Cache::Texture)
                 .key("thunder_fulu.ground"),
             box()
                 .inset(0)
                 .opacity(bind(score, {.from = {kLoop - 1.3f, kLoop - 0.15f},
                                       .clampFrom = true,
                                       .ease = sigil::motion::ease::inOutSine,
                                       .to = {1.0f, 0.0f}}))
                 .children({brushwork(), seal()})});
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
    return ::each(lines.array(), [&](const data::Json& line, size_t index) {
      const float at = start + (float)index * each;
      const bool record = line.kind() == data::Json::Kind::Object;
      return text(record ? line["words"] : line)
          .styleClass(std::string(record ? line["style"].string("chant")
                                         : "chant"))
          .opacity(sigil::motion::bind(score, {.from = {at, at + hold}, .clampFrom = true, .to = {0.14f, 0.98f}}));
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
                    Written stroke{
                        .line = draw::brush::segment({12, 22},
                                                     {kLength - 12, 22}, 1.2f),
                        .tool = combed(21),
                        .thickness = 21,
                        .opens = true,
                        .closes = true};
                    for (size_t index = 0; index < stroke.line.size(); ++index)
                      stroke.line[index].pressure = widthLaw(
                          (float)index / (float)(stroke.line.size() - 1));
                    dry(pen, stroke, stroke.line.size(), 46);
                  }, Cache::Texture),
              each(said["marks"].array(),
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

  /** When the part a row of the tempo table names begins to be written:
   *  its rows run head, aperture, body, gall, foot. */
  float partBegun(size_t row) const {
    static const char* const kParts[] = {"head", "aperture", "body", "gall",
                                         "foot"};
    const auto found = begun.find(row < 5 ? kParts[row] : "");
    return found == begun.end() ? 0.0f : found->second;
  }

  /** The tempo each part of the talisman is written at. The last row is
   *  the foot, lit in cinnabar, because its tempo is the table's point. */
  Element tempo() const {
    const data::Json& said = words["tempo"];
    std::vector<std::vector<Utf8>> rows;
    const auto read = said["rows"].array();
    for (size_t index = 0; index < read.size(); ++index) {
      std::vector<Utf8> row;
      for (const data::Json& cell : read[index].array())
        row.emplace_back(std::string(cell.string()));
      rows.push_back(std::move(row));
    }
    std::vector<std::span<const Utf8>> tableRows;
    for (const auto& row : rows) tableRows.emplace_back(row);
    return section(said["heading"],
                   {sigil::compose::kit::table(tableRows,
                                       {.columns = {{.width = 62},
                                                    {.width = 70},
                                                    {.width = 44},
                                                    {.width = 98},
                                                    {}},
                                        .gap = sketch::kit::theme().spacing.labelGap,
                                        .rowGap = sketch::kit::theme().spacing.rowGap,
                                        .cellLine = [this, last = rows.size() - 1](const Utf8& words, const kit::Table& table, size_t column, size_t row) {
                                          auto line = table.columns[column].figure ? kit::figure(words) : kit::captionNote(words);
                                          if (row == last) line.ink(kCinnabar);
                                          // A row comes up as the brush reaches its part.
                                          const float from = partBegun(row);
                                          return line.opacity(bind(score, {.from = {from - 0.3f, from + 0.2f}, .clampFrom = true, .to = {0.28f, 1.0f}}));
                                        }}),
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
    const auto stations = said["stations"].array();
    const auto station = [&](size_t index) {
      return SkPoint{kLeft + (float)stations[index]["x"].number() * kSpan,
                     kTop + (float)stations[index]["y"].number() * kSpan};
    };
    path::Polyline walk;
    for (size_t index = 0; index < stations.size(); ++index)
      walk.points.push_back({station(index).fX, station(index).fY});
    const auto seen = sigil::motion::bind(score, {.from = {12.7f, 13.6f}, .clampFrom = true});
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
                         .opacity(sigil::motion::bind(score, {.from = {lit, lit + 0.45f}, .clampFrom = true}))
                         .children(
                             {kit::disc(sigil::geometry::path::fromSk(at),12)
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
                                       std::string(star["name"].string())
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
         each(said["lines"].array(),
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
            context.assets.hub().load<sigil::data::Json>(context.local("data/strokes.json"))) {
      write((*strokes)["strokes"]);
      for (const data::Json& graph : (*strokes)["seal"].array()) {
        path::Polyline line;
        const auto numbers = graph.array();
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
    dents = material::shader(context.assets.hub(), context.local("data/dents.sksl"));

    // The terminal face wherever a line names none; a display face in
    // gold for the heads; the book italic for what is sung and glossed.
    const auto terminal = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
    const auto italic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                               sigil::weave::FaceSlant::Italic);
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
