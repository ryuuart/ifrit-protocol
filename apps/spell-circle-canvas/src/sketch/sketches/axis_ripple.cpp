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
// A looping cascade with a rest in it: every letter's beat is the same
// swell, opened one short beat after its neighbour's, and the swell fills
// only the front of the pass. So a crest a few letters wide enters at the
// H, rolls through to the V, and leaves the line standing light and cool
// until the next pass — a breath, then a pause, rather than a line that
// never settles. Under each letter a bar reads the same phase through the
// same swell, so the meter cannot drift from the letters it reports on.
//
// THE LIGHT THE CREST CARRIES
//
// Grade is weight without width, and the page lets it read as light: a
// letter at rest is set dim and cool, a letter on the crest full and warm,
// through the same swell that drives its grade, and a warm pool travels
// behind the word with the crest. The ground is a dark sheet lit from above
// the specimen line and darkened toward the corners;
// the proofs stand on two plates, and the run that overhangs its rule has
// the overhang itself struck through.
//
// EDIT THESE FIRST
//   kBeat        — seconds between one letter's swell and the next's; the
//                  crest's speed along the word.
//   kSwellSeconds — how long one letter's swell lasts; with kBeat, how
//                  many letters the crest spans.
//   kGradLight / kGradHeavy — the ends of the ramp, inside the face's own
//                  GRAD range of 400 to 1000.
//   kPeriod      — seconds per pass, the rest included.

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
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
#include <string>
#include <string_view>

namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

constexpr float kWidth = 1120.0f;
constexpr float kHeight = 620.0f;
constexpr float kPaddingX = 52.0f;
constexpr float kPaddingY = 38.0f;

constexpr material::Color kPaper = hexColor(0x0C0C0E);
constexpr material::Color kRefused = hexColor(0xE2504B);

/** The proof string: one cluster per letter, so letter i is range [i, i+1). */
constexpr std::string_view kProof = "HAMBURGEFONTSIV";

// ---- the wave -------------------------------------------------------------
constexpr float kGradLight = 400.0f;
constexpr float kGradHeavy = 1000.0f;
constexpr float kBeat = 0.085f;
constexpr float kSwellSeconds = 0.62f;
constexpr float kPeriod = 3.2f;
/** Where in its swell a letter is heaviest: the swell climbs quicker than
 *  it falls, so the crest leans forward the way it travels. */
constexpr float kCrestAt = 0.38f;

// ---- the light the crest carries -----------------------------------------
/** The channel multipliers over the ink: a letter at rest is dim and cool,
 *  a letter on the crest full and warm. They are light on the ink the sheet
 *  sets, not colours of their own. */
constexpr material::Color kRestLight = {0.52f, 0.56f, 0.66f, 1.0f};
constexpr material::Color kCrestLight = {1.0f, 0.94f, 0.82f, 1.0f};
constexpr float kPoolWidth = 460.0f;
constexpr float kPoolHeight = 250.0f;

// ---- the proof ------------------------------------------------------------
constexpr float kWeightLight = 300.0f;
constexpr float kWeightHeavy = 900.0f;

// ---- the meter ------------------------------------------------------------
constexpr float kLevelDrop = 10.0f;
constexpr float kLevelHeight = 34.0f;

/** THE SWELL one pass of the wave is, on a letter's own phase in [0, 1):
 *  its reach into the ramp. It rises eased to the crest, falls eased back,
 *  and rests at nothing for the rest of the pass. The text track and the
 *  meter both read it, so they are one curve. */
float swell(float phase) {
  const float along = phase * kPeriod / kSwellSeconds;
  if (along <= 0.0f || along >= 1.0f) return 0.0f;
  constexpr float kHalfTurn = 3.14159265f;
  if (along < kCrestAt)
    return 0.5f - 0.5f * std::cos(kHalfTurn * along / kCrestAt);
  return 0.5f + 0.5f * std::cos(kHalfTurn * (along - kCrestAt) /
                                (1.0f - kCrestAt));
}

