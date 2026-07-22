// ds2_bench.cpp — DEAD SPACE 2 (Visceral Games, 2011): the BENCH.
//
// The subject: the "Nanocircuit Repair" upgrade circuit Isaac gets at any
// Bench — a diegetic holographic panel that renders in front of him, in
// world space, without pausing the game (Dino Ignacio, GDC 2013 "Crafting
// Destruction: The Evolution of the Dead Space User Interface",
// gdcvault.com/play/1017723; hudsandguis.com/home/2012/08/22/
// dead-space-2-diegetic-interface-design). Power Nodes are spent into a
// rectilinear PCB trace: a trunk line with right-angle branch taps, typed
// nodes (DMG/CAP/CHR/REL) and unlabeled blank connector nodes that exist
// only to join branches (deadspace.fandom.com/wiki/Bench).
//
// The load-bearing reference is the actual in-game frame:
//   https://static.wikia.nocookie.net/deadspace/images/4/43/Bench_branches.jpg
// (1200x750, the CONTACT BEAM circuit — a DS2-exclusive weapon, which is
// what dates the screen to DS2 rather than the 2008 original). Every
// number below was read off that frame:
//
//   * 21 nodes on an 8-column x 4-row lattice, column pitch ~72 px, row
//     pitch ~51 px, trunk on row 1. 9 typed + 12 blank, 23 edges.
//   * the frame is NOT a rounded rect and not a plain octagon: its top
//     edge is TIERED — the outer thirds sit ~20 px lower than the raised
//     centre section, joined by 45 deg risers, and all four corners are
//     chamfered. The inner contour mirrors it INVERTED: its shoulders sit
//     ~11 px inside the outer, then DIP 24 px on a 45 deg fall and run
//     right as the header rule under the title. One contour, two jobs.
//   * every node is a four-layer stamp: dark well, cyan corona (a
//     speckled radial burst), bright type-coloured ring, muted fill.
//   * traces are CASED double hairlines with occasional 45 deg jogs, not
//     single strokes and not pure Manhattan.
//   * the legend is a bracket-framed region (corner Ls, never a closed
//     box) of right-aligned labels + colour dot + chevron pip bar + Pts.
//   * the ONE warm accent on an all-cyan screen is the brass Power Node
//     puck in the bottom-right NODES counter.
//
// Density: the reference tree is legible but sparse once the 3D framing
// (Isaac's shoulder, the depth of field) is gone. The Bench also upgrades
// the RIG and the Stasis Module, so the empty band under the weapon
// circuit carries two SMALLER trees in the same grammar — invented
// topology, transcribed vocabulary.
//
// Palette (calibrated off the compressed screenshot; hue/role held, not
// the 6th hex digit): panel #0B1E21, header/legend #123030, hologram cyan
// #8FE0E6, title #E4F7F8, DMG #7E3833/#EFC0B8, CAP #2B5170/#B7D9EE,
// CHR #55692F/#DCEBAF, REL #5F4620/#E7C68A, brass #C9A227 -> #E8C860.
// Type: Eurostile Bold Extended lineage (fan-typography consensus, not a
// credited Visceral asset) — DIN Alternate Bold stretched via scaleX.
//
// Render:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/ds2_bench.cpp \
//       --frame /tmp/ds2_bench.png --at 2.5

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// palette

