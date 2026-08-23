// rota_convocationis.cpp — A MAGIC CIRCLE THAT ASSEMBLES ITSELF: an invented
// conjuring wheel in the real idiom of the early-modern diagrams, run as the
// text engine's heaviest single sheet — fourteen curved baselines forming,
// orbiting and charging on one declared schedule.
// =============================================================================
// SUBJECT  The magical diagram as a CONSTRUCTION SEQUENCE. The historical
//          circles are not pictures, they are procedures — a compass strikes
//          the rules, a pen writes the names round the band, the figures are
//          inscribed in a stated order — and this study performs that order
//          rather than presenting its result: concentric rings of lettering
//          form in turn, the eye is handed from roundel to roundel, and the
//          finished wheel turns. It is an INVENTED sigil built from
//          documented parts; it reconstructs no specific artefact and claims
//          none.
//
// -----------------------------------------------------------------------------
// FROM THE RECORD
//
//   * The CONCENTRIC BAND OF NAMES between two rules, interrupted by
//     crosses, is the standard first circle of the Solomonic manuscripts
//     (the Clavicula Salomonis tradition): divine names written round the
//     rim with a cross drawn between phrases. The nine names on this
//     wheel's middle ring — EL, ELOHIM, ELOHE, ZEBAOTH, ELION, ESCERCHIE,
//     IAH, TETRAGRAMMATON, SADAI — are that tradition's own list.
//   * SATELLITE ROUNDELS attached to the main figure — small circles
//     bearing their own inscriptions — are how the Solomonic circles carry
//     their pentacles and how the printed grimoires set their planetary
//     seals.
//   * The SEVEN PLANETARY ANGELS and the MAGIC SQUARES are Agrippa's (De
//     occulta philosophia, book II): Saturn–Cassiel, Jupiter–Sachiel,
//     Mars–Samael, Sol–Michael, Venus–Anael, Mercury–Raphael,
//     Luna–Gabriel; and to each planet a square — Saturn the 3×3, Jupiter
//     4, Mars 5, Sol 6, Venus 7, Mercury 8, Luna 9. The Sun's 6×6 square
//     at this wheel's hub is Agrippa's own table, every row, column and
//     diagonal summing to 111.
//   * The minuscule ring is the Latin of the TABULA SMARAGDINA as the
//     printed alchemical corpus carries it: "quod est inferius est sicut
//     quod est superius…", the sentence the rotae quote when they want the
//     above bound to the below.
//   * INSCRIBED STAR POLYGONS with lettering along their chords are the
//     idiom of the seals and of Llull's combinatorial figures — lines
//     joining lettered stations so that reading becomes traversal.
//
// THIS STUDY'S OWN, flagged rather than smuggled:
//   * The wheel itself: this arrangement of those parts exists on no
//     recorded plate. The invocation round the rim is this study's Latin,
//     written for the band ("+ IN PRINCIPIO SCRIBITVR CIRCVLVS…"); the
//     {6/2} hexagram, the day-name captions on its chords, Sol's square at
//     the hub with the six other planets on the points, and every colour,
//     radius and millisecond are composition, not record.
//   * The ASSEMBLY is the subject and entirely invented: no manuscript
//     animates. The order performed here — rules, rim, names, tablet,
//     chords, roundels, square, charge — is a plausible working order, not
//     a documented one.
//
// -----------------------------------------------------------------------------
// THE MACHINE, and what it puts under load
//
//   * FOURTEEN TEXT-ON-PATH LEAVES at once: three full rings, six chord
//     captions on one six-contour baseline addressed by (k+0.5)/6, and six
//     roundel rings — each shaped once and re-placed per frame wherever
//     its `at` phase moves. Ring sizes are FITTED, not guessed: each ring
//     string is measured straight and its type sized so the run girds its
//     own circumference.
//   * THE SCHEDULE IS DECLARED, THEN READ BACK, never restated: every
//     stage's window is computed from `Stagger::spanMs` at declare time —
//     the roundels chain each start off the span of the cascade before it
//     — and the scribe point that leads the rim's writing is placed every
//     frame from `Composer::beatsOf`, so the dot and the letters cannot
//     disagree about where the pen is.
//   * THE RIM'S CASCADE IS A CUE TABLE: one start per word, stepping at
//     writing pace and PAUSING at every cross — irregular timing no even
//     spread expresses — with a cluster cascade nested inside each word's
//     beat and `fx::hold` vetoing every glyph whose beat has not opened.
//   * THE CHARGE IS ONE PASS, not per-glyph paint: `fx::pass` renders the
//     nine names into a layer and one SkSL pass blooms each name on its
//     own cascade clock, riding a baseline that is itself a moving
//     marquee — the pass, the marquee and the glyphs all read one
//     resolved schedule.
//   * MIXED REGISTERS on one wheel: majuscule rings on the grotesque that
//     carries GRAD (so the ignition can swell the names' weight with no
//     reshape), the tablet in serif italic minuscule, numerals mono and
//     scramble-decoded — the square resolves line by line under a nested
//     cascade, each digit churning until its beat closes.
//   * THE LOAD, deliberately: per-glyph alpha, rotation, colour multiplier
//     and axis coordinate all varying at once across ~1,300 glyphs on
//     curved baselines (the quantisation ladder and the atlas), six
//     roundels' rings counter-orbiting while their polygons spin (bound
//     transforms over cached content), and — for the charge's own window
//     — a full-band shader pass on every frame it rides the tree.
//
// EDIT THESE FIRST
//   kStepMs / kCrossMs — the writing pace of the rim, and the scribe's
//               pause at each cross. The whole timeline downstream moves
//               with them, because every later window is chained off this
//               cascade's span.
//   kSatBeat  — how much of a roundel's forming overlaps the next one's
//               start. 1.0 is strictly one at a time; lower hands the eye
//               on earlier.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/rota_convocationis.cpp \
//       --frame /tmp/rota_convocationis.png --at 13.0
//
//   ~1.0 s  the compass: rules and the division ladder sweep on
//   ~3.0 s  the rim mid-write, the scribe point leading the letters
//   ~6.5 s  the nine names risen; the tablet's minuscule sliding in
//   ~9.0 s  the hexagram struck, day names writing chord by chord
//  ~13.0 s  the roundel cycle: three lit, one forming, two dark
//  ~19.0 s  the square of the Sun resolving at the hub
//  ~21.5 s  ignition — the charge runs the names, the wheel turns
//  loops on a dark sheet.

