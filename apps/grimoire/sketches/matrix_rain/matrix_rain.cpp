/** @file
 * THE DIGITAL RAIN of The Matrix (1999), seen on an operator's tube: the
 * type stands in its cells and the light runs down it, at three depths,
 * while the glyphs churn under the light on clocks of their own.
 *
 * FROM THE RECORD. The code was designed by Simon Whiteley at Animal
 * Logic, who has said the characters came from his wife's Japanese
 * cookbooks: HALF-WIDTH KATAKANA, MIRRORED left to right, mixed with Latin
 * digits. Green on black; the LEADING glyph of a streak is near white with
 * a bloom about it, and the tail decays through the phosphor's green to
 * dark. Columns run at differing rates and phases, and a cell keeps being
 * replaced by another from the set whether or not a streak is passing.
 *
 * THIS STUDY'S OWN: three depths of rain — small, slow and hazed toward
 * cyan at the back, large and fast at the front — drifting apart as a
 * camera held by hand would see them; the head's bloom, a spot of light
 * that rides down with it; a cell that now and then catches and flares;
 * the title, which resolves out of churning glyphs, holds, and churns
 * back into the rain; and the TUBE: the phosphor warm at its middle,
 * rounded corners, raster lines, a sheen off the upper left and an
 * operator's trace line typing across the top.
 *
 * ONE SCHEDULE DRIVES THE LIGHT. A column's drop falls at its own rate
 * and rests off the foot for its own while before it enters again; where
 * the drop's head stands is one function of time and the column, read
 * twice — by the glyphs, each asking how long ago the head passed its
 * cell, and by the bloom, which is placed where the head is. Every
 * column's numbers are hashed from its plane's seed and its index, so
 * both readers find the same drop.
 *
 * The words, the charsets and every plane's clocks stand in
 * `data/rain.json`; the sheet sets each plane's type.
 */
// TAGS: Typography/Effects, Motion/Particles

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>
#include <sigilweave/unicode/Unicode.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace data = sigil::data;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace noise = sigil::core::noise;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using namespace std::chrono_literals;