/** The ripple's deviation: the swell landing on the grade, and the light
 *  the grade is read by.
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
               const float reach = swell(local);
               GlyphModifier modifier;
               modifier.axis = weave::FontVariation(
                   "GRAD", kGradLight + (kGradHeavy - kGradLight) * reach);
               // The light is a quantity, so the two ends mix in linear
               // light: a letter half way up the swell is already bright.
               modifier.colorMultiplier =
                   material::mixLinear(kRestLight, kCrestLight, reach);
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
          .var("bed", material::Color{1, 1, 1, 0.035f})
          .var("refused", kRefused)
          .var("refused-wash", material::withAlpha(kRefused, 0.10f))
          .var("refused-hatch", material::withAlpha(kRefused, 0.55f))
          .var("honoured", hexColor(0x63B8FF))
          .fontFamily(".SF NS, SF Pro, system-ui")
          .fontWeight(500)
          .fontSize(11.5f)
          .letterSpacing(0.2f)
          .ink(var("label")),
      rule("h1").fontSize(28).fontWeight(600).letterSpacing(-0.3f).ink(
          var("ink")),
      rule("masthead caption").fontSize(12).letterSpacing(0.3f),
      rule("eyebrow").fontSize(11.5f).letterSpacing(2.4f),
      rule("caption").fontSize(11).letterSpacing(0.6f),
      rule("label").fontSize(11).letterSpacing(1.6f),
      rule("footer").fontSize(11.5f).letterSpacing(0.2f).width(700),
      rule("hero, proof").fontWeight(700).ink(var("ink")),
      rule("hero").letterSpacing(1),
      rule("proof").fontSize(34).letterSpacing(0.6f),
      rule(".refused").ink(var("refused")),
      rule(".honoured").ink(var("honoured")),
      // A proof stands on a plate lit from its top edge.
      rule(".plate").padding(16, 22).borderRadius(12).fill(
          material::Paint::linearGradient(
              {0, 0}, {0, 190},
              {material::Color{1, 1, 1, 0.055f},
               material::Color{1, 1, 1, 0.012f}},
              {.units = material::GradientUnits::Pixels})),
  };
}

/** One end of one axis: the proof word at that coordinate. */
Text proofRun(const char (&tag)[5], float value) {
  return text(kProof).role("proof").font(
      {.variations = {weave::FontVariation(tag, value)}});
}

/** A difference of two measured widths, in the words its resolution allows.
 *  Both widths are whole pixels rounded up, so equal widths say only that
 *  the runs differ by less than a pixel, and unequal ones are right to the
 *  whole pixel and no finer. */
std::string toTheWholePixel(float drift) {
  if (std::fabs(drift) < 0.5f) return "LESS THAN 1 PX";
  return kit::formatted("%.0f PX, TO THE WHOLE PIXEL", drift);
}

/** The share of a pass each letter's beat opens after the one before it. */
float beatFraction() { return kBeat / kPeriod; }

/** The share of a pass at which letter @p index stands on the crest. */
float crestPhase(size_t index) {
  return ((float)index * kBeat + kCrestAt * kSwellSeconds) / kPeriod;
}

}  // namespace

// ===========================================================================

struct AxisRipple {
  choreograph::Output<float> phase{0};
  const StyleSheet sheet = look();

  float heroSize = 0;
  float heroWidth = 0;  // the measure the hero is set to
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
   *  phase through the same swell one beat later per letter.
   *
   *  A mark's margin is not read, and its insets take no sum, so "the
   *  letter's foot plus the drop" has no placement that says it.
   *  workaround: the cell stands at the foot and a constant translate
   *  carries it down the rest of the way. */
  [[nodiscard]] Element level(size_t index) const {
    const float lag = beatFraction() * (float)index;
    return box()
        .role("level")
        .left(0)
        .right(3)
        .top(pct(100))
        .translateY(kLevelDrop)
        .height(kLevelHeight)
        .fill(Fill::var("bed"))
        .children({box()
                       .cover()
                       // Deep at the foot and hot at the top, so a full bar
                       // reads as a lit column and a low one as embers.
                       .fill(material::Paint::linearGradient(
                           {0, 0}, {0, kLevelHeight},
                           {{0.0f, hexColor(0xD6ECFF)},
                            {0.35f, hexColor(0x63B8FF)},
                            {1.0f, hexColor(0x1C3F70)}},
                           {.units = material::GradientUnits::Pixels}))
                       .transformOrigin(pct(50), pct(100))
                       .scaleY(motion::bind(&phase)
                                   .source(lag, 1.0f + lag)
                                   .wave(&swell)
                                   .target(1.0f / kLevelHeight, 1.0f))});
  }

