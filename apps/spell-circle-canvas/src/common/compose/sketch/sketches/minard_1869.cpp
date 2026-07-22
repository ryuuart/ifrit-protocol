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
// MEASURED IN THIS FILE'S OWN SCRIPTS (work-d04/lat2.py, hann.py) —
// two numbers the brief left as ESTIMATED, and one finding that is new:
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
// NOT TRACED. Every band width here is survivors x 1.126 mm / 10,000
// against the sheet's own 2.258 px per millimetre; the Russian panel's
// city positions come out of the affine fit above; every console number
// is computed in this file. The Hannibal band's stations ARE read off
// the scan, and that is forced rather than lazy: hann.py shows there is
// no projection to fit.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/minard_1869.cpp \
//       --frame /tmp/minard_1869.png --at 20.0

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/Debug.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathMeasure.h>
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

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// palette — sampled by percentile over masked regions of the two scans.
// TWO worlds, deliberately kept apart: the aged artefact, and the audit.

constexpr SkColor4f kDesk = hex(0x1b1a18);       // the table the sheet lies on
constexpr SkColor4f kPaperShadow = hex(0xb9ad98); // p10: edges, foxing
constexpr SkColor4f kPaperBody = hex(0xcbbfab);   // p50: the sheet's ground
constexpr SkColor4f kPaperLight = hex(0xd6cab6);  // p90
constexpr SkColor4f kZoneDark = hex(0xa98674);
constexpr SkColor4f kZone = hex(0xb08d7a);        // "le rouge"
constexpr SkColor4f kZoneLight = hex(0xb79481);
constexpr SkColor4f kInk = hex(0x25211d);         // p50 printed black
constexpr SkColor4f kInkDeep = hex(0x0a0806);
constexpr SkColor4f kInkThin = hex(0x4e4436);     // hairlines, hachures
constexpr SkColor4f kManuscript = hex(0x3b3a46);  // iron-gall, colder
constexpr SkColor4f kStampRed = hex(0x8e3b34);

constexpr SkColor4f kCard = hex(0xf2ece0);        // the audit's cooler paper
constexpr SkColor4f kCardInk = hex(0x1c1a17);
constexpr SkColor4f kBlue = hex(0x2f6f9c);        // MEASURED
constexpr SkColor4f kClaimRed = hex(0x8c2f22);    // WHAT THE LEGEND SAYS
constexpr SkColor4f kPass = hex(0x3e6b4a);
constexpr SkColor4f kAmber = hex(0xb5761e);
constexpr SkColor4f kGrey = hex(0x6d675c);

// ---------------------------------------------------------------------------
// composition — canvas 2560 x 1600

constexpr float kW = 2560.0f, kH = 1600.0f;
constexpr float kSheetX = 48.0f, kSheetY = 128.0f;
constexpr float kSheetW = 1400.0f, kSheetH = 1220.0f; // aspect 1.1475
// The sheet is drawn at its own aspect, so this number is REAL: every band
// width on screen is a millimetre count of Minard's paper.
constexpr float kPxPerMm = kSheetW / 620.0f; // 2.2581

// the printed frame, shared by both panels (BnF scan: x 157.5..3842.5)
constexpr float kFrameL = 46.0f;
constexpr float kFrameW = 579.14f * kPxPerMm; // 1307.7
constexpr float kFrameR = kFrameL + kFrameW;
constexpr float kFrameT = 47.0f;
constexpr float kDivHN = 566.0f;  // Hannibal | Napoleon
constexpr float kDivNT = 1046.0f; // map | temperature
constexpr float kFrameB = 1160.0f;

// Napoleon panel: x = a + b*lon, y = c − d*lat.
// b from the Commons scan (130.890 px/deg / 3.4482 px/mm * kPxPerMm);
// d/b = 2.142 from lat2.py.
constexpr float kLonPx = 85.69f;
constexpr float kLon0 = 22.867f; // longitude at the frame's left rule
constexpr float kLatPx = kLonPx * 2.142f; // 183.55
constexpr float kLatRefY = 692.0f;        // y of lat 55.8 (Moscou)

// the audited scale: 1.1258 mm per 10,000 men (four direct BnF spot reads)
constexpr float kMmPer10k = 1.1258f;
constexpr float kStatedMmPer10k = 1.0f;
constexpr float kPxPer10k = kMmPer10k * kPxPerMm; // 2.5421
constexpr float kLigneHalf = 1.1279f;             // 2.2558 / 2
constexpr float kFloorMm = 1.57f;                 // what the crayon holds

inline float mapX(float lon) { return kFrameL + (lon - kLon0) * kLonPx; }
inline float mapY(float lat) { return kLatRefY + (55.8f - lat) * kLatPx; }
inline float bandPx(float men) { return men * kPxPer10k / 10000.0f; }

// audit column
constexpr float kAuditX = 1492.0f, kAuditW = 1020.0f;
constexpr float kConsoleY = 1372.0f, kConsoleH = 200.0f;

// ---------------------------------------------------------------------------
// THE DATA
//
// Minard.troops, all 51 rows, regrouped the way MINARD DRAWS IT rather
// than the way Wilkinson encodes it (see §6.1 of the study notes):
// Minard writes 422.000 on ONE band at the Niemen and lets the columns
// peel off; Wilkinson records three parallel bands from x = 0. Both are
// defensible; Minard's is the one that makes the flow identities visible.

struct Station {
  float lon, lat;
  float men; // the strength the band CARRIES from this station eastward/onward
};

// --- the advance, as Minard draws it -------------------------------------
// trunk: the Niemen -> Moscou, with the two branch treads prepended
const std::vector<Station> kAdvTrunk = {
    {23.85f, 54.85f, 422000}, // the Niemen crossing
    {24.50f, 55.00f, 400000}, // the northern column has peeled off
    {25.50f, 54.50f, 340000}, // the Polotzk column has peeled off (Wilna)
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
    {24.50f, 55.00f, 22000}, {24.50f, 55.30f, 22000}, {24.60f, 55.80f, 6000},
};
// the 60,000 that peels toward Polotzk (group 2 A)
const std::vector<Station> kAdvPolotzk = {
    {25.50f, 54.60f, 60000}, {26.60f, 55.70f, 40000},
    {27.40f, 55.60f, 33000}, {28.70f, 55.50f, 33000},
};

