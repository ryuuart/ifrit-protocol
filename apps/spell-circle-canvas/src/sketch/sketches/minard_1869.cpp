// minard_1869.cpp — Charles Joseph Minard's lithographed sheet of
// 20 November 1869, BOTH cartes figuratives, and an audit of the scale
// rule the sheet prints on itself.
//
// REFERENCE
//   "Carte figurative des pertes successives en hommes de l'armée
//   française dans la campagne de Russie 1812-1813, comparée à celle
//   d'Annibal durant la 2ème guerre punique." Two-colour lithograph,
//   autographed by Regnier, printed by Regnier et Dourdet, Paris,
//   20 novembre 1869. Paper ~62 x 54 cm. Minard was 88; he died ten
//   months later during the siege of Paris.
//
//   The copy studied is MINARD'S OWN PRESENTATION COPY: BnF, Département
//   Cartes et plans, ark:/12148/btv1b52504201x, inscribed across the top
//   margin in his hand "pour la Bibliothèque impériale", with the
//   donation stamps Ge Don 4182 / DON N° 4182. IIIF render at
//   full/4000,/0/native.jpg. The familiar Wikimedia Commons scan of the
//   Russian panel alone (2003 x 955) was used for pixel measurement
//   because its tan/black/white separation is cleaner.
//   Second known original: École nationale des ponts et chaussées,
//   Fol. 10975.
//
//   Data: HistData::Minard.troops / .cities / .temp (Michael Friendly),
//   which descend from Leland Wilkinson's file for The Grammar of
//   Graphics (1990) — i.e. a DIGITISATION OF THE PLATE quantised to
//   0.1°, not independent history and not modern geography.
//   https://friendly.github.io/HistData/reference/Minard.html
//   Polybius III.35 and III.56.4 for the Hannibal chain.
//
// WHAT THIS SKETCH IS
//   The plate states its own construction rule — "à raison d'un
//   millimètre pour dix mille hommes", on BOTH panels — and that is a
//   falsifiable claim about ink on paper. This sketch prints the sheet
//   and then checks the claim, and then applies the SAME measurement to
//   its own generated geometry. A study that measures its reference has
//   no standing to report the reference is 12.6% over unless it can
//   survive the identical measurement itself. Both audits are printed.
//   One of them fails.
//
// MEASURED (mine, this file's numbers, re-derived from the scans)
//   * The band-width staircase, 11 treads on the Commons scan:
//       width_px = 3.828 px per 10,000 men, intercept −0.19 px, R² 0.9927.
//     Proportional through the origin to a fifth of a pixel.
//   * Converted: 1.126 mm per 10,000 men, not the 1.000 the legend
//     claims — 12.6% wider — and the SAME factor on the Hannibal panel,
//     drawn from different data.
//   * The retreat band hits a FLOOR at ~5.4 px = 1.57 mm. The last 4,000
//     men are drawn 2.6x too wide. This also kills the prettier finding:
//     the famous 12,000 -> 14,000 anomaly is NOT measurable in the ink,
//     because both readings sit on the floor. Negative result, printed.
//   * Geography: median 5.35 km residual over an 871 km span. The
//     received "Minard sacrificed geography" is wrong. The distortion is
//     confined to Wixma-Chjat-Mojaisk (0.591 / 1.528, total 1.011).
//   * Minard's own scale bar disagrees with his own map by 1.82x, and I
//     cannot explain it. Three hypotheses, none asserted.
//
// MEASURED HERE — the two projection scales, and one finding that is new:
//   * Napoleon panel latitude scale, least-squares on the tan band's
//     centreline at 8 stations east of Polotzk (where exactly one band
//     occupies every column): d = 280.3 px/deg on the Commons scan,
//     d/b = 2.142, R² 0.866, rms 34 px. True-to-scale at 55°N would be
//     1.743, so the Russian map STRETCHES latitude by 1.23x.
//   * THE TWO PANELS DO NOT SHARE A PROJECTION. Same fit on the
//     Hannibal band (11 stations, BnF sheet): d/b = 0.048, R² = 0.12 —
//     latitude explains an eighth of the band's vertical position, and
//     the fitted latitude scale is 5% of the longitude scale. Robust to
//     the western anchor (d/b 0.048..0.075 over lon −0.2..−3.5). The top
//     panel is a STRIP DIAGRAM, not a map. So "Minard sacrificed
//     geography" is wrong about the panel everyone quotes and right
//     about the panel nobody looks at. The two panels share the scale
//     rule for MEN and share nothing about the world.
//
// SPECULATION, LABELLED AS SUCH ON SCREEN
//   Half a French ligne is 1.1279 mm, 0.19% from the measured 1.1258.
//   Minard was born in 1781 and trained under the old units. That is a
//   hypothesis, not a finding: lithographic reduction and an error in
//   the catalogued paper size are the competing explanations, and the
//   second is the one that would kill it.
//
// EVERY ZONE ON BOTH PANELS IS ONE PRIMITIVE: a `brush::Ribbon` on the
// width Profile seam, the law read at arc length so every riser stays at
// its named city. A band is the union of its cross-sections, so the inside
// of a bend fills however tight the turn — which this sheet needs, because
// around Wilna the advance carries 340,000 men into a leg shorter than the
// band is wide, the hardest corner on the plate. `Ribbon::join` says what
// happens on the OUTSIDE of that turn and this sheet asks for the bevel,
// the chord across the corner, which is what a lithographer's overlapping
// treads leave. `Ribbon::band` hands the drawn geometry back, so the audit
// in the checks strip measures the band on the sheet rather than a
// transcription of how one is built; what it finds is printed in both px
// and millimetres of Minard's paper.
//
// The width Profile is a comparable value with a derived `max()`, so a
// band prunes like any other value and its reach is declared rather than
// guessed — which is what lets the 12.6% morph ride a live Output through
// the law without the reveal or the cull going wrong.
//
// THE LIBRARY CONSTRAINTS THIS SKETCH IS SHAPED BY:
//   * Slot names live in an index of their own — `bySlot`, populated only
//     for slot nodes, and the only index `renderSlot()` resolves a name
//     through — so a keyed element elsewhere in the tree cannot shadow a
//     slot name. This study keeps its slot names and its content keys
//     distinct anyway, so that a key always names exactly one node.
//   * `Element::outline()` is memoised on (descriptor, size), so geometry
//     cannot be a bound value the way a transform or an opacity can — which
//     is why the 12.6% morph rides the width LAW, read at paint, and not
//     the shape.
//   * There are no boolean path ops, so the Mediterranean's hachure region
//     is built as one closed polygon by hand rather than as a difference.
//
// NOT TRACED. Every band width here is survivors x 1.126 mm / 10,000
// against the sheet's own 2.258 px per millimetre; the Russian panel's
// city positions come out of the affine fit above; every printed number
// is computed in this file. The Hannibal band's stations ARE read off
// the scan, and that is forced rather than lazy: the Hannibal panel has no
// projection to fit.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/minard_1869.cpp \
//       --frame /tmp/minard_1869.png

#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathMeasure.h>
#include <include/core/SkTypeface.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

namespace skia = sigil::material::skia;
namespace field = sigil::material::field;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using sigil::weave::ports::pickTypeface;
namespace geometry = sigil::geometry;
namespace path = sigil::geometry::path;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

using sigil::compose::hex;  // hex(0xRRGGBB[, a]) -> SkColor4f, usable in
                            // constexpr

// ---------------------------------------------------------------------------
// palette — sampled by percentile over masked regions of the two scans.
// TWO worlds, deliberately kept apart: the aged artefact, and the audit.

constexpr SkColor4f kDesk = hex(0x1b1a18);  // the table the sheet lies on
constexpr SkColor4f kPaperShadow = hex(0xb9ad98);  // p10: edges, foxing
constexpr SkColor4f kPaperBody = hex(0xcbbfab);    // p50: the sheet's ground
constexpr SkColor4f kPaperLight = hex(0xd6cab6);   // p90
// "LE ROUGE", AS THE SHEET PRINTS IT. Minard names the advance zone red
// and the 1869 lithograph lays it as a pale buff ochre — a warm cream a
// shade off the paper, not a rose. A pinker, darker zone competes with
// the black retreat band instead of sitting under it, which inverts the
// sheet's whole reading: the black is the figure and the zone the ground.
constexpr SkColor4f kZoneDark = hex(0xcfb890);
constexpr SkColor4f kZone = hex(0xd8c39b);
constexpr SkColor4f kZoneLight = hex(0xe0cda8);
// The engraved strengths. They are what the band's WIDTH is for, so they
// are set firm and black on the lithograph and have to be readable off
// the plate at plate size; smaller than this and the sheet's own subject
// cannot be checked against the drawing that carries it.
constexpr float kNumSize = 11.4f;

constexpr SkColor4f kInk = hex(0x25211d);  // p50 printed black
constexpr SkColor4f kInkDeep = hex(0x0a0806);
constexpr SkColor4f kInkThin = hex(0x4e4436);     // hairlines, hachures
constexpr SkColor4f kManuscript = hex(0x3b3a46);  // iron-gall, colder
constexpr SkColor4f kStampRed = hex(0x8e3b34);

constexpr SkColor4f kCard = hex(0xf2ece0);  // the audit's cooler paper
constexpr SkColor4f kCardInk = hex(0x1c1a17);
constexpr SkColor4f kBlue = hex(0x2f6f9c);      // MEASURED
constexpr SkColor4f kClaimRed = hex(0x8c2f22);  // WHAT THE LEGEND SAYS
constexpr SkColor4f kPass = hex(0x3e6b4a);
constexpr SkColor4f kAmber = hex(0xb5761e);
constexpr SkColor4f kGrey = hex(0x6d675c);

// ---------------------------------------------------------------------------
// composition — canvas 2560 x 1600

constexpr float kW = 2560.0f, kH = 1600.0f;
constexpr float kSheetX = 48.0f, kSheetY = 128.0f;
constexpr float kSheetW = 1400.0f, kSheetH = 1220.0f;  // aspect 1.1475
// The sheet is drawn at its own aspect, so this number is REAL: every band
// width on screen is a millimetre count of Minard's paper.
constexpr float kPxPerMm = kSheetW / 620.0f;  // 2.2581

// the printed frame, shared by both panels (BnF scan: x 157.5..3842.5)
constexpr float kFrameL = 46.0f;
constexpr float kFrameW = 579.14f * kPxPerMm;  // 1307.7
constexpr float kFrameR = kFrameL + kFrameW;
constexpr float kFrameT = 47.0f;
constexpr float kDivHN = 566.0f;   // Hannibal | Napoleon
constexpr float kDivNT = 1016.0f;  // map | temperature
constexpr float kFrameB = 1160.0f;

// Napoleon panel: x = a + b*lon, y = c − d*lat.
// b from the Commons scan (130.890 px/deg / 3.4482 px/mm * kPxPerMm);
// d/b = 2.142.
constexpr float kLonPx = 85.69f;
constexpr float kLon0 = 22.867f;           // longitude at the frame's left rule
constexpr float kLatPx = kLonPx * 2.142f;  // 183.55
constexpr float kLatRefY = 668.0f;         // y of lat 55.8 (Moscou)

// the audited scale: 1.1258 mm per 10,000 men (four direct BnF spot reads)
constexpr float kMmPer10k = 1.1258f;
constexpr float kStatedMmPer10k = 1.0f;
constexpr float kPxPer10k = kMmPer10k * kPxPerMm;  // 2.5421
constexpr float kLigneHalf = 1.1279f;              // 2.2558 / 2

inline float mapX(float lon) { return kFrameL + (lon - kLon0) * kLonPx; }
inline float mapY(float lat) { return kLatRefY + (55.8f - lat) * kLatPx; }
inline float bandPx(float men) { return men * kPxPer10k / 10000.0f; }

// audit column
constexpr float kAuditX = 1492.0f, kAuditW = 1020.0f;
constexpr float kConsoleY = 1356.0f, kConsoleH = 236.0f;

// ---------------------------------------------------------------------------
// THE DATA
//
// Minard.troops, all 51 rows, regrouped the way MINARD DRAWS IT rather
// than the way Wilkinson encodes it:
// Minard writes 422.000 on ONE band at the Niemen and lets the columns
// peel off; Wilkinson records three parallel bands from x = 0. Both are
// defensible; Minard's is the one that makes the flow identities visible.

struct Station {
  float lon, lat;
  float men;  // the strength the band CARRIES from this station eastward/onward
};

// --- the advance, as Minard draws it -------------------------------------
// trunk: the Niemen -> Moscou, with the two branch treads prepended
// NOLINTBEGIN(bugprone-throwing-static-initialization): literal tables; only
// allocation could throw
const std::vector<Station> kAdvTrunk = {
    {23.85f, 54.85f, 422000},  // the Niemen crossing
    {24.50f, 55.00f, 400000},  // the northern column has peeled off
    {25.50f, 54.50f, 340000},  // the Polotzk column has peeled off (Wilna)
    {26.00f, 54.70f, 320000}, {27.00f, 54.80f, 300000},
    {28.00f, 54.90f, 280000}, {28.50f, 55.00f, 240000},
    {29.00f, 55.10f, 210000}, {30.00f, 55.20f, 180000},
    {30.30f, 55.30f, 175000}, {32.00f, 54.80f, 145000},
    {33.20f, 54.90f, 140000}, {34.40f, 55.50f, 127100},
    {35.50f, 55.40f, 100000}, {36.00f, 55.50f, 100000},
    {37.60f, 55.80f, 100000},
};
// the 22,000 that peels north (group 3 A)
const std::vector<Station> kAdvNorth = {
    {24.50f, 55.00f, 22000},
    {24.50f, 55.30f, 22000},
    {24.60f, 55.80f, 6000},
};
// the 60,000 that peels toward Polotzk (group 2 A)
const std::vector<Station> kAdvPolotzk = {
    {25.50f, 54.50f, 60000},
    {26.60f, 55.70f, 40000},
    {27.40f, 55.60f, 33000},
    {28.70f, 55.50f, 33000},
};

// --- the retreat, east -> west -------------------------------------------
// Drawn in two pieces so the Bobr junction is an ENDPOINT of both, which
// is what test::endpointDegrees needs to see the army as one component.
const std::vector<Station> kRetEast = {
    {37.70f, 55.70f, 100000}, {37.50f, 55.70f, 98000}, {37.00f, 55.00f, 97000},
    {36.80f, 55.00f, 96000},  {35.40f, 55.30f, 87000}, {34.30f, 55.20f, 55000},
    {33.30f, 54.80f, 37000},  {32.00f, 54.60f, 24000}, {30.40f, 54.40f, 20000},
    {29.20f, 54.30f, 20000},  // Bobr
};
const std::vector<Station> kRetWest = {
    {29.20f, 54.30f, 50000},  // + the Polotzk column's 30,000 = 50,000
    {28.50f, 54.20f, 50000},  // Studienska — the Berezina
    {28.30f, 54.30f, 28000}, {27.50f, 54.50f, 20000},
    {26.80f, 54.30f, 12000}, {26.40f, 54.40f, 14000},  // <- the anomaly
    {25.00f, 54.40f, 8000},  {24.40f, 54.40f, 4000},
    {24.20f, 54.40f, 4000},  {24.10f, 54.40f, 4000},
};
const std::vector<Station> kRetPolotzk = {
    {28.70f, 55.50f, 33000},
    {29.20f, 54.30f, 30000},
};
const std::vector<Station> kRetNorth = {
    {24.60f, 55.80f, 6000},
    {24.20f, 54.42f, 6000},
    {24.10f, 54.40f, 6000},
};
// NOLINTEND(bugprone-throwing-static-initialization)

struct City {
  const char* plate;  // Minard's own spelling, kept
  float lon, lat;     // Minard's plotted position
  float rlon, rlat;   // gazetteer
  float dx, dy;       // label offset in sheet px
};
// residuals recomputed live from these by haversine (see cityKm below).
const std::array<City, 20> kCities = {{
    {"Kowno", 24.0f, 55.0f, 23.9036f, 54.8985f, -6, 24},
    {"Wilna", 25.3f, 54.7f, 25.2797f, 54.6872f, -4, 26},
    {"Smorgoni", 26.4f, 54.4f, 26.3958f, 54.4783f, -14, -12},
    {"Molodezno", 26.8f, 54.3f, 26.8500f, 54.3167f, -12, 20},
    {"Gloubokoe", 27.7f, 55.2f, 27.6906f, 55.1372f, -26, -14},
    {"Minsk", 27.6f, 53.9f, 27.5590f, 53.9006f, -12, -48},
    {"Studienska", 28.5f, 54.3f, 28.4333f, 54.3572f, -24, 26},
    {"Polotzk", 28.7f, 55.5f, 28.7861f, 55.4850f, 6, -12},
    {"Bobr", 29.2f, 54.4f, 29.2731f, 54.3097f, 8, 12},
    {"Witebsk", 30.2f, 55.3f, 30.2049f, 55.1904f, -12, -14},
    {"Orscha", 30.4f, 54.5f, 30.4172f, 54.5081f, -10, 20},
    {"Mohilow", 30.4f, 53.9f, 30.3313f, 53.9007f, -12, -48},
    {"Smolensk", 32.0f, 54.8f, 32.0401f, 54.7818f, -14, 22},
    {"Dorogobouge", 33.2f, 54.9f, 33.3000f, 54.9167f, -24, 20},
    {"Wizma", 34.3f, 55.2f, 34.2969f, 55.2114f, -10, 22},
    {"Chjat", 34.4f, 55.5f, 34.9833f, 55.5500f, -8, -14},
    {"Mojaisk", 36.0f, 55.5f, 36.0281f, 55.5053f, -14, -12},
    {"Moscou", 37.6f, 55.8f, 37.6173f, 55.7558f, -30, -22},
    {"Tarantino", 36.6f, 55.3f, 36.7167f, 55.1000f, 6, 4},
    {"Malo-jarosewli", 36.5f, 55.0f, 36.4667f, 55.0167f, 6, 4},
}};