  /** THE WARM POOL behind the word, travelling with the crest.
   *
   *  A letter's pen position cannot be read from a family a sheet names, so
   *  the pool walks the word at its mean letter width: it runs level with
   *  the crest across the word and may lead or trail it by part of a
   *  letter where the letters are wide or narrow. It rises as the crest
   *  enters at the H and fades as it leaves the V, so the rest between
   *  passes is dark. */
  [[nodiscard]] Element pool() const {
    const float letter = heroWidth / (float)kProof.size();
    const float first = crestPhase(0), last = crestPhase(kProof.size() - 1);
    const material::Color warm = hexColor(0xFFC27A);
    return box()
        .absolute()
        .left(0)
        .top(heroSize * 0.62f - kPoolHeight * 0.5f)
        .width(kPoolWidth)
        .height(kPoolHeight)
        .fill(material::Paint::radialGradient(
            {kPoolWidth * 0.5f, kPoolHeight * 0.5f}, kPoolWidth * 0.5f,
            {{0.0f, material::withAlpha(warm, 0.16f)},
             {0.45f, material::withAlpha(warm, 0.05f)},
             {1.0f, material::withAlpha(warm, 0.0f)}},
            {.units = material::GradientUnits::Pixels}))
        .scaleY(0.62f)
        .translateX(motion::bind(&phase)
                        .window(first, last)
                        .target(letter * 0.5f - kPoolWidth * 0.5f,
                                heroWidth - letter * 0.5f - kPoolWidth * 0.5f))
        .opacity(motion::bind(&phase).trapezoid(
            first - 2.0f * beatFraction(), first + beatFraction(),
            last - beatFraction(), last + 3.0f * beatFraction()));
  }

  /** The ripple: the word to the measure, every letter's grade on one
   *  looping cascade, with its meter hanging beneath it and the warm pool
   *  behind it. */
  [[nodiscard]] Element ripple() const {
    const float periodMs = kPeriod * 1000.0f;
    Text hero = text(kProof).role("hero").fontSize(heroSize).key("ripple");
    hero.textFx({.effect = gradeSwell(),
                 .stagger = {.eachMs = kBeat * 1000.0f,
                             .durationMs = periodMs,
                             .loopMs = periodMs},
                 .progress = &phase});
    for (size_t index = 0; index < kProof.size(); ++index)
      hero.textAttach(weave::selectors::range({(uint32_t)index,
                                               (uint32_t)index + 1}),
                      level(index));
    return box().column().gap(10).children(
        {document::eyebrow("GRAD — DRIVEN AT DRAW TIME, ONE SHAPING, "
                           "LETTERS FIXED"),
         // A mark reserves nothing, so the room under the word is the
         // meter's, held against the page's full column so the proofs
         // below cannot squeeze the meter back under the caption.
         box()
             .paddingBottom(kLevelDrop + kLevelHeight)
             .flexShrink(0)
             .children({pool(), std::move(hero)}),
         // The face's axis range is not read here: a family's variation
         // axes cannot be asked for without holding its typeface, so the
         // caption states the drive's own ends.
         document::caption(kit::formatted(
             "GRAD %.0f–%.0f · A %.2f S SWELL, %.0f MS A LETTER · %.1f S A "
             "PASS · THE RUN MOVES %s ACROSS THE RAMP",
             kGradLight, kGradHeavy, kSwellSeconds, kBeat * 1000.0f, kPeriod,
             toTheWholePixel(gradHeroDrift).c_str()))});
  }

