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
// FIVE CORRECTIONS TO MY OWN BRIEF, ALL FROM THE DATA.
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
//   4. Table 5 is 34 asterisms and 142 stars, not the 20 my brief carried.
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
// THE THIRD MODE OF LINE. Sigillum is ENGRAVED; the fulu is WRITTEN; this
// is DRAWN — a draughtsman's fine brush on 0.04 mm mulberry. One weight,
// one ring, three fills, and a hand-drawn join BOWS: every asterism line
// here is ops::Sketchy at low amplitude over a polyline through real dot
// centres, revealed by trim() in RA order so the joining sweeps
// right-to-left across the scroll, in reading order.
//
// BUILT FROM (the library, not by hand):
//   instancing::Atlas/Pool   1,460 dots as ONE leaf, Mode::Live, rebuilt
//                        from the precession matrix every frame while the
//                        epoch runs. FIVE cells, not "three plus a tint":
//                        an atlas cell is a whole ELEMENT TREE, so fill
//                        AND ring bake into one sprite (the brief
//                        predicted six cells or two passes; neither).
//   lines::Rails         the scroll's top and bottom rules are NOT equal
//                        weights; the map frames; the disc's limb
//   PathFormat::trimStart/trimEnd   28 mansion rules that stop short of
//                        both edges — interrupted, not chords
//   ops::Sketchy         every asterism join, and the paper's own edge
//   shapes::parametric   the disc's radial DEC scale and the graticule
//   shapes::annulus / sector / circle / spiral
//   brushes::Ribbon + widthFn   the archer, keyed to distance/fullLength
//   brushes::ScatterBrush   the roll's contact replication marks
//   patterns::grain (anisotropic) + Cache::Texture   the mulberry ground
//   decorations::brackets/gappedRule   with an EXPLICIT angleDeg — the
//                        disc's 28-fold division turns 12.86° per vertex
//                        and the 30° default finds no corners at all
//   TextPath::Orient::Radial   the disc's mansion names
//   trim() + bind().window()   one Output writes the whole plate
//   console::LineRing    four panels of checks, printed as they run
//   slot()/renderSlot()  the audit advances without a full render()
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/dunhuang_star_chart.cpp \
//       --frame /tmp/dunhuang_star_chart.png --at 29.0   (the settled plate)
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

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
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

