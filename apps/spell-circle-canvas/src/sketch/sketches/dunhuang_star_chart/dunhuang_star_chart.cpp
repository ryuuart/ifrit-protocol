// dunhuang_star_chart.cpp — BL Or.8210/S.3326, the Dunhuang Star Chart,
// REPROJECTED. Not traced: derived from real stars and the published
// projection, then checked against the published identifications.
//
// THE ARTEFACT. British Library Or.8210/S.3326 — the oldest complete star
// atlas known from any civilisation. Recovered by Aurel Stein from the
// sealed Library Cave (Cave 17) at Mogao, Dunhuang, in 1907; catalogued by
// Lionel Giles under "divination", no. 6974. Dated +649–684 on
// taboo-character evidence. A scroll of Chinese paper 3,940 mm long ×
// 244 mm wide, inscribed one side only, the original sheet 0.04 mm thick
// and analysed as PURE MULBERRY FIBRE. The beginning is missing, so there
// is no title and no author. Two sections run without a break: an
// uranomancy text of 80 columns under 26 cloud drawings (column 43 cites
// "your servant Chunfeng" — Li Chunfeng, the Tang court astronomer), then
// the atlas, 2,100 mm long, 13 maps, 50 columns of text. And then, at the
// very end, a bowman in traditional dress captioned as the god of
// lightning, over a title nobody can read.
//
// WHY IT MUST BE DERIVED. There is no published table of measured (x, y)
// positions for the 1,339 dots. What exists is a four-part chain, and this
// study is the join:
//
//   1. Bonnet-Bidaud, Praderie & Whitfield, JAHH 12(1) 39–59 (2009),
//      arXiv:0906.3034 — TABLE 3, the chart's own measured projection
//      (scales in °/cm, residuals, correlations, RA/DEC limits), and
//      TABLES 4 and 5, the per-asterism content of maps 5 and 13.
//      `pdftotext -layout` recovers all three cleanly.
//   2. Stellarium `skycultures/chinese_chenzhuo/index.json` (GPL) — 317
//      asterisms, 1,883 vertex words, 1,747 line vertices, 1,463 star
//      TOKENS. Provenance: the Chen Zhuo catalogue and the Three Schools'
//      Star Canons — Chen Zhuo (230s–320s CE) synthesised Shi, Gan and
//      Wu Xian into 283 constellations and 1,464 stars.
//   3. astronexus/HYG-Database v4.1 `hyg/CURRENT/hygdata_v41.csv`, joined
//      on HIP for RA/Dec/mag/proper motion.
//   4. The IDP scan (204.8 px/cm) for the PAPER — fibre, tone, the roll's
//      replication marks. Not for positions.
//
// FIVE THINGS THE DATA SETTLES, AGAINST THE OBVIOUS READING.
//
//   1. 1,463 is a TOKEN count, not a HIP count. Three of the tokens are
//      deep-sky objects — DSO:M44 (積屍氣, the cadaverous vapour in the
//      Ghost mansion), DSO:M7, DSO:M31. So the dataset carries 1,460 HIP
//      numbers, not 1,463, against Chen Zhuo's canonical 1,464.
//   2. A naive HIP join loses exactly THREE stars, and the cause is one
//      thing: HYG v4.1 BLANKS the hip field on the resolved components of
//      visual doubles. HIP 55203 = ξ UMa (in HYG twice, as "Alula
//      Australis" and "Alula Australis B", both unnumbered), HIP 78727 =
//      ξ Sco (twice, both unnumbered), HIP 115125 = 94 Aqr B (HYG keeps
//      only 94 Aqr A). Identities confirmed against SIMBAD. Falling back
//      to the Bayer designation recovers all three: 1,460 / 1,460.
//   3. THE PAPER PRECESSED TO +700, NOT +665. §4.1: "corrected for proper
//      motion and precessed to the date +700". This file follows the
//      paper. The difference is 0.489° of RA over 35 years, which is 4.6×
//      below map 5's own RA mean residual of 2.26° — printed on the plate,
//      because an epoch you cannot resolve is a result.
//   4. Table 5 runs to 34 asterisms and 142 stars — a longer list than
//      Table 4's twenty rows for map 5, not a matching one.
//   5. Ziwei is NOT the only mixed-colour asterism: Table 5 row 13,
//      Tianpei 天棓, reads "5 R, 1 B?" — question mark and all.
//
// THE TWO QUESTIONS, MEASURED RATHER THAN QUOTED. The published
// correlations cannot separate the candidate projections, and the reason
// is computable from Table 3 alone — so this plate computes it.
//
//   LINEAR vs MERCATOR. Over map 5's own DEC range (−27…+43°) the two
//   ordinates part company by at most 2.184° of declination — 4.14 mm on
//   244 mm of paper. Map 5's DEC mean residual is 1.61°. The signal is
//   1.36× the noise over 15 stars, which is why R differs by 0.002. And
//   with n = 15 and r = 0.996 the standard error on r is 0.0021, so the
//   published 0.002 is 0.95σ — the LARGER of the two comparisons, and it
//   favours PURE CYLINDRICAL, not Mercator. All three maps favour it, by
//   0.001–0.002, every time (0.974/0.972, 0.975/0.974, 0.996/0.994). A
//   3-of-3 sign test is p = 0.125. Not a result — but it is the opposite
//   sign to the "Mercator, centuries before Mercator" reading the paper
//   quotes from Needham, and the plate says so.
//
//   EQUIDISTANT vs STEREOGRAPHIC. Over the disc's 38° of polar distance
//   the two part by at most 0.432° — 0.85 mm of paper — against a radial
//   mean residual of 3.29°. Ratio 0.13. With n = 19 and r = 0.919 the
//   standard error on r is 0.037, so the published 0.013 win is 0.36σ.
//   The disc cannot answer the question BECAUSE IT STOPS AT +52°: over a
//   full hemisphere the same pair would part by 7.00°.
//
// A THIRD QUESTION THE PAPER DOES NOT ASK, AND ITS TWO NUMBERS CONTRADICT
// EACH OTHER. The atlas is 2,100 mm for 13 maps and 50 columns. Table 3
// gives an RA scale of 4.24–4.56 °/cm and a per-map RA extension of
// 45–48°. Take 48° at 4.56: the drawn map is 10.53 cm, twelve of them are
// 126.3 cm, and with the disc at 20.4 cm that leaves 63.3 cm for 50
// columns = 12.7 mm each — a normal Tang column. But 12 × 48° = 576° over
// a 360° sky, so adjacent maps would share 18°, and the sky would be drawn
// 1.6 times over — against a census of 1,339 dots for a canon of 1,464.
// Take instead 30° per map (12 × 30 = 360 exactly, one dot per star): the
// drawn map is 6.6 cm, and the columns come out 21.6 mm wide, which is not
// a Tang column. NEITHER READING CLOSES. This plate draws the first —
// 48°-wide frames butted at a 30° pitch — so the RA tick ladders JUMP BACK
// 18° at every map boundary, and the contradiction is visible rather than
// argued.
//
// DO NOT CLEAN UP THE ARTEFACT. Six documented defects in map 5's twenty
// asterisms are drawn AS FOUND and flagged, never fixed: Shuifu and Sidu
// have their labels interchanged; Shen (Orion) has no label at all, and
// its three hazy red stars — Fa, the dagger — carry none either; Ping is
// "labelled but not at its place"; Jiuliu is unlabelled; Zhangren and Zi
// "should be more S"; and Tiangao's reference mansion, Bi, is on the
// PREVIOUS map. On the disc: six unaccounted stars in the emperor's
// canopy, a nameless dot east of Gouchen, and "a red non-encircled star,
// slightly erased, could be the Pole star". A study that corrects those
// has destroyed the object.
//
// WHAT THE JOIN ANSWERED THAT THE PAPER LEFT OPEN. Table 5 asks of Huagai
// 華蓋, the canopy of the emperor: "7 stars (+ 6) — what are the 6? is it
// Gang? the character for Gang is absent." The Chen Zhuo dataset carries
// 杠(附華蓋) Gang as a real asterism appended to Huagai, 9 line vertices.
// So the six unlabelled dots are consistent with Gang being DRAWN and not
// WRITTEN — but Chen Zhuo's Gang is 9 stars and the map has 6, so the
// count does not close. Both numbers are printed and neither is chosen.
//
// AND WHERE THE THREE SOURCES DISAGREE, ALL THREE ARE PRINTED. Map 5's
// twenty asterisms: Chen Zhuo 116 stars, SXC 115, the map 108 — while
// Table 4's own stated total is 109, one MORE than its own n(map) column
// sums to. Table 5 sums to 141 against a stated 142. Wuche closes exactly
// only when Sanzhu is folded in (Chen Zhuo 5 + Sanzhu 9 = SXC's 14).
// Ziwei's two walls close exactly (東垣 8 + 西垣 7 = 15 = Table 5). And
// Table 4's 左旗 for the Auriga asterism is Chen Zhuo's 坐旗 — a different
// character for the same nine stars, while a DIFFERENT 左旗 exists in the
// Dipper region.
//
// THE MODE OF LINE. Not engraved and not written: DRAWN, with a
// draughtsman's fine brush on 0.04 mm mulberry. One weight,
// one ring, three fills, and a hand-drawn join BOWS: every asterism line
// here is shapers::Jitter at low amplitude over a polyline through real dot
// centres, revealed by spans::upTo(gate(...)) in RA order so the joining
// sweeps right-to-left across the scroll, in reading order.
//
// BUILT FROM (the library, not by hand):
//   instancing::Atlas/Pool   1,460 dots as ONE leaf, Mode::Live, rebuilt
//                        from the precession matrix every frame while the
//                        epoch runs. FIVE cells, not "three plus a tint":
//                        an atlas cell is a whole ELEMENT TREE, so fill
//                        AND ring bake into one sprite, and no second
//                        concentric pass is needed for the ring.
//   lines::Rails         the scroll's top and bottom rules are NOT equal
//                        weights; the map frames; the disc's limb
//   PathFormat::trimStart/trimEnd   28 mansion rules that stop short of
//                        both edges — interrupted, not chords
//   shapers::Jitter      every asterism join, and the paper's own edge
//   shapes::parametric   the disc's radial DEC scale and the graticule
//   shapes::annulus / sector / circle / spiral
//   brush::Ribbon + Profile   the archer, keyed in arc-length PX so the
//                        brush press does not slide under the reveal
//   brush::Scatter   the roll's contact replication marks
//   patterns::grain (anisotropic) + Cache::Texture   the mulberry ground
//   Bracket/Gapped Borders   with an EXPLICIT angleDeg — the
//                        disc's 28-fold division turns 12.86° per vertex
//                        and the 30° default finds no corners at all
//   TextPath::Orient::Radial   the disc's mansion names
//   spans::upTo + gate()  one Output writes the whole plate
//   feed::TextRing       three panels of checks, printed as they run
//   slot()/renderSlot()  the audit panel re-renders without a full
//                        render() of the scroll
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/dunhuang_star_chart/dunhuang_star_chart.cpp \
//       --frame /tmp/dunhuang_star_chart.png   (the settled plate)
//
//    1.6 s  the mulberry ground, the roll's contact ghosts, the 1950s
//           Kraft lining showing warm at the edges
//    2.2 s  1,460 real stars arrive as SKY, J2000, unprojected
//    4.4 s  precession runs — J2000 → +700, the whole sky sliding 18.5°
//    8.3 s  the fold: the stars leave the sphere and land on the scroll;
//           the disc forms separately, azimuthally, centred 2.4° off the
//           pole because Table 3 says +87.6° and not +90°
//   12.0 s  317 asterisms draw themselves, staggered by RA, right to left
//   18.6 s  the audit: map 5's twenty, defect by defect
//   23.5 s  the two projection questions resolve, and neither wins
//   26.0 s  the archer, and the title nobody can read
//   31 s loops.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
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

#include "Catalogue.h"

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace patterns = sigil::material::pattern;
namespace shapers = sigil::geometry::shapers;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace dunhuang;
using sigil::material::skia::Paint;
using sigil::weave::ports::pickTypeface;
namespace noise = sigil::core::noise;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

template <typename... A>
std::string fmt(const char* f, A... args) {
  char buf[512];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::snprintf(buf, sizeof(buf), f, args...);
#pragma clang diagnostic pop
  return buf;
}

// ---------------------------------------------------------------------------
// palette — mulberry paper, 0.04 mm, thirteen centuries old, fully lined with
// brown Kraft in the 1950s. The ink is a carbon black that has gone brown at
// the edges of every stroke; the three schools are cinnabar, that same
// carbon, and an ochre-white lead that has oxidised warm.

constexpr SkColor4f kVoid = hex(0x14120e);
constexpr SkColor4f kPaperDeep = hex(0xb99f72);
constexpr SkColor4f kPaperMid = hex(0xd6bf95);
constexpr SkColor4f kPaperLit = hex(0xe8d6ad);
constexpr SkColor4f kPaperPale = hex(0xf1e4c2);
constexpr SkColor4f kKraft = hex(0xa87f4c);
constexpr SkColor4f kInk = hex(0x2a2118);
constexpr SkColor4f kInkSoft = hex(0x5d4c37);
constexpr SkColor4f kInkFaint = hex(0x8a7458);
constexpr SkColor4f kCinnabar = hex(0xa8382a);
constexpr SkColor4f kLead = hex(0xf4ecd8);
constexpr SkColor4f kRule = hex(0x6b573c);
constexpr SkColor4f kTrace = hex(0x2f6d86);
constexpr SkColor4f kFlag = hex(0xb4531f);
constexpr SkColor4f kChalk = hex(0xcbb894);

// ---------------------------------------------------------------------------
// canvas, and the scroll's own metric. Everything below is in MILLIMETRES of
// real paper until it is multiplied by kPxMm.

constexpr float kW = 2560, kH = 1600;
constexpr float kPxMm = 2.22f;  // px per mm of scroll
constexpr float kD = 3.14159265358979f / 180.0f;

// Table 3, map 5 (the best-measured of the twelve)
constexpr float kRaPerMm = 0.456f;    // 4.56 °/cm horizontal
constexpr float kDecPerMm = 0.528f;   // 5.28 °/cm vertical
constexpr float kPolPerMm = 0.510f;   // 5.10 °/cm radial, map 13
constexpr float kAzGain = 1.05f;      // Table 3: azimuthal scale, theory 1.00
constexpr float kDiscCenDec = 87.6f;  // Table 3: map 13 centre DEC — NOT +90

constexpr float kScrollMm = 3940.0f, kWideMm = 244.0f;
constexpr float kAtlasMm = 2100.0f;
constexpr float kMapWmm = 48.0f / kRaPerMm;   // 105.26 mm of drawn map
constexpr float kMapHmm = 90.0f / kDecPerMm;  // 170.45 mm of drawn map
constexpr float kDiscMm = 204.0f;             // the disc's slot
constexpr float kSlotMm = (kAtlasMm - kDiscMm) / 12.0f;  // 158.0 mm
constexpr float kColBandMm = kSlotMm - kMapWmm;          // 52.74 mm of columns
constexpr float kColsPerMap = 50.0f / 12.0f;
constexpr float kColMm = kColBandMm / kColsPerMap;  // 12.66 mm per column

constexpr float kBandTop = 424.0f;
constexpr float kBandH = kWideMm * kPxMm;  // 541.68 px
constexpr float kBandMid = kBandTop + kBandH * 0.5f;
constexpr float kFrameTop = kBandTop + (kWideMm - kMapHmm) * 0.5f * kPxMm;
constexpr float kFrameH = kMapHmm * kPxMm;

// two scroll segments with a drafting break between them
constexpr float kBreakL = 1148.0f, kBreakR = 1206.0f;
constexpr float kSegTop = 244.0f;
constexpr float kSegH = 780.0f;
constexpr float kOriginR = 3420.0f;  // canvas x of scroll coordinate s = 0
constexpr float kOriginL = 5076.0f;

// the score
constexpr float tPaper = 0.15f;
constexpr float tSky = 2.20f;
constexpr float tPrec0 = 4.40f, tPrec1 = 8.00f;
constexpr float tFold0 = 8.30f, tFold1 = 12.20f;
constexpr float tLine0 = 12.00f, tLine1 = 18.40f;
constexpr float tAudit = 18.60f, tAuditEach = 0.26f;
constexpr float tProj = 23.40f;
constexpr float tArch = 26.00f;
constexpr float tSettle = 28.20f;
constexpr float kLoop = 31.0f;

inline float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
inline float smooth(float v) {
  v = clamp01(v);
  return v * v * (3 - 2 * v);
}
inline float wrap360(float d) {
  d = std::fmod(d, 360.0f);
  return d < 0 ? d + 360.0f : d;
}
inline float wrap180(float d) {
  d = wrap360(d);
  return d > 180.0f ? d - 360.0f : d;
}

