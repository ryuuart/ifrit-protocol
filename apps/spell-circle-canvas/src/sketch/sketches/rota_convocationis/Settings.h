#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Style.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace ch = choreograph;
namespace motion = sigil::motion;
namespace mskia = sigil::material::skia;
namespace sdf = sigil::material::sdf;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

namespace rota_convocationis {}
using namespace rota_convocationis;
namespace rota_convocationis {

// A magic circle wants a square field: the figure is the picture, and a
// panoramic sheet would only be two dark wings either side of it.
constexpr float kW = 1280.0f;
constexpr float kH = 1280.0f;

// ---- palette: chalk by candlelight, then one hue of light -----------------
constexpr SkColor4f kNight = hexColor(0x0A0812);
constexpr SkColor4f kNightLift = hexColor(0x141021);
constexpr SkColor4f kGold = hexColor(0xD8A94E);
constexpr SkColor4f kBone = hexColor(0xE9DFC8);
constexpr SkColor4f kEmber = hexColor(0x8A4A26);
constexpr SkColor4f kIron = hexColor(0x3B3554);     // construction lines
constexpr SkColor4f kIronDim = hexColor(0x262238);  // faint construction
constexpr SkColor4f kAsh = hexColor(0x8A8299);      // secondary type
constexpr SkColor4f kAshDim = hexColor(0x8A8299, 0.62f);
constexpr SkColor4f kRuneInk = hexColor(0x9C8FB8);  // the register's ink

// ---- the ignited palette: ONE hue family, value doing the drawing --------
// Not a second scheme beside the candlelit one. It is the state the circle
// changes INTO: chalk is pigment on a surface and reflects what the room
// gives it, light is emitted and collapses to one hue with a white core.
constexpr SkColor4f kCore = hexColor(0xFFF6E2);   // the white-hot core
constexpr SkColor4f kHalo = hexColor(0xFFC152);   // the saturated halo
constexpr SkColor4f kBloom = hexColor(0xC96F1E);  // the wide dim bloom

// ---- the circle's frame ---------------------------------------------------
constexpr SkPoint kEye{640.0f, 640.0f};  // centre in canvas px
constexpr float kR = 545.0f;             // the greatest circle

// THE RADIUS TABLE, in units of the greatest circle. Read it as the bands
// it makes, because that is how it was built: a band is a PAIR OF RULES
// with something written or drawn between them, and no rule stands alone.
// Two radii are not free — they are where a star's chords run tangent, so
// the figure inside decides where the circle under it goes. Two more are
// the seal ride below, which the same rule sets from this table's own
// fences.
constexpr float rEdge = 1.000f;      // ─┐ the serration: 240 teeth  ─┐
constexpr float rEdgeIn = 0.980f;    // ─┘                            │ THE
constexpr float rRuneOut = 0.958f;   // ─┐ THE REGISTER — the rune    │ SEAL
constexpr float rRune = 0.934f;      //   │ band, and its baseline    │ RIDE
constexpr float rRuneIn = 0.910f;    // ─┘                           ─┘
constexpr float rVoxOut = 0.898f;    // ─┐ THE INVOCATION — Latin majuscule
constexpr float rVox = 0.872f;       //   │
constexpr float rVoxIn = 0.846f;     // ─┘
constexpr float rTickOut = 0.836f;   // ─┐ the division ladder, three classes
constexpr float rTickMid = 0.818f;   // ─┤ …fenced like any other band, with
constexpr float rTickIn = 0.800f;    // ─┘ the turning ladder in its own half
constexpr float rNomOut = 0.790f;    // ─┐ THE NINE NAMES — the charged band
constexpr float rNom = 0.752f;       //   │
constexpr float rNomIn = 0.708f;     // ─┘
constexpr float rNomCase = 0.694f;   //   its hairline companion
constexpr float rArcOut = 0.676f;    // ─┐ the arc layer: two broken rings,
constexpr float rArcIn = 0.640f;     // ─┘ offset half a pitch, spoked
constexpr float rTexOut = 0.622f;    // ─┐ THE TEXTURE — the small register
constexpr float rTex = 0.598f;       //   │
constexpr float rTexIn = 0.572f;     // ─┘
constexpr float rTexCase = 0.560f;   // ─┐ a fine serration, the texture
constexpr float rSerrIn = 0.534f;    // ─┘ band's own companion ladder
constexpr float rStar = 0.520f;      // {12/3} vertices
constexpr float rCrescOut = 0.492f;  // ─┐ THREE crescents, 120° apart, laid
constexpr float rCrescIn = 0.454f;   // ─┘ ACROSS the compound: the one
                                     //   three-fold mark on the plate

// THE CAGE FLOOR: the annulus under the star, fenced like every other
// band and filled at the plate's own two pitches.
constexpr float rCageOut = 0.446f;
constexpr float rCageIn = 0.378f;
constexpr float rEnv = 0.368f;       // ─┐ where the {12/3} chords run
constexpr float rEnvIn = 0.356f;     // ─┘ tangent: a circle it decides
constexpr float rInner = 0.348f;     // {12/4} vertices
constexpr float rHubOut = 0.174f;    // ─┐ where the {12/4} chords run
constexpr float rHubIn = 0.160f;     // ─┘ tangent: the hub's own rule
constexpr float rHubCase = 0.150f;   //   the emblem's outermost line
constexpr float rEmblem = 0.140f;    //   the disc of light
constexpr float rHexagram = 0.118f;  //   the emblem's own figure
constexpr float rHubMote = 0.058f;   //   six motes inside the kern ring
constexpr float rHubKern = 0.104f;   //   …and the ring its chords enclose:
                                     //   three nested circles, so the centre
                                     //   steps down as the plate does rather
                                     //   than stopping dead at one line
constexpr float kSpurR = 23.0f;      // the one off-order mark, px

// THE SEAL RIDE — where the twelve sub-seals stand, and how big they are.
// Both numbers are read off the bands rather than chosen: a seal's inner
// edge lands on the register's inner rule and its outer edge stands proud
// of the greatest circle, so ONE SEAL IS EXACTLY AS TALL AS THE GROUND IT
// STANDS ON — the register and the serration together — and breaks the
// rim on its way out. That is the reason the seals are out here and not
// at mid radius: the rim is where a small circle can cover a whole band
// group without touching anything that is meant to be read, and the band
// it covers is script-shaped rather than readable. The invocation, one
// fence further in, is never crossed.
constexpr float rSealLip = 1.032f;  // how far a seal stands past rEdge
constexpr float rSealRide = (rRuneIn + rSealLip) * 0.5f;    // seal centres
constexpr float kSealR = (rSealLip - rRuneIn) * 0.5f * kR;  // ≈33 px
constexpr float kSealRing = kSealR * 0.78f;  // its ring-text baseline

// ---- the writing pace -----------------------------------------------------
constexpr float kStepMs = 55.0f;    // word to word round the band
constexpr float kCrossMs = 190.0f;  // the scribe's extra pause at a cross
constexpr float kSealBeat = 0.55f;  // fraction of a seal's span before the
                                    // next one starts forming

constexpr float kDeg = 3.14159265358979f / 180.0f;
constexpr int kStations = 12;        // the circle's rotational symmetry
constexpr int kSeals = kStations;    // …and the seals', one per station
constexpr int kLimens = kSeals / 2;  // the thresholds named on the chords
constexpr int kEmbers = 96;  // the rim's rising sparks, then the drizzle

// ---- content: invented, and invented in the open --------------------------

/** THE REGISTER — an invented alphabet, not a language.
 *
 *  The letterforms are Tifinagh's (U+2D30), taken for their shapes alone:
 *  a vocabulary of rings, crosses, bars and dotted figures, which is the
 *  vocabulary a circle of this kind already speaks. The subset below
 *  drops the forms that read as Latin letters, because a stray E in a
 *  rune band is the one thing that breaks it.
 *
 *  Words are DEALT rather than written: a seeded walk gives every band its
 *  own text, deterministically, so the plate is the same plate on every
 *  run and on every machine. An invented script has no orthography to be
 *  wrong about; a plate that re-dealt itself would not be one plate. */
const char32_t kRegister[] = {
    U'ⴰ', U'ⴱ', U'ⴳ', U'ⴴ', U'ⴵ', U'ⴶ', U'ⴷ', U'ⴹ', U'ⴺ', U'ⴻ', U'ⴼ',
    U'ⴽ', U'ⴿ', U'ⵀ', U'ⵁ', U'ⵂ', U'ⵃ', U'ⵄ', U'ⵅ', U'ⵆ', U'ⵇ', U'ⵉ',
    U'ⵊ', U'ⵌ', U'ⵍ', U'ⵎ', U'ⵏ', U'ⵑ', U'ⵓ', U'ⵔ', U'ⵕ', U'ⵖ', U'ⵗ',
    U'ⵙ', U'ⵚ', U'ⵛ', U'ⵜ', U'ⵝ', U'ⵟ', U'ⵡ', U'ⵢ', U'ⵣ', U'ⵥ',
};
constexpr int kRegisterN = (int)(sizeof(kRegister) / sizeof(kRegister[0]));

/** THE SMALL REGISTER — a second alphabet for the texture band, so the
 *  two innermost script rings are visibly not the same script. Canadian
 *  Aboriginal syllabics (U+1400) read as triangles, arcs and dotted
 *  wedges at four points, which is exactly what a texture band wants: a
 *  rhythm of marks rather than a run of letters. */
const char32_t kSmallRegister[] = {
    U'ᐁ', U'ᐃ', U'ᐅ', U'ᐊ', U'ᐍ', U'ᐐ', U'ᐓ', U'ᐠ', U'ᐯ', U'ᐱ', U'ᐺ', U'ᑉ',
    U'ᑌ', U'ᑐ', U'ᑦ', U'ᑭ', U'ᑲ', U'ᒃ', U'ᒋ', U'ᒣ', U'ᒻ', U'ᓀ', U'ᓓ', U'ᓪ',
    U'ᔁ', U'ᔦ', U'ᕋ', U'ᕕ', U'ᕝ', U'ᕰ', U'ᖏ', U'ᖦ', U'ᗞ', U'ᘁ', U'ᘔ', U'ᘢ',
};
constexpr int kSmallRegisterN =
    (int)(sizeof(kSmallRegister) / sizeof(kSmallRegister[0]));

/** Append @p cp to @p out as UTF-8. The bands are built as bytes because
 *  everything downstream — measuring, fitting, glyph counting — speaks
 *  the same encoding the content literals do. */
inline void appendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out += (char)cp;
  } else if (cp < 0x800) {
    out += (char)(0xC0u | (cp >> 6u));
    out += (char)(0x80u | (cp & 0x3Fu));
  } else {
    out += (char)(0xE0u | (cp >> 12u));
    out += (char)(0x80u | ((cp >> 6u) & 0x3Fu));
    out += (char)(0x80u | (cp & 0x3Fu));
  }
}

