#pragma once

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace arrange = sigil::geometry::arrange;
namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace path = sigil::geometry::path;
namespace sdf = sigil::material::sdf;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::material::skia::Effect;
using sigil::material::skia::Paint;
using sigil::material::skia::toColor;

namespace {

// ---------------------------------------------------------------------------
// palette

using sigil::compose::hexColor;  // 0xRRGGBB -> SkColor4f

const SkColor4f kBody = hexColor(0x0B1E21);
const SkColor4f kStrip = hexColor(0x102A2A);
const SkColor4f kCyan = hexColor(0x8FE0E6);
const SkColor4f kTitle = hexColor(0xE4F7F8);
const SkColor4f kDim = hexColor(0x33514E);
const SkColor4f kBrassLo = hexColor(0xC9A227);
const SkColor4f kBrassHi = hexColor(0xE8C860);
const SkColor4f kBrassDk = hexColor(0x5E4914);

// ---------------------------------------------------------------------------
// canvas geometry (1200 x 800)

constexpr float kW = 1200, kH = 800;
constexpr float kPX = 108, kPY = 52, kPW = 984, kPH = 690;
constexpr float kPR = kPX + kPW, kPB = kPY + kPH;
constexpr float kOuterCut = 26, kOuterStep = 22, kOuterShoulder = 132;
constexpr float kInset = 12, kInnerCut = 18, kInnerDip = 40;
constexpr float kInnerShoulderL = 150, kInnerShoulderR = 46;
constexpr float kRuleY = kPY + kInset + kInnerDip;  // the header rule, 104
constexpr float kBandY = 544, kBandH = 148;
constexpr float kLegX = 140, kLegW = 690;
constexpr float kCntX = 844, kCntW = 216;
constexpr float kHintY = 706;
constexpr float kPipW = 36, kPipH = 17, kPipGap = 6;

// ---------------------------------------------------------------------------
// type

inline sk_sp<SkTypeface> uiFace(bool bold) {
  const SkFontStyle want = bold ? SkFontStyle::Bold()
                                : SkFontStyle(SkFontStyle::kMedium_Weight,
                                              SkFontStyle::kNormal_Width,
                                              SkFontStyle::kUpright_Slant);
  // Eurostile Extended lineage; DIN Alternate is the closest squared-off
  // technical grotesque macOS ships, stretched the last of the way.
  return weave::ports::face(
      {"Eurostile", "Bank Gothic", "DIN Alternate", "Helvetica Neue", "Arial"},
      want);
}

// The bench's one register, over weave's designated-init `textStyle()`.
// Tracking arrives here in EM, not px, because the reference quotes it that
// way; the em size is known at this call, so the conversion lands here.
inline weave::TextStyle benchType(float size, SkColor4f color,
                                  float trackEm = 0.07f, bool bold = true,
                                  float stretch = 1.16f) {
  // The port holds the face, so asking it per style hands back the one
  // pointer every style and every memo below compares against — where a
  // static here would hold it in a dylib that is unloaded on reload.
  return weave::textStyle({.face = uiFace(bold),
                           .size = size,
                           .color = color,
                           .track = trackEm * size,
                           .condense = stretch});
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
    b.moveTo(arm, 0);
    b.lineTo(0, 0);
    b.lineTo(0, arm);
    b.moveTo(w - arm, 0);
    b.lineTo(w, 0);
    b.lineTo(w, arm);
    b.moveTo(w, h - arm);
    b.lineTo(w, h);
    b.lineTo(w - arm, h);
    b.moveTo(arm, h);
    b.lineTo(0, h);
    b.lineTo(0, h - arm);
    return b.detach();
  };
}

/** The node corona: short ragged radial ticks — the cheap analogue of the
 *  screenshot's speckled burst around every typed node. */
