#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/decode/Json.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/physics/Constraints.h>
#include <sigilmotion/physics/Points.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "../genesis_fire/Instrument.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::draw::Pen;
using sigil::material::skia::Paint;
namespace draw = sigil::draw;
namespace ch = choreograph;
namespace physics = sigil::motion::physics;

namespace hitman_verlet {}
using namespace hitman_verlet;
namespace hitman_verlet {

// ---------------------------------------------------------------------------
// Palette — this study's own chrome (a physics-debug register)

constexpr material::Color kInk = hexColor(0x0A0A0C);
constexpr material::Color kPanel = hexColor(0x101116);
constexpr material::Color kKeyline = hexColor(0x191B22);
constexpr material::Color kBone = hexColor(0xE8E6E1);
constexpr material::Color material::skia::toSkColor(kSteel) =
    hexColor(0x8A8F9C);
constexpr material::Color kBlue = hexColor(0x6FA8DC);
constexpr material::Color kRed = hexColor(0xC8402F);
constexpr material::Color kSolid = hexColor(0x2A2E38);
constexpr material::Color kTick = hexColor(0x5A6070);

// The constraint-error ramp — the study's whole visual thesis.
constexpr float kRampStop[5] = {0.000f, 0.004f, 0.010f, 0.020f, 0.035f};
constexpr material::Color kRampCol[5] = {hexColor(0x4FC79E), hexColor(0x93C866),
                                   hexColor(0xF2A73B), hexColor(0xE2673A),
                                   hexColor(0xC8402F)};

inline material::Color errColor(float e, float alpha = 1.0f) {
  if (e <= kRampStop[0])
    return {kRampCol[0].r, kRampCol[0].g, kRampCol[0].b, alpha};
  for (int i = 1; i < 5; ++i) {
    if (e <= kRampStop[i]) {
      const float u =
          (e - kRampStop[i - 1]) / (kRampStop[i] - kRampStop[i - 1]);
      const material::Color &a = kRampCol[i - 1], &b = kRampCol[i];
      return {a.r + (b.r - a.r) * u, a.g + (b.g - a.g) * u,
              a.b + (b.b - a.b) * u, alpha};
    }
  }
  return {kRampCol[4].r, kRampCol[4].g, kRampCol[4].b, alpha};
}

inline material::Color fadeTo(material::Color c, float a) {
  return {c.r, c.g, c.b, c.a * a};
}

// ---------------------------------------------------------------------------
// The frame. THE STAGE IS SQUARE BECAUSE THE PAPER'S WORLD IS A CUBE.

constexpr float kCanvasW = 1560, kCanvasH = 920;
constexpr float kStage = 736;              // px, = the 1000-unit cube
constexpr float kUnit = kStage / 1000.0f;  // 0.736 px per world unit
constexpr float kColW = 352;

// The page, laid out by the pen: padding 32, a 100 px header, then the
// stage and two panel columns whose fixed heights add up to the stage's.
constexpr float kPad = 32;
constexpr float kHeaderH = 100;
constexpr float kBodyY = kPad + kHeaderH + 20;  // 152
constexpr float kStageX = kPad;                 // 32
constexpr float kColAX = kPad + kStage + 28;    // 796
constexpr float kColBX = kColAX + kColW + 28;   // 1176
constexpr float kPanelGap = 24;
constexpr float kPanelAH[3] = {156, 288, 244};
constexpr float kPanelBH[3] = {236, 264, 188};
constexpr float kPanelPad = 14;

constexpr float panelTop(const float (&h)[3], int i) {
  float y = kBodyY;
  for (int k = 0; k < i; ++k) y += h[k] + kPanelGap;
  return y;
}

// The parameter table. Every rate here is per fixed simulation step.
constexpr double kSimHz = 60.0;
constexpr float kDrag = 0.99f;          // documented "1.99", per VERIFIED 2
constexpr float kGravityStep = 0.757f;  // a*dt^2, units/step^2 (derived)
constexpr float kFriction = 0.14f;
constexpr float kCapsule = 14.0f;   // world units
constexpr float kKneeMin = 100.0f;  // the documented inequality, my threshold
constexpr float kBlastK = 130000.0f;
constexpr float kHitPush = 45.0f;
constexpr double kLoop = 11.0;

// The re-measured Fig. 9 rest pose: fractions of figure height, feet = 0,
// +x = the figure's right. Head and neck sit on the symmetry axis; the
// diagram draws them 1.92% of figure height off it, which is its own
// drafting slop (see VERIFIED 4).
struct Norm {
  float x, y;
};
constexpr Norm kHead{0.0000f, 1.0000f};
constexpr Norm kNeck{0.0000f, 0.8846f};
constexpr Norm kSh{0.1727f, 0.7598f};
constexpr Norm kEl{0.2308f, 0.5960f};
constexpr Norm kHa{0.2308f, 0.4424f};
constexpr Norm kWa{0.1057f, 0.5384f};
constexpr Norm kHi{0.1441f, 0.4038f};
constexpr Norm kKn{0.1827f, 0.2018f};
constexpr Norm kFo{0.1827f, 0.0000f};

// The anchor: the paper's own restlength = 100 assigned to the thigh
// (measured ratio 0.20569) fixes the whole figure.
constexpr float kThighRatio = 0.20569f;
constexpr float kFigureH = 100.0f / kThighRatio;  // 486.17 world units

// The bump — the paper's Fig. 4 obstacle, drawn to scale.
constexpr SkPoint kBumpA{470, 0}, kBumpB{560, 120}, kBumpC{650, 0};
// The blast sits LEFT of the spawn so the body is thrown INTO the bump.
constexpr SkPoint kBlast{170, 20};

// ---------------------------------------------------------------------------
// Vector helpers (SkPoint already carries +, -, and *float)

inline float dot(SkPoint a, SkPoint b) { return a.fX * b.fX + a.fY * b.fY; }
inline float len(SkPoint a) { return std::sqrt(dot(a, a)); }

// ---------------------------------------------------------------------------
// Type

using instrument::faced;
using instrument::heavyFace;
using instrument::mono;
using instrument::monoB;
using instrument::monoBoldFace;
using instrument::monoFace;
using instrument::t;
using instrument::ui;
using instrument::uiFace;

/** The same three registers on the PEN: a pen carries one type and one
 *  fill, so a register is set rather than described. */
inline void penMono(Pen& pen, float size, material::Color c,
                    float track = 0.0f) {
  pen.textFont(instrument::penType(monoFace(), size, track));
  pen.fill(c);
}
inline void penMonoB(Pen& pen, float size, material::Color c,
                     float track = 0.0f) {
  pen.textFont(instrument::penType(monoBoldFace(), size, track));
  pen.fill(c);
}
inline void penUi(Pen& pen, float size, material::Color c, float track = 0.0f) {
  pen.textFont(instrument::penType(uiFace(), size, track));
  pen.fill(c);
}

/** A part's entrance as time arithmetic: what a described tree spells as
 *  `animate(from(0).to(1), {duration, delay})`, in a loop that has the
 *  clock in its hand. */
inline float cue(double ms, float delayMs, float durationMs,
                 const ch::EaseFn& ease = nullptr) {
  const float u = std::clamp(
      (float)((ms - (double)delayMs) / (double)durationMs), 0.0f, 1.0f);
  return ease ? ease(u) : u;
}

/** WHAT A PLOT ON THIS SHEET LOOKS LIKE. The chart kit names the part —
 *  the axis, the rules, a curve, a tick, a word, a band — and this says
 *  what each is drawn in, so a series is named at the layer and coloured
 *  here. `exact` and `approx` are the two curves of A3, and `hit` is the
 *  one column of its five that the shipped code lands on. */
inline sigil::compose::StyleSheet plotClasses() {
  sigil::compose::StyleSheet look{
      sigil::compose::rule(".plotAxis")
          .font({.color = material::skia::toSkColor(hexColor(0x2A2E38))}),
      sigil::compose::rule(".plotRule")
          .font({.color = material::skia::toSkColor(hexColor(0x2A2E38))}),
      sigil::compose::rule(".plotTick")
          .font({.face = monoFace(),
                 .size = 7.0f,
                 .color = material::skia::toSkColor(kTick)}),
      sigil::compose::rule(".plotLabel")
          .font({.face = monoFace(),
                 .size = 7.0f,
                 .color = material::skia::toSkColor(kSteel)}),
      sigil::compose::rule(".plotBar")
          .font(
              {.color = material::skia::toSkColor(hexColor(0x6FA8DC, 0.42f))}),
      sigil::compose::rule(".exact").font(
          {.face = monoFace(),
           .size = 7.0f,
           .color = material::skia::toSkColor(kSteel)}),
      sigil::compose::rule(".approx").font(
          {.face = monoFace(),
           .size = 7.0f,
           .color = material::skia::toSkColor(kBlue)}),
      sigil::compose::rule(".hit").font(
          {.color = material::skia::toSkColor(kBlue)})};
  return look;
}

/** A sidebar panel shell. Each panel is its own guest at its own box, so
 *  each carries its own entrance delay rather than taking a stagger from
 *  a column above it. */
inline Element panel(float height, std::string_view heading, int order) {
  const auto delay = std::chrono::milliseconds(85 * order);
  return box()
      .column()
      .width(kColW)
      .height(height)
      .flexShrink(0)
      .padding(kPanelPad)
      .gap(7)
      .borderRadius({5})
      .fill(kPanel)
      .overflow(Overflow::Clip)
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(
          animate(from(0.0f).to(1.0f), {.duration = 300ms, .delay = delay}))
      .translateX(
          animate(from(14.0f).to(0.0f), {.duration = 300ms, .delay = delay}))
      .key(std::string("panel") + std::to_string(order))
      .children({t(heading, ui(9.5f, kSteel, 1.9f)).height(12).flexShrink(0)});
}

}  // namespace hitman_verlet

// ===========================================================================
