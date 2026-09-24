// axis_ripple.cpp — PATTERN: the weight wave. The demo every variable
// typeface ships with, and the one constraint every version of it hides.
// =============================================================================
// THE PATTERN
//
// The variable-font specimen page: one headline, set to the measure, with a
// WAVE OF WEIGHT travelling along it — each letter riding the same swell a
// fixed beat behind its neighbour, so the line appears to inhale from left
// to right. The word is HAMBURGEFONTSIV, the type designer's proofing
// string, whose letters carry most of the shapes a Latin face must get
// right.
//
// THE CONSTRAINT THE WEB VERSION HIDES
//
// The web demo animates `wght`, and `wght` MOVES ADVANCES: a heavier letter
// is a wider one, so every frame is a fresh line layout and the letters
// slide as the wave passes. This engine drives an axis at DRAW TIME over
// glyphs shaped once, which is sound only where the axis leaves every
// advance alone, so it refuses an axis that moves one. `GRAD` is the axis
// made for exactly this: weight without width, drawn heavier on the same
// skeleton, and the letters stand still.
//
// The lower half is the proof rather than the claim: the word set at both
// ends of each axis, measured through the same cascade the page is set by,
// with a rule where the light run stops. On `wght` the heavy run overhangs
// the rule; on `GRAD` it stops on it.
//
// HOW THE RIPPLE IS SPELLED
//
// A looping cascade: every letter's beat is the same swell over one pass,
// opened one beat after its neighbour's, so the travelling wave is the
// schedule's and the effect is only the swell. Under each letter a bar
// reads the same phase through the same swell, so the meter cannot drift
// from the letters it reports on.
//
// EDIT THESE FIRST
//   kWavesAcross — the wavelength, in waves per word. 1 puts one crest and
//                  one trough on the line, as a specimen page shows.
//   kGradLight / kGradHeavy — the ends of the ramp, inside the face's own
//                  GRAD range of 400 to 1000.
//   kPeriod      — seconds per pass.

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string_view>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kWidth = 1120.0f;
constexpr float kHeight = 620.0f;
constexpr float kPaddingX = 52.0f;
constexpr float kPaddingY = 44.0f;

constexpr material::Color kPaper = hexColor(0x0C0C0E);

/** The proof string: one cluster per letter, so letter i is range [i, i+1). */
constexpr std::string_view kProof = "HAMBURGEFONTSIV";

// ---- the wave -------------------------------------------------------------
constexpr float kGradLight = 400.0f;
constexpr float kGradHeavy = 1000.0f;
constexpr float kWavesAcross = 1.0f;
constexpr float kPeriod = 2.6f;

// ---- the proof ------------------------------------------------------------
constexpr float kWeightLight = 300.0f;
constexpr float kWeightHeavy = 900.0f;

// ---- the meter ------------------------------------------------------------
constexpr float kLevelDrop = 10.0f;
constexpr float kLevelHeight = 34.0f;

/** THE SWELL one pass of the wave is, on a phase in [0, 1): the letter's
 *  own beat into the ramp, as a fraction of its full reach. The text track
 *  and the meter both read it, so they are one curve. */
float swell(float phase) {
  return 0.5f + 0.5f * std::sin(phase * 6.2831853f);
}

/** The ripple's deviation: the swell landing on the grade.
 *
 *  Only an advance-invariant axis is honoured, so every letter keeps the
 *  pen position shaping gave it for the whole ripple; saying so is what
 *  keeps the run on whole-pixel origins and one atlas strike per letter
 *  instead of one per phase. */
TextEffect gradeSwell() {
  return textFx::effect(
             "gradeSwell",
             [](const GlyphInfo&, float local,
                sigil::core::noise::Mix64Stream&) {
               GlyphModifier modifier;
               modifier.axis = weave::FontVariation(
                   "GRAD",
                   kGradLight + (kGradHeavy - kGradLight) * swell(local));
               return modifier;
             },
             0.0f, {kGradLight, kGradHeavy})
      .displacing(false);
}

/** HOW THE SHEET IS SET: the house grotesque everywhere — the one installed
 *  face that carries both a grade to drive and a weight to measure against
 *  it — spaced capitals for the labels, and the specimen type heavy and
 *  bright. The palette is custom properties on the root, read by the roles
 *  and by the two verdict classes. */