namespace {

constexpr float kWidth = 1280;
constexpr float kHeight = 780;

/** The void the tube falls off to. */
constexpr material::Color kVoid = {0.004f, 0.012f, 0.006f, 1};

/** THE GREENS a cell passes through after the head has left it: the
 *  phosphor at full, and the dark it decays toward. The sheet sets every
 *  plane in the head's near-white, and these multiply it down. */
constexpr material::Color kPhosphor = {0.30f, 1.0f, 0.46f, 1};
constexpr material::Color kDecayed = {0.02f, 0.26f, 0.07f, 1};

/** The span of the rain's clock, in seconds. The glyphs read time as a
 *  fraction of it, so it is long enough that no one watches it wrap. */
constexpr float kClockSeconds = 3600;

StyleSheet screen() {
  return StyleSheet{
      rule("plane")
          .fontFamily(
              "Hiragino Kaku Gothic ProN, Hiragino Sans, Osaka, sans-serif")
          .font({.verticalForm = weave::VerticalForm::kUpright})
          .writingMode(weave::WritingMode::kVerticalRL)
          .ink({0.93f, 1.0f, 0.95f, 1})
          .inset(0),
      rule(".far").fontSize(15).opacity(0.62f).ink({0.50f, 0.92f, 0.86f, 1}),
      rule(".mid").fontSize(22).opacity(0.86f),
      rule(".near").fontSize(34),
      rule("title")
          .fontFamily("Menlo, SF Mono, monospace")
          .fontWeight(700)
          .fontSize(68)
          .letterSpacing(22)
          .ink({0.90f, 1.0f, 0.93f, 1}),
      rule("caption, eyebrow")
          .fontFamily("Helvetica Neue, Arial, sans-serif")
          .fontWeight(500)
          .fontSize(10.5f)
          .letterSpacing(2.4f)
          .ink({0.22f, 0.40f, 0.27f, 1}),
      rule("eyebrow").fontWeight(700).ink({0.34f, 0.62f, 0.40f, 1}),
      // The operator's line: the monitor's own face, set in the head's
      // near-white, which its cascade darkens to green as it does the
      // rain's.
      rule("code")
          .fontFamily("Menlo, SF Mono, monospace")
          .fontSize(12)
          .letterSpacing(1.1f)
          .ink({0.93f, 1.0f, 0.95f, 1}),
  };
}

/** The glyphs of a charset, each one character's bytes: a character opens
 *  at every byte that is not a UTF-8 continuation byte. */
std::vector<std::string> glyphsOf(std::string_view utf8) {
  std::vector<std::string> glyphs;
  for (const char byte : utf8) {
    if ((byte & 0xC0) != 0x80) glyphs.emplace_back();
    if (!glyphs.empty()) glyphs.back().push_back(byte);
  }
  return glyphs;
}

/** The code points of a charset, which is what a substitution draws. */
std::u32string codepointsOf(const Utf8& charset) {
  const std::u16string units = weave::unicode::toUtf16(charset.bytes());
  std::u32string codepoints;
  for (size_t offset = 0; offset < units.size();)
    codepoints += weave::unicode::decodeAt(units, offset);
  return codepoints;
}

/** A hash of (seed, index) on [0, 1). */
float unitHash(uint32_t seed, uint32_t index) {
  return noise::hash(seed, index) * 0.5f + 0.5f;
}

/** WHICH CELLS ARE DIGITS, addressed by the characters themselves, so the
 *  partition follows whatever text a seed dealt. A substitution keeps the
 *  original glyph's pen position and is honoured only where the
 *  replacement has the original's advance; this face gives the half-width
 *  forms one advance and the digits another, so each class churns within
 *  itself. */
weave::Selector digitCells() { return weave::selectors::regex(u8"[0-9#]"); }

/** One depth of rain, read from the words file and cut to its cells. */
struct Plane {
  std::string name;
  uint32_t seed = 0;
  /** The slowest and fastest a column's drop falls, in cells a second. */
  float slowest = 1, fastest = 1;
  /** How many cells the tail takes to fall to a third of the head. */
  float tailCells = 1;
  /** The least and most cells of dark a drop leaves below the foot
   *  before it enters again. */
  float restLeast = 0, restMost = 0;
  /** How far the plane drifts, in pixels: the nearer, the further. */
  float drift = 0;
  /** The bloom about the head, in ems across; 0 is none. */
  float halo = 0;

  int columns = 1, rows = 1;
  float cellWidth = 1, cellHeight = 1;
  std::string text;

  float width() const { return kWidth + 2 * drift; }
  float height() const { return kHeight + 2 * drift; }
};

/** THE DROP IN ONE COLUMN: how fast it falls, the length of one pass —
 *  the column, the tail leaving it and the rest below — and where in that
 *  pass it stood when the screen came up, all hashed from the plane's
 *  seed and the column. */
struct Drop {
  float cellsPerSecond = 1;
  float passCells = 1;
  float startCells = 0;