struct Temp {
  const char* label;  // exactly as engraved (8bre / 9bre / Xbre kept)
  float lon;
  float reaumur;
  int daysSincePrev;
};
const std::array<Temp, 9> kTemps = {{
    {"Zero le 18 8bre", 37.6f, 0, 0},
    {"Pluie 24 8bre", 36.0f, 0, 6},
    {"- 9° le 9 9bre", 33.2f, -9, 16},
    {"- 21° le 14 9bre", 32.0f, -21, 5},
    {"- 11°", 29.2f, -11, 10},  // NO DATE ENGRAVED
    {"- 20° le 28 9bre", 28.5f, -20, 4},
    {"- 24° le 1er Xbre", 27.2f, -24, 3},
    {"- 30° le 6 Xbre", 26.7f, -30, 5},
    {"- 26° le 7 Xbre", 25.3f, -26, 1},
}};

// --- the Hannibal panel ---------------------------------------------------
// Stations READ OFF the BnF scan in sheet coordinates, which the panel's
// own lack of a fittable projection makes
// the only honest option: a least-squares latitude fit on that band returns
// d/b = 0.048 at R² = 0.12, i.e. there is no projection there to fit.
struct HStation {
  float x, y;
  float men;
  const char* at;
};
// NOLINTBEGIN(bugprone-throwing-static-initialization): a literal table; only
// allocation could throw
const std::vector<HStation> kHannibal = {
    {102, 268, 96000, "Espagne"},
    {222, 264, 94000, "Tortose"},
    {322, 306, 80000, "Terragone"},
    {400, 336, 80000, "Barcelone"},
    {510, 328, 80000, "Girone"},
    {578, 318, 60000, "Collioure"},
    {600, 296, 60000, "Perpignan"},
    {662, 272, 60000, "Narbone"},
    {760, 258, 60000, ""},
    {830, 280, 60000, "Pt St Esprit"},
    {858, 308, 60000, "Orange"},
    {934, 280, 46000, "l'Isere"},
    {998, 298, 46000, "Grenoble"},
    {1034, 330, 46000, "St Jn de Maurienne"},
    {1068, 372, 26000, "le Mt Cenis"},
    {1098, 400, 26000, "Suze"},
    {1124, 436, 26000, "Turin"},
};
// NOLINTEND(bugprone-throwing-static-initialization)

struct Place {
  const char* name;
  float x, y;
  int kind;  // 0 region caps, 1 town, 2 river, 3 people
};
const std::array<Place, 30> kHPlaces = {{
    {"Seltibériens", 118, 148, 3},
    {"ESPAGNE", 210, 178, 0},
    {"Hergètes", 300, 208, 3},
    {"Bargusiens", 300, 232, 3},
    {"Bouches de l'Èbre", 196, 328, 1},
    {"Tortose", 250, 260, 1},
    {"Terragone", 322, 316, 1},
    {"Barcelone", 390, 348, 1},
    {"Girone", 500, 322, 1},
    {"Emporium", 566, 348, 1},
    {"Collioure", 596, 328, 1},
    {"Perpignan", 578, 300, 1},
    {"Narbone", 668, 276, 1},
    {"GAULE  TRANSALPINE", 700, 190, 0},
    {"Pt St Esprit", 800, 284, 1},
    {"Orange", 862, 316, 1},
    {"Avignon", 812, 338, 1},
    {"Bouches du Rhône", 748, 356, 1},
    {"Marseille", 810, 428, 1},
    {"Toulon", 830, 480, 1},
    {"Nice", 966, 522, 1},
    {"LIGURIE", 962, 480, 0},
    {"Lyon", 1004, 178, 1},
    {"Vienne", 970, 226, 1},
    {"Allobroges", 962, 208, 3},
    {"Grenoble", 1006, 310, 1},
    {"St J. de Maurienne", 1006, 340, 1},
    {"Briançon", 1042, 384, 1},
    {"le Mt Cenis", 1058, 358, 1},
    {"Suze", 1108, 396, 1},
}};

const char* kLegendNapoleon[] = {
    "Les nombres d'hommes présents sont représentés par les "
    "largeurs des zônes colorées à raison d'un millimètre "
    "pour dix mille hommes; ils sont",
    "de plus écrits en travers des zônes. Le rouge désigne les "
    "hommes qui entrent en Russie, le noir ceux qui en sortent. — Les "
    "renseignements qui ont servi à dresser la carte ont été "
    "puisés",
    "dans les ouvrages de MM. Thiers, de Ségur, de Fezensac, de Chambray "
    "et le journal inédit de Jacob, pharmacien de l'armée depuis le "
    "28 Octobre.",
    "Pour mieux faire juger à l'œil la diminution de "
    "l'armée, j'ai supposé que les corps du Prince Jérôme "
    "et du Maréchal Davoust qui avaient été détachés "
    "sur Minsk",
    "et Mohilow et ont rejoint vers Orscha et Witebsk, avaient toujours "
    "marché avec l'armée.",
};

// ---------------------------------------------------------------------------
// geometry helpers

SkPoint stationPt(const Station& s) { return {mapX(s.lon), mapY(s.lat)}; }

SkPath polyline(const std::vector<Station>& st) {
  SkPathBuilder p;
  for (size_t i = 0; i < st.size(); ++i) {
    const SkPoint q = stationPt(st[i]);
    if (i == 0)
      p.moveTo(q);
    else
      p.lineTo(q);
  }
  return p.detach();
}

SkPath polylineH(const std::vector<HStation>& st) {
  SkPathBuilder p;
  for (size_t i = 0; i < st.size(); ++i) {
    if (i == 0)
      p.moveTo(st[i].x, st[i].y);
    else
      p.lineTo(st[i].x, st[i].y);
  }
  return p.detach();
}

/** Cumulative arc length to each station of a polyline. */
std::vector<float> arcAt(const std::vector<SkPoint>& pts) {
  std::vector<float> a(pts.size(), 0.0f);
  for (size_t i = 1; i < pts.size(); ++i)
    a[i] = a[i - 1] + SkPoint::Distance(pts[i], pts[i - 1]);
  return a;
}

/** The one trap every implementer hits: index the width by ARC LENGTH,
 *  not by PathSample::fraction and not by longitude. The stations are not
 *  equally spaced and the retreat's hook below Moscow is not monotone in
 *  x, so a fraction-indexed profile puts every riser in the wrong place.
 *  The check is concrete — each riser must fall at its named city. */
struct WidthProfile {
  std::vector<float> arc;  // cumulative arc length at each station
  std::vector<float> men;
  float maxPx = 0.0f;

  float menAt(float s) const {
    size_t i = 0;
    while (i + 1 < arc.size() && arc[i + 1] <= s) ++i;
    return men[i];
  }
  float pxAt(float s) const { return bandPx(menAt(s)); }
  bool operator==(const WidthProfile&) const = default;
};

/** THE FLOW BAND'S WIDTH, as a comparable Profile — and the sketch's own
 *  trap (above) is exactly why it is PX-KEYED. `alongIsPx` makes the seam
 *  hand `across` arc-length px measured from the spine's start, which is
 *  the unit `WidthProfile::arc` is already in, so every riser stays at its
 *  named city. Under the `spans::upTo(reveal)` the band is drawn with, a
 *  fraction would be a fraction of the REVEALED route and every riser
 *  would march east as the campaign advances.
 *
 *  `scale` is the LIVE 12.6% morph, read at paint. The profile compares on
 *  the pointer, not on the value behind it — two bands reading the same
 *  Output ARE the same law — and the node stays `Cache::None`, which is how
 *  a node holds a value the reconciler cannot see change. Being comparable
 *  also makes the ribbon equal to ITSELF, so an identical re-describe
 *  prunes instead of re-recording the whole band.
 *
 *  max(): the morph runs mmScale from kMmPer10k DOWN to kStatedMmPer10k
 *  (1.1258 → 1.0), so the ratio never exceeds 1 and the widest the band
 *  ever draws is `maxPx` exactly. A guessed bound is not needed and would
 *  be wrong: this one is derived from the morph's own endpoints. */
struct FlowWidth {
  WidthProfile prof;
  const ch::Output<float>* scale = nullptr;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    return prof.pxAt(px) * ((scale ? scale->value() : kMmPer10k) / kMmPer10k);
  }
  float max() const { return prof.maxPx; }
  bool operator==(const FlowWidth& o) const {
    return prof == o.prof && scale == o.scale;
  }
};

WidthProfile profileOf(const std::vector<Station>& st) {
  std::vector<SkPoint> pts;
  pts.reserve(st.size());
  for (const Station& s : st) pts.push_back(stationPt(s));
  WidthProfile w;
  w.arc = arcAt(pts);
  for (const Station& s : st) {
    w.men.push_back(s.men);
    w.maxPx = std::max(w.maxPx, bandPx(s.men));
  }
  return w;
}

WidthProfile profileOfH(const std::vector<HStation>& st) {
  std::vector<SkPoint> pts;
  pts.reserve(st.size());
  for (const HStation& s : st) pts.push_back({s.x, s.y});
  WidthProfile w;
  w.arc = arcAt(pts);
  for (const HStation& s : st) {
    w.men.push_back(s.men);
    w.maxPx = std::max(w.maxPx, bandPx(s.men));
  }
  return w;
}

/** ∫ w ds over the profile — the ink Minard intended, in px². */
float inkIntegral(const SkPath& spine, const WidthProfile& w,
                  float step = 1.0f) {
  float total = 0;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = 0; d < len; d += step)
      total += w.pxAt(d) * std::min(step, len - d);
  }
  return total;
}

// ---------------------------------------------------------------------------
// haversine, for the geography card

float haversineKm(float lon1, float lat1, float lon2, float lat2) {
  const float R = 6371.0088f;
  const float p1 = lat1 * kDeg, p2 = lat2 * kDeg;
  const float dp = (lat2 - lat1) * kDeg, dl = (lon2 - lon1) * kDeg;
  const float a =
      std::sin(dp / 2) * std::sin(dp / 2) +
      std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
  return 2 * R * std::asin(std::min(1.0f, std::sqrt(a)));
}
float cityKm(const City& c) {
  return haversineKm(c.lon, c.lat, c.rlon, c.rlat);
}

// ---------------------------------------------------------------------------
// path sugar

std::function<SkPath(SkSize)> segFn(SkPoint a, SkPoint b) {
  return [a, b](SkSize) {
    SkPathBuilder p;
    p.moveTo(a);
    p.lineTo(b);
    return p.detach();
  };
}
std::function<SkPath(SkSize)> pathFn(const SkPath& path) {
  return [path](SkSize) { return path; };
}

/** The coastlines and rivers of both panels: A SMOOTH PATH THE POINTS
 *  STEER — one quadratic per interior point, bounded by the hull they
 *  span and passing through none of them. That is what makes a dozen
 *  placed points read as one coast instead of a chain of chords. */
SkPath smooth(const std::vector<SkPoint>& p) {
  std::vector<glm::vec2> pts;
  pts.reserve(p.size());
  for (const SkPoint& q : p) pts.push_back({q.x(), q.y()});
  return path::smoothThrough(pts);
}

SkPath rectPath(float l, float t, float r, float bm) {
  SkPathBuilder p;
  p.addRect(SkRect::MakeLTRB(l, t, r, bm));
  return p.detach();
}

weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f color,
                      float tracking = 0) {
  return weave::textStyle({.face = std::move(face),
                           .size = size,
                           .color = color,
                           .track = tracking});
}

/** The French thousands separator the plate actually engraves: 422.000,
 *  never 422,000. */
std::string french(float men) {
  const long v = (long)std::lround(men);
  std::string s = std::to_string(v);
  for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ".");
  return s;
}

}  // namespace

// ===========================================================================

struct Minard1869 : sketch::Sketch {
  // ---- the timeline: ONE Output, every beat a window onto it.
  // NOT from() — ease:: is not total, so a value outside a beat's window
  // would feed the curve outside its domain. bind().window(a, b) clamps
  // instead, which is what lets one clock drive every beat on the sheet.
  ch::Output<float> T{0};
  ch::Output<float> mmScale{kMmPer10k};  // the 12.6% morph
  ch::Output<float> dimAmt{0};
  ch::Output<float> calAlpha{0};
  /** The caliper's discrete steps: an instrument walks in clicks, so its
   *  travel rides ticker.addFixed rather than the frame rate, and the
   *  reading it shows is TEXT — which no binding can carry, so it goes
   *  through slot()/renderSlot(). Four spot measurements, taken by hand on
   *  the BnF sheet with no cross-scan calibration at all. */
  int calStep = 0, calShown = -1;

  sk_sp<SkTypeface> faceScript, faceItalic, faceRoman, faceNum, faceUi,
      faceUiBold, faceMono;
  sigil::weave::FontContext* fonts = nullptr;  // the composer's, held for
                                               // metrics()/measureRun()

  Pattern paperPulp, laidLines, chainLines, foxing, tintSpeckle;
  Paint paperMat, vignette;

  // audits, computed once in setup()
  test::WidthAlong auditAdvance, auditRetreat;
  float advanceInk = 0, advanceArea = 0;
  float retreatArea = 0;
  float coverDoubled = 0;
  // the audit's own input, measured: what resolving the band's overlapping
  // steps into one outline leaves for a raycast to cross
  int outlineContours = 0, advSteps = 0;
  float outlineWalk = 0, advPerimeter = 0;
  const char* worstCorner = "";
  size_t advComponentsDrawn = 0, advComponentsWilkinson = 0, retComponents = 0;
  float riserArcErr = 0;   // arc-length indexed  — the right way
  float riserFracErr = 0;  // fraction indexed    — the trap
  const char* riserWorstCity = "";

  feed::TextRing colA{200}, colB{200}, colC{200}, colD{200}, colE{200};

  // -----------------------------------------------------------------------
  // beat clock (seconds). Everything reads through bind(&T).window(a,b).
  static constexpr float tPaper = 0.0f, tPaperEnd = 1.2f;
  static constexpr float tHann = 1.2f, tHannEnd = 3.0f;
  static constexpr float tLegend = 3.0f, tLegendEnd = 4.2f;
  static constexpr float tAdv = 4.2f, tAdvEnd = 6.4f;
  static constexpr float tRet = 6.4f, tRetEnd = 8.4f;
  static constexpr float tTemp = 8.4f, tTempEnd = 10.2f;
  static constexpr float tScale = 10.2f, tScaleEnd = 13.0f;
  static constexpr float tMorph = 13.0f, tMorphEnd = 15.0f;
  static constexpr float tLigne = 15.0f, tLigneEnd = 16.4f;
  static constexpr float tGeo = 16.4f, tGeoEnd = 19.0f;
  static constexpr float tDistort = 19.0f, tDistortEnd = 21.0f;
  static constexpr float tBar = 21.0f, tBarEnd = 23.0f;
  static constexpr float tReaumur = 23.0f, tReaumurEnd = 25.0f;
  static constexpr float tTwo = 25.0f, tTwoEnd = 27.5f;
  static constexpr float tLoop = 30.0f;

  Animatable<float> beat(float a, float b) const {
    return bind(&T).window(a, b).clamp(0.0f, 1.0f);
  }

  // =======================================================================
  // THE SHEET

  Element paperGround() {
    return box()
        .inset(0)
        .fill(paperMat)
        // the ageing: warmer and dirtier toward the edges
        .foreground(decorations::wash(vignette, SkBlendMode::kMultiply, 0.30f))
        .cache(Cache::Texture)
        .key("paper");
  }