#include <include/core/SkFontStyle.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilsketch/Sketch.h>
#include <sigilweave/Style.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

namespace {

constexpr float kW = 1600.0f;
constexpr float kH = 1000.0f;

// ---- palette: a grimoire by candlelight ----------------------------------
constexpr SkColor4f kNight = studio::hex(0x0A0812);
constexpr SkColor4f kNightLift = studio::hex(0x141021);
constexpr SkColor4f kGold = studio::hex(0xD8A94E);
constexpr SkColor4f kBone = studio::hex(0xE9DFC8);
constexpr SkColor4f kEmber = studio::hex(0x8A4A26);
constexpr SkColor4f kIron = studio::hex(0x3B3554);     // construction lines
constexpr SkColor4f kIronDim = studio::hex(0x262238);  // faint construction
constexpr SkColor4f kAsh = studio::hex(0x8A8299);      // secondary type
constexpr SkColor4f kAshDim = studio::hex(0x8A8299, 0.62f);
constexpr SkColor4f kVerdigris = studio::hex(0x6FA08C);  // the tablet's ink

// ---- the wheel's frame ----------------------------------------------------
constexpr SkPoint kEye{500.0f, 500.0f};  // wheel centre in canvas px
constexpr float kR = 430.0f;             // the greatest circle

// band radii, in units of the greatest circle
constexpr float rRimOut = 1.000f;   // outer rule
constexpr float rCorona = 0.952f;   // the invocation's baseline
constexpr float rRimIn = 0.910f;    // inner rule of the rim band
constexpr float rTickOut = 0.905f;  // the division ladder
constexpr float rTickIn = 0.878f;
constexpr float rNomina = 0.840f;  // the nine names' baseline
constexpr float rNomIn = 0.775f;   // rule under the names
constexpr float rGloss = 0.742f;   // the tablet's minuscule baseline
constexpr float rHex = 0.520f;     // hexagram vertices; roundel centres
constexpr float rHub = 0.245f;     // rule round the square of the Sun
constexpr float kSatR = 76.0f;     // roundel outer radius, px
constexpr float kSatRing = 61.0f;  // roundel ring-text baseline radius, px

// ---- the writing pace -----------------------------------------------------
constexpr float kStepMs = 55.0f;    // word to word round the rim
constexpr float kCrossMs = 190.0f;  // the scribe's extra pause at a cross
constexpr float kSatBeat = 0.88f;   // fraction of a roundel's span before
                                    // the next one starts forming

constexpr float kDeg = 3.14159265358979f / 180.0f;

// ---- content, from the record ---------------------------------------------

/** The first circle's names, in the Solomonic list's own order. */
const char* kNames[9] = {"EL",      "ELOHIM",         "ELOHE",
                         "ZEBAOTH", "ELION",          "ESCERCHIE",
                         "IAH",     "TETRAGRAMMATON", "SADAI"};

/** The tablet's Latin, as the printed corpus carries it. */
const char* kSmaragdina =
    "quod est inferius est sicut quod est superius \xc2\xb7 et quod est "
    "superius est sicut quod est inferius \xc2\xb7 ad perpetranda miracula "
    "rei vnius \xc2\xb7 ";

/** Agrippa's planetary angels and square orders, six about one: Sol keeps
 *  the hub, the remaining six take the hexagram's points clockwise from
 *  the top — this study's arrangement of his table. */
struct Planet {
  const char* planet;
  const char* angel;
  const char* ordo;  // the square's order, as the roundel writes it
  int order;         // …and as a number: the spinning polygon's sides
  const char* dies;  // the chord caption toward this planet's point
};
constexpr Planet kPlanets[6] = {
    {"SATVRNVS", "CASSIEL", "III", 3, "DIES SATVRNI"},
    {"IVPPITER", "SACHIEL", "IIII", 4, "DIES IOVIS"},
    {"MARS", "SAMAEL", "V", 5, "DIES MARTIS"},
    {"VENVS", "ANAEL", "VII", 7, "DIES VENERIS"},
    {"MERCVRIVS", "RAPHAEL", "VIII", 8, "DIES MERCVRII"},
    {"LVNA", "GABRIEL", "IX", 9, "DIES LVNAE"},
};

/** Agrippa's square of the Sun: 6×6, every row, column and diagonal 111.
 *  Single digits padded with a space — the face is monospaced, so a space
 *  is exactly one digit wide and the columns stand. */
const char* kSolSquare =
    " 6 32  3 34 35  1\n"
    " 7 11 27 28  8 30\n"
    "19 14 16 15 23 24\n"
    "18 20 22 21 17 13\n"
    "25 29 10  9 26 12\n"
    "36  5 33  4  2 31";

/** The invocation — this study's own Latin, in the band-broken-by-crosses
 *  form. Eight phrases; the leading cross closes the ring at the seam. */
const char* kInvocatio[8] = {
    "IN PRINCIPIO SCRIBITVR CIRCVLVS", "LITTERA SVRGIT IN ORBEM",
    "NOMINA IN CORONA FRANGVNTVR",     "SEPTEM SIGNA CONVOCANTVR",
    "ROTA IN ROTIS VOLVITVR",          "NVMERVS IN QVADRATO CANIT",
    "ORDO EX ORDINE NASCITVR",         "ET SIGILLVM VIVIT"};

// ---- the charge -----------------------------------------------------------

/** THE CHARGE, against the pass contract: `uContent`, `uUnitRect`,
 *  `uUnitPhase` and `kUnitCount` arrive from the runtime; everything else
 *  is this material's own. Each unit is one NAME on the ring. A name's
 *  flash is sin(π·phase) — nothing before its beat, white-gold at the
 *  crest, settled after — lifting the letters' own pixels and laying a
 *  soft radial wash around the unit's box, so the charge visibly RUNS the
 *  ring as the cascade opens name after name. At every phase 0 the pass
 *  is an exact pass-through, which is what lets it stay mounted for the
 *  whole loop. */
constexpr const char* kChargeSksl = R"(
uniform float4 uGold;
half4 main(float2 xy) {
  half4 c = uContent.eval(xy);
  float flash = 0.0;
  float wash = 0.0;
  for (int i = 0; i < kUnitCount; ++i) {
    float4 r = uUnitRect[i];
    float f = sin(clamp(uUnitPhase[i].x, 0.0, 1.0) * 3.14159265);
    float inside = step(r.x, xy.x) * step(xy.x, r.x + r.z) *
                   step(r.y, xy.y) * step(xy.y, r.y + r.w);
    flash = max(flash, f * inside);
    float2 mid = float2(r.x + r.z * 0.5, r.y + r.w * 0.5);
    float2 q = (xy - mid) / max(float2(r.z, r.w) * 0.85, float2(1.0));
    wash = max(wash, f * exp(-dot(q, q) * 2.6));
  }
  float cov = float(c.a);
  float a = clamp(cov + 0.42 * wash, 0.0, 1.0);
  float3 col = float3(c.rgb) * (1.0 + 2.1 * flash) +
               uGold.rgb * (0.55 * flash * cov + 0.30 * wash);
  return half4(half3(min(col, float3(a))), half(a));
})";

