// penrose_paving.cpp — THE PENROSE PAVING (Andrew Wiles Building, Oxford)
// =============================================================================
// A plan-view reconstruction of the paving Sir Roger Penrose designed in 2012
// for the forecourt of the Andrew Wiles Building, home of the Mathematical
// Institute, University of Oxford (opened October 2013): several thousand
// diamond granite setts of exactly TWO shapes — a fat rhomb (72°/108°) and a
// thin rhomb (36°/144°) — cut from Royal White and Kobra grey granite, each
// inlaid with polished stainless-steel arcs that chain across every shared
// edge into large interlocking rings, so the tiling's matching rule is legible
// underfoot.
//
// REFERENCES
//  - maths.ox.ac.uk/about-us/life-oxford-mathematics/oxford-mathematics-
//    alphabet/aperiodic-tiles — the Institute's own account: a pair of
//    RHOMBUSES (P3, not kite/dart P2); Penrose "added curved metal bands that
//    link up to produce an overarching pattern" for the 2012 redesign.
//  - hardscape.co.uk/maths-institute-oxford — the paving contractor's spec:
//    Royal White and Kobra grey Artscape granite, polished 30 mm stainless
//    steel inserts, "several thousand" diamond tiles of two shapes, "a
//    never-repeating pattern."
//  - commons.wikimedia.org/wiki/File:Penrose_tiling_at_Oxford_Mathematical_
//    Institute.jpg — Pierre Marshall (Extua), CC0, 4608×3456. The artefact
//    photograph, read directly: wide LIGHT bands (~10% of a tile edge) over
//    mid-grey speckled granite, saw-cut joints as hairline dark lines, the
//    arcs chaining into complete circles at some vertices and wandering loops
//    elsewhere. That photo is what the surface treatment below is tuned to.
//  - arXiv:cond-mat/9903010, Grimm & Schreiber, "Aperiodic Tilings on the
//    Computer" §6 — de Bruijn's dualization method stated precisely: an n-fold
//    grid of equidistant parallel lines rotated 2πk/n and shifted by γ_k, with
//    Σγ_k ≡ Γ (mod 1) fixing the local-isomorphism class; Γ=0 is the genuine
//    Penrose class, Γ=1/2 the "anti-Penrose" tiling with illegal vertex stars.
//  - de Bruijn (1981), "Algebraic theory of Penrose's non-periodic tilings" —
//    the vertex formula K_j(x) = ⌈ζ_j·x + γ_j⌉ and the four-corner sum.
//
// THE CONSTRUCTION — nothing here is authored. Every tile's position, its
// rotation, its prototile, its granite, its seed and its two inlay arcs are a
// closed-form function of FOUR INTEGERS (r, s, k_r, k_s):
//
//   ζ_j = (cos 72°j, sin 72°j),  γ_j = 1/5 for all j   (Σγ = 1 ≡ 0 mod 1)
//
//   For an ordered family pair (r,s) and integers (k_r,k_s) the two grid lines
//   ζ_r·x = k_r − γ_r and ζ_s·x = k_s − γ_s cross at one point x*. Then
//
//       K_j = ⌈ζ_j·x* + γ_j⌉        (K_r = k_r, K_s = k_s exactly)
//       z   = Σ_j K_j ζ_j
//       corners = z, z+ζ_r, z+ζ_r+ζ_s, z+ζ_s
//
//   d = min(|r−s|, 5−|r−s|); d=1 → fat rhomb, d=2 → thin rhomb. The tile's
//   world orientation is ζ_r and ζ_s themselves — never a stored angle — so
//   the arc decoration is built in the tile's own edge frame and is correct by
//   construction rather than by eyeballing.
//
//   CHANGE kOffset ALONE and you get a DIFFERENT, still-valid Penrose tiling:
//   0.2 is the Γ=0, exactly 5-fold-symmetric member (the composition centres
//   on its 5-fold point); 0.1 is Γ=1/2, the anti-Penrose tiling — its illegal
//   vertex stars show up immediately in the arc chains.
//
//   THE ARCS are not free either, and they are NOT "at the acute corners".
//   Every edge of the whole tiling runs tail → tail + ζ_j for a fixed j, a
//   globally consistent orientation the dualization hands you: the tile's
//   corners come out of Σ_j K_j ζ_j, so no edge is free to point the other
//   way. Mark each edge at a·s from its TAIL and both marks
//   around corner z sit at a·s while both around z+ζ_r+ζ_s sit at (1−a)·s;
//   the other two corners see one of each and admit no circular arc at all.
//   So the two arcs live on the ζ_r+ζ_s DIAGONAL — the acute pair on a fat
//   rhomb, the OBTUSE pair on a thin one — and they are de Bruijn's two
//   arrow classes. a = 1/2 is forced too: an arc at a thin rhomb's 144°
//   corner must clear the far edge at s·sin36° = 0.588s, so a ∈ [0.412,
//   0.588], and 1/2 is the only value making the two classes congruent.
//   The chain is then C0 across every shared edge whatever meets there (both
//   endpoints are the edge's exact midpoint) and C1 as well (an arc centred
//   on an endpoint of the edge crosses that edge perpendicularly, from both
//   sides). Proof, not assertion — verify() checks the angle sums, the arc
//   chain and the endpoint radii numerically at startup and prints the
//   result. Getting the corner pair wrong is not a subtle error: it changes
//   the loop topology from the photograph's interlocking rings to a field of
//   isolated little circles.
//
//   The deflation vignette runs the OTHER construction — Robinson-triangle
//   substitution, acute → 1 acute + 1 obtuse, obtuse → 1 acute + 2 obtuse,
//   ×1/φ — i.e. rhomb-level Fat → 2 Fat + 1 Thin, Thin → 1 Fat + 1 Thin, so
//   1 → 3 → 8 → 21 rhombs, checked three ways per generation: area
//   conservation, every child inside a parent, and a point-sampled coverage
//   test counting uncovered and double-covered samples — the first two both
//   pass on a subdivision that overlaps here and gaps there, so only the
//   third is actually a proof of tiling.
//
// IT IS STONE, NOT A DIAGRAM: every sett is a slab with a real saw-cut joint,
// a chamfered arris shaded against one world-fixed sun, a seeded granite
// material (two recipes — Royal White's larger sparse feather-vein, Kobra
// grey's finer denser speckle), and the inlay is a banded stone/metal section
// with a groove shadow and a specular ridge, drawn with a radial ramp centred
// on the arc's own centre so the gradient runs ACROSS the band width exactly.
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/penrose_paving.cpp \
//       --frame /tmp/penrose_paving.png
//
// --at 0.75 catches the crystal front mid-growth over the exact 5-fold
// centre, 1.45 the inlay chaining on, 4.6 the finished plaza.
// =============================================================================

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace matkit = sigil::material::kit;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace skia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;
namespace motion = sigil::motion;
using namespace sigil::motion;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Palette — representative matches for the NAMED granite products, not a
// colorimeter reading of the installed slabs. Royal White is pulled a
// little off its showroom "snow white" because these are weathered outdoor
// setts under an overcast sky, and because a φ²-weighted majority of the field
// is fat rhombs: at the catalogue value the plaza blows out to paper.

