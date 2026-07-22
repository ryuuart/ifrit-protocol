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
// is EMERGENT. Nothing about the two-dimensional design is authored here: the
// sett is typed in as Douglas printed it, the twill decides which thread is on
// top at each of the 63,504 crossings, and the plane falls out.
//
// DOCUMENTED — read directly, cited, and in the repo
//  - Harriet C. Douglas, "Scotch Tartan Setts: A Shuttle-Craft Guild Guide For
//    Weaving 132 Traditional Plaids", copyright September 1949, Virginia City,
//    Montana; digitised November 2002 by Nancy M. McKenna. University of
//    Arizona On-Line Digital Archive of Documents on Weaving:
//    https://www2.cs.arizona.edu/patterns/weaving/monographs/dhc_tar.pdf
//    THE load-bearing source. It gives the BLACK WATCH (42nd Regiment) sett
//    unit by unit with its own printed total of 252 ends (kSettA/B/C below,
//    transcribed literally); the CAMPBELL (of Argyll) sett, 416 ends; the loom
//    draft in one sentence — "a straight twill threading (1,2,3,4) and woven
//    as 2-2 balanced twill (1-2, 2-3, 3-4, 4-1)"; the balance rule quoted on
//    the card; and its own caveat, which turns out to be this piece's second
//    subject: "standardization is not complete and minor variations in color
//    and count are found among the works of various authorities."
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
//    kPalettes and kHexY/kHexW below is lifted from that table verbatim, and
//    all seventeen were re-verified against the downloaded PDF before this
//    file was written.
//  - The register's own rendered swatch of SRT 277 (imageCreation.aspx?
//    ref=277) — read for the twill handedness: a diagonal autocorrelation of
//    its blend cells is +0.40..+0.57 on the "\" lag against +0.23..+0.30 on
//    "/", so the rib runs top-left to bottom-right, which is warpUp() below.
//  - Glasgow Libraries, the Cockburn Collection: one volume, 1810-1820,
//    Lt-Gen Sir William Cockburn, 56 specimens of 'hard tartan' by William
//    Wilson & Sons of Bannockburn, now in the Mitchell Library, Glasgow.
//
// RECONSTRUCTED — not anybody's citation
//  - The assignment of particular register shades to the NAMED palette
//    families (Modern / Ancient / Muted / Weathered / Reproduction). The hexes
//    and their colour codes are the register's; which shade belongs to which
//    family is a reading of each family's documented description, not a
//    weaver's shade card.
//  - The unit-proportion finding printed on the card — that Black Watch and
//    Campbell of Argyll agree unit-for-unit to under one percent — is computed
//    here, from the two transcribed setts.
//  - Every number in the verification block is computed at startup by
//    verify(), never asserted. Run it and it recomputes.
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
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
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
// board, so this is the one sketch in the program lit the other way round —
// which is also where patterns::grain belongs, per its own header.

constexpr SkColor4f kCard = C(0xE8E2D6);
constexpr SkColor4f kWell = C(0xDCD4C4);
constexpr SkColor4f kRule = C(0x8A8478);
constexpr SkColor4f kInk = C(0x1A1815);
constexpr SkColor4f kInk2 = C(0x5A554C);
constexpr SkColor4f kRed = C(0x9A3324);

// ---------------------------------------------------------------------------
// The colours. Codes are the register's: K black, B blue, G green, Y yellow,
// W white (the warm one — undyed wool, "lachdann"). Five shade cards, ONE
// thread count: the register publishes nineteen permitted greens, sixteen
// blues and two blacks, and that is the entire mechanism behind the named
// palettes. The cloth's identity is the count; the colour is a variable.

enum Col : uint8_t { K = 0, B = 1, G = 2, Y = 3, W = 4 };
constexpr int kColCount = 5;

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
constexpr uint32_t kHexY = 0xE8C000; // "Yellow"
constexpr uint32_t kHexW = 0xE5DDD1; // "White"