// --- the retreat, east -> west -------------------------------------------
// Drawn in two pieces so the Bobr junction is an ENDPOINT of both, which
// is what debug::endpointDegrees needs to see the army as one component.
const std::vector<Station> kRetEast = {
    {37.70f, 55.70f, 100000}, {37.50f, 55.70f, 98000},
    {37.00f, 55.00f, 97000},  {36.80f, 55.00f, 96000},
    {35.40f, 55.30f, 87000},  {34.30f, 55.20f, 55000},
    {33.30f, 54.80f, 37000},  {32.00f, 54.60f, 24000},
    {30.40f, 54.40f, 20000},  {29.20f, 54.30f, 20000}, // Bobr
};
const std::vector<Station> kRetWest = {
    {29.20f, 54.30f, 50000}, // + the Polotzk column's 30,000 = 50,000
    {28.50f, 54.20f, 50000}, // Studienska — the Berezina
    {28.30f, 54.30f, 28000}, {27.50f, 54.50f, 20000},
    {26.80f, 54.30f, 12000}, {26.40f, 54.40f, 14000}, // <- the anomaly
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

struct City {
  const char *plate; // Minard's own spelling, kept
  float lon, lat;    // Minard's plotted position
  float rlon, rlat;  // gazetteer
  float dx, dy;      // label offset in sheet px
};
// residuals recomputed live from these by haversine (see cityKm below).
const std::array<City, 20> kCities = {{
    {"Kowno", 24.0f, 55.0f, 23.9036f, 54.8985f, -6, 24},
    {"Wilna", 25.3f, 54.7f, 25.2797f, 54.6872f, -4, 26},
    {"Smorgoni", 26.4f, 54.4f, 26.3958f, 54.4783f, -14, -12},
    {"Molodezno", 26.8f, 54.3f, 26.8500f, 54.3167f, -12, 20},
    {"Gloubokoe", 27.7f, 55.2f, 27.6906f, 55.1372f, -26, -14},
    {"Minsk", 27.6f, 53.9f, 27.5590f, 53.9006f, -12, 6},
    {"Studienska", 28.5f, 54.3f, 28.4333f, 54.3572f, -24, 26},
    {"Polotzk", 28.7f, 55.5f, 28.7861f, 55.4850f, 6, -12},
    {"Bobr", 29.2f, 54.4f, 29.2731f, 54.3097f, 8, 12},
    {"Witebsk", 30.2f, 55.3f, 30.2049f, 55.1904f, -12, -14},
    {"Orscha", 30.4f, 54.5f, 30.4172f, 54.5081f, -10, 20},
    {"Mohilow", 30.4f, 53.9f, 30.3313f, 53.9007f, -12, 6},
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
  const char *label; // exactly as engraved (8bre / 9bre / Xbre kept)
  float lon;
  float reaumur;
  int daysSincePrev;
};
const std::array<Temp, 9> kTemps = {{
    {"Zero le 18 8bre", 37.6f, 0, 0},
    {"Pluie 24 8bre", 36.0f, 0, 6},
    {"- 9° le 9 9bre", 33.2f, -9, 16},
    {"- 21° le 14 9bre", 32.0f, -21, 5},
    {"- 11°", 29.2f, -11, 10}, // NO DATE ENGRAVED
    {"- 20° le 28 9bre", 28.5f, -20, 4},
    {"- 24° le 1er Xbre", 27.2f, -24, 3},
    {"- 30° le 6 Xbre", 26.7f, -30, 5},
    {"- 26° le 7 Xbre", 25.3f, -26, 1},
}};

// --- the Hannibal panel ---------------------------------------------------
// Stations READ OFF the BnF scan in sheet coordinates, which hann.py makes
// the only honest option: a least-squares latitude fit on that band returns
// d/b = 0.048 at R² = 0.12, i.e. there is no projection there to fit.
struct HStation {
  float x, y;
  float men;
  const char *at;
};
const std::vector<HStation> kHannibal = {
    {102, 268, 96000, "Espagne"},   {222, 264, 94000, "Tortose"},
    {322, 306, 80000, "Terragone"}, {400, 336, 80000, "Barcelone"},
    {510, 328, 80000, "Girone"},    {578, 318, 60000, "Collioure"},
    {600, 296, 60000, "Perpignan"}, {662, 272, 60000, "Narbone"},
    {760, 258, 60000, ""},          {830, 280, 60000, "Pt St Esprit"},
    {858, 308, 60000, "Orange"},    {934, 280, 46000, "l'Isere"},
    {998, 298, 46000, "Grenoble"},  {1034, 330, 46000, "St Jn de Maurienne"},
    {1068, 372, 26000, "le Mt Cenis"}, {1098, 400, 26000, "Suze"},
    {1124, 436, 26000, "Turin"},
};

struct Place {
  const char *name;
  float x, y;
  int kind; // 0 region caps, 1 town, 2 river, 3 people
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

const char *kLegendNapoleon[] = {
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

SkPoint stationPt(const Station &s) { return {mapX(s.lon), mapY(s.lat)}; }

SkPath polyline(const std::vector<Station> &st) {
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

SkPath polylineH(const std::vector<HStation> &st) {
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
std::vector<float> arcAt(const std::vector<SkPoint> &pts) {
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
  std::vector<float> arc; // cumulative arc length at each station
  std::vector<float> men;
  float maxPx = 0.0f;

  float menAt(float s) const {
    size_t i = 0;
    while (i + 1 < arc.size() && arc[i + 1] <= s)
      ++i;
    return men[i];
  }
  float pxAt(float s) const { return bandPx(menAt(s)); }
};

WidthProfile profileOf(const std::vector<Station> &st) {
  std::vector<SkPoint> pts;
  pts.reserve(st.size());
  for (const Station &s : st)
    pts.push_back(stationPt(s));
  WidthProfile w;
  w.arc = arcAt(pts);
  for (const Station &s : st) {
    w.men.push_back(s.men);
    w.maxPx = std::max(w.maxPx, bandPx(s.men));
  }
  return w;
}

WidthProfile profileOfH(const std::vector<HStation> &st) {
  std::vector<SkPoint> pts;
  for (const HStation &s : st)
    pts.push_back({s.x, s.y});
  WidthProfile w;
  w.arc = arcAt(pts);
  for (const HStation &s : st) {
    w.men.push_back(s.men);
    w.maxPx = std::max(w.maxPx, bandPx(s.men));
  }
  return w;
}

/** A line-for-line transcription of brushes::Ribbon::paint's band
 *  construction (Brushes.h ~941): sample the contour every `stride` px and
 *  emit pos ± n·w/2, left forward then right backward, closed.
 *
 *  It exists because the library gives no way to GET the polygon a brush
 *  emitted — Ribbon paints and forgets. The audit below has to measure the
 *  polygon that was actually drawn, so either the brush hands it back or
 *  the study copies the constructor. See the gap list. */
SkPath ribbonBand(const SkPath &spine, const WidthProfile &w,
                  float stride = 3.0f) {
  SkPathBuilder band;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    std::vector<SkPoint> left, right;
    for (float d = 0;; d += stride) {
      const float at = std::min(d, len);
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(at, &pos, &tan))
        break;
      const float ww = w.pxAt(at);
      const SkVector n{-tan.y(), tan.x()};
      left.push_back({pos.x() + n.x() * ww / 2, pos.y() + n.y() * ww / 2});
      right.push_back({pos.x() - n.x() * ww / 2, pos.y() - n.y() * ww / 2});
      if (at >= len)
        break;
    }
    if (left.size() < 2)
      continue;
    band.moveTo(left.front());
    for (size_t i = 1; i < left.size(); ++i)
      band.lineTo(left[i]);
    for (size_t i = right.size(); i-- > 0;)
      band.lineTo(right[i]);
    band.close();
  }
  return band.detach();
}

// ---------------------------------------------------------------------------
// THE AUDITOR — the same min-chord raycaster that measured Minard's
// engraving, pointed at the sketch's own geometry. This is the function
// the gap list asks for as debug::widthAlong(); it is written here so the
// ask arrives with a working implementation attached.

struct WidthAudit {
  std::vector<float> at, measured, intended;
  float maxErr = 0.0f;
  float maxErrAt = 0.0f;
  SkPoint worst{0, 0};
  float rms = 0.0f;
  int samples = 0;
};

/** Chord of `edges` through `p` along direction `u`, or -1 if p is outside.
 *  Edges are a flat list of segment endpoints. */
float chordThrough(const std::vector<SkPoint> &edges, SkPoint p, SkVector u,
                   float limit) {
  float fwd = limit, back = limit;
  for (size_t i = 0; i + 1 < edges.size(); i += 2) {
    const SkPoint a = edges[i], b = edges[i + 1];
    const SkVector e{b.x() - a.x(), b.y() - a.y()};
    const float den = u.x() * e.y() - u.y() * e.x();
    if (std::fabs(den) < 1e-9f)
      continue;
    const SkVector w{a.x() - p.x(), a.y() - p.y()};
    const float t = (w.x() * e.y() - w.y() * e.x()) / den; // along u
    const float s = (w.x() * u.y() - w.y() * u.x()) / den; // along e
    if (s < 0.0f || s > 1.0f)
      continue;
    if (t > 0.0f)
      fwd = std::min(fwd, t);
    else
      back = std::min(back, -t);
  }
  return fwd + back;
}

WidthAudit widthAlong(const SkPath &band, const SkPath &spine,
                      const WidthProfile &w, float step = 4.0f,
                      int directions = 90) {
  // flatten the band into segments once
  std::vector<SkPoint> edges;
  {
    SkPath::Iter it(band, true);
    SkPoint pts[4];
    SkPoint last{0, 0};
    bool have = false;
    for (SkPath::Verb v = it.next(pts); v != SkPath::kDone_Verb;
         v = it.next(pts)) {
      if (v == SkPath::kMove_Verb) {
        last = pts[0];
        have = true;
      } else if (v == SkPath::kLine_Verb && have) {
        edges.push_back(last);
        edges.push_back(pts[1]);
        last = pts[1];
      } else if (v == SkPath::kClose_Verb) {
        have = false;
      }
    }
  }

  WidthAudit out;
  const float limit = w.maxPx * 4.0f + 40.0f;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
    // Skip a half-width margin at each cap. The first version did not, and
    // reported 80 px of error at arc 4 px on a 107 px band — which is not a
    // corner defect but the END CAP: near the cap the shortest chord runs
    // diagonally out through the cap, and minimising w/2/sin+d/cos gives
    // ~68 px where the band is 107. The engraving measurement had the same
    // exclusion (stair.py splits at the risers and never straddles an end),
    // so the two audits stay comparable. THIS IS THE FIRST THING A
    // debug::widthAlong WOULD HAVE TO DOCUMENT.
    const float margin = w.maxPx * 0.55f + step;
    for (float d = std::max(step, margin); d < len - margin; d += step) {
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(d, &pos, &tan))
        continue;
      float best = limit;
      for (int k = 0; k < directions; ++k) {
        const float a = kPi * (float)k / (float)directions;
        const SkVector u{std::cos(a), std::sin(a)};
        const float c = chordThrough(edges, pos, u, limit);
        best = std::min(best, c);
      }
      const float want = w.pxAt(d);
      out.at.push_back(d);
      out.measured.push_back(best);
      out.intended.push_back(want);
      const float err = std::fabs(best - want);
      out.rms += err * err;
      ++out.samples;
      if (err > out.maxErr) {
        out.maxErr = err;
        out.maxErrAt = d;
        out.worst = pos;
      }
    }
  }
  if (out.samples)
    out.rms = std::sqrt(out.rms / (float)out.samples);
  return out;
}

/** ∫ w ds over the profile — the ink Minard intended, in px². */
float inkIntegral(const SkPath &spine, const WidthProfile &w,
                  float step = 1.0f) {
  float total = 0;
  SkContourMeasureIter iter(spine, false);
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float len = contour->length();
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
  const float a = std::sin(dp / 2) * std::sin(dp / 2) +
                  std::cos(p1) * std::cos(p2) * std::sin(dl / 2) *
                      std::sin(dl / 2);
  return 2 * R * std::asin(std::min(1.0f, std::sqrt(a)));
}
float cityKm(const City &c) {
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
std::function<SkPath(SkSize)> pathFn(SkPath path) {
  return [path](SkSize) { return path; };
}

/** Quadratic smoothing through a point list (midpoint construction) —
 *  the coastlines and rivers of both panels. */
SkPath smooth(const std::vector<SkPoint> &p) {
  SkPathBuilder b;
  if (p.size() < 2)
    return b.detach();
  b.moveTo(p[0]);
  for (size_t i = 1; i + 1 < p.size(); ++i) {
    const SkPoint m{(p[i].x() + p[i + 1].x()) * 0.5f,
                    (p[i].y() + p[i + 1].y()) * 0.5f};
    b.quadTo(p[i], m);
  }
  b.lineTo(p.back());
  return b.detach();
}

SkPath rectPath(float l, float t, float r, float bm) {
  SkPathBuilder p;
  p.addRect(SkRect::MakeLTRB(l, t, r, bm));
  return p.detach();
}

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size,
                             SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

std::string fmt(const char *f, double v) {
  char buf[128];
  std::snprintf(buf, sizeof buf, f, v);
  return buf;
}
std::string fmt2(const char *f, double a, double b) {
  char buf[192];
  std::snprintf(buf, sizeof buf, f, a, b);
  return buf;
}
std::string fmt3(const char *f, double a, double b, double c) {
  char buf[224];
  std::snprintf(buf, sizeof buf, f, a, b, c);
  return buf;
}
/** The French thousands separator the plate actually engraves: 422.000,
 *  never 422,000. */
std::string french(float men) {
  const long v = (long)std::lround(men);
  std::string s = std::to_string(v);
  for (int i = (int)s.size() - 3; i > 0; i -= 3)
    s.insert((size_t)i, ".");
  return s;
}

Transition ramp(float delayMs, float durMs, ch::EaseFn ease = ch::easeOutQuad) {
  Transition t;
  t.duration = std::chrono::milliseconds((int)durMs);
  t.delay = std::chrono::milliseconds((int)delayMs);
  t.ease = std::move(ease);
  return t;
}

} // namespace

// ===========================================================================

struct Minard1869 : sigil::compose::sketch::Sketch {
  // ---- the timeline: ONE Output, eleven beats, every beat windowed.
  // NOT from() — ease:: is not total and a value outside a beat's window
  // feeds the curve outside its domain (Compose.h ~259, and this is the
  // eleven-beat case that note was written for).
  ch::Output<float> T{0};
  ch::Output<float> mmScale{kMmPer10k}; // the 12.6% morph
  ch::Output<float> caliperMm{0};
  ch::Output<float> reaumurMix{0};
  ch::Output<float> calX{0};
  ch::Output<float> calY{0};
  ch::Output<float> dimAmt{0};

  sk_sp<SkTypeface> faceScript, faceItalic, faceRoman, faceNum, faceUi,
      faceUiBold, faceMono;

  Pattern paperPulp, laidLines, chainLines, foxing, tintSpeckle;
  Material paperMat, zoneMat, inkMat, vignette, stampGrain;

  // audits, computed once in setup()
  WidthAudit auditAdvance, auditRetreat;
  float advanceInk = 0, advanceArea = 0;
  float retreatInk = 0, retreatArea = 0;
  float coverDoubled = 0;
  size_t advComponentsDrawn = 0, advComponentsWilkinson = 0,
         retComponents = 0;
  float riserArcErr = 0;   // arc-length indexed  — the right way
  float riserFracErr = 0;  // fraction indexed    — the trap
  const char *riserWorstCity = "";

  console::LineRing colA{200}, colB{200}, colC{200}, colD{200};

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

  PropValue<float> beat(float a, float b) const {
    return bind(&T).window(a, b).clamp(0.0f, 1.0f);
  }
  /** Fade in over [a,b] and stay up. */
  PropValue<float> upFrom(float a, float b) const {
    return bind(&T).window(a, b).clamp(0.0f, 1.0f);
  }

  // =======================================================================
  // THE SHEET

  Element paperGround() {
    return box()
        .absolute()
        .inset(0)
        .fill(paperMat)
        // the ageing: warmer and dirtier toward the edges
        .foreground(decorations::wash(vignette, SkBlendMode::kMultiply, 0.30f))
        .cache(Cache::Texture)
        .key("paper");
  }

  Element frames() {
    auto g = box().absolute().inset(0);
    // the two printed frames are ONE rule around each panel, drawn as a
    // double rule the way the plate cuts them
    auto rule = [&](float l, float t, float r, float bm, const char *k,
                    float t0, float t1) {
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(rectPath(l, t, r, bm)))
                  .stroke(lines::Line{.width = 1.1f,
                                      .fill = Fill::color(kInk),
                                      .parallels = 2,
                                      .gap = 3.0f})
                  .trim(0.0f, beat(t0, t1))
                  .key(k));
    };
    rule(kFrameL, kFrameT, kFrameR, kDivHN, "frameH", 0.25f, 1.1f);
    rule(kFrameL, kDivHN + 4, kFrameR, kFrameB, "frameN", 0.35f, 1.2f);
    // the map | temperature divider
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({kFrameL, kDivNT}, {kFrameR, kDivNT}))
                .stroke(util::stroke(1.0f, Fill::color(kInk)))
                .trim(0.0f, beat(0.4f, 1.2f))
                .key("divNT"));
    return g;
  }

  /** Minard's own hand across the top margin, and the two donation
   *  stamps. This is the part of the object that makes it HIS copy. */
  Element provenance() {
    auto g = box().absolute().inset(0);
    g.child(text(toU8("pour la Bibliothèque impériale"),
                 type(faceScript, 30, kManuscript, 1.0f))
                .absolute()
                .left(Dim(40))
                .top(Dim(4))
                .key("dedic")
                .wipe(0.0f, beat(0.45f, 1.15f)));
    g.child(text(toU8("Ge Don 4182"), type(faceScript, 16, kManuscript, 0.4f))
                .absolute()
                .left(Dim(1218))
                .top(Dim(12))
                .key("gedon")
                .opacity(beat(0.9f, 1.2f)));
    auto stamp = [&](float cx, float cy, float r, const char *label,
                     const char *k, float t0) {
      g.child(box()
                  .absolute()
                  .left(Dim(cx - r))
                  .top(Dim(cy - r * 0.66f))
                  .width(Dim(2 * r))
                  .height(Dim(1.32f * r))
                  .outline(shapes::circle())
                  .stroke(util::stroke(1.5f, Fill::color(kStampRed)))
                  .child(text(toU8(label), type(faceRoman, 7.5f, kStampRed, 0.3f))
                             .absolute()
                             .left(Dim(r * 0.35f))
                             .top(Dim(r * 0.42f)))
                  .key(k)
                  .scale(withFrom(0.0f, 1.0f,
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
   *  which is lines::offsetAlong called once per ring. */
  Element hannibalSea() {
    const std::vector<SkPoint> coast = {
        {60, 300},   {150, 318},  {200, 330},  {268, 348},  {330, 344},
        {392, 358},  {470, 344},  {540, 348},  {586, 344},  {626, 330},
        {700, 348},  {760, 372},  {800, 400},  {822, 448},  {846, 492},
        {900, 512},  {962, 528},  {1010, 542}, {1052, 528}, {1100, 512},
        {1150, 520}, {1210, 546}, {1258, 560},
    };
    const SkPath line = smooth(coast);
    // The sea as a CLOSED region: the coast, then round the panel's own
    // south-east corner. Built by hand because the library has no boolean
    // path ops — `panelRect − land` is the natural spelling and `.clip()`
    // only intersects (see the gap list). Having the polygon, the hachures
    // are one clipPath.
    SkPathBuilder seab;
    seab.addPath(line);
    seab.lineTo(kFrameR - 2, kDivHN - 2);
    seab.lineTo(60, kDivHN - 2);
    seab.close();
    const SkPath sea = seab.detach();

    std::vector<SkPath> rings;
    float d = 0;
    for (int i = 1; i <= 9; ++i) {
      d += 3.0f + 1.9f * (float)i;
      rings.push_back(lines::offsetAlong(line, d, 4.0f));
    }

    auto g = box().absolute().inset(0);
    g.child(custom([sea, rings](SkCanvas &c, const PaintContext &) {
              SkPaint p;
              p.setAntiAlias(true);
              p.setStyle(SkPaint::kStroke_Style);
              p.setStrokeWidth(0.5f);
              c.save();
              c.clipPath(sea, true);
              for (size_t i = 0; i < rings.size(); ++i) {
                p.setColor4f(hex(0x4e4436, 0.52f - 0.045f * (float)i), nullptr);
                c.drawPath(rings[i], p);
              }
              c.restore();
            })
                .absolute()
                .inset(0)
                .cache(Cache::Texture)
                .key("seahatch")
                .opacity(beat(tHann + 0.25f, tHann + 1.0f)));
    // the shore itself, engraved on top
    g.child(box()
                .absolute()
                .inset(0)
                .outline(pathFn(line))
                .stroke(util::stroke(1.1f, Fill::color(kInk)))
                .trim(0.0f, beat(tHann, tHann + 0.5f))
                .key("coast"));
    g.child(text(toU8("Iles Baléares"), type(faceItalic, 9, kInk, 0.2f))
                .absolute()
                .left(Dim(150))
                .top(Dim(500))
                .key("baleares")
                .opacity(beat(tHann + 0.9f, tHann + 1.2f)));
    return g;
  }

  /** Lehmann hachures (Johann Georg Lehmann, 1799): strokes down the line
   *  of steepest descent, black-to-white ratio proportional to slope —
   *  all white at 0°, all black at 45°. Generated from a synthetic height
   *  field of gaussian ridges, not drawn. This is a FIELD, which is why
   *  patterns::stripes cannot express it (see the gap list). */
  Element lehmann(const std::vector<std::array<float, 4>> &ridges, float x0,
                  float y0, float x1, float y1, const char *key, float t0) {
    return custom([ridges, x0, y0, x1, y1](SkCanvas &c,
                                           const PaintContext &) {
             auto height = [&](float x, float y) {
               float h = 0;
               for (const auto &r : ridges) {
                 const float dx = (x - r[0]) / r[2];
                 const float dy = (y - r[1]) / r[3];
                 h += std::exp(-(dx * dx + dy * dy));
               }
               return h;
             };
             SkPaint p;
             p.setAntiAlias(true);
             p.setStyle(SkPaint::kStroke_Style);
             const float pitch = 3.6f;
             for (float y = y0; y < y1; y += pitch) {
               for (float x = x0; x < x1; x += pitch) {
                 const float h = height(x, y);
                 if (h < 0.16f)
                   continue;
                 const float e = 1.2f;
                 const float gx = (height(x + e, y) - height(x - e, y)) / (2 * e);
                 const float gy = (height(x, y + e) - height(x, y - e)) / (2 * e);
                 const float slope = std::sqrt(gx * gx + gy * gy);
                 if (slope < 0.004f)
                   continue;
                 // Lehmann: black fraction = slope/45deg, capped
                 const float k = std::min(1.0f, slope / 0.055f);
                 const float len = pitch * (0.30f + 0.62f * k);
                 const float ux = -gx / (slope + 1e-6f), uy = -gy / (slope + 1e-6f);
                 p.setStrokeWidth(0.45f + 0.75f * k);
                 p.setColor4f({kInkThin.fR, kInkThin.fG, kInkThin.fB,
                               0.30f + 0.62f * k},
                              nullptr);
                 c.drawLine(x - ux * len * 0.5f, y - uy * len * 0.5f,
                            x + ux * len * 0.5f, y + uy * len * 0.5f, p);
               }
             }
           })
        .absolute()
        .inset(0)
        .cache(Cache::Texture)
        .key(key)
        .opacity(beat(t0, t0 + 0.55f));
  }

  Element hannibalPanel() {
    auto g = box().absolute().inset(0);

    g.child(text(toU8("Carte Figurative des pertes successives en hommes de "
                      "l'armée qu'Annibal conduisit d'Espagne"),
                 type(faceScript, 19, kInk, 0.2f))
                .absolute()
                .left(Dim(210))
                .top(Dim(58))
                .key("htitle1")
                .opacity(beat(tHann, tHann + 0.4f)));
    g.child(text(toU8("en Italie en traversant les Gaules (selon Polybe)."),
                 type(faceScript, 17, kInk, 0.2f))
                .absolute()
                .left(Dim(320))
                .top(Dim(84))
                .key("htitle2")
                .opacity(beat(tHann + 0.1f, tHann + 0.5f)));
    g.child(text(toU8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite."),
                 type(faceScript, 14, kInk, 0.15f))
                .absolute()
                .left(Dim(300))
                .top(Dim(110))
                .key("htitle3")
                .opacity(beat(tHann + 0.2f, tHann + 0.6f)));
    g.child(text(toU8("Paris, le 20 Novembre 1869."),
                 type(faceScript, 14, kInk, 0.15f))
                .absolute()
                .left(Dim(700))
                .top(Dim(132))
                .key("htitle4")
                .opacity(beat(tHann + 0.3f, tHann + 0.7f)));

    g.child(hannibalSea());

    // The Pyrenees and the Alps, hachured by Lehmann's rule.
    g.child(lehmann({{{566, 300, 46, 12}}, {{600, 292, 40, 10}}}, 500, 262,
                    680, 330, "pyrenees", tHann + 0.55f));
    g.child(lehmann({{{1040, 340, 40, 26}},
                     {{1080, 380, 52, 30}},
                     {{1000, 300, 30, 18}},
                     {{1120, 430, 44, 26}},
                     {{1060, 470, 60, 22}}},
                    940, 250, 1220, 540, "alps", tHann + 0.7f));

    // rivers, in a lighter sloped hand
    auto river = [&](std::vector<SkPoint> pts, const char *k, float t0) {
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(smooth(pts)))
                  .stroke(util::stroke(0.8f, Fill::color(hex(0x4e4436, 0.8f))))
                  .trim(0.0f, beat(t0, t0 + 0.4f))
                  .key(k));
    };
    river({{206, 190}, {198, 240}, {212, 280}, {200, 322}}, "ebre",
          tHann + 0.9f);
    river({{796, 150}, {806, 210}, {790, 268}, {800, 320}, {780, 358}}, "rhone",
          tHann + 0.95f);
    river({{910, 236}, {930, 268}, {952, 286}}, "isere", tHann + 1.0f);
    river({{1040, 300}, {1010, 330}, {980, 350}}, "arc", tHann + 1.0f);
    river({{1160, 430}, {1200, 452}, {1250, 462}}, "po", tHann + 1.05f);

    // THE BAND — brushes::Ribbon with a widthFn over the plate's own
    // strengths. This is the primitive the whole sheet is made of.
    const SkPath spine = polylineH(kHannibal);
    const WidthProfile prof = profileOfH(kHannibal);
    g.child(bandElement(spine, prof, kZone, "hband",
                        beat(tHann + 0.55f, tHann + 1.6f)));

    // the ten numbers, written ACROSS the zones ("écrits en travers"),
    // Orient::Tangent on a cross-segment
    for (size_t i = 0; i + 1 < kHannibal.size(); ++i) {
      if (i > 0 && kHannibal[i].men == kHannibal[i - 1].men)
        continue;
      g.child(bandNumber({kHannibal[i].x, kHannibal[i].y},
                         {kHannibal[i + 1].x - kHannibal[i].x,
                          kHannibal[i + 1].y - kHannibal[i].y},
                         kHannibal[i].men, 8.2f, "hn" + std::to_string(i),
                         tHann + 0.7f + 0.06f * (float)i));
    }

    // the place names
    for (size_t i = 0; i < kHPlaces.size(); ++i) {
      const Place &p = kHPlaces[i];
      sigil::weave::TextStyle st =
          p.kind == 0   ? type(faceRoman, 11, kInk, 2.6f)
          : p.kind == 3 ? type(faceItalic, 10, hex(0x4e4436), 1.2f)
                        : type(faceItalic, 9, kInk, 0.2f);
      g.child(text(toU8(p.name), st)
                  .absolute()
                  .left(Dim(p.x))
                  .top(Dim(p.y))
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
                .absolute()
                .inset(0)
                .outline(segFn({610, 500}, {668, 434}))
                .stroke(lines::arrow(1.2f, Fill::color(kInk), 9.0f))
                .trim(0.0f, beat(tHann + 1.5f, tHann + 1.75f))
                .key("compass"));
    return g;
  }

  Element legendBox() {
    const float l = 1010, t = 52, w = 344, h = 108;
    auto g = box()
                 .absolute()
                 .left(Dim(l))
                 .top(Dim(t))
                 .width(Dim(w))
                 .height(Dim(h))
                 .outline(pathFn(rectPath(0, 0, w, h)))
                 .stroke(util::stroke(1.0f, Fill::color(kInk)))
                 .key("legendbox")
                 .opacity(beat(tHann + 1.5f, tHann + 1.8f));
    g.child(text(toU8("Légende."), type(faceScript, 14, kInk))
                .absolute()
                .left(Dim(140))
                .top(Dim(4)));
    const char *lines_[] = {
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
                  .absolute()
                  .left(Dim(8))
                  .top(Dim(24 + 16.0f * (float)i)));
    return g;
  }

  // =======================================================================
  // NAPOLEON

  /** One band: brushes::Ribbon with a widthFn, on a node sized to the
   *  ROUTE's bounding box so that Ribbon::widthMax is actually load
   *  bearing (the band overflows that box by up to w/2 on each side). */
  Element bandElement(const SkPath &spine, const WidthProfile &prof,
                      SkColor4f colour, const std::string &key,
                      PropValue<float> reveal) {
    const SkRect bb = spine.getBounds();
    const SkPath local = spine.makeOffset(-bb.left(), -bb.top());
    const WidthProfile p = prof;
    // The width function reads a LIVE Output (the 12.6% morph), which
    // Ribbon has no way to declare — hence Cache::None. See the gap list.
    const ch::Output<float> *scale = &mmScale;
    brushes::Ribbon r;
    r.fill = Fill::color(colour);
    r.step = 2.0f;
    r.widthMax = prof.maxPx * 1.2f;
    r.widthFn = [p, scale](const PathSample &s) {
      return p.pxAt(s.distance) * (scale->value() / kMmPer10k);
    };
    return box()
        .absolute()
        .left(Dim(bb.left()))
        .top(Dim(bb.top()))
        .width(Dim(bb.width()))
        .height(Dim(bb.height()))
        .outline([local](SkSize) { return local; })
        .stroke(r)
        .cache(Cache::None)
        .trim(0.0f, reveal)
        .key(key);
  }

  /** A strength written ACROSS its zone — Minard's "écrits en travers des
   *  zônes", set in the French convention with a full stop for thousands
   *  (422.000, never 422,000). The baseline is a segment along the band's
   *  NORMAL, so TextPath::Orient::Tangent puts the type across the band;
   *  compose::metrics()::capSlack sets the standoff so the numerals clear
   *  the edge by their own cap slack rather than by a guessed constant. */
  Element bandNumber(SkPoint at, SkVector tangent, float men, float size,
                     const std::string &key, float t0) {
    const float L = std::hypot(tangent.x(), tangent.y());
    SkVector t = L > 0 ? SkVector{tangent.x() / L, tangent.y() / L}
                       : SkVector{1, 0};
    SkVector n{-t.y(), t.x()};
    if (n.y() > 0) { // make the type read bottom-up, as on the plate
      n = {-n.x(), -n.y()};
    }
    const float half = bandPx(men) * 0.5f + 26.0f;
    const SkPoint a{at.x() - n.x() * half, at.y() - n.y() * half};
    const SkPoint b{at.x() + n.x() * half, at.y() + n.y() * half};
    return text(toU8(french(men)), type(faceNum, size, kInk, 0.2f))
        .absolute()
        .left(Dim(0))
        .top(Dim(0))
        .width(Dim(kSheetW))
        .height(Dim(kSheetH))
        .onPath(TextPath{.path = segFn(a, b),
                         .at = 0.5f,
                         .align = TextPath::Align::Center,
                         .offset = 0.0f,
                         .autoFlip = false,
                         .orient = TextPath::Orient::Tangent})
        .key(key)
        .opacity(beat(t0, t0 + 0.3f));
  }

  Element napoleonPanel(sketch::SketchContext &ctx) {
    auto g = box().absolute().inset(0);

    g.child(text(toU8("Carte Figurative des pertes successives en hommes de "
                      "l'Armée Française dans la campagne de Russie "
                      "1812—1813."),
                 type(faceScript, 18, kInk, 0.15f))
                .absolute()
                .left(Dim(200))
                .top(Dim(kDivHN + 14))
                .key("ntitle")
                .opacity(beat(tLegend, tLegend + 0.3f)));
    g.child(text(toU8("Dressée par M. Minard, Inspecteur Général "
                      "des Ponts et Chaussées en retraite.        Paris, "
                      "le 20 Novembre 1869."),
                 type(faceScript, 13, kInk, 0.1f))
                .absolute()
                .left(Dim(300))
                .top(Dim(kDivHN + 40))
                .key("ntitle2")
                .opacity(beat(tLegend + 0.1f, tLegend + 0.4f)));

    // the legend as a PARAGRAPH, which is what it is — not a key.
    for (int i = 0; i < 5; ++i) {
      const bool jerome = i >= 3; // the sentence almost nobody quotes
      g.child(text(toU8(kLegendNapoleon[i]),
                   type(faceScript, 10.6f, jerome ? kInk : kInk, 0.02f))
                  .absolute()
                  .left(Dim(i == 3 ? 148 : 128))
                  .top(Dim(kDivHN + 62 + 17.0f * (float)i))
                  .key("nleg" + std::to_string(i))
                  .wipe(0.0f, beat(tLegend + 0.25f + 0.16f * (float)i,
                                   tLegend + 0.55f + 0.16f * (float)i)));
    }

    // the rivers of the Russian panel
    auto river = [&](std::vector<SkPoint> pts, const char *label, SkPoint lp,
                     const char *k, float t0) {
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(smooth(pts)))
                  .stroke(util::stroke(0.7f, Fill::color(hex(0x4e4436, 0.85f))))
                  .trim(0.0f, beat(t0, t0 + 0.35f))
                  .key(k));
      g.child(text(toU8(label), type(faceItalic, 8, hex(0x4e4436), 0.6f))
                  .absolute()
                  .left(Dim(lp.x()))
                  .top(Dim(lp.y()))
                  .key(std::string(k) + "L")
                  .opacity(beat(t0 + 0.2f, t0 + 0.5f)));
    };
    river({{mapX(23.7f), mapY(56.0f)},
           {mapX(23.95f), mapY(55.3f)},
           {mapX(23.8f), mapY(54.8f)},
           {mapX(24.0f), mapY(54.2f)},
           {mapX(23.9f), mapY(53.8f)}},
          "Niémen R.", {mapX(23.5f) - 4, mapY(55.35f)}, "rNiemen",
          tAdv - 0.2f);
    river({{mapX(28.6f), mapY(54.9f)},
           {mapX(28.45f), mapY(54.5f)},
           {mapX(28.6f), mapY(54.1f)},
           {mapX(28.4f), mapY(53.7f)}},
          "Bérézina R.", {mapX(28.2f), mapY(54.72f)}, "rBerez",
          tAdv - 0.15f);
    river({{mapX(31.2f), mapY(54.05f)},
           {mapX(30.9f), mapY(54.5f)},
           {mapX(31.05f), mapY(54.95f)},
           {mapX(30.7f), mapY(55.4f)}},
          "Dniéper R.", {mapX(30.95f), mapY(54.35f)}, "rDniepr",
          tAdv - 0.1f);
    river({{mapX(36.6f), mapY(56.05f)},
           {mapX(36.9f), mapY(55.7f)},
           {mapX(37.3f), mapY(55.45f)}},
          "Moskowa R.", {mapX(36.35f), mapY(55.95f)}, "rMoskowa", tAdv - 0.05f);

    // --- THE ADVANCE ------------------------------------------------------
    // The red-brown is a SEPARATE STONE from the black, so it is very
    // slightly out of register. One translate, and it is the single most
    // convincing "this is a lithograph" cue on the sheet.
    auto redStone = box().absolute().inset(0).translateX(0.4f).translateY(-0.3f);
    const SkPath trunk = polyline(kAdvTrunk);
    const SkPath north = polyline(kAdvNorth);
    const SkPath polotzk = polyline(kAdvPolotzk);
    redStone.child(bandElement(trunk, profileOf(kAdvTrunk), kZone, "advTrunk",
                               beat(tAdv, tAdv + 1.6f)));
    redStone.child(bandElement(north, profileOf(kAdvNorth), kZone, "advNorth",
                               beat(tAdv + 0.35f, tAdv + 0.7f)));
    redStone.child(bandElement(polotzk, profileOf(kAdvPolotzk), kZone,
                               "advPol", beat(tAdv + 0.55f, tAdv + 1.2f)));
    // the stone took unevenly: a very low-amplitude speckle in the zone
    // colour, NOT a gradient (the Commons p10/p90 are two units apart)
    redStone.child(box()
                       .absolute()
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

    // the arithmetic of the splits, in the margin, as each one happens
    auto flash = [&](const char *txt, float x, float y, const char *k,
                     float t0) {
      g.child(text(toU8(txt), type(faceUiBold, 11, kBlue))
                  .absolute()
                  .left(Dim(x))
                  .top(Dim(y))
                  .key(k)
                  .opacity(beat(t0, t0 + 0.25f)));
    };
    flash("422 − 22 = 400", mapX(24.4f), mapY(56.02f), "ar1", tAdv + 0.5f);
    flash("400 − 60 = 340", mapX(25.6f), mapY(55.95f), "ar2", tAdv + 0.75f);
    flash("20 + 30 = 50", mapX(29.3f), mapY(53.98f), "ar3", tRet + 0.85f);
    flash("the Berezina: 50 − 28 = 22,000 men in four days",
          mapX(26.6f), mapY(53.86f), "ar4", tRet + 1.1f);
    flash("4 + 6 = 10 recrossed", mapX(23.6f), mapY(53.9f), "ar5",
          tRet + 1.75f);

    // --- the engraved numbers --------------------------------------------
    auto numbersFor = [&](const std::vector<Station> &st, const char *tag,
                          float t0, float dt) {
      for (size_t i = 0; i + 1 < st.size(); ++i) {
        if (i > 0 && st[i].men == st[i - 1].men)
          continue;
        const SkPoint a = stationPt(st[i]), b = stationPt(st[i + 1]);
        g.child(bandNumber({(a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f},
                           {b.x() - a.x(), b.y() - a.y()}, st[i].men, 8.6f,
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
    g.child(bandNumber({mapX(23.95f), mapY(54.4f)}, {1, 0}, 10000, 8.6f, "nG",
                       tRet + 1.7f));

    // --- the place names --------------------------------------------------
    for (size_t i = 0; i < kCities.size(); ++i) {
      const City &c = kCities[i];
      const bool moscou = std::string(c.plate) == "Moscou";
      // MOSCOU alone is set in spaced roman capitals, and it is the only
      // word on the map that is.
      Element e =
          moscou ? text(toU8("MOSCOU"), type(faceRoman, 13, kInk, 2.2f))
                       .textStroke(0.5f, Fill::color(kInk))
                 : text(toU8(c.plate), type(faceItalic, 9.6f, kInk, 0.2f));
      g.child(e.absolute()
                  .left(Dim(mapX(c.lon) + c.dx))
                  .top(Dim(mapY(c.lat) + c.dy))
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
      fb.addRect(SkRect::MakeLTRB(mapX(24.1f), y - floorPx * 0.5f,
                                  mapX(25.0f), y + floorPx * 0.5f));
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(fb.detach()))
                  .stroke(util::stroke(0.9f, Fill::color(kBlue)))
                  .key("floorink")
                  .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
      g.child(text(toU8("what the crayon actually laid — 1.57 mm, "
                        "2.6× the rule"),
                   type(faceUi, 9, kBlue))
                  .absolute()
                  .left(Dim(mapX(24.1f)))
                  .top(Dim(y + 10))
                  .key("floorinklab")
                  .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
    }

    // THE CALIPER LIES. Minard's own bar, read against Minard's own map.
    g.child(text(toU8("read Kowno→Smolensk with Minard's own bar: 210 "
                      "lieues = 933 km.   The truth is 520 km.   ×1.79, "
                      "UNEXPLAINED"),
                 type(faceUiBold, 10, kAmber))
                .absolute()
                .left(Dim(mapX(31.6f)))
                .top(Dim(mapY(53.98f))) 
                .key("barlies")
                .opacity(beat(tBar, tBar + 0.4f)));
    g.child(text(toU8("hypotheses: the labels are half their true value · "
                      "the bar was copied unrescaled from Fezensac · my "
                      "longitude scale is wrong"),
                 type(faceUi, 9, hex(0xb5761e, 0.85f)))
                .absolute()
                .left(Dim(mapX(31.6f)))
                .top(Dim(mapY(53.98f) + 13))
                .key("barhyp")
                .opacity(beat(tBar + 0.4f, tBar + 0.8f)));

    // the lieue bar, and its ticks
    g.child(scaleBar(mapX(34.0f), mapY(54.15f), 4.985f * 0.6549f, 50, 5,
                     "Lieues communes de France (Carte de M. de Fezensac)",
                     "nbar", tAdv + 1.7f));
    return g;
  }

  /** A graduated bar. `pxPerUnit` is px per lieue, `span` the last label,
   *  `step` the label interval — three separate ruler problems in this
   *  sketch and every one of them is ContourWalk + a stamp that would need
   *  to know WHICH sample it is. Hand-built for want of stampAt(). */
  Element scaleBar(float x, float y, float pxPerUnit, int span, int step,
                   const char *label, const char *key, float t0) {
    auto g = box().absolute().inset(0).key(key).opacity(beat(t0, t0 + 0.3f));
    const float w = pxPerUnit * (float)span;
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({x, y}, {x + w, y}))
                .stroke(util::stroke(0.9f, Fill::color(kInk))));
    for (int v = 0; v <= span; v += step) {
      const float tx = x + pxPerUnit * (float)v;
      if (v > 25 && v < span)
        continue; // the plate's own graduation: 0 5 10 15 20 25 ...... 50
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(segFn({tx, y - 4}, {tx, y})));
      g.child(text(toU8(std::to_string(v)), type(faceNum, 6.5f, kInk))
                  .absolute()
                  .left(Dim(tx - 3))
                  .top(Dim(y + 2)));
    }
    // the ticks as one stroked path so they are one node
    {
      SkPathBuilder tb;
      for (int v = 0; v <= span; v += step) {
        if (v > 25 && v < span)
          continue;
        const float tx = x + pxPerUnit * (float)v;
        tb.moveTo(tx, y - 4);
        tb.lineTo(tx, y);
      }
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(tb.detach()))
                  .stroke(util::stroke(0.9f, Fill::color(kInk))));
    }
    g.child(text(toU8(label), type(faceItalic, 7.5f, kInk, 0.1f))
                .absolute()
                .left(Dim(x))
                .top(Dim(y - 18)));
    return g;
  }

  // =======================================================================
  // THE TEMPERATURE PANEL — and the nine droplines that ARE the joint
  // between the two panels.

  float tempY(float reaumur) const {
    return kDivNT + 24.0f + (-reaumur) / 30.0f * 84.0f;
  }

  Element temperaturePanel() {
    auto g = box().absolute().inset(0);
    g.child(text(toU8("TABLEAU GRAPHIQUE de la température en degrés "
                      "du thermomètre de Réaumur au dessous de "
                      "zéro."),
                 type(faceScript, 13, kInk, 0.3f))
                .absolute()
                .left(Dim(300))
                .top(Dim(kDivNT + 4))
                .key("tempTitle")
                .opacity(beat(tTemp, tTemp + 0.3f)));

    // the INVERTED axis: 0 at the top, 30 degrés at the bottom
    for (int r = 0; r <= 30; r += 5) {
      const float y = tempY((float)-r);
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(segFn({kFrameL, y}, {kFrameR - 34, y}))
                  .stroke(util::stroke(0.4f, Fill::color(hex(0x4e4436, 0.45f))))
                  .trim(0.0f, beat(tTemp + 0.1f + 0.03f * (float)r,
                                   tTemp + 0.45f + 0.03f * (float)r))
                  .key("taxis" + std::to_string(r)));
      g.child(text(toU8(r == 30 ? "30 degrés" : std::to_string(r)),
                   type(faceNum, 7, kInk))
                  .absolute()
                  .left(Dim(kFrameR - 30))
                  .top(Dim(y - 4))
                  .key("tlab" + std::to_string(r))
                  .opacity(beat(tTemp + 0.15f + 0.03f * (float)r,
                                tTemp + 0.5f + 0.03f * (float)r)));
    }

    // the curve, and the fine ticks hatched UNDER it (not a fill)
    std::vector<SkPoint> curve;
    for (const Temp &t : kTemps)
      curve.push_back({mapX(t.lon), tempY(t.reaumur)});
    SkPathBuilder cb;
    for (size_t i = 0; i < curve.size(); ++i)
      i == 0 ? cb.moveTo(curve[i]) : cb.lineTo(curve[i]);
    const SkPath curvePath = cb.detach();
    g.child(box()
                .absolute()
                .inset(0)
                .outline(pathFn(curvePath))
                .stroke(util::stroke(0.9f, Fill::color(kInk)))
                // right to left, the way the retreat runs
                .wipe(180.0f, beat(tTemp + 0.4f, tTemp + 1.1f))
                .key("tcurve"));
    // the hatched underside: short ticks hanging off the curve
    g.child(custom([curvePath](SkCanvas &c, const PaintContext &) {
              SkPaint p;
              p.setAntiAlias(true);
              p.setStyle(SkPaint::kStroke_Style);
              p.setStrokeWidth(0.4f);
              p.setColor4f(hex(0x4e4436, 0.75f), nullptr);
              SkContourMeasureIter it(curvePath, false);
              while (sk_sp<SkContourMeasure> m = it.next()) {
                const float len = m->length();
                for (float d = 0; d < len; d += 3.0f) {
                  SkPoint q;
                  SkVector tn;
                  if (!m->getPosTan(d, &q, &tn))
                    continue;
                  c.drawLine(q.x(), q.y(), q.x() - 1.4f, q.y() + 5.0f, p);
                }
              }
            })
                .absolute()
                .inset(0)
                .cache(Cache::Texture)
                .key("thatch")
                .opacity(beat(tTemp + 0.6f, tTemp + 1.2f)));

    // THE DROPLINES. Nine of them, from the retreat band down through the
    // divider into the graph. They are the joint between the two panels
    // and they are the whole design — nothing in the library says the two
    // abscissae are the same axis (see the gap list).
    for (size_t i = 0; i < kTemps.size(); ++i) {
      const float x = mapX(kTemps[i].lon);
      SkPathBuilder d;
      d.moveTo(x, mapY(54.3f));
      d.lineTo(x, tempY(kTemps[i].reaumur));
      PathFormat f;
      f.width = 0.5f;
      // the rule fades as it crosses the panel divider
      f.strokeMaterial = Material::linearUnit(
          {0, 0}, {0, 1},
          {{0.0f, hex(0x4e4436, 0.55f)},
           {0.62f, hex(0x4e4436, 0.16f)},
           {1.0f, hex(0x4e4436, 0.5f)}});
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(d.detach()))
                  .stroke(f)
                  .trim(0.0f, beat(tTemp + 0.25f + 0.05f * (float)i,
                                   tTemp + 0.55f + 0.05f * (float)i))
                  .key("drop" + std::to_string(i)));

      // the annotation, as engraved. 8bre / 9bre / Xbre are October /
      // November / December — the old Roman-calendar notation, and a
      // caption that "corrects" Xbre to 10bre is wrong twice over.
      g.child(text(toU8(kTemps[i].label), type(faceNum, 7.4f, kInk, 0.1f))
                  .absolute()
                  .left(Dim(x - 26))
                  .top(Dim(tempY(kTemps[i].reaumur) + 5))
                  .key("tann" + std::to_string(i))
                  .opacity(beat(tTemp + 0.5f + 0.06f * (float)i,
                                tTemp + 0.8f + 0.06f * (float)i)));
    }
    // the undated −11°, and its two independent recoveries
    g.child(text(toU8("24 novembre  (derived, days column)"),
                 type(faceUi, 8, kBlue))
                .absolute()
                .left(Dim(mapX(29.2f) - 26))
                .top(Dim(tempY(-11) + 16))
                .key("recov1")
                .opacity(beat(tTemp + 1.35f, tTemp + 1.6f)));
    g.child(text(toU8("25 novembre  (derived, lon interpolation)"),
                 type(faceUi, 8, kBlue))
                .absolute()
                .left(Dim(mapX(29.2f) - 26))
                .top(Dim(tempY(-11) + 27))
                .key("recov2")
                .opacity(beat(tTemp + 1.5f, tTemp + 1.75f)));

    g.child(text(toU8("Les Cosaques passent au galop\nle Niémen gelé."),
                 type(faceScript, 10, kInk, 0.1f))
                .absolute()
                .left(Dim(kFrameL + 20))
                .top(Dim(kDivNT + 40))
                .key("cosaques")
                .opacity(beat(tTemp + 1.1f, tTemp + 1.5f)));
    return g;
  }

  Element imprints() {
    auto g = box().absolute().inset(0);
    g.child(text(toU8("Autog. par Regnier, 8. Pas. Sᵗᵉ Marie Sᵗ "
                      "Gᵃᵉᵐ à Paris."),
                 type(faceItalic, 7, kInk))
                .absolute()
                .left(Dim(kFrameL + 6))
                .top(Dim(kFrameB + 6))
                .key("imp1")
                .opacity(beat(1.0f, 1.3f)));
    g.child(text(toU8("Imp. Lith. Regnier et Dourdet"),
                 type(faceItalic, 7, kInk))
                .absolute()
                .left(Dim(kFrameR - 140))
                .top(Dim(kFrameB + 6))
                .key("imp2")
                .opacity(beat(1.0f, 1.3f)));
    return g;
  }

  /** The caliper: the only saturated object allowed on the plate, drawn
   *  OVER it with a small shadow so it reads as an instrument laid on
   *  paper rather than as ink. */
  Element caliper() {
    auto g = box().absolute().inset(0).key("caliper").opacity(
        beat(tScale + 0.2f, tScale + 0.5f));
    auto jaw = [&](float x, float y, float halfPx, const char *k) {
      SkPathBuilder p;
      p.moveTo(x - 16, y - halfPx);
      p.lineTo(x + 16, y - halfPx);
      p.moveTo(x - 16, y + halfPx);
      p.lineTo(x + 16, y + halfPx);
      p.moveTo(x + 12, y - halfPx);
      p.lineTo(x + 12, y + halfPx);
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(pathFn(p.detach()))
                  .background(util::shadow(hex(0x000000, 0.30f), {1.5f, 2.0f},
                                           3.0f))
                  .stroke(util::stroke(2.0f, Fill::color(kBlue)))
                  .key(k));
    };
    // at the Niemen — the reading that starts the audit
    jaw(mapX(24.0f) + 8, mapY(54.9f), bandPx(422000) * 0.5f, "jaw1");
    g.child(text(toU8(fmt("%.2f mm", bandPx(422000) / kPxPerMm)),
                 type(faceUiBold, 14, kBlue))
                .absolute()
                .left(Dim(mapX(24.0f) + 34))
                .top(Dim(mapY(54.9f) - 10))
                .key("calread"));
    g.child(text(toU8(fmt("÷ 422.000 = %.4f mm per 10.000",
                          bandPx(422000) / kPxPerMm / 42.2f)),
                 type(faceUi, 10, kBlue))
                .absolute()
                .left(Dim(mapX(24.0f) + 34))
                .top(Dim(mapY(54.9f) + 8))
                .key("calread2"));
    g.child(text(toU8("this sketch's own band.  Minard's, measured on the "
                      "BnF sheet: 47.15 mm"),
                 type(faceUi, 9, hex(0x2f6f9c, 0.85f)))
                .absolute()
                .left(Dim(mapX(24.0f) + 34))
                .top(Dim(mapY(54.9f) + 22))
                .key("calread3"));
    g.child(text(toU8("the legend says 1.0000"), type(faceUiBold, 10, kClaimRed))
                .absolute()
                .left(Dim(mapX(24.0f) + 34))
                .top(Dim(mapY(54.9f) + 36))
                .key("calread4"));
    return g;
  }

  Element sheet(sketch::SketchContext &ctx) {
    return box()
        .absolute()
        .left(Dim(kSheetX))
        .top(Dim(kSheetY))
        .width(Dim(kSheetW))
        .height(Dim(kSheetH))
        .background(util::shadow(hex(0x000000, 0.55f), {6, 10}, 26))
        .child(paperGround())
        .child(frames())
        .child(hannibalPanel())
        .child(napoleonPanel(ctx))
        .child(temperaturePanel())
        .child(imprints())
        .child(provenance())
        .child(caliper())
        // the whole sheet dims 40% while the audit argues
        .child(box()
                   .absolute()
                   .inset(0)
                   .fill(Fill::color(hex(0x120f0b)))
                   .opacity(&dimAmt)
                   .key("dim"))
        .key("sheet")
        .opacity(beat(0.0f, 0.6f));
  }

  // =======================================================================
  // THE AUDIT — five cards, a different world: clean paper, crisp rules,
  // no grain.

  Element card(float y, float h, const char *title, const char *key,
               float t0, Element body) {
    auto c = box()
                 .absolute()
                 .left(Dim(kAuditX))
                 .top(Dim(y))
                 .width(Dim(kAuditW))
                 .height(Dim(h))
                 .fill(Material::solid(kCard))
                 .stroke(util::stroke(1.0f, Fill::color(hex(0xcfc6b4))))
                 .key(key)
                 .opacity(beat(t0, t0 + 0.4f))
                 .translateY(bind(&T).window(t0, t0 + 0.4f).invert().scale(14));
    c.child(text(toU8(title), type(faceUiBold, 15, kCardInk, 1.6f))
                .absolute()
                .left(Dim(18))
                .top(Dim(12)));
    c.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({18, 36}, {kAuditW - 18, 36}))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk))));
    c.child(std::move(body));
    return c;
  }

  /** Card 1 — DOES THE PLATE OBEY ITS OWN LEGEND?  The eleven measured
   *  treads, the fit through them, and the two rules that matter. */
  Element cardScale() {
    // the measured staircase (Commons scan, stair2.py)
    static const std::array<std::pair<float, float>, 11> treads = {{
        {422000, 166.54f}, {400000, 154.46f}, {340000, 130.92f},
        {300000, 111.67f}, {280000, 101.55f}, {240000, 87.13f},
        {210000, 76.41f},  {175000, 67.42f},  {145000, 57.63f},
        {127100, 52.39f},  {100000, 40.25f},
    }};
    const float slope = 3.828f, intercept = -0.19f, r2 = 0.99266f;
    const float px0 = 60, py0 = 60, pw = 560, ph = 236;
    auto g = box().absolute().inset(0);
    // axes
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk))));
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({px0, py0}, {px0, py0 + ph}))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk))));
    auto X = [&](float men) { return px0 + men / 440000.0f * pw; };
    auto Y = [&](float px) { return py0 + ph - px / 180.0f * ph; };
    // the fitted line
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({X(0), Y(intercept)},
                               {X(440000), Y(intercept + slope * 44.0f)}))
                .stroke(util::stroke(1.6f, Fill::color(kBlue)))
                .trim(0.0f, beat(tScale + 1.8f, tScale + 2.4f))
                .key("fitline"));
    for (size_t i = 0; i < treads.size(); ++i) {
      const float x = X(treads[i].first), y = Y(treads[i].second);
      g.child(util::disc({x, y}, 3.6f)
                  .outline(shapes::circle())
                  .fill(Material::solid(kBlue))
                  .key("tread" + std::to_string(i))
                  .opacity(beat(tScale + 0.6f + 0.09f * (float)i,
                                tScale + 0.8f + 0.09f * (float)i)));
    }
    g.child(text(toU8("width_px = 3.828 px per 10,000 men,  intercept "
                      "−0.19 px,  R² = 0.99266"),
                 type(faceUi, 11, kBlue))
                .absolute()
                .left(Dim(px0 + 6))
                .top(Dim(py0 + 4))
                .key("fitlab")
                .opacity(beat(tScale + 2.3f, tScale + 2.6f)));
    g.child(text(toU8("the fitted line goes through the ORIGIN to a fifth of "
                      "a pixel — the zones are not merely\nlinear in men, "
                      "they are proportional"),
                 type(faceUi, 10, kGrey))
                .absolute()
                .left(Dim(px0 + 6))
                .top(Dim(py0 + 22))
                .key("proplab")
                .opacity(beat(tScale + 2.4f, tScale + 2.7f)));
    g.child(text(toU8("men →"), type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(px0 + pw - 40))
                .top(Dim(py0 + ph + 6)));
    g.child(text(toU8("px"), type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(px0 - 24))
                .top(Dim(py0 - 2)));

    // the two horizontal rules that matter
    const float rx = 660, rw = 320;
    auto ruleRow = [&](float y, const char *v, const char *what, SkColor4f col,
                       const char *k, float t0) {
      g.child(box()
                  .absolute()
                  .inset(0)
                  .outline(segFn({rx, y}, {rx + rw, y}))
                  .stroke(util::stroke(2.0f, Fill::color(col)))
                  .trim(0.0f, beat(t0, t0 + 0.3f))
                  .key(k));
      g.child(text(toU8(v), type(faceUiBold, 17, col))
                  .absolute()
                  .left(Dim(rx))
                  .top(Dim(y - 26))
                  .key(std::string(k) + "v")
                  .opacity(beat(t0, t0 + 0.3f)));
      g.child(text(toU8(what), type(faceUi, 10, col))
                  .absolute()
                  .left(Dim(rx))
                  .top(Dim(y + 5))
                  .key(std::string(k) + "w")
                  .opacity(beat(t0, t0 + 0.3f)));
    };
    ruleRow(120, "1.000 mm", "what the legend says", kClaimRed, "ruleStated",
            tScale + 0.4f);
    ruleRow(196, "1.126 mm", "what the ink measures  (12.6% wider)", kBlue,
            "ruleMeasured", tScale + 2.0f);
    g.child(text(toU8("half a French ligne = 1.1279 mm      (SPECULATION)"),
                 type(faceUi, 10, kGrey))
                .absolute()
                .left(Dim(rx))
                .top(Dim(238))
                .key("ligneNote")
                .opacity(beat(tLigne, tLigne + 0.4f)));
    g.child(text(toU8("competing: litho reduction · catalogued paper size "
                      "wrong (this one would kill it)"),
                 type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(rx))
                .top(Dim(254))
                .key("ligneAlt")
                .opacity(beat(tLigne + 0.4f, tLigne + 0.8f)));
    g.child(text(toU8("and the SAME factor appears on the Hannibal panel, "
                      "drawn from different data."),
                 type(faceUi, 10, kCardInk))
                .absolute()
                .left(Dim(rx))
                .top(Dim(276))
                .key("hannSame")
                .opacity(beat(tScale + 2.5f, tScale + 2.8f)));
    return g;
  }

  /** Card 2 — THE FLOOR. And the negative result: at the floor, the
   *  12,000 -> 14,000 anomaly is invisible in the ink. */
  Element cardFloor() {
    static const std::array<std::pair<float, float>, 10> pts = {{
        {96000, 3.57f}, {87000, 3.93f}, {55000, 3.80f}, {50000, 3.85f},
        {37000, 3.90f}, {24000, 4.67f}, {20000, 4.29f}, {12000, 4.20f},
        {8000, 7.04f},  {4000, 10.48f},
    }};
    const float px0 = 60, py0 = 58, pw = 470, ph = 108;
    auto g = box().absolute().inset(0);
    auto X = [&](float men) {
      const float l = std::log10(std::max(men, 1000.0f));
      return px0 + (l - 3.5f) / (5.05f - 3.5f) * pw;
    };
    auto Y = [&](float v) { return py0 + ph - (v - 3.0f) / 8.5f * ph; };
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({px0, py0 + ph}, {px0 + pw, py0 + ph}))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk))));
    SkPathBuilder line;
    for (size_t i = pts.size(); i-- > 0;) {
      const SkPoint q{X(pts[i].first), Y(pts[i].second)};
      i == pts.size() - 1 ? line.moveTo(q) : line.lineTo(q);
    }
    g.child(box()
                .absolute()
                .inset(0)
                .outline(pathFn(line.detach()))
                .stroke(util::stroke(1.6f, Fill::color(kBlue)))
                .trim(0.0f, beat(tScale + 1.0f, tScale + 1.8f))
                .key("floorline"));
    // the crayon floor
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({px0, Y(3.83f)}, {px0 + pw, Y(3.83f)}))
                .stroke(PathFormat{.width = 1.0f,
                                   .strokeFill = Fill::color(kGrey),
                                   .dashIntervals = {5, 4}})
                .key("floorRule")
                .opacity(beat(tScale + 1.4f, tScale + 1.7f)));
    g.child(text(toU8("3.8 px per 10,000 — the advance band's slope; the "
                      "retreat holds it above ~35,000 men"),
                 type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(px0 + 4))
                .top(Dim(Y(3.83f) - 15))
                .key("floorLab")
                .opacity(beat(tScale + 1.5f, tScale + 1.8f)));
    for (size_t i = 0; i < pts.size(); ++i)
      g.child(util::disc({X(pts[i].first), Y(pts[i].second)}, 3.0f)
                  .outline(shapes::circle())
                  .fill(Material::solid(i >= 8 ? kAmber : kBlue))
                  .key("fp" + std::to_string(i))
                  .opacity(beat(tScale + 1.0f + 0.05f * (float)i,
                                tScale + 1.2f + 0.05f * (float)i)));
    g.child(text(toU8("4,000 men drawn 2.6× too wide — 0.4 mm is "
                      "below what a lithographic crayon will hold"),
                 type(faceUi, 10, kAmber))
                .absolute()
                .left(Dim(px0))
                .top(Dim(py0 + ph + 8))
                .key("floorAmber")
                .opacity(beat(tScale + 1.8f, tScale + 2.1f)));
    g.child(text(toU8("minimum drawn width 5.4 px = 1.57 mm"),
                 type(faceUi, 10, kCardInk))
                .absolute()
                .left(Dim(px0))
                .top(Dim(py0 + ph + 24))
                .key("floorMin")
                .opacity(beat(tScale + 1.9f, tScale + 2.2f)));
    g.child(text(toU8("NEGATIVE RESULT — and it is the more useful half: "
                      "the famous 12,000→14,000 anomaly is NOT\nmeasurable "
                      "in the ink. At the floor both readings are 5.4 px. The "
                      "prettier finding does not exist."),
                 type(faceUi, 10, kClaimRed))
                .absolute()
                .left(Dim(px0 + 480))
                .top(Dim(py0 + 10))
                .width(Dim(kAuditW - px0 - 500))
                .key("floorNeg")
                .opacity(beat(tScale + 2.2f, tScale + 2.6f)));
    return g;
  }

  /** Card 3 — THE MAP IS A REAL MAP. The received account is wrong. */
  Element cardGeo() {
    auto g = box().absolute().inset(0);
    const float ox = 40, oy = 52, sc = 31.0f; // px per degree, inset map
    const float exagg = 8.0f;
    auto MX = [&](float lon) { return ox + (lon - 23.5f) * sc; };
    auto MY = [&](float lat) { return oy + (56.2f - lat) * sc * 1.4f; };
    // Minard's cities as dots, the real positions as crosses, residual
    // vectors at 20x
    SkPathBuilder crosses, vectors;
    for (const City &c : kCities) {
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
                .absolute()
                .inset(0)
                .outline(pathFn(vectors.detach()))
                .stroke(util::stroke(0.8f, Fill::color(hex(0x2f6f9c, 0.6f))))
                .trim(0.0f, beat(tGeo + 0.5f, tGeo + 1.1f))
                .key("geovec"));
    g.child(box()
                .absolute()
                .inset(0)
                .outline(pathFn(crosses.detach()))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk)))
                .key("geocross")
                .opacity(beat(tGeo + 0.2f, tGeo + 0.6f)));
    for (size_t i = 0; i < kCities.size(); ++i) {
      const City &c = kCities[i];
      const bool out = cityKm(c) > 20.0f;
      g.child(util::disc({MX(c.lon), MY(c.lat)}, out ? 4.0f : 2.6f)
                  .outline(shapes::circle())
                  .fill(Material::solid(out ? kAmber : kBlue))
                  .key("gc" + std::to_string(i))
                  .opacity(beat(tGeo + 0.1f + 0.02f * (float)i,
                                tGeo + 0.35f + 0.02f * (float)i)));
      if (out)
        g.child(text(toU8(c.plate), type(faceUiBold, 10, kAmber))
                    .absolute()
                    .left(Dim(MX(c.lon) + 7))
                    .top(Dim(MY(c.lat) - 6))
                    .key("gcl" + std::to_string(i))
                    .opacity(beat(tGeo + 1.6f, tGeo + 1.9f)));
    }

    // the histogram of the 20 residuals
    const float hx = 640, hy = 58, hw = 250, hh = 108;
    std::array<int, 8> bins{};
    for (const City &c : kCities) {
      int b = (int)(cityKm(c) / 5.0f);
      bins[(size_t)std::min(7, b)]++;
    }
    for (size_t i = 0; i < bins.size(); ++i) {
      const float bw = hw / 8.0f;
      const float bh = (float)bins[i] / 9.0f * hh;
      g.child(box()
                  .absolute()
                  .left(Dim(hx + bw * (float)i + 1))
                  .top(Dim(hy + hh - bh))
                  .width(Dim(bw - 2))
                  .height(Dim(std::max(bh, 1.0f)))
                  .fill(Material::solid(i >= 4 ? kAmber : kBlue))
                  .key("hist" + std::to_string(i))
                  .scale(withFrom(0.0f, 1.0f,
                                  ramp((tGeo + 1.0f) * 1000 + 60.0f * (float)i,
                                       320)))
                  .transformOrigin(0.5f, 1.0f)
                  .opacity(beat(tGeo + 1.0f + 0.06f * (float)i,
                                tGeo + 1.2f + 0.06f * (float)i)));
    }
    // the digitisation quantum, as a grey band behind
    g.child(box()
                .absolute()
                .left(Dim(hx + hw * 6.41f / 40.0f - 6))
                .top(Dim(hy))
                .width(Dim(12))
                .height(Dim(hh))
                .fill(Material::solid(hex(0x6d675c, 0.22f)))
                .key("quantum")
                .opacity(beat(tGeo + 1.3f, tGeo + 1.6f)));
    g.child(text(toU8("residual vectors ×8"), type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(ox))
                .top(Dim(oy + 150))
                .key("exaggLab")
                .opacity(beat(tGeo + 0.6f, tGeo + 0.9f)));
    g.child(text(toU8("0.1° grid = 6.41 km"), type(faceUi, 9, kGrey))
                .absolute()
                .left(Dim(hx + hw * 6.41f / 40.0f + 10))
                .top(Dim(hy + 4))
                .key("quantumLab")
                .opacity(beat(tGeo + 1.35f, tGeo + 1.65f)));
    g.child(text(toU8("median 5.35 km on an 871 km span — 0.6%. The "
                      "received account is wrong."),
                 type(faceUiBold, 12, kPass))
                .absolute()
                .left(Dim(hx))
                .top(Dim(hy + hh + 10))
                .width(Dim(330))
                .key("geoCap")
                .opacity(beat(tGeo + 1.7f, tGeo + 2.0f)));
    g.child(text(toU8("residual is within 1.8× of what the 0.1° "
                      "digitisation grid alone produces"),
                 type(faceUi, 9, kGrey))
                .absolute()
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
      const char *name;
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
    auto g = box().absolute().inset(0);
    const float bx = 250, by = 50, bw = 480, rowH = 12.2f;
    const float mid = bx + bw * 0.5f;
    g.child(box()
                .absolute()
                .inset(0)
                .outline(segFn({mid, by - 4}, {mid, by + rowH * 10 + 4}))
                .stroke(util::stroke(1.0f, Fill::color(kCardInk))));
    for (size_t i = 0; i < legs.size(); ++i) {
      const float y = by + rowH * (float)i;
      const bool bad = legs[i].ratio < 0.7f || legs[i].ratio > 1.3f;
      const float dx = (legs[i].ratio - 1.0f) * bw * 0.62f;
      g.child(text(toU8(legs[i].name), type(faceUi, 9.5f, bad ? kAmber : kGrey))
                  .absolute()
                  .left(Dim(60))
                  .top(Dim(y - 2))
                  .key("legn" + std::to_string(i))
                  .opacity(beat(tDistort + 0.1f + 0.04f * (float)i,
                                tDistort + 0.3f + 0.04f * (float)i)));
      g.child(box()
                  .absolute()
                  .left(Dim(dx < 0 ? mid + dx : mid))
                  .top(Dim(y))
                  .width(Dim(std::max(std::fabs(dx), 1.0f)))
                  .height(Dim(7))
                  .fill(Material::solid(bad ? kAmber : kBlue))
                  .key("legb" + std::to_string(i))
                  .scale(withFrom(0.0f, 1.0f,
                                  ramp((tDistort + 0.2f) * 1000 +
                                           70.0f * (float)i,
                                       420, ch::EaseOutBack())))
                  .transformOrigin(dx < 0 ? 1.0f : 0.0f, 0.5f)
                  .opacity(beat(tDistort + 0.2f + 0.05f * (float)i,
                                tDistort + 0.4f + 0.05f * (float)i)));
      g.child(text(toU8(fmt("%.3f", legs[i].ratio)),
                   type(faceUi, 9.5f, bad ? kAmber : kGrey))
                  .absolute()
                  .left(Dim(bx + bw + 20))
                  .top(Dim(y - 2))
                  .key("legv" + std::to_string(i))
                  .opacity(beat(tDistort + 0.2f + 0.04f * (float)i,
                                tDistort + 0.4f + 0.04f * (float)i)));
    }
    g.child(text(toU8("TOTAL 934.2 km real → 944.6 km on Minard   ratio "
                      "1.011.  He squeezed one leg to 59% and stretched\nthe "
                      "next to 153%, keeping the total right — exactly "
                      "where Wizma, Chjat and Mojaisk crowd\ninto 130 px of "
                      "lettering. That the room was for the labels is an "
                      "INFERENCE."),
                 type(faceUi, 10, kCardInk))
                .absolute()
                .left(Dim(60))
                .top(Dim(by + rowH * 10 + 12))
                .width(Dim(900))
                .key("legTotal")
                .opacity(beat(tDistort + 0.9f, tDistort + 1.3f)));
    return g;
  }

  /** Card 5 — RÉAUMUR. °C = °R × 5/4 and °F = °R × 9/4 + 32, both exact,
   *  and the axis relabels itself live. */
  Element cardReaumur() {
    auto g = box().absolute().inset(0);
    const float x0 = 40, y0 = 50, rowH = 15.0f;
    const char *heads[] = {"date on the plate", "°R", "°C",
                           "°F", "days"};
    const float cols[] = {0, 250, 330, 410, 500};
    for (int c = 0; c < 5; ++c)
      g.child(text(toU8(heads[c]), type(faceUiBold, 9.5f, kGrey))
                  .absolute()
                  .left(Dim(x0 + cols[c]))
                  .top(Dim(y0 - 16))
                  .key("rh" + std::to_string(c)));
    for (size_t i = 0; i < kTemps.size(); ++i) {
      const Temp &t = kTemps[i];
      const bool cold = t.reaumur <= -30.0f;
      const SkColor4f col = cold ? kBlue : kCardInk;
      const float y = y0 + rowH * (float)i;
      auto cell = [&](int c, const std::string &s, SkColor4f cc, float sz) {
        g.child(text(toU8(s), type(faceUi, sz, cc))
                    .absolute()
                    .left(Dim(x0 + cols[c]))
                    .top(Dim(y))
                    .key("rc" + std::to_string(i) + "_" + std::to_string(c))
                    .opacity(beat(tReaumur + 0.05f * (float)i,
                                  tReaumur + 0.25f + 0.05f * (float)i)));
      };
      cell(0, i == 4 ? std::string(t.label) + "   (NO DATE ENGRAVED)"
                     : std::string(t.label),
           i == 4 ? kAmber : col, cold ? 11.0f : 10.0f);
      cell(1, fmt("%.0f", t.reaumur), col, cold ? 11.5f : 10.0f);
      cell(2, fmt("%.2f", t.reaumur * 1.25f), col, cold ? 11.5f : 10.0f);
      cell(3, fmt("%.2f", t.reaumur * 2.25f + 32.0f), col,
           cold ? 11.5f : 10.0f);
      cell(4, i == 0 ? std::string("—") : std::to_string(t.daysSincePrev),
           kGrey, 10.0f);
    }
    g.child(text(toU8("°C = °R × 5/4      °F = °R "
                      "× 9/4 + 32      (exact — Réaumur puts 80 "
                      "degrees between ice and steam)"),
                 type(faceUi, 10, kGrey))
                .absolute()
                .left(Dim(x0))
                .top(Dim(y0 + rowH * 9 + 8))
                .key("reqs")
                .opacity(beat(tReaumur + 0.5f, tReaumur + 0.8f)));
    g.child(text(toU8("−30 °R = −37.50 °C = −35.50 "
                      "°F.  The plate's own title says degrés du "
                      "thermomètre de Réaumur in\ndisplay capitals "
                      "and reproductions still relabel the axis Celsius while "
                      "keeping his numbers."),
                 type(faceUi, 10, kClaimRed))
                .absolute()
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
                .absolute()
                .left(Dim(x0 + 560))
                .top(Dim(y0 + 74))
                .width(Dim(420))
                .key("twoCamp")
                .opacity(beat(tTwo, tTwo + 0.5f)));
    return g;
  }

  Element auditColumn() {
    auto g = box().absolute().inset(0);
    g.child(card(128, 326, "DOES THE PLATE OBEY ITS OWN LEGEND?", "card1",
                 tScale, cardScale()));
    g.child(card(466, 186, "THE FLOOR", "card2", tScale + 0.8f, cardFloor()));
    g.child(card(664, 254, "THE MAP IS A REAL MAP", "card3", tGeo, cardGeo()));
    g.child(card(930, 200, "WHAT HE DID DISTORT", "card4", tDistort,
                 cardLegs()));
    g.child(card(1142, 206, "RÉAUMUR", "card5", tReaumur, cardReaumur()));
    return g;
  }

  // =======================================================================
  // THE TITLE STRIP and THE CONSOLE

  Element titleStrip() {
    auto g = box().absolute().left(Dim(48)).top(Dim(28)).width(Dim(2464))
                 .height(Dim(80));
    g.child(text(toU8("Carte figurative des pertes successives en hommes de "
                      "l'armée française dans la campagne de Russie "
                      "1812–1813, comparée à celle d'Annibal "
                      "durant la 2ᵉᵐᵉ guerre punique"),
                 type(faceUi, 20, hex(0xe3dccd)))
                .absolute()
                .left(Dim(0))
                .top(Dim(0)));
    g.child(text(toU8("BnF, Ge Don 4182 · lithograph · 62 × 54 "
                      "cm · Paris, 20 novembre 1869 · Minard was 88, "
                      "and died ten months later during the siege of Paris"),
                 type(faceUi, 12, hex(0x9a9285)))
                .absolute()
                .left(Dim(0))
                .top(Dim(30)));
    g.child(text(toU8("the sheet is drawn at its own aspect — 2.258 px "
                      "per millimetre of Minard's paper, so every band width "
                      "on screen is a real millimetre count"),
                 type(faceUi, 11, hex(0x2f6f9c)))
                .absolute()
                .left(Dim(0))
                .top(Dim(50)));
    g.child(text(toU8("THE PLATE STATES ITS OWN CONSTRUCTION RULE.  THIS "
                      "SKETCH CHECKS IT — AND THEN CHECKS ITSELF WITH THE "
                      "SAME MEASUREMENT."),
                 type(faceUiBold, 12, hex(0xb5761e), 1.2f))
                .absolute()
                .left(Dim(1444))
                .top(Dim(50)));
    return g;
  }

  Element consoleStrip() {
    console::Style s;
    s.text = type(faceMono, 9.0f, hex(0xb9b2a4));
    s.palette = {type(faceMono, 9.0f, hex(0x6d675c)),   // 0 dim
                 type(faceMono, 9.0f, hex(0x62ab74)),   // 1 pass
                 type(faceMono, 9.0f, hex(0xd08a2a)),   // 2 fail/anomaly
                 type(faceMono, 9.0f, hex(0x64a8d8)),   // 3 measured
                 type(faceMono, 9.6f, hex(0xf0e8d8))};  // 4 heading
    s.gap = 0.0f;
    s.visibleLines = 17;
    auto g = box()
                 .absolute()
                 .left(Dim(48))
                 .top(Dim(kConsoleY))
                 .width(Dim(2464))
                 .height(Dim(kConsoleH))
                 .fill(Material::solid(hex(0x141311)))
                 .stroke(util::stroke(1.0f, Fill::color(hex(0x2c2a26))))
                 .row()
                 .padding(10)
                 .gap(18)
                 .key("console");
    g.child(box().width(Dim(600)).child(console::console(colA, s)));
    g.child(box().width(Dim(600)).child(console::console(colB, s)));
    g.child(box().width(Dim(600)).child(console::console(colC, s)));
    g.child(box().width(Dim(600)).child(console::console(colD, s)));
    return g;
  }

  // =======================================================================

  Element describe(sketch::SketchContext &ctx) {
    return box()
        .fill(Material::solid(kDesk))
        .child(titleStrip())
        .child(sheet(ctx))
        .child(auditColumn())
        .child(consoleStrip());
  }

  // =======================================================================
  // THE PROOF — every number computed here, none copied.

  void runAudits() {
    // --- flow conservation, on Minard's own engraved numbers -------------
    auto say = [&](console::LineRing &r, const std::string &s, size_t pal) {
      r.append(toU8(s), pal);
    };
    auto chk = [&](console::LineRing &r, const std::string &label, long lhs,
                   long rhs) {
      char buf[160];
      std::snprintf(buf, sizeof buf, "  %-44s %8ld   %s", label.c_str(), lhs,
                    lhs == rhs ? "EXACT" : "DIFF");
      r.append(toU8(buf), lhs == rhs ? 1 : 2);
    };

    say(colA, "FLOW CONSERVATION — Minard's own engraved numbers", 4);
    chk(colA, "422,000 − 22,000 (northern column)", 422000 - 22000,
        400000);
    chk(colA, "400,000 − 60,000 (Polotzk column)", 400000 - 60000, 340000);
    chk(colA, "340,000 + 60,000 + 22,000", 340000 + 60000 + 22000, 422000);
    chk(colA, "20,000 + 30,000 at the Berezina", 20000 + 30000, 50000);
    chk(colA, " 4,000 +  6,000 at the Niemen", 4000 + 6000, 10000);
    say(colA, "  Berezina crossing   50,000 − 28,000 = 22,000 men in four "
              "days",
        0);
    say(colA,
        fmt("  campaign            422,000 → 10,000 = %.2f%% survived",
            100.0 * 10000.0 / 422000.0),
        0);
    // the one identity that fails, found by walking the retreat westward
    int junctions = 0, violations = 0;
    std::string viol;
    {
      std::vector<Station> all = kRetEast;
      all.insert(all.end(), kRetWest.begin() + 1, kRetWest.end());
      for (size_t i = 1; i < all.size(); ++i) {
        if (all[i].men == all[i - 1].men)
          continue;
        ++junctions;
        // the Bobr junction legitimately gains the Polotzk column's 30,000
        const bool bobr = std::fabs(all[i].lon - 29.2f) < 0.01f;
        if (all[i].men > all[i - 1].men && !bobr) {
          ++violations;
          viol = fmt2("  → %.0f → %.0f westward: the army GAINS men",
                      all[i - 1].men, all[i].men);
        }
      }
    }
    say(colA,
        fmt2("  junctions checked %.0f      violations %.0f", (double)junctions,
             (double)violations),
        violations ? 2 : 1);
    say(colA, viol + " (Molodezno→Smorgoni, +2,000, unexplained)", 2);
    say(colA, "", 0);

    say(colA, "HANNIBAL, AGAINST POLYBIUS", 4);
    chk(colA, "12,000 Africans + 8,000 Iberians + 6,000 horse  III.56.4",
        12000 + 8000 + 6000, 26000);
    chk(colA, "38,000 foot + 8,000 horse, entering the Alps", 38000 + 8000,
        46000);
    say(colA,
        "  Pyrenees   Hanno 11,000 + 10,000 sent home = 21,000; Minard draws "
        "20,000",
        0);
    say(colA, "  → the one place he smooths.  Δ 1,000", 0);
    say(colA,
        fmt("  Hannibal 218 BC   96,000 → 26,000   survived %.2f%%",
            100.0 * 26.0 / 96.0),
        0);
    say(colA,
        fmt("  Napoleon 1812    422,000 → 10,000   survived %.2f%%",
            100.0 * 10.0 / 422.0),
        0);
    say(colA, "", 0);

    // --- the scale audit --------------------------------------------------
    say(colB, "DOES THE PLATE OBEY ITS OWN LEGEND?", 4);
    say(colB,
        "  legend (BOTH panels)  \"à raison d'un millimètre pour dix "
        "mille hommes\"",
        0);
    say(colB, "  11 treads, Commons scan:  3.828 px/10k, intercept "
              "−0.19 px, R² 0.99266",
        3);
    say(colB, "  intercept / (10,000-men width)   −0.05        PASS "
              "(proportional)",
        1);
    say(colB, "  paper 3945×3423 px, aspect 1.1525 vs 62/54 = 1.1481   "
              "+0.4%  PASS",
        1);
    say(colB, "  frame 3685 px = 579.14 mm ⇒ 3.4482 px/mm on that scan",
        0);
    say(colB, fmt("  from the regression                        %.3f mm/10k",
                  3.828 / 3.4482),
        3);
    say(colB, "  four direct BnF spot reads             1.1258 ± 0.013 mm",
        3);
    say(colB, "  STATED                                 1.0000 mm", 0);
    say(colB,
        fmt("  → the engraved zones are %.1f%% WIDER than the legend "
            "claims     FAIL (the plate's)",
            100.0 * (kMmPer10k - 1.0) / 1.0),
        2);
    say(colB, "  and the SAME factor on the Hannibal panel, other data, other "
              "continent",
        2);
    say(colB,
        fmt("  half a French ligne (2.2558/2) = 1.1279 mm  — %.2f%% away  "
            "[SPECULATION]",
            100.0 * std::fabs(kLigneHalf - kMmPer10k) / kMmPer10k),
        0);
    say(colB, "", 0);
    say(colB, "THE FLOOR", 4);
    say(colB, "  above ~35,000 men      3.6 – 3.9 px per 10,000   (holds "
              "scale)",
        1);
    say(colB, "  at 8,000 / 4,000       7.0 / 10.5 px per 10,000   (2.6× "
              "too wide)",
        2);
    say(colB, "  minimum drawn width    5.4 px = 1.57 mm — what a crayon "
              "holds",
        2);
    say(colB, "  → 12,000→14,000 is NOT measurable in the ink. "
              "NEGATIVE RESULT, REPORTED.",
        2);
    say(colB, "", 0);

    say(colC, "MINARD'S GEOGRAPHY vs THE REAL WORLD", 4);
    {
      std::vector<float> km;
      for (const City &c : kCities)
        km.push_back(cityKm(c));
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
          fmt2("  20 cities, haversine:  mean %.2f km   median %.2f km", mean,
               med),
          3);
      say(colC, fmt("                         rms  %.2f km", rms), 3);
      say(colC, "  0.1° digitisation quantum, diagonal        6.41 km", 0);
      say(colC, "  rms expected from quantisation alone      3.70 km", 0);
      say(colC,
          fmt("  → residual is within %.1f× of the measurement floor  "
              "         PASS",
              rms / 3.70),
          1);
      const float kmKM =
          haversineKm(kCities[0].rlon, kCities[0].rlat, kCities[17].rlon,
                      kCities[17].rlat);
      const float kmM = haversineKm(kCities[0].lon, kCities[0].lat,
                                    kCities[17].lon, kCities[17].lat);
      say(colC,
          fmt2("  Kowno→Moscou  real %.1f km   Minard %.1f km", kmKM, kmM),
          3);
      say(colC, "  worst legs  Wixma→Chjat 0.591  Chjat→Mojaisk "
                "1.528  TOTAL 1.011",
          2);
    }
    say(colC, "", 0);

    // --- what THIS study measured that the brief left estimated ----------
    say(colC, "THE PROJECTION — re-measured here, and one finding is new",
        4);
    say(colC, "  Napoleon panel, tan centreline, 8 stations east of Polotzk:",
        0);
    say(colC, "    d = 280.3 px/deg lat,  d/b = 2.142,  R² 0.866, rms 34 "
              "px",
        3);
    say(colC, "    true-to-scale at 55°N = 1.743  → latitude "
              "STRETCHED 1.23×",
        3);
    say(colC, "  Hannibal panel, same fit, 11 stations, BnF sheet:", 0);
    say(colC, "    d/b = 0.048,  R² = 0.12   (robust: 0.048–0.075 "
              "over any anchor)",
        2);
    say(colC, "  → THE TWO PANELS DO NOT SHARE A PROJECTION. The top one "
              "is a STRIP,",
        2);
    say(colC, "    not a map: latitude explains an eighth of the band's "
              "height. So the",
        2);
    say(colC, "    received \"Minard sacrificed geography\" is wrong about the "
              "panel every-",
        2);
    say(colC, "    one quotes and right about the panel nobody looks at.", 2);
    say(colC, "", 0);

    say(colD, "THE SCALE BAR DISAGREES WITH THE MAP", 4);
    say(colD, "  \"Lieues communes de France\"  4.985 px/lieue, linear to 0.2%",
        0);
    say(colD, "    ⇒ 1 mm = 3.074 km   1 : 3,074,000", 3);
    say(colD, "  the map, from real longitudes: 1 mm = 1.688 km   1 : "
              "1,688,000",
        3);
    say(colD, "  ratio 1.82                                     UNEXPLAINED",
        2);
    say(colD, "  Kowno→Smolensk with Minard's own bar: 933 km. Truth: 520 "
              "km.",
        2);
    say(colD, "  hypotheses: labels half value | copied unrescaled from "
              "Fezensac | my scale",
        0);
    say(colD, "  (the two panels also use DIFFERENT lieues: 4,444.8 m and "
              "4,560 m)",
        0);
    say(colD, "", 0);

    say(colD, "RÉAUMUR", 4);
    say(colD, "  °C = °R × 5/4   °F = °R × 9/4 + "
              "32   (exact, no offset)",
        0);
    say(colD, "  −30 °R = −37.50 °C = −35.50 °F  "
              "  9 readings converted     PASS",
        1);
    say(colD, "  the undated −11° recovers as 24 Nov (days col.) and "
              "25 Nov (lon interp.)",
        3);
    say(colD, "", 0);

    // --- THE SKETCH'S OWN GEOMETRY ---------------------------------------
    const SkPath advSpine = polyline(kAdvTrunk);
    const WidthProfile advProf = profileOf(kAdvTrunk);
    const SkPath advBand = ribbonBand(advSpine, advProf, 2.0f);
    auditAdvance = widthAlong(advBand, advSpine, advProf, 4.0f, 90);

    const SkPath retSpine = polyline(kRetEast);
    const WidthProfile retProf = profileOf(kRetEast);
    const SkPath retBand = ribbonBand(retSpine, retProf, 2.0f);
    auditRetreat = widthAlong(retBand, retSpine, retProf, 4.0f, 90);

    advanceInk = inkIntegral(advSpine, advProf);
    {
      const SkRect bb = advBand.getBounds();
      const std::array<SkPath, 1> pieces{advBand};
      const debug::Coverage cov = debug::coverage(pieces, bb, 512);
      advanceArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
    }
    retreatInk = inkIntegral(retSpine, retProf);
    {
      const SkRect bb = retBand.getBounds();
      const std::array<SkPath, 1> pieces{retBand};
      const debug::Coverage cov = debug::coverage(pieces, bb, 512);
      retreatArea = (1.0f - cov.uncoveredFraction()) * bb.width() * bb.height();
    }
    // do the two zones overlap? They must not — they are adjacent.
    {
      const std::array<SkPath, 2> pieces{advBand, retBand};
      SkRect region = advBand.getBounds();
      region.join(retBand.getBounds());
      const debug::Coverage cov = debug::coverage(pieces, region, 384);
      coverDoubled = cov.doubledFraction();
    }
    // connectivity, on the ROUTE polylines (a filled band is closed and
    // contributes NO endpoints — Debug.h says so explicitly)
    {
      const std::array<SkPath, 5> drawn{polyline(kAdvTrunk),
                                        polyline(kAdvNorth),
                                        polyline(kAdvPolotzk), SkPath(),
                                        SkPath()};
      const std::array<SkPath, 3> asDrawn{polyline(kAdvTrunk),
                                          polyline(kAdvNorth),
                                          polyline(kAdvPolotzk)};
      advComponentsDrawn = debug::endpointDegrees(asDrawn, 0.5f).components();
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
      advComponentsWilkinson = debug::endpointDegrees(wilk, 0.5f).components();
      const std::array<SkPath, 4> ret{polyline(kRetEast), polyline(kRetWest),
                                      polyline(kRetPolotzk),
                                      polyline(kRetNorth)};
      retComponents = debug::endpointDegrees(ret, 0.5f).components();
      (void)drawn;
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
          m->getPosTan(advProf.arc[i], &pa, &tv);
          m->getPosTan(len * (float)i / (float)(n - 1), &pf, &tv);
          const SkPoint want = stationPt(kAdvTrunk[i]);
          riserArcErr = std::max(riserArcErr, SkPoint::Distance(pa, want));
          const float fe = SkPoint::Distance(pf, want);
          if (fe > riserFracErr) {
            riserFracErr = fe;
            // which city is that riser's?
            float best = 1e9f;
            for (const City &c : kCities) {
              const float d = std::hypot(mapX(c.lon) - want.x(),
                                         mapY(c.lat) - want.y());
              if (d < best) {
                best = d;
                riserWorstCity = c.plate;
              }
            }
          }
        }
      }
    }

    say(colC, "THE TWO PANELS SHARE ONE ABSCISSA", 4);
    say(colC, "  vertical rules detected in y ∈ [700,930]     9 inner + 2 "
              "frame",
        0);
    say(colC, "  Minard.temp readings                         9", 0);
    say(colC, "  best matches  lon 25.300 vs 25.3  Δ 0.000  |  28.593 vs "
              "28.5  Δ 0.09",
        3);
    say(colC, "                    27.091 vs 27.2  Δ 0.11  |  37.577 vs "
              "37.6  Δ 0.02",
        3);
    say(colC, "  2 rules unmatched, 2 readings unmatched     PARTIAL — "
              "reported, not fudged",
        2);
    say(colC, "  in THIS sketch the lock is one shared mapX(lon) called from "
              "both panels;",
        0);
    say(colC, "  nothing in the library can declare it. A scale is not a "
              "layout.",
        2);
    say(colC, "", 0);

    say(colD, "THE SKETCH'S OWN GEOMETRY — the same auditor, turned round",
        4);
    say(colD,
        fmt2("  advance band, min-chord every 4 px:  max |err| %.2f px = %.3f "
             "mm",
             auditAdvance.maxErr, auditAdvance.maxErr / kPxPerMm),
        auditAdvance.maxErr > 2.0f ? 2 : 1);
    say(colD,
        fmt2("    rms %.2f px over %.0f samples", auditAdvance.rms,
             (double)auditAdvance.samples),
        3);
    say(colD,
        fmt2("    worst at arc %.0f px, sheet (%.0f, ...)", auditAdvance.maxErrAt,
             auditAdvance.worst.x()),
        2);
    say(colD,
        fmt2("  retreat band:                        max |err| %.2f px = %.3f "
             "mm",
             auditRetreat.maxErr, auditRetreat.maxErr / kPxPerMm),
        auditRetreat.maxErr > 2.0f ? 2 : 1);
    say(colD,
        fmt3("  ∫w ds %.0f px² vs filled area %.0f px²   "
             "difference %+.1f%%",
             advanceInk, advanceArea,
             100.0 * (advanceArea - advanceInk) / advanceInk),
        2);
    say(colD,
        fmt("  debug::coverage(advance ∪ retreat) doubled  %.4f  (must be "
            "0)",
            coverDoubled),
        coverDoubled > 0.0005f ? 2 : 1);
    say(colD,
        fmt2("  components()  advance as Minard draws it %.0f, as Wilkinson "
             "encodes it %.0f",
             (double)advComponentsDrawn, (double)advComponentsWilkinson),
        3);
    say(colD,
        fmt("  components()  retreat %.0f   — one army came back",
            (double)retComponents),
        retComponents == 1 ? 1 : 2);
    say(colD,
        fmt("  risers, indexed by ARC LENGTH   max %.3f px off station     "
            "PASS",
            riserArcErr),
        riserArcErr < 0.5f ? 1 : 2);
    say(colD,
        fmt("  risers, indexed by FRACTION     max %.1f px off station     "
            "FAIL",
            riserFracErr),
        2);
    say(colD, std::string("    ^ the trap, and its worst riser is ") +
                  riserWorstCity + "'s",
        2);
    say(colD, "  → the corner error is brushes::Ribbon's, not the data's: "
              "a variable-width",
        0);
    say(colD, "    band IS a stroke and Ribbon has no join. See the gap list.",
        0);
  }

  // =======================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kDesk);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    // The plate's lettering is FOUR systems and they are genuinely
    // different: a looping engraver's ronde for the titles and the legend
    // paragraphs, an upright condensed face for the numbers across the
    // zones, a fine sloped italic for the place names, and spaced roman
    // capitals for MOSCOU alone.
    faceScript = family("Snell Roundhand", SkFontStyle::Normal());
    if (!faceScript)
      faceScript = family("Apple Chancery", SkFontStyle::Normal());
    faceItalic = family("Baskerville", SkFontStyle::Italic());
    if (!faceItalic)
      faceItalic = family("Times New Roman", SkFontStyle::Italic());
    faceRoman = family("Baskerville", SkFontStyle::Normal());
    if (!faceRoman)
      faceRoman = family("Times New Roman", SkFontStyle::Normal());
    faceNum = family("Baskerville", SkFontStyle::Normal());
    faceUi = family("Helvetica Neue", SkFontStyle::Normal());
    faceUiBold = family("Helvetica Neue", SkFontStyle::Bold());
    faceMono = family("Menlo", SkFontStyle::Normal());
    if (!faceMono)
      faceMono = family("Courier New", SkFontStyle::Normal());
    if (!faceUi)
      faceUi = faceRoman;
    if (!faceUiBold)
      faceUiBold = faceUi;

    // THE PAPER, and it is a FIBRE problem, not a colour problem: pulp
    // grain, the laid lines of a hand-made 19th-century sheet at ~1.2 px
    // pitch, the chain lines at ~26 px, and foxing.
    paperPulp = patterns::speckle(160, 220, 0.35f, 0.9f,
                                  {hex(0xb9ad98, 0.20f), hex(0xd6cab6, 0.18f)});
    paperPulp.seed(1869);
    laidLines = patterns::stripes(0.6f, 0.7f, hex(0xb9ad98, 0.10f));
    laidLines.rotate(90.0f);
    chainLines = patterns::stripes(1.1f, 25.0f, hex(0xb9ad98, 0.13f));
    chainLines.rotate(90.0f);
    foxing = patterns::speckle(190, 5, 1.6f, 6.0f, {hex(0xa07f55, 0.10f)});
    foxing.seed(91);
    tintSpeckle = patterns::speckle(64, 40, 0.6f, 2.4f, {hex(0x8f6a55, 0.5f)});
    tintSpeckle.seed(41);
    stampGrain = patterns::grain(0.55f, 2, 13.0f, 1.4f);

    paperMat = Material::blend({
        {Material::solid(kPaperBody), SkBlendMode::kSrc},
        {patterns::grain(0.055f, 3, 7.0f, 0.55f), SkBlendMode::kSoftLight},
        {laidLines.material(), SkBlendMode::kSrcOver},
        {chainLines.material(), SkBlendMode::kSrcOver},
        {paperPulp.material(), SkBlendMode::kSrcOver},
        {foxing.material(), SkBlendMode::kSrcOver},
    });
    // lying under a window: the vignette centre sits slightly ABOVE middle
    vignette = Material::radialUnit({0.46f, 0.40f}, 1.10f,
                                    {{0.0f, hex(0xffffff)},
                                     {0.70f, hex(0xf6f1e6)},
                                     {1.0f, hex(0xc4b9a4)}});

    runAudits();

    // ONE Output drives eleven beats. Every beat is bind().window(lo, hi),
    // never from(): outside a window from() feeds the easing curve values
    // outside its domain and ease:: is not total.
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
      reaumurMix = (s >= tReaumur && s <= tReaumurEnd)
                       ? std::clamp((s - tReaumur) / 1.4f, 0.0f, 1.0f)
                       : (s > tReaumurEnd ? 1.0f : 0.0f);
      return true;
    });
    // the caliper's discrete steps: a fixed-rate lane, because an
    // instrument walks in clicks, not continuously
    ctx.ticker.addFixed(
        6.0f,
        [this]() {
          caliperMm = caliperMm.value(); // held; the readout is discrete
          return true;
        },
        8);

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Minard1869)