constexpr SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f,
          (hex & 0xFF) / 255.0f, a};
}
inline SkColor4f alpha(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

const SkColor4f kBody = rgb(0x0B1E21);
const SkColor4f kStrip = rgb(0x102A2A);
const SkColor4f kCyan = rgb(0x8FE0E6);
const SkColor4f kTitle = rgb(0xE4F7F8);
const SkColor4f kDim = rgb(0x33514E);
const SkColor4f kBrassLo = rgb(0xC9A227);
const SkColor4f kBrassHi = rgb(0xE8C860);
const SkColor4f kBrassDk = rgb(0x5E4914);

// ---------------------------------------------------------------------------
// canvas geometry (1200 x 800)

constexpr float kW = 1200, kH = 800;
constexpr float kPX = 108, kPY = 52, kPW = 984, kPH = 690;
constexpr float kPR = kPX + kPW, kPB = kPY + kPH;
constexpr float kOuterCut = 26, kOuterStep = 22, kOuterShoulder = 132;
constexpr float kInset = 12, kInnerCut = 18, kInnerDip = 40;
constexpr float kInnerShoulderL = 150, kInnerShoulderR = 46;
constexpr float kRuleY = kPY + kInset + kInnerDip; // the header rule, 104
constexpr float kBandY = 544, kBandH = 148;
constexpr float kLegX = 140, kLegW = 690;
constexpr float kCntX = 844, kCntW = 216;
constexpr float kHintY = 706;
constexpr float kPipW = 36, kPipH = 17, kPipGap = 6;

// ---------------------------------------------------------------------------
// type

inline sk_sp<SkTypeface> uiFace(bool bold) {
  auto mgr = sigil::weave::ports::systemFontManager();
  if (!mgr)
    return nullptr;
  const SkFontStyle want =
      bold ? SkFontStyle::Bold()
           : SkFontStyle(SkFontStyle::kMedium_Weight,
                         SkFontStyle::kNormal_Width,
                         SkFontStyle::kUpright_Slant);
  // Eurostile Extended lineage; DIN Alternate is the closest squared-off
  // technical grotesque macOS ships, stretched the last of the way.
  for (const char *family : {"Eurostile", "Bank Gothic", "DIN Alternate",
                             "Helvetica Neue", "Arial"})
    if (sk_sp<SkTypeface> f = mgr->matchFamilyStyle(family, want))
      return f;
  return mgr->matchFamilyStyle(nullptr, want);
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float trackEm = 0.07f, bool bold = true,
                                    float stretch = 1.16f) {
  static const sk_sp<SkTypeface> faceB = uiFace(true);
  static const sk_sp<SkTypeface> faceR = uiFace(false);
  sigil::weave::TextStyle s;
  s.shaping.typeface = bold ? faceB : faceR;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = trackEm * size;
  s.shaping.scaleX = stretch;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// ---------------------------------------------------------------------------
// outline generators — one chamfer vocabulary, reused at five sizes

/** The plain chamfered octagon (legend, counter, chips). */
inline std::function<SkPath(SkSize)> chamfer(float cut) {
  return [cut](SkSize s) {
    const float w = s.width(), h = s.height();
    const float c = std::min(cut, std::min(w, h) * 0.5f);
    SkPathBuilder b;
    b.moveTo(c, 0);
    b.lineTo(w - c, 0);
    b.lineTo(w, c);
    b.lineTo(w, h - c);
    b.lineTo(w - c, h);
    b.lineTo(c, h);
    b.lineTo(0, h - c);
    b.lineTo(0, c);
    b.close();
    return b.detach();
  };
}

/** The panel's OUTER silhouette: chamfered corners plus the tiered top —
 *  the outer thirds sit `step` px lower than the raised centre, joined by
 *  45 deg risers. Read straight off the frame. */
inline std::function<SkPath(SkSize)> panelOuter(float cut, float step,
                                                float shoulder) {
  return [cut, step, shoulder](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(0, step + cut);
    b.lineTo(cut, step);
    b.lineTo(shoulder - step, step);
    b.lineTo(shoulder, 0);
    b.lineTo(w - shoulder, 0);
    b.lineTo(w - shoulder + step, step);
    b.lineTo(w - cut, step);
    b.lineTo(w, step + cut);
    b.lineTo(w, h - cut);
    b.lineTo(w - cut, h);
    b.lineTo(cut, h);
    b.lineTo(0, h - cut);
    b.close();
    return b.detach();
  };
}

/** The panel's INNER contour: the same vocabulary inverted — shoulders at
 *  the top, then a 45 deg fall into the header rule that runs the width.
 *  One contour does the frame AND the title underline. */
inline std::function<SkPath(SkSize)> panelInner(float cut, float dip,
                                                float shoulderL,
                                                float shoulderR) {
  return [cut, dip, shoulderL, shoulderR](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(0, cut);
    b.lineTo(cut, 0);
    b.lineTo(shoulderL - dip, 0);
    b.lineTo(shoulderL, dip);
    b.lineTo(w - shoulderR, dip);
    b.lineTo(w - shoulderR + dip, 0);
    b.lineTo(w - cut, 0);
    b.lineTo(w, cut);
    b.lineTo(w, h - cut);
    b.lineTo(w - cut, h);
    b.lineTo(cut, h);
    b.lineTo(0, h - cut);
    b.close();
    return b.detach();
  };
}

/** Four corner Ls — how the Bench frames its legend and counter regions
 *  (brackets, never a closed box). */
inline std::function<SkPath(SkSize)> cornerBrackets(float arm) {
  return [arm](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(arm, 0); b.lineTo(0, 0); b.lineTo(0, arm);
    b.moveTo(w - arm, 0); b.lineTo(w, 0); b.lineTo(w, arm);
    b.moveTo(w, h - arm); b.lineTo(w, h); b.lineTo(w - arm, h);
    b.moveTo(arm, h); b.lineTo(0, h); b.lineTo(0, h - arm);
    return b.detach();
  };
}

/** The node corona: short ragged radial ticks — the cheap analogue of the
 *  screenshot's speckled burst around every typed node. */
inline std::function<SkPath(SkSize)> burst(int count, float inner) {
  return [count, inner](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float r = s.width() * 0.5f;
    for (int i = 0; i < count; ++i) {
      const float a = 6.2831853f * (float)i / (float)count + 0.13f;
      const float c = std::cos(a), sn = std::sin(a);
      const float k = (i % 3 == 0) ? 1.0f : (i % 3 == 1 ? 0.86f : 0.93f);
      b.moveTo(cx + c * r * inner, cy + sn * r * inner);
      b.lineTo(cx + c * r * k, cy + sn * r * k);
    }
    return b.detach();
  };
}

/** One legend pip: a chevron cell (point right, notch left). */
inline std::function<SkPath(SkSize)> chevron() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height(), n = h * 0.34f;
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(w - n, 0);
    b.lineTo(w, h * 0.5f);
    b.lineTo(w - n, h);
    b.lineTo(0, h);
    b.lineTo(n, h * 0.5f);
    b.close();
    return b.detach();
  };
}

inline std::function<SkPath(SkSize)> hline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, s.height() * 0.5f);
    b.lineTo(s.width(), s.height() * 0.5f);
    return b.detach();
  };
}
inline std::function<SkPath(SkSize)> vline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() * 0.5f, 0);
    b.lineTo(s.width() * 0.5f, s.height());
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// the router the Bench actually draws with
//
// routers::orthogonal() was the first thing tried here and it is the
// WRONG shape twice over: it always breaks at midX (a Z, never an L at
// the target's column — a PCB never routes that way), and on an
// axis-aligned pair it emits DUPLICATE points, which makes every
// parallel-offset brush (lines::cased) spike at both endpoints. Routers
// are values, so this is the fix: an L with a 45 deg cut corner, a single
// clean segment for a pure run, and an optional 45 deg Z-JOG for long
// horizontals — the stepped trace the screenshot is full of.

inline Router pcb(float cut, float jog) {
  return [cut, jog](const SkRect &from, const SkRect &to) {
    const float ax = from.centerX(), ay = from.centerY();
    const float bx = to.centerX(), by = to.centerY();
    const float dx = bx - ax, dy = by - ay;
    SkPathBuilder b;
    b.moveTo(ax, ay);
    if (std::abs(dx) < 0.5f || std::abs(dy) < 0.5f) {
      const bool horiz = std::abs(dy) < 0.5f;
      const float run = horiz ? std::abs(dx) : std::abs(dy);
      if (jog != 0 && run > 90.0f) {
        // step out, rise 45 deg, run, fall 45 deg, step in
        const float s = horiz ? (dx > 0 ? 1.f : -1.f) : (dy > 0 ? 1.f : -1.f);
        const float j = std::abs(jog) * (jog > 0 ? 1.f : -1.f);
        const float lead = 26.0f;
        if (horiz) {
          b.lineTo(ax + s * lead, ay);
          b.lineTo(ax + s * (lead + std::abs(j)), ay + j);
          b.lineTo(bx - s * (lead + std::abs(j)), ay + j);
          b.lineTo(bx - s * lead, ay);
        } else {
          b.lineTo(ax, ay + s * lead);
          b.lineTo(ax + j, ay + s * (lead + std::abs(j)));
          b.lineTo(ax + j, by - s * (lead + std::abs(j)));
          b.lineTo(ax, by - s * lead);
        }
      }
      b.lineTo(bx, by);
      return b.detach();
    }
    const float c = std::min(cut, std::min(std::abs(dx), std::abs(dy)));
    const float sx = dx > 0 ? 1.f : -1.f, sy = dy > 0 ? 1.f : -1.f;
    b.lineTo(bx - sx * c, ay);
    b.lineTo(bx, ay + sy * c);
    b.lineTo(bx, by);
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// the CRT stack: one SkSL scanline field, stepped at 6 Hz

inline Material scanField(SkColor4f tint, float period) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float  uTime;
      uniform float  uPeriod;
      uniform float4 uColor;
      half4 main(float2 xy) {
        float y = xy.y + uTime * 7.0;          // slow creep
        float f = fract(y / uPeriod);
        float scan = smoothstep(0.66, 0.34, f);
        // the faint vertical structure the CRT panel carries under the
        // scanlines (two incommensurate sines = no visible repeat)
        float band = 0.5 + 0.5 * sin(xy.x * 0.061) * sin(xy.x * 0.0173 + 1.3);
        float a = uColor.a * (0.28 + 0.72 * scan) * (0.5 + 0.5 * band);
        return half4(half3(uColor.rgb) * a, a);
      }
    )"));
    if (!effect)
      SkDebugf("ds2 scanField: %s\n", err.c_str());
    return effect;
  }();
  if (!fx)
    return Material::solid({0, 0, 0, 0});
  return Material::sksl(fx, {{"uPeriod", period}})
      .uniform("uColor", tint)
      .quantizeTime(6.0f); // the declared-choppiness rule
}

