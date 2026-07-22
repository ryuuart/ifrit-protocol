// sigillum_aemeth.cpp — SIGILLVM DEI ÆMÆTH, solved from its own construction
// rule and from the coordinates of the plate that records it.
//
// THE ARTEFACT. The Sigillum Dei Aemeth, dictated to John Dee and Edward
// Kelley by the angels Michael and Uriel and finished "at Mortlake by
// Richemond, Anno Domini 1582, Martii 21". Drawn at the end of Liber
// Mysteriorum Secundus in Dee's holograph, BRITISH LIBRARY, SLOANE MS 3188,
// f. 30r (Ashmole's fair copy is Sloane MS 3677). Dee had it cut in wax:
// three discs survive in the BRITISH MUSEUM, reg. 1838,1232.90.a–c; the
// large one is wax, engraved, 23.2 cm across and 3.4 cm deep. It was the
// stand for a shew-stone — a ball of quartz sat on the middle of this
// figure for its whole working life, and the burnish it left is drawn here.
//
// NOTHING IS TRACED, AND NOTHING IS EYEBALLED. Page 31 of
// http://www.john-dee.org/Secundus.pdf (John Dee Publication Project) is the
// seal set as VECTOR TEXT: 310 PDF text objects with real coordinates. The
// study begins by extracting them:
//
//     pdftotext -bbox -f 31 -l 31 Secundus.pdf p31.html
//
// then least-squares (Kåsa) circle-fitting the outer letter ring to recover
// the true centre — the page's bounding-box centre is offset from it by
// (−0.82, −2.47) pt — and binning every glyph by radius. Fit: centre
// (305.185, 393.529) pt, R = 257.972 pt over 44 ring glyphs. Fitting the
// 9° lattice to the ring gives phase +4.30° with an angular rms of 0.81°.
// The radial bins fall out clean, and they are the ring table this file is
// built from (r/R of that fit, outermost first):
//
//     0.988–1.062  the circumference: 40 cells, letters + their numbers
//     0.820–0.912  the seven ANGLES: 7 rows of 7 letters, 47 recovered
//     0.718–0.787  the seven NAMES OF GOD, one per heptagon side
//     0.576–0.614  Filiæ Lucis        0.495–0.527  Filii Lucis
//     0.419–0.445  Filiæ Filiarum     0.343–0.370  Filii Filiorum
//     0.258–0.296  ZABATHIEL, one letter per side of the innermost heptagon
//     0.163–0.194  the five planetary Angeli Lucis, on the pentagram
//     0.033–0.078  LE·VA·NA·el — Levanael on the arms of the centre cross
//
// THE CIPHER CLOSES. Dee's rule: a number written ABOVE a letter (further
// out) steps that many cells right; BELOW (further in), that many left; a
// letter with no number ends the Name. Comparing each numeral's fitted
// radius to its own letter's decides above from below WITHOUT a judgement
// call, and the solver in this file then walks the rule from all 40 starts.
// Six names fall straight out; the seventh, Aaoth, has two candidate starts
// (cells 27 and 32, both a capital A) and the CELL COUNT picks it:
//
//     Galas    2·11·17·9·29·37        raw "Galaas"  → Michael's a-a rule
//     Gethog   18·25·4·35·21
//     Thaoth   1·5·27·38·33·23·12     raw "ThAaoth" → Michael's a-a rule
//     Horlωn   20·8·30·14·40·6
//     Innon    28·3·10·24·16
//     Aaoth    32·38·33·23·12         ← 27 would give 32 cells, not 33
//     Galethog 34·39·15·25·4·35·21
//
// 33 of the 40 cells are consumed and 7 are never visited — the published
// count, reproduced here from the plate rather than taken on trust. And the
// seven leftovers are not random: cells 7·13·19·22·26·31·36 (m o r y b n o)
// are exactly the seeds and interior of the two names the same rule finds
// and Michael never gave — YMON (22·7·13·31) and BORAOTH (26·36·19·32·…).
// That is the check that the extraction is right, because the "reported"
// letter run in circulation has no r at all and cannot produce either.
//
// {7/2}, NOT {7/3} — DECIDED BY THE COORDINATES. The core heptagon of a
// {7/2} heptagram sits at 0.6920 of the outer circumradius (0.3569 for
// {7/3}). With the heptagon fitted at 0.777 R the {7/2} core reaches 0.485 R
// along a point's own ray, which puts the Filiæ Filiarum (0.393) and Filii
// Filiorum (0.324) INSIDE the core heptagon, where the record says they are
// written; {7/3}'s core stops at 0.250 R, which would leave both orders
// floating in mid-point and would also collide with ZABATHIEL's own
// heptagon at 0.235–0.269. Decisive, and printed in panel D.
//
// ONE MORE THING THE COORDINATES SAY THAT NO TRANSCRIPTION DOES. Three of
// the four orders of the Children of Light are written round the seven
// points in the order the record lists them. The FILII LUCIS are not: their
// angular positions run I · Heeoa · Ih · Beigia · Ilr · Stimcul · Dmal,
// which is the recorded list read with a step of +4 (mod 7) — and since
// their names are 1,2,3,4,5,6,7 letters long, the plate carries the length
// ladder 1,5,2,6,3,7,4 round the figure. This sketch draws what is on the
// plate.
//
// SEVEN-FOLD EVERYWHERE EXCEPT A FIVE-FOLD HEART, ON A FORTY-FOLD RIM, and
// gcd(40,7)=gcd(40,5)=gcd(7,5)=1, so the three systems agree in exactly one
// direction — 12 o'clock — and the ending of the loop is arriving there.
//
// BUILT FROM (the library, not by hand):
//   lines::Rails          the engraver's asymmetric rule — heavy outer +
//                         hairline inner, per-rail width AND dash. Exactly
//                         the thing lines::Line could not say; it landed
//                         this run and this plate is nine rules of it.
//   lines::Line           Cap::Arrow on the jump arcs (a jump is DIRECTED)
//                         and Cap::Dot on the margin's leader lines
//   lines::hatch/crosshatch/radialHatch   the wax ground, the 40 wedge
//                         cells, the seven angle plates, the 28 tablets,
//                         and the deep recess between star and heptagon
//   brushes::PatternBrush corner tiles — "at each corner of these segments
//                         of circles, to make little Crosses", 1582
//   brushes::ScatterBrush the compass pricks: forty divisions are STEPPED
//                         round with dividers, not measured, and the point
//                         leaves a mark at every step
//   brushes::Ribbon       the calligraphic nib — the heptagon is RULED with
//                         a quill, so its seven sides come out at seven
//                         weights from one nib angle
//   ops::Sketchy          every circle: a compass in a wax cake wanders
//   PathFormat trimStart/trimEnd   40 radial dividers that stop short of
//                         both circles — interrupted rules, not chords
//   TextPath::Orient::Radial    the 40 letters and their numerals, the 28
//                         Children of Light, the pentagram's five initials
//   TextPath::Orient::Tangent over a SEVEN-CONTOUR chord path — the 49
//                         angle letters, the 49 Name letters, the glosses
//                         and ZABATHIEL, each run addressed by (k+0.5)/7 of
//                         ONE arc-length coordinate spanning all contours
//   TextPath::Orient::Upright   LE·VA·NA·el on the arms of the centre cross
//   shapes::polygon/star/sector/circle/annulus
//   trim() + withFrom()   the jump-walk: one path per Name, one CONTOUR per
//                         hop, so 0→1 marches the walk hop by hop
//   bind()                ONE Output driving two counter-rotations
//   slot()/renderSlot()   the solver as an independent update domain
//   debug::coverage       the 40 cells tile the annulus; the 7 angle plates
//                         tile their band
//   console::LineRing     four panels of checks, printed as they run
//   Cache::Texture        the wax, the angle plates, the turning heptad
//
// A star polygon {n/k} self-intersects n(k−1) times, so this heptagram has
// SEVEN crossings and fourteen passes — not the fourteen crossings a {7/3}
// would have. Panel D counts them and confirms each got exactly one over and
// one under: the {7/2} heptagram is ONE closed curve, 0→2→4→6→1→3→5→0, so
// alternating along that single traversal IS a valid alternating weave.
//
// WHAT IT COST, AND WHAT PAID FOR IT. First measured frame: 110 ms at
// 2400×1700. It is now 5.3 ms p50 / 6.7 ms p99 at 2000×1417, with the settle
// — the only moment a cached layer moves — at 12.4 / 16.4. Nothing was
// removed to get there; six things were understood:
//   1. a mask blur is not a decoration. One sigma-34 shadow under the cake
//      and a four-layer blurred filament on 33 solver arcs were 50 ms.
//   2. a KEYFRAME PATH THAT IS STILL RUNNING IS LIVE VOLATILITY EVEN WHILE
//      ITS VALUE IS CONSTANT. Seven text-on-path runs held a mid-path
//      opacity from 1.5 s to 9.9 s and painted live for eight seconds —
//      29 ms of a 38 ms frame, and they kept their parents out of a texture.
//   3. "All 40 cells are drawn cold" (§5.1) is also the perf answer: 73
//      independently fading glyph nodes hold a whole band out of its cache.
//   4. the seal's groups were sized to the WAX CAKE (1.058 R) when the
//      largest thing drawn on them reaches 0.87 R. Sizing them to their
//      content took 28% off every layer composite.
//   5. rotation-invariant geometry has no business inside a rotating layer.
//      Lifting the circles and the radial hatch field out of the turning
//      groups was 11 ms.
//   6. the solver is an independent update domain — slot() + renderSlot(),
//      because a full render() to advance the walk cost a 365 ms frame.
// And one that did NOT pay: explicit Cache::Texture on the big groups was
// SLOWER than the library's own promotion (19.8 ms vs 11.5 ms) — it bakes
// one giant layer where the library bakes several small ones. The exception
// is a group under a LIVE rotation, where the library falls back to replaying
// the picture through a rotated matrix (45 ms) and an explicit bake wins.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/sigillum_aemeth.cpp \
//       --frame /tmp/sigillum_aemeth.png --at 2.5
//
//   1.0 s  the two circles struck, the 40 cells cut
//   2.5 s  the solver walking Galas / Gethog — arcs leaping the rim
//   9.0 s  all seven Names assembled; the seven unvisited cells go dark
//  12.0 s  the seven birds: the rows land, the columns light, the
//          archangels print out of them
//  20.0 s  the three systems counter-rotating into their one alignment
//  26 s loops.

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// palette — beeswax, four centuries old, under museum light: one hue and
// many depths. Taken off the British Museum photographs of 1838,1232.90.a
// (the large disc) and .c (the small one); the cut is not a colour, it is a
// shadowed wall and a lit wall.

constexpr SkColor4f kVitrine = hex(0x14161c);
constexpr SkColor4f kWaxDeep = hex(0x8d6d3a);
constexpr SkColor4f kWaxMid = hex(0xb59463);
constexpr SkColor4f kWaxLit = hex(0xdcc596);
constexpr SkColor4f kWaxPale = hex(0xe9dab2);
constexpr SkColor4f kCutDark = hex(0x3f2c12);
constexpr SkColor4f kCutLite = hex(0xfaeecb);
constexpr SkColor4f kInk = hex(0x2b2118);
constexpr SkColor4f kInkSoft = hex(0x6a5a42);
constexpr SkColor4f kRubric = hex(0x8c2f22);
constexpr SkColor4f kTrace = hex(0x1f6f9c);
constexpr SkColor4f kGold = hex(0xb8862c);
constexpr SkColor4f kVellum = hex(0xece1c8);

