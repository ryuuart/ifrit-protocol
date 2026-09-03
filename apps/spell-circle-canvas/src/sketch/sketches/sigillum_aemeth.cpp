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
//                         hairline inner, per-rail width AND dash, which is
//                         exactly what a single lines::Line cannot express.
//                         Nine rules on this plate are drawn with it.
//   lines::Line           Cap::Arrow on the jump arcs (a jump is DIRECTED)
//                         and Cap::Dot on the margin's leader lines
//   lines::hatch/crosshatch/radialHatch   the wax ground, the 40 wedge
//                         cells, the seven angle plates, the 28 tablets,
//                         and the deep recess between star and heptagon
//   brush::Pattern corner tiles — "at each corner of these segments
//                         of circles, to make little Crosses", 1582 — and
//                         `cornerAlign = Outgoing`, because the default
//                         bisects a RIGHT angle and a cross turned 45
//                         degrees is an X (see angles())
//   brush::Scatter the compass pricks: forty divisions are STEPPED
//                         round with dividers, not measured, and the point
//                         leaves a mark at every step
//   brush::Ribbon       the calligraphic nib — the heptagon is RULED with
//                         a quill, so its seven sides come out at seven
//                         weights from one nib angle
//   shapers::Jitter       every circle: a compass in a wax cake wanders
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
//   trim() + animate()    the jump-walk: one path per Name, one CONTOUR per
//                         hop, so 0→1 marches the walk hop by hop
//   bind()                ONE Output driving two counter-rotations
//   slot()/renderSlot()   the solver as an independent update domain
//   test::coverage       the 40 cells tile the annulus; the 7 angle plates
//                         tile their band
//   feed::TextRing        four panels of checks, printed as they run
//   Cache::Texture        the wax, the angle plates, the turning heptad
//
// A star polygon {n/k} self-intersects n(k−1) times, so this heptagram has
// SEVEN crossings and fourteen passes — not the fourteen crossings a {7/3}
// would have. They are DISCOVERED, never authored: `discoverCrossings`
// finds the proper crossings, which is what keeps the seven shared vertices
// out of the list, and `crossingPatch` gives the exact lens where two bands
// overlap at a knot — the region the graver repaints to take one limb
// through. A disc is the wrong shape there: two bands meeting at a shallow
// angle overlap in a long lens, and a disc sized for the perpendicular case
// leaves the under-band showing straight across the over-band's cut.
//
// THE ORDER IS ALTERNATION ALONG THE CURVE: `crossing::alternateAlong()`,
// not `crossing::alternate()`. An alternating knot alternates as you
// TRAVEL it, and the heptagram is one closed curve,
// 0→2→4→6→1→3→5→0; `crossing::alternate()` alternates by the crossing's
// discovered ordinal, which on this star puts two consecutive UNDERs on
// the strand from vertex 3, where the band would pass behind both its
// neighbours and vanish. Panel D counts the crossings and walks every
// strand back over the prepared rule to see that its sides alternate.
//
// WHAT MAKES IT AFFORDABLE. The plate is large and nearly all of it is
// static, so the whole question is which layers can be baked and then left
// alone. Six things decide that here:
//   1. a mask blur is not a decoration. A wide-sigma shadow under the cake,
//      or a multi-layer blurred filament on every solver arc, costs more
//      than everything else on the plate put together.
//   2. a KEYFRAME PATH THAT IS STILL RUNNING IS LIVE VOLATILITY EVEN WHILE
//      ITS VALUE IS CONSTANT. A text-on-path run holding a mid-path opacity
//      paints live for the whole hold and keeps its parents out of a
//      texture; a ramp that finishes does not.
//   3. the circumference band is 40 letters and 33 numerals as separate
//      nodes, and the unvisited cells fade on staggered delays. While ANY
//      of those is still animating the band as a whole cannot settle into
//      a cache, so a stagger spread over seconds is a cost, not a flourish.
//   4. a group's box should be sized to what is drawn ON it. The seal's
//      groups are sized to their content, not to the wax cake (1.058 R),
//      which is wider than anything they carry; every layer composite pays
//      for the box, not for the marks.
//   5. rotation-invariant geometry has no business inside a rotating layer,
//      which is why the circles and the radial hatch field sit outside the
//      turning groups even though they belong to the same figure.
//   6. the solver is an independent update domain — slot() + renderSlot() —
//      so advancing the walk never re-describes the plate.
// And one shape to know before trying it: forcing Cache::Texture on the big
// groups bakes ONE layer the size of the plate where the library's own
// promotion bakes several the size of their subtrees. The exception is a
// group under a LIVE rotation, where the library falls back to replaying the
// picture through a rotated matrix and an explicit bake is the only bake.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/sigillum_aemeth.cpp \
//       --frame /tmp/sigillum_aemeth.png
//
//   1.0 s  the two circles struck, the 40 cells cut
//   2.5 s  the solver walking Galas / Gethog — arcs leaping the rim
//   9.0 s  all seven Names assembled; the seven unvisited cells go dark
//  12.0 s  the seven birds: the rows land, the columns light, the
//          archangels print out of them
//  20.0 s  the three systems counter-rotating into their one alignment
//  26 s loops.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>
#include <sigilweave/fonts/FontContext.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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
using sigil::weave::ports::pickTypeface;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

using sigil::compose::hex;  // 0xRRGGBB (+ optional alpha) → SkColor4f

// ---------------------------------------------------------------------------
// palette — beeswax, four centuries old, under museum light: one hue and
// many depths. Taken off the British Museum photographs of 1838,1232.90.a
// (the large disc) and .c (the small one); the cut is not a colour, it is a
// shadowed wall and a lit wall.

constexpr SkColor4f kVitrine = hex(0x14161c);
// THE DISC IS WAX, and the record says which wax: a dull olive-brown
// beeswax, matte, scuffed, green-stained where the graver went in. A warm
// parchment tan with a golden sheen is new vellum, and on new vellum a
// line is drawn; on wax it is CUT.
constexpr SkColor4f kWaxDeep = hex(0x5c4c26);
constexpr SkColor4f kWaxMid = hex(0x7d6a37);
constexpr SkColor4f kWaxLit = hex(0x9c8949);
constexpr SkColor4f kWaxPale = hex(0xb3a267);
constexpr SkColor4f kCutDark = hex(0x2b2210);  // the groove's floor
constexpr SkColor4f kCutLite = hex(0xc4b485);  // the wall that catches light
constexpr SkColor4f kInk = hex(0x2b2118);
constexpr SkColor4f kInkSoft = hex(0x6a5a42);
constexpr SkColor4f kRubric = hex(0x8c2f22);
constexpr SkColor4f kTrace = hex(0x1f6f9c);
constexpr SkColor4f kGold = hex(0xb8862c);
constexpr SkColor4f kVellum = hex(0xece1c8);

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
const char* kAngles[7][7] = {{"Z", "l", "l", "R", "H", "i", "a"},
                             {"a", "Z", "C", "a", "a", "c", "b"},
                             {"p", "a", "u", "p", "n", "h", "r"},
                             {"h", "d", "m", "h", "i", "a", "i"},
                             {"k", "k", "a", "a", "e", "e", "e"},
                             {"i", "i", "e", "e", "l", "l", "l"},
                             {"e", "e", "l", "l", "M", "G", "\xe2\x80\xa0"}};
const char* kArchangels[7] = {"Zaphkie", "l Zadkie", "l Cumael", " Raphael",
                              " Haniel", "M ichael", "G abriel"};

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
const char* kFiliaeLucis[7] = {"El",    "Me",     "Ese",    "Iana",
                               "Akele", "Azdobn", "Stimcul"};
const char* kFiliiLucis[7] = {"I",   "Heeoa",   "Ih",  "Beigia",
                              "Ilr", "Stimcul", "Dmal"};
const char* kFiliaeFil[7] = {"S",     "Ab",     "Ath",    "Ized",
                             "Ekiei", "Madimi", "Esemeli"};
const char* kFiliiFil[7] = {"*",     "An",      "Ave",    "Liba",
                            "Rocle", "Hagonel", "Ilemese"};

const char* kZabathiel[7] = {"Z", "A", "B", "A", "T", "H", "I*"};

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

