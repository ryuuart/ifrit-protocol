#pragma once

/** @file
 * THE CLOTH AS NUMBERS: a sett read from the register's own notation, the
 * shade cards it is woven in, the loom that interlaces it, and the
 * invariants the card prints — every one computed from the threads, none
 * asserted.
 */

#include <include/core/SkPath.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/testing/Checks.h>
#include <sigildata/decode/Json.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Weave.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace material = sigil::material;

// ---------------------------------------------------------------------------
// The shades. Codes are the register's: K black, B blue, G green, Y yellow,
// W white (the warm one, undyed wool). A thread is an index into whichever
// shade card is mounted, because a threadcount is the cloth's identity and
// its colour is a variable: the register permits nineteen greens, sixteen
// blues and two blacks, and that is the whole mechanism behind the named
// palettes.

constexpr std::string_view kShadeCodes = "KBGYW";
enum Shade : uint8_t { K = 0, B = 1, G = 2, Y = 3, W = 4 };
constexpr int kShadeCount = 5;

using Shades = std::array<sigil::material::Color, kShadeCount>;

/** ONE SHADE CARD: its register name, and the colour each code is dyed in.
 *  Only black, blue and green vary between cards; the two overchecks are
 *  one yellow and one white on every card. */
struct ShadeCard {
  std::string name;
  Shades shades;
};

inline std::vector<ShadeCard> readShadeCards(const sigil::data::Json& file) {
  std::vector<ShadeCard> cards;
  for (const sigil::data::Json& card : file["cards"].array()) {
    ShadeCard read{.name = std::string(card["name"].string())};
    for (int shade = 0; shade < kShadeCount; ++shade) {
      const std::string code(1, kShadeCodes[(size_t)shade]);
      const sigil::data::Json& own = card[code];
      read.shades[(size_t)shade] = material::parseColor(
          own.null() ? file["overchecks"][code].string() : own.string());
    }
    cards.push_back(std::move(read));
  }
  return cards;
}

// ---------------------------------------------------------------------------
// The sett, as its source prints it: named units of runs in the register's
// notation — "K18 B6" is eighteen black threads, then six blue — and the
// order the units repeat in. The whole repeat is printed, never the half
// between the pivots: read one as the other and the cloth comes out twice
// or half the right size and still looks right, which is what the first
// invariant is for.

using Run = material::pattern::ThreadRun;

inline std::vector<Run> readRuns(std::string_view notation) {
  std::vector<Run> runs;
  for (size_t at = 0; at < notation.size();) {
    const size_t end = std::min(notation.find(' ', at), notation.size());
    if (end > at + 1)
      runs.push_back(
          {std::stoi(std::string(notation.substr(at + 1, end - at - 1))),
           (int)kShadeCodes.find(notation[at])});
    at = end + 1;
  }
  return runs;
}

struct Sett {
  /** The unit names in the order the repeat runs through them. */
  std::vector<std::string> order;
  /** Each unit's runs, in that order. */
  std::vector<std::vector<Run>> units;
  /** The total the source itself prints, which the expansion must meet. */
  int publishedEnds = 0;

  std::vector<Run> runs() const {
    std::vector<Run> all;
    for (const std::vector<Run>& unit : units)
      all.insert(all.end(), unit.begin(), unit.end());
    return all;
  }
  std::vector<uint8_t> threads() const {
    return material::pattern::threadcount(runs());
  }
  std::vector<int> unitEnds() const {
    std::vector<int> ends;
    for (const std::vector<Run>& unit : units)
      ends.push_back(std::accumulate(
          unit.begin(), unit.end(), 0,
          [](int sum, const Run& run) { return sum + run.threads; }));
    return ends;
  }
};

inline Sett readSett(const sigil::data::Json& entry) {
  Sett sett{.publishedEnds = (int)entry["published ends"].number()};
  for (const sigil::data::Json& name : entry["order"].array()) {
    sett.order.emplace_back(name.string());
    sett.units.push_back(readRuns(entry["units"][name.string()].string()));
  }
  return sett;
}

// ---------------------------------------------------------------------------
// THE LOOM — the stored program, three lines of it:
//
//   threading[j] = j mod 4                 straight draw
//   treadling[i] = i mod 4                 as drawn in ("tromp as writ")
//   tieup[t]     = { t, (t+1) mod 4 }      each treadle lifts two shafts
//
// Warp end j is lifted on pick i when threading[j] is in tieup[treadling[i]]:
// two over, two under, stepping one end per pick — the 2/2 twill, with the
// rib on the "\" diagonal the register's swatch shows.

constexpr material::pattern::Weave kTwill =
    material::pattern::Weave::twill(2, 2);

/** The whole artefact as the weave generator reads it: ONE threadcount for
 *  both directions, because the register says the weft sequence is the
 *  warp sequence, over @p shades, with @p rib darkening the crossings
 *  where the weft is on top so the twill reads inside a block of one
 *  colour. */
inline material::pattern::Cloth cloth(const std::vector<uint8_t>& threads,
                                      const Shades& shades, float rib) {
  return {.warp = threads,
          .weft = threads,
          .shades = {shades.begin(), shades.end()},
          .weave = kTwill,
          .rib = rib};
}

