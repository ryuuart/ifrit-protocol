#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;

namespace skia = sigil::material::skia;
namespace field = sigil::material::field;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace path = sigil::geometry::path;
namespace shapers = sigil::geometry::shapers;
namespace shapes = sigil::geometry::shapes;
namespace weaveNs = sigil::weave;

using namespace sigil::compose;
namespace motion = sigil::motion;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace sigillum_aemeth {}
using namespace sigillum_aemeth;
namespace sigillum_aemeth {

using sigil::compose::hexColor;  // 0xRRGGBB (+ optional alpha) → SkColor4f

// ---------------------------------------------------------------------------
// palette — beeswax, four centuries old, under museum light: one hue and
// many depths. Taken off the British Museum photographs of 1838,1232.90.a
// (the large disc) and .c (the small one); the cut is not a colour, it is a
// shadowed wall and a lit wall.

constexpr SkColor4f kVitrine = hexColor(0x14161c);
// THE DISC IS WAX, and the record says which wax: a dull olive-brown
// beeswax, matte, scuffed, green-stained where the graver went in. A warm
// parchment tan with a golden sheen is new vellum, and on new vellum a
// line is drawn; on wax it is CUT.
constexpr SkColor4f kWaxDeep = hexColor(0x5c4c26);
constexpr SkColor4f kWaxMid = hexColor(0x7d6a37);
constexpr SkColor4f kWaxLit = hexColor(0x9c8949);
constexpr SkColor4f kWaxPale = hexColor(0xb3a267);
constexpr SkColor4f kCutDark = hexColor(0x2b2210);  // the groove's floor
constexpr SkColor4f kCutLite =
    hexColor(0xc4b485);  // the wall that catches light
constexpr SkColor4f kInk = hexColor(0x2b2118);
constexpr SkColor4f kInkSoft = hexColor(0x6a5a42);
constexpr SkColor4f kRubric = hexColor(0x8c2f22);
constexpr SkColor4f kTrace = hexColor(0x1f6f9c);
constexpr SkColor4f kGold = hexColor(0xb8862c);
constexpr SkColor4f kVellum = hexColor(0xece1c8);

// ---------------------------------------------------------------------------
// canvas & the seal's frame

constexpr float kS = 0.8333f;  // declared-canvas scale
constexpr float kW = 2000, kH = 1417;
constexpr float kR = 618.0f;               // the greatest Circle, in px
constexpr float kWaxEdge = 1.058f;         // the rim of the wax cake
constexpr float kRR = 0.882f * kR;         // half the seal GROUP's box —
                                           // the angle plates (0.868 R) are the
                                           // largest thing drawn on it
constexpr float kWaxHalf = kWaxEdge * kR;  // the cake overflows it
constexpr float kCx = 50.0f + kWaxHalf;    // seal centre in canvas px
constexpr float kCy = 50.0f + kWaxHalf;
constexpr float kD = 3.14159265358979f / 180.0f;

// the measured ring table, in units of the greatest Circle
constexpr float rGreat = 1.000f;
constexpr float rGreatIn = 0.972f;
constexpr float rBandIn = 0.876f;
constexpr float rNumOut = 0.950f;
constexpr float rCellLet = 0.909f;
constexpr float rNumIn = 0.898f;
constexpr float rAngleHept = 0.836f;  // heptagon whose sides carry the 49
constexpr float rHept = 0.777f;       // THE heptagon — the ruled line
constexpr float rNameHept = 0.727f;   // heptagon whose sides carry the Names
constexpr float rFiliaeLucis = 0.541f;
constexpr float rFiliiLucis = 0.464f;
constexpr float rFiliaeFil = 0.393f;
constexpr float rFiliiFil = 0.324f;
constexpr float rInnerHept = 0.285f;
constexpr float rPenta = 0.215f;
constexpr float rPentaTail = 0.161f;
constexpr float rPentaInit = 0.176f;
constexpr float rCross = 0.071f;

// the concentric rules that cut the star's points into cells
constexpr float kCellRings[5] = {0.590f, 0.505f, 0.428f, 0.355f, rInnerHept};

// {7/2}: cos(2π/7)/cos(π/7)
const float kStar72 =
    std::cos(2 * 3.14159265358979f / 7) / std::cos(3.14159265358979f / 7);
const float kStar73 =
    std::cos(3 * 3.14159265358979f / 7) / std::cos(2 * 3.14159265358979f / 7);

// ---------------------------------------------------------------------------
// polar helpers. θ is measured CLOCKWISE FROM 12 O'CLOCK, which is how Dee
// gives every instruction on this figure ("the begynning of the greatest
// Circle … and so procede toward thy right hand").

inline SkPoint P(float thDeg, float rNorm) {
  // Twelve o'clock is where the ellipse's own angle starts a quarter turn
  // back, which is what makes this whole figure's clockwise-from-twelve
  // reading the ordinary ring arithmetic.
  return arrange::onEllipse({kRR, kRR}, {rNorm * kR, rNorm * kR},
                            thDeg * kD - 1.5707963f);
}
// θ → the arc-length fraction of shapes::circle(), whose contour starts at
// due EAST and runs clockwise (SkPathBuilder::addOval, startIndex 1, kCW).
inline float frac(float thDeg) {
  return std::fmod((thDeg - 90.0f) / 360.0f + 4.0f, 1.0f);
}
// θ → Skia's canvas angle (0° = +x, sweeping clockwise) for sector()/arc().
inline float skAngle(float thDeg) { return thDeg - 90.0f; }

// ---------------------------------------------------------------------------
// THE PLATE'S OWN CONTENT, recovered from the vector coordinates.

struct Cell {
  const char* glyph;  // as drawn
  int number;         // 0 = none
  int step;           // +right / −left / 0 = ends the Name
};

// letter, number, and which side of the letter the number sits on, decided
// by comparing each numeral's fitted radius to its letter's.
const std::array<Cell, 40> kRing = {{
    {"T", 4, +4},   {"G", 9, +9},   {"n", 7, +7},   {"t", 9, -9},
    {"h", 22, +22}, {"n", 0, 0},    {"m", 6, +6},   {"o", 22, +22},
    {"a", 20, +20}, {"n", 14, +14}, {"a", 6, +6},   {"h", 0, 0},
    {"o", 18, +18}, {"l", 26, +26}, {"l", 30, -30}, {"n", 0, 0},
    {"l", 8, -8},   {"G", 7, +7},   {"r", 13, +13}, {"H", 12, -12},
    {"og", 0, 0},   {"y", 15, -15}, {"t", 11, -11}, {"o", 8, -8},
    {"e", 21, -21}, {"b", 10, +10}, {"A", 11, +11}, {"I", 15, +15},
    {"a", 8, +8},   {"r", 16, -16}, {"n", 0, 0},    {"A", 6, +6},
    {"o", 10, -10}, {"G", 5, +5},   {"h", 14, -14}, {"o", 17, -17},
    {"s", 0, 0},    {"a", 5, -5},   {"a", 24, -24}, {"\xcf\x89", 6, +6},
}};

// the seven Names, in the order Michael insisted on after he reordered them
struct NameSpec {
  const char* name;
  int start;  // 1-based cell
};
const std::array<NameSpec, 7> kNames = {{{"Galas", 2},
                                         {"Gethog", 18},
                                         {"Thaoth", 1},
                                         {"Horl\xcf\x89n", 20},
                                         {"Innon", 28},
                                         {"Aaoth", 32},
                                         {"Galethog", 34}}};

// the seven angles: one row per bird, per basket. Read DOWN the columns and
// the seven archangels run on continuously — 48 letters and a cross.
inline const char* kAngles[7][7] = {
    {"Z", "l", "l", "R", "H", "i", "a"},
    {"a", "Z", "C", "a", "a", "c", "b"},
    {"p", "a", "u", "p", "n", "h", "r"},
    {"h", "d", "m", "h", "i", "a", "i"},
    {"k", "k", "a", "a", "e", "e", "e"},
    {"i", "i", "e", "e", "l", "l", "l"},
    {"e", "e", "l", "l", "M", "G", "\xe2\x80\xa0"}};
inline const char* kArchangels[7] = {"Zaphkie",  "l Zadkie", "l Cumael",
                                     " Raphael", " Haniel",  "M ichael",
                                     "G abriel"};

// the seven Names of God from the square Table of 7, as WRITTEN on the
// plate: ligature 21/8 = "el" is drawn as one compound letter, 30 = L.
struct GodName {
  const char* glyphs[7];  // "*" = the 21/8 ligature drawn as geometry
  const char* reading;
  const char* gloss;
};
const std::array<GodName, 7> kGodNames = {{
    {{"S", "A", "A", "*", "E", "M", "E"}, "SAAIEME", "Vivit in c\xc3\xa6lis"},
    {{"B", "T", "Z", "K", "A", "S", "E"}, "BTZKASE", "Deus noster"},
    {{"H", "E", "I", "D", "E", "N", "E"}, "HEIDENE", "Dux noster"},
    {{"D", "E", "I", "M", "O", "30", "A"}, "DEIMOLA", "Hic est"},
    {{"I", "M", "E", "G", "C", "B", "E"}, "IMEGCBE", "Lux in \xc3\xa6ternum"},
    {{"I", "L", "A", "O", "*", "V", "N"}, "ILAOIVN", "Finis est"},
    {{"I", "H", "R", "L", "A", "A", "*"},
     "IHRLAAL",
     "Vera est h\xc3\xa6\x63 tabula"},
}};
// the small numerals Dee writes over four of those letters
const int kGodNumRow[7] = {0, 1, 0, 3, 4, 5, 6};

// the four orders of the Children of Light, IN PLATE ORDER (see the header:
// the Filii Lucis are not in list order on the object).
inline const char* kFiliaeLucis[7] = {"El",    "Me",     "Ese",    "Iana",
                                      "Akele", "Azdobn", "Stimcul"};
inline const char* kFiliiLucis[7] = {"I",   "Heeoa",   "Ih",  "Beigia",
                                     "Ilr", "Stimcul", "Dmal"};
inline const char* kFiliaeFil[7] = {"S",     "Ab",     "Ath",    "Ized",
                                    "Ekiei", "Madimi", "Esemeli"};
inline const char* kFiliiFil[7] = {"*",     "An",      "Ave",    "Liba",
                                   "Rocle", "Hagonel", "Ilemese"};

inline const char* kZabathiel[7] = {"Z", "A", "B", "A", "T", "H", "I*"};

struct Planet {
  const char *initial, *tail, *gloss;
};
const std::array<Planet, 5> kPentaNames = {{{"Z", "edekieil", "Jupiter"},
                                            {"M", "adimiel", "Mars"},
                                            {"S", "emeliel", "Sol"},
                                            {"N", "ogahel", "Venus"},
                                            {"C", "orabiel", "Mercurius"}}};

// ---------------------------------------------------------------------------
// THE SOLVER. Michael's rule, walked. This is the only place the seven
// Names exist in this file: they are not a table, they are an output.

struct Solved {
  std::string raw, reduced;
  std::vector<int> cells;  // 1-based
};

inline std::string dedupeAA(const std::vector<std::string>& g) {
  // "Where soever thow shalt finde two a a togither the first is not to be
  // placed within the Name."
  std::string out;
  for (size_t i = 0; i < g.size(); ++i) {
    const bool aa = i + 1 < g.size() && (g[i] == "a" || g[i] == "A") &&
                    (g[i + 1] == "a" || g[i + 1] == "A");
    if (!aa) out += g[i];
  }
  return out;
}

inline Solved walkFrom(int start1) {
  Solved s;
  std::vector<std::string> glyphs;
  int i = start1 - 1;
  for (int guard = 0; guard < 64; ++guard) {
    s.cells.push_back(i + 1);
    glyphs.emplace_back(kRing[(size_t)i].glyph);
    s.raw += kRing[(size_t)i].glyph;
    if (kRing[(size_t)i].step == 0) break;
    i = ((i + kRing[(size_t)i].step) % 40 + 40) % 40;
  }
  s.reduced = dedupeAA(glyphs);
  return s;
}

// ---------------------------------------------------------------------------
// GEOMETRY. The heptagon, the {7/2} heptagram, and its 7 crossings.

inline SkPoint heptVertex(int k, float rNorm) {
  return P((float)k * 360.0f / 7.0f, rNorm);
}

/** The seven sides as SEVEN OPEN CONTOURS of one path, wound clockwise so
 *  that glyph-up comes out radially outward — the engraver's convention.
 *  TextPath walks every contour in order as ONE arc-length coordinate, so
 *  side k's midpoint is at exactly (k + 0.5)/7 of the whole. */
inline shapes::OutlineFn heptChords(float rNorm, float inset) {
  return [rNorm, inset](SkSize) {
    SkPathBuilder b;
    for (int k = 0; k < 7; ++k) {
      SkPoint a = heptVertex(k, rNorm), c = heptVertex(k + 1, rNorm);
      const SkVector d{c.fX - a.fX, c.fY - a.fY};
      const float len = std::hypot(d.fX, d.fY);
      const SkVector u{d.fX / len, d.fY / len};
      b.moveTo(a.fX + u.fX * inset, a.fY + u.fY * inset);
      b.lineTo(c.fX - u.fX * inset, c.fY - u.fY * inset);
    }
    return b.detach();
  };
}

/** THE {7/2} HEPTAGRAM AS SEVEN STRANDS, and the crossings DISCOVERED among
 *  them. A star polygon {n/k} self-intersects n(k-1) times, so seven
 *  crossings and fourteen passes — and none of them is authored here:
 *  `path::discoverCrossings` finds the PROPER crossings, which is what keeps
 *  the seven shared vertices out of the list.
 *
 *  THE ORDER IS ALTERNATION ALONG THE CURVE — `crossing::alternateAlong()`,
 *  not `crossing::alternate()`. An alternating knot alternates as you TRAVEL
 *  it: the heptagram is one closed curve, 0→2→4→6→1→3→5→0, and going over then
 *  under along that traversal is what makes the interlace read.
 *  `crossing::alternate()` alternates by the crossing's discovered ORDINAL,
 *  which is a different sequence — on this star it puts two consecutive
 *  UNDERs on the strand from vertex 3, and the band there passes behind both
 *  its neighbours and vanishes.
 *
 *  The rule is PREPARED once against the discovered set, because a rule about
 *  the walk cannot be answered from one crossing alone: nothing in a single
 *  Crossing says how many crossings on its own strand come before it. What is
 *  prepared travels with the value, so the copy the paint program holds
 *  decides without rediscovering anything.
 *
 *  `outOfAlternation` reads the prepared rule back the way an engraver checks
 *  a plate — walk each strand and see that the sides it takes run over,
 *  under, over, under. A figure that alternates has none, and the feed
 *  reports it. */
struct Weave {
  SkPoint v[7];  // traversal vertices, in visiting order
  std::vector<SkPath> strands;
  std::vector<path::Crossing> crossings;
  /** Who passes over whom: the alternating weave along every strand. */
  path::CrossingRule rule = path::crossing::alternateAlong();
  /** Half the arc distance to the nearest neighbouring crossing, per
   *  ordinal — the cap `crossingPatch` requires so that two lenses on one
   *  strand cannot merge into a single contour. */
  std::vector<float> reachCap;
  /** Passes taking the same side as the previous pass on their own
   *  strand. */
  int outOfAlternation = 0;
};

inline Weave buildWeave(float rNorm) {
  Weave w;
  for (int k = 0; k < 7; ++k) w.v[k] = heptVertex((2 * k) % 7, rNorm);
  for (int i = 0; i < 7; ++i) {
    SkPathBuilder b;
    b.moveTo(w.v[i]);
    b.lineTo(w.v[(i + 1) % 7]);
    w.strands.push_back(b.detach());
  }
  w.crossings = path::discoverCrossings(w.strands);
  w.rule.prepare(w.crossings);
  w.reachCap.assign(w.crossings.size(), 1e9f);

  // The fourteen passes: a crossing joins two strands and is met once on
  // each, so the walk that the weave is read along is the passes and not
  // the crossings.
  struct Pass {
    size_t strand;
    float along;
    size_t cross;
    bool isA;
  };
  std::vector<Pass> passes;
  for (const path::Crossing& c : w.crossings) {
    passes.push_back({c.a, c.alongA, c.index, true});
    passes.push_back({c.b, c.alongB, c.index, false});
  }
  std::sort(passes.begin(), passes.end(), [](const Pass& x, const Pass& y) {
    return x.strand != y.strand ? x.strand < y.strand : x.along < y.along;
  });
  // Read the prepared rule back along each strand: this strand goes over
  // here, under at the next knot, over at the one after.
  size_t walking = passes.empty() ? 0 : passes.front().strand;
  int previous = -1;
  for (const Pass& pass : passes) {
    if (pass.strand != walking) {
      walking = pass.strand;
      previous = -1;
    }
    const path::Crossing& c = w.crossings[pass.cross];
    const bool over = (w.rule.decide(c) == path::Order::Over) == pass.isA;
    if (previous >= 0 && (previous != 0) == over) ++w.outOfAlternation;
    previous = over ? 1 : 0;
  }
  // The cap: half the distance along a shared strand to the next crossing.
  for (size_t i = 0; i < passes.size(); ++i)
    for (size_t j = i + 1; j < passes.size(); ++j) {
      if (passes[i].strand != passes[j].strand) break;
      const float d = std::fabs(passes[j].along - passes[i].along);
      const SkPoint& a = w.v[passes[i].strand];
      const SkPoint& b = w.v[(passes[i].strand + 1) % 7];
      const float px = d * std::hypot(b.fX - a.fX, b.fY - a.fY) * 0.5f;
      w.reachCap[passes[i].cross] = std::min(w.reachCap[passes[i].cross], px);
      w.reachCap[passes[j].cross] = std::min(w.reachCap[passes[j].cross], px);
    }
  return w;
}

/** A compass in warm wax wanders. shapers::Jitter inside a Brush re-runs
 *  SkDiscretePathEffect over a 3900 px circle on EVERY PAINT, which during a
 *  trim reveal is every frame; baking the jitter into the OUTLINE instead
 *  runs it once at layout and leaves the reveal as pure geometry. */
inline shapes::OutlineFn wobbled(shapes::OutlineFn base, uint32_t seed,
                                 float seg = 26.0f, float dev = 0.34f) {
  return [base = std::move(base), seed, seg, dev](SkSize s) {
    return shapers::Jitter{seg, dev, seed}.shape(base(s));
  };
}

// ---------------------------------------------------------------------------
// paint helpers

// A positional shorthand over type. Every text run on this plate is
// built from the same four fields, and there are hundreds of call sites.
inline weaveNs::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                               float tracking = 0) {
  return weaveNs::textStyle(
      {.face = std::move(face), .size = size, .color = c, .track = tracking});
}