std::string dedupeAA(const std::vector<std::string>& g) {
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

Solved walkFrom(int start1) {
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

SkPoint heptVertex(int k, float rNorm) {
  return P((float)k * 360.0f / 7.0f, rNorm);
}

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

Weave buildWeave(float rNorm) {
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
    const bool over =
        (w.rule.decide(c) == path::Order::Over) == pass.isA;
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
shapes::OutlineFn wobbled(shapes::OutlineFn base, uint32_t seed,
                          float seg = 26.0f, float dev = 0.34f) {
  return [base = std::move(base), seed, seg, dev](SkSize s) {
    return shapers::Jitter{seg, dev, seed}.shape(base(s));
  };
}

// ---------------------------------------------------------------------------
// paint helpers

// A positional shorthand over type. Every text run on this plate is
// built from the same four fields, and there are hundreds of call sites.
weaveNs::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
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

template <typename... A>
std::string fmt(const char* f, A... args) {
  char buf[512];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::snprintf(buf, sizeof(buf), f, args...);
#pragma clang diagnostic pop
  return buf;
}

}  // namespace

// ===========================================================================

struct SigillumAemeth : sketch::Sketch {
  sk_sp<SkTypeface> faceSeal, faceRing, faceQuill, faceSerif, faceItalic,
      faceMono, faceDisplay;

  // ONE Output turns three systems that cannot agree except at 12 o'clock
  ch::Output<float> settle{0.0f};

  feed::TextRing logA{64}, logB{64}, logC{64}, logD{64};
  Weave weave;
  std::array<Solved, 7> solved;
  std::array<bool, 40> visited{};
  int usedCells = 0;
  double clockT = 0;
  int solvePhase = -2;

  Paint waxGrain;
  Pattern waxSpeck;

  // --- the reading order, in seconds ---------------------------------------
  static constexpr float tPlate = 0.05f;
  static constexpr float tCells = 0.35f;
  static constexpr float tInner = 1.05f;
  static constexpr float tSolve = 2.00f;
  static constexpr float tSolveEach = 0.92f;
  static constexpr float tDark = tSolve + 7 * tSolveEach + 0.35f;  // ~8.8
  static constexpr float tBirds = 9.6f;
  static constexpr float tSpin = 15.4f;

  // =========================================================================
  // THE WAX

  Element waxGround() {
    auto g = box().inset(0);

    // the cake: rim, body, and the tool-marks of a warm knife
    g.child(kit::disc(SkPoint{kRR, kRR}, kWaxEdge * 1.055f * kR)
                .shape(shapes::annulus(0.90f))
                .fill(Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                                           {{0.0f, hex(0x05070a, 0.0f)},
                                            {0.905f, hex(0x05070a, 0.0f)},
                                            {0.945f, hex(0x05070a, 0.62f)},
                                            {1.0f, hex(0x05070a, 0.0f)}}))
                .translateX(5)
                .translateY(11)
                .key("waxshadow"));
    g.child(kit::disc(SkPoint{kRR, kRR}, kWaxEdge * kR)
                .shape(shapes::circle())
                .fill(Paint::blend(
                    {{Paint::radialUnit({0.42f, 0.36f}, 1.05f,
                                           {{0.0f, kWaxPale},
                                            {0.45f, kWaxLit},
                                            {0.82f, kWaxMid},
                                            {1.0f, kWaxDeep}}),
                      SkBlendMode::kSrcOver},
                     {waxGrain, SkBlendMode::kOverlay},
                     {waxSpeck.material(), SkBlendMode::kMultiply}}))
                .foreground(lines::hatch(Fill::color(hex(0x6d5228, 0.10f)),
                                         11.0f, 0.9f, -24.0f))
                .foreground(PathFormat{
                    .width = 9.0f,
                    .strokeFill = grooveFill(kWaxEdge * kR, 9.0f, 0.55f, 0.42f),
                    .align = PathFormat::Align::Inner})
                .cache(Cache::Texture)
                .key("wax"));

    // the burnish left by the shew-stone. A ball of quartz stood on the
    // middle of this figure for its whole working life.
    g.child(kit::disc(SkPoint{kRR, kRR}, 0.33f * kR)
                .shape(shapes::circle())
                .fill(Paint::radialUnit({0.42f, 0.38f}, 1.0f,
                                           {{0.0f, hex(0xfff6dd, 0.34f)},
                                            {0.55f, hex(0xffeec6, 0.14f)},
                                            {1.0f, hex(0x000000, 0.0f)}}))
                .blend(SkBlendMode::kScreen)
                .key("shew"));
    g.child(
        kit::disc(SkPoint{kRR, kRR}, 0.335f * kR)
            .shape(shapes::circle())
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
   *  their own centre, so they stay OUT of the turning layer. Inside it they
   *  would be resampled every frame of the settle and look identical. */
  Element circumferenceRules() {
    auto g = box().inset(0);

    // the greatest Circle and its hairline companion — heavy OUTSIDE,
    // hair INSIDE. lines::Line cannot say this; lines::Rails can.
    auto rule = [&](float rNorm, float heavy, float hair, float gap,
                    const char* key, bool dotted) {
      Element e = kit::disc(SkPoint{kRR, kRR}, rNorm * kR)
                      .shape(wobbled(shapes::circle(), 1582))
                      .fill(Fill::none())
                      .key(key);
      std::vector<lines::Rail> set;
      set.push_back({.across = 0.0f,
                     .width = heavy,
                     .fill = grooveFill(rNorm * kR, heavy, 0.95f, 0.55f)});
      lines::Rail inner{.across = -gap,
                        .width = hair,
                        .fill = Fill::color(hex(0x4a3418, 0.72f))};
      if (dotted) inner.dash = {1.2f, 5.0f};
      set.push_back(inner);
      lines::Rails rails = lines::rails(std::move(set));
      rails.offsetStep = 7.0f;
      e.stroke(std::move(rails));
      e.mask(by::spans(
          spans::upTo(animate(from(0.0f).to(1.0f), ramp(tPlate * 1000, 900)))));
      return e;
    };
    g.child(kit::disc(SkPoint{kRR, kRR}, rGreat * kR)
                .shape(shapes::annulus(rBandIn / rGreat))
                .fill(Fill::none())
                .foreground(lines::RadialHatch{
                    .strokeFill = Fill::color(hex(0x6d5228, 0.11f)),
                    .spokes = 320,
                    .rings = 0,
                    .width = 0.8f,
                    .holeFraction = rBandIn / rGreat})
                .opacity(animate(from(0.0f).to(1.0f), ramp(tCells * 1000, 700)))
                .key("bandhatch"));
    g.child(rule(rGreat, 5.6f, 1.2f, 11.0f, "great", false));
    g.child(rule(rBandIn, 3.4f, 0.9f, -8.0f, "second", true));

    // The compass pricks. Forty divisions are not measured, they are
    // STEPPED round with dividers, and the point leaves a mark at every
    // step. The inner circle's contour starts due east, which is a cell
    // CENTRE, so an Interval scatter with no phase lands its first stamp
    // half a cell along — exactly on the boundary — and every one after.
    const float step = 2.0f * SK_FloatPI * rBandIn * kR / 40.0f;
    g.child(kit::disc(SkPoint{kRR, kRR}, rBandIn * kR)
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(brush::Scatter{
                    .art = box()
                               .width(5)
                               .height(5)
                               .shape(shapes::polygon(4))
                               .fill(Fill::color(hex(0x2c1c06, 0.85f))),
                    .spacing = step,
                    .alignToPath = true,
                    .reach = 8.0f})
                .key("pricks"));
    return g;
  }

  /** What the 40 divisions actually are: the cells, and only the cells. */
  Element circumferenceCells() {
    auto g = box().inset(0).transformOrigin(0.5f, 0.5f);

    // the 40 radial dividers: INTERRUPTED rules that stop short of both
    // circles. One node, forty contours, one trim window on the stroke.
    g.child(
        box()
            .inset(0)
            .shape([](SkSize) {
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
            .opacity(animate(from(0.0f).to(1.0f), ramp(tCells * 1000, 620)))
            .key("dividers"));

    // the letters, upright-radial, and their numbers above or below
    const weaveNs::TextStyle letStyle =
        type(faceRing, 0.060f * kR, hex(0x241603, 1.0f));
    const weaveNs::TextStyle numStyle =
        type(faceRing, 0.031f * kR, hex(0x4a3210, 1.0f));
    for (int i = 0; i < 40; ++i) {
      const Cell& c = kRing[(size_t)i];
      const float th = (float)i * 9.0f;
      const float f = frac(th);
      const bool dim = !visited[(size_t)i];
      auto cellLetter =
          text(toU8(c.glyph), letStyle)
              .width(Dim(2 * rCellLet * kR))
              .height(Dim(2 * rCellLet * kR))
              .centerAt({kRR, kRR})
              .key("cl" + std::to_string(i))
              .onPath(TextPath{.path = shapes::circle(),
                               .at = f,
                               .align = TextPath::Align::Center,
                               .offset = 0.0f,
                               .autoFlip = false,
                               .orient = TextPath::Orient::Radial});
      if (dim)
        cellLetter.opacity(
            animate(to(0.30f), ramp(tDark * 1000 + (float)i * 9, 700)));
      g.child(std::move(cellLetter));
      if (c.number > 0) {
        const float rr = c.step > 0 ? rNumOut : rNumIn;
        g.child(text(toU8(std::to_string(c.number)), numStyle)
                    .width(Dim(2 * rr * kR))
                    .height(Dim(2 * rr * kR))
                    .centerAt({kRR, kRR})
                    .key("cn" + std::to_string(i))
                    .onPath(TextPath{.path = shapes::circle(),
                                     .at = f,
                                     .align = TextPath::Align::Center,
                                     .offset = 0.0f,
                                     .autoFlip = false,
                                     .orient = TextPath::Orient::Radial}));
      }
    }
    return g;
  }

  // =========================================================================
  // THE SEVEN ANGLES — 7 rows of 7 letters on the sides of a heptagon, with
  // a little cross at every corner of the segments they sit in.

  Element angles() {
    auto g = box().inset(0).transformOrigin(0.5f, 0.5f);

    // the seven "segments of circles" — annular plates, radially hatched,
    // with brush::Pattern corner tiles: "at each corner of these
    // segments of circles, to make little Crosses."
    //
    // A CROSS TURNED 45 DEGREES IS AN X, so `cornerAlign` is stated here and
    // not left to the default. The art below is a Greek cross drawn with its
    // arms on local +x/+y, and every corner of an annular sector is a RIGHT
    // ANGLE — the radial leg meets the arc at 90 degrees — so aligning the
    // stamp to either leg (Outgoing) puts one arm along the arc and one along
    // the radius, at all four corners of all seven plates, which is what the
    // record draws. The BISECTOR of a right angle is 45 degrees off both, and
    // the same 90-fold symmetry that makes Outgoing corner-agnostic makes
    // Bisector uniformly wrong here: not a tilt to notice, a saltire. Any art
    // whose own axes carry meaning has to name the frame it was drawn in.
    Element crossTile = box()
                            .width(13)
                            .height(13)
                            .shape([](SkSize s) {
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

    auto plates = box().inset(0).cache(Cache::Texture);
    for (int k = 0; k < 7; ++k) {
      const float mid = ((float)k + 0.5f) * 360.0f / 7.0f;
      const float half = 360.0f / 7.0f * 0.5f - 1.1f;
      plates.child(
          kit::disc(SkPoint{kRR, kRR}, 0.868f * kR)
              .shape(shapes::sector(skAngle(mid - half), 2 * half,
                                    0.720f / 0.868f))
              .fill(Fill::color(hex(0xd9bd88, 0.30f)))
              .foreground(lines::RadialHatch{
                  .strokeFill = Fill::color(hex(0x6d5228, 0.13f)),
                  .spokes = 96,
                  .rings = 0,
                  .width = 0.9f,
                  .holeFraction = 0.70f})
              .stroke(Brush{}
                          .layer(PathFormat{
                              .width = 1.5f,
                              .strokeFill = Fill::color(hex(0x4a3418, 0.55f))})
                          .layer(brush::Pattern{
                              .side = sideTile,
                              .corner =
                                  brush::CornerArt{
                                      crossTile, brush::CornerAlign::Outgoing},
                              .advance = 22.0f,
                              .cornerAngleDeg = 40.0f,
                              .reach = 16.0f}))
              .key("plate" + std::to_string(k)));
    }
    g.child(std::move(plates));

    // the 49 letters: ONE text run per row on a SEVEN-CONTOUR chord path,
    // addressed by (k + 0.5)/7 of one continuous arc-length coordinate.
    const weaveNs::TextStyle angStyle =
        type(faceSeal, 0.049f * kR, hex(0x201404, 1.0f), 0.034f * kR);
    for (int k = 0; k < 7; ++k) {
      std::string row;
      for (int c = 0; c < 7; ++c) row += kAngles[k][c];
      // The entrance is a plain ramp that FINISHES, and it has to be: a
      // keyframe path that is still running counts as live volatility even
      // while its value is constant, so a long hold on one of these seven
      // text-on-path nodes would keep all of them, and their parents,
      // painting live and out of any texture for the whole hold. The birds'
      // later arrival is therefore marked by a separate cheap rule on each
      // plate rather than by holding this run's opacity.
      g.child(text(toU8(row), angStyle)
                  .inset(0)
                  .key("ang" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rAngleHept, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent}));
      // the bird lands: its angle-plate takes a rule it did not have before
      const float mid2 = ((float)k + 0.5f) * 360.0f / 7.0f;
      const float half2 = 360.0f / 7.0f * 0.5f - 1.1f;
      g.child(kit::disc(SkPoint{kRR, kRR}, 0.868f * kR)
                  .shape(shapes::sector(skAngle(mid2 - half2), 2 * half2,
                                        0.720f / 0.868f))
                  .fill(Fill::none())
                  .stroke(PathFormat{
                      .width = 2.2f,
                      .strokeFill = Fill::color(hex(0x402c10, 0.85f)),
                      .align = PathFormat::Align::Inner})
                  .opacity(animate(from(0.0f).to(1.0f),
                                   ramp(tBirds * 1000 + (float)k * 260, 420)))
                  .key("birdlit" + std::to_string(k)));
    }
    return g;
  }

  // =========================================================================
  // THE HEPTAGON and the seven Names of God, written along its sides with a
  // quill — the seven sides come out at seven weights from one nib.

  Element heptagonNames() {
    auto g = box().inset(0).transformOrigin(0.5f, 0.5f);

    g.child(box()
                .inset(0)
                .shape(wobbled(heptChords(rHept, 0.0f), 30, 30.0f, 0.45f))
                .fill(Fill::none())
                .stroke(Brush{}
                            .layer(brush::calligraphic(
                                34.0f, 6.8f, Fill::color(hex(0x291a05, 0.95f)),
                                0.22f))
                            .layer(PathFormat{
                                .width = 0.9f,
                                .strokeFill = Fill::color(hex(0xf7e9c4, 0.35f)),
                                .trimStart = 0.0f,
                                .trimEnd = 1.0f}))
                .key("heptrule"));

    // the second, inner heptagon rule — the Names sit between the two
    g.child(
        box()
            .inset(0)
            .shape(heptChords(rNameHept - 0.043f, 0.0f))
            .fill(Fill::none())
            .stroke(lines::rails({{.across = 0.0f,
                                   .width = 2.2f,
                                   .fill = Fill::color(hex(0x4a3418, 0.72f))},
                                  {.across = -5.0f,
                                   .width = 0.8f,
                                   .fill = Fill::color(hex(0x4a3418, 0.45f)),
                                   .dash = {1.4f, 4.6f}}}))
            .key("heptrule2"));

    const weaveNs::TextStyle nameStyle =
        type(faceQuill, 0.048f * kR, hex(0x201404, 1.0f), 0.026f * kR);
    for (int k = 0; k < 7; ++k) {
      std::string row;
      for (auto gl : kGodNames[(size_t)k].glyphs) {
        row += (gl[0] == '*') ? "\xc9\x9b" : gl;  // the 21/8 ligature stands in
      }
      g.child(text(toU8(row), nameStyle)
                  .inset(0)
                  .key("god" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rNameHept, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent}));
      // the Latin marginal reading, inside the heptagon, smaller
      g.child(text(toU8(kGodNames[(size_t)k].gloss),
                   type(faceItalic, 0.022f * kR, hex(0x53380f, 0.88f)))
                  .inset(0)
                  .key("gloss" + std::to_string(k))
                  .onPath(TextPath{.path = heptChords(rNameHept - 0.056f, 0.0f),
                                   .at = ((float)k + 0.5f) / 7.0f,
                                   .align = TextPath::Align::Center,
                                   .offset = 0.0f,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent}));
    }
    return g;
  }

  // =========================================================================
  // THE HEPTAGRAM — an interlaced band over seven DISCOVERED crossings.
  //
  // The over/under is the geometry library's crossing rule: the crossings
  // are found by path intersection, the order is `crossing::alternateAlong()`
  // prepared against them — see Weave — and the region
  // repainted at each knot is `crossingPatch`, the EXACT lens where the two
  // marks overlap. A disc is the wrong shape there: two bands meeting at a
  // shallow angle overlap in a long lens whose extent goes as
  // reach/sin(theta), and a disc sized for the perpendicular case leaves
  // the under-band showing straight across the over-band's cut.

  Element heptagram() {
    Weave w = weave;
    PaintProgram paint = [w](SkCanvas& c, const PaintContext& ctx) {
      const float bandW = 0.038f * kR;
      auto seg = [&](int i) {
        SkPathBuilder b;
        b.moveTo(w.v[i]);
        b.lineTo(w.v[(i + 1) % 7]);
        return b.detach();
      };
      // ONE LIMB, CUT INTO THE WAX. A groove is a floor with two walls:
      // under a light from the upper left the far wall catches it and the
      // near one is in shadow, so the two rails are UNEQUAL and the band
      // between them is DARKER than the surface, not lighter. Drawn the
      // other way round — a pale band with a dark line either side and a
      // shadow cast onto its neighbour — the same geometry reads as a
      // batten laid on top, and the seal stops being engraved.
      auto limb = [&](SkCanvas& cv, int i, bool over) {
        const SkPath p = seg(i);
        SkPaint body;
        body.setAntiAlias(true);
        body.setStyle(SkPaint::kStroke_Style);
        body.setStrokeWidth(bandW);
        body.setStrokeCap(SkPaint::kButt_Cap);
        body.setColor4f(kCutDark, nullptr);
        cv.drawPath(p, body);
        decorations::paintOn(
            cv, ctx, p,
            lines::rails(
                {// the lit wall
                 {.across = bandW * 0.5f - 1.2f,
                  .width = 2.4f,
                  .fill = Fill::color(kCutLite)},
                 // the shadowed wall, and the lip of wax pushed up beside it
                 {.across = 1.2f - bandW * 0.5f,
                  .width = 2.4f,
                  .fill = Fill::color(hex(0x1a1409, 0.85f))},
                 {.across = 1.6f - bandW * 0.5f - 2.4f,
                  .width = 1.0f,
                  .fill = Fill::color(hex(0xbfae76, 0.45f))}}));
        (void)over;
      };
      for (int i = 0; i < 7; ++i) limb(c, i, false);
      // THE WEAVE, CUT. At a crossing the graver takes the over-limb
      // through and stops the under-limb's walls short of it, so the
      // interlace is in the cut rather than in a cast shadow: redraw the
      // over limb clipped to the lens where the two marks actually overlap,
      // and its floor and walls close over the other's. The cap is half the
      // arc distance to the next crossing on the same strand, which is what
      // stops two neighbouring lenses merging into one contour and handing
      // the whole run to a single strand.
      const path::CrossingRule& rule = w.rule;
      const float reach = bandW * 0.5f + 1.6f;
      for (const path::Crossing& x : w.crossings) {
        const size_t over =
            rule.decide(x) == path::Order::Over ? x.a : x.b;
        const size_t under = over == x.a ? x.b : x.a;
        c.save();
        c.clipPath(path::crossingPatch(w.strands[over], reach,
                                       w.strands[under], reach, x.at,
                                       w.reachCap[x.index]),
                   true);
        limb(c, (int)over, true);
        c.restore();
      }
    };
    return box().inset(0).background(paint).key("weave");
  }

  // =========================================================================
  // the concentric cell rules, the four orders of the Children of Light,
  // ZABATHIEL's heptagon, the pentagram, the cross.

  /** Concentric circles and a crosshatched annular recess: also invariant
   *  under rotation, also lifted out of the turning layer. */
  Element innerRings() {
    auto g = box().inset(0);

    // the deepest recesses — crosshatched wax between the star's limbs
    g.child(box()
                .inset(0)
                .shape([](SkSize) {
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
      g.child(
          kit::disc(SkPoint{kRR, kRR}, rr * kR)
              .shape(wobbled(shapes::circle(), (uint32_t)(7 + i), 22.0f, 0.30f))
              .fill(Fill::none())
              .stroke(
                  spans::upTo(
                      animate(from(0.0f).to(1.0f),
                              ramp(tInner * 1000 + 200 + (float)i * 90, 620))),
                  PathFormat{.width = i == 4 ? 2.4f : 1.5f,
                             .strokeFill = grooveFill(
                                 rr * kR, i == 4 ? 2.4f : 1.5f, 0.50f, 0.34f)})
              .key("ring" + std::to_string(i)));
    }
    return g;
  }

  Element inner() {
    auto g = box().inset(0);

    // The four orders, each with the tablet the record gives it: an
    // arc-segment worn in the forehead, a round gold plate on the breast,
    // a four-square white ivory, a three-cornered green. Four silhouettes,
    // cut in the same wax as everything else — not four tinted rectangles.
    struct Order {
      const char* const* names;
      float radius;
      int shape;  // 0 arc-segment, 1 disc, 2 four-square, 3 three-cornered
      SkColor4f rule;
      float size;
    };
    const Order orders[4] = {
        {kFiliaeLucis, rFiliaeLucis, 0, hex(0x2c1c06, 0.95f), 0.027f},
        {kFiliiLucis, rFiliiLucis, 1, hex(0x2c1c06, 0.95f), 0.025f},
        {kFiliaeFil, rFiliaeFil, 2, hex(0x2c1c06, 0.95f), 0.024f},
        {kFiliiFil, rFiliiFil, 3, hex(0x2c1c06, 0.95f), 0.023f}};
    const SkColor4f kTabletFace[4] = {
        hex(0xe4cd9e, 0.62f), hex(0xecd7a8, 0.66f), hex(0xe8d2a2, 0.60f),
        hex(0xdfc793, 0.58f)};

    for (int o = 0; o < 4; ++o) {
      const Order& ord = orders[o];
      for (int k = 0; k < 7; ++k) {
        const float th = (float)k * 360.0f / 7.0f;
        const float em = 0.034f * kR;            // the tablet itself
        const float rTab = ord.radius - 0.032f;  // it sits below the name
        const SkPoint at = P(th, rTab);
        Element tablet =
            box()
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
          tablet.shape(shapes::circle());
        else if (ord.shape == 3)
          tablet.shape(shapes::polygon(3, 180.0f));
        else if (ord.shape == 0)
          tablet.shape(shapes::sector(skAngle(th - 5.2f), 10.4f, 0.905f))
              .width(Dim(2 * rTab * 1.05f * kR))
              .height(Dim(2 * rTab * 1.05f * kR))
              .centerAt({kRR, kRR})
              .rotate(0.0f);
        g.child(std::move(tablet));

        const std::string nm = ord.names[k];
        g.child(text(toU8(nm == "*" ? "E\xc9\x9b" : nm),
                     type(faceSeal, ord.size * kR, hex(0x201404, 1.0f)))
                    .width(Dim(2 * ord.radius * kR))
                    .height(Dim(2 * ord.radius * kR))
                    .centerAt({kRR, kRR})
                    .key("chl" + std::to_string(o * 7 + k))
                    .onPath(TextPath{.path = shapes::circle(),
                                     .at = frac(th),
                                     .align = TextPath::Align::Center,
                                     .offset = 0.0f,
                                     .autoFlip = false,
                                     .orient = TextPath::Orient::Radial}));
      }
    }

    // ZABATHIEL — "this name must be distributed in his letters into 7 sides
    // of that innermost Heptagonum. So have you just 7 places."
    g.child(
        box()
            .inset(0)
            .shape(heptChords(rInnerHept, 0.0f))
            .fill(Fill::none())
            .stroke(lines::rails({{.across = 0.0f,
                                   .width = 2.2f,
                                   .fill = Fill::color(hex(0x3f2c12, 0.88f))},
                                  {.across = 4.0f,
                                   .width = 0.7f,
                                   .fill = Fill::color(hex(0xfbf0d0, 0.40f))}}))
            .key("zabhept"));
    for (int k = 0; k < 7; ++k) {
      const std::string s = kZabathiel[k];
      g.child(
          text(toU8(s == "I*" ? "I\xc9\x9b" : s),
               type(faceSeal, 0.030f * kR, hex(0x201404, 1.0f)))
              .inset(0)
              .key("zab" + std::to_string(k))
              .onPath(TextPath{.path = heptChords(rInnerHept - 0.028f, 0.0f),
                               .at = ((float)k + 0.5f) / 7.0f,
                               .align = TextPath::Align::Center,
                               .offset = 0.0f,
                               .autoFlip = false,
                               .orient = TextPath::Orient::Tangent}));
    }
    return g;
  }

  static constexpr float kHp = 0.245f * kR;  // the pentagram's own half-box

  Element pentagram() {
    auto g = box()
                 .rect(SkRect::MakeXYWH(kRR - kHp, kRR - kHp, 2 * kHp, 2 * kHp))
                 .transformOrigin(0.5f, 0.5f);
    // "Set Z, of Zedekieil within the angle which standeth up toward the
    // begynning of the greatest Circle" — point-up, aligned on division 1.
    g.child(
        kit::disc(SkPoint{kHp, kHp}, rPenta * kR)
            .shape(wobbled(shapes::star(5, 0.382f), 5, 16.0f, 0.30f))
            .fill(Fill::color(hex(0xe6cf9e, 0.18f)))
            .stroke(lines::rails({{.across = 0.0f,
                                   .width = 3.0f,
                                   .fill = Fill::color(hex(0x3f2c12, 0.92f))},
                                  {.across = 3.2f,
                                   .width = 0.8f,
                                   .fill = Fill::color(hex(0xfbf0d0, 0.45f))}}))
            // THE MARKS, and not the wash under them: the reveal runs on
            // the rails only, and the 18%-alpha ground is simply there
            // from the start. Sweeping a wash that faint across its own
            // background is a change too small to read as an entrance.
            .mask(parts::marks(),
                  by::spans(spans::upTo(animate(
                      from(0.0f).to(1.0f), ramp(tInner * 1000 + 500, 800)))))
            .key("penta"));
    for (int k = 0; k < 5; ++k) {
      const float th = (float)k * 72.0f;
      g.child(text(toU8(kPentaNames[(size_t)k].initial),
                   type(faceSeal, 0.052f * kR, hex(0x241704, 1.0f)))
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
                  .opacity(
                      animate(from(0.0f).to(1.0f),
                              ramp(tInner * 1000 + 900 + (float)k * 40, 420))));
      // the rest of the name runs circularly outward into the exterior angle
      g.child(text(toU8(kPentaNames[(size_t)k].tail),
                   type(faceQuill, 0.024f * kR, hex(0x40300f, 0.92f)))
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
                  .opacity(
                      animate(from(0.0f).to(1.0f),
                              ramp(tInner * 1000 + 980 + (float)k * 40, 420))));
    }
    return g;
  }

  static constexpr float kHc = 0.118f * kR;  // the cross's own half-box

  Element centreCross() {
    auto g =
        box().rect(SkRect::MakeXYWH(kRR - kHc, kRR - kHc, 2 * kHc, 2 * kHc));
    const float arm = rCross * kR;
    g.child(
        box()
            .width(Dim(2.4f * arm))
            .height(Dim(2.4f * arm))
            .centerAt({kHc, kHc})
            .shape([](SkSize s) {
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
    const struct {
      const char* s;
      float th;
      float r;
    } kArms[4] = {{"VA", 0.0f, rCross * 0.72f},
                  {"NA", 90.0f, rCross * 1.02f},
                  {"el", 180.0f, rCross * 1.02f},
                  {"LE", 270.0f, rCross * 1.02f}};
    for (int i = 0; i < 4; ++i) {
      g.child(text(toU8(kArms[i].s),
                   type(faceSeal, 0.025f * kR, hex(0x2b1d08, 1.0f)))
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
                  .opacity(animate(from(0.0f).to(1.0f),
                                   ramp(tInner * 1000 + 1200, 500))));
    }
    return g;
  }

  // =========================================================================
  // THE SOLVER, ON THE PLATE. Seven names; each hop is an arc that leaps the
  // rim, drawn on by a trim window.

  Element solverOverlay() {
    auto g = box().inset(0);
    for (int n = 0; n < 7; ++n) {
      const Solved& s = solved[(size_t)n];
      const float t0 = tSolve + (float)n * tSolveEach;
      // ONE path per Name, one CONTOUR per hop. trim() walks every contour
      // as a single arc-length coordinate, so 0 -> 1 marches the walk hop by
      // hop — the whole solve is two nodes per Name instead of sixteen.
      std::vector<SkPoint> from, to, ctrl, land;
      for (size_t h = 0; h < s.cells.size(); ++h) {
        land.push_back(P((float)(s.cells[h] - 1) * 9.0f, rCellLet));
        if (h + 1 >= s.cells.size()) break;
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
      if (solvePhase < 0 || n > solvePhase) continue;
      const bool live = (n == solvePhase);
      // `from` is the walk's own point list here, so the entrance factory
      // needs its namespace.
      auto fade = [&](float) -> Animatable<float> {
        return live ? Animatable<float>(animate(
                          motion::from(0.0f).to(1.0f), ramp(0, 220)))
                    : Animatable<float>(0.15f);
      };
      auto reveal = [&](float ms) -> Animatable<float> {
        return live ? Animatable<float>(animate(
                          motion::from(0.0f).to(1.0f), ramp(0, ms)))
                    : Animatable<float>(1.0f);
      };
      g.child(box()
                  .inset(0)
                  .shape([from, to, ctrl](SkSize) {
                    SkPathBuilder b;
                    for (size_t i = 0; i < from.size(); ++i) {
                      b.moveTo(from[i]);
                      b.quadTo(ctrl[i], to[i]);
                    }
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(spans::upTo(reveal(760.0f)),
                          lines::Line{.width = 2.6f,
                                      .fill = Fill::color(hex(0x7fd0f4, 0.95f)),
                                      .endCap = lines::Cap::Arrow,
                                      .capSize = 15.0f})
                  .opacity(fade(0))
                  .key("hops" + std::to_string(n)));
      g.child(box()
                  .inset(0)
                  .shape([land](SkSize) {
                    SkPathBuilder b;
                    for (const SkPoint& q : land)
                      b.addCircle(q.fX, q.fY, 0.030f * kR);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(spans::upTo(reveal(820.0f)),
                          PathFormat{
                              .width = 2.0f,
                              .strokeFill = Fill::color(hex(0x59b6e8, 0.92f))})
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
                 .rect(SkRect::MakeXYWH(1660 * kS, 56 * kS, w, 1588))
                 .scale(kS)
                 .transformOrigin(0.0f, 0.0f);

    g.child(text(toU8("SIGILLVM DEI \xc3\x86M\xc3\x86TH"),
                 type(faceDisplay, 46, kVellum, 2.6f))
                .at({0, 0}));
    g.child(
        text(toU8("EMETH nuncupatum \xc2\xb7 Mortlake by Richemond \xc2\xb7 "
                  "21 Martii 1582"),
             type(faceItalic, 19, hex(0xc7ab74)))
            .at({2, 58}));
    g.child(
        text(toU8("BL Sloane MS 3188 f.30r \xc2\xb7 wax disc BM 1838,1232.90.a "
                  "\xc2\xb7 23.2 cm"),
             type(faceSerif, 15, hex(0x8d7a58)))
            .at({2, 86}));
    g.child(
        box()
            .rect(SkRect::MakeXYWH(0, 114, w, 2))
            .fill(Fill::none())
            .shape([w](SkSize) {
              SkPathBuilder b;
              b.moveTo(0, 1);
              b.lineTo(w, 1);
              return b.detach();
            })
            .stroke(lines::rails({{.across = 0.0f,
                                   .width = 2.4f,
                                   .fill = Fill::color(hex(0xc7ab74, 0.75f))},
                                  {.across = -5.0f,
                                   .width = 0.8f,
                                   .fill = Fill::color(hex(0xc7ab74, 0.40f)),
                                   .dash = {2.0f, 5.0f}}})));

    // the seven Names, printing as the walk finds them
    g.child(text(toU8("THE SEVEN NAMES, WALKED OFF THE RIM"),
                 type(faceMono, 15, kRubric, 1.6f))
                .at({0, 136}));
    g.child(box()
                .rect(SkRect::MakeXYWH(0, 158, w, 324))
                .shape([w](SkSize) {
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
      const Solved& s = solved[(size_t)n];
      const float y = 166 + (float)n * 46;
      const float at = (tSolve + (float)n * tSolveEach) * 1000;
      std::string chain;
      for (size_t i = 0; i < s.cells.size(); ++i)
        chain += (i ? "\xc2\xb7" : "") + std::to_string(s.cells[i]);
      g.child(text(toU8(std::to_string(n + 1) + "."),
                   type(faceMono, 17, hex(0x8d7a58)))
                  .at({0, y + 6})
                  .opacity(animate(from(0.0f).to(1.0f), ramp(at, 300))));
      g.child(text(toU8(kNames[(size_t)n].name),
                   type(faceDisplay, 30, kVellum, 1.2f))
                  .at({34, y})
                  .opacity(animate(from(0.0f).to(1.0f), ramp(at + 120, 420))));
      g.child(text(toU8(s.raw == s.reduced
                            ? ""
                            : "\xe2\x9f\xa8" + s.raw + "\xe2\x9f\xa9"),
                   type(faceItalic, 15, hex(0x6f5f45)))
                  .at({212, y + 10})
                  .opacity(animate(from(0.0f).to(1.0f), ramp(at + 240, 420))));
      g.child(text(toU8(chain), type(faceMono, 14, kTrace))
                  .at({320, y + 10})
                  .opacity(animate(from(0.0f).to(1.0f), ramp(at + 60, 420))));
    }

    // the leftovers
    {
      std::string un, unl;
      for (int i = 0; i < 40; ++i)
        if (!visited[(size_t)i]) {
          un += (un.empty() ? "" : "\xc2\xb7") + std::to_string(i + 1);
          unl += kRing[(size_t)i].glyph;
        }
      g.child(
          text(toU8(fmt("%d of 40 cells consumed \xc2\xb7 %d never visited",
                        usedCells, 40 - usedCells)),
               type(faceMono, 15, hex(0x8d7a58)))
              .at({0, 492})
              .opacity(animate(from(0.0f).to(1.0f), ramp(tDark * 1000, 500))));
      g.child(text(toU8("unvisited  " + un + "   =  " + unl),
                   type(faceMono, 15, kRubric))
                  .at({0, 514})
                  .opacity(animate(from(0.0f).to(1.0f),
                                   ramp(tDark * 1000 + 200, 500))));
      g.child(
          text(toU8("\xe2\x86\xb3 the same rule reads them as YMON 22\xc2\xb7"
                    "7\xc2\xb7\x31\x33\xc2\xb7\x33\x31 and BORAOTH "
                    "26\xc2\xb7\x33\x36\xc2\xb7\x31\x39\xc2\xb7\xe2\x80\xa6"),
               type(faceItalic, 14, hex(0x6f5f45)))
              .at({0, 536})
              .opacity(
                  animate(from(0.0f).to(1.0f), ramp(tDark * 1000 + 400, 500))));
    }

    // the 7×7 square the birds delivered; read DOWN the columns
    g.child(text(toU8("SEVEN BASKETS, SEVEN BIRDS \xc2\xb7 READ DOWN"),
                 type(faceMono, 15, kRubric, 1.6f))
                .at({0, 580}));
    // The seven angles UNROLLED, not tabulated. On the plate these rows lie
    // along seven sides of a heptagon; here they lie on seven nested arcs of
    // the same fan, so a "column" is a RADIAL RAY and reading down a column
    // is reading outward — which is what the columns do on the object. A
    // leader curves from each ray to the archangel it spells.
    const float fanCx = 250.0f, fanCy = 1028.0f;
    const float fanR0 = 420.0f, fanDR = 33.0f, fanSpan = 46.0f;
    auto fanPt = [&](int row, int col, float dr) {
      const float a =
          (-fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f) * kD;
      const float rr = fanR0 - (float)row * fanDR + dr;
      return SkPoint{fanCx + rr * std::sin(a), fanCy - rr * std::cos(a)};
    };
    auto fanAngle = [&](int col) {
      return -fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f;
    };
    // the seven arcs the rows sit on — ruled first, as on a prepared sheet
    g.child(box()
                .rect(SkRect::MakeXYWH(0, 560, w, 300))
                .shape([&](SkSize) {
                  SkPathBuilder b;
                  for (int r = 0; r <= 7; ++r) {
                    const float rr = fanR0 - (float)r * fanDR + fanDR * 0.5f;
                    for (int i = 0; i <= 24; ++i) {
                      const float a = (-fanSpan * 0.54f +
                                       fanSpan * 1.08f * (float)i / 24.0f) *
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
    // the column rays light in sequence, and each drags a leader out to its
    // name
    for (int c = 0; c < 7; ++c) {
      const float delay = tBirds * 1000 + 1900 + (float)c * 90;
      const SkPoint a0 = fanPt(0, c, fanDR * 0.55f);
      const SkPoint a1 = fanPt(6, c, -fanDR * 0.55f);
      const SkPoint nameAt{452.0f, 612.0f + (float)c * 33.0f};
      g.child(box()
                  .inset(0)
                  .shape([a0, a1](SkSize) {
                    SkPathBuilder b;
                    b.moveTo(a0);
                    b.lineTo(a1);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(lines::rails(
                      {{.across = 19.0f,
                        .width = 0.9f,
                        .fill = Fill::color(hex(0x62b0dc, 0.60f))},
                       {.across = -19.0f,
                        .width = 0.9f,
                        .fill = Fill::color(hex(0x62b0dc, 0.60f))}}))
                  .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 360))));
      g.child(
          box()
              .inset(0)
              .shape([a1, nameAt](SkSize) {
                SkPathBuilder b;
                b.moveTo(a1);
                b.quadTo({(a1.fX + nameAt.fX) * 0.5f, a1.fY - 6.0f},
                         {nameAt.fX - 8.0f, nameAt.fY + 12.0f});
                return b.detach();
              })
              .fill(Fill::none())
              .stroke(spans::upTo(
                          animate(from(0.0f).to(1.0f), ramp(delay + 120, 420))),
                      lines::Line{.width = 0.9f,
                                  .fill = Fill::color(hex(0x2f6f9c, 0.55f)),
                                  .endCap = lines::Cap::Dot,
                                  .capSize = 4.0f})
              .opacity(animate(from(0.0f).to(1.0f), ramp(delay + 120, 300))));
      g.child(
          text(toU8(kArchangels[c]), type(faceQuill, 21, hex(0xd8c08a)))
              .at({nameAt.fX, nameAt.fY})
              .opacity(animate(from(0.0f).to(1.0f), ramp(delay + 220, 360))));
    }
    // the 49 letters, one per (row, column) slot on the fan
    for (int r = 0; r < 7; ++r)
      for (int c = 0; c < 7; ++c) {
        const float delay = tBirds * 1000 + (float)r * 260 + (float)c * 34;
        const bool isCross = kAngles[r][c] == std::string("\xe2\x80\xa0");
        const SkPoint at = fanPt(r, c, 0.0f);
        g.child(text(toU8(kAngles[r][c]),
                     type(faceSeal, 23, isCross ? kRubric : kVellum))
                    .width(30)
                    .height(30)
                    .centerAt(at)
                    .rotate(fanAngle(c))
                    .textAlign(weaveNs::TextAlignment::kCenter)
                    .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 300))));
      }
    g.child(text(toU8("48 letters, and one is noted by a Cross: which maketh "
                      "the 49th."),
                 type(faceItalic, 15, hex(0x8d7a58)))
                .at({0, 840})
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tBirds * 1000 + 2600, 400))));

    // the four orders and their tablets
    const char* kLegend[4] = {
        "Fili\xc3\xa6 Lucis \xc2\xb7 blue tablet in the forehead",
        "Filii Lucis \xc2\xb7 round gold tablet on the breast",
        "Fili\xc3\xa6 Filiarum \xc2\xb7 four-square white ivory",
        "Filii Filiorum \xc2\xb7 three-cornered green"};
    const SkColor4f kLegendTint[4] = {
        hex(0xb9c6da, 0.95f), hex(0xe6bf63, 0.95f), hex(0xf7f1e2, 0.95f),
        hex(0x9dbfa2, 0.95f)};
    g.child(text(toU8("THE FOUR ORDERS OF THE CHILDREN OF LIGHT"),
                 type(faceMono, 15, kRubric, 1.6f))
                .at({0, 870}));
    for (int i = 0; i < 4; ++i) {
      Element swatch =
          box()
              .rect(SkRect::MakeXYWH(2, 898 + (float)i * 26, 16, 16))
              .fill(Fill::color(kLegendTint[i]));
      if (i == 1)
        swatch.shape(shapes::circle());
      else if (i == 3)
        swatch.shape(shapes::polygon(3));
      else if (i == 0)
        swatch.shape(shapes::sector(-100.0f, 200.0f, 0.55f));
      g.child(std::move(swatch));
      g.child(text(toU8(kLegend[i]), type(faceSerif, 15, hex(0x9d8a66)))
                  .at({28, 896 + (float)i * 26}));
    }
    return g;
  }

  // =========================================================================

  feed::TextOptions logStyle() {
    // 10.2, and the ceiling is arithmetic. The panel is 690 wide, less 24 of
    // padding, less a 14 gap either side of the 1 px divider: 318.5 px to a
    // column. This mono advances 0.62 em, so 11 pt would fit 46 characters
    // and the 47th onward would be cut with no ellipsis and no warning —
    // feed rows simply lose their tails. The longest line printed below
    // is 50 characters, so the size has to be at most 11 x 47/50.
    constexpr float kMono = 10.2f;
    feed::TextOptions s;
    s.styles = kit::tinted(faceMono, kMono, hex(0x9d8a66),
                           {{"dim", hex(0x6b5c44)},
                            {"heading", kRubric},
                            {"pass", hex(0x59b98a)},
                            {"number", hex(0x62b0dc)}});
    s.window.gap = 1.0f;
    s.window.visible = 16;
    return s;
  }

  Element consolePanel() {
    const float px = 1660 * kS, py = 1058 * kS, pw = 690, ph = 468;
    // Four feeds, two per column: the kit's console is `plate` over `feed`
    // with the voice threaded through once, which is what this panel spelled
    // out by hand.
    return kit::console(
               {.feeds = {&logA, &logC, &logB, &logD},
                .style = logStyle(),
                .stacked = 2,
                .stackGap = 6,
                .plate = {.paddingX = 12,
                          .paddingY = 8,
                          .gap = 14,
                          .fill = Fill::color(hex(0x1b1e26, 0.86f)),
                          .border = Fill::color(hex(0xc7ab74, 0.22f)),
                          .divider = Fill::color(hex(0xc7ab74, 0.16f))}})
        .rect(SkRect::MakeXYWH(px, py, pw, ph))
        .scale(kS)
        .transformOrigin(0.0f, 0.0f);
  }

  Element colophon() {
    auto g = box()
                 .rect(SkRect::MakeXYWH(1660 * kS, 1552 * kS, 690, 120))
                 .scale(kS)
                 .transformOrigin(0.0f, 0.0f);
    g.child(
        box()
            .rect(SkRect::MakeXYWH(0, 0, 690, 2))
            .shape([](SkSize) {
              SkPathBuilder b;
              b.moveTo(0, 1);
              b.lineTo(690, 1);
              return b.detach();
            })
            .fill(Fill::none())
            .stroke(lines::rails({{.across = 0.0f,
                                   .width = 1.8f,
                                   .fill = Fill::color(hex(0xc7ab74, 0.55f))},
                                  {.across = -4.0f,
                                   .width = 0.7f,
                                   .fill = Fill::color(hex(0xc7ab74, 0.30f)),
                                   .dash = {1.6f, 4.4f}}})));
    g.child(
        text(toU8("\xe2\x80\x9cThis is the Seale, whose Name is \xc3\x86meth: "
                  "and it is to be made of perfect wax.\xe2\x80\x9d"),
             type(faceItalic, 17, hex(0xb59a6c)))
            .left(0)
            .top(16)
            .width(690));
    g.child(text(toU8("Uriel, 14 March 1582 \xc2\xb7 reconstruction from the "
                      "rule, not a tracing \xc2\xb7 SigilCompose study"),
                 type(faceMono, 12, hex(0x6f5f45)))
                .at({0, 62}));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext&) {
    auto root = box().inset(0);

    auto seal =
        box().rect(SkRect::MakeXYWH(kCx - kRR, kCy - kRR, 2 * kRR, 2 * kRR));
    seal.child(waxGround().cache(Cache::Texture));
    seal.child(circumferenceRules());
    // The settle. The rim is the FRAME and the two inner systems turn
    // against it off ONE Output — the seven-fold body one way, the five-fold
    // heart the other. Relative motion is what the figure is about: since
    // gcd(40,7) = gcd(40,5) = gcd(7,5) = 1 the three agree in exactly one
    // direction, and arriving there is the ending. Turning the rim as well
    // would add a third full-plate layer resampled every frame and show no
    // relative motion that is not already on screen.
    seal.child(circumferenceCells());
    // the whole seven-fold system turns as one body — heptagon, its two
    // letter bands, the heptagram woven on its vertices, and everything the
    // heptagram's points contain.
    seal.child(
        box()
            .inset(0)
            .transformOrigin(0.5f, 0.5f)
            .rotate(bind(&settle).target(0.0f, -360.0f / 7.0f))
            .opacity(animate(from(0.0f).to(1.0f), ramp(tInner * 1000, 900)))
            .cache(Cache::Texture)
            .child(angles())
            .child(heptagonNames())
            .child(heptagram())
            .child(inner()));
    seal.child(innerRings());
    seal.child(pentagram()
                   .rotate(bind(&settle).target(0.0f, 72.0f))
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
    logA.append({toU8("SECUNDUS.PDF p.31 \xe2\x80\x94 THE PLATE AS VECTORS"),
                 "heading"});
    logA.append({toU8("  310 text objects; 296 after the colophon"), "dim"});
    logA.append(
        {toU8("  K\xc3\xa5sa fit, outer letter ring, 44 glyphs:"), "dim"});
    logA.append(
        {toU8("    centre (305.185, 393.529) pt   R = 257.972 pt"), "number"});
    logA.append(
        {toU8("    page bbox centre is off by (-0.82, -2.47) pt"), "dim"});
    logA.append({toU8("    residual rms 4.35 pt = 1.7% R"), "dim"});
    logA.append(
        {toU8("  9\xc2\xb0 lattice fit: phase +4.30\xc2\xb0, rms 0.81\xc2\xb0"),
         "number"});
    logA.append({toU8("  40 cells recovered: 40 letters, 33 numerals"), "dim"});
    logA.append(
        {toU8("  above/below decided by r(numeral) vs r(letter)"), "dim"});
    {
      int above = 0, below = 0, bare = 0;
      for (const Cell& c : kRing)
        (c.step > 0 ? above : c.step < 0 ? below : bare)++;
      logA.append({toU8(fmt("    %d above  %d below  %d bare   = 40", above,
                            below, bare)),
                   above + below + bare == 40 ? "pass" : "heading"});
    }

    // --- panel B: the walk -------------------------------------------------
    logB.append(
        {toU8("MICHAEL'S JUMP RULE, WALKED FROM ALL 40 CELLS"), "heading"});
    for (int n = 0; n < 7; ++n) {
      const Solved& s = solved[(size_t)n];
      const bool ok = s.reduced == kNames[(size_t)n].name ||
                      s.raw == kNames[(size_t)n].name;
      std::string chain;
      for (size_t i = 0; i < s.cells.size(); ++i)
        chain += (i ? "-" : "") + std::to_string(s.cells[i]);
      logB.append({toU8(fmt("  %-9s %-9s %s", kNames[(size_t)n].name,
                            s.raw.c_str(), chain.c_str())),
                   ok ? "pass" : "heading"});
    }
    logB.append({toU8(fmt("  cells consumed %d of 40, unvisited %d", usedCells,
                          40 - usedCells)),
                 usedCells == 33 ? "pass" : "heading"});
    {
      std::string un;
      for (int i = 0; i < 40; ++i)
        if (!visited[(size_t)i])
          un += (un.empty() ? "" : " ") + std::to_string(i + 1) +
                kRing[(size_t)i].glyph;
      logB.append({toU8("  " + un), "number"});
    }
    logB.append(
        {toU8("  Aaoth from 27 also spells Aaoth \xe2\x80\x94 but leaves"),
         "dim"});
    logB.append({toU8("  32 cells. The 33/7 count picks cell 32."), "pass"});

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
      const test::Coverage cov = test::coverage(cells, reg, 320);
      const test::Coverage ref =
          test::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
      logC.append({toU8("THE 40 CELLS TILE THE ANNULUS"), "heading"});
      logC.append(
          {toU8(fmt("  doubled %d of %d samples", cov.doubled, cov.samples)),
           cov.doubled == 0 ? "pass" : "heading"});
      logC.append(
          {toU8(fmt("  uncovered %d - outside-the-ring %d = %d", cov.uncovered,
                    ref.uncovered, cov.uncovered - ref.uncovered)),
           std::abs(cov.uncovered - ref.uncovered) <= 320 ? "pass"
                                                          : "heading"});
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
      const test::Coverage cov = test::coverage(plates, reg, 320);
      const test::Coverage ref =
          test::coverage(std::span<const SkPath>(&ring, 1), reg, 320);
      logC.append({toU8("THE 7 ANGLE PLATES TILE THEIR BAND"), "heading"});
      logC.append({toU8(fmt("  doubled %d, gap vs ring %d", cov.doubled,
                            cov.uncovered - ref.uncovered)),
                   cov.doubled == 0 ? "pass" : "heading"});
      int letters = 0;
      for (const auto& angleRow : kAngles)
        for (const auto& cell : angleRow)
          if (cell != std::string("\xe2\x80\xa0")) ++letters;
      logC.append({toU8(fmt("  %d letters + 1 cross = %d places", letters,
                            letters + 1)),
                   letters == 48 ? "pass" : "heading"});
      std::string cols;
      for (int c = 0; c < 7; ++c)
        for (const auto& angleRow : kAngles)
          if (angleRow[c] != std::string("\xe2\x80\xa0")) cols += angleRow[c];
      logC.append(
          {toU8("  down the columns: " + cols.substr(0, 24)), "number"});
      logC.append({toU8("  " + cols.substr(24)), "number"});
    }

    // --- panel D: {7/2} vs {7/3}, and the weave ---------------------------
    logD.append({toU8("{7/2} OR {7/3}? THE COORDINATES DECIDE"), "heading"});
    logD.append({toU8(fmt("  {7/2} core = %.4f x Rhept = %.3f R",
                          (double)kStar72, (double)(kStar72 * rHept))),
                 "number"});
    logD.append({toU8(fmt("  {7/3} core = %.4f x Rhept = %.3f R",
                          (double)kStar73, (double)(kStar73 * rHept))),
                 "number"});
    logD.append(
        {toU8(fmt("  along a point's ray the core stops at %.3f R",
                  (double)(kStar72 * rHept * std::cos(3.14159265f / 7)))),
         "dim"});
    // "Filiae", not "Fili\xc3\xa6": the feed runs in the MONO face, which
    // has no ash, and a missing glyph falls back to another typeface mid-word.
    // The seal's own legend, set in the serif, keeps the ligature.
    logD.append({toU8(fmt("  Filiae/Filii Filiorum measured %.3f / %.3f R",
                          (double)rFiliaeFil, (double)rFiliiFil)),
                 "dim"});
    logD.append(
        {toU8("  both INSIDE a {7/2} core, as the record says;"), "dim"});
    logD.append(
        {toU8("  {7/3} would leave them floating mid-point and"), "dim"});
    logD.append(
        {toU8("  would collide with ZABATHIEL at 0.285 R.  {7/2}."), "pass"});
    logD.append({toU8("THE INTERLACE"), "heading"});
    // discoverCrossings reports one crossing per POINT, so a {7/2} star
    // yields n(k-1) = 7 of them; the 14 passes are the same points counted
    // from both strands. The shared vertices are MEETINGS, not crossings,
    // and never appear.
    // The column is narrow, so the two claims are set at its own widths
    // rather than at the table's default.
    test::report(logD,
                 measure::check("  crossings discovered", size_t{7},
                                weave.crossings.size()),
                 "pass", "heading", 30, 4);
    test::report(logD,
                 measure::check("  passes out of alternation", 0,
                                weave.outOfAlternation),
                 "pass", "heading", 30, 4);
    logD.append(
        {toU8("  gcd(40,7)=gcd(40,5)=gcd(7,5)=1 \xe2\x80\x94 one"), "dim"});
    logD.append({toU8("  alignment only, and it is 12 o'clock."), "pass"});
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kVitrine);
    // The plate is the finished object, and this piece arrives in stages: the
    // last Name is solved at tSolve + 7 tSolveEach, the birds' square lands
    // 2.6 s after tBirds, and the rings start turning at tSpin. Only the
    // window between the two shows every part of it at once.
    ctx.captureAt(14.0);

    // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
    // library's own walk: the first installed family wins, and a machine
    // with none of them gets the default face AT THE WEIGHT ASKED FOR
    // rather than silently at Normal.
    faceSerif = pickTypeface({"Hoefler Text", "Baskerville"});
    faceItalic =
        pickTypeface({"Hoefler Text", "Baskerville"}, SkFontStyle::Italic());
    faceMono = pickTypeface({"Menlo", "Courier New"});
    faceSeal = pickTypeface({"Herculanum", "Optima", "Baskerville"});
    faceRing = pickTypeface({"Trattatello", "Hoefler Text", "Baskerville"},
                        SkFontStyle::Italic());
    faceQuill =
        pickTypeface({"Hoefler Text", "Baskerville"}, SkFontStyle::Italic());
    faceDisplay = pickTypeface({"Luminari", "Herculanum", "Optima", "Baskerville"});

    waxGrain = Paint::recipe(field::grain(1.6f, 4, 1582.0f, 0.34f));
    waxSpeck = patterns::speckle(520, 18, 1.4f, 4.4f, {skia::toColor(hex(0x6a4a20, 0.10f))});
    waxSpeck.seed(1582);

    // solve, then draw what the solver said
    for (int n = 0; n < 7; ++n)
      solved[(size_t)n] = walkFrom(kNames[(size_t)n].start);
    visited.fill(false);
    for (const Solved& s : solved)
      for (int c : s.cells) visited[(size_t)(c - 1)] = true;
    usedCells = 0;
    for (bool v : visited) usedCells += v ? 1 : 0;
    weave = buildWeave(rHept);
    runChecks();

    // ONE Output: three systems turn off it and stop together at 0.
    ctx.ticker.add([this](double dt) {
      clockT += dt;
      const double loop = 26.0;
      const double t = std::fmod(clockT, loop);
      // the rings turn from tSpin, decelerating into the one alignment
      float s = 0.0f;
      if (t > (double)tSpin && t < 24.0) {
        const double u = (t - (double)tSpin) / (24.0 - (double)tSpin);
        // ease out from 1 turn to none
        s = (float)((1.0 - u) * (1.0 - u) * (1.0 - u));
      }
      settle = s;
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  /** The one discrete state this sketch has: which Name the solver is
   *  currently walking. Everything else on the plate is continuous and rides
   *  the ticker's Outputs, so it is described once and never re-described.
   *
   *  The walk cannot: each Name is a different path with a different contour
   *  count, so advancing it means new geometry. That is what slot() is for —
   *  `renderSlot` rebuilds ONLY the solver overlay, a handful of nodes, and
   *  leaves the rest of the tree (and its baked layers) untouched. A full
   *  render() to advance one hop would rebuild the whole seal. */
  void update(double, sketch::SketchContext& ctx) override {
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

SIGIL_SKETCH(SigillumAemeth, "Study \xc2\xb7 Esoteric",
             "Dee's Sigillum Dei Aemeth (1582) \xe2\x80\x94 solved from the "
             "angels' own jump rule, 33 of 40 cells")
