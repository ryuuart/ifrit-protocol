// rota_convocationis.cpp — A MAGIC CIRCLE THAT ASSEMBLES ITSELF AND THEN
// IGNITES: an invented conjuring wheel in the real idiom of the early-modern
// diagrams, drawn in chalk and then lit, run as the text engine's heaviest
// single sheet — fifteen curved baselines forming, orbiting and charging on
// one declared schedule.
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
//     {6/2} hexagram, its six spokes, the day-name captions on its chords,
//     the inner band that sets the tablet's Latin again as texture, Sol's
//     square at the hub with the six other planets on the points, and every
//     colour, radius and millisecond are composition, not record.
//   * The IGNITION is invented twice over: no manuscript lights, and the
//     screen craft it borrows its light from never drew this figure. The
//     treatment is quoted; the picture is not.
//   * The ASSEMBLY is the subject and entirely invented: no manuscript
//     animates. The order performed here — rules, rim, names, tablet,
//     chords, roundels, square, charge — is a plausible working order, not
//     a documented one.
//
// -----------------------------------------------------------------------------
// THE MACHINE, and what it puts under load
//
//   * FIFTEEN TEXT-ON-PATH LEAVES at once: four full rings, six chord
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
//     resolved schedule. The pass DECLARES WHERE IT RESTS, so it stays
//     mounted for the whole loop and costs nothing outside its window on
//     a ring that repaints for the whole loop because it orbits.
//   * THE LIGHT IS UNIONED BEFORE IT IS ADDED: each lighting group's whole
//     geometry is stroke-expanded and merged into one region at four
//     widths, so an additive stack cannot print a crossing at twice the
//     brightness of the lines that cross there. SigilShape is the geometry
//     PRODUCER here — booleans, roughening and a resample-and-lerp morph —
//     and compose consumes what it produces as ordinary comparable
//     silhouette values. That boundary does not move: compose does not
//     link SigilShape, and this file does.
//   * MIXED REGISTERS on one wheel: majuscule rings on the grotesque that
//     carries GRAD (so the ignition can swell the names' weight with no
//     reshape), the tablet in serif italic minuscule, numerals mono and
//     scramble-decoded — the square resolves line by line under a nested
//     cascade, each digit churning until its beat closes.
//   * THE LOAD, deliberately: per-glyph alpha, rotation, colour multiplier,
//     colour screen and axis coordinate all varying at once across every
//     glyph of the fifteen curved baselines (the quantisation ladder and
//     the atlas); four ladders, six roundels and the figure itself
//     counter-turning at their own rates over cached content; an
//     ember pool stamped as one draw; and — for the ignition's own window
//     — a full-band shader pass, a radial-ray material, a chromatic
//     backdrop and a screened flood, every one of them gated so that a
//     gain of zero is a node the painter never reaches.
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
//   ~1.6 s  the compass: the chalk rules and three tick classes sweep on
//   ~3.0 s  the rim mid-write, the scribe point leading the letters
//   ~5.5 s  the rim's light lands; the tablet's minuscule sliding in
//   ~7.8 s  the hexagram struck — a scribble of light resolving onto it
//  ~13.0 s  the roundel cycle: three lit, one forming, two dark
//  ~17.0 s  the square of the Sun resolved, the hub burning as the emblem
//  ~17.9 s  ignition's crest — rays, flood, embers, a colour fringe
//  ~21.5 s  the hum: the charged wheel breathing, turning, drizzling
//  loops on a dark sheet.

#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/TextFx.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilshape/Geometry.h>
#include <sigilshape/Ops.h>
#include <sigilsketch/Sketch.h>
#include <sigilweave/Style.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
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

// ---- the ignited palette: ONE hue family, value doing the drawing --------
// Not a second scheme beside the candlelit one. It is the state the wheel
// changes INTO: chalk is pigment on a surface and reflects what the room
// gives it, light is emitted and collapses to one hue with a white core.
constexpr SkColor4f kCore = studio::hex(0xFFF6E2);   // the white-hot core
constexpr SkColor4f kHalo = studio::hex(0xFFC152);   // the saturated halo
constexpr SkColor4f kBloom = studio::hex(0xC96F1E);  // the wide dim bloom

// ---- the wheel's frame ----------------------------------------------------
constexpr SkPoint kEye{500.0f, 500.0f};  // wheel centre in canvas px
constexpr float kR = 430.0f;             // the greatest circle

// band radii, in units of the greatest circle
constexpr float rRimOut = 1.000f;   // outer rule
constexpr float rRimCase = 0.988f;  // its hairline companion
constexpr float rCorona = 0.952f;   // the invocation's baseline
constexpr float rRimIn = 0.910f;    // inner rule of the rim band
constexpr float rTickOut = 0.905f;  // the division ladder
constexpr float rTickIn = 0.878f;
constexpr float rFineOut = 0.874f;  // the fine ladder, one pitch finer
constexpr float rFineIn = 0.858f;
constexpr float rNomina = 0.840f;    // the nine names' baseline
constexpr float rNomIn = 0.775f;     // rule under the names
constexpr float rNomCase = 0.763f;   // its hairline companion
constexpr float rGloss = 0.742f;     // the tablet's minuscule baseline
constexpr float rHex = 0.520f;       // hexagram vertices; roundel centres
constexpr float rMicroOut = 0.418f;  // the micro-script band's rules
constexpr float rMicro = 0.376f;     // …and its baseline
constexpr float rMicroIn = 0.336f;
constexpr float rHub = 0.245f;     // rule round the square of the Sun
constexpr float kSatR = 76.0f;     // roundel outer radius, px
constexpr float kSatRing = 61.0f;  // roundel ring-text baseline radius, px

// ---- the writing pace -----------------------------------------------------
constexpr float kStepMs = 55.0f;    // word to word round the rim
constexpr float kCrossMs = 190.0f;  // the scribe's extra pause at a cross
constexpr float kSatBeat = 0.88f;   // fraction of a roundel's span before
                                    // the next one starts forming

constexpr float kDeg = 3.14159265358979f / 180.0f;
constexpr int kEmbers = 96;  // the rim's rising sparks, then the drizzle

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

