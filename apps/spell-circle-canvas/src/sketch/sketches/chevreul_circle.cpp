// chevreul_circle.cpp — "1er CERCLE CHROMATIQUE DE Mr CHEVREUL,
// RENFERMANT LES COULEURS FRANCHES", Plate V of M.-E. Chevreul, *Des
// couleurs et de leurs applications aux arts industriels à l'aide des
// cercles chromatiques*, Paris: J.-B. Baillière et fils, 1864; engraved
// by R. H. Digeon (chromochalcographie), printed by Lamoureux.
// Rebuilt as what it is — a MEASURING INSTRUMENT — with the law it was
// built to serve demonstrated beside it.
//
// ===========================================================================
// DOCUMENTED — read directly and cited
// ===========================================================================
//   * Chevreul, *The Principles of Harmony and Contrast of Colours*,
//     trans. Charles Martel (London: Longman / Bohn) — the English text of
//     the 1839 *De la loi du contraste simultané des couleurs*, in numbered
//     paragraphs, so every quotation below is locatable.
//     https://archive.org/details/principlesofharm00chev
//       §6    the four complementary statements
//       §16   the law itself, quoted verbatim on the plate
//       §20   "the seventeen observations are all of them"
//       §21–37 the seventeen juxtapositions, transcribed complete
//       §38   "do we know of two coloured bodies … Certainly not!"
//       §160  the luminosity claim (yellow lighter, blue darker than red)
//       §161  the construction: 120° → 60° → 30° → ÷5, and the diameter rule
//       §162  "the seventy-two scales"
//       §163–165 the quadrant, ten radii, twenty tones, normal grey
//   * Plate V itself. Science History Institute Digital Collections,
//     https://digital.sciencehistory.org/works/d217qp656 — 2880×3789
//     derivative studied here; catalogue caption reproduced in the
//     medallion; printer of plates Lamoureux, 1864. Physical leaf 37 cm.
//   * Plate VI (2ème cercle, couleurs rabattues à 1/10 de noir),
//     https://digital.sciencehistory.org/works/6t053g131 — confirms the
//     1864 atlas realises §163's quadrant as ten printed circles.
//   * B. MacEvoy, handprint.com/HP/WCL/chevreul.html — Gobelins director of
//     dyes 1824–52; the 1839 book's 22-step tone scale against the 1861/64
//     circles' 20, treated here as a difference between two presentations.
//   * "Cercles chromatiques / Chevreul", colorants.hypotheses.org — the
//     72 stated the other way: 3 primaries + 23 + 23 + 23.
//
// ===========================================================================
// RECONSTRUCTED — measurement and reading, not citation
// ===========================================================================
//   * THE 72 HEXES. Medians of one photograph of one 161-year-old print:
//     a least-squares circle fit over 720 rays (centre 1416.7, 1722.2;
//     R 1004.3 px), sector separators from the angular saturation profile
//     folded at 5° (minimum at 0.7°+5k), median RGB over ±1.5° × radii
//     520–940 px. The CORRECTED column divides out the plate's own
//     unprinted paper — #EFE8D9, six margin patches — IN LINEAR LIGHT and
//     re-encodes. That is a defensible one-line von Kries and still a
//     reconstruction: it says nothing about the scanner's profile, which
//     is undeclared.
//   * THE ANGULAR ANCHOR. The rim cell at 93.2° reads ROUGE; VERT lands at
//     273.2°, exactly 180° away. The plate is rotated 3.2° in this scan.
//     THIS FIGURE IS BUILT AT 90.0° (ROUGE straight down, VERT straight up,
//     as the plate is composed) so that the 180.00° separation stays a
//     MEASUREMENT of the plate rather than a property of my arithmetic.
//   * THE GREY GAMME. §164 gives black and white "in varying quantity" and
//     publishes no numbers. Equal quantities of pigment ≈ equal
//     REFLECTANCE, so it is generated here as Y = (20−t)/19, sRGB-encoded.
//     The equal-code-value ramp beside it is the modern demonstration
//     convention, not Chevreul's.
//   * INDIGO = BLEU-VIOLET (54). Chevreul never assigns indigo a sector;
//     BLEU-VIOLET is the only one of his twelve names sitting where
//     Newton's indigo does. "Greenish-Yellow" is read as JAUNE-VERT (30),
//     the nearest of the twelve.
//   * THE INDEX RING inside the colour band is not on the plate. It is
//     added so the +36 rule is checkable by eye, and labelled as such.
//   * EVERY NUMBER IN THE VERIFICATION BLOCK is recomputed by verify() at
//     startup from the arrays in this file. None is asserted: each row is a
//     `measure::Check` whose printed line is COMPUTED from the two values
//     it reports, so the sentence and the measurement cannot drift apart.
//     Three rows are FINDINGS rather than claims — the hue does not
//     advance at every one of the seventy-two steps, one of Chevreul's
//     four complementary statements misses, and his §160 claim that blue
//     is darker than red does not hold on his own plate. A finding's
//     verdict is printed and never counted against the run, because its
//     failing IS the result.
//
// THE LIMB IS SET TANGENTIALLY, NOT RADIALLY, and it is worth checking
// against the plate before "correcting" it. Rotating rim crops into the
// outward-up frame shows ROUGE, VERT, BLEU and ORANGÉ-JAUNE all running
// ALONG the arc, with GLYPH-UP POINTING RADIALLY INWARD everywhere — you
// turn the plate so the sector you are reading is at the bottom. That is one
// uniform engraver's convention, and it needs a COUNTER-CLOCKWISE baseline,
// which is what `rimBaseline()` below builds. Orient::Radial is exercised
// where something genuinely radiates: the reconstructed index ring.
//
// NO Composer::setView() ANYWHERE, DELIBERATELY. The whole piece is the
// claim that a described sRGB value is the delivered sRGB value, and an
// output transform would silently invalidate checks 10 and 12.
//
// WHAT THREE ENVIRONMENT SWITCHES ISOLATE.
//   CHEVREUL_STATS=1 dumps Composer::stats(). In steady state there is one
//       render() and everything else is a binding.
//   CHEVREUL_REDESCRIBE=1 re-describes the whole plate every frame. What
//       that exposes is ONE node: the plate-tone wash, a field::grain
//       under .cache(Cache::Texture) whose shape is an
//       `.shape(shapes::circle())` LAMBDA. An outline() callable can never
//       compare equal, so its Texture bake is thrown away and redone every
//       frame.
//   CHEVREUL_NOLIMB=1 drops the 78 onPath limb runs, which cannot prune
//       because TextPath carries no operator==.
//
// Run (13.0 s loop; 12.6 s is the settled plate):
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/chevreul_circle.cpp \
//       --frame /tmp/chevreul_circle.png

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace measure = sigil::measure;
namespace ocio = sigil::material::ocio;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Effect;
using sigil::material::skia::Paint;
using sigil::weave::ports::pickTypeface;
// The whole composition is pinned: an engraved plate has no layout.
using sigil::compose::kit::at;
using sigil::geometry::path::centred;
using namespace std::chrono_literals;
namespace ch = choreograph;
namespace weave = sigil::weave;