/** @p words words of @p lo…@p hi letters, space-separated, dealt from one
 *  of the two registers. The draw is fixed-width integer arithmetic, so
 *  the same seed gives the same band wherever this runs; the `| 1u`
 *  keeps the stream off the state every shift fixes. */
inline std::string deal(uint32_t seed, int words, int lo, int hi,
                        bool small = false) {
  const char32_t* set = small ? kSmallRegister : kRegister;
  const int n = small ? kSmallRegisterN : kRegisterN;
  std::string out;
  uint32_t s = seed | 1u;
  for (int w = 0; w < words; ++w) {
    sigil::core::noise::xorshiftNext(s);
    const int len = lo + (int)(s % (uint32_t)(hi - lo + 1));
    for (int i = 0; i < len; ++i) {
      sigil::core::noise::xorshiftNext(s);
      appendUtf8(out, set[s % (uint32_t)n]);
    }
    out += ' ';
  }
  return out;
}

/** THE NINE NAMES the charge runs. Invented, and built to be pronounced —
 *  a name band is read as a name band because the runs look like words a
 *  mouth could close on, whatever they mean. */
inline const char* kNames[9] = {"AZRAEVN", "VELMOTH",  "SIRAKEL",
                                "OMBRIAX", "THELVNE",  "KARANDIS",
                                "VOXIMER", "HALDRETH", "ZEPHARIN"};

