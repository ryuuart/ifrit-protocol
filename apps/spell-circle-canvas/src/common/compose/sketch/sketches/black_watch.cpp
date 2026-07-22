// black_watch.cpp — THE GOVERNMENT SETT (BLACK WATCH), WOVEN
// =============================================================================
// A mill pattern card for the tartan the world calls Black Watch, rebuilt as
// CLOTH — thread by thread, from a published thread count, on the four-shaft
// loom draft it is actually woven on.
//
// The whole artefact is 24 integers and a modulus-4 rule. A tartan is a
// ONE-DIMENSIONAL sequence of coloured threads woven against itself; the
// register states the rule in its own words — "This stripe sequence is the
// same vertically (warp) and horizontally (weft)." Every square, every check,
// and every one of the "third colours" that appear where unlike threads cross
// is EMERGENT. Nothing about the two-dimensional design is authored here:
// the sett is typed in as Douglas printed it, the twill decides which thread
// is on top at each of the 63,504 crossings, and the plane falls out.
//
// DOCUMENTED — read directly, cited, and in the repo
//  - Harriet C. Douglas, "Scotch Tartan Setts: A Shuttle-Craft Guild Guide
//    For Weaving 132 Traditional Plaids", copyright September 1949, Virginia
//    City, Montana; digitised November 2002 by Nancy M. McKenna. University
//    of Arizona On-Line Digital Archive of Documents on Weaving:
//    https://www2.cs.arizona.edu/patterns/weaving/monographs/dhc_tar.pdf
//    THE load-bearing source. It gives the BLACK WATCH (42nd Regiment) sett
//    unit by unit with its own printed total of 252 ends (kSettA/B/C below,
//    transcribed literally); the CAMPBELL (of Argyll) sett, 416 ends; the
//    loom draft in one sentence — "a straight twill threading (1,2,3,4) and
//    woven as 2-2 balanced twill (1-2, 2-3, 3-4, 4-1)"; the balance rule and
//    the as-drawn-in treadling quoted on the card; and its own caveat, which
//    turns out to be this piece's second subject: "standardization is not
//    complete and minor variations in color and count are found among the
//    works of various authorities."
//  - Scottish Register of Tartans, "Black Watch (Government)", SRT ref 277:
//    https://www.tartanregister.gov.uk/tartanDetails?ref=277 — STA 207,
//    STWR 207, tartan date 01/01/1739, category Military, designer Unknown,
//    and the registration note quoted verbatim on the card: "The Cockburn
//    Collection (1810-15) includes four specimens of the Government tartan
//    labelled; 'Campbell Argyll', 'Grant', 'Munro' and 'Sutherland'."
//  - Scottish Register of Tartans, threadcount guidance:
//    https://www.tartanregister.gov.uk/threadcount — the notation, the `/`
//    pivot marker, the 2/2 twill construction, the 240-260-threads-to-a-
//    six-inch-sett rule, and "the DNA of a tartan".
//  - Scottish Register of Tartans, official colour shades table (PDF):
//    https://tartanregister.gov.uk/docs/Colour_shades.pdf — EVERY hex in
//    kPalettes and kY/kW below is lifted from that table verbatim and was
//    re-verified against the downloaded PDF before this file was written.
//  - The register's own rendered swatch of SRT 277 (imageCreation.aspx?
//    ref=277) — read for the twill handedness: a diagonal autocorrelation of
//    its blend cells is +0.40..+0.57 on the "\" lag and +0.23..+0.30 on "/",
//    so the rib runs top-left to bottom-right, which is warpUp() below.
//  - Glasgow Libraries, the Cockburn Collection: one volume, 1810-1820,
//    Lt-Gen Sir William Cockburn, 56 specimens of 'hard tartan' by William
//    Wilson & Sons of Bannockburn, now in the Mitchell Library.
//
// RECONSTRUCTED — not anybody's citation
//  - The assignment of particular register shades to the NAMED palette
//    families (Modern / Ancient / Muted / Weathered / Reproduction). The
//    hexes and their colour codes are the register's; which shade belongs to
//    which family is a reading of each family's documented description, not a
//    weaver's shade card.
//  - The unit-proportion finding printed on the card (Black Watch and
//    Campbell of Argyll agree unit-for-unit to under 1%) is computed here.
//  - Every number in the verification block is computed at startup by
//    verify(), never asserted: run it and it recomputes.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/black_watch.cpp \
//       --frame /tmp/black_watch.png --at 7.4
//
// --at 0.45 catches the warp alone on the beam (the whole design, in one
// dimension, and it does not look like a tartan yet); 2.0 the cloth weaving
// itself; 4.6 the arithmetic proving; 6.0 the palette turn; 7.4 the card.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Debug.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Pattern.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilimage/ImageAsset.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/ports/SystemFonts.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;

namespace {

constexpr SkColor4f C(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16) & 0xff) / 255.0f,
          (float)((hex >> 8) & 0xff) / 255.0f, (float)(hex & 0xff) / 255.0f, a};
}

// ---------------------------------------------------------------------------
// The card it is mounted on. Black Watch is a dark cloth; it sits on manila
// board, so this is the one sketch in the program lit the other way round.

constexpr SkColor4f kCard = C(0xE8E2D6);
constexpr SkColor4f kWell = C(0xDCD4C4);
constexpr SkColor4f kRule = C(0x8A8478);
constexpr SkColor4f kInk = C(0x1A1815);
constexpr SkColor4f kInk2 = C(0x5A554C);
constexpr SkColor4f kRed = C(0x9A3324);
constexpr SkColor4f kDeep = C(0x3E3A33);

// ---------------------------------------------------------------------------
// The colours. Codes are the register's: K black, B blue, G green, Y yellow,
// W white (the warm one — undyed wool, `lachdann`). Five shade cards, one
// thread count: the register publishes nineteen permitted greens, sixteen
// blues and two blacks, and that is the whole mechanism behind the named
// palettes. The cloth's identity is the count; the colour is a variable.

enum Col : uint8_t { K = 0, B = 1, G = 2, Y = 3, W = 4, kColCount = 5 };

struct Palette {
  const char *name;
  uint32_t k, b, g;
};
constexpr std::array<Palette, 5> kPalettes{{
    {"MODERN / ORDINARY", 0x101010, 0x2C2C80, 0x006818},
    {"ANCIENT / OLD COLOURS", 0x1C1C1C, 0x2C4084, 0x5C6428},
    {"MUTED", 0x1C1714, 0x202060, 0x3F5642},
    {"WEATHERED / FADED", 0x4C3428, 0x5F749C, 0x767E52},
    {"REPRODUCTION (DALGLIESH)", 0x555A64, 0x5C8CA8, 0x789484},
}};
constexpr uint32_t kY = 0xE8C000; // Yellow
constexpr uint32_t kW = 0xE5DDD1; // White

using Shades = std::array<SkColor4f, kColCount>;
inline Shades shadesOf(const Palette &p) {
  return {C(p.k), C(p.b), C(p.g), C(kY), C(kW)};
}

// ---------------------------------------------------------------------------
// THE SETT — Douglas 1949, transcribed literally, unit by unit. Do not merge,
// do not mirror: Douglas prints the WHOLE repeat as A + B + C + B, where the
// register's own notation would print only the half between the pivots. Take
// both and you get a 504-thread cloth that looks perfectly fine and is twice
// the right size, which is exactly what invariant #1 is for.

struct Run {
  Col c;
  int n;
};

// A   18 black  6 blue  2 black  6 blue  2 black  18 blue
//      2 black  6 blue  2 black  6 blue 18 black                    (86)
const std::vector<Run> kSettA{{K, 18}, {B, 6},  {K, 2}, {B, 6},  {K, 2}, {B, 18},
                              {K, 2},  {B, 6},  {K, 2}, {B, 6},  {K, 18}};