template <typename... A> std::string fmt(const char *f, A... args) {
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
constexpr float kPxMm = 2.22f;      // px per mm of scroll
constexpr float kD = 3.14159265358979f / 180.0f;

// Table 3, map 5 (the best-measured of the twelve)
constexpr float kRaPerMm = 0.456f;  // 4.56 °/cm horizontal
constexpr float kDecPerMm = 0.528f; // 5.28 °/cm vertical
constexpr float kPolPerMm = 0.510f; // 5.10 °/cm radial, map 13
constexpr float kAzGain = 1.05f;    // Table 3: azimuthal scale, theory 1.00
constexpr float kDiscCenDec = 87.6f; // Table 3: map 13 centre DEC — NOT +90

constexpr float kScrollMm = 3940.0f, kWideMm = 244.0f;
constexpr float kAtlasMm = 2100.0f;
constexpr float kMapWmm = 48.0f / kRaPerMm;   // 105.26 mm of drawn map
constexpr float kMapHmm = 90.0f / kDecPerMm;  // 170.45 mm of drawn map
constexpr float kDiscMm = 204.0f;             // the disc's slot
constexpr float kSlotMm = (kAtlasMm - kDiscMm) / 12.0f; // 158.0 mm
constexpr float kColBandMm = kSlotMm - kMapWmm;         // 52.74 mm of columns
constexpr float kColsPerMap = 50.0f / 12.0f;
constexpr float kColMm = kColBandMm / kColsPerMap;      // 12.66 mm per column

constexpr float kBandTop = 424.0f;
constexpr float kBandH = kWideMm * kPxMm;   // 541.68 px
constexpr float kBandMid = kBandTop + kBandH * 0.5f;
constexpr float kFrameTop = kBandTop + (kWideMm - kMapHmm) * 0.5f * kPxMm;
constexpr float kFrameH = kMapHmm * kPxMm;

// two scroll segments with a drafting break between them
constexpr float kBreakL = 1148.0f, kBreakR = 1206.0f;
constexpr float kSegTop = 244.0f;
constexpr float kSegH = 780.0f;
constexpr float kOriginR = 3420.0f; // canvas x of scroll coordinate s = 0
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
inline float smooth(float v) { v = clamp01(v); return v * v * (3 - 2 * v); }
inline float wrap360(float d) {
  d = std::fmod(d, 360.0f);
  return d < 0 ? d + 360.0f : d;
}
inline float wrap180(float d) {
  d = wrap360(d);
  return d > 180.0f ? d - 360.0f : d;
}

// ---------------------------------------------------------------------------
// PRECESSION — IAU 1976 ζ/z/θ, the same routine chaucer_astrolabe runs at
// T = −6.74 cy for 1326. Here T sweeps 0 → −13.00 cy, J2000 → +700.

struct Mat3 { float m[9]; };

Mat3 precMatrix(float T) {
  const float s = kD / 3600.0f;
  const float z1 = (2306.2181f * T + 0.30188f * T * T + 0.017998f * T * T * T) * s;
  const float z2 = (2306.2181f * T + 1.09468f * T * T + 0.018203f * T * T * T) * s;
  const float th = (2004.3109f * T - 0.42665f * T * T - 0.041833f * T * T * T) * s;
  const float c1 = std::cos(z1), s1 = std::sin(z1);
  const float c2 = std::cos(z2), s2 = std::sin(z2);
  const float ct = std::cos(th), st = std::sin(th);
  return {{c1 * ct * c2 - s1 * s2, -s1 * ct * c2 - c1 * s2, -st * c2,
           c1 * ct * s2 + s1 * c2, -s1 * ct * s2 + c1 * c2, -st * s2,
           c1 * st, -s1 * st, ct}};
}

inline void precess(const Mat3 &M, float ra, float dec, float &raOut,
                    float &decOut) {
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
// THE STAR TABLE — 1,460 HIP stars from Stellarium's chinese_chenzhuo joined
// to HYG v4.1, proper motion carried back 1,300 years, positions still on the
// J2000 EQUINOX so the precession above is the only rotation applied.

// 1460 stars: J2000 EQUINOX, position at epoch +700 (proper motion applied, -1300 yr).
const float kStarData[] = {
  230.203f,71.828f,3.00f,222.719f,74.151f,2.07f,216.869f,75.688f,4.25f,212.263f,77.535f,4.80f,201.995f,78.633f,5.74f,222.090f,82.593f,5.63f,
  192.396f,83.407f,5.38f,183.032f,77.609f,5.14f,194.678f,75.468f,6.00f,188.432f,69.784f,3.85f,187.592f,69.222f,5.01f,281.438f,74.056f,5.25f,
  275.196f,71.325f,4.22f,264.237f,68.642f,4.77f,257.215f,65.708f,3.17f,246.011f,61.494f,2.73f,240.695f,58.444f,4.01f,231.238f,58.960f,3.29f,
  211.145f,64.370f,3.67f,188.716f,70.022f,4.95f,172.893f,69.338f,3.82f,160.767f,69.081f,5.01f,145.770f,72.263f,5.15f,124.837f,75.751f,5.55f,
  104.899f,76.982f,4.55f,80.792f,79.173f,5.08f,228.451f,67.488f,5.15f,224.465f,65.921f,4.63f,232.826f,64.181f,5.74f,236.635f,62.620f,5.19f,
  230.672f,63.375f,5.72f,230.648f,62.061f,5.99f,231.977f,60.671f,5.90f,274.615f,72.860f,3.55f,265.453f,72.246f,4.57f,267.303f,76.873f,5.02f,
  277.435f,77.547f,5.65f,269.940f,79.959f,5.74f,259.865f,80.136f,5.74f,245.472f,69.113f,5.26f,247.020f,68.756f,4.94f,250.230f,64.595f,4.84f,
  241.616f,67.788f,5.44f,253.802f,65.116f,4.88f,210.495f,68.680f,6.34f,213.044f,69.450f,5.18f,13.516f,83.711f,5.59f,52.828f,84.959f,5.62f,
  36.703f,89.268f,1.97f,262.990f,86.567f,4.35f,251.441f,82.036f,4.21f,235.980f,77.795f,4.29f,321.112f,80.529f,5.97f,311.951f,80.566f,5.36f,
  307.208f,81.011f,5.96f,306.979f,81.417f,5.38f,310.561f,82.523f,5.75f,318.347f,81.229f,6.12f,16.740f,86.261f,4.24f,302.203f,77.703f,4.38f,
  290.372f,79.612f,6.06f,307.868f,74.961f,5.18f,316.338f,78.116f,5.91f,287.211f,76.604f,5.11f,325.345f,71.277f,4.55f,329.898f,73.238f,5.04f,
  322.151f,70.558f,3.23f,332.412f,72.340f,4.79f,338.719f,73.634f,5.08f,346.966f,75.400f,4.41f,354.920f,77.586f,3.21f,18.690f,87.153f,6.20f,
  351.159f,87.301f,5.56f,333.020f,86.093f,5.27f,342.703f,85.332f,5.89f,343.241f,84.338f,4.70f,341.799f,83.137f,4.77f,337.464f,78.832f,5.45f,
  338.098f,76.230f,5.70f,339.240f,75.369f,5.80f,319.527f,62.568f,2.45f,330.766f,64.596f,4.26f,336.773f,65.133f,5.52f,342.479f,66.245f,3.50f,
  345.865f,67.206f,5.25f,349.603f,68.108f,4.75f,30.912f,72.413f,3.95f,39.544f,72.813f,5.17f,47.965f,74.425f,4.85f,145.630f,69.263f,5.72f,
  143.687f,69.802f,4.54f,137.586f,67.167f,4.80f,135.657f,67.623f,4.74f,130.103f,64.318f,4.59f,128.691f,65.161f,5.47f,290.155f,65.700f,4.60f,
  300.689f,67.856f,4.51f,296.957f,70.254f,3.84f,288.049f,67.628f,3.07f,283.544f,71.282f,4.82f,289.035f,73.318f,4.45f,73.512f,66.340f,4.26f,
  94.713f,69.356f,4.76f,104.315f,58.467f,4.35f,89.828f,54.333f,3.72f,80.856f,57.565f,5.24f,75.859f,60.448f,4.03f,74.336f,53.749f,4.43f,
  76.686f,51.661f,4.98f,268.320f,56.844f,3.73f,262.976f,55.150f,4.86f,262.617f,52.297f,2.79f,269.156f,51.497f,2.24f,264.870f,46.005f,3.82f,
  195.107f,66.603f,5.37f,191.813f,62.779f,5.88f,207.857f,64.725f,4.58f,202.169f,59.933f,5.40f,217.964f,60.219f,6.26f,218.125f,55.403f,5.74f,
  160.480f,68.452f,5.74f,155.319f,68.760f,5.88f,127.666f,60.757f,3.35f,137.649f,63.536f,4.67f,142.796f,63.052f,3.65f,147.955f,59.094f,3.78f,
  143.767f,51.871f,3.17f,137.298f,51.616f,4.46f,160.629f,65.743f,5.12f,156.041f,65.574f,4.94f,157.653f,64.276f,6.07f,154.919f,48.437f,6.00f,
  158.747f,57.069f,5.16f,157.771f,55.993f,4.82f,159.829f,53.697f,5.55f,163.435f,54.589f,5.12f,162.842f,56.580f,5.66f,160.919f,57.218f,5.79f,
  167.447f,44.508f,3.00f,168.921f,52.753f,6.51f,164.835f,51.884f,6.22f,169.222f,49.480f,5.88f,174.498f,50.628f,6.14f,176.587f,47.769f,3.69f,
  203.684f,49.006f,4.68f,199.579f,49.675f,5.14f,201.553f,46.035f,5.88f,214.194f,46.031f,4.18f,176.723f,55.640f,5.27f,172.491f,56.750f,6.29f,
  175.121f,57.966f,6.35f,178.988f,56.599f,5.83f,166.036f,61.764f,1.81f,165.407f,56.370f,2.34f,178.392f,53.691f,2.41f,183.788f,57.030f,3.32f,
  193.435f,55.963f,1.76f,200.905f,54.933f,2.23f,206.952f,49.319f,1.85f,201.230f,54.994f,3.99f,216.438f,51.995f,4.04f,214.128f,51.335f,4.75f,
  213.335f,51.794f,4.53f,184.998f,-0.658f,3.89f,190.638f,-1.471f,2.74f,194.071f,3.417f,3.39f,195.645f,10.952f,2.85f,197.666f,17.483f,4.32f,
  177.406f,1.863f,3.59f,170.317f,6.034f,4.05f,170.929f,10.558f,4.00f,168.582f,15.458f,3.33f,168.472f,20.571f,2.56f,185.194f,3.335f,4.97f,
  191.492f,9.704f,5.65f,190.441f,10.268f,4.88f,191.444f,7.672f,5.22f,194.746f,17.397f,4.76f,193.050f,17.076f,6.32f,191.657f,16.571f,5.12f,
  198.296f,27.559f,4.23f,193.340f,21.256f,4.89f,188.784f,18.369f,5.03f,185.221f,17.761f,4.72f,184.031f,14.910f,5.09f,176.299f,8.266f,4.84f,
  176.472f,6.594f,4.04f,180.218f,6.625f,4.65f,181.383f,8.712f,4.12f,177.297f,16.262f,6.05f,177.451f,14.613f,2.14f,178.915f,15.646f,5.53f,
  177.200f,14.282f,5.90f,177.781f,12.269f,6.37f,181.055f,21.461f,5.89f,177.052f,20.220f,4.50f,175.220f,21.369f,5.26f,194.096f,38.283f,5.61f,
  168.810f,23.099f,4.56f,175.269f,34.339f,5.31f,177.476f,34.929f,5.73f,180.451f,36.071f,5.59f,186.499f,39.031f,5.01f,188.775f,41.252f,4.24f,
  184.149f,33.102f,4.99f,184.391f,28.924f,5.71f,186.769f,28.298f,4.35f,186.606f,27.272f,4.92f,186.752f,26.829f,4.98f,187.236f,25.918f,5.29f,
  187.757f,24.570f,5.47f,188.397f,24.286f,6.28f,186.088f,26.100f,5.17f,185.631f,25.849f,4.78f,185.567f,24.775f,6.21f,185.109f,26.659f,5.52f,
  184.763f,26.012f,6.44f,184.096f,23.948f,4.93f,185.131f,25.995f,6.16f,187.370f,24.108f,5.47f,186.290f,23.941f,6.03f,174.237f,-0.839f,4.30f,
  172.572f,-2.997f,4.77f,169.205f,-3.639f,4.45f,169.303f,2.062f,5.18f,166.864f,1.986f,5.52f,165.454f,-2.472f,4.73f,163.935f,24.755f,4.30f,
  165.586f,20.166f,4.42f,161.652f,14.218f,5.49f,162.316f,10.554f,5.32f,161.477f,30.695f,5.36f,160.798f,26.343f,5.51f,160.900f,23.185f,5.08f,
  161.566f,18.906f,5.50f,135.041f,48.120f,3.12f,135.926f,47.177f,3.57f,154.358f,42.930f,3.45f,155.621f,41.487f,3.06f,169.631f,33.084f,3.49f,
  169.727f,31.739f,4.33f,258.766f,24.896f,3.12f,262.677f,26.105f,4.41f,266.734f,27.992f,3.42f,271.886f,28.760f,3.84f,281.419f,20.668f,4.19f,
  286.355f,13.898f,2.99f,284.041f,4.194f,4.62f,275.525f,-2.646f,3.23f,269.760f,-9.732f,3.32f,264.411f,-15.376f,3.54f,257.579f,-15.760f,2.43f,
  247.593f,21.495f,2.78f,245.498f,19.137f,3.74f,242.032f,17.049f,5.00f,238.997f,16.125f,3.85f,236.521f,15.437f,3.65f,233.727f,10.538f,3.80f,
  236.018f,6.410f,2.63f,237.658f,4.455f,3.71f,243.603f,-3.643f,2.73f,244.551f,-4.707f,3.23f,249.285f,-10.576f,2.54f,270.765f,-8.167f,4.77f,
  270.065f,-3.674f,4.62f,262.230f,0.323f,5.41f,259.165f,-0.422f,4.72f,261.691f,-5.071f,4.53f,264.466f,-8.111f,4.58f,260.191f,-12.848f,4.32f,
  255.280f,-4.195f,4.82f,265.883f,4.510f,2.76f,266.982f,2.734f,3.75f,271.319f,2.848f,4.03f,270.161f,2.935f,3.93f,270.433f,1.310f,4.42f,
  270.065f,4.373f,4.79f,271.823f,8.723f,4.64f,271.860f,9.535f,3.71f,272.191f,20.817f,4.37f,270.374f,21.582f,4.26f,271.957f,26.092f,5.83f,
  268.853f,26.048f,5.47f,263.693f,12.640f,2.08f,258.664f,14.378f,2.78f,253.708f,20.958f,5.39f,253.886f,18.429f,5.35f,255.773f,14.115f,4.97f,
  256.326f,12.745f,4.89f,248.217f,11.517f,4.84f,246.339f,14.055f,4.57f,254.524f,9.379f,3.19f,253.522f,10.178f,4.39f,251.459f,8.577f,5.15f,
  252.562f,7.251f,5.48f,251.951f,5.261f,5.22f,248.143f,5.521f,5.63f,252.860f,1.221f,5.51f,256.326f,0.824f,6.00f,261.628f,4.138f,4.34f,
  236.009f,32.519f,5.57f,233.241f,31.362f,4.14f,232.032f,29.074f,3.66f,233.623f,26.747f,2.22f,235.731f,26.277f,3.81f,237.431f,26.091f,4.59f,
  239.428f,26.900f,4.14f,240.377f,29.854f,4.98f,240.346f,33.583f,5.39f,250.707f,38.953f,3.48f,246.350f,37.394f,5.53f,242.261f,36.367f,4.73f,
  237.812f,35.783f,4.79f,234.851f,36.638f,4.64f,231.190f,37.347f,4.31f,225.506f,40.401f,3.49f,276.067f,39.509f,5.11f,274.973f,36.050f,4.33f,
  269.062f,37.248f,3.86f,259.333f,33.102f,4.80f,255.400f,33.568f,5.27f,255.092f,30.917f,3.92f,250.518f,31.478f,2.81f,245.587f,33.813f,5.20f,
  245.565f,30.853f,4.86f,258.774f,36.808f,3.16f,259.437f,37.268f,4.64f,260.938f,37.143f,4.15f,201.314f,-11.150f,0.98f,203.774f,-0.613f,3.38f,
  201.081f,-5.151f,5.76f,203.029f,-6.240f,4.68f,205.872f,3.564f,5.35f,203.517f,3.668f,4.92f,197.500f,-5.527f,4.38f,207.420f,21.258f,4.92f,
  205.266f,22.505f,5.63f,205.186f,19.947f,5.73f,201.909f,-15.981f,4.76f,197.979f,-16.095f,5.04f,202.452f,-23.285f,6.40f,199.704f,-23.157f,2.99f,
  208.915f,-47.272f,2.55f,220.494f,-47.379f,2.30f,218.894f,-42.146f,2.33f,211.904f,-36.183f,2.06f,200.303f,-36.681f,2.75f,197.904f,-43.361f,5.24f,
  190.483f,-48.959f,2.20f,189.528f,-48.539f,3.85f,187.028f,-50.226f,3.91f,186.650f,-51.447f,4.82f,207.389f,-41.680f,3.41f,207.416f,-42.467f,3.47f,
  209.579f,-42.094f,3.83f,211.523f,-41.172f,4.36f,210.981f,-60.364f,0.61f,222.637f,-61.008f,-0.01f,207.972f,-32.983f,4.32f,207.380f,-34.429f,4.19f,
  206.621f,-32.991f,4.23f,215.045f,-43.062f,5.55f,215.771f,-39.504f,4.41f,213.740f,-41.831f,5.61f,216.541f,-45.216f,4.56f,215.158f,-45.160f,4.78f,
  214.858f,-46.057f,3.55f,209.684f,-44.796f,3.87f,210.429f,-45.596f,4.34f,209.155f,-46.560f,5.82f,202.768f,-39.403f,3.90f,201.453f,-39.734f,5.11f,
  201.739f,-41.498f,5.69f,213.221f,-10.325f,4.18f,214.013f,-5.849f,4.07f,217.102f,-2.227f,4.81f,214.784f,-13.382f,4.52f,214.334f,19.904f,-0.05f,
  208.706f,-1.495f,5.16f,205.437f,-8.718f,5.03f,206.492f,-12.429f,5.50f,212.709f,-16.298f,4.93f,213.868f,-18.197f,5.53f,214.684f,-18.703f,5.86f,
  216.224f,-24.800f,5.34f,221.333f,16.983f,4.60f,220.178f,16.415f,4.49f,220.268f,13.733f,3.78f,208.694f,18.527f,2.68f,206.997f,17.437f,4.50f,
  207.404f,15.783f,4.05f,222.669f,-27.938f,4.42f,221.922f,-26.084f,5.23f,217.054f,-29.483f,4.97f,215.856f,-27.712f,4.78f,222.759f,-16.017f,2.75f,
  228.069f,-19.780f,4.54f,233.857f,-14.792f,3.91f,229.287f,-9.375f,2.61f,225.745f,2.087f,4.39f,218.073f,38.253f,3.04f,221.267f,27.067f,2.35f,
  218.592f,29.697f,4.47f,218.000f,30.328f,3.57f,218.185f,22.245f,5.91f,212.609f,25.113f,4.82f,209.666f,21.711f,5.76f,214.992f,16.285f,4.84f,
  214.363f,15.261f,5.84f,213.684f,13.946f,6.45f,213.616f,12.979f,5.53f,213.720f,10.160f,5.29f,214.779f,13.016f,5.41f,226.046f,-25.266f,3.25f,
  223.965f,-20.792f,5.72f,223.588f,-24.632f,5.27f,235.657f,-49.481f,6.02f,237.941f,-47.049f,6.00f,240.781f,-49.233f,4.65f,237.750f,-25.742f,4.63f,
  238.409f,-25.318f,4.59f,239.894f,-41.738f,4.99f,220.941f,-35.110f,4.06f,223.222f,-37.797f,5.02f,220.503f,-37.782f,4.01f,224.799f,-42.096f,3.13f,
  224.650f,-43.120f,2.68f,222.922f,-43.566f,4.32f,227.220f,-45.270f,4.07f,226.292f,-47.044f,3.91f,228.138f,-52.074f,3.41f,228.037f,-48.720f,3.88f,
  224.150f,-47.871f,5.62f,223.928f,-33.855f,5.32f,225.756f,-32.636f,5.45f,226.648f,-30.908f,5.97f,228.660f,-31.520f,4.91f,229.461f,-30.147f,4.35f,
  230.493f,-36.230f,3.57f,230.797f,-36.851f,4.54f,231.356f,-38.729f,4.60f,230.352f,-40.639f,3.22f,234.586f,-42.588f,4.34f,235.383f,-44.565f,4.64f,
  232.355f,-46.731f,5.26f,241.614f,-45.187f,4.73f,235.683f,-37.419f,5.23f,234.939f,-34.407f,4.66f,235.681f,-34.699f,4.75f,211.976f,43.865f,5.13f,
  241.362f,-19.796f,2.56f,240.087f,-22.608f,2.29f,239.718f,-26.105f,2.89f,239.227f,-29.205f,3.87f,241.706f,-20.661f,3.93f,241.834f,-20.852f,4.31f,
  243.003f,-19.452f,4.00f,246.034f,-20.020f,4.48f,246.758f,-18.448f,4.22f,247.802f,-16.599f,4.29f,250.401f,-17.742f,4.91f,250.463f,-19.940f,5.55f,
  248.026f,-21.479f,4.45f,246.399f,-23.438f,4.57f,241.114f,-11.363f,4.84f,239.552f,-14.273f,4.95f,238.419f,-16.778f,4.13f,235.499f,-19.641f,4.75f,
  239.650f,-24.823f,5.43f,238.889f,-26.255f,5.63f,238.631f,-27.329f,6.15f,245.301f,-25.586f,2.90f,247.356f,-26.424f,1.06f,248.974f,-28.208f,2.82f,
  245.971f,-39.194f,5.37f,249.087f,-35.260f,4.18f,247.851f,-34.698f,4.24f,242.862f,-41.074f,5.86f,240.038f,-38.387f,3.42f,241.655f,-36.791f,4.22f,
  245.625f,-43.908f,5.89f,243.833f,-47.355f,5.13f,246.803f,-47.547f,4.46f,247.926f,-41.817f,5.31f,249.096f,-42.858f,5.46f,251.183f,-40.832f,5.64f,
  252.809f,-34.201f,2.29f,252.972f,-38.040f,3.00f,253.499f,-42.361f,4.70f,258.027f,-43.135f,3.32f,264.327f,-42.997f,1.86f,266.896f,-40.125f,2.99f,
  265.625f,-39.021f,2.39f,263.406f,-37.093f,1.62f,262.693f,-37.285f,2.70f,253.090f,-38.009f,3.56f,269.199f,-44.335f,4.85f,272.816f,-45.941f,4.52f,
  271.663f,-50.088f,3.65f,265.041f,-49.351f,4.76f,263.928f,-46.492f,4.56f,262.852f,-23.953f,4.78f,261.593f,-24.133f,4.16f,260.506f,-24.991f,3.27f,
  259.029f,-26.190f,4.33f,267.445f,-37.053f,3.19f,271.475f,-30.359f,2.98f,275.236f,-29.819f,2.72f,276.060f,-34.340f,1.79f,274.465f,-36.702f,3.10f,
  276.752f,-45.949f,3.49f,276.723f,-48.097f,5.44f,277.132f,-48.988f,4.10f,271.216f,-35.889f,5.98f,273.440f,-21.058f,3.84f,277.011f,-25.354f,2.82f,
  281.393f,-26.991f,3.17f,283.811f,-26.278f,2.05f,286.756f,-27.580f,3.32f,285.659f,-29.881f,2.60f,284.419f,-21.102f,3.52f,286.141f,-21.721f,3.76f,
  287.441f,-21.010f,2.88f,289.413f,-18.949f,4.88f,290.428f,-17.855f,3.92f,290.431f,-15.953f,4.52f,278.809f,-8.130f,3.85f,280.566f,-9.053f,4.70f,
  280.872f,-8.279f,4.88f,281.887f,-5.694f,5.38f,281.796f,-4.742f,4.22f,284.243f,-5.832f,4.83f,285.429f,-5.725f,4.02f,286.569f,-4.850f,3.43f,
  285.723f,-3.702f,5.40f,287.329f,-37.870f,4.11f,286.561f,-36.962f,4.23f,284.741f,-37.067f,4.83f,284.165f,-37.335f,5.36f,280.945f,-38.304f,5.11f,
  278.347f,-38.718f,5.67f,278.074f,-39.690f,5.16f,278.360f,-42.305f,4.62f,279.912f,-43.171f,5.42f,282.198f,-43.673f,5.46f,284.084f,-42.700f,5.35f,
  285.751f,-42.079f,4.74f,287.065f,-40.487f,4.57f,287.505f,-39.328f,4.10f,299.481f,-15.458f,5.01f,295.604f,-16.121f,5.06f,269.663f,-28.757f,5.99f,
  269.161f,-28.059f,5.76f,268.725f,-24.886f,6.18f,269.950f,-23.798f,4.74f,272.927f,-23.691f,4.96f,275.379f,-24.914f,6.19f,274.511f,-27.042f,4.66f,
  272.011f,-28.446f,4.55f,284.608f,-31.018f,6.09f,286.103f,-31.040f,5.49f,298.878f,-26.326f,4.70f,299.724f,-26.205f,4.84f,300.651f,-27.715f,4.43f,
  299.233f,-27.165f,4.54f,295.906f,-37.532f,6.14f,293.533f,-40.034f,5.89f,292.303f,-43.404f,5.70f,293.348f,-45.263f,5.59f,298.804f,-41.887f,4.12f,
  297.952f,-39.870f,5.32f,299.931f,-35.267f,4.37f,289.914f,-35.417f,5.59f,290.656f,-44.451f,3.96f,300.200f,-45.115f,5.80f,277.771f,-32.972f,5.37f,
  305.235f,-14.786f,3.05f,306.826f,-18.208f,5.08f,307.465f,-18.554f,5.94f,307.221f,-17.811f,4.77f,305.160f,-12.754f,4.77f,304.491f,-12.546f,3.58f,
  304.845f,-19.115f,5.28f,304.492f,-21.801f,5.86f,303.319f,-26.967f,5.73f,307.377f,-22.380f,6.13f,308.216f,-24.928f,6.35f,306.357f,-28.665f,5.86f,
  309.851f,-23.940f,6.36f,310.347f,-25.991f,6.29f,310.302f,-31.576f,5.75f,305.599f,-42.020f,5.60f,311.564f,-39.188f,5.48f,306.837f,-37.362f,6.24f,
  315.754f,-38.592f,5.32f,313.397f,-39.775f,5.34f,312.756f,-37.907f,5.52f,321.519f,-37.825f,5.64f,318.314f,-36.425f,5.97f,318.175f,-39.384f,5.25f,
  298.811f,6.581f,3.71f,297.500f,8.729f,0.76f,296.559f,10.614f,2.72f,281.080f,39.648f,4.67f,279.141f,38.680f,0.03f,281.180f,37.596f,4.34f,
  295.018f,18.021f,4.39f,296.849f,18.530f,3.68f,295.259f,17.488f,4.39f,299.665f,19.484f,3.51f,300.814f,18.508f,5.99f,300.013f,17.521f,5.33f,
  301.175f,17.217f,5.80f,300.873f,16.034f,5.73f,303.548f,15.177f,4.94f,293.445f,7.435f,4.45f,291.283f,3.086f,3.36f,294.798f,5.399f,5.18f,
  291.630f,0.339f,4.64f,294.180f,-1.279f,4.36f,298.116f,1.008f,3.87f,302.813f,-0.824f,3.24f,307.387f,-2.877f,4.91f,307.986f,-9.892f,5.66f,
  299.047f,11.422f,5.28f,300.242f,8.563f,5.90f,301.029f,7.273f,5.51f,305.804f,5.356f,5.30f,309.817f,-14.948f,5.24f,310.161f,-16.151f,5.79f,
  308.523f,-13.747f,6.11f,283.629f,36.897f,4.22f,282.519f,33.364f,3.52f,284.737f,32.689f,3.25f,286.826f,36.102f,5.25f,283.824f,43.917f,4.08f,
  288.440f,39.146f,4.43f,289.092f,38.133f,4.35f,291.536f,36.313f,5.17f,292.943f,34.454f,4.74f,311.907f,-9.483f,3.78f,313.146f,-8.971f,4.73f,
  313.036f,-5.505f,5.55f,311.936f,-5.013f,4.43f,309.817f,0.493f,5.15f,309.579f,-1.099f,4.31f,309.179f,-2.544f,4.91f,311.772f,-2.479f,6.28f,
  312.331f,-0.554f,6.24f,308.299f,11.314f,4.03f,309.664f,10.079f,5.07f,309.440f,11.381f,5.42f,308.459f,13.019f,5.39f,309.682f,13.315f,5.69f,
  311.674f,16.195f,4.27f,309.889f,15.909f,3.77f,309.343f,14.612f,3.64f,310.872f,15.090f,4.43f,308.810f,14.670f,4.64f,318.231f,30.252f,3.21f,
  319.474f,34.894f,4.41f,318.608f,37.897f,3.74f,314.289f,41.176f,3.94f,310.357f,45.280f,1.25f,303.318f,46.815f,4.80f,296.221f,45.113f,2.86f,
  305.556f,40.257f,2.23f,311.398f,33.851f,2.48f,289.239f,53.324f,3.80f,292.414f,51.684f,3.76f,294.115f,50.126f,4.49f,292.848f,50.290f,5.55f,
  300.360f,64.825f,5.22f,307.359f,62.999f,4.21f,311.256f,61.543f,3.41f,311.381f,57.665f,4.52f,301.291f,61.970f,5.40f,298.987f,58.854f,4.98f,
  303.309f,56.538f,4.28f,312.959f,-26.918f,4.12f,311.544f,-25.214f,4.13f,313.722f,-17.917f,5.78f,316.457f,-17.211f,4.08f,316.116f,-19.846f,4.82f,
  312.490f,-33.772f,4.89f,311.533f,-21.508f,5.91f,314.159f,-26.273f,5.70f,320.550f,-16.836f,4.28f,318.902f,-20.651f,5.17f,315.432f,-26.872f,6.04f,
  316.793f,-24.990f,4.49f,319.461f,-32.165f,4.71f,316.605f,-32.345f,5.20f,315.324f,-32.258f,4.67f,316.491f,-30.100f,5.69f,322.881f,-5.569f,2.90f,
  318.934f,5.282f,3.92f,320.704f,6.807f,5.16f,316.139f,5.503f,5.63f,318.605f,10.117f,4.47f,317.567f,10.186f,4.70f,324.401f,19.314f,5.46f,
  320.481f,19.782f,4.08f,326.515f,22.950f,5.29f,322.477f,23.638f,4.52f,326.661f,-16.020f,2.85f,324.952f,-16.654f,3.69f,331.594f,-13.849f,4.29f,
  332.645f,-11.568f,5.43f,325.609f,-18.863f,4.72f,324.265f,-19.466f,4.51f,323.724f,-20.100f,5.70f,322.129f,-21.805f,4.50f,321.668f,-22.418f,3.77f,
  324.002f,-26.163f,5.73f,330.203f,-28.454f,5.43f,333.569f,-27.768f,5.45f,333.407f,-25.184f,5.58f,333.566f,-21.095f,5.33f,332.246f,-18.517f,5.80f,
  330.505f,-17.882f,6.28f,327.869f,-18.595f,6.19f,340.155f,-27.043f,4.18f,337.851f,-32.339f,4.29f,332.105f,-34.026f,4.99f,326.223f,-32.992f,4.35f,
  323.063f,-33.943f,5.97f,322.259f,-31.234f,6.52f,320.449f,-29.158f,6.53f,331.440f,-0.316f,2.95f,332.448f,6.187f,3.52f,326.035f,9.875f,2.38f,
  336.313f,1.376f,4.80f,337.139f,-0.034f,3.65f,335.367f,-1.391f,3.86f,338.807f,-0.097f,4.04f,327.453f,30.184f,5.07f,328.149f,28.816f,5.52f,
  325.928f,28.830f,4.49f,328.262f,25.925f,5.09f,326.143f,25.640f,4.14f,331.634f,25.335f,3.77f,331.382f,28.967f,5.69f,333.030f,24.950f,5.97f,
  335.323f,28.329f,4.78f,333.988f,37.748f,4.14f,333.191f,34.623f,5.34f,332.502f,33.185f,4.28f,340.081f,44.273f,4.50f,337.623f,43.125f,4.52f,
  333.452f,39.709f,4.50f,325.841f,38.285f,5.69f,323.640f,38.500f,4.87f,321.838f,37.116f,5.30f,325.502f,35.509f,5.98f,329.163f,63.627f,5.11f,
  326.365f,61.121f,4.25f,325.873f,58.781f,4.23f,325.521f,51.190f,4.69f,326.696f,49.310f,4.23f,323.508f,45.626f,3.98f,319.611f,43.949f,5.04f,
  316.228f,43.928f,3.72f,316.647f,47.649f,4.56f,337.281f,58.414f,4.07f,333.442f,57.026f,4.18f,332.704f,58.200f,3.39f,332.883f,59.418f,5.05f,
  333.008f,60.756f,5.37f,334.240f,-9.036f,5.80f,335.044f,-7.822f,5.35f,335.886f,-7.198f,5.92f,337.768f,-6.516f,6.15f,340.909f,-18.820f,4.68f,
  341.917f,-14.053f,5.68f,340.352f,-12.225f,6.78f,337.447f,-12.915f,6.41f,336.094f,-13.535f,5.76f,336.564f,-16.744f,5.55f,335.403f,-21.568f,5.12f,
  335.840f,-24.762f,5.53f,338.891f,-23.989f,5.97f,341.927f,-19.539f,5.24f,331.205f,-0.891f,5.29f,330.820f,-2.151f,4.74f,346.167f,15.221f,2.49f,
  345.867f,28.033f,2.44f,350.147f,23.744f,4.58f,342.444f,24.617f,3.51f,340.440f,29.315f,4.80f,340.745f,30.231f,2.93f,341.610f,23.569f,3.97f,
  351.269f,23.391f,4.42f,342.909f,9.820f,5.16f,344.501f,9.410f,6.43f,343.777f,8.806f,4.91f,346.749f,9.414f,4.54f,347.513f,9.825f,5.39f,
  347.380f,8.679f,5.05f,341.587f,12.351f,4.20f,340.337f,10.835f,3.41f,333.098f,-14.177f,6.04f,334.199f,-12.833f,5.34f,337.661f,-10.668f,4.82f,
  334.726f,-13.301f,5.96f,343.147f,-7.591f,3.73f,349.777f,-5.117f,5.56f,356.951f,-2.764f,5.49f,1.250f,-0.484f,6.32f,4.416f,1.685f,6.19f,
  6.356f,1.944f,5.77f,8.835f,-0.483f,5.94f,6.631f,-0.050f,6.16f,343.861f,-20.134f,6.36f,345.335f,-22.782f,6.27f,345.010f,-25.135f,5.66f,
  344.276f,-29.563f,1.17f,343.146f,-32.868f,4.46f,344.108f,49.735f,4.99f,345.952f,49.992f,4.64f,344.264f,48.686f,5.34f,348.088f,49.372f,4.53f,
  349.417f,49.013f,4.82f,349.862f,48.607f,5.44f,354.307f,46.610f,3.81f,355.061f,44.341f,4.15f,354.520f,43.269f,4.29f,349.945f,42.077f,5.81f,
  345.469f,42.326f,3.62f,346.790f,52.812f,6.12f,344.809f,52.643f,6.31f,342.191f,54.416f,6.14f,349.109f,53.299f,5.58f,359.746f,55.757f,4.88f,
  358.599f,57.501f,4.51f,356.722f,58.632f,4.88f,352.495f,58.547f,4.89f,350.631f,60.134f,5.56f,346.649f,59.420f,4.84f,346.935f,57.062f,5.57f,
  2.859f,-35.174f,5.24f,6.457f,-42.178f,2.40f,6.498f,-43.692f,3.93f,353.748f,-42.619f,4.69f,353.199f,-37.832f,4.38f,354.426f,-45.488f,4.74f,
  346.744f,-43.515f,4.28f,347.522f,-45.237f,3.88f,346.708f,-38.894f,5.62f,3.670f,-18.906f,4.44f,3.019f,-17.929f,5.29f,0.924f,-17.333f,4.55f,
  2.848f,-15.371f,4.89f,1.128f,-10.505f,4.99f,357.625f,-14.394f,5.70f,355.644f,-14.521f,4.49f,355.610f,-15.440f,5.27f,355.436f,-17.817f,4.82f,
  356.454f,-18.683f,5.28f,357.829f,-18.908f,5.17f,3.719f,-9.567f,5.77f,3.593f,-7.781f,5.13f,4.862f,-8.810f,3.56f,2.086f,-8.811f,5.99f,
  1.337f,-5.739f,4.61f,0.473f,-5.999f,4.37f,2.566f,-5.238f,5.84f,0.449f,-3.024f,5.13f,2.048f,-2.446f,6.18f,359.689f,-3.530f,4.88f,
  354.927f,-14.202f,4.97f,354.402f,-13.070f,5.66f,355.262f,-11.682f,5.89f,356.834f,-11.877f,5.74f,358.188f,-8.988f,5.76f,349.679f,-13.442f,5.20f,
  349.724f,-9.608f,4.99f,349.469f,-9.179f,4.41f,348.838f,-9.082f,4.24f,349.219f,-7.721f,4.93f,348.564f,-5.978f,4.22f,346.246f,-7.696f,5.44f,
  350.208f,-5.886f,6.17f,350.618f,-15.046f,5.19f,350.789f,-20.066f,3.96f,351.531f,-20.619f,4.38f,353.320f,-20.918f,4.70f,343.361f,-11.617f,5.80f,
  342.403f,-13.579f,4.05f,343.679f,-15.812f,3.27f,347.340f,-21.184f,3.68f,347.465f,-22.455f,4.71f,346.647f,-23.743f,4.48f,3.307f,15.187f,2.83f,
  2.041f,29.149f,2.07f,359.774f,6.904f,4.03f,354.851f,5.784f,4.13f,352.037f,6.395f,4.27f,350.057f,5.403f,5.05f,345.965f,3.824f,4.48f,
  356.611f,3.496f,4.95f,355.558f,1.836f,4.49f,351.702f,1.290f,4.95f,349.016f,3.276f,3.70f,10.509f,50.515f,4.80f,11.172f,48.287f,4.48f,
  11.131f,47.861f,5.66f,10.880f,47.038f,4.95f,9.212f,44.477f,5.14f,7.014f,44.400f,5.18f,2.578f,46.072f,5.01f,2.963f,48.149f,6.18f,
  4.432f,51.434f,6.11f,6.057f,52.022f,5.58f,20.886f,-30.929f,5.84f,14.642f,-29.360f,4.30f,15.576f,-31.558f,5.50f,8.433f,-29.548f,5.55f,
  6.991f,-32.989f,4.86f,11.648f,15.491f,5.36f,12.245f,17.013f,5.07f,11.875f,24.297f,4.08f,9.734f,29.404f,4.34f,9.783f,30.891f,3.27f,
  9.214f,33.721f,4.34f,9.342f,35.401f,5.45f,10.287f,39.460f,5.30f,12.443f,41.085f,4.53f,14.118f,38.486f,3.86f,17.355f,35.661f,2.07f,
  17.782f,31.429f,5.15f,17.884f,30.103f,4.51f,19.856f,27.268f,4.74f,18.430f,24.591f,4.67f,16.403f,21.478f,5.33f,14.319f,23.434f,4.40f,
  30.500f,2.764f,3.82f,28.380f,3.179f,4.61f,25.366f,5.486f,4.45f,22.440f,6.161f,4.84f,18.381f,7.595f,5.21f,15.765f,7.881f,4.27f,
  12.140f,7.603f,4.44f,18.555f,-8.023f,5.14f,15.803f,-4.799f,5.40f,13.250f,-1.138f,4.78f,11.821f,5.693f,5.74f,17.183f,5.710f,5.51f,
  19.467f,3.623f,5.13f,19.188f,-2.477f,5.42f,10.809f,-17.998f,2.04f,17.373f,47.246f,4.26f,20.021f,58.232f,4.95f,21.237f,60.253f,2.66f,
  26.458f,63.942f,5.63f,28.573f,63.677f,3.35f,29.878f,64.627f,5.29f,37.293f,67.389f,4.46f,17.128f,58.268f,5.77f,14.158f,60.718f,2.15f,
  11.537f,58.017f,3.46f,10.094f,56.549f,2.24f,9.232f,53.900f,3.69f,1.925f,59.215f,2.28f,8.247f,62.933f,4.17f,31.718f,23.515f,2.01f,
  28.623f,20.847f,2.64f,28.352f,19.330f,3.88f,34.748f,23.171f,6.45f,34.538f,19.901f,5.58f,32.623f,19.510f,5.68f,33.137f,21.210f,5.23f,
  37.610f,25.261f,5.88f,25.740f,20.513f,5.24f,23.693f,18.484f,5.90f,22.448f,18.357f,6.01f,22.861f,15.347f,3.62f,24.889f,16.410f,5.98f,
  29.950f,-21.069f,3.99f,26.664f,-16.246f,3.49f,27.851f,-10.321f,3.74f,21.034f,-8.109f,3.60f,17.068f,-10.132f,3.46f,11.050f,-10.568f,4.77f,
  38.466f,-28.232f,4.96f,35.558f,-23.815f,5.19f,31.117f,-29.300f,4.68f,33.316f,44.237f,4.84f,30.954f,42.348f,2.10f,24.283f,41.543f,4.10f,
  21.730f,45.446f,4.83f,24.465f,48.669f,3.59f,25.901f,50.694f,4.01f,32.051f,37.875f,4.78f,32.320f,35.001f,3.00f,34.309f,33.866f,4.03f,
  38.004f,36.141f,5.15f,40.862f,27.711f,4.65f,41.916f,29.293f,4.52f,42.469f,27.303f,3.61f,45.574f,4.118f,2.54f,44.925f,8.913f,4.71f,
  41.131f,10.125f,4.27f,36.197f,10.615f,5.48f,33.260f,8.852f,4.36f,37.025f,8.465f,4.30f,38.979f,5.601f,4.87f,40.878f,3.288f,3.47f,
  39.865f,0.329f,4.08f,38.049f,-1.023f,5.36f,35.558f,-0.868f,5.42f,35.491f,0.398f,5.29f,34.374f,1.624f,5.60f,52.711f,12.937f,4.14f,
  52.606f,11.342f,5.14f,51.773f,9.746f,3.73f,51.230f,9.058f,3.61f,42.663f,55.900f,3.77f,43.566f,52.764f,3.93f,46.562f,49.646f,4.05f,
  47.286f,44.909f,3.79f,47.041f,40.956f,2.09f,46.234f,38.879f,3.32f,42.556f,38.358f,4.22f,40.570f,40.260f,4.91f,46.394f,56.680f,4.77f,
  46.199f,53.508f,2.91f,51.067f,49.871f,1.79f,54.111f,48.203f,4.32f,55.718f,47.803f,3.01f,62.155f,47.725f,3.96f,63.722f,48.416f,4.12f,
  64.535f,50.316f,4.60f,64.190f,53.612f,5.20f,44.678f,39.677f,4.68f,61.653f,50.364f,4.25f,56.294f,24.482f,4.30f,56.193f,24.305f,5.45f,
  56.448f,24.384f,3.87f,56.210f,24.130f,3.72f,56.863f,24.121f,2.85f,56.573f,23.964f,4.14f,57.284f,24.070f,3.62f,66.965f,21.635f,5.72f,
  61.138f,22.103f,4.36f,61.877f,15.171f,6.02f,58.237f,17.338f,5.97f,60.153f,17.308f,6.31f,61.995f,17.344f,5.89f,63.112f,17.285f,6.09f,
  56.306f,42.578f,3.77f,59.458f,40.019f,2.90f,59.740f,35.790f,3.98f,58.531f,31.887f,2.84f,56.076f,32.292f,3.84f,55.592f,33.967f,4.97f,
  57.394f,33.091f,5.14f,75.492f,43.824f,3.03f,76.614f,41.259f,3.18f,75.615f,41.084f,3.69f,69.175f,41.272f,4.25f,40.394f,-14.569f,5.98f,
  38.052f,-15.192f,4.74f,39.827f,-11.787f,4.83f,36.492f,-12.286f,4.88f,40.102f,-9.424f,5.79f,35.450f,-10.748f,5.43f,62.325f,-16.386f,5.45f,
  59.485f,-13.468f,2.97f,56.515f,-12.123f,4.43f,55.846f,-10.031f,3.52f,53.590f,-9.465f,3.72f,48.960f,-8.836f,4.80f,44.078f,-8.819f,3.89f,
  41.033f,-13.856f,4.24f,41.150f,-18.586f,4.47f,42.775f,-20.998f,4.76f,45.655f,-23.604f,4.08f,49.859f,-21.769f,3.70f,53.430f,-21.623f,4.26f,
  56.775f,-23.059f,4.22f,58.416f,-24.610f,4.64f,59.977f,-24.022f,4.62f,67.113f,19.194f,3.53f,66.331f,17.940f,4.30f,65.693f,17.553f,3.77f,
  64.905f,15.636f,3.65f,68.957f,16.578f,0.87f,67.125f,15.880f,3.40f,66.543f,15.630f,4.48f,60.173f,12.495f,3.41f,69.788f,15.925f,4.67f,
  66.534f,22.830f,4.28f,66.301f,22.310f,4.21f,69.939f,-1.049f,6.11f,71.369f,-3.250f,4.01f,67.971f,-0.041f,4.91f,69.079f,-3.351f,3.93f,
  65.938f,-3.725f,5.17f,64.634f,-6.418f,4.43f,62.962f,-6.867f,4.04f,63.602f,-10.198f,4.87f,65.100f,27.379f,4.97f,70.317f,28.626f,5.73f,
  73.178f,27.910f,5.97f,74.529f,25.068f,5.79f,77.027f,24.267f,5.50f,79.812f,22.126f,4.96f,75.747f,21.605f,4.62f,72.813f,18.852f,5.08f,
  69.571f,20.688f,5.85f,70.562f,22.964f,4.27f,69.747f,-12.117f,4.99f,72.471f,-13.707f,6.27f,72.531f,-16.236f,5.03f,71.852f,-16.996f,5.49f,
  71.013f,-18.660f,5.53f,70.100f,-19.636f,4.32f,67.148f,-19.418f,5.95f,67.279f,-13.048f,5.61f,69.574f,-14.240f,3.86f,74.247f,33.173f,2.69f,
  79.133f,46.152f,0.08f,89.911f,44.948f,1.90f,89.911f,37.239f,2.65f,81.563f,28.670f,1.65f,83.183f,32.193f,4.71f,81.784f,30.210f,5.69f,
  81.165f,31.145f,5.94f,87.743f,37.322f,4.72f,87.869f,39.148f,3.97f,87.307f,39.190f,4.51f,73.170f,36.705f,4.79f,74.793f,37.926f,4.93f,
  72.495f,37.474f,4.89f,80.004f,33.959f,5.05f,79.077f,34.297f,5.99f,79.744f,33.759f,5.38f,79.531f,33.430f,4.54f,78.863f,32.683f,5.01f,
  79.540f,40.339f,4.69f,78.366f,38.511f,4.82f,81.163f,37.390f,5.02f,84.410f,21.149f,2.97f,77.427f,15.603f,4.81f,76.135f,15.415f,4.65f,
  74.122f,13.531f,4.06f,73.701f,11.421f,5.18f,73.709f,10.197f,4.64f,72.653f,8.912f,4.35f,72.291f,6.957f,3.19f,72.803f,5.605f,3.68f,
  73.562f,2.441f,3.71f,70.056f,-24.489f,5.56f,75.342f,-20.046f,4.91f,73.777f,-16.737f,5.71f,73.823f,-16.431f,5.71f,74.436f,-14.228f,6.15f,
  74.967f,-12.506f,4.78f,74.953f,-10.214f,5.39f,71.021f,-8.504f,5.78f,68.479f,-6.737f,5.71f,68.425f,-29.668f,4.49f,68.908f,-30.558f,3.81f,
  65.977f,-34.038f,3.97f,64.446f,-33.796f,3.55f,57.115f,-37.616f,4.30f,57.386f,-36.180f,4.17f,55.750f,-37.288f,4.59f,54.272f,-40.269f,4.57f,
  52.562f,-41.308f,6.12f,48.477f,-43.332f,4.26f,44.591f,-40.314f,2.88f,40.103f,-39.846f,4.11f,39.904f,-42.886f,4.74f,83.785f,9.935f,3.39f,
  83.706f,9.490f,4.39f,84.191f,9.401f,4.09f,88.330f,27.616f,4.56f,89.500f,25.955f,4.81f,91.031f,23.307f,4.16f,90.979f,20.139f,4.64f,
  104.416f,45.095f,4.90f,102.703f,41.831f,4.99f,99.833f,42.509f,4.80f,99.138f,38.453f,5.40f,93.088f,32.695f,5.78f,93.874f,29.593f,4.32f,
  87.743f,27.964f,5.60f,84.930f,25.906f,5.18f,83.857f,24.047f,5.37f,88.783f,7.403f,0.45f,85.188f,-1.943f,1.74f,84.053f,-1.202f,1.69f,
  83.001f,-0.299f,2.25f,78.634f,-8.201f,0.18f,81.286f,6.354f,1.64f,86.939f,-9.669f,2.07f,83.845f,-4.836f,4.58f,83.845f,-5.417f,4.98f,
  83.857f,-5.910f,2.75f,77.287f,-8.753f,4.25f,75.360f,-7.176f,4.80f,76.993f,-5.059f,2.78f,79.407f,-6.841f,3.59f,76.358f,-22.345f,3.19f,
  78.216f,-16.200f,3.29f,78.065f,-11.858f,4.45f,78.312f,-12.941f,4.36f,79.895f,-13.175f,4.29f,80.000f,-12.318f,5.29f,83.181f,-17.823f,2.58f,
  82.063f,-20.728f,2.81f,86.230f,-22.315f,3.59f,87.742f,-20.645f,3.76f,84.452f,-28.705f,5.28f,109.541f,16.554f,3.58f,106.029f,20.571f,4.01f,
  102.890f,21.774f,5.28f,100.985f,25.136f,3.06f,97.243f,20.217f,4.13f,95.718f,22.553f,2.87f,99.429f,16.423f,1.93f,101.365f,12.965f,3.35f,
  93.744f,22.510f,3.31f,115.085f,5.599f,0.40f,111.806f,8.303f,2.89f,112.062f,8.922f,4.33f,116.585f,28.043f,1.16f,113.737f,31.942f,1.58f,
  112.210f,31.715f,4.16f,98.800f,28.027f,5.26f,99.604f,28.990f,5.76f,101.193f,28.979f,5.42f,103.198f,33.978f,3.60f,107.798f,30.263f,4.41f,
  111.481f,27.829f,3.78f,113.997f,26.935f,4.06f,116.121f,24.418f,3.57f,114.807f,34.627f,4.89f,120.891f,27.807f,4.94f,94.276f,9.964f,5.39f,
  97.945f,11.534f,5.22f,98.406f,14.187f,5.57f,94.080f,12.205f,5.04f,121.932f,21.606f,5.30f,118.922f,19.898f,5.38f,116.560f,18.529f,4.89f,
  114.868f,17.674f,5.04f,103.635f,13.206f,4.73f,105.571f,15.345f,5.78f,107.094f,15.968f,5.47f,108.337f,16.174f,5.07f,99.475f,-18.234f,4.42f,
  99.147f,-19.231f,3.95f,98.759f,-22.971f,4.54f,97.965f,-23.421f,4.34f,92.243f,-22.408f,5.49f,91.919f,-19.186f,5.28f,91.248f,-16.483f,4.92f,
  91.545f,-14.940f,4.67f,93.932f,-13.714f,5.00f,95.355f,-11.774f,5.48f,96.063f,-11.518f,5.21f,97.830f,-12.385f,5.16f,99.818f,-14.143f,4.82f,
  95.676f,-17.956f,1.98f,97.048f,-32.589f,4.47f,95.539f,-33.417f,3.85f,89.385f,-35.287f,4.36f,87.715f,-35.914f,3.12f,84.912f,-34.065f,2.65f,
  82.791f,-35.458f,3.86f,107.966f,-0.495f,4.15f,120.577f,2.296f,4.39f,101.493f,-16.274f,-1.44f,117.326f,-24.860f,3.34f,111.025f,-29.306f,2.45f,
  102.464f,-32.510f,3.50f,104.655f,-28.973f,1.50f,105.432f,-27.937f,3.49f,107.099f,-26.394f,1.83f,109.678f,-24.956f,4.37f,113.291f,-24.718f,5.84f,
  105.757f,-23.835f,3.02f,95.976f,-52.704f,-0.62f,127.922f,18.115f,5.33f,128.194f,20.457f,5.33f,130.863f,21.483f,4.66f,131.178f,18.237f,3.94f,
  131.683f,28.776f,4.03f,133.921f,27.940f,5.23f,137.013f,29.653f,5.42f,134.905f,32.431f,5.23f,122.155f,-2.982f,4.36f,119.954f,-3.679f,4.93f,
  120.282f,-1.365f,4.69f,118.207f,-5.418f,5.76f,116.491f,-6.739f,5.49f,114.344f,-4.117f,5.14f,112.332f,-1.902f,5.60f,131.826f,-1.901f,5.28f,
  128.519f,-2.159f,5.80f,126.439f,-3.898f,3.91f,128.876f,-7.988f,5.72f,130.922f,-7.233f,4.63f,132.348f,-3.435f,5.30f,132.688f,-27.742f,4.02f,
  120.911f,-40.009f,2.21f,131.513f,-46.043f,3.87f,122.386f,-47.340f,1.75f,136.064f,-47.094f,3.75f,137.011f,-43.438f,2.23f,144.279f,-49.370f,4.34f,
  130.813f,3.399f,4.30f,129.696f,3.347f,4.45f,129.440f,5.706f,4.14f,131.778f,6.433f,3.38f,132.115f,5.849f,4.35f,133.885f,5.940f,3.11f,
  136.500f,5.096f,4.99f,138.550f,2.425f,3.89f,134.606f,11.868f,4.26f,136.944f,10.672f,5.23f,138.823f,14.946f,5.36f,144.947f,-1.120f,3.90f,
  143.000f,-1.183f,4.54f,142.251f,-2.768f,4.59f,142.029f,-6.048f,5.38f,141.902f,-8.671f,1.99f,140.126f,-9.546f,4.80f,139.954f,-11.979f,4.77f,
  135.396f,41.862f,3.96f,136.645f,38.457f,4.56f,139.726f,36.847f,3.82f,140.361f,34.387f,3.14f,144.178f,31.178f,5.57f,145.896f,30.012f,5.64f,
  141.176f,26.200f,4.47f,142.938f,22.982f,4.32f,146.481f,23.778f,2.97f,148.278f,26.027f,3.88f,154.165f,23.420f,3.43f,154.874f,19.897f,2.01f,
  151.834f,16.763f,3.48f,152.185f,11.965f,1.36f,145.340f,9.906f,3.52f,158.205f,9.308f,3.84f,152.006f,10.021f,4.39f,157.028f,36.747f,4.20f,
  156.511f,33.817f,4.72f,159.680f,31.974f,4.68f,163.288f,34.318f,3.79f,143.632f,-5.894f,5.56f,147.803f,-4.233f,6.01f,148.149f,-8.088f,5.07f,
  130.088f,-52.935f,3.60f,131.158f,-54.671f,1.93f,139.286f,-59.280f,2.21f,142.827f,-57.037f,3.16f,140.535f,-55.015f,2.47f,147.863f,-14.839f,4.11f,
  152.721f,-12.318f,3.61f,156.571f,-16.807f,3.83f,148.735f,-18.996f,4.94f,145.086f,-14.325f,5.07f,159.683f,-16.886f,4.91f,147.932f,-46.551f,4.58f,
  142.745f,-40.484f,3.60f,149.758f,-35.885f,5.23f,149.133f,-33.426f,5.83f,164.809f,-33.717f,5.70f,164.145f,-37.093f,4.60f,165.027f,-42.227f,4.37f,
  159.397f,-48.225f,3.84f,155.075f,-47.701f,5.66f,157.997f,-45.067f,5.76f,155.595f,-41.672f,4.82f,153.757f,-42.140f,3.85f,153.870f,-43.093f,5.59f,
  151.553f,-47.351f,5.06f,162.371f,-16.266f,3.11f,165.120f,-18.345f,4.08f,169.882f,-14.853f,3.56f,171.257f,-17.685f,4.06f,176.180f,-18.342f,4.71f,
  165.842f,-11.265f,5.51f,180.133f,-10.271f,5.54f,174.192f,-9.803f,4.70f,171.161f,-10.868f,4.81f,162.428f,-9.843f,5.85f,159.036f,-11.987f,5.71f,
  174.630f,-13.248f,5.48f,167.913f,-22.790f,4.46f,168.002f,-26.812f,6.43f,173.131f,-26.754f,6.15f,174.740f,-24.634f,6.40f,166.411f,-27.291f,4.92f,
  178.916f,-28.467f,5.93f,175.411f,-29.278f,6.44f,173.077f,-29.313f,4.93f,167.213f,-28.073f,5.43f,159.353f,-27.420f,4.87f,161.658f,-49.401f,2.69f,
  168.193f,-49.113f,5.37f,170.274f,-54.490f,3.90f,182.117f,-50.720f,2.58f,177.823f,-45.170f,4.47f,188.596f,-23.377f,2.65f,187.545f,-16.465f,2.94f,
  184.012f,-17.550f,2.58f,182.559f,-22.624f,3.02f,185.183f,-22.206f,5.20f,188.177f,-16.175f,4.30f,182.792f,-23.595f,5.45f,177.202f,-26.746f,5.10f,
  178.658f,-25.740f,5.26f,178.252f,-33.909f,4.29f,175.073f,-34.744f,4.70f,173.339f,-31.843f,3.54f,173.238f,-31.089f,5.13f,185.916f,-35.410f,5.32f,
  192.684f,-33.994f,4.90f,196.711f,-35.831f,5.63f,198.188f,-37.820f,4.85f,193.326f,-40.171f,4.25f,189.989f,-39.978f,4.63f,187.107f,-39.036f,5.45f,
  158.097f,-53.791f,4.89f,159.839f,-55.605f,4.29f,161.754f,-56.757f,5.14f,163.133f,-57.242f,5.26f,167.151f,-58.976f,3.93f,172.945f,-59.442f,5.07f,
  183.812f,-58.745f,2.79f,185.732f,-57.672f,5.38f,187.773f,-57.018f,1.59f,186.881f,-58.994f,5.38f,176.645f,-61.173f,4.11f,168.155f,-60.318f,4.59f,
  163.318f,-58.867f,3.78f,158.907f,-57.558f,4.45f,156.712f,-54.879f,5.58f,154.912f,-55.031f,4.59f,155.240f,-56.044f,4.50f,156.856f,-57.640f,4.65f,
  156.979f,-58.740f,3.81f,159.698f,-59.183f,4.69f,160.896f,-60.568f,4.58f,166.664f,-62.428f,4.62f,173.972f,-63.017f,3.11f,185.466f,-60.434f,3.59f,
  191.965f,-59.684f,1.25f,154.289f,-61.335f,3.39f,158.019f,-61.689f,3.30f,160.755f,-64.399f,2.74f,177.436f,-63.790f,4.30f,180.879f,-63.315f,4.32f,
  184.637f,-63.999f,4.06f,186.678f,-63.094f,0.77f,
};
constexpr int kStarCount = 1460;

const uint16_t kVerts[] = {
  0,1,2,3,4,5,6,7,8,9,9,10,10,11,12,13,14,15,16,17,18,19,20,21,
  22,23,24,25,26,27,28,29,30,65535,31,29,32,33,33,34,34,35,36,37,38,35,39,40,
  41,65535,39,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,52,58,58,59,60,
  65535,59,61,65535,59,62,65535,59,63,64,65,66,64,67,68,69,70,65,69,65535,68,65,67,71,
  72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
  96,97,98,95,65535,98,99,100,97,101,102,103,104,105,106,101,65535,105,107,108,109,110,111,112,
  113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,131,132,133,134,135,
  136,137,132,138,138,139,140,141,142,139,143,143,144,145,146,147,147,148,149,150,151,148,152,153,
  154,155,156,157,158,159,159,160,161,65535,160,162,163,164,165,166,167,168,169,170,171,172,173,173,
  174,175,176,174,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,65535,192,190,193,194,
  194,195,195,196,196,197,197,198,198,199,200,201,202,203,65535,202,204,205,206,207,208,209,210,211,
  65535,208,212,213,214,65535,207,215,216,217,65535,209,212,215,65535,210,213,218,216,65535,211,219,220,214,
  217,65535,220,217,65535,206,215,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,
  238,239,240,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
  256,257,258,259,260,261,262,263,264,265,266,267,268,263,269,270,271,272,273,274,65535,273,275,65535,
  273,276,277,278,279,280,281,282,283,283,284,284,285,286,287,288,289,290,291,292,293,294,295,296,
  297,298,299,300,301,302,303,304,305,306,307,308,300,309,310,311,312,313,314,315,316,317,318,319,
  320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,334,335,336,337,338,339,340,341,342,
  343,344,345,346,347,348,349,350,351,348,352,353,354,355,356,357,358,359,360,361,362,363,364,365,
  366,367,368,369,370,371,372,373,374,375,65535,373,376,377,377,378,379,380,381,382,383,384,385,386,
  387,388,389,390,391,392,393,394,395,396,397,398,399,399,400,400,401,402,403,404,405,406,407,408,
  409,410,411,412,407,413,414,415,416,417,418,416,419,420,421,421,422,423,424,425,426,427,428,429,
  430,65535,428,431,65535,428,432,65535,422,433,434,435,65535,434,436,437,65535,434,438,439,440,441,442,443,
  444,65535,443,445,65535,442,446,447,448,65535,441,425,449,449,450,451,452,453,65535,454,450,454,455,456,
  456,457,458,459,460,461,462,463,464,465,466,467,468,468,469,470,471,472,473,474,475,65535,474,476,
  65535,477,478,65535,477,479,65535,480,481,65535,480,482,65535,483,484,65535,483,485,65535,474,477,480,483,474,
  486,487,488,489,490,491,492,493,65535,492,494,65535,487,495,496,497,498,499,500,496,501,502,503,504,
  505,505,506,507,508,509,510,511,512,513,513,514,515,516,517,518,519,520,521,522,523,524,525,526,
  527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,535,549,
  550,551,552,553,554,555,556,557,558,551,559,560,561,562,563,564,561,565,566,567,568,569,570,565,
  65535,571,570,65535,572,566,65535,567,573,65535,574,569,575,575,576,577,578,579,576,580,65535,576,581,582,
  583,584,65535,585,586,587,65535,588,589,590,65535,589,586,583,591,592,593,65535,592,594,595,65535,594,596,
  65535,594,597,598,65535,597,599,600,601,602,603,604,605,606,607,608,65535,607,609,610,611,612,613,614,
  615,616,617,65535,616,618,619,620,621,622,623,624,625,626,627,628,629,65535,628,630,631,632,633,634,
  635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,649,65535,652,653,654,655,
  656,657,654,65535,656,658,659,660,661,662,663,664,665,666,667,659,668,669,670,671,672,673,674,675,
  65535,672,676,677,678,679,680,681,681,682,683,684,684,685,685,686,686,687,688,689,689,690,690,691,
  692,693,693,694,694,695,696,697,698,699,700,701,702,703,704,705,706,707,708,709,710,711,712,713,
  714,715,716,717,718,719,720,721,709,722,723,724,725,726,727,728,729,730,731,65535,730,732,733,734,
  65535,733,735,65535,733,732,736,737,65535,736,738,65535,736,739,65535,736,740,741,742,65535,741,743,744,745,
  746,747,748,749,750,751,752,753,754,751,755,756,757,758,759,760,761,762,763,764,765,65535,764,766,
  65535,764,767,65535,764,768,769,770,771,772,773,774,775,776,777,778,779,780,781,782,773,783,784,785,
  786,787,65535,786,788,65535,786,789,789,790,788,791,787,792,793,794,795,65535,794,796,797,65535,796,798,
  799,800,801,802,803,804,801,65535,803,805,806,807,808,809,810,811,812,809,813,814,815,816,816,817,
  817,818,819,820,65535,819,821,822,823,824,825,826,827,828,65535,819,829,65535,830,829,831,65535,829,832,
  833,834,835,836,837,838,839,836,840,841,842,65535,841,843,844,65535,843,845,65535,843,846,847,65535,846,
  848,849,850,851,852,853,854,855,856,857,858,859,65535,853,860,861,862,65535,861,863,864,865,65535,864,
  866,65535,864,867,868,65535,867,869,65535,855,870,871,872,873,874,65535,875,876,877,878,879,880,881,65535,
  880,882,65535,875,883,884,885,886,65535,875,887,888,889,890,891,892,893,894,895,896,897,898,899,900,
  901,902,903,904,905,906,907,908,909,910,911,912,913,904,914,915,916,65535,915,917,918,919,920,921,
  922,923,924,925,926,927,928,929,930,931,932,933,934,935,921,936,937,938,939,940,941,942,943,944,
  945,946,947,948,949,943,950,950,951,951,952,953,954,955,956,957,958,958,959,960,961,962,65535,963,
  959,65535,963,960,65535,963,961,65535,963,962,964,964,965,966,967,968,969,970,971,968,972,973,974,975,
  976,977,974,978,979,980,981,982,983,984,985,986,987,988,989,990,991,992,65535,988,993,994,995,996,
  997,998,999,1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1011,1012,1013,1014,1015,1016,1017,1018,1019,1020,
  1021,1022,1023,1024,1025,1026,1027,1028,1029,1030,1031,1032,1033,1034,1034,1035,1035,1036,1037,1038,1039,1040,1041,1042,
  1043,1043,1044,1044,1045,1046,65535,1045,1047,65535,1045,1048,65535,1045,1049,1050,1051,1052,1053,1054,1055,1056,1056,1057,
  1058,1059,1060,1061,1062,1063,1064,1065,1066,1067,1068,1069,1070,1071,1072,1073,1074,1075,1076,1077,1078,1079,1080,1081,
  1082,1083,1084,1085,1086,65535,1087,1088,1089,1086,1090,65535,1087,1091,1091,1091,1092,1093,1094,1095,65535,1094,1096,65535,
  1094,1097,1098,1099,1100,1101,1102,1103,1104,1105,1106,1107,1108,1109,1110,1111,1108,1112,1113,1114,1115,1116,1117,1118,
  1119,1120,1112,1121,1122,1123,1124,1125,1126,1127,65535,1126,1128,1129,1130,65535,1129,1131,1132,1133,1134,1135,1136,1137,
  65535,1138,1136,1139,1140,1141,65535,1140,1142,1143,1143,1144,1145,1146,1147,1148,1149,1150,1151,1152,1153,1154,1155,1156,
  1157,1158,1159,1160,1161,1162,1163,1164,1165,1166,1167,1168,1169,1170,1171,1172,1173,1174,1175,1176,65535,1175,1177,1178,
  1179,1180,1181,1182,1183,1184,1185,1186,1187,1188,1189,1190,1191,1192,1193,1194,1195,65535,1194,1196,65535,1192,1197,1198,
  1199,1200,1201,1202,1203,1204,1205,1206,1207,1208,1209,1210,1211,1212,1213,1214,1215,1215,1216,1217,1218,1219,65535,1218,
  1220,1221,65535,1220,1222,1217,65535,1222,1223,65535,1221,1224,1224,1224,1225,1226,1227,1228,1229,1230,1231,1232,1233,1234,
  1235,1236,1237,1238,1239,1239,1240,1240,1241,1242,1243,1244,1241,1245,1246,1247,1248,1249,1250,1251,1252,1253,1254,1255,
  1256,1257,1258,1259,1260,1261,1262,1263,1264,1265,1253,1266,1266,1267,1268,1269,1270,1271,1272,1273,1274,1275,1275,1276,
  1277,1278,1279,1280,1281,1282,1283,1276,65535,1284,1281,1277,1285,1285,1286,1287,1288,1289,1286,1290,1291,1292,1293,1290,
  1294,1295,1296,65535,1295,1297,1298,1299,1300,1301,1302,1303,1304,65535,1305,1306,1301,1307,1307,1308,1309,1310,65535,1309,
  1311,1312,65535,1311,1313,1314,1315,1316,1317,1318,1319,1320,1321,1322,1323,65535,1322,1324,1323,1325,1326,1327,1328,1329,
  1330,1331,1332,1333,1334,1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,65535,1345,1347,65535,1345,1348,1349,
  1350,1351,1352,1353,1354,65535,1353,1355,1356,1357,1358,1359,1360,1357,1361,1362,1363,1364,65535,1361,1364,65535,1361,1365,
  65535,1363,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,1367,1381,1382,1383,1384,1385,65535,
  1383,1386,65535,1387,1388,1389,1386,1390,1391,65535,1389,1392,65535,1393,1382,65535,1393,1384,65535,1393,1394,1395,1396,65535,
  1394,1397,65535,1398,1399,1400,1401,1397,1402,1403,1404,1405,1406,1407,1408,1409,1410,1411,1408,65535,1410,1412,65535,1409,
  1413,65535,1411,1414,1412,1412,1413,1413,1414,1414,1415,1416,1417,1418,1419,1420,1421,1422,1423,1424,1425,1426,1427,1421,
  1428,1429,1430,1431,1432,1433,1434,1435,1436,65535,1437,1438,1439,1440,1441,1442,65535,1443,1444,1445,1446,1447,1448,1449,
  1450,1451,1452,65535,1453,1454,1455,1456,1457,1458,1459,
};

struct AstRec { const char *id; const char *pinyin; const char *native; const char *english;
                uint16_t first; uint16_t words; uint16_t stars; };
const AstRec kAst[] = {
  {"P01","Bei Ji","北极","Northern Pole",0,5,5},
  {"P02","Si Fu","四辅","Four Advisors",5,4,4},
  {"P03","Tian Yi","天一","Celestial One",9,2,2},
  {"P04","Tai Yi","太一","Supreme One",11,2,2},
  {"P05","Zi Wei Dong Yuan","紫微东垣","Purple Forbidden Palace East Wall",13,8,8},
  {"P06","Zi Wei Xi Yuan","紫微西垣","Purple Forbidden Palace West Wall",21,7,7},
  {"P07","Yin De","阴德","Hidden Virtue",28,2,2},
  {"P08","Shang Shu","尚书","Royal Secretary",30,7,6},
  {"P09","Nü Shi","女史","Female Protocol",37,2,2},
  {"P10","Zhu Shi","柱史","Official of Royal Archives",39,2,2},
  {"P11","Nü Yu","女御","Maids-in-waiting",41,5,5},
  {"P12","Tian Zhu","天柱","Celestial Pillar",46,7,6},
  {"P13","Da Li","大理","Chief Judge",53,2,2},
  {"P14","Gou Chen","钩陈","Curved Array",55,6,6},
  {"P15","Liu Jia","六甲","Six Jia",61,7,7},
  {"P16","Tian Huang Da Di","天皇大帝","Great Emperor of Heaven",68,2,2},
  {"P17","Wu Di Nei Zuo","五帝内座","Inner Seats of the Five Emperors",70,11,8},
  {"P18","Hua Gai","华盖","Canopy of the Emperor",81,14,13},
  {"P19","Gang","杠(附华盖)","Canopy Support (Adjunct to the Canopy)",95,9,9},
  {"P20","Chuan She","传舍","Guest House",104,9,9},
  {"P21","Nei Jie","内阶","Inner Steps",113,6,6},
  {"P22","Tian Chu","天厨","Celestial Kitchen",119,10,9},
  {"P23","Ba Gu","八谷","Eight Kinds of Crops",129,11,10},
  {"P24","Tian Bang","天棓","Celestial Flail",140,5,5},
  {"P25","Tian Chuang","天床","Celestial Bed",145,6,6},
  {"P26","Nei Chu","内厨","Inner Kitchen",151,2,2},
  {"P27","Wen Chang","文昌","Administrative Center",153,6,6},
  {"P28","San Gong (Wuxian)","三公(巫咸)","Three Excellencies (Wuxian)",159,3,3},
  {"P29","Tai Zun","太尊","Royals",162,2,2},
  {"P30","Tian Lao","天牢","Celestial Prison",164,7,7},
  {"P31","Tai Yang Shou","太阳守","Guard of the Sun",171,2,2},
  {"P32","Shi","势","Eunuch",173,5,5},
  {"P33","Xiang","相","Prime Minister",178,2,2},
  {"P34","San gong(Gan Shi)","三公(甘氏)","Three Excellencies (Gan School)",180,3,3},
  {"P35","Xuan Ge","玄戈","Sombre Lance",183,2,2},
  {"P36","Tian Li","天理","Judge for Nobility",185,5,5},
  {"P37","Bei Dou","北斗","Northern Dipper",190,7,7},
  {"P38","Fu","辅(附北斗)","Assistant (Vassal of Northern Dipper)",197,2,2},
  {"P39","Tian Qiang","天枪","Celestial Spear",199,5,4},
  {"S01","Tai Wei Zuo Yuan","太微左垣","Supreme Palace Left Wall",204,5,5},
  {"S02","Tai Wei You Yuan","太微右垣","Supreme Palace Right Wall",209,5,5},
  {"S03","Ye Zhe","谒者","Usher to the Court",214,2,2},
  {"S04","San Gong Nei Zuo","三公内座","Inner Seats of Three Excellencies",216,4,4},
  {"S05","Jiu Qing Nei Zuo","九卿内座","Inner Seats of Nine Senior Officers",220,3,3},
  {"S06","Nei Wu Zhu Hou(Tai Wei Yuan)","内五诸侯","Inner Five Lords",223,5,5},
  {"S07","Nei Ping","内屏","Inner Screen",228,4,4},
  {"S08","Wu Di Zuo","五帝坐","Seats of the Five Emperors",232,7,6},
  {"S09","Xing Chen","幸臣","Officer of Honour",239,2,2},
  {"S10","Tai Zi","太子","Crown Prince",241,2,2},
  {"S11","Cong Guan(Tai Wei)","从官(太微)","Retinue (In Supreme Palace)",243,2,2},
  {"S12","Lang Jiang","郎将","Captain of the Bodyguards",245,2,2},
  {"S13","Hu Ben","虎贲","Emperor's Bodyguard",247,2,2},
  {"S14","Chang Chen","常陈","Royal Guards",249,9,8},
  {"S15","Lang Wei","郎位","Officers of the Imperial Guard",258,37,30},
  {"S16","Ming Tang","明堂","The Hall of Glory",295,3,3},
  {"S17","Ling Tai","灵台","Astronomical Observatory",298,3,3},
  {"S18","Shao Wei","少微","Junior Officers",301,4,4},
  {"S19","Chang Yuan","长垣","Long Wall",305,4,4},
  {"S20","San Tai","三台(上台)","Three Steps (Upper Step)",309,2,2},
  {"S21","San Tai","三台(中台)","Three Steps (Middle Step)",311,2,2},
  {"S22","San Tai","三台(下台)","Three Steps (Lower Step)",313,2,2},
  {"S23","San Tai","三台","Three Steps",315,6,6},
  {"H01","Tian Shi Zuo Yuan","天市左垣","Heavenly Market Left Wall",321,11,11},
  {"H02","Tian Shi You Yuan","天市右垣","Heavenly Market Right Wall",332,11,11},
  {"H03","Shi Lou","市楼","Municipal Office",343,7,7},
  {"H04","Che Si","车肆","Commodity Market",350,2,2},
  {"H05","Zong Zheng","宗正","Official for the Royal Clan",352,2,2},
  {"H06","Zong Ren","宗人","Official of Religious Ceremonies",354,8,6},
  {"H07","Zong","宗","Patriarchal Clan",362,2,2},
  {"H08","Bo Du","帛度","Textile Ruler",364,2,2},
  {"H09","Tu Si","屠肆","Butcher's Shops",366,2,2},
  {"H10","Hou","候","Astrologer",368,2,2},
  {"H11","Di Zuo","帝坐(天市)","Emperor's Seat",370,2,2},
  {"H12","Huan Zhe","宦者","Eunuch Official",372,4,4},
  {"H13","Lie Si","列肆","Jewel Market",376,2,2},
  {"H14","Dou","斗","Dipper for Liquids",378,5,5},
  {"H15","Hu","斛","Dipper for Solids",383,4,4},
  {"H16","Guan Suo","贯索","Coiled Thong",387,10,10},
  {"H17","Qi Gong","七公","Seven Excellencies",397,7,7},
  {"H18","Tian Ji","天纪","Celestial Discipline",404,9,9},
  {"H19","Nü Chuang","女床","Woman's Bed",413,3,3},
  {"01A","Jiao Xiu","角","Horn",416,2,2},
  {"01B","Ping Dao","平道","Flat Road",418,2,2},
  {"01C","Tian Tian(Jiao Xiu)","天田(角宿)","Celestial Farmland (In Horn Mansion)",420,2,2},
  {"01D","Jin Xian","进贤","Recommending Virtuous Men",422,2,2},
  {"01E","Zhou Ding","周鼎","Tripod of the Zhou",424,3,3},
  {"01F","Tian Men","天门","Celestial Gate",427,2,2},
  {"01G","Ping","平","Judging",429,2,2},
  {"01H","Ku Lou","库楼","Arsenal",431,11,11},
  {"01I","Heng","衡","Railings",442,4,4},
  {"01J","Nan Men","南门","Southern Gate",446,2,2},
  {"01K","Zhu(Jiao Xiu)","柱(库楼)","Pillars (In Arsenal)",448,3,3},
  {"01L","Zhu(Jiao Xiu)","柱(库楼)","Pillars (In Arsenal)",451,3,3},
  {"01M","Zhu(Jiao Xiu)","柱(库楼)","Pillars (In Arsenal)",454,3,3},
  {"01N","Zhu(Jiao Xiu)","柱(库楼)","Pillars (In Arsenal)",457,3,3},
  {"01O","Zhu(Jiao Xiu)","柱(库楼)","Pillars (In Arsenal)",460,3,3},
  {"02A","Kang","亢","Neck",463,6,5},
  {"02B","Da Jiao","大角","Great Horn",469,2,2},
  {"02C","Zhe Wei","折威","Executions",471,7,7},
  {"02D","Zuo She Ti","左摄提","Left Conductor",478,3,3},
  {"02E","You She Ti","右摄提","Right Conductor",481,3,3},
  {"02F","Dun Wan","顿顽","Trials",484,2,2},
  {"02G","Yang Men","阳门","Gate of Yang",486,2,2},
  {"03A","Di","氐","Root",488,4,4},
  {"03B","Tian Ru","天乳","Celestial Milk",492,2,2},
  {"03C","Zhao Yao","招摇","Twinkling Indicator",494,2,2},
  {"03D","Geng He","梗河","Celestial Lance",496,3,3},
  {"03E","Di Zuo (Da Jiao)","帝坐(大角)","Seat of the Emperor (At Great Horn)",499,3,3},
  {"03F","Kang Chi","亢池","Boats and Lake",502,7,7},
  {"03G","Zhen Che","阵车","Battle Chariots",509,3,3},
  {"03H","Che Qi","车骑","Chariots and Cavalry",512,4,4},
  {"03I","Tian Fu","天辐","Celestial Spokes",516,2,2},
  {"03J","Qi Zhen Jiang Jun","骑阵将军","Chariots and Cavalry General",518,2,2},
  {"03K","Qi Guan","骑官","Imperial Guards",520,44,36},
  {"03L","Tian Ku","天库","Celestial Arsenal",564,2,2},
  {"04A","Fang","房","Room",566,7,6},
  {"04B","Gou Qian","钩钤(附房)","Lock (Adjunct of Room)",573,2,2},
  {"04C","Jian Bi","键闭","Door Bolt",575,2,2},
  {"04D","Fa","罚","Punishment",577,3,3},
  {"04E","Dong Xian","东咸","Eastern Door",580,4,4},
  {"04F","Xi Xian","西咸","Western Door",584,4,4},
  {"04G","Ri","日","Solar Star",588,2,2},
  {"04H","Cong Guan(Fang Xiu)","从官(房宿)","Retinue (In Room Mansion)",590,2,2},
  {"05A","Xin","心","Heart",592,3,3},
  {"05B","Ji Zu","积卒","Group of Soldiers",595,29,21},
  {"06A","Wei","尾","Tail",624,14,12},
  {"06C","Gui","龟","Tortoise",638,6,6},
  {"06D","Tian Jiang","天江","Celestial River",644,4,4},
  {"06E","Fu Yue","傅说","Fu Yue",648,2,2},
  {"06F","Yu","鱼","Fish",650,0,0},
  {"07A","Ji","箕","Winnowing Basket",650,4,4},
  {"07B","Chu","杵","Pestle",654,3,3},
  {"07C","Kang","糠","Chaff",657,2,2},
  {"08A","Nan Dou","南斗","Southern Dipper",659,6,6},
  {"08B","Jian","建","Banner",665,6,6},
  {"08C","Tian Bian","天弁","Market Officer",671,9,9},
  {"08D","Bie","鳖","River Turtle",680,15,15},
  {"08E","Tian Ji","天鸡","Celestial Cock",695,2,2},
  {"08F","Tian Yue","天籥","Celestial Keyhole",697,9,9},
  {"08G","Gou","狗","Dog",706,2,2},
  {"08H","Gou Guo","狗国","Territory of Dog",708,5,5},
  {"08I","Tian Yuan","天渊","Celestial Spring",713,19,15},
  {"08J","Nong Zhang Ren","农丈人","Peasant",732,2,2},
  {"09A","Qian Niu","牵牛","Leading Ox",734,9,8},
  {"09B","Tian Tian(Niu Xiu)","天田(牛宿)","Celestial Farmland (In Ox Mansion)",743,15,12},
  {"09C","Jiu Kan","九坎","Nine Water Wells",758,17,13},
  {"09D","He Gu","河鼓","Drum at the River",775,3,3},
  {"09E","Zhi Nü","织女","Weaving Girl",778,3,3},
  {"09F","Zuo Qi","左旗","Left Flag",781,11,10},
  {"09G","Gu Qi","鼓旗","Flag of the Drum",792,11,10},
  {"09H","Tian Fu","天桴","Celestial Drumstick",803,4,4},
  {"09I","Luo Yan","罗堰","Network of Dykes",807,5,4},
  {"09J","Jian Tai","渐台","Clepsydra Terrace",812,4,4},
  {"09K","Nian Dao","辇道","Imperial Passageway",816,5,5},
  {"10A","Xu Nü","须女","Working Woman",821,4,4},
  {"10B","Li Zhu","离珠","Pearls on Ladies' Wear",825,5,5},
  {"10C","Bai Gua","败瓜","Rotten Gourd",830,8,7},
  {"10D","Hu Gua","瓠瓜","Good Gourd",838,8,7},
  {"10E","Tian Jin","天津","Celestial Ford",846,10,10},
  {"10F","Xi Zhong","奚仲","Xi Zhong",856,4,4},
  {"10G","Fu Kuang","扶筐","Basket for Mulberry Leaves",860,9,8},
  {"10H","Shi Er Guo (Zhao)","赵","Twelve States (Zhao State)",869,2,2},
  {"10I","Shi Er Guo (Yue)","越","Twelve States (Yue State)",871,2,2},
  {"10J","Shi Er Guo (Zhou)","周","Twelve States (Zhou State)",873,2,2},
  {"10K","Shi Er Guo (Qi)","齐","Twelve States (Qi State)",875,2,2},
  {"10L","Shi Er Guo (Zheng)","郑","Twelve States (Zheng State)",877,2,2},
  {"10M","Shi Er Guo (Chu)","楚","Twelve States (Chu State)",879,2,2},
  {"10N","Shi Er Guo (Qin)","秦","Twelve States (Qin State)",881,2,2},
  {"10O","Shi Er Guo (Yan)","燕","Twelve States (Yan State)",883,2,2},
  {"10P","Shi Er Guo (Wei)","魏","Twelve States (Wei State)",885,2,2},
  {"10Q","Shi Er Guo (Dai)","代","Twelve States (Dai State)",887,2,2},
  {"10R","Shi Er Guo (Jin)","晋","Twelve States (Jin State)",889,2,2},
  {"10S","Shi Er Guo (Han)","韩","Twelve States (Han State)",891,2,2},
  {"11A","Xu","虚","Emptiness",893,2,2},
  {"11B","Si Ming","司命","Deified Judge of Life",895,2,2},
  {"11C","Si Lu","司禄","Deified Judge of Rank",897,2,2},
  {"11D","Si wei","司危","Deified Judge of Disaster and Good Fortune",899,2,2},
  {"11E","Si Fei","司非","Deified Judge of Right and Wrong",901,2,2},
  {"11F","Ku","哭","Crying",903,2,2},
  {"11G","Qi","泣","Weeping",905,2,2},
  {"11H","Tian Lei Cheng","天垒城","Celestial Ramparts",907,14,14},
  {"11I","Bai Jiu","败臼","Decayed Mortar",921,4,4},
  {"11J","Li Yu","离瑜","Jade Ornament on Ladies' Wear",925,3,3},
  {"12A","Wei","危","Rooftop",928,6,5},
  {"12B","Fen Mu","坟墓(附危)","Tomb (Adjunct to Rooftop)",934,8,6},
  {"12C","Ren","人","Humans",942,11,8},
  {"12D","Jiu","臼","Mortar",953,6,5},
  {"12E","Nei Chu","内杵","Inner Pestle",959,3,3},
  {"12F","Che Fu","车府","Big Yard for Chariots",962,8,8},
  {"12G","Tian Gou","天钩","Celestial Hook",970,9,9},
  {"12H","Zao Fu","造父","Zaofu",979,11,8},
  {"12I","Xu Liang","虚梁","Temple",990,4,4},
  {"12J","Tian Qian","天钱","Celestial Money",994,11,11},
  {"12K","Gai Wu","盖屋","Roofing",1005,2,2},
  {"13A","Ying Shi","营室","Military Encampment",1007,9,7},
  {"13B","Li Gong","离宫(附营室)","Resting Palace (Adjunct to Military Encampment)",1016,2,2},
  {"13C","Li Gong","离宫(附营室)","Resting Palace (Adjunct to Military Encampment)",1018,2,2},
  {"13D","Li Gong","离宫(附营室)","Resting Palace (Adjunct to Military Encampment)",1020,2,2},
  {"13E","Lei Dian","雷电","Thunder and Lightning",1022,10,8},
  {"13F","Tu Gong Li","土公吏","Official for Materials Supply",1032,2,2},
  {"13G","Lei Bi Zhen","垒壁阵","Line of Ramparts",1034,16,15},
  {"13H","Fu Yue","𫓧钺","Axe",1050,3,3},
  {"13I","Bei Luo Shi Men","北落师门","North Gate of the Military Camp",1053,2,2},
  {"13J","Tian Gang","天纲","Materials for Making Tents",1055,2,2},
  {"13K","Teng She","螣蛇","Flying Serpent",1057,31,27},
  {"13L","Ba Kui","八魁","Net for Catching Birds",1088,17,13},
  {"13M","Yu Lin Jun","羽林军","Palace Guard",1105,63,53},
  {"14A","Dong Bi","东壁","Eastern Wall",1168,2,2},
  {"14B","Pi Li","霹雳","Thunderbolt",1170,5,5},
  {"14C","Yun Yu","云雨","Cloud and Rain",1175,4,4},
  {"14D","Tian Jiu","天厩","Celestial Stable",1179,11,11},
  {"14E","Fu Zhi","𫓧锧","Sickle",1190,7,6},
  {"14F","Tu Gong","土公","Official for Earthworks and Buildings",1197,2,2},
  {"15A","Kui Xiu","奎","Legs",1199,16,16},
  {"15B","Wai Ping","外屏","Outer Fence",1215,7,7},
  {"15C","Tian Hun","天溷","Celestial Pigsty",1222,8,8},
  {"15D","Tu Si Kong(Kui Xiu)","土司空(奎宿)","Master of Constructions (In Legs Mansion)",1230,2,2},
  {"15E","Jun Nan Men","军南门","Southern Military Gate",1232,2,2},
  {"15F","Ge Dao","阁道","Flying Corridor",1234,6,6},
  {"15G","Fu Lu","附路","Auxiliary Road",1240,2,2},
  {"15H","Wang Liang","王良","Wang Liang",1242,16,12},
  {"15I","Ce","策","Whip",1258,2,2},
  {"16A","Lou","娄","Bond",1260,3,3},
  {"16B","Zuo Geng","左更","Official in Charge of the Forest",1263,6,6},
  {"16C","You Geng","右更","Official in Charge of Pasturing",1269,6,6},
  {"16D","Tian Cang","天仓","Square Celestial Granary",1275,6,6},
  {"16E","Tian Yu","天庾","Ricks of Grain",1281,3,3},
  {"16F","Tian Da Jiang Jun","天大将军","Great General of Heaven",1284,12,11},
  {"17A","Wei","胃","Stomach",1296,3,3},
  {"17B","Tian Qun","天囷","Circular Celestial Granary",1299,13,13},
  {"17C","Tian Lin","天廪","Celestial Foodstuff",1312,4,4},
  {"17D","Da Ling","大陵","Mausoleum",1316,8,8},
  {"17E","Tian Chuan","天船","Celestial Boat",1324,9,9},
  {"17F","Ji Shi(Da Ling)","积尸(大陵)","Heap of Corpses (In Mausoleum)",1333,2,2},
  {"17G","Ji Shui(Tian Chuan)","积水(天船)","Stored water (In Celestial Boat)",1335,2,2},
  {"18A","Mao","昴","Hairy Head",1337,7,7},
  {"18B","Tian E","天阿","Celestial Concave",1344,2,2},
  {"18C","Yue","月","Lunar Star",1346,2,2},
  {"18D","Tian Yin","天阴","Celestial Yin Force",1348,11,8},
  {"18E","Juan She","卷舌","Rolled Tongue",1359,6,6},
  {"18F","Tian Chan","天谗","Celestial Slander",1365,2,2},
  {"18G","Li Shi","砺石","Whetstone",1367,4,4},
  {"18H","Chu Hao","刍藁","Hay",1371,6,6},
  {"18I","Tian Yuan","天苑","Celestial Meadows",1377,16,16},
  {"19A","Bi","毕","Net",1393,13,11},
  {"19B","Fu Er","附耳(附毕)","Whisper (Adjunct to Net)",1406,2,2},
  {"19C","Tian Jie","天街","Celestial Street",1408,2,2},
  {"19D","Tian Jie","天节","Celestial Tally",1410,12,10},
  {"19E","Zhu Wang","诸王","Feudal Kings",1422,6,6},
  {"19F","Tian Gao","天高","Celestial High Terrace",1428,5,5},
  {"19G","Jiu Zhou Shu Kou","九州殊口","Interpreters of Nine Dialects",1433,10,10},
  {"19H","Wu Che","五车","Five Chariots",1443,5,5},
  {"19I","Zhu(Bi Xiu)","柱(五车)","Pillars (In Five Chariots)",1448,5,4},
  {"19J","Zhu(Bi Xiu)","柱(五车)","Pillars (In Five Chariots)",1453,5,4},
  {"19K","Zhu(Bi Xiu)","柱(五车)","Pillars (In Five Chariots)",1458,3,3},
  {"19L","Tian Huang","天潢","Celestial Pier",1461,7,6},
  {"19M","Xian Chi","咸池","Pool of Harmony",1468,5,4},
  {"19N","Tian Guan","天关","Celestial Pass",1473,2,2},
  {"19O","Can Qi","参旗","Banner of Three Stars",1475,9,9},
  {"19P","Jiu Liu","九斿","Imperial Military Flag",1484,9,9},
  {"19Q","Tian Yuan","天园","Celestial Orchard",1493,13,13},
  {"20A","Zi Xi","觜觿","Beak",1506,5,4},
  {"20B","Si Guai","司怪","Deity in Charge of Monsters",1511,4,4},
  {"20C","Zuo Qi","坐旗","Seat Flags",1515,9,9},
  {"21A","Shen","参","Three Stars",1524,11,9},
  {"21B","Fa","伐(附参)","Attack (Adjunct to the Three Stars)",1535,3,3},
  {"21C","Yu Jing","玉井","Jade Well",1538,4,4},
  {"21D","Ping","屏","Screen",1542,2,2},
  {"21E","Jun Jing","军井","Military Well",1544,4,4},
  {"21F","Ce","厕","Toilet",1548,4,4},
  {"21G","Tian Shi","天矢","Celestial Excrement",1552,2,2},
  {"22A","Dong Jing","东井","Eastern Well",1554,18,14},
  {"22B","Yue","钺(附东井)","Battle Axe (Adjunct to Eastern Well)",1572,2,2},
  {"22C","Nan He","南河","South River",1574,3,3},
  {"22D","Bei He","北河","North River",1577,3,3},
  {"22E","Tian Zun","天樽","Celestial Wine Cup",1580,3,3},
  {"22F","Wu Zhu Hou(JingXiu)","五诸侯(井宿)","Five Feudal Kings",1583,5,5},
  {"22G","Ji Shui(Jin Xiu)","积水(井宿)","Accumulated Water",1588,2,2},
  {"22H","Ji Xin","积薪","Pile of Firewood",1590,2,2},
  {"22I","Shui Fu","水府","Official for Irrigation",1592,5,5},
  {"22J","Shui Wei","水位","Water Level",1597,4,4},
  {"22K","Si Du","四渎","Four Channels",1601,4,4},
  {"22L","Jun Shi","军市","Market for Soldiers",1605,14,14},
  {"22M","Ye Ji","野鸡","Wild Cockerel",1619,2,2},
  {"22N","Sun","孙","Grandson",1621,2,2},
  {"22O","Zi","子","Son",1623,2,2},
  {"22P","Zhang Ren","丈人","Grandfather",1625,2,2},
  {"22Q","Que Qiu","阙丘","Palace Gate",1627,2,2},
  {"22R","Lang","狼","Wolf",1629,2,2},
  {"22S","Hu Shi","弧矢","Bow and Arrow",1631,13,12},
  {"22T","Lao Ren","老人","Old Man",1644,2,2},
  {"23A","Yu Gui","舆鬼","Cabin Ghosts",1646,5,5},
  {"23B","Ji Shi","积尸（舆鬼）","Cumulative Corpse (In Cabin Ghosts)",1651,0,0},
  {"23C","Guan","爟","Beacon Fire",1651,5,5},
  {"23D","Tian Gou","天狗","Celestial Dog",1656,9,8},
  {"23E","Wai Chu","外厨","Outer Kitchen",1665,8,7},
  {"23F","Tian Ji","天记","Judge for Estimating the Age of Animals",1673,2,2},
  {"23G","Tian She","天社","Celestial Earth God's Temple",1675,10,8},
  {"24A","Liu","柳","Willow",1685,8,8},
  {"24B","Jiu Qi","酒旗","Banner of Wine Shop",1693,6,5},
  {"25A","Qi Xing","七星","Seven Stars",1699,7,7},
  {"25B","Xuan Yuan","轩辕","Xuanyuan",1706,21,19},
  {"25C","Nei Ping","内平","High Judge",1727,4,4},
  {"25D","Tian Xiang","天相","Celestial Premier",1731,5,4},
  {"25E","Tian Ji","天稷","Celestial Cereals",1736,6,6},
  {"26A","Zhang","张","Extended Net",1742,13,10},
  {"26B","Tian Miao","天庙","Celestial Temple",1755,15,15},
  {"27A","Yi","翼","Wings",1770,39,31},
  {"27B","Dong Ou","东瓯","Dongou",1809,5,5},
  {"28A","Zhen","轸","Chariot",1814,14,11},
  {"28B","Chang Sha","长沙(附轸)","Changsha (Adjunct to Chariot)",1828,2,2},
  {"28C","Zuo Xia","左辖(附轸)","Left linchpin (Adjunct to Chariot)",1830,2,2},
  {"28D","You Xia","右辖(附轸)","Right linchpin (Adjunct to Chariot)",1832,2,2},
  {"28E","Jun Men","军门","Military Gate",1834,2,2},
  {"28F","Tu Si Kong(Zhen Xiu)","土司空(轸宿)","Master of Constructions (In Chariot Mansion)",1836,4,4},
  {"28G","Qing Qiu","青丘","Green Hill",1840,8,8},
  {"28H","Qi Fu","器府","House for Musical Instruments",1848,35,32},
};
constexpr int kAstCount = 317;

const int kXiuIndex[28] = {328,373,395,452,471,487,506,516,576,640,695,729,785,893,921,966,997,1039,1083,1176,1194,1221,1286,1316,1329,1361,1382,1410};


inline float starRa(int i) { return kStarData[i * 3 + 0]; }
inline float starDec(int i) { return kStarData[i * 3 + 1]; }
inline float starMag(int i) { return kStarData[i * 3 + 2]; }
constexpr uint16_t kVSep = 0xFFFF;

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
inline float mapGcDec(int k) {   // Table 3 "geometrical centre (DEC)"
  if (k == 1) return -14.0f;
  if (k == 2) return -8.0f;
  if (k == 5) return 5.0f;
  return -6.0f;                  // the mean of the three, for the nine
}                                // maps the paper did not fit
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

struct Departure { float maxDeg, mm, ratio, sigma; };

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
    sx += dec; sy += y; sxx += dec * dec; sxy += dec * y;
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
    xs[(size_t)i] = p; ys[(size_t)i] = y;
    sx += p; sy += y; sxx += p * p; sxy += p * y;
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
  const char *defect;
};
const M5Row kMap5[20] = {
    {"19H", "Wuche + Sanzhu", "\xe4\xba\x94\xe8\xbb\x8a+\xe4\xb8\x89\xe6\x9f\xb1",
     "five chariots + three poles", 'R', 14, 14, 5, nullptr},
    {"19E", "Zhuwang", "\xe8\xab\xb8\xe7\x8e\x8b", "several princes", 'R', 6, 5, 5,
     nullptr},
    {"20C", "Zuoqi", "\xe5\xb7\xa6\xe6\x97\x97", "left banner", 'B', 9, 8, 5,
     "Table 4 writes \xe5\xb7\xa6\xe6\x97\x97; Chen Zhuo writes \xe5\x9d\x90\xe6\x97\x97"},
    {"22E", "Tianzun", "\xe5\xa4\xa9\xe6\xa8\xbd", "celestial wine cup", 'B', 3, 3, 1,
     nullptr},
    {"19F", "Tiangao", "\xe5\xa4\xa9\xe9\xab\x98", "celestial high terrace", 'W', 4,
     4, 4, "reference mansion Bi is on MAP 4, not this one"},
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
    {"22L", "Junshi", "\xe8\xbb\x8d\xe5\xb8\x82", "soldiers' market", 'R', 13, 11, 5,
     nullptr},
    {"21D", "Ping", "\xe5\xb1\x8f", "toilet screen", 'W', 2, 2, 2,
     "labelled but NOT AT ITS PLACE; should be S of Junjing"},
    {"21E", "Junjing", "\xe8\xbb\x8d\xe4\xba\x95", "soldiers' well", 'B', 4, 4, 2,
     nullptr},
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
  const char *note;
};
const M13Row kMap13[] = {
    {"P22", "Tianchu", "\xe5\xa4\xa9\xe5\x8e\xa8", 'B', 5, 6, nullptr},
    {"P17", "Wudizuo", "\xe4\xba\x94\xe5\xb8\x9d\xe5\x86\x85\xe5\xba\xa7", 'B', 5, 5,
     nullptr},
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
    {"P16", "Tianhuang", "\xe5\xa4\xa9\xe7\x9a\x87\xe5\xa4\xa7\xe5\xb8\x9d", 'B', 1, 4,
     nullptr},
    {"P05", "Ziwei E wall", "\xe7\xb4\xab\xe5\xbe\xae\xe6\x9d\xb1\xe5\x9e\xa3", 'M', 8,
     8, "14 R + 1 B over both walls \xc2\xb7 8 + 7 = 15, closes"},
    {"P06", "Ziwei W wall", "\xe7\xb4\xab\xe5\xbe\xae\xe8\xa5\xbf\xe5\x9e\xa3", 'M', 7,
     7, nullptr},
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
    {"P31", "Taiyangshou", "\xe5\xa4\xaa\xe9\x98\xb3\xe5\xae\x88", 'R', 1, 1, nullptr},
    {"P32", "Shi", "\xe5\x8a\xbf", 'B', 4, 4, nullptr},
    {"P30", "Tianlao", "\xe5\xa4\xa9\xe7\x89\xa2", 'R', 6, 6, nullptr},
};
constexpr int kMap13Count = (int)(sizeof(kMap13) / sizeof(kMap13[0]));

// the 28 mansions in order, and their determinative stars' HIP numbers as
// Stellarium's lunar_system.defining_stars gives them
const char *kXiu[28] = {
    "\xe8\xa7\x92", "\xe4\xba\xa2", "\xe6\xb0\x90", "\xe6\x88\xbf", "\xe5\xbf\x83",
    "\xe5\xb0\xbe", "\xe7\xae\x95", "\xe6\x96\x97", "\xe7\x89\x9b", "\xe5\xa5\xb3",
    "\xe8\x99\x9b", "\xe5\x8d\xb1", "\xe5\xae\xa4", "\xe5\xa3\x81", "\xe5\xa5\x8e",
    "\xe5\xa9\x81", "\xe8\x83\x83", "\xe6\x98\xb4", "\xe7\x95\xa2", "\xe8\xa7\x9c",
    "\xe5\x8f\x83", "\xe4\xba\x95", "\xe9\xac\xbc", "\xe6\x9f\xb3", "\xe6\x98\x9f",
    "\xe5\xbc\xb5", "\xe7\xbf\xbc", "\xe8\xbb\xb8"};
const char *kXiuPinyin[28] = {"Jiao", "Kang", "Di",  "Fang", "Xin", "Wei", "Ji",
                              "Dou",  "Niu",  "Nu",  "Xu",   "Wei", "Shi", "Bi",
                              "Kui",  "Lou",  "Wei", "Mao",  "Bi",  "Zui", "Shen",
                              "Jing", "Gui",  "Liu", "Xing", "Zhang", "Yi", "Zhen"};
const int kXiuHip[28] = {65474, 69427, 72622,  78265,  80112,  82514,  88635,
                         92041, 100345, 102618, 106278, 109074, 113963, 1067,
                         3693,  8903,  12719,  17499,  20889,  26176,  25930,
                         30343, 41822, 42313,  46390,  48356,  53740,  59803};

/** Distinct stars in an asterism. `AstRec::stars` is the VERTEX count and a
 *  Chen Zhuo polyline revisits stars (東井 is 14 vertices over 9 stars), so
 *  comparing it to Table 4's n(SXC) compares two different things. */
int astUnique(const AstRec &A) {
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

SkColor4f schoolInk(char c) {
  switch (c) {
  case 'R': return kCinnabar;
  case 'B': return kInk;
  case 'W': return kLead;
  default: return kInkFaint;
  }
}

} // namespace

// ===========================================================================

struct DunhuangStarChart : sigil::compose::sketch::Sketch {
  sk_sp<SkTypeface> faceSerif, faceItalic, faceMono, faceDisplay, faceHan;

  // ONE Output writes the plate: the score position in seconds.
  ch::Output<float> scribe{0.0f};
  ch::Output<float> reveal{0.0f}; // 0→1 over the asterism sweep
  double clockT = 0;
  int auditPhase = -2;

  console::LineRing logA{72}, logB{72}, logC{72}, logD{72};

  /** THE ONE DISCRETE STATE, AND IT IS A CACHING STATE. Every reveal on this
   *  plate is a window() on `scribe`, which is a BOUND property and therefore
   *  live volatility for ever — even once its value has been 1.0 for ten
   *  seconds. Past the score's end the same reveal is described as a plain
   *  constant, so ~500 nodes fall back into their parents' recordings. Two
   *  render() calls per 31 s loop buy it. (sigillum_aemeth found the same
   *  shape and paid 365 ms for a full render(); this one is cheap because
   *  nothing below re-measures.) */
  bool settled = false;
  PropValue<float> gate(float t0, float t1) const {
    if (settled) return PropValue<float>(1.0f);
    return PropValue<float>(bind(&scribe).window(t0, t1));
  }
  /** A mark that comes and GOES: gone in the settled state. */
  PropValue<float> flash(float t0, float t1, float t2) const {
    if (settled) return PropValue<float>(0.0f);
    return PropValue<float>(
        bind(&scribe).from(t0, t1).clamp(0, 1).scale(1.0f).offset(0.0f)
            .clamp(0, 1));
  }

  Material paperGrain;
  Pattern paperSpeck;
  Element rollMark;

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
    float fold0 = 0;       // its own beat in the fold's stagger
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
  std::vector<int> astMap;      // 5, 13, or 0
  std::vector<float> astRa;     // mean RA at +700, for the reveal stagger

  /** The asterism line art, built ONCE. describe() runs twice per loop (the
   *  settle flip and the loop reset) and rebuilding 1,747 precessions and 317
   *  paths inside it cost a 20.4 ms frame — measured, then removed. */
  struct AstArt { SkPath local; SkRect box; float t0; SkColor4f ink; };
  std::vector<AstArt> astArt;
  /** Map 5's twenty centroids and the 28 determinative RAs, resolved ONCE.
   *  describe() runs twice per loop and both were O(asterisms x vertices)
   *  inside it — one 22.3 ms frame at the settle flip, measured then removed. */
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
  static bool scrollX(float s, float &x) {
    const float xr = kOriginR - s * kPxMm;
    if (xr >= kBreakR - 2.0f && xr <= kW + 90.0f) { x = xr; return true; }
    const float xl = kOriginL - s * kPxMm;
    if (xl >= -90.0f && xl <= kBreakL + 2.0f) { x = xl; return true; }
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
  static bool paperPoint(float ra, float dec, SkPoint &out, int &region) {
    if (dec >= 52.0f) {                         // map 13 — azimuthal
      const float rmm = (kDiscCenDec - dec) / kPolPerMm;
      const float a = (kAzGain * wrap180(ra - 278.0f)) * kD;
      float xc;
      if (!scrollX(discCentreS(), xc)) return false;
      out = {xc + rmm * kPxMm * std::cos(a), kBandMid + rmm * kPxMm * std::sin(a)};
      region = 13;
      return true;
    }
    if (dec > 45.0f || dec < -45.0f) { region = 0; return false; }
    const int k = mapOfRa(ra);
    const float dRa = wrap180(ra - mapCentre(k));
    if (std::abs(dRa) > 24.0f) { region = 0; return false; }
    const float s = mapSlotS(k) + kMapWmm * 0.5f + dRa / kRaPerMm;
    float x;
    if (!scrollX(s, x)) { region = k; return false; }
    const float decTop = mapGcDec(k) + 45.0f;
    out = {x, kFrameTop + (decTop - dec) / kDecPerMm * kPxMm};
    region = k;
    return true;
  }

  /** The disc's own frame, for the ornament and the mansion spokes. */
  static bool discFrame(SkPoint &c, float &rOuter) {
    float xc;
    if (!scrollX(discCentreS(), xc)) return false;
    c = {xc, kBandMid};
    rOuter = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
    return true;
  }

  // =========================================================================
  // THE POOL — rebuilt only when the epoch or the fold actually moved.

  void rebuild(float epoch, float fold) {
    if (std::abs(epoch - lastEpoch) < 1e-3f && std::abs(fold - lastFold) < 1e-4f)
      return;
    lastEpoch = epoch;
    lastFold = fold;
    const Mat3 M = precMatrix((epoch - 2000.0f) * 0.01f);
    auto pos = pool->positions();
    auto tint = pool->tints();
    for (int i = 0; i < nStars; ++i) {
      Placed &p = placed[(size_t)i];
      precess(M, starRa(i), starDec(i), p.raNow, p.decNow);
      p.sky = skyPoint(p.raNow, p.decNow);
      int region = 0;
      SkPoint paper;
      p.onPaper = paperPoint(p.raNow, p.decNow, paper, region);
      p.paper = p.onPaper ? paper : p.sky;
      const float f = smooth((fold - p.fold0) / 0.42f) * (p.onPaper ? 1.0f : 1.0f);
      pos[(size_t)i] = {p.sky.fX + (p.paper.fX - p.sky.fX) * f,
                        p.sky.fY + (p.paper.fY - p.sky.fY) * f};
      SkColor4f t{1, 1, 1, 1};
      // The sky the chart does NOT carry — the +45..+52 band between the
      // cylindrical maps and the disc, everything south of -45 — leaves the
      // plate entirely rather than lingering as ghosts ON the paper, where it
      // reads as dots. The count is in the console instead.
      if (!p.onPaper) t.fA = 1.0f - f;
      tint[(size_t)i] = t;
    }
    pool->touch();
  }

  // =========================================================================
  // BUILDING

  void computeJoins() {
    placed.assign((size_t)nStars, Placed{});
    astSchool.assign((size_t)nAst, ' ');
    astMap.assign((size_t)nAst, 0);
    astRa.assign((size_t)nAst, 0.0f);

    for (const M5Row &r : kMap5) {
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) { astSchool[(size_t)a] = r.col; astMap[(size_t)a] = 5; }
      m5Sxc += r.nSxc;
      m5Map += r.nMap;
    }
    for (int j = 0; j < kMap13Count; ++j) {
      const M13Row &r = kMap13[j];
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) { astSchool[(size_t)a] = r.col; astMap[(size_t)a] = 13; }
    }
    // 伐 Fa, the dagger: "the three hazy red stars ... (no specific label)".
    // Red, NOT encircled, and never named — 'H' is that state, not a school.
    for (int a = 0; a < nAst; ++a)
      if (std::string(kAst[a].id) == "21B") { astSchool[(size_t)a] = 'H'; astMap[(size_t)a] = 5; }

    // per-star school: from its asterism where the paper gives one
    std::vector<char> starSchool((size_t)nStars, ' ');
    const Mat3 M = precMatrix(-13.0f);
    for (int a = 0; a < nAst; ++a) {
      const AstRec &A = kAst[a];
      double sx = 0, sy = 0;
      int n = 0;
      for (int w = 0; w < A.words; ++w) {
        const uint16_t v = kVerts[A.first + w];
        if (v == kVSep) continue;
        if (astSchool[(size_t)a] != ' ')
          starSchool[v] = astSchool[(size_t)a] == 'M' ? 'R' : astSchool[(size_t)a];
        float ra, dec;
        precess(M, starRa(v), starDec(v), ra, dec);
        sx += std::cos(ra * kD); sy += std::sin(ra * kD);
        ++n;
      }
      astRa[(size_t)a] = n ? wrap360((float)std::atan2(sy, sx) / kD) : 0.0f;
      if (astMap[(size_t)a] == 5) m5ChenZhuo += astUnique(A);
    }

    for (int i = 0; i < nStars; ++i) {
      Placed &p = placed[(size_t)i];
      float ra, dec;
      precess(M, starRa(i), starDec(i), ra, dec);
      SkPoint dummy;
      int region = 0;
      const bool on = paperPoint(ra, dec, dummy, region);
      (void)on;
      if (dec >= 52.0f) ++nOnDisc;
      else if (dec > 45.0f) ++nInGap;
      else if (dec < -45.0f) ++nTooSouth;
      else ++nOnMaps;
      const char sc = starSchool[(size_t)i];
      if (sc == 'H') { p.cell = cellBare; ++nSchooled; }   // Fa, hazy and bare
      else if (sc == ' ') { p.cell = cellOpen; ++nUnattested; }
      else { ++nSchooled; p.cell = sc == 'R' ? cellRed : sc == 'B' ? cellBlack : cellWhite; }
      // the fold sweeps right-to-left, in reading order: by RA
      p.fold0 = 0.56f * (wrap360(ra - 296.0f) / 360.0f);
    }

    depMerc = mercatorVsLinear(-27.0f, 43.0f, 1.61f, 5.28f, 0.996f, 15);
    depStereo = stereoVsEquidistant(0.0f, 38.0f, 3.29f, 5.10f, 0.919f, 19);
  }

  // --- the paper ----------------------------------------------------------

  Element ground() {
    auto g = box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
                 .key("ground");
    g.child(box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
                .fill(Material::linear({0, 0}, {kW, kH},
                                       {{0.0f, hex(0x171410)}, {0.5f, hex(0x1d1913)},
                                        {1.0f, hex(0x120f0c)}}))
                .cache(Cache::Texture));
    return g;
  }

  /** The scroll band: the mulberry sheet, the Kraft lining showing at the
   *  edges, the roll's contact replication marks, and two rules of UNEQUAL
   *  weight along the top and bottom — a scroll's edges are not a rect. */
  Element scrollBand(float x0, float x1, const char *keyName, float tilt) {
    const float w = x1 - x0;
    // ASK FOR THE BAKE BY NAME. The band carries a paper gradient, an
    // anisotropic grain and a speckle wash over 780 kpx, and it is ROTATED
    // (a scroll does not lie square) — which disqualifies it from the
    // library's device-space promotion, since a bake pinned to one device
    // rect is not matrix-independent. Measured: 24.5 ms of a 29.9 ms frame
    // for the two bands, replaying three shaders over 1.5 Mpx forever.
    auto g = box().absolute().left(x0).top(kBandTop).width(Dim(w))
                 .height(Dim(kBandH)).rotate(tilt).key(keyName)
                 .cache(Cache::Texture)
                 .opacity(gate(tPaper, tPaper + 1.0f));

    // the sheet itself, with the fibre running along the roll
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(kBandH))
                .fill(Material::linear({0, 0}, {0, kBandH},
                                       {{0.00f, kKraft},
                                        {0.055f, kPaperDeep},
                                        {0.30f, kPaperMid},
                                        {0.58f, kPaperLit},
                                        {0.88f, kPaperMid},
                                        {0.945f, kPaperDeep},
                                        {1.00f, kKraft}}))
                .cache(Cache::Texture));

    // the fibre — anisotropic grain, luminance not hue, under a bake
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(kBandH))
                .fill(paperGrain)
                .opacity(0.20f)
                .blend(SkBlendMode::kSoftLight)
                .cache(Cache::Texture));
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(kBandH))
                .foreground(Wash{.material = paperSpeck.material(),
                                 .blend = SkBlendMode::kMultiply, .amount = 0.55f})
                .cache(Cache::Texture));

    // "replication marks by contact due to long conservation in a rolled
    // state" — the ghost of the adjacent turn, one circumference over
    const float circ = 244.0f * 3.14159f * 0.5f * kPxMm; // ~85 cm of roll
    for (int k = -3; k <= 3; ++k) {
      const float gx = std::fmod(std::abs(x0) + (float)k * circ, w);
      g.child(box().absolute().left(gx).top(6).width(Dim(3.0f)).height(Dim(kBandH - 12))
                  .fill(Fill::color(hex(0x8b6e45, 0.10f))));
    }

    // top and bottom rules — unequal, per lines::Rails
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(kBandH))
                .outline([](SkSize s) {
                  SkPathBuilder b;
                  b.moveTo(0, 1.5f);
                  b.lineTo(s.width(), 1.5f);
                  b.moveTo(0, s.height() - 1.5f);
                  b.lineTo(s.width(), s.height() - 1.5f);
                  return b.detach();
                })
                .stroke(Brush{}
                            .op(ops::Sketchy{.segLength = 34, .deviation = 0.9f,
                                             .seed = 3326})
                            .leg(lines::Rails{.rails = {
                                     {.offset = 0, .width = 1.9f,
                                      .fill = Fill::color(hex(0x6b573c, 0.62f))},
                                     {.offset = 4.5f, .width = 0.55f,
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
    if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4) return box().width(0).height(0);
    const float w = xr - xl;
    if (w <= 2) return box().width(0).height(0);
    auto g = box().absolute().left(xl - segLo(seg)).top(kFrameTop - kSegTop)
                 .width(Dim(w)).height(Dim(kFrameH))
                 .key(fmt("map%d_%d", k, seg))
                 .opacity(gate(tFold0 - 0.4f, tFold0 + 1.1f));

    // the ruled frame — a hand-drawn rectangle, brackets at the corners
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(kFrameH))
                .stroke(Brush{}
                            .op(ops::Sketchy{.segLength = 30, .deviation = 1.1f,
                                             .seed = (uint32_t)(600 + k)})
                            .leg(lines::Line{.width = 1.25f,
                                             .fill = Fill::color(hex(0x4a3b28, 0.78f))}))
                .foreground(decorations::brackets(2.0f, Fill::color(kInk), 15.0f,
                                                  0.0f, 30.0f)));

    // the equator — the one line whose position the paper says varies ±5°
    const float yEq = (mapGcDec(k) + 45.0f) / kDecPerMm * kPxMm;
    g.child(box().absolute().left(0).top(yEq - 1).width(Dim(w)).height(Dim(2))
                .outline([](SkSize s) {
                  SkPathBuilder b; b.moveTo(0, 1); b.lineTo(s.width(), 1);
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
      g.child(box().absolute().left(x - 0.4f).top(0).width(Dim(0.9f))
                  .height(Dim(big ? 9.0f : 5.0f))
                  .fill(Fill::color(hex(0x4a3b28, 0.66f))));
      g.child(box().absolute().left(x - 0.4f).top(kFrameH - (big ? 9.0f : 5.0f))
                  .width(Dim(0.9f)).height(Dim(big ? 9.0f : 5.0f))
                  .fill(Fill::color(hex(0x4a3b28, 0.66f))));
      if (big)
        g.child(text(toU8(fmt("%d", (int)std::lround(wrap360(mapCentre(k) + (float)t)))),
                     type(faceMono, 7.4f, hex(0x5d4c37, 0.72f)))
                    .absolute().left(x - 11).top(kFrameH + 3).width(Dim(24))
                    .textAlign(sigil::weave::TextAlignment::kCenter));
    }

    // the map's own number and month, in the margin above
    g.child(text(toU8(fmt("%d", k)), type(faceDisplay, 13.0f, hex(0x4a3b28, 0.82f)))
                .absolute().left(w - 20).top(-19).width(Dim(18))
                .textAlign(sigil::weave::TextAlignment::kEnd));

    // THE MANSION BOUNDARIES, ruled where the DETERMINATIVE STARS put them
    // and not at 12.86° apiece — Stellarium's lunar_system.defining_stars,
    // precessed to +700 like everything else. Interrupted rules: they stop
    // short of both edges of the frame.
    for (int m = 0; m < 28; ++m) {
      const float dRa = wrap180(xiuRa[(size_t)m] - mapCentre(k));
      if (std::abs(dRa) > 23.0f) continue;
      const float x = w * 0.5f + dRa / kRaPerMm * kPxMm;
      g.child(box().absolute().left(x - 6).top(0).width(Dim(12)).height(Dim(kFrameH))
                  .outline([](SkSize sz) {
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
      g.child(text(toU8(kXiu[m]),
                   type(faceHan ? faceHan : faceSerif, 11.5f, hex(0x2a2118, 0.88f)))
                  .absolute().left(x - 9).top(-32).width(Dim(18))
                  .textAlign(sigil::weave::TextAlignment::kCenter));
      g.child(text(toU8(kXiuPinyin[m]), type(faceMono, 7.0f, hex(0x5d4c37, 0.75f)))
                  .absolute().left(x - 20).top(-45).width(Dim(40))
                  .textAlign(sigil::weave::TextAlignment::kCenter));
    }
    return g;
  }

  /** The 50 columns of text, drawn as columns of marks rather than a grid.
   *  Real Tang manuscript columns are UNRULED — the discipline is in the
   *  hand — so what is drawn is the drift, not a lattice. */
  Element columnBand(int k, int seg) {
    const float s0 = mapSlotS(k) + kMapWmm;
    const float xl = segX(seg, s0 + kColBandMm), xr = segX(seg, s0);
    if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4) return box().width(0).height(0);
    const float w = xr - xl;
    if (w <= 2) return box().width(0).height(0);
    auto g = box().absolute().left(xl - segLo(seg)).top(kFrameTop + 8 - kSegTop)
                 .width(Dim(w)).height(Dim(kFrameH - 16)).key(fmt("cols%d_%d", k, seg))
                 .opacity(gate(tFold0 + 0.2f, tFold0 + 1.5f));
    const int nCols = (int)std::round(kColBandMm / kColMm);
    for (int c = 0; c < nCols; ++c) {
      const float u = (float)c / (float)std::max(1, nCols - 1);
      const float drift = std::sin((float)(k * 7 + c) * 1.31f) * 2.4f;
      const float cx = w - 4.0f - u * (w - 10.0f) + drift;
      const int glyphs = 20 + ((k * 5 + c * 3) % 11);
      for (int gI = 0; gI < glyphs; ++gI) {
        const float gy = 5.0f + (float)gI * 9.1f +
                         std::sin((float)(c * 13 + gI * 5)) * 0.8f;
        if (gy > kFrameH - 22) break;
        g.child(box().absolute().left(cx - 3.9f).top(gy).width(Dim(7.8f))
                    .height(Dim(6.2f))
                    .fill(Fill::color(hex(0x241d15, 0.80f)))
                    .outline(shapes::blob((uint32_t)(k * 97 + c * 13 + gI), 0.42f, 7)));
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
    auto g = box().absolute().left(c.fX - rOut - segLo(seg))
                 .top(c.fY - rOut - kSegTop).width(Dim(d))
                 .height(Dim(d)).key(fmt("disc%d", seg))
                 .opacity(gate(tFold0 - 0.2f, tFold0 + 1.4f));

    // the limb: a heavy outer rule and a hairline inner one
    g.child(box().absolute().left(0).top(0).width(Dim(d)).height(Dim(d))
                .outline(shapes::circle())
                .stroke(Brush{}
                            .op(ops::Sketchy{.segLength = 22, .deviation = 1.0f,
                                             .seed = 1300})
                            .leg(lines::heavyHairHeavy(1.7f, 0.5f,
                                                       Fill::color(hex(0x3a2e1e, 0.86f)),
                                                       5.0f))));

    // the DEC rings, at the published 5.10 °/cm — parametric, not stamped
    for (int dec = 60; dec <= 85; dec += 5) {
      const float rr = (kDiscCenDec - (float)dec) / kPolPerMm * kPxMm;
      if (rr <= 3 || rr >= rOut - 1) continue;
      g.child(box().absolute().left(rOut - rr).top(rOut - rr).width(Dim(rr * 2))
                  .height(Dim(rr * 2))
                  .outline(shapes::circle())
                  .stroke(PathFormat{.width = 0.5f,
                                     .strokeFill = Fill::color(hex(0x5d4c37, 0.30f)),
                                     .dashIntervals = {3, 5}}));
    }

    // the 28 mansion spokes — INTERRUPTED rules, stopping short of both the
    // limb and the pole, ruled at the determinative stars' own +700 RA and
    // NOT at 12.86° apiece
    for (int m = 0; m < 28; ++m) {
      const float ang = kAzGain * wrap180(xiuRa[(size_t)m] - 278.0f);
      g.child(box().absolute().left(0).top(0).width(Dim(d)).height(Dim(d))
                  .outline([ang, rOut](SkSize) {
                    SkPathBuilder b;
                    const float a = ang * kD;
                    b.moveTo(rOut + std::cos(a) * rOut * 0.14f,
                             rOut + std::sin(a) * rOut * 0.14f);
                    b.lineTo(rOut + std::cos(a) * rOut, rOut + std::sin(a) * rOut);
                    return b.detach();
                  })
                  .stroke(PathFormat{.width = 0.62f,
                                     .strokeFill = Fill::color(hex(0x4a3b28, 0.44f)),
                                     .trimStart = 0.05f,
                                     .trimEnd = 0.90f}));
      {
        const float a = ang * kD;
        const float lx = rOut + std::cos(a) * (rOut - 16.0f);
        const float ly = rOut + std::sin(a) * (rOut - 16.0f);
        g.child(text(toU8(kXiu[m]),
                     type(faceHan ? faceHan : faceSerif, 11.0f, hex(0x2a2118, 0.85f)))
                    .absolute().left(lx - 8).top(ly - 8).width(Dim(16))
                    .textAlign(sigil::weave::TextAlignment::kCenter));
      }
    }

    // the DISC'S CENTRE and the TRUE POLE are not the same point: Table 3
    // puts the centre at DEC +87.6°, so the +700 pole sits 4.7 mm away.
    const float poleOff = (90.0f - kDiscCenDec) / kPolPerMm * kPxMm;
    g.child(box().absolute().left(rOut - 5).top(rOut - 5).width(Dim(10)).height(Dim(10))
                .outline(shapes::circle())
                .stroke(PathFormat{.width = 0.7f,
                                   .strokeFill = Fill::color(hex(0x4a3b28, 0.7f))}));
    g.child(box().absolute().left(rOut - 3.2f).top(rOut - poleOff - 3.2f)
                .width(Dim(6.4f)).height(Dim(6.4f))
                .outline(shapes::star(4, 0.34f))
                .fill(Fill::color(kTrace))
                .opacity(gate(tProj - 1.4f, tProj - 0.4f)));
    // "a RED NON-ENCIRCLED star, slightly erased, could be the Pole star"
    // (Table 5, row 15, Beiji). Drawn erased, drawn unnamed, not resolved.
    g.child(box().absolute().left(rOut - poleOff * 0.45f - 4.0f)
                .top(rOut - poleOff * 0.7f - 4.0f).width(Dim(8)).height(Dim(8))
                .outline(shapes::circle())
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
    auto g = box().absolute().left(cx - rOut - 8).top(kBandMid + rOut - kSegTop + 16)
                 .width(Dim(rOut * 2 + 16)).key(fmt("discnote%d", seg))
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
                   type(faceMono, 8.0f, i >= 2 ? hex(0x8a3020, 0.95f)
                                               : hex(0x4a3b28, 0.9f)))
                  .absolute().left(0).top((float)i * 10.4f).width(Dim(rOut * 2 + 16)));
    g.child(text(toU8("slightly erased, sits near it \xe2\x80\x94 \"could be the Pole "
                      "star\". Drawn as found."),
                 type(faceMono, 8.0f, hex(0x8a3020, 0.95f)))
                .absolute().left(0).top(41.6f).width(Dim(rOut * 2 + 16)));
    return g;
  }

  // --- the asterism line art ----------------------------------------------

  /** One asterism, as line art: a polyline through real dot centres, bowed
   *  by ops::Sketchy because a hand-drawn join is not a rule, revealed by
   *  trim() on its own beat. */
  void buildAsterismArt() {
    astArt.clear();
    const Mat3 M = precMatrix(-13.0f);
    for (int a = 0; a < nAst; ++a) {
      const AstRec &A = kAst[a];
      SkPathBuilder pb;
      // NOT SkRect::join — it EARLY-OUTS on an empty rect, and a single
      // point IS an empty rect, so joining points one at a time leaves the
      // box inverted and every asterism draws nothing. Cost: one render.
      float bl = 1e9f, bt = 1e9f, br = -1e9f, bb = -1e9f;
      bool open = false;
      int pts = 0;
      SkPoint prev{0, 0};
      int prevRegion = -1;
      bool havePrev = false;
      for (int w = 0; w < A.words; ++w) {
        const uint16_t v = kVerts[A.first + w];
        if (v == kVSep) { open = false; havePrev = false; prevRegion = -1; continue; }
        float ra, dec;
        precess(M, starRa(v), starDec(v), ra, dec);
        SkPoint p;
        int region = 0;
        if (!paperPoint(ra, dec, p, region)) {
          open = false; havePrev = false; prevRegion = -1; continue;
        }
        if (p.fX < -70 || p.fX > kW + 70) {
          open = false; havePrev = false; prevRegion = -1; continue;
        }
        // an asterism whose stars fall on DIFFERENT maps is drawn BROKEN —
        // the scroll's RA axis is discontinuous at every map boundary, so a
        // join across one is a line that does not exist on the paper
        if (havePrev && (region != prevRegion ||
                         SkPoint::Distance(prev, p) > 230.0f)) {
          open = false; havePrev = false;
        }
        prevRegion = region;
        if (!open) { pb.moveTo(p); open = true; }
        else pb.lineTo(p);
        bl = std::min(bl, p.fX); bt = std::min(bt, p.fY);
        br = std::max(br, p.fX); bb = std::max(bb, p.fY);
        prev = p; havePrev = true; ++pts;
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
    nDrawnAst = (int)astArt.size();
  }

  Element asterismLines() {
    auto g = box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
                 .key("asterisms");
    for (size_t i = 0; i < astArt.size(); ++i) {
      const AstArt &A = astArt[i];
      // the node's box is the ASTERISM's box, never the plate's
      g.child(box().absolute().left(A.box.left()).top(A.box.top())
                  .width(Dim(A.box.width())).height(Dim(A.box.height()))
                  .outline([p = A.local](SkSize) { return p; })
                  .trim(0.0f, gate(A.t0, A.t0 + 0.9f))
                  .stroke(Brush{}
                              .op(ops::Sketchy{.segLength = 13.0f,
                                               .deviation = 0.85f,
                                               .seed = (uint32_t)(i * 31 + 7)})
                              .leg(lines::Line{.width = 1.05f,
                                               .fill = Fill::color(A.ink),
                                               .capSize = 0.0f})));
    }
    return g;
  }
  int nDrawnAst = 0;

  /** Where an asterism's stars land on the paper, and its own box. */
  bool astCentroid(const char *cid, SkPoint &out, int &region) const {
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
        sx += p.fX; sy += p.fY; ++n;
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
    auto g = box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
                 .key("m5lab");
    for (int i = 0; i < 20; ++i) {
      const M5Row &r = kMap5[(size_t)i];
      const SkPoint c = m5Cent[(size_t)i];
      const int region = m5Region[(size_t)i];
      if (region != 5) continue;
      const float fl = segX(1, mapSlotS(5) + kMapWmm), fr = segX(1, mapSlotS(5));
      if (c.fX < fl - 2 || c.fX > fr + 2) continue;
      if (c.fY < kFrameTop || c.fY > kFrameTop + kFrameH) continue;
      const float t = tAudit + (float)i * tAuditEach;

      // the audit's own ring. Transient on a clean row; PERMANENT on the six
      // documented defects, so the settled plate carries exactly the errata.
      g.child(box().absolute().left(c.fX - 30).top(c.fY - 30).width(Dim(60))
                  .height(Dim(60)).outline(shapes::circle())
                  .opacity(r.defect ? gate(t, t + 0.3f)
                                    : flash(t, t + 0.3f, t + 3.0f))
                  .stroke(PathFormat{.width = 1.1f,
                                     .strokeFill = Fill::color(
                                         r.defect ? hex(0xb4531f, 0.85f)
                                                  : hex(0x2f6d86, 0.7f)),
                                     .dashIntervals = {4, 4}}));

      // WHAT IS WRITTEN ON THE PAPER, defects included
      const char *written = r.native;
      float lx = c.fX + 6, ly = c.fY - 30;
      bool none = false;
      const std::string cid = r.cid;
      if (cid == "21A" || cid == "19P") none = true;             // Shen, Jiuliu
      if (cid == "22I") written = kMap5[9].native;               // Shuifu <- Sidu
      if (cid == "22K") written = kMap5[8].native;               // Sidu <- Shuifu
      if (cid == "21D") { ly = c.fY - 96; lx = c.fX + 26; }      // Ping, misplaced
      if (!none)
        g.child(text(toU8(written),
                     type(faceHan ? faceHan : faceSerif, 12.5f, hex(0x241d15, 0.92f)))
                    .absolute().left(lx).top(ly).width(Dim(60))
                    .opacity(gate(tLine1 - 0.6f, tLine1 + 0.5f)));
      else
        g.child(text(toU8("[no label]"), type(faceMono, 8.2f, hex(0xb4531f, 0.9f)))
                    .absolute().left(c.fX + 22).top(c.fY - 40).width(Dim(70))
                    .opacity(gate(t, t + 0.3f)));
      if (cid == "21D")   // the leader from the misplaced label to its stars
        g.child(box().absolute().left(std::min(lx, c.fX)).top(ly + 10)
                    .width(Dim(std::abs(lx - c.fX) + 4))
                    .height(Dim(c.fY - ly - 10))
                    .outline([](SkSize sz) {
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
    auto g = box().absolute().left(x - w * 0.5f).top(kBandMid - h * 0.52f - kSegTop)
                 .width(Dim(w)).height(Dim(h)).key("archer")
                 .opacity(gate(tArch, tArch + 1.1f));

    // the figure — every mark a Ribbon over a real polyline, the width law
    // keyed to distance/fullLength so the press does not slide under trim()
    struct Bone { std::vector<SkPoint> pts; float w0; };
    const std::vector<Bone> bones = {
        // the cap, then the head
        {{{86, 30}, {104, 22}, {122, 30}, {118, 38}, {90, 38}, {86, 30}}, 2.6f},
        {{{92, 40}, {112, 40}, {116, 52}, {110, 62}, {96, 62}, {90, 52}, {92, 40}}, 2.8f},
        {{{103, 62}, {102, 78}}, 3.6f},                                  // neck
        {{{102, 78}, {100, 108}, {99, 132}}, 5.6f},                      // spine
        {{{100, 82}, {72, 88}, {46, 92}, {34, 94}}, 4.6f},               // bow arm
        {{{101, 84}, {126, 96}, {140, 84}, {138, 70}}, 4.6f},            // draw arm
        {{{78, 84}, {60, 122}, {52, 176}, {58, 214}}, 3.2f},             // left robe
        {{{124, 84}, {142, 124}, {150, 178}, {144, 214}}, 3.2f},         // right robe
        {{{58, 214}, {84, 222}, {118, 222}, {144, 214}}, 3.0f},          // hem
        {{{80, 222}, {74, 250}, {66, 274}}, 4.4f},                       // left leg
        {{{122, 222}, {130, 250}, {140, 274}}, 4.4f},                    // right leg
        {{{60, 274}, {78, 278}}, 3.0f},                                  // feet
        {{{134, 274}, {152, 278}}, 3.0f},
        {{{74, 132}, {126, 130}}, 2.4f},                                 // belt
        {{{99, 66}, {92, 100}, {104, 128}}, 2.0f},                       // lapel
    };
    for (size_t i = 0; i < bones.size(); ++i) {
      const Bone &b = bones[i];
      SkPathBuilder pb;
      pb.moveTo(b.pts[0]);
      for (size_t j = 1; j < b.pts.size(); ++j) pb.lineTo(b.pts[j]);
      SkPath p = pb.detach();
      float len = 0;
      for (size_t j = 1; j < b.pts.size(); ++j) len += SkPoint::Distance(b.pts[j - 1], b.pts[j]);
      const float w0 = b.w0;
      brushes::Ribbon rib;
      rib.fill = Fill::color(hex(0x241d15, 0.90f));
      rib.step = 2.0f;
      rib.widthMax = w0 * 1.9f;
      rib.widthFn = [len, w0](const PathSample &s) {
        const float t = len > 0 ? s.distance / len : 0.0f;
        return w0 * (0.55f + 0.75f * std::exp(-9.0f * t) + 0.35f * t);
      };
      g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(h))
                  .outline([p](SkSize) { return p; })
                  .trim(0.0f, gate(tArch + 0.05f * (float)i,
                                                   tArch + 0.05f * (float)i + 0.55f))
                  .stroke(rib));
    }
    // the bow and the arrow
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(h))
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(34, 8);
                  b.cubicTo(-10, 56, -10, 126, 34, 176);
                  b.moveTo(34, 8);
                  b.lineTo(20, 92);
                  b.lineTo(34, 176);
                  return b.detach();
                })
                .trim(0.0f, gate(tArch + 0.45f, tArch + 1.0f))
                .stroke(lines::Line{.width = 1.9f,
                                    .fill = Fill::color(hex(0x241d15, 0.88f))}));
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(h))
                .outline([](SkSize) {
                  SkPathBuilder b; b.moveTo(140, 90); b.lineTo(6, 92);
                  return b.detach();
                })
                .trim(0.0f, gate(tArch + 0.75f, tArch + 1.15f))
                .stroke(lines::arrow(1.5f, Fill::color(kCinnabar), 9.0f)));
    g.child(text(toU8("a bowman in traditional dress, captioned THE GOD OF"),
                 type(faceMono, 8.4f, hex(0x4a3b28, 0.85f)))
                .absolute().left(-18).top(h - 12).width(Dim(300))
                .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
    g.child(text(toU8("LIGHTNING, over a title nobody can read convincingly"),
                 type(faceMono, 8.4f, hex(0x4a3b28, 0.85f)))
                .absolute().left(-18).top(h + 0).width(Dim(300))
                .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
    return g;
  }

  /** The closing title. The circulating reading 'Qi jie meng ji dian jing yi
   *  juan' ignores the first character and reads a non-standard third
   *  character as 夢; 蔑 is likelier. The paper's verdict is quoted, and the
   *  graphs are drawn ILLEGIBLE on purpose. */
  Element unreadTitle() {
    const float x = segX(0, 2118.0f) - segLo(0);
    auto g = box().absolute().left(x - 34).top(kBandTop + 44 - kSegTop).width(Dim(68))
                 .height(Dim(kBandH - 88)).key("title")
                 .opacity(gate(tArch + 0.9f, tArch + 1.8f));
    for (int i = 0; i < 6; ++i) {
      const float y = 8.0f + (float)i * 46.0f;
      SkPathBuilder pb;
      const uint32_t seed = (uint32_t)(3326 + i * 17);
      const int nh = 2 + (int)(shapes::detail::hashNoise(seed, 1) * 2.99f);
      for (int h = 0; h < nh; ++h) {   // horizontals, the spine of a graph
        const float hy = y + 7 + (float)h * (26.0f / (float)nh) +
                         shapes::detail::hashNoise(seed, (uint32_t)(h + 10)) * 3.0f;
        const float x0 = 14 + shapes::detail::hashNoise(seed, (uint32_t)(h + 20)) * 7.0f;
        const float x1 = 52 - shapes::detail::hashNoise(seed, (uint32_t)(h + 30)) * 8.0f;
        pb.moveTo(x0, hy);
        pb.lineTo(x1, hy + shapes::detail::hashNoise(seed, (uint32_t)(h + 40)) * 2.0f - 1.0f);
      }
      const float vx = 26 + shapes::detail::hashNoise(seed, 50) * 12.0f;
      pb.moveTo(vx, y + 4);
      pb.lineTo(vx + shapes::detail::hashNoise(seed, 51) * 4.0f - 2.0f, y + 36);
      if (shapes::detail::hashNoise(seed, 52) > 0.45f) {  // a left-falling stroke
        pb.moveTo(34, y + 6);
        pb.lineTo(15, y + 34);
      }
      if (shapes::detail::hashNoise(seed, 53) > 0.55f) {  // and a right-falling one
        pb.moveTo(30, y + 10);
        pb.lineTo(50, y + 34);
      }
      g.child(box().absolute().left(0).top(0).width(Dim(68)).height(Dim(kBandH - 88))
                  .outline([p = pb.detach()](SkSize) { return p; })
                  .trim(0.0f, gate(tArch + 0.9f + (float)i * 0.08f,
                                                   tArch + 1.4f + (float)i * 0.08f))
                  .stroke(Brush{}
                              .op(ops::Sketchy{.segLength = 9.0f, .deviation = 0.7f,
                                               .seed = seed})
                              .leg(lines::Line{.width = 1.9f,
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
    auto g = box().absolute().left(lx).top(ly).width(Dim(lw)).height(Dim(lh))
                 .rotate(0.32f).key("locator")
                 .opacity(gate(0.3f, 1.2f));
    g.child(box().absolute().left(0).top(0).width(Dim(lw)).height(Dim(lh))
                .fill(Material::linear({0, 0}, {0, lh},
                                       {{0.0f, hex(0xa2865c)}, {0.5f, hex(0xd6bf95)},
                                        {1.0f, hex(0xa2865c)}}))
                .stroke(PathFormat{.width = 0.9f,
                                   .strokeFill = Fill::color(hex(0x2a2118, 0.75f))}));
    // the 26 clouds and the 80 columns of the divination section, at the RIGHT
    for (int c = 0; c < 26; ++c) {
      const float cx = lw - 24.0f - (float)c * 24.0f;
      g.child(box().absolute().left(cx - 7).top(6).width(Dim(14)).height(Dim(9))
                  .outline(shapes::blob((uint32_t)(700 + c), 0.34f, 6))
                  .fill(Fill::color(hex(0x33291c, 0.85f))));
    }
    for (int c = 0; c < 80; ++c) {
      const float cx = lw - 20.0f - (float)c * 7.6f;
      g.child(box().absolute().left(cx).top(19).width(Dim(1.1f))
                  .height(Dim(lh - 25))
                  .fill(Fill::color(hex(0x33291c, 0.55f))));
    }
    // the 13 maps
    const float atlasRight = lw - kScrollMm * mm + kAtlasMm * mm;
    for (int k = 1; k <= 12; ++k) {
      const float x0 = atlasRight - (mapSlotS(k) + kMapWmm) * mm;
      g.child(box().absolute().left(x0).top(lh * 0.16f)
                  .width(Dim(kMapWmm * mm)).height(Dim(lh * 0.68f))
                  .stroke(PathFormat{.width = 0.8f,
                                     .strokeFill = Fill::color(hex(0x2a2118, 0.9f))}));
    }
    {
      const float dcx = atlasRight - discCentreS() * mm;
      const float dr = lh * 0.34f;
      g.child(box().absolute().left(dcx - dr).top(lh * 0.5f - dr).width(Dim(dr * 2))
                  .height(Dim(dr * 2)).outline(shapes::circle())
                  .stroke(PathFormat{.width = 0.8f,
                                     .strokeFill = Fill::color(hex(0x2a2118, 0.9f))}));
    }
    // the two windows this plate actually shows
    struct Win { float s0, s1; };
    const Win wins[2] = {{(kOriginL - kBreakL) / kPxMm, (kOriginL + 90) / kPxMm},
                         {(kOriginR - kW - 90) / kPxMm, (kOriginR - kBreakR) / kPxMm}};
    for (const Win &wn : wins) {
      // the plate's canvas reaches ~227 mm past the atlas's own left end, so
      // window 0 runs off the strip; clamp it to the scroll it annotates
      const float a = std::max(0.0f, atlasRight - wn.s1 * mm);
      const float b = std::min(lw, atlasRight - wn.s0 * mm);
      g.child(box().absolute().left(a).top(-4).width(Dim(b - a)).height(Dim(lh + 8))
                  .fill(Fill::color(hex(0x2f6d86, 0.30f)))
                  .foreground(decorations::brackets(1.4f, Fill::color(kTrace), 9.0f,
                                                    0.0f, 30.0f)));
    }
    g.child(text(toU8("THE WHOLE SCROLL, 1:16 \xc2\xb7 3,940 \xc3\x97 244 mm \xc2\xb7 right: "
                      "26 cloud drawings over 80 columns of uranomancy \xc2\xb7 left: the "
                      "13-map atlas, 2,100 mm \xc2\xb7 shaded: what this plate shows"),
                 type(faceMono, 8.6f, hex(0x9a8a68, 0.9f)))
                .absolute().left(2).top(lh + 5).width(Dim(1700)));
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
    auto g = box().absolute().left(150).top(234).width(Dim(S)).height(Dim(S))
                 .key("pole")
                 .opacity(gate(tPrec0 - 0.8f, tPrec0 + 0.2f));
    auto poleAt = [](float epoch, float &ra, float &dec) {
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
      g.child(box().absolute().left(cx - rr).top(cy - rr).width(Dim(rr * 2))
                  .height(Dim(rr * 2)).outline(shapes::circle())
                  .stroke(PathFormat{.width = 0.5f,
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
    g.child(box().absolute().left(0).top(0).width(Dim(S)).height(Dim(S))
                .outline([t = tb.detach()](SkSize) { return t; })
                .trim(0.0f, gate(tPrec0, tPrec1))
                .stroke(lines::Line{.width = 2.0f, .fill = Fill::color(kTrace)}));
    // the whole 26,000-year circle, faint, for context
    SkPathBuilder wb;
    for (int e = -24000; e <= 4000; e += 250) {
      float ra, dec;
      poleAt((float)e, ra, dec);
      const SkPoint q = plot(ra, dec);
      if (90.0f - dec > 30.0f) continue;
      (wb.countPoints() == 0) ? wb.moveTo(q) : wb.lineTo(q);
    }
    g.child(box().absolute().left(0).top(0).width(Dim(S)).height(Dim(S))
                .outline([t = wb.detach()](SkSize) { return t; })
                .stroke(PathFormat{.width = 0.7f,
                                   .strokeFill = Fill::color(hex(0x8a7458, 0.45f)),
                                   .dashIntervals = {3, 4}}));
    // alp UMi is 0.74° from the J2000 pole, so its dot lands ON the centre
    // marker and the default up-right label lands ON "J2000 pole". The two
    // captions flank the coincident pair on one line instead.
    struct Ref { float ra, dec; const char *name; float lx, ly; };
    const Ref refs[3] = {{37.9529f, 89.2641f, "alp UMi", 6.0f, 6.0f},
                         {222.6764f, 74.1555f, "bet UMi", 5.0f, -5.0f},
                         {211.0973f, 64.3758f, "alp Dra", -8.0f, -13.0f}};
    for (const Ref &r : refs) {
      const SkPoint q = plot(r.ra, r.dec);
      g.child(box().absolute().left(q.fX - 3).top(q.fY - 3).width(Dim(6)).height(Dim(6))
                  .outline(shapes::circle()).fill(Fill::color(kCinnabar)));
      g.child(text(toU8(r.name), type(faceMono, 7.4f, hex(0x9a8a68)))
                  .absolute().left(q.fX + r.lx).top(q.fY + r.ly).width(Dim(60)));
    }
    {
      float ra, dec;
      poleAt(700.0f, ra, dec);
      const SkPoint q = plot(ra, dec);
      g.child(box().absolute().left(q.fX - 6).top(q.fY - 6).width(Dim(12)).height(Dim(12))
                  .outline(shapes::star(4, 0.30f)).fill(Fill::color(kTrace))
                  .opacity(gate(tPrec1 - 0.4f, tPrec1 + 0.3f)));
    }
    g.child(box().absolute().left(cx - 3).top(cy - 3).width(Dim(6)).height(Dim(6))
                .outline(shapes::circle())
                .stroke(PathFormat{.width = 0.9f,
                                   .strokeFill = Fill::color(hex(0xe0cfa6, 0.8f))}));
    g.child(text(toU8("J2000 pole"), type(faceMono, 7.4f, hex(0x6d6249)))
                .absolute().left(cx - 52).top(cy + 6).width(Dim(70)));
    return g;
  }

  Element poleText() {
    auto g = box().absolute().left(300).top(228).width(Dim(452)).key("poletext")
                 .opacity(gate(tPrec0 - 0.6f, tPrec0 + 0.4f));
    g.child(text(toU8("THE CHART DATES ITSELF"),
                 type(faceDisplay, 12.0f, hex(0xc9a35c), 1.0f))
                .absolute().left(0).top(0).width(Dim(430)));
    const char *rows[7] = {
        "the celestial pole's own track, from the SAME IAU 1976",
        "matrix the 1,460 stars ride. rings at 10/20/30 deg.",
        "polar distance at +700, computed here:",
        "   alp UMi 7.88   bet UMi 10.66   alp Dra 19.14",
        "the paper measures the map's pole 3.9 +/- 2.9 deg from",
        "the sky at +700 and calls it \"fully consistent\". so does",
        "this: 3.9 deg is HALF alp UMi's own distance at that date.",
    };
    for (int i = 0; i < 7; ++i)
      g.child(text(toU8(rows[i]), type(faceMono, 9.0f, i == 3 ? kChalk : hex(0x9a8a68)))
                  .absolute().left(0).top(18.0f + (float)i * 12.2f).width(Dim(430)));

    // THE EPOCH, RUNNING. One Output remapped three ways: it turns the star
    // field's rotation matrix, walks the pole's track above, and slides this
    // marker — bind() doing the unit conversion at each call site instead of
    // three Outputs in the tick loop.
    const float bw = 430.0f;
    g.child(box().absolute().left(0).top(112).width(Dim(bw)).height(Dim(9))
                .outline([](SkSize sz) {
                  SkPathBuilder b;
                  b.moveTo(0, 0); b.lineTo(0, sz.height());
                  b.moveTo(0, sz.height() * 0.5f);
                  b.lineTo(sz.width(), sz.height() * 0.5f);
                  b.moveTo(sz.width(), 0); b.lineTo(sz.width(), sz.height());
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
                .absolute().left(0).top(124).width(Dim(40)));
    g.child(text(toU8("J2000"), type(faceMono, 8.0f, hex(0x9a8a68)))
                .absolute().left(bw - 40).top(124).width(Dim(40))
                .textAlign(sigil::weave::TextAlignment::kEnd));
    g.child(box().absolute().left(-4).top(107).width(Dim(8)).height(Dim(19))
                .outline(shapes::polygon(3, 180.0f))
                .fill(Fill::color(kCinnabar))
                .translateX(settled
                                ? PropValue<float>(0.0f)
                                : PropValue<float>(bind(&scribe)
                                                       .window(tPrec0, tPrec1)
                                                       .invert()
                                                       .to(0.0f, bw))));
    g.child(text(toU8("13.00 Julian centuries \xc2\xb7 the sky slides 18.5\xc2\xb0 in RA"),
                 type(faceMono, 8.4f, hex(0xc9a35c)))
                .absolute().left(0).top(136).width(Dim(430)));
    return g;
  }

  /** THE THIRD QUESTION, DRAWN. Twelve maps of 48 deg on a 30 deg pitch: the
   *  frames butt on the paper but their RA windows OVERLAP by 18 deg, so the
   *  scroll's own RA axis jumps BACKWARD at every map boundary. This ruler
   *  is the two readings side by side. */
  Element raRuler(int seg) {
    auto g = box().absolute().left(0).top(0).width(Dim(segHi(seg) - segLo(seg)))
                 .height(Dim(kSegH)).key(fmt("raruler%d", seg))
                 .opacity(gate(tFold1 - 0.4f, tFold1 + 0.6f));
    const float y = kBandTop - kSegTop - 46.0f;
    if (seg == 1)
      g.child(text(toU8("EACH FRAME SPANS 48\xc2\xb0 OF RA ON A 30\xc2\xb0 PITCH \xc2\xb7 "
                        "HATCHED: the 18\xc2\xb0 it shares with its neighbour \xc2\xb7 "
                        "the axis JUMPS BACK at every boundary"),
                   type(faceMono, 8.4f, hex(0xc9a35c, 0.9f)))
                  .absolute().left(600).top(y - 26).width(Dim(900))); // just clear of the sheet
    for (int k = 1; k <= 12; ++k) {
      const float s0 = mapSlotS(k);
      const float xl = segX(seg, s0 + kMapWmm) - segLo(seg);
      const float xr = segX(seg, s0) - segLo(seg);
      if (xr < -6 || xl > segHi(seg) - segLo(seg) + 6) continue;
      g.child(box().absolute().left(xl).top(y).width(Dim(xr - xl)).height(Dim(11))
                  .outline([](SkSize sz) {
                    SkPathBuilder b;
                    b.moveTo(0, 0); b.lineTo(0, sz.height());
                    b.moveTo(0, sz.height() * 0.5f);
                    b.lineTo(sz.width(), sz.height() * 0.5f);
                    b.moveTo(sz.width(), 0); b.lineTo(sz.width(), sz.height());
                    return b.detach();
                  })
                  .stroke(lines::Line{.width = 1.0f,
                                      .fill = Fill::color(hex(0xc9a35c, 0.7f))}));
      g.child(text(toU8(fmt("%d\xc2\xb0", (int)std::lround(wrap360(mapCentre(k) - 24.0f)))),
                   type(faceMono, 7.6f, hex(0xc9a35c, 0.85f)))
                  .absolute().left(xr - 26).top(y + 13).width(Dim(28))
                  .textAlign(sigil::weave::TextAlignment::kEnd));
      g.child(text(toU8(fmt("%d\xc2\xb0", (int)std::lround(wrap360(mapCentre(k) + 24.0f)))),
                   type(faceMono, 7.6f, hex(0xc9a35c, 0.85f)))
                  .absolute().left(xl - 2).top(y + 13).width(Dim(28)));
      // the 18 deg this frame shares with its LEFT neighbour, hatched
      const float ov = 18.0f / kRaPerMm * kPxMm;
      g.child(box().absolute().left(xl).top(y - 11).width(Dim(ov)).height(Dim(9))
                  .fill(Fill::color(hex(0xb4531f, 0.16f)))
                  .background(lines::hatch(Fill::color(hex(0xb4531f, 0.75f)), 3.6f,
                                           0.7f, 45.0f)));
    }
    return g;
  }

  Element breakMark() {
    auto g = box().absolute().left(kBreakL - 8).top(kBandTop - 16)
                 .width(Dim(kBreakR - kBreakL + 16)).height(Dim(kBandH + 32))
                 .key("break")
                 .opacity(gate(tPaper + 0.4f, tPaper + 1.4f));
    const float w = kBreakR - kBreakL + 16, h = kBandH + 32;
    g.child(box().absolute().left(0).top(0).width(Dim(w)).height(Dim(h))
                .fill(Fill::color(hex(0x171410, 0.96f))));
    for (int i = 0; i < 2; ++i) {
      const float x = 8.0f + (float)i * (w - 16.0f);
      g.child(box().absolute().left(x - 9).top(0).width(Dim(18)).height(Dim(h))
                  .outline([](SkSize s) {
                    SkPathBuilder b;
                    b.moveTo(s.width() * 0.5f, 0);
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
    const float sL = (kOriginR - kBreakR) / kPxMm, sR = (kOriginL - kBreakL) / kPxMm;
    g.child(text(toU8(fmt("%d mm", (int)std::lround(sR - sL))),
                 type(faceMono, 8.2f, hex(0x9a8a68, 0.85f)))
                .absolute().left(-16).top(h + 4).width(Dim(w + 32))
                .textAlign(sigil::weave::TextAlignment::kCenter));
    return g;
  }

  console::Style logStyle() {
    console::Style s;
    s.text = type(faceMono, 9.2f, hex(0x9a8a68));
    s.palette = {type(faceMono, 9.2f, hex(0x6d6249)),
                 type(faceMono, 9.2f, hex(0xc9a35c)),
                 type(faceMono, 9.2f, hex(0x6ba87e)),
                 type(faceMono, 9.2f, hex(0xcf6a4a)),
                 type(faceMono, 9.2f, hex(0xc4483a))};
    s.gap = 1.0f;
    s.visibleLines = 12;
    return s;
  }

  Element projectionPanel() {
    auto g = box().absolute().left(96).top(1046).width(Dim(700)).key("proj")
                 .opacity(gate(tProj, tProj + 0.9f));
    g.child(text(toU8("TWO QUESTIONS THE CHART CANNOT ANSWER, AND WHY"),
                 type(faceDisplay, 13.0f, hex(0xc9a35c), 1.1f))
                .absolute().left(0).top(0).width(Dim(690)));

    // curve A: the Mercator ordinate against its own best-fit line
    const float pw = 320.0f, ph = 132.0f;
    struct Plot { float x, lo, hi; bool merc; const char *cap; };
    const Plot plots[2] = {{0, -27, 43, true, "map 5 DEC \xe2\x88\x92" "27\xc2\xb0\xe2\x80\xa6" "+43\xc2\xb0"},
                           {366, 0, 38, false, "map 13 polar distance 0\xc2\xb0\xe2\x80\xa6" "38\xc2\xb0"}};
    for (int pi = 0; pi < 2; ++pi) {
      const Plot &pl = plots[pi];
      auto p = box().absolute().left(pl.x).top(30).width(Dim(pw)).height(Dim(ph));
      p.child(box().absolute().left(0).top(0).width(Dim(pw)).height(Dim(ph))
                  .stroke(decorations::gappedRule(0.9f, Fill::color(hex(0x8a7458, 0.5f)),
                                                  16.0f, 0.0f, 30.0f)));
      const float lo = pl.lo, hi = pl.hi;
      const bool merc = pl.merc;
      p.child(box().absolute().left(0).top(0).width(Dim(pw)).height(Dim(ph))
                  .outline([lo, hi, merc, pw, ph](SkSize) {
                    // the departure curve, self-normalised
                    float ys[81];
                    float sx = 0, sy = 0, sxx = 0, sxy = 0;
                    for (int i = 0; i <= 80; ++i) {
                      const float v = lo + (hi - lo) * (float)i / 80.0f;
                      ys[i] = merc ? std::log(std::tan((45.0f + v * 0.5f) * kD))
                                   : std::tan(v * 0.5f * kD);
                      sx += v; sy += ys[i]; sxx += v * v; sxy += v * ys[i];
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
      const Departure &dp = merc ? depMerc : depStereo;
      const float resid = merc ? 1.61f : 3.29f;
      const float halfRaw = resid / dp.maxDeg * ph * 0.40f;
      const float half = std::min(halfRaw, ph * 0.5f);
      p.child(box().absolute().left(0).top(ph * 0.5f - half).width(Dim(pw))
                  .height(Dim(half * 2))
                  .fill(Fill::color(hex(0xa8382a, 0.13f)))
                  .stroke(PathFormat{.width = 0.6f,
                                     .strokeFill = Fill::color(hex(0xa8382a, 0.45f)),
                                     .dashIntervals = {4, 4}}));
      p.child(text(toU8(pl.cap), type(faceMono, 8.4f, hex(0x9a8a68)))
                  .absolute().left(0).top(ph + 4).width(Dim(pw)));
      p.child(text(toU8(merc ? "linear \xe2\x88\x92 Mercator (blue) vs the hand (red band)"
                             : "equidist. \xe2\x88\x92 stereo. (blue); the hand is 7.6\xc3\x97 "
                               "the plot, off scale"),
                   type(faceMono, 8.4f, hex(0x6d6249)))
                  .absolute().left(0).top(ph + 15).width(Dim(pw)));
      g.child(std::move(p));
    }
    const char *lines_[6] = {
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
                  .absolute().left(0).top(196 + (float)i * 13.4f).width(Dim(690)));
    g.child(text(toU8("all three maps favour PURE CYLINDRICAL (0.974/0.972, "
                      "0.975/0.974, 0.996/0.994) \xe2\x80\x94 3 of 3, p=0.125"),
                 type(faceMono, 9.6f, hex(0xcf6a4a)))
                .absolute().left(0).top(280).width(Dim(690)));
    g.child(text(toU8("the disc cannot decide BECAUSE IT STOPS AT +52\xc2\xb0: over a "
                      "full hemisphere the pair would part by 7.00\xc2\xb0"),
                 type(faceMono, 9.6f, hex(0x6d6249)))
                .absolute().left(0).top(294).width(Dim(690)));
    return g;
  }

  /** Map 5's twenty asterisms, audited one at a time. Six of them carry a
   *  documented defect and every one is drawn AS FOUND. */
  Element auditPanel() {
    auto g = box().absolute().left(840).top(1046).width(Dim(880)).key("audit")
                 .opacity(gate(tAudit - 0.9f, tAudit - 0.2f));
    g.child(text(toU8("MAP 5 \xc2\xb7 THE ORION REGION \xc2\xb7 TABLE 4 OF "
                      "BONNET-BIDAUD, PRADERIE & WHITFIELD 2009"),
                 type(faceDisplay, 13.0f, hex(0xc9a35c), 1.0f))
                .absolute().left(0).top(0).width(Dim(880)));
    g.child(text(toU8("month 4 \xc2\xb7 xiu Zui, Shen, Jing \xc2\xb7 listed N\xe2\x86\x92" "S, "
                      "W\xe2\x86\x92" "E, i.e. by increasing RA \xc2\xb7 R=Shi shi  B=Gan shi  "
                      "W=Wu Xian shi"),
                 type(faceMono, 8.6f, hex(0x9a8a68)))
                .absolute().left(0).top(16).width(Dim(880)));
    // the subtitle above runs top 16..24 at 8.6 px; the column header needs
    // its own line, not the same one
    const float y0 = 40.0f, rowH = 15.2f;
    // one hand-spaced monospace string cannot land on these columns: the rows
    // are absolutely placed, the numeric block is a THIRD size, and the CJK
    // pair in the middle is double-advance. Each head sits on its own column.
    struct Head { float x; const char *s; };
    const Head heads[9] = {{11, "#"},        {30, "ASTERISM"}, {160, "\xe4\xb8\xad\xe6\x96\x87"},
                           {228, "COL"},     {253, "SXC"},     {281, "MAP"},
                           {312, "CZ"},      {362, "CONF"},    {400, "DEFECT"}};
    for (const Head &h : heads)
      g.child(text(toU8(h.s), type(faceMono, 8.6f, hex(0x6d6249)))
                  .absolute().left(h.x).top(y0 - 13).width(Dim(120)));
    for (int i = 0; i < 20; ++i) {
      const M5Row &r = kMap5[(size_t)i];
      const float y = y0 + (float)i * rowH;
      const float t = tAudit + (float)i * tAuditEach;
      auto row = box().absolute().left(0).top(y).width(Dim(880)).height(Dim(rowH))
                     .opacity(gate(t, t + 0.35f));
      int cz = 0;
      for (int a = 0; a < nAst; ++a)
        if (std::string(kAst[a].id) == r.cid) cz = astUnique(kAst[a]);
      row.child(text(toU8(fmt("%3d", i + 1)),
                     type(faceMono, 9.4f, hex(0x6d6249)))
                    .absolute().left(0).top(0).width(Dim(26)));
      row.child(text(toU8(r.pinyin), type(faceMono, 9.4f, kChalk))
                    .absolute().left(30).top(0).width(Dim(126)));
      row.child(text(toU8(r.native),
                     type(faceHan ? faceHan : faceSerif, 10.4f, schoolInk(r.col)))
                    .absolute().left(160).top(-2).width(Dim(64)));
      row.child(box().absolute().left(232).top(3.4f).width(Dim(8)).height(Dim(8))
                    .outline(shapes::circle())
                    .fill(Fill::color(schoolInk(r.col)))
                    .stroke(PathFormat{.width = 0.8f,
                                       .strokeFill = Fill::color(kInk)}));
      row.child(text(toU8(fmt("%4d %4d %4d", r.nSxc, r.nMap, cz)),
                     type(faceMono, 9.4f,
                          r.nSxc == r.nMap ? hex(0x9a8a68) : hex(0xcf6a4a)))
                    .absolute().left(250).top(0).width(Dim(94)));
      // the confidence index, as five cells
      for (int c = 0; c < 5; ++c)
        row.child(box().absolute().left(356 + (float)c * 7.0f).top(3.6f)
                      .width(Dim(5.2f)).height(Dim(7.0f))
                      .fill(Fill::color(c < r.conf ? hex(0xc9a35c, 0.85f)
                                                   : hex(0x6d6249, 0.28f))));
      if (r.defect)
        row.child(text(toU8(r.defect), type(faceMono, 9.0f, hex(0xb4531f)))
                      .absolute().left(400).top(0).width(Dim(478)));
      g.child(std::move(row));
    }
    const float yT = y0 + 20.0f * rowH + 8.0f;
    g.child(box().absolute().left(0).top(yT - 4).width(Dim(878)).height(Dim(0.8f))
                .fill(Fill::color(hex(0x8a7458, 0.5f)))
                .opacity(gate(tAudit + 5.4f, tAudit + 5.9f)));
    const std::string tot =
        fmt("TOTALS  SXC %d   map %d   Chen Zhuo %d distinct (Fa's 3 in, Sanzhu's "
            "9 absent \xe2\x80\x94 5 + 9 = SXC's 14 for Wuche, exactly)",
            m5Sxc, m5Map, m5ChenZhuo);
    g.child(text(toU8(tot), type(faceMono, 9.4f, kChalk))
                .absolute().left(0).top(yT).width(Dim(878))
                .opacity(gate(tAudit + 5.5f, tAudit + 6.0f)));
    g.child(text(toU8("Table 4's own n(map) column sums to 108. Its stated total "
                      "is 109. The census is soft, and the paper says so."),
                 type(faceMono, 9.4f, hex(0xcf6a4a)))
                .absolute().left(0).top(yT + 13).width(Dim(878))
                .opacity(gate(tAudit + 5.7f, tAudit + 6.2f)));
    g.child(text(toU8("6 documented defects in 20 asterisms, drawn AS FOUND \xe2\x80\x94 "
                      "ringed on map 5 above. A study that corrects them has "
                      "destroyed the object."),
                 type(faceMono, 9.4f, hex(0xb4531f)))
                .absolute().left(0).top(yT + 26).width(Dim(878))
                .opacity(gate(tAudit + 5.9f, tAudit + 6.4f)));
    return g;
  }

  /** THE DISC'S OWN ERRATA. Table 5 is 34 asterisms and 142 stars — and its
   *  n(map) column sums to 141. Everything here is quoted, nothing resolved. */
  Element map13Panel() {
    auto g = box().absolute().left(96).top(1362).width(Dim(700)).key("m13")
                 .opacity(gate(tAudit + 4.6f, tAudit + 5.4f));
    g.child(text(toU8("MAP 13 \xc2\xb7 THE CIRCUMPOLAR DISC \xc2\xb7 TABLE 5"),
                 type(faceDisplay, 12.0f, hex(0xc9a35c), 1.0f))
                .absolute().left(0).top(0).width(Dim(700)));
    const char *rows[10] = {
        "34 asterisms, stated total 142 stars; the n(map) column sums to 141",
        "(its Tianpei row reads \"5 or 6\", which is where the one goes).",
        "\xe7\xb4\xab\xe5\xbe\xae Ziwei: two walls, E and W, 14 red + 1 black \xe2\x80\x94 Chen Zhuo's",
        "  \xe6\x9d\xb1\xe5\x9e\xa3 8 + \xe8\xa5\xbf\xe5\x9e\xa3 7 = 15. CLOSES EXACTLY.",
        "\xe8\x8f\xaf\xe8\x93\x8b Huagai: 7 stars \"+ 6\" the authors cannot account for.",
        "  Chen Zhuo HAS \xe6\x9d\xa0 Gang appended to it \xe2\x80\x94 but 9 stars, not 6.",
        "  consistent, and it does not close. drawn on the disc, unlabelled.",
        "NI 1: one star, no character, east of Gouchen. \xe5\xa4\xa9\xe6\xa3\x93 Tianpei is the",
        "  SECOND mixed-colour asterism (\"5 R, 1 B?\"), not Ziwei alone.",
        "\xe4\xb8\x89\xe5\x85\xac Sangong: Chen Zhuo files one copy under WU XIAN, the other",
    };
    for (int i = 0; i < 10; ++i)
      g.child(text(toU8(rows[i]),
                   type(faceMono, 9.2f, i == 3 || i == 5 ? kChalk : hex(0x9a8a68)))
                  .absolute().left(0).top(18.0f + (float)i * 12.4f).width(Dim(700)));
    g.child(text(toU8("under GAN. The map draws both BLACK. Printed, not corrected."),
                 type(faceMono, 9.2f, hex(0xb4531f)))
                .absolute().left(0).top(18.0f + 10 * 12.4f).width(Dim(700)));
    return g;
  }

  Element consolePanel() {
    const float x = 1768, y = 1042, w = 700, h = 452;
    auto g = box().absolute().left(x).top(y).width(Dim(w)).height(Dim(h))
                 .fill(Fill::color(hex(0x100e0b, 0.86f)))
                 .stroke(stroke(1.0f, Fill::color(hex(0x8a7458, 0.24f)),
                                PathFormat::Align::Inner))
                 .key("console");
    g.child(box().absolute().left(12).top(9).width(Dim(w - 24)).height(Dim(h - 18))
                .column().gap(6)
                .child(console::console(logA, logStyle()))
                .child(box().height(1).fill(Fill::color(hex(0x8a7458, 0.16f))))
                .child(console::console(logB, logStyle()))
                .child(box().height(1).fill(Fill::color(hex(0x8a7458, 0.16f))))
                .child(console::console(logC, logStyle())));
    return g;
  }

  Element ruleNote() {
    auto g = box().absolute().left(766).top(228).width(Dim(770)).key("rulenote")
                 .opacity(gate(tFold1 - 0.2f, tFold1 + 0.8f));
    const char *rows[6] = {
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
                  .absolute().left(0).top(i ? 16.0f + (float)i * 12.2f : 0.0f)
                  .width(Dim(700)));
    g.child(text(toU8("take 30 deg per map instead (12 x 30 = 360, one dot per "
                      "star, matching the 1,339 census) and the columns come out"),
                 type(faceMono, 9.0f, hex(0xcf6a4a)))
                .absolute().left(0).top(92).width(Dim(700)));
    g.child(text(toU8("21.6 mm wide, which is not a Tang column. NEITHER READING "
                      "CLOSES. This plate draws the first, so you can see it."),
                 type(faceMono, 9.0f, hex(0xcf6a4a)))
                .absolute().left(0).top(104).width(Dim(700)));
    return g;
  }

  Element headings() {
    auto g = box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
                 .key("head");
    g.child(text(toU8("THE DUNHUANG STAR CHART, REPROJECTED"),
                 type(faceDisplay, 27.0f, hex(0xe0cfa6), 2.4f))
                .absolute().left(96).top(16).width(Dim(1200)));
    g.child(text(toU8("British Library Or.8210/S.3326 \xc2\xb7 Mogao Cave 17, "
                      "Dunhuang \xc2\xb7 +649\xe2\x80\x93" "684 \xc2\xb7 3,940 \xc3\x97 244 mm, "
                      "pure mulberry fibre 0.04 mm \xc2\xb7 1,339 dots in 257 asterisms"),
                 type(faceMono, 10.2f, hex(0x9a8a68)))
                .absolute().left(98).top(46).width(Dim(1500)));
    g.child(text(toU8("NOT TRACED. 1,460 real stars precessed J2000 \xe2\x86\x92 +700 "
                      "(IAU 1976) and pushed through Table 3's own measured "
                      "projection."),
                 type(faceMono, 10.2f, hex(0xc9a35c)))
                .absolute().left(1660).top(16).width(Dim(830)));
    g.child(text(toU8("PLATE I \xc2\xb7 north up, WEST AT RIGHT, RA increasing "
                      "right-to-left \xe2\x80\x94 the direction the scroll reads"),
                 type(faceMono, 9.4f, hex(0x6d6249)))
                .absolute().left(1660).top(34).width(Dim(830)));
    // the scale bar, in cm of real paper
    const float barMm = 100.0f;
    g.child(box().absolute().left(96).top(1546).width(Dim(barMm * kPxMm))
                .height(Dim(7))
                .outline([](SkSize s) {
                  SkPathBuilder b;
                  b.moveTo(0, 6); b.lineTo(0, 0); b.lineTo(s.width(), 0);
                  b.lineTo(s.width(), 6);
                  for (int i = 1; i < 10; ++i)
                    { b.moveTo(s.width() * (float)i / 10.0f, 0);
                      b.lineTo(s.width() * (float)i / 10.0f, i % 5 ? 3 : 5); }
                  return b.detach();
                })
                .stroke(lines::Line{.width = 1.0f,
                                    .fill = Fill::color(hex(0x9a8a68, 0.8f))}));
    g.child(text(toU8("10 cm of scroll \xc2\xb7 IDP scan 204.8 px/cm"),
                 type(faceMono, 8.6f, hex(0x6d6249)))
                .absolute().left(96 + barMm * kPxMm + 10).top(1544).width(Dim(420)));
    g.child(text(toU8("data: Stellarium chinese_chenzhuo (GPL) \xc2\xb7 "
                      "astronexus/HYG v4.1 \xc2\xb7 arXiv:0906.3034 Tables 3\xe2\x80\x93" "5 "
                      "\xc2\xb7 IDP 7861395E5F814419BA05483EAB254832"),
                 type(faceMono, 8.6f, hex(0x6d6249)))
                .absolute().left(1660).top(1544).width(Dim(880)));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &) {
    auto root = box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH));
    root.child(ground());
    root.child(locator());

    // the equatorial graticule the stars arrive in, fading as the fold runs
    root.child(box().absolute().left(108).top(250).width(Dim(2344)).height(Dim(764))
                   .key("grat")
                   .opacity(gate(tSky - 0.6f, tSky + 0.6f))
                   .zIndex(-1)
                   .outline([](SkSize s) {
                     SkPathBuilder b;
                     for (int i = 0; i <= 12; ++i) {
                       const float x = s.width() * (float)i / 12.0f;
                       b.moveTo(x, 0); b.lineTo(x, s.height());
                     }
                     for (int j = 0; j <= 6; ++j) {
                       const float y = s.height() * (float)j / 6.0f;
                       b.moveTo(0, y); b.lineTo(s.width(), y);
                     }
                     return b.detach();
                   })
                   .stroke(PathFormat{.width = 0.8f,
                                      .strokeFill = Fill::color(hex(0x2f6d86, 0.42f)),
                                      .dashIntervals = {3, 7}}));

    root.child(scrollBand(-90, kBreakL, "bandL", -0.42f));
    root.child(scrollBand(kBreakR, kW + 90, "bandR", -0.42f));
    for (int seg = 0; seg < 2; ++seg) {
      auto sg = box().absolute().left(segLo(seg)).top(kSegTop)
                    .width(Dim(segHi(seg) - segLo(seg))).height(Dim(kSegH))
                    .clip(true).key(seg ? "segR" : "segL");
      for (int k = 1; k <= 12; ++k) {
        sg.child(mapFrame(k, seg));
        sg.child(columnBand(k, seg));
      }
      sg.child(discPlate(seg));
      sg.child(discNotes(seg));
      sg.child(raRuler(seg));
      if (seg == 0) { sg.child(unreadTitle()); sg.child(archer()); }
      root.child(std::move(sg));
    }
    root.child(breakMark());

    // the star field — ONE leaf for 1,460 dots
    root.child(box().absolute().left(0).top(0).width(Dim(kW)).height(Dim(kH))
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

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas((int)kW, (int)kH);
    ctx.background(kVoid);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager()) return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    faceSerif = family("Hoefler Text", SkFontStyle::Normal());
    faceItalic = family("Hoefler Text", SkFontStyle::Italic());
    faceMono = family("Menlo", SkFontStyle::Normal());
    faceDisplay = family("Optima", SkFontStyle::Bold());
    faceHan = family("Songti SC", SkFontStyle::Normal());
    if (!faceHan) faceHan = family("PingFang SC", SkFontStyle::Normal());
    if (!faceHan) faceHan = family("Hiragino Sans", SkFontStyle::Normal());
    if (!faceSerif) faceSerif = family("Baskerville", SkFontStyle::Normal());
    if (!faceItalic) faceItalic = faceSerif;
    if (!faceMono) faceMono = family("Courier New", SkFontStyle::Normal());
    if (!faceDisplay) faceDisplay = faceSerif;
    if (!faceHan) faceHan = faceSerif;

    // the fibre runs ALONG the roll: anisotropic luminance grain, not noise
    paperGrain = patterns::grain(1.15f, 4, 3326.0f, 0.42f, 5.5f);
    paperSpeck = patterns::speckle(900, 34, 0.20f, 0.85f,
                                   {hex(0x6a5330, 0.10f), hex(0x2a2118, 0.08f)});
    paperSpeck.seed(649);

    computeJoins();
    buildAsterismArt();
    buildFixtures();

    // FIVE cells, because an atlas cell is a whole ELEMENT TREE: the black
    // ring and the school fill bake into ONE sprite, so 1,460 dots stay one
    // draw and there is no second concentric pass.
    atlas = std::make_shared<instancing::Atlas>(3.0f);
    auto dot = [](SkColor4f fill, bool ring) {
      auto e = box().width(11).height(11).outline(shapes::circle());
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

    logA.append(toU8("THE JOIN"), 1);
    logA.append(toU8(fmt("chinese_chenzhuo: %d asterisms, 1,883 vertex words",
                         nAst)), 0);
    logA.append(toU8("1,463 star TOKENS \xe2\x80\x94 3 are DSO (M44/M7/M31)"), 3);
    logA.append(toU8(fmt("  so %d HIP numbers vs Chen Zhuo's canonical 1,464",
                         nStars)), 0);
    logA.append(toU8("HYG v4.1 join on HIP: 1,457 direct, 3 LOST"), 3);
    logA.append(toU8("  55203 xi UMa, 78727 xi Sco, 115125 94 Aqr B"), 0);
    logA.append(toU8("  cause: HYG BLANKS hip on resolved double components"), 0);
    logA.append(toU8("  Bayer fallback recovers all three"), 2);
    logA.append(toU8(fmt("RESOLVED %d / %d = 100.00%%", nStars, nStars)), 2);
    logA.append(toU8("largest 1300-yr proper motion 1.476 deg (HIP 19849)"), 0);

    logB.append(toU8("THE EPOCH, AND THE DECLINATION WINDOW"), 1);
    logB.append(toU8("paper precessed to +700, NOT +665 (sect. 4.1)"), 3);
    logB.append(toU8("  665 vs 700 = 0.489 deg RA; map 5's RA residual 2.26"), 0);
    logB.append(toU8("  4.6x below the chart's own hand. UNRESOLVABLE."), 2);
    logB.append(toU8(fmt("of %d stars at +700:", nStars)), 0);
    logB.append(toU8(fmt("  %4d fall on maps 1-12  (|DEC| <= 45)", nOnMaps)), 0);
    logB.append(toU8(fmt("  %4d fall on the disc    (DEC >= +52)", nOnDisc)), 0);
    logB.append(toU8(fmt("  %4d fall in the UNCOVERED band +45..+52", nInGap)), 3);
    logB.append(toU8(fmt("  %4d are south of DEC -45, off the chart", nTooSouth)), 3);
    logB.append(toU8("Chang'an is 34.3N, so DEC < -55.7 never rises at all"), 0);

    logC.append(toU8("THE SCHOOLS, AND WHAT IS NOT ATTESTED"), 1);
    logC.append(toU8("S.3326 is the FIRST document to colour the three"), 0);
    logC.append(toU8("  schools: Shi shi RED, Gan shi BLACK, Wu Xian WHITE"), 0);
    logC.append(toU8(fmt("Tables 4+5 give a colour for 54 asterisms; %d stars",
                         nSchooled)), 0);
    logC.append(toU8(fmt("%d stars have NO published school: drawn undeclared",
                         nUnattested)), 3);
    logC.append(toU8("guessing the rest would be inventing the evidence"), 0);
    logC.append(toU8("Huagai +6 unaccounted: Chen Zhuo HAS Gang, 9 stars"), 3);
    logC.append(toU8("  9 != 6, so it is consistent and does not close"), 0);
    logC.append(toU8("Sangong: Chen Zhuo files one under WU XIAN (white),"), 0);
    logC.append(toU8("  the map draws BOTH black. printed, not corrected."), 3);

    ctx.ticker.add([this, &tick = ctx.ticker](double) {
      clockT = tick.elapsed();
      const float t = (float)std::fmod(clockT, (double)kLoop);
      scribe = t;
      const float e = 2000.0f + (700.0f - 2000.0f) *
                                    smooth((t - tPrec0) / (tPrec1 - tPrec0));
      const float f = smooth((t - tFold0) / (tFold1 - tFold0));
      reveal = clamp01((t - tLine0) / (tLine1 - tLine0));
      rebuild(e, f);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("audit", auditPanel());
  }

  /** Two render()s per loop, at the score's end and at its start. */
  void update(double, sketch::SketchContext &ctx) override {
    const float t = (float)std::fmod(clockT, (double)kLoop);
    const bool want = t >= tSettle;
    if (want != settled) {
      settled = want;
      ctx.composer.render(describe(ctx));
      ctx.composer.renderSlot("audit", auditPanel());
    }
  }
};

SIGIL_SKETCH(DunhuangStarChart)

// ---------------------------------------------------------------------------
// WHAT IT COST, AND THE ONE LINE THAT PAID FOR IT. 2560 x 1600, `--bench`,
// across the whole score:
//
//     t = 3.0   p50 6.02 / p99 7.88      the paper and the sky
//     t = 15.0  p50 8.70 / p99 9.48      317 asterisms drawing themselves
//     t = 20.0  p50 9.18 / p99 9.76      the audit walking map 5
//     t = 27.0  p50 9.76 / p99 11.06     (max 22.34 — see below)
//     t = 29.0  p50 10.09 / p99 10.83    the settled plate
//
// THE FIRST MEASUREMENT WAS 29.92 ms p50, AND 24.5 ms OF IT WAS TWO NODES.
// The two scroll bands carry a paper gradient, an anisotropic grain and a
// speckle wash over 1.5 Mpx between them, and they are ROTATED -0.42 deg
// because a scroll does not lie square. That constant rotation is enough to
// disqualify them from the library's device-space promotion — a bake pinned
// to one device rect is not matrix-independent — so `--bench` printed
// `not baked: rotated, mirrored or skewed` and three shaders re-ran over
// every pixel, every frame, for ever. One `.cache(Cache::Texture)` per band
// took the frame 29.92 -> 5.81 ms, 5.1x, and changed nothing on screen.
//
// The reading to take from that: "not baked: rotated" is not a warning about
// live animation. A node that never moves at all is refused the same way if
// its matrix is not axis-aligned, and a decorative half-degree tilt is enough
// to do it. Any rotated node with area is a node you must bake by name.
//
// THREE MORE THINGS MEASURED:
//   1. EVERY REVEAL ON THIS PLATE IS A window() ON ONE OUTPUT, and a bound
//      property is live volatility for ever — including ten seconds after its
//      value settled at 1.0. Past the score's end the same reveals are
//      described as plain constants (`gate()`), which drops ~500 nodes back
//      into their parents' recordings. Two render() calls per 31 s loop.
//   2. THE PRICE OF THAT IS ONE FRAME. The settle flip re-describes the whole
//      tree: 22.34 ms, once per 1,860 frames. p99 does not see it (11.06 ms)
//      because 120 sampled frames contain at most one. It came down from
//      worse by hoisting everything O(asterisms x vertices) out of describe()
//      — the 317 asterism paths, map 5's twenty centroids, and the 28
//      determinative RAs are all built once in setup().
//   3. 1,460 dots are 1.28 ms as ONE instancing leaf in Mode::Live, and the
//      pool rebuild early-outs whenever the epoch and the fold have not moved,
//      so the settled plate pays only the stamp. The atlas is FIVE cells and
//      no tint: a cell is a whole element tree, so the school fill and the
//      black ring bake into one sprite. The brief predicted six cells or a
//      two-pass pool for the ring; neither was needed.