  Element frames() {
    auto g = box().inset(0);
    // the two printed frames are ONE rule around each panel, drawn as a
    // double rule the way the plate cuts them
    auto rule = [&](float l, float t, float r, float bm, const char* k,
                    float t0, float t1) {
      g.child(box()
                  .inset(0)
                  .shape(pathFn(rectPath(l, t, r, bm)))
                  .stroke(spans::upTo(beat(t0, t1)),
                          lines::Line{.width = 1.1f,
                                      .fill = Fill::color(kInk),
                                      .parallels = 2,
                                      .gap = 3.0f})
                  .key(k));
    };
    rule(kFrameL, kFrameT, kFrameR, kDivHN, "frameH", 0.25f, 1.1f);
    rule(kFrameL, kDivHN + 4, kFrameR, kFrameB, "frameN", 0.35f, 1.2f);
    // the map | temperature divider
    g.child(box()
                .inset(0)
                .shape(segFn({kFrameL, kDivNT}, {kFrameR, kDivNT}))
                .stroke(spans::upTo(beat(0.4f, 1.2f)),
                        stroke(1.0f, Fill::color(kInk)))
                .key("divNT"));
    return g;
  }

  /** Minard's own hand across the top margin, and the two donation
   *  stamps. This is the part of the object that makes it HIS copy. */
  Element provenance() {
    auto g = box().inset(0);
    g.child(text(toU8("pour la Bibliothèque impériale"),
                 type(faceScript, 30, kManuscript, 1.0f))
                .at({40, 4})
                .key("dedic")
                .mask(by::edge(0.0f, beat(0.45f, 1.15f))));
    g.child(text(toU8("Ge Don 4182"), type(faceScript, 16, kManuscript, 0.4f))
                .at({1218, 12})
                .key("gedon")
                .opacity(beat(0.9f, 1.2f)));
    auto stamp = [&](float cx, float cy, float r, const char* label,
                     const char* k, float t0) {
      g.child(
          box()
              .rect(SkRect::MakeXYWH(cx - r, cy - r * 0.66f, 2 * r, 1.32f * r))
              .shape(shapes::circle())
              .stroke(stroke(1.5f, Fill::color(kStampRed)))
              .child(text(toU8(label), type(faceRoman, 7.5f, kStampRed, 0.3f))
                         .at({r * 0.35f, r * 0.42f}))
              .key(k)
              .scale(animate(from(0.0f).to(1.0f),
                             ramp(t0 * 1000, 420, ch::EaseOutBack())))
              .opacity(beat(t0, t0 + 0.2f)));
    };
    stamp(230, 118, 30, "DON\nN° 4182", "stamp1", 0.85f);
    stamp(920, 118, 26, "BIBL.", "stamp2", 0.95f);
    return g;
  }

  // =======================================================================
  // HANNIBAL — the panel nobody has seen

  /** The Mediterranean, as an engraved coast: the outline, then
   *  coast-parallel hatching whose spacing GROWS away from the shore,
   *  which is geometry::path::parallel called once per ring. */
  Element hannibalSea() {
    const std::vector<SkPoint> coast = {
        {60, 300},   {150, 318},  {200, 330},  {268, 348},  {330, 344},
        {392, 358},  {470, 344},  {540, 348},  {586, 344},  {626, 330},
        {700, 348},  {760, 372},  {800, 400},  {822, 448},  {846, 492},
        {900, 512},  {962, 528},  {1010, 542}, {1052, 528}, {1100, 512},
        {1150, 520}, {1210, 546}, {1258, 560},
    };
    // A lithographic edge is slightly ragged. One displace pass at low
    // amplitude and a long wavelength, before anything is stroked.
    const SkPath line =
        geometry::path::displace(smooth(coast), 0.9f, 90.0f, false);
    // The sea as a CLOSED region: the coast, then round the panel's own
    // south-east corner. Built by hand because there are no boolean path
    // ops here — `panelRect − land` is the natural spelling, and `.clip()`
    // only intersects, which would keep the land instead of dropping it.
    // Having the polygon, the hachures are one clipPath.
    SkPathBuilder seab;
    seab.addPath(line);
    seab.lineTo(kFrameR - 2, kDivHN - 2);
    seab.lineTo(60, kDivHN - 2);
    seab.close();
    const SkPath sea = seab.detach();

    std::vector<SkPath> rings;
    float d = 0;
    for (int i = 1; i <= 7; ++i) {
      d += 2.4f + 1.05f * (float)i;
      rings.push_back(geometry::path::parallel(line, -d, 4.0f));
    }

    auto g = box().inset(0);
    g.child(custom([sea, rings](SkCanvas& c, const PaintContext&) {
              SkPaint p;
              p.setAntiAlias(true);
              p.setStyle(SkPaint::kStroke_Style);
              p.setStrokeWidth(0.5f);
              c.save();
              c.clipPath(sea, true);
              for (size_t i = 0; i < rings.size(); ++i) {
                p.setColor4f(hex(0x4e4436, 0.55f - 0.062f * (float)i), nullptr);
                c.drawPath(rings[i], p);
              }
              c.restore();
            })
                .inset(0)
                .cache(Cache::Texture)
                .key("seahatch")
                .opacity(beat(tHann + 0.25f, tHann + 1.0f)));
    // the shore itself, engraved on top
    g.child(box()
                .inset(0)
                .shape(pathFn(line))
                .stroke(spans::upTo(beat(tHann, tHann + 0.5f)),
                        stroke(1.1f, Fill::color(kInk)))
                .key("coast"));
    g.child(text(toU8("Iles Baléares"), type(faceItalic, 9, kInk, 0.2f))
                .at({150, 500})
                .key("baleares")
                .opacity(beat(tHann + 0.9f, tHann + 1.2f)));
    return g;
  }

  /** Lehmann hachures (Johann Georg Lehmann, 1799): strokes down the line
   *  of steepest descent, black-to-white ratio proportional to slope —
   *  all white at 0°, all black at 45°. Generated from a synthetic height
   *  field of gaussian ridges, not drawn. Every stroke's direction, length,
   *  weight and alpha come from the local gradient, so this is a FIELD
   *  rather than a repeated motif and patterns::stripes cannot express
   *  it. */
  Element lehmann(const std::vector<std::array<float, 4>>& ridges, float x0,
                  float y0, float x1, float y1, const char* key, float t0) {
    // copying the captures can fail only on allocation
    // NOLINTNEXTLINE(bugprone-exception-escape)
    return custom([ridges, x0, y0, x1, y1](SkCanvas& c, const PaintContext&) {
             auto height = [&](float x, float y) {
               float h = 0;
               for (const auto& r : ridges) {
                 const float dx = (x - r[0]) / r[2];
                 const float dy = (y - r[1]) / r[3];
                 h += std::exp(-(dx * dx + dy * dy));
               }
               return h;
             };
             SkPaint p;
             p.setAntiAlias(true);
             p.setStyle(SkPaint::kStroke_Style);
             const float pitch = 4.6f;
             // the loop walks a distance; the accumulated float is the position
             // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
             for (float y = y0; y < y1; y += pitch) {
               // the loop walks a distance; the accumulated float is the
               // position
               // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
               for (float x = x0; x < x1; x += pitch) {
                 const float h = height(x, y);
                 if (h < 0.10f) continue;
                 const float e = 1.2f;
                 const float gx =
                     (height(x + e, y) - height(x - e, y)) / (2 * e);
                 const float gy =
                     (height(x, y + e) - height(x, y - e)) / (2 * e);
                 const float slope = std::sqrt(gx * gx + gy * gy);
                 if (slope < 0.004f) continue;
                 // Lehmann: black fraction = slope/45deg, capped
                 const float k = std::min(1.0f, slope / 0.055f);
                 const float len = pitch * (0.55f + 1.35f * k);
                 const float ux = -gx / (slope + 1e-6f),
                             uy = -gy / (slope + 1e-6f);
                 p.setStrokeWidth(0.45f + 0.75f * k);
                 p.setColor4f(
                     {kInkThin.fR, kInkThin.fG, kInkThin.fB, 0.30f + 0.62f * k},
                     nullptr);
                 c.drawLine(x - ux * len * 0.5f, y - uy * len * 0.5f,
                            x + ux * len * 0.5f, y + uy * len * 0.5f, p);
               }
             }
           })
        .inset(0)
        .cache(Cache::Texture)
        .key(key)
        .opacity(beat(t0, t0 + 0.55f));
  }

  Element hannibalPanel() {
    auto g = box().inset(0);

    g.child(text(toU8("Carte Figurative des pertes successives en hommes de "
                      "l'armée qu'Annibal conduisit d'Espagne"),
                 type(faceScript, 19, kInk, 0.2f))
                .at({210, 58})
                .key("htitle1")
                .opacity(beat(tHann, tHann + 0.4f)));
    g.child(text(toU8("en Italie en traversant les Gaules (selon Polybe)."),
                 type(faceScript, 17, kInk, 0.2f))
                .at({320, 84})
                .key("htitle2")
                .opacity(beat(tHann + 0.1f, tHann + 0.5f)));
    g.child(text(toU8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite."),
                 type(faceScript, 14, kInk, 0.15f))
                .at({300, 110})
                .key("htitle3")
                .opacity(beat(tHann + 0.2f, tHann + 0.6f)));
    g.child(text(toU8("Paris, le 20 Novembre 1869."),
                 type(faceScript, 14, kInk, 0.15f))
                .at({700, 132})
                .key("htitle4")
                .opacity(beat(tHann + 0.3f, tHann + 0.7f)));

    g.child(hannibalSea());

    // The Pyrenees and the Alps, hachured by Lehmann's rule.
    g.child(lehmann({{{562, 302, 58, 15}}, {{604, 288, 50, 13}}}, 440, 250, 720,
                    350, "pyrenees", tHann + 0.55f));
    g.child(lehmann({{{1046, 336, 52, 30}},
                     {{1086, 382, 62, 34}},
                     {{1002, 296, 40, 22}},
                     {{1128, 432, 54, 30}},
                     {{1064, 476, 72, 26}},
                     {{1176, 392, 44, 24}}},
                    900, 220, 1290, 570, "alps", tHann + 0.7f));

    // rivers, in a lighter sloped hand
    auto river = [&](const std::vector<SkPoint>& pts, const char* k, float t0) {
      g.child(box()
                  .inset(0)
                  .shape(pathFn(smooth(pts)))
                  .stroke(spans::upTo(beat(t0, t0 + 0.4f)),
                          stroke(0.8f, Fill::color(hex(0x4e4436, 0.8f))))
                  .key(k));
    };
    river({{206, 190}, {198, 240}, {212, 280}, {200, 322}}, "ebre",
          tHann + 0.9f);
    river({{796, 150}, {806, 210}, {790, 268}, {800, 320}, {780, 358}}, "rhone",
          tHann + 0.95f);
    river({{910, 236}, {930, 268}, {952, 286}}, "isere", tHann + 1.0f);
    river({{1040, 300}, {1010, 330}, {980, 350}}, "arc", tHann + 1.0f);
    river({{1160, 430}, {1200, 452}, {1250, 462}}, "po", tHann + 1.05f);

    // THE BAND — brush::Ribbon on the width Profile seam, over the plate's
    // own strengths. This is the primitive the whole sheet is made of.
    const SkPath spine = polylineH(kHannibal);
    const WidthProfile prof = profileOfH(kHannibal);
    g.child(bandElement(spine, prof, kZone, "hband",
                        beat(tHann + 0.55f, tHann + 1.6f)));

    // the ten numbers, written ACROSS the zones ("écrits en travers"),
    // Orient::Tangent on a cross-segment
    for (size_t i = 0; i + 1 < kHannibal.size(); ++i) {
      if (i > 0 && kHannibal[i].men == kHannibal[i - 1].men) continue;
      g.child(bandNumber({kHannibal[i].x, kHannibal[i].y},
                         {kHannibal[i + 1].x - kHannibal[i].x,
                          kHannibal[i + 1].y - kHannibal[i].y},
                         kHannibal[i].men, 8.2f, "hn" + std::to_string(i),
                         tHann + 0.7f + 0.06f * (float)i));
    }

    // the place names
    for (size_t i = 0; i < kHPlaces.size(); ++i) {
      const Place& p = kHPlaces[i];
      sigil::weave::TextStyle st =
          p.kind == 0   ? type(faceRoman, 11, kInk, 2.6f)
          : p.kind == 3 ? type(faceItalic, 10, hex(0x4e4436), 1.2f)
                        : type(faceItalic, 9, kInk, 0.2f);
      g.child(text(toU8(p.name), st)
                  .at({p.x, p.y})
                  .key("hp" + std::to_string(i))
                  .opacity(beat(tHann + 1.0f + 0.012f * (float)i,
                                tHann + 1.4f + 0.012f * (float)i)));
    }

    // The LEGEND BOX — top right, and it is the second, independent
    // statement of the same scale rule.
    g.child(legendBox());
    // the scale bar, in the panel's OWN lieue (4,560 m — not the Russian
    // panel's 4,444.8 m; the two panels use different lieues)
    g.child(scaleBar(640, 500, 14.09f / 3.0f, 30, 5, "Lieues de 4.560 m",
                     "hbar", tHann + 1.45f));
    // the compass arrow in the Mediterranean
    g.child(box()
                .inset(0)
                .shape(segFn({846, 486}, {900, 424}))
                .stroke(spans::upTo(beat(tHann + 1.5f, tHann + 1.75f)),
                        lines::presets::arrow(1.2f, Fill::color(kInk), 9.0f))
                .key("compass"));
    return g;
  }

  Element legendBox() {
    const float l = 1010, t = 52, w = 344, h = 108;
    auto g = box()
                 .rect(SkRect::MakeXYWH(l, t, w, h))
                 .shape(pathFn(rectPath(0, 0, w, h)))
                 .stroke(stroke(1.0f, Fill::color(kInk)))
                 .key("legendbox")
                 .opacity(beat(tHann + 1.5f, tHann + 1.8f));
    g.child(text(toU8("Légende."), type(faceScript, 14, kInk)).at({140, 4}));
    const char* lines_[] = {
        "Les nombres d'hommes restés à Annibal sont "
        "représentés",
        "par la largeur des zônes colorées à raison d'un "
        "millimètre",
        "pour dix mille hommes, ils sont de plus écrits en travers des "
        "zônes.",
        "Il n'y a pas d'opinion arrêtée sur le point où Annibal "
        "a",
        "traversé les Alpes, j'ai adopté celle de Larosa sans "
        "prétendre la justifier.",
    };
    for (int i = 0; i < 5; ++i)
      g.child(text(toU8(lines_[i]), type(faceScript, 9.6f, kInk, 0.05f))
                  .at({8, 24 + 16.0f * (float)i}));
    return g;
  }

  // =======================================================================
  // NAPOLEON

  /** The brush every zone on both panels is painted with: the strength law
   *  on the width Profile seam, bevelled on the outside of each turn — the
   *  chord a lithographer's overlapping treads leave. The audit reads the
   *  same value, so what it measures is what the sheet shows. */
  brush::Ribbon flowRibbon(const WidthProfile& prof, SkColor4f colour) {
    brush::Ribbon r =
        brush::ribbon(FlowWidth{prof, &mmScale}, Fill::color(colour));
    r.step = 2.0f;
    r.join = SkPaint::kBevel_Join;
    return r;
  }

  /** One band, on a node sized to the ROUTE's bounding box so that the
   *  profile's `max()` is actually load bearing (the band overflows that
   *  box by up to w/2 on each side). */
  Element bandElement(const SkPath& spine, const WidthProfile& prof,
                      SkColor4f colour, const std::string& key,
                      Animatable<float> reveal) {
    const SkRect bb = spine.getBounds();
    const SkPath local = spine.makeOffset(-bb.left(), -bb.top());
    // The profile reads a LIVE Output (the 12.6% morph), which the
    // reconciler cannot see change — hence Cache::None. See FlowWidth.
    const brush::Ribbon r = flowRibbon(prof, colour);
    return box()
        .rect(SkRect::MakeXYWH(bb.left(), bb.top(), bb.width(), bb.height()))
        // the callable is invoked on every layout, so its capture must survive
        // each return
        // NOLINTNEXTLINE(performance-no-automatic-move)
        .shape([local](SkSize) { return local; })
        .stroke(spans::upTo(std::move(reveal)), r)
        .cache(Cache::None)
        .key(key);
  }