// ---------------------------------------------------------------------------
// THE INVARIANTS. The cheap ones do not catch the commonest bug: build the
// bands as inclusive ranges and the sum still closes, the sequence is still
// palindromic and the cloth still looks right. Only point-sampled coverage
// sees it. A weave has no chained contour to close, so its integrity is
// arithmetic and areal, never topological.

struct Verdict {
  std::vector<int> unitEnds;
  int total = 0;
  std::vector<int> mirrors;
  int mirrorGap = 0;
  int maxWarpFloat = 0, maxWeftFloat = 0;
  std::array<int, kShadeCount> counts{};
  int solids = 0, blends = 0;
  int samples = 0, uncovered = 0, doubled = 0;
  int argyllTotal = 0, argyllSolids = 0, argyllBlends = 0;
  /** The largest difference between the two setts' unit fractions, as a
   *  percentage rounded to the two places the card prints. */
  double unitDrift = 0;
};

/** Solid and blend colour counts over the whole sett square. The 2/2 twill
 *  is balanced, so where warp a crosses weft b the eye sees the UNORDERED
 *  pair — which is where n(n+1)/2 comes from. */
inline void perceivedColours(const std::vector<uint8_t>& threads, int& solids,
                             int& blends) {
  bool seen[kShadeCount][kShadeCount] = {};
  for (uint8_t warp : threads)
    for (uint8_t weft : threads)
      seen[std::min(warp, weft)][std::max(warp, weft)] = true;
  solids = blends = 0;
  for (int low = 0; low < kShadeCount; ++low)
    for (int high = low; high < kShadeCount; ++high)
      if (seen[low][high]) (low == high ? solids : blends)++;
}

inline Verdict verify(const Sett& watch, const Sett& argyll) {
  Verdict verdict;
  const std::vector<uint8_t> threads = watch.threads();
  const int ends = (int)threads.size();
  verdict.unitEnds = watch.unitEnds();
  verdict.total = ends;

  // Reflective: exactly two mirror boundaries, exactly half apart.
  verdict.mirrors = material::pattern::pivots(threads);
  if (verdict.mirrors.size() == 2)
    verdict.mirrorGap = verdict.mirrors[1] - verdict.mirrors[0];

  // 2/2 balance: nothing anywhere floats longer than two, down a column or
  // across a row, read over two repeats so a float across the seam counts.
  for (int line = 0; line < ends; ++line) {
    int warpRun = 0, weftRun = 0;
    for (int along = 0; along < 2 * ends; ++along) {
      warpRun =
          material::pattern::warpUp(kTwill, line, along) ? warpRun + 1 : 0;
      weftRun =
          material::pattern::warpUp(kTwill, along, line) ? 0 : weftRun + 1;
      verdict.maxWarpFloat = std::max(verdict.maxWarpFloat, warpRun);
      verdict.maxWeftFloat = std::max(verdict.maxWeftFloat, weftRun);
    }
  }

  for (uint8_t shade : threads) verdict.counts[shade]++;
  perceivedColours(threads, verdict.solids, verdict.blends);

  // Exact cover: the warp bands crossed with the weft bands must tile the
  // sett square with no gap and no overlap — the one check here that sees
  // a band overlapping its neighbour by a thread.
  std::vector<float> starts;
  float cursor = 0;
  const std::vector<Run> runs = watch.runs();
  for (const Run& run : runs) {
    starts.push_back(cursor);
    cursor += (float)run.threads;
  }
  std::vector<SkPath> cells;
  for (size_t across = 0; across < runs.size(); ++across)
    for (size_t down = 0; down < runs.size(); ++down)
      cells.push_back(SkPath::Rect(SkRect::MakeXYWH(
          starts[across], starts[down], (float)runs[across].threads,
          (float)runs[down].threads)));
  const sigil::compose::test::Coverage cover = sigil::compose::test::coverage(
      cells, SkRect::MakeWH((float)ends, (float)ends), 256);
  verdict.samples = cover.samples;
  verdict.uncovered = cover.uncovered;
  verdict.doubled = cover.doubled;

  // Campbell of Argyll: the same law over five colours, and no exact
  // mirror, since its two overchecks are different colours.
  const std::vector<uint8_t> argyllThreads = argyll.threads();
  verdict.argyllTotal = (int)argyllThreads.size();
  perceivedColours(argyllThreads, verdict.argyllSolids, verdict.argyllBlends);

  // The unit proportions of the two setts, unit for unit: the whole
  // argument of the comparison strip.
  const std::vector<int> argyllEnds = argyll.unitEnds();
  double drift = 0;
  for (size_t unit = 0;
       unit < std::min(verdict.unitEnds.size(), argyllEnds.size()); ++unit)
    drift = std::max(drift,
                     std::abs((double)verdict.unitEnds[unit] / ends -
                              (double)argyllEnds[unit] / verdict.argyllTotal));
  verdict.unitDrift = std::round(drift * 10000.0) / 100.0;
  return verdict;
}

}  // namespace