using Shades = std::array<SkColor4f, 5>;
inline Shades shadesOf(const Palette &p) {
  return {C(p.k), C(p.b), C(p.g), C(kHexY), C(kHexW)};
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

// A   18 black  6 blue  2 black  6 blue  2 black 18 blue
//      2 black  6 blue  2 black  6 blue 18 black                    (86)
const std::vector<Run> kSettA{{K, 18}, {B, 6}, {K, 2},  {B, 6}, {K, 2}, {B, 18},
                              {K, 2},  {B, 6}, {K, 2},  {B, 6}, {K, 18}};
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
const std::vector<Run> kArgA{{K, 30}, {B, 6}, {K, 6},  {B, 6}, {K, 6}, {B, 30},
                             {K, 6},  {B, 6}, {K, 6},  {B, 6}, {K, 30}};
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
inline std::vector<Run> concatRuns(const std::vector<std::vector<Run>> &units) {
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
// THE LOOM — the stored program, three lines of it.
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
 *  register says the weft sequence IS the warp sequence, and Douglas's
 *  as-drawn-in treadling says the same thing about the picks. */
inline uint8_t clothAt(const std::vector<uint8_t> &S, int x, int y) {
  const int n = (int)S.size();
  const int xi = ((x % n) + n) % n, yi = ((y % n) + n) % n;
  return warpUp(x, y) ? S[(size_t)xi] : S[(size_t)yi];
}

// ---------------------------------------------------------------------------
// THE INVARIANTS. Six exact checks, all computed here, none asserted. The
// cheap ones do not catch the commonest bug: build the 24 bands as inclusive
// ranges and the sum still says 252, the sequence is still palindromic and the
// cloth still looks right. Only point-sampled coverage sees it.
//
// debug::endpointDegrees deliberately does NOT appear. A weave has no chained
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
  int argyllTotal = 0, argyllMirrors = 0, argyllSolids = 0,
      argyllPerceived = 0, argyllPredicted = 0;
  bool argyllLaw = false;
  float unitDrift = 0; // max |BW unit fraction - CA unit fraction|
};

/** All boundaries b with S[(b+k) mod N] == S[(b-1-k) mod N] for every k. */
inline std::vector<int> findMirrors(const std::vector<uint8_t> &S) {
  const int n = (int)S.size();
  std::vector<int> out;
  for (int b = 0; b < n; ++b) {
    bool ok = true;
    for (int k = 0; k < n / 2 && ok; ++k)
      ok = S[(size_t)((b + k) % n)] == S[(size_t)((((b - 1 - k) % n) + n) % n)];
    if (ok)
      out.push_back(b);
  }
  return out;
}

/** Solid and blend colour counts over the whole sett square. The 2/2 twill is
 *  balanced, so over any 4x4 block the crossing of warp a with weft b shows
 *  eight cells of each: the perceived colour is the UNORDERED pair, which is
 *  where n(n+1)/2 comes from. */
inline void perceivedColours(const std::vector<uint8_t> &S, int &solid,
                             int &blend) {
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
      if (seen[i][j])
        (i == j ? solid : blend)++;
}

inline int gcdOf(int a, int b) { return b ? gcdOf(b, a % b) : a; }

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
  for (uint8_t c : S)
    v.counts[c]++;
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
  for (const Run &r : bwRuns) {
    start.push_back(cur);
    cur += (float)r.n;
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
  // record it with the "..." repeating notation rather than a "/" pivot.
  v.argyllTotal = (int)A.size();
  v.argyllMirrors = (int)findMirrors(A).size();
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
    v.unitDrift =
        std::max(v.unitDrift, std::abs(bwU[i] / (float)v.total -
                                       caU[i] / (float)v.argyllTotal));
  return v;
}

// ---------------------------------------------------------------------------
// Baking. One pixel per thread, written straight into a bitmap, then sampled
// with kNearest at an INTEGER magnification. Element::sampling() landed while
// this was being written; before it, every panel showing the cloth below 1:1
// was at the mercy of a hardcoded kLinear. It still only takes integer scales
// here, because 2 px threads on a 4 px interlacement period minified by any
// non-integer factor is a moire generator whatever the filter is.

/** N32 is kRGBA_8888 on this build and kBGRA_8888 on others, and writing an
 *  SkColor straight into getAddr32() silently swaps R and B on one of them —
 *  the first render of this card came back with the blue threads maroon. */
inline uint32_t packPixel(const SkBitmap &bm, SkColor4f c) {
  const SkColor s = c.toSkColor();
  const uint32_t a = SkColorGetA(s), r = SkColorGetR(s), g = SkColorGetG(s),
                 b = SkColorGetB(s);
  return bm.colorType() == kBGRA_8888_SkColorType
             ? (a << 24) | (r << 16) | (g << 8) | b
             : (a << 24) | (b << 16) | (g << 8) | r;
}

/** `rib` darkens the cells where the WEFT is on top, which is what makes the
 *  twill legible inside a block of one colour. The main cloth panel gets this
 *  as a multiplied overlay tile instead (one element for 63,504 cells); the
 *  drawdown bakes it in, because at 9 px per thread it has no overlay. */
sk_sp<SkImage> bakeCloth(const std::vector<uint8_t> &S, const Shades &sh,
                         int originX, int originY, int w, int h,
                         float rib = 0.0f) {
  SkBitmap bm;
  bm.allocN32Pixels(w, h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      SkColor4f c = sh[clothAt(S, originX + x, originY + y)];
      if (rib > 0 && !warpUp(originX + x, originY + y)) {
        const float k = 1.0f - rib;
        c = {c.fR * k, c.fG * k, c.fB * k, c.fA};
      }
      *bm.getAddr32(x, y) = packPixel(bm, c);
    }
  bm.setImmutable();
  return bm.asImage();
}

/** One cell of the blend table: warp colour `a` crossed with weft colour `b`,
 *  at thread scale. The same generator as the cloth — if these disagree with
 *  the main panel then one of the two is wrong. */
sk_sp<SkImage> bakeBlend(SkColor4f a, SkColor4f b, int threads) {
  SkBitmap bm;
  bm.allocN32Pixels(threads, threads);
  for (int y = 0; y < threads; ++y)
    for (int x = 0; x < threads; ++x)
      *bm.getAddr32(x, y) = packPixel(bm, warpUp(x, y) ? a : b);
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
// paragraph at a real measure.

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              SkFontStyle::Slant slant,
                              const char *fallback = nullptr) {
  auto mgr = weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, SkFontStyle::kNormal_Width, slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
inline const sk_sp<SkTypeface> &sans() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kUpright_Slant, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &sansB() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::kBold_Weight,
                                    SkFontStyle::kUpright_Slant, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &mono() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kUpright_Slant, "Courier New");
  return f;
}
inline const sk_sp<SkTypeface> &serif() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::kNormal_Weight,
           SkFontStyle::kUpright_Slant, "Times New Roman");
  return f;
}
inline const sk_sp<SkTypeface> &serifIt() {
  static sk_sp<SkTypeface> f =
      face("Baskerville", SkFontStyle::kNormal_Weight,
           SkFontStyle::kItalic_Slant, "Times New Roman");
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

// A box at absolute card coordinates. The whole composition is pinned — there
// is no layout in a pattern card — so every panel spells this once.
inline Element at(float x, float y, float w, float h) {
  return box().absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
}
inline Element label(const std::string &s, const weave::TextStyle &st, float x,
                     float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.6f).child(text(U(s), st));
}
inline Element centred(const std::string &s, const weave::TextStyle &st,
                       float x, float y, float w) {
  return at(x, y, w, st.shaping.fontSize * 1.6f)
      .child(text(U(s), st)
                 .textAlign(weave::TextAlignment::kCenter)
                 .width(Dim(w)));
}
inline Element rule(float x, float y, float w, float h, SkColor4f c) {
  return at(x, y, w, h).fill(c);
}

// The curves. bind()'s map() runs after from() normalises, and from() lets the
// value out of [0,1] on both sides, so every shaping function here has to be
// total on the reals.
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