// ---------------------------------------------------------------------------
// canvas & the seal's frame

constexpr float kS = 0.8333f;              // declared-canvas scale
constexpr float kW = 2000, kH = 1417;
constexpr float kR = 618.0f;            // the greatest Circle, in px
constexpr float kWaxEdge = 1.058f;      // the rim of the wax cake
constexpr float kRR = 0.882f * kR;      // half the seal GROUP's box —
                                        // the angle plates (0.868 R) are the
                                        // largest thing drawn on it
constexpr float kWaxHalf = kWaxEdge * kR; // the cake overflows it
constexpr float kCx = 50.0f + kWaxHalf; // seal centre in canvas px
constexpr float kCy = 50.0f + kWaxHalf;
constexpr float kD = 3.14159265358979f / 180.0f;

// the measured ring table, in units of the greatest Circle
constexpr float rGreat = 1.000f;
constexpr float rGreatIn = 0.972f;
constexpr float rBandIn = 0.876f;
constexpr float rNumOut = 0.950f;
constexpr float rCellLet = 0.909f;
constexpr float rNumIn = 0.898f;
constexpr float rAngleHept = 0.836f; // heptagon whose sides carry the 49
constexpr float rHept = 0.777f;      // THE heptagon — the ruled line
constexpr float rNameHept = 0.727f;  // heptagon whose sides carry the Names
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
const float kStar72 = std::cos(2 * 3.14159265358979f / 7) /
                      std::cos(3.14159265358979f / 7);
const float kStar73 = std::cos(3 * 3.14159265358979f / 7) /
                      std::cos(2 * 3.14159265358979f / 7);

// ---------------------------------------------------------------------------
// polar helpers. θ is measured CLOCKWISE FROM 12 O'CLOCK, which is how Dee
// gives every instruction on this figure ("the begynning of the greatest
// Circle … and so procede toward thy right hand").

SkPoint P(float thDeg, float rNorm) {
  const float a = thDeg * kD;
  return {kRR + rNorm * kR * std::sin(a), kRR - rNorm * kR * std::cos(a)};
}
// θ → the arc-length fraction of shapes::circle(), whose contour starts at
// due EAST and runs clockwise (SkPathBuilder::addOval, startIndex 1, kCW).
float frac(float thDeg) {
  return std::fmod((thDeg - 90.0f) / 360.0f + 4.0f, 1.0f);
}
// θ → Skia's canvas angle (0° = +x, sweeping clockwise) for sector()/arc().
float skAngle(float thDeg) { return thDeg - 90.0f; }

// ---------------------------------------------------------------------------
// §2 — THE PLATE'S OWN CONTENT, recovered from the vector coordinates.

struct Cell {
  const char *glyph; // as drawn
  int number;        // 0 = none
  int step;          // +right / −left / 0 = ends the Name
};

// letter, number, and which side of the letter the number sits on, decided
// by comparing each numeral's fitted radius to its letter's.
const std::array<Cell, 40> kRing = {{
    {"T", 4, +4},    {"G", 9, +9},    {"n", 7, +7},    {"t", 9, -9},
    {"h", 22, +22},  {"n", 0, 0},     {"m", 6, +6},    {"o", 22, +22},
    {"a", 20, +20},  {"n", 14, +14},  {"a", 6, +6},    {"h", 0, 0},
    {"o", 18, +18},  {"l", 26, +26},  {"l", 30, -30},  {"n", 0, 0},
    {"l", 8, -8},    {"G", 7, +7},    {"r", 13, +13},  {"H", 12, -12},
    {"og", 0, 0},    {"y", 15, -15},  {"t", 11, -11},  {"o", 8, -8},
    {"e", 21, -21},  {"b", 10, +10},  {"A", 11, +11},  {"I", 15, +15},
    {"a", 8, +8},    {"r", 16, -16},  {"n", 0, 0},     {"A", 6, +6},
    {"o", 10, -10},  {"G", 5, +5},    {"h", 14, -14},  {"o", 17, -17},
    {"s", 0, 0},     {"a", 5, -5},    {"a", 24, -24},  {"\xcf\x89", 6, +6},
}};