// Slots in the two baked tables. The chalk table holds one wobbled
// outline per construction line; the glow table one emissive stack per
// LIGHTING GROUP — a set of lines that ignite together and may cross each
// other, but crosses nothing in another group.
enum : int {
  kChalkRimOut,
  kChalkRimCase,
  kChalkRimIn,
  kChalkNomIn,
  kChalkNomCase,
  kChalkMicroOut,
  kChalkMicroIn,
  kChalkHub,
  kChalkHex,
  kChalkSpokes,
  kChalkCount,
};
enum : int { kGlowRim, kGlowNom, kGlowHex, kGlowHub, kGlowCount };

/** A path baked once at setup, wrapped as a comparable silhouette: the
 *  ADDRESS is the identity, so the node prunes and its recording caches
 *  exactly as a `shapes::` generator's would. The sketch owns the storage
 *  and fills it before the first describe; nothing moves it afterwards. */
struct Baked {
  const SkPath* held = nullptr;
  bool operator==(const Baked&) const = default;
  SkPath path(SkSize) const { return held ? *held : SkPath(); }
};

/** THE EMISSIVE OUTLINE of one lighting group, as four nested regions.
 *  Every line of the group is expanded to a region and the regions are
 *  UNIONED, so a place where two lines cross is covered ONCE; the four
 *  grades are that union grown three more times. Painting them additively
 *  is then safe, which is the whole reason for the union: stroke over
 *  stroke under `kPlus` prints every junction at twice the brightness of
 *  the lines running through it, and the references read as one circuit
 *  of even light with no bright knots at the crossings. */
struct Glow {
  SkPath core, halo, mid, bloom;
};

/** A circle of the wheel as a path in panel-local px. */
SkPath ringPath(float rNorm) {
  SkPathBuilder b;
  b.addOval(SkRect::MakeLTRB(kEye.x() - rNorm * kR, kEye.y() - rNorm * kR,
                             kEye.x() + rNorm * kR, kEye.y() + rNorm * kR));
  return b.detach();
}

/** THE HAND-DRAWN RULE: the compass wobbles, the pen has a nib. A rule
 *  struck in chalk is never a perfect circle, and the eye reads the
 *  difference immediately — which is what makes the clean emissive circle
 *  landing on top of it at the strike say "ignited" rather than "brighter".
 *  The seed is the band's own index, so two neighbouring rules wobble
 *  differently and no two share a wave. */
SkPath chalkRing(float rNorm, uint32_t seed) {
  return sigil::shape::ops::Roughen{
      .amplitude = 1.15f, .segmentPx = 34.0f, .seed = seed, .smooth = true}(
      ringPath(rNorm));
}

/** A LINE as a region of the given half-width. Stroke expansion, and not
 *  `ops::offset`, because offset unites its source with the expansion —
 *  correct for growing a region, and for a CLOSED line (a circle, the
 *  star's two triangles) it would hand back the interior as well. A rule
 *  is a line and never a disc. */
SkPath expand(const SkPath& line, float halfWidth) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halfWidth * 2.0f);
  p.setStrokeJoin(SkPaint::kRound_Join);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(line, p);
}

/** The group's lines widened four times and unioned at each width. */
Glow bakeGlow(const std::vector<SkPath>& lines, float coreHalf) {
  namespace ops = sigil::shape::ops;
  auto at = [&](float k) {
    std::vector<SkPath> regions;
    regions.reserve(lines.size());
    for (const SkPath& line : lines)
      regions.push_back(expand(line, coreHalf * k));
    return ops::unite(regions);
  };
  return Glow{at(1.0f), at(2.8f), at(7.0f), at(18.0f)};
}

/** THE RAYS: radial streaks thrown past the figure at ignition, sampled
 *  on angle so the count is a number and not a hundred nodes. The falloff
 *  is a window in radius — nothing at the hub, nothing past the sheet —
 *  and the streaks' own widths come from two beats of a hashed angle, so
 *  the fan reads as unequal spokes of light rather than a gear. */
constexpr const char* kRaysSksl = R"(
uniform float2 uResolution;
uniform float4 uInk;
half4 main(float2 xy) {
  float2 c = uResolution * 0.5;
  float2 d = xy - c;
  float r = length(d) / max(c.x, 1.0);
  float a = atan(d.y, d.x);
  float k = sin(a * 19.0) * 0.5 + 0.5;
  float j = sin(a * 47.0 + 1.7) * 0.5 + 0.5;
  float streak = pow(k, 6.0) * 0.75 + pow(j, 14.0) * 0.55;
  float band = smoothstep(0.15, 0.40, r) * (1.0 - smoothstep(0.44, 0.95, r));
  float v = streak * band;
  return half4(half3(uInk.rgb * v), half(v));
})";

/** THE CREST'S COLOUR FRINGE: the picture beneath re-sampled with its red
 *  and blue pulled apart ALONG THE RADIUS, which is where a lens throws
 *  them on a light this bright. Attached as a backdrop and gated by the
 *  node's own opacity, so it is a crossfade to the fringed frame at the
 *  crest and literally not painted at any other moment. */
constexpr const char* kFringeSksl = R"(
uniform shader content;
uniform float uSpread;
uniform float uCx;
uniform float uCy;
half4 main(float2 xy) {
  float2 d = xy - float2(uCx, uCy);
  float r = length(d);
  float2 u = r > 0.001 ? d / r : float2(0.0);
  float k = uSpread * (0.5 + r * 0.0022);
  half4 lo = content.eval(xy + u * k);
  half4 mid = content.eval(xy);
  half4 hi = content.eval(xy - u * k);
  return half4(lo.r, mid.g, hi.b, mid.a);
})";