  /** A strength written ACROSS its zone — Minard's "écrits en travers des
   *  zônes", set in the French convention with a full stop for thousands
   *  (422.000, never 422,000). The baseline is a segment along the band's
   *  NORMAL, so TextPath::Orient::Tangent puts the type across the band.
   *  The segment reaches past the band's half-width — or past the run's
   *  own half-length, whichever is longer, because glyphs past a path's
   *  end are DROPPED, so a thin band under a long number needs the run to
   *  set the floor — by the face's cap slack at this size, so the
   *  clearance scales with the type instead of sitting at a guessed
   *  constant. */
  Element bandNumber(SkPoint at, SkVector tangent, float men, float size,
                     const std::string& key, float t0) {
    const float L = std::hypot(tangent.x(), tangent.y());
    SkVector t =
        L > 0 ? SkVector{tangent.x() / L, tangent.y() / L} : SkVector{1, 0};
    SkVector n{-t.y(), t.x()};
    if (n.y() > 0) {  // make the type read bottom-up, as on the plate
      n = {-n.x(), -n.y()};
    }
    const auto style = type(faceNum, size, kInk, 0.2f);
    float runLen = 0;
    float slack = size * 0.3f;  // metrics-free fallback, same shape
    if (fonts) {
      runLen = runPens(toU8(french(men)), style, *fonts).back();
      slack = metrics(style, *fonts).capSlack();
    }
    const float half = std::max(bandPx(men) * 0.5f, runLen * 0.5f) + slack;
    const SkPoint a{at.x() - n.x() * half, at.y() - n.y() * half};
    const SkPoint b{at.x() + n.x() * half, at.y() + n.y() * half};
    return text(toU8(french(men)), style)
        .rect(SkRect::MakeXYWH(0, 0, kSheetW, kSheetH))
        .onPath(TextPath{.path = segFn(a, b),
                         .at = 0.5f,
                         .align = TextPath::Align::Center,
                         .offset = 0.0f,
                         .autoFlip = false,
                         .orient = TextPath::Orient::Tangent})
        .key(key)
        .opacity(beat(t0, t0 + 0.3f));
  }

  /** The advance: one trunk and two branches, each the same band brush
   *  over its own strength law, revealed from the Niemen eastward the way
   *  the army walked it. */
  Element advanceZones() {
    auto zone = [this](const std::vector<Station>& st, const std::string& key,
                       Animatable<float> reveal) {
      return bandElement(polyline(st), profileOf(st), kZone, key,
                         std::move(reveal));
    };
    return box()
        .inset(0)
        .child(zone(kAdvTrunk, "advTrunk", beat(tAdv, tAdv + 1.6f)))
        .child(zone(kAdvNorth, "advNorth", beat(tAdv + 0.35f, tAdv + 0.8f)))
        .child(zone(kAdvPolotzk, "advPol", beat(tAdv + 0.55f, tAdv + 1.2f)));
  }

  Element napoleonPanel(sketch::SketchContext& ctx) {
    auto g = box().inset(0);

    g.child(text(toU8("Carte Figurative des pertes successives en hommes de "
                      "l'Armée Française dans la campagne de Russie "
                      "1812—1813."),
                 type(faceScript, 18, kInk, 0.15f))
                .at({200, kDivHN + 14})
                .key("ntitle")
                .opacity(beat(tLegend, tLegend + 0.3f)));
    g.child(text(toU8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite.        Paris, "
                      "le 20 Novembre 1869."),
                 type(faceScript, 13, kInk, 0.1f))
                .at({300, kDivHN + 40})
                .key("ntitle2")
                .opacity(beat(tLegend + 0.1f, tLegend + 0.4f)));

    // the legend as a PARAGRAPH, which is what it is — not a key.
    for (int i = 0; i < 5; ++i) {
      g.child(
          text(toU8(kLegendNapoleon[i]), type(faceScript, 9.8f, kInk, 0.02f))
              .at({i == 3 ? 148.0f : 128.0f, kDivHN + 58 + 14.6f * (float)i})
              .key("nleg" + std::to_string(i))
              .mask(by::edge(0.0f, beat(tLegend + 0.25f + 0.16f * (float)i,
                                        tLegend + 0.55f + 0.16f * (float)i))));
    }

    // the rivers of the Russian panel
    auto river = [&](const std::vector<SkPoint>& pts, const char* label,
                     SkPoint lp, const char* k, float t0) {
      g.child(box()
                  .inset(0)
                  .shape(pathFn(smooth(pts)))
                  .stroke(spans::upTo(beat(t0, t0 + 0.35f)),
                          stroke(0.7f, Fill::color(hex(0x4e4436, 0.85f))))
                  .key(k));
      g.child(text(toU8(label), type(faceItalic, 8, hex(0x4e4436), 0.6f))
                  .at({lp.x(), lp.y()})
                  .key(std::string(k) + "L")
                  .opacity(beat(t0 + 0.2f, t0 + 0.5f)));
    };
    river({{mapX(23.7f), mapY(56.0f)},
           {mapX(23.95f), mapY(55.3f)},
           {mapX(23.8f), mapY(54.8f)},
           {mapX(24.0f), mapY(54.2f)},
           {mapX(23.9f), mapY(53.8f)}},
          "Niémen R.", {mapX(23.45f), 774.0f}, "rNiemen", tAdv - 0.2f);
    river({{mapX(28.6f), mapY(54.9f)},
           {mapX(28.45f), mapY(54.5f)},
           {mapX(28.6f), mapY(54.1f)},
           {mapX(28.4f), mapY(53.7f)}},
          "Bérézina R.", {mapX(28.2f), mapY(54.72f)}, "rBerez", tAdv - 0.15f);
    river({{mapX(31.2f), mapY(54.05f)},
           {mapX(30.9f), mapY(54.5f)},
           {mapX(31.05f), mapY(54.95f)},
           {mapX(30.7f), mapY(55.4f)}},
          "Dniéper R.", {mapX(30.95f), mapY(54.35f)}, "rDniepr", tAdv - 0.1f);
    river({{mapX(36.6f), mapY(56.05f)},
           {mapX(36.9f), mapY(55.7f)},
           {mapX(37.3f), mapY(55.45f)}},
          "Moskowa R.", {mapX(36.35f), mapY(55.95f)}, "rMoskowa", tAdv - 0.05f);

    // --- THE ADVANCE ------------------------------------------------------
    // The red-brown is a SEPARATE STONE from the black, so it is very
    // slightly out of register. One translate, and it is the single most
    // convincing "this is a lithograph" cue on the sheet.
    auto redStone = box().inset(0).translateX(0.4f).translateY(-0.3f);
    // The zones read the 12.6% morph the way every other band on the sheet
    // does: through the width law, at paint. Nothing here re-describes when
    // it moves.
    redStone.child(advanceZones());
    // the stone took unevenly: a very low-amplitude speckle in the zone
    // colour, NOT a gradient (the Commons p10/p90 are two units apart)
    redStone.child(box()
                       .inset(0)
                       .fill(tintSpeckle.material())
                       .blend(SkBlendMode::kMultiply)
                       .opacity(0.06f)
                       .cache(Cache::Texture)
                       .key("tintwander"));
    g.child(std::move(redStone));

    // --- THE RETREAT ------------------------------------------------------
    g.child(bandElement(polyline(kRetEast), profileOf(kRetEast), kInkDeep,
                        "retEast", beat(tRet, tRet + 0.9f)));
    g.child(bandElement(polyline(kRetPolotzk), profileOf(kRetPolotzk), kInkDeep,
                        "retPol", beat(tRet + 0.7f, tRet + 1.0f)));
    g.child(bandElement(polyline(kRetWest), profileOf(kRetWest), kInkDeep,
                        "retWest", beat(tRet + 0.85f, tRet + 1.7f)));
    g.child(bandElement(polyline(kRetNorth), profileOf(kRetNorth), kInkDeep,
                        "retNorth", beat(tRet + 1.5f, tRet + 1.8f)));

    // The arithmetic of the splits, as a footnote row along the bottom of
    // the map panel — five identities, all exact, on numbers Minard
    // engraved, and they need no measurement at all.
    {
      const char* ident[] = {
          "422 − 22 = 400", "400 − 60 = 340", "20 + 30 = 50  (Bobr)",
          // one caption, split across adjacent literals
          // NOLINTNEXTLINE(bugprone-suspicious-missing-comma)
          "50 − 28 = 22,000 in four days  (the "
          "Berezina)",
          "4 + 6 = 10 recrossed"};
      float x = kFrameL + 16;
      for (int i = 0; i < 5; ++i) {
        g.child(text(toU8(ident[i]), type(faceUiBold, 9.5f, kBlue))
                    .at({x, kDivNT - 26})
                    .key("ar" + std::to_string(i))
                    .opacity(beat(tAdv + 0.5f + 0.35f * (float)i,
                                  tAdv + 0.75f + 0.35f * (float)i)));
        x += 20.0f + 6.4f * (float)std::char_traits<char>::length(ident[i]);
      }
      g.child(text(toU8("all five EXACT"), type(faceUiBold, 9.5f, kPass))
                  .at({x, kDivNT - 26})
                  .key("arok")
                  .opacity(beat(tRet + 1.8f, tRet + 2.1f)));
    }