// ---- helpers --------------------------------------------------------------

/** Wheel-frame polar → canvas px. θ clockwise from 12 o'clock, the
 *  direction every band on this wheel is written in. */
SkPoint P(float thDeg, float rNorm) {
  const float a = thDeg * kDeg;
  return {kEye.x() + rNorm * kR * std::sin(a),
          kEye.y() - rNorm * kR * std::cos(a)};
}

int glyphsOf(const std::string& s) {
  int n = 0;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = (unsigned char)s[i];
    const int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (c != ' ' && c != '\n') ++n;
    i += (size_t)len;
  }
  return n;
}

}  // namespace

// ===========================================================================

struct RotaConvocationis : sigil::compose::sketch::Sketch {
  // The two hand-stepped scalars, plus the drifts and spins that are pure
  // shapes of them. Everything scheduled is a window on `cycle`.
  ch::Output<float> cycle{0};  // seconds within one loop, wrapping
  ch::Output<float> secs{0};   // monotonic — breaths and orbits

  ch::Output<float> coronaDrift{0};    // rim marquee, after ignition
  ch::Output<float> nominaDrift{0};    // the names' counter-orbit
  ch::Output<float> satDrift[6] = {};  // each roundel's ring orbit
  ch::Output<float> satSpin[6] = {};   // each roundel's polygon
  ch::Output<float> scribeX{0}, scribeY{0}, scribeA{0};

  sk_sp<SkTypeface> faceRing, faceRingBold, faceTitle, faceItal, faceMono;

  // Fitted content: each ring's text and the size that girds its band.
  std::string coronaText, nominaText, glossText;
  std::string satText[6];
  float coronaSize = 18, nominaSize = 30, glossSize = 15, satSize[6] = {};
  std::vector<float> coronaCues;  // one start per word; pauses at crosses
  int coronaWords = 0, coronaMaxWord = 1;

  // The computed timeline, seconds. Every value is chained from a span.
  double tBand = 0, tNames = 0, tGloss = 0, tHex = 0, capAt[6] = {},
         tSat[6] = {}, tSquare = 0, tIgnite = 0, loopSecs = 30;
  double lastElapsed = 0;    // the scribe's decay reads real dt
  bool passMounted = false;  // the charge pass rides only its own window
  float bandSpanS = 0, namesSpanS = 0, glossSpanS = 0, capSpanS = 0,
        satSpanS = 0, squareSpanS = 0;

  int totalGlyphs = 0;

  // ------------------------------------------------------------------
  // schedule verbs

  /** A beat on the wheel's timeline, in seconds of the loop: clamped
   *  outside its range, so an unstarted track reads 0 and a finished one
   *  reads 1, and the whole assembly re-performs on the wrap. */
  [[nodiscard]] Animatable<float> beat(double from, double to) {
    return bind(&cycle).window((float)from, (float)to);
  }
  /** A one-shot swell inside the loop — up, held, gone — for the flashes
   *  ignition throws on things that are not text. */
  [[nodiscard]] Animatable<float> pulse(double from, double to, double edge) {
    const float l = (float)loopSecs;
    return bind(&cycle)
        .source(0.0f, l)
        .trapezoid((float)from / l, (float)(from + edge) / l,
                   (float)(to - edge * 1.6) / l, (float)to / l)
        .map(&ch::easeInOutQuad);
  }
  /** The sheet's own envelope: up at the head, out before the wrap, so
   *  the loop cuts on a dark sheet. */
  [[nodiscard]] Animatable<float> envelope() {
    const float l = (float)loopSecs;
    return bind(&cycle)
        .source(0.0f, l)
        .trapezoid(0.05f / l, 0.55f / l, (l - 1.9f) / l, (l - 0.55f) / l)
        .map(&ch::easeInOutQuad);
  }

  // ------------------------------------------------------------------
  // type