  static Drop of(const Plane& plane, uint32_t column) {
    const float pace = unitHash(plane.seed, column * 3);
    const float rest = unitHash(plane.seed, column * 3 + 1);
    const float pass = (float)plane.rows + plane.tailCells * 3 +
                       std::lerp(plane.restLeast, plane.restMost, rest);
    return {.cellsPerSecond = std::lerp(plane.slowest, plane.fastest, pace),
            .passCells = pass,
            .startCells = unitHash(plane.seed, column * 3 + 2) * pass};
  }
  /** Where the head stands at @p seconds, in cells from the top of the
   *  column, on [0, passCells). */
  float headAt(float seconds) const {
    return std::fmod(startCells + seconds * cellsPerSecond, passCells);
  }
};

/** ONE CELL OF THE RAIN at one moment: the light the drop leaves in it,
 *  the glyph it shows, and, rarely, a flare. The stream is seeded from the
 *  cell, so its own churn rate, phase and seeds are the same every frame. */
GlyphModifier cell(const Plane& plane, const std::u32string& charset,
                   bool mirrored, const GlyphInfo& glyph, float seconds,
                   noise::Mix64Stream& random) {
  const float churnRate = random.range(0.25f, 2.4f);
  const float churnPhase = random.unit();
  const uint32_t churnSeed = random.bits();
  const uint32_t flareSeed = random.bits();

  const Drop drop = Drop::of(plane, glyph.lineIndex);
  // A glyph's pen stands on its cell's baseline, inside the cell.
  const float row = std::floor(glyph.rest.y / plane.cellHeight);
  // Cells since the head passed this one, on [0, passCells).
  float since = drop.headAt(seconds) - row;
  if (since < 0) since += drop.passCells;

  GlyphModifier modifier;
  if (mirrored) modifier.scaleX = -1.0f;
  // Under the head the glyph churns fast; elsewhere on its own clock.
  const float churn =
      since < 2 ? seconds * 14 : seconds * churnRate + churnPhase;
  modifier.codepoint =
      charset[(size_t)(unitHash(churnSeed, (uint32_t)churn) * charset.size()) %
              charset.size()];
  if (since < 1) return modifier;  // the head: the sheet's near-white
  const float light = std::exp(-(since - 1) / plane.tailCells);
  modifier.colorMultiplier = material::mixLinear(kDecayed, kPhosphor, light);
  modifier.alpha = light;
  // Now and then a lit cell catches and flares back to the head's white.
  if (light > 0.25f && unitHash(flareSeed, (uint32_t)(seconds * 6)) > 0.99f) {
    modifier.colorMultiplier = {1, 1, 1, 1};
    modifier.alpha = 1;
  }
  return modifier;
}

}  // namespace

struct MatrixRain {
  /** The one clock: seconds since the screen came up. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);
  std::vector<std::string> kana, digits;
  std::u32string kanaCodepoints, digitCodepoints;
  uint32_t digitsOneIn = 1;
  std::vector<Plane> planes;
  std::string credit, statement, motto;
  struct {
    std::string words, charset;
    float periodSeconds = 1, phaseSeconds = 0;
  } titleLine;
  struct {
    std::string words;
    float eachMs = 0, durationMs = 0, loopMs = 0, blinkSeconds = 0;
  } traceLine;

  /** A plane read from its entry in the words file, and its text dealt:
   *  as many columns as fit it, each ended by a newline after as many
   *  cells as fit its height — measured on the plane's own class, so the
   *  sheet's size is the one the grid is cut for. */
  Plane readPlane(sketch::SketchContext& ctx, const data::Json& entry) const {
    Plane plane{.name = std::string(entry["plane"].string()),
                .seed = (uint32_t)entry["seed"].number(),
                .slowest = (float)entry["cellsPerSecond"][0].number(),
                .fastest = (float)entry["cellsPerSecond"][1].number(),
                .tailCells = (float)entry["tailCells"].number(),
                .restLeast = (float)entry["restCells"][0].number(),
                .restMost = (float)entry["restCells"][1].number(),
                .drift = (float)entry["drift"].number(),
                .halo = (float)entry["halo"].number()};
    std::string probe;
    for (int at = 0; at < 8; ++at) probe += kana.front();
    const auto column = ctx.measure(text(probe)
                                        .role("plane")
                                        .styleClass(plane.name)
                                        .applyStyleSheet(screen()));
    plane.cellHeight = column.height() / 8;
    plane.cellWidth = std::max(1.0f, column.width());
    plane.rows =
        std::max(1, (int)std::floor(plane.height() / plane.cellHeight));
    plane.columns =
        std::max(1, (int)std::floor(plane.width() / plane.cellWidth));
    noise::Mix64Stream random(plane.seed);
    for (int at = 0; at < plane.columns; ++at) {
      if (at > 0) plane.text += '\n';
      for (int row = 0; row < plane.rows; ++row) {
        const auto& set = random.bits() % digitsOneIn == 0 ? digits : kana;
        plane.text += set[random.bits() % set.size()];
      }
    }
    return plane;
  }

  /** The rain's clock as a text track reads it: a fraction of the long
   *  span, which the cells turn back into seconds. */
  motion::Animatable<float> clock() const {
    return motion::bind(seconds, {.from = {0, kClockSeconds}, .wrap = 1.0f});
  }

