/** @file
 * THE DIGITAL RAIN of The Matrix (1999), the in-film kind: an operator's
 * screen where the type never moves and the light runs down it. Three
 * falling curtains of vertical type over a churning bed, thousands of
 * glyphs substituted, tinted and faded on their own clocks at once — a
 * text engine's worst case, with no layout to hide behind and no hero
 * line to carry the eye.
 *
 * FROM THE RECORD. The code was designed by Simon Whiteley at Animal
 * Logic, who has said the characters came from his wife's Japanese
 * cookbooks: HALF-WIDTH KATAKANA, MIRRORED left to right, mixed with Latin
 * digits. Green on black; the LEADING glyph of a streak is near white and
 * the tail decays through green toward dark. Columns run at differing
 * rates and phases, and the glyphs CHURN — a cell keeps being replaced by
 * another from the set whether or not a streak is passing through it. The
 * title sequence lets characters travel down the frame; the operators'
 * monitors hold them in place and run the brightness down the column,
 * which is what this screen shows.
 *
 * THIS STUDY'S OWN: every colour, size, rate and seed, and the four planes
 * — a dim bed and three falling depths — where the film's screens are one.
 *
 * THE MACHINE, declared once in `setup` and never re-described:
 *   - ONE TEXT PER PLANE in vertical-RL columns, one column a LINE unit,
 *     so "each column on its own clock" is a seeded random ladder over
 *     lines spread across the loop period, with a nested cluster cascade
 *     running the glyphs down inside each column's beat.
 *   - THE RAIN NEVER STOPS because the cascade LOOPS: every glyph's beat
 *     re-opens once per period at its column's scattered offset, so every
 *     age of streak is on screen at once. Its master is one clock of
 *     seconds, shaped into a phase that wraps on the declared period, so
 *     the drive and the schedule cannot drift.
 *   - THE STREAK is one keyframe table read at different local times: the
 *     head's flash, the green decay and the dark between drops.
 *   - THE CHURN is `textFx::scramble` on two more tracks, the field split
 *     by a selector into its two advance classes; the record's mirror and
 *     a seeded phosphor lift ride a third and fourth.
 *   - THE LOAD, deliberately: per-glyph alpha and tint every frame, a
 *     matrix draw per mirrored glyph, and blurred glow underlays beneath
 *     two planes that per-glyph fades split into fade classes. Every
 *     underlay must land beneath every foreground; a halo drawn over a
 *     neighbouring glyph's body is this study failing.
 *
 * The words, the charsets and every plane's clock stand in
 * `data/rain.json`; how each plane looks stands in `screen()`.
 */
// TAGS: Typography/Effects, Motion/Particles

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/kit/PaintLayers.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

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