// The timeline, in one place: one Output, five beats.
constexpr float kCycle = 8.0f;
constexpr float kBeamEnd = 0.7f / kCycle;  // 0.0875 — the warp is on the beam
constexpr float kWeaveEnd = 4.1f / kCycle; // 0.5125 — 378 picks have beaten in
constexpr float kProveEnd = 4.9f / kCycle; // 0.6125 — the arithmetic
constexpr float kTurnEnd = 6.8f / kCycle;  // 0.85   — five shade cards

// The card.
constexpr float kCanvasW = 1600, kCanvasH = 1440;
constexpr float kPx = 2; // px per thread, everywhere, and never a fraction
constexpr float kClothX = 64, kClothY = 254;
constexpr float kClothW = 1008, kClothH = 756; // 2.0 x 1.5 setts
constexpr int kPicks = (int)(kClothH / kPx);   // 378
constexpr float kBarY = 152, kBarH = 34;
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
  // Pattern per describe would re-render its tile on every render().
  Pattern warpPattern;                 // the 1-D sequence, 252 bands
  std::array<Pattern, 12> pickPattern; // (colour, twill phase)
  Pattern threadGrid;                  // the interlacement grooves
  Material warpMat, gridMat, cardGrain, yarnGrain, drawGrid, swatchMat;
  std::vector<Material> pickMat;  // 12, resolved once
  std::vector<Material> clothMat; // 5 palettes, whole cloth
  Material argyllMat;
  std::array<Material, 9> blendMat;
  std::shared_ptr<const sigil::image::ImageAsset> drawdownAsset;
  std::vector<std::string> verifyLines;
  std::shared_ptr<weave::Paragraph> quote;

  // The drawdown window: threads 60..91 of the sett. The 18-thread black of
  // unit A sits between the last blue and the first green, so no shorter
  // window carries all three colours at once; this one runs K2 B6 K18 G6 and
  // therefore shows both kinds of blend cell as well as three solids.
  static constexpr int kDrawOrigin = 60, kDrawN = 32;
  static constexpr float kDrawCell = 9;

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
    //    sett wide; the bands are half-open, from the same cursor rule the
    //    expansion used.
    {
      const std::vector<Run> runs = bwRuns;
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

    // 2. THE WEFT — the defining property of a twill is that the
    //    interlacement advances ONE THREAD PER PICK, and a Pattern cannot pan
    //    (ROADMAP 14). Saved only by the phase being mod 4: the weft shows
    //    where (x - y) mod 4 is in {2, 3}, i.e. a 4-px-on / 4-px-off stripe
    //    whose phase is ((i + 2) mod 4) * 2 px. Three colours by four phases
    //    is twelve patterns, built once, and every one of the 378 picks
    //    selects one of them. A weave with any other modulus would have had
    //    nothing to select from.
    for (int c = 0; c < 3; ++c)
      for (int ph = 0; ph < 4; ++ph) {
        const SkColor4f col = modern[(size_t)c];
        const float off = (float)ph * kPx;
        pickPattern[(size_t)(c * 4 + ph)] = Pattern::tile(
            {4 * kPx, kPx}, [col, off](SkCanvas &cv, SkSize sz, uint32_t) {
              SkPaint p;
              p.setColor4f(col, nullptr);
              // the float, plus its wrap copy so the tile stays seamless
              cv.drawRect(SkRect::MakeXYWH(off, 0, 2 * kPx, sz.height()), p);
              cv.drawRect(
                  SkRect::MakeXYWH(off - sz.width(), 0, 2 * kPx, sz.height()),
                  p);
            });
      }
    pickMat.clear();
    for (Pattern &p : pickPattern)
      pickMat.push_back(p.material());

    // 3. THE INTERLACEMENT SHADOW — in real cloth the thread that is UNDER at
    //    a cell is shaded by the one on top, and that is why the twill rib is
    //    legible even inside a block of ONE colour. Read it off the register's
    //    swatch: its solid green square is not flat, it is ribbed. So the
    //    overlay is not a grid, it is the DRAFT again, at very low alpha —
    //    one 4x4-thread tile (the twill's own period), weft-up cells darkened,
    //    plus a hairline at each pick and end boundary for the yarn grooves.
    //    One element, multiplied over the whole panel.
    threadGrid =
        Pattern::tile({4 * kPx, 4 * kPx}, [](SkCanvas &c, SkSize, uint32_t) {
          SkPaint p;
          p.setColor4f({0, 0, 0, 0.17f}, nullptr);
          for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
              if (!warpUp(x, y))
                c.drawRect(SkRect::MakeXYWH((float)x * kPx, (float)y * kPx,
                                            kPx, kPx),
                           p);
          p.setColor4f({0, 0, 0, 0.16f}, nullptr);
          for (int i = 0; i < 4; ++i)
            c.drawRect(SkRect::MakeXYWH(0, (float)i * kPx, 4 * kPx, 1), p);
          p.setColor4f({0, 0, 0, 0.09f}, nullptr);
          for (int i = 0; i < 4; ++i)
            c.drawRect(SkRect::MakeXYWH((float)i * kPx, 0, 1, 4 * kPx), p);
        });
    gridMat = threadGrid.material();
    drawGrid = patterns::gridLines(kDrawCell, 0.7f, C(0x8A8478, 0.6f))
                   .material();

    // 4. WHOLE-CLOTH BAKES — one per palette family, 252 x 252 at one pixel
    //    per thread, magnified x2 with kNearest. Same 252 threads, same
    //    draft, five legitimate cloths: the palette turn costs five images
    //    and changes nothing structural, which IS the colour argument.
    clothMat.clear();
    for (const Palette &p : kPalettes)
      clothMat.push_back(imageMat(
          bakeCloth(S, shadesOf(p), 0, 0, (int)S.size(), (int)S.size()), kPx));

    // The provenance swatches are CROPS, never a scaled-down sett: 90 x 55
    // threads at the same 2 px, taken at the same offset in all four. Origin
    // (34, 40) puts a blue square, a green square, the black bars between
    // them and both kinds of blend cell inside 180 x 110 px.
    swatchMat = imageMat(bakeCloth(S, modern, 34, 40, 90, 55), kPx);
    // Campbell's two overchecks are 208 threads apart, so no single square
    // crop contains both. This one takes its warp from the yellow band and
    // its weft from the white one — a real region of the cloth, and the only
    // place the two overchecks are seen crossing.
    argyllMat = imageMat(bakeCloth(A, modern, 150, 358, 90, 55), kPx);

    // 5. THE BLEND TABLE and THE DRAWDOWN — the same generator at different
    //    scales, never authored separately.
    for (int wp = 0; wp < 3; ++wp)
      for (int wf = 0; wf < 3; ++wf)
        blendMat[(size_t)(wf * 3 + wp)] =
            imageMat(bakeBlend(modern[(size_t)wp], modern[(size_t)wf], 6), 8);
    drawdownAsset = std::make_shared<const sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(
            bakeCloth(S, modern, kDrawOrigin, kDrawOrigin, kDrawN, kDrawN,
                      0.22f)));

    // 6. Card tooth and yarn tooth. grain() is the LUMINANCE field, and an
    //    opaque manila board is where its header says it belongs. Both keep
    //    frequency * stretch * 2^(octaves-1) under 0.4, or the y axis aliases
    //    into hash noise with no diagnostic.
    cardGrain = patterns::grain(0.045f, 4, 7.0f, 0.60f);
    yarnGrain = patterns::grain(0.09f, 3, 3.0f, 0.75f);

    buildVerifyLines();
    buildQuote();
  }

  void buildVerifyLines() {
    auto ok = [](bool b) { return b ? "OK" : "**"; };
    verifyLines = {
        fmt("SETT CLOSES      %d + %d + %d + %d = %d ends  (printed: %d)%s",
            v.unitA, v.unitB, v.unitC, v.unitB, v.total, kPublishedEnds,
            ok(v.closes)),
        fmt("REFLECTIVE       mirrors at thread %d, %d   gap %d = %d/2%s",
            v.mirrors.size() > 0 ? v.mirrors[0] : -1,
            v.mirrors.size() > 1 ? v.mirrors[1] : -1, v.mirrorGap, v.total,
            ok(v.reflective)),
        fmt("2/2 BALANCE      max warp float %d   max weft float %d   "
            "%d x %d%s",
            v.maxWarpFloat, v.maxWeftFloat, v.total, v.total, ok(v.balanced)),
        fmt("THREAD RATIO     K %d : B %d : G %d  =  %d : %d : %d%s",
            v.counts[K], v.counts[B], v.counts[G], v.counts[K] / v.gcd3,
            v.counts[B] / v.gcd3, v.counts[G] / v.gcd3, ok(v.blueIsThird)),
        fmt("COLOUR LAW       n = %d  ->  %d solid + %d blend = %d = "
            "n(n+1)/2%s",
            v.solids, v.solids, v.blends, v.perceived, ok(v.colourLaw)),
        fmt("EXACT COVER      %d uncovered / %d doubled of %d samples%s",
            v.uncovered, v.doubled, v.samples, ok(v.exactCover)),
        fmt("CAMPBELL ARGYLL  %d ends   n = %d -> %d perceived   "
            "%d mirrors%s",
            v.argyllTotal, v.argyllSolids, v.argyllPerceived, v.argyllMirrors,
            ok(v.argyllLaw && v.argyllTotal == kPublishedArgyll)),
        fmt("UNIT DRIFT       max |BW - CA| over units A B C D = %.2f %%",
            v.unitDrift * 100.0f),
        fmt("TWILL ANGLE      42 ends/in = 42 picks/in  ->  %.2f deg",
            std::atan2(1.0, 1.0) * 180.0 / 3.14159265358979),
        fmt("SETT WIDTH       %d ends / 42 epi = %.2f in = %.1f mm", v.total,
            (float)v.total / 42.0f, (float)v.total / 42.0f * 25.4f),
    };
  }

  void buildQuote() {
    // One genuinely justified paragraph: Knuth-Plass, hyphenation on, a real
    // 320 px measure. SigilWeave breaks at SOFT HYPHENS only, so the
    // discretionaries are typed in (U+00AD) the way a compositor would set
    // them — HyphenationOptions has no automatic dictionary behind it.
    weave::TextStyle body = ty(serif(), 13, kInk);
    body.shaping.languageTag = "en-GB";
    weave::TextStyle attrib = ty(serifIt(), 11, kInk2);
    weave::ParagraphBuilder b(body);
    b.addText(u8"“The per­fect bal­ance of the weav­ing "
              u8"— ex­actly as many weft shots per inch as there "
              u8"are warp ends, to give a 45-de­gree twill an­gle "
              u8"— is of ut­most im­por­tance in "
              u8"pro­duc­ing a true Tar­tan, as each of the "
              u8"col­or blocks must be squared in its prop­er "
              u8"suc­ces­sion.”  ");
    b.pushStyle(attrib);
    b.addText(u8"— Harriet C. Douglas, Scotch Tartan Setts, "
              u8"Shuttle-Craft Guild, 1949");
    quote = std::make_shared<weave::Paragraph>(b.build());
  }

  // =========================================================================
  // The cloth. One striped ground, 378 phase-shifted picks, and nothing else.

  Element theCloth() {
    Element panel =
        at(kClothX, kClothY, kClothW, kClothH)
            .clip(true)
            .background(styles::dropShadow(C(0x3E3A33, 0.55f), {3, 4}, 10))
            .fill(kWell);

    // the warp on the beam: the whole design, in one dimension
    panel.child(at(0, 0, kClothW, kClothH).fill(warpMat));

    // the picks. Every one is an absolutely-placed leaf with a computed rect
    // and its own progress window — 378 Yoga nodes in a panel with no layout
    // in it (ROADMAP 2's positioned-leaf-set ask, hit again from the weaving
    // side, where the "instance" is 1008 x 2 px of a pattern fill).
    const float span = kWeaveEnd - kBeamEnd;
    for (int i = 0; i < kPicks; ++i) {
      const int col = (int)S[(size_t)(i % (int)S.size())];
      const int phase = (i + 2) % 4;
      const float w0 = kBeamEnd + span * ((float)i / (float)kPicks);
      panel.child(
          at(0, (float)i * kPx, kClothW, kPx)
              .fill(pickMat[(size_t)(col * 4 + phase)])
              .opacity(bind(&loom).from(w0, w0 + 0.0035f).clamp(0.0f, 1.0f)));
    }

    // the shutter: the warp beams on left-to-right. There is no .wipe(), and
    // scaleX on the ground itself would squash the bands rather than reveal
    // them, so the reveal is a retreating card-coloured blind (ROADMAP 6).
    panel.child(
        at(0, 0, kClothW, kClothH)
            .fill(kWell)
            .transformOrigin(1.0f, 0.5f)
            .scaleX(
                bind(&loom).from(0.0f, kBeamEnd).invert().clamp(0.0f, 1.0f)));

    // the five cloths. Pixel-identical to the woven picks at palette 0, so
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

    // the surface, above the weave: rib shadow, then yarn tooth. These are
    // siblings rather than decorations, and not for the reason a study would
    // guess — Element::overlay() has landed, but it paints UNDER children,
    // and foreground() (which is above them) takes a Decoration, of which
    // none of the four primitives is "flood the outline with this Material
    // through this blend mode". The raw-PaintProgram form would work and
    // never prunes. Sibling elements with .blend() do prune, so that is what
    // these are. Both grain layers carry Cache::Texture: a static SkSL
    // Material's SHADER caches, its PIXELS do not, and re-evaluating this one
    // per frame cost 480 ms (see the report).
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(gridMat)
                    .blend(SkBlendMode::kMultiply)
                    .opacity(0.9f));
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(yarnGrain)
                    .blend(SkBlendMode::kOverlay)
                    .cache(Cache::Texture)
                    .opacity(0.14f));

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
                                     .scale(0.8f)));
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
  // The sett bar: the thread count as the one-dimensional object it is,
  // warp-aligned with the cloth under it.

  Element theSettBar() {
    Element g = box();
    g.child(at(kClothX, kBarY, kClothW, kBarH)
                .fill(warpMat)
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Outer)));

    // run numerals, staggered over two rows so a 4 px band still gets one
    for (size_t r = 0; r < bwRuns.size(); ++r) {
      const float cx =
          kClothX + ((float)runStart[r] + (float)bwRuns[r].n * 0.5f) * kPx;
      g.child(centred(std::to_string(bwRuns[r].n),
                      mn(8, r % 2 ? kInk2 : kInk, 0.4f), cx - 14,
                      kBarY + kBarH + 3 + (r % 2 ? 10.0f : 0.0f), 28));
    }

    // the two pivots, snapping in when the arithmetic proves
    for (int rep = 0; rep < 2; ++rep)
      for (size_t i = 0; i < v.mirrors.size(); ++i) {
        const float x = kClothX + (float)(v.mirrors[i] + rep * v.total) * kPx;
        if (x > kClothX + kClothW)
          continue;
        g.child(at(x - 6, kBarY - 11, 12, 10)
                    .outline(shapes::polygon(3, 180))
                    .fill(kRed)
                    .transformOrigin(0.5f, 1.0f)
                    .scale(bind(&loom)
                               .from(kWeaveEnd, kWeaveEnd + 0.035f)
                               .map(backOut())));
        if (rep == 0)
          g.child(centred(i == 0 ? "PIVOT  B/9" : "PIVOT  B/3",
                          mn(8, kRed, 0.8f), x - 45, kBarY - 26, 90)
                      .opacity(bind(&loom)
                                   .from(kWeaveEnd + 0.01f, kWeaveEnd + 0.045f)
                                   .clamp(0.0f, 1.0f)));
      }

    // the register's own phrase, out of the way of the pivot flags
    g.child(label("“THE DNA OF A TARTAN.”   — SCOTTISH REGISTER OF TARTANS, "
                  "ON THE THREADCOUNT",
                  mn(8.5f, kRed, 0.4f), 660, kBarY - 26, 420));

    // the count itself, set as one mono run
    std::string count;
    static const char kCode[] = "KBGYW";
    for (size_t r = 0; r < bwRuns.size(); ++r)
      count += fmt("%c%d ", kCode[bwRuns[r].c], bwRuns[r].n);
    g.child(label(count, mn(11.5f, kInk, 0.3f), kClothX, kBarY + kBarH + 27,
                  kClothW));
    g.child(label("HALF-SETT, REGISTER NOTATION:  B/9 K2 B6 K2 B6 K18 G18 K6 "
                  "G18 K18 B18 K2 B/3   =  126  =  252 / 2",
                  mn(8.5f, kInk2, 0.5f), kClothX, kBarY + kBarH + 47, kClothW));
    return g;
  }

  // =========================================================================
  // The weaver's draft: threading across the top, tie-up in the corner,
  // treadling down the right, drawdown filling the body. Four centuries old,
  // and still the clearest notation anyone has for a stored program.

  Element theDraft() {
    const float c = kDrawCell;
    const float x0 = kColX, y0 = 172;
    const float bodyY = y0 + 4 * c + 20;    // 228
    const float tieX = x0 + kDrawN * c + 8; // 1400
    Element g = box();

    g.child(label("THE DRAFT  ·  4 SHAFTS, STRAIGHT DRAW, 2/2 BALANCED TWILL, "
                  "TROMP AS WRIT",
                  mn(9, kInk, 0.5f), kColX, 140, kColW + 40));

    auto cell = [&](float x, float y, bool on) {
      return at(x, y, c, c)
          .fill(on ? kInk : kCard)
          .foreground(
              stroke(0.6f, Fill::color(kRule), PathFormat::Align::Inner));
    };

    // threading — shaft (j mod 4), shaft 1 at the top
    for (int j = 0; j < kDrawN; ++j)
      for (int s = 0; s < 4; ++s)
        g.child(cell(x0 + (float)j * c, y0 + (float)s * c,
                     ((kDrawOrigin + j) % 4) == s));
    // tie-up — treadle t lifts shafts t and t+1
    for (int t = 0; t < 4; ++t)
      for (int s = 0; s < 4; ++s)
        g.child(cell(tieX + (float)t * c, y0 + (float)s * c,
                     s == t || s == (t + 1) % 4));
    // treadling — as-drawn-in, and it highlights in sync with the fell line
    for (int i = 0; i < kDrawN; ++i) {
      const int t = (kDrawOrigin + i) % 4;
      for (int tt = 0; tt < 4; ++tt)
        g.child(cell(tieX + (float)tt * c, bodyY + (float)i * c, tt == t));
      const float a = kBeamEnd + (kWeaveEnd - kBeamEnd) * (float)i /
                                     (float)kDrawN;
      const float b = kBeamEnd + (kWeaveEnd - kBeamEnd) * (float)(i + 1) /
                                     (float)kDrawN;
      g.child(at(tieX - 3, bodyY + (float)i * c, 4 * c + 6, c)
                  .fill(C(0x9A3324, 0.30f))
                  .opacity(bind(&loom).from(a, b).map(plateau(0.35f))));
    }
    // drawdown — the cloth itself, at 12 px per thread, kNearest
    g.child(
        at(x0, bodyY, (float)kDrawN * c, (float)kDrawN * c)
            .child(image(drawdownAsset)
                       .absolute()
                       .inset(0)
                       .sampling(SkSamplingOptions(SkFilterMode::kNearest)))
            .child(at(0, 0, (float)kDrawN * c, (float)kDrawN * c)
                       .fill(drawGrid))
            .foreground(
                stroke(1, Fill::color(kInk), PathFormat::Align::Outer)));

    g.child(label(fmt("ENDS %d-%d OF THE SETT (K2 B6 K18 G6), SQUARED AGAINST "
                      "THEMSELVES  ·  %d PX / THREAD",
                      kDrawOrigin + 1, kDrawOrigin + kDrawN, (int)c),
                  mn(8, kInk2, 0.4f), x0, bodyY + (float)kDrawN * c + 6,
                  kColW + 40));
    // shaft numbers down the left of the threading block
    for (int s = 0; s < 4; ++s)
      g.child(centred(std::to_string(s + 1), mn(7, kInk2), x0 - 15,
                      y0 + (float)s * c - 1, 13));
    g.child(centred("THREADING", mn(7, kInk2, 0.6f), x0 + 60, y0 - 11, 120));
    g.child(centred("TIE-UP", mn(7, kInk2, 0.6f), tieX - 8, y0 - 11, 60));
    g.child(centred("TREADLING", mn(7, kInk2, 0.6f), tieX - 14, bodyY - 12,
                    72));
    g.child(centred("DRAWDOWN", mn(7, kInk2, 0.6f), x0 + 60, bodyY - 12, 120));
    return g;
  }

  // =========================================================================

  Element theBlendTable() {
    const float y0 = 584, cell = 46, gap = 6, gx = kColX + 20;
    Element g = box();
    g.child(label("THE THIRD COLOURS  ·  WARP ACROSS, WEFT DOWN",
                  mn(9, kInk, 0.5f), kColX, 556, kColW));
    static const char *kName[3] = {"K", "B", "G"};
    for (int i = 0; i < 3; ++i) {
      g.child(centred(kName[i], mn(9, kInk2, 0.8f),
                      gx + (float)i * (cell + gap), y0 - 14, cell));
      g.child(centred(kName[i], mn(9, kInk2, 0.8f), kColX,
                      y0 + (float)i * (cell + gap) + cell / 2 - 7, 16));
    }
    for (int wf = 0; wf < 3; ++wf)
      for (int wp = 0; wp < 3; ++wp)
        g.child(at(gx + (float)wp * (cell + gap),
                   y0 + (float)wf * (cell + gap), cell, cell)
                    .fill(blendMat[(size_t)(wf * 3 + wp)])
                    .foreground(stroke(1, Fill::color(wp == wf ? kRule : kInk),
                                       PathFormat::Align::Outer)));
    const float tx = gx + 3 * (cell + gap) + 12;
    g.child(label("EACH CELL IS 6 × 6 THREADS AT 8 PX.", mn(8, kInk2, 0.3f),
                  tx, y0 - 2, 200));
    g.child(label("THE BLENDS ARE WOVEN, NOT MIXED.", mn(8, kInk2, 0.3f), tx,
                  y0 + 10, 200));
    g.child(label(fmt("n = %d  →  %d solid + %d blend  =  %d  =  n(n+1)/2",
                      v.solids, v.solids, v.blends, v.perceived),
                  mn(8.5f, kRed, 0.2f), tx, y0 + 30, 210));
    g.child(label("A tartan has six colours from\nthree threads because the "
                  "blend\nis spatial: at 42 ends per inch\nthe eye does the "
                  "mixing, not\nthe dyer.",
                  ty(serifIt(), 11, kInk2), tx, y0 + 52, 200));
    return g;
  }

  // =========================================================================

  Element thePaletteStrip() {
    const float y0 = 790, rowH = 34;
    Element g = box();
    g.child(label("ONE THREAD COUNT, FIVE SHADE CARDS  ·  SCOTTISH REGISTER "
                  "OF TARTANS",
                  mn(9, kInk, 0.5f), kColX, 762, kColW + 40));
    // which family the cloth is wearing, right now
    const float spans[5][2] = {{kWeaveEnd, kProveEnd}, {0.6375f, 0.6625f},
                               {0.6875f, 0.7125f},     {0.7375f, 0.7625f},
                               {0.7875f, 0.8125f}};
    static const char *code[3] = {"K", "B", "G"};
    for (int r = 0; r < 5; ++r) {
      const float y = y0 + (float)r * rowH;
      const Palette &p = kPalettes[(size_t)r];
      g.child(label(p.name, mn(7.5f, kInk, 0.4f), kColX + 10, y + 1, 140));
      const uint32_t hex[3] = {p.k, p.b, p.g};
      for (int i = 0; i < 3; ++i) {
        const float x = kColX + 150 + (float)i * 94;
        g.child(at(x, y, 86, 14).fill(C(hex[i])));
        g.child(label(fmt("%s #%06X", code[i], hex[i]), mn(7, kInk2, 0.2f), x,
                      y + 16, 86));
      }
      auto mark = [&](float a, float b) {
        return at(kColX, y - 2, 5, 22)
            .fill(kRed)
            .opacity(bind(&loom).from(a, b).map(plateau(0.12f)));
      };
      g.child(mark(spans[r][0] - 0.012f, spans[r][1] + 0.012f));
      if (r == 0)
        g.child(mark(0.8125f, 1.03f)); // and back to Modern for the hold
    }
    return g;
  }

  // =========================================================================
  // The argument. Four identical cloths under four clan names, and then the
  // cloth Wilsons actually sold as Campbell of Argyll: the same sett with two
  // centre-lines recoloured.

  Element theProvenance() {
    const float y0 = 1052, sw = 180, sh = 110, gap = 16;
    static const char *kNames[4] = {"Campbell Argyll", "Grant", "Munro",
                                    "Sutherland"};
    Element g = box();
    g.child(label("THE COCKBURN COLLECTION, 1810–15  ·  ONE CLOTH, FOUR "
                  "LABELS",
                  mn(9, kInk, 0.5f), kClothX, 1030, 700));
    for (int i = 0; i < 4; ++i) {
      const float x = kClothX + (float)i * (sw + gap);
      // the SAME crop of the SAME cloth, four times over
      g.child(at(x, y0, sw, sh)
                  .clip(true)
                  .background(styles::dropShadow(C(0x3E3A33, 0.45f), {2, 3}, 7))
                  .fill(swatchMat)
                  .foreground(stroke(1, Fill::color(kRule),
                                     PathFormat::Align::Outer))
                  .child(at(0, 0, sw, sh)
                             .fill(gridMat)
                             .blend(SkBlendMode::kMultiply)
                             .opacity(0.85f)));
      g.child(centred(kNames[i], ty(serifIt(), 13, kInk), x, y0 + sh + 6, sw)
                  .opacity(bind(&loom)
                               .from(0.63f + (float)i * 0.022f,
                                     0.66f + (float)i * 0.022f)
                               .clamp(0.0f, 1.0f)));
    }
    g.child(label("“The Cockburn Collection (1810-15) includes four specimens "
                  "of the Government tartan labelled;",
                  ty(serif(), 10.5f, kInk2), kClothX, y0 + sh + 28, 800));
    g.child(label("‘Campbell Argyll’, ‘Grant’, ‘Munro’ and ‘Sutherland’.”   "
                  "— Scottish Register of Tartans, registration note, SRT 277",
                  ty(serif(), 10.5f, kInk2), kClothX, y0 + sh + 43, 800));

    // ...and the cloth that carries one of those names honestly
    const float ax = kClothX + 4 * (sw + gap) + 12;
    g.child(at(ax, y0, sw, sh)
                .clip(true)
                .background(styles::dropShadow(C(0x3E3A33, 0.45f), {2, 3}, 7))
                .fill(argyllMat)
                .foreground(
                    stroke(1.5f, Fill::color(kRed), PathFormat::Align::Outer))
                .child(at(0, 0, sw, sh)
                           .fill(gridMat)
                           .blend(SkBlendMode::kMultiply)
                           .opacity(0.85f)));
    g.child(centred("Campbell of Argyll", ty(serifIt(), 13, kRed), ax,
                    y0 + sh + 6, sw));
    g.child(label("416 ends: the Government sett", mn(8, kInk2, 0.2f), ax,
                  y0 + sh + 28, sw + 30));
    g.child(label("with its two black centre-lines", mn(8, kInk2, 0.2f), ax,
                  y0 + sh + 39, sw + 30));
    g.child(label("recoloured YELLOW and WHITE.", mn(8, kRed, 0.2f), ax,
                  y0 + sh + 50, sw + 30));
    return g;
  }

  // =========================================================================
  // Structure against numbers: the two setts, each normalised to its own
  // total, unit for unit. They agree to under one percent — a tartan's
  // identity is its structure; its numbers are a matter of authority.

  Element theComparison() {
    const float y0 = 1276, barW = 880, barH = 26, x0 = kClothX + 140;
    Element g = box();
    g.child(label("THE SAME SETT, TWICE  ·  EACH NORMALISED TO ITS OWN TOTAL, "
                  "UNIT FOR UNIT",
                  mn(9, kInk, 0.5f), kClothX, 1242, 900));

    struct Bar {
      const char *name;
      const std::vector<Run> *runs;
      int total;
      float y;
    };
    const Bar bars[2] = {
        {"BLACK WATCH        252", &bwRuns, v.total, y0},
        {"CAMPBELL ARGYLL   416", &caRuns, v.argyllTotal, y0 + barH + 8}};
    const Shades modern = shadesOf(kPalettes[0]);
    for (const Bar &b : bars) {
      g.child(label(b.name, mn(8.5f, kInk, 0.4f), kClothX, b.y + 8, 140));
      float cur = 0;
      for (const Run &r : *b.runs) {
        const float w = barW * (float)r.n / (float)b.total;
        Element seg = at(x0 + cur, b.y, w, barH).fill(modern[(size_t)r.c]);
        if (r.c == Y || r.c == W)
          seg.foreground(
              stroke(1.5f, Fill::color(kRed), PathFormat::Align::Outer));
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
      const float cx =
          x0 + barW * (cum + (float)units[u] * 0.5f) / (float)v.total;
      g.child(centred(uname[u], mn(8.5f, kRed, 0.6f), cx - 12, y0 - 15, 24));
      cum += (float)units[u];
      if (u < 3)
        g.child(at(x0 + barW * cum / (float)v.total - 0.5f, y0 - 3, 1,
                   2 * barH + 14)
                    .fill(C(0x9A3324, 0.8f)));
    }
    g.child(label(fmt("UNIT FRACTIONS AGREE TO %.2f %%  ·  IDENTICAL "
                      "STRUCTURE, RUN FOR RUN  ·  DIFFERENT NUMBERS  ·  "
                      "THE OVERCHECKS ARE RINGED",
                      v.unitDrift * 100.0f),
                  mn(8.5f, kRed, 0.3f), x0, y0 + 2 * barH + 14, 900));
    return g;
  }

  // =========================================================================

  Element theVerification() {
    const float x0 = 1060, y0 = 1052, lh = 13.6f;
    Element g = box();
    g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", mn(9, kInk, 0.5f), x0,
                  1030, kColW));
    g.child(at(x0 - 12, y0 - 8, 472, (float)verifyLines.size() * lh + 14)
                .fill(C(0xDCD4C4, 0.8f))
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    for (size_t i = 0; i < verifyLines.size(); ++i) {
      const float w0 = kWeaveEnd + (float)i * 0.0092f;
      const std::string &s = verifyLines[i];
      const bool okLine = s.size() > 2 && s.compare(s.size() - 2, 2, "OK") == 0;
      Element row =
          at(x0, y0 + (float)i * lh, 450, 13)
              .opacity(bind(&loom).from(w0, w0 + 0.011f).clamp(0.0f, 1.0f));
      row.child(text(U(okLine ? s.substr(0, s.size() - 2) : s),
                     mn(9.5f, kInk, 0.1f)));
      if (okLine)
        row.child(at(414, 0, 24, 13).child(text(U("OK"), mn(9.5f, kRed, 0.6f))));
      g.child(std::move(row));
    }
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    Element root = stack().width(Dim(kCanvasW)).height(Dim(kCanvasH));

    // the board, with its tooth
    root.child(at(0, 0, kCanvasW, kCanvasH).fill(kCard));
    root.child(at(0, 0, kCanvasW, kCanvasH)
                   .fill(cardGrain)
                   .blend(SkBlendMode::kMultiply)
                   .cache(Cache::Texture)
                   .opacity(0.13f));
    root.child(at(24, 24, kCanvasW - 48, kCanvasH - 48)
                   .foreground(
                       stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));

    // 1. the heading
    root.child(label("BLACK WATCH (GOVERNMENT)", sb(34, kInk, 4.6f), kClothX,
                     46, 900));
    root.child(label("STA 207  ·  STWR 207  ·  SRT 277  ·  TARTAN DATE "
                     "01/01/1739  ·  CATEGORY MILITARY  ·  DESIGNER UNKNOWN",
                     mn(10.5f, kInk2, 1.1f), kClothX, 96, 1200));
    root.child(rule(kClothX, 122, kCanvasW - 2 * kClothX, 1, kRule));

    // the specimen ticket — the physical facts, in the header's dead corner
    {
      static const char *spec[] = {
          "WORSTED WOOL  ·  2/2 BALANCED TWILL  ·  STRAIGHT DRAW",
          "252 ENDS AT 42 EPI  =  6.00 IN SETT  =  152.4 MM",
          "KILTING CLOTH 10 / 13 / 16 OZ  ·  REGIMENTAL WORSTED ~21 OZ",
          "SAMPLE: SCOTTISH TARTANS AUTHORITY, SCARLETT COLLECTION",
          "OLDEST DATED: COCKBURN COLLECTION, MITCHELL LIBRARY, GLASGOW",
      };
      for (int i = 0; i < 5; ++i)
        root.child(label(spec[i], mn(7.5f, kInk2, 0.35f), kColX,
                         48 + (float)i * 12.5f, kColW + 40));
      root.child(rule(kColX - 14, 46, 1, 62, kRule));
    }

    root.child(theSettBar());
    root.child(theCloth());
    root.child(theDraft());
    root.child(theBlendTable());
    root.child(thePaletteStrip());
    root.child(theProvenance());
    root.child(theVerification());
    root.child(theComparison());

    // the justified paragraph, at a real 320 px measure
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
      o.lineMetrics.height = 16.0f;
      root.child(
          at(kColX, 1246, 320, 150).child(text(quote, o).width(Dim(320))));
    }

    root.child(rule(kClothX, 1408, kCanvasW - 2 * kClothX, 1, kRule));
    root.child(label(
        "SETT AFTER H. C. DOUGLAS, SCOTCH TARTAN SETTS, SHUTTLE-CRAFT GUILD, "
        "1949  ·  252 ENDS  ·  2/2 TWILL, STRAIGHT DRAW  ·  PALETTE: SCOTTISH "
        "REGISTER OF TARTANS COLOUR SHADES  ·  63,504 CELLS, NONE AUTHORED",
        mn(8.5f, kInk2, 0.7f), kClothX, 1414, kCanvasW - 2 * kClothX));
    return root;
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    build();
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(kCard);
    ctx.ticker.add([this](double dt) {
      clock += dt;
      loom = (float)(std::fmod(clock, (double)kCycle) / (double)kCycle);
      return true;
    });
    ctx.composer.render(describe(ctx));

    std::fprintf(stderr,
                 "[black watch] %d ends  mirrors %zu (gap %d)  floats %d/%d  "
                 "perceived %d  cover %d uncovered / %d doubled of %d  "
                 "argyll %d ends, %d mirrors, %d perceived  "
                 "unit drift %.3f%%\n",
                 v.total, v.mirrors.size(), v.mirrorGap, v.maxWarpFloat,
                 v.maxWeftFloat, v.perceived, v.uncovered, v.doubled,
                 v.samples, v.argyllTotal, v.argyllMirrors, v.argyllPerceived,
                 v.unitDrift * 100.0f);
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // Read after the card has come to rest, and after the host has actually
    // drawn: the headless path steps the clock without drawing until it
    // captures, so an early read reports a frame that never happened.
    if (!reported && elapsed > 7.42) {
      reported = true;
      const Composer::Stats &s = ctx.composer.stats();
      std::fprintf(stderr,
                   "[black watch] instances %zu  pictures %zu  recorded %zu  "
                   "painted-live %zu  layout %.2f ms  volatile %.2f ms  "
                   "paint %.2f ms\n",
                   s.instances, s.picturesLive, s.picturesRecorded,
                   s.nodesPainted, s.layoutMs, s.volatileMs, s.paintMs);
    }
  }
};

SIGIL_SKETCH(BlackWatch)
