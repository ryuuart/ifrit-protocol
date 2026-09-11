#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkContourMeasure.h>
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
#include <sigilcompose/kit/Plate.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Polyline.h>
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
#include <charconv>
#include <cmath>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace patterns = sigil::material::pattern;
namespace path = sigil::geometry::path;
namespace data = sigil::data;
namespace shapers = sigil::geometry::shapers;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
namespace geometry = sigil::geometry;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace thunder_fulu {}
using namespace thunder_fulu;
namespace thunder_fulu {

// ---------------------------------------------------------------------------
// palette — cinnabar on beaten iron under an altar lamp. 朱砂 is mercury
// sulphide ground in glue: a body colour that sits ON the metal, so it is the
// only bright thing here, and where the brush runs dry the IRON shows through
// rather than a paler red.

constexpr SkColor4f kNight = hexColor(0x08070a);
constexpr SkColor4f kIronDeep = hexColor(0x232120);
constexpr SkColor4f kIronMid = hexColor(0x35312c);
constexpr SkColor4f kIronLit = hexColor(0x4c453b);
constexpr SkColor4f kIronEdge = hexColor(0x6c6354);
constexpr SkColor4f kCinnabar = hexColor(0xcf3018);
constexpr SkColor4f kCinnaWet = hexColor(0xf2542a);
constexpr SkColor4f kCinnaDry = hexColor(0x8f2313);
constexpr SkColor4f kGold = hexColor(0xb2914f);
constexpr SkColor4f kGoldDim = hexColor(0x6d5a33);
constexpr SkColor4f kChalk = hexColor(0xd8cdb6);
constexpr SkColor4f kVoidBlue = hexColor(0x4f92d8);
constexpr SkColor4f kVoidRed = hexColor(0xd8422a);
constexpr SkColor4f kVoidWhite = hexColor(0xe7e2d6);

// ---------------------------------------------------------------------------
// canvas & the plate. 五寸 × 三寸 — the ratio is the specification, so it is
// the one number nothing else is allowed to round.

constexpr float kW = 1900, kH = 1220;
constexpr float kPW = 624.0f;              // the plate: 三寸
constexpr float kPH = kPW * 5.0f / 3.0f;   // 五寸 = 1040
constexpr float kPL = 76.0f, kPT = 92.0f;  // where it stands on the canvas
constexpr float kCol = 306.0f;             // the spine: every part is strung
                                           // on it, and the asymmetries off
                                           // it are deliberate

// ---------------------------------------------------------------------------
// THE STROKE DATA, verbatim from makemeahanzi `graphics.txt`.
// Layout: per stroke, [n, x0,y0, x1,y1, … x(n-1),y(n-1)] with the 1024 em box
// already flipped to screen orientation (y = 900 − y_published). In DRAWING
// ORDER and DRAWING DIRECTION — which is the whole point of the file.

struct Glyph {
  std::string id;  // romanised: every caption on this canvas is Latin
  std::vector<int16_t> data;
  int strokes = 0;
};

enum G { GANG, JI, RU, LV, LING, YU, WU, GUI, YUN, LEI, HAO, GCOUNT };

// KanjiVG kvg:type, mapped to the six classes, in KanjiVG's stroke order.
// 罡 is ABSENT from KanjiVG entirely (kanji/07f61.svg → 404). That absence
// is why this file has a classifier at all.
enum Cls { HENG, SHU, PIE, NA, DIAN, TURN, CLSN };
inline const char* kClsName[CLSN] = {"HENG", "SHU ", "PIE ",
                                     "NA  ", "DIAN", "TURN"};

// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct KvgRow {
  int glyph;
  std::vector<int8_t> cls;
  int n;
};

/** THE TWO FILES THIS CANVAS IS WRITTEN FROM, as one value: the eleven
 *  characters' medians in drawing order, and the eight of them KanjiVG
 *  also classifies. */
struct Font {
  std::vector<Glyph> glyphs;
  std::vector<KvgRow> kvg;

  /** Which of the eleven carries @p id, or -1. */
  int find(std::string_view id) const {
    for (size_t i = 0; i < glyphs.size(); ++i)
      if (glyphs[i].id == id) return (int)i;
    return -1;
  }
};

/** The whitespace-separated numbers of one field. */
template <typename T>
inline std::vector<T> words(std::string_view run) {
  std::vector<T> out;
  for (size_t at = 0; at < run.size();) {
    if (run[at] == ' ') {
      ++at;
      continue;
    }
    int value = 0;
    const auto read =
        std::from_chars(run.data() + at, run.data() + run.size(), value);
    if (read.ec != std::errc{}) break;
    out.push_back((T)value);
    at = (size_t)(read.ptr - run.data());
  }
  return out;
}