namespace {

constexpr float kWidth = 1280;
constexpr float kHeight = 780;

/** The void the screen is drawn on, which the caption's ground fades
 *  back into. */
constexpr material::Color kVoid = {0.004f, 0.012f, 0.006f, 1};

/** HOW THE SCREEN LOOKS. Every plane is set in one voice: the half-width
 *  forms standing UPRIGHT in vertical-RL columns (their default vertical
 *  orientation is rotated, and the screens show them standing), in the
 *  HEAD's near-white — `colorMultiplier` only darkens, so the brightest
 *  moment of a streak is the one the sheet owns. A plane's class is its
 *  depth: its size, its haze, and its halation. A phosphor screen excites
 *  a spot rather than drawing a glyph, and the spot spreads into a green
 *  bloom heavy enough to fill a glyph's counters; the far plane takes none,
 *  because a blurred underlay is a second pass over every glyph and at
 *  sixteen pixels it does not change the picture. */
StyleSheet screen() {
  const auto halation = [](float sigma) {
    return weave::Type{.underlays = std::vector<weave::PaintLayer>{
                           weave::kit::glow(0xC040FF70, sigma, 1.9f)}};
  };
  return StyleSheet{
      rule("plane")
          .fontFamily(
              "Hiragino Kaku Gothic ProN, Hiragino Sans, Osaka, sans-serif")
          .font({.verticalForm = weave::VerticalForm::kUpright})
          .writingMode(weave::WritingMode::kVerticalRL)
          .ink({0.97f, 1.0f, 0.98f, 1}),
      rule("screen > plane").inset(0).overflow(Overflow::Clip),
      rule(".bed").fontSize(20).ink({0.030f, 0.095f, 0.042f, 1}),
      rule(".far").fontSize(16).opacity(0.55f),
      rule(".mid").fontSize(23).opacity(0.80f).font(halation(5.5f)),
      rule(".near").fontSize(32).font(halation(9)),
      rule("caption")
          .fontFamily("Helvetica Neue, Arial, sans-serif")
          .fontWeight(500)
          .fontSize(10.5f)
          .letterSpacing(2.2f)
          .ink({0.24f, 0.42f, 0.28f, 1}),
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

// workaround: `textFx::scramble` takes its charset as UTF-32 while every
// text leaf takes UTF-8, so the charset the words file carries is decoded
// here to reach it.
std::u32string codepointsOf(const std::vector<std::string>& glyphs) {
  std::u32string codepoints;
  for (const std::string& glyph : glyphs) {
    const int length = (int)glyph.size();
    const auto lead = (unsigned char)glyph[0];
    char32_t point = length == 1 ? lead : lead & (0x7Fu >> length);
    for (int at = 1; at < length; ++at)
      point = (point << 6u) | ((unsigned char)glyph[at] & 0x3Fu);
    codepoints += point;
  }
  return codepoints;
}

/** THE TWO CHARSETS, cut where the substitution gate cuts. A substitution
 *  keeps the original glyph's pen position, so it is honoured only where
 *  the replacement has the original's advance; the face this screen is set
 *  in gives every half-width form one advance and the digits and '#'
 *  another ('4' is cut wider still and is left out). So each class churns
 *  within itself, and one mixed charset would freeze exactly the cells
 *  whose roll crossed the class line. */
struct Charset {
  std::vector<std::string> kana, digits;
  /** Roughly one cell in this many is a digit, sprinkled through the
   *  kana rather than dealt evenly. */
  uint32_t digitsOneIn = 7;
};

/** WHICH CELLS ARE DIGITS, addressed by the characters themselves, so the
 *  partition follows whatever text a seed dealt. */
weave::Selector digitCells() { return weave::selectors::regex(u8"[0-9#]"); }

/** THE STREAK over one glyph's local time: the near-white flash (the
 *  head colour itself), the decay to phosphor green, the long dim, and
 *  gone by 1. Between beats a looping glyph rests at local 1, so the dark
 *  between drops is the table's own tail and the re-opening flash is its
 *  head. */
TextEffect streak() {
  return textFx::keys({
      {0.000f, {}},
      {0.155f, {}},
      {0.300f, {.colorMultiplier = {0.27f, 0.96f, 0.42f, 1}}},
      {0.600f, {.colorMultiplier = {0.09f, 0.50f, 0.16f, 1}}},
      {1.000f, {.alpha = 0.0f, .colorMultiplier = {0.02f, 0.20f, 0.06f, 1}}},
  });
}

/** The phosphor lift every cell wears: a seeded screen of green, most
 *  cells barely and a few hard (the square of a uniform draw), so no two
 *  cells burn alike. Stable per glyph, so the field caches between churn
 *  steps. When @p mirrored the glyph also flips about its own centre, as
 *  the record's half-width forms do while the columns keep their order. */
GlyphModifier phosphor(noise::Mix64Stream& random, bool mirrored) {
  const float lift = random.unit() * random.unit();
  GlyphModifier modifier;
  modifier.colorScreen = {0.10f * lift, 0.45f * lift, 0.16f * lift, 0.0f};
  if (mirrored) modifier.scaleX = -1.0f;
  return modifier;
}
TextEffect mirroredKana() {
  return textFx::effect(
      "rain-kana",
      [](const GlyphInfo&, float, noise::Mix64Stream& random) {
        return phosphor(random, true);
      },
      0.0f);
}
TextEffect uprightDigits() {
  return textFx::effect(
      "rain-digits",
      [](const GlyphInfo&, float, noise::Mix64Stream& random) {
        return phosphor(random, false);
      },
      0.0f);
}

/** One plane of the screen: its class in the sheet, the text dealt for
 *  it, and its clocks. A plane with no `loopMs` never falls — the bed. */
struct Plane {
  std::string name;
  std::string text;
  int columns = 1;
  uint32_t seed = 0;
  float churnSeconds = 1;
  float churnOffsetSeconds = 0;
  float eachMs = 0;
  float durationMs = 0;
  float loopMs = 0;
};

}  // namespace

struct MatrixRain {
  /** The one clock: seconds since the screen came up. Every plane's fall
   *  and churn is a phase shaped from it on the property that reads it. */
  choreograph::Output<float> seconds{0.0f};
  Charset charset;
  std::u32string kanaCodepoints, digitCodepoints;
  Plane bed;
  std::vector<Plane> curtains;
  std::string caption;

  /** A plane read from its entry in the words file, and its text dealt:
   *  as many columns as fit the screen, each ended by a newline after as
   *  many cells as fit its height — measured on the plane's own class, so
   *  the sheet's size is the one the grid is cut for. Every cell shares one
   *  vertical advance whichever class it draws from, so the mixed text
   *  keeps the grid. */
  Plane readPlane(sketch::SketchContext& ctx, const data::Json& entry) const {
    Plane plane{.name = std::string(entry["plane"].text()),
                .seed = (uint32_t)entry["seed"].number(),
                .churnSeconds = (float)entry["churnSeconds"].number(1),
                .churnOffsetSeconds =
                    (float)entry["churnOffsetSeconds"].number(),
                .eachMs = (float)entry["eachMs"].number(),
                .durationMs = (float)entry["durationMs"].number(),
                .loopMs = (float)entry["loopMs"].number()};
    std::string probe;
    for (int cell = 0; cell < 8; ++cell) probe += charset.kana.front();
    const auto column = ctx.measure(text(probe)
                                          .role("plane")
                                          .styleClass(plane.name)
                                          .applyStyleSheet(screen()));
    const int rows =
        std::max(1, (int)std::floor(kHeight / (column.height() / 8)));
    plane.columns =
        std::max(1, (int)std::floor(kWidth / std::max(1.0f, column.width())));
    noise::Mix64Stream random(plane.seed);
    for (int at = 0; at < plane.columns; ++at) {
      if (at > 0) plane.text += '\n';
      for (int row = 0; row < rows; ++row) {
        const auto& set = random.bits() % charset.digitsOneIn == 0
                              ? charset.digits
                              : charset.kana;
        plane.text += set[random.bits() % set.size()];
      }
    }
    return plane;
  }

  /** THE CHURN AND THE LIFT every plane wears: the kana mirrored and the
   *  digits upright, each lifted, each class substituting within its own
   *  charset off the plane's wrapping churn phase. A plane that also falls
   *  states its streak before this, since the tracks are read in the order
   *  they are written. */
  Text churning(Text leaf, const Plane& plane) const {
    const motion::Animatable<float> churn =
        motion::bind(&seconds)
            .scale(1.0f / plane.churnSeconds)
            .offset(plane.churnOffsetSeconds / plane.churnSeconds)
            .wrap(1.0f);
    return leaf.textFx({.where = !digitCells(), .effect = mirroredKana()})
        .textFx({.where = digitCells(), .effect = uprightDigits()})
        .textFx({.where = !digitCells(),
                 .effect = textFx::scramble(kanaCodepoints, 20),
                 .progress = churn})
        .textFx({.where = digitCells(),
                 .effect = textFx::scramble(digitCodepoints, 20),
                 .progress = churn});
  }

  /** One falling curtain. Each column starts at its own seeded rank of a
   *  scrambled even ladder spread across the loop period ((columns − 1) /
   *  columns of it, so the last rank does not fold onto the first); inside
   *  that beat the cluster cascade runs the glyphs down at the plane's
   *  rate; and the whole schedule re-opens every period. Where a column's
   *  ladder outruns the period, successive drops share the column one
   *  period apart, as the screens' do. */
  Element curtain(const Plane& plane) const {
    const float columns = (float)plane.columns;
    motion::Spread cascade{
        .amountMs = plane.loopMs * (columns - 1.0f) / columns,
        .from = motion::Spread::From::Random};
    cascade.seed = plane.seed;
    cascade.then({.eachMs = plane.eachMs, .durationMs = plane.durationMs});
    cascade.loopMs = plane.loopMs;
    return churning(
        text(plane.text)
            .role("plane")
            .styleClass(plane.name)
            .key(plane.name)
            .textFx({.effect = streak(),
                     .stagger = cascade,
                     .unit = weave::Unit::Line,
                     .innerUnit = weave::Unit::Cluster,
                     .progress = motion::bind(&seconds)
                                     .scale(1000.0f / plane.loopMs)
                                     .wrap(1.0f)}),
        plane);
  }

  Element describe() const {
    return stack()
        .role("screen")
        .fill(Fill::color(kVoid))
        .applyStyleSheet(screen())
        .children({
            // The bed: the whole screen faintly alive, never bright and
            // never absent, churning under the curtains.
            churning(text(bed.text)
                         .role("plane")
                         .styleClass(bed.name)
                         .key(bed.name),
                     bed),
            each(curtains, [this](const Plane& plane) {
              return curtain(plane);
            }),
            // THE GLASS over the rain, which never moves: the monitor's
            // falloff toward its corners, and the caption on a ground of
            // the void rising off the bottom edge — the field runs edge to
            // edge and every column churns, so a line laid straight onto
            // it would compete with a moving glyph behind every letter.
            box()
                .cover()
                .key("glass")
                .hitTestable(false)
                .cache(Cache::Texture)
                .children({
                    box().cover().fill(kit::vignette(
                        {kWidth, kHeight}, {0.002f, 0.008f, 0.004f, 0.55f},
                        0.35f)),
                    kit::at(0, kHeight - 52, kWidth, 52)
                        .fill(linearGradient({0, 0}, {0, 52},
                                             {{kVoid.r, kVoid.g, kVoid.b, 0},
                                              {kVoid.r, kVoid.g, kVoid.b, 0.72f},
                                              {kVoid.r, kVoid.g, kVoid.b, 0.92f}},
                                             {0.0f, 0.45f, 1.0f}))
                        .paddingTop(22)
                        .paddingLeft(26)
                        .children({document::caption(caption)}),
                }),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    // Deep into the steady state: fresh heads, long tails, and columns
    // resting dark between drops.
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 7.0,
                             .background = kVoid});
    ctx.ticker.add([this](double, double elapsed) {
      seconds = (float)elapsed;
      return true;
    });

    const sketch::kit::Document words{ctx, "data/rain.json"};
    const data::Json& charsets = words["charsets"];
    charset = {.kana = glyphsOf(charsets["kana"].text()),
               .digits = glyphsOf(charsets["digits"].text()),
               .digitsOneIn = std::max<uint32_t>(
                   1, (uint32_t)charsets["digitsOneIn"].number(7))};
    kanaCodepoints = codepointsOf(charset.kana);
    digitCodepoints = codepointsOf(charset.digits);
    caption = std::string(words["caption"].text());
    if (charset.kana.empty() || charset.digits.empty()) return;
    bed = readPlane(ctx, words["bed"]);
    for (const data::Json& entry : words["curtains"].items())
      curtains.push_back(readPlane(ctx, entry));
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(MatrixRain, "Study · Type",
             "The Matrix's digital rain (1999) — four planes of mirrored "
             "half-width katakana on one sheet and one clock, thousands of "
             "glyphs churning on declared schedules")
