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
//     startup from the arrays in this file. None is asserted.
//
// THE LIMB, CORRECTED AGAINST THE PLATE. The implementation brief called
// for TextPath::Orient::Radial on all 72 rim labels. The plate does not do
// that: rotating rim crops into the outward-up frame shows ROUGE, VERT,
// BLEU and ORANGÉ-JAUNE all set TANGENTIALLY, running along the arc, with
// GLYPH-UP POINTING RADIALLY INWARD everywhere — you turn the plate so the
// sector you are reading is at the bottom. That is one uniform engraver's
// convention (the same kind of decision the Nightingale study found, with
// the opposite sign). It needs a COUNTER-CLOCKWISE baseline, which
// shapes::circle() cannot produce; see LIBRARY GAPS in the report.
// Orient::Radial is exercised where something genuinely radiates: the
// reconstructed index ring.
//
// NO Composer::setView() ANYWHERE, DELIBERATELY. The whole piece is the
// claim that a described sRGB value is the delivered sRGB value, and an
// output transform would silently invalidate checks 10 and 12. There is no
// way in the API to DECLARE that: "I thought about colour and chose not to
// transform it" and "nobody thought about colour" produce identical trees.
//
// COST, MEASURED (Composer::stats(), CHEVREUL_STATS=1 in the environment):
//   steady state — one render(), everything else a binding —
//       908 instances, 84–91 pictures live, paint 0.07 ms/frame.
//   re-describing the whole plate every frame (CHEVREUL_REDESCRIBE=1):
//       paint 43.5 ms. Removing ONE node — the 584x584 plate-tone wash,
//       a patterns::grain under .cache(Cache::Texture) whose shape is an
//       .outline(shapes::circle()) lambda — takes that to 0.10 ms.
//       43.4 of 43.5 ms is one Texture bake being thrown away every frame
//       because an outline() callable can never compare (ROADMAP §3).
//   the 78 non-pruning onPath limb runs (CHEVREUL_NOLIMB=1), which is the
//       cost this study was predicted to find: +0.03 ms reconcile,
//       +0.03 ms layout, +0.01 ms paint. Real, and 600x smaller than §3.
//
// Run (13.0 s loop; 12.6 s is the settled plate):
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/chevreul_circle.cpp \
//       --frame /tmp/chevreul_circle.png --at 12.6

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <sigilcompose/Debug.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Ocio.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;
namespace weave = sigil::weave;