StyleSheet look() {
  return StyleSheet{
      rule(":root")
          .var("ink", hexColor(0xF4F1EA))
          .var("label", hexColor(0x7E8492))
          .var("faint", hexColor(0x3A3F4B))
          .var("bed", material::Color{1, 1, 1, 0.04f})
          .var("overhang", hexColor(0xE2504B))
          .var("axis", hexColor(0x63B8FF))
          .fontFamily(".SF NS, SF Pro, system-ui")
          .fontWeight(500)
          .fontSize(11.5f)
          .letterSpacing(0.2f)
          .ink(var("label")),
      rule("h1").fontSize(26).ink(var("ink")),
      rule("masthead caption").fontSize(12).letterSpacing(0.3f),
      rule("eyebrow").fontSize(11.5f).letterSpacing(2.4f),
      rule("caption").fontSize(11).letterSpacing(0.6f),
      rule("label").fontSize(11).letterSpacing(1.6f),
      rule("hero, proof").fontWeight(700).ink(var("ink")),
      rule("hero").letterSpacing(1),
      rule("proof").fontSize(34).letterSpacing(0.6f),
      rule(".refused").ink(var("overhang")),
      rule(".honoured").ink(var("axis")),
  };
}

/** One end of one axis: the proof word at that coordinate. */
Text proofRun(const char (&tag)[5], float value) {
  return text(kProof).role("proof").font(
      {.variations = {weave::FontVariation(tag, value)}});
}

/** The share of a pass each letter's beat opens after the one before it. */
float beatFraction() { return kWavesAcross / (float)kProof.size(); }

}  // namespace

// ===========================================================================

struct AxisRipple {
  choreograph::Output<float> phase{0};
  const StyleSheet sheet = look();

  float heroSize = 0;
  float gradHeroDrift = 0;  // the run's width across the ramp, at the hero size
  float weightLight = 0, weightHeavy = 0;  // the wght proof rows
  float gradLight = 0, gradHeavy = 0;      // the GRAD proof rows

  /** The laid-out width of @p leaf under this sheet, through the cascade
   *  the page is set by.
   *
   *  A passage's measured box is rounded up to whole pixels, and a run's
   *  exact pen positions can be measured only from a style holding a face,
   *  never from the family a sheet names, so every width on this page is
   *  printed to the whole pixel. */
  float widthOf(sketch::SketchContext& ctx, Element leaf) const {
    return ctx.measure(box().applyStyleSheet(sheet).children({std::move(leaf)}))
        .width();
  }

  /** THE METER CELL under letter @p index: the grade it is being drawn at,
   *  as a level rising from the foot of its bar. It hangs off the letter's
   *  own rect, so it is exactly that letter's width, and reads the same
   *  phase through the same swell one beat later per letter. */
  [[nodiscard]] Element level(size_t index) const {
    const float lag = beatFraction() * (float)index;
    return box()
        .role("level")
        .left(0)
        .right(3)
        .top(pct(100))
        .marginTop(kLevelDrop)
        .height(kLevelHeight)
        .fill(Fill::var("bed"))
        .children({box()
                       .cover()
                       .fill(Fill::var("axis"))
                       .transformOrigin(pct(50), pct(100))
                       .scaleY(motion::bind(&phase)
                                   .source(lag, 1.0f + lag)
                                   .wave(&swell)
                                   .target(1.0f / kLevelHeight, 1.0f))});
  }

  /** The ripple: the word to the measure, every letter's grade on one
   *  looping cascade, with its meter hanging beneath it. */
  [[nodiscard]] Element ripple() const {
    const float periodMs = kPeriod * 1000.0f;
    Text hero = text(kProof).role("hero").fontSize(heroSize).key("ripple");
    hero.textFx({.effect = gradeSwell(),
                 .stagger = {.eachMs = beatFraction() * periodMs,
                             .durationMs = periodMs,
                             .loopMs = periodMs},
                 .progress = &phase});
    for (size_t index = 0; index < kProof.size(); ++index)
      hero.textAttach(weave::selectors::range({(uint32_t)index,
                                               (uint32_t)index + 1}),
                      level(index));
    // A mark reserves nothing, so the room under the word is the meter's.
    hero.marginBottom(kLevelDrop + kLevelHeight);
    return box().column().gap(10).children(
        {document::eyebrow("GRAD — DRIVEN AT DRAW TIME, ONE SHAPING, "
                           "LETTERS FIXED"),
         std::move(hero),
         // The face's axis range is not read here: a family's variation
         // axes cannot be asked for without holding its typeface, so the
         // caption states the drive's own ends.
         document::caption(kit::formatted(
             "GRAD %.0f–%.0f · %.0f WAVE ACROSS THE WORD · %.1f S PER PASS · "
             "RUN WIDTH Δ %.0f PX ACROSS THE RAMP",
             kGradLight, kGradHeavy, kWavesAcross, kPeriod, gradHeroDrift))});
  }