/** THE INVOCATION — this study's own Latin, in the band-broken-by-crosses
 *  form. Eight phrases; the leading cross closes the ring at the seam,
 *  and each says something the circle then does. */
inline const char* kInvocatio[8] = {
    "IN PRINCIPIO CIRCVLVS SCRIBITVR", "LITTERA SVRGIT IN ORBEM",
    "DVODECIM RADII CONVENIVNT",       "BIS SENA SIGILLA VIGILANT",
    "STELLA IN STELLA VOLVITVR",       "NOMEN IN CORONA FRANGITVR",
    "ORDO EX ORDINE NASCITVR",         "ET SIGILLVM VIVIT"};

/** THE TWELVE SEALS riding the rim, one to a station. Each is a small
 *  circle of its own: two words of the register — the band the seal
 *  stands on, said back inside it — and an ordinal that decodes at its
 *  centre. The polygon ladder runs three to eight and then repeats, so a
 *  seal and the seal opposite it carry the same figure and the twelve
 *  read as two sixes, which is this study's arithmetic and nobody
 *  else's. */
struct Seal {
  const char* ordo;  // the ordinal, as the seal's centre writes it
  int order;         // …and the spinning polygon's sides
};
constexpr Seal kSealTable[kSeals] = {
    {"I", 3},   {"II", 4},   {"III", 5}, {"IV", 6}, {"V", 7},  {"VI", 8},
    {"VII", 3}, {"VIII", 4}, {"IX", 5},  {"X", 6},  {"XI", 7}, {"XII", 8},
};

