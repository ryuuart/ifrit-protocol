// ds2_bench.cpp — DEAD SPACE 2 (Visceral Games, 2011): the BENCH.
//
// The subject: the "Nanocircuit Repair" upgrade circuit Isaac gets at any
// Bench — a diegetic holographic panel that renders in front of him, in
// world space, without pausing the game (Dino Ignacio, GDC 2013 "Crafting
// Destruction: The Evolution of the Dead Space User Interface";
// hudsandguis.com/home/2012/08/22/dead-space-2-diegetic-interface-design).
// Power Nodes are spent into a rectilinear PCB trace: a trunk line with
// right-angle branch taps, typed nodes (DMG/CAP/CHR/REL) and unlabeled
// blank connector nodes that exist only to join branches
// (deadspace.fandom.com/wiki/Bench).
//
// The load-bearing reference is the actual in-game frame:
//   https://static.wikia.nocookie.net/deadspace/images/4/43/Bench_branches.jpg
// (1200x750, the CONTACT BEAM circuit — a DS2-exclusive weapon, which is
// what dates the screen to DS2 rather than the 2008 original). Every
// number below was read off that frame:
//
//   * 21 nodes on an 8-column x 4-row lattice, column pitch ~72 px, row
//     pitch ~51 px, trunk on row 2. 9 typed + 12 blank.
//   * the frame is NOT a rounded rect and not a plain octagon: its top
//     edge is TIERED — the outer thirds sit ~20 px lower than the raised
//     centre section, joined by 45 deg risers, and all four corners are
//     chamfered. The inner contour mirrors it inverted: its shoulders sit
//     ~11 px inside the outer, then DIP 24 px on a 45 deg fall and run
//     right as the header rule under the title.
//   * every node is a four-layer stamp: dark well, cyan corona (a
//     speckled radial burst), bright type-coloured ring, muted fill.
//   * traces are CASED double hairlines, not single strokes.
//   * the legend is a bracket-framed region (corner Ls, not a box) of
//     right-aligned labels + colour dot + chevron pip bar + "N Pts.".
//   * the ONE warm accent on an all-cyan screen is the brass Power Node
//     puck in the bottom-right NODES counter.
//
// Palette (calibrated off the compressed screenshot; hue/role held, not
// the 6th hex digit): panel #0C1D20, header/legend #123030, hologram cyan
// #8FE0E6, title #E4F7F8, DMG #C1584A/#E8998A, CAP #4C84A8/#8FC4E0,
// CHR #93A64A/#C7DE8E, REL #8C6B36/#D2A85E, brass #C9A227 -> #E8C860.
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

const SkColor4f kBodyDeep = rgb(0x081619);
const SkColor4f kBody = rgb(0x0F2427);
const SkColor4f kStrip = rgb(0x143230);
const SkColor4f kCyan = rgb(0x8FE0E6);
const SkColor4f kTitle = rgb(0xE4F7F8);
const SkColor4f kDim = rgb(0x3A5B57);
const SkColor4f kBrassLo = rgb(0xC9A227);
const SkColor4f kBrassHi = rgb(0xE8C860);
const SkColor4f kBrassDk = rgb(0x6E561A);