namespace {

// ---------------------------------------------------------------------------
// palette — measured off the plate (§4.1 of the brief)

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

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
constexpr float kSectorDeg = 360.0f / (float)kSectors; // 5.0 exactly
constexpr float kRougeDeg = 90.0f;  // BUILT here; the scan measures 93.2
constexpr float kScanRouge = 93.2f; // measured on the 1864 scan
constexpr float kScanVert = 273.2f;

const std::array<uint32_t, 72> kCorrectedHex = {{
    0xA04256, 0xA44556, 0xA74655, 0xAA4B55, 0xAC4C50, 0xAF504F,
    0xBB5C50, 0xC46652, 0xC8684D, 0xCF6B4B, 0xD26C47, 0xDB7649,
    0xE27847, 0xE77F48, 0xE8874C, 0xE78B4E, 0xE69054, 0xE59557,
    0xE29B5B, 0xE2A361, 0xE1AA64, 0xE1B065, 0xE0B763, 0xDFBD61,
    0xDDC360, 0xD8C861, 0xD3C560, 0xCDC360, 0xC4BE5E, 0xC0BF61,
    0xB6BA60, 0xABB45D, 0xA1B262, 0x93AA63, 0x8AA265, 0x87A168,
    0x819B6C, 0x79966C, 0x6E8F6D, 0x668A6C, 0x62886A, 0x5E876D,
    0x5B8670, 0x588272, 0x578379, 0x527E7A, 0x4F7E85, 0x497786,
    0x467184, 0x4C7389, 0x456780, 0x3F5C7B, 0x3D5578, 0x425579,
    0x3F496F, 0x444B71, 0x47486C, 0x4A4B6F, 0x4B486C, 0x494567,
    0x4E4768, 0x524667, 0x544667, 0x584867, 0x584562, 0x594360,
    0x654361, 0x6F3E5C, 0x7B3E5A, 0x7D405B, 0x853E5A, 0x9B3C53,
}};

const std::array<uint32_t, 72> kScannedHex = {{
    0x963B48, 0x9A3E48, 0x9D3F47, 0x9F4347, 0xA14443, 0xA44842,
    0xAF5343, 0xB85C44, 0xBC5E40, 0xC2613E, 0xC5623B, 0xCE6B3D,
    0xD46D3B, 0xD9733C, 0xDA7A3F, 0xD97E41, 0xD88346, 0xD78749,
    0xD48D4C, 0xD49451, 0xD39A54, 0xD3A055, 0xD2A653, 0xD1AC51,
    0xCFB150, 0xCBB651, 0xC6B350, 0xC0B150, 0xB8AD4F, 0xB4AE51,
    0xABA950, 0xA0A44E, 0x97A252, 0x8A9A53, 0x819355, 0x7E9257,
    0x798D5B, 0x71885B, 0x67825C, 0x5F7D5B, 0x5C7B59, 0x587A5C,
    0x55795E, 0x527660, 0x517766, 0x4C7267, 0x4A7270, 0x446C71,
    0x41666F, 0x476874, 0x405D6C, 0x3B5368, 0x394D65, 0x3D4D66,
    0x3B425D, 0x3F435F, 0x42415B, 0x45435D, 0x46415B, 0x443E56,
    0x494057, 0x4C3F56, 0x4E3F56, 0x524156, 0x523E52, 0x533C50,
    0x5E3C51, 0x68384D, 0x73384B, 0x75394C, 0x7D384B, 0x913645,
}};

// The twelve named scales, §162's order. The hyphenated ones are set on the
// plate as two stacked lines, and are so here.
struct ScaleName {
  const char *line1;
  const char *line2; // "" = one line
};
const std::array<ScaleName, 12> kNames = {{
    {"ROUGE", ""},   {"ROUGE", "ORANGÉ"}, {"ORANGÉ", ""},
    {"ORANGÉ", "JAUNE"}, {"JAUNE", ""},   {"JAUNE", "VERT"},
    {"VERT", ""},    {"VERT", "BLEU"},    {"BLEU", ""},
    {"BLEU", "VIOLET"},  {"VIOLET", ""},  {"VIOLET", "ROUGE"},
}};
const std::array<const char *, 12> kFlatNames = {{
    "ROUGE", "ROUGE-ORANGÉ", "ORANGÉ", "ORANGÉ-JAUNE", "JAUNE", "JAUNE-VERT",
    "VERT", "VERT-BLEU", "BLEU", "BLEU-VIOLET", "VIOLET", "VIOLET-ROUGE",
}};

// Newton's seven, placed on Chevreul's twelve names (§5.4). kIndigo is a
// READING, not a citation.
constexpr int kNRed = 0, kNOrange = 12, kNYellow = 24, kNGreen = 36,
              kNBlue = 48, kNIndigo = 54, kNViolet = 60;
const std::array<int, 7> kNewton = {{kNRed, kNOrange, kNYellow, kNGreen,
                                     kNBlue, kNIndigo, kNViolet}};
const std::array<const char *, 7> kNewtonName = {
    {"RED", "ORANGE", "YELLOW", "GREEN", "BLUE", "INDIGO", "VIOLET"}};

// §21–§37, transcribed complete, in Chevreul's own plate order 1–17.
struct Observation {
  int plate;      // Chevreul's plate number
  int a, b;       // index into kNewton
  const char *modA;
  const char *modB;
  int para;       // paragraph
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
// Y = 0.5). §4.3's table exists partly as the worked check: tone 10 of the
// grey gamme must be #C0C0C0 and not #808080.

inline float toLinear(float c) {
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
inline float toSrgb(float c) {
  c = std::clamp(c, 0.0f, 1.0f);
  return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}
inline SkColor4f lerpLinear(SkColor4f a, SkColor4f b, float t) {
  return {toSrgb(toLinear(a.fR) + (toLinear(b.fR) - toLinear(a.fR)) * t),
          toSrgb(toLinear(a.fG) + (toLinear(b.fG) - toLinear(a.fG)) * t),
          toSrgb(toLinear(a.fB) + (toLinear(b.fB) - toLinear(a.fB)) * t),
          a.fA + (b.fA - a.fA) * t};
}
/** Relative luminance, Rec.709 / sRGB primaries. */
inline float luminance(SkColor4f c) {
  return 0.2126f * toLinear(c.fR) + 0.7152f * toLinear(c.fG) +
         0.0722f * toLinear(c.fB);
}
struct Lab {
  float L, a, b;
};
inline Lab toLab(SkColor4f c) {
  const float R = toLinear(c.fR), G = toLinear(c.fG), B = toLinear(c.fB);
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
  if (t < 15)
    return lerpLinear(broken, kWhite, (float)(15 - t) / 14.0f);
  if (t > 15)
    return lerpLinear(broken, kBlack, (float)(t - 15) / 5.0f);
  return broken;
}

/** Chevreul's own prediction (§18, §20), made numeric: nudge a colour
 *  toward the complement of its neighbour, in linear light. */
inline SkColor4f predicted(SkColor4f self, int neighbourSector,
                           const std::array<SkColor4f, 72> &wheel,
                           float amount = 0.22f) {
  return lerpLinear(self, wheel[(size_t)complementOf(neighbourSector)], amount);
}

// ---------------------------------------------------------------------------
// typography

inline sk_sp<SkTypeface> face(const char *family, SkFontStyle style,
                              const char *fallback) {
  auto mgr = weave::ports::systemFontManager();
  if (!mgr)
    return nullptr;
  sk_sp<SkTypeface> t = mgr->matchFamilyStyle(family, style);
  if (!t && fallback)
    t = mgr->matchFamilyStyle(fallback, style);
  if (!t)
    t = mgr->matchFamilyStyle(nullptr, style);
  return t;
}
inline const sk_sp<SkTypeface> &serif() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Normal(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface> &serifIt() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Italic(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface> &serifBold() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::Bold(), "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface> &mono() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Normal(), "Courier New");
  return f;
}

inline weave::TextStyle ty(const sk_sp<SkTypeface> &tf, float size,
                           SkColor4f color, float track = 0) {
  weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}
inline weave::TextStyle sr(float sz, SkColor4f c, float tr = 0) {
  return ty(serif(), sz, c, tr);
}
inline weave::TextStyle sbd(float sz, SkColor4f c, float tr = 0) {
  return ty(serifBold(), sz, c, tr);
}
inline weave::TextStyle it(float sz, SkColor4f c, float tr = 0) {
  return ty(serifIt(), sz, c, tr);
}
inline weave::TextStyle mn(float sz, SkColor4f c, float tr = 0) {
  return ty(mono(), sz, c, tr);
}

inline std::u8string U(const std::string &s) { return toU8(s); }
inline std::string fmt(const char *f, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return std::string(buf);
}
inline std::string hexOf(SkColor4f c) {
  auto q = [](float v) { return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f); };
  return fmt("#%02X%02X%02X", q(c.fR), q(c.fG), q(c.fB));
}

// The whole composition is pinned: an engraved plate has no layout.
inline Element at(float x, float y, float w, float h) {
  return box().absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
}
inline Element label(const std::string &s, const weave::TextStyle &st, float x,
                     float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f).child(text(U(s), st));
}
inline Element centred(const std::string &s, const weave::TextStyle &st,
                       float x, float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f)
      .child(text(U(s), st).textAlign(weave::TextAlignment::kCenter).width(Dim(w)));
}
inline Element rightAt(const std::string &s, const weave::TextStyle &st,
                       float x, float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.7f)
      .child(text(U(s), st).textAlign(weave::TextAlignment::kEnd).width(Dim(w)));
}

/** The rim baseline: a circle wound COUNTER-CLOCKWISE and starting at
 *  screen-angle 90° (the bottom of the plate, where ROUGE is). Two facts
 *  fall out of it and both matter:
 *   - the arc-length fraction of sector n is exactly n/72;
 *   - the tangent runs the way the engraver set the type, so glyph-up
 *     points radially INWARD everywhere, which is what the plate does.
 *  shapes::circle() is addOval(kCW) and gives the opposite convention. */
inline shapes::OutlineFn rimBaseline() {
  return [](SkSize s) {
    const SkRect r = SkRect::MakeWH(s.width(), s.height());
    SkPathBuilder b;
    b.arcTo(r, 90.0f, -180.0f, true);
    b.arcTo(r, -90.0f, -180.0f, false);
    b.close();
    return b.detach();
  };
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
  float missOrigin = 0, missOriginMax = 0, missCentroid = 0, missCentroidMax = 0;
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

} // namespace