namespace {

// ---------------------------------------------------------------------------
// palette — sampled off the 1864 plate

constexpr SkColor4f kPaper = hex(0xEFE8D9);  // the plate's unprinted paper
constexpr SkColor4f kWell = hex(0xE4DCCA);   // panel wells, the limb's tint
constexpr SkColor4f kRule = hex(0x8C8578);   // engraved rules and hairlines
constexpr SkColor4f kInk = hex(0x221F1A);    // letterpress
constexpr SkColor4f kInk2 = hex(0x5C554A);   // small caps, numerals
constexpr SkColor4f kRed = hex(0x8E2F26);    // annotation red
constexpr SkColor4f kShade = hex(0x3A352D);  // mounted-panel shadow
constexpr SkColor4f kBlack = hex(0x000000);
constexpr SkColor4f kWhite = hex(0xFFFFFF);

// ---------------------------------------------------------------------------
// the seventy-two couleurs franches. n is Chevreul's index from ROUGE; the
// index increases as the plate's angle DECREASES. Transcribed from the
// measurement run, not retyped.

constexpr int kSectors = 72;
constexpr float kSectorDeg = 360.0f / (float)kSectors;  // 5.0 exactly
constexpr float kRougeDeg = 90.0f;   // BUILT here; the scan measures 93.2
constexpr float kScanRouge = 93.2f;  // measured on the 1864 scan
constexpr float kScanVert = 273.2f;

const std::array<uint32_t, 72> kCorrectedHex = {{
    0xA04256, 0xA44556, 0xA74655, 0xAA4B55, 0xAC4C50, 0xAF504F, 0xBB5C50,
    0xC46652, 0xC8684D, 0xCF6B4B, 0xD26C47, 0xDB7649, 0xE27847, 0xE77F48,
    0xE8874C, 0xE78B4E, 0xE69054, 0xE59557, 0xE29B5B, 0xE2A361, 0xE1AA64,
    0xE1B065, 0xE0B763, 0xDFBD61, 0xDDC360, 0xD8C861, 0xD3C560, 0xCDC360,
    0xC4BE5E, 0xC0BF61, 0xB6BA60, 0xABB45D, 0xA1B262, 0x93AA63, 0x8AA265,
    0x87A168, 0x819B6C, 0x79966C, 0x6E8F6D, 0x668A6C, 0x62886A, 0x5E876D,
    0x5B8670, 0x588272, 0x578379, 0x527E7A, 0x4F7E85, 0x497786, 0x467184,
    0x4C7389, 0x456780, 0x3F5C7B, 0x3D5578, 0x425579, 0x3F496F, 0x444B71,
    0x47486C, 0x4A4B6F, 0x4B486C, 0x494567, 0x4E4768, 0x524667, 0x544667,
    0x584867, 0x584562, 0x594360, 0x654361, 0x6F3E5C, 0x7B3E5A, 0x7D405B,
    0x853E5A, 0x9B3C53,
}};

const std::array<uint32_t, 72> kScannedHex = {{
    0x963B48, 0x9A3E48, 0x9D3F47, 0x9F4347, 0xA14443, 0xA44842, 0xAF5343,
    0xB85C44, 0xBC5E40, 0xC2613E, 0xC5623B, 0xCE6B3D, 0xD46D3B, 0xD9733C,
    0xDA7A3F, 0xD97E41, 0xD88346, 0xD78749, 0xD48D4C, 0xD49451, 0xD39A54,
    0xD3A055, 0xD2A653, 0xD1AC51, 0xCFB150, 0xCBB651, 0xC6B350, 0xC0B150,
    0xB8AD4F, 0xB4AE51, 0xABA950, 0xA0A44E, 0x97A252, 0x8A9A53, 0x819355,
    0x7E9257, 0x798D5B, 0x71885B, 0x67825C, 0x5F7D5B, 0x5C7B59, 0x587A5C,
    0x55795E, 0x527660, 0x517766, 0x4C7267, 0x4A7270, 0x446C71, 0x41666F,
    0x476874, 0x405D6C, 0x3B5368, 0x394D65, 0x3D4D66, 0x3B425D, 0x3F435F,
    0x42415B, 0x45435D, 0x46415B, 0x443E56, 0x494057, 0x4C3F56, 0x4E3F56,
    0x524156, 0x523E52, 0x533C50, 0x5E3C51, 0x68384D, 0x73384B, 0x75394C,
    0x7D384B, 0x913645,
}};

// The twelve named scales, §162's order. The hyphenated ones are set on the
// plate as two stacked lines, and are so here.
struct ScaleName {
  const char* line1;
  const char* line2;  // "" = one line
};
const std::array<ScaleName, 12> kNames = {{
    {"ROUGE", ""},
    {"ROUGE", "ORANGÉ"},
    {"ORANGÉ", ""},
    {"ORANGÉ", "JAUNE"},
    {"JAUNE", ""},
    {"JAUNE", "VERT"},
    {"VERT", ""},
    {"VERT", "BLEU"},
    {"BLEU", ""},
    {"BLEU", "VIOLET"},
    {"VIOLET", ""},
    {"VIOLET", "ROUGE"},
}};
const std::array<const char*, 12> kFlatNames = {{
    "ROUGE",
    "ROUGE-ORANGÉ",
    "ORANGÉ",
    "ORANGÉ-JAUNE",
    "JAUNE",
    "JAUNE-VERT",
    "VERT",
    "VERT-BLEU",
    "BLEU",
    "BLEU-VIOLET",
    "VIOLET",
    "VIOLET-ROUGE",
}};

// Newton's seven, placed on Chevreul's twelve names (§5.4). kIndigo is a
// READING, not a citation.
constexpr int kNRed = 0, kNOrange = 12, kNYellow = 24, kNGreen = 36,
              kNBlue = 48, kNIndigo = 54, kNViolet = 60;
const std::array<int, 7> kNewton = {
    {kNRed, kNOrange, kNYellow, kNGreen, kNBlue, kNIndigo, kNViolet}};
const std::array<const char*, 7> kNewtonName = {
    {"RED", "ORANGE", "YELLOW", "GREEN", "BLUE", "INDIGO", "VIOLET"}};

// §21–§37, transcribed complete, in Chevreul's own plate order 1–17.
struct Observation {
  int plate;  // Chevreul's plate number
  int a, b;   // index into kNewton
  const char* modA;
  const char* modB;
  int para;  // paragraph
};
const std::array<Observation, 17> kObs = {{
    {1, 1, 0, "inclines to yellow", "inclines to violet", 26},
    {2, 0, 2, "inclines to violet", "inclines to green", 34},
    {3, 0, 4, "inclines to orange", "inclines to green", 35},
    {4, 5, 0, "becomes bluer", "inclines to orange", 28},
    {5, 6, 0, "inclines to indigo", "yellower, to orange", 27},
    {6, 1, 2, "becomes redder", "inclines to green", 29},
    {7, 1, 3, "redder, brighter", "bluer, less yellow", 21},
    {8, 1, 5, "yellower, less red", "bluer, less red", 22},
    {9, 1, 6, "becomes yellower", "inclines to indigo", 23},
    {10, 3, 2, "becomes bluer", "inclines to orange", 30},
    {11, 2, 4, "inclines to orange", "inclines to indigo", 36},
    {12, 3, 4, "becomes yellower", "inclines to indigo", 31},
    {13, 3, 5, "becomes yellower", "redder, more violet", 24},
    {14, 3, 6, "becomes yellower", "becomes redder", 25},
    {15, 5, 4, "inclines to violet", "inclines to green", 33},
    {16, 6, 4, "becomes redder", "becomes greenish", 32},
    {17, 5, 6, "appears bluer", "inclines to red", 37},
}};

// ---------------------------------------------------------------------------
// colour arithmetic. Everything that MIXES does so in LINEAR LIGHT: Chevreul
// is describing quantities of pigment on a surface, and an sRGB-code-value
// lerp is systematically too dark in the middle (a 50/50 white–black mix
// comes out #808080, Y = 0.216, where the physical answer is #BCBCBC,
// Y = 0.5). The grey gamme is the worked check: tone 10 of it must come out
// #C0C0C0 and not #808080.

using sigil::material::linearToSrgb;
using sigil::material::srgbToLinear;

inline SkColor4f lerpLinear(SkColor4f a, SkColor4f b, float t) {
  return {linearToSrgb(srgbToLinear(a.fR) +
                       (srgbToLinear(b.fR) - srgbToLinear(a.fR)) * t),
          linearToSrgb(srgbToLinear(a.fG) +
                       (srgbToLinear(b.fG) - srgbToLinear(a.fG)) * t),
          linearToSrgb(srgbToLinear(a.fB) +
                       (srgbToLinear(b.fB) - srgbToLinear(a.fB)) * t),
          a.fA + (b.fA - a.fA) * t};
}
/** Relative luminance, Rec.709 / sRGB primaries. */
inline float luminance(SkColor4f c) {
  return 0.2126f * srgbToLinear(c.fR) + 0.7152f * srgbToLinear(c.fG) +
         0.0722f * srgbToLinear(c.fB);
}
struct Lab {
  float L, a, b;
};
inline Lab toLab(SkColor4f c) {
  const float R = srgbToLinear(c.fR), G = srgbToLinear(c.fG),
              B = srgbToLinear(c.fB);
  const float X = 0.4124564f * R + 0.3575761f * G + 0.1804375f * B;
  const float Y = 0.2126729f * R + 0.7151522f * G + 0.0721750f * B;
  const float Z = 0.0193339f * R + 0.1191920f * G + 0.9503041f * B;
  auto f = [](float t) {
    return t > 216.0f / 24389.0f ? std::cbrt(t)
                                 : (24389.0f / 27.0f * t + 16.0f) / 116.0f;
  };
  const float fx = f(X / 0.95047f), fy = f(Y), fz = f(Z / 1.08883f);
  return {116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz)};
}

/** Chevreul's index n -> the sector's START angle in Skia degrees
 *  (0° = +x, sweeping clockwise). n = 0 is ROUGE, straight down. */
inline float sectorStart(int n) {
  return kRougeDeg - kSectorDeg * ((float)n + 0.5f);
}
inline float sectorMid(int n) { return kRougeDeg - kSectorDeg * (float)n; }
inline int complementOf(int n) { return (n + 36) % kSectors; }
inline int sepSectors(int a, int b) {
  const int d = std::abs(a - b) % kSectors;
  return std::min(d, kSectors - d);
}

/** §164: tone 15 of radius k is (10−k)/10 of the colour with k/10 black.
 *  §160: tones BELOW the normal tone add white, tones above add black. The
 *  normal tone of ROUGE is 15 on the 1..20 scale. */
inline SkColor4f quadrantCell(SkColor4f hue, int k /*1..10*/, int t /*1..20*/) {
  const SkColor4f broken = lerpLinear(hue, kBlack, (float)k / 10.0f);
  if (t < 15) return lerpLinear(broken, kWhite, (float)(15 - t) / 14.0f);
  if (t > 15) return lerpLinear(broken, kBlack, (float)(t - 15) / 5.0f);
  return broken;
}

/** Chevreul's own prediction (§18, §20), made numeric: nudge a colour
 *  toward the complement of its neighbour, in linear light. */
inline SkColor4f predicted(SkColor4f self, int neighbourSector,
                           const std::array<SkColor4f, 72>& wheel,
                           float amount = 0.22f) {
  return lerpLinear(self, wheel[(size_t)complementOf(neighbourSector)], amount);
}

// ---------------------------------------------------------------------------
// typography

inline sk_sp<SkTypeface> face(const char* family, SkFontStyle style,
                              const char* fallback) {
  return pickTypeface({family, fallback}, style);
}
inline const sk_sp<SkTypeface>& serif() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Normal(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface>& serifIt() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Italic(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface>& serifBold() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Bold(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface>& mono() {
  static sk_sp<SkTypeface> f =
      face("Menlo", SkFontStyle::Normal(), "Courier New");
  return f;
}

// The plate's four registers, each one library `type()` call: the roman it
// is set in, its bold, its italic, and the mono the numbers run in.
inline weave::TextStyle sr(float sz, SkColor4f c, float tr = 0) {
  return weave::textStyle({.face = serif(), .size = sz, .color = c, .track = tr});
}
inline weave::TextStyle sbd(float sz, SkColor4f c, float tr = 0) {
  return weave::textStyle({.face = serifBold(), .size = sz, .color = c, .track = tr});
}
inline weave::TextStyle it(float sz, SkColor4f c, float tr = 0) {
  return weave::textStyle({.face = serifIt(), .size = sz, .color = c, .track = tr});
}
inline weave::TextStyle mn(float sz, SkColor4f c, float tr = 0) {
  return weave::textStyle({.face = mono(), .size = sz, .color = c, .track = tr});
}

inline std::u8string U(const std::string& s) { return toU8(s); }
inline std::string fmt(const char* f, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return std::string(buf);
}
inline std::string hexOf(SkColor4f c) {
  auto q = [](float v) {
    return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f);
  };
  return fmt("#%02X%02X%02X", q(c.fR), q(c.fG), q(c.fB));
}

inline Element label(const std::string& s, const weave::TextStyle& st, float x,
                     float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f).child(text(U(s), st));
}
inline Element centred(const std::string& s, const weave::TextStyle& st,
                       float x, float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f)
      .child(text(U(s), st)
                 .textAlign(weave::TextAlignment::kCenter)
                 .width(Dim(w)));
}
inline Element rightAt(const std::string& s, const weave::TextStyle& st,
                       float x, float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f)
      .child(
          text(U(s), st).textAlign(weave::TextAlignment::kEnd).width(Dim(w)));
}

/** The rim baseline: a circle wound COUNTER-CLOCKWISE and starting at
 *  screen-angle 90° (the bottom of the plate, where ROUGE is). Two facts
 *  fall out of it and both matter:
 *   - the arc-length fraction of sector n is exactly n/72;
 *   - the tangent runs the way the engraver set the type, so glyph-up
 *     points radially INWARD everywhere, which is what the plate does.
 *
 *  startIndex 2 is the BOTTOM of the box: addOval indexes 0 top, 1 right,
 *  2 bottom, 3 left in both directions, which is screen-angle 90°. So
 *  kCCW + 2 starts the contour at the bottom and runs anticlockwise, which
 *  is the engraver's convention above and cannot be had from the default
 *  clockwise oval. */
inline shapes::OutlineFn rimBaseline() {
  return shapes::circle(SkPathDirection::kCCW, 2);
}
/** A radius through the centre of the box, as a straight diameter. */
inline shapes::OutlineFn diameter() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}