inline std::function<SkPath(SkSize)> burst(int count, float inner) {
  // A division ladder with THREE length classes, which is why kit::Ticks
  // carries a `classify` escape hatch at all: no long/short pair says
  // 1.0 / 0.86 / 0.93. The 0.13 rad kick is the frame's own origin.
  return shapes::ticks(
      {.divisions = count,
       .mark = {inner, 1.0f},
       .classify =
           [](int i, shapes::Span sp) {
             sp.outer = (i % 3 == 0) ? 1.00f : (i % 3 == 1) ? 0.86f : 0.93f;
             return sp;
           }},
      {.zero = path::Zero::East,
       .sense = path::Sense::CW,
       .originDeg = 0.13f * 180.0f / 3.14159265f});
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
// routers::orthogonal(Bend, cornerRadius, chamferCut) answers two of the
// three things this trace needs — a bend that is not always midX, and a
// 45 deg cut corner — and its own header says the zero-argument spelling
// is the frozen one whose degenerate verbs make a parallel-offset brush
// (lines::cased) spike at both endpoints. What is genuinely not there is
// the 45 deg Z-JOG for long horizontals, the stepped trace the screenshot
// is full of, and a router is a value, so the whole one is written here
// rather than composed from a half of it.

inline Router pcb(float cut, float jog) {
  return [cut, jog](const SkRect& from, const SkRect& to) {
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

/** THE 6 Hz STEP, PHASE-SHIFTED HALF A STEP. `quantizeTime(6)` puts a
 *  boundary at every whole sixth of a second, and a host that captures at
 *  a whole number of seconds captures exactly on one: the clock a plate
 *  reaches is a sum of steps of 1/60 rather than the exact number, so a
 *  float tie-break decides which side of the boundary the scanlines land
 *  on and every line on the panel moves 1.17 px between two runs of the
 *  same code. Quantising `t + 1/12` moves every boundary a half step off
 *  the whole seconds, which is where every host in this repository
 *  captures. The look is identical: it is the same ladder, read half a
 *  rung along. */
inline float steppedTime(double t) {
  return (float)(std::floor((t + 1.0 / 12.0) * 6.0) / 6.0);
}

inline Paint scanField(SkColor4f tint, float period,
                       const choreograph::Output<float>* clock) {
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
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
  if (!fx) {
    SkDebugf("ds2 scanField: %s\n", err.c_str());
    return Paint::solid({0, 0, 0, 0});
  }
  return Paint::sksl(fx, {{"uPeriod", period}})
      .uniform("uColor", tint)
      .uniform("uTime", clock);
}

// ---------------------------------------------------------------------------
// circuits

enum Kind : int { Blank = 0, DMG, CAP, CHR, REL };

struct NodeDef {
  int col, row;
  Kind kind;
};
struct EdgeDef {
  int a, b;
  float jog;
};

struct KindArt {
  SkColor4f fill, ring;
  const char* label;
};
inline KindArt artOf(Kind k) {
  switch (k) {
    case DMG:
      return {hexColor(0x6E332F), hexColor(0xE9BCB4), "DMG"};
    case CAP:
      return {hexColor(0x274963), hexColor(0xB2D6EC), "CAP"};
    case CHR:
      return {hexColor(0x4C5E2B), hexColor(0xD6E8AA), "CHR"};
    case REL:
      return {hexColor(0x563F1D), hexColor(0xE2C088), "REL"};
    case Blank:
    default:
      return {hexColor(0x0A1B1E), mskia::withAlpha(kCyan, 0.88f), nullptr};
  }
}

// -- the CONTACT BEAM circuit, transcribed node-for-node from the frame --
constexpr NodeDef kBeamNodes[] = {
    // row 0 — the upper branch caps
    {1, 0, Blank},
    {2, 0, DMG},
    {4, 0, CAP},
    {7, 0, DMG},
    // row 1 — the trunk
    {0, 1, Blank},
    {1, 1, Blank},
    {2, 1, CAP},
    {3, 1, Blank},
    {4, 1, Blank},
    {5, 1, Blank},
    {6, 1, Blank},
    {7, 1, Blank},
    // row 2 — the lower branches
    {1, 2, CHR},
    {2, 2, Blank},
    {3, 2, REL},
    {5, 2, Blank},
    {6, 2, CHR},
    {7, 2, CAP},
    // row 3 — the second lower rank
    {2, 3, Blank},
    {3, 3, Blank},
    {5, 3, DMG},
};
// Every edge is an independent two-endpoint route sharing anchor KEYS —
// a degree-3 blank node is a T-junction because three routes END on it,
// not because any primitive knows the word "junction".
constexpr EdgeDef kBeamEdges[] = {
    {4, 5, 0},   {5, 6, 0},   {6, 7, 0},   {7, 8, 0},  // trunk, left to right
    {8, 9, 0},   {9, 10, 0},  {10, 11, 0}, {5, 0, 0},
    {0, 1, 0},   {8, 2, 0},   {11, 3, 0},               // upper taps
    {6, 13, 0},  {13, 12, 0}, {13, 18, 0}, {7, 14, 0},  // lower taps
    {14, 19, 0}, {9, 15, 0},  {15, 16, 0}, {10, 16, 0},
    {11, 17, 0}, {15, 20, 0}, {18, 19, 0}, {19, 20, 0},  // second lower rank
};

// -- the two smaller Bench trees: the RIG and the Stasis Module --
constexpr NodeDef kMiniNodes[] = {
    {0, 1, Blank}, {1, 1, Blank}, {2, 1, CAP}, {3, 1, Blank},
    {4, 1, Blank}, {1, 0, CHR},   {3, 0, DMG},
};
constexpr EdgeDef kMiniEdges[] = {
    {0, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 4, 0}, {1, 5, 0}, {3, 6, 0},
};
constexpr NodeDef kMini2Nodes[] = {
    {0, 1, Blank}, {1, 1, Blank}, {2, 1, REL}, {3, 1, Blank},
    {4, 1, Blank}, {2, 0, DMG},   {4, 0, CAP},
};
constexpr EdgeDef kMini2Edges[] = {
    {0, 1, 0}, {1, 2, 0}, {2, 3, 0}, {3, 4, 0}, {2, 5, 0}, {4, 6, 0},
};

struct Circuit {
  const char* tag;
  float x0, xp, y0, yp;
  const NodeDef* nodes;
  int nodeCount;
  const EdgeDef* edges;
  int edgeCount;
  float typedDia, blankDia, labelDy, labelSize, traceAlpha;
  const char* caption;
  bool bigSocket;
  int entryIndex;  // the node the feed socket wraps (the trunk's head)

  SkPoint at(int i) const {
    return {x0 + xp * (float)nodes[i].col, y0 + yp * (float)nodes[i].row};
  }
  std::string key(int i) const { return std::string(tag) + std::to_string(i); }
};

const Circuit kBeam{"b",
                    236,
                    110,
                    164,
                    78,
                    kBeamNodes,
                    (int)(sizeof(kBeamNodes) / sizeof(kBeamNodes[0])),
                    kBeamEdges,
                    (int)(sizeof(kBeamEdges) / sizeof(kBeamEdges[0])),
                    28,
                    21,
                    -30,
                    11.5f,
                    0.66f,
                    nullptr,
                    true,
                    4};
const Circuit kRig{"r",
                   210,
                   76,
                   470,
                   46,
                   kMiniNodes,
                   (int)(sizeof(kMiniNodes) / sizeof(kMiniNodes[0])),
                   kMiniEdges,
                   (int)(sizeof(kMiniEdges) / sizeof(kMiniEdges[0])),
                   19,
                   12,
                   -22,
                   9.0f,
                   0.48f,
                   "R.I.G. — ARMOR PLATE",
                   false,
                   0};
const Circuit kStasis{"s",
                      650,
                      76,
                      470,
                      46,
                      kMini2Nodes,
                      (int)(sizeof(kMini2Nodes) / sizeof(kMini2Nodes[0])),
                      kMini2Edges,
                      (int)(sizeof(kMini2Edges) / sizeof(kMini2Edges[0])),
                      19,
                      12,
                      -22,
                      9.0f,
                      0.48f,
                      "STASIS MODULE",
                      false,
                      0};

// ---------------------------------------------------------------------------
// legend data (fill counts read off the frame: 5/6, then a thinning
// ladder — the exact ratios are not load-bearing, the geometry is)

struct StatRow {
  const char* label;
  Kind kind;
  int filled, total;
  const char* value;
};
constexpr StatRow kStats[] = {
    {"DAMAGE", DMG, 5, 6, "100 Pts."},
    {"CAPACITY", CAP, 3, 6, "44 Pts."},
    {"RELOAD", REL, 2, 6, "18 Pts."},
    {"CHARGE", CHR, 1, 6, "9 Pts."},
};
constexpr int kStatCount = 4;

}  // namespace

// ===========================================================================