/** LUMINANCE grain — the CRT capture's dirt, equal in all three channels
 *  so an kOverlay pass reads as LIGHT rather than as a hue shift.
 *
 *  This is patterns::grain()'s job and patterns::grain() SEGFAULTS on the
 *  raster path: its fBm loop breaks on a uniform-dependent condition
 *  (`for (int o=0;o<8;++o) { if (float(o) >= uOctaves) break; ... }`) and
 *  Skia's raster pipeline does not survive lowering it — the effect
 *  compiles clean, then kills the process at draw. Bisected by unrolling
 *  that one loop, which fixes it. Two octaves, straight-line, here. */
inline Material grainField(float freq, float seed) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float uFreq;
      uniform float uSeed;
      float hash21(float2 p) {
        p = fract(p * float2(123.34, 456.21) + uSeed);
        p += dot(p, p + 45.32);
        return fract(p.x * p.y);
      }
      float vnoise(float2 p) {
        float2 i = floor(p);
        float2 f = fract(p);
        float2 u = f * f * (3.0 - 2.0 * f);
        return mix(mix(hash21(i), hash21(i + float2(1, 0)), u.x),
                   mix(hash21(i + float2(0, 1)), hash21(i + float2(1, 1)),
                       u.x), u.y);
      }
      half4 main(float2 pos) {
        float2 q = pos * uFreq;
        float v = (0.5 * vnoise(q) + 0.25 * vnoise(q * 2.0)) / 0.75;
        return half4(half3(v), 1.0);
      }
    )"));
    if (!effect)
      SkDebugf("ds2 grainField: %s\n", err.c_str());
    return effect;
  }();
  if (!fx)
    return Material::solid({0.5f, 0.5f, 0.5f, 1});
  return Material::sksl(fx, {{"uFreq", freq}, {"uSeed", seed}});
}

// ---------------------------------------------------------------------------
// circuits

enum Kind : int { Blank = 0, DMG, CAP, CHR, REL };

struct NodeDef { int col, row; Kind kind; };
struct EdgeDef { int a, b; float jog; };

struct KindArt { SkColor4f fill, ring; const char *label; };
inline KindArt artOf(Kind k) {
  switch (k) {
  case DMG: return {rgb(0x7E3833), rgb(0xEFC0B8), "DMG"};
  case CAP: return {rgb(0x2B5170), rgb(0xB7D9EE), "CAP"};
  case CHR: return {rgb(0x55692F), rgb(0xDCEBAF), "CHR"};
  case REL: return {rgb(0x5F4620), rgb(0xE7C68A), "REL"};
  case Blank:
  default: return {{0.02f, 0.08f, 0.09f, 0.5f}, alpha(kCyan, 0.88f), nullptr};
  }
}

// -- the CONTACT BEAM circuit, transcribed node-for-node from the frame --
constexpr NodeDef kBeamNodes[] = {
    // row 0 — the upper branch caps
    {1, 0, Blank}, {2, 0, DMG},   {4, 0, CAP},   {7, 0, DMG},
    // row 1 — the trunk
    {0, 1, Blank}, {1, 1, Blank}, {2, 1, CAP},   {3, 1, Blank},
    {4, 1, Blank}, {5, 1, Blank}, {6, 1, Blank}, {7, 1, Blank},
    // row 2 — the lower branches
    {1, 2, CHR},   {2, 2, Blank}, {3, 2, REL},   {5, 2, Blank},
    {6, 2, CHR},   {7, 2, CAP},
    // row 3 — the second lower rank
    {2, 3, Blank}, {3, 3, Blank}, {5, 3, DMG},
};
// Every edge is an independent two-endpoint route sharing anchor KEYS —
// a degree-3 blank node is a T-junction because three routes END on it,
// not because any primitive knows the word "junction".
constexpr EdgeDef kBeamEdges[] = {
    {4, 5, 0},  {5, 6, 0},  {6, 7, 0},  {7, 8, 0},   // trunk, left to right
    {8, 9, 0},  {9, 10, 0}, {10, 11, 0},
    {5, 0, 0},  {0, 1, -10}, {8, 2, 0},  {11, 3, 0},  // upper taps
    {6, 13, 0}, {13, 12, 10}, {13, 18, 0}, {7, 14, 0},// lower taps
    {14, 19, 0}, {9, 15, 0}, {15, 16, -10}, {10, 16, 0},
    {11, 17, 0}, {15, 20, 0},
    {18, 19, 0}, {19, 20, 10},                        // second lower rank
};

// -- the two smaller Bench trees: the RIG and the Stasis Module --
constexpr NodeDef kMiniNodes[] = {
    {0, 1, Blank}, {1, 1, Blank}, {2, 1, CAP}, {3, 1, Blank}, {4, 1, Blank},
    {1, 0, CHR},   {3, 0, DMG},
};
constexpr EdgeDef kMiniEdges[] = {
    {0, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 4, 0}, {1, 5, 0}, {3, 6, 0},
};
constexpr NodeDef kMini2Nodes[] = {
    {0, 1, Blank}, {1, 1, Blank}, {2, 1, REL}, {3, 1, Blank}, {4, 1, Blank},
    {2, 0, DMG},   {4, 0, CAP},
};
constexpr EdgeDef kMini2Edges[] = {
    {0, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 4, 0}, {2, 5, 0}, {4, 6, 0},
};

struct Circuit {
  const char *tag;
  float x0, xp, y0, yp;
  const NodeDef *nodes;
  int nodeCount;
  const EdgeDef *edges;
  int edgeCount;
  float typedDia, blankDia, labelDy, labelSize, traceAlpha;
  const char *caption;
  bool bigSocket;

