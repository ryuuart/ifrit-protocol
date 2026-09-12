#pragma once

// Construction data and drawing primitives owned by this study.

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
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/ocio/Ocio.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace measure = sigil::measure;
namespace ocio = sigil::material::ocio;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Effect;
using sigil::material::skia::Paint;
// The whole composition is pinned: an engraved plate has no layout.
using sigil::compose::kit::at;
using sigil::geometry::path::centred;
using namespace std::chrono_literals;
using namespace sigil::weave::literals;
namespace ch = choreograph;
namespace weave = sigil::weave;

namespace chevreul_circle {}
using namespace chevreul_circle;
namespace chevreul_circle {

// ---------------------------------------------------------------------------
// palette — sampled off the 1864 plate

constexpr SkColor4f kPaper = hexColor(0xEFE8D9);  // the plate's unprinted paper
constexpr SkColor4f kWell = hexColor(0xE4DCCA);  // panel wells, the limb's tint
constexpr SkColor4f kRule = hexColor(0x8C8578);  // engraved rules and hairlines
constexpr SkColor4f kInk = hexColor(0x221F1A);   // letterpress
constexpr SkColor4f kInk2 = hexColor(0x5C554A);  // small caps, numerals
constexpr SkColor4f kRed = hexColor(0x8E2F26);   // annotation red
constexpr SkColor4f kShade = hexColor(0x3A352D);  // mounted-panel shadow
constexpr SkColor4f kBlack = hexColor(0x000000);
constexpr SkColor4f kWhite = hexColor(0xFFFFFF);

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

using sigil::material::Lab;
using sigil::material::linearToSrgb;
using sigil::material::luminance;
using sigil::material::srgbToLinear;
using sigil::material::toLab;

inline SkColor4f lerpLinear(SkColor4f a, SkColor4f b, float t) {
  return {linearToSrgb(srgbToLinear(a.fR) +
                       (srgbToLinear(b.fR) - srgbToLinear(a.fR)) * t),
          linearToSrgb(srgbToLinear(a.fG) +
                       (srgbToLinear(b.fG) - srgbToLinear(a.fG)) * t),
          linearToSrgb(srgbToLinear(a.fB) +
                       (srgbToLinear(b.fB) - srgbToLinear(a.fB)) * t),
          a.fA + (b.fA - a.fA) * t};
}
/** Chevreul's index n -> the sector's START angle in Skia degrees
 *  (0° = +x, sweeping clockwise). n = 0 is ROUGE, straight down. */
inline float sectorStart(int n) {
  return arrange::along(kRougeDeg, -360.0f, (size_t)n, kSectors,
                        arrange::Turn::Closed) -
         kSectorDeg * 0.5f;
}
inline float sectorMid(int n) {
  return arrange::along(kRougeDeg, -360.0f, (size_t)n, kSectors,
                        arrange::Turn::Closed);
}
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

inline sk_sp<SkTypeface> serif() {
  return weave::ports::face({"Baskerville", "Times New Roman"},
                            SkFontStyle::Normal());
}
inline sk_sp<SkTypeface> serifIt() {
  return weave::ports::face({"Baskerville", "Times New Roman"},
                            SkFontStyle::Italic());
}
inline sk_sp<SkTypeface> serifBold() {
  return weave::ports::face({"Baskerville", "Times New Roman"},
                            SkFontStyle::Bold());
}
inline sk_sp<SkTypeface> mono() {
  return sketch::kit::houseFace(sketch::kit::Voice::Terminal);
}

// THE PLATE'S OWN SHEET. Every kit component reads the theme in scope, and
// this plate is letterpress on laid paper rather than the house sheet's pale
// ink on black, so the study binds its own: the plate's two faces, its ink,
// and the tight row a machine-read table is set at. The faces are resolved
// once and held, because a style is compared by face POINTER.
inline const sketch::kit::Theme& sheet() {
  static const sketch::kit::Theme look = [] {
    sketch::kit::Theme t;
    t.palette.ground = kPaper;
    t.palette.cellGround = kWell;
    t.palette.ink = kInk;
    t.palette.ash = kInk2;
    t.palette.rule = kRule;
    t.palette.figure = kInk;
    t.type.sans = serif();
    t.type.mono = mono();
    t.type.captionLabel = {8, 0.05f, true};
    t.type.captionNote = {8, 0.05f, true};
    t.spacing.rowGap = 1.6f;
    t.spacing.labelGap = 5;
    t.spacing.swatchSide = 4;
    return t;
  }();
  return look;
}

// THE PLATE'S VOICE is stated once, on the root of the tree it is described
// into: the mono the machine-read lines run in, in the grey of the small
// caps and numerals. A line set otherwise says only what differs — a size,
// a tracking, the letterpress black, a serif cut — and the lines the plate
// sets more than once are classes over the sheet's registers, bound with
// the sheet wherever the plate is described. The italic cut names its own
// face because a sheet holds two.
inline const weave::StyleSheet& classes() {
  static const weave::StyleSheet look =
      sheet()
          .styleSheet()
          .set("heading", {.size = 8.5f, .color = kInk, .track = 0.5f})
          .set("note", {.size = 7.0f, .track = 0.2f})
          .set("column", {.size = 6.5f, .track = 0.2f})
          .set("readout", {.size = 8.0f, .track = 0.2f})
          .set("finding", {.size = 8.0f, .color = kRed, .track = 0.2f})
          .set("quote", {.face = serifIt(), .size = 8.5f});
  return look;
}

inline std::u8string U(const std::string& s) { return toUtf8(s); }
inline std::string hexOf(SkColor4f c) {
  auto q = [](float v) {
    return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f);
  };
  return kit::formatted("#%02X%02X%02X", q(c.fR), q(c.fG), q(c.fB));
}

/** One line of type at a plate position — ranged left, centred, or right
 *  — set in whatever the caller states on it, a class or a partial, over
 *  the root voice. The line box is 1.7 em of that type, so it follows the
 *  size the line resolves to. */
inline Element label(const std::string& s, float x, float y, float w) {
  return at(x, y, w, 0).height(1.7_em).child(text(U(s)));
}
inline Element centred(const std::string& s, float x, float y, float w) {
  return at(x, y, w, 0)
      .height(1.7_em)
      .child(text(U(s))
                 .textAlign(weave::TextAlignment::kCenter)
                 .width(Dimension(w)));
}
inline Element rightAt(const std::string& s, float x, float y, float w) {
  return at(x, y, w, 0)
      .height(1.7_em)
      .child(
          text(U(s)).textAlign(weave::TextAlignment::kEnd).width(Dimension(w)));
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
inline shapes::OutlineFunction rimBaseline() {
  return shapes::circle(SkPathDirection::kCCW, 2);
}
/** A radius through the centre of the box, as a straight diameter. */
inline shapes::OutlineFunction diameter() {
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

}  // namespace chevreul_circle

// ===========================================================================