// ---------------------------------------------------------------------------
// PRECESSION — IAU 1976 ζ/z/θ. T is Julian centuries from J2000 and sweeps
// 0 → −13.00 cy over the score, J2000 → +700.

struct Mat3 {
  float m[9];
};

Mat3 precMatrix(float T) {
  const float s = kD / 3600.0f;
  const float z1 =
      (2306.2181f * T + 0.30188f * T * T + 0.017998f * T * T * T) * s;
  const float z2 =
      (2306.2181f * T + 1.09468f * T * T + 0.018203f * T * T * T) * s;
  const float th =
      (2004.3109f * T - 0.42665f * T * T - 0.041833f * T * T * T) * s;
  const float c1 = std::cos(z1), s1 = std::sin(z1);
  const float c2 = std::cos(z2), s2 = std::sin(z2);
  const float ct = std::cos(th), st = std::sin(th);
  return {{c1 * ct * c2 - s1 * s2, -s1 * ct * c2 - c1 * s2, -st * c2,
           c1 * ct * s2 + s1 * c2, -s1 * ct * s2 + c1 * c2, -st * s2, c1 * st,
           -s1 * st, ct}};
}

inline void precess(const Mat3& M, float ra, float dec, float& raOut,
                    float& decOut) {
  const float r = ra * kD, d = dec * kD;
  const float cd = std::cos(d);
  const float v0 = cd * std::cos(r), v1 = cd * std::sin(r), v2 = std::sin(d);
  const float w0 = M.m[0] * v0 + M.m[1] * v1 + M.m[2] * v2;
  const float w1 = M.m[3] * v0 + M.m[4] * v1 + M.m[5] * v2;
  const float w2 = M.m[6] * v0 + M.m[7] * v1 + M.m[8] * v2;
  raOut = wrap360(std::atan2(w1, w0) / kD);
  decOut = std::asin(std::clamp(w2, -1.0f, 1.0f)) / kD;
}

// ---------------------------------------------------------------------------
// THE CATALOGUE — the 1,460 stars, the vertex words, the 317 asterisms
// and the 28 mansion anchors — stands in Catalogue.cpp beside this
// file, generated once from the join and frozen: a unit of this sketch
// that an edit here never compiles again.

// ---------------------------------------------------------------------------
// THE MAPS. Twelve hour-angle maps on a 30° RA ladder anchored at map 1's
// published centre; the two other published centres run 5–6° hot against it,
// which is the same scatter the paper reports for the equator. The DRAWN
// width is Table 3's 48° extension. See the header: the two do not reconcile.

constexpr float kMapCentre1 = 308.0f;
inline float mapCentre(int k) {  // k = 1..12
  if (k == 1) return 308.0f;     // published
  if (k == 2) return 344.0f;     // published (ladder says 338)
  if (k == 5) return 73.0f;      // published (ladder says 68)
  return wrap360(kMapCentre1 + 30.0f * (float)(k - 1));
}
inline float mapGcDec(int k) {  // Table 3 "geometrical centre (DEC)"
  if (k == 1) return -14.0f;
  if (k == 2) return -8.0f;
  if (k == 5) return 5.0f;
  return -6.0f;  // the mean of the three, for the nine
}  // maps the paper did not fit
inline int mapOfRa(float ra) {
  int k = (int)std::lround(wrap180(ra - kMapCentre1) / 30.0f) + 1;
  while (k < 1) k += 12;
  while (k > 12) k -= 12;
  return k;
}
// scroll coordinate s, in mm from the atlas's RIGHT edge, increasing LEFTWARD
// (which is the direction the scroll reads and the direction RA increases).
inline float mapSlotS(int k) { return (float)(k - 1) * kSlotMm; }
inline float discCentreS() { return 12.0f * kSlotMm + kDiscMm * 0.5f; }

// ---------------------------------------------------------------------------
// THE TWO PROJECTION QUESTIONS, computed rather than quoted.

struct Departure {
  float maxDeg, mm, ratio, sigma;
};

// max departure of a Mercator ordinate from its own best-fit straight line
// over [lo, hi] of declination, expressed back in DEGREES of declination.
Departure mercatorVsLinear(float lo, float hi, float resid, float degPerCm,
                           float r, int n) {
  const int N = 400;
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  std::array<float, N + 1> xs{}, ys{};
  for (int i = 0; i <= N; ++i) {
    const float dec = lo + (hi - lo) * (float)i / (float)N;
    const float y = std::log(std::tan((45.0f + dec * 0.5f) * kD));
    xs[(size_t)i] = dec;
    ys[(size_t)i] = y;
    sx += dec;
    sy += y;
    sxx += dec * dec;
    sxy += dec * y;
  }
  const float nn = (float)(N + 1);
  const float b = (nn * sxy - sx * sy) / (nn * sxx - sx * sx);
  const float a = (sy - b * sx) / nn;
  float mx = 0;
  for (int i = 0; i <= N; ++i)
    mx = std::max(mx, std::abs((ys[(size_t)i] - (a + b * xs[(size_t)i])) / b));
  const float se = (1.0f - r * r) / std::sqrt((float)(n - 1));
  return {mx, mx / degPerCm * 10.0f, mx / resid, se};
}

// … and its azimuthal twin: stereographic tan(p/2) against equidistant p.
Departure stereoVsEquidistant(float lo, float hi, float resid, float degPerCm,
                              float r, int n) {
  const int N = 400;
  float sx = 0, sy = 0, sxx = 0, sxy = 0;
  std::array<float, N + 1> xs{}, ys{};
  for (int i = 0; i <= N; ++i) {
    const float p = lo + (hi - lo) * (float)i / (float)N;
    const float y = std::tan(p * 0.5f * kD);
    xs[(size_t)i] = p;
    ys[(size_t)i] = y;
    sx += p;
    sy += y;
    sxx += p * p;
    sxy += p * y;
  }
  const float nn = (float)(N + 1);
  const float b = (nn * sxy - sx * sy) / (nn * sxx - sx * sx);
  const float a = (sy - b * sx) / nn;
  float mx = 0;
  for (int i = 0; i <= N; ++i)
    mx = std::max(mx, std::abs((ys[(size_t)i] - (a + b * xs[(size_t)i])) / b));
  const float se = (1.0f - r * r) / std::sqrt((float)(n - 1));
  return {mx, mx / degPerCm * 10.0f, mx / resid, se};
}

// ---------------------------------------------------------------------------
// TABLE 4 — the content of map 5 (the Orion region), transcribed from
// arXiv:0906.3034 verbatim, defects and all. `cid` keys into the Chen Zhuo
// dataset above; 'M' in `col` is a mixed-colour asterism.

struct M5Row {
  const char *cid, *pinyin, *native, *gloss;
  char col;
  int nSxc, nMap, conf;
  const char* defect;
};
const M5Row kMap5[20] = {
    {"19H", "Wuche + Sanzhu",
     "\xe4\xba\x94\xe8\xbb\x8a+\xe4\xb8\x89\xe6\x9f\xb1",
     "five chariots + three poles", 'R', 14, 14, 5, nullptr},
    {"19E", "Zhuwang", "\xe8\xab\xb8\xe7\x8e\x8b", "several princes", 'R', 6, 5,
     5, nullptr},
    {"20C", "Zuoqi", "\xe5\xb7\xa6\xe6\x97\x97", "left banner", 'B', 9, 8, 5,
     "Table 4 writes \xe5\xb7\xa6\xe6\x97\x97; Chen Zhuo writes "
     "\xe5\x9d\x90\xe6\x97\x97"},
    {"22E", "Tianzun", "\xe5\xa4\xa9\xe6\xa8\xbd", "celestial wine cup", 'B', 3,
     3, 1, nullptr},
    {"19F", "Tiangao", "\xe5\xa4\xa9\xe9\xab\x98", "celestial high terrace",
     'W', 4, 4, 4, "reference mansion Bi is on MAP 4, not this one"},
    {"22A", "Jing", "\xe4\xba\x95", "eastern well", 'W', 8, 8, 5, nullptr},
    {"19O", "Shenqi", "\xe5\x8f\x83\xe6\x97\x97", "Shen banner", 'R', 9, 6, 5,
     nullptr},
    {"20A", "Zui", "\xe8\xa7\x9c", "bird beak", 'W', 3, 3, 5, nullptr},
    {"22I", "Shuifu", "\xe6\xb0\xb4\xe5\xba\x9c", "water palace", 'B', 4, 4, 4,
     "labels INTERCHANGED with Sidu"},
    {"22K", "Sidu", "\xe5\x9b\x9b\xe7\x80\x86", "four rivers", 'B', 4, 4, 4,
     "labels INTERCHANGED with Shuifu"},
    {"21A", "Shen", "\xe5\x8f\x83", "warrior-hunter", 'M', 10, 10, 5,
     "NO LABEL on the map; Fa the dagger unlabelled too"},
    {"19P", "Jiuliu", "\xe4\xb9\x9d\xe6\x96\x9a", "nine flags", 'B', 9, 9, 5,
     "NO LABEL on the map"},
    {"21C", "Yujing", "\xe7\x8e\x89\xe4\xba\x95", "jade well", 'R', 4, 4, 5,
     nullptr},
    {"22M", "Yeji", "\xe9\x87\x8e\xe9\x9b\x9e", "pheasant cock", 'R', 1, 1, 5,
     nullptr},
    {"22L", "Junshi", "\xe8\xbb\x8d\xe5\xb8\x82", "soldiers' market", 'R', 13,
     11, 5, nullptr},
    {"21D", "Ping", "\xe5\xb1\x8f", "toilet screen", 'W', 2, 2, 2,
     "labelled but NOT AT ITS PLACE; should be S of Junjing"},
    {"21E", "Junjing", "\xe8\xbb\x8d\xe4\xba\x95", "soldiers' well", 'B', 4, 4,
     2, nullptr},
    {"21F", "Ce", "\xe5\x8e\x95", "toilet with a shed", 'W', 4, 4, 5, nullptr},
    {"22P", "Zhangren", "\xe4\xb8\x88\xe4\xba\xba", "husband man", 'B', 2, 2, 5,
     "should be more S of Ce and Junjing"},
    {"22O", "Zi", "\xe5\xad\x90", "son", 'W', 2, 2, 5,
     "should be more S of Ce and Junjing"},
};

// TABLE 5 — the circumpolar disc, the rows that carry a colour or a defect.
struct M13Row {
  const char *cid, *pinyin, *native;
  char col;
  int nSxc, nMap;
  const char* note;
};
const M13Row kMap13[] = {
    {"P22", "Tianchu", "\xe5\xa4\xa9\xe5\x8e\xa8", 'B', 5, 6, nullptr},
    {"P17", "Wudizuo", "\xe4\xba\x94\xe5\xb8\x9d\xe5\x86\x85\xe5\xba\xa7", 'B',
     5, 5, nullptr},
    {"P20", "Chuanshe", "\xe4\xbc\xa0\xe8\x88\x8d", 'B', 9, 7, nullptr},
    {"P12", "Tianzhu", "\xe5\xa4\xa9\xe6\x9f\xb1", 'B', 5, 5, nullptr},
    {"P15", "Liujia", "\xe5\x85\xad\xe7\x94\xb2", 'B', 6, 5, nullptr},
    {"P18", "Huagai", "\xe8\x8f\xaf\xe8\x93\x8b", 'B', 7, 7,
     "+6 UNACCOUNTED \xc2\xb7 \"is it Gang? the character is absent\""},
    {"P19", "Gang", "\xe6\x9d\xa0", 'B', 9, 0,
     "Chen Zhuo HAS it, appended to Huagai \xc2\xb7 9 vs the map's 6"},
    {"P14", "Gouchen", "\xe9\x92\xa9\xe9\x99\x88", 'R', 5, 6, nullptr},
    {"", "NI 1", "\xe2\x80\x94", 'R', 0, 1,
     "one star, NO CHARACTER, east of Gouchen"},
    {"P16", "Tianhuang", "\xe5\xa4\xa9\xe7\x9a\x87\xe5\xa4\xa7\xe5\xb8\x9d",
     'B', 1, 4, nullptr},
    {"P05", "Ziwei E wall", "\xe7\xb4\xab\xe5\xbe\xae\xe6\x9d\xb1\xe5\x9e\xa3",
     'M', 8, 8, "14 R + 1 B over both walls \xc2\xb7 8 + 7 = 15, closes"},
    {"P06", "Ziwei W wall", "\xe7\xb4\xab\xe5\xbe\xae\xe8\xa5\xbf\xe5\x9e\xa3",
     'M', 7, 7, nullptr},
    {"P10", "Zhuxiashi", "\xe6\x9f\xb1\xe5\x8f\xb2", 'R', 1, 1, nullptr},
    {"P09", "Nushi", "\xe5\xa5\xb3\xe5\x8f\xb2", 'R', 1, 1, nullptr},
    {"P24", "Tianpei", "\xe5\xa4\xa9\xe6\xa3\x93", 'M', 5, 5,
     "\"5 R, 1 B?\" \xe2\x80\x94 the SECOND mixed asterism"},
    {"P08", "Shangshu", "\xe5\xb0\x9a\xe4\xb9\xa6", 'B', 5, 5, nullptr},
    {"P01", "Beiji", "\xe5\x8c\x97\xe6\x9e\x81", 'R', 5, 4,
     "+3 black unlabelled \xc2\xb7 a RED NON-ENCIRCLED star, erased"},
    {"P02", "Sifu", "\xe5\x9b\x9b\xe8\xbe\x85", 'B', 4, 4, nullptr},
    {"P21", "Neijie", "\xe5\x86\x85\xe9\x98\xb6", 'B', 6, 6, nullptr},
    {"P23", "Bagu", "\xe5\x85\xab\xe8\xb0\xb7", 'B', 8, 8, nullptr},
    {"P25", "Tianchuang", "\xe5\xa4\xa9\xe5\xba\x8a", 'B', 6, 4, nullptr},
    {"P04", "Taiyi", "\xe5\xa4\xaa\xe4\xb8\x80", 'B', 1, 1, nullptr},
    {"P03", "Tianyi", "\xe5\xa4\xa9\xe4\xb8\x80", 'B', 1, 1, nullptr},
    {"P28", "Sangong (Wuxian)", "\xe4\xb8\x89\xe5\x85\xac", 'B', 3, 3,
     "Chen Zhuo files it under WU XIAN (white); the map draws it BLACK"},
    {"P34", "Sangong (Gan)", "\xe4\xb8\x89\xe5\x85\xac", 'B', 3, 3, nullptr},
    {"P39", "Tianqiang", "\xe5\xa4\xa9\xe6\x9e\xaa", 'R', 3, 3, nullptr},
    {"P37", "Beidou", "\xe5\x8c\x97\xe6\x96\x97", 'R', 8, 7, nullptr},
    {"P36", "Tianli", "\xe5\xa4\xa9\xe7\x90\x86", 'B', 4, 4, nullptr},
    {"P27", "Wenchang", "\xe6\x96\x87\xe6\x98\x8c", 'R', 6, 5, nullptr},
    {"P35", "Xuange", "\xe7\x8e\x84\xe6\x88\x88", 'R', 1, 1, nullptr},
    {"P33", "Xiang", "\xe7\x9b\xb8", 'R', 1, 1, nullptr},
    {"P31", "Taiyangshou", "\xe5\xa4\xaa\xe9\x98\xb3\xe5\xae\x88", 'R', 1, 1,
     nullptr},
    {"P32", "Shi", "\xe5\x8a\xbf", 'B', 4, 4, nullptr},
    {"P30", "Tianlao", "\xe5\xa4\xa9\xe7\x89\xa2", 'R', 6, 6, nullptr},
};

// the 28 mansions in order, and their determinative stars' HIP numbers as
// Stellarium's lunar_system.defining_stars gives them
const char* kXiu[28] = {
    "\xe8\xa7\x92", "\xe4\xba\xa2", "\xe6\xb0\x90", "\xe6\x88\xbf",
    "\xe5\xbf\x83", "\xe5\xb0\xbe", "\xe7\xae\x95", "\xe6\x96\x97",
    "\xe7\x89\x9b", "\xe5\xa5\xb3", "\xe8\x99\x9b", "\xe5\x8d\xb1",
    "\xe5\xae\xa4", "\xe5\xa3\x81", "\xe5\xa5\x8e", "\xe5\xa9\x81",
    "\xe8\x83\x83", "\xe6\x98\xb4", "\xe7\x95\xa2", "\xe8\xa7\x9c",
    "\xe5\x8f\x83", "\xe4\xba\x95", "\xe9\xac\xbc", "\xe6\x9f\xb3",
    "\xe6\x98\x9f", "\xe5\xbc\xb5", "\xe7\xbf\xbc", "\xe8\xbb\xb8"};