/** THE THRESHOLDS, named on the outer compound's chords: six of the
 *  twelve, the odd ones. The seals ride the rim and the chords are bands
 *  deeper in, so no caption can stand beside the seal it names: the
 *  correspondence is carried by the numbering alone, which is how a plate
 *  says two things belong together without drawing a line between them. */
constexpr const char* kLimina[kLimens] = {
    "LIMEN PRIMVM",   "LIMEN TERTIVM", "LIMEN QVINTVM",
    "LIMEN SEPTIMVM", "LIMEN NONVM",   "LIMEN VNDECIMVM",
};

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

/** The charge's ABI: one colour, the gold the flash lays on the names. */
struct ChargeParams {
  sigil::material::Color uGold;
};

/** The definition, made where it is asked for — which is ONCE: a recipe's
 *  identity is the object, so a second one of this body compiles a second
 *  program and never compares equal to the first. Asking for one per
 *  describe would be that mistake; a static holding one would hold it in a
 *  dylib a hot reload unloads. */
inline std::shared_ptr<const sigil::material::Recipe> chargeRecipe() {
  return std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<ChargeParams>("rota.charge")
          .body(sigil::material::Target::SkSL, kChargeSksl));
}

// ---- helpers --------------------------------------------------------------

/** Circle-frame polar → canvas px. θ clockwise from 12 o'clock, the
 *  direction every band here is written in. */
inline SkPoint P(float thDeg, float rNorm) {
  // Twelve o'clock is where the ellipse's own angle starts a quarter turn
  // back, which is what makes this wheel's clockwise-from-twelve reading
  // the ordinary ring arithmetic.
  return arrange::onEllipse({kEye.x(), kEye.y()}, {rNorm * kR, rNorm * kR},
                            thDeg * kDeg - 1.5707963f);
}

/** The pitch of the twelve stations, and the angle of station k. */
constexpr float kPitch = 360.0f / (float)kStations;
inline float station(int k) { return kPitch * (float)k; }

// Slots in the two baked tables. The chalk table holds one wobbled
// outline per construction line; the glow table one emissive stack per
// LIGHTING GROUP — a set of lines that ignite together, may cross each
// other freely, and TURN TOGETHER, because a group is baked as one union
// and a union can only be rotated whole.
enum : int {
  kChalkEdge,
  kChalkEdgeIn,
  kChalkRuneOut,
  kChalkRuneIn,
  kChalkVoxOut,
  kChalkVoxIn,
  kChalkTickMid,
  kChalkNomOut,
  kChalkNomIn,
  kChalkNomCase,
  kChalkTexOut,
  kChalkTexIn,
  kChalkTexCase,
  kChalkSerrIn,
  kChalkEnv,
  kChalkEnvIn,
  kChalkHubOut,
  kChalkHubIn,
  kChalkHubCase,
  kChalkHubKern,
  kChalkArcs,
  kChalkDash,
  kChalkCresc,
  kChalkStar,
  kChalkInner,
  kChalkHexagram,
  kChalkCount,
};
enum : int {
  kGlowRim,    // the outer rules and the register's frame
  kGlowNom,    // the invocation's and the names' frames
  kGlowArc,    // the broken arcs, their spokes, nodes and the dashed ring
  kGlowStar,   // the {12/3} compound, its spokes, nodes and crescents
  kGlowInner,  // the {12/4} compound and its spokes
  kGlowHub,    // the hub's rules and the emblem's figures
  kGlowSpur,   // the one off-order medallion, straddling the rim
  kGlowCount,
};

/** THE EMISSIVE OUTLINE of one lighting group, as four nested regions.
 *  Every line of the group is expanded to a region and the regions are
 *  UNIONED, so a place where two lines cross is covered ONCE; the four
 *  grades are that union grown three more times. Painting them additively
 *  is then safe, which is the whole reason for the union: stroke over
 *  stroke under `kPlus` prints every junction at twice the brightness of
 *  the lines running through it, and a figure this dense is mostly
 *  junctions — it would light as a constellation of bright knots instead
 *  of one circuit of even light. */
