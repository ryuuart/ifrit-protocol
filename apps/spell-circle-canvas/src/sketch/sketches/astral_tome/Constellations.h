#pragma once

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/core/Core.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::material::skia::Paint;
namespace ch = choreograph;

namespace at {

// ---------------------------------------------------------------------------
// THE GRID. One scale for the whole page: 3 canvas px per GUI px.

constexpr float kU = 3.0f;  ///< canvas px per GUI px
/** THE ARTEFACT'S OWN HEIGHT, and the band under it. A plate of a page
 *  is a plate of the page; what the study proves stands in a band
 *  declared OUTSIDE it, so the tome's own geometry is derived from
 *  `kArtH` and never from the canvas the band grew. */
constexpr float kArtH = 750.0f;
constexpr float kBandH = 78.0f;
constexpr float kCanvasW = 1200.0f, kCanvasH = kArtH + kBandH;
constexpr float kGuiW = 420.0f, kGuiH = 270.0f;  // GuiScreenJournal(270, 420)
constexpr float kGuiLeft = (kCanvasW - kGuiW * kU) * 0.5f;  // -30
constexpr float kGuiTop = (kArtH - kGuiH * kU) * 0.5f;      // -30

inline float g(float gui) { return gui * kU; }
inline float gx(float gui) { return kGuiLeft + gui * kU; }
inline float gy(float gui) { return kGuiTop + gui * kU; }

constexpr int kGrid = 31;                           // IConstellation:32
constexpr float kRenderBox = 95.0f;                 // Cluster:240 — SQUARE
constexpr float kCellW = 80.0f, kCellH = 110.0f;    // Cluster:59 — the HIT box
constexpr float kUlen = kRenderBox / (float)kGrid;  // 3.0645 GUI px
constexpr float kLineBreadth = 2.0f;                // Cluster:240

using sigil::compose::hexColor;  // 0xRRGGBB -> SkColor4f

// Palette, sampled out of the mod's own PNGs (see the header).
const SkColor4f kLeatherDark =
    hexColor(0x0A0800);  // guijspacebook, darkest bulk
const SkColor4f kLeatherMid =
    hexColor(0x2C1602);  // its commonest opaque colour
const SkColor4f kLeatherWarm = hexColor(0x634913);
const SkColor4f kGilt = hexColor(0x9B7A2D);   // its brightest
const SkColor4f kOlive = hexColor(0x7D6C00);  // guijarrow
const SkColor4f kOliveDim = hexColor(0x574E25);
const SkColor4f kNebula =
    hexColor(0x0B080B);  // guiresbgcst mean * (.8,.8,1)*.7
const SkColor4f kFieldStar = hexColor(0x8F8FB3);  // its white points, same tint
const SkColor4f kInk = hexColor(0xDDDDDD);        // Cluster:253 text 0xBBDDDDDD
constexpr float kInkAlpha = 0xBB / 255.0f;

// ---------------------------------------------------------------------------
// java.util.Random, exactly — the divisor sequence has to be byte-identical to
// the mod's or the twinkle is a different pattern that merely looks similar.
// 48-bit LCG, multiplier 0x5DEECE66D, addend 0xB; nextInt(bound) for a
// non-power-of-two bound is `next(31) % bound` with the overflow rejection.

struct JavaRand {
  uint64_t s;
  explicit JavaRand(uint64_t seed)
      : s((seed ^ 0x5DEECE66DULL) & ((1ULL << 48u) - 1)) {}
  int32_t next(int bits) {
    s = (s * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48u) - 1);
    return (int32_t)(s >> (48u - (unsigned)bits));
  }
  int nextInt(int bound) {
    int32_t r = next(31);
    const int32_t m = bound - 1;
    if (((uint32_t)bound & (uint32_t)m) == 0)
      return (int)(((uint64_t)bound * (uint64_t)r) >> 31u);
    for (int32_t u = r; u - (r = u % bound) + m < 0; u = next(31));
    return (int)r;
  }
};

/** The list `new Random(0x4196A15C91A5E199L)` produces, restarted per
 *  constellation exactly as Cluster:228 does. */
inline std::vector<int> divisorSequence(int n) {
  JavaRand r(0x4196A15C91A5E199ULL);
  std::vector<int> out;
  out.reserve((size_t)n);
  for (int i = 0; i < n; ++i) out.push_back(12 + r.nextInt(10));
  return out;
}

constexpr int kDivMin = 12, kDivCount = 10;  // 12 + nextInt(10) -> [12, 21]

// ---------------------------------------------------------------------------
// THE DATA. RegistryConstellations:296-368, verbatim, in registration order.

struct Con {
  const char* name;
  uint32_t color;
  int starCount;
  std::array<std::pair<int, int>, 9> stars;
  int linkCount;
  std::array<std::pair<int, int>, 10> links;  // 1-based, as addConnection reads
};