/** The fringe compiled once for the process. */
sk_sp<SkRuntimeEffect> fringeEffect() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(kFringeSksl));
    if (!effect) std::fprintf(stderr, "[rota] fringe: %s\n", err.c_str());
    return effect;
  }();
  return fx;
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
  ch::Output<float> microDrift{0};     // the micro-script band, the other way
  ch::Output<float> ladderSpin{0};     // the fine ladder — the fast layer
  ch::Output<float> hexSpin{0};        // the figure, counter to the ladder
  ch::Output<float> satDrift[6] = {};  // each roundel's ring orbit
  ch::Output<float> satSpin[6] = {};   // each roundel's polygon
  ch::Output<float> scribeX{0}, scribeY{0}, scribeA{0};

  // How hard each lighting group is burning. One scalar carries three
  // things a single declared window cannot say together — the strike's
  // cross-fade, the flash the strike throws, and the breath the charged
  // wheel keeps afterwards — and it is bound, so the four cached
  // recordings of the group's emissive stack replay under it untouched.
  ch::Output<float> litRim{0}, litNom{0}, litHex{0}, litHub{0};
  ch::Output<float> litSat[6] = {};
  ch::Output<float> morphStep{0};  // the hexagram's strike-morph ladder
  ch::Output<float> humScale{1};   // the charged wheel's scale breath
  ch::Output<float> floodA{0}, raysA{0}, emberA{0};
  // The fringe is a BACKDROP: it re-samples what is already painted, and a
  // backdrop is not blended back through the node's alpha the way a fill
  // is — an opacity of a half would not be half a fringe. So the ramp
  // lives in the shader's own spread and the opacity is a hard gate,
  // exactly zero outside the crest, which is what keeps the effect from
  // being paid for on any frame that does not want it.
  ch::Output<float> fringeK{0}, fringeA{0};

  sk_sp<SkTypeface> faceRing, faceRingBold, faceTitle, faceItal, faceMono;

  // Geometry baked once, before the first describe, and never moved: the
  // chalk's wobble, each lighting group's emissive stack, and the
  // hexagram's morph ladder.
  std::vector<SkPath> chalk;
  std::vector<Glow> glows;
  std::vector<SkPath> hexSteps;
  std::shared_ptr<instancing::Atlas> emberAtlas;
  std::shared_ptr<instancing::Pool> embers;
  int emberFrame = 0;

  // Fitted content: each ring's text and the size that girds its band.
  std::string coronaText, nominaText, glossText, microText;
  std::string satText[6];
  float coronaSize = 18, nominaSize = 30, glossSize = 15, microSize = 8,
        satSize[6] = {};
  std::vector<float> coronaCues;  // one start per word; pauses at crosses
  int coronaWords = 0, coronaMaxWord = 1;

  // The computed timeline, seconds. Every value is chained from a span.
  double tBand = 0, tNames = 0, tGloss = 0, tHex = 0, capAt[6] = {},
         tSat[6] = {}, tSquare = 0, tIgnite = 0, loopSecs = 30;
  double lastElapsed = 0;  // the scribe's decay reads real dt
  float bandSpanS = 0, namesSpanS = 0, glossSpanS = 0, microSpanS = 0,
        capSpanS = 0, satSpanS = 0, squareSpanS = 0;

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
   *  compass stroke itself. The outline is the baked chalk ring, so what
   *  the sweep lays down carries the hand's wobble; the box is the whole
   *  panel because the path is in the panel's own px. */
  [[nodiscard]] Element rule(const char* key, int chalkIndex, float width,
                             SkColor4f color, double from, double dur) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .shape(Baked{&chalk[(size_t)chalkIndex]})
        .fill(Fill::none())
        .stroke(spans::upTo(beat(from, from + dur)),
                util::stroke(width, Fill::color(color)));
  }

  /** ONE GRADE of a lighting group's emissive stack. Each grade is a flat
   *  fill of an already-unioned region and carries the group's own gain,
   *  so a fill-only leaf takes the blend and the opacity straight onto its
   *  paint: no layer opens, and the four grades add onto the sheet
   *  directly, which is what makes the pile read as light rather than as
   *  four translucent rings. */
  [[nodiscard]] Element grade(const std::string& key, const SkPath& region,
                              SkColor4f ink, float alpha,
                              const ch::Output<float>* gain) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .shape(Baked{&region})
        .fill(Fill::color({ink.fR, ink.fG, ink.fB, alpha}))
        .blend(SkBlendMode::kPlus)
        .opacity(gain);
  }

  /** THE IGNITED LINE — white-hot core, saturated halo, two grades of
   *  bloom. The value hierarchy is the drawing: the core carries the
   *  shape, the halo carries the hue, the bloom carries the reach. */
  [[nodiscard]] Element emissive(const std::string& key, const Glow& g,
                                 const ch::Output<float>* gain) {
    return box()
        .key(key)
        .absolute()
        .inset(0)
        .hitTestable(false)
        .child(grade(key + "-bloom", g.bloom, kBloom, 0.085f, gain))
        .child(grade(key + "-mid", g.mid, kBloom, 0.16f, gain))
        .child(grade(key + "-halo", g.halo, kHalo, 0.42f, gain))
        .child(grade(key + "-core", g.core, kCore, 0.96f, gain));
  }

  /** A DIVISION LADDER at one length class. `skipEvery` leaves a hole
   *  where a heavier class stands, because two passes stamping the same
   *  mark print it darker than either weight meant to. */
  [[nodiscard]] Element ladder(const char* key, int divisions, int skipEvery,
                               float outer, float inner, float width,
                               SkColor4f color, double from, double dur) {
    kit::Ticks t{.divisions = divisions,
                 .mark = {inner / outer, 1.0f},
                 .longEvery = skipEvery,
                 .longMark = {1.0f, 1.0f}};
    return box()
        .key(key)
        .absolute()
        .inset((1.0f - outer) * kR + (500.0f - kR))
        .shape(kit::ticks(t))
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
             .progress = beat(tBand, tBand + bandSpanS)})
        // THE STRIKE, per word: a letter does not fade up, it arrives lit
        // and cools. The screen term lifts each channel by the headroom it
        // has left rather than adding into a clip, so the flash reads as
        // the glyph glowing white for a beat and not as a white rectangle
        // where the glyph was.
        .fx({.effect = fx::keys({{0.00f, {.colorScreen = kHalo}},
                                 {0.18f, {.colorScreen = kCore}},
                                 {1.00f, {}}},
                                &ch::easeOutQuad),
             .stagger = coronaCascade(),
             .progress = beat(tBand, tBand + bandSpanS)});
  }

  /** THE NINE NAMES, one text leaf whose baseline is a circle it orbits.
   *  Three tracks and the marquee compose: the forming cascade (held
   *  rise), the ignition swell on GRAD — up and back down, two phases
   *  driving one axis, which lerp — and the charge pass, whose SkSL holds
   *  the whole ignition schedule as per-unit uniforms.
   *
   *  THE PASS DECLARES ITS OWN REST, which is what lets it stay mounted
   *  for the whole loop on a ring that is volatile for the whole loop
   *  because it orbits. The charge's flash is sin(π·phase): exactly zero
   *  at phase 0 and at phase 1, where the material returns its input
   *  pixels untouched — so `restsAt(0, 1)` is a true promise and the
   *  runtime skips both the layer and the shader on every frame outside
   *  the charge's window. The one-shot cascade clamps a unit to exactly 0
   *  before its beat and exactly 1 after, which is the comparison the
   *  skip makes; both ends therefore engage, where under a looping
   *  cascade only the settled end would. */
  [[nodiscard]] Element nomina() {
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
            .effect(styles::textGlow(kHalo, 6.0f))
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
                 .progress = beat(tIgnite, tIgnite + 2.6)})
            // The charge reaching a name flashes it, on the pass's own
            // cascade so the letters and the shader open together.
            .fx({.effect = fx::keys({{0.00f, {}},
                                     {0.30f, {.colorScreen = kCore}},
                                     {1.00f, {}}},
                                    &ch::easeInOutQuad),
                 .stagger =
                     stagger(unit::Word, {.eachMs = 170, .durationMs = 820}),
                 .progress = beat(tIgnite, tIgnite + 2.6)});
    names.fx(
        {.effect = fx::pass(Material::sksl(kChargeSksl).uniform("uGold", kGold))
                       .restsAt(0.0f, 1.0f),
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

  /** THE FIGURE: the {6/2} star, its spokes, and the light that lands on
   *  them — the wheel's one turning assembly of geometry, counter to the
   *  fine ladder outside it. The chalk is swept on at the strike; the
   *  morph ladder resolves the light from a scribble to the true figure;
   *  the emissive stack is what stays.
   *
   *  THE MORPH IS A LADDER OF BAKED STEPS, not one interpolated path. A
   *  path re-cooked every frame is content nothing can cache, and this one
   *  is a resample-and-lerp over four hundred points. Baked, each step is
   *  an ordinary comparable silhouette whose recording is built once, and
   *  a bound scalar walks the ladder: neighbouring steps cross-fade, so
   *  what the eye follows is continuous while what the library records is
   *  a bounded set. */
  [[nodiscard]] Element figure() {
    Element turn =
        box().key("figure").absolute().inset(0).hitTestable(false).rotate(
            bind(&hexSpin).target(0.0f, 360.0f));
    turn.child(box()
                   .key("spokes")
                   .absolute()
                   .inset(0)
                   .shape(Baked{&chalk[kChalkSpokes]})
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(tHex + 0.2, tHex + 1.5)),
                           util::stroke(0.7f, Fill::color(kIronDim))));
    turn.child(box()
                   .key("hex-strokes")
                   .absolute()
                   .inset(0)
                   .shape(Baked{&chalk[kChalkHex]})
                   .fill(Fill::none())
                   .stroke(spans::upTo(beat(tHex, tHex + 1.3)),
                           util::stroke(1.4f, Fill::color(kIron))));
    for (int i = 0; i < (int)hexSteps.size(); ++i)
      turn.child(box()
                     .key("hex-morph" + std::to_string(i))
                     .absolute()
                     .inset(0)
                     .hitTestable(false)
                     .shape(Baked{&hexSteps[(size_t)i]})
                     .fill(Fill::none())
                     .stroke(util::stroke(1.6f, Fill::color(kCore)))
                     .blend(SkBlendMode::kPlus)
                     .opacity(bind(&morphStep)
                                  .window((float)i - 1.0f, (float)i + 1.0f)
                                  .pingPong()));
    turn.child(emissive("hex-lit", glows[kGlowHex], &litHex));
    return turn;
  }

  /** THE HEXAGRAM'S CAPTIONS. They ride a SECOND path — the same six
   *  chords as open contours — so chord k's midpoint is at exactly
   *  (k+0.5)/6 of one arc-length coordinate, and each day name is a leaf
   *  addressed by fraction alone. They do not turn with the figure: a
   *  caption is read, and the mechanism is what moves. */
  [[nodiscard]] Element hexagram() {
    Element fig = disc(kEye, rHex * kR).key("hexagramma");
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
    roundel.child(
        box()
            .key(id + "-ground")
            .absolute()
            .inset(0)
            .corners({kSatR})
            .fill(Fill::color(studio::hex(0x0D0A16, 0.94f)))
            // The ground is dressed rather than shaded: an inner
            // glow is a blurred band hugging its own edge, a value
            // decoration that records once with the disc it sits
            // on. It gives the roundel a lip of light without a
            // second node and without a shader.
            .overlay(styles::innerGlow(studio::hex(0xE79A32, 0.30f), 14.0f))
            .opacity(beat(at, at + 0.4)));
    // The roundel's own emissive rule. A roundel is a small magic circle,
    // so it lights like one — but its two rules are concentric and cross
    // nothing, which is the case the SDF answers in one pass: silhouette,
    // core and halo are three uniforms of one shader rather than a union
    // and four fills.
    {
      const sdf::Style lit{.borderWidth = 1.5f,
                           .borderColor = kCore,
                           .glowRadius = 8.0f,
                           .glowColor = studio::hex(0xFFC152, 0.42f)};
      const float side = sdf::minBoxFor(lit, 2.0f * kSatR);
      roundel.child(box()
                        .key(id + "-lit")
                        .absolute()
                        .inset(kSatR - side * 0.5f)
                        .hitTestable(false)
                        .fill(sdf::material(sdf::circle(), lit))
                        .blend(SkBlendMode::kPlus)
                        .opacity(&litSat[k]));
    }
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
            .effect(styles::textGlow(kHalo, 4.0f))
            .fx({.effect = fx::hold(fx::scramble(U"IVXLC", 12)),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 90, .durationMs = 620}),
                 .progress = beat(at + 0.55, at + 0.55 + satSpanS * 0.9)})
            .fx({.effect = fx::keys({{0.00f, {}},
                                     {0.80f, {}},
                                     {0.90f, {.colorAdd = kCore}},
                                     {1.00f, {}}}),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 90, .durationMs = 620}),
                 .progress = beat(at + 0.55, at + 0.55 + satSpanS * 0.9)}));
    return roundel;
  }

  /** THE SQUARE OF THE SUN at the hub: six monospaced rows resolving
   *  line by line, each digit churning through the charset until its own
   *  beat closes.
   *
   *  IT IS THE EMBLEM, and an emblem is the brightest stable thing on the
   *  plate — the one place the eye returns to between the events. So the
   *  digits are set bone rather than gold (the value hierarchy, not the
   *  hue, is what makes a centre dominant), they take their own flash as
   *  each row lands, and the whole tablet sits in a disc of light with a
   *  glow of its own. The blur is affordable HERE and nowhere else on the
   *  wheel: this node is two hundred pixels across and does not orbit. */
  [[nodiscard]] Element quadratum() {
    Stagger rows = stagger(unit::Line, {.eachMs = 170});
    rows.then(unit::Cluster, {.eachMs = 46, .durationMs = 560});
    Element hub = box()
                      .key("hub")
                      .centerAt(kEye)
                      .column()
                      .alignItems(Align::Center)
                      .gap(7);
    hub.child(text(toU8(kSolSquare), mono(17.5f, kBone, 2.6f))
                  .key("quadratum")
                  .effect(styles::textGlow(kHalo, 5.0f))
                  .fx({.effect = fx::hold(fx::scramble(U"0123456789", 14)),
                       .stagger = rows,
                       .progress = beat(tSquare, tSquare + squareSpanS)})
                  .fx({.effect = fx::keys({{0.00f, {}},
                                           {0.72f, {}},
                                           {0.86f, {.colorAdd = kHalo}},
                                           {1.00f, {}}}),
                       .stagger = rows,
                       .progress = beat(tSquare, tSquare + squareSpanS)}));
    hub.child(
        text(toU8("TABVLA SOLIS \xc2\xb7 MICHAEL"), label(9.5f, kAshDim, 2.8f))
            .key("hub-cap")
            .opacity(beat(tSquare + squareSpanS * 0.8,
                          tSquare + squareSpanS * 0.8 + 0.5)));
    return hub;
  }

  /** The disc of light the emblem sits in — the SDF's one pass carrying
   *  fill and glow together, sized by the reserve the style declares so
   *  the box cannot crop its own falloff. */
  [[nodiscard]] Element emblemDisc() {
    const sdf::Style emblem{.fill = studio::hex(0x1A1008, 0.62f),
                            .glowRadius = 30.0f,
                            .glowColor = studio::hex(0xFFB13A, 0.5f)};
    const float side = sdf::minBoxFor(emblem, 2.0f * rHub * kR * 0.86f);
    return disc(kEye, side * 0.5f)
        .key("hub-disc")
        .hitTestable(false)
        .fill(sdf::material(sdf::circle(), emblem))
        .blend(SkBlendMode::kPlus)
        .opacity(&litHub);
  }

  /** THE MICRO-SCRIPT BAND: the tablet's own Latin again, a third the
   *  size, tracked shut and set dim — script read as TEXTURE rather than
   *  as words, which is what fills the field between the hub and the
   *  figure without inventing anything to put there. Every glyph is still
   *  a real glyph; a baked tile would be cheaper and would be a lie about
   *  what this plate is. It drifts the other way from the names. */
  [[nodiscard]] Element minuscula() {
    return text(toU8(microText), ital(microSize, kAshDim, 0.0f))
        .key("minuscula")
        .centerAt(kEye)
        .width(2 * rMicro * kR)
        .height(2 * rMicro * kR)
        .onPath({.path = shapes::circle(),
                 .at = &microDrift,
                 .align = TextPath::Align::Start,
                 .offset = -microSize * 0.30f,
                 .autoFlip = false})
        .fx({.effect = fx::hold(fx::rise(microSize * 0.9f)),
             .stagger = stagger(unit::Cluster, {.eachMs = 4,
                                                .durationMs = 300,
                                                .from = Stagger::From::Random,
                                                .seed = 61}),
             .progress = beat(tGloss + 0.3, tGloss + 0.3 + microSpanS)});
  }

  [[nodiscard]] Element wheel() {
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

    // THE FLOOD: light thrown at the whole sheet from behind the wheel.
    // It screens, so it lifts what is already there toward white instead
    // of laying a wash over it, and it is worth its full-panel gradient
    // only while it is on — at gain zero the node is not painted at all.
    panel.child(
        box()
            .key("flood")
            .absolute()
            .inset(-120)
            .hitTestable(false)
            .fill(Material::glowUnit({0.5f, 0.5f}, 0.86f,
                                     {{0.0f, studio::hex(0xFFD98A, 0.55f)},
                                      {0.42f, studio::hex(0xE79A32, 0.30f)},
                                      {1.0f, studio::hex(0xC96F1E, 0.0f)}}))
            .blend(SkBlendMode::kScreen)
            .opacity(&floodA));

    // THE RAYS, thrown past the figure at ignition — the reading that
    // makes an ignition a whole-frame event and not a brighter drawing.
    // They pass BEHIND the lettering: light coming out from the figure is
    // occluded by the figure, and rays laid over the type would only be a
    // veil across the words.
    panel.child(box()
                    .key("rays")
                    .absolute()
                    .inset(-170)
                    .hitTestable(false)
                    .fill(Material::sksl(kRaysSksl).uniform("uInk", kHalo))
                    .blend(SkBlendMode::kPlus)
                    .opacity(&raysA));

    // The compass work: the rules with their hairline companions, and the
    // division ladder in three length classes at two pitches — the density
    // an engraved figure carries and a single ladder cannot.
    panel.child(rule("rule-out", kChalkRimOut, 1.7f, kIron, 0.35, 1.3));
    panel.child(
        rule("rule-out-case", kChalkRimCase, 0.5f, kIronDim, 0.45, 1.2));
    panel.child(rule("rule-rim", kChalkRimIn, 0.9f, kIron, 0.55, 1.3));
    panel.child(rule("rule-nom", kChalkNomIn, 0.8f, kIronDim, 0.75, 1.3));
    panel.child(
        rule("rule-nom-case", kChalkNomCase, 0.5f, kIronDim, 0.85, 1.2));
    panel.child(rule("rule-micro-out", kChalkMicroOut, 0.6f, kIronDim,
                     tGloss + 0.1, 1.1));
    panel.child(rule("rule-micro-in", kChalkMicroIn, 0.6f, kIronDim,
                     tGloss + 0.3, 1.1));
    panel.child(rule("rule-hub", kChalkHub, 0.9f, kIron, tSquare - 0.4, 0.9));
    panel.child(ladder("ticks-long", 8, 0, rTickOut, rTickIn - 0.020f, 1.2f,
                       kAshDim, 0.7, 1.4));
    panel.child(ladder("ticks-mid", 24, 3, rTickOut, rTickIn - 0.008f, 0.9f,
                       kIron, 0.7, 1.6));
    panel.child(
        ladder("ticks-short", 72, 3, rTickOut, rTickIn, 0.8f, kIron, 0.7, 2.1));

    // THE FAST LAYER: a 288-mark hairline ladder turning at seconds per
    // revolution, against the figure inside it turning the other way.
    // Counter-rotation is only legible as MECHANISM when the rates differ
    // by enough to see in one glance, and the ladder is the layer that can
    // afford it — it is cached geometry replayed under a bound transform,
    // where the lettering it sits beside would have to re-place every
    // glyph.
    panel.child(box()
                    .key("ladder-turn")
                    .absolute()
                    .inset(0)
                    .hitTestable(false)
                    .rotate(bind(&ladderSpin).target(0.0f, 360.0f))
                    .child(ladder("ticks-fine", 288, 4, rFineOut, rFineIn, 0.6f,
                                  kIron, 0.9, 2.3)));

    // The bands, hub, figures and roundels.
    panel.child(corona());
    panel.child(nomina());
    panel.child(smaragdina());
    panel.child(minuscula());
    panel.child(figure());
    panel.child(hexagram());
    for (int k = 0; k < 6; ++k) panel.child(satellite(k));
    panel.child(emblemDisc());
    panel.child(quadratum());

    // The light that lands on the struck rules, group by group.
    panel.child(emissive("rim-lit", glows[kGlowRim], &litRim));
    panel.child(emissive("nom-lit", glows[kGlowNom], &litNom));
    panel.child(emissive("hub-lit", glows[kGlowHub], &litHub));

    // The embers: a live pool stamped as one draw, rising off the rim at
    // ignition and drizzling for as long as the wheel is charged.
    panel.child(box()
                    .key("embers")
                    .absolute()
                    .inset(0)
                    .hitTestable(false)
                    .opacity(&emberA)
                    .child(instancing::instances(emberAtlas, embers,
                                                 instancing::Mode::Live,
                                                 SkBlendMode::kPlus)));

    // THE CREST'S FRINGE: half a second of the frame beneath re-sampled
    // with its channels pulled apart along the radius.
    panel.child(box()
                    .key("fringe")
                    .absolute()
                    .inset(0)
                    .hitTestable(false)
                    .backdrop(Effect::shader(fringeEffect(),
                                             {{"uCx", 500.0f}, {"uCy", 500.0f}})
                                  .uniform("uSpread", &fringeK))
                    .opacity(&fringeA));

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
                                      .scaleX(beat(from, to)))
                           // The column inherits the STRIKE and nothing
                           // else: when a stage closes on the wheel its
                           // rule takes the same flash the wheel took, so
                           // the reader's eye is told where to look
                           // without the ledger being lit like the figure.
                           .child(box()
                                      .absolute()
                                      .inset(0)
                                      .hitTestable(false)
                                      .fill(Fill::color(kHalo))
                                      .blend(SkBlendMode::kPlus)
                                      .opacity(pulse(to, to + 0.55, 0.06)))));
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
                        " GLYPHS IN THE FIGVRE \xc2\xb7 15 CVRVED BASELINES "
                        "\xc2\xb7 EVERY START CHAINED FROM A SPAN, NONE FITTED "
                        "BY HAND"),
                   label(9.5f, kAshDim, 2.4f))
                  .key("colophon-2")
                  .opacity(beat(tIgnite + 0.4, tIgnite + 1.2)));
    return col;
  }

  [[nodiscard]] Element describe() {
    return stack()
        .fill(Material::glowUnit({0.3125f, 0.5f}, 0.9f,
                                 {{0.0f, kNightLift}, {1.0f, kNight}}))
        .child(box()
                   .row()
                   .absolute()
                   .inset(0)
                   .opacity(envelope())
                   // The charged wheel breathes: under a percent of scale,
                   // which is not a size change so much as the reading that
                   // a finished circle is holding something in.
                   .child(wheel().scale(&humScale))
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
    microText = std::string(kSmaragdina) + kSmaragdina;
    coronaSize =
        fitToRing(ctx, coronaText, ring(18, kBone, 2.2f), rCorona * kR);
    nominaSize =
        fitToRing(ctx, nominaText, ring(30, kGold, 4.2f), rNomina * kR);
    glossSize =
        fitToRing(ctx, glossText, ital(15, kVerdigris, 1.1f), rGloss * kR);
    microSize =
        fitToRing(ctx, microText, ital(8, kAshDim, 0.0f), rMicro * kR, 0.995f);
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
    microSpanS = stagger(unit::Cluster, {.eachMs = 4,
                                         .durationMs = 300,
                                         .from = Stagger::From::Random,
                                         .seed = 61})
                     .spanMs((uint32_t)glyphsOf(microText)) /
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
                  glyphsOf(glossText) + glyphsOf(microText) +
                  glyphsOf(kSolSquare);
    for (int k = 0; k < 6; ++k)
      totalGlyphs += glyphsOf(satText[k]) + glyphsOf(kPlanets[k].ordo) +
                     glyphsOf(kPlanets[k].dies);

    bakeGeometry();
    ctx.composer.render(describe());
  }

  // ------------------------------------------------------------------
  /** Everything the wheel draws that is a PATH rather than a box, cooked
   *  once before the first describe and never touched again: the chalk's
   *  wobble, each lighting group's unioned emissive stack, and the
   *  hexagram's morph ladder. All of it is content-static, which is what
   *  entitles the nodes that wear it to record once and replay under
   *  bound transforms and bound gains for the rest of the loop. */
  void bakeGeometry() {
    namespace ops = sigil::shape::ops;
    namespace geom = sigil::shape;

    chalk.assign((size_t)kChalkCount, SkPath());
    chalk[kChalkRimOut] = chalkRing(rRimOut, 11);
    chalk[kChalkRimCase] = chalkRing(rRimCase, 23);
    chalk[kChalkRimIn] = chalkRing(rRimIn, 37);
    chalk[kChalkNomIn] = chalkRing(rNomIn, 53);
    chalk[kChalkNomCase] = chalkRing(rNomCase, 71);
    chalk[kChalkMicroOut] = chalkRing(rMicroOut, 89);
    chalk[kChalkMicroIn] = chalkRing(rMicroIn, 101);
    chalk[kChalkHub] = chalkRing(rHub, 113);

    // The figure: the {6/2} star's chords, and six spokes running out to
    // its vertices from just clear of the hub rule, so nothing in this
    // group crosses anything in another one.
    kit::Frame frame{.centre = kEye, .radius = kR};
    const SkPath hex = kit::chords(
        frame, {.sides = 6, .step = 2, .radius = rHex, .closed = true});
    SkPathBuilder spokes;
    for (int k = 0; k < 6; ++k) {
      spokes.moveTo(P((float)k * 60.0f, rHub + 0.02f));
      spokes.lineTo(P((float)k * 60.0f, rHex));
    }
    const SkPath spokePath = spokes.detach();
    chalk[kChalkHex] =
        ops::Roughen{.amplitude = 0.9f, .segmentPx = 40.0f, .seed = 127}(hex);
    chalk[kChalkSpokes] = ops::Roughen{
        .amplitude = 0.7f, .segmentPx = 46.0f, .seed = 139}(spokePath);

    glows.assign((size_t)kGlowCount, Glow{});
    glows[kGlowRim] = bakeGlow({ringPath(rRimOut), ringPath(rRimIn)}, 0.9f);
    glows[kGlowNom] = bakeGlow({ringPath(rNomIn)}, 0.7f);
    glows[kGlowHex] = bakeGlow({hex, spokePath}, 0.85f);
    glows[kGlowHub] = bakeGlow({ringPath(rHub)}, 0.8f);

    // THE STRIKE-MORPH: the light arrives as a scribble and resolves onto
    // the true figure. Both ends are resampled to the same point count on
    // the same arc-length parameterisation and the ladder is the lerp
    // between them, so step 0 is the scribble, the last step is the chords
    // exactly, and nothing in between has to be computed again.
    constexpr int kSteps = 10;
    constexpr int kSamples = 220;
    const SkPath scribble =
        ops::Roughen{.amplitude = 13.0f, .segmentPx = 16.0f, .seed = 211}(hex);
    const std::vector<geom::Sampled> from = geom::resample(scribble, kSamples);
    const std::vector<geom::Sampled> to = geom::resample(hex, kSamples);
    hexSteps.assign((size_t)kSteps, SkPath());
    for (int i = 0; i < kSteps; ++i) {
      const float t = (float)i / (float)(kSteps - 1);
      SkPathBuilder b;
      for (size_t c = 0; c < to.size() && c < from.size(); ++c)
        b.addPath(geom::toPath(geom::lerp(from[c], to[c], t), true));
      hexSteps[(size_t)i] = b.detach();
    }

    // THE EMBERS: one cell baked from a soft dot, and a pool the ticker
    // owns. The stamp is one draw whatever the count, which is what lets
    // the drizzle keep running for the whole charged idle.
    emberAtlas = std::make_shared<instancing::Atlas>(2.0f);
    emberFrame = emberAtlas->cell(
        box().fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                      {{0.0f, studio::hex(0xFFF3D2)},
                                       {0.22f, studio::hex(0xFFD277, 0.85f)},
                                       {0.55f, studio::hex(0xE79A32, 0.30f)},
                                       {1.0f, studio::hex(0xC96F1E, 0.0f)}})),
        {22, 22});
    embers = std::make_shared<instancing::Pool>();
    embers->resize(kEmbers);
  }

  /** THE STRIKE, and what it leaves burning. Three readings compose into
   *  one scalar per lighting group, which is why they are stepped here
   *  rather than declared as a window each: the cross-fade that lands the
   *  light on a struck rule, the flash the strike itself throws — up in
   *  two frames, down over half a second, the shape a hot thing has and a
   *  fade does not — and, once the wheel is charged, the breath that keeps
   *  it from ever reading as a finished picture again. */
  float strike(double tc, double at, float rest) const {
    if (tc < at) return 0.0f;
    const double since = tc - at;
    const float onset = (float)std::min(1.0, since / 0.30);
    const float flash = (float)std::exp(-since / 0.34) * 1.0f;
    const float charged =
        tc > tIgnite ? 0.10f + 0.05f * (float)std::sin((tc - tIgnite) * 1.35)
                     : 0.0f;
    return std::min(1.0f,
                    onset * (rest + charged) + flash * (1.0f - rest * 0.4f));
  }

  /** The ignition's own envelope: a fast rise to the crest and a long
   *  decay, so what follows the crest is an AFTERGLOW and not a cut. */
  static float burst(double tc, double at, double rise, double fall) {
    if (tc < at) return 0.0f;
    const double since = tc - at;
    if (since < rise) return (float)(since / rise);
    return (float)std::exp(-(since - rise) / fall);
  }

  /** Every gain the fire drives, plus the ember pool. */
  void stepFire(double tc) {
    litRim = strike(tc, tBand + bandSpanS, 0.34f);
    litNom = strike(tc, tNames + namesSpanS, 0.30f);
    litHex = strike(tc, tHex + 1.1, 0.44f);
    litHub = strike(tc, tSquare + squareSpanS * 0.9, 0.46f);
    for (int k = 0; k < 6; ++k)
      litSat[k] = strike(tc, tSat[k] + 0.25 + satSpanS, 0.30f);

    // The morph walks its ladder over the strike's own half-second.
    const double morphAt = tHex + 1.1;
    // The walk runs from one step BEFORE the first to one PAST the last,
    // so the ladder is dark at both ends: nothing of the scribble stands
    // on the plate before the strike, and nothing of it is left behind
    // when the emissive stack takes the figure over.
    morphStep = -1.0f + (float)(std::clamp((tc - morphAt) / 0.78, 0.0, 1.0) *
                                (double)(hexSteps.size() + 1));

    // IGNITION IS FULL-FRAME. The rays are the widest and the shortest,
    // the flood the deepest, the fringe half a second at the crest alone.
    raysA = 0.85f * burst(tc, tIgnite + 0.15, 0.22, 0.85);
    floodA = 0.9f * burst(tc, tIgnite + 0.1, 0.30, 1.5) +
             (tc > tIgnite + 2.0
                  ? 0.06f + 0.03f * (float)std::sin((tc - tIgnite) * 1.35)
                  : 0.0f);
    const float fringe = 1.7f * burst(tc, tIgnite + 0.26, 0.09, 0.20);
    fringeK = fringe;
    fringeA = fringe > 0.02f ? 1.0f : 0.0f;
    humScale =
        tc > tIgnite
            ? 1.0f + 0.005f * (float)std::sin((tc - tIgnite) * 1.35 - 1.57)
            : 1.0f;

    stepEmbers(tc);
  }

  /** THE EMBERS: seeded on the rim, thrown up at ignition, then a sparse
   *  drizzle for as long as the wheel is charged. Each spark is a pure
   *  function of its index and the time since ignition, so the pool needs
   *  no per-frame state and the loop's wrap re-deals it exactly. */
  void stepEmbers(double tc) {
    if (!embers) return;
    const double since = tc - tIgnite;
    emberA = since < 0 ? 0.0f
                       : std::min(1.0f, (float)(since / 0.25)) *
                             (0.30f + 0.70f * burst(tc, tIgnite, 0.25, 1.1));
    if (emberA.value() <= 0.0f) return;
    auto positions = embers->positions();
    auto scales = embers->scales();
    auto tints = embers->tints();
    auto frames = embers->frames();
    for (int i = 0; i < kEmbers; ++i) {
      const float u = (float)i / (float)kEmbers;
      const float seed = std::fmod(u * 7919.0f, 1.0f);
      const float life = 2.2f + seed * 2.4f;
      const float age = (float)std::fmod(std::max(0.0, since) + u * life, life);
      const float t = age / life;
      const float th = (u * 360.0f + seed * 53.0f) * kDeg;
      // Struck off the RIM and rising: the sparks belong to the edge of
      // the figure, not to the field it encloses, and lettering is not
      // improved by fireflies over it.
      const float r = kR * (0.905f + 0.10f * seed);
      const float sway = std::sin(age * 1.9f + seed * 6.28f) * 13.0f;
      positions[(size_t)i] = {kEye.x() + r * std::sin(th) + sway,
                              kEye.y() - r * std::cos(th) - t * 190.0f};
      scales[(size_t)i] = (0.45f + seed * 0.75f) * (1.0f - t * 0.5f);
      const float a = std::pow(1.0f - t, 1.4f);
      tints[(size_t)i] = {1.0f, 0.93f - 0.12f * seed, 0.74f, a};
      frames[(size_t)i] = emberFrame;
    }
  }

  void update(double elapsed,
              sigil::compose::sketch::SketchContext& ctx) override {
    const double tc = std::fmod(elapsed, loopSecs);
    cycle = (float)tc;
    secs = (float)elapsed;

    // THE WHEEL'S TURNING, phased so nothing moves until it has formed,
    // and RATED so the counter-rotation is legible. The periods are
    // seconds per revolution and not minutes, because a rate the eye
    // cannot resolve in one glance is not mechanism, it is drift: the
    // geometry layers take the fast rates, the lettering the slow ones,
    // and neighbours turn opposite ways.
    const double namesOn = tNames + namesSpanS;
    nominaDrift =
        (float)(tc > namesOn ? std::fmod((tc - namesOn) / 78.0, 1.0) : 0.0);
    coronaDrift =
        (float)(tc > tIgnite ? 1.0 - std::fmod((tc - tIgnite) / 118.0, 1.0)
                             : 0.0);
    microDrift =
        (float)(tc > tGloss ? 1.0 - std::fmod((tc - tGloss) / 46.0, 1.0) : 0.0);
    ladderSpin = (float)std::fmod(std::max(0.0, tc - 2.1) / 21.0, 1.0);
    const double figureOn = tHex + 1.3;
    hexSpin =
        (float)(tc > figureOn ? 1.0 - std::fmod((tc - figureOn) / 34.0, 1.0)
                              : 0.0);
    for (int k = 0; k < 6; ++k) {
      const double on = tSat[k] + 0.25 + satSpanS;
      const double turn = tc > on ? (tc - on) / (k % 2 ? -17.0 : 17.0) : 0.0;
      satDrift[k] = (float)(turn - std::floor(turn));
      const double spinOn = tSat[k] + 0.3;
      const double spin =
          tc > spinOn ? (tc - spinOn) / (k % 2 ? 12.0 : -12.0) : 0.0;
      satSpin[k] = (float)(spin - std::floor(spin));
    }

    stepFire(tc);

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
