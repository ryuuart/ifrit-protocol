#pragma once

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilgeometry/path/Lattice.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace matkit = sigil::material::kit;
namespace mat = sigil::material;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace measure = sigil::measure;
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

const SkColor4f kWhiteBase = hexColor(0xBFBCB2);  // Royal White, weathered
const SkColor4f kWhiteLit = hexColor(0xD4D0C6);   // Royal White, sun side
const SkColor4f kWhiteVein =
    hexColor(0x2B2A28);  // black feather-vein inclusions
const SkColor4f kGreyBase = hexColor(0x82858A);  // Kobra grey
const SkColor4f kGreyLit = hexColor(0x969A9E);   // Kobra grey, sun side
const SkColor4f kGreyVein = hexColor(0x3E4042);  // Kobra's tighter speckle

const SkColor4f kSteelBase =
    hexColor(0xEEF1F2);  // polished stainless, overcast
const SkColor4f kSteelSpec = hexColor(0xFEFEFE);  // direct catch-light
const SkColor4f kSteelEdge = hexColor(0xC9CED1);  // the insert's chamfered lip
const SkColor4f kGroove =
    hexColor(0x5A5F63, 0.38f);  // occlusion in the milled slot

const SkColor4f kJointBed =
    hexColor(0x33363A);  // saw-cut joint / bedding mortar
const SkColor4f kNight = hexColor(0x101112);
const SkColor4f kCaption = hexColor(0x9CA0A2);

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
// geometry and no tile carries a schedule of its own. It is NOT
// `Spread::rankBy`: a rank ladder spaces its units evenly in rank order,
// where a ring twice as far out here waits twice as long — which is what
// makes the sweep read as one wave crossing the paving rather than as a
// queue of tiles.

constexpr double kPeriod = 9.2;
constexpr double kTileT0 = 0.05, kTileSweep = 1.35, kTileDur = 0.52;
constexpr double kArcT0 = 1.20, kArcSweep = 1.05, kArcDur = 0.58;
constexpr double kSheen0 = 2.55, kSheenDur = 1.10;
const double kGenAt[4] = {0.70, 1.55, 2.45, 3.35};

// ---------------------------------------------------------------------------
// de Bruijn's pentagrid

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
  // vectors), so the plaza's own half-diagonal in px is that many rhomb
  // edges of tiling. Two edges of margin past it, and the clip below
  // discards whatever the corner of the canvas does not reach — no
  // authored index table anywhere.
  const float reach = std::hypot(kW * 0.5f, kH * 0.5f) + padPx;
  const SkRect keep = SkRect::MakeWH(kW, kH).makeOutset(padPx, padPx);
  const path::MultigridTiling tiling = path::multigrid(
      path::multigridRing(5, kOffset),
      {.radius = (double)(reach / module) + 2.0, .maxRhombs = 200000});

  for (const path::MultigridRhomb& r : tiling.rhombs) {
    const int apart = std::abs(r.families[0] - r.families[1]);
    Tile t;
    t.r = r.families[0];
    t.s = r.families[1];
    t.kr = r.lines[0];
    t.ks = r.lines[1];
    t.fat = std::min(apart, 5 - apart) == 1;
    double sx = 0, sy = 0;
    for (int i = 0; i < 4; ++i) {
      const glm::dvec2 c = tiling.vertices[(size_t)r.corners[i]];
      t.v[i] = {kCx + (float)(c.x * module), kCy + (float)(c.y * module)};
      sx += c.x;
      sy += c.y;
    }
    t.centre = {kCx + (float)(sx * 0.25 * module),
                kCy + (float)(sy * 0.25 * module)};
    t.radius = std::hypot(t.centre.x() - kCx, t.centre.y() - kCy);

    // The arcs always sit on the ζ_r+ζ_s DIAGONAL — v[0] (the tiling's
    // canonical low corner) and v[2] — never on "the acute pair". That is
    // forced, not chosen: in the dualization every edge of the whole tiling
    // runs tail → tail + ζ_j for a fixed j, so marking each edge at a·s
    // from its tail puts BOTH marks around v[0] at a·s and both around
    // v[2] at (1−a)·s, while the other two corners see one of each and
    // admit no circular arc at all. The two arc classes are exactly de
    // Bruijn's single/double arrow. (a = 1/2 here is also forced: an arc at
    // the thin rhomb's 144° corner must clear the far edge at s·sin36° =
    // 0.588s, so a ∈ [0.412, 0.588], and 1/2 is the only value that makes
    // the two classes congruent — which is what makes the chain C1 as well
    // as C0.)
    t.arcAt[0] = 0;
    t.arcAt[1] = 2;
    t.seed = hash4(t.r, t.s, t.kr, t.ks);

    SkRect bb = SkRect::MakeEmpty();
    bb.setBounds({t.v, 4});
    if (SkRect::Intersects(bb, keep)) out.push_back(t);
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
  mat::Bank m_bank{40};
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
