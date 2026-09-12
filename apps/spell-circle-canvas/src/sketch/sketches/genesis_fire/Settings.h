#pragma once

// Stage dimensions, emission parameters, palette and reusable panel furniture.

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkVertices.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Chance.h>
#include <sigildraw/Draw.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/physics/Physics.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Meter.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Instrument.h"

namespace sketch = sigil::sketch;
namespace chance = sigil::core::chance;
namespace field = sigil::material::field;
namespace patterns = sigil::material::pattern;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::draw::Pen;
using sigil::material::skia::Paint;
using sigil::material::skia::toColor;
namespace draw = sigil::draw;
namespace ch = choreograph;

namespace genesis {

// ---------------------------------------------------------------------------
// Palette — this study's own chrome, not sourced

constexpr SkColor4f kInk = hexColor(0x06070B);
constexpr SkColor4f kPanel = hexColor(0x0B0D14);
constexpr SkColor4f kBone = hexColor(0xE9ECF3);
constexpr SkColor4f kSteel = hexColor(0x77819A);
constexpr SkColor4f kSteelDim = hexColor(0x545E74);
constexpr SkColor4f kKeyline = hexColor(0x242A36);
constexpr SkColor4f kCyan = hexColor(0x4FB8D8);

// THE EMISSION SEED. Everything about the fire's colour is a consequence
// of this triple plus "light adds and clamps" [R83 §2.5].
constexpr float kE0r = 0.220f, kE0g = 0.050f, kE0b = 0.009f;

/** clamp(n * e0) — the colour of a pixel covered by n particles. This is
 *  not a palette; it is an overlap count. Red saturates at n=5, green at
 *  n=20, blue at n=111. */
inline SkColor4f overlap(int n) {
  return {std::min(1.0f, (float)n * kE0r), std::min(1.0f, (float)n * kE0g),
          std::min(1.0f, (float)n * kE0b), 1.0f};
}
constexpr int kRampN[14] = {1, 2, 3, 4, 5, 8, 12, 16, 20, 28, 40, 60, 85, 111};

// ---------------------------------------------------------------------------
// Geometry. The canvas size and the ink background are this sketch's own,
// declared in setup(); nothing here inherits a host default.

// The canvas is the stage's own 4:3 plus the header over it and the
// caption band under it — a band declared outside the artefact needs
// room outside the artefact.
constexpr float kCanvasW = 1440, kCanvasH = 926;
constexpr float kStageW = 888, kStageH = 666;  // 4:3 — the 500-line raster
constexpr float kSideW = 448;

// The page, laid out by the pen: the padding, the header, the stage and
// the sidebar's five panel boxes, each a number rather than a flex
// negotiation.
constexpr float kPad = 36;
constexpr float kHeaderH = 102;
constexpr float kBodyY = kPad + kHeaderH + 24;  // 162
constexpr float kStageX = kPad;                 // 36
constexpr float kSideX = kPad + kStageW + 32;   // 956
constexpr float kCaptionY = kBodyY + kStageH + 10;
constexpr float kPanelGap = 8;
constexpr float kPanelH[5] = {104, 192, 98, 144, 96};

/** The top of sidebar panel @p i, counting from zero. */
constexpr float panelTop(int i) {
  float y = kBodyY;
  for (int k = 0; k < i; ++k) y += kPanelH[k] + kPanelGap;
  return y;
}

constexpr float kLimbCx = 444.0f, kLimbCy = 1620.0f, kLimbR = 1150.0f;

inline float limbY(float x) {
  const float dx = x - kLimbCx;
  const float k = kLimbR * kLimbR - dx * dx;
  return kLimbCy - (k > 0 ? std::sqrt(k) : 0.0f);
}

// The reconstructed parameter table. All rates are per SIMULATION FRAME at
// 24 Hz, because [R83] counts lifetimes in frames and the frames in question
// are film frames.
constexpr double kSimHz = 24.0;
constexpr double kSimStep = 1.0 / kSimHz;
constexpr double kLoopSeconds = 10.0;

constexpr float kSpread = 168.0f;    // px/s  (7.0 px/frame)
constexpr float kSiteStep = 18.5f;   // px    (< R_gen, so systems overlap)
constexpr float kX0 = -80.0f;        // impact point, off-frame left
constexpr int kSiteCount = 53;       // covers -80 .. 882
constexpr float kGenWindow = 52.8f;  // frames (2.20 s)
constexpr float kRGen = 26.0f;       // generation segment half-width, px
constexpr float kPsiMax = 34.0f * 0.0174532925f;  // ejection cone, rad

// The birth rate is the one constant the census PINS. A limb view stacks
// the ring in depth (Fig. 6 photographs ~200 systems at once; a vertical
// slice anchors 53 columns), so each column carries kDepth systems' worth
// of births — see the census panel's derivation, which is printed on the
// canvas and measured against the live count.
constexpr int kDepth = 3;
// The A/B bench's pool size: enough slots that the cloud visibly
// accumulates inside a 130x52 cell. Births stop once this many are alive.
constexpr size_t kAbCount = 700;
constexpr float kInitialMeanParts = 41.0f * (float)kDepth;
constexpr float kDeltaMeanParts =
    -kInitialMeanParts / kGenWindow;  // rate hits 0 exactly at the window
constexpr float kVarParts = 13.0f * (float)kDepth;

constexpr float kMeanSpeed = 15.0f, kVarSpeed = 4.8f;  // px/frame
constexpr float kGravity = 1.02f;                      // px/frame^2
constexpr float kMeanLife = 34.0f, kVarLife = 12.0f;   // FRAMES
constexpr float kMeanSize = 4.0f, kVarSize = 1.6f;     // px diameter
constexpr float kSizeRate = 0.05f;  // px/frame — [R83 §2.3] size changes at
                                    // a rate global to the system
constexpr float kColorVar = 0.35f;
// Documented ORDERING ("green and blue dropped off quickly, and the red
// followed at a slower rate"); the rates are reconstruction.
constexpr float kDecR = kE0r / 34.0f, kDecG = kE0g / 12.0f, kDecB = kE0b / 8.5f;
constexpr float kMinIntensity = 0.020f;

// Derived timing, printed on the canvas.
constexpr double kFrontCrossSeconds = (kStageW - kX0) / kSpread;  // 5.762 s

// What a particle carries beyond the point set's own position, velocity
// and lifetime: four of [R83 §2.2]'s seven attributes, and the index of
// the second-level system that threw it, which the per-system gravity
// and the below-surface cull read.
constexpr const char* kSize = "size";
constexpr const char* kRed = "red";
constexpr const char* kGreen = "green";
constexpr const char* kBlue = "blue";
constexpr const char* kSite = "site";

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

/** The same register on the PEN: a pen carries one type and one fill, so
 *  a register is set rather than described. */
inline void penMono(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(instrument::penType(monoFace(), size, track));
  pen.fill(c);
}

// The planet's silhouette: the limb arc, closed down to the stage floor.
inline std::function<SkPath(SkSize)> limbOutline() {
  return [](SkSize s) {
    SkPathBuilder b;
    constexpr int kSamples = 160;
    b.moveTo(0, limbY(0));
    for (int i = 1; i <= kSamples; ++i) {
      const float x = s.width() * (float)i / (float)kSamples;
      b.lineTo(x, limbY(x));
    }
    b.lineTo(s.width(), s.height());
    b.lineTo(0, s.height());
    b.close();
    return b.detach();
  };
}

/** A panel shell: ground, keyline, corners, padding. Each panel is its
 *  own guest at its own box, so each carries its own entrance delay
 *  rather than taking a stagger from a column above it. */
inline Element panel(float height, int order) {
  const auto delay = std::chrono::milliseconds(90 * order);
  return box()
      .column()
      .width(kSideW)
      .height(height)
      .shrink(0)
      .padding(12)
      .corners({5})
      .fill(kPanel)
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(
          animate(from(0.0f).to(1.0f), {.duration = 300ms, .delay = delay}))
      .translateX(
          animate(from(14.0f).to(0.0f), {.duration = 300ms, .delay = delay}))
      .key(std::string("panel") + std::to_string(order));
}

inline Element panelHead(const char* s) {
  return t(s, ui(9.5f, kSteel, 1.9f)).height(13).shrink(0);
}

}  // namespace genesis

// ===========================================================================

using namespace genesis;