  /** The cells of one class of a plane, lit by its drops: the kana
   *  mirrored, the digits upright, each churning within its own charset. */
  TextEffect rain(const Plane& plane, bool mirrored) const {
    const std::u32string& charset = mirrored ? kanaCodepoints : digitCodepoints;
    return textFx::effect(
               "rain-" + plane.name + (mirrored ? "-kana" : "-digits"),
               [plane, charset, mirrored](const GlyphInfo& glyph,
                                          float progress,
                                          noise::Mix64Stream& random) {
                 return cell(plane, charset, mirrored, glyph,
                             progress * kClockSeconds, random);
               },
               0.0f)
        .displacing(false);
  }

  /** THE BLOOM ABOUT A HEAD: a spot of phosphor light, added to what it
   *  passes, placed where the column's drop stands. It is drawn once and
   *  moved; the head is always the brightest the rain gets, so one spot
   *  serves every head of the plane. */
  Element bloom(const Plane& plane, uint32_t column) const {
    const Drop drop = Drop::of(plane, column);
    const float across = plane.cellHeight * plane.halo;
    const float tall = across * 1.35f;
    // Vertical-RL columns fill from the right, each glyph set against the
    // right of its column, one em across.
    const float centreX =
        plane.width() - (float)column * plane.cellWidth - plane.cellHeight / 2;
    return kit::at(centreX - across / 2, -tall / 2, across, tall)
        .hitTestable(false)
        .blendMode(material::BlendMode::PlusLighter)
        .translateY(motion::bind(
            seconds,
            {.to = {drop.startCells * plane.cellHeight,
                    (drop.startCells + drop.cellsPerSecond) * plane.cellHeight},
             .wrap = drop.passCells * plane.cellHeight}))
        .children({box()
                       .cover()
                       .key("bloom")
                       .cache(Cache::Texture)
                       .fill(material::radialGradient(
                           {0.5f, 0.5f}, 0.5f,
                           {{0.0f, {0.62f, 1.0f, 0.72f, 0.66f}},
                            {0.18f, {0.26f, 0.95f, 0.45f, 0.40f}},
                            {0.50f, {0.05f, 0.50f, 0.18f, 0.13f}},
                            {1.0f, {0, 0.2f, 0.06f, 0}}}))});
  }

  /** ONE DEPTH OF RAIN: its field of cells, lit by the drops, the blooms
   *  riding its heads, and the whole plane drifting by its depth. */
  Element depth(const Plane& plane) const {
    std::vector<Element> blooms;
    if (plane.halo > 0)
      for (int column = 0; column < plane.columns; ++column)
        blooms.push_back(bloom(plane, (uint32_t)column));
    const auto sway = [&](uint32_t seed) {
      return motion::bind(seconds, {.to = {0, 0},
                                    .wiggle = {.amount = plane.drift,
                                               .frequency = 0.06f,
                                               .seed = plane.seed * 7 + seed,
                                               .octaves = 2}});
    };
    return kit::at(-plane.drift, -plane.drift, plane.width(), plane.height())
        .key(plane.name)
        .hitTestable(false)
        .translateX(sway(0))
        .translateY(sway(1))
        .children({
            text(plane.text)
                .role("plane")
                .styleClass(plane.name)
                .textFx({.where = !digitCells(),
                         .effect = rain(plane, true),
                         .tween = {.duration = 1s},
                         .progress = clock()})
                .textFx({.where = digitCells(),
                         .effect = rain(plane, false),
                         .tween = {.duration = 1s},
                         .progress = clock()}),
            stack().cover().children(std::move(blooms)),
        });
  }