inline Font readFont(sketch::Assets& assets) {
  Font font;
  const auto file = [&assets](const char* name) {
    return assets.table("data/thunder_fulu/" + std::string(name));
  };

  if (const auto t = file("strokes.csv")) {
    const auto id = t->column<std::string>("id");
    const auto strokes = t->column<double>("strokes");
    const auto medians = t->column<std::string>("medians");
    for (size_t i = 0; i < id.size(); ++i)
      font.glyphs.push_back(
          {id[i], words<int16_t>(medians[i]), (int)strokes[i]});
  }

  if (const auto t = file("kanjivg.csv")) {
    const auto id = t->column<std::string>("id");
    const auto classes = t->column<std::string>("classes");
    for (size_t i = 0; i < id.size(); ++i) {
      std::vector<int8_t> cls = words<int8_t>(classes[i]);
      const int n = (int)cls.size();
      font.kvg.push_back({font.find(id[i]), std::move(cls), n});
    }
  }

  return font;
}

// ---------------------------------------------------------------------------
// MEDIANS → GEOMETRY.

using Poly = std::vector<SkPoint>;

/** One character's medians, in em units [0,1]², y down, drawing order. */
inline std::vector<Poly> medians(const Font& font, int glyph) {
  const Glyph& g = font.glyphs[(size_t)glyph];
  std::vector<Poly> out;
  const int16_t* p = g.data.data();
  for (int s = 0; s < g.strokes; ++s) {
    const int n = *p++;
    Poly poly;
    poly.reserve((size_t)n);
    for (int i = 0; i < n; ++i, p += 2)
      poly.push_back({(float)p[0] / 1024.0f, (float)p[1] / 1024.0f});
    out.push_back(std::move(poly));
  }
  return out;
}

/** N stations spaced evenly by ARC LENGTH along a median — the
 *  classifier's own measure, and what hanzi-writer and animCJK do before
 *  animating. `path::resample` is that walk; what is here is only the
 *  conversion between this file's SkPoint medians and the library's
 *  glm points. */
inline Poly resample(const Poly& p, int n) {
  if (p.size() < 2) return p;
  path::Polyline line;
  line.points.reserve(p.size());
  for (const SkPoint& q : p) line.points.push_back({q.fX, q.fY});
  const path::Sampled r = path::resample(line, n);
  Poly out;
  out.reserve(r.points.size());
  for (const glm::vec2& q : r.points) out.push_back({q.x, q.y});
  return out;
}

struct Feat {
  float length = 0;  // em
  float angle = 0;   // deg, body chord, +x right, +y DOWN
  float turn = 0;    // deg, peak deviation from the entry direction
};

/** The three numbers the taxonomy actually turns on. The 起筆 and 收筆
 *  flicks live in the outer 14% at each end and swing the chord by up to
 *  90°, so the body span — and only the body span — is measured. */
inline Feat features(const Poly& m) {
  Feat f;
  const Poly r = resample(m, 33);
  if (r.size() < 3) return f;
  for (size_t i = 0; i + 1 < m.size(); ++i)
    f.length += std::hypot(m[i + 1].fX - m[i].fX, m[i + 1].fY - m[i].fY);
  const size_t a = (size_t)(0.14f * 32), b = (size_t)(0.86f * 32);
  const Poly body(r.begin() + (long)a, r.begin() + (long)b + 1);
  const float a0 = std::atan2(body[1].fY - body[0].fY, body[1].fX - body[0].fX);
  float turn = 0;
  for (size_t i = 1; i + 1 < body.size(); ++i) {
    const float dx = body[i + 1].fX - body[i].fX;
    const float dy = body[i + 1].fY - body[i].fY;
    if (std::hypot(dx, dy) < 1e-9f) continue;
    float t = std::atan2(dy, dx) - a0;
    while (t > SK_ScalarPI) t -= 2 * SK_ScalarPI;
    while (t < -SK_ScalarPI) t += 2 * SK_ScalarPI;
    turn = std::max(turn, std::abs(t));
  }
  f.turn = turn * 57.29578f;
  f.angle = std::atan2(body.back().fY - body.front().fY,
                       body.back().fX - body.front().fX) *
            57.29578f;
  return f;
}

/** The recovered stroke class. Validated against KanjiVG in panel B. */
inline int classify(const Poly& m) {
  const Feat f = features(m);
  if (f.turn > 62.0f) return TURN;
  if (f.length < 0.20f) return DIAN;
  if (std::abs(f.angle) < 27.0f) return HENG;
  if (f.angle >= 27.0f && f.angle <= 72.0f) return NA;
  if (f.angle > 100.0f || f.angle < -100.0f) return PIE;
  return SHU;
}

/** 提按 — lift and press. w₀ as a fraction of the glyph's em box: a 橫 is
 *  thin, a 捺 is fat, a 點 is nearly all 頓. */