  /** One axis, proved: the word at each end of it, left edges aligned, a
   *  rule anchored where the light run stopped, and the verdict in the
   *  class that colours it. */
  [[nodiscard]] static Element axisPanel(const Utf8& heading,
                                         const char (&tag)[5], float light,
                                         float heavy, float lightWidth,
                                         float heavyWidth,
                                         std::string_view verdictClass,
                                         const char* movement,
                                         const char* consequence) {
    // An unsliced selector resolves to the union of every glyph's box, so
    // pct(100) of it is the last letter's trailing edge.
    Text lightRun = proofRun(tag, light);
    lightRun.textAttach(weave::Selector{},
                        kit::line({.length = Dimension(96),
                                   .column = true,
                                   .fill = Fill::var("overhang")})
                            .left(pct(100))
                            .top(0));
    const auto row = [](float value, Element run) {
      return box().row().alignItems(Align::Baseline).gap(14).children(
          {document::label(kit::formatted("%.0f", value)).width(52),
           std::move(run)});
    };
    const float drift = heavyWidth - lightWidth;
    return box().column().gap(12).flexGrow(1).children(
        {document::eyebrow(heading),
         box().column().gap(6).children(
             {row(light, std::move(lightRun)),
              row(heavy, proofRun(tag, heavy))}),
         document::caption(
             kit::formatted("%s THE RUN BY %.0f PX (%.1f%%) · %s", movement,
                            drift,
                            lightWidth > 0 ? 100.0f * drift / lightWidth : 0.0f,
                            consequence))
             .styleClass(std::string(verdictClass))});
  }

  /** The proof, twice: the axis that moves advances beside the axis that
   *  does not. */
  [[nodiscard]] Element proofPanels() const {
    return box()
        .row()
        .gap(44)
        .key("proof")
        .cache(Cache::Texture)
        .children(
            {axisPanel(kit::formatted("wght %.0f → %.0f — A SHAPING AXIS",
                                      kWeightLight, kWeightHeavy),
                       "wght", kWeightLight, kWeightHeavy, weightLight,
                       weightHeavy, "refused", "WIDENS",
                       "EVERY LETTER AFTER THE FIRST MOVES, SO THE DRIVE IS "
                       "REFUSED"),
             axisPanel(kit::formatted("GRAD %.0f → %.0f — A DRAWN AXIS",
                                      kGradLight, kGradHeavy),
                       "GRAD", kGradLight, kGradHeavy, gradLight, gradHeavy,
                       "honoured", "MOVES",
                       "THE HEAVY RUN STOPS ON THE LIGHT ONE'S RULE, SO THE "
                       "DRIVE IS HONOURED")});
  }

  [[nodiscard]] Element describe() const {
    return box()
        .column()
        .padding(kPaddingY, kPaddingX)
        .gap(24)
        .applyStyleSheet(sheet)
        .fill(linearGradient({0, 0}, {0, kHeight},
                             {kPaper, hexColor(0x111116), kPaper},
                             {0.0f, 0.6f, 1.0f}))
        .children({box().role("masthead").column().gap(5).children(
                        {document::h1("The axis ripple"),
                         document::caption("OpenType Font Variations · 2016")}),
                   kit::line({.fill = Fill::var("faint")}), ripple(),
                   proofPanels(), box().flexGrow(1),
                   document::footer(
                       "Grade changes the weight without changing the width. "
                       "The blue row keeps the same pen positions throughout "
                       "the wave.")});
  }

  void setup(sketch::SketchContext& ctx) {
    // A quarter-pass in: the crest is inside the word rather than at
    // either end, so both the ramp up and the ramp down are on the page.
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = kPeriod * 0.79,
                             .background = kPaper});
    if (!ctx.fonts) return;

    // THE HERO IS SET TO THE MEASURE. A run's width is affine in its size,
    // because the tracking is px and does not scale, so two sizes identify
    // the line and the measure is read off it.
    const float measure = kWidth - 2.0f * kPaddingX;
    const auto heroWidth = [&](float size) {
      return widthOf(ctx, text(kProof).role("hero").fontSize(size));
    };
    const float small = heroWidth(32), large = heroWidth(64);
    heroSize = 32 + (measure - small) * 32 / (large - small);

    const auto gradHero = [&](float value) {
      return widthOf(ctx, text(kProof).role("hero").fontSize(heroSize).font(
                              {.variations = {weave::FontVariation("GRAD",
                                                                   value)}}));
    };
    gradHeroDrift = gradHero(kGradHeavy) - gradHero(kGradLight);
    weightLight = widthOf(ctx, proofRun("wght", kWeightLight));
    weightHeavy = widthOf(ctx, proofRun("wght", kWeightHeavy));
    gradLight = widthOf(ctx, proofRun("GRAD", kGradLight));
    gradHeavy = widthOf(ctx, proofRun("GRAD", kGradHeavy));

    ctx.ticker.add([this, &ticker = ctx.ticker] {
      phase = motion::phase(ticker.elapsed(), kPeriod);
    });

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(AxisRipple, "Study · Type",
             "The variable-font weight wave — driven on GRAD, with the "
             "wght advance drift measured and printed beside it")