// ===========================================================================

struct ChevreulCircle : sigil::compose::sketch::Sketch {
  static constexpr float kW = 1800.0f;
  static constexpr float kH = 1200.0f;

  // the wheel
  static constexpr SkPoint kC{436, 500};
  static constexpr float kRColour = 292.0f;  // outer edge of the colour band
  static constexpr float kInner = 0.34f;     // the paper medallion
  static constexpr float kRLimbIn = 296.0f;
  static constexpr float kRLimbOut = 328.0f;
  static constexpr float kRSweepIn = 333.0f; // the continuous-sweep ring
  static constexpr float kRSweepOut = 344.0f;

  // the staircases
  static constexpr float kBandW = 44.0f;
  static constexpr int kBandN = 20;
  static constexpr float kStairX = 860.0f;   // 20 x 44 = 880, integer, exact
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
  std::vector<std::string> verifyLines;
  std::vector<bool> verifyFail;
  std::string derivation1, derivation2;
  std::string counterText = "";

  Material paperGrain, plateTone, sweepRing, medallionGlow;
  std::shared_ptr<weave::Paragraph> lawPara;

  std::shared_ptr<instancing::Atlas> quadAtlas;
  std::shared_ptr<instancing::Pool> quadPool;
  int quadFrame = 0;

  // ==================================================================
  // verification

  void computeColours() {
    for (int n = 0; n < 72; ++n) {
      corrected[(size_t)n] = hex(kCorrectedHex[(size_t)n]);
      scanned[(size_t)n] = hex(kScannedHex[(size_t)n]);
      lab[(size_t)n] = toLab(corrected[(size_t)n]);
    }
    // §164 read as equal REFLECTANCE, against the modern equal-code-value
    // ramp the illusion demonstrations use.
    for (int t = 1; t <= 20; ++t) {
      const float Y = (float)(20 - t) / 19.0f;
      const float s = toSrgb(Y);
      gamme[(size_t)(t - 1)] = {s, s, s, 1};
      const float c = (float)(20 - t) / 19.0f;
      gammeCode[(size_t)(t - 1)] = {c, c, c, 1};
    }
  }