inline float w0ForClass(int cls) {
  static const float k[CLSN] = {0.0255f, 0.0310f, 0.0292f,
                                0.0440f, 0.0470f, 0.0318f};
  return k[cls];
}

/** 起 · 行 · 收 — the width law, normalised to w₀. Flagged as a MODEL: the
 *  shape is what the robotic-calligraphy literature fits (two symmetric
 *  cubics either side of the median, B-BSMG 2024); the constants are chosen
 *  so 逆鋒 reads at 1.77, the belly bottoms at 0.73 and the 頓 press peaks
 *  at 1.42 — the ratios a loaded wolf-hair brush actually leaves. */
inline float widthLaw(float s) {
  s = std::clamp(s, 0.0f, 1.0f);
  const float d = s - 0.88f;
  return 1.15f * std::exp(-12.0f * s) + 0.62f + 0.28f * s +
         0.55f * std::exp(-90.0f * d * d);
}

/** The law's peak, in units of w₀. It is widthLaw(0) = 1.15 + 0.62 exactly
 *  (the 頓 gaussian is e^-69.7 there, i.e. nothing), and the law falls
 *  immediately from there — so the 逆鋒 entry IS the maximum, and it is the
 *  1.77 the comment above quotes. Every profile below returns it from
 *  `max()`, which is what the cull bounds are grown by: understate it and
 *  the widest part of the band gets clipped at the node's edge. */
constexpr float kWidthLawMax = 1.77f;

/** ONE STROKE'S WIDTH, as a comparable Profile keyed in PX of arc length.
 *
 *  `alongIsPx` is load-bearing. Every stroke on this plate is written under
 *  a reveal — `spans::upTo(bind(&scribe).window(t0, t1))` — so the
 *  decoration is handed the REVEALED contour, and a fraction would be a
 *  fraction of what has been drawn SO FAR. Keyed that way the 頓 press
 *  would slide down the stroke as it writes; keyed in px against `fullLen`,
 *  the length the stroke was AUTHORED at, the press stays where the brush
 *  puts it. `fullLen` must therefore be the whole spine's length, measured
 *  once, and never the revealed part. */
struct StrokePress {
  float fullLen = 1.0f;
  float w0 = 6.0f;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    return w0 * widthLaw(fullLen > 0 ? px / fullLen : 0.0f);
  }
  float max() const { return w0 * kWidthLawMax; }
  bool operator==(const StrokePress&) const = default;
};

/** 一氣立斷 — THE FOOT, one contour of 75 spans (38 strokes and 37
 *  ligatures) and no lift. Same px key, same reason; the table itself is
 *  in absolute px along the contour, so nothing about it can be expressed
 *  as a fraction of a reveal.
 *
 *  A span of zero width is the LIGATURE: the brush never leaves the
 *  plate, it only lightens — so the floor is w₀·0.16 everywhere, and
 *  that floor is also what the gaps between spans read. */
struct FootPress {
  std::vector<std::array<float, 3>> spans;  // {d0, d1, w} — d in px
  float w0 = 6.0f;
  static constexpr bool alongIsPx = true;
  float across(float px) const {
    for (const std::array<float, 3>& sp : spans)
      if (px >= sp[0] && px <= sp[1]) {
        if (sp[2] <= 0.0f) return w0 * 0.16f;
        const float u = (px - sp[0]) / std::max(1e-3f, sp[1] - sp[0]);
        return w0 * sp[2] * widthLaw(u);
      }
    return w0 * 0.16f;
  }
  float max() const {
    float widest = 0.16f;
    for (const std::array<float, 3>& sp : spans)
      widest = std::max(widest, sp[2] * kWidthLawMax);
    return w0 * widest;
  }
  bool operator==(const FootPress&) const = default;
};

/** The law PLOTTED — the specimen bands on the field-notes page. Keyed in
 *  FRACTION, and that is right here: these two sit under an unqualified
 *  stroke with no reveal on the node, so `along` is a fraction of the
 *  whole spine every frame. They are also the reason the plate can print
 *  the law: this is the same widthLaw the ink uses, drawn once across a
 *  straight axis and once across each of the six class specimens. */
struct LawBand {
  float w0 = 21.0f;
  float across(float along) const { return w0 * widthLaw(along); }
  float max() const { return w0 * kWidthLawMax; }
  bool operator==(const LawBand&) const = default;
};

/** Polyline → a smooth path through it (quads through the midpoints). The
 *  medians are sparse on purpose — 3 to 14 points for a whole stroke — so
 *  the smoothing is not decoration, it is what makes the centreline a
 *  centreline rather than a chain of chords. */