// Easing curves. bind().window() clamps its input to [0,1] before the curve
// runs, so these only have to be total on [0,1] — which is exactly what
// window() is for.
inline ch::EaseFn pulses(int n) {
  return [n](float t) {
    const float x = std::fmod(std::max(t, 0.0f) * (float)n, 1.0f);
    return x < 0.5f ? x * 2.0f : 2.0f - 2.0f * x;
  };
}
/** up, hold, away, back — beat 4's gesture in one total function. */
inline ch::EaseFn upHoldAwayBack() {
  return [](float t) {
    if (t < 0.22f) return t / 0.22f;
    if (t < 0.50f) return 1.0f;
    if (t < 0.66f) return 1.0f - (t - 0.50f) / 0.16f;
    if (t < 0.80f) return 0.0f;
    return std::min(1.0f, (t - 0.80f) / 0.16f);
  };
}

// ---------------------------------------------------------------------------
// the verification. Every field is COMPUTED by verify(); nothing is asserted.

struct Verdict {
  // 1 — the circle closes, two ways
  int named = 0, perNamed = 0, closes1 = 0, closes2 = 0;
  // 2 — the system total
  long total = 0;
  // 3 — the plate's own diameter
  float plateDelta = 0;
  // 4 — §6 against §161
  int compExact = 0, compOff = 0;
  float compWorstDeg = 0;
  // 5 — the seventeen
  int pairs21 = 0, byName = 0, byStrict = 0, byLoose = 0;
  bool nameSetMatches = false;
  // 6 — the hue winds once
  float hueSum = 0, hueMean = 0, hueMin = 0, hueMax = 0, hueSd = 0;
  int huePositive = 0;
  float hueWorst = 0;
  int hueWorstAt = -1;
  // 8 — the diameters
  float missOrigin = 0, missOriginMax = 0, missCentroid = 0,
        missCentroidMax = 0;
  float centA = 0, centB = 0, meanChroma = 0, missPercent = 0;
  // 9 — luminosity
  float yJaune = 0, yBleu = 0, yRouge = 0;
  bool jauneHighest = false, bleuDarker = false;
  // 10 — the staircase
  int bands = 0, bandsExact = 0;
  float bandSigmaMax = 0;
  int bandMaxDev = 0;
  // 11 — exact cover
  int covSamples = 0, covUncovered = 0, covDoubled = 0;
  // endpointDegrees
  size_t closedContours = 0, endpointPoints = 0;
  // 12 — instance tints
  int tintCells = 0, tintExact = 0, tintMaxDev = 0;
  // OCIO
  bool ocioAvailable = false;
  std::string ocioSample = "—";
};

}  // namespace

// ===========================================================================

struct ChevreulCircle : sketch::Sketch {
  static constexpr float kW = 1800.0f;
  static constexpr float kH = 1200.0f;

  // the wheel
  static constexpr SkPoint kC{436, 500};
  static constexpr float kRColour = 292.0f;  // outer edge of the colour band
  static constexpr float kInner = 0.34f;     // the paper medallion
  // The paper between two blades, in degrees of the 5 deg sector pitch.
  static constexpr float kBladeGapDeg = 1.25f;
  // How far the reconstruction is lifted toward the plate ON A WALL. The
  // medians are read off one photograph with the paper divided out in
  // linear light, which is a defensible reconstruction and reads a stop
  // duller than the engraving does under gallery light; this is a chroma
  // gain about each blade's own luminance, and it is the only place the
  // measured numbers are departed from.
  static constexpr float kWallLift = 1.22f;
  // The three surface passes, as dials rather than as environment reads: a
  // scene that describes itself differently depending on what is in the
  // environment cannot be photographed reproducibly.
  static constexpr bool kPlateTone = true;
  static constexpr bool kLimb = true;
  static constexpr bool kPaperGrain = true;
  /// Re-describe every frame, to price what cannot prune.
  static constexpr bool kRedescribe = false;
  static constexpr float kRLimbIn = 296.0f;
  static constexpr float kRLimbOut = 328.0f;
  static constexpr float kRSweepIn = 333.0f;  // the continuous-sweep ring
  static constexpr float kRSweepOut = 344.0f;

  // the staircases
  static constexpr float kBandW = 44.0f;
  static constexpr int kBandN = 20;
  static constexpr float kStairX = 860.0f;  // 20 x 44 = 880, integer, exact
  static constexpr float kStairYA = 566.0f;
  static constexpr float kStairYB = 646.0f;
  static constexpr float kStairYC = 726.0f;
  static constexpr float kStairH = 66.0f;

  // the quadrant
  static constexpr float kQX = 96.0f, kQY = 950.0f;
  static constexpr float kQCellW = 68.0f, kQCellH = 6.0f;
  static constexpr float kQGapX = 2.0f, kQGapY = 1.0f;

  std::array<SkColor4f, 72> corrected{}, scanned{};
  std::array<Lab, 72> lab{};
  std::array<SkColor4f, 20> gamme{}, gammeCode{};

  ch::Output<float> demo{0};
  Verdict v;
  measure::Table verdict;
  std::string derivation1, derivation2;
  std::string counterText;

  Paint paperGrain, plateTone, sweepRing, medallionGlow;
  std::shared_ptr<weave::Paragraph> lawPara;

  std::shared_ptr<instancing::Atlas> quadAtlas;
  std::shared_ptr<instancing::Pool> quadPool;
  int quadFrame = 0;

  // ==================================================================
  // verification

  /** An unpremultiplied channel as the 8-bit code a screen is handed —
   *  what every read-back claim on this plate is stated in. */
  static int code(float v) {
    return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
  }

  /** The measured colour, lifted toward what the engraving looks like on
   *  a wall: chroma scaled about the colour's own luminance, so the hue
   *  and the value the medians establish are both left alone and only the
   *  distance from grey changes. */
  static SkColor4f wallLift(SkColor4f c) {
    const float y = 0.2126f * c.fR + 0.7152f * c.fG + 0.0722f * c.fB;
    const auto lift = [y](float v) {
      return std::clamp(y + (v - y) * kWallLift, 0.0f, 1.0f);
    };
    return {lift(c.fR), lift(c.fG), lift(c.fB), c.fA};
  }

  void computeColours() {
    for (int n = 0; n < 72; ++n) {
      corrected[(size_t)n] = wallLift(hex(kCorrectedHex[(size_t)n]));
      scanned[(size_t)n] = hex(kScannedHex[(size_t)n]);
      lab[(size_t)n] = toLab(corrected[(size_t)n]);
    }
    // §164 read as equal REFLECTANCE, against the modern equal-code-value
    // ramp the illusion demonstrations use.
    for (int t = 1; t <= 20; ++t) {
      const float Y = (float)(20 - t) / 19.0f;
      const float s = linearToSrgb(Y);
      gamme[(size_t)(t - 1)] = {s, s, s, 1};
      const float c = (float)(20 - t) / 19.0f;
      gammeCode[(size_t)(t - 1)] = {c, c, c, 1};
    }
  }

