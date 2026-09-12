#pragma once

#include <include/core/SkBitmap.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Weave.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Length.h>
#include <sigilweave/style/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;

namespace field = sigil::material::field;
namespace matkit = sigil::material::kit;
namespace measure = sigil::measure;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using sigil::material::skia::Paint;
// The whole composition is pinned — there is no layout in a pattern
// card — so every panel is a box at absolute card coordinates.
using sigil::compose::kit::at;
using sigil::geometry::path::centred;
using namespace std::chrono_literals;
using namespace sigil::weave::literals;

namespace {

// ---------------------------------------------------------------------------
// The card it is mounted on. Black Watch is a dark cloth, and it sits on
// manila board, so this whole plate is light-on-dark inverted: pale ground,
// dark ink. `material::kit::board` is that board: one paint under a fine
// tooth and a slow wear, generated per pixel from its parameters — the
// ground and its grain as ONE node rather than a flat fill with a
// multiplied noise laid over it.

constexpr SkColor4f kCard = hexColor(0xE8E2D6);
constexpr SkColor4f kWell = hexColor(0xDCD4C4);
constexpr SkColor4f kRule = hexColor(0x8A8478);
constexpr SkColor4f kInk = hexColor(0x1A1815);
constexpr SkColor4f kInk2 = hexColor(0x5A554C);
constexpr SkColor4f kRed = hexColor(0x9A3324);

// ---------------------------------------------------------------------------
// The colours. Codes are the register's: K black, B blue, G green, Y yellow,
// W white (the warm one — undyed wool, "lachdann"). Five shade cards, ONE
// thread count: the register publishes nineteen permitted greens, sixteen
// blues and two blacks, and that is the entire mechanism behind the named
// palettes. The cloth's identity is the count; the colour is a variable.

enum Col : uint8_t { K = 0, B = 1, G = 2, Y = 3, W = 4 };
constexpr int kColCount = 5;

struct Palette {
  const char* name;
  uint32_t k, b, g;
};
constexpr std::array<Palette, 5> kPalettes{{
    {"MODERN / ORDINARY", 0x101010, 0x2C2C80, 0x006818},
    {"ANCIENT / OLD COLOURS", 0x1C1C1C, 0x2C4084, 0x5C6428},
    {"MUTED", 0x1C1714, 0x202060, 0x3F5642},
    {"WEATHERED / FADED", 0x4C3428, 0x5F749C, 0x767E52},
    {"REPRODUCTION (DALGLIESH)", 0x555A64, 0x5C8CA8, 0x789484},
}};
constexpr uint32_t kHexY = 0xE8C000;  // "Yellow"
constexpr uint32_t kHexW = 0xE5DDD1;  // "White"

using Shades = std::array<SkColor4f, 5>;
inline Shades shadesOf(const Palette& p) {
  return {hexColor(p.k), hexColor(p.b), hexColor(p.g), hexColor(kHexY),
          hexColor(kHexW)};
}

// ---------------------------------------------------------------------------
// THE SETT — Douglas 1949, transcribed literally, unit by unit. Do not merge,
// do not mirror: Douglas prints the WHOLE repeat as A + B + C + B, where the
// register's own notation would print only the half between the pivots. Take
// both and you get a 504-thread cloth that looks perfectly fine and is twice
// the right size, which is exactly what invariant #1 is for.

// A run is `threads of shade`, and the shade is an index into whichever of
// the five cards above is mounted: the library's own unit, because a
// threadcount is the cloth's identity and the colour is a variable.
using Run = patterns::ThreadRun;

// A   18 black  6 blue  2 black  6 blue  2 black 18 blue
//      2 black  6 blue  2 black  6 blue 18 black                    (86)
// NOLINTBEGIN(bugprone-throwing-static-initialization): literal tables; only
// allocation could throw
const std::vector<Run> kSettA{{18, K}, {6, B}, {2, K}, {6, B}, {2, K}, {18, B},
                              {2, K},  {6, B}, {2, K}, {6, B}, {18, K}};
// B   18 green  6 black 18 green                                    (42)
const std::vector<Run> kSettB{{18, G}, {6, K}, {18, G}};
// C   18 black 18 blue  2 black  6 blue  2 black 18 blue 18 black   (82)
const std::vector<Run> kSettC{{18, K}, {18, B}, {2, K}, {6, B},
                              {2, K},  {18, B}, {18, K}};
//                                                     Repeat B.  252 ends.
constexpr int kPublishedEnds = 252;  // the total Douglas himself prints

// CAMPBELL (of Argyll) — same pamphlet, transcribed complete. Identical
// structure, run for run; the two black centre-lines of the green squares are
// a YELLOW and a WHITE overcheck instead. That is the documented Wilsons-of-
// Bannockburn mechanism for producing "clan" tartans, caught in the act in a
// 1949 weaving manual.
const std::vector<Run> kArgA{{30, K}, {6, B}, {6, K}, {6, B}, {6, K}, {30, B},
                             {6, K},  {6, B}, {6, K}, {6, B}, {30, K}};
const std::vector<Run> kArgB{{32, G}, {6, Y}, {32, G}};
const std::vector<Run> kArgC{{30, K}, {30, B}, {6, K}, {6, B},
                             {6, K},  {30, B}, {30, K}};
const std::vector<Run> kArgD{{32, G}, {6, W}, {32, G}};
// NOLINTEND(bugprone-throwing-static-initialization)
constexpr int kPublishedArgyll = 416;

inline int sumRuns(const std::vector<Run>& r) {
  int t = 0;
  for (const Run& x : r) t += x.threads;
  return t;
}
inline std::vector<Run> concatRuns(const std::vector<std::vector<Run>>& units) {
  std::vector<Run> out;
  for (const auto& u : units) out.insert(out.end(), u.begin(), u.end());
  return out;
}

// ---------------------------------------------------------------------------
// THE LOOM — the stored program, three lines of it.
//
//   threading[j] = j mod 4                 straight draw: 1,2,3,4,1,2,3,4,...
//   treadling[i] = i mod 4                 as-drawn-in ("tromp as writ")
//   tieup[t]     = { t, (t+1) mod 4 }      each treadle lifts two shafts
//
// Warp end j is lifted on pick i iff threading[j] is in tieup[treadling[i]],
// which is two over, two under, stepping one end per pick — the 2/2 twill,
// and with y increasing downward it puts the rib on the "\" diagonal, which
// is what the register's swatch shows.
constexpr patterns::Weave kTwill = patterns::Weave::twill(2, 2);

/** Whether the warp is up, at this loom's own interlacing. */
inline bool warpUp(int x, int y) { return patterns::warpUp(kTwill, x, y); }

/** The whole artefact as the library reads it: one threadcount used for
 *  BOTH directions — the register says the weft sequence IS the warp
 *  sequence, and Douglas's as-drawn-in treadling says the same about the
 *  picks — over the shade card `sh`, with `rib` darkening the picks where
 *  the weft is on top. */
inline patterns::Cloth cloth(const std::vector<uint8_t>& S, const Shades& sh,
                             float rib = 0.0f) {
  return {.warp = S,
          .weft = S,
          .shades = {sh.begin(), sh.end()},
          .weave = kTwill,
          .rib = rib};
}

// ---------------------------------------------------------------------------
// THE INVARIANTS. Six exact checks, all computed here, none asserted. The
// cheap ones do not catch the commonest bug: build the 24 bands as inclusive
// ranges and the sum still says 252, the sequence is still palindromic and the
// cloth still looks right. Only point-sampled coverage sees it.
//
// test::endpointDegrees deliberately does NOT appear. A weave has no chained
// contour to close; its integrity is arithmetic (#3) and areal (#6), never
// topological, and forcing the other assertion on it would prove nothing.

struct Verdict {
  int unitA = 0, unitB = 0, unitC = 0, total = 0;
  bool closes = false;
  std::vector<int> mirrors;
  int mirrorGap = 0;
  bool reflective = false;
  int maxWarpFloat = 0, maxWeftFloat = 0;
  bool balanced = false;
  std::array<int, 5> counts{};
  int gcd3 = 1;
  bool blueIsThird = false;
  int solids = 0, blends = 0, perceived = 0, predicted = 0;
  bool colourLaw = false;
  int samples = 0, uncovered = 0, doubled = 0;
  bool exactCover = false;
  int argyllTotal = 0, argyllMirrors = 0, argyllSolids = 0, argyllPerceived = 0,
      argyllPredicted = 0;
  bool argyllLaw = false;
  float unitDrift = 0;  // max |BW unit fraction - CA unit fraction|
};

/** Solid and blend colour counts over the whole sett square. The 2/2 twill is
 *  balanced, so over any 4x4 block the crossing of warp a with weft b shows
 *  eight cells of each: the perceived colour is the UNORDERED pair, which is
 *  where n(n+1)/2 comes from. */
inline void perceivedColours(const std::vector<uint8_t>& S, int& solid,
                             int& blend) {
  bool seen[kColCount][kColCount] = {};
  const int n = (int)S.size();
  for (int x = 0; x < n; ++x)
    for (int y = 0; y < n; ++y) {
      const uint8_t a = S[(size_t)x], b = S[(size_t)y];
      seen[std::min(a, b)][std::max(a, b)] = true;
    }
  solid = blend = 0;
  for (int i = 0; i < kColCount; ++i)
    for (int j = i; j < kColCount; ++j)
      if (seen[i][j]) (i == j ? solid : blend)++;
}

inline int gcdOf(int a, int b) { return b ? gcdOf(b, a % b) : a; }

Verdict verify(const std::vector<Run>& bwRuns, const std::vector<uint8_t>& S,
               const std::vector<uint8_t>& A) {
  Verdict v;

  // #1 -- the sett closes, against the total the SOURCE prints.
  v.unitA = sumRuns(kSettA);
  v.unitB = sumRuns(kSettB);
  v.unitC = sumRuns(kSettC);
  v.total = (int)S.size();
  v.closes = v.unitA + v.unitB + v.unitC + v.unitB == v.total &&
             v.total == kPublishedEnds;

  // #2 -- reflective: exactly two mirror boundaries, exactly half apart.
  v.mirrors = patterns::pivots(S);
  if (v.mirrors.size() == 2) {
    v.mirrorGap = v.mirrors[1] - v.mirrors[0];
    v.reflective = v.mirrorGap == v.total / 2;
  }

  // #3 -- 2/2 balance: runs of warpUp down each column, runs of !warpUp
  // across each row. Nothing anywhere may float longer than two.
  const int n = v.total;
  for (int x = 0; x < n; ++x) {
    int run = 0;
    for (int y = 0; y < 2 * n; ++y)
      if (warpUp(x, y))
        v.maxWarpFloat = std::max(v.maxWarpFloat, ++run);
      else
        run = 0;
  }
  for (int y = 0; y < n; ++y) {
    int run = 0;
    for (int x = 0; x < 2 * n; ++x)
      if (!warpUp(x, y))
        v.maxWeftFloat = std::max(v.maxWeftFloat, ++run);
      else
        run = 0;
  }
  v.balanced = v.maxWarpFloat == 2 && v.maxWeftFloat == 2;

  // #4 -- thread ratio.
  for (uint8_t c : S) v.counts[c]++;
  v.gcd3 = gcdOf(gcdOf(v.counts[K], v.counts[B]), v.counts[G]);
  v.blueIsThird = v.counts[B] * 3 == v.total;

  // #5 -- the colour law.
  perceivedColours(S, v.solids, v.blends);
  v.perceived = v.solids + v.blends;
  v.predicted = v.solids * (v.solids + 1) / 2;
  v.colourLaw = v.perceived == v.predicted;

  // #6 -- exact cover. The 24 warp bands crossed with the 24 weft bands must
  // tile the sett square with no gap and no overlap. This is the only one of
  // the six that sees a one-thread band overlap.
  std::vector<SkPath> cells;
  cells.reserve(bwRuns.size() * bwRuns.size());
  std::vector<float> start;
  float cur = 0;
  for (const Run& r : bwRuns) {
    start.push_back(cur);
    cur += (float)r.threads;
  }
  for (size_t i = 0; i < bwRuns.size(); ++i)
    for (size_t j = 0; j < bwRuns.size(); ++j)
      cells.push_back(SkPath::Rect(SkRect::MakeXYWH(start[i], start[j],
                                                    (float)bwRuns[i].threads,
                                                    (float)bwRuns[j].threads)));
  const test::Coverage cov =
      test::coverage(cells, SkRect::MakeWH((float)n, (float)n), 256);
  v.samples = cov.samples;
  v.uncovered = cov.uncovered;
  v.doubled = cov.doubled;
  v.exactCover = cov.exact();

  // Campbell of Argyll: same law, five colours, and NO exact mirror — the
  // yellow overcheck and the white one are different colours, so it is
  // reflective in proportion and asymmetric in colour. The register would
  // record it with the "..." repeating notation rather than a "/" pivot.
  v.argyllTotal = (int)A.size();
  v.argyllMirrors = (int)patterns::pivots(A).size();
  int ab = 0;
  perceivedColours(A, v.argyllSolids, ab);
  v.argyllPerceived = v.argyllSolids + ab;
  v.argyllPredicted = v.argyllSolids * (v.argyllSolids + 1) / 2;
  v.argyllLaw = v.argyllPerceived == v.argyllPredicted;

  // The unit proportions of the two setts — the whole argument of the
  // comparison strip: 86/252 against 138/416, and so on down the four units.
  const float bwU[4] = {(float)v.unitA, (float)v.unitB, (float)v.unitC,
                        (float)v.unitB};
  const float caU[4] = {(float)sumRuns(kArgA), (float)sumRuns(kArgB),
                        (float)sumRuns(kArgC), (float)sumRuns(kArgD)};
  for (int i = 0; i < 4; ++i)
    v.unitDrift = std::max(
        v.unitDrift,
        std::abs(bwU[i] / (float)v.total - caU[i] / (float)v.argyllTotal));
  return v;
}

// ---------------------------------------------------------------------------
// Baking. One pixel per thread, written straight into a bitmap, then sampled
// with kNearest at an INTEGER magnification. Element::sampling() is what makes
// the filter choice reachable at all; the magnification must additionally stay
// integer, because 2 px threads on a 4 px interlacement period minified by any
// non-integer factor is a moire generator whatever the filter is.

/** A window of the cloth, taken at (originX, originY). `rib` darkens the
 *  cells where the WEFT is on top, which is what makes the twill legible
 *  inside a block of one colour; the main cloth panel gets that as a
 *  multiplied overlay tile instead (one element for 63,504 cells), and the
 *  drawdown bakes it in, because at 9 px per thread it has no overlay. */
sk_sp<SkImage> bakeCloth(const std::vector<uint8_t>& S, const Shades& sh,
                         int originX, int originY, int w, int h,
                         float rib = 0.0f) {
  return patterns::clothImage(cloth(S, sh, rib), {originX, originY},
                              SkISize::Make(w, h));
}

/** One cell of the blend table: warp colour `a` crossed with weft colour `b`,
 *  at thread scale. The same generator as the cloth — a one-thread sett each
 *  way — so if these disagree with the main panel then one of the two is
 *  wrong. */
sk_sp<SkImage> bakeBlend(SkColor4f a, SkColor4f b, int threads) {
  return patterns::clothImage(
      {.warp = {0}, .weft = {1}, .shades = {a, b}, .weave = kTwill}, {0, 0},
      SkISize::Make(threads, threads));
}

inline Paint imageMat(const sk_sp<SkImage>& img, float px,
                      SkTileMode tile = SkTileMode::kRepeat) {
  return Paint::image(img, tile, tile, SkMatrix::Scale(px, px),
                      SkSamplingOptions(SkFilterMode::kNearest));
}

// ---------------------------------------------------------------------------
// Type. Three registers on one card: a tracked display line, mono numerals
// small enough to sit under a 4 px band, and one genuinely justified
// paragraph at a real measure.

inline sk_sp<SkTypeface> sans() {
  return weave::ports::face({"Helvetica Neue", "Arial"},
                            SkFontStyle::kNormal_Weight);
}
inline sk_sp<SkTypeface> sansB() {
  return weave::ports::face({"Helvetica Neue", "Arial"},
                            SkFontStyle::kBold_Weight);
}
inline sk_sp<SkTypeface> mono() {
  return sketch::kit::houseFace(sketch::kit::Voice::Terminal);
}
inline sk_sp<SkTypeface> serif() {
  return weave::ports::face({"Baskerville", "Times New Roman"},
                            SkFontStyle::kNormal_Weight);
}
inline sk_sp<SkTypeface> serifIt() {
  return weave::ports::face({"Baskerville", "Times New Roman"},
                            SkFontStyle::kNormal_Weight,
                            SkFontStyle::kItalic_Slant);
}

// THE CARD'S OWN SHEET. Every kit component reads the theme in scope, and
// this card is dark ink on manila rather than the house sheet's pale ink on
// black, so the study binds its own: the card's two faces, the card's ink,
// and the tight row it sets a machine-read table at. The faces are resolved
// once and held, because a style is compared by face POINTER.
inline const sketch::kit::Theme& sheet() {
  static const sketch::kit::Theme look = [] {
    sketch::kit::Theme t;
    t.palette.ground = kCard;
    t.palette.cellGround = kWell;
    t.palette.ink = kInk;
    t.palette.ash = kInk2;
    t.palette.rule = kRule;
    t.palette.figure = kInk;
    t.type.sans = sans();
    t.type.mono = mono();
    t.type.captionLabel = {9.5f, 0.1f, true};
    t.type.captionNote = {9.5f, 0.1f, true};
    t.spacing.rowGap = 2.4f;
    t.spacing.labelGap = 6;
    t.spacing.swatchSide = 5;
    return t;
  }();
  return look;
}

// THE CARD'S VOICE is stated once, on the root of the tree it is described
// into: the mono every machine-read line runs in, in the grey of its small
// print. A line set otherwise says only what differs — a size, a tracking,
// the ink black, a serif cut — and the lines the card sets more than once
// are classes over the sheet's registers, bound with the sheet wherever the
// card is described. The serif cuts name their own face because a sheet
// holds two.
inline const weave::StyleSheet& classes() {
  static const weave::StyleSheet look =
      sheet()
          .styleSheet()
          .set("heading", {.size = 9, .color = kInk, .track = 0.5f})
          .set("tag", {.size = 7, .track = 0.6f})
          .set("note", {.size = 8, .track = 0.2f})
          .set("quote", {.face = serif(), .size = 10.5f})
          .set("name", {.face = serifIt(), .size = 13});
  return look;
}

inline std::u8string U(const std::string& s) { return toUtf8(s); }

/** One line of type at a card position, ranged left or centred, set in
 *  whatever the caller states on it — a class or a partial — over the root
 *  voice. The line box is 1.6 em of that type, so it follows the size the
 *  line resolves to. */
inline Element label(const std::string& s, float x, float y, float w) {
  return at(x, y, w, 0).height(1.6_em).child(text(U(s)));
}
inline Element centred(const std::string& s, float x, float y, float w) {
  return at(x, y, w, 0)
      .height(1.6_em)
      .child(text(U(s))
                 .textAlign(weave::TextAlignment::kCenter)
                 .width(Dimension(w)));
}
inline Element rule(float x, float y, float w, float h, SkColor4f c) {
  return at(x, y, w, h).fill(c);
}

// The curves. bind()'s map() runs after from() normalises, and from() lets the
// value out of [0,1] on both sides, so every shaping function here has to be
// total on the reals — which is why the overshoot below is CLAMPED and the
// library's own unbounded `ease::outBack` is not used raw: a beat that has
// not started yet would read a scale of thousands.
//
// Both are `ease::Curve` — a captureless shape beside the numbers it reads —
// rather than a capturing lambda, because a curve that cannot be compared
// makes the whole binding holding it incomparable, and a node whose binding
// re-patches on every describe never prunes.
inline sigil::motion::ease::Curve plateau(float edge) {
  return {[](float t, const float* p) {
            const float e = p[0];
            if (t <= 0.0f || t >= 1.0f) return 0.0f;
            if (t < e) return t / e;
            if (t > 1.0f - e) return (1.0f - t) / e;
            return 1.0f;
          },
          {edge}};
}
inline sigil::motion::ease::Curve backOut() {
  return {[](float t, const float* p) {
            return choreograph::easeOutBack(std::clamp(t, 0.0f, 1.0f), p[0]);
          },
          sigil::motion::ease::outBack().parameters[0]};
}

// The timeline, in one place: one Output, five beats.
constexpr float kCycle = 8.0f;
constexpr float kBeamEnd = 0.7f / kCycle;   // 0.0875 — the warp is on the beam
constexpr float kWeaveEnd = 4.1f / kCycle;  // 0.5125 — 378 picks have beaten in
constexpr float kProveEnd = 4.9f / kCycle;  // 0.6125 — the arithmetic
constexpr float kTurnEnd = 6.8f / kCycle;   // 0.85   — five shade cards

// The card.
constexpr float kCanvasW = 1600, kCanvasH = 1440;
constexpr float kPx = 2;  // px per thread, everywhere, and never a fraction
constexpr float kClothX = 64, kClothY = 254;
constexpr float kClothW = 1008, kClothH = 756;  // 2.0 x 1.5 setts
constexpr int kPicks = (int)(kClothH / kPx);    // 378
constexpr float kBarY = 152, kBarH = 34;
constexpr float kColX = 1104, kColW = 432;

}  // namespace

// ===========================================================================