  SkPoint at(int i) const {
    return {x0 + xp * (float)nodes[i].col, y0 + yp * (float)nodes[i].row};
  }
  std::string key(int i) const {
    return std::string(tag) + std::to_string(i);
  }
};

const Circuit kBeam{"b",   236, 110, 164, 78,
                    kBeamNodes,
                    (int)(sizeof(kBeamNodes) / sizeof(kBeamNodes[0])),
                    kBeamEdges,
                    (int)(sizeof(kBeamEdges) / sizeof(kBeamEdges[0])),
                    30, 19, -31, 11.5f, 0.66f, nullptr, true};
const Circuit kRig{"r",   210, 76, 470, 46,
                   kMiniNodes,
                   (int)(sizeof(kMiniNodes) / sizeof(kMiniNodes[0])),
                   kMiniEdges,
                   (int)(sizeof(kMiniEdges) / sizeof(kMiniEdges[0])),
                   19, 12, -22, 9.0f, 0.48f, "R.I.G. — INTEGRITY", false};
const Circuit kStasis{"s", 650, 76, 470, 46,
                      kMini2Nodes,
                      (int)(sizeof(kMini2Nodes) / sizeof(kMini2Nodes[0])),
                      kMini2Edges,
                      (int)(sizeof(kMini2Edges) / sizeof(kMini2Edges[0])),
                      19, 12, -22, 9.0f, 0.48f, "STASIS MODULE", false};

// ---------------------------------------------------------------------------
// legend data (fill counts read off the frame: 5/6, then a thinning
// ladder — the exact ratios are not load-bearing, the geometry is)

struct StatRow { const char *label; Kind kind; int filled, total;
                 const char *value; };
constexpr StatRow kStats[] = {
    {"DAMAGE", DMG, 5, 6, "100 Pts."},
    {"CAPACITY", CAP, 3, 6, "44 Pts."},
    {"RELOAD", REL, 2, 6, "18 Pts."},
    {"CHARGE", CHR, 1, 6, "9 Pts."},
};
constexpr int kStatCount = 4;

} // namespace

// ===========================================================================

struct Ds2Bench : sigil::compose::sketch::Sketch {
  // one bound glow per node — the idle pulse is desynced by index
  std::array<choreograph::Output<float>, 24> glow;
  choreograph::Output<float> socketPulse{1.0f};
  choreograph::Output<float> scanY{0};
  choreograph::Output<float> jitterX{0};
  choreograph::Output<float> holoAlpha{1.0f};

  // the legend pip masses: one atlas (2 cells), one pool per row
  std::shared_ptr<instancing::Atlas> pips =
      std::make_shared<instancing::Atlas>(2.0f);
  std::array<std::shared_ptr<instancing::Pool>, kStatCount> pipPools;
  int pipFilled = 0, pipEmpty = 1;
  int glowSlot = 0;
  // LUMINANCE noise, not fractal RGB — an overlay of the latter hue-shifts
  // the surface it dirties instead of lighting it
  Material grain = grainField(0.9f, 7.0f);

  double nextGlitch = 4.2, glitchEnd = 0;

  // -------------------------------------------------------------------
  // the pip atlas: two chevron cells, tinted per instance