  void verify(sketch::SketchContext& ctx) {
    // --- 1. the circle closes, two ways -----------------------------
    v.named = 12;
    v.perNamed = 6;  // one named + five numbered intermediates
    v.closes1 = v.named * v.perNamed;
    v.closes2 = 3 + 3 * 23;  // colorants.hypotheses.org's framing
    derivation1 =
        fmt("120/2 = 60  ->  60/2 = 30  ->  30/5 = 6 deg;  "
            "360/6 = %d sectors of %.1f deg",
            360 / 6 * 1, kSectorDeg);
    // (the arithmetic, spelled the way §161 builds it)
    derivation1 =
        fmt("3 arcs of 120 -> 6 of 60 -> 12 of 30, each divided "
            "in 5: 12 + 60 = %d",
            v.closes1);

    // --- 2. the system total (§163-§165) ----------------------------
    const long plane = 72L * 20L;        // the circle's own plane
    const long broken = 9L * 72L * 20L;  // nine radii broken by tenths
    const long grey = 20L;               // the tenth radius, normal grey
    v.total = plane + broken + grey;
    derivation2 = fmt("72 x 20 = %ld  +  9 x 72 x 20 = %ld  +  20 grey  =  %ld",
                      plane, broken, v.total);

    // --- 3. the plate's own diameter --------------------------------
    v.plateDelta = kScanVert - kScanRouge;

    // --- 4. §6's four statements against §161's +36 rule ------------
    struct Claim {
      int a, b;
      const char* text;
    };
    const std::array<Claim, 4> claims = {{
        {kNRed, kNGreen, "Red / Green"},
        {kNOrange, kNBlue, "Orange / Blue"},
        {18 /*ORANGE-JAUNE*/, 54 /*BLEU-VIOLET*/, "Orange-Yellow / Indigo"},
        {30 /*JAUNE-VERT*/, 60 /*VIOLET*/, "Greenish-Yellow / Violet"},
    }};
    for (const Claim& c : claims) {
      const int want = complementOf(c.a);
      const int off = sepSectors(want, c.b);
      if (off == 0)
        ++v.compExact;
      else {
        ++v.compOff;
        v.compWorstDeg = std::max(v.compWorstDeg, (float)off * kSectorDeg);
      }
    }

    // --- 5. the seventeen are all of them ---------------------------
    std::array<std::array<bool, 7>, 7> inTable{};
    for (const Observation& o : kObs) {
      inTable[(size_t)o.a][(size_t)o.b] = true;
      inTable[(size_t)o.b][(size_t)o.a] = true;
    }
    // the four pairs Chevreul's PROSE names complementary
    std::array<std::array<bool, 7>, 7> byProse{};
    auto mark = [&](int i, int j) {
      byProse[(size_t)i][(size_t)j] = byProse[(size_t)j][(size_t)i] = true;
    };
    mark(0, 3);  // §6  Red / Green
    mark(1, 4);  // §6  Orange / Blue
    mark(2, 6);  // §6  Greenish-Yellow / Violet
    mark(2, 5);  // §29-§36 "the complementary of Yellow (Indigo inclining
                 //          to violet)"
    v.pairs21 = 0;
    v.byName = v.byStrict = v.byLoose = 0;
    v.nameSetMatches = true;
    for (int i = 0; i < 7; ++i)
      for (int j = i + 1; j < 7; ++j) {
        ++v.pairs21;
        const int sep = sepSectors(kNewton[(size_t)i], kNewton[(size_t)j]);
        if (!byProse[(size_t)i][(size_t)j]) ++v.byName;
        if (sep != 36) ++v.byStrict;
        if (sep < 30) ++v.byLoose;
        // the sharp version: the prose set must be EXACTLY the absent set
        if (byProse[(size_t)i][(size_t)j] == inTable[(size_t)i][(size_t)j])
          v.nameSetMatches = false;
      }

    // --- 6/7. the measured hues wind once, and not evenly -----------
    std::array<float, 72> hab{}, steps{};
    for (int n = 0; n < 72; ++n) {
      float h =
          std::atan2(lab[(size_t)n].b, lab[(size_t)n].a) * 180.0f / 3.14159265f;
      if (h < 0) h += 360.0f;
      hab[(size_t)n] = h;
    }
    v.huePositive = 0;
    v.hueWorst = 1e9f;
    for (int n = 0; n < 72; ++n) {
      float d = hab[(size_t)((n + 1) % 72)] - hab[(size_t)n];
      d = std::fmod(std::fmod(d, 360.0f) + 360.0f, 360.0f);
      if (d > 180.0f) d -= 360.0f;
      steps[(size_t)n] = d;
      if (d > 0) ++v.huePositive;
      if (d < v.hueWorst) {
        v.hueWorst = d;
        v.hueWorstAt = n;
      }
    }
    v.hueSum = 0;
    v.hueMin = 1e9f;
    v.hueMax = -1e9f;
    for (float d : steps) {
      v.hueSum += d;
      v.hueMin = std::min(v.hueMin, d);
      v.hueMax = std::max(v.hueMax, d);
    }
    v.hueMean = v.hueSum / 72.0f;
    float acc = 0;
    for (float d : steps) acc += (d - v.hueMean) * (d - v.hueMean);
    v.hueSd = std::sqrt(acc / 72.0f);

    // --- 8. the diameters, against two candidate centres ------------
    v.centA = v.centB = 0;
    for (const Lab& l : lab) {
      v.centA += l.a;
      v.centB += l.b;
    }
    v.centA /= 72.0f;
    v.centB /= 72.0f;
    v.meanChroma = 0;
    for (const Lab& l : lab) v.meanChroma += std::hypot(l.a, l.b);
    v.meanChroma /= 72.0f;
    auto chordMiss = [&](float px, float py, float& mean, float& worst) {
      mean = 0;
      worst = 0;
      for (int n = 0; n < 36; ++n) {
        const Lab &A = lab[(size_t)n], &B = lab[(size_t)n + 36];
        const float dx = B.a - A.a, dy = B.b - A.b;
        const float len = std::hypot(dx, dy);
        const float d = len > 1e-6f
                            ? std::fabs(dx * (A.b - py) - dy * (A.a - px)) / len
                            : 0.0f;
        mean += d;
        worst = std::max(worst, d);
      }
      mean /= 36.0f;
    };
    chordMiss(0.0f, 0.0f, v.missOrigin, v.missOriginMax);
    chordMiss(v.centA, v.centB, v.missCentroid, v.missCentroidMax);
    v.missPercent = 100.0f * v.missCentroid / v.meanChroma;

    // --- 9. §160's luminosity claim ---------------------------------
    v.yJaune = luminance(corrected[24]);
    v.yBleu = luminance(corrected[48]);
    v.yRouge = luminance(corrected[0]);
    v.jauneHighest = true;
    for (int i = 0; i < 12; ++i)
      if (i != 4 && luminance(corrected[(size_t)i * 6]) > v.yJaune)
        v.jauneHighest = false;
    v.bleuDarker = v.yBleu < v.yRouge;  // §160 says it should be

    // --- 10. the staircase, read back off a raster surface ----------
    //     Done FIRST of the drawing checks and before anything else is
    //     described: if a described #8C8C8C does not come back as
    //     0x8C8C8C, everything downstream is measuring a transform
    //     nobody declared.
    v.bands = kBandN;
    if (ctx.fonts) {
      Element strip = box().row();
      for (int b = 0; b < kBandN; ++b)
        strip.child(box()
                        .width(Dim(kBandW))
                        .height(Dim(32))
                        .shrink(0)
                        .fill(Fill::color(gamme[(size_t)b])));
      // test::rasterize is the read-back: it wraps the tree in the shell
      // snapshot() needs, draws it at an explicit canvas size and hands the
      // pixels over. N32 rather than the float default, because the claim
      // is about the 8-bit value a viewer's screen is handed.
      const test::Raster r = test::rasterize(
          std::move(strip), *ctx.fonts, {(int)(kBandW * kBandN), 32},
          kN32_SkColorType);
      if (r.valid()) {
        for (int b = 0; b < kBandN; ++b) {
          const int x0 = (int)(b * kBandW);
          const SkColor4f want = gamme[(size_t)b];
          int dev = 0;
          // interior only: the band edges are antialiased and a 1 px
          // blend seam there is correct behaviour, not a defect.
          double sum = 0, sum2 = 0;
          int cnt = 0;
          for (int x = x0 + 6; x < x0 + (int)kBandW - 6; x += 2)
            for (int y = 6; y < 26; y += 2) {
              const SkColor4f got = r.at(x, y);
              dev = std::max({dev, std::abs(code(got.fR) - code(want.fR)),
                              std::abs(code(got.fG) - code(want.fG)),
                              std::abs(code(got.fB) - code(want.fB))});
              const double lum = code(got.fG);
              sum += lum;
              sum2 += lum * lum;
              ++cnt;
            }
          const double mean = cnt ? sum / cnt : 0.0;
          const double var =
              cnt ? std::max(0.0, sum2 / cnt - mean * mean) : 0.0;
          v.bandSigmaMax = std::max(v.bandSigmaMax, (float)std::sqrt(var));
          v.bandMaxDev = std::max(v.bandMaxDev, dev);
          if (dev == 0) ++v.bandsExact;
        }
      }
    }

    // --- 11. the 72 sectors exactly tile the annulus ----------------
    //     Run against the UN-OVERLAPPED geometry; the drawn wheel uses a
    //     0.25 deg overlap so the antialiased seams do not double the
    //     radialHatch separators. The SkPath-region overload is what makes
    //     a radial tiling testable at all: with the SkRect form every
    //     corner of the bounding square reports as an uncovered gap.
    {
      const SkSize sz{2 * kRColour, 2 * kRColour};
      std::vector<SkPath> pieces;
      pieces.reserve(73);
      for (int n = 0; n < 72; ++n)
        pieces.push_back(
            shapes::sector(sectorStart(n), kSectorDeg, kInner)(sz));
      SkPathBuilder hub;
      hub.addOval(
          SkRect::MakeXYWH(kRColour * (1 - kInner), kRColour * (1 - kInner),
                           2 * kRColour * kInner, 2 * kRColour * kInner));
      pieces.push_back(hub.detach());
      SkPathBuilder region;
      region.addOval(SkRect::MakeWH(sz.width(), sz.height()));
      const SkPath regionPath = region.detach();
      const test::Coverage cov = test::coverage(pieces, regionPath, 256);
      v.covSamples = cov.samples;
      v.covUncovered = cov.uncovered;
      v.covDoubled = cov.doubled;

      // endpointDegrees on the same 72 pieces. It reports how many contours
      // were CLOSED rather than counting their endpoints, so a ring of
      // closed sectors reads as "this test does not apply" rather than as 72
      // points of degree 1.
      std::vector<SkPath> sectorsOnly(pieces.begin(), pieces.end() - 1);
      const test::VertexDegrees deg = test::endpointDegrees(sectorsOnly);
      v.closedContours = deg.closedContours;
      v.endpointPoints = deg.points.size();
    }

    // --- 12. instance tints, read back ------------------------------
    buildQuadrantPool();
    if (ctx.fonts && quadAtlas && quadPool) {
      const float gw = 10 * (kQCellW + kQGapX) - kQGapX;
      const float gh = 20 * (kQCellH + kQGapY) - kQGapY;
      Element probe = box().width(Dim(gw)).height(Dim(gh)).child(
          instancing::instances(quadAtlas, quadPool, instancing::Mode::Data));
      const test::Raster r = test::rasterize(
          std::move(probe), *ctx.fonts,
          {(int)std::ceil(gw), (int)std::ceil(gh)}, kN32_SkColorType);
      if (r.valid()) {
        const auto pos = quadPool->positions();
        const auto tints = quadPool->tints();
        for (size_t i = 0; i < pos.size(); ++i) {
          const SkColor4f got = r.at((int)pos[i].fX, (int)pos[i].fY);
          const SkColor4f want = tints[i];
          const int dev = std::max({std::abs(code(got.fR) - code(want.fR)),
                                    std::abs(code(got.fG) - code(want.fG)),
                                    std::abs(code(got.fB) - code(want.fB))});
          ++v.tintCells;
          v.tintMaxDev = std::max(v.tintMaxDev, dev);
          if (dev == 0) ++v.tintExact;
        }
      }
    }

    // --- the OCIO seam, MEASURED rather than asserted -----------------
    //     Nothing in the API states what a value comes out as, so this
    //     measures it. Push tone 10 of the
    //     §164 gamme (#C0C0C0) through ocio::exponent(2.2) and read it back
    //     off a raster surface, exactly the way check 10 reads the
    //     staircase — so "the seam works" is a number on the plate.
    v.ocioAvailable = ocio::available();
    if (v.ocioAvailable && ctx.fonts) {
      // test::rasterize draws a described tree and hands back the PIXELS,
      // shell and canvas size included — the read-back this probe needs,
      // and the same one check 10 makes against the staircase. N32 rather
      // than the float default: the claim is about the 8-bit value a
      // viewer's screen is handed.
      const test::Raster r = test::rasterize(
          box()
              .width(Dim(32))
              .height(Dim(32))
              .fill(Fill::color(gamme[9]))
              .effect(Effect::recipe(ocio::exponent(2.2f))),
          *ctx.fonts, {32, 32}, kN32_SkColorType);
      if (r.valid()) {
        const SkColor4f got = r.at(16, 16);
        v.ocioSample = fmt("#%02X%02X%02X", (int)std::lround(got.fR * 255.0f),
                           (int)std::lround(got.fG * 255.0f),
                           (int)std::lround(got.fB * 255.0f));
      }
    }
  }

  void buildQuadrantPool() {
    quadAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // ONE white cell. Every one of the 200 colours arrives as a tint, which
    // is precisely the fidelity question nobody had asked of this path.
    quadFrame =
        quadAtlas->cell(box().fill(Fill::color(kWhite)), {kQCellW, kQCellH});
    quadPool = std::make_shared<instancing::Pool>();
    quadPool->resize(200);
    auto pos = quadPool->positions();
    auto tint = quadPool->tints();
    auto fr = quadPool->frames();
    for (int k = 0; k < 10; ++k)
      for (int t = 0; t < 20; ++t) {
        const size_t i = (size_t)k * 20 + (size_t)t;
        pos[i] = {(float)k * (kQCellW + kQGapX) + kQCellW * 0.5f,
                  (float)t * (kQCellH + kQGapY) + kQCellH * 0.5f};
        tint[i] = quadrantCell(corrected[0], k + 1, t + 1);
        fr[i] = quadFrame;
      }
    quadPool->commit();
  }