// B   18 green  6 black 18 green                                    (42)
const std::vector<Run> kSettB{{G, 18}, {K, 6}, {G, 18}};
// C   18 black 18 blue  2 black  6 blue  2 black 18 blue 18 black   (82)
const std::vector<Run> kSettC{{K, 18}, {B, 18}, {K, 2}, {B, 6},
                              {K, 2},  {B, 18}, {K, 18}};
//                                                     Repeat B.  252 ends.
constexpr int kPublishedEnds = 252; // the total Douglas himself prints

// CAMPBELL (of Argyll) — same pamphlet, transcribed complete. Identical
// structure, run for run; the two black centre-lines of the green squares are
// a YELLOW and a WHITE overcheck instead. That is the documented Wilsons-of-
// Bannockburn mechanism for producing "clan" tartans, caught in the act in a
// 1949 weaving manual.
const std::vector<Run> kArgA{{K, 30}, {B, 6},  {K, 6}, {B, 6},  {K, 6}, {B, 30},
                             {K, 6},  {B, 6},  {K, 6}, {B, 6},  {K, 30}};
const std::vector<Run> kArgB{{G, 32}, {Y, 6}, {G, 32}};
const std::vector<Run> kArgC{{K, 30}, {B, 30}, {K, 6}, {B, 6},
                             {K, 6},  {B, 30}, {K, 30}};
const std::vector<Run> kArgD{{G, 32}, {W, 6}, {G, 32}};
constexpr int kPublishedArgyll = 416;

inline int sumRuns(const std::vector<Run> &r) {
  int t = 0;
  for (const Run &x : r)
    t += x.n;
  return t;
}
inline std::vector<Run> concatRuns(std::vector<std::vector<Run>> units) {
  std::vector<Run> out;
  for (const auto &u : units)
    out.insert(out.end(), u.begin(), u.end());
  return out;
}
/** Runs -> one thread per entry. The ONE expansion in the file: bands are
 *  half-open [cursor, cursor + n) and the next starts at cursor + n. Every
 *  inclusive-range variant of this loop still sums to 252, still comes out
 *  palindromic, and still looks exactly like Black Watch — only invariant #6
 *  sees it. */
inline std::vector<uint8_t> expand(const std::vector<Run> &runs) {
  std::vector<uint8_t> s;
  for (const Run &r : runs)
    for (int i = 0; i < r.n; ++i)
      s.push_back((uint8_t)r.c);
  return s;
}

// ---------------------------------------------------------------------------
// THE LOOM — the stored program, four lines of it.
//
//   threading[j] = j mod 4                 straight draw: 1,2,3,4,1,2,3,4,...
//   treadling[i] = i mod 4                 as-drawn-in ("tromp as writ")
//   tieup[t]     = { t, (t+1) mod 4 }      each treadle lifts two shafts
//
// Warp end j is lifted on pick i iff threading[j] is in tieup[treadling[i]],
// which reduces to one expression. With y increasing downward this puts the
// rib on the "\" diagonal, which is what the register's swatch shows.

inline bool warpUp(int x, int y) { return ((x - y) % 4 + 4) % 4 < 2; }

/** The entire artefact. `S` is one array; there is no second sequence — the
 *  register says the weft sequence IS the warp sequence and Douglas's
 *  as-drawn-in treadling says the same thing about the picks. */
inline uint8_t clothAt(const std::vector<uint8_t> &S, int x, int y) {
  const int n = (int)S.size();
  const int xi = ((x % n) + n) % n, yi = ((y % n) + n) % n;
  return warpUp(x, y) ? S[(size_t)xi] : S[(size_t)yi];
}

// ---------------------------------------------------------------------------
// THE INVARIANTS. Six exact checks, all computed here, none asserted. The
// cheap ones do not catch the commonest bug: build the 24 bands as inclusive
// ranges and the sum still says 252, the sequence is still palindromic and
// the cloth still looks right. Only point-sampled coverage sees it.

struct Verdict {
  // #1 the sett closes
  int unitA = 0, unitB = 0, unitC = 0, total = 0;
  bool closes = false;
  // #2 reflective
  std::vector<int> mirrors;
  int mirrorGap = 0;
  bool reflective = false;
  // #3 2/2 balance
  int maxWarpFloat = 0, maxWeftFloat = 0;
  bool balanced = false;
  // #4 thread ratio
  std::array<int, kColCount> counts{};
  int gcd3 = 1;
  bool blueIsThird = false;
  // #5 the colour law
  int solids = 0, blends = 0, perceived = 0, predicted = 0;
  bool colourLaw = false;
  // #6 exact cover
  int samples = 0, uncovered = 0, doubled = 0;
  bool exactCover = false;
  // Campbell
  int argyllTotal = 0, argyllMirrors = 0, argyllPerceived = 0,
      argyllPredicted = 0;
  float unitDrift = 0; // max |BW unit fraction - CA unit fraction|
};

inline std::vector<int> findMirrors(const std::vector<uint8_t> &S) {
  const int n = (int)S.size();
  std::vector<int> out;
  for (int b = 0; b < n; ++b) {
    bool ok = true;
    for (int k = 0; k < n / 2 && ok; ++k)
      ok = S[(size_t)(((b + k) % n))] ==
           S[(size_t)((((b - 1 - k) % n) + n) % n)];
    if (ok)
      out.push_back(b);
  }
  return out;
}

inline int perceivedColours(const std::vector<uint8_t> &S) {
  bool seen[kColCount][kColCount] = {};
  const int n = (int)S.size();
  for (int x = 0; x < n; ++x)
    for (int y = 0; y < n; ++y) {
      // The 2/2 twill is balanced, so over any 4x4 block the crossing of
      // warp a with weft b shows eight cells of each: the perceived colour
      // is the UNORDERED pair, which is where n(n+1)/2 comes from.
      const uint8_t a = S[(size_t)x], b = S[(size_t)y];
      seen[std::min(a, b)][std::max(a, b)] = true;
    }
  int solid = 0, blend = 0;
  for (int i = 0; i < kColCount; ++i)
    for (int j = i; j < kColCount; ++j)
      if (seen[i][j])
        (i == j ? solid : blend)++;
  return solid * 1000 + blend; // packed; unpacked by the caller
}

inline int gcd(int a, int b) { return b ? gcd(b, a % b) : a; }