const SkColor4f kWhiteBase = hex(0xBFBCB2);  // Royal White, weathered
const SkColor4f kWhiteLit = hex(0xD4D0C6);   // Royal White, sun side
const SkColor4f kWhiteVein = hex(0x2B2A28);  // black feather-vein inclusions
const SkColor4f kGreyBase = hex(0x82858A);   // Kobra grey
const SkColor4f kGreyLit = hex(0x969A9E);    // Kobra grey, sun side
const SkColor4f kGreyVein = hex(0x3E4042);   // Kobra's tighter speckle

const SkColor4f kSteelBase = hex(0xEEF1F2);      // polished stainless, overcast
const SkColor4f kSteelSpec = hex(0xFEFEFE);      // direct catch-light
const SkColor4f kSteelEdge = hex(0xC9CED1);      // the insert's chamfered lip
const SkColor4f kGroove = hex(0x5A5F63, 0.38f);  // occlusion in the milled slot

const SkColor4f kJointBed = hex(0x33363A);  // saw-cut joint / bedding mortar
const SkColor4f kNight = hex(0x101112);
const SkColor4f kCaption = hex(0x9CA0A2);

// ---------------------------------------------------------------------------
// Composition. The artefact is a PLAZA, so the paving runs full bleed: the
// field is terminated by the canvas edge and a daylight falloff rather than by
// a frame. An aperiodic field has no periodic register to frame into — any
// border would cut tiles at an arbitrary place and imply a repeat that is not
// there. The pentagrid origin sits at canvas centre, which puts the γ=1/5
// construction's exact 5-fold point at the centre of the composition.

constexpr float kW = 1600, kH = 1200;
constexpr float kCx = kW * 0.5f, kCy = kH * 0.5f;
constexpr float kModule = 78.0f;  // <<< the rhomb side, px. THE free constant.
constexpr float kJoint = 1.5f;    // saw-cut joint, total width px
constexpr float kChamfer = 2.3f;  // arris chamfer band, px
constexpr float kBandW = 9.8f;    // steel inlay band, px (≈0.125·s, per photo)
constexpr float kCorner = 1000.0f;  // canvas half-diagonal — the ripple's reach
constexpr float kDiagW = 328.0f, kDiagH = 224.0f;  // the vignette's drawing box

// The sun. One world-fixed direction for every chamfer on every sett.
const SkVector kSunTo{0.48f, 0.877f};  // the light's direction of travel

// ---------------------------------------------------------------------------
// Timeline. Every delay is a continuous function of a tile's own centre
// distance from the pentagrid origin, so the radial stagger falls out of the
// geometry and no tile carries a schedule of its own.

constexpr double kPeriod = 9.2;
constexpr double kTileT0 = 0.05, kTileSweep = 1.35, kTileDur = 0.52;
constexpr double kArcT0 = 1.20, kArcSweep = 1.05, kArcDur = 0.58;
constexpr double kSheen0 = 2.55, kSheenDur = 1.10;
const double kGenAt[4] = {0.70, 1.55, 2.45, 3.35};

inline float clamp01(double v) { return (float)std::clamp(v, 0.0, 1.0); }
// ---------------------------------------------------------------------------
// de Bruijn's pentagrid

struct V2 {
  double x = 0, y = 0;
};
inline V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.y + b.y}; }
inline V2 operator*(V2 a, double k) { return {a.x * k, a.y * k}; }
inline double dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }

const std::array<V2, 5>& zeta() {
  static const std::array<V2, 5> z = [] {
    std::array<V2, 5> out{};
    for (int j = 0; j < 5; ++j) {
      const double a = 2.0 * 3.14159265358979323846 * (double)j / 5.0;
      out[(size_t)j] = {std::cos(a), std::sin(a)};
    }
    return out;
  }();
  return z;
}

// γ_j — the SAME offset in every family, so Σγ = 1 ≡ 0 (mod 1) (de Bruijn's
// Γ = 0, the genuine Penrose class) AND the whole five-line system is
// invariant under the 72° rotation that cyclically permutes the families, so
// the tiling carries an exact 5-fold rotation about the origin.
constexpr double kOffset = 0.2;

struct Tile {
  int r = 0, s = 0, kr = 0, ks = 0;
  bool fat = true;
  SkPoint v[4]{};      // world px; v[0] = z, the canonical low corner
  SkPoint centre{};    // world px
  float radius = 0;    // px from the pentagrid origin
  int arcAt[2]{0, 2};  // v[] indices carrying the two matching-rule arcs
  uint32_t seed = 0;
};

inline uint32_t hash4(int a, int b, int c, int d) {
  uint32_t h = 2166136261u;
  auto mix = [&h](uint32_t v) {
    h ^= v;
    h *= 16777619u;
    h ^= h >> 13u;
  };
  mix((uint32_t)(a + 97));
  mix((uint32_t)(b + 193));
  mix((uint32_t)(c + 4099));
  mix((uint32_t)(d + 8191));
  return h;
}