const char* kXiuPinyin[28] = {
    "Jiao", "Kang", "Di",  "Fang", "Xin",  "Wei",   "Ji",  "Dou", "Niu", "Nu",
    "Xu",   "Wei",  "Shi", "Bi",   "Kui",  "Lou",   "Wei", "Mao", "Bi",  "Zui",
    "Shen", "Jing", "Gui", "Liu",  "Xing", "Zhang", "Yi",  "Zhen"};
/** Distinct stars in an asterism. `AstRec::stars` is the VERTEX count and a
 *  Chen Zhuo polyline revisits stars (東井 is 14 vertices over 9 stars), so
 *  comparing it to Table 4's n(SXC) compares two different things. */
int astUnique(const AstRec& A) {
  int n = 0;
  uint16_t seen[64];
  for (int w = 0; w < A.words && n < 64; ++w) {
    const uint16_t v = kVerts[A.first + w];
    if (v == 0xFFFF) continue;
    bool dup = false;
    for (int j = 0; j < n; ++j) dup |= seen[j] == v;
    if (!dup) seen[n++] = v;
  }
  return n;
}

weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                             float tracking = 0) {
  return weave::textStyle(
      {.face = std::move(face), .size = size, .color = c, .track = tracking});
}

SkColor4f schoolInk(char c) {
  switch (c) {
    case 'R':
      return kCinnabar;
    case 'B':
      return kInk;
    case 'W':
      return kLead;
    default:
      return kInkFaint;
  }
}

/** THE ARCHER'S BRUSH PRESS, as a comparable px-keyed Profile.
 *
 *  `alongIsPx` is the whole reason this is not a fraction: every bone is
 *  drawn under `spans::upTo(gate(...))`, so the contour the decoration is
 *  handed grows as the figure draws. Keyed in px, `fullLen` is the length
 *  the law was AUTHORED against and the heavy head stays at the start of
 *  the bone instead of riding the reveal's leading edge.
 *
 *  max(): the law is 0.55 + 0.75·e^(-9t) + 0.35t on [0,1], monotone down
 *  then up, so its peak is at t=0 and equals w0·1.30 exactly. Reporting
 *  more than that only pads the stroke's claimed bounds. */
struct BonePress {
  float fullLen = 1.0f;
  float w0 = 1.0f;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    const float t = fullLen > 0 ? px / fullLen : 0.0f;
    return w0 * (0.55f + 0.75f * std::exp(-9.0f * t) + 0.35f * t);
  }
  float max() const { return w0 * 1.30f; }
  bool operator==(const BonePress&) const = default;
};

}  // namespace

// ===========================================================================

struct DunhuangStarChart : sketch::Sketch {
  sk_sp<SkTypeface> faceSerif, faceItalic, faceMono, faceDisplay, faceHan;

  // ONE Output writes the plate: the score position in seconds.
  ch::Output<float> scribe{0.0f};
  double clockT = 0;

  feed::TextRing logA{72}, logB{72}, logC{72};

  /** THE ONE DISCRETE STATE, AND IT IS A CACHING STATE. Every reveal on this
   *  plate is a window() on `scribe`, which is a BOUND property and therefore
   *  counts as live volatility for ever — even once its value has been 1.0
   *  for ten seconds. Past the score's end the same reveal is described as a
   *  plain constant instead, so the several hundred nodes that carry one fall
   *  back into their parents' recordings. The cost is the two render() calls
   *  per loop that flip the state, and they are cheap here because nothing
   *  below re-measures text. */
  bool settled = false;
  Animatable<float> gate(float t0, float t1) const {
    if (settled) return Animatable<float>(1.0f);
    return Animatable<float>(bind(&scribe).window(t0, t1));
  }
  /** A mark that COMES AND GOES: it ramps 0 → 1 across [t0, t1], then
   *  falls back to 0 by t2, so the mark is a visit rather than a residue.
   *  One binding carries the whole visit — the window normalises [t0, t2]
   *  onto [0,1] and the map folds that into a rise that peaks where t1
   *  lands and a fall that reaches zero at the window's end. Past the
   *  score, `settled` describes it as a plain 0 so the node drops out of
   *  the live set entirely. */
  Animatable<float> flash(float t0, float t1, float t2) const {
    if (settled) return Animatable<float>(0.0f);
    const float peak =
        std::min(0.999f, std::max(0.001f, (t1 - t0) / (t2 - t0)));
    return Animatable<float>(bind(&scribe)
                                 .window(t0, t2)
                                 .map([peak](float v) {
                                   return v < peak ? v / peak
                                                   : (1.0f - v) / (1.0f - peak);
                                 })
                                 .clamp(0, 1));
  }

  Paint paperGrain;
  Pattern paperSpeck;

  // --- the star field -----------------------------------------------------
  std::shared_ptr<instancing::Atlas> atlas;
  std::shared_ptr<instancing::Pool> pool;
  int cellRed = 0, cellBlack = 1, cellWhite = 2, cellOpen = 3, cellBare = 4;

  struct Placed {          // where each star lands once the fold completes
    SkPoint sky{0, 0};     // the equatorial plate carrée
    SkPoint paper{0, 0};   // the scroll
    bool onPaper = false;  // survived the DEC window and the visible segments
    int cell = 3;
    float raNow = 0, decNow = 0;
    float fold0 = 0;  // its own beat in the fold's stagger
  };
  std::vector<Placed> placed;
  float lastEpoch = 1e9f, lastFold = -1;

  // --- the joins, counted -------------------------------------------------
  int nStars = kStarCount, nAst = kAstCount;
  int nOnDisc = 0, nOnMaps = 0, nInGap = 0, nTooSouth = 0, nUnattested = 0;
  int nSchooled = 0;
  int m5ChenZhuo = 0, m5Sxc = 0, m5Map = 0;
  Departure depMerc{}, depStereo{};

  // asterism → school, resolved from Tables 4 and 5 by Chen Zhuo id
  std::vector<char> astSchool;
  std::vector<int> astMap;   // 5, 13, or 0
  std::vector<float> astRa;  // mean RA at +700, for the reveal stagger

  /** The asterism line art, built ONCE in setup(). describe() runs twice per
   *  loop (the settle flip and the loop reset), so 1,747 precessions and 317
   *  path builds inside it would land whole on those two frames. */
  struct AstArt {
    SkPath local;
    SkRect box;
    float t0;
    SkColor4f ink;
  };
  std::vector<AstArt> astArt;
  /** Map 5's twenty centroids and the 28 determinative RAs, resolved ONCE in
   *  setup(). Both are O(asterisms x vertices), so leaving them inside
   *  describe() would put that whole walk on each of the two re-describes. */
  std::array<SkPoint, 20> m5Cent{};
  std::array<int, 20> m5Region{};
  std::array<float, 28> xiuRa{};

  // =========================================================================
  // GEOMETRY

  /** The equatorial plate carrée the stars arrive in — RA increasing to the
   *  LEFT, exactly as on the scroll, so the fold is a fold and not a flip. */
  static SkPoint skyPoint(float ra, float dec) {
    const float u = wrap360(ra - 296.0f) / 360.0f;
    return {2452.0f - u * 2344.0f, 632.0f - dec * (382.0f / 90.0f)};
  }

  /** Scroll coordinate s (mm, from the atlas's right edge, growing leftward)
   *  to canvas x — through whichever of the two segments holds it. Returns
   *  false when s falls in the omitted stretch or off the plate. */
  static bool scrollX(float s, float& x) {
    const float xr = kOriginR - s * kPxMm;
    if (xr >= kBreakR - 2.0f && xr <= kW + 90.0f) {
      x = xr;
      return true;
    }
    const float xl = kOriginL - s * kPxMm;
    if (xl >= -90.0f && xl <= kBreakL + 2.0f) {
      x = xl;
      return true;
    }
    return false;
  }
  // the two scroll segments, each its own clipping window: a map cut by the
  // drafting break must be drawn CUT, not skipped
  static float segLo(int seg) { return seg == 0 ? -92.0f : kBreakR; }
  static float segHi(int seg) { return seg == 0 ? kBreakL : kW + 92.0f; }
  static float segX(int seg, float s) {
    return (seg == 0 ? kOriginL : kOriginR) - s * kPxMm;
  }

  /** A star at (ra, dec) of epoch +700 → the paper. Twelve cylindrical maps
   *  for |dec| ≤ 45, the azimuthal disc for dec ≥ +52. Between them is the
   *  chart's own uncovered band, and it is real. */
  static bool paperPoint(float ra, float dec, SkPoint& out, int& region) {
    if (dec >= 52.0f) {  // map 13 — azimuthal
      const float rmm = (kDiscCenDec - dec) / kPolPerMm;
      const float a = (kAzGain * wrap180(ra - 278.0f)) * kD;
      float xc;
      if (!scrollX(discCentreS(), xc)) return false;
      out = {xc + rmm * kPxMm * std::cos(a),
             kBandMid + rmm * kPxMm * std::sin(a)};
      region = 13;
      return true;
    }
    if (dec > 45.0f || dec < -45.0f) {
      region = 0;
      return false;
    }
    const int k = mapOfRa(ra);
    const float dRa = wrap180(ra - mapCentre(k));
    if (std::abs(dRa) > 24.0f) {
      region = 0;
      return false;
    }
    const float s = mapSlotS(k) + kMapWmm * 0.5f + dRa / kRaPerMm;
    float x;
    if (!scrollX(s, x)) {
      region = k;
      return false;
    }
    const float decTop = mapGcDec(k) + 45.0f;
    out = {x, kFrameTop + (decTop - dec) / kDecPerMm * kPxMm};
    region = k;
    return true;
  }

  /** The disc's own frame, for the ornament and the mansion spokes. */
  static bool discFrame(SkPoint& c, float& rOuter) {
    float xc;
    if (!scrollX(discCentreS(), xc)) return false;
    c = {xc, kBandMid};
    rOuter = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
    return true;
  }

  // =========================================================================
  // THE POOL — rebuilt only when the epoch or the fold actually moved.

  void rebuild(float epoch, float fold) {
    if (std::abs(epoch - lastEpoch) < 1e-3f &&
        std::abs(fold - lastFold) < 1e-4f)
      return;
    lastEpoch = epoch;
    lastFold = fold;
    const Mat3 M = precMatrix((epoch - 2000.0f) * 0.01f);
    auto pos = pool->positions();
    auto tint = pool->tints();
    for (int i = 0; i < nStars; ++i) {
      Placed& p = placed[(size_t)i];
      precess(M, starRa(i), starDec(i), p.raNow, p.decNow);
      p.sky = skyPoint(p.raNow, p.decNow);
      int region = 0;
      SkPoint paper;
      p.onPaper = paperPoint(p.raNow, p.decNow, paper, region);
      p.paper = p.onPaper ? paper : p.sky;
      const float f = smooth((fold - p.fold0) / 0.42f);
      pos[(size_t)i] = {p.sky.fX + (p.paper.fX - p.sky.fX) * f,
                        p.sky.fY + (p.paper.fY - p.sky.fY) * f};
      SkColor4f t{1, 1, 1, 1};
      // The sky the chart does NOT carry — the +45..+52 band between the
      // cylindrical maps and the disc, everything south of -45 — leaves the
      // plate entirely rather than lingering as ghosts ON the paper, where it
      // reads as dots. The count is in the feed instead.
      if (!p.onPaper) t.fA = 1.0f - f;
      tint[(size_t)i] = t;
    }
    pool->commit();
  }

  // =========================================================================
  // BUILDING