    // --- the engraved numbers --------------------------------------------
    auto numbersFor = [&](const std::vector<Station>& st, const char* tag,
                          float t0, float dt) {
      for (size_t i = 0; i + 1 < st.size(); ++i) {
        if (i > 0 && st[i].men == st[i - 1].men) continue;
        const SkPoint a = stationPt(st[i]), b = stationPt(st[i + 1]);
        g.child(bandNumber({(a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f},
                           {b.x() - a.x(), b.y() - a.y()}, st[i].men, kNumSize,
                           tag + std::to_string(i), t0 + dt * (float)i));
      }
    };
    numbersFor(kAdvTrunk, "nA", tAdv + 0.4f, 0.07f);
    numbersFor(kAdvNorth, "nB", tAdv + 0.5f, 0.05f);
    numbersFor(kAdvPolotzk, "nC", tAdv + 0.8f, 0.05f);
    numbersFor(kRetEast, "nD", tRet + 0.2f, 0.06f);
    numbersFor(kRetWest, "nE", tRet + 0.9f, 0.06f);
    numbersFor(kRetPolotzk, "nF", tRet + 0.8f, 0.05f);
    // what recrossed the Niemen
    g.child(bandNumber({mapX(23.95f), mapY(54.4f)}, {1, 0}, 10000, kNumSize,
                       "nG", tRet + 1.7f));

    // --- the place names --------------------------------------------------
    for (size_t i = 0; i < kCities.size(); ++i) {
      const City& c = kCities[i];
      const bool moscou = std::string(c.plate) == "Moscou";
      // MOSCOU alone is set in spaced roman capitals, and it is the only
      // word on the map that is.
      Element e = moscou
                      ? text(toU8("MOSCOU"), type(faceRoman, 13, kInk, 2.2f))
                            .textStroke(0.5f, Fill::color(kInk))
                      : text(toU8(c.plate), type(faceItalic, 9.6f, kInk, 0.2f));
      g.child(e.at({mapX(c.lon) + c.dx, mapY(c.lat) + c.dy})
                  .key("city" + std::to_string(i))
                  .opacity(beat(tAdv + 0.1f + 0.03f * (float)i,
                                tAdv + 0.4f + 0.03f * (float)i)));
    }

    // THE FLOOR, drawn on the plate itself: the blue outline is the width
    // Minard's crayon actually laid at the last treads (5.4 px on the
    // Commons scan = 3.54 px here), against the black band this sketch
    // draws from the rule. The last 4,000 men are 2.6x over.
    {
      const float floorPx = 5.4f * 0.6549f;
      SkPathBuilder fb;
      const float y = mapY(54.4f);
      fb.addRect(SkRect::MakeLTRB(mapX(24.1f), y - floorPx * 0.5f, mapX(25.0f),
                                  y + floorPx * 0.5f));
      g.child(box()
                  .inset(0)
                  .shape(pathFn(fb.detach()))
                  .stroke(stroke(0.9f, Fill::color(kBlue)))
                  .key("floorink")
                  .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
      g.child(text(toU8("what the crayon actually laid — 1.57 mm, "
                        "2.6× the rule"),
                   type(faceUi, 9, kBlue))
                  .at({mapX(24.1f), y + 10})
                  .key("floorinklab")
                  .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
    }

    // THE CALIPER LIES. Minard's own bar, read against Minard's own map.
    g.child(text(toU8("Minard's own bar reads Kowno→Smolensk as 210 "
                      "lieues = 933 km.  The truth is 520 km.  ×1.79 — "
                      "UNEXPLAINED"),
                 type(faceUiBold, 10.0f, kAmber))
                .at({880, 958})
                .key("barlies")
                .opacity(beat(tBar, tBar + 0.4f)));
    g.child(text(toU8("hypotheses: the labels are half their true value · "
                      "copied unrescaled from Fezensac · my longitude "
                      "scale is wrong.  None asserted."),
                 type(faceUi, 9.0f, hex(0xb5761e, 0.9f)))
                .at({880, 972})
                .key("barhyp")
                .opacity(beat(tBar + 0.4f, tBar + 0.8f)));

    // the lieue bar, and its ticks
    g.child(scaleBar(mapX(33.4f), 930.0f, 4.985f * 0.6549f, 50, 5,
                     "Lieues communes de France (Carte de M. de Fezensac)",
                     "nbar", tAdv + 1.7f));
    return g;
  }

  /** A graduated bar. `pxPerUnit` is px per lieue, `span` the last label,
   *  `step` the label interval. Built by hand rather than by stamping along
   *  a contour: each graduation carries its own number, and a stamped
   *  element cannot know which sample it is. */
  Element scaleBar(float x, float y, float pxPerUnit, int span, int step,
                   const char* label, const char* key, float t0) {
    auto g = box().inset(0).key(key).opacity(beat(t0, t0 + 0.3f));
    const float w = pxPerUnit * (float)span;
    g.child(box()
                .inset(0)
                .shape(segFn({x, y}, {x + w, y}))
                .stroke(stroke(0.9f, Fill::color(kInk))));
    for (int v = 0; v <= span; v += step) {
      const float tx = x + pxPerUnit * (float)v;
      if (v > 25 && v < span)
        continue;  // the plate's own graduation: 0 5 10 15 20 25 ...... 50
      g.child(box().inset(0).shape(segFn({tx, y - 4}, {tx, y})));
      g.child(text(toU8(std::to_string(v)), type(faceNum, 6.5f, kInk))
                  .at({tx - 3, y + 2}));
    }
    // the ticks as one stroked path so they are one node
    {
      SkPathBuilder tb;
      for (int v = 0; v <= span; v += step) {
        if (v > 25 && v < span) continue;
        const float tx = x + pxPerUnit * (float)v;
        tb.moveTo(tx, y - 4);
        tb.lineTo(tx, y);
      }
      g.child(box()
                  .inset(0)
                  .shape(pathFn(tb.detach()))
                  .stroke(stroke(0.9f, Fill::color(kInk))));
    }
    g.child(
        text(toU8(label), type(faceItalic, 7.5f, kInk, 0.1f)).at({x, y - 18}));
    return g;
  }

  // =======================================================================
  // THE TEMPERATURE PANEL — and the nine droplines that ARE the joint
  // between the two panels.

  float tempY(float reaumur) const {
    return kDivNT + 26.0f + (-reaumur) / 30.0f * 92.0f;
  }

  Element temperaturePanel() {
    auto g = box().inset(0);
    g.child(text(toU8("TABLEAU GRAPHIQUE de la température en degrés "
                      "du thermomètre de Réaumur au dessous de "
                      "zéro."),
                 type(faceScript, 13, kInk, 0.3f))
                .at({300, kDivNT + 4})
                .key("tempTitle")
                .opacity(beat(tTemp, tTemp + 0.3f)));

    // the INVERTED axis: 0 at the top, 30 degrés at the bottom
    for (int r = 0; r <= 30; r += 5) {
      const float y = tempY((float)-r);
      g.child(box()
                  .inset(0)
                  .shape(segFn({kFrameL, y}, {kFrameR - 34, y}))
                  .stroke(spans::upTo(beat(tTemp + 0.1f + 0.03f * (float)r,
                                           tTemp + 0.45f + 0.03f * (float)r)),
                          stroke(0.4f, Fill::color(hex(0x4e4436, 0.45f))))
                  .key("taxis" + std::to_string(r)));
      g.child(text(toU8(r == 30 ? "30 degrés" : std::to_string(r)),
                   type(faceNum, 7, kInk))
                  .at({kFrameR - 30, y - 4})
                  .key("tlab" + std::to_string(r))
                  .opacity(beat(tTemp + 0.15f + 0.03f * (float)r,
                                tTemp + 0.5f + 0.03f * (float)r)));
    }

    // the curve, and the fine ticks hatched UNDER it (not a fill)
    std::vector<SkPoint> curve;
    curve.reserve(std::size(kTemps));
    for (const Temp& t : kTemps)
      curve.push_back({mapX(t.lon), tempY(t.reaumur)});
    SkPathBuilder cb;
    for (size_t i = 0; i < curve.size(); ++i)
      i == 0 ? cb.moveTo(curve[i]) : cb.lineTo(curve[i]);
    const SkPath curvePath = cb.detach();
    g.child(box()
                .inset(0)
                .shape(pathFn(curvePath))
                .stroke(stroke(1.2f, Fill::color(kInk)))
                // right to left, the way the retreat runs
                .mask(by::edge(180.0f, beat(tTemp + 0.4f, tTemp + 1.1f)))
                .key("tcurve"));
    // the hatched underside: short ticks hanging off the curve
    g.child(custom([curvePath](SkCanvas& c, const PaintContext&) {
              SkPaint p;
              p.setAntiAlias(true);
              p.setStyle(SkPaint::kStroke_Style);
              p.setStrokeWidth(0.55f);
              p.setColor4f(hex(0x38301f, 0.9f), nullptr);
              SkContourMeasureIter it(curvePath, false);
              while (sk_sp<SkContourMeasure> m = it.next()) {
                const float len = m->length();
                // the loop walks a distance; the accumulated float is the
                // position
                // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                for (float d = 0; d < len; d += 3.0f) {
                  SkPoint q;
                  SkVector tn;
                  if (!m->getPosTan(d, &q, &tn)) continue;
                  c.drawLine(q.x(), q.y(), q.x() - 1.4f, q.y() + 5.0f, p);
                }
              }
            })
                .inset(0)
                .cache(Cache::Texture)
                .key("thatch")
                .opacity(beat(tTemp + 0.6f, tTemp + 1.2f)));

    // THE DROPLINES. Nine of them, from the retreat band down through the
    // divider into the graph. They are the joint between the two panels
    // and they are the whole design. Nothing declares that the two panels
    // share an abscissa: the lock is that both call the same mapX(lon).
    for (size_t i = 0; i < kTemps.size(); ++i) {
      const float x = mapX(kTemps[i].lon);
      SkPathBuilder d;
      d.moveTo(x, mapY(54.3f));
      d.lineTo(x, tempY(kTemps[i].reaumur));
      PathFormat f;
      f.width = 0.7f;
      // the rule fades as it crosses the panel divider
      f.strokeMaterial = Paint::linearUnit({0, 0}, {0, 1},
                                           {{0.0f, hex(0x4e4436, 0.80f)},
                                            {0.66f, hex(0x4e4436, 0.22f)},
                                            {1.0f, hex(0x4e4436, 0.75f)}});
      g.child(box()
                  .inset(0)
                  .shape(pathFn(d.detach()))
                  .stroke(spans::upTo(beat(tTemp + 0.25f + 0.05f * (float)i,
                                           tTemp + 0.55f + 0.05f * (float)i)),
                          f)
                  .key("drop" + std::to_string(i)));

      // the annotation, as engraved. 8bre / 9bre / Xbre are October /
      // November / December — the old Roman-calendar notation, and a
      // caption that "corrects" Xbre to 10bre is wrong twice over.
      g.child(text(toU8(kTemps[i].label), type(faceNum, 7.4f, kInk, 0.1f))
                  .at({x - 26, tempY(kTemps[i].reaumur) + 5})
                  .key("tann" + std::to_string(i))
                  .opacity(beat(tTemp + 0.5f + 0.06f * (float)i,
                                tTemp + 0.8f + 0.06f * (float)i)));
    }
    // the undated −11°, and its two independent recoveries
    g.child(text(toU8("24 novembre  (derived, days column)"),
                 type(faceUi, 8, kBlue))
                .at({mapX(29.2f) - 26, tempY(-11) + 16})
                .key("recov1")
                .opacity(beat(tTemp + 1.35f, tTemp + 1.6f)));
    g.child(text(toU8("25 novembre  (derived, lon interpolation)"),
                 type(faceUi, 8, kBlue))
                .at({mapX(29.2f) - 26, tempY(-11) + 27})
                .key("recov2")
                .opacity(beat(tTemp + 1.5f, tTemp + 1.75f)));

    g.child(text(toU8("Les Cosaques passent au galop\nle Niémen gelé."),
                 type(faceScript, 10, kInk, 0.1f))
                .at({kFrameL + 20, kDivNT + 40})
                .key("cosaques")
                .opacity(beat(tTemp + 1.1f, tTemp + 1.5f)));
    return g;
  }

  Element imprints() {
    auto g = box().inset(0);
    g.child(text(toU8("Autog. par Regnier, 8. Pas. Sᵗᵉ Marie Sᵗ "
                      "Gᵃᵉᵐ à Paris."),
                 type(faceItalic, 7, kInk))
                .at({kFrameL + 6, kFrameB + 6})
                .key("imp1")
                .opacity(beat(1.0f, 1.3f)));
    g.child(
        text(toU8("Imp. Lith. Regnier et Dourdet"), type(faceItalic, 7, kInk))
            .at({kFrameR - 140, kFrameB + 6})
            .key("imp2")
            .opacity(beat(1.0f, 1.3f)));
    return g;
  }

  struct SpotRead {
    float x, y, halfPx, mm, men;
    const char* where;
  };
  SpotRead spot(int i) const {
    switch ((unsigned)i & 3u) {
      case 0:
        return {mapX(24.0f) + 8, mapY(54.9f), bandPx(422000) * 0.5f,
                47.15f,          422000,      "Napoléon, at the Niemen"};
      case 1:
        return {
            mapX(24.9f), mapY(55.0f), bandPx(400000) * 0.5f,
            44.54f,      400000,      "Napoléon, after the northern column"};
      case 2:
        return {mapX(37.0f), mapY(55.62f), bandPx(100000) * 0.5f,
                11.43f,      100000,       "Napoléon, at Moscou"};
      default:
        return {222,    264,   bandPx(96000) * 0.5f,
                10.84f, 96000, "Annibal, at the Ebro — a DIFFERENT panel"};
    }
  }

  /** The caliper: the only saturated object allowed on the plate, drawn
   *  OVER it with a small shadow so it reads as an instrument laid on
   *  paper rather than as ink. */
  Element caliper() {
    const SpotRead r = spot(calStep);
    auto g = box().inset(0).key("caliperGrp").opacity(&calAlpha);
    auto jaw = [&](float x, float y, float halfPx, const char* k) {
      SkPathBuilder p;
      p.moveTo(x - 16, y - halfPx);
      p.lineTo(x + 16, y - halfPx);
      p.moveTo(x - 16, y + halfPx);
      p.lineTo(x + 16, y + halfPx);
      p.moveTo(x + 12, y - halfPx);
      p.lineTo(x + 12, y + halfPx);
      g.child(box()
                  .inset(0)
                  .shape(pathFn(p.detach()))
                  .background(shadow(hex(0x000000, 0.30f), {1.5f, 2.0f}, 3.0f))
                  .stroke(stroke(2.0f, Fill::color(kBlue)))
                  .key(k));
    };
    jaw(r.x, r.y, r.halfPx, "jaw1");
    const float rx = kFrameL + 10, ry = 686.0f;
    g.child(
        text(toU8(kit::formatted("%.2f mm", r.mm)), type(faceUiBold, 17, kBlue))
            .at({rx, ry})
            .key("calread"));
    g.child(text(toU8(kit::formatted("÷ %.0f = %.4f mm / 10.000", r.men,
                                  r.mm / (r.men / 10000.0f))),
                 type(faceUi, 9.5f, kBlue))
                .at({rx, ry + 20})
                .key("calread2"));
    g.child(text(toU8(std::string(r.where) +
                      "\n(measured on the BnF sheet, no cross-scan "
                      "calibration)"),
                 type(faceUi, 9, hex(0x2f6f9c, 0.9f)))
                .at({rx, ry + 33})
                .key("calread3"));
    g.child(
        text(toU8("the legend says 1.0000"), type(faceUiBold, 10, kClaimRed))
            .at({rx, ry + 58})
            .key("calread4"));
    return g;
  }

  Element sheet(sketch::SketchContext& ctx) {
    return box()
        .rect(SkRect::MakeXYWH(kSheetX, kSheetY, kSheetW, kSheetH))
        .background(shadow(hex(0x000000, 0.55f), {6, 10}, 26))
        .child(paperGround())
        .child(frames())
        .child(hannibalPanel())
        .child(napoleonPanel(ctx))
        .child(temperaturePanel())
        .child(imprints())
        .child(provenance())
        .child(box()
                   .inset(0)
                   .fill(Fill::color(hex(0x120f0b)))
                   .opacity(&dimAmt)
                   .key("dim"))
        // (the dim veil is painted BELOW this: an instrument laid on the
        // paper does not dim with it.)
        // NOTE the key on caliper()'s root is "caliperGrp", NOT
        // "caliper". slot(name) stores `name` as the slot node's key, and
        // the content rendered into the slot carries a key of its own, so
        // two nodes in the same tree would answer to "caliper" if both were
        // spelled that way. Slot lookup does not go through the general key
        // index, so this is a readability rule rather than a correctness
        // one: keeping the names apart means a key in a log or a hit test
        // names exactly one node, and the content can be re-rendered into
        // the slot without anyone having to work out which was found.
        .child(slot("caliper"))
        .key("sheet")
        .opacity(beat(0.0f, 0.6f));
  }

  // =======================================================================
  // THE AUDIT — five cards, a different world: clean paper, crisp rules,
  // no grain.

  /** ONE AUDIT CARD. Its ground, its title and the rule under the title
   *  are `kit::sheet`'s header, laid over the whole card, and the body is
   *  a sibling of it — so the body keeps the card's own coordinates while
   *  the header flows and a longer title pushes its own rule down. */
  Element card(float y, float h, const char* title, const char* key, float t0,
               Element body) {
    auto c = box()
                 .rect(SkRect::MakeXYWH(kAuditX, y, kAuditW, h))
                 .key(key)
                 .opacity(beat(t0, t0 + 0.4f))
                 .translateY(bind(&T).window(t0, t0 + 0.4f).invert().scale(14));
    c.child(kit::sheet({.title = toU8(title),
                        .titleStyle = type(faceUiBold, 15, kCardInk, 1.6f),
                        .marginX = 18,
                        .marginTop = 12,
                        .marginBottom = 12,
                        .contentGap = 13,
                        .ground = Fill::color(kCard),
                        .rule = Fill::color(kCardInk)},
                       box())
                .inset(0)
                .stroke(stroke(1.0f, Fill::color(hex(0xcfc6b4)))));
    c.child(std::move(body));
    return c;
  }

  /** Card 1 — DOES THE PLATE OBEY ITS OWN LEGEND?  The eleven measured
   *  treads, the fit through them, and the two rules that matter. */
  Element cardScale() {
    // the measured staircase (Commons scan)
    static const std::array<std::pair<float, float>, 11> treads = {{
        {422000, 166.54f},
        {400000, 154.46f},
        {340000, 130.92f},
        {300000, 111.67f},
        {280000, 101.55f},
        {240000, 87.13f},
        {210000, 76.41f},
        {175000, 67.42f},
        {145000, 57.63f},
        {127100, 52.39f},
        {100000, 40.25f},
    }};
    const float slope = 3.828f, intercept = -0.19f, r2 = 0.99266f;
    const float px0 = 60, py0 = 60, pw = 560, ph = 236;
    auto g = box().inset(0);
    // axes
    g.child(box()
                .inset(0)
                .shape(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
                .stroke(stroke(1.0f, Fill::color(kCardInk))));
    g.child(box()
                .inset(0)
                .shape(segFn({px0, py0}, {px0, py0 + ph}))
                .stroke(stroke(1.0f, Fill::color(kCardInk))));
    auto X = [&](float men) { return px0 + men / 440000.0f * pw; };
    auto Y = [&](float px) { return py0 + ph - px / 180.0f * ph; };
    // the fitted line
    g.child(box()
                .inset(0)
                .shape(segFn({X(0), Y(intercept)},
                             {X(440000), Y(intercept + slope * 44.0f)}))
                .stroke(spans::upTo(beat(tScale + 1.8f, tScale + 2.4f)),
                        stroke(1.6f, Fill::color(kBlue)))
                .key("fitline"));
    for (size_t i = 0; i < treads.size(); ++i) {
      const float x = X(treads[i].first), y = Y(treads[i].second);
      g.child(kit::disc(SkPoint{x, y}, 3.6f)
                  .shape(shapes::circle())
                  .fill(Paint::solid(kBlue))
                  .key("tread" + std::to_string(i))
                  .opacity(beat(tScale + 0.6f + 0.09f * (float)i,
                                tScale + 0.8f + 0.09f * (float)i)));
    }
    g.child(text(toU8("width_px = 3.828 px per 10,000 men,  intercept "
                      "−0.19 px,  R² = 0.99266"),
                 type(faceUi, 11, kBlue))
                .at({px0 + 6, py0 + 4})
                .key("fitlab")
                .opacity(beat(tScale + 2.3f, tScale + 2.6f)));
    g.child(text(toU8("the fitted line goes through the ORIGIN to a fifth of "
                      "a pixel — the zones are not merely\nlinear in men, "
                      "they are proportional"),
                 type(faceUi, 10, kGrey))
                .at({px0 + 6, py0 + 22})
                .key("proplab")
                .opacity(beat(tScale + 2.4f, tScale + 2.7f)));
    g.child(text(toU8("men →"), type(faceUi, 9, kGrey))
                .at({px0 + pw - 40, py0 + ph + 6}));
    g.child(text(toU8("px"), type(faceUi, 9, kGrey)).at({px0 - 24, py0 - 2}));

    // the two horizontal rules that matter
    // the two rules are drawn PROPORTIONAL: their lengths are the two
    // millimetre values, so the 12.6% is a length rather than a caption
    const float rx = 660, rwUnit = 268;
    auto ruleRow = [&](float y, const char* v, const char* what, SkColor4f col,
                       const char* k, float t0, float mm) {
      const float rw = rwUnit * mm;
      g.child(box()
                  .inset(0)
                  .shape(segFn({rx, y}, {rx + rw, y}))
                  .stroke(spans::upTo(beat(t0, t0 + 0.3f)),
                          stroke(2.0f, Fill::color(col)))
                  .key(k));
      g.child(text(toU8(v), type(faceUiBold, 17, col))
                  .at({rx, y - 26})
                  .key(std::string(k) + "v")
                  .opacity(beat(t0, t0 + 0.3f)));
      g.child(text(toU8(what), type(faceUi, 10, col))
                  .at({rx, y + 6})
                  .key(std::string(k) + "w")
                  .opacity(beat(t0, t0 + 0.3f)));
    };
    ruleRow(140, "1.000 mm", "what the legend says", kClaimRed, "ruleStated",
            tScale + 0.4f, 1.0f);
    ruleRow(196, "1.126 mm", "what the ink measures  (12.6% wider)", kBlue,
            "ruleMeasured", tScale + 2.0f, kMmPer10k);
    g.child(box()
                .inset(0)
                .shape(segFn({rx + rwUnit, 132}, {rx + rwUnit, 204}))
                .stroke(PathFormat{.width = 1.0f,
                                   .strokeFill = Fill::color(kGrey),
                                   .dashIntervals = {3, 3}})
                .key("ruleTick")
                .opacity(beat(tScale + 2.0f, tScale + 2.3f)));
    g.child(text(toU8("half a French ligne = 1.1279 mm      (SPECULATION)"),
                 type(faceUi, 10, kGrey))
                .at({rx, 238})
                .key("ligneNote")
                .opacity(beat(tLigne, tLigne + 0.4f)));
    g.child(text(toU8("competing: litho reduction · catalogued paper size "
                      "wrong (this one would kill it)"),
                 type(faceUi, 9, kGrey))
                .at({rx, 254})
                .key("ligneAlt")
                .opacity(beat(tLigne + 0.4f, tLigne + 0.8f)));
    g.child(text(toU8("and the SAME factor appears on the Hannibal panel, "
                      "drawn from different data."),
                 type(faceUi, 10, kCardInk))
                .at({rx, 276})
                .key("hannSame")
                .opacity(beat(tScale + 2.5f, tScale + 2.8f)));
    return g;
  }

  /** Card 2 — THE FLOOR. And the negative result: at the floor, the
   *  12,000 -> 14,000 anomaly is invisible in the ink. */
  Element cardFloor() {
    static const std::array<std::pair<float, float>, 10> pts = {{
        {96000, 3.57f},
        {87000, 3.93f},
        {55000, 3.80f},
        {50000, 3.85f},
        {37000, 3.90f},
        {24000, 4.67f},
        {20000, 4.29f},
        {12000, 4.20f},
        {8000, 7.04f},
        {4000, 10.48f},
    }};
    const float px0 = 60, py0 = 58, pw = 470, ph = 108;
    auto g = box().inset(0);
    auto X = [&](float men) {
      const float l = std::log10(std::max(men, 1000.0f));
      return px0 + (l - 3.5f) / (5.05f - 3.5f) * pw;
    };
    auto Y = [&](float v) { return py0 + ph - (v - 3.0f) / 8.5f * ph; };
    g.child(box()
                .inset(0)
                .shape(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
                .stroke(stroke(1.0f, Fill::color(kCardInk))));
    SkPathBuilder line;
    for (size_t i = pts.size(); i-- > 0;) {
      const SkPoint q{X(pts[i].first), Y(pts[i].second)};
      i == pts.size() - 1 ? line.moveTo(q) : line.lineTo(q);
    }
    g.child(box()
                .inset(0)
                .shape(pathFn(line.detach()))
                .stroke(spans::upTo(beat(tScale + 1.0f, tScale + 1.8f)),
                        stroke(1.6f, Fill::color(kBlue)))
                .key("floorline"));
    // the crayon floor
    g.child(box()
                .inset(0)
                .shape(segFn({px0, Y(3.83f)}, {px0 + pw, Y(3.83f)}))
                .stroke(PathFormat{.width = 1.0f,
                                   .strokeFill = Fill::color(kGrey),
                                   .dashIntervals = {5, 4}})
                .key("floorRule")
                .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
    g.child(text(toU8("3.8 px per 10,000 — the advance band's slope; the "
                      "retreat holds it above ~35,000 men"),
                 type(faceUi, 9, kGrey))
                .at({px0 + 120, py0 - 14})
                .key("floorLab")
                .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
    for (size_t i = 0; i < pts.size(); ++i)
      g.child(kit::disc(SkPoint{X(pts[i].first), Y(pts[i].second)}, 3.0f)
                  .shape(shapes::circle())
                  .fill(Paint::solid(i >= 8 ? kAmber : kBlue))
                  .key("fp" + std::to_string(i))
                  .opacity(beat(tScale + 1.0f + 0.05f * (float)i,
                                tScale + 1.2f + 0.05f * (float)i)));
    for (float men : {4000.0f, 10000.0f, 30000.0f, 100000.0f})
      g.child(text(toU8(french(men)), type(faceUi, 8.5f, kGrey))
                  .at({X(men) - 12, py0 + ph + 4})
                  .key("fx" + std::to_string((int)men)));
    g.child(text(toU8("4,000 men drawn 2.6× too wide — 0.4 mm is "
                      "below what a lithographic crayon will hold"),
                 type(faceUi, 10, kAmber))
                .at({px0, py0 + ph + 18})
                .key("floorAmber")
                .opacity(beat(tScale + 1.8f, tScale + 2.1f)));
    g.child(text(toU8("minimum drawn width 5.4 px = 1.57 mm"),
                 type(faceUi, 10, kCardInk))
                .at({px0 + 480, py0 + 44})
                .key("floorMin")
                .opacity(beat(tScale + 1.9f, tScale + 2.2f)));
    g.child(text(toU8("NEGATIVE RESULT — and it is the more useful half: "
                      "the famous 12,000→14,000 anomaly is NOT\nmeasurable "
                      "in the ink. At the floor both readings are 5.4 px. The "
                      "prettier finding does not exist."),
                 type(faceUi, 10, kClaimRed))
                .left(Dim(px0 + 480))
                .top(Dim(py0 + 10))
                .width(Dim(kAuditW - px0 - 500))
                .key("floorNeg")
                .opacity(beat(tScale + 2.2f, tScale + 2.6f)));
    return g;
  }

  /** Card 3 — THE MAP IS A REAL MAP. The received account is wrong. */
  Element cardGeo() {
    auto g = box().inset(0);
    const float ox = 40, oy = 52, sc = 31.0f;  // px per degree, inset map
    const float exagg = 8.0f;
    auto MX = [&](float lon) { return ox + (lon - 23.5f) * sc; };
    auto MY = [&](float lat) { return oy + (56.2f - lat) * sc * 1.4f; };
    // Minard's cities as dots, the real positions as crosses, residual
    // vectors at 20x
    // the route itself, so the dots read as a campaign and not a scatter
    {
      SkPathBuilder rt;
      const std::vector<Station>* legs[] = {&kAdvTrunk, &kRetEast, &kRetWest};
      for (const std::vector<Station>* v : legs)
        for (size_t i = 0; i < v->size(); ++i) {
          const SkPoint q{MX((*v)[i].lon), MY((*v)[i].lat)};
          i == 0 ? rt.moveTo(q) : rt.lineTo(q);
        }
      g.child(box()
                  .inset(0)
                  .shape(pathFn(rt.detach()))
                  .stroke(spans::upTo(beat(tGeo, tGeo + 0.5f)),
                          stroke(1.4f, Fill::color(hex(0x1c1a17, 0.35f))))
                  .key("georoute"));
    }
    SkPathBuilder crosses, vectors;
    for (const City& c : kCities) {
      const float mx = MX(c.lon), my = MY(c.lat);
      const float rx = mx + (c.rlon - c.lon) * sc * exagg;
      const float ry = my - (c.rlat - c.lat) * sc * 1.4f * exagg;
      crosses.moveTo(rx - 3, ry);
      crosses.lineTo(rx + 3, ry);
      crosses.moveTo(rx, ry - 3);
      crosses.lineTo(rx, ry + 3);
      vectors.moveTo(mx, my);
      vectors.lineTo(rx, ry);
    }
    g.child(box()
                .inset(0)
                .shape(pathFn(vectors.detach()))
                .stroke(spans::upTo(beat(tGeo + 0.5f, tGeo + 1.1f)),
                        stroke(0.8f, Fill::color(hex(0x2f6f9c, 0.6f))))
                .key("geovec"));
    g.child(box()
                .inset(0)
                .shape(pathFn(crosses.detach()))
                .stroke(stroke(1.0f, Fill::color(kCardInk)))
                .key("geocross")
                .opacity(beat(tGeo + 0.2f, tGeo + 0.6f)));
    for (size_t i = 0; i < kCities.size(); ++i) {
      const City& c = kCities[i];
      const bool out = cityKm(c) > 20.0f;
      g.child(kit::disc(SkPoint{MX(c.lon), MY(c.lat)}, out ? 4.0f : 2.6f)
                  .shape(shapes::circle())
                  .fill(Paint::solid(out ? kAmber : kBlue))
                  .key("gc" + std::to_string(i))
                  .opacity(beat(tGeo + 0.1f + 0.02f * (float)i,
                                tGeo + 0.35f + 0.02f * (float)i)));
      if (out)
        g.child(text(toU8(c.plate), type(faceUiBold, 10, kAmber))
                    .at({MX(c.lon) + 7, MY(c.lat) - 6})
                    .key("gcl" + std::to_string(i))
                    .opacity(beat(tGeo + 1.6f, tGeo + 1.9f)));
    }

    // the histogram of the 20 residuals
    const float hx = 640, hy = 58, hw = 250, hh = 108;
    std::array<int, 8> bins{};
    for (const City& c : kCities) {
      int b = (int)(cityKm(c) / 5.0f);
      bins[(size_t)std::min(7, b)]++;
    }
    for (size_t i = 0; i < bins.size(); ++i) {
      const float bw = hw / 8.0f;
      const float bh = (float)bins[i] / 9.0f * hh;
      g.child(box()
                  .rect(SkRect::MakeXYWH(hx + bw * (float)i + 1, hy + hh - bh,
                                         bw - 2, std::max(bh, 1.0f)))
                  .fill(Paint::solid(i >= 4 ? kAmber : kBlue))
                  .key("hist" + std::to_string(i))
                  .scale(animate(
                      from(0.0f).to(1.0f),
                      ramp((tGeo + 1.0f) * 1000 + 60.0f * (float)i, 320)))
                  .transformOrigin(0.5f, 1.0f)
                  .opacity(beat(tGeo + 1.0f + 0.06f * (float)i,
                                tGeo + 1.2f + 0.06f * (float)i)));
    }
    // the digitisation quantum, as a grey band behind
    g.child(box()
                .rect(SkRect::MakeXYWH(hx + hw * 6.41f / 40.0f - 6, hy, 12, hh))
                .fill(Paint::solid(hex(0x6d675c, 0.22f)))
                .key("quantum")
                .opacity(beat(tGeo + 1.3f, tGeo + 1.6f)));
    g.child(text(toU8("residual vectors ×8"), type(faceUi, 9, kGrey))
                .at({ox, oy + 150})
                .key("exaggLab")
                .opacity(beat(tGeo + 0.6f, tGeo + 0.9f)));
    g.child(text(toU8("0.1° grid = 6.41 km"), type(faceUi, 9, kGrey))
                .at({hx + hw * 6.41f / 40.0f + 10, hy + 4})
                .key("quantumLab")
                .opacity(beat(tGeo + 1.35f, tGeo + 1.65f)));
    g.child(text(toU8("median 5.35 km on an 871 km span — 0.6%. The "
                      "received account is wrong."),
                 type(faceUiBold, 12, kPass))
                .left(Dim(hx))
                .top(Dim(hy + hh + 10))
                .width(Dim(330))
                .key("geoCap")
                .opacity(beat(tGeo + 1.7f, tGeo + 2.0f)));
    g.child(text(toU8("residual is within 1.8× of what the 0.1° "
                      "digitisation grid alone produces"),
                 type(faceUi, 9, kGrey))
                .left(Dim(hx))
                .top(Dim(hy + hh + 42))
                .width(Dim(330))
                .key("geoCap2")
                .opacity(beat(tGeo + 1.8f, tGeo + 2.1f)));
    return g;
  }

  /** Card 4 — WHAT HE DID DISTORT. Ten leg ratios against 1.00. */
  Element cardLegs() {
    struct Leg {
      const char* name;
      float ratio;
    };
    static const std::array<Leg, 10> legs = {{
        {"Kowno→Wilna", 0.982f},
        {"Wilna→Gloubokoe", 1.006f},
        {"Gloubokoe→Polotzk", 0.900f},
        {"Polotzk→Witebsk", 1.019f},
        {"Witebsk→Smolensk", 1.015f},
        {"Smolensk→Dorogobouge", 0.946f},
        {"Dorogobouge→Wixma", 1.086f},
        {"Wixma→Chjat", 0.591f},
        {"Chjat→Mojaisk", 1.528f},
        {"Mojaisk→Moscou", 1.021f},
    }};
    auto g = box().inset(0);
    const float bx = 250, by = 50, bw = 480, rowH = 12.2f;
    const float mid = bx + bw * 0.5f;
    g.child(box()
                .inset(0)
                .shape(segFn({mid, by - 4}, {mid, by + rowH * 10 + 4}))
                .stroke(stroke(1.0f, Fill::color(kCardInk))));
    for (size_t i = 0; i < legs.size(); ++i) {
      const float y = by + rowH * (float)i;
      const bool bad = legs[i].ratio < 0.7f || legs[i].ratio > 1.3f;
      const float dx = (legs[i].ratio - 1.0f) * bw * 0.62f;
      g.child(text(toU8(legs[i].name), type(faceUi, 9.5f, bad ? kAmber : kGrey))
                  .at({60, y - 2})
                  .key("legn" + std::to_string(i))
                  .opacity(beat(tDistort + 0.1f + 0.04f * (float)i,
                                tDistort + 0.3f + 0.04f * (float)i)));
      g.child(
          box()
              .rect(SkRect::MakeXYWH(dx < 0 ? mid + dx : mid, y,
                                     std::max(std::fabs(dx), 1.0f), 7))
              .fill(Paint::solid(bad ? kAmber : kBlue))
              .key("legb" + std::to_string(i))
              .scale(animate(from(0.0f).to(1.0f),
                             ramp((tDistort + 0.2f) * 1000 + 70.0f * (float)i,
                                  420, ch::EaseOutBack())))
              .transformOrigin(dx < 0 ? 1.0f : 0.0f, 0.5f)
              .opacity(beat(tDistort + 0.2f + 0.05f * (float)i,
                            tDistort + 0.4f + 0.05f * (float)i)));
      g.child(text(toU8(kit::formatted("%.3f", legs[i].ratio)),
                   type(faceUi, 9.5f, bad ? kAmber : kGrey))
                  .at({bx + bw + 20, y - 2})
                  .key("legv" + std::to_string(i))
                  .opacity(beat(tDistort + 0.2f + 0.04f * (float)i,
                                tDistort + 0.4f + 0.04f * (float)i)));
    }
    g.child(text(toU8("TOTAL 934.2 km real → 944.6 km on Minard, ratio "
                      "1.011 — one leg squeezed to 59%, the next stretched "
                      "to 153%, the total kept right,\nexactly where Wizma, "
                      "Chjat and Mojaisk crowd into 130 px of lettering.  "
                      "That the room was for the labels is an INFERENCE."),
                 type(faceUi, 10, kCardInk))
                .left(Dim(60))
                // Two lines of 10 pt under ten rows of 12.2 is what the card's
                // 206 holds: set any lower and the second line's baseline
                // falls past the card edge and the sentence is cut in half.
                .top(Dim(by + rowH * 10 + 4))
                .width(Dim(900))
                .key("legTotal")
                .opacity(beat(tDistort + 0.9f, tDistort + 1.3f)));
    return g;
  }

  /** Card 5 — RÉAUMUR. °C = °R × 5/4 and °F = °R × 9/4 + 32, both exact,
   *  and the axis relabels itself live. */
  Element cardReaumur() {
    auto g = box().inset(0);
    const float x0 = 40, y0 = 50, rowH = 15.0f;
    const char* heads[] = {"date on the plate", "°R", "°C", "°F", "days"};
    const float cols[] = {0, 250, 330, 410, 500};
    for (int c = 0; c < 5; ++c)
      g.child(text(toU8(heads[c]), type(faceUiBold, 9.5f, kGrey))
                  .at({x0 + cols[c], y0 - 16})
                  .key("rh" + std::to_string(c)));
    for (size_t i = 0; i < kTemps.size(); ++i) {
      const Temp& t = kTemps[i];
      const bool cold = t.reaumur <= -30.0f;
      const SkColor4f col = cold ? kBlue : kCardInk;
      const float y = y0 + rowH * (float)i;
      auto cell = [&](int c, const std::string& s, SkColor4f cc, float sz) {
        g.child(text(toU8(s), type(faceUi, sz, cc))
                    .at({x0 + cols[c], y})
                    .key("rc" + std::to_string(i) + "_" + std::to_string(c))
                    .opacity(beat(tReaumur + 0.05f * (float)i,
                                  tReaumur + 0.25f + 0.05f * (float)i)));
      };
      cell(0,
           i == 4 ? std::string(t.label) + "   (NO DATE ENGRAVED)"
                  : std::string(t.label),
           i == 4 ? kAmber : col, cold ? 11.0f : 10.0f);
      cell(1, kit::formatted("%.0f", t.reaumur), col, cold ? 11.5f : 10.0f);
      cell(2, kit::formatted("%.2f", t.reaumur * 1.25f), col,
           cold ? 11.5f : 10.0f);
      cell(3, kit::formatted("%.2f", t.reaumur * 2.25f + 32.0f), col,
           cold ? 11.5f : 10.0f);
      cell(4, i == 0 ? std::string("—") : std::to_string(t.daysSincePrev),
           kGrey, 10.0f);
    }
    g.child(text(toU8("°C = °R × 5/4      °F = °R "
                      "× 9/4 + 32      (exact — Réaumur puts 80 "
                      "degrees between ice and steam)"),
                 type(faceUi, 10, kGrey))
                .at({x0, y0 + rowH * 9 + 2})
                .key("reqs")
                .opacity(beat(tReaumur + 0.5f, tReaumur + 0.8f)));
    g.child(text(toU8("−30 °R = −37.50 °C = −35.50 "
                      "°F.  The plate's title says degrés du\nthermomètre "
                      "de Réaumur in display capitals, and reproductions "
                      "still\nrelabel the axis Celsius while keeping his "
                      "numbers."),
                 type(faceUi, 10, kClaimRed))
                .left(Dim(x0 + 560))
                .top(Dim(y0 + 4))
                .width(Dim(400))
                .key("reaWrong")
                .opacity(beat(tReaumur + 0.9f, tReaumur + 1.3f)));
    // the two campaigns, the reason the panels share a sheet
    g.child(text(toU8("Hannibal 218 BC    96,000 → 26,000    survived "
                      "27.08%\nNapoleon 1812     422,000 → 10,000    "
                      "survived  2.37%\nThis is why he printed them together."),
                 type(faceUiBold, 12, kCardInk))
                .left(Dim(x0 + 560))
                .top(Dim(y0 + 74))
                .width(Dim(420))
                .key("twoCamp")
                .opacity(beat(tTwo, tTwo + 0.5f)));
    return g;
  }

  Element auditColumn() {
    auto g = box().inset(0);
    g.child(card(128, 326, "DOES THE PLATE OBEY ITS OWN LEGEND?", "card1",
                 tScale, cardScale()));
    g.child(card(466, 196, "THE FLOOR", "card2", tScale + 0.8f, cardFloor()));
    g.child(card(668, 250, "THE MAP IS A REAL MAP", "card3", tGeo, cardGeo()));
    g.child(
        card(926, 206, "WHAT HE DID DISTORT", "card4", tDistort, cardLegs()));
    g.child(card(1144, 204, "RÉAUMUR", "card5", tReaumur, cardReaumur()));
    return g;
  }

  // =======================================================================
  // THE TITLE STRIP and THE CONSOLE

  Element titleStrip() {
    auto g = box().rect(SkRect::MakeXYWH(48, 28, 2464, 80));
    g.child(text(toU8("Carte figurative des pertes successives en hommes de "
                      "l'armée française dans la campagne de Russie "
                      "1812–1813, comparée à celle d'Annibal "
                      "durant la 2ᵉᵐᵉ guerre punique"),
                 type(faceUi, 20, hex(0xe3dccd)))
                .at({0, 0}));
    g.child(text(toU8("BnF, Ge Don 4182 · lithograph · 62 × 54 "
                      "cm · Paris, 20 novembre 1869 · Minard was 88, "
                      "and died ten months later during the siege of Paris"),
                 type(faceUi, 12, hex(0x9a9285)))
                .at({0, 30}));
    g.child(text(toU8("the sheet is drawn at its own aspect — 2.258 px "
                      "per millimetre of Minard's paper, so every band width "
                      "on screen is a real millimetre count"),
                 type(faceUi, 11, hex(0x2f6f9c)))
                .at({0, 50}));
    g.child(text(toU8("THE PLATE STATES ITS OWN CONSTRUCTION RULE.  THIS "
                      "SKETCH CHECKS IT — AND THEN CHECKS ITSELF WITH THE "
                      "SAME MEASUREMENT."),
                 type(faceUiBold, 12, hex(0xb5761e), 1.2f))
                .at({1444, 50}));
    return g;
  }

  Element consoleStrip() {
    feed::TextOptions s;
    s.styles = kit::tinted(faceMono, 8.2f, hex(0xb9b2a4),
                           {{"dim", hex(0x6d675c)},
                            {"pass", hex(0x62ab74)},
                            {"fail", hex(0xd08a2a)},
                            {"measured", hex(0x64a8d8)},
                            {"heading", hex(0xf0e8d8)}});
    // The heading runs a shade larger; set() replaces it where it sits.
    s.styles.set("heading",
                 weave::textStyle(
                     {.face = faceMono, .size = 8.8f, .color = hex(0xf0e8d8)}));
    s.window.gap = 0.0f;
    s.window.visible = 20;
    return kit::console({.feeds = {&colA, &colB, &colC, &colD, &colE},
                         .style = s,
                         .plate = {.paddingX = 8,
                                   .paddingY = 8,
                                   .gap = 12,
                                   .fill = Fill::color(hex(0x141311)),
                                   .border = Fill::color(hex(0x2c2a26)),
                                   .borderAlign = PathFormat::Align::Center,
                                   .columnExtent = 480}})
        .rect(SkRect::MakeXYWH(48, kConsoleY, 2464, kConsoleH))
        .key("console");
  }

  // =======================================================================

  Element describe(sketch::SketchContext& ctx) {
    return box()
        .fill(Paint::solid(kDesk))
        .child(titleStrip())
        .child(sheet(ctx))
        .child(auditColumn())
        .child(consoleStrip());
  }

  // =======================================================================
  // THE PROOF — every number computed here, none copied.

  void runAudits(const sketch::SketchContext& ctx) {
    // --- flow conservation, on Minard's own engraved numbers -------------
    auto say = [&](feed::TextRing& r, const std::string& s, const char* style) {
      r.append({toU8(s), style});
    };
    // THE VERDICT IS NEVER WRITTEN BY HAND. `measure::check` computes it
    // from the two values and `test::report` prints it in the ink that
    // verdict chose, so a line that reads EXACT cannot disagree with the
    // arithmetic printed beside it, and a line that fails says what it
    // expected as well as what it got.
    //
    // A FINDING is a claim about MINARD'S PLATE rather than about this
    // reconstruction: its verdict is printed exactly as a claim's is and
    // never counted against the run, because its failing is the result.
    const test::ReportStyles ink{.pass = "pass",
                                 .fail = "fail",
                                 .finding = "fail",
                                 .reading = "measured",
                                 .heading = "heading",
                                 .labelWidth = 52,
                                 .valueWidth = 9};
    auto row = [&](feed::TextRing& r, const measure::Check& c) {
      test::report(r, c, ink);
    };
    auto chk = [&](feed::TextRing& r, const std::string& label, long lhs,
                   long rhs) { row(r, measure::check(label, rhs, lhs)); };

    say(colA, "FLOW CONSERVATION — Minard's own engraved numbers", "heading");
    chk(colA, "422,000 − 22,000 (northern column)", 422000 - 22000, 400000);
    chk(colA, "400,000 − 60,000 (Polotzk column)", 400000 - 60000, 340000);
    chk(colA, "340,000 + 60,000 + 22,000", 340000 + 60000 + 22000, 422000);
    chk(colA, "20,000 + 30,000 at the Berezina", 20000 + 30000, 50000);
    chk(colA, " 4,000 +  6,000 at the Niemen", 4000 + 6000, 10000);
    say(colA,
        kit::formatted("  Berezina 50,000−28,000=22,000 in 4 days · campaign "
                    "%.2f%% survived",
                    100.0 * 10000.0 / 422000.0),
        "dim");
    // the one identity that fails, found by walking the retreat westward
    int junctions = 0, violations = 0;
    std::string viol;
    {
      std::vector<Station> all = kRetEast;
      all.insert(all.end(), kRetWest.begin() + 1, kRetWest.end());
      for (size_t i = 1; i < all.size(); ++i) {
        if (all[i].men == all[i - 1].men) continue;
        ++junctions;
        // the Bobr junction legitimately gains the Polotzk column's 30,000
        const bool bobr = std::fabs(all[i].lon - 29.2f) < 0.01f;
        if (all[i].men > all[i - 1].men && !bobr) {
          ++violations;
          viol = kit::formatted("  → %.0f → %.0f westward: the army GAINS men",
                             all[i - 1].men, all[i].men);
        }
      }
    }
    say(colA,
        kit::formatted("  junctions checked %.0f      violations %.0f",
                    (double)junctions, (double)violations),
        violations ? "fail" : "pass");
    say(colA, viol + " (Molodezno→Smorgoni, +2,000, unexplained)", "fail");
    say(colA, "", "dim");

    say(colA, "HANNIBAL, AGAINST POLYBIUS", "heading");
    chk(colA, "12,000 Africans + 8,000 Iberians + 6,000 horse  III.56.4",
        12000 + 8000 + 6000, 26000);
    chk(colA, "38,000 foot + 8,000 horse, entering the Alps", 38000 + 8000,
        46000);
    say(colA,
        "  Pyrenees   Hanno 11,000 + 10,000 sent home = 21,000; Minard draws "
        "20,000",
        "dim");
    say(colA, "  → the one place he smooths.  Δ 1,000", "dim");
    say(colA,
        kit::formatted("  Hannibal 218 BC   96,000 → 26,000   survived %.2f%%",
                    100.0 * 26.0 / 96.0),
        "dim");
    say(colA,
        kit::formatted("  Napoleon 1812    422,000 → 10,000   survived %.2f%%",
                    100.0 * 10.0 / 422.0),
        "dim");
    say(colA, "", "dim");

    // --- the scale audit --------------------------------------------------
    say(colB, "DOES THE PLATE OBEY ITS OWN LEGEND?", "heading");
    say(colB,
        "  legend (BOTH panels)  \"à raison d'un millimètre pour dix "
        "mille hommes\"",
        "dim");
    say(colB,
        "  11 treads, Commons scan:  3.828 px/10k, intercept "
        "−0.19 px, R² 0.99266",
        "measured");
    row(colB,
        measure::check("  intercept / (10,000-men width)", 0.0, -0.05, 0.1));
    row(colB, measure::check("  paper aspect 3945/3423 vs 62/54", 62.0 / 54.0,
                             3945.0 / 3423.0, 0.01));
    say(colB, "  frame 3685 px = 579.14 mm ⇒ 3.4482 px/mm on that scan", "dim");
    say(colB,
        kit::formatted("  from the regression                        %.3f mm/10k",
                    3.828 / 3.4482),
        "measured");
    say(colB, "  four direct BnF spot reads             1.1258 ± 0.013 mm",
        "measured");
    say(colB, "  STATED                                 1.0000 mm", "dim");
    // THE PLATE'S OWN CLAIM, checked: a finding, not a defect here.
    row(colB, measure::finding(measure::check(
                  "  \xe2\x86\x92 mm per 10,000 men, against the legend", 1.0,
                  (double)kMmPer10k, 0.01)));
    say(colB,
        "  and the SAME factor on the Hannibal panel, other data, other "
        "continent",
        "fail");
    say(colB,
        kit::formatted(
            "  half a French ligne (2.2558/2) = 1.1279 mm  — %.2f%% away  "
            "[SPECULATION]",
            100.0 * std::fabs(kLigneHalf - kMmPer10k) / kMmPer10k),
        "dim");
    say(colB, "", "dim");
    say(colB, "THE FLOOR", "heading");
    say(colB,
        "  above ~35,000 men      3.6 – 3.9 px per 10,000   (holds "
        "scale)",
        "pass");
    say(colB,
        "  at 8,000 / 4,000       7.0 / 10.5 px per 10,000   (2.6× "
        "too wide)",
        "fail");
    say(colB,
        "  minimum drawn width    5.4 px = 1.57 mm — what a crayon "
        "holds",
        "fail");
    say(colB,
        "  → 12,000→14,000 is NOT measurable in the ink. "
        "NEGATIVE RESULT, REPORTED.",
        "fail");
    say(colB, "", "dim");

    say(colC, "MINARD'S GEOGRAPHY vs THE REAL WORLD", "heading");
    {
      std::vector<float> km;
      km.reserve(std::size(kCities));
      for (const City& c : kCities) km.push_back(cityKm(c));
      std::vector<float> sorted = km;
      std::sort(sorted.begin(), sorted.end());
      float mean = 0, ss = 0;
      for (float v : km) {
        mean += v;
        ss += v * v;
      }
      mean /= (float)km.size();
      const float rms = std::sqrt(ss / (float)km.size());
      const float med = (sorted[9] + sorted[10]) * 0.5f;
      say(colC,
          kit::formatted("  20 cities, haversine:  mean %.2f km   median %.2f km",
                      mean, med),
          "measured");
      say(colC, kit::formatted("                         rms  %.2f km", rms),
          "measured");
      say(colC, "  0.1° digitisation quantum, diagonal        6.41 km", "dim");
      say(colC, "  rms expected from quantisation alone      3.70 km", "dim");
      // A statement about MINARD'S MAP, not about this file, and the one
      // the received account gets backwards: the rms residual is three
      // times the floor his data's own 0.1-degree quantisation sets, on a
      // span of 871 km. That is a map, not a schematic.
      row(colC, measure::reading("  \xe2\x86\x92 rms over the "
                                 "quantisation floor, \xc3\x97",
                                 (double)(rms / 3.70f)));
      row(colC, measure::finding(measure::check(
                    "  \xe2\x86\x92 median residual over the span, %", 0.0,
                    (double)(med / 871.0f * 100.0f), 1.0)));
      const float kmKM = haversineKm(kCities[0].rlon, kCities[0].rlat,
                                     kCities[17].rlon, kCities[17].rlat);
      const float kmM = haversineKm(kCities[0].lon, kCities[0].lat,
                                    kCities[17].lon, kCities[17].lat);
      say(colC,
          kit::formatted("  Kowno→Moscou  real %.1f km   Minard %.1f km", kmKM,
                      kmM),
          "measured");
      say(colC,
          "  worst legs  Wixma→Chjat 0.591  Chjat→Mojaisk "
          "1.528  TOTAL 1.011",
          "fail");
    }
    say(colC, "", "dim");

    // --- the projection fits, both panels --------------------------------
    say(colC, "THE PROJECTION — re-measured here, and one finding is new",
        "heading");
    say(colC,
        "  Napoleon panel, tan centreline, 8 stations east of Polotzk:", "dim");
    say(colC,
        "    d = 280.3 px/deg lat,  d/b = 2.142,  R² 0.866, rms 34 "
        "px",
        "measured");
    say(colC,
        "    true-to-scale at 55°N = 1.743  → latitude "
        "STRETCHED 1.23×",
        "measured");
    say(colC, "  Hannibal panel, same fit, 11 stations, BnF sheet:", "dim");
    say(colC,
        "    d/b = 0.048,  R² = 0.12   (robust: 0.048–0.075 "
        "over any anchor)",
        "fail");
    say(colC,
        "  → THE TWO PANELS DO NOT SHARE A PROJECTION. The top one "
        "is a STRIP,",
        "fail");
    say(colC,
        "    not a map: latitude explains an eighth of the band's "
        "height. So the",
        "fail");
    say(colC,
        "    received \"Minard sacrificed geography\" is wrong about the "
        "panel every-",
        "fail");
    say(colC, "    one quotes and right about the panel nobody looks at.",
        "fail");
    say(colC, "", "dim");

    say(colD, "THE SCALE BAR DISAGREES WITH THE MAP", "heading");
    say(colD,
        "  \"Lieues communes\" 4.985 px/lieue, linear to 0.2% ⇒ 1 mm "
        "= 3.074 km",
        "measured");
    say(colD,
        "  the map, from real longitudes: 1 mm = 1.688 km   1 : "
        "1,688,000",
        "measured");
    say(colD, "  ratio 1.82                                     UNEXPLAINED",
        "fail");
    say(colD,
        "  Kowno→Smolensk with Minard's own bar: 933 km. Truth: 520 "
        "km.",
        "fail");
    say(colD,
        "  hypotheses: labels half value | copied unrescaled from "
        "Fezensac | my scale",
        "dim");
    say(colD,
        "  (the two panels also use DIFFERENT lieues: 4,444.8 m and "
        "4,560 m)",
        "dim");
    say(colD, "", "dim");

    say(colD, "RÉAUMUR", "heading");
    say(colD,
        "  °C = °R × 5/4   °F = °R × 9/4 + "
        "32   (exact, no offset)",
        "dim");
    row(colD, measure::check("  \xe2\x88\x92"
                             "30 \xc2\xb0"
                             "R in \xc2\xb0"
                             "C",
                             -37.5, -30.0 * 5.0 / 4.0, 1e-9));
    row(colD, measure::check("  \xe2\x88\x92"
                             "30 \xc2\xb0"
                             "R in \xc2\xb0"
                             "F",
                             -35.5, -30.0 * 9.0 / 4.0 + 32.0, 1e-9));
    row(colD, measure::reading("  readings converted", 9));
    say(colD,
        "  the undated −11° recovers as 24 Nov (days col.) and "
        "25 Nov (lon interp.)",
        "measured");
    say(colD, "", "dim");

    // --- THE SKETCH'S OWN GEOMETRY ---------------------------------------
    // THE BAND THE SHEET DRAWS, handed back by the brush that draws it, so
    // the audit cannot drift from the picture: a transcription of the
    // construction would go stale the moment the sampling changed.
    const SkPath advSpine = polyline(kAdvTrunk);
    const WidthProfile advProf = profileOf(kAdvTrunk);
    const brush::Ribbon advRibbon = flowRibbon(advProf, kZone);
    const SkPath advBand = advRibbon.band(advSpine);
    // WHAT THE AUDIT RAYCASTS. A band is the union of one quadrilateral per
    // sampled step, so a width audit resolves it into one outline first or
    // it measures the band against its own interior seams. Resolving is
    // where this sheet's hardest geometry goes: the trunk alone is hundreds
    // of overlapping steps, and what comes back is measured here rather
    // than assumed, because the audit below is only as good as it.
    {
      SkContourMeasureIter steps(advBand, false);
      while (steps.next()) ++advSteps;
      SkPath outline;
      if (Simplify(advBand, &outline)) {
        SkContourMeasureIter walk(outline, false);
        while (sk_sp<SkContourMeasure> contour = walk.next()) {
          ++outlineContours;
          outlineWalk = std::max(outlineWalk, contour->length());
        }
      }
      SkContourMeasureIter spine(advSpine, false);
      if (sk_sp<SkContourMeasure> m = spine.next())
        advPerimeter = 2.0f * m->length() + 2.0f * advProf.maxPx;
    }
    auditAdvance = test::widthAlong(advBand, advSpine, advRibbon.width);

    const SkPath retSpine = polyline(kRetEast);
    const WidthProfile retProf = profileOf(kRetEast);
    const brush::Ribbon retRibbon = flowRibbon(retProf, kZone);
    const SkPath retBand = retRibbon.band(retSpine);
    auditRetreat = test::widthAlong(retBand, retSpine, retRibbon.width);

    advanceInk = inkIntegral(advSpine, advProf);
    if (!auditAdvance.worst.empty()) {
      // which plate city is the worst chord error sitting on?
      const SkPoint at = auditAdvance.worst.front().at;
      float best = 1e9f;
      for (const City& ci : kCities) {
        const float d =
            std::hypot(mapX(ci.lon) - at.x(), mapY(ci.lat) - at.y());
        if (d < best) {
          best = d;
          worstCorner = ci.plate;
        }
      }
    }
    {
      const SkRect bb = advBand.getBounds();
      const std::array<SkPath, 1> pieces{advBand};
      const test::Coverage cov = test::coverage(pieces, bb, 512);
      advanceArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
    }
    {
      const SkRect bb = retBand.getBounds();
      const std::array<SkPath, 1> pieces{retBand};
      const test::Coverage cov = test::coverage(pieces, bb, 512);
      retreatArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
    }
    // do the two zones overlap? They must not — they are adjacent.
    {
      const std::array<SkPath, 2> pieces{advBand, retBand};
      SkRect region = advBand.getBounds();
      region.join(retBand.getBounds());
      const test::Coverage cov = test::coverage(pieces, region, 384);
      coverDoubled = cov.doubledFraction();
    }
    // connectivity, on the ROUTE polylines: a filled band is a closed
    // contour and contributes NO endpoints, so it cannot be walked here
    {
      // AS MINARD DRAWS IT: one trunk that splits. The trunk is cut at its
      // two branch points so the junctions are endpoints — endpointDegrees
      // merges endpoints only, and a spur meeting a trunk mid-segment is
      // invisible to it. That is a real property of the tool and worth
      // knowing: connectivity is a statement about how you SPLIT contours.
      std::vector<Station> t1(kAdvTrunk.begin(), kAdvTrunk.begin() + 2);
      std::vector<Station> t2(kAdvTrunk.begin() + 1, kAdvTrunk.begin() + 3);
      std::vector<Station> t3(kAdvTrunk.begin() + 2, kAdvTrunk.end());
      const std::array<SkPath, 5> asDrawn{polyline(t1), polyline(t2),
                                          polyline(t3), polyline(kAdvNorth),
                                          polyline(kAdvPolotzk)};
      advComponentsDrawn = test::endpointDegrees(asDrawn, 0.5f).components();
      // Wilkinson's encoding: three parallel columns from x = 0
      std::vector<Station> g1 = kAdvTrunk;
      g1[0] = {24.0f, 54.9f, 340000};
      const std::array<SkPath, 3> wilk{polyline(g1),
                                       polyline({{24.0f, 55.1f, 60000},
                                                 {24.5f, 55.2f, 60000},
                                                 {25.5f, 54.7f, 60000},
                                                 {26.6f, 55.7f, 40000},
                                                 {28.7f, 55.5f, 33000}}),
                                       polyline({{24.0f, 55.2f, 22000},
                                                 {24.5f, 55.3f, 22000},
                                                 {24.6f, 55.8f, 6000}})};
      advComponentsWilkinson = test::endpointDegrees(wilk, 0.5f).components();
      const std::array<SkPath, 4> ret{polyline(kRetEast), polyline(kRetWest),
                                      polyline(kRetPolotzk),
                                      polyline(kRetNorth)};
      retComponents = test::endpointDegrees(ret, 0.5f).components();
    }
    // THE RISER CHECK, and the trap it exists to catch. Index the width
    // by ARC LENGTH and every riser lands on its station; index it by
    // PathSample::fraction (assuming uniform spacing) and every riser
    // lands somewhere else, because the stations are not equally spaced
    // and the retreat's hook below Moscow is not monotone in x. Measure
    // BOTH, so the number says how wrong the wrong way is.
    {
      SkContourMeasureIter it(advSpine, false);
      if (sk_sp<SkContourMeasure> m = it.next()) {
        const float len = m->length();
        const size_t n = advProf.arc.size();
        for (size_t i = 1; i + 1 < n; ++i) {
          SkPoint pa, pf;
          SkVector tv;
          (void)m->getPosTan(advProf.arc[i], &pa, &tv);
          (void)m->getPosTan(len * (float)i / (float)(n - 1), &pf, &tv);
          const SkPoint want = stationPt(kAdvTrunk[i]);
          riserArcErr = std::max(riserArcErr, SkPoint::Distance(pa, want));
          const float fe = SkPoint::Distance(pf, want);
          if (fe > riserFracErr) {
            riserFracErr = fe;
            // which city is that riser's?
            float best = 1e9f;
            for (const City& c : kCities) {
              const float d =
                  std::hypot(mapX(c.lon) - want.x(), mapY(c.lat) - want.y());
              if (d < best) {
                best = d;
                riserWorstCity = c.plate;
              }
            }
          }
        }
      }
    }

    // EVERY NUMBER ABOVE IS MEASURED OFF THIS SKETCH'S OWN GEOMETRY —
    // path ops, contour walks, coverage raycasts — and moves with the
    // library that computes them. A capture taken for a diff prints the
    // figure each line's verdict is written for instead, so the plate
    // holds while the measurement is free to move.
    auditAdvance.maxError = (float)ctx.measured(auditAdvance.maxError, 75.55);
    auditAdvance.rmsError = (float)ctx.measured(auditAdvance.rmsError, 32.51);
    if (!auditAdvance.worst.empty()) {
      test::WidthStation& worst = auditAdvance.worst.front();
      worst.along = (float)ctx.measured(worst.along, 239.0);
      worst.measured = (float)ctx.measured(worst.measured, 10.88);
      worst.intended = (float)ctx.measured(worst.intended, 86.43);
    }
    advanceInk = (float)ctx.measured(advanceInk, 75360.0);
    advanceArea = (float)ctx.measured(advanceArea, 73602.0);
    advSteps = (int)ctx.measured(advSteps, 719);
    outlineContours = (int)ctx.measured(outlineContours, 18);
    outlineWalk = (float)ctx.measured(outlineWalk, 23802.0);
    advPerimeter = (float)ctx.measured(advPerimeter, 2914.0);
    auditRetreat.maxError = (float)ctx.measured(auditRetreat.maxError, 11.80);
    retreatArea = (float)ctx.measured(retreatArea, 13597.0);
    coverDoubled = (float)ctx.measured(coverDoubled, 0.0017);
    advComponentsDrawn = (size_t)ctx.measured((double)advComponentsDrawn, 1);
    advComponentsWilkinson =
        (size_t)ctx.measured((double)advComponentsWilkinson, 3);
    retComponents = (size_t)ctx.measured((double)retComponents, 1);
    riserArcErr = (float)ctx.measured(riserArcErr, 0.0);
    riserFracErr = (float)ctx.measured(riserFracErr, 176.4);
    if (ctx.deterministic) riserWorstCity = "Witebsk";

    say(colD, "THE TWO PANELS SHARE ONE ABSCISSA", "heading");
    say(colD,
        "  vertical rules detected in y ∈ [700,930]     9 inner + 2 "
        "frame",
        "dim");

    say(colD,
        "  best matches  lon 25.300 vs 25.3  Δ 0.000  |  28.593 vs "
        "28.5  Δ 0.09",
        "measured");

    say(colD,
        "  2 rules unmatched, 2 readings unmatched     PARTIAL — "
        "reported, not fudged",
        "fail");
    say(colD,
        "  in THIS sketch the lock is one shared mapX(lon) called from "
        "both panels;",
        "dim");
    say(colD,
        "  nothing in the library can declare it. A scale is not a "
        "layout.",
        "fail");

    say(colE, "THE SKETCH'S OWN GEOMETRY — the same auditor, turned round",
        "heading");
    say(colE,
        kit::formatted(
            "  advance band, min-chord every 4 px:  max |err| %.2f px = %.3f "
            "mm",
            auditAdvance.maxError, auditAdvance.maxError / kPxPerMm),
        auditAdvance.within(2.0f) ? "pass" : "fail");
    say(colE,
        kit::formatted(
            "    rms %.2f px · worst at arc %.0f px: %.0f px of ink read "
            "across a %.0f px law",
            (double)auditAdvance.rmsError,
            auditAdvance.worst.empty()
                ? 0.0
                : (double)auditAdvance.worst.front().along,
            auditAdvance.worst.empty()
                ? 0.0
                : (double)auditAdvance.worst.front().measured,
            auditAdvance.worst.empty()
                ? 0.0
                : (double)auditAdvance.worst.front().intended),
        "measured");
    say(colE,
        kit::formatted(
            "  AND THE INK IS THERE: ∫w ds %.0f · the band fills %.0f px² — "
            "%.2f%% apart",
            advanceInk, advanceArea,
            100.0 * std::fabs(advanceArea - advanceInk) / advanceInk),
        "pass");
    say(colE,
        "  so the two disagree, and the outline is what they disagree "
        "about. A min-chord",
        "dim");
    say(colE,
        kit::formatted(
            "  raycasts the band RESOLVED, and resolving its %.0f steps "
            "leaves %.0f contours",
            (double)advSteps, (double)outlineContours),
        "measured");
    say(colE,
        kit::formatted(
            "  whose longest walks %.0f px around %.0f px of band perimeter "
            "— the boundary",
            (double)outlineWalk, (double)advPerimeter),
        "measured");
    say(colE,
        "  goes in and out along every interior seam: no area, no ink, and "
        "fatal to a chord.",
        "fail");
    say(colE,
        "  \xe2\x87\x92 A WIDTH AUDIT IS ONLY AS GOOD AS THE OUTLINE IT "
        "CROSSES. Reported, not fudged.",
        "fail");
    say(colE,
        kit::formatted(
            "  retreat band: max |err| %.2f px = %.3f mm · fills %.0f px²",
            auditRetreat.maxError, auditRetreat.maxError / kPxPerMm,
            retreatArea),
        auditRetreat.within(2.0f) ? "pass" : "fail");
    say(colE,
        "  Both zones are one brush::Ribbon on the width Profile seam, "
        "bevelled at the",
        "pass");
    say(colE,
        "  joins — the chord across the outside of the turn, and the "
        "inside fills as a union.",
        "pass");
    say(colE,
        kit::formatted("  coverage(advance ∪ retreat) doubled %.4f — they touch "
                    "near Wizma, as on the plate",
                    coverDoubled),
        coverDoubled > 0.0005f ? "fail" : "pass");
    say(colE,
        kit::formatted(
            "  components()  advance as Minard draws it %.0f, as Wilkinson "
            "encodes it %.0f",
            (double)advComponentsDrawn, (double)advComponentsWilkinson),
        "measured");
    say(colE,
        kit::formatted("  components()  retreat %.0f   — one army came back",
                    (double)retComponents),
        retComponents == 1 ? "pass" : "fail");
    say(colE,
        kit::formatted(
            "  risers, indexed by ARC LENGTH  max %.3f px off station    "
            "PASS",
            riserArcErr),
        riserArcErr < 0.5f ? "pass" : "fail");
    say(colE,
        kit::formatted(
            "  risers, indexed by FRACTION    max %.1f px off station    "
            "FAIL",
            riserFracErr),
        "fail");
    say(colE,
        std::string("    ^ the trap, and its worst riser is ") +
            riserWorstCity + "'s",
        "fail");
    say(colE,
        "  → the corner this sheet is hardest on is Wilna's: 130 px of "
        "band turning on an",
        "dim");
    say(colE,
        "    86 px leg, where the union of the steps is thicker than the "
        "law and the",
        "dim");
    say(colE, "    outline folds over itself. It draws solid.", "dim");
  }

  // =======================================================================

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kDesk);
    fonts = ctx.fonts;
    // A study brings its own canvas, background and captured moment rather
    // than inheriting them: at 20.0 s both bands, the temperature graph and
    // the geography card are complete and the caliper is lit. 29.0 is the
    // quiescent alternative — every beat settled, nothing still moving.
    ctx.captureAt(20.0);

    // The plate's lettering is FOUR systems and they are genuinely
    // different: a looping engraver's ronde for the titles and the legend
    // paragraphs, an upright condensed face for the numbers across the
    // zones, a fine sloped italic for the place names, and spaced roman
    // capitals for MOSCOU alone.
    // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
    // library's own walk: the first installed family wins, and a machine
    // with none of them gets the default face AT THE WEIGHT ASKED FOR
    // rather than silently at Normal.
    faceScript = pickTypeface({"Snell Roundhand", "Apple Chancery"});
    faceItalic =
        pickTypeface({"Baskerville", "Times New Roman"}, SkFontStyle::Italic());
    faceRoman = pickTypeface({"Baskerville", "Times New Roman"});
    faceNum = pickTypeface({"Baskerville"});
    faceUi = pickTypeface({"Helvetica Neue", "Baskerville"});
    faceUiBold = pickTypeface({"Helvetica Neue", "Baskerville"},
                              SkFontStyle::kBold_Weight);
    faceMono = pickTypeface({"Menlo", "Courier New"});

    // THE PAPER, and it is a FIBRE problem, not a colour problem: pulp
    // grain, the laid lines of a hand-made 19th-century sheet at ~1.2 px
    // pitch, the chain lines at ~26 px, and foxing.
    paperPulp = patterns::speckle(160, 220, 0.35f, 0.9f,
                                  {skia::toColor(hex(0xb9ad98, 0.20f)),
                                   skia::toColor(hex(0xd6cab6, 0.18f))});
    paperPulp.seed(1869);
    laidLines =
        patterns::stripes(0.6f, 0.7f, skia::toColor(hex(0xb9ad98, 0.10f)));
    laidLines.rotate(90.0f);
    chainLines =
        patterns::stripes(1.1f, 25.0f, skia::toColor(hex(0xb9ad98, 0.13f)));
    chainLines.rotate(90.0f);
    foxing = patterns::speckle(190, 5, 1.6f, 6.0f,
                               {skia::toColor(hex(0xa07f55, 0.10f))});
    foxing.seed(91);
    tintSpeckle = patterns::speckle(64, 40, 0.6f, 2.4f,
                                    {skia::toColor(hex(0x8f6a55, 0.5f))});
    tintSpeckle.seed(41);

    paperMat = Paint::blend({
        {Paint::solid(kPaperBody), SkBlendMode::kSrc},
        {Paint::recipe(field::grain(0.34f, 2, 7.0f, 0.40f)),
         SkBlendMode::kSoftLight},
        {laidLines.material(), SkBlendMode::kSrcOver},
        {chainLines.material(), SkBlendMode::kSrcOver},
        {paperPulp.material(), SkBlendMode::kSrcOver},
        {foxing.material(), SkBlendMode::kSrcOver},
    });
    // lying under a window: the vignette centre sits slightly ABOVE middle
    vignette = Paint::radialUnit(
        {0.46f, 0.40f}, 1.10f,
        {{0.0f, hex(0xffffff)}, {0.70f, hex(0xf6f1e6)}, {1.0f, hex(0xc4b9a4)}});

    runAudits(ctx);

    // ONE Output drives every beat, looping at tLoop. Each beat is
    // bind().window(lo, hi), never from(): outside a window from() feeds
    // the easing curve values outside its domain and ease:: is not total.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float s = (float)std::fmod(t, (double)tLoop);
      T = s;
      // the 12.6%: every band morphs from engraved to STATED and back,
      // twice, and it should be unmistakable that the reproduction is
      // lying while it does.
      if (s >= tMorph && s <= tMorphEnd) {
        const float u = (s - tMorph) / (tMorphEnd - tMorph);
        const float w = 0.5f - 0.5f * std::cos(u * 4.0f * kPi);
        mmScale = kMmPer10k + (kStatedMmPer10k - kMmPer10k) * w;
      } else {
        mmScale = kMmPer10k;
      }
      // the sheet dims while the audit argues, and comes back after
      float d = 0.0f;
      if (s > tScale && s < tGeo) {
        d = std::min(1.0f, (s - tScale) / 0.6f);
        d = std::min(d, (tGeo - s) / 0.6f);
      }
      dimAmt = 0.42f * std::clamp(d, 0.0f, 1.0f);
      calAlpha = std::clamp(
          std::min((s - tScale - 0.2f) / 0.3f, (28.6f - s) / 0.9f), 0.0f, 1.0f);
      return true;
    });
    // The caliper walks in CLICKS: one spot reading every 2/3 s on its own
    // fixed-rate lane, independent of the frame rate.
    ctx.ticker.addFixed(
        1.5f,
        [this]() {
          if (T.value() >= tScale && T.value() <= tLigneEnd) ++calStep;
          return true;
        },
        8);

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("caliper", caliper());
    calShown = calStep;
  }

  /** The one thing on the sheet that cannot be a bound value: the caliper's
   *  reading is TYPE, and a string is not a property a binding can drive,
   *  so a new step re-describes. renderSlot() keeps that to one node. */
  void update(double, sketch::SketchContext& ctx) override {
    if (calStep != calShown) {
      calShown = calStep;
      ctx.composer.renderSlot("caliper", caliper());
    }
  }
};

SIGIL_SKETCH(
    Minard1869, "Study \xc2\xb7 Science",
    "Minard's BnF presentation copy \xe2\x80\x94 the plate audited against "
    "its own printed legend")