  /** THE TITLE, out of the rain and back: a there-and-back over its
   *  period, stretched and clamped so it rests hidden, climbs, rests
   *  resolved and falls again. While it climbs every letter churns through
   *  the charset and lands at its own moment; a void opens behind it so
   *  the word stands off the rain. */
  Element title() const {
    const auto reveal = motion::bind(
        seconds, {.from = {-titleLine.phaseSeconds,
                           titleLine.periodSeconds - titleLine.phaseSeconds},
                  .alternate = true,
                  .to = {-0.5f, 1.7f},
                  .clamp = {0.0f, 1.0f}});
    const TextEffect arrival = textFx::tween(
        {.from = GlyphModifier{.alpha = 0.0f, .colorMultiplier = kPhosphor},
         .keyframes = {{.to = GlyphModifier{.colorMultiplier = kPhosphor},
                        .duration = 350ms},
                       {.to = GlyphModifier{}, .duration = 650ms}}});
    return kit::at(0, kHeight * 0.5f - 90, kWidth, 180)
        .key("title")
        .column()
        .alignItems(Align::Center)
        .justifyContent(Justify::Center)
        .children({
            box()
                .cover()
                .opacity(reveal)
                .cache(Cache::Texture)
                .fill(material::radialGradient(
                    {0.5f, 0.5f}, 0.5f,
                    {{0.0f, material::withAlpha(kVoid, 0.94f)},
                     {0.6f, material::withAlpha(kVoid, 0.70f)},
                     {1.0f, material::withAlpha(kVoid, 0)}})),
            text(titleLine.words)
                .role("title")
                .cache(Cache::Texture)
                .filter(material::Filter::glow({0.16f, 1.0f, 0.38f, 0.85f}, 9))
                .textFx({.effect = textFx::scramble(titleLine.charset, 18),
                         .tween = {.duration = 1s},
                         .progress = reveal})
                .textFx({.effect = arrival,
                         .tween = {.duration = 1s},
                         .progress = reveal}),
        });
  }

  /** THE OPERATOR'S LINE across the top of the glass. One looping
   *  cascade strikes the characters in order, holds them and wipes them
   *  in the order they were typed; the cursor, the line's last character,
   *  blinks on its own clock. */
  Element trace() const {
    constexpr material::Color settled = {0.34f, 0.93f, 0.50f, 1};
    const TextEffect traced = textFx::tween(
        {.from = GlyphModifier{.alpha = 0.0f},
         .keyframes = {
             {.to = GlyphModifier{}, .duration = 4ms},
             {.to = GlyphModifier{.colorMultiplier = settled},
              .duration = 41ms},
             {.to = GlyphModifier{.colorMultiplier = settled},
              .duration = 855ms},
             {.to = GlyphModifier{.alpha = 0.0f, .colorMultiplier = settled},
              .duration = 25ms},
             {.to = GlyphModifier{.alpha = 0.0f}, .duration = 75ms}}});
    const TextEffect blink =
        textFx::tween({.keyframes = {{.to = GlyphModifier{.alpha = 0.0f},
                                      .duration = 1000ms}}});
    const motion::Duration each =
        std::chrono::duration<double, std::milli>(traceLine.eachMs);
    return document::code(traceLine.words)
        .key("trace")
        .textFx(
            {.effect = traced,
             .tween = {.duration = std::chrono::duration<double, std::milli>(
                           traceLine.durationMs),
                       .delay = motion::stagger(each),
                       .loop = -1,
                       .loopDelay = std::chrono::duration<double, std::milli>(
                           traceLine.loopMs - traceLine.durationMs)},
             .progress = motion::bind(
                 seconds,
                 {.to = {0.0f, 1000.0f / traceLine.loopMs}, .wrap = 1.0f})})
        .textFx({.where = weave::selectors::regex(u8"█"),
                 .effect = blink,
                 .progress = motion::bind(
                     seconds, {.from = {0, traceLine.blinkSeconds},
                               .envelope = motion::envelope::square(0.55f),
                               .to = {1.0f, 0.0f}})});
  }