/** ONE GRADE of a glow, cooked once: the union re-based to its own bounds'
 *  origin, and the box that origin sits in. A node is a box with a local
 *  shape, and a region worked out in the sheet's frame is neither — split
 *  here rather than at describe time, because a path re-based every frame
 *  is a new path every frame and the node it dresses re-records forever.
 *  The box is the region's own extent, so a grade covers the light it
 *  paints and not the whole sheet. */
struct Grade {
  SkPath local;
  SkRect box = SkRect::MakeEmpty();
};

struct Glow {
  Grade core, halo, mid, bloom;
};

/** A circle of the figure as a path in canvas px. */
inline SkPath ringPath(float rNorm) {
  SkPathBuilder b;
  b.addOval(SkRect::MakeLTRB(kEye.x() - rNorm * kR, kEye.y() - rNorm * kR,
                             kEye.x() + rNorm * kR, kEye.y() + rNorm * kR));
  return b.detach();
}

/** N ARC SEGMENTS on one ring, each spanning @p spanDeg of the station
 *  pitch and centred on a station offset by @p fromDeg. An arc that stops
 *  short of its neighbour is what makes a ring read as a mechanism rather
 *  than as another circle, and offsetting one ring half a pitch from the
 *  ring beside it is what makes the pair read as two. */
inline SkPath arcRing(float rNorm, int count, float spanDeg, float fromDeg) {
  SkPathBuilder b;
  const float rad = rNorm * kR;
  const SkRect oval = SkRect::MakeLTRB(kEye.x() - rad, kEye.y() - rad,
                                       kEye.x() + rad, kEye.y() + rad);
  for (int k = 0; k < count; ++k) {
    // Skia measures from due east; the table above is measured from
    // twelve o'clock, which is the whole difference.
    const float mid = arrange::along(fromDeg, 360.0f, (size_t)k, (size_t)count,
                                     arrange::Turn::Closed) -
                      90.0f;
    b.addArc(oval, mid - spanDeg * 0.5f, spanDeg);
  }
  return b.detach();
}

/** N RADIAL SPOKES between two radii. Three sets of these cross the
 *  figure at three depths, each set living in the layer it crosses, so a
 *  spoke never runs from one turning layer into another. */
inline SkPath spokeRing(int count, float r0, float r1, float fromDeg) {
  SkPathBuilder b;
  for (int k = 0; k < count; ++k) {
    const float th = arrange::along(fromDeg, 360.0f, (size_t)k, (size_t)count,
                                    arrange::Turn::Closed);
    b.moveTo(P(th, r0));
    b.lineTo(P(th, r1));
  }
  return b.detach();
}

/** N CRESCENTS: a pair of concentric arcs closed at both ends by a radial
 *  tie, with a short ladder of @p rungs across the gap. A crescent spans a
 *  fraction of the circle and belongs to no ring — which is what makes it
 *  read as an applied mark rather than as another band, and what lets a
 *  plate that counts twelve everywhere else carry three of something. The
 *  fenced gap is the plate's own rule applied to a mark: even here, the
 *  ladder runs BETWEEN two lines. */
inline SkPath crescentRing(float rOut, float rIn, int count, float spanDeg,
                           float fromDeg, int rungs) {
  SkPathBuilder b;
  const float ro = rOut * kR;
  const float ri = rIn * kR;
  const SkRect ovalOut = SkRect::MakeLTRB(kEye.x() - ro, kEye.y() - ro,
                                          kEye.x() + ro, kEye.y() + ro);
  const SkRect ovalIn = SkRect::MakeLTRB(kEye.x() - ri, kEye.y() - ri,
                                         kEye.x() + ri, kEye.y() + ri);
  for (int k = 0; k < count; ++k) {
    const float mid = arrange::along(fromDeg, 360.0f, (size_t)k, (size_t)count,
                                     arrange::Turn::Closed);
    const float lo = mid - spanDeg * 0.5f;
    b.addArc(ovalOut, lo - 90.0f, spanDeg);
    b.addArc(ovalIn, lo - 90.0f, spanDeg);
    for (int r = 0; r <= rungs; ++r) {
      const float th = arrange::along(lo, spanDeg, (size_t)r, (size_t)rungs + 1,
                                      arrange::Turn::Open);
      // The two ends are full ties; the rungs between them are stubs off
      // the inner arc, so the mark reads as a bracket and not as a grid.
      const bool end = r == 0 || r == rungs;
      b.moveTo(P(th, rIn));
      b.lineTo(P(th, end ? rOut : rIn + (rOut - rIn) * 0.45f));
    }
  }
  return b.detach();
}