  /** One axis, proved, on its plate: the word at each end of it, left
   *  edges aligned, a rule anchored where the light run stopped, whatever
   *  the heavy run sets past that rule washed and struck through, and the
   *  verdict in the class that colours all three. */
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
                                   .fill = Fill::var(verdictClass)})
                            .left(pct(100))
                            .top(0));
    const float drift = heavyWidth - lightWidth;
    Text heavyRun = proofRun(tag, heavy);
    if (drift >= 1.0f)
      heavyRun.textAttach(
          weave::Selector{},
          box()
              .left(Dimension(lightWidth))
              .right(0)
              .top(0)
              .bottom(0)
              .fill(Fill::var("refused-wash"))
              .background(lines::presets::hatch(Fill::var("refused-hatch"),
                                                5.0f, 1.0f, -55.0f)));
    const auto row = [](float value, Element run) {
      return box().row().alignItems(Align::Baseline).gap(14).children(
          {document::label(kit::formatted("%.0f", value)).width(52),
           std::move(run)});
    };
    return box().column().gap(12).flexGrow(1).flexBasis(0).styleClass("plate").children(
        {document::eyebrow(heading),
         box().column().gap(6).children(
             {row(light, std::move(lightRun)),
              row(heavy, std::move(heavyRun))}),
         document::caption(
             kit::formatted("%s THE RUN BY %s · %s", movement,
                            toTheWholePixel(drift).c_str(), consequence))
             .styleClass(std::string(verdictClass))});
  }

  /** The proof, twice: the axis that moves advances beside the axis that
   *  does not. */
  [[nodiscard]] Element proofPanels() const {
    return box()
        .row()
        .gap(20)
        .key("proof")
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

  /** THE GROUND, the page's one texture: a dark sheet lit from above the
   *  specimen line and darkened toward the corners. Nothing on it moves,
   *  so it is baked once and blitted under everything that does.
   *
   *  The sheet asks for a fine grain, but `kit::grained` puts no grain on
   *  a near-black ground: it folds its noise in by soft light, whose
   *  change on a dark destination stays under one 8-bit level, so this
   *  fill reads as the flat paper until the kit's grain holds its
   *  strength on a dark ground. */
  [[nodiscard]] static Element ground() {
    const material::Color skylight = {0.36f, 0.42f, 0.58f, 1.0f};
    return box()
        .cover()
        .key("ground")
        .cache(Cache::Texture)
        .fill(kit::grained(kPaper, 0.07f, 0.85f))
        .children({box().cover().fill(material::Paint::radialGradient(
                       {kWidth * 0.5f, kHeight * 0.36f}, kWidth * 0.62f,
                       {{0.0f, material::withAlpha(skylight, 0.13f)},
                        {0.5f, material::withAlpha(skylight, 0.04f)},
                        {1.0f, material::withAlpha(skylight, 0.0f)}},
                       {.units = material::GradientUnits::Pixels})),
                   box().cover().fill(kit::vignette({kWidth, kHeight},
                                                    {0, 0, 0, 0.55f}, 0.4f))});
  }

  [[nodiscard]] Element describe() const {
    return box()
        .column()
        .padding(kPaddingY, kPaddingX)
        .gap(20)
        .applyStyleSheet(sheet)
        .fill(kPaper)
        .children({ground(), box().role("masthead").column().gap(5).children(
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
    // The crest on the word's eighth letter: the rise ahead of it and the
    // fall behind it are both on the page, with the line at rest either
    // side.
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = crestPhase(7) * kPeriod,
                             .background = kPaper});
    if (!ctx.fonts) return;

    // THE HERO IS SET TO THE MEASURE. A run's width is affine in its size,
    // because the tracking is px and does not scale, so two sizes identify
    // the line and the measure is read off it.
    const float measure = kWidth - 2.0f * kPaddingX;
    heroWidth = measure;
    const auto widthAtSize = [&](float size) {
      return widthOf(ctx, text(kProof).role("hero").fontSize(size));
    };
    const float small = widthAtSize(32), large = widthAtSize(64);
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