inline SkPath smoothPath(const Poly& p) {
  std::vector<glm::vec2> pts;
  pts.reserve(p.size());
  for (const SkPoint& q : p) pts.push_back({q.fX, q.fY});
  return path::smoothThrough(pts);
}

inline float pathLength(const SkPath& p) {
  float total = 0;
  SkContourMeasureIter it(p, false);
  while (sk_sp<SkContourMeasure> c = it.next()) total += c->length();
  return total;
}

/** Place an em-unit polyline into a box. */
inline Poly place(const Poly& m, SkPoint origin, float sx, float sy) {
  Poly out;
  out.reserve(m.size());
  for (const SkPoint& q : m)
    out.push_back({origin.fX + q.fX * sx, origin.fY + q.fY * sy});
  return out;
}

// ---------------------------------------------------------------------------
// THE BIG DIPPER, and the tread on it.
//
// J2000 RA/Dec, gnomonically projected about the asterism's own centroid and
// normalised so the seven stars span 1.0 in x. 左輔 Alcor comes out 0.0079
// from 開陽 Mizar, which is the true sky and useless as a station — every
// 步罡 plate separates the pair by hand, and so does the margin note here.

struct Star {
  float x, y;
  const char* name;
  const char* ritual;
};
const Star kDipper[9] = {
    {0.9886f, 0.0000f, "Dubhe", "TIAN SHU"},
    {1.0000f, 0.2288f, "Merak", "TIAN XUAN"},
    {0.6862f, 0.3434f, "Phecda", "TIAN JI"},
    {0.5559f, 0.2011f, "Megrez", "TIAN QUAN"},
    {0.3229f, 0.2469f, "Alioth", "YU HENG"},
    {0.1425f, 0.2909f, "Mizar", "KAI YANG"},
    {0.0000f, 0.5302f, "Alkaid", "YAO GUANG"},
    // 左輔 Alcor: measured at (0.1347, 0.2883) — 0.0079 from Mizar. Pushed
    // out along the handle's normal to be a station, and said so.
    {0.0640f, 0.4020f, "Alcor", "ZUO FU"},
    // 右弼 is invisible. Its station is doctrine, not observation.
    {0.2180f, 0.4390f, "(invisible)", "YOU BI"},
};

// The twelve talismans of 卷四十六, verbatim and in the order they appear.
// 斬虹符 is the hero; the other eleven stand on the tread.
struct Fu {
  const char* pinyin;
  const char* gloss;
};
const Fu kOthers[11] = {
    {"QI FENG FU", "raise wind"},        {"QI YUN FU", "raise cloud"},
    {"QI YU FU", "pray for rain"},       {"QI LEI FU", "raise thunder"},
    {"QI DIAN FU", "raise lightning"},   {"QI XUE FU", "pray for snow"},
    {"JIANG BAO FU", "bring down hail"}, {"JUAN WU FU", "roll up fog"},
    {"QI QING FU", "pray for clear"},    {"ZHAO LONG FU", "summon the dragon"},
    {"JIANG XUE FU", "bring down snow"},
};

// ---------------------------------------------------------------------------
// THE SCORE. Seconds. The tempo marks are quotations, not choices.

constexpr float tPlate = 0.05f, tPlateEnd = 1.15f;
constexpr float tLoad = 1.20f;
constexpr float tHead = 1.90f, tHeadEach = 1.00f, tHeadGap = 0.05f;
constexpr float tRing = 5.05f, tRingDur = 1.15f;
constexpr float tVoid = 6.35f, tVoidDur = 1.40f;
constexpr float tBody = 7.85f, tBodyEach = 0.240f;   // 33 strokes → 7.92 s
constexpr float tGall = 15.95f, tGallEach = 0.360f;  // 10 strokes → 3.60 s
constexpr float tTap = 19.65f, tTapDur = 0.25f;
constexpr float tFoot = 20.00f, tFootEach = 0.034f;  // 38 strokes → 1.292 s
constexpr float tSeal = 21.45f;
constexpr float tStars = 13.10f;
constexpr float kLoop = 27.0f;

inline weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                             float tracking = 0) {
  return weave::textStyle(
      {.face = std::move(face), .size = size, .color = c, .track = tracking});
}

// the 踏符頭 chant: one line said silently per hook, as the hook goes down
inline const char* kHeadChant[3] = {
    "1st stroke  the world moves",
    "2nd stroke  the patriarch's sword",
    "3rd stroke  malign gods, a thousand li hence",
};
// the six phrases sung WHILE 罡 is drawn — the chant must finish exactly as
// the tenth stroke lands
inline const char* kGallChant[6] = {
    "open the gate of heaven", "kill the ghost-road",
    "open the earth-prison",   "bar the road of men",
    "slay the ghost-troops",   "break the ghost's belly",
};

}  // namespace thunder_fulu

// ===========================================================================