  /** THE VERIFICATION, as one table. Every row's printed line is COMPUTED
   *  from the two values it reports, so a claim and its evidence cannot
   *  drift apart the way a hand-typed "OK" can. The rows that state
   *  something about CHEVREUL rather than about this reconstruction — his
   *  §160 luminosity claim, his §6 complementaries against his own §161
   *  construction — are findings: their verdict is printed and never
   *  counted against the run, because a finding that fails is the result.
   *  The one row that is a measurement with nothing to judge is a
   *  reading. */
  void buildVerifyTable() {
    verdict = {};
    verdict
        .add(measure::check("CIRCLE CLOSES     12 named \xc3\x97 6", 72,
                            v.closes1))
        .add(measure::check("                  3 + 3\xc3\x97" "23", 72,
                            v.closes2))
        .add(measure::check("SYSTEM TOTAL      72\xc3\x97" "20\xc3\x97"
                            "10 + 20 grey",
                            14420L, v.total))
        .add(measure::check("PLATE DIAMETER    ROUGE\xe2\x86\x92VERT, deg",
                            180.0, (double)v.plateDelta, 0.005))
        .add(measure::check("SEVENTEEN         C(7,2)=21 \xe2\x88\x92 4 named",
                            17, v.byName))
        .add(measure::check("                  and they are HIS seventeen",
                            v.nameSetMatches))
        .add(measure::reading("  by geometry     sep==36 / sep>=30",
                              fmt("%d / %d", v.byStrict, v.byLoose)))
        // A statement about the 161-year-old PRINT, not about this
        // reconstruction: whether the measured hue really advances at every
        // one of the seventy-two steps.
        .add(measure::finding(measure::check(
            "HUE WINDS ONCE    steps > 0, of 72", 72, v.huePositive)))
        .add(measure::reading(
            "EQUAL SECTORS     step mean / sd",
            fmt("%.2f / %.2f", (double)v.hueMean, (double)v.hueSd)))
        .add(measure::reading(
            "DIAMETERS         miss: origin / centroid",
            fmt("%.2f / %.2f", (double)v.missOrigin, (double)v.missCentroid)))
        // \xc2\xa7" "6's four complementary statements against \xc2\xa7" "161's
        // own construction. Three land on the nose; greenish-yellow/violet
        // does not, and that is Chevreul's, not the reconstruction's.
        .add(measure::finding(measure::check(
            "COMPLEMENTARIES   \xc2\xa7" "6 pairs exact, of 4", 4,
            v.compExact)))
        // \xc2\xa7" "160: yellow lighter and blue darker than red, measured
        // off the plate's own medians.
        .add(measure::finding(measure::check(
            "LUMINOSITY \xc2\xa7" "160  jaune is the lightest",
            v.jauneHighest)))
        .add(measure::finding(measure::check(
            "                  bleu darker than rouge", v.bleuDarker)))
        .add(measure::check("STAIRCASE         hexes exact, of 20", v.bands,
                            v.bandsExact))
        .add(measure::check("                  max within-band \xcf\x83", 0.0,
                            (double)v.bandSigmaMax, 0.0))
        .add(measure::check("EXACT COVER       uncovered", 0, v.covUncovered))
        .add(measure::check("                  doubled", 0, v.covDoubled))
        .add(measure::check("INSTANCE TINTS    cells exact", v.tintCells,
                            v.tintExact))
        .add(measure::check("                  max channel deviation", 0,
                            v.tintMaxDev));
  }

  void buildLaw() {
    weave::TextStyle body = sr(13, kInk);
    body.shaping.languageTag = "en-GB";
    weave::ParagraphBuilder b(body);
    // SigilWeave breaks at SOFT HYPHENS only, so the discretionaries are
    // typed in the way a compositor would set them.
    b.addText(
        u8"“In the case where the eye sees at the same time two "
        u8"con­tigu­ous col­ours, they will appear as "
        u8"dis­sim­i­lar as pos­si­ble, both "
        u8"in their op­ti­cal com­po­si­tion "
        u8"and in the height of their tone.”  ");
    b.pushStyle(it(11, kInk2));
    b.addText(
        u8"— M. E. Chevreul, §16, De la loi du contraste simultané des "
        u8"couleurs, 1839; trans. Charles Martel.");
    lawPara = std::make_shared<weave::Paragraph>(b.build());
  }

  // ==================================================================
  // the plate

  Element theHeader() {
    Element g = box();
    g.child(label("1er CERCLE CHROMATIQUE DE Mr CHEVREUL — RENFERMANT LES "
                  "COULEURS FRANCHES",
                  sbd(26, kInk, 2.6f), 56, 40, 1700)
                .opacity(bind(&demo).window(0.0f, 0.02f)));
    g.child(label("PL. V · DES COULEURS ET DE LEURS APPLICATIONS AUX ARTS "
                  "INDUSTRIELS · J.-B. BAILLIÈRE ET FILS, PARIS, 1864 · "
                  "DIGEON SC. · LAMOUREUX IMP. · 37 CM",
                  mn(9.5f, kInk2, 0.7f), 58, 84, 1700)
                .opacity(bind(&demo).window(0.01f, 0.04f)));
    g.child(at(56, 104, 1688, 1).fill(Fill::color(kRule)));
    return g;
  }

