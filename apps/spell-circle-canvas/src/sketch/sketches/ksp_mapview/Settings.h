#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Conic.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Globe.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdio>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace data = sigil::data;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace matkit = sigil::material::kit;
namespace sdf = sigil::material::sdf;
namespace arrange = sigil::geometry::arrange;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
using sigil::material::skia::Effect;
using sigil::material::skia::Paint;
using sigil::material::skia::Stop;
using sigil::material::skia::toColor;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace ksp {

// ---------------------------------------------------------------------------
// Palette

constexpr material::Color kSpace = hexColor(0x090A0C);  // sampled
constexpr material::Color kSpaceEdge = hexColor(0x06070A);
constexpr material::Color kNebula = hexColor(0x6E7288);
constexpr material::Color kNebula2 = hexColor(0x525F82);

constexpr material::Color kOceanLit = hexColor(0x235274);  // sampled mid-tone
constexpr material::Color kOceanDark = hexColor(0x12283A);
constexpr material::Color kLandMoss = hexColor(0x5F8C42);
constexpr material::Color kLandTan = hexColor(0xA08A5C);
constexpr material::Color kAtmo = hexColor(0x6FB3D9);

constexpr material::Color kOrbit =
    hexColor(0x4DD0C8);  // current vessel, cyan-teal
constexpr material::Color kOrbitCore = hexColor(0xDCF7F5);
constexpr material::Color kTarget =
    hexColor(0xD8CE7A);  // target orbit, pale yellow
constexpr material::Color kEscape =
    hexColor(0xD9DCE2);  // escape trajectory, white

constexpr material::Color kApLabel = hexColor(0x66D6C8);
constexpr material::Color kPeLabel = hexColor(0xB87CE0);
constexpr material::Color kAnLabel = hexColor(0xB7D66E);

constexpr material::Color kProgradeC =
    hexColor(0x4CD964);  // green  — prograde family
constexpr material::Color kNormalC =
    hexColor(0xB87CE0);  // purple — normal family
constexpr material::Color kRadialC =
    hexColor(0x5AC8E0);  // blue   — radial family

constexpr material::Color kOrange = hexColor(0xDC6F2A);    // sampled (1770,250)
constexpr material::Color kCardBody = hexColor(0xE4E5E7);  // sampled off-white
constexpr material::Color kCardStrip = hexColor(0xF4F4F5);
constexpr material::Color kCardInk = hexColor(0x2A2A2C);
constexpr material::Color kCardSub = hexColor(0x3A3A3E);

constexpr material::Color kLcd =
    hexColor(0x35C93A);  // sampled, two hits averaged
constexpr material::Color kLcdBg = hexColor(0x1C1E20);
constexpr material::Color kLcdVal = hexColor(0xDCEEF2);
constexpr material::Color kAmber = hexColor(0xE3D24A);  // burn readout text

constexpr material::Color kGun = hexColor(0x5E6C77);  // sampled (1893,55)
constexpr material::Color kGunHi = hexColor(0x8D9AA3);
constexpr material::Color kBezel = hexColor(0x9AA2A6);  // navball bezel silver
constexpr material::Color kBezelDk = hexColor(0x555D64);
constexpr material::Color kPanel = hexColor(0x33383E);
constexpr material::Color kPanelDk = hexColor(0x1B1F23);

constexpr material::Color kSky = hexColor(0x1180AC);
constexpr material::Color kSkyHi = hexColor(0x8ED4E8);  // sampled (430,260)
constexpr material::Color kGround = hexColor(0x8B5A2E);
constexpr material::Color kGroundLo = hexColor(0x5A3A1E);
constexpr material::Color kGold = hexColor(0xFCB100);  // sampled

constexpr material::Color kRcs = hexColor(0x73AC43);       // sampled (248,172)
constexpr material::Color kSas = hexColor(0x77A9B0);       // sampled (520,172)
constexpr material::Color kStageTab = hexColor(0xBE5907);  // sampled
constexpr material::Color kFuel = hexColor(0x666C0A);      // sampled
constexpr material::Color kStageLcd =
    hexColor(0xBFBFBF);  // sampled — the LIGHT panel
constexpr material::Color kGo = hexColor(0x4CAF50);
constexpr material::Color kDvArc = hexColor(0x7FE33F);  // the bright burn arc

// ---------------------------------------------------------------------------
// Type — two families, both plain. KSP1 shipped Unity's humanist grotesque;
// the LCD readouts are a distinctly different, monospaced numeral set.

inline sk_sp<SkTypeface> sans() {
  return weave::ports::face({"Helvetica Neue", "Arial"},
                            SkFontStyle::kNormal_Weight);
}
inline sk_sp<SkTypeface> sansB() {
  return weave::ports::face({"Helvetica Neue", "Arial"},
                            SkFontStyle::kBold_Weight);
}
inline sk_sp<SkTypeface> mono() {
  return sketch::kit::houseFace(sketch::kit::Voice::Terminal);
}

inline weave::TextStyle ty(const sk_sp<SkTypeface>& tf, float size,
                           material::Color color, float track = 0) {
  return weave::textStyle({.face = tf,
                           .size = size,
                           .color = material::skia::toSkColor(color),
                           .track = track});
}
inline weave::TextStyle body(float sz, material::Color c, float tr = 0) {
  return ty(sans(), sz, c, tr);
}
inline weave::TextStyle bold(float sz, material::Color c, float tr = 0) {
  return ty(sansB(), sz, c, tr);
}
inline weave::TextStyle lcd(float sz, material::Color c, float tr = 0) {
  return ty(mono(), sz, c, tr);
}
inline Text t(const char* s, weave::TextStyle st) {
  return text(s, std::move(st));
}
/** The same line as a PARTIAL over the font in force where it lands —
 *  the info card's sans 11 in the card ink — naming only what differs. */
inline Text t(const char* s, weave::Type partial) {
  return text(s).font(std::move(partial));
}

/** The readings @p node's list holds, each `{name, value}`. */
inline std::vector<kit::Reading> readings(const data::Json& node) {
  std::vector<kit::Reading> out;
  for (const data::Json& row : node.items())
    out.push_back({.name = row["name"], .value = row["value"]});
  return out;
}

/** A node centred on a canvas point — the marker/gizmo idiom. */
template <class Node>
  requires std::convertible_to<Node, Element>
Node at(Node node, SkPoint c, float w, float h) {
  node.width(w).height(h).centerAt(c);
  return node;
}

// ---------------------------------------------------------------------------
// Orbital mechanics — the conic, in canvas pixels.
//
// `path::Conic` is the curve family: r(v) = p / (1 + e·cos v) about the
// FOCUS, which is where Kerbin is. Screen angles are measured y-DOWN, so
// increasing v runs clockwise on screen; that is exactly what "prograde"
// means for a retrograde-looking 2D map and it is consistent throughout.
// e < 1 closes; e > 1 is the hyperbolic escape.

using path::Conic;
using path::ConicSpan;

/** A span of a conic as a COMPARABLE shape: the curve is a function of
 *  the few numbers it stands on, so those numbers are the key and a node
 *  wearing one settles instead of re-recording on every describe. */
inline Shape trajectory(const Conic& conic, ConicSpan span) {
  return keyedShape(std::pair(conic, span),
                    [conic, span] { return path::conicPath(conic, span); });
}

/** The gizmo hangs off the conic's own directions, and both are wanted as
 *  Skia vectors at the call site. */
inline SkPoint pointAt(const Conic& conic, float anomalyDeg) {
  const glm::vec2 p = conic.at(anomalyDeg);
  return {p.x, p.y};
}
inline float bearingDeg(glm::vec2 direction) {
  return std::atan2(direction.y, direction.x) * 57.29578f;
}

// The scene's three trajectories. Kerbin sits at the shared focus.
constexpr SkPoint kKerbin{500, 330};
constexpr glm::vec2 kFocus{kKerbin.fX, kKerbin.fY};
constexpr float kKerbinR = 140;

// e = 0.42 with a 175 px periapsis: the eccentricity has to be visible or
// the focus placement is a claim nobody can check.
inline Conic currentOrbit() { return {kFocus, 248.5f, 0.42f, 150.0f}; }
inline Conic targetOrbit() { return {kFocus, 507.5f, 0.75f, 270.0f}; }
inline Conic escapeArc() { return {kFocus, 430.0f, 1.55f, 96.0f}; }

constexpr float kNodeNu = -130.0f;  ///< the manoeuvre node's true anomaly

// ---------------------------------------------------------------------------
// The map's glyphs.
//
// EVERY ONE IS A COMPARABLE VALUE. A raw `std::function<SkPath(SkSize)>`
// compares equal to nothing, so every node wearing one re-patches and
// re-records on each describe however still the glyph is; a kit generator
// is a function of the few numbers it holds, so those numbers are the key
// and the node settles on them.
//
// They are the geometry kit's own, which is what makes them
// comparable values a node can settle on: `arrow` with a head of its own
// size for the manoeuvre handles, `ring` for the out-of-plane pair,
// `Circle{.uniform}` where a box is a pixel out of square, `polygon(4)`
// for the map markers and `chevron` for the gold level mark.
//
// THE MARKERS ARE `polygon(4)` AND NOT `polygon(4, 45)`: the kit's polygon
// puts its first vertex UP and steps round the box's own ellipse, so four
// sides unrotated IS the diamond with its vertices on the box's edges.
// Rotated 45 degrees it is the square, whose vertices stand on the box's
// diagonals instead — a different figure and a different size.
//
// THE EYES ARE `Circle{.uniform = true}`: a kerbal's eyes and the pupils
// in them are drawn in boxes a pixel or two taller than they are wide, and
// the default circle is the box's oval, which at that size reads as an
// egg. Everything on a square box is `shapes::circle()` and says so.

/** The manoeuvre-handle PADDLE: a shaft from the hub end out to a
 *  triangular head whose TIP is at the box's right edge, drawn pointing +x
 *  so the arm's rotate() is its bearing. The head is a stated size across
 *  and a stated length along, which the six arms share over boxes of
 *  different lengths — so the head length is the one number that becomes
 *  a fraction of the box the arm happens to be. */
inline Shape paddle(float length) {
  return shapes::arrow(4.5f / 20.0f, 14.0f / length, 15.0f / 20.0f);
}

// ---------------------------------------------------------------------------
// A marching-dot decoration.
//
// PathFormat, lines::Line and Rails all carry a `dashPhaseBinding`, so a
// dash phase can march without re-describing. This decoration exists for
// the other half of the effect: the dots are drawn as ROUND CAPS on their
// own paint, sized independently of the line under them, which a dash
// interval on the line itself cannot express. It is the library's own
// extension seam used as designed — a value DecorationScheme with a bound
// Output and isAnimated() == true.

struct MarchingDots {
  float width = 1.0f;
  material::Color color = {1, 1, 1, 1};
  std::vector<SkScalar> intervals{1.5f, 5.0f};
  const ch::Output<float>* phase = nullptr;
  float speed = 1.0f;  ///< px of phase per unit of the bound output

  bool operator==(const MarchingDots&) const = default;
  bool isAnimated() const { return phase != nullptr; }
  float bleed() const { return width; }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    if (ctx.outline.isEmpty()) return;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setColor4f(material::skia::toSkColor(color), nullptr);
    const float ph = phase ? phase->value() * speed : 0.0f;
    p.setPathEffect(
        SkDashPathEffect::Make(SkSpan(intervals.data(), intervals.size()), ph));
    canvas.drawPath(ctx.outline, p);
  }
};

}  // namespace ksp

// ===========================================================================