  [[nodiscard]] sigil::weave::TextStyle ring(float size, SkColor4f color,
                                             float track = 3.0f) const {
    return studio::type(
        {.face = faceRingBold, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle ital(float size, SkColor4f color,
                                             float track = 0.6f) const {
    return studio::type(
        {.face = faceItal, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle mono(float size, SkColor4f color,
                                             float track = 1.0f) const {
    return studio::type(
        {.face = faceMono, .size = size, .color = color, .track = track});
  }
  [[nodiscard]] sigil::weave::TextStyle label(float size, SkColor4f color,
                                              float track = 2.6f) const {
    return studio::type(
        {.face = faceRing, .size = size, .color = color, .track = track});
  }

  /** The size at which @p s girds a circle of radius @p radius: measured
   *  straight, refined once because tracking is px and does not scale
   *  with the type. */
  [[nodiscard]] float fitToRing(sigil::compose::sketch::SketchContext& ctx,
                                const std::string& s,
                                const sigil::weave::TextStyle& style,
                                float radius, float fill = 0.985f) {
    const float target = 2.0f * 3.14159265f * radius * fill;
    float size = style.shaping.fontSize;
    for (int pass = 0; pass < 2; ++pass) {
      sigil::weave::TextStyle probe = style;
      probe.shaping.fontSize = size;
      const SkSize m = ctx.measure(text(toU8(s), probe));
      if (m.width() > 1.0f) size *= target / m.width();
    }
    return size;
  }

  // ------------------------------------------------------------------
  // the wheel

  /** A stroked circle of the wheel, revealed as an arc sweep — the
   *  compass stroke itself. */
  [[nodiscard]] Element rule(const char* key, float rNorm, float width,
                             SkColor4f color, double from, double dur) {
    return disc(kEye, rNorm * kR)
        .key(key)
        .corners({rNorm * kR})
        .fill(Fill::none())
        .stroke(spans::upTo(beat(from, from + dur)),
                util::stroke(width, Fill::color(color)));
  }

  /** The rim's cascade: one cue per word — writing pace, with the pen
   *  resting at every cross — and the letters of each word beating inside
   *  its cue. Built as a named value because `then` mutates in place. */
  [[nodiscard]] Stagger coronaCascade() const {
    Stagger cascade = stagger(unit::Word, cues(coronaCues));
    cascade.then(unit::Cluster, {.eachMs = 22, .durationMs = 300});
    return cascade;
  }

  /** THE RIM — the invocation between the two outermost rules. Its
   *  entrance composes with the baseline: the hold vetoes a glyph until
   *  its beat, the rise then lifts it onto the ring along the ring's own
   *  local perpendicular, and the tint warms it from ember to bone as it
   *  lands. After ignition the whole run becomes a slow marquee. */
  [[nodiscard]] Element corona() {
    return text(toU8(coronaText), ring(coronaSize, kBone, 2.2f))
        .key("corona")
        .centerAt(kEye)
        .width(2 * rCorona * kR)
        .height(2 * rCorona * kR)
        .onPath({.path = shapes::circle(),
                 .at = &coronaDrift,
                 .align = TextPath::Align::Start,
                 .offset = -coronaSize * 0.34f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::rise(coronaSize * 1.1f)),
             .stagger = coronaCascade(),
             .progress = beat(tBand, tBand + bandSpanS)})
        .fx({.effect = fx::tint(kEmber, kBone),
             .stagger = coronaCascade(),
             .progress = beat(tBand, tBand + bandSpanS)});
  }

  /** THE NINE NAMES, one text leaf whose baseline is a circle it orbits.
   *  Three tracks and the marquee compose: the forming cascade (held
   *  rise), the ignition swell on GRAD — up and back down, two phases
   *  driving one axis, which lerp — and the charge pass, whose SkSL holds
   *  the whole ignition schedule as per-unit uniforms.
   *
   *  THE PASS IS MOUNTED ONLY AROUND IGNITION, and that is a measured
   *  decision, not a flourish: a mounted pass draws its layer and runs
   *  its shader on every frame the node paints, whatever the phases say
   *  — the runtime cannot know an arbitrary material is an identity at
   *  rest — and this ring is volatile for the whole loop because it
   *  orbits. Left mounted, the pass alone is the frame: on the CPU
   *  raster path it multiplies the whole sheet's cost by two orders of
   *  magnitude while every phase is still zero. Mounting it just before
   *  its window opens and dropping it once the charge has settled is
   *  one re-describe each way; at both seams the pass is an exact
   *  pass-through, so the swap lands on identical pixels. */
  [[nodiscard]] Element nomina(bool withPass) {
    Stagger form = stagger(unit::Word, {.amountMs = 1900});
    form.then(unit::Cluster, {.eachMs = 24, .durationMs = 420});

    TextEffect swell =
        fx::seq(fx::axis("GRAD", 400.0f, 860.0f).until(0.45f).xfade(0.25f),
                fx::axis("GRAD", 860.0f, 400.0f));

    // How far past the ring's snug box the pass may paint: the glyphs
    // straddle the baseline circle and stand proud of the box at its four
    // extremes, the forming rise carries them further, and the charge's
    // wash spreads around each name's rect. Over-reporting is safe;
    // under-reporting shears the outer halves off at the layer's edge.
    constexpr float kReach = 90.0f;
    Element names =
        text(toU8(nominaText), ring(nominaSize, kGold, 4.2f))
            .key("nomina")
            .centerAt(kEye)
            .width(2 * rNomina * kR)
            .height(2 * rNomina * kR)
            .onPath({.path = shapes::circle(),
                     .at = &nominaDrift,
                     .align = TextPath::Align::Start,
                     .offset = -nominaSize * 0.34f,
                     .autoFlip = false})
            .fx({.effect = fx::hold(fx::rise(nominaSize * 0.8f)),
                 .stagger = form,
                 .progress = beat(tNames, tNames + namesSpanS)})
            .fx({.effect = std::move(swell),
                 .stagger =
                     stagger(unit::Word, {.eachMs = 130, .durationMs = 900}),
                 .progress = beat(tIgnite, tIgnite + 2.6)});
    if (withPass)
      names.fx(
          {.effect =
               fx::pass(Material::sksl(kChargeSksl).uniform("uGold", kGold)),
           .stagger = stagger(unit::Word, {.eachMs = 170, .durationMs = 820}),
           .progress = beat(tIgnite, tIgnite + 2.6),
           .reach = kReach});
    return names;
  }

  /** THE TABLET — the minuscule ring, serif italic, deliberately the one
   *  band that never orbits: the commentary stands still while the wheel
   *  turns.
   *
   *  The lozenges that stand at three words live on the colophon, whose
   *  straight baseline suits them; a mark on THIS ring would land on the
   *  curved rect, the same placement `beatsOf` reports. */
  [[nodiscard]] Element smaragdina() {
    return text(toU8(glossText), ital(glossSize, kVerdigris, 1.1f))
        .key("smaragdina")
        .centerAt(kEye)
        .width(2 * rGloss * kR)
        .height(2 * rGloss * kR)
        .onPath({.path = shapes::circle(),
                 .at = 0.0f,
                 .align = TextPath::Align::Start,
                 .offset = -glossSize * 0.30f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::slide(-14.0f)),
             .stagger =
                 stagger(unit::Cluster, {.eachMs = 13, .durationMs = 380}),
             .progress = beat(tGloss, tGloss + glossSpanS)});
  }

  /** THE HEXAGRAM and its captions. The strokes are the {6/2} star's two
   *  triangles revealed as one sweep; the captions ride a SECOND path —
   *  the same six chords as open contours — so chord k's midpoint is at
   *  exactly (k+0.5)/6 of one arc-length coordinate, and each day name is
   *  a leaf addressed by fraction alone. */
  [[nodiscard]] Element hexagram() {
    Element fig = disc(kEye, rHex * kR).key("hexagramma");
    fig.child(box()
                  .key("hex-strokes")
                  .absolute()
                  .inset(0)
                  .shape(kit::chords({.sides = 6, .step = 2, .closed = true}))
                  .fill(Fill::none())
                  .stroke(spans::upTo(beat(tHex, tHex + 1.3)),
                          util::stroke(1.4f, Fill::color(kIron))))
        .child(box()
                   .key("hex-flash")
                   .absolute()
                   .inset(0)
                   .shape(kit::chords({.sides = 6, .step = 2, .closed = true}))
                   .fill(Fill::none())
                   .stroke(util::stroke(1.4f, Fill::color(kGold)))
                   .opacity(pulse(tIgnite + 0.4, tIgnite + 3.2, 0.5)));
    const Shape chordPath =
        kit::chords({.sides = 6, .step = 2, .inset = kSatR * 1.12f});
    for (int k = 0; k < 6; ++k) {
      fig.child(text(toU8(kPlanets[k].dies), label(12.5f, kAsh, 2.2f))
                    .key("dies" + std::to_string(k))
                    .absolute()
                    .inset(0)
                    .onPath({.path = chordPath,
                             .at = ((float)k + 0.5f) / 6.0f,
                             .align = TextPath::Align::Center,
                             .offset = 5.0f,
                             .autoFlip = true})
                    .fx({.effect = fx::typeOn(),
                         .stagger = stagger(unit::Cluster,
                                            {.eachMs = 34, .durationMs = 120}),
                         .progress = beat(capAt[k], capAt[k] + capSpanS)}));
    }
    return fig;
  }

  /** ONE ROUNDEL — a sub-circle that is a small magic circle of its own:
   *  rules struck, a ring of lettering tumbling on, the square's order
   *  decoding at the centre, the order-sided polygon spinning behind it.
   *  Roundel k's whole window starts where roundel k−1's span says, so
   *  the six form in turn and the eye is handed round the hexagram. */
  [[nodiscard]] Element satellite(int k) {
    const Planet& p = kPlanets[k];
    const SkPoint c = P((float)k * 60.0f, rHex);
    const double at = tSat[k];
    const std::string id = "sat" + std::to_string(k);

    Element roundel = disc(c, kSatR).key(id);
    // The ground: occludes the chords under the roundel, and wears a
    // gold aura for a breath at ignition.
    roundel
        .child(box()
                   .key(id + "-ground")
                   .absolute()
                   .inset(0)
                   .corners({kSatR})
                   .fill(Fill::color(studio::hex(0x0D0A16, 0.94f)))
                   .opacity(beat(at, at + 0.4)))
        .child(
            box()
                .key(id + "-aura")
                .absolute()
                .inset(-18)
                .fill(Material::glowUnit({0.5f, 0.5f}, 0.5f,
                                         {{0.0f, studio::hex(0xD8A94E, 0.30f)},
                                          {1.0f, studio::hex(0xD8A94E, 0.0f)}}))
                .opacity(pulse(tIgnite + 0.5 + k * 0.16,
                               tIgnite + 2.6 + k * 0.16, 0.4)));
    // The rules, struck as sweeps.
    roundel
        .child(box()
                   .key(id + "-rule-out")
                   .absolute()
                   .inset(0)
                   .corners({kSatR})
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(at, at + 0.55)),
                           util::stroke(1.3f, Fill::color(kIron))))
        .child(box()
                   .key(id + "-rule-in")
                   .absolute()
                   .inset(kSatR - kSatRing + 9.0f)
                   .corners({kSatRing - 9.0f})
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(at + 0.15, at + 0.7)),
                           util::stroke(0.8f, Fill::color(kIronDim))));
    // The order-sided polygon, spinning once the roundel is lit.
    roundel.child(box()
                      .key(id + "-poly")
                      .absolute()
                      .inset(kSatR - 34.0f)
                      .shape(shapes::polygon(p.order))
                      .fill(Fill::none())
                      .stroke(util::stroke(0.9f, Fill::color(kIronDim)))
                      .rotate(bind(&satSpin[k]).target(0.0f, 360.0f))
                      .opacity(beat(at + 0.3, at + 0.9)));
    // The ring: planet, angel, table — tumbling onto the circle, then
    // orbiting; alternate roundels orbit the other way.
    roundel.child(
        text(toU8(satText[k]), ring(satSize[k], kBone, 1.8f))
            .key(id + "-ring")
            .absolute()
            .inset(kSatR - kSatRing)
            .onPath({.path = shapes::circle(),
                     .at = &satDrift[k],
                     .align = TextPath::Align::Start,
                     .offset = -satSize[k] * 0.34f,
                     .autoFlip = false})
            .fx({.effect = fx::hold(fx::spinIn(70.0f, 9.0f)),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 34, .durationMs = 520}),
                 .progress = beat(at + 0.25, at + 0.25 + satSpanS)}));
    // The order at the centre, decoding — held, so a digit waiting its
    // beat is absent rather than churning wrong.
    roundel.child(
        text(toU8(std::string(p.ordo)), mono(26.0f, kGold, 2.0f))
            .key(id + "-ordo")
            .centerAt({kSatR, kSatR})
            .fx({.effect = fx::hold(fx::scramble(U"IVXLC", 12)),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 90, .durationMs = 620}),
                 .progress = beat(at + 0.55, at + 0.55 + satSpanS * 0.9)}));
    return roundel;
  }

  /** THE SQUARE OF THE SUN at the hub: six monospaced rows resolving
   *  line by line, each digit churning through the charset until its own
   *  beat closes. */
  [[nodiscard]] Element quadratum() {
    Stagger rows = stagger(unit::Line, {.eachMs = 170});
    rows.then(unit::Cluster, {.eachMs = 46, .durationMs = 560});
    return box()
        .key("hub")
        .centerAt(kEye)
        .column()
        .alignItems(Align::Center)
        .gap(7)
        .child(text(toU8(kSolSquare), mono(17.5f, kGold, 2.6f))
                   .key("quadratum")
                   .fx({.effect = fx::hold(fx::scramble(U"0123456789", 14)),
                        .stagger = rows,
                        .progress = beat(tSquare, tSquare + squareSpanS)}))
        .child(text(toU8("TABVLA SOLIS \xc2\xb7 MICHAEL"),
                    label(9.5f, kAshDim, 2.8f))
                   .key("hub-cap")
                   .opacity(beat(tSquare + squareSpanS * 0.8,
                                 tSquare + squareSpanS * 0.8 + 0.5)));
  }

  [[nodiscard]] Element wheel(bool withPass) {
    Element panel = box().key("rota").width(1000).height(1000).shrink(0);

    // The ground wash under the wheel.
    panel.child(box()
                    .key("rota-wash")
                    .absolute()
                    .inset(0)
                    .fill(Material::glowUnit({0.5f, 0.5f}, 0.62f,
                                             {{0.0f, kNightLift},
                                              {0.66f, studio::hex(0x0D0A18)},
                                              {1.0f, kNight}})));

    // The compass work: rules and the division ladder, swept on.
    panel.child(rule("rule-out", rRimOut, 1.7f, kIron, 0.35, 1.3));
    panel.child(rule("rule-rim", rRimIn, 0.9f, kIron, 0.55, 1.3));
    panel.child(rule("rule-nom", rNomIn, 0.8f, kIronDim, 0.75, 1.3));
    panel.child(rule("rule-hub", rHub, 0.9f, kIron, tSquare - 0.4, 0.9));
    panel.child(box()
                    .key("ticks")
                    .absolute()
                    .inset((1.0f - rTickOut) * kR + (500.0f - kR))
                    .shape(kit::ticks(
                        {.divisions = 72,
                         .mark = {rTickIn / rTickOut, 1.0f},
                         .longEvery = 9,
                         .longMark = {rTickIn / rTickOut - 0.012f, 1.0f}}))
                    .fill(Fill::none())
                    .stroke(spans::upTo(beat(0.7, 2.1)),
                            util::stroke(0.8f, Fill::color(kIronDim))));

    // The bands, hub, figures and roundels.
    panel.child(corona());
    panel.child(nomina(withPass));
    panel.child(smaragdina());
    panel.child(hexagram());
    for (int k = 0; k < 6; ++k) panel.child(satellite(k));
    panel.child(quadratum());

    // The scribe: the point of the pen, led round the rim by the writing
    // cascade — placed every frame from the schedule read back, so it
    // cannot drift from the letters it appears to write.
    panel.child(
        box()
            .key("scribe")
            .left(-9)
            .top(-9)
            .width(18)
            .height(18)
            .hitTestable(false)
            .fill(Material::glowUnit({0.5f, 0.5f}, 0.5f,
                                     {{0.0f, studio::hex(0xFFE9B0)},
                                      {0.35f, studio::hex(0xD8A94E, 0.55f)},
                                      {1.0f, studio::hex(0xD8A94E, 0.0f)}}))
            .translateX(&scribeX)
            .translateY(&scribeY)
            .opacity(&scribeA));
    return panel;
  }

  // ------------------------------------------------------------------
  // the protocol column

  [[nodiscard]] Element ledgerRow(int i, const char* numeral, const char* name,
                                  const char* gloss, double from, double to) {
    return box()
        .key("stage" + std::to_string(i))
        .row()
        .gap(12)
        .alignItems(Align::Baseline)
        .opacity(beat(from, from + 0.45))
        .child(text(toU8(std::string(numeral)), mono(12.0f, kGold, 1.0f))
                   .width(44))
        .child(
            box()
                .column()
                .gap(4)
                .grow(1)
                .child(text(toU8(std::string(name)), label(12.5f, kBone, 2.8f)))
                .child(
                    text(toU8(std::string(gloss)), ital(11.5f, kAshDim, 0.4f)))
                .child(box()
                           .height(2)
                           .width(pct(100))
                           .fill(Fill::color(kIronDim))
                           .child(box()
                                      .height(2)
                                      .width(pct(100))
                                      .fill(Fill::color(kEmber))
                                      .transformOrigin(0.0f, 0.5f)
                                      .scaleX(beat(from, to)))));
  }

  /** The colophon: one paragraph, three registers by NAME — the planets
   *  addressed by the treatment they were written in, breathing on GRAD
   *  while the wheel turns — the numerals found by pattern, and a lozenge
   *  MARKED at the word the whole wheel converges on. */
  [[nodiscard]] Element colophon() {
    sigil::weave::StyleSet set{ital(12.5f, kAsh, 0.4f)};
    set.set("nomen", label(12.0f, kBone, 2.2f));
    RichText copy = rich(set.base());
    copy.styles(set)
        .add(u8"Nine names gird the wheel; ")
        .add(u8"SIX PLANETS", "nomen")
        .add(u8" convene about ")
        .add(u8"SOL", "nomen")
        .add(
            u8", whose square of 36 numerals binds every rank to 111. "
            u8"Every start below is read off the cascade before it.");
    return text(copy)
        .mark(sel::text(u8"SOL"),
              box()
                  .key("m-sol")
                  .left(pct(50))
                  .top(pct(112))
                  .width(6)
                  .height(6)
                  .shape(shapes::polygon(4))
                  .fill(Fill::color(kGold))
                  .opacity(beat(tIgnite + 0.9, tIgnite + 1.5)))
        .key("colophon")
        .width(pct(100))
        .spanPaint(sel::regex(u8"[0-9]+"),
                   sigil::weave::PaintStyle(kGold.toSkColor()))
        .fx({.where = sel::style("nomen"),
             .effect = fx::axis("GRAD", 400.0f, 780.0f),
             .stagger = stagger(
                 unit::Word,
                 {.eachMs = 90, .durationMs = 700, .beatsOver = beats::Text}),
             .progress = bind(&secs).source(0.0f, 6.4f).cosine()})
        .opacity(beat(tIgnite, tIgnite + 0.8));
  }

  [[nodiscard]] Element protocol() {
    Element col =
        box().key("protocol").column().grow(1).gap(16).padding(56, 64, 56, 46);
    col.child(
        text(toU8("ROTA EX LITTERIS CONVOCATA"), label(11.5f, kAshDim, 3.4f))
            .key("eyebrow")
            .opacity(beat(0.25, 0.85)));
    col.child(
        text(toU8("ROTA CONVOCATIONIS"), studio::type({.face = faceTitle,
                                                       .size = 42.0f,
                                                       .color = kBone,
                                                       .track = 1.2f}))
            .key("titulus")
            .fx({.effect = fx::rise(18.0f),
                 .stagger = {.eachMs = 0, .amountMs = 460, .durationMs = 540},
                 .progress = beat(0.35, 1.8)}));
    col.child(text(toU8("an invented conjuring wheel in the Solomonic and "
                        "Agrippan idiom \xe2\x80\x94 it assembles, ring by "
                        "ring, and is read in the order it forms"),
                   ital(12.5f, kAsh, 0.3f))
                  .key("subtitle")
                  .width(pct(100))
                  .opacity(beat(0.7, 1.4)));
    col.child(box().height(6));

    struct Stage {
      const char* numeral;
      const char* name;
      const char* gloss;
      double from, to;
    };
    const Stage stages[8] = {
        {"I", "CIRCVLI", "the compass strikes the rules", 0.35, 2.1},
        {"II", "CORONA", "the invocation, letter by letter round the rim",
         tBand, tBand + bandSpanS},
        {"III", "NOMINA", "nine names of the first circle rise", tNames,
         tNames + namesSpanS},
        {"IIII", "SMARAGDINA", "the tablet's words, minuscule and still",
         tGloss, tGloss + glossSpanS},
        {"V", "HEXAGRAMMA", "six chords carry six days", tHex,
         capAt[5] + capSpanS},
        {"VI", "PLANETAE", "the roundels convene, each in its turn", tSat[0],
         tSat[5] + satSpanS + 0.25},
        {"VII", "QVADRATVM", "the square of the Sun resolves", tSquare,
         tSquare + squareSpanS},
        {"VIII", "IGNITIO", "the charge runs the names; the wheel turns",
         tIgnite, tIgnite + 2.6},
    };
    for (int i = 0; i < 8; ++i)
      col.child(ledgerRow(i, stages[i].numeral, stages[i].name, stages[i].gloss,
                          stages[i].from, stages[i].to));

    col.child(box().grow(1));
    col.child(colophon());
    col.child(text(toU8(std::to_string(totalGlyphs) +
                        " GLYPHS IN THE FIGVRE \xc2\xb7 14 CVRVED BASELINES "
                        "\xc2\xb7 EVERY START CHAINED FROM A SPAN, NONE FITTED "
                        "BY HAND"),
                   label(9.5f, kAshDim, 2.4f))
                  .key("colophon-2")
                  .opacity(beat(tIgnite + 0.4, tIgnite + 1.2)));
    return col;
  }

  [[nodiscard]] Element describe(bool withPass) {
    return stack()
        .fill(Material::glowUnit({0.3125f, 0.5f}, 0.9f,
                                 {{0.0f, kNightLift}, {1.0f, kNight}}))
        .child(box()
                   .row()
                   .absolute()
                   .inset(0)
                   .opacity(envelope())
                   .child(wheel(withPass))
                   .child(protocol()));
  }

  // ------------------------------------------------------------------

  void setup(sigil::compose::sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kNight);

    faceRing = studio::pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 500);
    faceRingBold =
        studio::pickFace({".SF NS", "SF Pro", "Helvetica Neue"}, 600);
    faceTitle =
        studio::pickFace({"Iowan Old Style", "Charter", "Georgia"}, 600);
    faceItal = studio::pickFace({"Iowan Old Style", "Charter", "Georgia"}, 400,
                                SkFontStyle::kItalic_Slant);
    faceMono = studio::pickFace({"Menlo", "SF Mono", "Courier New"}, 500);

    // ---- content, fitted to its own bands --------------------------------
    coronaText = "+ ";
    for (int i = 0; i < 8; ++i) {
      coronaText += kInvocatio[i];
      coronaText += i < 7 ? " + " : " ";
    }
    nominaText.clear();
    for (const char* n : kNames) {
      nominaText += n;
      nominaText += " \xc2\xb7 ";
    }
    glossText = kSmaragdina;
    coronaSize =
        fitToRing(ctx, coronaText, ring(18, kBone, 2.2f), rCorona * kR);
    nominaSize =
        fitToRing(ctx, nominaText, ring(30, kGold, 4.2f), rNomina * kR);
    glossSize =
        fitToRing(ctx, glossText, ital(15, kVerdigris, 1.1f), rGloss * kR);
    for (int k = 0; k < 6; ++k) {
      satText[k] = std::string(kPlanets[k].planet) + " \xc2\xb7 " +
                   kPlanets[k].angel + " \xc2\xb7 TABVLA " + kPlanets[k].ordo +
                   " \xc2\xb7 ";
      satSize[k] =
          fitToRing(ctx, satText[k], ring(12, kBone, 1.8f), kSatRing, 0.97f);
    }

    // ---- the rim's cue table: writing pace, pausing at each cross --------
    coronaCues.clear();
    coronaWords = 0;
    coronaMaxWord = 1;
    {
      float t = 0.0f;
      size_t i = 0;
      while (i < coronaText.size()) {
        while (i < coronaText.size() && coronaText[i] == ' ') ++i;
        if (i >= coronaText.size()) break;
        size_t j = i;
        while (j < coronaText.size() && coronaText[j] != ' ') ++j;
        coronaCues.push_back(t);
        ++coronaWords;
        coronaMaxWord = std::max(coronaMaxWord, (int)(j - i));
        t += kStepMs;
        if (coronaText[i] == '+') t += kCrossMs;
        i = j;
      }
    }

    // ---- the timeline, every window chained from a span ------------------
    bandSpanS =
        coronaCascade().spanMs((uint32_t)coronaWords, (uint32_t)coronaMaxWord) /
        1000.0f;
    {
      Stagger form = stagger(unit::Word, {.amountMs = 1900});
      form.then(unit::Cluster, {.eachMs = 24, .durationMs = 420});
      namesSpanS = form.spanMs(18, 14) / 1000.0f;
    }
    glossSpanS = stagger(unit::Cluster, {.eachMs = 13, .durationMs = 380})
                     .spanMs((uint32_t)glyphsOf(glossText)) /
                 1000.0f;
    capSpanS =
        stagger(unit::Cluster, {.eachMs = 34, .durationMs = 120}).spanMs(13) /
        1000.0f;
    satSpanS = stagger(unit::Cluster, {.eachMs = 34, .durationMs = 520})
                   .spanMs((uint32_t)glyphsOf(satText[0])) /
               1000.0f;
    {
      Stagger rows = stagger(unit::Line, {.eachMs = 170});
      rows.then(unit::Cluster, {.eachMs = 46, .durationMs = 560});
      squareSpanS = rows.spanMs(6, 12) / 1000.0f;
    }

    tBand = 1.1;
    tNames = tBand + bandSpanS * 0.62;
    tGloss = tNames + namesSpanS * 0.58;
    tHex = tGloss + glossSpanS * 0.72;
    for (int k = 0; k < 6; ++k) capAt[k] = tHex + 0.55 + k * capSpanS * 0.72;
    tSat[0] = capAt[2];
    for (int k = 1; k < 6; ++k) tSat[k] = tSat[k - 1] + satSpanS * kSatBeat;
    tSquare = tSat[5] + satSpanS * 0.8;
    tIgnite = tSquare + squareSpanS + 0.4;
    loopSecs = tIgnite + 2.6 + 2.8 + 1.6;

    // Mid-cycle among the roundels: rings and hexagram formed, three
    // roundels lit, one forming — the frame where the assembly reads.
    ctx.captureAt(tSat[3] + satSpanS * 0.55);

    // The chained timeline, printed: every number below came out of a
    // span, and this is where to read what the chaining resolved to.
    std::fprintf(stderr,
                 "[rota] corona %.2f+%.2fs  nomina %.2f+%.2fs  tablet "
                 "%.2f+%.2fs  hex %.2f  roundels %.2f..%.2f (+%.2fs each)  "
                 "square %.2f+%.2fs  ignition %.2f  loop %.2fs\n",
                 tBand, bandSpanS, tNames, namesSpanS, tGloss, glossSpanS, tHex,
                 tSat[0], tSat[5], satSpanS, tSquare, squareSpanS, tIgnite,
                 loopSecs);

    totalGlyphs = glyphsOf(coronaText) + glyphsOf(nominaText) +
                  glyphsOf(glossText) + glyphsOf(kSolSquare);
    for (int k = 0; k < 6; ++k)
      totalGlyphs += glyphsOf(satText[k]) + glyphsOf(kPlanets[k].ordo) +
                     glyphsOf(kPlanets[k].dies);

    ctx.composer.render(describe(false));
  }

  void update(double elapsed,
              sigil::compose::sketch::SketchContext& ctx) override {
    const double tc = std::fmod(elapsed, loopSecs);
    cycle = (float)tc;
    secs = (float)elapsed;

    // The charge pass rides the tree only around its own window — see
    // nomina() for why. One re-describe mounts it on identical pixels
    // just before ignition and one drops it after the settle.
    const bool wantPass = tc > tIgnite - 0.25 && tc < tIgnite + 2.85;
    if (wantPass != passMounted) {
      passMounted = wantPass;
      ctx.composer.render(describe(passMounted));
    }

    // The wheel's turning, phased so nothing moves until it has formed:
    // the rim drifts clockwise after ignition, the names counter-orbit
    // from the moment they have risen, and alternate roundels turn
    // opposite ways.
    const double namesOn = tNames + namesSpanS;
    nominaDrift =
        (float)(tc > namesOn ? std::fmod((tc - namesOn) / 210.0, 1.0) : 0.0);
    coronaDrift =
        (float)(tc > tIgnite ? 1.0 - std::fmod((tc - tIgnite) / 340.0, 1.0)
                             : 0.0);
    for (int k = 0; k < 6; ++k) {
      const double on = tSat[k] + 0.25 + satSpanS;
      const double turn = tc > on ? (tc - on) / (k % 2 ? -46.0 : 46.0) : 0.0;
      satDrift[k] = (float)(turn - std::floor(turn));
      const double spinOn = tSat[k] + 0.3;
      const double spin =
          tc > spinOn ? (tc - spinOn) / (k % 2 ? 34.0 : -34.0) : 0.0;
      satSpin[k] = (float)(spin - std::floor(spin));
    }

    // The scribe, placed from the schedule read back: the most recently
    // opened beat of the rim's writing cascade is where the pen is.
    float bestStart = -1.0f;
    SkRect at = SkRect::MakeEmpty();
    for (const Beat& b : ctx.composer.beatsOf("corona", 0))
      if (b.localT > 0.0f && b.localT < 1.0f && b.startMs > bestStart) {
        bestStart = b.startMs;
        at = b.rect;
      }
    const bool writing =
        bestStart >= 0.0f && tc > tBand && tc < tBand + (double)bandSpanS;
    if (writing) {
      scribeX = at.centerX();
      scribeY = at.centerY();
    }
    // The dying glow decays in TIME, not per frame, so a jittering frame
    // interval cannot change how long the pen's light lingers.
    const double dt = std::max(0.0, elapsed - lastElapsed);
    lastElapsed = elapsed;
    scribeA = writing ? 1.0f : std::max(0.0, scribeA.value() - 4.5 * dt);
  }
};

SIGIL_SKETCH(RotaConvocationis)