/** N SMALL CIRCLES standing on a ring — the furniture that sits at every
 *  star vertex and at every arc's station. They are drawn as one path so
 *  twelve nodes cost one node. */
inline SkPath nodeRing(int count, float rNorm, float px, float fromDeg) {
  SkPathBuilder b;
  for (int k = 0; k < count; ++k) {
    const SkPoint c = P(arrange::along(fromDeg, 360.0f, (size_t)k,
                                       (size_t)count, arrange::Turn::Closed),
                        rNorm);
    b.addOval(SkRect::MakeLTRB(c.fX - px, c.fY - px, c.fX + px, c.fY + px));
  }
  return b.detach();
}

/** THE HAND-DRAWN RULE: the compass wobbles, the pen has a nib. A rule
 *  struck in chalk is never a perfect circle, and the eye reads the
 *  difference immediately — which is what makes the clean emissive circle
 *  landing on top of it at the strike say "ignited" rather than
 *  "brighter". The seed is the line's own index, so two neighbouring
 *  rules wobble differently and no two share a wave. */
inline SkPath chalked(const SkPath& line, uint32_t seed,
                      float amplitude = 1.15f) {
  return sigil::geometry::path::ops::Roughen{
      .amplitude = amplitude, .segmentPx = 34.0f, .seed = seed, .smooth = true}(
      line);
}

/** A LINE as a region of the given half-width. Stroke expansion, and not
 *  `ops::offset`, because offset unites its source with the expansion —
 *  correct for growing a region, and for a CLOSED line (a circle, a star
 *  compound's rings) it would hand back the interior as well. A rule is a
 *  line and never a disc. */
inline SkPath expand(const SkPath& line, float halfWidth) {
  SkPaint p;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halfWidth * 2.0f);
  p.setStrokeJoin(SkPaint::kRound_Join);
  p.setStrokeCap(SkPaint::kRound_Cap);
  return skpathutils::FillPathWithPaint(line, p);
}

/** The group's lines widened four times and unioned at each width. */
inline Glow bakeGlow(const std::vector<SkPath>& lines, float coreHalf) {
  namespace ops = sigil::geometry::path::ops;
  auto at = [&](float k) {
    std::vector<SkPath> regions;
    regions.reserve(lines.size());
    for (const SkPath& line : lines)
      regions.push_back(expand(line, coreHalf * k));
    const SkPath united = ops::unite(regions);
    const SkRect box = united.getBounds();
    return Grade{
        united.makeTransform(SkMatrix::Translate(-box.left(), -box.top())),
        box};
  };
  return Glow{at(1.0f), at(2.8f), at(7.0f), at(18.0f)};
}

/** THE RAYS: radial streaks thrown past the figure at ignition, sampled
 *  on angle so the count is a number and not a hundred nodes. The falloff
 *  is a window in radius — nothing at the hub, nothing past the sheet —
 *  and the streaks' own widths come from two beats of a hashed angle, so
 *  the fan reads as unequal spokes of light rather than a gear. */
constexpr const char* kRaysSksl = R"(
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

/** The rays' ABI. The body reads the node's box, which the recipe declares
 *  as a frame input rather than a field: the runtime fills it. */
struct RaysParams {
  sigil::material::Color uInk;
};

inline std::shared_ptr<const sigil::material::Recipe> raysRecipe() {
  return std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<RaysParams>("rota.rays")
          .frame(sigil::material::FrameInput::Resolution)
          .body(sigil::material::Target::SkSL, kRaysSksl));
}

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

/** The fringe, compiled where it is asked for. The caller holds it for
 *  the length of a declaration: a `static` here would outlive the dylib
 *  a hot-reloaded sketch is compiled into, and an effect compared by
 *  pointer after that reload points into code that is gone. */
inline sk_sp<SkRuntimeEffect> fringeEffect() {
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(kFringeSksl));
  if (!effect) std::fprintf(stderr, "[rota] fringe: %s\n", err.c_str());
  return effect;
}

inline int glyphsOf(const std::string& s) {
  int n = 0;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = (unsigned char)s[i];
    const int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    if (c != ' ' && c != '\n') ++n;
    i += (size_t)len;
  }
  return n;
}

}  // namespace rota_convocationis

// ===========================================================================