  /** THE GLASS over the rain, which never moves: the tube's falloff toward
   *  its corners, a sheen off the upper left where the room's light lands
   *  on its curve, its raster lines, and two grounds of the void rising off
   *  the top and bottom edges for the words to stand on. */
  Element glass() const {
    const auto ground = [](float height, bool fromTop) {
      const std::vector<material::ColorStop> stops = {
          {0.0f, material::withAlpha(kVoid, 0.92f)},
          {0.5f, material::withAlpha(kVoid, 0.70f)},
          {1.0f, material::withAlpha(kVoid, 0)}};
      return material::linearGradient(
          {0, fromTop ? 0 : height}, {0, fromTop ? height : 0}, stops,
          {.units = material::GradientUnits::Pixels});
    };
    return box()
        .cover()
        .key("glass")
        .hitTestable(false)
        .cache(Cache::Texture)
        .foreground(decorations::wash(material::pattern::scanlines(
            {.color = {0, 0, 0, 0.16f}, .period = 3, .on = 1})))
        .children({
            box().cover().fill(material::radialGradient(
                {0.5f, 0.5f}, 0.72f,
                {{0.55f, material::withAlpha(kVoid, 0)},
                 {1.0f, material::withAlpha(kVoid, 0.85f)}})),
            box().cover().fill(material::linearGradient(
                {0, 0}, {kWidth * 0.55f, kHeight * 0.75f},
                {{0.0f, {0.80f, 1.0f, 0.88f, 0.060f}},
                 {0.45f, {0.80f, 1.0f, 0.88f, 0.012f}},
                 {1.0f, {0.80f, 1.0f, 0.88f, 0}}},
                {.units = material::GradientUnits::Pixels})),
            kit::at(0, 0, kWidth, 58).fill(ground(58, true)),
            kit::at(0, kHeight - 74, kWidth, 74)
                .fill(ground(74, false))
                .row()
                .justifyContent(Justify::SpaceBetween)
                .alignItems(Align::End)
                .paddingBottom(17)
                .paddingLeft(30)
                .paddingRight(30)
                .children({box().column().gap(5).children(
                               {document::eyebrow(credit),
                                document::caption(statement)}),
                           document::caption(motto)}),
        });
  }

  Element describe() const {
    return stack()
        .role("screen")
        .borderRadius(34)
        .overflow(Overflow::Clip)
        .fill(material::radialGradient(
            {kWidth * 0.5f, kHeight * 0.46f},
            std::hypot(kWidth, kHeight) * 0.5f,
            {{0.0f, {0.014f, 0.050f, 0.026f, 1}},
             {0.55f, {0.007f, 0.020f, 0.011f, 1}},
             {1.0f, kVoid}},
            {.units = material::GradientUnits::Pixels}))
        .applyStyleSheet(screen())
        .children({
            each(planes, [this](const Plane& plane) { return depth(plane); }),
            title(),
            glass(),
            kit::at(30, 22, kWidth - 60, 20).children({trace()}),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    // The title held over a rain deep in its steady state: fresh heads,
    // long tails and columns resting dark between drops.
    sketch::kit::stage(
        ctx,
        {.size = {kWidth, kHeight}, .captureAt = 7.0, .background = kVoid});
    ctx.engine.timer([this](motion::Duration, motion::Duration elapsed) {
      seconds = (float)elapsed.count();
      return true;
    });

    const sketch::kit::Document words{ctx, "data/rain.json"};
    const data::Json& charsets = words["charsets"];
    kana = glyphsOf(charsets["kana"].string());
    digits = glyphsOf(charsets["digits"].string());
    digitsOneIn =
        std::max<uint32_t>(1, (uint32_t)charsets["digitsOneIn"].number());
    kanaCodepoints = codepointsOf(charsets["kana"]);
    digitCodepoints = codepointsOf(charsets["digits"]);
    credit = std::string(words["credit"].string());
    statement = std::string(words["statement"].string());
    motto = std::string(words["motto"].string());
    const data::Json& title = words["title"];
    titleLine = {.words = std::string(title["words"].string()),
                 .charset = std::string(title["charset"].string()),
                 .periodSeconds = (float)title["periodSeconds"].number(),
                 .phaseSeconds = (float)title["phaseSeconds"].number()};
    const data::Json& trace = words["trace"];
    traceLine = {.words = std::string(trace["words"].string()),
                 .eachMs = (float)trace["eachMs"].number(),
                 .durationMs = (float)trace["durationMs"].number(),
                 .loopMs = (float)trace["loopMs"].number(),
                 .blinkSeconds = (float)trace["blinkSeconds"].number()};
    if (kana.empty() || digits.empty()) return;
    for (const data::Json& entry : words["planes"].array())
      planes.push_back(readPlane(ctx, entry));
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(MatrixRain, "Study · Type",
             "The Matrix's digital rain (1999) — three depths of mirrored "
             "half-width katakana lit by falling drops, blooming heads and "
             "a title that resolves out of the churn")