// the seven Names, in the order Michael insisted on after he reordered them
struct NameSpec {
  const char *name;
  int start; // 1-based cell
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
const char *kAngles[7][7] = {
    {"Z", "l", "l", "R", "H", "i", "a"}, {"a", "Z", "C", "a", "a", "c", "b"},
    {"p", "a", "u", "p", "n", "h", "r"}, {"h", "d", "m", "h", "i", "a", "i"},
    {"k", "k", "a", "a", "e", "e", "e"}, {"i", "i", "e", "e", "l", "l", "l"},
    {"e", "e", "l", "l", "M", "G", "\xe2\x80\xa0"}};
const char *kArchangels[7] = {"Zaphkie", "l Zadkie", "l Cumael",
                              " Raphael", " Haniel", "M ichael",
                              "G abriel"};

// the seven Names of God from the square Table of 7, as WRITTEN on the
// plate: ligature 21/8 = "el" is drawn as one compound letter, 30 = L.
struct GodName {
  const char *glyphs[7]; // "*" = the 21/8 ligature drawn as geometry
  const char *reading;
  const char *gloss;
};
const std::array<GodName, 7> kGodNames = {{
    {{"S", "A", "A", "*", "E", "M", "E"}, "SAAIEME", "Vivit in c\xc3\xa6lis"},
    {{"B", "T", "Z", "K", "A", "S", "E"}, "BTZKASE", "Deus noster"},
    {{"H", "E", "I", "D", "E", "N", "E"}, "HEIDENE", "Dux noster"},
    {{"D", "E", "I", "M", "O", "30", "A"}, "DEIMOLA", "Hic est"},
    {{"I", "M", "E", "G", "C", "B", "E"}, "IMEGCBE", "Lux in \xc3\xa6ternum"},
    {{"I", "L", "A", "O", "*", "V", "N"}, "ILAOIVN", "Finis est"},
    {{"I", "H", "R", "L", "A", "A", "*"}, "IHRLAAL",
     "Vera est h\xc3\xa6\x63 tabula"},
}};
// the small numerals Dee writes over four of those letters
const int kGodNumRow[7] = {0, 1, 0, 3, 4, 5, 6};

// the four orders of the Children of Light, IN PLATE ORDER (see the header:
// the Filii Lucis are not in list order on the object).
const char *kFiliaeLucis[7] = {"El",    "Me",     "Ese",     "Iana",
                               "Akele", "Azdobn", "Stimcul"};
const char *kFiliiLucis[7] = {"I",   "Heeoa", "Ih",      "Beigia",
                              "Ilr", "Stimcul", "Dmal"};
const char *kFiliaeFil[7] = {"S",     "Ab",     "Ath",     "Ized",
                             "Ekiei", "Madimi", "Esemeli"};
const char *kFiliiFil[7] = {"*",     "An",      "Ave",     "Liba",
                            "Rocle", "Hagonel", "Ilemese"};

const char *kZabathiel[7] = {"Z", "A", "B", "A", "T", "H", "I*"};

struct Planet {
  const char *initial, *tail, *gloss;
};
const std::array<Planet, 5> kPentaNames = {{{"Z", "edekieil", "Jupiter"},
                                            {"M", "adimiel", "Mars"},
                                            {"S", "emeliel", "Sol"},
                                            {"N", "ogahel", "Venus"},
                                            {"C", "orabiel", "Mercurius"}}};

// ---------------------------------------------------------------------------
// §3 — THE SOLVER. Michael's rule, walked. This is the only place the seven
// Names exist in this file: they are not a table, they are an output.

struct Solved {
  std::string raw, reduced;
  std::vector<int> cells; // 1-based
};

std::string dedupeAA(const std::vector<std::string> &g) {
  // "Where soever thow shalt finde two a a togither the first is not to be
  // placed within the Name."
  std::string out;
  for (size_t i = 0; i < g.size(); ++i) {
    const bool aa = i + 1 < g.size() && (g[i] == "a" || g[i] == "A") &&
                    (g[i + 1] == "a" || g[i + 1] == "A");
    if (!aa)
      out += g[i];
  }
  return out;
}

Solved walkFrom(int start1) {
  Solved s;
  std::vector<std::string> glyphs;
  int i = start1 - 1;
  for (int guard = 0; guard < 64; ++guard) {
    s.cells.push_back(i + 1);
    glyphs.emplace_back(kRing[(size_t)i].glyph);
    s.raw += kRing[(size_t)i].glyph;
    if (kRing[(size_t)i].step == 0)
      break;
    i = ((i + kRing[(size_t)i].step) % 40 + 40) % 40;
  }
  s.reduced = dedupeAA(glyphs);
  return s;
}

// ---------------------------------------------------------------------------
// §4 — GEOMETRY. The heptagon, the {7/2} heptagram, and its 14 crossings.

SkPoint heptVertex(int k, float rNorm) { return P((float)k * 360.0f / 7.0f, rNorm); }

/** The seven sides as SEVEN OPEN CONTOURS of one path, wound clockwise so
 *  that glyph-up comes out radially outward — the engraver's convention.
 *  TextPath walks every contour in order as ONE arc-length coordinate, so
 *  side k's midpoint is at exactly (k + 0.5)/7 of the whole. */
shapes::OutlineFn heptChords(float rNorm, float inset) {
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

/** The {7/2} heptagram is ONE closed curve: 0→2→4→6→1→3→5→0. Fourteen
 *  crossings, twenty-eight passes; alternating along the traversal is a
 *  proper over-under weave, and the console checks that every crossing
 *  really did get one of each. */
struct Weave {
  SkPoint v[7];            // traversal vertices, in visiting order
  struct Cross {
    SkPoint at;
    int segA, segB;        // the two segments
    float tA, tB;
    bool aOver;
  };
  std::vector<Cross> crossings;
  int inconsistent = 0;
};

Weave buildWeave(float rNorm) {
  Weave w;
  for (int k = 0; k < 7; ++k)
    w.v[k] = heptVertex((2 * k) % 7, rNorm);
  // every unordered pair of non-adjacent segments meets exactly once
  struct Pass { int seg; float t; size_t cross; };
  std::vector<Pass> passes;
  for (int i = 0; i < 7; ++i)
    for (int j = i + 1; j < 7; ++j) {
      const SkPoint a = w.v[i], b = w.v[(i + 1) % 7];
      const SkPoint c = w.v[j], d = w.v[(j + 1) % 7];
      const float rx = b.fX - a.fX, ry = b.fY - a.fY;
      const float sx = d.fX - c.fX, sy = d.fY - c.fY;
      const float den = rx * sy - ry * sx;
      if (std::fabs(den) < 1e-4f)
        continue;
      const float t = ((c.fX - a.fX) * sy - (c.fY - a.fY) * sx) / den;
      const float u = ((c.fX - a.fX) * ry - (c.fY - a.fY) * rx) / den;
      if (t <= 1e-3f || t >= 1 - 1e-3f || u <= 1e-3f || u >= 1 - 1e-3f)
        continue;
      const size_t idx = w.crossings.size();
      w.crossings.push_back({{a.fX + rx * t, a.fY + ry * t}, i, j, t, u, false});
      passes.push_back({i, t, idx});
      passes.push_back({j, u, idx});
    }
  std::sort(passes.begin(), passes.end(), [](const Pass &x, const Pass &y) {
    return x.seg != y.seg ? x.seg < y.seg : x.t < y.t;
  });
  // alternate over/under along the single traversal
  std::vector<int> seen(w.crossings.size(), -1);
  for (size_t k = 0; k < passes.size(); ++k) {
    const bool over = (k % 2) == 0;
    Weave::Cross &c = w.crossings[passes[k].cross];
    if (seen[passes[k].cross] < 0) {
      seen[passes[k].cross] = over ? 1 : 0;
      c.aOver = (passes[k].seg == c.segA) ? over : !over;
    } else if ((seen[passes[k].cross] == 1) == over) {
      ++w.inconsistent; // both passes claim the same side
    }
  }
  return w;
}

/** A compass in warm wax wanders. ops::Sketchy inside a Brush re-runs
 *  SkDiscretePathEffect over a 3900 px circle on EVERY PAINT, which during a
 *  trim reveal is every frame; baking the jitter into the OUTLINE instead
 *  runs it once at layout and leaves the reveal as pure geometry. */
shapes::OutlineFn wobbled(shapes::OutlineFn base, uint32_t seed,
                          float seg = 26.0f, float dev = 0.34f) {
  return [base = std::move(base), seed, seg, dev](SkSize s) {
    return ops::sketchy(seg, dev, seed)(base(s));
  };
}

// ---------------------------------------------------------------------------
// paint helpers

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                            float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(c, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

Transition ramp(float delayMs, float durMs, ch::EaseFn e = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(e);
  return t;
}

/** THE ENGRAVED V-GROOVE, borrowed wholesale from chaucer_astrolabe. A cut
 *  in wax is a cross-section — a shadowed wall and a lit wall — which is
 *  the one paint a stroke cannot carry. It can here for the same reason it
 *  could there: every rule on this plate that matters is a CIRCLE, so a
 *  radial ramp centred on that circle's own centre is constant ALONG the
 *  groove and varies ACROSS it. Third study; still a trick. */
Fill grooveFill(float rad, float w, float darkA, float liteA) {
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

template <typename... A> std::string fmt(const char *f, A... args) {
  char buf[512];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::snprintf(buf, sizeof(buf), f, args...);
#pragma clang diagnostic pop
  return buf;
}

} // namespace

// ===========================================================================

struct SigillumAemeth : sigil::compose::sketch::Sketch {
  sk_sp<SkTypeface> faceSeal, faceRing, faceQuill, faceSerif, faceItalic,
      faceMono, faceDisplay;

  // ONE Output turns three systems that cannot agree except at 12 o'clock
  ch::Output<float> settle{0.0f};
  ch::Output<float> tracer{0.0f};
  ch::Output<float> pulseT{0.0f};

  console::LineRing logA{64}, logB{64}, logC{64}, logD{64};
  Weave weave;
  std::array<Solved, 7> solved;
  std::array<bool, 40> visited{};
  int usedCells = 0;
  double clockT = 0;
  int solvePhase = -2;

  Material waxGrain;
  Pattern waxSpeck;

  // --- the reading order, in seconds ---------------------------------------
  static constexpr float tPlate = 0.05f;
  static constexpr float tCells = 0.35f;
  static constexpr float tInner = 1.05f;
  static constexpr float tSolve = 2.00f;
  static constexpr float tSolveEach = 0.92f;
  static constexpr float tDark = tSolve + 7 * tSolveEach + 0.35f; // ~9.6
  static constexpr float tBirds = 9.6f;
  static constexpr float tSpin = 15.4f;

  // =========================================================================
  // THE WAX

  Element waxGround() {
    auto g = box().absolute().inset(0);

    // the cake: rim, body, and the tool-marks of a warm knife
    g.child(disc({kRR, kRR}, kWaxEdge * 1.055f * kR)
                .outline(shapes::annulus(0.90f))
                .fill(Material::radialUnit(
                    {0.5f, 0.5f}, 1.0f,
                    {{0.0f, hex(0x05070a, 0.0f)},
                     {0.905f, hex(0x05070a, 0.0f)},
                     {0.945f, hex(0x05070a, 0.62f)},
                     {1.0f, hex(0x05070a, 0.0f)}}))
                .translateX(5)
                .translateY(11)
                .key("waxshadow"));
    g.child(disc({kRR, kRR}, kWaxEdge * kR)
                .outline(shapes::circle())
                .fill(Material::blend(
                    {{Material::radialUnit({0.42f, 0.36f}, 1.05f,
                                           {{0.0f, kWaxPale},
                                            {0.45f, kWaxLit},
                                            {0.82f, kWaxMid},
                                            {1.0f, kWaxDeep}}),
                      SkBlendMode::kSrcOver},
                     {waxGrain, SkBlendMode::kOverlay},
                     {waxSpeck.material(), SkBlendMode::kMultiply}}))
                .foreground(lines::hatch(Fill::color(hex(0x6d5228, 0.10f)),
                                         11.0f, 0.9f, -24.0f))
                .foreground(PathFormat{.width = 9.0f,
                                       .strokeFill = grooveFill(
                                           kWaxEdge * kR, 9.0f, 0.55f, 0.42f),
                                       .align = PathFormat::Align::Inner})
                .cache(Cache::Texture)
                .key("wax"));

    // the burnish left by the shew-stone. A ball of quartz stood on the
    // middle of this figure for its whole working life.
    g.child(disc({kRR, kRR}, 0.33f * kR)
                .outline(shapes::circle())
                .fill(Material::radialUnit(
                    {0.42f, 0.38f}, 1.0f,
                    {{0.0f, hex(0xfff6dd, 0.34f)},
                     {0.55f, hex(0xffeec6, 0.14f)},
                     {1.0f, hex(0x000000, 0.0f)}}))
                .blend(SkBlendMode::kScreen)
                .key("shew"));
    g.child(disc({kRR, kRR}, 0.335f * kR)
                .outline(shapes::circle())
                .fill(Fill::none())
                .stroke(PathFormat{.width = 2.0f,
                                   .strokeFill = Fill::color(hex(0x7d5f2c, 0.20f))})
                .key("shewring"));
    return g;
  }

  // =========================================================================
  // THE CIRCUMFERENCE — 40 cells of 9°, division 1 at 12 o'clock, running
  // clockwise "toward the right hand".

  /** The rules and the radial hatch field: invariant under rotation about
   *  their own centre, so they stay OUT of the turning layer. Keeping them
   *  in cost 11 ms a frame during the settle, for no visible difference. */
  Element circumferenceRules() {
    auto g = box().absolute().inset(0);

    // the greatest Circle and its hairline companion — heavy OUTSIDE,
    // hair INSIDE. lines::Line cannot say this; lines::Rails can.
    auto rule = [&](float rNorm, float heavy, float hair, float gap,
                    const char *key, bool dotted) {
      Element e = disc({kRR, kRR}, rNorm * kR)
                      .outline(wobbled(shapes::circle(), 1582))
                      .fill(Fill::none())
                      .key(key);
      std::vector<lines::Rail> set;
      set.push_back({.offset = 0.0f,
                     .width = heavy,
                     .fill = grooveFill(rNorm * kR, heavy, 0.95f, 0.55f)});
      lines::Rail inner{.offset = gap,
                        .width = hair,
                        .fill = Fill::color(hex(0x4a3418, 0.72f))};
      if (dotted)
        inner.dash = {1.2f, 5.0f};
      set.push_back(inner);
      lines::Rails rails = lines::rails(std::move(set));
      rails.offsetStep = 7.0f;
      e.stroke(std::move(rails));
      e.trim(0.0f, withFrom(0.0f, 1.0f, ramp(tPlate * 1000, 900)));
      return e;
    };
    g.child(disc({kRR, kRR}, rGreat * kR)
                .outline(shapes::annulus(rBandIn / rGreat))
                .fill(Fill::none())
                .foreground(lines::RadialHatch{
                    .strokeFill = Fill::color(hex(0x6d5228, 0.11f)),
                    .spokes = 320,
                    .rings = 0,
                    .width = 0.8f,
                    .holeFraction = rBandIn / rGreat})
                .opacity(withFrom(0.0f, 1.0f, ramp(tCells * 1000, 700)))
                .key("bandhatch"));
    g.child(rule(rGreat, 5.6f, 1.2f, 11.0f, "great", false));
    g.child(rule(rBandIn, 3.4f, 0.9f, -8.0f, "second", true));

    // The compass pricks. Forty divisions are not measured, they are
    // STEPPED round with dividers, and the point leaves a mark at every
    // step. The inner circle's contour starts due east, which is a cell
    // CENTRE, so an Interval scatter with no phase lands its first stamp
    // half a cell along — exactly on the boundary — and every one after.
    const float step = 2.0f * SK_FloatPI * rBandIn * kR / 40.0f;
    g.child(disc({kRR, kRR}, rBandIn * kR)
                .outline(shapes::circle())
                .fill(Fill::none())
                .stroke(brushes::ScatterBrush{
                    .art = box()
                               .width(5)
                               .height(5)
                               .outline(shapes::polygon(4))
                               .fill(Fill::color(hex(0x2c1c06, 0.85f))),
                    .spacing = step,
                    .alignToPath = true,
                    .reach = 8.0f})
                .key("pricks"));
    return g;
  }

  /** What the 40 divisions actually are: the cells, and only the cells. */
  Element circumferenceCells() {
    auto g = box().absolute().inset(0).transformOrigin(0.5f, 0.5f);

    // the 40 radial dividers: INTERRUPTED rules that stop short of both
    // circles. One node, forty contours, one trim window on the stroke.
    g.child(box()
                .absolute()
                .inset(0)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  for (int i = 0; i < 40; ++i) {
                    const float th = (float)i * 9.0f - 4.5f;
                    b.moveTo(P(th, rBandIn));
                    b.lineTo(P(th, rGreat));
                  }
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(PathFormat{.width = 1.9f,
                                   .strokeFill = Fill::color(hex(0x2c1c06, 1.0f)),
                                   .cap = SkPaint::kRound_Cap,
                                   .trimStart = 0.09f,
                                   .trimEnd = 0.91f})
                .opacity(withFrom(0.0f, 1.0f, ramp(tCells * 1000, 620)))
                .key("dividers"));

    // the letters, upright-radial, and their numbers above or below
    const sigil::weave::TextStyle letStyle =
        type(faceRing, 0.060f * kR, hex(0x241603, 1.0f));
    const sigil::weave::TextStyle numStyle =
        type(faceRing, 0.031f * kR, hex(0x4a3210, 1.0f));
    for (int i = 0; i < 40; ++i) {
      const Cell &c = kRing[(size_t)i];
      const float th = (float)i * 9.0f;
      const float f = frac(th);
      const bool dim = !visited[(size_t)i];
      auto cellLetter =
          text(toU8(c.glyph), letStyle)
              .absolute()
              .width(Dim(2 * rCellLet * kR))
              .height(Dim(2 * rCellLet * kR))
              .centerAt({kRR, kRR})
              .key("cl" + std::to_string(i))
              .onPath(TextPath{.path = shapes::circle(),
                               .at = f,
                               .align = TextPath::Align::Center,
                               .offset = 0.0f,
                               .autoFlip = false,
                               .orient = TextPath::Orient::Radial})
              ;
      if (dim)
        cellLetter.opacity(with(0.30f, ramp(tDark * 1000 + (float)i * 9, 700)));
      g.child(std::move(cellLetter));
      if (c.number > 0) {
        const float rr = c.step > 0 ? rNumOut : rNumIn;
        g.child(text(toU8(std::to_string(c.number)), numStyle)
                    .absolute()
                    .width(Dim(2 * rr * kR))
                    .height(Dim(2 * rr * kR))
                    .centerAt({kRR, kRR})
                    .key("cn" + std::to_string(i))
                    .onPath(TextPath{.path = shapes::circle(),
                                     .at = f,
                                     .align = TextPath::Align::Center,
                                     .offset = 0.0f,
                                     .autoFlip = false,
                                     .orient = TextPath::Orient::Radial})
                    );
      }
    }
    return g;
  }

  // =========================================================================
  // THE SEVEN ANGLES — 7 rows of 7 letters on the sides of a heptagon, with
  // a little cross at every corner of the segments they sit in.

  Element angles() {
    auto g = box().absolute().inset(0).transformOrigin(0.5f, 0.5f);

    // the seven "segments of circles" — annular plates, radially hatched,
    // with brushes::PatternBrush corner tiles: "at each corner of these
    // segments of circles, to make little Crosses."
    Element crossTile = box()
                            .width(13)
                            .height(13)
                            .outline([](SkSize s) {
                              SkPathBuilder b;
                              const float w = s.width(), h = s.height();
                              const float t = w * 0.20f;
                              b.moveTo(w * 0.5f - t, 0);
                              b.lineTo(w * 0.5f + t, 0);
                              b.lineTo(w * 0.5f + t, h * 0.5f - t);
                              b.lineTo(w, h * 0.5f - t);
                              b.lineTo(w, h * 0.5f + t);
                              b.lineTo(w * 0.5f + t, h * 0.5f + t);
                              b.lineTo(w * 0.5f + t, h);
                              b.lineTo(w * 0.5f - t, h);
                              b.lineTo(w * 0.5f - t, h * 0.5f + t);
                              b.lineTo(0, h * 0.5f + t);
                              b.lineTo(0, h * 0.5f - t);
                              b.lineTo(w * 0.5f - t, h * 0.5f - t);
                              b.close();
                              return b.detach();
                            })
                            .fill(Fill::color(hex(0x402c10, 0.92f)));
    Element sideTile = box().width(20).height(3).fill(Fill::none());

    auto plates = box().absolute().inset(0).cache(Cache::Texture);
    for (int k = 0; k < 7; ++k) {
      const float mid = ((float)k + 0.5f) * 360.0f / 7.0f;
      const float half = 360.0f / 7.0f * 0.5f - 1.1f;
      plates.child(disc({kRR, kRR}, 0.868f * kR)
                  .outline(shapes::sector(skAngle(mid - half), 2 * half,
                                          0.720f / 0.868f))
                  .fill(Fill::color(hex(0xd9bd88, 0.30f)))
                  .foreground(lines::RadialHatch{
                      .strokeFill = Fill::color(hex(0x6d5228, 0.13f)),
                      .spokes = 96,
                      .rings = 0,
                      .width = 0.9f,
                      .holeFraction = 0.70f})
                  .stroke(Brush{}
                              .leg(PathFormat{
                                  .width = 1.5f,
                                  .strokeFill = Fill::color(hex(0x4a3418, 0.55f))})
                              .leg(brushes::PatternBrush{
                                  .side = sideTile,
                                  .corner = crossTile,
                                  .advance = 22.0f,
                                  .cornerAngleDeg = 40.0f,
                                  .reach = 16.0f}))
                  .key("plate" + std::to_string(k)));
    }
    g.child(std::move(plates));

    // the 49 letters: ONE text run per row on a SEVEN-CONTOUR chord path,
    // addressed by (k + 0.5)/7 of one continuous arc-length coordinate.
    const sigil::weave::TextStyle angStyle =
        type(faceSeal, 0.049f * kR, hex(0x201404, 1.0f), 0.034f * kR);
    for (int k = 0; k < 7; ++k) {
      std::string row;
      for (int c = 0; c < 7; ++c)
        row += kAngles[k][c];
      // NOTE (perf, measured): this run used to hold a mid-path opacity via
      // withKeyframes from 1.55 s to 9.9 s. A keyframe path that is still
      // RUNNING is live volatility even while its value is constant, so all
      // seven of these text-on-path nodes painted live for eight seconds —
      // 29 ms of a 38 ms frame, and they blocked their parents from being
      // promoted to textures. The entrance is now a plain ramp, and the
      // birds' arrival is marked by a separate cheap rule on each plate.
      g.child(text(toU8(row), angStyle)
                  .absolute()
                  .inset(0)
                  .key("ang" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rAngleHept, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  );
      // the bird lands: its angle-plate takes a rule it did not have before
      const float mid2 = ((float)k + 0.5f) * 360.0f / 7.0f;
      const float half2 = 360.0f / 7.0f * 0.5f - 1.1f;
      g.child(disc({kRR, kRR}, 0.868f * kR)
                  .outline(shapes::sector(skAngle(mid2 - half2), 2 * half2,
                                          0.720f / 0.868f))
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = 2.2f,
                      .strokeFill = Fill::color(hex(0x402c10, 0.85f)),
                      .align = PathFormat::Align::Inner})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tBirds * 1000 + (float)k * 260, 420)))
                  .key("birdlit" + std::to_string(k)));
    }
    return g;
  }

  // =========================================================================
  // THE HEPTAGON and the seven Names of God, written along its sides with a
  // quill — the seven sides come out at seven weights from one nib.

  Element heptagonNames() {
    auto g = box().absolute().inset(0).transformOrigin(0.5f, 0.5f);

    g.child(box()
                .absolute()
                .inset(0)
                .outline(wobbled(heptChords(rHept, 0.0f), 30, 30.0f, 0.45f))
                .fill(Fill::none())
                .stroke(Brush{}
                            .leg(brushes::calligraphic(
                                34.0f, 6.8f, Fill::color(hex(0x291a05, 0.95f)),
                                0.22f))
                            .leg(PathFormat{
                                .width = 0.9f,
                                .strokeFill = Fill::color(hex(0xf7e9c4, 0.35f)),
                                .trimStart = 0.0f,
                                .trimEnd = 1.0f}))
                .key("heptrule"));

    // the second, inner heptagon rule — the Names sit between the two
    g.child(box()
                .absolute()
                .inset(0)
                .outline(heptChords(rNameHept - 0.043f, 0.0f))
                .fill(Fill::none())
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 2.2f,
                                       .fill = Fill::color(hex(0x4a3418, 0.72f))},
                                      {.offset = 5.0f,
                                       .width = 0.8f,
                                       .fill = Fill::color(hex(0x4a3418, 0.45f)),
                                       .dash = {1.4f, 4.6f}}}))
                .key("heptrule2"));

    const sigil::weave::TextStyle nameStyle =
        type(faceQuill, 0.048f * kR, hex(0x201404, 1.0f), 0.026f * kR);
    for (int k = 0; k < 7; ++k) {
      std::string row;
      for (int c = 0; c < 7; ++c) {
        const char *gl = kGodNames[(size_t)k].glyphs[c];
        row += (gl[0] == '*') ? "\xc9\x9b" : gl; // the 21/8 ligature stands in
      }
      g.child(text(toU8(row), nameStyle)
                  .absolute()
                  .inset(0)
                  .key("god" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rNameHept, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  );
      // the Latin marginal reading, inside the heptagon, smaller
      g.child(text(toU8(kGodNames[(size_t)k].gloss),
                   type(faceItalic, 0.022f * kR, hex(0x53380f, 0.88f)))
                  .absolute()
                  .inset(0)
                  .key("gloss" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rNameHept - 0.056f, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  );
    }
    return g;
  }

  // =========================================================================
  // THE HEPTAGRAM — an interlaced band, fourteen crossings, drawn as one
  // custom leaf because the library has no over/under primitive.

  Element heptagram() {
    Weave w = weave;
    PaintProgram paint = [w](SkCanvas &c, const PaintContext &ctx) {
      const float bandW = 0.038f * kR;
      auto seg = [&](int i) {
        SkPathBuilder b;
        b.moveTo(w.v[i]);
        b.lineTo(w.v[(i + 1) % 7]);
        return b.detach();
      };
      auto limb = [&](SkCanvas &cv, int i, bool shadowed) {
        const SkPath p = seg(i);
        if (shadowed) {
          // The strap passing UNDER must be occluded, not merely interrupted:
          // the over-strap casts a soft shadow onto it, offset across its own
          // width. Without this the weave is in the data and not in the eye.
          SkPaint cast;
          cast.setAntiAlias(true);
          cast.setStyle(SkPaint::kStroke_Style);
          cast.setStrokeWidth(bandW + 7.0f);
          cast.setStrokeCap(SkPaint::kButt_Cap);
          cast.setColor4f(hex(0x150d02, 0.30f), nullptr);
          cv.save();
          cv.translate(4.0f, 5.5f);
          cv.drawPath(p, cast);
          cast.setStrokeWidth(bandW + 5.0f);
          cast.setColor4f(hex(0x150d02, 0.42f), nullptr);
          cv.translate(-1.4f, -1.9f);
          cv.drawPath(p, cast);
          cast.setStrokeWidth(bandW + 2.0f);
          cast.setColor4f(hex(0x150d02, 0.55f), nullptr);
          cv.translate(-1.2f, -1.7f);
          cv.drawPath(p, cast);
          cv.restore();
        }
        SkPaint body;
        body.setAntiAlias(true);
        body.setStyle(SkPaint::kStroke_Style);
        body.setStrokeWidth(bandW);
        body.setStrokeCap(SkPaint::kButt_Cap);
        body.setColor4f(hex(0xf0dcae, 1.0f), nullptr);
        cv.drawPath(p, body);
        decorations::paintOn(
            cv, ctx, p,
            lines::rails({{.offset = -bandW * 0.5f,
                           .width = 2.6f,
                           .fill = Fill::color(hex(0x241603, 1.0f))},
                          {.offset = -bandW * 0.5f + 3.0f,
                           .width = 0.8f,
                           .fill = Fill::color(hex(0xfbf0d0, 0.55f))},
                          {.offset = bandW * 0.5f,
                           .width = 2.6f,
                           .fill = Fill::color(hex(0x241603, 1.0f))},
                          {.offset = bandW * 0.5f - 3.0f,
                           .width = 0.8f,
                           .fill = Fill::color(hex(0x8a6c3c, 0.45f))}}));
      };
      for (int i = 0; i < 7; ++i)
        limb(c, i, false);
      // the weave: at each crossing, redraw whichever limb passes OVER,
      // clipped to a disc the width of the band.
      for (const Weave::Cross &x : w.crossings) {
        const int over = x.aOver ? x.segA : x.segB;
        SkPathBuilder clipB;
        clipB.addCircle(x.at.fX, x.at.fY, bandW * 1.32f);
        c.save();
        c.clipPath(clipB.detach(), true);
        limb(c, over, true);
        c.restore();
      }
    };
    return box()
        .absolute()
        .inset(0)
        .background(paint)
        .key("weave");
  }

  // =========================================================================
  // the concentric cell rules, the four orders of the Children of Light,
  // ZABATHIEL's heptagon, the pentagram, the cross.

  /** Concentric circles and a crosshatched annular recess: also invariant
   *  under rotation, also lifted out of the turning layer. */
  Element innerRings() {
    auto g = box().absolute().inset(0);

    // the deepest recesses — crosshatched wax between the star's limbs
    g.child(box()
                .absolute()
                .inset(0)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.setFillType(SkPathFillType::kEvenOdd);
                  for (int k = 0; k < 7; ++k) {
                    const SkPoint v = heptVertex(k, rHept);
                    k == 0 ? b.moveTo(v) : b.lineTo(v);
                  }
                  b.close();
                  for (int k = 0; k < 14; ++k) {
                    const float rr = (k % 2 == 0) ? rHept : rHept * kStar72;
                    const SkPoint v = P((float)k * 360.0f / 14.0f, rr);
                    k == 0 ? b.moveTo(v) : b.lineTo(v);
                  }
                  b.close();
                  return b.detach();
                })
                .fill(Fill::color(hex(0x7d5f2c, 0.10f)))
                .foreground(lines::crosshatch(Fill::color(hex(0x5a4218, 0.16f)),
                                              8.0f, 0.8f, 22.0f))
                .key("recess"));

    // the concentric rules that cut the points into cells
    for (int i = 0; i < 5; ++i) {
      const float rr = kCellRings[i];
      g.child(disc({kRR, kRR}, rr * kR)
                  .outline(wobbled(shapes::circle(), (uint32_t)(7 + i), 22.0f,
                                   0.30f))
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = i == 4 ? 2.4f : 1.5f,
                      .strokeFill = grooveFill(rr * kR, i == 4 ? 2.4f : 1.5f,
                                               0.50f, 0.34f)})
                  .trim(0.0f, withFrom(0.0f, 1.0f,
                                       ramp(tInner * 1000 + 200 + (float)i * 90,
                                            620)))
                  .key("ring" + std::to_string(i)));
    }
    return g;
  }

  Element inner() {
    auto g = box().absolute().inset(0);

    // The four orders, each with the tablet the record gives it: an
    // arc-segment worn in the forehead, a round gold plate on the breast,
    // a four-square white ivory, a three-cornered green. Four silhouettes,
    // cut in the same wax as everything else — not four tinted rectangles.
    struct Order {
      const char *const *names;
      float radius;
      int shape; // 0 arc-segment, 1 disc, 2 four-square, 3 three-cornered
      SkColor4f rule;
      float size;
    };
    const Order orders[4] = {
        {kFiliaeLucis, rFiliaeLucis, 0, hex(0x2c1c06, 0.95f), 0.027f},
        {kFiliiLucis, rFiliiLucis, 1, hex(0x2c1c06, 0.95f), 0.025f},
        {kFiliaeFil, rFiliaeFil, 2, hex(0x2c1c06, 0.95f), 0.024f},
        {kFiliiFil, rFiliiFil, 3, hex(0x2c1c06, 0.95f), 0.023f}};
    const SkColor4f kTabletFace[4] = {hex(0xe4cd9e, 0.62f), hex(0xecd7a8, 0.66f),
                                      hex(0xe8d2a2, 0.60f), hex(0xdfc793, 0.58f)};

    for (int o = 0; o < 4; ++o) {
      const Order &ord = orders[o];
      for (int k = 0; k < 7; ++k) {
        const float th = (float)k * 360.0f / 7.0f;
        const float em = 0.034f * kR;                 // the tablet itself
        const float rTab = ord.radius - 0.032f;       // it sits below the name
        const SkPoint at = P(th, rTab);
        Element tablet =
            box()
                .absolute()
                .width(Dim(em))
                .height(Dim(em))
                .centerAt(at)
                .fill(Fill::color(kTabletFace[o]))
                .foreground(lines::hatch(Fill::color(hex(0x4a3418, 0.30f)),
                                         3.6f, 0.7f, 20.0f + (float)o * 40.0f))
                .stroke(PathFormat{.width = 1.7f,
                                   .strokeFill = Fill::color(ord.rule)})
                .rotate(th)
                .key("tab" + std::to_string(o * 7 + k));
        if (ord.shape == 1)
          tablet.outline(shapes::circle());
        else if (ord.shape == 3)
          tablet.outline(shapes::polygon(3, 180.0f));
        else if (ord.shape == 0)
          tablet.outline(shapes::sector(skAngle(th - 5.2f), 10.4f, 0.905f))
              .width(Dim(2 * rTab * 1.05f * kR))
              .height(Dim(2 * rTab * 1.05f * kR))
              .centerAt({kRR, kRR})
              .rotate(0.0f);
        g.child(std::move(tablet));

        const std::string nm = ord.names[k];
        g.child(text(toU8(nm == "*" ? "E\xc9\x9b" : nm),
                     type(faceSeal, ord.size * kR, hex(0x201404, 1.0f)))
                    .absolute()
                    .width(Dim(2 * ord.radius * kR))
                    .height(Dim(2 * ord.radius * kR))
                    .centerAt({kRR, kRR})
                    .key("chl" + std::to_string(o * 7 + k))
                    .onPath(TextPath{.path = shapes::circle(),
                                     .at = frac(th),
                                     .align = TextPath::Align::Center,
                                     .offset = 0.0f,
                                     .autoFlip = false,
                                     .orient = TextPath::Orient::Radial})
                    );
      }
    }

    // ZABATHIEL — "this name must be distributed in his letters into 7 sides
    // of that innermost Heptagonum. So have you just 7 places."
    g.child(box()
                .absolute()
                .inset(0)
                .outline(heptChords(rInnerHept, 0.0f))
                .fill(Fill::none())
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 2.2f,
                                       .fill = Fill::color(hex(0x3f2c12, 0.88f))},
                                      {.offset = -4.0f,
                                       .width = 0.7f,
                                       .fill = Fill::color(hex(0xfbf0d0, 0.40f))}}))
                .key("zabhept"));
    for (int k = 0; k < 7; ++k) {
      const std::string s = kZabathiel[k];
      g.child(text(toU8(s == "I*" ? "I\xc9\x9b" : s),
                   type(faceSeal, 0.030f * kR, hex(0x201404, 1.0f)))
                  .absolute()
                  .inset(0)
                  .key("zab" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rInnerHept - 0.028f, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  );
    }
    return g;
  }

  static constexpr float kHp = 0.245f * kR;   // the pentagram's own half-box

  Element pentagram() {
    auto g = box()
                 .absolute()
                 .left(kRR - kHp)
                 .top(kRR - kHp)
                 .width(Dim(2 * kHp))
                 .height(Dim(2 * kHp))
                 .transformOrigin(0.5f, 0.5f);
    // "Set Z, of Zedekieil within the angle which standeth up toward the
    // begynning of the greatest Circle" — point-up, aligned on division 1.
    g.child(disc({kHp, kHp}, rPenta * kR)
                .outline(wobbled(shapes::star(5, 0.382f), 5, 16.0f, 0.30f))
                .fill(Fill::color(hex(0xe6cf9e, 0.18f)))
                .stroke(lines::rails(
                    {{.offset = 0.0f,
                      .width = 3.0f,
                      .fill = Fill::color(hex(0x3f2c12, 0.92f))},
                     {.offset = -3.2f,
                      .width = 0.8f,
                      .fill = Fill::color(hex(0xfbf0d0, 0.45f))}}))
                .trim(0.0f, withFrom(0.0f, 1.0f, ramp(tInner * 1000 + 500, 800)))
                .key("penta"));
    for (int k = 0; k < 5; ++k) {
      const float th = (float)k * 72.0f;
      g.child(text(toU8(kPentaNames[(size_t)k].initial),
                   type(faceSeal, 0.052f * kR, hex(0x241704, 1.0f)))
                  .absolute()
                  .width(Dim(2 * rPentaInit * kR))
                  .height(Dim(2 * rPentaInit * kR))
                  .centerAt({kHp, kHp})
                  .key("pi" + std::to_string(k))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = frac(th),
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Radial})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tInner * 1000 + 900 + (float)k * 40,
                                         420))));
      // the rest of the name runs circularly outward into the exterior angle
      g.child(text(toU8(kPentaNames[(size_t)k].tail),
                   type(faceQuill, 0.024f * kR, hex(0x40300f, 0.92f)))
                  .absolute()
                  .width(Dim(2 * rPentaTail * kR))
                  .height(Dim(2 * rPentaTail * kR))
                  .centerAt({kHp, kHp})
                  .key("pt" + std::to_string(k))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = frac(th + 38.0f),
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  .opacity(withFrom(0.0f, 1.0f,
                                    ramp(tInner * 1000 + 980 + (float)k * 40,
                                         420))));
    }
    return g;
  }

  static constexpr float kHc = 0.118f * kR;   // the cross's own half-box

  Element centreCross() {
    auto g = box()
                 .absolute()
                 .left(kRR - kHc)
                 .top(kRR - kHc)
                 .width(Dim(2 * kHc))
                 .height(Dim(2 * kHc));
    const float arm = rCross * kR;
    g.child(box()
                .absolute()
                .width(Dim(2.4f * arm))
                .height(Dim(2.4f * arm))
                .centerAt({kHc, kHc})
                .outline([](SkSize s) {
                  SkPathBuilder b;
                  const float w = s.width(), h = s.height();
                  const float t = w * 0.085f;
                  const float top = h * 0.06f;
                  b.moveTo(w * 0.5f - t, top);
                  b.lineTo(w * 0.5f + t, top);
                  b.lineTo(w * 0.5f + t, h * 0.34f - t);
                  b.lineTo(w * 0.90f, h * 0.34f - t);
                  b.lineTo(w * 0.90f, h * 0.34f + t);
                  b.lineTo(w * 0.5f + t, h * 0.34f + t);
                  b.lineTo(w * 0.5f + t, h * 0.96f);
                  b.lineTo(w * 0.5f - t, h * 0.96f);
                  b.lineTo(w * 0.5f - t, h * 0.34f + t);
                  b.lineTo(w * 0.10f, h * 0.34f + t);
                  b.lineTo(w * 0.10f, h * 0.34f - t);
                  b.lineTo(w * 0.5f - t, h * 0.34f - t);
                  b.close();
                  return b.detach();
                })
                .fill(Fill::color(hex(0xe9d4a4, 0.34f)))
                .stroke(PathFormat{.width = 2.4f,
                                   .strokeFill = Fill::color(hex(0x3f2c12, 0.92f))})
                .key("crux"));
    // LE · VA · NA · el, on the arms — Levanael read left, top, right, foot
    const struct { const char *s; float th; float r; } kArms[4] = {
        {"VA", 0.0f, rCross * 0.72f},
        {"NA", 90.0f, rCross * 1.02f},
        {"el", 180.0f, rCross * 1.02f},
        {"LE", 270.0f, rCross * 1.02f}};
    for (int i = 0; i < 4; ++i) {
      g.child(text(toU8(kArms[i].s),
                   type(faceSeal, 0.025f * kR, hex(0x2b1d08, 1.0f)))
                  .absolute()
                  .width(Dim(2 * kArms[i].r * kR))
                  .height(Dim(2 * kArms[i].r * kR))
                  .centerAt({kHc, kHc})
                  .key("lev" + std::to_string(i))
                  .onPath(TextPath{.path = shapes::circle(),
                                   .at = frac(kArms[i].th),
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Upright})
                  .opacity(withFrom(0.0f, 1.0f, ramp(tInner * 1000 + 1200, 500))));
    }
    return g;
  }

  // =========================================================================
  // THE SOLVER, ON THE PLATE. Seven names; each hop is an arc that leaps the
  // rim, drawn on by a trim window.

  Element solverOverlay() {
    auto g = box().absolute().inset(0);
    for (int n = 0; n < 7; ++n) {
      const Solved &s = solved[(size_t)n];
      const float t0 = tSolve + (float)n * tSolveEach;
      // ONE path per Name, one CONTOUR per hop. trim() walks every contour
      // as a single arc-length coordinate, so 0 -> 1 marches the walk hop by
      // hop — the whole solve is two nodes per Name instead of sixteen.
      std::vector<SkPoint> from, to, ctrl, land;
      for (size_t h = 0; h < s.cells.size(); ++h) {
        land.push_back(P((float)(s.cells[h] - 1) * 9.0f, rCellLet));
        if (h + 1 >= s.cells.size())
          break;
        const int a0 = s.cells[h] - 1, b0 = s.cells[h + 1] - 1;
        const SkPoint a = P((float)a0 * 9.0f, rCellLet - 0.028f);
        const SkPoint b = P((float)b0 * 9.0f, rCellLet - 0.028f);
        int d = b0 - a0;
        while (d > 20) d -= 40;
        while (d < -20) d += 40;
        const float bulge = std::min(0.46f, std::fabs((float)d) * 0.030f);
        const SkPoint mid{(a.fX + b.fX) * 0.5f, (a.fY + b.fY) * 0.5f};
        const SkVector toC{kRR - mid.fX, kRR - mid.fY};
        const float m = std::max(1e-3f, std::hypot(toC.fX, toC.fY));
        from.push_back(a);
        to.push_back(b);
        ctrl.push_back({mid.fX + toC.fX / m * bulge * kR,
                        mid.fY + toC.fY / m * bulge * kR});
      }
      (void)t0;
      if (solvePhase < 0 || n > solvePhase)
        continue;
      const bool live = (n == solvePhase);
      auto fade = [&](float) -> PropValue<float> {
        return live ? PropValue<float>(withFrom(0.0f, 1.0f, ramp(0, 220)))
                    : PropValue<float>(0.15f);
      };
      auto reveal = [&](float ms) -> PropValue<float> {
        return live ? PropValue<float>(withFrom(0.0f, 1.0f, ramp(0, ms)))
                    : PropValue<float>(1.0f);
      };
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline([from, to, ctrl](SkSize) {
                    SkPathBuilder b;
                    for (size_t i = 0; i < from.size(); ++i) {
                      b.moveTo(from[i]);
                      b.quadTo(ctrl[i], to[i]);
                    }
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(lines::Line{.width = 2.6f,
                                      .fill = Fill::color(hex(0x7fd0f4, 0.95f)),
                                      .endCap = lines::Cap::Arrow,
                                      .capSize = 15.0f})
                  .trim(0.0f, reveal(760.0f))
                  .opacity(fade(0))
                  .key("hops" + std::to_string(n)));
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline([land](SkSize) {
                    SkPathBuilder b;
                    for (const SkPoint &q : land)
                      b.addCircle(q.fX, q.fY, 0.030f * kR);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = 2.0f,
                      .strokeFill = Fill::color(hex(0x59b6e8, 0.92f))})
                  .trim(0.0f, reveal(820.0f))
                  .opacity(fade(0))
                  .key("lands" + std::to_string(n)));
    }
    return g;
  }

  // =========================================================================
  // THE MARGIN — title, the seven Names assembling, the 7×7 square, legend.

  Element margin() {
    const float w = 690;
    auto g = box()
                 .absolute()
                 .left(1660 * kS)
                 .top(56 * kS)
                 .width(w)
                 .height(1588)
                 .scale(kS)
                 .transformOrigin(0.0f, 0.0f);

    g.child(text(toU8("SIGILLVM DEI \xc3\x86M\xc3\x86TH"),
                 type(faceDisplay, 46, kVellum, 2.6f))
                .absolute()
                .left(0)
                .top(0));
    g.child(text(toU8("EMETH nuncupatum \xc2\xb7 Mortlake by Richemond \xc2\xb7 "
                      "21 Martii 1582"),
                 type(faceItalic, 19, hex(0xc7ab74)))
                .absolute()
                .left(2)
                .top(58));
    g.child(text(toU8("BL Sloane MS 3188 f.30r \xc2\xb7 wax disc BM 1838,1232.90.a "
                      "\xc2\xb7 23.2 cm"),
                 type(faceSerif, 15, hex(0x8d7a58)))
                .absolute()
                .left(2)
                .top(86));
    g.child(box()
                .absolute()
                .left(0)
                .top(114)
                .width(w)
                .height(2)
                .fill(Fill::none())
                .outline([w](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(0, 1);
                  b.lineTo(w, 1);
                  return b.detach();
                })
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 2.4f,
                                       .fill = Fill::color(hex(0xc7ab74, 0.75f))},
                                      {.offset = 5.0f,
                                       .width = 0.8f,
                                       .fill = Fill::color(hex(0xc7ab74, 0.40f)),
                                       .dash = {2.0f, 5.0f}}})));

    // the seven Names, printing as the walk finds them
    g.child(text(toU8("THE SEVEN NAMES, WALKED OFF THE RIM"),
                 type(faceMono, 15, kRubric, 1.6f))
                .absolute()
                .left(0)
                .top(136));
    g.child(box()
                .absolute()
                .left(0)
                .top(158)
                .width(w)
                .height(324)
                .outline([w](SkSize) {
                  SkPathBuilder b;
                  for (int n = 0; n <= 7; ++n) {
                    b.moveTo(0, 4 + (float)n * 46);
                    b.lineTo(w, 4 + (float)n * 46);
                  }
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 0.8f,
                    .strokeFill = Fill::color(hex(0xc7ab74, 0.14f))}));
    for (int n = 0; n < 7; ++n) {
      const Solved &s = solved[(size_t)n];
      const float y = 166 + (float)n * 46;
      const float at = (tSolve + (float)n * tSolveEach) * 1000;
      std::string chain;
      for (size_t i = 0; i < s.cells.size(); ++i)
        chain += (i ? "\xc2\xb7" : "") + std::to_string(s.cells[i]);
      g.child(text(toU8(std::to_string(n + 1) + "."),
                   type(faceMono, 17, hex(0x8d7a58)))
                  .absolute()
                  .left(0)
                  .top(y + 6)
                  .opacity(withFrom(0.0f, 1.0f, ramp(at, 300))));
      g.child(text(toU8(kNames[(size_t)n].name),
                   type(faceDisplay, 30, kVellum, 1.2f))
                  .absolute()
                  .left(34)
                  .top(y)
                  .opacity(withFrom(0.0f, 1.0f, ramp(at + 120, 420))));
      g.child(text(toU8(s.raw == s.reduced ? "" : "\xe2\x9f\xa8" + s.raw + "\xe2\x9f\xa9"),
                   type(faceItalic, 15, hex(0x6f5f45)))
                  .absolute()
                  .left(212)
                  .top(y + 10)
                  .opacity(withFrom(0.0f, 1.0f, ramp(at + 240, 420))));
      g.child(text(toU8(chain), type(faceMono, 14, kTrace))
                  .absolute()
                  .left(320)
                  .top(y + 10)
                  .opacity(withFrom(0.0f, 1.0f, ramp(at + 60, 420))));
    }

    // the leftovers
    {
      std::string un, unl;
      for (int i = 0; i < 40; ++i)
        if (!visited[(size_t)i]) {
          un += (un.empty() ? "" : "\xc2\xb7") + std::to_string(i + 1);
          unl += kRing[(size_t)i].glyph;
        }
      g.child(text(toU8(fmt("%d of 40 cells consumed \xc2\xb7 %d never visited",
                            usedCells, 40 - usedCells)),
                   type(faceMono, 15, hex(0x8d7a58)))
                  .absolute()
                  .left(0)
                  .top(492)
                  .opacity(withFrom(0.0f, 1.0f, ramp(tDark * 1000, 500))));
      g.child(text(toU8("unvisited  " + un + "   =  " + unl),
                   type(faceMono, 15, kRubric))
                  .absolute()
                  .left(0)
                  .top(514)
                  .opacity(withFrom(0.0f, 1.0f, ramp(tDark * 1000 + 200, 500))));
      g.child(text(toU8("\xe2\x86\xb3 the same rule reads them as YMON 22\xc2\xb7"
                        "7\xc2\xb7\x31\x33\xc2\xb7\x33\x31 and BORAOTH "
                        "26\xc2\xb7\x33\x36\xc2\xb7\x31\x39\xc2\xb7\xe2\x80\xa6"),
                   type(faceItalic, 14, hex(0x6f5f45)))
                  .absolute()
                  .left(0)
                  .top(536)
                  .opacity(withFrom(0.0f, 1.0f, ramp(tDark * 1000 + 400, 500))));
    }

    // the 7×7 square the birds delivered; read DOWN the columns
    g.child(text(toU8("SEVEN BASKETS, SEVEN BIRDS \xc2\xb7 READ DOWN"),
                 type(faceMono, 15, kRubric, 1.6f))
                .absolute()
                .left(0)
                .top(580));
    // The seven angles UNROLLED, not tabulated. On the plate these rows lie
    // along seven sides of a heptagon; here they lie on seven nested arcs of
    // the same fan, so a "column" is a RADIAL RAY and reading down a column
    // is reading outward — which is what the columns do on the object. A
    // leader curves from each ray to the archangel it spells.
    const float fanCx = 250.0f, fanCy = 1028.0f;
    const float fanR0 = 420.0f, fanDR = 33.0f, fanSpan = 46.0f;
    auto fanPt = [&](int row, int col, float dr) {
      const float a = (-fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f) *
                      kD;
      const float rr = fanR0 - (float)row * fanDR + dr;
      return SkPoint{fanCx + rr * std::sin(a), fanCy - rr * std::cos(a)};
    };
    auto fanAngle = [&](int col) {
      return -fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f;
    };
    // the seven arcs the rows sit on — ruled first, as on a prepared sheet
    g.child(box()
                .absolute()
                .left(0)
                .top(560)
                .width(w)
                .height(300)
                .outline([&](SkSize) {
                  SkPathBuilder b;
                  for (int r = 0; r <= 7; ++r) {
                    const float rr = fanR0 - (float)r * fanDR + fanDR * 0.5f;
                    for (int i = 0; i <= 24; ++i) {
                      const float a =
                          (-fanSpan * 0.54f + fanSpan * 1.08f * (float)i / 24.0f) *
                          kD;
                      const SkPoint q{fanCx + rr * std::sin(a),
                                      fanCy - 560.0f - rr * std::cos(a)};
                      i == 0 ? b.moveTo(q) : b.lineTo(q);
                    }
                  }
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(PathFormat{
                    .width = 0.8f,
                    .strokeFill = Fill::color(hex(0xc7ab74, 0.15f))}));
    // the column rays light in sequence, and each drags a leader out to its name
    for (int c = 0; c < 7; ++c) {
      const float delay = tBirds * 1000 + 1900 + (float)c * 90;
      const SkPoint a0 = fanPt(0, c, fanDR * 0.55f);
      const SkPoint a1 = fanPt(6, c, -fanDR * 0.55f);
      const SkPoint nameAt{452.0f, 612.0f + (float)c * 33.0f};
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline([a0, a1](SkSize) {
                    SkPathBuilder b;
                    b.moveTo(a0);
                    b.lineTo(a1);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(lines::rails(
                      {{.offset = -19.0f,
                        .width = 0.9f,
                        .fill = Fill::color(hex(0x62b0dc, 0.60f))},
                       {.offset = 19.0f,
                        .width = 0.9f,
                        .fill = Fill::color(hex(0x62b0dc, 0.60f))}}))
                  .opacity(withFrom(0.0f, 1.0f, ramp(delay, 360))));
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline([a1, nameAt](SkSize) {
                    SkPathBuilder b;
                    b.moveTo(a1);
                    b.quadTo({(a1.fX + nameAt.fX) * 0.5f, a1.fY - 6.0f},
                             {nameAt.fX - 8.0f, nameAt.fY + 12.0f});
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(lines::Line{.width = 0.9f,
                                      .fill = Fill::color(hex(0x2f6f9c, 0.55f)),
                                      .endCap = lines::Cap::Dot,
                                      .capSize = 4.0f})
                  .trim(0.0f, withFrom(0.0f, 1.0f, ramp(delay + 120, 420)))
                  .opacity(withFrom(0.0f, 1.0f, ramp(delay + 120, 300))));
      g.child(text(toU8(kArchangels[c]), type(faceQuill, 21, hex(0xd8c08a)))
                  .absolute()
                  .left(nameAt.fX)
                  .top(nameAt.fY)
                  .opacity(withFrom(0.0f, 1.0f, ramp(delay + 220, 360))));
    }
    // the 49 letters, one per (row, column) slot on the fan
    for (int r = 0; r < 7; ++r)
      for (int c = 0; c < 7; ++c) {
        const float delay = tBirds * 1000 + (float)r * 260 + (float)c * 34;
        const bool isCross = kAngles[r][c] == std::string("\xe2\x80\xa0");
        const SkPoint at = fanPt(r, c, 0.0f);
        g.child(text(toU8(kAngles[r][c]),
                     type(faceSeal, 23, isCross ? kRubric : kVellum))
                    .absolute()
                    .width(30)
                    .height(30)
                    .centerAt(at)
                    .rotate(fanAngle(c))
                    .textAlign(sigil::weave::TextAlignment::kCenter)
                    .opacity(withFrom(0.0f, 1.0f, ramp(delay, 300))));
      }
    g.child(text(toU8("48 letters, and one is noted by a Cross: which maketh "
                      "the 49th."),
                 type(faceItalic, 15, hex(0x8d7a58)))
                .absolute()
                .left(0)
                .top(840)
                .opacity(withFrom(0.0f, 1.0f, ramp(tBirds * 1000 + 2600, 400))));

    // the four orders and their tablets
    const char *kLegend[4] = {
        "Fili\xc3\xa6 Lucis \xc2\xb7 blue tablet in the forehead",
        "Filii Lucis \xc2\xb7 round gold tablet on the breast",
        "Fili\xc3\xa6 Filiarum \xc2\xb7 four-square white ivory",
        "Filii Filiorum \xc2\xb7 three-cornered green"};
    const SkColor4f kLegendTint[4] = {hex(0xb9c6da, 0.95f), hex(0xe6bf63, 0.95f),
                                      hex(0xf7f1e2, 0.95f), hex(0x9dbfa2, 0.95f)};
    g.child(text(toU8("THE FOUR ORDERS OF THE CHILDREN OF LIGHT"),
                 type(faceMono, 15, kRubric, 1.6f))
                .absolute()
                .left(0)
                .top(870));
    for (int i = 0; i < 4; ++i) {
      Element swatch = box()
                           .absolute()
                           .left(2)
                           .top(898 + (float)i * 26)
                           .width(16)
                           .height(16)
                           .fill(Fill::color(kLegendTint[i]));
      if (i == 1)
        swatch.outline(shapes::circle());
      else if (i == 3)
        swatch.outline(shapes::polygon(3));
      else if (i == 0)
        swatch.outline(shapes::sector(-100.0f, 200.0f, 0.55f));
      g.child(std::move(swatch));
      g.child(text(toU8(kLegend[i]), type(faceSerif, 15, hex(0x9d8a66)))
                  .absolute()
                  .left(28)
                  .top(896 + (float)i * 26));
    }
    return g;
  }

  // =========================================================================

  console::Style logStyle() {
    // 10.2, not 11. The panel is 690 wide, less 24 of padding, less a 14 gap
    // either side of the 1 px divider: 318.5 to a column, and this mono is
    // 0.62 em, so 11 pt fits 46 characters and nothing warns you about the
    // 47th. FOUR lines were being truncated with no ellipsis and no clue —
    // the two longest checks in the whole plate lost their units ("R = 257.972"
    // for "R = 257.972 pt", "off by (-0.82, -2.47) p"), one lost a closing
    // paren ("vs r(letter"), and the {7/2} verdict lost its full stop. The
    // longest line is 50 characters, so the type has to be 11 x 47/50 or less.
    constexpr float kMono = 10.2f;
    console::Style s;
    s.text = type(faceMono, kMono, hex(0x9d8a66));
    s.palette = {type(faceMono, kMono, hex(0x6b5c44)),  // 0 dim
                 type(faceMono, kMono, kRubric),        // 1 heading
                 type(faceMono, kMono, hex(0x59b98a)),  // 2 PASS
                 type(faceMono, kMono, hex(0x62b0dc))}; // 3 number
    s.gap = 1.0f;
    s.visibleLines = 16;
    return s;
  }

  Element consolePanel() {
    const float px = 1660 * kS, py = 1058 * kS, pw = 690, ph = 468;
    auto g = box()
                 .absolute()
                 .left(px)
                 .top(py)
                 .width(pw)
                 .height(ph)
                 .scale(kS)
                 .transformOrigin(0.0f, 0.0f)
                 .fill(Fill::color(hex(0x1b1e26, 0.86f)))
                 .stroke(stroke(1.0f, Fill::color(hex(0xc7ab74, 0.22f)),
                                PathFormat::Align::Inner));
    g.child(box()
                .absolute()
                .left(12)
                .top(8)
                .width(pw - 24)
                .height(ph - 16)
                .row()
                .gap(14)
                .child(box()
                           .column()
                           .grow(1)
                           .gap(6)
                           .child(console::console(logA, logStyle()))
                           .child(console::console(logC, logStyle())))
                .child(box().width(1).fill(Fill::color(hex(0xc7ab74, 0.16f))))
                .child(box()
                           .column()
                           .grow(1)
                           .gap(6)
                           .child(console::console(logB, logStyle()))
                           .child(console::console(logD, logStyle()))));
    return g;
  }

  Element colophon() {
    auto g = box()
                 .absolute()
                 .left(1660 * kS)
                 .top(1552 * kS)
                 .width(690)
                 .height(120)
                 .scale(kS)
                 .transformOrigin(0.0f, 0.0f);
    g.child(box()
                .absolute()
                .left(0)
                .top(0)
                .width(690)
                .height(2)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(0, 1);
                  b.lineTo(690, 1);
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(lines::rails(
                    {{.offset = 0.0f,
                      .width = 1.8f,
                      .fill = Fill::color(hex(0xc7ab74, 0.55f))},
                     {.offset = 4.0f,
                      .width = 0.7f,
                      .fill = Fill::color(hex(0xc7ab74, 0.30f)),
                      .dash = {1.6f, 4.4f}}})));
    g.child(text(toU8("\xe2\x80\x9cThis is the Seale, whose Name is \xc3\x86meth: "
                      "and it is to be made of perfect wax.\xe2\x80\x9d"),
                 type(faceItalic, 17, hex(0xb59a6c)))
                .absolute()
                .left(0)
                .top(16)
                .width(690));
    g.child(text(toU8("Uriel, 14 March 1582 \xc2\xb7 reconstruction from the "
                      "rule, not a tracing \xc2\xb7 SigilCompose study"),
                 type(faceMono, 12, hex(0x6f5f45)))
                .absolute()
                .left(0)
                .top(62));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &) {
    auto root = box().absolute().inset(0);

    auto seal = box()
                    .absolute()
                    .left(kCx - kRR)
                    .top(kCy - kRR)
                    .width(Dim(2 * kRR))
                    .height(Dim(2 * kRR));
    seal.child(waxGround().cache(Cache::Texture));
    seal.child(circumferenceRules());
    // The settle. The rim is the FRAME and the two inner systems turn
    // against it off ONE Output — the seven-fold body one way, the five-fold
    // heart the other. Relative motion is what the figure is about: since
    // gcd(40,7) = gcd(40,5) = gcd(7,5) = 1 the three agree in exactly one
    // direction, and arriving there is the ending. (The rim turned too until
    // it was measured: a third rotating full-plate layer is ~7 ms a frame of
    // pure layer resample and buys no relative motion that is not already
    // on screen.)
    seal.child(circumferenceCells());
    // the whole seven-fold system turns as one body — heptagon, its two
    // letter bands, the heptagram woven on its vertices, and everything the
    // heptagram's points contain.
    seal.child(box()
                   .absolute()
                   .inset(0)
                   .transformOrigin(0.5f, 0.5f)
                   .rotate(bind(&settle).to(0.0f, -360.0f / 7.0f))
                   .opacity(withFrom(0.0f, 1.0f, ramp(tInner * 1000, 900)))
                   .cache(Cache::Texture)
                   .child(angles())
                   .child(heptagonNames())
                   .child(heptagram())
                   .child(inner()));
    seal.child(innerRings());
    seal.child(pentagram()
                   .rotate(bind(&settle).to(0.0f, 72.0f))
                   .transformOrigin(0.5f, 0.5f));
    seal.child(centreCross());
    seal.child(slot("solver"));
    root.child(std::move(seal));
    root.child(margin());
    root.child(consolePanel());
    root.child(colophon());
    return root;
  }

  // =========================================================================
  // THE CHECKS

  void runChecks() {
    // --- panel A: the extraction and the fit ------------------------------
    logA.append(toU8("SECUNDUS.PDF p.31 \xe2\x80\x94 THE PLATE AS VECTORS"), 1);
    logA.append(toU8("  310 text objects; 296 after the colophon"), 0);
    logA.append(toU8("  K\xc3\xa5sa fit, outer letter ring, 44 glyphs:"), 0);
    logA.append(toU8("    centre (305.185, 393.529) pt   R = 257.972 pt"), 3);
    logA.append(toU8("    page bbox centre is off by (-0.82, -2.47) pt"), 0);
    logA.append(toU8("    residual rms 4.35 pt = 1.7% R"), 0);
    logA.append(toU8("  9\xc2\xb0 lattice fit: phase +4.30\xc2\xb0, rms 0.81\xc2\xb0"), 3);
    logA.append(toU8("  40 cells recovered: 40 letters, 33 numerals"), 0);
    logA.append(toU8("  above/below decided by r(numeral) vs r(letter)"), 0);
    {
      int above = 0, below = 0, bare = 0;
      for (const Cell &c : kRing)
        (c.step > 0 ? above : c.step < 0 ? below : bare)++;
      logA.append(toU8(fmt("    %d above  %d below  %d bare   = 40",
                           above, below, bare)),
                  above + below + bare == 40 ? 2 : 1);
    }

    // --- panel B: the walk -------------------------------------------------
    logB.append(toU8("MICHAEL'S JUMP RULE, WALKED FROM ALL 40 CELLS"), 1);
    for (int n = 0; n < 7; ++n) {
      const Solved &s = solved[(size_t)n];
      const bool ok = s.reduced == kNames[(size_t)n].name ||
                      s.raw == kNames[(size_t)n].name;
      std::string chain;
      for (size_t i = 0; i < s.cells.size(); ++i)
        chain += (i ? "-" : "") + std::to_string(s.cells[i]);
      logB.append(toU8(fmt("  %-9s %-9s %s", kNames[(size_t)n].name,
                           s.raw.c_str(), chain.c_str())),
                  ok ? 2 : 1);
    }
    logB.append(toU8(fmt("  cells consumed %d of 40, unvisited %d", usedCells,
                         40 - usedCells)),
                usedCells == 33 ? 2 : 1);
    {
      std::string un;
      for (int i = 0; i < 40; ++i)
        if (!visited[(size_t)i])
          un += (un.empty() ? "" : " ") + std::to_string(i + 1) + kRing[(size_t)i].glyph;
      logB.append(toU8("  " + un), 3);
    }
    logB.append(toU8("  Aaoth from 27 also spells Aaoth \xe2\x80\x94 but leaves"), 0);
    logB.append(toU8("  32 cells. The 33/7 count picks cell 32."), 2);

    // --- panel C: coverage -------------------------------------------------
    {
      std::vector<SkPath> cells;
      cells.reserve(40);
      for (int i = 0; i < 40; ++i) {
        const float th = (float)i * 9.0f - 4.5f;
        SkPathBuilder b;
        const int n = 12;
        for (int j = 0; j <= n; ++j) {
          const float a = (th + 9.0f * (float)j / (float)n) * kD;
          const SkPoint q{rGreat * std::sin(a), -rGreat * std::cos(a)};
          j == 0 ? b.moveTo(q) : b.lineTo(q);
        }
        for (int j = n; j >= 0; --j) {
          const float a = (th + 9.0f * (float)j / (float)n) * kD;
          b.lineTo({rBandIn * std::sin(a), -rBandIn * std::cos(a)});
        }
        b.close();
        cells.push_back(b.detach());
      }
      SkPathBuilder ringB;
      ringB.setFillType(SkPathFillType::kEvenOdd);
      ringB.addCircle(0, 0, rGreat);
      ringB.addCircle(0, 0, rBandIn);
      const SkPath ring = ringB.detach();
      const SkRect reg = SkRect::MakeLTRB(-rGreat, -rGreat, rGreat, rGreat);
      const debug::Coverage cov = debug::coverage(cells, reg, 320);
      const debug::Coverage ref =
          debug::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
      logC.append(toU8("THE 40 CELLS TILE THE ANNULUS"), 1);
      logC.append(toU8(fmt("  doubled %d of %d samples", cov.doubled, cov.samples)),
                  cov.doubled == 0 ? 2 : 1);
      logC.append(toU8(fmt("  uncovered %d - outside-the-ring %d = %d",
                           cov.uncovered, ref.uncovered,
                           cov.uncovered - ref.uncovered)),
                  std::abs(cov.uncovered - ref.uncovered) <= 320 ? 2 : 1);
    }
    {
      std::vector<SkPath> plates;
      for (int k = 0; k < 7; ++k) {
        const float mid = ((float)k + 0.5f) * 360.0f / 7.0f;
        const float half = 360.0f / 7.0f * 0.5f;
        SkPathBuilder b;
        const int n = 14;
        for (int j = 0; j <= n; ++j) {
          const float a = (mid - half + 2 * half * (float)j / (float)n) * kD;
          const SkPoint q{0.868f * std::sin(a), -0.868f * std::cos(a)};
          j == 0 ? b.moveTo(q) : b.lineTo(q);
        }
        for (int j = n; j >= 0; --j) {
          const float a = (mid - half + 2 * half * (float)j / (float)n) * kD;
          b.lineTo({0.720f * std::sin(a), -0.720f * std::cos(a)});
        }
        b.close();
        plates.push_back(b.detach());
      }
      SkPathBuilder ringB;
      ringB.setFillType(SkPathFillType::kEvenOdd);
      ringB.addCircle(0, 0, 0.868f);
      ringB.addCircle(0, 0, 0.720f);
      const SkPath ring = ringB.detach();
      const SkRect reg = SkRect::MakeLTRB(-0.868f, -0.868f, 0.868f, 0.868f);
      const debug::Coverage cov = debug::coverage(plates, reg, 320);
      const debug::Coverage ref =
          debug::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
      logC.append(toU8("THE 7 ANGLE PLATES TILE THEIR BAND"), 1);
      logC.append(toU8(fmt("  doubled %d, gap vs ring %d", cov.doubled,
                           cov.uncovered - ref.uncovered)),
                  cov.doubled == 0 ? 2 : 1);
      int letters = 0;
      for (int r = 0; r < 7; ++r)
        for (int c = 0; c < 7; ++c)
          if (kAngles[r][c] != std::string("\xe2\x80\xa0"))
            ++letters;
      logC.append(toU8(fmt("  %d letters + 1 cross = %d places", letters,
                           letters + 1)),
                  letters == 48 ? 2 : 1);
      std::string cols;
      for (int c = 0; c < 7; ++c)
        for (int r = 0; r < 7; ++r)
          if (kAngles[r][c] != std::string("\xe2\x80\xa0"))
            cols += kAngles[r][c];
      logC.append(toU8("  down the columns: " + cols.substr(0, 24)), 3);
      logC.append(toU8("  " + cols.substr(24)), 3);
    }

    // --- panel D: {7/2} vs {7/3}, and the weave ---------------------------
    logD.append(toU8("{7/2} OR {7/3}? THE COORDINATES DECIDE"), 1);
    logD.append(toU8(fmt("  {7/2} core = %.4f x Rhept = %.3f R", (double)kStar72,
                         (double)(kStar72 * rHept))),
                3);
    logD.append(toU8(fmt("  {7/3} core = %.4f x Rhept = %.3f R", (double)kStar73,
                         (double)(kStar73 * rHept))),
                3);
    logD.append(toU8(fmt("  along a point's ray the core stops at %.3f R",
                         (double)(kStar72 * rHept * std::cos(3.14159265f / 7)))),
                0);
    // "Filiae", not "Fili\xc3\xa6": the console runs in the MONO face and that
    // face has no ash. It fell back to a different typeface mid-word and the
    // line read "Fili=/Filii". The seal's own legend, which is set in the
    // serif, keeps the ligature.
    logD.append(toU8(fmt("  Filiae/Filii Filiorum measured %.3f / %.3f R",
                         (double)rFiliaeFil, (double)rFiliiFil)),
                0);
    logD.append(toU8("  both INSIDE a {7/2} core, as the record says;"), 0);
    logD.append(toU8("  {7/3} would leave them floating mid-point and"), 0);
    logD.append(toU8("  would collide with ZABATHIEL at 0.285 R.  {7/2}."), 2);
    logD.append(toU8("THE INTERLACE"), 1);
    logD.append(toU8(fmt("  %zu crossings, %zu passes, %d inconsistent",
                         weave.crossings.size(), weave.crossings.size() * 2,
                         weave.inconsistent)),
                (weave.crossings.size() == 14 && weave.inconsistent == 0) ? 2 : 1);
    logD.append(toU8("  gcd(40,7)=gcd(40,5)=gcd(7,5)=1 \xe2\x80\x94 one"), 0);
    logD.append(toU8("  alignment only, and it is 12 o'clock."), 2);
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kVitrine);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    faceSeal = family("Herculanum", SkFontStyle::Normal());
    faceRing = family("Trattatello", SkFontStyle::Normal());
    faceQuill = family("Hoefler Text", SkFontStyle::Italic());
    faceDisplay = family("Luminari", SkFontStyle::Normal());
    faceSerif = family("Hoefler Text", SkFontStyle::Normal());
    faceItalic = family("Hoefler Text", SkFontStyle::Italic());
    faceMono = family("Menlo", SkFontStyle::Normal());
    if (!faceSerif)
      faceSerif = family("Baskerville", SkFontStyle::Normal());
    if (!faceItalic)
      faceItalic = family("Baskerville", SkFontStyle::Italic());
    if (!faceSeal)
      faceSeal = family("Optima", SkFontStyle::Normal());
    if (!faceQuill)
      faceQuill = faceItalic;
    if (!faceRing)
      faceRing = faceItalic;
    if (!faceDisplay)
      faceDisplay = faceSeal;
    if (!faceMono)
      faceMono = family("Courier New", SkFontStyle::Normal());
    if (!faceSeal)
      faceSeal = faceSerif;

    waxGrain = patterns::grain(1.6f, 4, 1582.0f, 0.34f);
    waxSpeck = patterns::speckle(520, 18, 1.4f, 4.4f, {hex(0x6a4a20, 0.10f)});
    waxSpeck.seed(1582);

    // solve, then draw what the solver said
    for (int n = 0; n < 7; ++n)
      solved[(size_t)n] = walkFrom(kNames[(size_t)n].start);
    visited.fill(false);
    for (const Solved &s : solved)
      for (int c : s.cells)
        visited[(size_t)(c - 1)] = true;
    usedCells = 0;
    for (bool v : visited)
      usedCells += v ? 1 : 0;
    weave = buildWeave(rHept);
    runChecks();

    // ONE Output: three systems turn off it and stop together at 0.
    ctx.ticker.add([this](double dt) {
      clockT += dt;
      const double loop = 26.0;
      const double t = std::fmod(clockT, loop);
      pulseT = (float)t;
      // the rings turn from tSpin, decelerating into the one alignment
      const double a = (t - (double)tSpin) / (24.0 - (double)tSpin);
      const double k = a <= 0 ? 0.0 : a >= 1 ? 0.0 : 1.0 - (1.0 - (1.0 - a)) * 0.0;
      (void)k;
      float s = 0.0f;
      if (t > (double)tSpin && t < 24.0) {
        const double u = (t - (double)tSpin) / (24.0 - (double)tSpin);
        // ease out from 1 turn to none
        s = (float)((1.0 - u) * (1.0 - u) * (1.0 - u));
      }
      settle = s;
      tracer = (float)t;
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  /** The one discrete state this sketch has, and it is a CACHING state.
   *  At rest the library promotes the seven-fold group's parts to small
   *  textures and the frame is 8 ms. Under a live rotation it falls back to
   *  replaying that group's picture — every AA clip, every hatch lattice and
   *  every glyph re-rasterised through a rotated matrix — and the frame is
   *  45 ms. Forcing Cache::Texture fixes the spin and costs 8 ms at rest, so
   *  neither setting wins everywhere. Re-describing at the phase boundary
   *  does: two render() calls per 26 s loop. */
  void update(double, sketch::SketchContext &ctx) override {
    const double t = std::fmod(clockT, 26.0);
    // -1 before the walk and again once the plate turns; 7 = every Name
    // solved, all of them held as faint ghosts while the birds arrive.
    int phase = -1;
    if (t >= (double)tSolve && t < (double)tSpin - 0.4)
      phase = std::min(7, (int)((t - (double)tSolve) / (double)tSolveEach));
    if (phase != solvePhase) {
      solvePhase = phase;
      ctx.composer.renderSlot("solver", solverOverlay());
    }
  }
};

SIGIL_SKETCH(SigillumAemeth)
