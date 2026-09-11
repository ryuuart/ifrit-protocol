#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Projection.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/stats/Fit.h>
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
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "Catalogue.h"

namespace sketch = sigil::sketch;
namespace data = sigil::data;
namespace field = sigil::material::field;
namespace patterns = sigil::material::pattern;
namespace arrange = sigil::geometry::arrange;
namespace path = sigil::geometry::path;
namespace shapers = sigil::geometry::shapers;
namespace measure = sigil::measure;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace dunhuang;
using sigil::material::skia::Paint;
namespace noise = sigil::core::noise;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace dunhuang_star_chart {}
using namespace dunhuang_star_chart;
namespace dunhuang_star_chart {

// ---------------------------------------------------------------------------
// palette — mulberry paper, 0.04 mm, thirteen centuries old, fully lined with
// brown Kraft in the 1950s. The ink is a carbon black that has gone brown at
// the edges of every stroke; the three schools are cinnabar, that same
// carbon, and an ochre-white lead that has oxidised warm.

constexpr SkColor4f kVoid = hexColor(0x14120e);
constexpr SkColor4f kPaperDeep = hexColor(0xb99f72);
constexpr SkColor4f kPaperMid = hexColor(0xd6bf95);
constexpr SkColor4f kPaperLit = hexColor(0xe8d6ad);
constexpr SkColor4f kPaperPale = hexColor(0xf1e4c2);
constexpr SkColor4f kKraft = hexColor(0xa87f4c);
constexpr SkColor4f kInk = hexColor(0x2a2118);
constexpr SkColor4f kInkSoft = hexColor(0x5d4c37);
constexpr SkColor4f kInkFaint = hexColor(0x8a7458);
constexpr SkColor4f kCinnabar = hexColor(0xa8382a);
constexpr SkColor4f kLead = hexColor(0xf4ecd8);
constexpr SkColor4f kRule = hexColor(0x6b573c);
constexpr SkColor4f kTrace = hexColor(0x2f6d86);
constexpr SkColor4f kFlag = hexColor(0xb4531f);
constexpr SkColor4f kChalk = hexColor(0xcbb894);

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

inline float smooth(float v) {
  v = std::clamp(v, 0.0f, 1.0f);
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
// 0 → −13.00 cy over the score, J2000 → +700. The three published angles
// are the astronomy and stand here; the turn of the sphere they name is
// path::Rotation's three-angle form.

inline path::Rotation precession(float T) {
  const float s = 1.0f / 3600.0f;  // the angles are published in arcseconds
  const float zeta =
      (2306.2181f * T + 0.30188f * T * T + 0.017998f * T * T * T) * s;
  const float z =
      (2306.2181f * T + 1.09468f * T * T + 0.018203f * T * T * T) * s;
  const float theta =
      (2004.3109f * T - 0.42665f * T * T - 0.041833f * T * T * T) * s;
  return path::Rotation::zyz(z, -theta, zeta);
}

/** The catalogue is held as two float lanes per star, so the turn is read
 *  back into the pair it came out of. */
inline void precess(const path::Rotation& turn, float ra, float dec,
                    float& raOut, float& decOut) {
  const path::Spherical p = turn({ra, dec});
  raOut = p.lonDeg;
  decOut = p.latDeg;
}

// ---------------------------------------------------------------------------
// THE CATALOGUE — the 1,460 stars, the 317 asterisms with their vertex
// words, and the 28 mansion anchors — is read from the data files
// beside this sketch by Catalogue.cpp: numbers the join produced once,
// held where numbers belong, so an edit here never touches them.

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

/** The two candidate projections, at unit scale: the chart is tested for
 *  how far each one's ordinate departs from a ruler, and a scale would
 *  cancel out of that. The stereographic is centred on the equator, which
 *  is the aspect a strip of sky along it is the azimuthal twin of. */
const path::Projection kMercator{.scheme = path::Scheme::Mercator};
const path::Projection kStereographic{.scheme = path::Scheme::Stereographic,
                                      .scale = 0.5f};

/** THE ORDINATE A PROJECTION LAYS DOWN, sampled over [lo, hi] in @p n + 1
 *  even steps — read off the scheme itself, so the curve drawn and the
 *  number reported cannot be two spellings of one law. */
inline std::vector<float> ordinate(float lo, float hi, bool mercator, int n) {
  std::vector<float> ys;
  ys.reserve((size_t)n + 1);
  for (int i = 0; i <= n; ++i) {
    const float v = arrange::along(lo, hi - lo, (size_t)i, (size_t)n + 1,
                                   arrange::Turn::Open);
    ys.push_back((mercator ? kMercator : kStereographic).radiusAt(v));
  }
  return ys;
}
inline std::vector<float> abscissa(float lo, float hi, int n) {
  std::vector<float> xs;
  xs.reserve((size_t)n + 1);
  for (int i = 0; i <= n; ++i)
    xs.push_back(arrange::along(lo, hi - lo, (size_t)i, (size_t)n + 1,
                                arrange::Turn::Open));
  return xs;
}

/** How far the ordinate departs from its own best-fit straight line, back
 *  in DEGREES: the max residual divided by the fitted slope, which is the
 *  degrees of declination that residual is worth.
 *
 *  The fit is `measure::lineFit` in FLOAT, and the precision is the point:
 *  the number printed and the curve drawn from it come off one set of
 *  sums, so they cannot report different fits of the same ordinate. */
inline Departure departure(float lo, float hi, bool mercator, float resid,
                           float degPerCm, float r, int n) {
  const int N = 400;
  const std::vector<float> xs = abscissa(lo, hi, N);
  const std::vector<float> ys = ordinate(lo, hi, mercator, N);
  const measure::LineFit<float> fit = measure::lineFit<float>(xs, ys);
  const float mx = fit.maxResidual / std::abs(fit.slope);
  const float se = (1.0f - r * r) / std::sqrt((float)(n - 1));
  return {mx, mx / degPerCm * 10.0f, mx / resid, se};
}

inline Departure mercatorVsLinear(float lo, float hi, float resid,
                                  float degPerCm, float r, int n) {
  return departure(lo, hi, true, resid, degPerCm, r, n);
}
inline Departure stereoVsEquidistant(float lo, float hi, float resid,
                                     float degPerCm, float r, int n) {
  return departure(lo, hi, false, resid, degPerCm, r, n);
}

// ---------------------------------------------------------------------------
// TABLE 4 — the content of map 5 (the Orion region), transcribed from
// arXiv:0906.3034 verbatim, defects and all. `cid` keys into the Chen Zhuo
// dataset above; 'M' in `col` is a mixed-colour asterism.

struct M5Row {
  std::string cid, pinyin, native, gloss;
  char school = ' ';
  int sxc = 0, map = 0, confidence = 0;
  std::string defect;
};

struct M13Row {
  std::string cid, pinyin, native;
  char school = ' ';
  int sxc = 0, map = 0;
  std::string note;
};

/** The two concordances, read from the files beside this sketch: what
 *  the paper writes against what Chen Zhuo's catalogue holds, one row
 *  per asterism the published table lists. */
struct Concordance {
  std::vector<M5Row> map5;
  std::vector<M13Row> map13;

  /** Map 5's row @p i, and an empty row where the files answered none. */
  const M5Row& five(int i) const {
    static const M5Row none;
    return i >= 0 && (size_t)i < map5.size() ? map5[(size_t)i] : none;
  }
};

inline Concordance readConcordance(sketch::Assets& assets) {
  Concordance c;
  const auto file = [&assets](const char* name) {
    return assets.table("data/dunhuang/" + std::string(name));
  };
  const auto letter = [](const std::string& text) {
    return text.empty() ? ' ' : text[0];
  };

  if (const auto t = file("map5.csv")) {
    const auto cid = t->column<std::string>("cid");
    const auto pinyin = t->column<std::string>("pinyin");
    const auto native = t->column<std::string>("native");
    const auto gloss = t->column<std::string>("gloss");
    const auto school = t->column<std::string>("school");
    const auto sxc = t->column<double>("sxc");
    const auto map = t->column<double>("map");
    const auto confidence = t->column<double>("confidence");
    const auto defect = t->column<std::string>("defect");
    for (size_t i = 0; i < cid.size(); ++i)
      c.map5.push_back({cid[i], pinyin[i], native[i], gloss[i],
                        letter(school[i]), (int)sxc[i], (int)map[i],
                        (int)confidence[i], defect[i]});
  }

  if (const auto t = file("map13.csv")) {
    const auto cid = t->column<std::string>("cid");
    const auto pinyin = t->column<std::string>("pinyin");
    const auto native = t->column<std::string>("native");
    const auto school = t->column<std::string>("school");
    const auto sxc = t->column<double>("sxc");
    const auto map = t->column<double>("map");
    const auto note = t->column<std::string>("note");
    for (size_t i = 0; i < cid.size(); ++i)
      c.map13.push_back({cid[i], pinyin[i], native[i], letter(school[i]),
                         (int)sxc[i], (int)map[i], note[i]});
  }

  return c;
}

/** Distinct stars in an asterism. `AstRec::stars` is the VERTEX count and a
 *  Chen Zhuo polyline revisits stars (東井 is 14 vertices over 9 stars), so
 *  comparing it to Table 4's n(SXC) compares two different things. */
inline int astUnique(const Catalogue& cat, const AstRec& A) {
  int n = 0;
  uint16_t seen[64];
  for (int w = 0; w < A.words && n < 64; ++w) {
    const uint16_t v = cat.verts[A.first + w];
    if (v == 0xFFFF) continue;
    bool dup = false;
    for (int j = 0; j < n; ++j) dup |= seen[j] == v;
    if (!dup) seen[n++] = v;
  }
  return n;
}

inline weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                             float tracking = 0) {
  return weave::textStyle(
      {.face = std::move(face), .size = size, .color = c, .track = tracking});
}

inline SkColor4f schoolInk(char c) {
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

}  // namespace dunhuang_star_chart

// ===========================================================================