  void computeJoins() {
    placed.assign((size_t)nStars, Placed{});
    astSchool.assign((size_t)nAst, ' ');
    astMap.assign((size_t)nAst, 0);
    astRa.assign((size_t)nAst, 0.0f);

    for (const M5Row& r : kMap5) {
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) {
          astSchool[(size_t)a] = r.col;
          astMap[(size_t)a] = 5;
        }
      m5Sxc += r.nSxc;
      m5Map += r.nMap;
    }
    for (const auto& r : kMap13) {
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) {
          astSchool[(size_t)a] = r.col;
          astMap[(size_t)a] = 13;
        }
    }
    // 伐 Fa, the dagger: "the three hazy red stars ... (no specific label)".
    // Red, NOT encircled, and never named — 'H' is that state, not a school.
    for (int a = 0; a < nAst; ++a)
      if (std::string(kAst[a].id) == "21B") {
        astSchool[(size_t)a] = 'H';
        astMap[(size_t)a] = 5;
      }

    // per-star school: from its asterism where the paper gives one
    std::vector<char> starSchool((size_t)nStars, ' ');
    const Mat3 M = precMatrix(-13.0f);
    for (int a = 0; a < nAst; ++a) {
      const AstRec& A = kAst[a];
      double sx = 0, sy = 0;
      int n = 0;
      for (int w = 0; w < A.words; ++w) {
        const uint16_t v = kVerts[A.first + w];
        if (v == kVSep) continue;
        if (astSchool[(size_t)a] != ' ')
          starSchool[v] =
              astSchool[(size_t)a] == 'M' ? 'R' : astSchool[(size_t)a];
        float ra, dec;
        precess(M, starRa(v), starDec(v), ra, dec);
        sx += std::cos(ra * kD);
        sy += std::sin(ra * kD);
        ++n;
      }
      astRa[(size_t)a] = n ? wrap360((float)std::atan2(sy, sx) / kD) : 0.0f;
      if (astMap[(size_t)a] == 5) m5ChenZhuo += astUnique(A);
    }

    for (int i = 0; i < nStars; ++i) {
      Placed& p = placed[(size_t)i];
      float ra, dec;
      precess(M, starRa(i), starDec(i), ra, dec);
      SkPoint dummy;
      int region = 0;
      const bool on = paperPoint(ra, dec, dummy, region);
      (void)on;
      if (dec >= 52.0f)
        ++nOnDisc;
      else if (dec > 45.0f)
        ++nInGap;
      else if (dec < -45.0f)
        ++nTooSouth;
      else
        ++nOnMaps;
      const char sc = starSchool[(size_t)i];
      if (sc == 'H') {
        p.cell = cellBare;
        ++nSchooled;
      }  // Fa, hazy and bare
      else if (sc == ' ') {
        p.cell = cellOpen;
        ++nUnattested;
      } else {
        ++nSchooled;
        p.cell = sc == 'R' ? cellRed : sc == 'B' ? cellBlack : cellWhite;
      }
      // the fold sweeps right-to-left, in reading order: by RA
      p.fold0 = 0.56f * (wrap360(ra - 296.0f) / 360.0f);
    }

    depMerc = mercatorVsLinear(-27.0f, 43.0f, 1.61f, 5.28f, 0.996f, 15);
    depStereo = stereoVsEquidistant(0.0f, 38.0f, 3.29f, 5.10f, 0.919f, 19);
  }

  // --- the paper ----------------------------------------------------------

  Element ground() {
    auto g = box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("ground");
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(kW))
                .height(Dim(kH))
                .fill(Paint::linear({0, 0}, {kW, kH},
                                       {{0.0f, hex(0x171410)},
                                        {0.5f, hex(0x1d1913)},
                                        {1.0f, hex(0x120f0c)}}))
                .cache(Cache::Texture));
    return g;
  }

  /** The scroll band: the mulberry sheet, the Kraft lining showing at the
   *  edges, the roll's contact replication marks, and two rules of UNEQUAL
   *  weight along the top and bottom — a scroll's edges are not a rect. */
  Element scrollBand(float x0, float x1, const char* keyName, float tilt) {
    const float w = x1 - x0;
    // ASK FOR THE BAKE BY NAME. The band carries a paper gradient, an
    // anisotropic grain and a speckle wash over its whole area, and it is
    // ROTATED (a scroll does not lie square) — which disqualifies it from
    // the library's automatic device-space promotion, since a bake pinned to
    // one device rect is not matrix-independent. Without this explicit cache
    // the three shaders re-run over every pixel of both bands on every frame,
    // although nothing here ever moves. The rule generalises: a node is
    // refused the automatic bake for a decorative half-degree tilt exactly as
    // it is for animation, so any rotated node with area must be baked by
    // name.
    auto g = box()
                 .left(x0)
                 .top(kBandTop)
                 .width(Dim(w))
                 .height(Dim(kBandH))
                 .rotate(tilt)
                 .key(keyName)
                 .cache(Cache::Texture)
                 .opacity(gate(tPaper, tPaper + 1.0f));

    // the sheet itself, with the fibre running along the roll
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(kBandH))
                .fill(Paint::linear({0, 0}, {0, kBandH},
                                       {{0.00f, kKraft},
                                        {0.055f, kPaperDeep},
                                        {0.30f, kPaperMid},
                                        {0.58f, kPaperLit},
                                        {0.88f, kPaperMid},
                                        {0.945f, kPaperDeep},
                                        {1.00f, kKraft}}))
                .cache(Cache::Texture));

    // the fibre — anisotropic grain, luminance not hue, under a bake
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(kBandH))
                .fill(paperGrain)
                .opacity(0.20f)
                .blend(SkBlendMode::kSoftLight)
                .cache(Cache::Texture));
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(kBandH))
                .foreground(Wash{.material = paperSpeck.material(),
                                 .blend = SkBlendMode::kMultiply,
                                 .amount = 0.55f})
                .cache(Cache::Texture));

    // "replication marks by contact due to long conservation in a rolled
    // state" — the ghost of the adjacent turn, one circumference over,
    // taking the sheet's 244 mm width as the roll's diameter
    const float circ = 244.0f * 3.14159f * kPxMm;  // ~77 cm of scroll
    for (int k = -3; k <= 3; ++k) {
      const float gx = std::fmod(std::abs(x0) + (float)k * circ, w);
      g.child(box()
                  .left(gx)
                  .top(6)
                  .width(Dim(3.0f))
                  .height(Dim(kBandH - 12))
                  .fill(Fill::color(hex(0x8b6e45, 0.10f))));
    }

    // top and bottom rules — unequal, per lines::Rails
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(w))
            .height(Dim(kBandH))
            .shape([](SkSize s) {
              SkPathBuilder b;
              b.moveTo(0, 1.5f);
              b.lineTo(s.width(), 1.5f);
              b.moveTo(0, s.height() - 1.5f);
              b.lineTo(s.width(), s.height() - 1.5f);
              return b.detach();
            })
            .stroke(
                Brush{}
                    .shaped(shapers::Jitter{
                        .segLength = 34, .deviation = 0.9f, .seed = 3326})
                    .layer(lines::Rails{
                        .rails = {{.across = 0,
                                   .width = 1.9f,
                                   .fill = Fill::color(hex(0x6b573c, 0.62f))},
                                  {.across = -4.5f,
                                   .width = 0.55f,
                                   .fill = Fill::color(hex(0x6b573c, 0.34f)),
                                   .dash = {9, 6}}}})));
    return g;
  }

  // --- the map furniture --------------------------------------------------

  /** One hour-angle map: its ruled frame, its RA tick ladder (whose numbers
   *  JUMP BACK 18° at every boundary — the contradiction, drawn), its
   *  equator, and the interrupted mansion rules that fall inside it. */
  Element mapFrame(int k, int seg) {
    const float s0 = mapSlotS(k);
    const float xl = segX(seg, s0 + kMapWmm), xr = segX(seg, s0);
    if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4)
      return box().width(0).height(0);
    const float w = xr - xl;
    if (w <= 2) return box().width(0).height(0);
    auto g = box()
                 .left(xl - segLo(seg))
                 .top(kFrameTop - kSegTop)
                 .width(Dim(w))
                 .height(Dim(kFrameH))
                 .key(fmt("map%d_%d", k, seg))
                 .opacity(gate(tFold0 - 0.4f, tFold0 + 1.1f));

    // THE APPARATUS SITS UNDER THE INK. The paper has no frames at all —
    // the twelve maps abut and are separated only by the text columns —
    // so the frame, its corner brackets, the RA ladder and its numerals
    // are this study's own scaffolding, carrying its argument about the
    // 18° jump at every boundary. Scaffolding drawn at the weight of the
    // brush competes with the thing it is scaffolding: every one of them
    // is lighter than the faintest star mark on the map it encloses.
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(kFrameH))
                .stroke(Brush{}
                            .shaped(shapers::Jitter{
                                .segLength = 30,
                                .deviation = 1.1f,
                                .seed = (uint32_t)(600 + k)})
                            .layer(lines::Line{
                                .width = 1.0f,
                                .fill = Fill::color(hex(0x8a7458, 0.42f))}))
                .stroke(spans::corners(15.0f),
                        brush::solid(1.4f, Fill::color(hex(0x6b573c, 0.52f)))));

    // the equator — the one line whose position the paper says varies ±5°
    const float yEq = (mapGcDec(k) + 45.0f) / kDecPerMm * kPxMm;
    g.child(
        box()
            .left(0)
            .top(yEq - 1)
            .width(Dim(w))
            .height(Dim(2))
            .shape([](SkSize s) {
              SkPathBuilder b;
              b.moveTo(0, 1);
              b.lineTo(s.width(), 1);
              return b.detach();
            })
            .stroke(PathFormat{.width = 0.75f,
                               .strokeFill = Fill::color(hex(0x8a3020, 0.42f)),
                               .dashIntervals = {5, 5},
                               .trimStart = 0.04f,
                               .trimEnd = 0.96f}));

    // the RA ladder: a tick every 6°, numbered every 12°
    for (int t = -24; t <= 24; t += 6) {
      const float x = w * 0.5f + (float)t / kRaPerMm * kPxMm;
      if (x < 1 || x > w - 1) continue;
      const bool big = (t % 12) == 0;
      g.child(box()
                  .left(x - 0.4f)
                  .top(0)
                  .width(Dim(0.9f))
                  .height(Dim(big ? 8.0f : 4.5f))
                  .fill(Fill::color(hex(0x8a7458, 0.40f))));
      g.child(box()
                  .left(x - 0.4f)
                  .top(kFrameH - (big ? 8.0f : 4.5f))
                  .width(Dim(0.9f))
                  .height(Dim(big ? 8.0f : 4.5f))
                  .fill(Fill::color(hex(0x8a7458, 0.40f))));
      if (big)
        g.child(text(toU8(fmt("%d", (int)std::lround(
                                        wrap360(mapCentre(k) + (float)t)))),
                     type(faceMono, 7.4f, hex(0x8a7458, 0.60f)))
                    .left(x - 11)
                    .top(kFrameH + 3)
                    .width(Dim(24))
                    .textAlign(weave::TextAlignment::kCenter));
    }

    // the map's own number and month, in the margin above
    g.child(
        text(toU8(fmt("%d", k)), type(faceDisplay, 13.0f, hex(0x4a3b28, 0.82f)))
            .left(w - 20)
            .top(-19)
            .width(Dim(18))
            .textAlign(weave::TextAlignment::kEnd));

    // THE MANSION BOUNDARIES, ruled where the DETERMINATIVE STARS put them
    // and not at 12.86° apiece — Stellarium's lunar_system.defining_stars,
    // precessed to +700 like everything else. Interrupted rules: they stop
    // short of both edges of the frame.
    for (int m = 0; m < 28; ++m) {
      const float dRa = wrap180(xiuRa[(size_t)m] - mapCentre(k));
      if (std::abs(dRa) > 23.0f) continue;
      const float x = w * 0.5f + dRa / kRaPerMm * kPxMm;
      g.child(
          box()
              .left(x - 6)
              .top(0)
              .width(Dim(12))
              .height(Dim(kFrameH))
              .shape([](SkSize sz) {
                SkPathBuilder b;
                b.moveTo(sz.width() * 0.5f, 0);
                b.lineTo(sz.width() * 0.5f, sz.height());
                return b.detach();
              })
              .stroke(PathFormat{.width = 0.7f,
                                 .strokeFill = Fill::color(hex(0x4a3b28, 0.5f)),
                                 .dashIntervals = {7, 5},
                                 .trimStart = 0.06f,
                                 .trimEnd = 0.94f}));
      g.child(text(toU8(kXiu[m]), type(faceHan ? faceHan : faceSerif, 11.5f,
                                       hex(0x2a2118, 0.88f)))
                  .left(x - 9)
                  .top(-32)
                  .width(Dim(18))
                  .textAlign(weave::TextAlignment::kCenter));
      g.child(
          text(toU8(kXiuPinyin[m]), type(faceMono, 7.0f, hex(0x5d4c37, 0.75f)))
              .left(x - 20)
              .top(-45)
              .width(Dim(40))
              .textAlign(weave::TextAlignment::kCenter));
    }
    return g;
  }

  /** The 50 columns of text, drawn as columns of marks rather than a grid.
   *  Real Tang manuscript columns are UNRULED — the discipline is in the
   *  hand — so what is drawn is the drift, not a lattice. */
  Element columnBand(int k, int seg) {
    const float s0 = mapSlotS(k) + kMapWmm;
    const float xl = segX(seg, s0 + kColBandMm), xr = segX(seg, s0);
    if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4)
      return box().width(0).height(0);
    const float w = xr - xl;
    if (w <= 2) return box().width(0).height(0);
    auto g = box()
                 .left(xl - segLo(seg))
                 .top(kFrameTop + 8 - kSegTop)
                 .width(Dim(w))
                 .height(Dim(kFrameH - 16))
                 .key(fmt("cols%d_%d", k, seg))
                 .opacity(gate(tFold0 + 0.2f, tFold0 + 1.5f));
    const int nCols = (int)std::round(kColBandMm / kColMm);
    for (int c = 0; c < nCols; ++c) {
      const float u = (float)c / (float)std::max(1, nCols - 1);
      const float drift = std::sin((float)(k * 7 + c) * 1.31f) * 2.4f;
      const float cx = w - 4.0f - u * (w - 10.0f) + drift;
      const int glyphs = 20 + ((k * 5 + c * 3) % 11);
      // The columns are drawn as MARKS, not as characters: a Tang column
      // at this scale is a stack of squarish brush shapes, and the shape
      // is what the eye reads at plate size. Setting the atlas's own
      // asterism names here as vertical runs was tried and the runs do
      // not reach the plate, which is a question about vertical text
      // inside an absolutely-placed leaf rather than about this file.
      for (int gI = 0; gI < glyphs; ++gI) {
        const float gy =
            5.0f + (float)gI * 9.1f + std::sin((float)(c * 13 + gI * 5)) * 0.8f;
        if (gy > kFrameH - 22) break;
        g.child(box()
                    .left(cx - 3.9f)
                    .top(gy)
                    .width(Dim(7.8f))
                    .height(Dim(6.2f))
                    .fill(Fill::color(hex(0x241d15, 0.80f)))
                    .shape(shapes::blob((uint32_t)(k * 97 + c * 13 + gI), 0.42f,
                                        7)));
      }
    }
    return g;
  }

  // --- the circumpolar disc -----------------------------------------------

  Element discPlate(int seg) {
    const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
    const SkPoint c{segX(seg, discCentreS()), kBandMid};
    if (c.fX + rOut < segLo(seg) - 4 || c.fX - rOut > segHi(seg) + 4)
      return box().width(0).height(0);
    const float d = rOut * 2.0f;
    auto g = box()
                 .left(c.fX - rOut - segLo(seg))
                 .top(c.fY - rOut - kSegTop)
                 .width(Dim(d))
                 .height(Dim(d))
                 .key(fmt("disc%d", seg))
                 .opacity(gate(tFold0 - 0.2f, tFold0 + 1.4f));

    // the limb: a heavy outer rule and a hairline inner one
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(d))
            .height(Dim(d))
            .shape(shapes::circle())
            .stroke(
                Brush{}
                    .shaped(shapers::Jitter{
                        .segLength = 22, .deviation = 1.0f, .seed = 1300})
                    .layer(brush::presets::heavyHairHeavy(
                        1.7f, 0.5f, Fill::color(hex(0x3a2e1e, 0.86f)), 5.0f))));

    // the DEC rings, at the published 5.10 °/cm — parametric, not stamped
    for (int dec = 60; dec <= 85; dec += 5) {
      const float rr = (kDiscCenDec - (float)dec) / kPolPerMm * kPxMm;
      if (rr <= 3 || rr >= rOut - 1) continue;
      g.child(box()
                  .left(rOut - rr)
                  .top(rOut - rr)
                  .width(Dim(rr * 2))
                  .height(Dim(rr * 2))
                  .shape(shapes::circle())
                  .stroke(PathFormat{
                      .width = 0.5f,
                      .strokeFill = Fill::color(hex(0x5d4c37, 0.30f)),
                      .dashIntervals = {3, 5}}));
    }

    // the 28 mansion spokes — INTERRUPTED rules, stopping short of both the
    // limb and the pole, ruled at the determinative stars' own +700 RA and
    // NOT at 12.86° apiece
    for (int m = 0; m < 28; ++m) {
      const float ang = kAzGain * wrap180(xiuRa[(size_t)m] - 278.0f);
      g.child(box()
                  .left(0)
                  .top(0)
                  .width(Dim(d))
                  .height(Dim(d))
                  .shape([ang, rOut](SkSize) {
                    SkPathBuilder b;
                    const float a = ang * kD;
                    b.moveTo(rOut + std::cos(a) * rOut * 0.14f,
                             rOut + std::sin(a) * rOut * 0.14f);
                    b.lineTo(rOut + std::cos(a) * rOut,
                             rOut + std::sin(a) * rOut);
                    return b.detach();
                  })
                  .stroke(PathFormat{
                      .width = 0.62f,
                      .strokeFill = Fill::color(hex(0x4a3b28, 0.44f)),
                      .trimStart = 0.05f,
                      .trimEnd = 0.90f}));
      {
        const float a = ang * kD;
        const float lx = rOut + std::cos(a) * (rOut - 16.0f);
        const float ly = rOut + std::sin(a) * (rOut - 16.0f);
        g.child(text(toU8(kXiu[m]), type(faceHan ? faceHan : faceSerif, 11.0f,
                                         hex(0x2a2118, 0.85f)))
                    .left(lx - 8)
                    .top(ly - 8)
                    .width(Dim(16))
                    .textAlign(weave::TextAlignment::kCenter));
      }
    }

    // the DISC'S CENTRE and the TRUE POLE are not the same point: Table 3
    // puts the centre at DEC +87.6°, so the +700 pole sits 4.7 mm away.
    const float poleOff = (90.0f - kDiscCenDec) / kPolPerMm * kPxMm;
    g.child(box()
                .left(rOut - 5)
                .top(rOut - 5)
                .width(Dim(10))
                .height(Dim(10))
                .shape(shapes::circle())
                .stroke(PathFormat{
                    .width = 0.7f,
                    .strokeFill = Fill::color(hex(0x4a3b28, 0.7f))}));
    g.child(box()
                .left(rOut - 3.2f)
                .top(rOut - poleOff - 3.2f)
                .width(Dim(6.4f))
                .height(Dim(6.4f))
                .shape(shapes::star(4, 0.34f))
                .fill(Fill::color(kTrace))
                .opacity(gate(tProj - 1.4f, tProj - 0.4f)));
    // "a RED NON-ENCIRCLED star, slightly erased, could be the Pole star"
    // (Table 5, row 15, Beiji). Drawn erased, drawn unnamed, not resolved.
    g.child(box()
                .left(rOut - poleOff * 0.45f - 4.0f)
                .top(rOut - poleOff * 0.7f - 4.0f)
                .width(Dim(8))
                .height(Dim(8))
                .shape(shapes::circle())
                .fill(Fill::color(hex(0xa8382a, 0.34f)))
                .opacity(gate(tFold1, tFold1 + 0.8f)));
    return g;
  }

  /** The disc's captions, on the paper below it — a note written beside the
   *  drawing, which is where a note goes. */
  Element discNotes(int seg) {
    const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
    const float cx = segX(seg, discCentreS()) - segLo(seg);
    if (cx + rOut < -4 || cx - rOut > segHi(seg) - segLo(seg) + 4)
      return box().width(0).height(0);
    auto g = box()
                 .left(cx - rOut - 8)
                 .top(kBandMid + rOut - kSegTop + 16)
                 .width(Dim(rOut * 2 + 16))
                 .key(fmt("discnote%d", seg))
                 .opacity(gate(tProj - 1.2f, tProj - 0.3f));
    const std::string rows[4] = {
        "MAP 13 \xc2\xb7 azimuthal, RA at 1.05\xc2\xb0/cm of circumference,",
        "DEC radial at 5.10\xc2\xb0/cm, +90\xc2\xb0 to +52\xc2\xb0 (Table 3).",
        fmt("disc CENTRE is DEC +87.6\xc2\xb0, NOT the pole: the +700 pole"),
        fmt("falls %.1f mm away, marked. and a red UNENCIRCLED star,",
            (90.0f - kDiscCenDec) / kPolPerMm),
    };
    for (int i = 0; i < 4; ++i)
      g.child(text(toU8(rows[(size_t)i]),
                   type(faceMono, 8.0f,
                        i >= 2 ? hex(0x8a3020, 0.95f) : hex(0x4a3b28, 0.9f)))
                  .left(0)
                  .top((float)i * 10.4f)
                  .width(Dim(rOut * 2 + 16)));
    g.child(text(toU8("slightly erased, sits near it \xe2\x80\x94 \"could be "
                      "the Pole "
                      "star\". Drawn as found."),
                 type(faceMono, 8.0f, hex(0x8a3020, 0.95f)))
                .left(0)
                .top(41.6f)
                .width(Dim(rOut * 2 + 16)));
    return g;
  }

  // --- the asterism line art ----------------------------------------------

  /** One asterism, as line art: a polyline through real dot centres, bowed
   *  by shapers::Jitter because a hand-drawn join is not a rule, revealed by
   *  trim() on its own beat. */
  void buildAsterismArt() {
    astArt.clear();
    const Mat3 M = precMatrix(-13.0f);
    for (int a = 0; a < nAst; ++a) {
      const AstRec& A = kAst[a];
      SkPathBuilder pb;
      // Bounds accumulated by hand, NOT with SkRect::join — join EARLY-OUTS
      // on an empty rect, and a single point IS an empty rect, so growing a
      // box one point at a time leaves it inverted and every asterism draws
      // nothing.
      float bl = 1e9f, bt = 1e9f, br = -1e9f, bb = -1e9f;
      bool open = false;
      int pts = 0;
      SkPoint prev{0, 0};
      int prevRegion = -1;
      bool havePrev = false;
      for (int w = 0; w < A.words; ++w) {
        const uint16_t v = kVerts[A.first + w];
        if (v == kVSep) {
          open = false;
          havePrev = false;
          prevRegion = -1;
          continue;
        }
        float ra, dec;
        precess(M, starRa(v), starDec(v), ra, dec);
        SkPoint p;
        int region = 0;
        if (!paperPoint(ra, dec, p, region)) {
          open = false;
          havePrev = false;
          prevRegion = -1;
          continue;
        }
        if (p.fX < -70 || p.fX > kW + 70) {
          open = false;
          havePrev = false;
          prevRegion = -1;
          continue;
        }
        // an asterism whose stars fall on DIFFERENT maps is drawn BROKEN —
        // the scroll's RA axis is discontinuous at every map boundary, so a
        // join across one is a line that does not exist on the paper
        if (havePrev &&
            (region != prevRegion || SkPoint::Distance(prev, p) > 230.0f))
          open = false;
        prevRegion = region;
        if (!open) {
          pb.moveTo(p);
          open = true;
        } else
          pb.lineTo(p);
        bl = std::min(bl, p.fX);
        bt = std::min(bt, p.fY);
        br = std::max(br, p.fX);
        bb = std::max(bb, p.fY);
        prev = p;
        havePrev = true;
        ++pts;
      }
      if (pts < 2) continue;
      SkPath path = pb.detach();
      const SkRect bounds = SkRect::MakeLTRB(bl - 9, bt - 9, br + 9, bb + 9);
      SkPathBuilder shift;
      shift.addPath(path, -bounds.left(), -bounds.top());
      const float u = wrap360(astRa[(size_t)a] - 296.0f) / 360.0f;
      astArt.push_back({shift.detach(), bounds,
                        tLine0 + u * (tLine1 - tLine0 - 0.9f),
                        astSchool[(size_t)a] == 'W' ? hex(0x6a5a3f, 0.80f)
                                                    : hex(0x24201a, 0.86f)});
    }
  }

  Element asterismLines() {
    auto g =
        box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("asterisms");
    for (size_t i = 0; i < astArt.size(); ++i) {
      const AstArt& A = astArt[i];
      // the node's box is the ASTERISM's box, never the plate's
      g.child(box()
                  .left(A.box.left())
                  .top(A.box.top())
                  .width(Dim(A.box.width()))
                  .height(Dim(A.box.height()))
                  .shape([p = A.local](SkSize) { return p; })
                  .stroke(spans::upTo(gate(A.t0, A.t0 + 0.9f)),
                          Brush{}
                              .shaped(shapers::Jitter{
                                  .segLength = 13.0f,
                                  .deviation = 0.85f,
                                  .seed = (uint32_t)(i * 31 + 7)})
                              .layer(lines::Line{.width = 1.05f,
                                                 .fill = Fill::color(A.ink),
                                                 .capSize = 0.0f})));
    }
    return g;
  }

  /** Where an asterism's stars land on the paper, and its own box. */
  bool astCentroid(const char* cid, SkPoint& out, int& region) const {
    for (int a = 0; a < nAst; ++a) {
      if (std::string(kAst[a].id) != cid) continue;
      const Mat3 M = precMatrix(-13.0f);
      double sx = 0, sy = 0;
      int n = 0, reg = 0;
      for (int w = 0; w < kAst[a].words; ++w) {
        const uint16_t v = kVerts[kAst[a].first + w];
        if (v == kVSep) continue;
        float ra, dec;
        precess(M, starRa(v), starDec(v), ra, dec);
        SkPoint p;
        if (!paperPoint(ra, dec, p, reg)) continue;
        sx += p.fX;
        sy += p.fY;
        ++n;
      }
      if (!n) return false;
      out = {(float)(sx / n), (float)(sy / n)};
      region = reg;
      return true;
    }
    return false;
  }

  /** MAP 5'S TWENTY, LABELLED AS THE SCRIBE LABELLED THEM. Two labels are
   *  interchanged, two asterisms carry none at all, and one sits where it
   *  does not belong — all six defects drawn, none corrected, each flagged.
   *  The flags are the audit's, not the chart's: the chart just has them. */
  void buildFixtures() {
    const Mat3 M = precMatrix(-13.0f);
    for (int m = 0; m < 28; ++m) {
      float ra = 0, dec = 0;
      precess(M, starRa(kXiuIndex[m]), starDec(kXiuIndex[m]), ra, dec);
      xiuRa[(size_t)m] = ra;
    }
    for (int i = 0; i < 20; ++i) {
      m5Region[(size_t)i] = 0;
      m5Cent[(size_t)i] = {0, 0};
      astCentroid(kMap5[(size_t)i].cid, m5Cent[(size_t)i], m5Region[(size_t)i]);
    }
  }

  Element map5Labels() {
    auto g = box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("m5lab");
    for (int i = 0; i < 20; ++i) {
      const M5Row& r = kMap5[(size_t)i];
      const SkPoint c = m5Cent[(size_t)i];
      const int region = m5Region[(size_t)i];
      if (region != 5) continue;
      const float fl = segX(1, mapSlotS(5) + kMapWmm),
                  fr = segX(1, mapSlotS(5));
      if (c.fX < fl - 2 || c.fX > fr + 2) continue;
      if (c.fY < kFrameTop || c.fY > kFrameTop + kFrameH) continue;
      const float t = tAudit + (float)i * tAuditEach;

      // the checking panel's own ring, raised on this row's beat. A clean row
      // takes flash() — up on its beat and faded back out three seconds
      // later — while a defect row takes gate() and stays up for the rest of
      // the running score, so from mid-audit onward the plate carries
      // exactly the six documented defects.
      g.child(box()
                  .left(c.fX - 30)
                  .top(c.fY - 30)
                  .width(Dim(60))
                  .height(Dim(60))
                  .shape(shapes::circle())
                  .opacity(r.defect ? gate(t, t + 0.3f)
                                    : flash(t, t + 0.3f, t + 3.0f))
                  .stroke(PathFormat{
                      .width = 1.1f,
                      .strokeFill = Fill::color(r.defect ? hex(0xb4531f, 0.85f)
                                                         : hex(0x2f6d86, 0.7f)),
                      .dashIntervals = {4, 4}}));

      // WHAT IS WRITTEN ON THE PAPER, defects included
      const char* written = r.native;
      float lx = c.fX + 6, ly = c.fY - 30;
      bool none = false;
      const std::string cid = r.cid;
      if (cid == "21A" || cid == "19P") none = true;  // Shen, Jiuliu
      if (cid == "22I") written = kMap5[9].native;    // Shuifu <- Sidu
      if (cid == "22K") written = kMap5[8].native;    // Sidu <- Shuifu
      if (cid == "21D") {
        ly = c.fY - 96;
        lx = c.fX + 26;
      }  // Ping, misplaced
      if (!none)
        g.child(text(toU8(written), type(faceHan ? faceHan : faceSerif, 12.5f,
                                         hex(0x241d15, 0.92f)))
                    .left(lx)
                    .top(ly)
                    .width(Dim(60))
                    .opacity(gate(tLine1 - 0.6f, tLine1 + 0.5f)));
      else
        g.child(
            text(toU8("[no label]"), type(faceMono, 8.2f, hex(0xb4531f, 0.9f)))
                .left(c.fX + 22)
                .top(c.fY - 40)
                .width(Dim(70))
                .opacity(gate(t, t + 0.3f)));
      if (cid == "21D")  // the leader from the misplaced label to its stars
        g.child(
            box()
                .left(std::min(lx, c.fX))
                .top(ly + 10)
                .width(Dim(std::abs(lx - c.fX) + 4))
                .height(Dim(c.fY - ly - 10))
                .shape([](SkSize sz) {
                  SkPathBuilder b;
                  b.moveTo(sz.width(), 0);
                  b.lineTo(0, sz.height());
                  return b.detach();
                })
                .opacity(gate(t, t + 0.3f))
                .stroke(lines::Line{.width = 0.7f,
                                    .fill = Fill::color(hex(0xb4531f, 0.8f)),
                                    .startCap = lines::Cap::Dot,
                                    .capSize = 4.0f}));
    }
    return g;
  }

  // --- the archer, and the title nobody can read --------------------------

  Element archer() {
    const float x = segX(0, 2205.0f) - segLo(0);
    const float w = 210.0f, h = 300.0f;
    auto g = box()
                 .left(x - w * 0.5f)
                 .top(kBandMid - h * 0.52f - kSegTop)
                 .width(Dim(w))
                 .height(Dim(h))
                 .key("archer")
                 .opacity(gate(tArch, tArch + 1.1f));

    // the figure — every mark a Ribbon over a real polyline, the width law
    // keyed in PX of arc length (Profile + alongIsPx) so the press does not
    // slide under the spans::upTo reveal each bone is drawn with. A fraction
    // would be a fraction of the REVEALED bone and the heavy 起 head would
    // walk down the limb as it draws.
    struct Bone {
      std::vector<SkPoint> pts;
      float w0;
    };
    const std::vector<Bone> bones = {
        // the cap, then the head
        {{{86, 30}, {104, 22}, {122, 30}, {118, 38}, {90, 38}, {86, 30}}, 2.6f},
        {{{92, 40},
          {112, 40},
          {116, 52},
          {110, 62},
          {96, 62},
          {90, 52},
          {92, 40}},
         2.8f},
        {{{103, 62}, {102, 78}}, 3.6f},                           // neck
        {{{102, 78}, {100, 108}, {99, 132}}, 5.6f},               // spine
        {{{100, 82}, {72, 88}, {46, 92}, {34, 94}}, 4.6f},        // bow arm
        {{{101, 84}, {126, 96}, {140, 84}, {138, 70}}, 4.6f},     // draw arm
        {{{78, 84}, {60, 122}, {52, 176}, {58, 214}}, 3.2f},      // left robe
        {{{124, 84}, {142, 124}, {150, 178}, {144, 214}}, 3.2f},  // right robe
        {{{58, 214}, {84, 222}, {118, 222}, {144, 214}}, 3.0f},   // hem
        {{{80, 222}, {74, 250}, {66, 274}}, 4.4f},                // left leg
        {{{122, 222}, {130, 250}, {140, 274}}, 4.4f},             // right leg
        {{{60, 274}, {78, 278}}, 3.0f},                           // feet
        {{{134, 274}, {152, 278}}, 3.0f},
        {{{74, 132}, {126, 130}}, 2.4f},            // belt
        {{{99, 66}, {92, 100}, {104, 128}}, 2.0f},  // lapel
    };
    for (size_t i = 0; i < bones.size(); ++i) {
      const Bone& b = bones[i];
      SkPathBuilder pb;
      pb.moveTo(b.pts[0]);
      for (size_t j = 1; j < b.pts.size(); ++j) pb.lineTo(b.pts[j]);
      SkPath p = pb.detach();
      float len = 0;
      for (size_t j = 1; j < b.pts.size(); ++j)
        len += SkPoint::Distance(b.pts[j - 1], b.pts[j]);
      const float w0 = b.w0;
      brush::Ribbon rib;
      rib.fill = Fill::color(hex(0x241d15, 0.90f));
      rib.step = 2.0f;
      rib.width = BonePress{len, w0};
      g.child(box()
                  .left(0)
                  .top(0)
                  .width(Dim(w))
                  .height(Dim(h))
                  .shape([p](SkSize) { return p; })
                  .stroke(spans::upTo(gate(tArch + 0.05f * (float)i,
                                           tArch + 0.05f * (float)i + 0.55f)),
                          rib));
    }
    // the bow and the arrow
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(w))
            .height(Dim(h))
            .shape([](SkSize) {
              SkPathBuilder b;
              b.moveTo(34, 8);
              b.cubicTo(-10, 56, -10, 126, 34, 176);
              b.moveTo(34, 8);
              b.lineTo(20, 92);
              b.lineTo(34, 176);
              return b.detach();
            })
            .stroke(spans::upTo(gate(tArch + 0.45f, tArch + 1.0f)),
                    lines::Line{.width = 1.9f,
                                .fill = Fill::color(hex(0x241d15, 0.88f))}));
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(h))
                .shape([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(140, 90);
                  b.lineTo(6, 92);
                  return b.detach();
                })
                .stroke(spans::upTo(gate(tArch + 0.75f, tArch + 1.15f)),
                        lines::arrow(1.5f, Fill::color(kCinnabar), 9.0f)));
    g.child(text(toU8("a bowman in traditional dress, captioned THE GOD OF"),
                 type(faceMono, 8.4f, hex(0x4a3b28, 0.85f)))
                .left(-18)
                .top(h - 12)
                .width(Dim(300))
                .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
    g.child(text(toU8("LIGHTNING, over a title nobody can read convincingly"),
                 type(faceMono, 8.4f, hex(0x4a3b28, 0.85f)))
                .left(-18)
                .top(h + 0)
                .width(Dim(300))
                .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
    return g;
  }

  /** The closing title. The circulating reading 'Qi jie meng ji dian jing yi
   *  juan' ignores the first character and reads a non-standard third
   *  character as 夢; 蔑 is likelier. The paper's verdict is quoted, and the
   *  graphs are drawn ILLEGIBLE on purpose. */
  Element unreadTitle() {
    const float x = segX(0, 2118.0f) - segLo(0);
    auto g = box()
                 .left(x - 34)
                 .top(kBandTop + 44 - kSegTop)
                 .width(Dim(68))
                 .height(Dim(kBandH - 88))
                 .key("title")
                 .opacity(gate(tArch + 0.9f, tArch + 1.8f));
    for (int i = 0; i < 6; ++i) {
      const float y = 8.0f + (float)i * 46.0f;
      SkPathBuilder pb;
      const uint32_t seed = (uint32_t)(3326 + i * 17);
      const int nh = 2 + (int)(noise::hash(seed, 1) * 2.99f);
      for (int h = 0; h < nh; ++h) {  // horizontals, the spine of a graph
        const float hy = y + 7 + (float)h * (26.0f / (float)nh) +
                         noise::hash(seed, (uint32_t)(h + 10)) * 3.0f;
        const float x0 = 14 + noise::hash(seed, (uint32_t)(h + 20)) * 7.0f;
        const float x1 = 52 - noise::hash(seed, (uint32_t)(h + 30)) * 8.0f;
        pb.moveTo(x0, hy);
        pb.lineTo(x1, hy + noise::hash(seed, (uint32_t)(h + 40)) * 2.0f - 1.0f);
      }
      const float vx = 26 + noise::hash(seed, 50) * 12.0f;
      pb.moveTo(vx, y + 4);
      pb.lineTo(vx + noise::hash(seed, 51) * 4.0f - 2.0f, y + 36);
      if (noise::hash(seed, 52) > 0.45f) {  // a left-falling stroke
        pb.moveTo(34, y + 6);
        pb.lineTo(15, y + 34);
      }
      if (noise::hash(seed, 53) > 0.55f) {  // and a right-falling one
        pb.moveTo(30, y + 10);
        pb.lineTo(50, y + 34);
      }
      g.child(
          box()
              .left(0)
              .top(0)
              .width(Dim(68))
              .height(Dim(kBandH - 88))
              .shape([p = pb.detach()](SkSize) { return p; })
              .stroke(
                  spans::upTo(gate(tArch + 0.9f + (float)i * 0.08f,
                                   tArch + 1.4f + (float)i * 0.08f)),
                  Brush{}
                      .shaped(shapers::Jitter{
                          .segLength = 9.0f, .deviation = 0.7f, .seed = seed})
                      .layer(lines::Line{
                          .width = 1.9f,
                          .fill = Fill::color(hex(0x241d15, 0.78f))})));
    }
    return g;
  }

  // =========================================================================
  // THE APPARATUS

  Element locator() {
    const float lw = 1740.0f, lh = lw * kWideMm / kScrollMm;
    const float lx = 720.0f, ly = 92.0f;
    const float mm = lw / kScrollMm;
    auto g = box()
                 .left(lx)
                 .top(ly)
                 .width(Dim(lw))
                 .height(Dim(lh))
                 .rotate(0.32f)
                 .key("locator")
                 .opacity(gate(0.3f, 1.2f));
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(lw))
                .height(Dim(lh))
                .fill(Paint::linear({0, 0}, {0, lh},
                                       {{0.0f, hex(0xa2865c)},
                                        {0.5f, hex(0xd6bf95)},
                                        {1.0f, hex(0xa2865c)}}))
                .stroke(PathFormat{
                    .width = 0.9f,
                    .strokeFill = Fill::color(hex(0x2a2118, 0.75f))}));
    // the 26 clouds and the 80 columns of the divination section, at the RIGHT
    for (int c = 0; c < 26; ++c) {
      const float cx = lw - 24.0f - (float)c * 24.0f;
      g.child(box()
                  .left(cx - 7)
                  .top(6)
                  .width(Dim(14))
                  .height(Dim(9))
                  .shape(shapes::blob((uint32_t)(700 + c), 0.34f, 6))
                  .fill(Fill::color(hex(0x33291c, 0.85f))));
    }
    for (int c = 0; c < 80; ++c) {
      const float cx = lw - 20.0f - (float)c * 7.6f;
      g.child(box()
                  .left(cx)
                  .top(19)
                  .width(Dim(1.1f))
                  .height(Dim(lh - 25))
                  .fill(Fill::color(hex(0x33291c, 0.55f))));
    }
    // the 13 maps
    const float atlasRight = lw - kScrollMm * mm + kAtlasMm * mm;
    for (int k = 1; k <= 12; ++k) {
      const float x0 = atlasRight - (mapSlotS(k) + kMapWmm) * mm;
      g.child(box()
                  .left(x0)
                  .top(lh * 0.16f)
                  .width(Dim(kMapWmm * mm))
                  .height(Dim(lh * 0.68f))
                  .stroke(PathFormat{
                      .width = 0.8f,
                      .strokeFill = Fill::color(hex(0x2a2118, 0.9f))}));
    }
    {
      const float dcx = atlasRight - discCentreS() * mm;
      const float dr = lh * 0.34f;
      g.child(box()
                  .left(dcx - dr)
                  .top(lh * 0.5f - dr)
                  .width(Dim(dr * 2))
                  .height(Dim(dr * 2))
                  .shape(shapes::circle())
                  .stroke(PathFormat{
                      .width = 0.8f,
                      .strokeFill = Fill::color(hex(0x2a2118, 0.9f))}));
    }
    // the two windows this plate actually shows
    struct Win {
      float s0, s1;
    };
    const Win wins[2] = {
        {(kOriginL - kBreakL) / kPxMm, (kOriginL + 90) / kPxMm},
        {(kOriginR - kW - 90) / kPxMm, (kOriginR - kBreakR) / kPxMm}};
    for (const Win& wn : wins) {
      // the plate's canvas reaches ~227 mm past the atlas's own left end, so
      // window 0 runs off the strip; clamp it to the scroll it annotates
      const float a = std::max(0.0f, atlasRight - wn.s1 * mm);
      const float b = std::min(lw, atlasRight - wn.s0 * mm);
      g.child(box()
                  .left(a)
                  .top(-4)
                  .width(Dim(b - a))
                  .height(Dim(lh + 8))
                  .fill(Fill::color(hex(0x2f6d86, 0.30f)))
                  .stroke(spans::corners(9.0f),
                          brush::solid(1.4f, Fill::color(kTrace))));
    }
    g.child(text(toU8("THE WHOLE SCROLL, 1:16 \xc2\xb7 3,940 \xc3\x97 244 mm "
                      "\xc2\xb7 right: "
                      "26 cloud drawings over 80 columns of uranomancy "
                      "\xc2\xb7 left: the "
                      "13-map atlas, 2,100 mm \xc2\xb7 shaded: what this plate "
                      "shows"),
                 type(faceMono, 8.6f, hex(0x9a8a68, 0.9f)))
                .left(2)
                .top(lh + 5)
                .width(Dim(1700)));
    return g;
  }

  /** WHERE THE POLE WAS. The paper dates the chart partly from the pole:
   *  "the measured shift in polar distance of the pole reference point
   *  between the S.3326 map and the sky at date +700 is only marginally
   *  significant with a difference of (3.9±2.9)°." This panel draws the
   *  pole's own track, computed from the same IAU 1976 matrix the stars
   *  ride, and marks the four epochs the paper names. */
  Element poleDrift() {
    const float S = 132.0f, cx = S * 0.5f, cy = S * 0.5f;
    const float pxPerDeg = (S * 0.5f - 12.0f) / 30.0f;
    auto g = box()
                 .left(150)
                 .top(234)
                 .width(Dim(S))
                 .height(Dim(S))
                 .key("pole")
                 .opacity(gate(tPrec0 - 0.8f, tPrec0 + 0.2f));
    auto poleAt = [](float epoch, float& ra, float& dec) {
      const Mat3 M = precMatrix((epoch - 2000.0f) * 0.01f);
      ra = wrap360(std::atan2(M.m[5], M.m[2]) / kD);
      dec = std::asin(std::clamp(M.m[8], -1.0f, 1.0f)) / kD;
    };
    auto plot = [&](float ra, float dec) {
      const float r = (90.0f - dec) * pxPerDeg;
      const float a = ra * kD;
      return SkPoint{cx + r * std::cos(a), cy + r * std::sin(a)};
    };
    for (int ring = 10; ring <= 30; ring += 10) {
      const float rr = (float)ring * pxPerDeg;
      g.child(box()
                  .left(cx - rr)
                  .top(cy - rr)
                  .width(Dim(rr * 2))
                  .height(Dim(rr * 2))
                  .shape(shapes::circle())
                  .stroke(PathFormat{
                      .width = 0.5f,
                      .strokeFill = Fill::color(hex(0x8a7458, 0.30f)),
                      .dashIntervals = {2, 5}}));
    }
    // the track, drawn BACKWARD from J2000 as the precession runs
    SkPathBuilder tb;
    for (int e = 2000; e >= 500; e -= 25) {
      float ra, dec;
      poleAt((float)e, ra, dec);
      const SkPoint q = plot(ra, dec);
      (e == 2000) ? tb.moveTo(q) : tb.lineTo(q);
    }
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(S))
            .height(Dim(S))
            .shape([t = tb.detach()](SkSize) { return t; })
            .stroke(spans::upTo(gate(tPrec0, tPrec1)),
                    lines::Line{.width = 2.0f, .fill = Fill::color(kTrace)}));
    // the whole 26,000-year circle, faint, for context
    SkPathBuilder wb;
    for (int e = -24000; e <= 4000; e += 250) {
      float ra, dec;
      poleAt((float)e, ra, dec);
      const SkPoint q = plot(ra, dec);
      if (90.0f - dec > 30.0f) continue;
      (wb.countPoints() == 0) ? wb.moveTo(q) : wb.lineTo(q);
    }
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(S))
            .height(Dim(S))
            .shape([t = wb.detach()](SkSize) { return t; })
            .stroke(PathFormat{.width = 0.7f,
                               .strokeFill = Fill::color(hex(0x8a7458, 0.45f)),
                               .dashIntervals = {3, 4}}));
    // alp UMi is 0.74° from the J2000 pole, so its dot lands ON the centre
    // marker and the default up-right label lands ON "J2000 pole". The two
    // captions flank the coincident pair on one line instead.
    struct Ref {
      float ra, dec;
      const char* name;
      float lx, ly;
    };
    const Ref refs[3] = {{37.9529f, 89.2641f, "alp UMi", 6.0f, 6.0f},
                         {222.6764f, 74.1555f, "bet UMi", 5.0f, -5.0f},
                         {211.0973f, 64.3758f, "alp Dra", -8.0f, -13.0f}};
    for (const Ref& r : refs) {
      const SkPoint q = plot(r.ra, r.dec);
      g.child(box()
                  .left(q.fX - 3)
                  .top(q.fY - 3)
                  .width(Dim(6))
                  .height(Dim(6))
                  .shape(shapes::circle())
                  .fill(Fill::color(kCinnabar)));
      g.child(text(toU8(r.name), type(faceMono, 7.4f, hex(0x9a8a68)))
                  .left(q.fX + r.lx)
                  .top(q.fY + r.ly)
                  .width(Dim(60)));
    }
    {
      float ra, dec;
      poleAt(700.0f, ra, dec);
      const SkPoint q = plot(ra, dec);
      g.child(box()
                  .left(q.fX - 6)
                  .top(q.fY - 6)
                  .width(Dim(12))
                  .height(Dim(12))
                  .shape(shapes::star(4, 0.30f))
                  .fill(Fill::color(kTrace))
                  .opacity(gate(tPrec1 - 0.4f, tPrec1 + 0.3f)));
    }
    g.child(box()
                .left(cx - 3)
                .top(cy - 3)
                .width(Dim(6))
                .height(Dim(6))
                .shape(shapes::circle())
                .stroke(PathFormat{
                    .width = 0.9f,
                    .strokeFill = Fill::color(hex(0xe0cfa6, 0.8f))}));
    g.child(text(toU8("J2000 pole"), type(faceMono, 7.4f, hex(0x6d6249)))
                .left(cx - 52)
                .top(cy + 6)
                .width(Dim(70)));
    return g;
  }

  Element poleText() {
    auto g = box()
                 .left(300)
                 .top(228)
                 .width(Dim(452))
                 .key("poletext")
                 .opacity(gate(tPrec0 - 0.6f, tPrec0 + 0.4f));
    g.child(text(toU8("THE CHART DATES ITSELF"),
                 type(faceDisplay, 12.0f, hex(0xc9a35c), 1.0f))
                .left(0)
                .top(0)
                .width(Dim(430)));
    const char* rows[7] = {
        "the celestial pole's own track, from the SAME IAU 1976",
        "matrix the 1,460 stars ride. rings at 10/20/30 deg.",
        "polar distance at +700, computed here:",
        "   alp UMi 7.88   bet UMi 10.66   alp Dra 19.14",
        "the paper measures the map's pole 3.9 +/- 2.9 deg from",
        "the sky at +700 and calls it \"fully consistent\". so does",
        "this: 3.9 deg is HALF alp UMi's own distance at that date.",
    };
    for (int i = 0; i < 7; ++i)
      g.child(text(toU8(rows[i]),
                   type(faceMono, 9.0f, i == 3 ? kChalk : hex(0x9a8a68)))
                  .left(0)
                  .top(18.0f + (float)i * 12.2f)
                  .width(Dim(430)));

    // THE EPOCH, RUNNING. One Output remapped three ways: it turns the star
    // field's rotation matrix, walks the pole's track above, and slides this
    // marker — bind() doing the unit conversion at each call site instead of
    // three Outputs in the tick loop.
    const float bw = 430.0f;
    g.child(box()
                .left(0)
                .top(112)
                .width(Dim(bw))
                .height(Dim(9))
                .shape([](SkSize sz) {
                  SkPathBuilder b;
                  b.moveTo(0, 0);
                  b.lineTo(0, sz.height());
                  b.moveTo(0, sz.height() * 0.5f);
                  b.lineTo(sz.width(), sz.height() * 0.5f);
                  b.moveTo(sz.width(), 0);
                  b.lineTo(sz.width(), sz.height());
                  for (int c = 1; c < 13; ++c) {
                    const float x = sz.width() * (float)c / 13.0f;
                    b.moveTo(x, sz.height() * 0.5f - 2.5f);
                    b.lineTo(x, sz.height() * 0.5f + 2.5f);
                  }
                  return b.detach();
                })
                .stroke(lines::Line{.width = 0.9f,
                                    .fill = Fill::color(hex(0x9a8a68, 0.8f))}));
    g.child(text(toU8("+700"), type(faceMono, 8.0f, hex(0x9a8a68)))
                .left(0)
                .top(124)
                .width(Dim(40)));
    g.child(text(toU8("J2000"), type(faceMono, 8.0f, hex(0x9a8a68)))
                .left(bw - 40)
                .top(124)
                .width(Dim(40))
                .textAlign(weave::TextAlignment::kEnd));
    g.child(box()
                .left(-4)
                .top(107)
                .width(Dim(8))
                .height(Dim(19))
                .shape(shapes::polygon(3, 180.0f))
                .fill(Fill::color(kCinnabar))
                .translateX(settled
                                ? Animatable<float>(0.0f)
                                : Animatable<float>(bind(&scribe)
                                                        .window(tPrec0, tPrec1)
                                                        .invert()
                                                        .target(0.0f, bw))));
    g.child(text(toU8("13.00 Julian centuries \xc2\xb7 the sky slides "
                      "18.5\xc2\xb0 in RA"),
                 type(faceMono, 8.4f, hex(0xc9a35c)))
                .left(0)
                .top(136)
                .width(Dim(430)));
    return g;
  }

  /** THE THIRD QUESTION, DRAWN. Twelve maps of 48 deg on a 30 deg pitch: the
   *  frames butt on the paper but their RA windows OVERLAP by 18 deg, so the
   *  scroll's own RA axis jumps BACKWARD at every map boundary. This ruler
   *  is the two readings side by side. */
  Element raRuler(int seg) {
    auto g = box()
                 .left(0)
                 .top(0)
                 .width(Dim(segHi(seg) - segLo(seg)))
                 .height(Dim(kSegH))
                 .key(fmt("raruler%d", seg))
                 .opacity(gate(tFold1 - 0.4f, tFold1 + 0.6f));
    const float y = kBandTop - kSegTop - 46.0f;
    if (seg == 1)
      g.child(text(toU8("EACH FRAME SPANS 48\xc2\xb0 OF RA ON A 30\xc2\xb0 "
                        "PITCH \xc2\xb7 "
                        "HATCHED: the 18\xc2\xb0 it shares with its neighbour "
                        "\xc2\xb7 "
                        "the axis JUMPS BACK at every boundary"),
                   type(faceMono, 8.4f, hex(0xc9a35c, 0.9f)))
                  .left(600)
                  .top(y - 26)
                  .width(Dim(900)));  // just clear of the sheet
    for (int k = 1; k <= 12; ++k) {
      const float s0 = mapSlotS(k);
      const float xl = segX(seg, s0 + kMapWmm) - segLo(seg);
      const float xr = segX(seg, s0) - segLo(seg);
      if (xr < -6 || xl > segHi(seg) - segLo(seg) + 6) continue;
      g.child(
          box()
              .left(xl)
              .top(y)
              .width(Dim(xr - xl))
              .height(Dim(11))
              .shape([](SkSize sz) {
                SkPathBuilder b;
                b.moveTo(0, 0);
                b.lineTo(0, sz.height());
                b.moveTo(0, sz.height() * 0.5f);
                b.lineTo(sz.width(), sz.height() * 0.5f);
                b.moveTo(sz.width(), 0);
                b.lineTo(sz.width(), sz.height());
                return b.detach();
              })
              .stroke(lines::Line{.width = 1.0f,
                                  .fill = Fill::color(hex(0xc9a35c, 0.7f))}));
      g.child(text(toU8(fmt("%d\xc2\xb0",
                            (int)std::lround(wrap360(mapCentre(k) - 24.0f)))),
                   type(faceMono, 7.6f, hex(0xc9a35c, 0.85f)))
                  .left(xr - 26)
                  .top(y + 13)
                  .width(Dim(28))
                  .textAlign(weave::TextAlignment::kEnd));
      g.child(text(toU8(fmt("%d\xc2\xb0",
                            (int)std::lround(wrap360(mapCentre(k) + 24.0f)))),
                   type(faceMono, 7.6f, hex(0xc9a35c, 0.85f)))
                  .left(xl - 2)
                  .top(y + 13)
                  .width(Dim(28)));
      // the 18 deg this frame shares with its LEFT neighbour, hatched
      const float ov = 18.0f / kRaPerMm * kPxMm;
      g.child(box()
                  .left(xl)
                  .top(y - 11)
                  .width(Dim(ov))
                  .height(Dim(9))
                  .fill(Fill::color(hex(0xb4531f, 0.16f)))
                  .background(lines::hatch(Fill::color(hex(0xb4531f, 0.75f)),
                                           3.6f, 0.7f, 45.0f)));
    }
    return g;
  }

  Element breakMark() {
    auto g = box()
                 .left(kBreakL - 8)
                 .top(kBandTop - 16)
                 .width(Dim(kBreakR - kBreakL + 16))
                 .height(Dim(kBandH + 32))
                 .key("break")
                 .opacity(gate(tPaper + 0.4f, tPaper + 1.4f));
    const float w = kBreakR - kBreakL + 16, h = kBandH + 32;
    g.child(box().left(0).top(0).width(Dim(w)).height(Dim(h)).fill(
        Fill::color(hex(0x171410, 0.96f))));
    for (int i = 0; i < 2; ++i) {
      const float x = 8.0f + (float)i * (w - 16.0f);
      g.child(
          box()
              .left(x - 9)
              .top(0)
              .width(Dim(18))
              .height(Dim(h))
              .shape([](SkSize s) {
                SkPathBuilder b;
                b.moveTo(s.width() * 0.5f, 0);
                // the loop walks a distance; the accumulated float is the
                // position
                // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                for (float y = 0; y < s.height(); y += 22.0f) {
                  b.lineTo(s.width() * 0.5f + 6, y + 5.5f);
                  b.lineTo(s.width() * 0.5f - 6, y + 16.5f);
                  b.lineTo(s.width() * 0.5f, y + 22.0f);
                }
                return b.detach();
              })
              .stroke(lines::Line{.width = 1.2f,
                                  .fill = Fill::color(hex(0x8a7458, 0.8f))}));
    }
    const float sL = (kOriginR - kBreakR) / kPxMm,
                sR = (kOriginL - kBreakL) / kPxMm;
    g.child(text(toU8(fmt("%d mm", (int)std::lround(sR - sL))),
                 type(faceMono, 8.2f, hex(0x9a8a68, 0.85f)))
                .left(-16)
                .top(h + 4)
                .width(Dim(w + 32))
                .textAlign(weave::TextAlignment::kCenter));
    return g;
  }

  feed::TextOptions logStyle() {
    feed::TextOptions s;
    s.styles.base(type(faceMono, 9.2f, hex(0x9a8a68)))
        .set("dim", type(faceMono, 9.2f, hex(0x6d6249)))
        .set("heading", type(faceMono, 9.2f, hex(0xc9a35c)))
        .set("pass", type(faceMono, 9.2f, hex(0x6ba87e)))
        .set("number", type(faceMono, 9.2f, hex(0xcf6a4a)))
        .set("fail", type(faceMono, 9.2f, hex(0xc4483a)));
    s.window.gap = 1.0f;
    s.window.visible = 12;
    return s;
  }

  Element projectionPanel() {
    auto g = box().left(96).top(1046).width(Dim(700)).key("proj").opacity(
        gate(tProj, tProj + 0.9f));
    g.child(text(toU8("TWO QUESTIONS THE CHART CANNOT ANSWER, AND WHY"),
                 type(faceDisplay, 13.0f, hex(0xc9a35c), 1.1f))
                .left(0)
                .top(0)
                .width(Dim(690)));

    // curve A: the Mercator ordinate against its own best-fit line
    const float pw = 320.0f, ph = 132.0f;
    struct Plot {
      float x, lo, hi;
      bool merc;
      const char* cap;
    };
    const Plot plots[2] = {{0, -27, 43, true,
                            "map 5 DEC \xe2\x88\x92"
                            "27\xc2\xb0\xe2\x80\xa6"
                            "+43\xc2\xb0"},
                           {366, 0, 38, false,
                            "map 13 polar distance 0\xc2\xb0\xe2\x80\xa6"
                            "38\xc2\xb0"}};
    for (const auto& pl : plots) {
      auto p = box().left(pl.x).top(30).width(Dim(pw)).height(Dim(ph));
      p.child(box().left(0).top(0).width(Dim(pw)).height(Dim(ph)).stroke(
          spans::edges(16.0f),
          brush::solid(0.9f, Fill::color(hex(0x8a7458, 0.5f)))));
      const float lo = pl.lo, hi = pl.hi;
      const bool merc = pl.merc;
      p.child(
          box()
              .left(0)
              .top(0)
              .width(Dim(pw))
              .height(Dim(ph))
              .shape([lo, hi, merc, pw, ph](SkSize) {
                // the departure curve, self-normalised
                float ys[81];
                float sx = 0, sy = 0, sxx = 0, sxy = 0;
                for (int i = 0; i <= 80; ++i) {
                  const float v = lo + (hi - lo) * (float)i / 80.0f;
                  ys[i] = merc ? std::log(std::tan((45.0f + v * 0.5f) * kD))
                               : std::tan(v * 0.5f * kD);
                  sx += v;
                  sy += ys[i];
                  sxx += v * v;
                  sxy += v * ys[i];
                }
                const float n = 81.0f;
                const float b = (n * sxy - sx * sy) / (n * sxx - sx * sx);
                const float a = (sy - b * sx) / n;
                float mx = 1e-9f;
                float dep[81];
                for (int i = 0; i <= 80; ++i) {
                  const float v = lo + (hi - lo) * (float)i / 80.0f;
                  dep[i] = (ys[i] - (a + b * v)) / b;
                  mx = std::max(mx, std::abs(dep[i]));
                }
                SkPathBuilder pb;
                for (int i = 0; i <= 80; ++i)
                  (i ? pb.lineTo(pw * (float)i / 80.0f,
                                 ph * 0.5f - dep[i] / mx * ph * 0.40f)
                     : pb.moveTo(0.0f, ph * 0.5f - dep[0] / mx * ph * 0.40f));
                return pb.detach();
              })
              .stroke(lines::Line{.width = 1.5f, .fill = Fill::color(kTrace)}));
      // the chart's own residual band, to the same vertical scale
      const Departure& dp = merc ? depMerc : depStereo;
      const float resid = merc ? 1.61f : 3.29f;
      const float halfRaw = resid / dp.maxDeg * ph * 0.40f;
      const float half = std::min(halfRaw, ph * 0.5f);
      p.child(box()
                  .left(0)
                  .top(ph * 0.5f - half)
                  .width(Dim(pw))
                  .height(Dim(half * 2))
                  .fill(Fill::color(hex(0xa8382a, 0.13f)))
                  .stroke(PathFormat{
                      .width = 0.6f,
                      .strokeFill = Fill::color(hex(0xa8382a, 0.45f)),
                      .dashIntervals = {4, 4}}));
      p.child(text(toU8(pl.cap), type(faceMono, 8.4f, hex(0x9a8a68)))
                  .left(0)
                  .top(ph + 4)
                  .width(Dim(pw)));
      p.child(text(toU8(merc ? "linear \xe2\x88\x92 Mercator (blue) vs the "
                               "hand (red band)"
                             : "equidist. \xe2\x88\x92 stereo. (blue); the "
                               "hand is 7.6\xc3\x97 "
                               "the plot, off scale"),
                   type(faceMono, 8.4f, hex(0x6d6249)))
                  .left(0)
                  .top(ph + 15)
                  .width(Dim(pw)));
      g.child(std::move(p));
    }
    const char* lines_[6] = {
        "Mercator parts from linear by %.3f\xc2\xb0 max = %.2f mm of paper;",
        "  map 5's own DEC residual is 1.61\xc2\xb0. signal/noise %.2f, n=15,",
        "  SE(r) %.4f -> the published 0.002 is %.2f sigma. NOT A RESULT.",
        "stereographic parts from equidistant by %.3f\xc2\xb0 = %.2f mm;",
        "  radial residual 3.29\xc2\xb0. signal/noise %.2f, n=19, SE(r) %.4f",
        "  -> the published 0.013 is %.2f sigma. NOT A RESULT EITHER.",
    };
    const std::string rows[6] = {
        fmt(lines_[0], depMerc.maxDeg, depMerc.mm),
        fmt(lines_[1], depMerc.ratio),
        fmt(lines_[2], depMerc.sigma, 0.002f / depMerc.sigma),
        fmt(lines_[3], depStereo.maxDeg, depStereo.mm),
        fmt(lines_[4], depStereo.ratio, depStereo.sigma),
        fmt(lines_[5], 0.013f / depStereo.sigma),
    };
    for (int i = 0; i < 6; ++i)
      g.child(text(toU8(rows[(size_t)i]), type(faceMono, 9.6f, kChalk))
                  .left(0)
                  .top(196 + (float)i * 13.4f)
                  .width(Dim(690)));
    g.child(text(toU8("all three maps favour PURE CYLINDRICAL (0.974/0.972, "
                      "0.975/0.974, 0.996/0.994) \xe2\x80\x94 3 of 3, p=0.125"),
                 type(faceMono, 9.6f, hex(0xcf6a4a)))
                .left(0)
                .top(280)
                .width(Dim(690)));
    g.child(text(toU8("the disc cannot decide BECAUSE IT STOPS AT +52\xc2\xb0: "
                      "over a "
                      "full hemisphere the pair would part by 7.00\xc2\xb0"),
                 type(faceMono, 9.6f, hex(0x6d6249)))
                .left(0)
                .top(294)
                .width(Dim(690)));
    return g;
  }

  /** Map 5's twenty asterisms, audited one at a time. Six of them carry a
   *  documented defect and every one is drawn AS FOUND. */
  Element auditPanel() {
    auto g = box().left(840).top(1046).width(Dim(880)).key("audit").opacity(
        gate(tAudit - 0.9f, tAudit - 0.2f));
    g.child(text(toU8("MAP 5 \xc2\xb7 THE ORION REGION \xc2\xb7 TABLE 4 OF "
                      "BONNET-BIDAUD, PRADERIE & WHITFIELD 2009"),
                 type(faceDisplay, 13.0f, hex(0xc9a35c), 1.0f))
                .left(0)
                .top(0)
                .width(Dim(880)));
    g.child(text(toU8("month 4 \xc2\xb7 xiu Zui, Shen, Jing \xc2\xb7 listed "
                      "N\xe2\x86\x92"
                      "S, "
                      "W\xe2\x86\x92"
                      "E, i.e. by increasing RA \xc2\xb7 R=Shi shi  B=Gan shi  "
                      "W=Wu Xian shi"),
                 type(faceMono, 8.6f, hex(0x9a8a68)))
                .left(0)
                .top(16)
                .width(Dim(880)));
    // the subtitle above runs top 16..24 at 8.6 px; the column header needs
    // its own line, not the same one
    const float y0 = 40.0f, rowH = 15.2f;
    // one hand-spaced monospace string cannot land on these columns: the rows
    // are absolutely placed, the numeric block is a THIRD size, and the CJK
    // pair in the middle is double-advance. Each head sits on its own column.
    struct Head {
      float x;
      const char* s;
    };
    const Head heads[9] = {
        {11, "#"},    {30, "ASTERISM"}, {160, "\xe4\xb8\xad\xe6\x96\x87"},
        {228, "COL"}, {253, "SXC"},     {281, "MAP"},
        {312, "CZ"},  {362, "CONF"},    {400, "DEFECT"}};
    for (const Head& h : heads)
      g.child(text(toU8(h.s), type(faceMono, 8.6f, hex(0x6d6249)))
                  .left(h.x)
                  .top(y0 - 13)
                  .width(Dim(120)));
    for (int i = 0; i < 20; ++i) {
      const M5Row& r = kMap5[(size_t)i];
      const float y = y0 + (float)i * rowH;
      const float t = tAudit + (float)i * tAuditEach;
      auto row = box().left(0).top(y).width(Dim(880)).height(Dim(rowH)).opacity(
          gate(t, t + 0.35f));
      int cz = 0;
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) cz = astUnique(kAst[a]);
      row.child(
          text(toU8(fmt("%3d", i + 1)), type(faceMono, 9.4f, hex(0x6d6249)))
              .left(0)
              .top(0)
              .width(Dim(26)));
      row.child(text(toU8(r.pinyin), type(faceMono, 9.4f, kChalk))
                    .left(30)
                    .top(0)
                    .width(Dim(126)));
      row.child(text(toU8(r.native), type(faceHan ? faceHan : faceSerif, 10.4f,
                                          schoolInk(r.col)))
                    .left(160)
                    .top(-2)
                    .width(Dim(64)));
      row.child(box()
                    .left(232)
                    .top(3.4f)
                    .width(Dim(8))
                    .height(Dim(8))
                    .shape(shapes::circle())
                    .fill(Fill::color(schoolInk(r.col)))
                    .stroke(PathFormat{.width = 0.8f,
                                       .strokeFill = Fill::color(kInk)}));
      row.child(text(toU8(fmt("%4d %4d %4d", r.nSxc, r.nMap, cz)),
                     type(faceMono, 9.4f,
                          r.nSxc == r.nMap ? hex(0x9a8a68) : hex(0xcf6a4a)))
                    .left(250)
                    .top(0)
                    .width(Dim(94)));
      // the confidence index, as five cells
      for (int c = 0; c < 5; ++c)
        row.child(box()
                      .left(356 + (float)c * 7.0f)
                      .top(3.6f)
                      .width(Dim(5.2f))
                      .height(Dim(7.0f))
                      .fill(Fill::color(c < r.conf ? hex(0xc9a35c, 0.85f)
                                                   : hex(0x6d6249, 0.28f))));
      if (r.defect)
        row.child(text(toU8(r.defect), type(faceMono, 9.0f, hex(0xb4531f)))
                      .left(400)
                      .top(0)
                      .width(Dim(478)));
      g.child(std::move(row));
    }
    const float yT = y0 + 20.0f * rowH + 8.0f;
    g.child(box()
                .left(0)
                .top(yT - 4)
                .width(Dim(878))
                .height(Dim(0.8f))
                .fill(Fill::color(hex(0x8a7458, 0.5f)))
                .opacity(gate(tAudit + 5.4f, tAudit + 5.9f)));
    const std::string tot = fmt(
        "TOTALS  SXC %d   map %d   Chen Zhuo %d distinct (Fa's 3 in, Sanzhu's "
        "9 absent \xe2\x80\x94 5 + 9 = SXC's 14 for Wuche, exactly)",
        m5Sxc, m5Map, m5ChenZhuo);
    g.child(text(toU8(tot), type(faceMono, 9.4f, kChalk))
                .left(0)
                .top(yT)
                .width(Dim(878))
                .opacity(gate(tAudit + 5.5f, tAudit + 6.0f)));
    g.child(
        text(toU8("Table 4's own n(map) column sums to 108. Its stated total "
                  "is 109. The census is soft, and the paper says so."),
             type(faceMono, 9.4f, hex(0xcf6a4a)))
            .left(0)
            .top(yT + 13)
            .width(Dim(878))
            .opacity(gate(tAudit + 5.7f, tAudit + 6.2f)));
    g.child(text(toU8("6 documented defects in 20 asterisms, drawn AS FOUND "
                      "\xe2\x80\x94 "
                      "ringed on map 5 above. A study that corrects them has "
                      "destroyed the object."),
                 type(faceMono, 9.4f, hex(0xb4531f)))
                .left(0)
                .top(yT + 26)
                .width(Dim(878))
                .opacity(gate(tAudit + 5.9f, tAudit + 6.4f)));
    return g;
  }

  /** THE DISC'S OWN ERRATA. Table 5 is 34 asterisms and 142 stars — and its
   *  n(map) column sums to 141. Everything here is quoted, nothing resolved. */
  Element map13Panel() {
    auto g = box().left(96).top(1362).width(Dim(700)).key("m13").opacity(
        gate(tAudit + 4.6f, tAudit + 5.4f));
    g.child(text(toU8("MAP 13 \xc2\xb7 THE CIRCUMPOLAR DISC \xc2\xb7 TABLE 5"),
                 type(faceDisplay, 12.0f, hex(0xc9a35c), 1.0f))
                .left(0)
                .top(0)
                .width(Dim(700)));
    const char* rows[10] = {
        "34 asterisms, stated total 142 stars; the n(map) column sums to 141",
        "(its Tianpei row reads \"5 or 6\", which is where the one goes).",
        "\xe7\xb4\xab\xe5\xbe\xae Ziwei: two walls, E and W, 14 red + 1 black "
        "\xe2\x80\x94 Chen Zhuo's",
        "  \xe6\x9d\xb1\xe5\x9e\xa3 8 + \xe8\xa5\xbf\xe5\x9e\xa3 7 = 15. "
        "CLOSES EXACTLY.",
        "\xe8\x8f\xaf\xe8\x93\x8b Huagai: 7 stars \"+ 6\" the authors cannot "
        "account for.",
        "  Chen Zhuo HAS \xe6\x9d\xa0 Gang appended to it \xe2\x80\x94 but 9 "
        "stars, not 6.",
        "  consistent, and it does not close. drawn on the disc, unlabelled.",
        "NI 1: one star, no character, east of Gouchen. "
        "\xe5\xa4\xa9\xe6\xa3\x93 Tianpei is the",
        "  SECOND mixed-colour asterism (\"5 R, 1 B?\"), not Ziwei alone.",
        "\xe4\xb8\x89\xe5\x85\xac Sangong: Chen Zhuo files one copy under WU "
        "XIAN, the other",
    };
    for (int i = 0; i < 10; ++i)
      g.child(
          text(toU8(rows[i]),
               type(faceMono, 9.2f, i == 3 || i == 5 ? kChalk : hex(0x9a8a68)))
              .left(0)
              .top(18.0f + (float)i * 12.4f)
              .width(Dim(700)));
    g.child(
        text(
            toU8(
                "under GAN. The map draws both BLACK. Printed, not corrected."),
            type(faceMono, 9.2f, hex(0xb4531f)))
            .left(0)
            .top(18.0f + 10 * 12.4f)
            .width(Dim(700)));
    return g;
  }

  Element consolePanel() {
    const float x = 1768, y = 1042, w = 700, h = 452;
    auto g = box()
                 .left(x)
                 .top(y)
                 .width(Dim(w))
                 .height(Dim(h))
                 .fill(Fill::color(hex(0x100e0b, 0.86f)))
                 .stroke(stroke(1.0f, Fill::color(hex(0x8a7458, 0.24f)),
                                PathFormat::Align::Inner))
                 .key("console");
    g.child(box()
                .left(12)
                .top(9)
                .width(Dim(w - 24))
                .height(Dim(h - 18))
                .column()
                .gap(6)
                .child(feed::feed(logA, logStyle()))
                .child(box().height(1).fill(Fill::color(hex(0x8a7458, 0.16f))))
                .child(feed::feed(logB, logStyle()))
                .child(box().height(1).fill(Fill::color(hex(0x8a7458, 0.16f))))
                .child(feed::feed(logC, logStyle())));
    return g;
  }

  Element ruleNote() {
    auto g = box()
                 .left(766)
                 .top(228)
                 .width(Dim(770))
                 .key("rulenote")
                 .opacity(gate(tFold1 - 0.2f, tFold1 + 0.8f));
    const char* rows[6] = {
        "THE THIRD QUESTION, AND THE PAPER DOES NOT ASK IT",
        "2,100 mm / 13 maps / 50 columns, RA scale 4.24-4.56 deg/cm,",
        "map extension 45-48 deg (Table 3). 48 deg at 4.56 = 10.53 cm",
        "of drawn map; 12 of those + a 20.4 cm disc leaves 63 cm for 50",
        "columns = 12.7 mm each, a NORMAL Tang column. But 12 x 48 = 576",
        "deg over a 360 deg sky: the gold bars below SHARE 18 deg each.",
    };
    for (int i = 0; i < 6; ++i)
      g.child(text(toU8(rows[i]),
                   type(i ? faceMono : faceDisplay, i ? 9.0f : 12.0f,
                        i ? hex(0x9a8a68) : hex(0xc9a35c), i ? 0.0f : 1.0f))
                  .left(0)
                  .top(i ? 16.0f + (float)i * 12.2f : 0.0f)
                  .width(Dim(700)));
    g.child(
        text(toU8("take 30 deg per map instead (12 x 30 = 360, one dot per "
                  "star, matching the 1,339 census) and the columns come out"),
             type(faceMono, 9.0f, hex(0xcf6a4a)))
            .left(0)
            .top(92)
            .width(Dim(700)));
    g.child(
        text(toU8("21.6 mm wide, which is not a Tang column. NEITHER READING "
                  "CLOSES. This plate draws the first, so you can see it."),
             type(faceMono, 9.0f, hex(0xcf6a4a)))
            .left(0)
            .top(104)
            .width(Dim(700)));
    return g;
  }

  Element headings() {
    auto g = box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("head");
    g.child(text(toU8("THE DUNHUANG STAR CHART, REPROJECTED"),
                 type(faceDisplay, 27.0f, hex(0xe0cfa6), 2.4f))
                .left(96)
                .top(16)
                .width(Dim(1200)));
    g.child(text(toU8("British Library Or.8210/S.3326 \xc2\xb7 Mogao Cave 17, "
                      "Dunhuang \xc2\xb7 +649\xe2\x80\x93"
                      "684 \xc2\xb7 3,940 \xc3\x97 244 mm, "
                      "pure mulberry fibre 0.04 mm \xc2\xb7 1,339 dots in 257 "
                      "asterisms"),
                 type(faceMono, 10.2f, hex(0x9a8a68)))
                .left(98)
                .top(46)
                .width(Dim(1500)));
    g.child(text(toU8("NOT TRACED. 1,460 real stars precessed J2000 "
                      "\xe2\x86\x92 +700 "
                      "(IAU 1976) and pushed through Table 3's own measured "
                      "projection."),
                 type(faceMono, 10.2f, hex(0xc9a35c)))
                .left(1660)
                .top(16)
                .width(Dim(830)));
    g.child(
        text(toU8("PLATE I \xc2\xb7 north up, WEST AT RIGHT, RA increasing "
                  "right-to-left \xe2\x80\x94 the direction the scroll reads"),
             type(faceMono, 9.4f, hex(0x6d6249)))
            .left(1660)
            .top(34)
            .width(Dim(830)));
    // the scale bar, in cm of real paper
    const float barMm = 100.0f;
    g.child(box()
                .left(96)
                .top(1546)
                .width(Dim(barMm * kPxMm))
                .height(Dim(7))
                .shape([](SkSize s) {
                  SkPathBuilder b;
                  b.moveTo(0, 6);
                  b.lineTo(0, 0);
                  b.lineTo(s.width(), 0);
                  b.lineTo(s.width(), 6);
                  for (int i = 1; i < 10; ++i) {
                    b.moveTo(s.width() * (float)i / 10.0f, 0);
                    b.lineTo(s.width() * (float)i / 10.0f, i % 5 ? 3 : 5);
                  }
                  return b.detach();
                })
                .stroke(lines::Line{.width = 1.0f,
                                    .fill = Fill::color(hex(0x9a8a68, 0.8f))}));
    g.child(text(toU8("10 cm of scroll \xc2\xb7 IDP scan 204.8 px/cm"),
                 type(faceMono, 8.6f, hex(0x6d6249)))
                .left(96 + barMm * kPxMm + 10)
                .top(1544)
                .width(Dim(420)));
    g.child(text(toU8("data: Stellarium chinese_chenzhuo (GPL) \xc2\xb7 "
                      "astronexus/HYG v4.1 \xc2\xb7 arXiv:0906.3034 Tables "
                      "3\xe2\x80\x93"
                      "5 "
                      "\xc2\xb7 IDP 7861395E5F814419BA05483EAB254832"),
                 type(faceMono, 8.6f, hex(0x6d6249)))
                .left(1660)
                .top(1544)
                .width(Dim(880)));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext&) {
    auto root = box().left(0).top(0).width(Dim(kW)).height(Dim(kH));
    root.child(ground());
    root.child(locator());

    // the equatorial graticule the stars arrive in, fading as the fold runs
    root.child(
        box()
            .left(108)
            .top(250)
            .width(Dim(2344))
            .height(Dim(764))
            .key("grat")
            .opacity(gate(tSky - 0.6f, tSky + 0.6f))
            .zIndex(-1)
            .shape([](SkSize s) {
              SkPathBuilder b;
              for (int i = 0; i <= 12; ++i) {
                const float x = s.width() * (float)i / 12.0f;
                b.moveTo(x, 0);
                b.lineTo(x, s.height());
              }
              for (int j = 0; j <= 6; ++j) {
                const float y = s.height() * (float)j / 6.0f;
                b.moveTo(0, y);
                b.lineTo(s.width(), y);
              }
              return b.detach();
            })
            .stroke(PathFormat{.width = 0.8f,
                               .strokeFill = Fill::color(hex(0x2f6d86, 0.42f)),
                               .dashIntervals = {3, 7}}));

    root.child(scrollBand(-90, kBreakL, "bandL", -0.42f));
    root.child(scrollBand(kBreakR, kW + 90, "bandR", -0.42f));
    for (int seg = 0; seg < 2; ++seg) {
      auto sg = box()
                    .left(segLo(seg))
                    .top(kSegTop)
                    .width(Dim(segHi(seg) - segLo(seg)))
                    .height(Dim(kSegH))
                    .clip(true)
                    .key(seg ? "segR" : "segL");
      for (int k = 1; k <= 12; ++k) {
        sg.child(mapFrame(k, seg));
        sg.child(columnBand(k, seg));
      }
      sg.child(discPlate(seg));
      sg.child(discNotes(seg));
      sg.child(raRuler(seg));
      if (seg == 0) {
        sg.child(unreadTitle());
        sg.child(archer());
      }
      root.child(std::move(sg));
    }
    root.child(breakMark());

    // the star field — ONE leaf for 1,460 dots
    root.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(kW))
            .height(Dim(kH))
            .key("stars")
            .opacity(gate(tSky - 0.5f, tSky + 0.7f))
            .child(instancing::instances(atlas, pool, instancing::Mode::Live)));

    root.child(asterismLines());
    root.child(map5Labels());

    root.child(headings());
    root.child(poleDrift());
    root.child(poleText());
    root.child(ruleNote());
    root.child(projectionPanel());
    root.child(map13Panel());
    root.child(slot("audit"));
    root.child(consolePanel());
    return root;
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas((int)kW, (int)kH);
    ctx.background(kVoid);
    // This study brings its own canvas size and background (above) rather
    // than inheriting a default, and names its own still frame here. The
    // settled plate holds [28.2, 31.0) of the 31 s loop, so 29.0 s sits
    // inside that hold with 2 s of margin before the wrap. Anything much
    // earlier catches the score mid-precession: unprojected sky, no scroll,
    // no asterisms, no checking panel.
    ctx.captureAt(29.0);

    // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
    // library's own walk: the first installed family wins, and a machine
    // with none of them gets the default face AT THE WEIGHT ASKED FOR
    // rather than silently at Normal.
    faceSerif = pickTypeface({"Hoefler Text", "Baskerville"});
    faceItalic =
        pickTypeface({"Hoefler Text", "Baskerville"}, SkFontStyle::Italic());
    faceMono = pickTypeface({"Menlo", "Courier New"});
    faceDisplay =
        pickTypeface({"Optima", "Baskerville"}, SkFontStyle::kBold_Weight);
    faceHan =
        pickTypeface({"Songti SC", "PingFang SC", "Hiragino Sans", "Baskerville"});

    // the fibre runs ALONG the roll: anisotropic luminance grain, not noise
    paperGrain = Paint::recipe(field::grain(1.15f, 4, 3326.0f, 0.42f, 5.5f));
    paperSpeck = patterns::speckle(
        900, 34, 0.20f, 0.85f,
        {skia::toColor(hex(0x6a5330, 0.10f)),
         skia::toColor(hex(0x2a2118, 0.08f))});
    paperSpeck.seed(649);

    computeJoins();
    buildAsterismArt();
    buildFixtures();

    // FIVE cells, because an atlas cell is a whole ELEMENT TREE: the black
    // ring and the school fill bake into ONE sprite, so 1,460 dots stay one
    // draw and there is no second concentric pass.
    atlas = std::make_shared<instancing::Atlas>(3.0f);
    auto dot = [](SkColor4f fill, bool ring) {
      auto e = box().width(11).height(11).shape(shapes::circle());
      if (fill.fA > 0) e.fill(Fill::color(fill));
      if (ring)
        e.stroke(PathFormat{.width = 1.15f,
                            .strokeFill = Fill::color(hex(0x1d1710, 0.92f)),
                            .align = PathFormat::Align::Inner});
      return e;
    };
    cellRed = atlas->cell(dot(kCinnabar, true), {11, 11});
    cellBlack = atlas->cell(dot(kInk, true), {11, 11});
    cellWhite = atlas->cell(dot(kLead, true), {11, 11});
    // NOT an empty ring: the chart's default mark IS a dot with a black
    // ring, and only the FILL carries the school. A star whose school the
    // tables do not give is drawn as a dot of undeclared colour.
    cellOpen = atlas->cell(dot(hex(0x6f5c40), true), {11, 11});
    cellBare = atlas->cell(dot(kCinnabar, false), {11, 11});

    pool = std::make_shared<instancing::Pool>();
    pool->resize((size_t)nStars);
    {
      auto frames = pool->frames();
      auto scales = pool->scales();
      for (int i = 0; i < nStars; ++i) {
        frames[(size_t)i] = placed[(size_t)i].cell;
        // "the dots are all of similar size" — the chart does NOT encode
        // magnitude, and resisting that instinct is part of the study.
        scales[(size_t)i] = 0.74f;
      }
    }
    rebuild(2000.0f, 0.0f);

    logA.append({toU8("THE JOIN"), "heading"});
    logA.append(
        {toU8(fmt("chinese_chenzhuo: %d asterisms, 1,883 vertex words", nAst)),
         "dim"});
    logA.append({toU8("1,463 star TOKENS \xe2\x80\x94 3 are DSO (M44/M7/M31)"),
                 "number"});
    logA.append({toU8(fmt("  so %d HIP numbers vs Chen Zhuo's canonical 1,464",
                          nStars)),
                 "dim"});
    logA.append({toU8("HYG v4.1 join on HIP: 1,457 direct, 3 LOST"), "number"});
    logA.append({toU8("  55203 xi UMa, 78727 xi Sco, 115125 94 Aqr B"), "dim"});
    logA.append(
        {toU8("  cause: HYG BLANKS hip on resolved double components"), "dim"});
    logA.append({toU8("  Bayer fallback recovers all three"), "pass"});
    logA.append(
        {toU8(fmt("RESOLVED %d / %d = 100.00%%", nStars, nStars)), "pass"});
    logA.append(
        {toU8("largest 1300-yr proper motion 1.476 deg (HIP 19849)"), "dim"});

    logB.append({toU8("THE EPOCH, AND THE DECLINATION WINDOW"), "heading"});
    logB.append(
        {toU8("paper precessed to +700, NOT +665 (sect. 4.1)"), "number"});
    logB.append(
        {toU8("  665 vs 700 = 0.489 deg RA; map 5's RA residual 2.26"), "dim"});
    logB.append(
        {toU8("  4.6x below the chart's own hand. UNRESOLVABLE."), "pass"});
    logB.append({toU8(fmt("of %d stars at +700:", nStars)), "dim"});
    logB.append(
        {toU8(fmt("  %4d fall on maps 1-12  (|DEC| <= 45)", nOnMaps)), "dim"});
    logB.append(
        {toU8(fmt("  %4d fall on the disc    (DEC >= +52)", nOnDisc)), "dim"});
    logB.append({toU8(fmt("  %4d fall in the UNCOVERED band +45..+52", nInGap)),
                 "number"});
    logB.append(
        {toU8(fmt("  %4d are south of DEC -45, off the chart", nTooSouth)),
         "number"});
    logB.append(
        {toU8("Chang'an is 34.3N, so DEC < -55.7 never rises at all"), "dim"});

    logC.append({toU8("THE SCHOOLS, AND WHAT IS NOT ATTESTED"), "heading"});
    logC.append(
        {toU8("S.3326 is the FIRST document to colour the three"), "dim"});
    logC.append(
        {toU8("  schools: Shi shi RED, Gan shi BLACK, Wu Xian WHITE"), "dim"});
    logC.append({toU8(fmt("Tables 4+5 give a colour for 54 asterisms; %d stars",
                          nSchooled)),
                 "dim"});
    logC.append({toU8(fmt("%d stars have NO published school: drawn undeclared",
                          nUnattested)),
                 "number"});
    logC.append(
        {toU8("guessing the rest would be inventing the evidence"), "dim"});
    logC.append(
        {toU8("Huagai +6 unaccounted: Chen Zhuo HAS Gang, 9 stars"), "number"});
    logC.append(
        {toU8("  9 != 6, so it is consistent and does not close"), "dim"});
    logC.append(
        {toU8("Sangong: Chen Zhuo files one under WU XIAN (white),"), "dim"});
    logC.append({toU8("  the map draws BOTH black. printed, not corrected."),
                 "number"});

    ctx.ticker.add([this, &tick = ctx.ticker](double) {
      clockT = tick.elapsed();
      const float t = (float)std::fmod(clockT, (double)kLoop);
      scribe = t;
      const float e = 2000.0f + (700.0f - 2000.0f) *
                                    smooth((t - tPrec0) / (tPrec1 - tPrec0));
      const float f = smooth((t - tFold0) / (tFold1 - tFold0));
      rebuild(e, f);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("audit", auditPanel());
  }

  /** Two render()s per loop, at the score's end and at its start. */
  void update(double, sketch::SketchContext& ctx) override {
    const float t = (float)std::fmod(clockT, (double)kLoop);
    const bool want = t >= tSettle;
    if (want != settled) {
      settled = want;
      ctx.composer.render(describe(ctx));
      ctx.composer.renderSlot("audit", auditPanel());
    }
  }
};

SIGIL_SKETCH(DunhuangStarChart, "Study \xc2\xb7 Esoteric",
             "The Dunhuang star chart (c. 649\xe2\x80\x93"
             "684) reprojected from "
             "1,460 real stars \xe2\x80\x94 and it refuses to answer")