std::vector<Tile> buildField(float module, float padPx) {
  std::vector<Tile> out;
  // z ≈ (5/2)·x* (because Σ_j ζ_jζ_jᵀ = (5/2)I for five evenly spaced unit
  // vectors), so the index range needed to reach radius R in px is
  // |k| ≲ R/(2.5·module) + 1. Take three extra rings of margin and let the
  // clip below discard the rest — no authored index table.
  const float reach = std::hypot(kW * 0.5f, kH * 0.5f) + padPx;
  const int K = (int)std::ceil(reach / (module * 2.5f)) + 3;
  const SkRect keep = SkRect::MakeWH(kW, kH).makeOutset(padPx, padPx);

  for (int r = 0; r < 5; ++r) {
    for (int s = r + 1; s < 5; ++s) {
      const V2 zr = zeta()[(size_t)r], zs = zeta()[(size_t)s];
      const double det = zr.x * zs.y - zr.y * zs.x;
      if (std::abs(det) < 1e-9)
        continue;  // never happens: five distinct 72° directions
      const int dd = std::min(std::abs(r - s), 5 - std::abs(r - s));
      const bool fat = (dd == 1);

      for (int kr = -K; kr <= K; ++kr) {
        for (int ks = -K; ks <= K; ++ks) {
          const double a = (double)kr - kOffset;
          const double b = (double)ks - kOffset;
          // The one intersection point of grid line k_r (family r) and k_s
          // (family s).
          const V2 x{(a * zs.y - b * zr.y) / det, (zr.x * b - zs.x * a) / det};

          V2 z{0, 0};
          for (int j = 0; j < 5; ++j) {
            const int Kj =
                (j == r) ? kr
                : (j == s)
                    ? ks
                    : (int)std::ceil(dot(zeta()[(size_t)j], x) + kOffset);
            z = z + zeta()[(size_t)j] * (double)Kj;
          }

          Tile t;
          t.r = r;
          t.s = s;
          t.kr = kr;
          t.ks = ks;
          t.fat = fat;
          const V2 c[4] = {z, z + zr, z + zr + zs, z + zs};
          double sx = 0, sy = 0;
          for (int i = 0; i < 4; ++i) {
            t.v[i] = {kCx + (float)(c[i].x * module),
                      kCy + (float)(c[i].y * module)};
            sx += c[i].x;
            sy += c[i].y;
          }
          t.centre = {kCx + (float)(sx * 0.25 * module),
                      kCy + (float)(sy * 0.25 * module)};
          t.radius = std::hypot(t.centre.x() - kCx, t.centre.y() - kCy);

          // The arcs always sit on the ζ_r+ζ_s DIAGONAL — v[0] (= z, the
          // canonical low corner) and v[2] — never on "the acute pair". That
          // is forced, not chosen: in the dualization every edge of the whole
          // tiling runs tail → tail + ζ_j for a fixed j, so marking each edge
          // at a·s from its tail puts BOTH marks around v[0] at a·s and both
          // around v[2] at (1−a)·s, while the other two corners see one of
          // each and admit no circular arc at all. The two arc classes are
          // exactly de Bruijn's single/double arrow. (a = 1/2 here is also
          // forced: an arc at the thin rhomb's 144° corner must clear the
          // far edge at s·sin36° = 0.588s, so a ∈ [0.412, 0.588], and 1/2 is
          // the only value that makes the two classes congruent — which is
          // what makes the chain C1 as well as C0.)
          t.arcAt[0] = 0;
          t.arcAt[1] = 2;
          t.seed = hash4(r, s, kr, ks);

          SkRect bb = SkRect::MakeEmpty();
          bb.setBounds({t.v, 4});
          if (SkRect::Intersects(bb, keep)) out.push_back(t);
        }
      }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// The arc decoration, built in the tile's OWN edge frame

struct ArcSpec {
  SkPoint centre{};  // the arc-carrying vertex (v[0] or v[2])
  float startDeg = 0, sweepDeg = 0;
  SkPoint end0{}, end1{};  // the two edge midpoints it lands on
};

inline float wrap180(float d) {
  while (d > 180.0f) d -= 360.0f;
  while (d <= -180.0f) d += 360.0f;
  return d;
}

ArcSpec arcAt(const Tile& t, int k, float module) {
  const int ai = t.arcAt[k];
  const SkPoint A = t.v[ai];
  const SkPoint N1 = t.v[(ai + 1) % 4];
  const SkPoint N2 = t.v[(ai + 3) % 4];
  const float R = module * 0.5f;
  const float a0 = std::atan2(N1.y() - A.y(), N1.x() - A.x()) * 57.29578f;
  const float a1 = std::atan2(N2.y() - A.y(), N2.x() - A.x()) * 57.29578f;
  ArcSpec spec;
  spec.centre = A;
  spec.startDeg = a0;
  spec.sweepDeg = wrap180(a1 - a0);
  spec.end0 = {A.x() + (N1.x() - A.x()) * 0.5f,
               A.y() + (N1.y() - A.y()) * 0.5f};
  spec.end1 = {A.x() + (N2.x() - A.x()) * 0.5f,
               A.y() + (N2.y() - A.y()) * 0.5f};
  (void)R;
  return spec;
}

SkPath arcPath(const ArcSpec& a, float module, float shortenPx) {
  const float R = module * 0.5f;
  const float trim = shortenPx / R * 57.29578f;
  const float sgn = a.sweepDeg >= 0 ? 1.0f : -1.0f;
  const SkRect oval = SkRect::MakeLTRB(a.centre.x() - R, a.centre.y() - R,
                                       a.centre.x() + R, a.centre.y() + R);
  SkPathBuilder b;
  b.arcTo(oval, a.startDeg + sgn * trim, a.sweepDeg - sgn * 2 * trim, true);
  return b.detach();
}

// ---------------------------------------------------------------------------
// Verification: the tiling must obey its own rules before any of the surface
// treatment is worth looking at. A dualization that is subtly wrong still
// renders a plausible field of rhombs, so the checks below are numeric and
// run at startup rather than being left to the eye.

struct Audit {
  int tiles = 0, fat = 0, thin = 0;
  double ratio = 0;
  int interiorVerts = 0, badVerts = 0;
  double worstVertErr = 0;
  int arcNodes = 0, chained = 0, danglingInterior = 0;
  double worstMidErr = 0, worstTangentErr = 0;
};

Audit verify(const std::vector<Tile>& tiles, float module) {
  Audit a;
  a.tiles = (int)tiles.size();

  auto qkey = [](SkPoint p) {
    return (int64_t)(((uint64_t)std::llround(p.x() * 8.0) << 24u) ^
                     (uint64_t)std::llround(p.y() * 8.0));
  };

  // 1. Angle sums. Every interior vertex of a genuine tiling closes at 360°;
  //    a gap or an overlap in the dualization shows up here first.
  boost::unordered_flat_map<int64_t, std::pair<double, SkPoint>> vsum;
  for (const Tile& t : tiles) {
    (t.fat ? a.fat : a.thin)++;
    for (int i = 0; i < 4; ++i) {
      const SkPoint p = t.v[i], q = t.v[(i + 1) % 4], r = t.v[(i + 3) % 4];
      const double d1x = q.x() - p.x(), d1y = q.y() - p.y();
      const double d2x = r.x() - p.x(), d2y = r.y() - p.y();
      const double ang =
          std::acos(std::clamp((d1x * d2x + d1y * d2y) / (std::hypot(d1x, d1y) *
                                                          std::hypot(d2x, d2y)),
                               -1.0, 1.0)) *
          57.29577951;
      auto& e = vsum[qkey(p)];
      e.first += ang;
      e.second = p;
    }
  }
  const float inset = module * 1.6f;
  for (const auto& kv : vsum) {
    const SkPoint p = kv.second.second;
    if (p.x() < inset || p.y() < inset || p.x() > kW - inset ||
        p.y() > kH - inset)
      continue;
    a.interiorVerts++;
    const double err = std::abs(kv.second.first - 360.0);
    a.worstVertErr = std::max(a.worstVertErr, err);
    if (err > 0.5) a.badVerts++;
  }
  a.ratio = a.thin > 0 ? (double)a.fat / (double)a.thin : 0.0;

  // 2. Matching-arc chain. Every arc endpoint must land on an edge MIDPOINT,
  //    and every interior edge must collect exactly two of them (one from each
  //    of the two tiles sharing it) — that is the chain. The radius check
  //    below pins the edge length at the same time: an endpoint exactly
  //    module/2 from its own arc centre is what makes two arcs of radius
  //    module/2, centred on the two ends of a shared edge, meet tangentially
  //    at its midpoint.
  boost::unordered_flat_map<int64_t, std::pair<int, SkPoint>> ends;
  boost::unordered_flat_map<int64_t, int> mids;
  for (const Tile& t : tiles) {
    for (int i = 0; i < 4; ++i) {
      const SkPoint p = t.v[i], q = t.v[(i + 1) % 4];
      mids[qkey({(p.x() + q.x()) * 0.5f, (p.y() + q.y()) * 0.5f})]++;
    }
    for (int k = 0; k < 2; ++k) {
      const ArcSpec s = arcAt(t, k, module);
      // The edge each endpoint sits on, read back from the tile's own
      // vertices: end0 is the midpoint of the edge to v[ai+1], end1 of the
      // edge to v[ai+3].
      const int ai = t.arcAt[k];
      const SkPoint edgeFar[2] = {t.v[(ai + 1) % 4], t.v[(ai + 3) % 4]};
      const SkPoint endPts[2] = {s.end0, s.end1};
      for (int ei = 0; ei < 2; ++ei) {
        const SkPoint e = endPts[ei];
        auto& slot = ends[qkey(e)];
        slot.first++;
        slot.second = e;
        // radius check: the endpoint sits exactly module/2 from the centre
        a.worstMidErr = std::max(
            a.worstMidErr, (double)std::abs(std::hypot(e.x() - s.centre.x(),
                                                       e.y() - s.centre.y()) -
                                            module * 0.5f));
        // Tangent check, the C1 half: the arc's tangent at e must be
        // perpendicular to the edge e sits on. The tangent comes from the
        // arc's own radius; the edge direction comes from the tile's two
        // VERTICES — two independent measurements, so an arc centre or an
        // endpoint that drifts off its tile moves this dot product off zero.
        const double rx = e.x() - s.centre.x(), ry = e.y() - s.centre.y();
        const double L = std::hypot(rx, ry);
        const double tx = -ry / L, ty = rx / L;  // arc tangent at e
        const double edx = edgeFar[ei].x() - t.v[ai].x();
        const double edy = edgeFar[ei].y() - t.v[ai].y();
        const double eL = std::hypot(edx, edy);  // the edge, vertex to vertex
        a.worstTangentErr =
            std::max(a.worstTangentErr, std::abs((tx * edx + ty * edy) / eL));
      }
    }
  }
  for (const auto& kv : ends) {
    a.arcNodes++;
    const SkPoint p = kv.second.second;
    const bool interior = p.x() > inset && p.y() > inset &&
                          p.x() < kW - inset && p.y() < kH - inset;
    if (kv.second.first == 2)
      a.chained++;
    else if (interior)
      a.danglingInterior++;
    if (mids.find(kv.first) == mids.end())
      a.danglingInterior++;  // an endpoint that is not an edge midpoint at all
  }
  return a;
}

// ---------------------------------------------------------------------------
// Granite. Two recipes, per the sourcing: Royal White carries larger, sparser
// black feather-veining; Kobra grey a finer, denser, more uniform speckle —
// two recipes, not one texture tinted twice. Each is seeded off the tile's
// own identity so that a field made of two repeated shapes never reads as a
// stamped texture.

struct Granite {
  SkColor4f base, lit, vein;
  float speckleFreq;   // features/px — the mineral grain
  float speckleAmp;    // soft-light contrast
  float blotchFreq;    // the slow tonal drift across a slab
  float veinContrast;  // how hard the dark inclusions bite
};

// The two granites are photographed at one distance, and at that distance
// both read as a fine even salt-and-pepper: the difference between them is
// the SIZE of the dark inclusions, not the pitch of the field. A coarse
// pitch on one and a fine one on the other reads as two materials rendered
// at two resolutions.
const Granite kRoyalWhite{kWhiteBase, kWhiteLit, kWhiteVein, 0.88f,
                          1.05f,      0.030f,    0.42f};
const Granite kKobraGrey{kGreyBase, kGreyLit, kGreyVein, 1.00f,
                         0.88f,     0.070f,   0.32f};

// A bank keyed by (species, seed bucket). A tile's seed is folded into one of
// 40 buckets per granite, which is more variety than a field of two prototiles
// at ten orientations can expose, and it caps the number of live materials at
// 80 instead of one per sett. Because the instance is HELD rather than
// re-minted per describe, its identity is stable and a re-describe prunes.
//
// The stone itself is `material::kit::stone`: a quarry's two tones on a
// diagonal bed, veined with grain and flecked with a speckle in its own
// colours, generated per pixel from its parameters and a seed. The BUCKET is
// what varies a piece — a seed the recipe reads, and a jitter on the tone,
// both applied in the maker.
class GraniteBank {
 public:
  Paint get(const Granite& g, uint32_t seed, bool fat) {
    // The species is the params' bytes and the bucket is the seed, so the
    // two rhombs of one granite at one bucket are ONE material.
    const matkit::StoneParams species{
        .hi = skia::toColor(g.lit),
        .lo = skia::toColor(g.base),
        // The bed runs across the sett rather than along it, so a rotated
        // prototile does not read as a stripe following its own long axis,
        // and it is LONG compared with a 78 px sett: a shallow ramp from the
        // sun-facing corner to the shaded one, not a stripe.
        .bedAngle = fat ? 24.0f : 62.0f,
        .bedLength = 260.0f,
        .bedDepth = 0.30f,
        // FEATURES PER PIXEL, and this is the number that decides whether
        // the stone reads as granite or as cloud: at ~1 per px the veining
        // is at the crystal size — a 600 mm sett drawn 78 px wide is 7.7 mm
        // per pixel — and an order of magnitude lower is weather.
        .grainScale = g.speckleFreq,
        .grainContrast = g.speckleAmp * 0.42f,
        .stretch = 1.0f,
        // The dark inclusions: what separates the two granites at this
        // distance is their SIZE, not the pitch of the field.
        .speckle = 0.34f,
        .speckleCell = g.blotchFreq > 0.05f ? 6.5f : 4.0f,
        .speckleAlpha = 0.26f};
    return Paint::recipe(
        m_bank.get(matkit::stoneRecipe(), species, seed, [&](uint32_t bucket) {
          // The per-bucket tone jitter: one slab lighter than the next, out
          // of the same quarry.
          const float jitter = ((float)(bucket % 13) / 12.0f - 0.5f) * 0.115f;
          auto tone = [&](sigil::material::Color c) {
            return sigil::material::Color{
                std::clamp(c.r * (1 + jitter), 0.f, 1.f),
                std::clamp(c.g * (1 + jitter), 0.f, 1.f),
                std::clamp(c.b * (1 + jitter), 0.f, 1.f), 1};
          };
          matkit::StoneParams p = species;
          p.hi = tone(species.hi);
          p.lo = tone(species.lo);
          p.seed = (float)bucket;
          return sigil::material::Material(matkit::stoneRecipe(), p);
        }));
  }

 private:
  material::Bank m_bank{40};
};

// ---------------------------------------------------------------------------
// Geometry helpers

SkVector normv(SkVector v) {
  const float l = v.length();
  return l > 1e-6f ? SkVector{v.x() / l, v.y() / l} : SkVector{1, 0};
}

// THE SETT'S TWO INSET RINGS — the joint pull-back and the chamfer band.
// `path::insetPolygon` moves every vertex one for one, so each source corner
// keeps its partner in the moved ring, which is what a chamfer band between
// the two needs and an outline offset cannot give. The mitre is the price: a
// corner of interior angle θ moves distance/sin(θ/2), and at the thin rhomb's
// 36° corners that is 3.24 distances — geometrically right for a silhouette
// and, for a BAND, a wedge that swallows the corner. The miter limit blunts
// it instead, which is what a stonemason's arris does anyway.
void insetQuad(const SkPoint in[4], float d, SkPoint out[4],
               float miterLimit = 1e6f) {
  std::array<glm::vec2, 4> poly{};
  for (int i = 0; i < 4; ++i) poly[(size_t)i] = {in[i].x(), in[i].y()};
  const std::vector<glm::vec2> moved = path::insetPolygon(poly, d, miterLimit);
  for (int i = 0; i < 4 && i < (int)moved.size(); ++i)
    out[i] = {moved[(size_t)i].x, moved[(size_t)i].y};
}

SkPath quadPath(const SkPoint q[4], SkPoint origin) {
  SkPathBuilder b;
  b.moveTo(q[0].x() - origin.x(), q[0].y() - origin.y());
  for (int i = 1; i < 4; ++i)
    b.lineTo(q[i].x() - origin.x(), q[i].y() - origin.y());
  b.close();
  return b.detach();
}

// ---------------------------------------------------------------------------
// Robinson-triangle deflation — the vignette's independent construction.
// type 0 = acute (36-72-72), half of a THIN rhomb; first vertex is its 36°
// apex. type 1 = obtuse (36-36-108), half of a FAT rhomb; first vertex is its
// 108° apex. acute → 1 acute + 1 obtuse; obtuse → 1 acute + 2 obtuse, so at
// rhomb level Fat → 2 Fat + 1 Thin and Thin → 1 Fat + 1 Thin.

constexpr double kPhi = 1.6180339887498949;

struct Tri {
  int type = 0;
  SkPoint a{}, b{}, c{};
};

inline SkPoint lerpP(SkPoint p, SkPoint q, double t) {
  return {(float)(p.x() + (q.x() - p.x()) * t),
          (float)(p.y() + (q.y() - p.y()) * t)};
}

std::vector<Tri> deflate(const std::vector<Tri>& in) {
  std::vector<Tri> out;
  out.reserve(in.size() * 3);
  for (const Tri& t : in) {
    if (t.type == 0) {
      const SkPoint P = lerpP(t.a, t.b, 1.0 / kPhi);
      out.push_back({0, t.c, P, t.b});
      out.push_back({1, P, t.c, t.a});
    } else {
      const SkPoint Q = lerpP(t.b, t.a, 1.0 / kPhi);
      const SkPoint R = lerpP(t.b, t.c, 1.0 / kPhi);
      out.push_back({1, R, t.c, t.a});
      out.push_back({1, Q, R, t.b});
      out.push_back({0, R, Q, t.a});
    }
  }
  return out;
}

/** Is p inside triangle t (tolerantly)? The area audit below cannot see a
 *  child that is the right SIZE in the wrong PLACE; this can. */
inline bool insideTri(const Tri& t, SkPoint p, double eps) {
  auto side = [](SkPoint a, SkPoint b, SkPoint q) {
    return (double)(b.x() - a.x()) * (q.y() - a.y()) -
           (double)(b.y() - a.y()) * (q.x() - a.x());
  };
  const double s1 = side(t.a, t.b, p), s2 = side(t.b, t.c, p),
               s3 = side(t.c, t.a, p);
  const bool neg = s1 < -eps || s2 < -eps || s3 < -eps;
  const bool pos = s1 > eps || s2 > eps || s3 > eps;
  return !(neg && pos);
}

inline double triArea(const Tri& t) {
  return std::abs((double)(t.b.x() - t.a.x()) * (t.c.y() - t.a.y()) -
                  (double)(t.c.x() - t.a.x()) * (t.b.y() - t.a.y())) *
         0.5;
}

// ---------------------------------------------------------------------------

}  // namespace

// ===========================================================================

struct PenrosePaving : sketch::Sketch {
  std::vector<Tile> tiles;
  GraniteBank bank;
  Audit audit;

  // One raw progress per sett, in [0,1]. bind() reshapes it at the property
  // it feeds, so the fade and the seating overshoot ride the same `grow`
  // value under different curves instead of needing a vector of Outputs each.
  std::vector<choreograph::Output<float>> grow, arcT;
  choreograph::Output<float> sheen{0};
  double t = 0;
  int generation = -1;

  // --- the deflation vignette ---------------------------------------------
  std::vector<std::vector<Tri>> gens;
  double genArea0 = 0;
  int genAreaFails = 0;

  // -------------------------------------------------------------------------
  // One sett: an absolutely-placed box whose OUTLINE is the rhomb, inset by
  // half the saw-cut joint. No rotate() anywhere — the shape carries its own
  // orientation, so the chamfer's raking light and the granite ramp stay
  // world-fixed across all ten tile orientations for free.

  Element sett(const Tile& t, size_t i) {
    SkPoint outer[4], top[4];
    insetQuad(t.v, kJoint * 0.5f, outer, 2.6f);
    insetQuad(t.v, kJoint * 0.5f + kChamfer, top, 2.6f);

    SkRect bb = SkRect::MakeEmpty();
    bb.setBounds({t.v, 4});
    bb.outset(2.0f, 2.0f);
    const SkPoint org{bb.left(), bb.top()};

    SkPath shape = quadPath(outer, org);

    // The chamfer: four real trapezoids between the sett's outer edge and its
    // top face, each shaded by its own outward normal against ONE sun.
    std::array<SkPoint, 4> lo{}, hi{};
    std::array<float, 4> keyed{};
    for (int e = 0; e < 4; ++e) {
      lo[(size_t)e] = {outer[e].x() - org.x(), outer[e].y() - org.y()};
      hi[(size_t)e] = {top[e].x() - org.x(), top[e].y() - org.y()};
    }
    const SkPoint ctr{t.centre.x() - org.x(), t.centre.y() - org.y()};
    for (int e = 0; e < 4; ++e) {
      const SkPoint p = lo[(size_t)e], q = lo[(size_t)((e + 1) % 4)];
      SkVector n = normv({-(q.y() - p.y()), q.x() - p.x()});
      const SkVector mid{(p.x() + q.x()) * 0.5f - ctr.x(),
                         (p.y() + q.y()) * 0.5f - ctr.y()};
      if (n.x() * mid.x() + n.y() * mid.y() < 0) n = {-n.x(), -n.y()};
      keyed[(size_t)e] = -(n.x() * kSunTo.x() + n.y() * kSunTo.y());
    }

    auto chamfer = [lo, hi, keyed](SkCanvas& c, const PaintContext&) {
      SkPaint p;
      p.setAntiAlias(true);
      for (int e = 0; e < 4; ++e) {
        const int f = (e + 1) % 4;
        SkPathBuilder b;
        b.moveTo(lo[(size_t)e]);
        b.lineTo(lo[(size_t)f]);
        b.lineTo(hi[(size_t)f]);
        b.lineTo(hi[(size_t)e]);
        b.close();
        const float k = keyed[(size_t)e];
        // A sawn granite sett is nearly flush: the arris is a 2 mm pencil
        // chamfer on a 600 mm slab. Read the photograph, not the diagram —
        // what separates two setts there is a hairline, not a bevel.
        if (k >= 0)
          p.setColor4f({1.0f, 0.99f, 0.96f, 0.05f + 0.13f * k}, nullptr);
        else
          p.setColor4f({0.05f, 0.05f, 0.06f, 0.05f - 0.15f * k}, nullptr);
        c.drawPath(b.detach(), p);
      }
    };

    Element e =
        box()
            .left(bb.left())
            .top(bb.top())
            .width(bb.width())
            .height(bb.height())
            .shape([shape](SkSize) { return shape; })
            .fill(bank.get(t.fat ? kRoyalWhite : kKobraGrey, t.seed, t.fat))
            .foreground(Decoration(PaintProgram(chamfer)))
            // the saw cut: a hairline of the joint's own colour just
            // inside the silhouette, so neighbouring setts never fuse
            .stroke(stroke(0.7f, Fill::color(hex(0x3D4043, 0.34f)),
                           PathFormat::Align::Inner))
            // one Output, two curves: the fade eases out cubic, the
            // seating overshoots — shaped at the property, not in
            // the tick loop
            .opacity(
                bind(&grow[i]).map(choreograph::easeOutCubic).clamp(0.0f, 1.0f))
            .scale(
                bind(&grow[i]).map(ease::outBack(1.32f)).target(0.52f, 1.0f));

    for (int k = 0; k < 2; ++k) e.child(inlay(t, k, org, i));
    return e;
  }

  // --- one inlaid arc, in the tile's own (ζ_r, ζ_s) frame ------------------
  // The band's cross-section is a RADIAL ramp centred on the arc's own centre
  // of curvature: at constant radius the band is exactly the ring between
  // R−w/2 and R+w/2, so the gradient runs across the band's width and nowhere
  // else. That is the one thing a linear gradient cannot do on a curve.

  Element inlay(const Tile& t, int k, SkPoint parentOrg, size_t i) {
    const ArcSpec spec = arcAt(t, k, kModule);
    const SkPath world = arcPath(spec, kModule, kJoint * 0.5f + 0.9f);
    SkRect bb = world.getBounds();
    const float pad = kBandW * 0.5f + 2.6f;
    bb.outset(pad, pad);

    const SkPath local =
        world.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
    const SkPoint c{spec.centre.x() - bb.left(), spec.centre.y() - bb.top()};

    const float R = kModule * 0.5f;
    const float gR = R + kBandW;
    const float inner = (R - kBandW * 0.5f) / gR;
    const float outer = (R + kBandW * 0.5f) / gR;
    // A POLISHED INSERT IS FLAT. The bevel is a rim, not a section: the
    // face holds one tone across the middle three quarters of the band and
    // the edge tone is confined to the two millimetres either side of it.
    // Run across the whole width, the same ramp reads as piping with a
    // dark core rather than as a broad flat ribbon.
    const float rim = (outer - inner) * 0.12f;
    Fill band =
        radialGradient(c, gR,
                       {kSteelEdge, kSteelEdge, kSteelBase, kSteelSpec,
                        kSteelBase, kSteelEdge, kSteelEdge},
                       {0.0f, inner, inner + rim, (inner + outer) * 0.5f,
                        outer - rim, outer, 1.0f});

    return box()
        .left(bb.left() - parentOrg.x())
        .top(bb.top() - parentOrg.y())
        .width(bb.width())
        .height(bb.height())
        // the callable is invoked on every layout, so its capture must survive
        // each return
        // NOLINTNEXTLINE(performance-no-automatic-move)
        .shape([local](SkSize) { return local; })
        .stroke(spans::upTo(&arcT[i]),
                Brush{}  // the milled slot the insert sits in — a hairline of
                         // occlusion either side, not an outline
                    .layer(PathFormat{.width = kBandW + 0.9f,
                                      .strokeFill = Fill::color(kGroove)})
                    // the 30 mm polished insert
                    .layer(PathFormat{.width = kBandW, .strokeFill = band}));
  }

  // -------------------------------------------------------------------------
  // The deflation vignette — the OTHER construction, rendered flat so it
  // reads as the diagram beside the paving rather than as more paving.

  Element diagram(int gen) {
    const std::vector<Tri>& tri = gens[(size_t)std::clamp(gen, 0, 3)];
    // positioned(): every triangle carries its own bb rect — the
    // deflation patch is the field's pattern in miniature, Yoga-free.
    auto group = positioned()
                     .inset(0, 0, 0, 0)
                     .key("gen" + std::to_string(gen))
                     .staggerChildren(9ms, motion::Spread::From::Center)
                     .transformOrigin(0.5f, 0.5f)
                     .scale(animate(from(0.94f).to(1.0f),
                                    Transition{320ms, ease::outBack(1.1f)}));

    for (size_t i = 0; i < tri.size(); ++i) {
      const Tri& src = tri[i];
      // Two antialiased half-triangles sharing a rhomb diagonal leave a
      // hairline seam along it, which makes the whole diagram read as
      // TRIANGLES instead of rhombs — the one thing it must not say. Push
      // each half 0.45 px out from its own centroid so the halves overlap.
      Tri g = src;
      {
        const float cx = (src.a.x() + src.b.x() + src.c.x()) / 3.0f;
        const float cy = (src.a.y() + src.b.y() + src.c.y()) / 3.0f;
        for (SkPoint* q : {&g.a, &g.b, &g.c}) {
          const SkVector d = normv({q->x() - cx, q->y() - cy});
          *q = {q->x() + d.x() * 0.8f, q->y() + d.y() * 0.8f};
        }
      }
      SkPoint pts[3] = {g.a, g.b, g.c};
      SkRect bb = SkRect::MakeEmpty();
      bb.setBounds({pts, 3});
      bb.outset(1, 1);
      SkPathBuilder b;
      b.moveTo(g.a.x() - bb.left(), g.a.y() - bb.top());
      b.lineTo(g.b.x() - bb.left(), g.b.y() - bb.top());
      b.lineTo(g.c.x() - bb.left(), g.c.y() - bb.top());
      b.close();
      SkPath p = b.detach();
      group.child(
          box()
              .key("g" + std::to_string(gen) + "_" + std::to_string(i))
              .left(bb.left())
              .top(bb.top())
              .width(bb.width())
              .height(bb.height())
              .shape([p](SkSize) { return p; })
              .fill(Fill::color(g.type == 1 ? hex(0xB6B2A7) : hex(0x76797E)))
              // NO per-piece scale: scaling each half about its own
              // centre pulls a subdivision apart, and a deflation
              // diagram that shows gaps is saying the opposite of
              // what it is for. The enclosing group takes the
              // entrance transform instead.
              .opacity(animate(from(0.0f).to(1.0f),
                               Transition{260ms, choreograph::easeOutQuad})));
    }

    // The rhomb outlines: every triangle is run b→a→c and left OPEN, so the
    // b→c edge is never drawn — that edge is the diagonal each pair of halves
    // shares, so a Robinson triangle's two real rhomb edges are exactly the
    // two this run does draw. Close the path and the diagram claims a tiling
    // by triangles, which is the one thing it must not say.
    auto edges = tri;
    group.child(custom([edges](SkCanvas& c, const PaintContext&) {
                  SkPaint p;
                  p.setAntiAlias(true);
                  p.setStyle(SkPaint::kStroke_Style);
                  p.setStrokeWidth(1.0f);
                  p.setColor4f(hex(0x1B1D1E, 0.85f), nullptr);
                  for (const Tri& g : edges) {
                    SkPathBuilder b;
                    b.moveTo(g.b);
                    b.lineTo(g.a);
                    b.lineTo(g.c);
                    c.drawPath(b.detach(), p);
                  }
                })
                    .inset(0, 0, 0, 0)
                    .cache(Cache::None));
    return group;
  }

  Element inset() {
    const SkRect r = SkRect::MakeXYWH(1178, 76, 348, 268);
    return box()
        .left(r.left())
        .top(r.top())
        .width(r.width())
        .height(r.height())
        .fill(Fill::color(hex(0x121517, 0.84f)))
        .stroke(stroke(1.0f, Fill::color(hex(0x5E6163, 0.55f)),
                       PathFormat::Align::Inner))
        .background(styles::dropShadow(hex(0x000000, 0.55f), {0, 6}, 22))
        .child(text(toU8("DEFLATION \xc2\xb7 FAT \xe2\x86\x92 2 FAT + 1 THIN, "
                         "\xc3\x97"
                         "1/\xcf\x86"),
                    weave::textStyle(
                        {.size = 10.5f, .color = hex(0x8E9295), .track = 1.0f}))
                   .left(14)
                   .top(12))
        .child(box().left(10).top(34).width(kDiagW).height(kDiagH).child(
            slot("deflate")));
  }

  // -------------------------------------------------------------------------

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    // A positioned leaf set: every sett already carries its own computed
    // rect, so there is nothing for a layout pass to solve. Under a plain
    // box() each sett and its two inlays would mount three flex nodes;
    // positioned() mounts them with none.
    auto field = positioned().inset(0, 0, 0, 0);
    for (size_t i = 0; i < tiles.size(); ++i) field.child(sett(tiles[i], i));

    char spec[220];
    std::snprintf(spec, sizeof(spec),
                  "DE BRUIJN PENTAGRID  \xce\xb3=1/5 (\xce\x93=0)  s=%.0f px  "
                  "%d SETTS  FAT:THIN = %.3f  (\xcf\x86 = 1.618)",
                  kModule, audit.tiles, audit.ratio);

    return stack()
        .fill(Fill::color(kJointBed))
        // the bedding course showing through the saw cuts
        // A procedural grain evaluated over every pixel of the canvas, and
        // the most expensive node in the frame by a wide margin. Nothing it
        // depends on animates, so Cache::Texture bakes it once and the cache
        // never invalidates; the node's opacity is what keeps it out of the
        // automatic bake, so the cache has to be asked for by hand.
        .child(
            box()
                .inset(0, 0, 0, 0)
                .fill(Paint::recipe(field::grain(0.9f, 1, 12.0f, 0.55f, 1.0f)))
                .opacity(0.20f)
                .cache(Cache::Texture))
        .child(field)
        // Weathering at PLAZA scale — cells a couple of hundred px across,
        // i.e. metres of traffic staining that crosses joints because dirt
        // does not know where the setts are. Baked: it never changes, and a
        // two-octave grain over the whole canvas is not worth re-evaluating
        // once a frame to get the same pixels back.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kMultiply)
                   .opacity(0.42f)
                   .cache(Cache::Texture)
                   .fill(Paint::blend(
                       {{Paint::solid(hex(0xFFFFFF)), SkBlendMode::kSrcOver},
                        {Paint::recipe(
                             field::grain(0.0042f, 2, 91.0f, 0.62f, 1.15f)),
                         SkBlendMode::kSoftLight}})))
        // ---- daylight. One multiply pass carries the sun's falloff across
        // the plaza. It is SHALLOW: the header calls this a plan view and
        // the forecourt is photographed in flat daylight, so a key bright
        // enough to be read as a studio light is reading as a light rather
        // than as a floor.
        // Static, so it is baked: nothing here depends on the clock and the
        // gradient covers the whole canvas.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kMultiply)
                   .cache(Cache::Texture)
                   .fill(radialGradient(
                       {470, 280}, 1280,
                       {hex(0xFAFAF8), hex(0xE6E6E4), hex(0xB2B4B8),
                        hex(0x74777C), hex(0x42454A)},
                       {0.0f, 0.22f, 0.50f, 0.78f, 1.0f})))
        // the sun pool itself, added back — also static, baked for the same
        // reason as the pass above
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kPlus)
                   .opacity(0.5f)
                   .cache(Cache::Texture)
                   .fill(radialGradient(
                       {470, 280}, 1100,
                       {hex(0xFFF8E8, 0.13f), hex(0xFFF3DA, 0.075f),
                        hex(0xFFF0D0, 0.025f), hex(0x000000, 0.0f)},
                       {0.0f, 0.34f, 0.68f, 1.0f})))
        // wet-stone sheen — a broad, low raking band that sweeps once per
        // loop as the arcs finish, so the field reads as a wet surface
        // catching the sky rather than as flat fill
        .child(
            box()
                .inset(0, 0, 0, 0)
                .blend(SkBlendMode::kScreen)
                .opacity(&sheen)
                .fill(linearGradient({180, 0}, {1500, 1200},
                                     {hex(0x000000, 0.0f), hex(0xBFD2E0, 0.09f),
                                      hex(0x000000, 0.0f)},
                                     {0.30f, 0.50f, 0.72f})))
        .child(inset())
        // ---- the site plaque. A civic plaque sits on the paving, so give
        // it a shadowed band to sit in rather than dropping 10 px type onto
        // speckled granite where it cannot be read at any exposure.
        .child(box().left(0).top(kH - 190).width(kW).height(190).fill(
            linearGradient({0, kH - 190}, {0, kH},
                           {hex(0x000000, 0.0f), hex(0x08090A, 0.42f),
                            hex(0x08090A, 0.72f)},
                           {0.0f, 0.5f, 1.0f})))
        .child(box()
                   .left(56)
                   .top(1084)
                   .width(1010)
                   .height(96)
                   .fill(Fill::color(hex(0x101314, 0.90f)))
                   .stroke(stroke(1.0f, Fill::color(hex(0x676B6D, 0.45f)),
                                  PathFormat::Align::Inner))
                   .background(
                       styles::dropShadow(hex(0x000000, 0.5f), {0, 5}, 18)))
        .child(text(toU8("PENROSE TILING \xc2\xb7 P3 RHOMBI \xc2\xb7 ROYAL "
                         "WHITE & KOBRA GREY GRANITE \xc2\xb7 POLISHED 30 mm "
                         "STAINLESS INSERTS"),
                    weave::textStyle(
                        {.size = 13.0f, .color = hex(0xDCE0E2), .track = 1.9f}))
                   .left(76)
                   .top(1100)
                   .opacity(1.0f))
        .child(text(toU8("MATHEMATICAL INSTITUTE, ANDREW WILES BUILDING, "
                         "OXFORD \xc2\xb7 R. PENROSE 1974 / PAVING 2012"),
                    weave::textStyle(
                        {.size = 11.5f, .color = hex(0xA9AEB1), .track = 1.5f}))
                   .left(76)
                   .top(1126)
                   .opacity(1.0f))
        .child(text(toU8(spec),
                    weave::textStyle(
                        {.size = 10.5f, .color = hex(0x8E9598), .track = 1.3f}))
                   .left(76)
                   .top(1152)
                   .opacity(1.0f));
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    // The finished plaza. 0.75 catches the crystal front mid-growth over the
    // exact five-fold centre, 1.45 the inlay chaining on.
    ctx.captureAt(4.6);
    ctx.canvas(kW, kH);
    ctx.background(kNight);

    tiles = buildField(kModule, kModule * 1.2f);
    audit = verify(tiles, kModule);

    std::printf(
        "\n[penrose] pentagrid gamma=%.3f  module=%.1f px\n"
        "[penrose] tiles=%d  fat=%d  thin=%d  fat:thin=%.4f  (phi=1.6180)\n"
        "[penrose] interior vertices=%d  angle-sum failures=%d  worst err=%.4f "
        "deg\n"
        "[penrose] arc nodes=%d  chained(deg 2)=%d  dangling interior=%d\n"
        "[penrose] worst |endpoint-midpoint| = %.6f px   worst tangent dot = "
        "%.2e\n",
        kOffset, kModule, audit.tiles, audit.fat, audit.thin, audit.ratio,
        audit.interiorVerts, audit.badVerts, audit.worstVertErr, audit.arcNodes,
        audit.chained, audit.danglingInterior, audit.worstMidErr,
        audit.worstTangentErr);

    // --- the deflation vignette's own construction + area audit ------------
    {
      // Seed: ONE fat rhomb = two obtuse Robinson halves mirrored across its
      // long diagonal (which joins the 72° corners and has length φ·s).
      const double a0 = -0.9424777961;  // −54°: the first edge's direction
      const V2 u{std::cos(a0), std::sin(a0)};
      const V2 v{std::cos(a0 + 1.2566370614), std::sin(a0 + 1.2566370614)};
      auto P = [&](V2 p) { return SkPoint{(float)p.x, (float)p.y}; };
      const SkPoint V0 = P({0, 0}), V1 = P(u), V2p = P(u + v), V3 = P(v);
      gens.clear();
      gens.push_back({{1, V1, V0, V2p}, {1, V3, V2p, V0}});
      genArea0 = 0;
      for (const Tri& t : gens[0]) genArea0 += triArea(t);
      genAreaFails = 0;
      int genOutside = 0;
      for (int g = 0; g < 3; ++g) {
        const std::vector<Tri> parents = gens.back();
        gens.push_back(deflate(parents));
        double area = 0;
        for (const Tri& t : gens.back()) area += triArea(t);
        if (std::abs(area - genArea0) > genArea0 * 1e-4) genAreaFails++;
        // every child must lie inside SOME parent — equal areas alone would
        // happily accept a correctly-sized child dropped in the wrong place
        for (const Tri& ch : gens.back()) {
          bool ok = false;
          for (const Tri& pa : parents)
            if (insideTri(pa, ch.a, 1e-6) && insideTri(pa, ch.b, 1e-6) &&
                insideTri(pa, ch.c, 1e-6)) {
              ok = true;
              break;
            }
          if (!ok) genOutside++;
        }
      }
      // Deflation subdivides the SAME region, so one fit computed on the
      // seed lands every generation: the patch holds still while its tiles
      // get finer by 1/φ — the shrink-expand view, read the other way round.
      SkRect bb = SkRect::MakeEmpty();
      {
        SkPoint pts[4] = {V0, V1, V2p, V3};
        bb.setBounds({pts, 4});
      }
      const float m = 16.0f;
      const float k = std::min((kDiagW - 2 * m) / bb.width(),
                               (kDiagH - 2 * m) / bb.height());
      const float ox = (kDiagW - bb.width() * k) * 0.5f - bb.left() * k;
      const float oy = (kDiagH - bb.height() * k) * 0.5f - bb.top() * k;
      for (auto& gen : gens)
        for (Tri& tr : gen)
          for (SkPoint* q : {&tr.a, &tr.b, &tr.c})
            *q = {q->x() * k + ox, q->y() * k + oy};

      // Point-sampled coverage: the audit that actually sees a hole. Area
      // sums and containment both pass on a subdivision that overlaps here
      // and gaps there.
      {
        SkRect bb0 = SkRect::MakeEmpty();
        {
          SkPoint pts[6] = {gens[0][0].a, gens[0][0].b, gens[0][0].c,
                            gens[0][1].a, gens[0][1].b, gens[0][1].c};
          bb0.setBounds({pts, 6});
        }
        int uncovered = 0, doubled = 0, inside = 0;
        for (int iy = 0; iy < 120; ++iy)
          for (int ix = 0; ix < 120; ++ix) {
            const SkPoint q{
                bb0.left() + bb0.width() * ((float)ix + 0.5f) / 120,
                bb0.top() + bb0.height() * ((float)iy + 0.5f) / 120};
            int inSeed = 0;
            for (const Tri& t : gens[0])
              if (insideTri(t, q, -1e-4)) inSeed++;
            if (inSeed == 0) continue;
            inside++;
            int n = 0;
            for (const Tri& t : gens[3])
              if (insideTri(t, q, -1e-4)) n++;
            if (n == 0)
              uncovered++;
            else if (n > 1)
              doubled++;
          }
        std::printf(
            "[penrose] deflation coverage @gen3: %d samples in the seed,"
            " %d uncovered, %d double-covered\n",
            inside, uncovered, doubled);
      }
      std::printf(
          "[penrose] deflation rhombs: %zu -> %zu -> %zu -> %zu   "
          "area failures=%d  children outside their parent=%d\n",
          gens[0].size() / 2, gens[1].size() / 2, gens[2].size() / 2,
          gens[3].size() / 2, genAreaFails, genOutside);
      std::fflush(stdout);
    }

    grow = std::vector<choreograph::Output<float>>(tiles.size());
    arcT = std::vector<choreograph::Output<float>>(tiles.size());
    for (size_t i = 0; i < tiles.size(); ++i) {
      grow[i] = 0.0f;
      arcT[i] = 0.0f;
    }
    t = 0;
    generation = -1;

    ctx.ticker.add([this](double dt) {
      t += dt;
      const double now = std::fmod(t, kPeriod);
      for (size_t i = 0; i < tiles.size(); ++i) {
        const double u = (double)tiles[i].radius / (double)kCorner;
        // the ripple front: a raw linear progress, nothing shaped here
        grow[i] = clamp01((now - (kTileT0 + kTileSweep * u)) / kTileDur);
        arcT[i] = choreograph::easeOutCubic(
            clamp01((now - (kArcT0 + kArcSweep * u)) / kArcDur));
      }
      const float sp = clamp01((now - kSheen0) / kSheenDur);
      sheen = std::sin(sp * 3.14159265f);  // one pass, then gone
      return true;
    });

    ctx.composer.render(describe(ctx));
    generation = 0;
    ctx.composer.renderSlot("deflate", diagram(0));
  }

  void update(double, sketch::SketchContext& ctx) override {
    const double now = std::fmod(t, kPeriod);
    int g = 0;
    for (int i = 3; i >= 0; --i)
      if (now >= kGenAt[i]) {
        g = i;
        break;
      }
    if (now < kGenAt[0]) g = 0;
    if (g != generation) {
      generation = g;
      // Only the vignette's mount point re-renders. The paving's setts and
      // their baked caches are outside this slot and are not re-described.
      ctx.composer.renderSlot("deflate", diagram(g));
    }
  }
};

SIGIL_SKETCH(
    PenrosePaving, "Study \xc2\xb7 Pattern",
    "Penrose's 2012 P3 paving, Oxford \xe2\x80\x94 549 setts from de Bruijn's "
    "pentagrid")