using motion::ramp;  // ramp(delayMs, durationMs) — a delayed eased reveal

/** THE ENGRAVED V-GROOVE. A cut in wax is a cross-section — a shadowed wall
 *  and a lit wall — which is the one thing a stroke's own paint cannot
 *  carry. It works here because every rule on this plate that matters is a
 *  CIRCLE: a radial ramp centred on that circle's own centre is constant
 *  ALONG the groove and varies ACROSS it. On any path that is not
 *  concentric with the gradient, this trick falls apart. */
inline Fill grooveFill(float rad, float w, float darkA, float liteA) {
  const float g = rad + w;
  const float a = (rad - w * 0.5f) / g, b = (rad + w * 0.5f) / g;
  const float m = (a + b) * 0.5f, e = (b - a) * 0.24f;
  return radialGradient(
      {rad, rad}, g,
      {SkColor4f{kCutDark.fR, kCutDark.fG, kCutDark.fB, darkA},
       SkColor4f{kCutDark.fR, kCutDark.fG, kCutDark.fB, darkA},
       SkColor4f{kCutLite.fR, kCutLite.fG, kCutLite.fB, liteA},
       SkColor4f{kCutLite.fR, kCutLite.fG, kCutLite.fB, liteA}},
      {0.0f, m - e, m + e, 1.0f});
}

}  // namespace sigillum_aemeth

// ===========================================================================