Verdict verify(const std::vector<Run> &bwRuns, const std::vector<uint8_t> &S,
               const std::vector<uint8_t> &A) {
  Verdict v;

  // #1 -- the sett closes, against the total the SOURCE prints.
  v.unitA = sumRuns(kSettA);
  v.unitB = sumRuns(kSettB);
  v.unitC = sumRuns(kSettC);
  v.total = (int)S.size();
  v.closes = v.unitA + v.unitB + v.unitC + v.unitB == v.total &&
             v.total == kPublishedEnds;

  // #2 -- reflective: exactly two mirror boundaries, exactly half apart.
  v.mirrors = findMirrors(S);
  if (v.mirrors.size() == 2) {
    v.mirrorGap = v.mirrors[1] - v.mirrors[0];
    v.reflective = v.mirrorGap == v.total / 2;
  }

  // #3 -- 2/2 balance: scan runs of warpUp down each column and runs of
  // !warpUp across each row. Nothing anywhere may float longer than 2.
  const int n = v.total;
  for (int x = 0; x < n; ++x) {
    int run = 0;
    for (int y = 0; y < 2 * n; ++y) {
      if (warpUp(x, y))
        v.maxWarpFloat = std::max(v.maxWarpFloat, ++run);
      else
        run = 0;
    }
  }
  for (int y = 0; y < n; ++y) {
    int run = 0;
    for (int x = 0; x < 2 * n; ++x) {
      if (!warpUp(x, y))
        v.maxWeftFloat = std::max(v.maxWeftFloat, ++run);
      else
        run = 0;
    }
  }
  v.balanced = v.maxWarpFloat == 2 && v.maxWeftFloat == 2;

  // #4 -- thread ratio.
  for (uint8_t c : S)
    v.counts[c]++;
  v.gcd3 = gcd(gcd(v.counts[K], v.counts[B]), v.counts[G]);
  v.blueIsThird = v.counts[B] * 3 == v.total;

  // #5 -- the colour law, over the whole sett square.
  const int packed = perceivedColours(S);
  v.solids = packed / 1000;
  v.blends = packed % 1000;
  v.perceived = v.solids + v.blends;
  v.predicted = v.solids * (v.solids + 1) / 2;
  v.colourLaw = v.perceived == v.predicted;

  // #6 -- exact cover. The 24 warp bands crossed with the 24 weft bands must
  // tile the sett square with no gap and no overlap; debug::coverage is the
  // only one of these six that sees a one-thread band overlap.
  std::vector<SkPath> cells;
  cells.reserve(bwRuns.size() * bwRuns.size());
  std::vector<float> start;
  {
    float cur = 0;
    for (const Run &r : bwRuns) {
      start.push_back(cur);
      cur += (float)r.n;
    }
  }
  for (size_t i = 0; i < bwRuns.size(); ++i)
    for (size_t j = 0; j < bwRuns.size(); ++j)
      cells.push_back(SkPath::Rect(SkRect::MakeXYWH(
          start[i], start[j], (float)bwRuns[i].n, (float)bwRuns[j].n)));
  const debug::Coverage cov =
      debug::coverage(cells, SkRect::MakeWH((float)n, (float)n), 256);
  v.samples = cov.samples;
  v.uncovered = cov.uncovered;
  v.doubled = cov.doubled;
  v.exactCover = cov.exact();

  // Campbell of Argyll: same law, five colours, and NO exact mirror — the
  // yellow overcheck and the white one are different colours, so it is
  // reflective in proportion and asymmetric in colour. The register would
  // record it with the `...` repeating notation rather than a `/` pivot.
  v.argyllTotal = (int)A.size();
  v.argyllMirrors = (int)findMirrors(A).size();
  const int ap = perceivedColours(A);
  v.argyllPerceived = ap / 1000 + ap % 1000;
  v.argyllPredicted = (ap / 1000) * (ap / 1000 + 1) / 2;

  // The unit proportions of the two setts, which is the whole argument of
  // the comparison strip: 86/252 against 138/416, and so on.
  const float bwU[4] = {(float)v.unitA, (float)v.unitB, (float)v.unitC,
                        (float)v.unitB};
  const float caU[4] = {(float)sumRuns(kArgA), (float)sumRuns(kArgB),
                        (float)sumRuns(kArgC), (float)sumRuns(kArgD)};
  for (int i = 0; i < 4; ++i)
    v.unitDrift = std::max(
        v.unitDrift, std::abs(bwU[i] / (float)v.total -
                              caU[i] / (float)v.argyllTotal));
  return v;
}

// ---------------------------------------------------------------------------
// Baking. One pixel per thread, written straight into a bitmap, then sampled
// with kNearest at an INTEGER magnification. Element::sampling() landed while
// this was being written; before it, every panel showing the cloth below 1:1
// was at the mercy of a hardcoded kLinear and the composition had to bend
// around it. It still only takes integer scales here, because 2 px threads on
// a 4 px interlacement period minified by any non-integer factor is a moire
// generator whatever the filter.

sk_sp<SkImage> bakeCloth(const std::vector<uint8_t> &S, const Shades &sh,
                         int originX, int originY, int w, int h) {
  SkBitmap bm;
  bm.allocN32Pixels(w, h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor4f c = sh[clothAt(S, originX + x, originY + y)];
      *bm.getAddr32(x, y) = c.toSkColor();
    }
  bm.setImmutable();
  return bm.asImage();
}

/** One blend cell of the table: warp colour `a` crossed with weft colour
 *  `b`, at thread scale. Same generator as the cloth — if these disagree
 *  with the main panel one of the two is wrong. */
sk_sp<SkImage> bakeBlend(SkColor4f a, SkColor4f b, int threads) {
  SkBitmap bm;
  bm.allocN32Pixels(threads, threads);
  for (int y = 0; y < threads; ++y)
    for (int x = 0; x < threads; ++x)
      *bm.getAddr32(x, y) = (warpUp(x, y) ? a : b).toSkColor();
  bm.setImmutable();
  return bm.asImage();
}

inline Material imageMat(const sk_sp<SkImage> &img, float px,
                         SkTileMode tile = SkTileMode::kRepeat) {
  return Material::image(img, tile, tile, SkMatrix::Scale(px, px),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

// ---------------------------------------------------------------------------
// Type. Three registers on one card: a tracked display line, mono numerals
// small enough to sit under a 4 px band, and one genuinely justified
// paragraph.

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              const char *fallback = nullptr) {
  auto mgr = weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                              SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
inline const sk_sp<SkTypeface> &sans() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kNormal_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &sansB() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kBold_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &mono() {
  static sk_sp<SkTypeface> f =
      face("Menlo", SkFontStyle::kNormal_Weight, "Courier New");
  return f;
}
inline const sk_sp<SkTypeface> &serif() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::kNormal_Weight, "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface> &serifIt() {
  static sk_sp<SkTypeface> f = [] {
    auto mgr = weave::ports::systemFontManager();
    sk_sp<SkTypeface> f2 = mgr->matchFamilyStyle(
        "Baskerville", SkFontStyle(SkFontStyle::kNormal_Weight,
                                   SkFontStyle::kNormal_Width,
                                   SkFontStyle::kItalic_Slant));
    return f2 ? f2 : serif();
  }();
  return f;
}

inline weave::TextStyle ty(const sk_sp<SkTypeface> &tf, float size,
                           SkColor4f color, float track = 0) {
  weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}
inline weave::TextStyle mn(float sz, SkColor4f c, float tr = 0) {
  return ty(mono(), sz, c, tr);
}
inline weave::TextStyle sn(float sz, SkColor4f c, float tr = 0) {
  return ty(sans(), sz, c, tr);
}
inline weave::TextStyle sb(float sz, SkColor4f c, float tr = 0) {
  return ty(sansB(), sz, c, tr);
}

inline std::u8string U(const std::string &s) { return toU8(s); }
inline std::string fmt(const char *f, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return std::string(buf);
}

// A box at absolute card coordinates — the whole composition is pinned, so
// every panel spells this once.
inline Element at(float x, float y, float w, float h) {
  return box()
      .absolute()
      .left(Dim(x))
      .top(Dim(y))
      .width(Dim(w))
      .height(Dim(h));
}
inline Element label(const std::string &s, const weave::TextStyle &st, float x,
                     float y, float w = 600) {
  return at(x, y, w, st.shaping.fontSize * 1.6f).child(text(U(s), st));
}
inline Element rule(float x, float y, float w, float h, SkColor4f c) {
  return at(x, y, w, h).fill(c);
}

// The curves. bind()'s map() runs after from() normalises, and from() lets
// the value out of [0,1] on both sides, so every shaping function here has
// to be total on the reals.
inline choreograph::EaseFn plateau(float edge) {
  return [edge](float t) {
    if (t <= 0.0f || t >= 1.0f)
      return 0.0f;
    if (t < edge)
      return t / edge;
    if (t > 1.0f - edge)
      return (1.0f - t) / edge;
    return 1.0f;
  };
}
inline choreograph::EaseFn backOut() {
  return [](float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float s = 1.70158f, u = t - 1.0f;
    return u * u * ((s + 1.0f) * u + s) + 1.0f;
  };
}

// The timeline, in one place. One Output, five beats.
constexpr float kCycle = 8.0f;
constexpr float kBeamEnd = 0.7f / kCycle;   // 0.0875
constexpr float kWeaveEnd = 4.1f / kCycle;  // 0.5125
constexpr float kProveEnd = 4.9f / kCycle;  // 0.6125
constexpr float kTurnEnd = 6.8f / kCycle;   // 0.85

// Geometry of the card.
constexpr float kW = 1600, kH = 1340;
constexpr float kPx = 2;              // px per thread, everywhere, always
constexpr float kClothX = 64, kClothY = 228;
constexpr float kClothW = 1008, kClothH = 756; // 2.0 x 1.5 setts
constexpr int kPicks = (int)(kClothH / kPx);   // 378
constexpr float kBarY = 140, kBarH = 40;
constexpr float kColX = 1104, kColW = 432;

} // namespace