inline SkColor4f alpha(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// canvas geometry (1200 x 800)

constexpr float kW = 1200, kH = 800;
// the panel
constexpr float kPX = 108, kPY = 52, kPW = 984, kPH = 690;
constexpr float kPR = kPX + kPW, kPB = kPY + kPH;
constexpr float kOuterCut = 26, kOuterStep = 22, kOuterShoulder = 132;
constexpr float kInset = 12, kInnerCut = 18, kInnerDip = 40;
constexpr float kInnerShoulderL = 150, kInnerShoulderR = 46;
// the header rule, in canvas space
constexpr float kRuleY = kPY + kInset + kInnerDip; // 104
// the circuit lattice: 8 columns x 4 rows, the reference's own topology
constexpr float kCol0 = 246, kColPitch = 92;
constexpr float kRow0 = 190, kRowPitch = 78;
inline float colX(int c) { return kCol0 + kColPitch * (float)c; }
inline float rowY(int r) { return kRow0 + kRowPitch * (float)r; }
// the lower band
constexpr float kBandY = 528, kBandH = 168;
constexpr float kLegX = 140, kLegW = 690;
constexpr float kCntX = 852, kCntW = 208;
constexpr float kHintY = 706;

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

/** The node corona: short radial ticks, the cheap analogue of the
 *  screenshot's speckled burst around every typed node. */
inline std::function<SkPath(SkSize)> burst(int count, float inner) {
  return [count, inner](SkSize s) {
    SkPathBuilder b;
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float r = s.width() * 0.5f;
    for (int i = 0; i < count; ++i) {
      const float a = 6.2831853f * (float)i / (float)count + 0.13f;
      const float c = std::cos(a), sn = std::sin(a);
      const float k = (i % 3 == 0) ? 1.0f : 0.82f; // ragged, not a gear
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

// ---------------------------------------------------------------------------
// the circuit, transcribed from the frame

enum Kind : int { Blank = 0, DMG, CAP, CHR, REL };

struct Node {
  int col, row;
  Kind kind;
};

// 8 columns x 4 rows; trunk on row 1. Exactly the screenshot's 21 nodes.
constexpr Node kNodes[] = {
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
constexpr int kNodeCount = (int)(sizeof(kNodes) / sizeof(kNodes[0]));

struct Edge {
  int a, b;
};
// Every edge is an independent two-endpoint route sharing anchor KEYS —
// a degree-3 blank node is a T-junction because three routes end on it,
// not because any primitive knows the word "junction".
constexpr Edge kEdges[] = {
    // trunk, left to right (7 segments)
    {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 10}, {10, 11},
    // upper taps
    {5, 0}, {0, 1}, {8, 2}, {11, 3},
    // lower taps
    {6, 13}, {13, 12}, {13, 18}, {7, 14}, {14, 19}, {9, 15},
    {15, 16}, {10, 16}, {11, 17}, {15, 20},
    // the second lower rank
    {18, 19}, {19, 20},
};
constexpr int kEdgeCount = (int)(sizeof(kEdges) / sizeof(kEdges[0]));

inline SkPoint nodeAt(int i) {
  return {colX(kNodes[i].col), rowY(kNodes[i].row)};
}
inline std::string nodeKey(int i) { return "n" + std::to_string(i); }

struct KindArt {
  SkColor4f fill, ring;
  const char *label;
};
inline KindArt artOf(Kind k) {
  switch (k) {
  case DMG: return {rgb(0x8E3E38), rgb(0xE8998A), "DMG"};
  case CAP: return {rgb(0x2F5A7C), rgb(0x8FC4E0), "CAP"};
  case CHR: return {rgb(0x5E7434), rgb(0xC7DE8E), "CHR"};
  case REL: return {rgb(0x6B4E23), rgb(0xD2A85E), "REL"};
  case Blank:
  default: return {{0, 0, 0, 0}, alpha(kCyan, 0.86f), nullptr};
  }
}

constexpr float kTypedDia = 30, kBlankDia = 19;

// ---------------------------------------------------------------------------
// legend data (fill counts read off the frame: 5/6, 1/6, 1/6, 1/6 — the
// exact ratios are not load-bearing, the geometry is)

struct StatRow {
  const char *label;
  Kind kind;
  int filled, total;
  const char *value;
};
constexpr StatRow kStats[] = {
    {"DAMAGE", DMG, 5, 6, "100 Pts."},
    {"CAPACITY", CAP, 3, 6, "44 Pts."},
    {"RELOAD", REL, 2, 6, "18 Pts."},
    {"CHARGE", CHR, 1, 6, "9 Pts."},
};
constexpr int kStatCount = 4;
constexpr float kPipW = 40, kPipH = 19, kPipGap = 7;

} // namespace

// ===========================================================================

struct Ds2Bench : sigil::compose::sketch::Sketch {
  // one bound glow per node — the idle pulse is desynced by index
  std::array<choreograph::Output<float>, 24> glow;
  choreograph::Output<float> socketPulse{1.0f};
  choreograph::Output<float> scanY{0};
  choreograph::Output<float> jitterX{0};
  choreograph::Output<float> holoAlpha{1.0f};
  choreograph::Output<float> cursorPhase{0};

  // the legend pip masses: one atlas (2 cells), one pool per row
  std::shared_ptr<instancing::Atlas> pips =
      std::make_shared<instancing::Atlas>(2.0f);
  std::array<std::shared_ptr<instancing::Pool>, kStatCount> pipPools;
  int pipFilled = 0, pipEmpty = 1;

  double nextGlitch = 4.2, glitchEnd = 0;

  // -------------------------------------------------------------------
  // the pip atlas: two chevron cells, tinted per instance

  void bakePips() {
    Element filled =
        box().outline(chevron())
            .fill(Material::linear({0, 0}, {0, kPipH},
                                   {{0.0f, rgb(0xD8E7E7)},
                                    {0.45f, rgb(0x9FB6B8)},
                                    {0.55f, rgb(0x7E979A)},
                                    {1.0f, rgb(0xC2D6D8)}}))
            .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.55f)),
                           PathFormat::Align::Inner));
    Element empty = box().outline(chevron()).stroke(
        stroke(1.2f, Fill::color(alpha(kDim, 0.85f)),
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
        tints[i] = on ? SkColor4f{0.72f + 0.28f * art.ring.fR,
                                  0.72f + 0.28f * art.ring.fG,
                                  0.72f + 0.28f * art.ring.fB, 1.0f}
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
                         {{0.0f, rgb(0x16262F, a * 0.4f)},
                          {0.38f, rgb(0x1E3340, a)},
                          {1.0f, rgb(0x0A141C, a * 0.2f)}}))
                     .effect(Effect::filter(
                         SkImageFilters::Blur(11, 16, nullptr)))
                     .zIndex(0));
    };
    strut(-30, 96, 0.85f);
    strut(64, 34, 0.5f);
    strut(1104, 92, 0.85f);
    strut(1036, 30, 0.45f);
    // one horizontal beam behind the panel's head
    root.child(box()
                   .absolute().left(Dim(-40.0f)).top(Dim(6.0f))
                   .width(Dim(kW + 80)).height(Dim(30.0f))
                   .fill(Material::linear({0, 0}, {0, 30},
                                          {{0.0f, rgb(0x2A424F, 0.55f)},
                                           {1.0f, rgb(0x0C1820, 0.3f)}}))
                   .effect(Effect::filter(SkImageFilters::Blur(6, 9, nullptr)))
                   .zIndex(0));
  }

  // -------------------------------------------------------------------
  // the panel plate + its frame vocabulary

  void plate(Element &root) {
    // body: solid + the live scanline field + a radial lift, ONE shader
    root.child(
        box()
            .key("plate")
            .absolute().left(Dim(kPX)).top(Dim(kPY))
            .width(Dim(kPW)).height(Dim(kPH))
            .outline(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
            .fill(Material::blend(
                {{Material::solid(kBody), SkBlendMode::kSrcOver},
                 {Material::radial({kPW * 0.42f, kPH * 0.34f}, kPW * 0.72f,
                                   {{0.0f, rgb(0xFFFFFF)},
                                    {0.55f, rgb(0xC9D8D8)},
                                    {1.0f, rgb(0x63797B)}}),
                  SkBlendMode::kMultiply},
                 {scanField(alpha(kCyan, 0.085f), 3.0f),
                  SkBlendMode::kScreen}}))
            .zIndex(1));

    // frame: a soft plus-blended halo under a crisp cyan keyline
    root.child(box()
                   .absolute().left(Dim(kPX)).top(Dim(kPY))
                   .width(Dim(kPW)).height(Dim(kPH))
                   .outline(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
                   .stroke(LayeredBrush{{
                       {13, alpha(kCyan, 0.10f), 7, {}, 0, SkBlendMode::kPlus},
                       {5, alpha(kCyan, 0.24f), 2.6f, {}, 0,
                        SkBlendMode::kPlus},
                       {2.6f, alpha(rgb(0xD8F6F8), 0.95f)},
                   }})
                   .zIndex(6));

    // the inner contour: a thin line whose dipped centre IS the header rule
    root.child(box()
                   .absolute().left(Dim(kPX + kInset)).top(Dim(kPY + kInset))
                   .width(Dim(kPW - 2 * kInset)).height(Dim(kPH - 2 * kInset))
                   .outline(panelInner(kInnerCut, kInnerDip, kInnerShoulderL,
                                       kInnerShoulderR))
                   .stroke(stroke(1.15f, Fill::color(alpha(kCyan, 0.62f))))
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
                                          Fill::color(alpha(kCyan, 0.30f)),
                                      .dashIntervals = {2.0f, 6.0f}})
                   .zIndex(6));
  }

  // -------------------------------------------------------------------
  // header

  void header(Element &root) {
    root.child(box()
                   .absolute().left(Dim(kPX)).top(Dim(kPY + 22))
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
                              .effect(styles::textGlow(alpha(kCyan, 0.55f),
                                                       5.0f))));

    root.child(box().absolute().left(Dim(kPX + 40)).top(Dim(kRuleY - 26))
                   .zIndex(7)
                   .child(text(toU8("NANOCIRCUIT REPAIR"),
                               type(11, alpha(kCyan, 0.62f), 0.14f, false))));
    root.child(box().absolute().right(Dim(kW - kPR + 44))
                   .top(Dim(kRuleY - 26)).zIndex(7)
                   .child(text(toU8("RIG LINK / OK"),
                               type(11, alpha(kCyan, 0.62f), 0.14f, false))));
  }

  // -------------------------------------------------------------------
  // the entry socket: the bracket-and-arrow the current flows in through

  void entrySocket(Element &root) {
    const SkPoint at = nodeAt(4);
    root.child(
        custom([](SkCanvas &canvas, const PaintContext &ctx) {
          const float w = ctx.size.width(), h = ctx.size.height();
          SkPaint p;
          p.setAntiAlias(true);
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeWidth(1.6f);
          p.setColor4f(alpha(kCyan, 0.8f), nullptr);
          // the outer socket housing: a rectangle open on the left
          SkPathBuilder b;
          b.moveTo(w * 0.30f, h * 0.10f);
          b.lineTo(w * 0.98f, h * 0.10f);
          b.lineTo(w * 0.98f, h * 0.90f);
          b.lineTo(w * 0.30f, h * 0.90f);
          // the inner bracket
          b.moveTo(w * 0.52f, h * 0.28f);
          b.lineTo(w * 0.40f, h * 0.28f);
          b.lineTo(w * 0.40f, h * 0.72f);
          b.lineTo(w * 0.52f, h * 0.72f);
          canvas.drawPath(b.detach(), p);
          // the feed arrow
          SkPathBuilder t;
          t.moveTo(w * 0.02f, h * 0.30f);
          t.lineTo(w * 0.24f, h * 0.50f);
          t.lineTo(w * 0.02f, h * 0.70f);
          t.close();
          p.setStyle(SkPaint::kFill_Style);
          canvas.drawPath(t.detach(), p);
        })
            .absolute()
            .left(Dim(at.fX - 96)).top(Dim(at.fY - 44))
            .width(Dim(104.0f)).height(Dim(88.0f))
            .opacity(&socketPulse)
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // traces — every edge an independent routed connector sharing node keys

  void traces(Element &root) {
    auto layer = box().absolute().inset(0).zIndex(4)
                     .staggerChildren(34ms, Stagger::From::Start);
    for (int i = 0; i < kEdgeCount; ++i) {
      const Edge &e = kEdges[i];
      layer.child(
          connector(nodeKey(e.a), nodeKey(e.b), routers::orthogonal())
              .key("e" + std::to_string(i))
              .absolute().inset(0)
              .trim(0.0f, withFrom(0.0f, 1.0f, {620ms}))
              .stroke(LayeredBrush{{{7.0f, alpha(kCyan, 0.085f), 3.4f, {}, 0,
                                     SkBlendMode::kPlus}}})
              .stroke(lines::cased(1.25f, Fill::color(alpha(kCyan, 0.62f)),
                                   4.4f)));
    }
    root.child(std::move(layer));
  }

  // -------------------------------------------------------------------
  // node stamps: one SDF pass each (well + corona + fill + ring)

  void nodes(Element &root) {
    auto layer = box().absolute().inset(0).zIndex(5)
                     .staggerChildren(34ms, Stagger::From::Start);
    for (int i = 0; i < kNodeCount; ++i) {
      const SkPoint at = nodeAt(i);
      const KindArt art = artOf(kNodes[i].kind);
      const bool typed = kNodes[i].kind != Blank;
      const float dia = typed ? kTypedDia : kBlankDia;

      const sdf::Style st{
          .fill = typed ? art.fill : SkColor4f{0.03f, 0.09f, 0.10f, 0.55f},
          .borderWidth = typed ? 2.6f : 1.9f,
          .borderColor = typed ? art.ring : alpha(kCyan, 0.9f),
          .glowRadius = typed ? 7.5f : 4.5f,
          .glowColor = alpha(kCyan, typed ? 0.5f : 0.3f),
          .shadowOffset = {0, 0},
          .shadowBlur = typed ? 9.0f : 5.5f,
          .shadowColor = rgb(0x020A0C, typed ? 0.92f : 0.7f)};
      Material m = sdf::material(sdf::circle(), st);
      m.uniform("uGlowR", &glow[(size_t)(i % (int)glow.size())]);

      const float boxSize = sdf::minBoxFor(st, dia);
      layer.child(box()
                      .key(nodeKey(i))
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

      if (typed) {
        // the speckled corona + the type label, both keyed leaves: the
        // atlas has no per-instance string, so labels stay real text
        layer.child(box()
                        .width(Dim(dia + 26)).height(Dim(dia + 26))
                        .centerAt(at)
                        .outline(burst(22, 0.70f))
                        .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.26f))))
                        .opacity(withFrom(0.0f, 1.0f, {320ms}))
                        .zIndex(4));
        layer.child(box()
                        .width(Dim(dia * 0.44f)).height(Dim(dia * 0.44f))
                        .centerAt(at)
                        .fill(Material::radial(
                            {dia * 0.22f, dia * 0.22f}, dia * 0.30f,
                            {{0.0f, alpha(art.ring, 0.55f)},
                             {1.0f, alpha(art.ring, 0.0f)}}))
                        .zIndex(5));
        layer.child(text(toU8(art.label), type(11.5f, alpha(kCyan, 0.9f),
                                               0.11f))
                        .centerAt({at.fX + 27, at.fY - 30})
                        .opacity(withFrom(0.0f, 1.0f, {320ms}))
                        .zIndex(5));
      }
    }
    root.child(std::move(layer));
  }

  // -------------------------------------------------------------------
  // legend: bracket-framed stat rows with instanced chevron pips

  Element statRow(int r) {
    const StatRow &s = kStats[r];
    const KindArt art = artOf(s.kind);
    const float barW = (float)s.total * kPipW + (float)(s.total - 1) * kPipGap;
    return box()
        .row().alignItems(Align::Center).height(Dim(30.0f))
        .child(box().width(Dim(168.0f)).alignItems(Align::End)
                   .child(text(toU8(s.label),
                               type(14.5f, alpha(kCyan, 0.95f), 0.10f))))
        .child(box().width(Dim(9.0f)).height(Dim(9.0f)).margin(13, 0, 13, 0)
                   .outline(shapes::polygon(12))
                   .fill(Material::radial({4.5f, 4.5f}, 5.0f,
                                          {{0.0f, art.ring},
                                           {1.0f, art.fill}})))
        .child(box().width(Dim(barW)).height(Dim(kPipH))
                   .opacity(withFrom(0.0f, 1.0f, {320ms}))
                   .translateX(withFrom(-16.0f, 0.0f, {380ms}))
                   .child(instancing::instances(pips, pipPools[(size_t)r])))
        .child(box().width(Dim(30.0f)))
        .child(box().width(Dim(96.0f))
                   .child(text(toU8(s.value),
                               type(13, rgb(0xDCEEF2), 0.02f, false))));
  }

  void legend(Element &root) {
    auto card =
        box().key("legend")
            .absolute().left(Dim(kLegX)).top(Dim(kBandY))
            .width(Dim(kLegW)).height(Dim(kBandH))
            .fill(Material::blend(
                {{Material::solid(alpha(kStrip, 0.55f)), SkBlendMode::kSrcOver},
                 {scanField(alpha(kCyan, 0.055f), 3.0f),
                  SkBlendMode::kScreen}}))
            .outline(chamfer(12))
            .zIndex(7)
            .column().padding(20, 16).gap(4)
            .staggerChildren(70ms, Stagger::From::Start);

    // column heads
    card.child(box().row().height(Dim(16.0f))
                   .child(box().width(Dim(168.0f)).alignItems(Align::End)
                              .child(text(toU8("SPECIFICATION"),
                                          type(9.5f, alpha(kCyan, 0.45f),
                                               0.20f, false))))
                   .child(box().width(Dim(31.0f)))
                   .child(text(toU8("NANOCIRCUIT LOAD"),
                               type(9.5f, alpha(kCyan, 0.45f), 0.20f, false)))
                   .child(box().grow(1))
                   .child(text(toU8("VALUE"),
                               type(9.5f, alpha(kCyan, 0.45f), 0.20f, false))));
    for (int r = 0; r < kStatCount; ++r)
      card.child(statRow(r));
    root.child(std::move(card));

    // the bracket chrome the Bench frames its regions with
    root.child(box().absolute().left(Dim(kLegX - 8)).top(Dim(kBandY - 8))
                   .width(Dim(kLegW + 16)).height(Dim(kBandH + 16))
                   .outline(cornerBrackets(26))
                   .stroke(stroke(1.5f, Fill::color(alpha(kCyan, 0.75f))))
                   .zIndex(8));
    root.child(box().absolute().left(Dim(kLegX + 18)).top(Dim(kBandY + 44))
                   .width(Dim(kLegW - 36)).height(Dim(1.0f))
                   .outline(hline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.28f))))
                   .zIndex(8));
    root.child(box().absolute().left(Dim(kLegX + kLegW - 128))
                   .top(Dim(kBandY + 16)).width(Dim(1.0f))
                   .height(Dim(kBandH - 32))
                   .outline(vline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.28f))))
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
                {{Material::solid(alpha(kStrip, 0.55f)), SkBlendMode::kSrcOver},
                 {scanField(alpha(kCyan, 0.055f), 3.0f),
                  SkBlendMode::kScreen}}))
            .column().alignItems(Align::Center).padding(14, 12).gap(0)
            .zIndex(7)
            .child(box().width(Dim(118.0f)).height(Dim(26.0f))
                       .alignItems(Align::Center).justify(Justify::Center)
                       .outline(chamfer(6))
                       .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.5f))))
                       .child(text(toU8("NODES"),
                                   type(12.5f, alpha(kCyan, 0.95f), 0.16f))))
            // the brass power-node puck: side wall, top face, bore ring
            .child(box().width(Dim(76.0f)).height(Dim(52.0f))
                       .margin(0, 14, 0, 8)
                       .child(box().absolute().left(Dim(4.0f)).top(Dim(16.0f))
                                  .width(Dim(68.0f)).height(Dim(32.0f))
                                  .corners({16})
                                  .fill(Material::linear(
                                      {0, 0}, {0, 32},
                                      {{0.0f, kBrassLo},
                                       {0.45f, rgb(0x8A6E1C)},
                                       {1.0f, kBrassDk}})))
                       .child(box().absolute().left(Dim(4.0f)).top(Dim(2.0f))
                                  .width(Dim(68.0f)).height(Dim(30.0f))
                                  .outline(shapes::squircle(2.0f))
                                  .fill(Material::linear(
                                      {0, 0}, {58, 30},
                                      {{0.0f, kBrassHi},
                                       {0.4f, rgb(0xD9B23C)},
                                       {1.0f, rgb(0x9C7B22)}}))
                                  .stroke(stroke(1.0f,
                                                 Fill::color(rgb(0xF3DC94,
                                                                 0.8f)))))
                       .child(box().absolute().left(Dim(24.0f))
                                  .top(Dim(9.0f)).width(Dim(28.0f))
                                  .height(Dim(15.0f))
                                  .outline(shapes::squircle(2.0f))
                                  .stroke(stroke(1.4f,
                                                 Fill::color(rgb(0x7A5F16,
                                                                 0.9f))))))
            .child(text(toU8("2"), type(42, kTitle, 0.0f))
                       .key("nodecount")
                       .transition({.duration = 200ms})));

    root.child(box().absolute().left(Dim(kCntX - 8)).top(Dim(kBandY - 8))
                   .width(Dim(kCntW + 16)).height(Dim(kBandH + 16))
                   .outline(cornerBrackets(22))
                   .stroke(stroke(1.5f, Fill::color(alpha(kCyan, 0.75f))))
                   .zIndex(8));
  }

  // -------------------------------------------------------------------
  // the hardware hint row along the panel's bottom rail

  void hints(Element &root) {
    root.child(box().absolute().left(Dim(kPX + 32)).top(Dim(kHintY))
                   .width(Dim(kPW - 64)).height(Dim(1.0f))
                   .outline(hline())
                   .stroke(stroke(1.0f, Fill::color(alpha(kCyan, 0.4f))))
                   .zIndex(8));

    auto hint = [&](std::string label) {
      return box().row().alignItems(Align::Center).gap(7).child(
          text(toU8(label), type(12, alpha(kCyan, 0.8f), 0.06f, false)));
    };

    root.child(
        box().absolute().left(Dim(kPX)).top(Dim(kHintY + 8))
            .width(Dim(kPW)).height(Dim(28.0f))
            .row().alignItems(Align::Center).justify(Justify::Center).gap(52)
            .zIndex(8)
            .child(box().row().alignItems(Align::Center).gap(8)
                       .child(custom([this](SkCanvas &canvas,
                                            const PaintContext &ctx) {
                                const float r = ctx.size.width() * 0.5f;
                                SkPaint p;
                                p.setAntiAlias(true);
                                p.setStyle(SkPaint::kStroke_Style);
                                p.setStrokeWidth(1.4f);
                                p.setColor4f(alpha(kCyan, 0.85f), nullptr);
                                canvas.drawCircle(r, r, r - 1.2f, p);
                                const float k =
                                    0.5f + 0.5f * std::sin(
                                               (float)ctx.elapsedSeconds * 3.4f);
                                p.setStyle(SkPaint::kFill_Style);
                                p.setColor4f(alpha(kCyan, 0.35f + 0.5f * k),
                                             nullptr);
                                canvas.drawCircle(r, r, r * 0.42f, p);
                                p.setStyle(SkPaint::kStroke_Style);
                                for (int i = 0; i < 4; ++i) {
                                  const float a = 1.5707963f * (float)i;
                                  const float c = std::cos(a), s = std::sin(a);
                                  canvas.drawLine(r + c * r * 0.62f,
                                                  r + s * r * 0.62f,
                                                  r + c * r * 0.92f,
                                                  r + s * r * 0.92f, p);
                                }
                              })
                                  .width(Dim(15.0f)).height(Dim(15.0f))
                                  .cache(Cache::None))
                       .child(text(toU8("Navigate"),
                                   type(12, alpha(kCyan, 0.8f), 0.06f,
                                        false))))
            .child(hint("[Enter] Select"))
            .child(hint("[Esc] Exit")));

    // the two empty hardware sockets the bezel carries at its corners
    for (float x : {kPX + 34, kPR - 46}) {
      root.child(box().absolute().left(Dim(x)).top(Dim(kHintY + 14))
                     .width(Dim(12.0f)).height(Dim(12.0f))
                     .stroke(stroke(1.2f, Fill::color(alpha(kCyan, 0.5f))))
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
                                           {0.5f, alpha(kCyan, 0.055f)},
                                           {1.0f, alpha(kCyan, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::None)
                   .zIndex(9));
    // the vignette that seats the panel back into the dark
    root.child(box().absolute().inset(0)
                   .fill(Material::radial({kW * 0.5f, kH * 0.46f}, kW * 0.62f,
                                          {{0.0f, rgb(0x000000, 0.0f)},
                                           {0.62f, rgb(0x000000, 0.10f)},
                                           {1.0f, rgb(0x02060A, 0.72f)}}))
                   .zIndex(11));
  }

  // -------------------------------------------------------------------

  Element describe(sketch::SketchContext &ctx) {
    (void)ctx;
    auto root = stack().fill(Material::radial(
        {kW * 0.5f, kH * 0.5f}, 900,
        {{0.0f, rgb(0x0A141C)}, {0.62f, rgb(0x060C12)},
         {1.0f, rgb(0x030507)}}));
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
    traces(holo);
    nodes(holo);
    entrySocket(holo);
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
        glow[i] = 6.2f + 2.1f * (float)std::sin(t * 2.75 + (double)i * 0.62);
      socketPulse = 0.78f + 0.22f * (float)std::sin(t * 3.9);
      cursorPhase = (float)std::fmod(t, 1.0);
      scanY = (float)std::fmod(t * 96.0, (double)(kPH - 40));

      // the hologram stutter: 90-120 ms of snap (not eased) jitter every
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