const std::array<Con, 4> kPage0 = {{
    {"DISCIDIA",
     0xE01903,
     8,
     {{{7, 2},
       {3, 6},
       {5, 12},
       {20, 11},
       {15, 17},
       {26, 21},
       {23, 27},
       {15, 25},
       {0, 0}}},
     7,
     {{{1, 2},
       {2, 3},
       {2, 4},
       {4, 5},
       {5, 7},
       {6, 7},
       {7, 8},
       {0, 0},
       {0, 0},
       {0, 0}}}},
    {"ARMARA",
     0xB7BBB8,
     7,
     {{{8, 4},
       {9, 15},
       {11, 26},
       {19, 25},
       {23, 14},
       {23, 4},
       {15, 7},
       {0, 0},
       {0, 0}}},
     10,
     {{{1, 2},
       {2, 3},
       {3, 4},
       {4, 5},
       {5, 6},
       {6, 7},
       {7, 1},
       {2, 5},
       {2, 7},
       {5, 7}}}},
    {"VICIO",
     0x00BDAD,
     7,
     {{{3, 8},
       {13, 9},
       {6, 23},
       {14, 16},
       {23, 24},
       {22, 16},
       {24, 4},
       {0, 0},
       {0, 0}}},
     6,
     {{{1, 2},
       {2, 7},
       {3, 4},
       {4, 7},
       {5, 6},
       {6, 7},
       {0, 0},
       {0, 0},
       {0, 0},
       {0, 0}}}},
    {"AEVITAS",
     0x2EE400,
     9,
     {{{15, 14},
       {7, 12},
       {3, 6},
       {21, 8},
       {25, 2},
       {13, 21},
       {9, 26},
       {17, 28},
       {27, 17}}},
     8,
     {{{1, 2},
       {2, 3},
       {1, 4},
       {4, 5},
       {1, 6},
       {6, 7},
       {6, 8},
       {4, 9},
       {0, 0},
       {0, 0}}}},
}};

/** Cluster:288 — the hand-placed zig-zag. High, low, high, low; 80 px pitch,
 *  a 50/60 px swing. Authored, not computed; do not tidy it into a grid. */
const std::array<SkPoint, 4> kOffsets = {
    {{45, 55}, {125, 105}, {200, 45}, {280, 110}}};

/** The remaining twelve, for the tier rail. RegistryConstellations:370-553. */
struct Tier {
  const char* name;
  uint32_t color;
  const char* band;
};
const std::array<Tier, 12> kRest = {{
    {"EVORSIO", 0xA00100, "BRIGHT"},
    {"LUCERNA", 0xFFE709, "DIM"},
    {"MINERALIS", 0xCB7D0A, "DIM"},
    {"HOROLOGIUM", 0x7D16B4, "DIM"},
    {"OCTANS", 0x706EFF, "DIM"},
    {"BOOTES", 0xD41CD6, "DIM"},
    {"FORNAX", 0xFF4E1B, "DIM"},
    {"PELOTRIO", 0xEC006B, "DIM"},
    {"GELU", 0x758BA8, "FAINT"},
    {"ULTERIA", 0x347463, "FAINT"},
    {"ALCARA", 0x802952, "FAINT"},
    {"VORUX", 0xA8881E, "FAINT"},
}};

/** Star centre in the chart's LOCAL canvas space. Render:300 offsets the quad
 *  by -1 grid unit and spans 2 units, so the quad is centred on the grid point
 *  and the CENTRE is simply (x*u, y*u). */
inline SkPoint starAt(const Con& c, int index1) {
  const auto& s = c.stars[(size_t)(index1 - 1)];
  return {g(kUlen * (float)s.first), g(kUlen * (float)s.second)};
}

inline int degreeOf(const Con& c, int index1) {
  int d = 0;
  for (int i = 0; i < c.linkCount; ++i)
    if (c.links[(size_t)i].first == index1 ||
        c.links[(size_t)i].second == index1)
      ++d;
  return d;
}

/** THE LINK'S WIDTH LAW, as a comparable Profile: full in the middle, 40%
 *  at both endpoints, so a link reads as drawn FROM star TO star. It is
 *  not `profile::taper`, which ramps LINEARLY from one stated width to
 *  another: this one is symmetric about the midpoint and square-rooted,
 *  which is the shoulder an engraved rule has and a ramp does not.
 *
 *  Fraction-keyed, and correctly so — the link's ribbons sit under an
 *  UNQUALIFIED stroke with no reveal on the node, so `along` is a fraction
 *  of the whole spine every frame and stays put. (The px key exists for
 *  the laws that do sit under a reveal; this one does not need it.)
 *
 *  `max()` is `peak` exactly: sin(pi/2) = 1 at the midpoint, so the law
 *  tops out at 0.40 + 0.60 = 1.0 of it.
 *
 *  THE max(k, 0) IS LOAD-BEARING, not defensive padding. `3.14159265f`
 *  rounds UP to 3.1415927, so `sin(pi * 1.0f)` is -8.74e-08 — NEGATIVE —
 *  and `sqrt` of that is NaN. Every construction samples the law at exactly
 *  along = 1 (the zip walk's last sample is at `d == len`; the rail walk's
 *  is at `k == steps`), so without the clamp one NaN vertex enters the band
 *  path, and a path with a non-finite point does not draw AT ALL. The
 *  failure mode is silent: the bloom and the body vanish entirely and only
 *  the rails remain. */
struct LinkTaper {
  float peak = 1.0f;
  float across(float along) const {
    const float k = std::sin(3.14159265f * std::clamp(along, 0.0f, 1.0f));
    return peak * (0.40f + 0.60f * std::sqrt(std::max(k, 0.0f)));
  }
  float max() const { return peak; }
  bool operator==(const LinkTaper&) const = default;
};

}  // namespace at

// =============================================================================