// ===========================================================================

struct BlackWatch : sigil::compose::sketch::Sketch {
  // --- the data -----------------------------------------------------------
  std::vector<Run> bwRuns, caRuns;
  std::vector<uint8_t> S, A;
  Verdict v;
  std::vector<int> runStart;

  // --- the loom -----------------------------------------------------------
  choreograph::Output<float> loom{0};
  double clock = 0;
  bool reported = false;

  // --- baked material ------------------------------------------------------
  // Held as members: the shared bake IS the identity (Pattern.h), and a fresh
  // Pattern per describe would re-render its tile every render().
  Pattern warpPattern;                 // the 1-D sequence, 252 bands
  std::array<Pattern, 12> pickPattern; // (colour, twill phase) — see below
  Pattern threadGrid;                  // the interlacement grooves
  Material warpMat, gridMat, cardGrain, yarnGrain;
  std::vector<Material> pickMat;       // 12, resolved once
  std::vector<Material> clothMat;      // 5 palettes, whole-cloth
  Material argyllMat;
  std::array<Material, 9> blendMat;
  std::shared_ptr<const sigil::image::ImageAsset> drawdownAsset;
  std::vector<std::string> verifyLines;
  std::shared_ptr<weave::Paragraph> quote;

  // The drawdown window: threads 66..89 of the sett, chosen because it is the
  // only 24-thread window that carries all three colours (blue 66-67, black
  // 68-85, green 86-89) and therefore both kinds of blend cell.
  static constexpr int kDrawOrigin = 66, kDrawN = 24, kDrawCell = 12;

  // =========================================================================

  void build() {
    bwRuns = concatRuns({kSettA, kSettB, kSettC, kSettB});
    caRuns = concatRuns({kArgA, kArgB, kArgC, kArgD});
    S = expand(bwRuns);
    A = expand(caRuns);
    {
      int cur = 0;
      for (const Run &r : bwRuns) {
        runStart.push_back(cur);
        cur += r.n;
      }
    }
    v = verify(bwRuns, S, A);

    const Shades modern = shadesOf(kPalettes[0]);

    // 1. THE WARP GROUND — one element for 252 threads. A tile exactly one
    //    sett wide; the bands are half-open, drawn from the same cursor the
    //    expansion used.
    {
      auto runs = bwRuns;
      warpPattern = Pattern::tile(
          {kPx * (float)S.size(), 8},
          [runs, modern](SkCanvas &c, SkSize sz, uint32_t) {
            SkPaint p;
            float cur = 0;
            for (const Run &r : runs) {
              p.setColor4f(modern[r.c], nullptr);
              c.drawRect(SkRect::MakeXYWH(cur, 0, kPx * (float)r.n,
                                          sz.height()),
                         p);
              cur += kPx * (float)r.n;
            }
          });
      warpMat = warpPattern.material();
    }

    // 2. THE WEFT — the defining property of twill is that the interlacement
    //    advances ONE THREAD PER PICK, and Pattern cannot pan (ROADMAP 14).
    //    Saved only by the phase being mod 4: the weft shows where
    //    (x - y) mod 4 is in {2, 3}, i.e. a 4-px-on / 4-px-off stripe whose
    //    phase is ((i + 2) mod 4) * 2 px. Three colours by four phases is
    //    twelve patterns, built once, and every one of the 378 picks selects
    //    one of them. A weave with any other modulus would have had nothing.
    for (int c = 0; c < 3; ++c)
      for (int ph = 0; ph < 4; ++ph) {
        const SkColor4f col = modern[(Col)c];
        const float off = (float)ph * 2.0f * kPx / 2.0f; // ph * 2 px
        pickPattern[(size_t)(c * 4 + ph)] = Pattern::tile(
            {4 * kPx, kPx}, [col, off](SkCanvas &c2, SkSize sz, uint32_t) {
              SkPaint p;
              p.setColor4f(col, nullptr);
              // the float, with its wrap-around copy so the tile is seamless
              c2.drawRect(SkRect::MakeXYWH(off, 0, 2 * kPx, sz.height()), p);
              c2.drawRect(
                  SkRect::MakeXYWH(off - sz.width(), 0, 2 * kPx, sz.height()),
                  p);
            });
      }
    pickMat.clear();
    for (auto &p : pickPattern)
      pickMat.push_back(p.material());

    // 3. THE INTERLACEMENT GROOVE — in real cloth the thread that is UNDER
    //    at a cell is shaded by the one on top. One 2x2 tile multiplied over
    //    the whole panel is the whole cue, and it is what separates a woven
    //    plaid from a printed one.
    threadGrid = Pattern::tile({kPx, kPx}, [](SkCanvas &c, SkSize, uint32_t) {
      SkPaint p;
      p.setColor4f({0, 0, 0, 0.30f}, nullptr);
      c.drawRect(SkRect::MakeWH(kPx, 1), p);
      p.setColor4f({0, 0, 0, 0.18f}, nullptr);
      c.drawRect(SkRect::MakeWH(1, kPx), p);
    });
    gridMat = threadGrid.material();

    // 4. WHOLE-CLOTH BAKES — one per palette family, 252 x 252 at one pixel
    //    per thread, magnified x2 with kNearest. Same 252 threads, same
    //    draft, five legitimate cloths: the palette turn costs five images
    //    and changes nothing structural, which IS the colour argument.
    clothMat.clear();
    for (const Palette &p : kPalettes)
      clothMat.push_back(
          imageMat(bakeCloth(S, shadesOf(p), 0, 0, (int)S.size(),
                             (int)S.size()),
                   kPx));
    argyllMat = imageMat(
        bakeCloth(A, modern, 0, 0, (int)A.size(), (int)A.size()), kPx);

    // 5. THE BLEND TABLE and THE DRAWDOWN — the same generator at a
    //    different scale, never authored separately.
    for (int wp = 0; wp < 3; ++wp)
      for (int wf = 0; wf < 3; ++wf)
        blendMat[(size_t)(wf * 3 + wp)] =
            imageMat(bakeBlend(modern[(Col)wp], modern[(Col)wf], 6), 8);
    drawdownAsset = std::make_shared<const sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(bakeCloth(
            S, modern, kDrawOrigin, kDrawOrigin, kDrawN, kDrawN)));