  Element theWheel(sketch::SketchContext& ctx) {
    Element g = box();

    // the panel's own shadow, attached FIRST so the fill paints over it
    g.child(kit::disc(kC, kRSweepOut + 6)
                .shape(shapes::circle())
                .background(styles::dropShadow(hex(0x3A352D, 0.30f), {3, 3}, 8))
                .fill(Fill::color(kPaper)));

    // ---- the limb's tint and its two engraved circles ---------------
    g.child(kit::disc(kC, kRLimbOut)
                .shape(shapes::annulus(kRLimbIn / kRLimbOut))
                .fill(Fill::color(kWell))
                .opacity(bind(&demo).window(0.15f, 0.19f)));
    for (float r : {kRLimbIn, kRLimbOut})
      g.child(kit::disc(kC, r)
                  .key(fmt("limb%.0f", r))
                  .shape(shapes::circle())
                  .fill(Fill::none())
                  .stroke(spans::upTo(bind(&demo).window(0.14f, 0.20f)),
                          stroke(1.0f, Fill::color(kRule))));

    // ---- the 72 couleurs franches -----------------------------------
    // THE GAP IS ANGULAR, which is what makes it taper. On the engraving
    // the paper between two blades is wide at the rim and closes toward
    // the medallion: it is a constant fraction of the sector PITCH, so its
    // width in pixels grows with the radius. A constant-width hairline —
    // which is what a stroked separator gives — reads as one continuous
    // ring of colour with rules drawn on it, and that is the difference
    // between a measuring instrument and a colour wheel. kBladeGapDeg of
    // the 5 deg pitch leaves a quarter of the pitch as paper at the rim.
    // Check 11 runs against the un-gapped geometry, which is the pitch the
    // plate is a statement about.
    for (int n = 0; n < 72; ++n) {
      const float lo = 0.005f + 0.0021f * (float)n;
      g.child(kit::disc(kC, kRColour)
                  .key("sector" + std::to_string(n))
                  .shape(shapes::sector(sectorStart(n) + kBladeGapDeg * 0.5f,
                                        kSectorDeg - kBladeGapDeg, kInner))
                  .fill(Fill::color(corrected[(size_t)n]))
                  .transformOrigin(0.5f, 0.5f)
                  .opacity(bind(&demo).window(lo, lo + 0.010f))
                  .scale(bind(&demo)
                             .window(lo, lo + 0.014f)
                             .map(ch::EaseFn(ease::outBack(1.2f)))
                             .target(0.86f, 1.0f)));
    }

    // The plate's seventy-two white radii are the GAPS, not a decoration
    // drawn over them: paper showing between blades, tapering with the
    // pitch. A stroked radial family here would be a second, constant-width
    // white on top of a tapering one.

    // plate tone: real intaglio leaves the whole printed area faintly
    // toned. Clipped to the wheel, cached as a texture.
    if (kPlateTone)
      g.child(kit::disc(kC, kRColour)
                  .shape(shapes::circle())
                  .fill(Fill::none())
                  .foreground(decorations::wash(plateTone,
                                                SkBlendMode::kMultiply, 0.055f))
                  .cache(Cache::Texture));

    // ---- the continuous-sweep ring ----------------------------------
    // The same 72 measured values as ONE gradient: 144 stops (doubled, so
    // the steps stay franches) in one shader, against the discrete sectors
    // beside it. Continuous against franche is the distinction the plate's
    // own title makes.
    g.child(kit::disc(kC, kRSweepOut)
                .key("sweepring")
                .shape(shapes::annulus(kRSweepIn / kRSweepOut))
                .fill(sweepRing)
                .opacity(bind(&demo).window(0.17f, 0.22f)));

    // ---- the medallion ----------------------------------------------
    const float rMed = kRColour * kInner;
    g.child(kit::disc(kC, rMed + 3)
                .shape(shapes::circle())
                .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                         {{0.0f, hex(0x8C8578, 0.0f)},
                                          {0.72f, hex(0x8C8578, 0.0f)},
                                          {1.0f, hex(0x8C8578, 0.22f)}})));
    g.child(kit::disc(kC, rMed)
                .shape(shapes::circle())
                .fill(Fill::color(kPaper))
                .stroke(stroke(1.0f, Fill::color(kRule))));
    {
      // the plate's own engraved caption, five lines, its own line breaks
      struct Cap {
        const char* s;
        float size;
        float track;
        bool bold;
        float dy;
      };
      const std::array<Cap, 6> caps = {{
          {"1er", 11, 0, false, -66},
          {"CERCLE CHROMATIQUE", 13.5f, 0.7f, false, -46},
          {"DE", 8, 2.0f, false, -25},
          {"Mr CHEVREUL", 17, 0.9f, true, -6},
          {"RENFERMANT", 7.5f, 1.6f, false, 17},
          {"LES·COULEURS·FRANCHES.", 9.5f, 0.5f, false, 36},
      }};
      for (size_t i = 0; i < caps.size(); ++i) {
        const Cap& c = caps[i];
        g.child(centred(c.s,
                        c.bold ? sbd(c.size, kInk, c.track)
                               : sr(c.size, kInk, c.track),
                        kC.fX - 97, kC.fY + c.dy, 194)
                    .key("cap" + std::to_string(i))
                    .opacity(bind(&demo).window(0.185f + 0.006f * (float)i,
                                                0.205f + 0.006f * (float)i)));
      }
      g.child(at(kC.fX - 48, kC.fY + 54, 96, 1)
                  .fill(Fill::color(kInk))
                  .transformOrigin(0.5f, 0.5f)
                  .scale(bind(&demo).window(0.22f, 0.24f)));
    }

    // ---- the limb: 12 names + 60 numerals, TANGENTIAL, glyph-up inward
    //      (the plate's convention, read off the scan — see the header)
    const weave::TextStyle nameSt = sr(8.5f, kInk, 0.55f);
    const weave::TextStyle numSt = sr(9.5f, kInk2, 0);
    const float rMid = (kRLimbIn + kRLimbOut) * 0.5f;
    // TextPath::offset positions the BASELINE, and glyph-up points inward
    // here, so centring a run in the limb band needs its cap height —
    // which is what compose::metrics() is for. Guessing it puts every one
    // of the seventy-two labels half a cap off centre.
    const float capName =
        ctx.fonts ? sigil::compose::metrics(nameSt, *ctx.fonts).capHeight
                  : 6.0f;
    const float capNum =
        ctx.fonts ? sigil::compose::metrics(numSt, *ctx.fonts).capHeight : 6.6f;
    for (int n = 0; n < 72; ++n) {
      const float f =
          (float)n / 72.0f;  // exact, by construction of rimBaseline
      const float lo = 0.20f + 0.0009f * (float)n;
      constexpr bool kNoLimb = !kLimb;
      auto run = [&](const std::string& s, const weave::TextStyle& st,
                     float offset, const std::string& key) {
        if (kNoLimb) return;
        // The BASELINE resolves against the text node's OWN laid-out box, so
        // the run has to be the disc-sized element itself; wrapping it in a
        // sized parent silently collapses all 72 labels onto one point.
        g.child(text(U(s), st)
                    .key(key)
                    .width(Dim(2 * rMid))
                    .height(Dim(2 * rMid))
                    .centerAt(kC)
                    .onPath(TextPath{.path = rimBaseline(),
                                     .at = f,
                                     .align = TextPath::Align::Center,
                                     .offset = offset,
                                     .autoFlip = false,
                                     .orient = TextPath::Orient::Tangent})
                    .opacity(bind(&demo).window(lo, lo + 0.02f)));
      };
      if (n % 6 == 0) {
        const ScaleName& nm = kNames[(size_t)(n / 6)];
        if (nm.line2[0] == '\0') {
          run(nm.line1, nameSt, -capName * 0.5f, "nm" + std::to_string(n));
        } else {
          run(nm.line1, nameSt, capName * 0.5f + 1.0f,
              "nm" + std::to_string(n));
          run(nm.line2, nameSt, -capName * 1.5f - 1.0f,
              "nm2" + std::to_string(n));
        }
      } else {
        run(std::to_string(n % 6), numSt, -capNum * 0.5f,
            "nu" + std::to_string(n));
      }
      // the cell divider, on the sector boundary
      const float bd = sectorStart(n) * 3.14159265f / 180.0f;
      g.child(box()
                  .left(Dim(kC.fX - kRLimbOut))
                  .top(Dim(kC.fY - kRLimbOut))
                  .width(Dim(2 * kRLimbOut))
                  .height(Dim(2 * kRLimbOut))
                  .key("div" + std::to_string(n))
                  .fill(Fill::none())
                  .shape([bd](SkSize s) {
                    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
                    SkPathBuilder p;
                    p.moveTo(cx + std::cos(bd) * kRLimbIn,
                             cy + std::sin(bd) * kRLimbIn);
                    p.lineTo(cx + std::cos(bd) * kRLimbOut,
                             cy + std::sin(bd) * kRLimbOut);
                    return p.detach();
                  })
                  .stroke(stroke(0.7f, Fill::color(kRule)))
                  .opacity(bind(&demo).window(0.18f, 0.21f)));
    }

    // ---- the index ring: NOT ON THE PLATE ---------------------------
    // Twelve numerals set RADIALLY just outside the medallion, so the +36
    // rule is readable straight off the figure. This is the one thing here
    // that genuinely radiates, which is what Orient::Radial is for.
    for (int i = 0; i < 12; ++i) {
      const int n = i * 6;
      g.child(text(U(std::to_string(n)), mn(10.0f, kRed, 0.3f))
                  .key("ix" + std::to_string(n))
                  .width(Dim(2 * (kRSweepOut + 11)))
                  .height(Dim(2 * (kRSweepOut + 11)))
                  .centerAt(kC)
                  .onPath(TextPath{.path = rimBaseline(),
                                   .at = (float)n / 72.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Radial})
                  .opacity(bind(&demo).window(0.24f, 0.27f)));
    }

    // ---- beat 2: a diameter rides the wheel -------------------------
    g.child(kit::disc(kC, kRColour)
                .key("diam")
                .fill(Fill::none())
                .shape(diameter())
                .stroke(stroke(1.2f, Fill::color(kRed)))
                .transformOrigin(0.5f, 0.5f)
                .rotate(bind(&demo).window(0.18f, 0.30f).target(0.0f, 180.0f))
                .opacity(bind(&demo).window(0.18f, 0.30f).map(pulses(1))));

    // ---- the "161 years of paper" inset ------------------------------
    {
      const float x0 = 58, y0 = 762, w = 19, h = 21;
      Element ins = box();
      ins.child(
          label("161 YEARS OF PAPER", mn(7.0f, kInk2, 0.5f), x0, y0 - 14, 200));
      for (int i = 0; i < 8; ++i) {
        const int n = i * 9;
        ins.child(at(x0 + (float)i * (w + 1), y0, w, h)
                      .fill(Fill::color(scanned[(size_t)n])));
        ins.child(at(x0 + (float)i * (w + 1), y0 + h + 1, w, h)
                      .fill(Fill::color(corrected[(size_t)n])));
      }
      ins.child(label("scanned / corrected", mn(6.5f, kInk2, 0.2f), x0,
                      y0 + 2 * h + 3, 160));
      ins.opacity(bind(&demo).window(0.26f, 0.29f));
      g.child(std::move(ins));
    }

    // ---- the two constructions, printed --------------------------------
    {
      const std::array<std::pair<std::string, SkColor4f>, 4> lines = {{
          {"§161  " + derivation1 + ";   each scale 5.00 deg", kInk2},
          {fmt("built at ROUGE = %.1f deg — the plate's own composition; the "
               "scan measures ROUGE %.1f, VERT %.1f, delta %.2f",
               kRougeDeg, kScanRouge, kScanVert, v.plateDelta),
           kInk2},
          {fmt("test::coverage over an SkPath REGION: %d/%d of %d · "
               "endpointDegrees: %zu closed contours, %zu endpoints",
               v.covUncovered, v.covDoubled, v.covSamples, v.closedContours,
               v.endpointPoints),
           kInk2},
          {"outer band = the same 72 values as ONE 146-stop sweep gradient · "
           "outer numerals = index n, NOT ON THE PLATE",
           kRed},
      }};
      for (size_t i = 0; i < lines.size(); ++i)
        g.child(label(lines[i].first, mn(7.4f, lines[i].second, 0.15f), 56,
                      864 + (float)i * 11.8f, 760)
                    .opacity(bind(&demo).window(0.26f, 0.29f)));
    }
    return g;
  }

  // ------------------------------------------------------------------
  Element theQuadrant() {
    Element g = box();
    g.child(
        label("CHEVREUL'S QUADRANT · §163–§165 · ROUGE, TEN RADII × TWENTY "
              "TONES = 200 CELLS",
              mn(9.0f, kInk, 0.6f), 56, 918, 760));
    const float gw = 10 * (kQCellW + kQGapX) - kQGapX;
    const float gh = 20 * (kQCellH + kQGapY) - kQGapY;

    // column heads
    const std::array<const char*, 10> heads = {{"1/10", "2/10", "3/10", "4/10",
                                                "5/10", "6/10", "7/10", "8/10",
                                                "9/10", "NOIR"}};
    for (int k = 0; k < 10; ++k)
      g.child(centred(heads[(size_t)k], mn(6.5f, kInk2, 0.2f),
                      kQX + (float)k * (kQCellW + kQGapX), 936, kQCellW)
                  .opacity(bind(&demo).window(0.80f + 0.012f * (float)k,
                                              0.815f + 0.012f * (float)k)));
    // row numbers, and 15 marked as the normal tone
    for (int t : {1, 5, 10, 15, 20})
      g.child(rightAt(std::to_string(t),
                      t == 15 ? mn(6.5f, kRed, 0) : mn(6.5f, kInk2, 0), 56,
                      kQY + (float)(t - 1) * (kQCellH + kQGapY) - 2.0f, 34));
    g.child(at(kQX - 4, kQY + 14.0f * (kQCellH + kQGapY) - 1, gw + 8, 1)
                .fill(Fill::color(hex(0x8E2F26, 0.55f)))
                .opacity(bind(&demo).window(0.93f, 0.95f)));

    g.child(at(kQX, kQY, gw, gh)
                .background(styles::dropShadow(hex(0x3A352D, 0.22f), {2, 2}, 5))
                .fill(Fill::color(kWell))
                .child(instancing::instances(quadAtlas, quadPool,
                                             instancing::Mode::Live)));

    g.child(label(derivation2 + "   — mixed in LINEAR light, per §164's "
                                "quantities of pigment",
                  mn(8.0f, kInk2, 0.2f), 56, kQY + gh + 6, 760));
    g.child(label(fmt("instanced: 1 atlas cell, 200 tints, %d/%d colour-exact "
                      "on readback (max channel dev %d)",
                      v.tintExact, v.tintCells, v.tintMaxDev),
                  mn(8.0f, v.tintExact == v.tintCells ? kInk2 : kRed, 0.2f), 56,
                  kQY + gh + 20, 760));
    return g;
  }

  // ------------------------------------------------------------------
  Element theLabPlot() {
    const float x0 = 852, y0 = 136, S = 380;
    const float cx = x0 + S * 0.5f, cy = y0 + 24 + (S - 48) * 0.5f;
    const float scale = (S - 96) * 0.5f / 60.0f;  // a* b* -60..+60
    auto P = [&](float a, float b) {
      return SkPoint{cx + a * scale, cy - b * scale};
    };
    Element g = box();
    g.child(at(x0, y0, S, S)
                .fill(Fill::color(kWell))
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("CIELAB a* b* · THE 72 MEASURED COLOURS · 36 DIAMETERS",
                  mn(8.5f, kInk, 0.5f), x0 + 10, y0 + 6, S - 20));

    // axes and ticks, drawn on hand-built geometry through the brush
    // vocabulary — decorations::paintOn is the seam for that.
    g.child(at(x0, y0, S, S)
                .fill(Fill::none())
                .child(custom([=](SkCanvas& c, const PaintContext& pc) {
                         SkPathBuilder ax;
                         for (int t = -60; t <= 60; t += 20) {
                           const SkPoint a = P((float)t, -60),
                                         b = P((float)t, 60);
                           ax.moveTo(a.fX - x0, a.fY - y0);
                           ax.lineTo(b.fX - x0, b.fY - y0);
                           const SkPoint c0 = P(-60, (float)t),
                                         d0 = P(60, (float)t);
                           ax.moveTo(c0.fX - x0, c0.fY - y0);
                           ax.lineTo(d0.fX - x0, d0.fY - y0);
                         }
                         decorations::paintOn(
                             c, pc, ax.detach(),
                             stroke(0.5f, Fill::color(hex(0x8C8578, 0.35f))));
                         SkPathBuilder cross;
                         const SkPoint o = P(0, 0);
                         cross.moveTo(o.fX - x0 - 9, o.fY - y0);
                         cross.lineTo(o.fX - x0 + 9, o.fY - y0);
                         cross.moveTo(o.fX - x0, o.fY - y0 - 9);
                         cross.lineTo(o.fX - x0, o.fY - y0 + 9);
                         decorations::paintOn(c, pc, cross.detach(),
                                              stroke(1.2f, Fill::color(kInk)));
                       }).inset(0)));

    // the 36 chords, drawing in one at a time
    for (int n = 0; n < 36; ++n) {
      const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
      const SkPoint B = P(lab[(size_t)n + 36].a, lab[(size_t)n + 36].b);
      SkRect bb = SkRect::MakeLTRB(std::min(A.fX, B.fX), std::min(A.fY, B.fY),
                                   std::max(A.fX, B.fX), std::max(A.fY, B.fY));
      bb.outset(2, 2);
      const SkPoint a0{A.fX - bb.left(), A.fY - bb.top()};
      const SkPoint b0{B.fX - bb.left(), B.fY - bb.top()};
      const float lo = 0.19f + 0.0026f * (float)n;
      g.child(at(bb.left(), bb.top(), bb.width(), bb.height())
                  .key("chord" + std::to_string(n))
                  .fill(Fill::none())
                  .shape([a0, b0](SkSize) {
                    SkPathBuilder p;
                    p.moveTo(a0);
                    p.lineTo(b0);
                    return p.detach();
                  })
                  .stroke(spans::upTo(bind(&demo).window(lo, lo + 0.012f)),
                          stroke(0.8f, Fill::color(hex(0x8C8578, 0.85f)))));
    }
    // the 72 points, each in its own colour
    for (int n = 0; n < 72; ++n) {
      const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
      const float lo = 0.005f + 0.0021f * (float)n;
      g.child(kit::disc(A, 3.5f)
                  .key("labpt" + std::to_string(n))
                  .shape(shapes::circle())
                  .fill(Fill::color(corrected[(size_t)n]))
                  .stroke(stroke(0.4f, Fill::color(hex(0x221F1A, 0.5f))))
                  .transformOrigin(0.5f, 0.5f)
                  .opacity(bind(&demo).window(lo, lo + 0.01f)));
    }
    // the centroid — the piece's whole argument
    g.child(kit::disc(P(v.centA, v.centB), 9.0f)
                .key("centroid")
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(stroke(1.6f, Fill::color(kRed)))
                .transformOrigin(0.5f, 0.5f)
                .scale(bind(&demo)
                           .window(0.275f, 0.30f)
                           .map(ch::EaseFn(ease::outBack(2.0f)))));
    g.child(label("a* →", mn(7, kInk2), x0 + S - 42, y0 + S - 44, 40));
    g.child(label("b* ↑", mn(7, kInk2), cx + 6, y0 + 26, 40));
    g.child(label("+ = a*b* ORIGIN     ○ = CENTROID OF THE 72",
                  mn(7.0f, kInk2, 0.3f), x0 + 10, y0 + S - 46, S - 20));
    g.child(slot("chordcount"));
    return g;
  }

  Element chordCounter() {
    const float x0 = 852, y0 = 136, S = 380;
    Element g = box();
    g.child(
        label(counterText, mn(8.0f, kRed, 0.2f), x0 + 10, y0 + S - 32, S - 20));
    g.child(label(fmt("centroid a* %.2f  b* %.2f   ·   mean C* %.1f", v.centA,
                      v.centB, v.meanChroma),
                  mn(7.5f, kInk2, 0.2f), x0 + 10, y0 + S - 18, S - 20));
    return g;
  }

  // ------------------------------------------------------------------
  Element theObservations() {
    const float x0 = 1268, y0 = 136, W = 476, H = 380;
    Element g = box();
    g.child(at(x0, y0, W, H)
                .fill(Fill::color(kWell))
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("LES DIX-SEPT OBSERVATIONS · §21–§37", mn(8.5f, kInk, 0.5f),
                  x0 + 10, y0 + 6, W - 20));
    const float rowH = 19.4f, top = y0 + 24;
    for (size_t i = 0; i < kObs.size(); ++i) {
      const Observation& o = kObs[i];
      const float y = top + (float)i * rowH;
      const float lo = 0.645f + 0.0075f * (float)i;
      const SkColor4f ca = corrected[(size_t)kNewton[(size_t)o.a]];
      const SkColor4f cb = corrected[(size_t)kNewton[(size_t)o.b]];
      Element row = at(x0 + 8, y, W - 16, rowH - 2)
                        .key("obs" + std::to_string(i))
                        .opacity(bind(&demo).window(lo, lo + 0.006f));
      row.child(rightAt(std::to_string(o.plate), mn(7.5f, kInk2), 0, 3, 16));
      row.child(at(22, 1, 15, 15).fill(Fill::color(ca)));
      row.child(at(37, 1, 15, 15).fill(Fill::color(cb)));
      row.child(label("→", mn(8, kInk2), 56, 1, 14));
      // the predicted pair fades in AFTER its sources land
      Element pred = box()
                         .key("pr" + std::to_string(i))
                         .opacity(bind(&demo).window(lo + 0.003f, lo + 0.009f));
      pred.child(at(72, 1, 15, 15)
                     .fill(Fill::color(
                         predicted(ca, kNewton[(size_t)o.b], corrected))));
      pred.child(at(87, 1, 15, 15)
                     .fill(Fill::color(
                         predicted(cb, kNewton[(size_t)o.a], corrected))));
      row.child(std::move(pred));
      row.child(label(
          fmt("%s · %s", kNewtonName[(size_t)o.a], kNewtonName[(size_t)o.b]),
          mn(7.0f, kInk, 0.2f), 108, 3, 108));
      row.child(label(fmt("%s / %s", o.modA, o.modB), it(8.5f, kInk2), 218,
                      1.5f, 250));
      g.child(std::move(row));
    }
    g.child(label(fmt("C(7,2) = %d − 4 complémentaires = %d      "
                      "(by geometry: %d, or %d — neither is 17)",
                      v.pairs21, v.byName, v.byStrict, v.byLoose),
                  mn(8.0f, kRed, 0.2f), x0 + 10, y0 + H - 32, W - 20)
                .opacity(bind(&demo).window(0.79f, 0.80f)));
    g.child(
        label("indigo read as BLEU-VIOLET (54); greenish-yellow as "
              "JAUNE-VERT (30) — both readings, not citations",
              mn(6.5f, kInk2, 0.2f), x0 + 10, y0 + H - 18, W - 20));
    return g;
  }

  // ------------------------------------------------------------------
  /** One twenty-band ramp. @p graded puts the OCIO view on EACH BAND
   *  rather than on the group: a group of absolutely-placed children has
   *  no size of its own, so an effect there opens a layer the size of the
   *  canvas and runs the LUT over every pixel of the plate every frame.
   *  Twenty bounded layers of one band each are the same picture. */
  Element aStaircase(const std::array<SkColor4f, 20>& ramp, float y, float h,
                     const char* keyBase, bool withGap, bool graded = false) {
    Element g = box();
    for (int b = 0; b < kBandN; ++b) {
      Element band = at(kStairX + (float)b * kBandW, y, kBandW, h)
                         .key(fmt("%s%d", keyBase, b))
                         .fill(Fill::color(ramp[(size_t)b]));
      if (graded)
        band.effect(Effect::recipe(ocio::exponent(2.2f)))
            .cache(Cache::Texture);
      if (withGap)
        band.translateX(bind(&demo)
                            .window(0.30f, 0.50f)
                            .map(pulses(2))
                            .scale(((float)b - 9.5f) * 2.6f));
      g.child(std::move(band));
    }
    return g;
  }

  Element theIllusion() {
    Element g = box();
    g.child(label("THE CHEVREUL ILLUSION · TWENTY FLAT BANDS",
                  mn(9, kInk, 0.6f), 852, 552, 500));
    g.child(
        rightAt("§164 read as equal REFLECTANCE   ·   the modern equal "
                "code-value ramp   ·   §164 under γ 2.2",
                mn(7.5f, kInk2, 0.3f), 1100, 553, 644));

    g.child(at(kStairX, kStairYA - 2, kBandW * kBandN, kStairH + 4)
                .background(styles::dropShadow(hex(0x3A352D, 0.22f), {2, 2}, 5))
                .fill(Fill::color(kWell)));
    g.child(aStaircase(gamme, kStairYA, kStairH, "sa", true));
    g.child(label("§164 · Y = (20−t)/19, sRGB-encoded · tone 10 = " +
                      hexOf(gamme[9]) + " (not #808080)",
                  mn(7.0f, kInk2, 0.2f), kStairX, kStairYA + kStairH + 5, 520));

    g.child(at(kStairX, kStairYB - 2, kBandW * kBandN, kStairH + 4)
                .background(styles::dropShadow(hex(0x3A352D, 0.22f), {2, 2}, 5))
                .fill(Fill::color(kWell)));
    g.child(aStaircase(gammeCode, kStairYB, kStairH, "sb", true));
    g.child(label("equal code value · tone 10 = " + hexOf(gammeCode[9]) +
                      " (Y = 0.216, not 0.526) — the lerp this piece "
                      "deliberately does not do",
                  mn(7.0f, kInk2, 0.2f), kStairX, kStairYB + kStairH + 5, 720));

    // the OCIO strip
    if (v.ocioAvailable) {
      g.child(aStaircase(gamme, kStairYC, 28.0f, "sc", false, true));
      g.child(label(fmt("§164 ramp under ocio::exponent(2.2) — an OCIO-baked "
                        "LUT Effect: tone 10 %s measures %s through it",
                        hexOf(gamme[9]).c_str(), v.ocioSample.c_str()),
                    mn(7.0f, kInk2, 0.2f), kStairX, kStairYC + 32, 760));
    } else {
      g.child(label("OCIO: not compiled in", mn(9.0f, kRed, 0.4f), kStairX,
                    kStairYC + 10, 400));
    }

    // every fourth band's hex, inked against its own band
    for (int b = 0; b < kBandN; b += 4)
      g.child(centred(
          hexOf(gamme[(size_t)b]), mn(6.8f, b < 12 ? kInk : kWhite, 0),
          kStairX + (float)b * kBandW, kStairYA + kStairH - 12, kBandW));

    g.child(
        label("“the light tone will appear lighter, and the deep tone "
              "deeper, commencing at the line of contact” — Introduction",
              it(9.5f, kInk), 852, 774, 600));
    g.child(rightAt(fmt("%d bands · per-band σ = %.2f · %d/%d hexes exact "
                        "byte for byte",
                        v.bands, v.bandSigmaMax, v.bandsExact, v.bands),
                    mn(8.5f, kRed, 0.2f), 1300, 776, 444));
    return g;
  }

  // ------------------------------------------------------------------
  Element theContrast() {
    const float x0 = 852, y0 = 838, W = 380;
    Element g = box();
    g.child(label("SIMULTANEOUS CONTRAST · TWELVE IDENTICAL PATCHES",
                  mn(8.5f, kInk, 0.5f), x0, y0, W));
    const float cw = 88, chh = 66, gx = x0 + 6, gy = y0 + 18;
    // Beat 4's grounds arrive and withdraw as a DIRECTIONAL WIPE at 90 deg
    // (downward), which is what Chevreul's own method looks like: take the
    // ground away and the twelve patches are plainly identical.
    // wipe() reveals the fraction of THE NODE'S OWN LAID-OUT BOX before the
    // edge, so the container has to be a real box: a bare box() holding
    // absolutely-positioned children measures zero and the wipe hides the
    // whole subtree with no diagnostic at all. Hence the explicit rect.
    Element lattice =
        at(gx, gy, 4 * cw, 3 * chh)
            .key("grounds")
            .mask(by::edge(
                90.0f, bind(&demo).window(0.50f, 0.64f).map(upHoldAwayBack())));
    for (int i = 0; i < 12; ++i) {
      const int col = i % 4, row = i / 4;
      lattice.child(at((float)col * cw, (float)row * chh, cw - 3, chh - 3)
                        .fill(Fill::color(corrected[(size_t)i * 6])));
    }
    g.child(std::move(lattice));
    for (int i = 0; i < 12; ++i) {
      const int col = i % 4, row = i / 4;
      g.child(at(gx + (float)col * cw + (cw - 3 - 30) * 0.5f,
                 gy + (float)row * chh + (chh - 3 - 30) * 0.5f, 30, 30)
                  .fill(Fill::color(gamme[14])));  // Chevreul's grey tone 15
    }
    const float ry = gy + 3 * chh + 6;
    for (int i = 0; i < 12; ++i)
      g.child(
          at(gx + (float)i * 27.0f, ry, 24, 24).fill(Fill::color(gamme[14])));
    g.child(label(fmt("all twelve patches are %s — Chevreul's grey, tone 15",
                      hexOf(gamme[14]).c_str()),
                  mn(8.0f, kRed, 0.2f), x0, ry + 28, W));
    g.child(label("§16: “they will appear as dissimilar as possible”",
                  it(8.5f, kInk2), x0, ry + 42, W));
    return g;
  }

  // ------------------------------------------------------------------
  Element theVerification() {
    const float x0 = 1268, y0 = 838, W = 476;
    Element g = box();
    // the justified law, at a real measure
    weave::ParagraphLayoutOptions o;
    o.alignment = weave::TextAlignment::kJustify;
    o.lineBreakStrategy = weave::LineBreakStrategy::kKnuthPlass;
    o.hyphenation.enabled = true;
    o.hyphenation.penalty = 45.0f;
    o.justification.spaceStretch = 0.55f;
    o.justification.spaceShrink = 0.30f;
    o.justification.lastLineAlignment = weave::TextAlignment::kStart;
    o.knuthPlass.tolerance = 6000.0f;
    o.lineMetrics.height = 16.0f;
    if (lawPara)
      g.child(at(x0, y0, 380, 96).child(text(lawPara, o).width(Dim(380))));

    // Each row of the table, revealed on its own beat. The LINE is the
    // table's — computed from the two values the row reports — and it
    // carries its own verdict, so there is no second hand-typed one beside
    // it. What the plate adds is the INK: a claim that failed is set in
    // red, a finding that failed in red too (its failing is Chevreul's, and
    // the summary counts the two apart), a reading in the quiet grey.
    const float ty0 = y0 + 88, lh = 11.0f;
    const size_t rows = verdict.rows.size();
    g.child(at(x0 - 8, ty0 - 8, W - 4, (float)rows * lh + 16)
                .fill(Fill::color(kWell))
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", mn(7.5f, kInk2, 0.5f),
                  x0, ty0 - 22, W));
    for (size_t i = 0; i < rows; ++i) {
      const measure::Check& c = verdict.rows[i];
      const SkColor4f ink =
          !c.judged() ? kInk2 : (c.pass ? kInk : kRed);
      const float lo = 0.30f + 0.034f * (float)i;
      g.child(at(x0, ty0 + (float)i * lh, W - 20, lh)
                  .key("vr" + std::to_string(i))
                  .opacity(bind(&demo).window(lo, lo + 0.012f))
                  .child(text(U(c.line(38, 8)), mn(8.0f, ink, 0.05f))));
    }
    g.child(label(
        "§38: “do we know, at the present day, of two coloured "
        "bodies … Certainly not!”",
        it(8.5f, kInk2), x0, ty0 + (float)rows * lh + 12, W));
    return g;
  }

  // ==================================================================
  Element describe(sketch::SketchContext& ctx) {
    Element root = stack().width(Dim(kW)).height(Dim(kH));

    // the leaf: measured paper, its tooth, and the platemark
    root.child(at(0, 0, kW, kH).fill(Fill::color(kPaper)));
    if (kPaperGrain)
      root.child(
          at(0, 0, kW, kH)
              .fill(paperGrain)
              .blend(SkBlendMode::kMultiply)
              .opacity(0.085f)
              .cache(Cache::Texture));  // 1800x1200 of generated material
    root.child(
        at(28, 28, kW - 56, kH - 56)
            .fill(Fill::none())
            .foreground(stroke(1.0f, Fill::color(hex(0x8C8578, 0.55f)))));

    root.child(theHeader());
    root.child(theWheel(ctx));
    root.child(theQuadrant());
    root.child(theLabPlot());
    root.child(theObservations());
    root.child(theIllusion());
    root.child(theContrast());
    root.child(theVerification());

    root.child(label(
        "COLOURS MEASURED FROM SCIENCE HISTORY INSTITUTE ND1280 .C497 1864, "
        "PL. V, 2880×3789 · PAPER WHITE #EFE8D9 DIVIDED OUT IN LINEAR LIGHT · "
        "CONSTRUCTION AFTER CHEVREUL §6, §16, §160–§165 · TRANS. C. MARTEL · "
        "NO OUTPUT VIEW TRANSFORM IS SET, DELIBERATELY",
        mn(8.0f, kInk2, 0.55f), 56, 1168, 1690));
    return root;
  }

  // ==================================================================
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);
    // The still has to name its moment: this is a 14 s loop (13 s reveal +
    // 1 s hold), and 12.6 s is fully settled with 1.4 s of margin before the
    // reset. An undeclared capture catches the plate roughly half-built,
    // with most verification rows and later panels unrevealed.
    ctx.captureAt(12.6);

    computeColours();

    // materials held as members so their identity survives re-describes
    paperGrain = Paint::recipe(field::grain(0.013f, 4, 11.0f, 0.32f));
    plateTone = Paint::recipe(field::grain(0.085f, 3, 5.0f, 0.45f));

    // the 72 measured values as ONE gradient: 144 stops, doubled so the
    // steps stay franches rather than blending into each other.
    {
      // SkShaders::SweepGradient measures from +x clockwise over
      // [startDeg, endDeg] and CLAMPS outside it, so the ring is authored in
      // the gradient's own 0..360 frame rather than by rotating the range.
      // Sector n spans screen angles [90 − 5(n+0.5), 90 − 5(n−0.5)], so the
      // band boundaries land at (2.5 + 5j)/360 and the band below boundary j
      // is sector (18 − j) mod 72.
      std::vector<sigil::material::skia::Stop> stops;
      stops.reserve(146);
      auto C = [&](int n) { return corrected[(size_t)(((n % 72) + 72) % 72)]; };
      stops.push_back({0.0f, C(18)});
      for (int j = 0; j < 72; ++j) {
        const float p = (2.5f + 5.0f * (float)j) / 360.0f;
        stops.push_back({p, C(18 - j)});
        stops.push_back({p, C(17 - j)});
      }
      stops.push_back({1.0f, C(18)});
      sweepRing = Paint::sweep({kRSweepOut, kRSweepOut}, std::move(stops),
                                  0.0f, 360.0f);
    }

    verify(ctx);
    buildVerifyTable();
    buildLaw();

    // one Output, 0 -> 1 over 13.0 s, then a 1.0 s hold, then loop.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const double u = std::fmod(t, 14.0);
      demo = (float)std::clamp(u / 13.0, 0.0, 1.0);
      // §163's quadrant turning on the circle's axis: one column every
      // 90 ms through beat 6. Mode::Live reads the pool every frame.
      if (quadPool) {
        const float d = demo.value();
        auto tints = quadPool->tints();
        for (int k = 0; k < 10; ++k) {
          const float lo = 0.80f + 0.012f * (float)k;
          const float a = std::clamp((d - lo) / 0.010f, 0.0f, 1.0f);
          for (int r = 0; r < 20; ++r) tints[(size_t)k * 20 + (size_t)r].fA = a;
        }
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("chordcount", chordCounter());
  }

  int frames = 0;

  void update(double, sketch::SketchContext& ctx) override {
    // Cost, measured rather than guessed: CHEVREUL_STATS=1 dumps the
    // composer's own per-phase timings for a few frames. The two numbers
    // this piece cares about are picturesLive (72 static flat fills plus
    // 78 onPath runs) and paintMs, since a TextPath carries no operator==
    // and therefore cannot prune.
    // kRedescribe re-describes the whole plate every frame, which is what
    // prices the un-prunable nodes: TextPath has no operator== by design,
    // so the 78 limb runs re-record on every render(). Set kLimb false for
    // the other half of the comparison.
    if (kRedescribe) ctx.composer.render(describe(ctx));
    ++frames;

    // The counting numbers of beat 2. Animatable covers floats, colours and
    // fills but not text, so a counter is a renderSlot() — done here rather
    // than by re-describing the plate, so every other cache stays valid.
    const float d = demo.value();
    const float u = std::clamp((d - 0.20f) / 0.09f, 0.0f, 1.0f);
    const std::string next =
        fmt("36 chords miss the ORIGIN by %.2f · the CENTROID by %.2f  "
            "(%.1f%%)",
            v.missOrigin * u, v.missCentroid * u, v.missPercent * u);
    if (next != counterText) {
      counterText = next;
      ctx.composer.renderSlot("chordcount", chordCounter());
    }
  }
};

SIGIL_SKETCH(
    ChevreulCircle, "Study \xc2\xb7 Science",
    "Chevreul's 1er cercle chromatique, Plate V, 1864 \xe2\x80\x94 a study "
    "whose content is a palette")