  void verify(sketch::SketchContext &ctx) {
    // --- 1. the circle closes, two ways -----------------------------
    v.named = 12;
    v.perNamed = 6; // one named + five numbered intermediates
    v.closes1 = v.named * v.perNamed;
    v.closes2 = 3 + 3 * 23; // colorants.hypotheses.org's framing
    derivation1 = fmt("120/2 = 60  ->  60/2 = 30  ->  30/5 = 6 deg;  "
                      "360/6 = %d sectors of %.1f deg",
                      360 / 6 * 1, kSectorDeg);
    // (the arithmetic, spelled the way §161 builds it)
    derivation1 = fmt("3 arcs of 120 -> 6 of 60 -> 12 of 30, each divided "
                      "in 5: 12 + 60 = %d",
                      v.closes1);

    // --- 2. the system total (§163-§165) ----------------------------
    const long plane = 72L * 20L;          // the circle's own plane
    const long broken = 9L * 72L * 20L;    // nine radii broken by tenths
    const long grey = 20L;                 // the tenth radius, normal grey
    v.total = plane + broken + grey;
    derivation2 = fmt("72 x 20 = %ld  +  9 x 72 x 20 = %ld  +  20 grey  =  %ld",
                      plane, broken, v.total);

    // --- 3. the plate's own diameter --------------------------------
    v.plateDelta = kScanVert - kScanRouge;

    // --- 4. §6's four statements against §161's +36 rule ------------
    struct Claim { int a, b; const char *text; };
    const std::array<Claim, 4> claims = {{
        {kNRed, kNGreen, "Red / Green"},
        {kNOrange, kNBlue, "Orange / Blue"},
        {18 /*ORANGE-JAUNE*/, 54 /*BLEU-VIOLET*/, "Orange-Yellow / Indigo"},
        {30 /*JAUNE-VERT*/, 60 /*VIOLET*/, "Greenish-Yellow / Violet"},
    }};
    for (const Claim &c : claims) {
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
    for (const Observation &o : kObs) {
      inTable[(size_t)o.a][(size_t)o.b] = true;
      inTable[(size_t)o.b][(size_t)o.a] = true;
    }
    // the four pairs Chevreul's PROSE names complementary
    std::array<std::array<bool, 7>, 7> byProse{};
    auto mark = [&](int i, int j) {
      byProse[(size_t)i][(size_t)j] = byProse[(size_t)j][(size_t)i] = true;
    };
    mark(0, 3); // §6  Red / Green
    mark(1, 4); // §6  Orange / Blue
    mark(2, 6); // §6  Greenish-Yellow / Violet
    mark(2, 5); // §29-§36 "the complementary of Yellow (Indigo inclining
                //          to violet)"
    v.pairs21 = 0;
    v.byName = v.byStrict = v.byLoose = 0;
    v.nameSetMatches = true;
    for (int i = 0; i < 7; ++i)
      for (int j = i + 1; j < 7; ++j) {
        ++v.pairs21;
        const int sep = sepSectors(kNewton[(size_t)i], kNewton[(size_t)j]);
        if (!byProse[(size_t)i][(size_t)j])
          ++v.byName;
        if (sep != 36)
          ++v.byStrict;
        if (sep < 30)
          ++v.byLoose;
        // the sharp version: the prose set must be EXACTLY the absent set
        if (byProse[(size_t)i][(size_t)j] == inTable[(size_t)i][(size_t)j])
          v.nameSetMatches = false;
      }

    // --- 6/7. the measured hues wind once, and not evenly -----------
    std::array<float, 72> hab{}, steps{};
    for (int n = 0; n < 72; ++n) {
      float h = std::atan2(lab[(size_t)n].b, lab[(size_t)n].a) * 180.0f / 3.14159265f;
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
      if (d < v.hueWorst) { v.hueWorst = d; v.hueWorstAt = n; }
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
    for (const Lab &l : lab) { v.centA += l.a; v.centB += l.b; }
    v.centA /= 72.0f;
    v.centB /= 72.0f;
    v.meanChroma = 0;
    for (const Lab &l : lab) v.meanChroma += std::hypot(l.a, l.b);
    v.meanChroma /= 72.0f;
    auto chordMiss = [&](float px, float py, float &mean, float &worst) {
      mean = 0; worst = 0;
      for (int n = 0; n < 36; ++n) {
        const Lab &A = lab[(size_t)n], &B = lab[(size_t)(n + 36)];
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
      if (i != 4 && luminance(corrected[(size_t)(i * 6)]) > v.yJaune)
        v.jauneHighest = false;
    v.bleuDarker = v.yBleu < v.yRouge; // §160 says it should be

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
      sk_sp<SkPicture> pic = snapshot(std::move(strip), *ctx.fonts);
      sk_sp<SkSurface> surf = SkSurfaces::Raster(
          SkImageInfo::MakeN32Premul((int)(kBandW * kBandN), 32));
      if (pic && surf) {
        surf->getCanvas()->clear(SK_ColorTRANSPARENT);
        surf->getCanvas()->drawPicture(pic.get());
        SkPixmap pm;
        if (surf->peekPixels(&pm)) {
          for (int b = 0; b < kBandN; ++b) {
            const int x0 = (int)(b * kBandW);
            const SkColor want = gamme[(size_t)b].toSkColor();
            int dev = 0;
            // interior only: the band edges are antialiased and a 1 px
            // blend seam there is correct behaviour, not a defect.
            double sum = 0, sum2 = 0;
            int cnt = 0;
            for (int x = x0 + 6; x < x0 + (int)kBandW - 6; x += 2)
              for (int y = 6; y < 26; y += 2) {
                const SkColor got = pm.getColor(x, y);
                dev = std::max({dev,
                                std::abs((int)SkColorGetR(got) - (int)SkColorGetR(want)),
                                std::abs((int)SkColorGetG(got) - (int)SkColorGetG(want)),
                                std::abs((int)SkColorGetB(got) - (int)SkColorGetB(want))});
                const double lum = SkColorGetG(got);
                sum += lum;
                sum2 += lum * lum;
                ++cnt;
              }
            const double mean = cnt ? sum / cnt : 0.0;
            const double var = cnt ? std::max(0.0, sum2 / cnt - mean * mean) : 0.0;
            v.bandSigmaMax = std::max(v.bandSigmaMax, (float)std::sqrt(var));
            v.bandMaxDev = std::max(v.bandMaxDev, dev);
            if (dev == 0)
              ++v.bandsExact;
          }
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
        pieces.push_back(shapes::sector(sectorStart(n), kSectorDeg, kInner)(sz));
      SkPathBuilder hub;
      hub.addOval(SkRect::MakeXYWH(kRColour * (1 - kInner), kRColour * (1 - kInner),
                                   2 * kRColour * kInner, 2 * kRColour * kInner));
      pieces.push_back(hub.detach());
      SkPathBuilder region;
      region.addOval(SkRect::MakeWH(sz.width(), sz.height()));
      const SkPath regionPath = region.detach();
      const debug::Coverage cov = debug::coverage(pieces, regionPath, 256);
      v.covSamples = cov.samples;
      v.covUncovered = cov.uncovered;
      v.covDoubled = cov.doubled;

      // endpointDegrees on the same 72 pieces. It used to report 72 points
      // of degree 1 for a ring of closed sectors; it now retracts them and
      // says how many contours were CLOSED, so "this test does not apply"
      // is legible instead of inferred.
      std::vector<SkPath> sectorsOnly(pieces.begin(), pieces.end() - 1);
      const debug::VertexDegrees deg = debug::endpointDegrees(sectorsOnly);
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
      sk_sp<SkPicture> pic = snapshot(std::move(probe), *ctx.fonts);
      sk_sp<SkSurface> surf = SkSurfaces::Raster(
          SkImageInfo::MakeN32Premul((int)std::ceil(gw), (int)std::ceil(gh)));
      if (pic && surf) {
        surf->getCanvas()->clear(SK_ColorTRANSPARENT);
        surf->getCanvas()->drawPicture(pic.get());
        SkPixmap pm;
        if (surf->peekPixels(&pm)) {
          const auto pos = quadPool->positions();
          const auto tints = quadPool->tints();
          for (size_t i = 0; i < pos.size(); ++i) {
            const SkColor got =
                pm.getColor((int)pos[i].fX, (int)pos[i].fY);
            const SkColor want = tints[i].toSkColor();
            const int dev = std::max(
                {std::abs((int)SkColorGetR(got) - (int)SkColorGetR(want)),
                 std::abs((int)SkColorGetG(got) - (int)SkColorGetG(want)),
                 std::abs((int)SkColorGetB(got) - (int)SkColorGetB(want))});
            ++v.tintCells;
            v.tintMaxDev = std::max(v.tintMaxDev, dev);
            if (dev == 0)
              ++v.tintExact;
          }
        }
      }
    }

    // --- the OCIO seam, MEASURED rather than asserted -----------------
    //     Ocio.h is the least-exercised header in the library and nothing
    //     anywhere states what a value comes out as. Push tone 10 of the
    //     §164 gamme (#C0C0C0) through ocio::exponent(2.2) and read it back
    //     off a raster surface, exactly the way check 10 reads the
    //     staircase — so "the seam works" is a number on the plate.
    v.ocioAvailable = ocio::available();
    if (v.ocioAvailable && ctx.fonts) {
      // snapshot() sizes by the ROOT'S CHILDREN, not the root's own dims,
      // so the probe needs a shell — the same rule Instances.h's atlas bake
      // states and nothing else does. Without it the effect lands on an
      // empty root and reads back a value that is not what gets drawn.
      Element probe = box().child(box()
                                      .width(Dim(32))
                                      .height(Dim(32))
                                      .fill(Fill::color(gamme[9]))
                                      .effect(ocio::exponent(2.2f)));
      sk_sp<SkPicture> pic = snapshot(std::move(probe), *ctx.fonts);
      sk_sp<SkSurface> surf =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
      if (pic && surf) {
        surf->getCanvas()->clear(SK_ColorTRANSPARENT);
        surf->getCanvas()->drawPicture(pic.get());
        SkPixmap pm;
        if (surf->peekPixels(&pm)) {
          const SkColor got = pm.getColor(16, 16);
          v.ocioSample = fmt("#%02X%02X%02X", SkColorGetR(got),
                             SkColorGetG(got), SkColorGetB(got));
        }
      }
    }
  }

  void buildQuadrantPool() {
    quadAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // ONE white cell. Every one of the 200 colours arrives as a tint, which
    // is precisely the fidelity question nobody had asked of this path.
    quadFrame = quadAtlas->cell(box().fill(Fill::color(kWhite)),
                                {kQCellW, kQCellH});
    quadPool = std::make_shared<instancing::Pool>();
    quadPool->resize(200);
    auto pos = quadPool->positions();
    auto tint = quadPool->tints();
    auto fr = quadPool->frames();
    for (int k = 0; k < 10; ++k)
      for (int t = 0; t < 20; ++t) {
        const size_t i = (size_t)(k * 20 + t);
        pos[i] = {(float)k * (kQCellW + kQGapX) + kQCellW * 0.5f,
                  (float)t * (kQCellH + kQGapY) + kQCellH * 0.5f};
        tint[i] = quadrantCell(corrected[0], k + 1, t + 1);
        fr[i] = quadFrame;
      }
    quadPool->touch();
  }

  void buildVerifyLines() {
    auto ok = [](bool b) { return b ? "OK" : "FAIL"; };
    verifyLines.clear();
    verifyFail.clear();
    auto add = [&](const std::string &s, bool fail) {
      verifyLines.push_back(s);
      verifyFail.push_back(fail);
    };
    add(fmt("CIRCLE CLOSES     %d named x %d = %d      3 + 3x23 = %d",
            v.named, v.perNamed, v.closes1, v.closes2),
        !(v.closes1 == 72 && v.closes2 == 72));
    add(fmt("SYSTEM TOTAL      72 x 20 x 10 + 20 grey = %ld", v.total),
        v.total != 14420);
    add(fmt("PLATE DIAMETER    ROUGE %.1f  VERT %.1f  delta %.2f", kScanRouge,
            kScanVert, v.plateDelta),
        std::fabs(v.plateDelta - 180.0f) > 0.005f);
    add(fmt("SEVENTEEN         C(7,2)=%d - 4 named complementary = %d",
            v.pairs21, v.byName),
        v.byName != 17 || !v.nameSetMatches);
    add(fmt("  by geometry     sep==36 -> %d ;  sep>=30 -> %d", v.byStrict,
            v.byLoose),
        true);
    add(fmt("HUE WINDS ONCE    sum %.2f deg   %d/72 steps > 0", v.hueSum,
            v.huePositive),
        v.huePositive != 72);
    add(fmt("EQUAL SECTORS     hue step mean %.2f sd %.2f min %.2f max %.2f",
            v.hueMean, v.hueSd, v.hueMin, v.hueMax),
        false);
    add(fmt("DIAMETERS         miss: origin %.2f   centroid %.2f  (%.1f%%)",
            v.missOrigin, v.missCentroid, v.missPercent),
        false);
    add(fmt("COMPLEMENTARIES   %d of 4 exact;  greenish-yellow/violet off "
            "%.0f deg",
            v.compExact, v.compWorstDeg),
        v.compOff != 0);
    add(fmt("LUMINOSITY (160)  jaune %.4f highest;  bleu %.4f > rouge %.4f",
            v.yJaune, v.yBleu, v.yRouge),
        !v.bleuDarker);
    add(fmt("STAIRCASE         %d bands, sigma %.2f, %d/%d hexes exact",
            v.bands, v.bandSigmaMax, v.bandsExact, v.bands),
        !(v.bandsExact == v.bands && v.bandSigmaMax == 0.0f));
    add(fmt("EXACT COVER       %d uncovered / %d doubled of %d", v.covUncovered,
            v.covDoubled, v.covSamples),
        !(v.covUncovered == 0 && v.covDoubled == 0));
    add(fmt("INSTANCE TINTS    %d/%d cells exact, max channel dev %d",
            v.tintExact, v.tintCells, v.tintMaxDev),
        v.tintExact != v.tintCells);
  }

  void buildLaw() {
    weave::TextStyle body = sr(13, kInk);
    body.shaping.languageTag = "en-GB";
    weave::ParagraphBuilder b(body);
    // SigilWeave breaks at SOFT HYPHENS only, so the discretionaries are
    // typed in the way a compositor would set them.
    b.addText(u8"“In the case where the eye sees at the same time two "
              u8"con­tigu­ous col­ours, they will appear as "
              u8"dis­sim­i­lar as pos­si­ble, both "
              u8"in their op­ti­cal com­po­si­tion "
              u8"and in the height of their tone.”  ");
    b.pushStyle(it(11, kInk2));
    b.addText(u8"— M. E. Chevreul, §16, De la loi du contraste simultané des "
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

  Element theWheel(sketch::SketchContext &ctx) {
    Element g = box();

    // the panel's own shadow, attached FIRST so the fill paints over it
    g.child(disc(kC, kRSweepOut + 6)
                .outline(shapes::circle())
                .background(styles::dropShadow(hex(0x3A352D, 0.30f), {3, 3}, 8))
                .fill(Fill::color(kPaper)));

    // ---- the limb's tint and its two engraved circles ---------------
    g.child(disc(kC, kRLimbOut)
                .outline(shapes::annulus(kRLimbIn / kRLimbOut))
                .fill(Fill::color(kWell))
                .opacity(bind(&demo).window(0.15f, 0.19f)));
    for (float r : {kRLimbIn, kRLimbOut})
      g.child(disc(kC, r)
                  .key(fmt("limb%.0f", r))
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .stroke(stroke(1.0f, Fill::color(kRule)))
                  .trim(0.0f, bind(&demo).window(0.14f, 0.20f)));

    // ---- the 72 couleurs franches -----------------------------------
    // Drawn with a 0.25 deg overlap: with no overlap the antialiased edges
    // leave a paper-coloured seam at every boundary, which reads as a thin
    // white radius — exactly what the plate has — and then DOUBLES the
    // radialHatch separators. The hatch supplies all the white; check 11
    // runs against the un-overlapped geometry.
    for (int n = 0; n < 72; ++n) {
      const float lo = 0.005f + 0.0021f * (float)n;
      g.child(disc(kC, kRColour)
                  .key("sector" + std::to_string(n))
                  .outline(shapes::sector(sectorStart(n) - 0.125f,
                                          kSectorDeg + 0.25f, kInner))
                  .fill(Fill::color(corrected[(size_t)n]))
                  .transformOrigin(0.5f, 0.5f)
                  .opacity(bind(&demo).window(lo, lo + 0.010f))
                  .scale(bind(&demo)
                             .window(lo, lo + 0.014f)
                             .map(ch::EaseFn(ease::outBack(1.2f)))
                             .to(0.86f, 1.0f)));
    }

    // the plate's seventy-two white radii: ONE decoration, not 72 elements.
    // rotateDeg puts the rules on the sector BOUNDARIES (92.5 deg mod 5).
    {
      lines::RadialHatch h = lines::radialHatch(Fill::color(kPaper), 72, 2.4f);
      h.rotateDeg = 2.5f;
      h.holeFraction = kInner * 0.70f;
      g.child(disc(kC, kRColour)
                  .key("radii")
                  .outline(shapes::annulus(kInner))
                  .fill(Fill::none())
                  .foreground(h)
                  .opacity(bind(&demo).window(0.16f, 0.20f)));
    }

    // plate tone: real intaglio leaves the whole printed area faintly
    // toned. Clipped to the wheel, cached as a texture.
    if (!std::getenv("CHEVREUL_NOTONE"))
      g.child(disc(kC, kRColour)
                  .outline(shapes::circle())
                  .fill(Fill::none())
                  .foreground(decorations::wash(plateTone,
                                                SkBlendMode::kMultiply, 0.055f))
                  .cache(Cache::Texture));

    // ---- the continuous-sweep ring ----------------------------------
    // The same 72 measured values as ONE gradient: 144 stops (doubled, so
    // the steps stay franches) in one shader. The six-stop cap that made
    // this impossible closed on 2026-07-22; this ring is the confirmation,
    // and it is on-subject — continuous against franche is the distinction
    // the plate's own title makes.
    g.child(disc(kC, kRSweepOut)
                .key("sweepring")
                .outline(shapes::annulus(kRSweepIn / kRSweepOut))
                .fill(sweepRing)
                .opacity(bind(&demo).window(0.17f, 0.22f)));

    // ---- the medallion ----------------------------------------------
    const float rMed = kRColour * kInner;
    g.child(disc(kC, rMed + 3)
                .outline(shapes::circle())
                .fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                         {{0.0f, hex(0x8C8578, 0.0f)},
                                          {0.72f, hex(0x8C8578, 0.0f)},
                                          {1.0f, hex(0x8C8578, 0.22f)}})));
    g.child(disc(kC, rMed)
                .outline(shapes::circle())
                .fill(Fill::color(kPaper))
                .stroke(stroke(1.0f, Fill::color(kRule))));
    {
      // the plate's own engraved caption, five lines, its own line breaks
      struct Cap { const char *s; float size; float track; bool bold; float dy; };
      const std::array<Cap, 6> caps = {{
          {"1er", 11, 0, false, -66},
          {"CERCLE CHROMATIQUE", 13.5f, 0.7f, false, -46},
          {"DE", 8, 2.0f, false, -25},
          {"Mr CHEVREUL", 17, 0.9f, true, -6},
          {"RENFERMANT", 7.5f, 1.6f, false, 17},
          {"LES·COULEURS·FRANCHES.", 9.5f, 0.5f, false, 36},
      }};
      for (size_t i = 0; i < caps.size(); ++i) {
        const Cap &c = caps[i];
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
        ctx.fonts ? sigil::compose::metrics(nameSt, *ctx.fonts).capHeight : 6.0f;
    const float capNum =
        ctx.fonts ? sigil::compose::metrics(numSt, *ctx.fonts).capHeight : 6.6f;
    for (int n = 0; n < 72; ++n) {
      const float f = (float)n / 72.0f; // exact, by construction of rimBaseline
      const float lo = 0.20f + 0.0009f * (float)n;
      static const bool kNoLimb = std::getenv("CHEVREUL_NOLIMB") != nullptr;
      auto run = [&](const std::string &s, const weave::TextStyle &st,
                     float offset, const std::string &key) {
        if (kNoLimb)
          return;
        // The BASELINE resolves against the text node's OWN laid-out box, so
        // the run has to be the disc-sized element itself; wrapping it in a
        // sized parent silently collapses all 72 labels onto one point.
        g.child(text(U(s), st)
                    .key(key)
                    .absolute()
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
        const ScaleName &nm = kNames[(size_t)(n / 6)];
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
                  .absolute()
                  .left(Dim(kC.fX - kRLimbOut))
                  .top(Dim(kC.fY - kRLimbOut))
                  .width(Dim(2 * kRLimbOut))
                  .height(Dim(2 * kRLimbOut))
                  .key("div" + std::to_string(n))
                  .fill(Fill::none())
                  .outline([bd](SkSize s) {
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
                  .absolute()
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
    g.child(disc(kC, kRColour)
                .key("diam")
                .fill(Fill::none())
                .outline(diameter())
                .stroke(stroke(1.2f, Fill::color(kRed)))
                .transformOrigin(0.5f, 0.5f)
                .rotate(bind(&demo).window(0.18f, 0.30f).to(0.0f, 180.0f))
                .opacity(bind(&demo).window(0.18f, 0.30f).map(pulses(1))));

    // ---- the "161 years of paper" inset ------------------------------
    {
      const float x0 = 58, y0 = 762, w = 19, h = 21;
      Element ins = box();
      ins.child(label("161 YEARS OF PAPER", mn(7.0f, kInk2, 0.5f), x0, y0 - 14, 200));
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
          {fmt("debug::coverage over an SkPath REGION: %d/%d of %d · "
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
    g.child(label("CHEVREUL'S QUADRANT · §163–§165 · ROUGE, TEN RADII × TWENTY "
                  "TONES = 200 CELLS",
                  mn(9.0f, kInk, 0.6f), 56, 918, 760));
    const float gw = 10 * (kQCellW + kQGapX) - kQGapX;
    const float gh = 20 * (kQCellH + kQGapY) - kQGapY;

    // column heads
    const std::array<const char *, 10> heads = {
        {"1/10", "2/10", "3/10", "4/10", "5/10", "6/10", "7/10", "8/10", "9/10",
         "NOIR"}};
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
    const float scale = (S - 96) * 0.5f / 60.0f; // a* b* -60..+60
    auto P = [&](float a, float b) {
      return SkPoint{cx + a * scale, cy - b * scale};
    };
    Element g = box();
    g.child(at(x0, y0, S, S)
                .fill(Fill::color(kWell))
                .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("CIELAB a* b* · THE 72 MEASURED COLOURS · 36 DIAMETERS",
                  mn(8.5f, kInk, 0.5f), x0 + 10, y0 + 6, S - 20));

    // axes and ticks, drawn on hand-built geometry through the brush
    // vocabulary — decorations::paintOn is the seam for that.
    g.child(at(x0, y0, S, S).fill(Fill::none()).child(
        custom([=](SkCanvas &c, const PaintContext &pc) {
          SkPathBuilder ax;
          for (int t = -60; t <= 60; t += 20) {
            const SkPoint a = P((float)t, -60), b = P((float)t, 60);
            ax.moveTo(a.fX - x0, a.fY - y0);
            ax.lineTo(b.fX - x0, b.fY - y0);
            const SkPoint c0 = P(-60, (float)t), d0 = P(60, (float)t);
            ax.moveTo(c0.fX - x0, c0.fY - y0);
            ax.lineTo(d0.fX - x0, d0.fY - y0);
          }
          decorations::paintOn(c, pc, ax.detach(),
                               stroke(0.5f, Fill::color(hex(0x8C8578, 0.35f))));
          SkPathBuilder cross;
          const SkPoint o = P(0, 0);
          cross.moveTo(o.fX - x0 - 9, o.fY - y0);
          cross.lineTo(o.fX - x0 + 9, o.fY - y0);
          cross.moveTo(o.fX - x0, o.fY - y0 - 9);
          cross.lineTo(o.fX - x0, o.fY - y0 + 9);
          decorations::paintOn(c, pc, cross.detach(),
                               stroke(1.2f, Fill::color(kInk)));
        })
            .absolute()
            .inset(0)));

    // the 36 chords, drawing in one at a time
    for (int n = 0; n < 36; ++n) {
      const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
      const SkPoint B = P(lab[(size_t)(n + 36)].a, lab[(size_t)(n + 36)].b);
      SkRect bb = SkRect::MakeLTRB(std::min(A.fX, B.fX), std::min(A.fY, B.fY),
                                   std::max(A.fX, B.fX), std::max(A.fY, B.fY));
      bb.outset(2, 2);
      const SkPoint a0{A.fX - bb.left(), A.fY - bb.top()};
      const SkPoint b0{B.fX - bb.left(), B.fY - bb.top()};
      const float lo = 0.19f + 0.0026f * (float)n;
      g.child(at(bb.left(), bb.top(), bb.width(), bb.height())
                  .key("chord" + std::to_string(n))
                  .fill(Fill::none())
                  .outline([a0, b0](SkSize) {
                    SkPathBuilder p;
                    p.moveTo(a0);
                    p.lineTo(b0);
                    return p.detach();
                  })
                  .stroke(stroke(0.8f, Fill::color(hex(0x8C8578, 0.85f))))
                  .trim(0.0f, bind(&demo).window(lo, lo + 0.012f)));
    }
    // the 72 points, each in its own colour
    for (int n = 0; n < 72; ++n) {
      const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
      const float lo = 0.005f + 0.0021f * (float)n;
      g.child(disc(A, 3.5f)
                  .key("labpt" + std::to_string(n))
                  .outline(shapes::circle())
                  .fill(Fill::color(corrected[(size_t)n]))
                  .stroke(stroke(0.4f, Fill::color(hex(0x221F1A, 0.5f))))
                  .transformOrigin(0.5f, 0.5f)
                  .opacity(bind(&demo).window(lo, lo + 0.01f)));
    }
    // the centroid — the piece's whole argument
    g.child(disc(P(v.centA, v.centB), 9.0f)
                .key("centroid")
                .outline(shapes::circle())
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
    g.child(label(counterText, mn(8.0f, kRed, 0.2f), x0 + 10, y0 + S - 32,
                  S - 20));
    g.child(label(fmt("centroid a* %.2f  b* %.2f   ·   mean C* %.1f",
                      v.centA, v.centB, v.meanChroma),
                  mn(7.5f, kInk2, 0.2f), x0 + 10, y0 + S - 18, S - 20));
    return g;
  }

  // ------------------------------------------------------------------
  Element theObservations() {
    const float x0 = 1268, y0 = 136, W = 476, H = 380;
    Element g = box();
    g.child(at(x0, y0, W, H)
                .fill(Fill::color(kWell))
                .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("LES DIX-SEPT OBSERVATIONS · §21–§37", mn(8.5f, kInk, 0.5f),
                  x0 + 10, y0 + 6, W - 20));
    const float rowH = 19.4f, top = y0 + 24;
    for (size_t i = 0; i < kObs.size(); ++i) {
      const Observation &o = kObs[i];
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
      Element pred = box().key("pr" + std::to_string(i))
                         .opacity(bind(&demo).window(lo + 0.003f, lo + 0.009f));
      pred.child(at(72, 1, 15, 15)
                     .fill(Fill::color(predicted(ca, kNewton[(size_t)o.b], corrected))));
      pred.child(at(87, 1, 15, 15)
                     .fill(Fill::color(predicted(cb, kNewton[(size_t)o.a], corrected))));
      row.child(std::move(pred));
      row.child(label(fmt("%s · %s", kNewtonName[(size_t)o.a],
                          kNewtonName[(size_t)o.b]),
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
    g.child(label("indigo read as BLEU-VIOLET (54); greenish-yellow as "
                  "JAUNE-VERT (30) — both readings, not citations",
                  mn(6.5f, kInk2, 0.2f), x0 + 10, y0 + H - 18, W - 20));
    return g;
  }

  // ------------------------------------------------------------------
  Element aStaircase(const std::array<SkColor4f, 20> &ramp, float y, float h,
                     const char *keyBase, bool withGap) {
    Element g = box();
    for (int b = 0; b < kBandN; ++b) {
      Element band = at(kStairX + (float)b * kBandW, y, kBandW, h)
                         .key(fmt("%s%d", keyBase, b))
                         .fill(Fill::color(ramp[(size_t)b]));
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
    g.child(label("THE CHEVREUL ILLUSION · TWENTY FLAT BANDS", mn(9, kInk, 0.6f),
                  852, 552, 500));
    g.child(rightAt("§164 read as equal REFLECTANCE   ·   the modern equal "
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
      Element third = aStaircase(gamme, kStairYC, 28.0f, "sc", false);
      third.effect(ocio::exponent(2.2f));
      g.child(std::move(third));
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
      g.child(centred(hexOf(gamme[(size_t)b]),
                      mn(6.8f, b < 12 ? kInk : kWhite, 0),
                      kStairX + (float)b * kBandW, kStairYA + kStairH - 12,
                      kBandW));

    g.child(label("“the light tone will appear lighter, and the deep tone "
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
    // ground away and the twelve patches are plainly identical. Roadmap §6
    // closed this; before it, this gesture was a translate plus a clip.
    // wipe() reveals the fraction of THE NODE'S OWN LAID-OUT BOX before the
    // edge, so the container has to be a real box: a bare box() holding
    // absolutely-positioned children measures zero and the wipe hides the
    // whole subtree with no diagnostic at all. Hence the explicit rect.
    Element lattice = at(gx, gy, 4 * cw, 3 * chh)
                          .key("grounds")
                          .wipe(90.0f, bind(&demo)
                                           .window(0.50f, 0.64f)
                                           .map(upHoldAwayBack()));
    for (int i = 0; i < 12; ++i) {
      const int col = i % 4, row = i / 4;
      lattice.child(at((float)col * cw, (float)row * chh, cw - 3, chh - 3)
                        .fill(Fill::color(corrected[(size_t)(i * 6)])));
    }
    g.child(std::move(lattice));
    for (int i = 0; i < 12; ++i) {
      const int col = i % 4, row = i / 4;
      g.child(at(gx + (float)col * cw + (cw - 3 - 30) * 0.5f,
                 gy + (float)row * chh + (chh - 3 - 30) * 0.5f, 30, 30)
                  .fill(Fill::color(gamme[14]))); // Chevreul's grey tone 15
    }
    const float ry = gy + 3 * chh + 6;
    for (int i = 0; i < 12; ++i)
      g.child(at(gx + (float)i * 27.0f, ry, 24, 24).fill(Fill::color(gamme[14])));
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

    const float ty0 = y0 + 96, lh = 13.2f;
    g.child(at(x0 - 8, ty0 - 8, W - 4, (float)verifyLines.size() * lh + 16)
                .fill(Fill::color(kWell))
                .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", mn(7.5f, kInk2, 0.5f), x0,
                  ty0 - 22, W));
    for (size_t i = 0; i < verifyLines.size(); ++i) {
      const float lo = 0.30f + 0.050f * (float)i;
      Element row = at(x0, ty0 + (float)i * lh, W - 20, lh)
                        .key("vr" + std::to_string(i))
                        .opacity(bind(&demo).window(lo, lo + 0.012f));
      row.child(text(U(verifyLines[i]), mn(8.4f, kInk, 0.05f)));
      const bool fail = verifyFail[i];
      row.child(rightAt(i == 4 ? "" : (fail ? "FAIL" : "OK"),
                        fail ? mn(8.4f, kRed, 0.6f) : mn(8.4f, kRed, 0.2f), 0, 0,
                        W - 24));
      g.child(std::move(row));
    }
    g.child(label("§38: “do we know, at the present day, of two coloured "
                  "bodies … Certainly not!”",
                  it(8.5f, kInk2), x0, ty0 + (float)verifyLines.size() * lh + 12,
                  W));
    return g;
  }

  // ==================================================================
  Element describe(sketch::SketchContext &ctx) {
    Element root = stack().width(Dim(kW)).height(Dim(kH));

    // the leaf: measured paper, its tooth, and the platemark
    root.child(at(0, 0, kW, kH).fill(Fill::color(kPaper)));
    if (!std::getenv("CHEVREUL_NOGRAIN"))
      root.child(at(0, 0, kW, kH)
                     .fill(paperGrain)
                     .blend(SkBlendMode::kMultiply)
                     .opacity(0.085f)
                     .cache(Cache::Texture)); // 1800x1200 of generated material
    root.child(at(28, 28, kW - 56, kH - 56)
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
        mn(8.0f, kInk2, 0.55f), 56, 1130, 1690));
    return root;
  }

  // ==================================================================
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPaper);

    computeColours();

    // materials held as members so their identity survives re-describes
    paperGrain = patterns::grain(0.013f, 4, 11.0f, 0.32f);
    plateTone = patterns::grain(0.085f, 3, 5.0f, 0.45f);

    // the 72 measured values as ONE gradient: 144 stops, doubled so the
    // steps stay franches rather than blending into each other.
    {
      // SkShaders::SweepGradient measures from +x clockwise over
      // [startDeg, endDeg] and CLAMPS outside it, so the ring is authored in
      // the gradient's own 0..360 frame rather than by rotating the range.
      // Sector n spans screen angles [90 − 5(n+0.5), 90 − 5(n−0.5)], so the
      // band boundaries land at (2.5 + 5j)/360 and the band below boundary j
      // is sector (18 − j) mod 72.
      std::vector<Stop> stops;
      stops.reserve(146);
      auto C = [&](int n) { return corrected[(size_t)(((n % 72) + 72) % 72)]; };
      stops.push_back({0.0f, C(18)});
      for (int j = 0; j < 72; ++j) {
        const float p = (2.5f + 5.0f * (float)j) / 360.0f;
        stops.push_back({p, C(18 - j)});
        stops.push_back({p, C(17 - j)});
      }
      stops.push_back({1.0f, C(18)});
      sweepRing = Material::sweep({kRSweepOut, kRSweepOut}, std::move(stops),
                                  0.0f, 360.0f);
    }

    verify(ctx);
    buildVerifyLines();
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
          for (int r = 0; r < 20; ++r)
            tints[(size_t)(k * 20 + r)].fA = a;
        }
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("chordcount", chordCounter());
  }

  int frames = 0;

  void update(double, sketch::SketchContext &ctx) override {
    // Cost, measured rather than guessed: CHEVREUL_STATS=1 dumps the
    // composer's own per-phase timings for a few frames. The two numbers
    // this piece cares about are picturesLive (72 static flat fills plus
    // 78 onPath runs) and paintMs, since a TextPath carries no operator==
    // and therefore cannot prune.
    // CHEVREUL_REDESCRIBE=1 re-describes the whole plate every frame, which
    // is what prices gap 3: TextPath has no operator== by design, so the 78
    // limb runs can never prune and re-record on every render(). Pair it
    // with CHEVREUL_NOLIMB=1 for the other half of the measurement.
    if (std::getenv("CHEVREUL_REDESCRIBE"))
      ctx.composer.render(describe(ctx));
    if (++frames > 2 && frames < 7 && std::getenv("CHEVREUL_STATS")) {
      const Composer::Stats &st = ctx.composer.stats();
      SkDebugf("[chevreul] frame %d  instances %d  picturesLive %d  "
               "reconcile %.2f  layout %.2f  volatile %.2f  paint %.2f ms\n",
               frames, (int)st.instances, (int)st.picturesLive,
               st.reconcileMs, st.layoutMs, st.volatileMs, st.paintMs);
    }

    // The counting numbers of beat 2. PropValue covers floats, colours and
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

SIGIL_SKETCH(ChevreulCircle)