    // 6. Card tooth and yarn tooth. grain() is the luminance field, and this
    //    is the opaque surface its header says it belongs on.
    cardGrain = patterns::grain(0.09f, 4, 7.0f, 0.55f);
    yarnGrain = patterns::grain(0.16f, 3, 3.0f, 0.75f, 1.6f);

    buildVerifyLines();
    buildQuote();
  }

  void buildVerifyLines() {
    auto ok = [](bool b) { return b ? "OK" : "**"; };
    verifyLines = {
        fmt("SETT CLOSES      %d + %d + %d + %d = %d ends   (printed %d)  %s",
            v.unitA, v.unitB, v.unitC, v.unitB, v.total, kPublishedEnds,
            ok(v.closes)),
        fmt("REFLECTIVE       mirrors at thread %d, %d   gap %d = %d/2   %s",
            v.mirrors.size() > 0 ? v.mirrors[0] : -1,
            v.mirrors.size() > 1 ? v.mirrors[1] : -1, v.mirrorGap, v.total,
            ok(v.reflective)),
        fmt("2/2 BALANCE      max warp float %d   max weft float %d       %s",
            v.maxWarpFloat, v.maxWeftFloat, ok(v.balanced)),
        fmt("THREAD RATIO     K %d : B %d : G %d  =  %d : %d : %d       %s",
            v.counts[K], v.counts[B], v.counts[G], v.counts[K] / v.gcd3,
            v.counts[B] / v.gcd3, v.counts[G] / v.gcd3,
            ok(v.blueIsThird)),
        fmt("COLOUR LAW       n = %d -> %d solid + %d blend = %d = n(n+1)/2  %s",
            v.solids, v.solids, v.blends, v.perceived, ok(v.colourLaw)),
        fmt("EXACT COVER      %d uncovered / %d doubled of %d samples   %s",
            v.uncovered, v.doubled, v.samples, ok(v.exactCover)),
        fmt("CAMPBELL ARGYLL  %d ends   n = %d -> %d perceived   %d mirrors %s",
            v.argyllTotal, (int)std::lround((std::sqrt(
                               8.0 * v.argyllPerceived + 1.0) - 1.0) / 2.0),
            v.argyllPerceived, v.argyllMirrors,
            ok(v.argyllPerceived == v.argyllPredicted)),
        fmt("UNIT PROPORTION  max |BW - CA| over units A B C D = %.2f %%",
            v.unitDrift * 100.0f),
        fmt("TWILL ANGLE      42 ends/in = 42 picks/in  ->  %.2f deg",
            std::atan2(1.0, 1.0) * 180.0 / 3.14159265358979),
        fmt("SETT WIDTH       %d ends / 42 epi = %.2f in = %.1f mm", v.total,
            v.total / 42.0f, v.total / 42.0f * 25.4f),
    };
  }

  void buildQuote() {
    // One genuinely justified paragraph: Knuth-Plass, hyphenation on, a real
    // 320 px measure. SigilWeave breaks at SOFT HYPHENS only, so the
    // discretionaries are typed in (U+00AD) the way a compositor would set
    // them -- there is no automatic hyphenation dictionary behind
    // HyphenationOptions.
    weave::TextStyle body = ty(serif(), 13, kInk);
    body.shaping.languageTag = "en-GB";
    weave::TextStyle attrib = ty(serifIt(), 11, kInk2);
    weave::ParagraphBuilder b(body);
    b.addText(
        u8"“The per­fect bal­ance of the weav­ing — "
        u8"ex­actly as many weft shots per inch as there are warp ends, "
        u8"to give a 45-de­gree twill an­gle — is of ut­most "
        u8"im­por­tance in pro­duc­ing a true Tar­tan, "
        u8"as each of the col­or blocks must be squared in its "
        u8"prop­er suc­ces­sion.”  ");
    b.pushStyle(attrib);
    b.addText(u8"— Harriet C. Douglas, Scotch Tartan Setts, "
              u8"Shuttle-Craft Guild, 1949");
    quote = std::make_shared<weave::Paragraph>(b.build());
  }

  // =========================================================================
  // The cloth. One striped ground, 378 phase-shifted picks, and nothing else.

  Element theCloth() {
    Element panel = at(kClothX, kClothY, kClothW, kClothH)
                        .clip(true)
                        .background(styles::dropShadow(C(0x3E3A33, 0.55f),
                                                       {3, 4}, 9))
                        .fill(kWell);

    // the warp on the beam: the whole design, in one dimension
    panel.child(at(0, 0, kClothW, kClothH).fill(warpMat));

    // the picks. Every one is an absolutely-placed leaf with a computed rect
    // and its own progress window -- 378 Yoga nodes in a panel with no layout
    // in it (ROADMAP 2's positioned-leaf-set ask, from the weaving side).
    const float span = kWeaveEnd - kBeamEnd;
    for (int i = 0; i < kPicks; ++i) {
      const int col = (int)S[(size_t)(i % (int)S.size())];
      const int phase = ((i + 2) % 4);
      const float w0 = kBeamEnd + span * ((float)i / (float)kPicks);
      panel.child(at(0, (float)i * kPx, kClothW, kPx)
                      .fill(pickMat[(size_t)(col * 4 + phase)])
                      .opacity(bind(&loom)
                                   .from(w0, w0 + 0.0035f)
                                   .clamp(0.0f, 1.0f)));
    }

    // the shutter: the warp beams on left-to-right. There is no .wipe(), and
    // scaleX on the ground would squash the bands rather than reveal them, so
    // the reveal is a retreating card-coloured blind (ROADMAP 6).
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(kWell)
                    .transformOrigin(1.0f, 0.5f)
                    .scaleX(bind(&loom).from(0.0f, kBeamEnd).invert().clamp(
                        0.0f, 1.0f)));

    // the five cloths. Identical pixels to the woven picks at palette 0, so
    // the hand-off at the end of the weaving beat is invisible; then each
    // family fades over the last, and Modern comes back for the hold.
    struct Turn {
      int pal;
      float a, b;
    };
    const Turn turns[] = {{0, 0.5050f, kWeaveEnd}, {1, 0.6125f, 0.6375f},
                          {2, 0.6625f, 0.6875f},   {3, 0.7125f, 0.7375f},
                          {4, 0.7625f, 0.7875f},   {0, 0.8125f, kTurnEnd}};
    for (const Turn &t : turns)
      panel.child(at(0, 0, kClothW, kClothH)
                      .fill(clothMat[(size_t)t.pal])
                      .opacity(bind(&loom).from(t.a, t.b).clamp(0.0f, 1.0f)));

    // the surface, above the weave: grooves, then yarn tooth. foreground()
    // paints above children and background() hides under the fill, so these
    // are siblings -- Element::overlay() (ROADMAP 10c) is the missing slot.
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(gridMat)
                    .blend(SkBlendMode::kMultiply)
                    .opacity(0.85f));
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(yarnGrain)
                    .blend(SkBlendMode::kOverlay)
                    .opacity(0.16f));

    // the mirror axes, flashed once while the arithmetic proves itself
    for (int rep = 0; rep < 2; ++rep)
      for (int m : v.mirrors) {
        const float x = (float)(m + rep * v.total) * kPx;
        if (x > kClothW)
          continue;
        panel.child(at(x - 0.5f, 0, 1, kClothH)
                        .fill(kRed)
                        .opacity(bind(&loom)
                                     .from(kWeaveEnd, kProveEnd)
                                     .map(plateau(0.25f))
                                     .scale(0.75f)));
      }

    // the fell line: the shuttle's current pick, riding the leading edge
    panel.child(at(0, 0, kClothW, 2)
                    .fill(kRed)
                    .translateY(bind(&loom)
                                    .from(kBeamEnd, kWeaveEnd)
                                    .to(0.0f, kClothH)
                                    .clamp(0.0f, kClothH))
                    .opacity(bind(&loom)
                                 .from(kBeamEnd - 0.01f, kWeaveEnd + 0.01f)
                                 .map(plateau(0.03f))));
    return panel;
  }

  // =========================================================================
  // The sett bar: the thread count as the 1-D object it actually is.

  Element theSettBar() {
    Element g = box();
    g.child(at(kClothX, kBarY, kClothW, kBarH)
                .fill(warpMat)
                .foreground(stroke(1, Fill::color(kRule),
                                   PathFormat::Align::Outer)));

    // run numerals, staggered over two rows so a 4 px band still gets one
    for (size_t r = 0; r < bwRuns.size(); ++r) {
      const float cx =
          kClothX + ((float)runStart[r] + (float)bwRuns[r].n * 0.5f) * kPx;
      g.child(at(cx - 14, kBarY + kBarH + 2 + (r % 2 ? 10.0f : 0.0f), 28, 11)
                  .child(text(U(std::to_string(bwRuns[r].n)),
                              mn(8, r % 2 ? kInk2 : kInk, 0.4f))
                             .textAlign(weave::TextAlignment::kCenter)
                             .width(Dim(28))));
    }

    // the two pivots, snapping in when the arithmetic proves
    for (int rep = 0; rep < 2; ++rep)
      for (size_t i = 0; i < v.mirrors.size(); ++i) {
        const float x = kClothX + (float)(v.mirrors[i] + rep * v.total) * kPx;
        if (x > kClothX + kClothW)
          continue;
        g.child(at(x - 6, kBarY - 12, 12, 10)
                    .outline(shapes::polygon(3, 180))
                    .fill(kRed)
                    .transformOrigin(0.5f, 1.0f)
                    .scale(bind(&loom)
                               .from(kWeaveEnd, kWeaveEnd + 0.035f)
                               .map(backOut())));
        if (rep == 0)
          g.child(at(x - 40, kBarY - 26, 80, 12)
                      .child(text(U(i == 0 ? "PIVOT B/9" : "PIVOT B/3"),
                                  mn(8, kRed, 0.8f))
                                 .textAlign(weave::TextAlignment::kCenter)
                                 .width(Dim(80)))
                      .opacity(bind(&loom)
                                   .from(kWeaveEnd + 0.01f,
                                         kWeaveEnd + 0.045f)
                                   .clamp(0.0f, 1.0f)));
      }

    // the count itself, set as one mono run -- "the DNA of a tartan"
    std::string count;
    static const char *kCode = "KBGYW";
    for (size_t r = 0; r < bwRuns.size(); ++r)
      count += fmt("%c%d ", kCode[bwRuns[r].c], bwRuns[r].n);
    g.child(label(count, mn(11.5f, kInk, 0.3f), kClothX, kBarY + kBarH + 26,
                  kClothW));
    g.child(label("HALF-SETT, REGISTER NOTATION   B/9 K2 B6 K2 B6 K18 G18 K6 "
                  "G18 K18 B18 K2 B/3   =  126  =  252 / 2",
                  mn(8.5f, kInk2, 0.5f), kClothX, kBarY + kBarH + 42,
                  kClothW));
    return g;
  }

  // =========================================================================
  // The weaver's draft: threading across the top, tie-up in the corner,
  // treadling down the right, drawdown filling the body. Four centuries old
  // and still the clearest notation anyone has for a stored program.

  Element theDraft() {
    const float c = kDrawCell;
    const float x0 = kColX, y0 = 176;
    const float bodyY = y0 + 4 * c + 8;
    const float tieX = x0 + kDrawN * c + 8;
    Element g = box();

    g.child(label("THE DRAFT  ·  4 SHAFTS, STRAIGHT DRAW, 2/2 BALANCED "
                  "TWILL, TROMP AS WRIT",
                  mn(9, kInk, 0.6f), kColX, 152, kColW));

    auto cell = [&](float x, float y, bool on, SkColor4f fillC) {
      return at(x, y, c, c)
          .fill(on ? fillC : kCard)
          .foreground(stroke(0.6f, Fill::color(kRule),
                             PathFormat::Align::Inner));
    };

    // threading: shaft (j mod 4), shaft 1 at the top
    for (int j = 0; j < kDrawN; ++j)
      for (int s = 0; s < 4; ++s)
        g.child(cell(x0 + (float)j * c, y0 + (float)s * c,
                     ((kDrawOrigin + j) % 4) == s, kInk));
    // tie-up: treadle t lifts shafts t and t+1
    for (int t = 0; t < 4; ++t)
      for (int s = 0; s < 4; ++s)
        g.child(cell(tieX + (float)t * c, y0 + (float)s * c,
                     s == t || s == (t + 1) % 4, kInk));
    // treadling: as-drawn-in, and it highlights in sync with the fell line
    for (int i = 0; i < kDrawN; ++i) {
      const int t = (kDrawOrigin + i) % 4;
      for (int tt = 0; tt < 4; ++tt)
        g.child(cell(tieX + (float)tt * c, bodyY + (float)i * c, tt == t,
                     kInk));
      g.child(at(tieX - 2, bodyY + (float)i * c, 4 * c + 4, c)
                  .fill(C(0x9A3324, 0.25f))
                  .opacity(bind(&loom)
                               .from(kBeamEnd + (kWeaveEnd - kBeamEnd) *
                                                    (float)i / (float)kDrawN,
                                     kBeamEnd + (kWeaveEnd - kBeamEnd) *
                                                    (float)(i + 1) /
                                                    (float)kDrawN)
                               .map(plateau(0.4f))));
    }
    // drawdown: the cloth itself, at 12 px per thread, kNearest
    g.child(at(x0, bodyY, (float)kDrawN * c, (float)kDrawN * c)
                .child(image(drawdownAsset)
                           .absolute()
                           .inset(0)
                           .sampling(SkSamplingOptions(SkFilterMode::kNearest)))
                .child(at(0, 0, (float)kDrawN * c, (float)kDrawN * c)
                           .fill(patterns::gridLines(c, 0.6f,
                                                     C(0x8A8478, 0.55f))
                                     .material()))
                .foreground(stroke(1, Fill::color(kInk),
                                   PathFormat::Align::Outer)));

    g.child(label(fmt("ENDS %d–%d OF THE SETT, SQUARED AGAINST THEMSELVES"
                      "  ·  12 PX / THREAD",
                      kDrawOrigin + 1, kDrawOrigin + kDrawN),
                  mn(8, kInk2, 0.5f), x0, bodyY + (float)kDrawN * c + 6,
                  kColW));
    return g;
  }

  // =========================================================================

  Element theBlendTable() {
    const float y0 = 540, cell = 48, gap = 6;
    Element g = box();
    g.child(label("THE THIRD COLOURS  ·  WARP ACROSS, WEFT DOWN",
                  mn(9, kInk, 0.6f), kColX, 516, kColW));
    static const char *kName[3] = {"K", "B", "G"};
    const float gx = kColX + 22;
    for (int i = 0; i < 3; ++i) {
      g.child(at(gx + (float)i * (cell + gap), y0 - 13, cell, 11)
                  .child(text(U(kName[i]), mn(9, kInk2, 0.8f))
                             .textAlign(weave::TextAlignment::kCenter)
                             .width(Dim(cell))));
      g.child(at(kColX, y0 + (float)i * (cell + gap) + cell / 2 - 6, 18, 11)
                  .child(text(U(kName[i]), mn(9, kInk2, 0.8f))
                             .textAlign(weave::TextAlignment::kEnd)
                             .width(Dim(18))));
    }
    for (int wf = 0; wf < 3; ++wf)
      for (int wp = 0; wp < 3; ++wp)
        g.child(at(gx + (float)wp * (cell + gap),
                   y0 + (float)wf * (cell + gap), cell, cell)
                    .fill(blendMat[(size_t)(wf * 3 + wp)])
                    .foreground(stroke(1, Fill::color(wp == wf ? kRule : kInk),
                                       PathFormat::Align::Outer)));
    g.child(label(fmt("EACH CELL IS 6 × 6 THREADS AT 8 PX · THE "
                      "BLENDS ARE WOVEN, NOT MIXED"),
                  mn(8, kInk2, 0.4f), gx + 3 * (cell + gap) + 8, y0 - 2, 160));
    g.child(label(fmt("n = %d  →  %d solid + %d blend  =  %d  =  "
                      "n(n+1)/2",
                      v.solids, v.solids, v.blends, v.perceived),
                  mn(9, kRed, 0.4f), gx + 3 * (cell + gap) + 8, y0 + 44, 200));
    g.child(label("A tartan has six colours from three\nthreads because the "
                  "blend is spatial:\nat 42 ends per inch the eye does\nthe "
                  "mixing, not the dyer.",
                  ty(serifIt(), 11, kInk2), gx + 3 * (cell + gap) + 8,
                  y0 + 66, 190));
    return g;
  }

  // =========================================================================

  Element thePaletteStrip() {
    const float y0 = 700, rowH = 36;
    Element g = box();
    g.child(label("ONE THREAD COUNT, FIVE SHADE CARDS  ·  SCOTTISH "
                  "REGISTER OF TARTANS",
                  mn(9, kInk, 0.6f), kColX, 676, kColW));
    // which family the cloth is wearing, right now
    const float spans[5][2] = {{kWeaveEnd, 0.6125f}, {0.6375f, 0.6625f},
                               {0.6875f, 0.7125f},   {0.7375f, 0.7625f},
                               {0.7875f, 0.8125f}};
    for (int r = 0; r < 5; ++r) {
      const float y = y0 + (float)r * rowH;
      const Palette &p = kPalettes[(size_t)r];
      g.child(label(p.name, mn(8, kInk, 0.5f), kColX + 10, y, 130));
      const uint32_t hex[3] = {p.k, p.b, p.g};
      static const char *code[3] = {"K", "B", "G"};
      for (int i = 0; i < 3; ++i) {
        const float x = kColX + 148 + (float)i * 96;
        g.child(at(x, y, 88, 15).fill(C(hex[i])));
        g.child(label(fmt("%s #%06X", code[i], hex[i]), mn(7, kInk2, 0.3f), x,
                      y + 17, 88));
      }
      // the mark, taking the row as the cloth turns
      auto mark = [&](float a, float b) {
        return at(kColX, y - 2, 5, 20)
            .fill(kRed)
            .opacity(bind(&loom).from(a, b).map(plateau(0.12f)));
      };
      g.child(mark(spans[r][0] - 0.012f, spans[r][1] + 0.012f));
      if (r == 0)
        g.child(mark(0.8125f, 1.02f)); // and back to Modern for the hold
    }
    return g;
  }

  // =========================================================================
  // The argument. Four identical cloths under four clan names, and then the
  // cloth Wilsons actually sold as Campbell of Argyll: the same sett with two
  // centre-lines recoloured.

  Element theProvenance() {
    const float y0 = 1006, sw = 180, sh = 116, gap = 16;
    static const char *kNames[4] = {"Campbell Argyll", "Grant", "Munro",
                                    "Sutherland"};
    Element g = box();
    g.child(label("THE COCKBURN COLLECTION, 1810–15  ·  ONE CLOTH, "
                  "FOUR LABELS",
                  mn(9, kInk, 0.6f), kClothX, 982, 700));
    for (int i = 0; i < 4; ++i) {
      const float x = kClothX + (float)i * (sw + gap);
      // the SAME crop of the SAME cloth, four times over
      g.child(at(x, y0, sw, sh)
                  .clip(true)
                  .fill(clothMat[0])
                  .background(styles::dropShadow(C(0x3E3A33, 0.45f), {2, 3}, 6))
                  .foreground(stroke(1, Fill::color(kRule),
                                     PathFormat::Align::Outer))
                  .child(at(0, 0, sw, sh)
                             .fill(gridMat)
                             .blend(SkBlendMode::kMultiply)
                             .opacity(0.8f)));
      g.child(at(x, y0 + sh + 5, sw, 16)
                  .child(text(U(kNames[i]), ty(serifIt(), 12.5f, kInk))
                             .textAlign(weave::TextAlignment::kCenter)
                             .width(Dim(sw)))
                  .opacity(bind(&loom)
                               .from(0.63f + (float)i * 0.022f,
                                     0.66f + (float)i * 0.022f)
                               .clamp(0.0f, 1.0f)));
    }
    g.child(label("“The Cockburn Collection (1810-15) includes four "
                  "specimens of the Government tartan labelled;",
                  ty(serif(), 10.5f, kInk2), kClothX, y0 + sh + 26, 800));
    g.child(label("‘Campbell Argyll’, ‘Grant’, "
                  "‘Munro’ and ‘Sutherland’.”  "
                  "— Scottish Register of Tartans, registration note, "
                  "SRT ref 277",
                  ty(serif(), 10.5f, kInk2), kClothX, y0 + sh + 40, 800));

    // ...and the cloth that carries the name honestly
    const float ax = kClothX + 4 * (sw + gap) + 16;
    g.child(at(ax, y0, sw, sh)
                .clip(true)
                .fill(argyllMat)
                .background(styles::dropShadow(C(0x3E3A33, 0.45f), {2, 3}, 6))
                .foreground(stroke(1, Fill::color(kRed),
                                   PathFormat::Align::Outer))
                .child(at(0, 0, sw, sh)
                           .fill(gridMat)
                           .blend(SkBlendMode::kMultiply)
                           .opacity(0.8f)));
    g.child(at(ax, y0 + sh + 5, sw, 16)
                .child(text(U("Campbell of Argyll"), ty(serifIt(), 12.5f, kRed))
                           .textAlign(weave::TextAlignment::kCenter)
                           .width(Dim(sw))));
    g.child(label("416 ends. The Government sett with", mn(8, kInk2, 0.3f), ax,
                  y0 + sh + 26, sw + 40));
    g.child(label("the two black centre-lines replaced", mn(8, kInk2, 0.3f),
                  ax, y0 + sh + 37, sw + 40));
    g.child(label("by a YELLOW and a WHITE overcheck.", mn(8, kRed, 0.3f), ax,
                  y0 + sh + 48, sw + 40));
    return g;
  }

  // =========================================================================
  // Structure against numbers: the two setts, each normalised to its own
  // total, unit for unit. They agree to under one percent, which is the
  // finding -- a tartan's identity is its structure; its numbers are a
  // matter of authority.

  Element theComparison() {
    const float y0 = 1196, barW = 900, barH = 26, x0 = kClothX + 118;
    Element g = box();
    g.child(label("THE SAME SETT, TWICE  ·  EACH NORMALISED TO ITS OWN "
                  "TOTAL, UNIT FOR UNIT",
                  mn(9, kInk, 0.6f), kClothX, 1172, 900));

    struct Bar {
      const char *name;
      const std::vector<Run> *runs;
      int total;
      float y;
    };
    const Bar bars[2] = {{"BLACK WATCH  252", &bwRuns, v.total, y0},
                         {"CAMPBELL ARGYLL  416", &caRuns, v.argyllTotal,
                          y0 + barH + 8}};
    const Shades modern = shadesOf(kPalettes[0]);
    for (const Bar &b : bars) {
      g.child(label(b.name, mn(8.5f, kInk, 0.5f), kClothX, b.y + 7, 118));
      float cur = 0;
      for (const Run &r : *b.runs) {
        const float w = barW * (float)r.n / (float)b.total;
        Element seg = at(x0 + cur, b.y, w, barH).fill(modern[r.c]);
        if (r.c == Y || r.c == W)
          seg.foreground(stroke(1.5f, Fill::color(kRed),
                                PathFormat::Align::Outer));
        g.child(std::move(seg));
        cur += w;
      }
      g.child(at(x0, b.y, barW, barH)
                  .foreground(stroke(1, Fill::color(kRule),
                                     PathFormat::Align::Outer)));
    }
    // the unit boundaries, dropped through both bars
    float cum = 0;
    const int units[4] = {v.unitA, v.unitB, v.unitC, v.unitB};
    static const char *uname[4] = {"A", "B", "C", "B"};
    for (int u = 0; u < 4; ++u) {
      const float cx = x0 + barW * (cum + (float)units[u] * 0.5f) /
                                (float)v.total;
      g.child(at(cx - 12, y0 - 13, 24, 11)
                  .child(text(U(uname[u]), mn(8.5f, kRed, 0.6f))
                             .textAlign(weave::TextAlignment::kCenter)
                             .width(Dim(24))));
      cum += (float)units[u];
      if (u < 3)
        g.child(at(x0 + barW * cum / (float)v.total - 0.5f, y0 - 4, 1,
                   2 * barH + 16)
                    .fill(C(0x9A3324, 0.75f)));
    }
    g.child(label(fmt("UNIT FRACTIONS AGREE TO %.2f %%  ·  IDENTICAL "
                      "STRUCTURE, RUN FOR RUN  ·  DIFFERENT NUMBERS",
                      v.unitDrift * 100.0f),
                  mn(8.5f, kRed, 0.4f), x0, y0 + 2 * barH + 14, 700));
    return g;
  }

  // =========================================================================

  Element theVerification() {
    const float x0 = 1044, y0 = 1006;
    Element g = box();
    g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", mn(9, kInk, 0.6f), x0,
                  982, kColW));
    g.child(at(x0 - 10, y0 - 6, 500, (float)verifyLines.size() * 13.6f + 12)
                .fill(C(0xDCD4C4, 0.75f))
                .foreground(stroke(1, Fill::color(kRule),
                                   PathFormat::Align::Inner)));
    for (size_t i = 0; i < verifyLines.size(); ++i) {
      const float w0 = kWeaveEnd + (float)i * 0.0095f;
      const std::string &s = verifyLines[i];
      const bool okLine = s.size() > 2 && s.compare(s.size() - 2, 2, "OK") == 0;
      Element row = at(x0, y0 + (float)i * 13.6f, 490, 13)
                        .opacity(bind(&loom)
                                     .from(w0, w0 + 0.011f)
                                     .clamp(0.0f, 1.0f));
      row.child(text(U(okLine ? s.substr(0, s.size() - 2) : s),
                     mn(9.5f, kInk, 0.15f)));
      if (okLine)
        row.child(at(478, 0, 20, 13).child(text(U("OK"), mn(9.5f, kRed, 0.6f))));
      g.child(std::move(row));
    }
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    Element root = stack().width(Dim(kW)).height(Dim(kH));

    // the board, with its tooth
    root.child(at(0, 0, kW, kH).fill(kCard));
    root.child(at(0, 0, kW, kH)
                   .fill(cardGrain)
                   .blend(SkBlendMode::kMultiply)
                   .opacity(0.12f));
    root.child(at(24, 24, kW - 48, kH - 48)
                   .foreground(stroke(1, Fill::color(kRule),
                                      PathFormat::Align::Inner)));

    // 1. the heading
    root.child(label("BLACK WATCH (GOVERNMENT)", sb(34, kInk, 4.6f), kClothX,
                     44, 900));
    root.child(label("STA 207  ·  STWR 207  ·  SRT 277  ·  "
                     "TARTAN DATE 01/01/1739  ·  CATEGORY MILITARY  "
                     "·  DESIGNER UNKNOWN",
                     mn(10.5f, kInk2, 1.2f), kClothX, 92, 1100));
    root.child(label("A THREADCOUNT HAS BEEN DESCRIBED AS THE DNA OF A "
                     "TARTAN.  — SCOTTISH REGISTER OF TARTANS",
                     mn(9, kRed, 0.8f), kClothX, 110, 1100));
    root.child(rule(kClothX, 130, kW - 2 * kClothX, 1, kRule));

    root.child(theSettBar());
    root.child(theCloth());
    root.child(theDraft());
    root.child(theBlendTable());
    root.child(thePaletteStrip());

    // the justified paragraph, at a real measure
    {
      weave::ParagraphLayoutOptions o;
      o.alignment = weave::TextAlignment::kJustify;
      o.lineBreakStrategy = weave::LineBreakStrategy::kKnuthPlass;
      o.hyphenation.enabled = true;
      o.hyphenation.penalty = 45.0f;
      o.justification.spaceStretch = 0.55f;
      o.justification.spaceShrink = 0.30f;
      o.justification.lastLineAlignment = weave::TextAlignment::kStart;
      o.knuthPlass.tolerance = 6000.0f;
      o.lineMetrics.height = 16.5f;
      root.child(at(kColX, 900, 320, 100).child(text(quote, o).width(Dim(320))));
    }

    root.child(theProvenance());
    root.child(theVerification());
    root.child(theComparison());

    root.child(rule(kClothX, 1284, kW - 2 * kClothX, 1, kRule));
    root.child(label(
        "SETT AFTER H. C. DOUGLAS, SCOTCH TARTAN SETTS, SHUTTLE-CRAFT GUILD, "
        "1949  ·  252 ENDS  ·  2/2 TWILL, STRAIGHT DRAW  ·  "
        "PALETTE: SCOTTISH REGISTER OF TARTANS COLOUR SHADES  ·  "
        "63,504 CELLS, NONE OF THEM AUTHORED",
        mn(8.5f, kInk2, 0.9f), kClothX, 1294, kW - 2 * kClothX));
    return root;
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    build();
    ctx.canvas(kW, kH);
    ctx.background(kCard);
    ctx.ticker.add([this](double dt) {
      clock += dt;
      loom = (float)(std::fmod(clock, (double)kCycle) / (double)kCycle);
      return true;
    });
    ctx.composer.render(describe(ctx));

    std::fprintf(stderr,
                 "[black watch] %d ends  mirrors %zu  floats %d/%d  "
                 "colours %d  cover %d/%d of %d  argyll %d ends, %d mirrors, "
                 "%d perceived  unit drift %.3f%%\n",
                 v.total, v.mirrors.size(), v.maxWarpFloat, v.maxWeftFloat,
                 v.perceived, v.uncovered, v.doubled, v.samples, v.argyllTotal,
                 v.argyllMirrors, v.argyllPerceived, v.unitDrift * 100.0f);
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    if (!reported && elapsed > 3.0) {
      reported = true;
      const auto &s = ctx.composer.stats();
      std::fprintf(stderr,
                   "[black watch] instances %zu  pictures %zu  recorded %zu  "
                   "painted-live %zu  layout %.2fms  volatile %.2fms  "
                   "paint %.2fms\n",
                   s.instances, s.picturesLive, s.picturesRecorded,
                   s.nodesPainted, s.layoutMs, s.volatileMs, s.paintMs);
    }
  }
};

SIGIL_SKETCH(BlackWatch)