  void bakePips() {
    Element filled =
        box().outline(chevron())
            .fill(Material::linear({0, 0}, {0, kPipH},
                                   {{0.0f, rgb(0xC8DADA)},
                                    {0.42f, rgb(0x92AAAC)},
                                    {0.52f, rgb(0x70898C)},
                                    {1.0f, rgb(0xB0C6C8)}}))
            .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.5f)),
                           PathFormat::Align::Inner));
    Element empty = box().outline(chevron()).stroke(
        stroke(1.2f, Fill::color(alpha(kCyan, 0.30f)),
               PathFormat::Align::Inner));
    pipFilled = pips->cell(std::move(filled), {kPipW, kPipH});
    pipEmpty = pips->cell(std::move(empty), {kPipW, kPipH});

    for (int r = 0; r < kStatCount; ++r) {
      auto pool = std::make_shared<instancing::Pool>();
      // place::repeat IS the Repeater law — linear translate per copy.
      instancing::place::repeat(*pool, (size_t)kStats[r].total,
                                {kPipW * 0.5f, kPipH * 0.5f},
                                {kPipW + kPipGap, 0.0f});
      auto frames = pool->frames();
      auto tints = pool->tints();
      const KindArt art = artOf(kStats[r].kind);
      for (int i = 0; i < kStats[r].total; ++i) {
        const bool on = i < kStats[r].filled;
        frames[i] = on ? pipFilled : pipEmpty;
        // the tint channel carries the row's identity into a shared cell
        tints[i] = on ? SkColor4f{0.66f + 0.34f * art.ring.fR,
                                  0.66f + 0.34f * art.ring.fG,
                                  0.66f + 0.34f * art.ring.fB, 1.0f}
                      : SkColor4f{1, 1, 1, 1};
      }
      pool->touch();
      pipPools[(size_t)r] = std::move(pool);
    }
  }

  // -------------------------------------------------------------------
  // backdrop: the blurred ship interior, implied not modeled

  void backdrop(Element &root) {
    auto strut = [&](float x, float w, float a) {
      root.child(box()
                     .absolute().left(Dim(x)).top(Dim(-40.0f))
                     .width(Dim(w)).height(Dim(kH + 80))
                     .fill(Material::linear(
                         {0, 0}, {0, kH},
                         {{0.0f, rgb(0x16262F, a * 0.35f)},
                          {0.38f, rgb(0x1C303C, a)},
                          {1.0f, rgb(0x080F16, a * 0.2f)}}))
                     .effect(Effect::filter(
                         SkImageFilters::Blur(12, 18, nullptr)))
                     .zIndex(0));
    };
    strut(-30, 96, 0.8f);
    strut(66, 30, 0.45f);
    strut(1108, 92, 0.8f);
    strut(1040, 26, 0.4f);
    root.child(box()
                   .absolute().left(Dim(-40.0f)).top(Dim(2.0f))
                   .width(Dim(kW + 80)).height(Dim(28.0f))
                   .fill(Material::linear({0, 0}, {0, 28},
                                          {{0.0f, rgb(0x243B47, 0.5f)},
                                           {1.0f, rgb(0x0A141C, 0.25f)}}))
                   .effect(Effect::filter(SkImageFilters::Blur(7, 10, nullptr)))
                   .zIndex(0));
  }

  // -------------------------------------------------------------------
  // the panel plate + its frame vocabulary

  void plate(Element &root) {
    // body: solid + a radial lift + the live scanline field, ONE shader
    root.child(
        box()
            .key("plate")
            .absolute().left(Dim(kPX)).top(Dim(kPY))
            .width(Dim(kPW)).height(Dim(kPH))
            .outline(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
            .fill(Material::blend(
                {{Material::solid(kBody), SkBlendMode::kSrcOver},
                 // unit-square ramp: the lift is authored against the box,
                 // not against a pixel extent transcribed by hand
                 {Material::radialUnit({0.40f, 0.32f}, 1.15f,
                                       {{0.0f, rgb(0xFFFFFF)},
                                        {0.5f, rgb(0xC0D0D0)},
                                        {1.0f, rgb(0x4E6264)}}),
                  SkBlendMode::kMultiply},
                 {scanField(alpha(kCyan, 0.075f), 3.0f),
                  SkBlendMode::kScreen}}))
            .zIndex(1));

    // the grain the compressed CRT capture carries — enough to kill the
    // "clean vector art" read without becoming VHS noise
    root.child(box()
                   .absolute().left(Dim(kPX)).top(Dim(kPY))
                   .width(Dim(kPW)).height(Dim(kPH))
                   .outline(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
                   .fill(grain)
                   .opacity(0.07f)
                   .blend(SkBlendMode::kOverlay)
                   .zIndex(2));

    // frame: a soft plus-blended halo under a crisp cyan keyline
    root.child(box()
                   .absolute().left(Dim(kPX)).top(Dim(kPY))
                   .width(Dim(kPW)).height(Dim(kPH))
                   .outline(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
                   .stroke(LayeredBrush{{
                       {14, alpha(kCyan, 0.09f), 8, {}, 0, SkBlendMode::kPlus},
                       {5, alpha(kCyan, 0.22f), 2.6f, {}, 0,
                        SkBlendMode::kPlus},
                       {2.4f, alpha(rgb(0xCFF2F5), 0.95f)},
                   }})
                   .zIndex(6));

    // the inner contour: a thin line whose dipped centre IS the header rule
    root.child(box()
                   .absolute().left(Dim(kPX + kInset)).top(Dim(kPY + kInset))
                   .width(Dim(kPW - 2 * kInset)).height(Dim(kPH - 2 * kInset))
                   .outline(panelInner(kInnerCut, kInnerDip, kInnerShoulderL,
                                       kInnerShoulderR))
                   .stroke(stroke(1.1f, Fill::color(alpha(kCyan, 0.55f))))
                   .zIndex(6));
    // and a dotted echo just inside it (the frame's second, ticked pass)
    root.child(box()
                   .absolute().left(Dim(kPX + kInset + 7))
                   .top(Dim(kPY + kInset + 7))
                   .width(Dim(kPW - 2 * kInset - 14))
                   .height(Dim(kPH - 2 * kInset - 14))
                   .outline(panelInner(kInnerCut - 4, kInnerDip - 7,
                                       kInnerShoulderL - 7,
                                       kInnerShoulderR - 7))
                   .stroke(PathFormat{.width = 1.0f,
                                      .strokeFill =
                                          Fill::color(alpha(kCyan, 0.26f)),
                                      .dashIntervals = {2.0f, 6.0f}})
                   .zIndex(6));
  }

  // -------------------------------------------------------------------
  // header

  void header(Element &root) {
    root.child(box()
                   .absolute().left(Dim(kPX)).top(Dim(kPY + 20))
                   .width(Dim(kPW)).height(Dim(kRuleY - kPY - 22))
                   .alignItems(Align::Center).justify(Justify::Center)
                   .zIndex(7)
                   .child(text(toU8("CONTACT BEAM"), type(31, kTitle, 0.10f))
                              .key("title")
                              .glyphFx(GlyphFx{
                                  .effect = glyphfx::typeOn(),
                                  .stagger = {.eachMs = 26,
                                              .durationMs = 190},
                                  .progress = withFrom(0.0f, 1.0f, {760ms})})
                              .effect(styles::textGlow(alpha(kCyan, 0.5f),
                                                       5.0f))));

    // the shoulder marks ride the frame's lower tier, left and right of the
    // raised centre — the only flat band the silhouette leaves free
    root.child(box().absolute().left(Dim(kPX + 30)).top(Dim(kPY + 30))
                   .zIndex(7)
                   .child(text(toU8("NANOCIRCUIT"),
                               type(9, alpha(kCyan, 0.5f), 0.2f, false))));
    root.child(box().absolute().right(Dim(kW - kPR + 30))
                   .top(Dim(kPY + 30)).zIndex(7)
                   .child(text(toU8("LINK · OK"),
                               type(9, alpha(kCyan, 0.5f), 0.2f, false))));

    // under the rule: the repair caption at left, and at right the RIG's
    // integrity as an ANNULAR GAUGE — shapes::sector is a closed wedge, so
    // the track and the fill are the same generator twice
    root.child(box().absolute().left(Dim(kPX + 34)).top(Dim(kRuleY + 13))
                   .zIndex(7)
                   .child(text(toU8("NANOCIRCUIT REPAIR · TIER III"),
                               type(10.5f, alpha(kCyan, 0.5f), 0.2f, false))));
    const float gaugeD = 26, gaugeX = 786, gaugeY = kRuleY + 6;
    root.child(box().absolute().left(Dim(gaugeX)).top(Dim(gaugeY))
                   .width(Dim(gaugeD)).height(Dim(gaugeD))
                   .outline(shapes::sector(0, 359.99f, 0.58f))
                   .fill(Material::solid(alpha(kCyan, 0.18f)))
                   .zIndex(7));
    root.child(box().absolute().left(Dim(gaugeX)).top(Dim(gaugeY))
                   .width(Dim(gaugeD)).height(Dim(gaugeD))
                   .outline(shapes::sector(-90, 360 * 0.78f, 0.58f))
                   .fill(Material::solid(alpha(kCyan, 0.9f)))
                   .zIndex(7));
    root.child(box().absolute().left(Dim(gaugeX + 34)).top(Dim(kRuleY + 13))
                   .zIndex(7)
                   .child(text(toU8("R.I.G. INTEGRITY 78%"),
                               type(10.5f, alpha(kCyan, 0.5f), 0.2f, false))));
  }

  // -------------------------------------------------------------------
  // the entry socket: the bracket-and-arrow the current flows in through

  void entrySocket(Element &root, SkPoint at, bool big) {
    if (!big) {
      root.child(custom([](SkCanvas &canvas, const PaintContext &ctx) {
                   const float w = ctx.size.width(), h = ctx.size.height();
                   SkPaint p;
                   p.setAntiAlias(true);
                   p.setColor4f(alpha(kCyan, 0.8f), nullptr);
                   SkPathBuilder t;
                   t.moveTo(0, h * 0.16f);
                   t.lineTo(w * 0.8f, h * 0.5f);
                   t.lineTo(0, h * 0.84f);
                   t.close();
                   canvas.drawPath(t.detach(), p);
                 })
                     .absolute()
                     .left(Dim(at.fX - 24)).top(Dim(at.fY - 9))
                     .width(Dim(16.0f)).height(Dim(18.0f))
                     .opacity(&socketPulse)
                     .zIndex(8));
      return;
    }
    root.child(
        custom([](SkCanvas &canvas, const PaintContext &ctx) {
          const float w = ctx.size.width(), h = ctx.size.height();
          SkPaint p;
          p.setAntiAlias(true);
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeWidth(1.6f);
          p.setColor4f(alpha(kCyan, 0.78f), nullptr);
          // the socket housing: a rectangle broken on the left, where the
          // feed enters
          SkPathBuilder b;
          b.moveTo(w * 0.30f, h * 0.34f);
          b.lineTo(w * 0.30f, h * 0.06f);
          b.lineTo(w * 0.99f, h * 0.06f);
          b.lineTo(w * 0.99f, h * 0.94f);
          b.lineTo(w * 0.30f, h * 0.94f);
          b.lineTo(w * 0.30f, h * 0.66f);
          // the inner bracket
          b.moveTo(w * 0.58f, h * 0.26f);
          b.lineTo(w * 0.44f, h * 0.26f);
          b.lineTo(w * 0.44f, h * 0.74f);
          b.lineTo(w * 0.58f, h * 0.74f);
          canvas.drawPath(b.detach(), p);
          SkPathBuilder t;
          t.moveTo(w * 0.02f, h * 0.31f);
          t.lineTo(w * 0.24f, h * 0.50f);
          t.lineTo(w * 0.02f, h * 0.69f);
          t.close();
          p.setStyle(SkPaint::kFill_Style);
          canvas.drawPath(t.detach(), p);
        })
            .absolute()
            .left(Dim(at.fX - 100)).top(Dim(at.fY - 46))
            .width(Dim(108.0f)).height(Dim(92.0f))
            .opacity(&socketPulse)
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // one circuit: traces first (so node glow sits on the wire), then nodes

  void circuit(Element &root, const Circuit &c) {
    auto wires = box().absolute().inset(0).zIndex(4)
                     .staggerChildren(30ms, Stagger::From::Start);
    for (int i = 0; i < c.edgeCount; ++i) {
      const EdgeDef &e = c.edges[i];
      wires.child(
          connector(c.key(e.a), c.key(e.b), pcb(9.0f, e.jog))
              .key(std::string(c.tag) + "e" + std::to_string(i))
              .absolute().inset(0)
              .trim(0.0f, withFrom(0.0f, 1.0f, {620ms}))
              .stroke(LayeredBrush{{{7.0f, alpha(kCyan, 0.075f), 3.4f, {}, 0,
                                     SkBlendMode::kPlus}}})
              .stroke(lines::cased(1.2f,
                                   Fill::color(alpha(kCyan, c.traceAlpha)),
                                   c.typedDia > 24 ? 4.2f : 3.4f)));
    }
    root.child(std::move(wires));

    auto layer = box().absolute().inset(0).zIndex(5)
                     .staggerChildren(30ms, Stagger::From::Start);
    for (int i = 0; i < c.nodeCount; ++i) {
      const SkPoint at = c.at(i);
      const KindArt art = artOf(c.nodes[i].kind);
      const bool typed = c.nodes[i].kind != Blank;
      const float dia = typed ? c.typedDia : c.blankDia;

      const sdf::Style st{
          .fill = art.fill,
          .borderWidth = typed ? 2.4f : 1.7f,
          .borderColor = art.ring,
          .glowRadius = typed ? 6.0f : 3.6f,
          .glowColor = alpha(kCyan, typed ? 0.38f : 0.24f),
          .shadowOffset = {0, 0},
          .shadowBlur = typed ? 8.0f : 5.0f,
          .shadowColor = rgb(0x01080A, typed ? 0.95f : 0.8f)};
      Material m = sdf::material(sdf::circle(), st);
      m.uniform("uGlowR", &glow[(size_t)(glowSlot++ % (int)glow.size())]);

      const float boxSize = sdf::minBoxFor(st, dia);
      layer.child(box()
                      .key(c.key(i))
                      .width(Dim(boxSize)).height(Dim(boxSize))
                      .centerAt(at)
                      .fill(std::move(m))
                      .opacity(withFrom(0.0f, 1.0f, {260ms}))
                      .scale(withFrom(
                          0.72f, 1.0f,
                          Transition{.duration = 260ms,
                                     .ease = [](float t) {
                                       return choreograph::easeOutBack(t);
                                     }}))
                      .zIndex(typed ? 3 : 2));

      if (!typed)
        continue;
      // the speckled corona + the type label, both keyed leaves: the
      // instancing atlas has no per-instance string, so labels stay text
      layer.child(box()
                      .width(Dim(dia + 24)).height(Dim(dia + 24))
                      .centerAt(at)
                      .outline(burst(24, 0.72f))
                      .stroke(stroke(0.9f, Fill::color(alpha(kCyan, 0.20f))))
                      .opacity(withFrom(0.0f, 1.0f, {320ms}))
                      .zIndex(4));
      layer.child(box()
                      .width(Dim(dia * 0.42f)).height(Dim(dia * 0.42f))
                      .centerAt({at.fX - dia * 0.09f, at.fY - dia * 0.10f})
                      .fill(Material::radial(
                          {dia * 0.21f, dia * 0.21f}, dia * 0.28f,
                          {{0.0f, alpha(art.ring, 0.42f)},
                           {1.0f, alpha(art.ring, 0.0f)}}))
                      .zIndex(5));
      layer.child(text(toU8(art.label),
                       type(c.labelSize, alpha(kCyan, 0.85f), 0.11f))
                      .centerAt({at.fX + dia * 0.88f, at.fY + c.labelDy})
                      .opacity(withFrom(0.0f, 1.0f, {320ms}))
                      .zIndex(5));
    }
    root.child(std::move(layer));

    entrySocket(root, c.at(0), c.bigSocket);

    if (!c.caption)
      return;
    int typedCount = 0;
    for (int i = 0; i < c.nodeCount; ++i)
      typedCount += c.nodes[i].kind != Blank;
    char slots[24];
    std::snprintf(slots, sizeof(slots), "%d / %d NODES", typedCount,
                  c.nodeCount);
    root.child(box().absolute().left(Dim(c.x0 - 34)).top(Dim(c.y0 - 58))
                   .row().alignItems(Align::Center).gap(14).zIndex(8)
                   .child(text(toU8(c.caption),
                               type(11, alpha(kCyan, 0.62f), 0.18f)))
                   .child(text(toU8(slots),
                               type(9.5f, alpha(kCyan, 0.4f), 0.18f, false))));
    root.child(box().absolute().left(Dim(c.x0 - 34))
                   .top(Dim(c.y0 - 32)).width(Dim(280.0f))
                   .height(Dim(1.0f))
                   .outline(hline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.28f))))
                   .zIndex(8));
  }

  // -------------------------------------------------------------------
  // legend: bracket-framed stat rows with instanced chevron pips

  Element statRow(int r) {
    const StatRow &s = kStats[r];
    const KindArt art = artOf(s.kind);
    const float barW = (float)s.total * kPipW + (float)(s.total - 1) * kPipGap;
    return box()
        .row().alignItems(Align::Center).height(Dim(24.0f))
        .child(box().width(Dim(160.0f)).alignItems(Align::End)
                   .child(text(toU8(s.label),
                               type(14, alpha(kCyan, 0.95f), 0.10f))))
        .child(box().width(Dim(9.0f)).height(Dim(9.0f)).margin(13, 0, 13, 0)
                   .outline(shapes::polygon(12))
                   .fill(Material::radial({4.5f, 4.5f}, 5.0f,
                                          {{0.0f, art.ring},
                                           {1.0f, art.fill}})))
        .child(box().width(Dim(barW)).height(Dim(kPipH))
                   .opacity(withFrom(0.0f, 1.0f, {320ms}))
                   .translateX(withFrom(-16.0f, 0.0f, {380ms}))
                   .child(instancing::instances(pips, pipPools[(size_t)r])))
        .child(box().grow(1))
        .child(box().width(Dim(84.0f))
                   .child(text(toU8(s.value),
                               type(13, rgb(0xDCEEF2), 0.02f, false))));
  }

  void legend(Element &root) {
    auto card =
        box().key("legend")
            .absolute().left(Dim(kLegX)).top(Dim(kBandY))
            .width(Dim(kLegW)).height(Dim(kBandH))
            .fill(Material::blend(
                {{Material::solid(alpha(kStrip, 0.6f)), SkBlendMode::kSrcOver},
                 {scanField(alpha(kCyan, 0.05f), 3.0f),
                  SkBlendMode::kScreen}}))
            .outline(chamfer(12))
            .zIndex(7)
            .column().padding(20, 12).gap(3)
            .staggerChildren(70ms, Stagger::From::Start);

    card.child(box().row().height(Dim(14.0f))
                   .child(box().width(Dim(160.0f)).alignItems(Align::End)
                              .child(text(toU8("SPECIFICATION"),
                                          type(9, alpha(kCyan, 0.42f),
                                               0.22f, false))))
                   .child(box().width(Dim(35.0f)))
                   .child(text(toU8("NANOCIRCUIT LOAD"),
                               type(9, alpha(kCyan, 0.42f), 0.22f, false)))
                   .child(box().grow(1))
                   .child(box().width(Dim(84.0f))
                              .child(text(toU8("VALUE"),
                                          type(9, alpha(kCyan, 0.42f), 0.22f,
                                               false)))));
    for (int r = 0; r < kStatCount; ++r)
      card.child(statRow(r));
    root.child(std::move(card));

    root.child(box().absolute().left(Dim(kLegX - 8)).top(Dim(kBandY - 8))
                   .width(Dim(kLegW + 16)).height(Dim(kBandH + 16))
                   .outline(cornerBrackets(26))
                   .stroke(stroke(1.5f, Fill::color(alpha(kCyan, 0.72f))))
                   .zIndex(8));
    root.child(box().absolute().left(Dim(kLegX + 16)).top(Dim(kBandY + 32))
                   .width(Dim(kLegW - 32)).height(Dim(1.0f))
                   .outline(hline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.26f))))
                   .zIndex(8));
    root.child(box().absolute().left(Dim(kLegX + kLegW - 108))
                   .top(Dim(kBandY + 14)).width(Dim(1.0f))
                   .height(Dim(kBandH - 28))
                   .outline(vline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.26f))))
                   .zIndex(8));
  }

  // -------------------------------------------------------------------
  // the NODES counter — the one warm-metal object on an all-cyan screen

  void counter(Element &root) {
    root.child(
        box().key("counter")
            .absolute().left(Dim(kCntX)).top(Dim(kBandY))
            .width(Dim(kCntW)).height(Dim(kBandH))
            .outline(chamfer(12))
            .fill(Material::blend(
                {{Material::solid(alpha(kStrip, 0.6f)), SkBlendMode::kSrcOver},
                 {scanField(alpha(kCyan, 0.05f), 3.0f),
                  SkBlendMode::kScreen}}))
            .column().alignItems(Align::Center).padding(16, 11).gap(2)
            .zIndex(7)
            .child(box().width(Dim(112.0f)).height(Dim(21.0f))
                       .alignItems(Align::Center).justify(Justify::Center)
                       .outline(chamfer(6))
                       .stroke(stroke(1.0f,
                                      Fill::color(alpha(kCyan, 0.45f))))
                       .child(text(toU8("NODES"),
                                   type(12, alpha(kCyan, 0.95f), 0.16f))))
            // the brass power-node puck: side wall, top face, bore ring
            .child(box().width(Dim(66.0f)).height(Dim(46.0f)).margin(0, 6, 0, 0)
                       .child(box().absolute().left(Dim(2.0f)).top(Dim(14.0f))
                                  .width(Dim(62.0f)).height(Dim(28.0f))
                                  .corners({14})
                                  .fill(Material::linear(
                                      {0, 0}, {0, 28},
                                      {{0.0f, kBrassLo},
                                       {0.45f, rgb(0x7E6318)},
                                       {1.0f, kBrassDk}})))
                       .child(box().absolute().left(Dim(2.0f)).top(Dim(2.0f))
                                  .width(Dim(62.0f)).height(Dim(27.0f))
                                  .outline(shapes::squircle(2.0f))
                                  .fill(Material::linear(
                                      {0, 0}, {52, 27},
                                      {{0.0f, kBrassHi},
                                       {0.4f, rgb(0xD3AA33)},
                                       {1.0f, rgb(0x8E6F1E)}}))
                                  .stroke(stroke(
                                      1.0f,
                                      Fill::color(rgb(0xF3DC94, 0.75f)))))
                       .child(box().absolute().left(Dim(22.0f))
                                  .top(Dim(8.0f)).width(Dim(24.0f))
                                  .height(Dim(13.0f))
                                  .outline(shapes::squircle(2.0f))
                                  .stroke(stroke(
                                      1.3f,
                                      Fill::color(rgb(0x74590F, 0.9f))))))
            .child(text(toU8("2"), type(40, kTitle, 0.0f))
                       .key("nodecount")
                       .transition({.duration = 200ms})));

    root.child(box().absolute().left(Dim(kCntX - 8)).top(Dim(kBandY - 8))
                   .width(Dim(kCntW + 16)).height(Dim(kBandH + 16))
                   .outline(cornerBrackets(22))
                   .stroke(stroke(1.5f, Fill::color(alpha(kCyan, 0.72f))))
                   .zIndex(8));
  }

  // -------------------------------------------------------------------
  // the hardware hint row along the panel's bottom rail

  void hints(Element &root) {
    root.child(box().absolute().left(Dim(kPX + 32)).top(Dim(kHintY))
                   .width(Dim(kPW - 64)).height(Dim(1.0f))
                   .outline(hline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.36f))))
                   .zIndex(8));

    auto hint = [&](std::string label) {
      return text(toU8(label), type(12, alpha(kCyan, 0.78f), 0.06f, false));
    };

    root.child(
        box().absolute().left(Dim(kPX)).top(Dim(kHintY + 8))
            .width(Dim(kPW)).height(Dim(26.0f))
            .row().alignItems(Align::Center).justify(Justify::Center).gap(56)
            .zIndex(8)
            .child(box().row().alignItems(Align::Center).gap(8)
                       .child(custom([](SkCanvas &canvas,
                                        const PaintContext &ctx) {
                                const float r = ctx.size.width() * 0.5f;
                                SkPaint p;
                                p.setAntiAlias(true);
                                p.setStyle(SkPaint::kStroke_Style);
                                p.setStrokeWidth(1.3f);
                                p.setColor4f(alpha(kCyan, 0.82f), nullptr);
                                canvas.drawCircle(r, r, r - 1.1f, p);
                                const float k =
                                    0.5f +
                                    0.5f * std::sin(
                                        (float)ctx.elapsedSeconds * 3.4f);
                                p.setStyle(SkPaint::kFill_Style);
                                p.setColor4f(alpha(kCyan, 0.3f + 0.5f * k),
                                             nullptr);
                                canvas.drawCircle(r, r, r * 0.4f, p);
                                p.setStyle(SkPaint::kStroke_Style);
                                for (int i = 0; i < 4; ++i) {
                                  const float a = 1.5707963f * (float)i;
                                  const float c = std::cos(a),
                                              s = std::sin(a);
                                  canvas.drawLine(r + c * r * 0.6f,
                                                  r + s * r * 0.6f,
                                                  r + c * r * 0.9f,
                                                  r + s * r * 0.9f, p);
                                }
                              })
                                  .width(Dim(15.0f)).height(Dim(15.0f))
                                  .cache(Cache::None))
                       .child(hint("Navigate")))
            .child(hint("[Enter] Select"))
            .child(hint("[Esc] Exit")));

    // the empty hardware sockets the bezel carries at its bottom corners
    for (float x : {kPX + 34, kPR - 46}) {
      root.child(box().absolute().left(Dim(x)).top(Dim(kHintY + 13))
                     .width(Dim(12.0f)).height(Dim(12.0f))
                     .stroke(stroke(1.2f, Fill::color(alpha(kCyan, 0.45f))))
                     .zIndex(8));
    }
  }

  // -------------------------------------------------------------------
  // the CRT overlay: the refresh bar that warps what it passes over

  void overlay(Element &root) {
    root.child(box()
                   .absolute().left(Dim(kPX + 6)).top(Dim(kPY + 30))
                   .width(Dim(kPW - 12)).height(Dim(34.0f))
                   .translateY(&scanY)
                   .backdrop(styles::ripple(1.6f, 78.0f, 0.0f))
                   .fill(Material::linear({0, 0}, {0, 34},
                                          {{0.0f, alpha(kCyan, 0.0f)},
                                           {0.5f, alpha(kCyan, 0.05f)},
                                           {1.0f, alpha(kCyan, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::None)
                   .zIndex(9));
    root.child(box().absolute().inset(0)
                   .fill(Material::radial({kW * 0.5f, kH * 0.46f}, kW * 0.60f,
                                          {{0.0f, rgb(0x000000, 0.0f)},
                                           {0.55f, rgb(0x000000, 0.14f)},
                                           {1.0f, rgb(0x01050A, 0.86f)}}))
                   .zIndex(11));
  }

  // -------------------------------------------------------------------

  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    glowSlot = 0;
    auto root = stack().fill(Material::radial(
        {kW * 0.5f, kH * 0.5f}, 880,
        {{0.0f, rgb(0x09131B)}, {0.6f, rgb(0x050B11)},
         {1.0f, rgb(0x020406)}}));
    backdrop(root);

    // everything that belongs to the hologram rides one jittering group,
    // so a glitch reads as "the whole panel stuttered"
    auto holo = box().absolute().inset(0)
                    .translateX(&jitterX)
                    .opacity(&holoAlpha)
                    .scale(withFrom(0.955f, 1.0f, {380ms}))
                    .transformOriginPx({kW * 0.5f, kH * 0.5f})
                    .zIndex(2);
    plate(holo);
    header(holo);
    circuit(holo, kBeam);
    circuit(holo, kRig);
    circuit(holo, kStasis);
    legend(holo);
    counter(holo);
    hints(holo);
    overlay(holo);
    root.child(std::move(holo));
    return root;
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas((int)kW, (int)kH);
    ctx.background(rgb(0x02060A));
    bakePips();

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      for (size_t i = 0; i < glow.size(); ++i)
        glow[i] = 6.0f + 2.1f * (float)std::sin(t * 2.75 + (double)i * 0.62);
      socketPulse = 0.78f + 0.22f * (float)std::sin(t * 3.9);
      scanY = (float)std::fmod(t * 96.0, (double)(kPH - 40));

      // the hologram stutter: ~100 ms of snap (not eased) jitter every
      // 4-7 s, scheduled deterministically so a capture is reproducible
      if (t >= nextGlitch && t > glitchEnd) {
        glitchEnd = t + 0.10;
        nextGlitch = t + 4.0 + std::fmod(t * 7.31, 3.0);
      }
      if (t < glitchEnd) {
        const int step = (int)((glitchEnd - t) * 30.0) % 3;
        jitterX = step == 0 ? 3.0f : step == 1 ? -2.0f : 1.0f;
        holoAlpha = 0.76f;
      } else {
        jitterX = 0.0f;
        holoAlpha = 1.0f;
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Ds2Bench)
