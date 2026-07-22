// ksp_mapview.cpp — Kerbal Space Program (Squad, 1.0 2015; Alpha 2011),
// the stock flight UI: Map View's orbit/manoeuvre-planner screen composited
// with Flight View's navball instrument cluster.
//
// COMPOSITE, NOT A SCREENSHOT. Stock KSP puts these on two view modes (M
// toggles). Nobody ever saw a full orbit ellipse and a navball at once. This
// is the mockup a UI artist or a stream overlay would build — one grammar,
// two screens — and it is stated here rather than implied.
//
// REFERENCES (both read as pixels, not from memory):
//  · ref-images/ksp-mapview-orbit-node.jpg — official Steam store screenshot,
//    appid 220200, 1920×1080. Map View around Kerbin.
//  · ref-images/ksp-flightview-navball.jpg — same set, Flight View, with an
//    active manoeuvre node's burn readout beside the ball.
//
// WHAT THE REFERENCE SAID THAT THE WRITE-UP DID NOT:
//  1. The info-card VALUES are orange and the LABELS are dark. The brief had
//     it as dark-on-white rows with orange section headers only. Look at
//     "Sphere of influence  Kerbin" — "Kerbin" is #DC6F2A. Every value is.
//     That inversion is most of what makes the card read as KSP.
//  2. The right-edge toolbar buttons are ROUNDED SQUARES (~44 px), not
//     circles, and they sit hard against the screen edge with the card
//     tucked left of them.
//  3. The dashed orbit lines are DOTS, not dashes — ~1 px on, ~5 px off on
//     the yellow target orbit and the white escape trajectory. Long dashes
//     read as a diagram; dots read as KSP.
//  4. THROTTLE and G FORCE are CURVED labels running along the navball
//     bezel, not straight ones beside it. Same for their scale numerals.
//     (This is what Element::onPath is for; it is used here for exactly
//     that, plus one arc label riding the orbit line.)
//  5. The burn readout under the ball ("Est. Burn: N/A" / "Node in T + 42m,
//     29s") is YELLOW-AMBER, not neutral grey.
//  6. The staging LiquidFuel bars carry their label INSIDE the olive fill.
//  7. There is no manoeuvre node in the map-view frame at all — the gizmo
//     is licensed by the flight-view frame's active-node readout and by the
//     documented six-axis colour convention, and is drawn from geometry.
//
// GEOMETRY IS REAL. Every orbit here is a conic r = p/(1 + e·cos ν) with
// Kerbin at the FOCUS (not the centre — the brief's centred squircle is the
// one thing a KSP player would notice instantly), sampled in true anomaly.
// The manoeuvre node sits at ν = −130° on the current orbit; prograde is the
// analytic tangent dP/dν there, radial-out is r̂, and the flight-path angle
// between them comes out at ≈113.8° — so normal/antinormal drop into the two
// wide gaps at ±57° and nothing collides. No stylised 60° fan was needed:
// the brief's predicted 12–20° collision is an artefact of picking a node
// near an apsis, not a property of ellipses.
//
// The navball is a real orthographic sphere: one SkSL pass inverts the
// projection per pixel (z = +√(1−r²) picks the FRONT hemisphere, so the back
// is never sampled), undoes roll/pitch/heading, and draws latitude and
// longitude bands as distances to the nearest grid PLANE — which gets the
// limb foreshortening for free and needs no fwidth().
//
// PALETTE: every hex tagged "sampled" below was read off the reference
// pixels; the rest are the documented colour conventions for the six
// manoeuvre axes (green prograde / purple normal / blue radial).
//
// FOUR THINGS THE RENDER TAUGHT THAT NO AMOUNT OF READING WOULD HAVE:
//  · `Material::radialUnit`'s radius is a fraction of the HALF-DIAGONAL, so
//    a planet terminator authored at 1.28 puts the dark end of its ramp
//    entirely OUTSIDE the disc and the shading silently disappears. 1.02
//    is where the ramp finishes at the far limb of an inscribed circle.
//  · `patterns::grain` composited over a NEAR-TRANSPARENT base does not
//    modulate it — it composites as its own luminance. The first nebula
//    came back a white cloud. Grain belongs on opaque surfaces (it is on
//    the altimeter plate here) and the milky way is mottle: eighteen small
//    soft blobs beat one big one, at any alpha.
//  · A `foreground()` hatch paints above the node's CHILDREN, so the stage
//    tab's hazard stripe greyed out its own digit. Moving the hatch to
//    `background()` hides it under the fill instead; the fix is a sibling —
//    striped plate, then the digit, as two children of a stack().
//  · A hyperbola sampled out to its asymptote drops off-canvas points and
//    therefore splits into several contours, and `onPath` only uses the
//    FIRST one — so the escape label rendered nothing until the ν window
//    was hand-fitted to the part that crosses the frame.

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace ksp {

// ---------------------------------------------------------------------------
// Palette

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f, a};
}
inline SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }
inline SkColor4f lift(SkColor4f c, float k) {
  return {std::min(1.0f, c.fR + k), std::min(1.0f, c.fG + k),
          std::min(1.0f, c.fB + k), c.fA};
}

constexpr SkColor4f kSpace = C(0x090A0C);   // sampled
constexpr SkColor4f kSpaceEdge = C(0x06070A);
constexpr SkColor4f kNebula = C(0x6E7288);
constexpr SkColor4f kNebula2 = C(0x525F82);

constexpr SkColor4f kOceanLit = C(0x235274);  // sampled mid-tone
constexpr SkColor4f kOceanDark = C(0x12283A);
constexpr SkColor4f kLandMoss = C(0x5F8C42);
constexpr SkColor4f kLandTan = C(0xA08A5C);
constexpr SkColor4f kAtmo = C(0x6FB3D9);

constexpr SkColor4f kOrbit = C(0x4DD0C8);     // current vessel, cyan-teal
constexpr SkColor4f kOrbitCore = C(0xDCF7F5);
constexpr SkColor4f kTarget = C(0xD8CE7A);    // target orbit, pale yellow
constexpr SkColor4f kEscape = C(0xD9DCE2);    // escape trajectory, white

constexpr SkColor4f kApLabel = C(0x66D6C8);
constexpr SkColor4f kPeLabel = C(0xB87CE0);
constexpr SkColor4f kAnLabel = C(0xB7D66E);

constexpr SkColor4f kProgradeC = C(0x4CD964);  // green  — prograde family
constexpr SkColor4f kNormalC = C(0xB87CE0);    // purple — normal family
constexpr SkColor4f kRadialC = C(0x5AC8E0);    // blue   — radial family

constexpr SkColor4f kOrange = C(0xDC6F2A);     // sampled (1770,250)
constexpr SkColor4f kCardBody = C(0xE4E5E7);   // sampled off-white
constexpr SkColor4f kCardStrip = C(0xF4F4F5);
constexpr SkColor4f kCardInk = C(0x2A2A2C);
constexpr SkColor4f kCardSub = C(0x3A3A3E);

constexpr SkColor4f kLcd = C(0x35C93A);        // sampled, two hits averaged
constexpr SkColor4f kLcdBg = C(0x1C1E20);
constexpr SkColor4f kLcdVal = C(0xDCEEF2);
constexpr SkColor4f kAmber = C(0xE3D24A);      // burn readout text

constexpr SkColor4f kGun = C(0x5E6C77);        // sampled (1893,55)
constexpr SkColor4f kGunHi = C(0x8D9AA3);
constexpr SkColor4f kBezel = C(0x9AA2A6);      // navball bezel silver
constexpr SkColor4f kBezelDk = C(0x555D64);
constexpr SkColor4f kPanel = C(0x33383E);
constexpr SkColor4f kPanelDk = C(0x1B1F23);

constexpr SkColor4f kSky = C(0x1180AC);
constexpr SkColor4f kSkyHi = C(0x8ED4E8);      // sampled (430,260)
constexpr SkColor4f kGround = C(0x8B5A2E);
constexpr SkColor4f kGroundLo = C(0x5A3A1E);
constexpr SkColor4f kGold = C(0xFCB100);       // sampled

constexpr SkColor4f kRcs = C(0x73AC43);        // sampled (248,172)
constexpr SkColor4f kSas = C(0x77A9B0);        // sampled (520,172)
constexpr SkColor4f kStageTab = C(0xBE5907);   // sampled
constexpr SkColor4f kFuel = C(0x666C0A);       // sampled
constexpr SkColor4f kStageLcd = C(0xBFBFBF);   // sampled — the LIGHT panel
constexpr SkColor4f kGo = C(0x4CAF50);
constexpr SkColor4f kDvArc = C(0x7FE33F);      // the bright burn arc

// ---------------------------------------------------------------------------
// Type — two families, both plain. KSP1 shipped Unity's humanist grotesque;
// the LCD readouts are a distinctly different, monospaced numeral set.

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              const char *fallback = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                              SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
inline const sk_sp<SkTypeface> &sans() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kNormal_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &sansB() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kBold_Weight, "Arial");
  return f;
}
inline const sk_sp<SkTypeface> &mono() {
  static sk_sp<SkTypeface> f =
      face("Menlo", SkFontStyle::kNormal_Weight, "Courier New");
  return f;
}

inline sigil::weave::TextStyle ty(const sk_sp<SkTypeface> &tf, float size,
                                  SkColor4f color, float track = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}
inline sigil::weave::TextStyle body(float sz, SkColor4f c, float tr = 0) {
  return ty(sans(), sz, c, tr);
}
inline sigil::weave::TextStyle bold(float sz, SkColor4f c, float tr = 0) {
  return ty(sansB(), sz, c, tr);
}
inline sigil::weave::TextStyle lcd(float sz, SkColor4f c, float tr = 0) {
  return ty(mono(), sz, c, tr);
}
inline Element t(const char *s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

inline Element place(Element e, float x, float y, float w, float h) {
  e.absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
  return e;
}
/** A node centred on a canvas point — the marker/gizmo idiom. */
inline Element at(Element e, SkPoint c, float w, float h) {
  e.absolute().width(Dim(w)).height(Dim(h)).centerAt(c);
  return e;
}

// ---------------------------------------------------------------------------
// Orbital mechanics — the conic, in canvas pixels.
//
// r(ν) = p / (1 + e·cos ν) with the primary at the FOCUS. Screen angles are
// measured y-DOWN, so increasing θ runs clockwise on screen; that is exactly
// what "prograde" means for a retrograde-looking 2D map and it is consistent
// throughout. e < 1 closes; e > 1 is the hyperbolic escape.

struct Conic {
  SkPoint focus{0, 0};
  float p = 200;      ///< semi-latus rectum, px
  float e = 0.3f;     ///< eccentricity
  float argp = 0;     ///< argument of periapsis, screen degrees

  float radius(float nuDeg) const {
    const float nu = nuDeg * 0.017453293f;
    return p / std::max(1e-3f, 1.0f + e * std::cos(nu));
  }
  SkPoint at(float nuDeg) const {
    const float th = (argp + nuDeg) * 0.017453293f;
    const float r = radius(nuDeg);
    return {focus.fX + r * std::cos(th), focus.fY + r * std::sin(th)};
  }
  /** dP/dν, normalised — the prograde direction (direction of travel). */
  SkVector prograde(float nuDeg) const {
    const float nu = nuDeg * 0.017453293f;
    const float th = (argp + nuDeg) * 0.017453293f;
    const float r = radius(nuDeg);
    const float drdnu = r * e * std::sin(nu) / std::max(1e-3f, 1 + e * std::cos(nu));
    SkVector v{drdnu * std::cos(th) - r * std::sin(th),
               drdnu * std::sin(th) + r * std::cos(th)};
    v.normalize();
    return v;
  }
  /** r̂ — radial-OUT, away from the primary. */
  SkVector radialOut(float nuDeg) const {
    const float th = (argp + nuDeg) * 0.017453293f;
    return {std::cos(th), std::sin(th)};
  }
  /** True anomaly of the asymptote (hyperbolae only). */
  float nuInfinity() const {
    return e <= 1 ? 180.0f : std::acos(-1.0f / e) * 57.29578f;
  }
  SkPath path(float nu0, float nu1, int steps) const {
    SkPathBuilder b;
    bool started = false;
    for (int i = 0; i <= steps; ++i) {
      const float nu = nu0 + (nu1 - nu0) * (float)i / (float)steps;
      const SkPoint q = at(nu);
      // A hyperbola runs to infinity; keep the path finite so contour
      // measure (dashes, trim, onPath) stays sane.
      if (std::abs(q.fX) > 6000 || std::abs(q.fY) > 6000) {
        started = false;
        continue;
      }
      if (!started) {
        b.moveTo(q);
        started = true;
      } else {
        b.lineTo(q);
      }
    }
    if (e < 1 && std::abs(nu1 - nu0) >= 359.5f)
      b.close();
    return b.detach();
  }
  std::function<SkPath(SkSize)> outline(float nu0, float nu1,
                                        int steps = 320) const {
    Conic self = *this;
    return [self, nu0, nu1, steps](SkSize) { return self.path(nu0, nu1, steps); };
  }
};

// The scene's three trajectories. Kerbin sits at the shared focus.
constexpr SkPoint kKerbin{500, 330};
constexpr float kKerbinR = 140;

// e = 0.42 with a 175 px periapsis: the eccentricity has to be visible or
// the focus placement is a claim nobody can check.
inline Conic currentOrbit() { return {kKerbin, 248.5f, 0.42f, 150.0f}; }
inline Conic targetOrbit() { return {kKerbin, 507.5f, 0.75f, 270.0f}; }
inline Conic escapeArc() { return {kKerbin, 430.0f, 1.55f, 96.0f}; }

constexpr float kNodeNu = -130.0f;  ///< the manoeuvre node's true anomaly

// ---------------------------------------------------------------------------
// Shape generators the kit does not ship.

/** The manoeuvre-handle PADDLE: a shaft from the hub end out to a triangular
 *  head whose TIP is at the box's right edge, drawn pointing +x so the arm's
 *  rotate() is its bearing. Shapes.h has polygon/star/blob/squircle/arc/
 *  sector/parallelogram and no directional pointer of any kind, so every one
 *  of the six arms is this hand-rolled path (see the gaps list). */
inline std::function<SkPath(SkSize)> paddle(float shaftW, float headW,
                                            float headL) {
  return [shaftW, headW, headL](SkSize s) {
    const float w = s.width(), h = s.height(), cy = h * 0.5f;
    const float hl = std::min(headL, w);
    const float sh = shaftW * 0.5f, hh = headW * 0.5f;
    SkPathBuilder b;
    b.moveTo(0, cy - sh);
    b.lineTo(w - hl, cy - sh);
    b.lineTo(w - hl, cy - hh);
    b.lineTo(w, cy);
    b.lineTo(w - hl, cy + hh);
    b.lineTo(w - hl, cy + sh);
    b.lineTo(0, cy + sh);
    b.close();
    return b.detach();
  };
}

/** Ring + centre dot: normal / antinormal point OUT of the map plane, and an
 *  arrow cannot say that honestly in flat 2D. Two contours, one outline. */
inline std::function<SkPath(SkSize)> ringDot(float ringW, float dotR) {
  return [ringW, dotR](SkSize s) {
    const float cx = s.width() * 0.5f, cy = s.height() * 0.5f;
    const float r = std::min(cx, cy);
    SkPathBuilder b;
    b.addCircle(cx, cy, r);
    b.addCircle(cx, cy, std::max(1.0f, r - ringW), SkPathDirection::kCCW);
    if (dotR > 0)
      b.addCircle(cx, cy, dotR);
    return b.detach();
  };
}

/** The hollow variant of the same glyph — ring only, no dot. */
inline std::function<SkPath(SkSize)> ringOnly(float ringW) {
  return ringDot(ringW, 0);
}

inline std::function<SkPath(SkSize)> circleOutline() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.addCircle(s.width() * 0.5f, s.height() * 0.5f,
                std::min(s.width(), s.height()) * 0.5f);
    return b.detach();
  };
}

/** Small diamond — the Ap/Pe/AN/DN map marker. */
inline std::function<SkPath(SkSize)> diamond() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() * 0.5f, 0);
    b.lineTo(s.width(), s.height() * 0.5f);
    b.lineTo(s.width() * 0.5f, s.height());
    b.lineTo(0, s.height() * 0.5f);
    b.close();
    return b.detach();
  };
}

/** The gold level chevron: KSP's is a wide flat V with two outrigger bars,
 *  read straight off the flight-view frame. */
inline std::function<SkPath(SkSize)> chevron() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float arm = w * 0.20f, drop = h * 0.34f, th = h * 0.16f;
    SkPathBuilder b;
    // the V
    b.moveTo(cx - w * 0.20f, cy - drop * 0.35f);
    b.lineTo(cx, cy + drop);
    b.lineTo(cx + w * 0.20f, cy - drop * 0.35f);
    b.lineTo(cx + w * 0.20f - th * 0.4f, cy - drop * 0.35f - th);
    b.lineTo(cx, cy + drop - th * 1.5f);
    b.lineTo(cx - w * 0.20f + th * 0.4f, cy - drop * 0.35f - th);
    b.close();
    // outrigger bars
    b.addRect({cx - w * 0.5f, cy - th * 0.5f, cx - w * 0.5f + arm, cy + th * 0.5f});
    b.addRect({cx + w * 0.5f - arm, cy - th * 0.5f, cx + w * 0.5f, cy + th * 0.5f});
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// A marching-dot decoration.
//
// PathFormat and lines::Line both carry `dashIntervals`/`dashPhase`, but only
// `trimPhase` accepts a bound ch::Output — a dash phase can only change by
// re-describing. Marching ants on three orbit lines every frame is exactly
// what the declared-volatility rule exists to avoid, so this is the library's
// own extension seam used as designed: a value DecorationScheme with a bound
// Output and animated() == true. It should not have to live in a sketch.

struct MarchingDots {
  float width = 1.0f;
  SkColor4f color = {1, 1, 1, 1};
  std::vector<SkScalar> intervals{1.5f, 5.0f};
  const ch::Output<float> *phase = nullptr;
  float speed = 1.0f;   ///< px of phase per unit of the bound output

  bool operator==(const MarchingDots &) const = default;
  bool animated() const { return phase != nullptr; }
  float bleed() const { return width; }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (ctx.outline.isEmpty())
      return;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setColor4f(color, nullptr);
    const float ph = phase ? phase->value() * speed : 0.0f;
    p.setPathEffect(SkDashPathEffect::Make(
        SkSpan(intervals.data(), intervals.size()), ph));
    canvas.drawPath(ctx.outline, p);
  }
};

// ---------------------------------------------------------------------------
// The navball: an orthographic sphere, evaluated per pixel.
//
// Inverse projection — screen disc → sphere surface → sphere-local frame by
// undoing roll, pitch and heading — then latitude/longitude bands from plane
// distances (exact, and the foreshortening near the limb falls out for free).
// The back hemisphere is never sampled: z = +sqrt(1 − r²) picks the front.
// One monolithic main(), no user-defined SkSL functions, no loops.

inline sk_sp<SkRuntimeEffect> navballEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    const char *src = R"(
uniform float2 uResolution;
uniform float  uYaw;
uniform float  uPitch;
uniform float  uRoll;
uniform float4 uSky;
uniform float4 uSkyHi;
uniform float4 uGnd;
uniform float4 uGndLo;

half4 main(float2 xy) {
  float2 c = uResolution * 0.5;
  float R = min(c.x, c.y);
  float2 q = (xy - c) / R;
  float rr = dot(q, q);
  if (rr > 1.0) { return half4(0.0); }
  float z = sqrt(max(0.0, 1.0 - rr));
  float3 v = float3(q.x, -q.y, z);

  float cr = cos(-uRoll), sr = sin(-uRoll);
  v = float3(v.x * cr - v.y * sr, v.x * sr + v.y * cr, v.z);
  float cp = cos(-uPitch), sp = sin(-uPitch);
  v = float3(v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp);
  float cyw = cos(uYaw), syw = sin(uYaw);
  v = float3(v.x * cyw + v.z * syw, v.y, -v.x * syw + v.z * cyw);

  float lat = asin(clamp(v.y, -1.0, 1.0));
  float lon = atan(v.x, v.z);
  float px  = 1.0 / R;

  float hemi = smoothstep(-0.010, 0.010, v.y);
  float3 sky = mix(uSky.rgb, uSkyHi.rgb, clamp(lat / 1.5707963, 0.0, 1.0));
  float3 gnd = mix(uGnd.rgb, uGndLo.rgb, clamp(-lat / 1.5707963, 0.0, 1.0));
  float3 col = mix(gnd, sky, hemi);

  float s10 = 0.17453293;
  float lk  = floor(lon / s10 + 0.5) * s10;
  float dMe = abs(v.x * cos(lk) - v.z * sin(lk));
  float l90 = floor(lon / 1.5707963 + 0.5) * 1.5707963;
  float dM9 = abs(v.x * cos(l90) - v.z * sin(l90));
  float pk  = floor(lat / s10 + 0.5) * s10;
  float dPa = abs(v.y - sin(pk));
  float p30 = floor(lat / 0.5235988 + 0.5) * 0.5235988;
  float dP3 = abs(v.y - sin(p30));

  float w = 0.85 * px;
  float minor = max(1.0 - smoothstep(w, w * 2.4, dMe),
                    1.0 - smoothstep(w, w * 2.4, dPa));
  float major = max(1.0 - smoothstep(w * 1.5, w * 3.4, dM9),
                    1.0 - smoothstep(w * 1.5, w * 3.4, dP3));
  float horiz = 1.0 - smoothstep(w * 1.7, w * 3.6, abs(v.y));

  col = mix(col, float3(1.0), clamp(minor * 0.34 + major * 0.62, 0.0, 1.0));
  col = mix(col, float3(1.0), horiz * 0.95);

  col *= (0.58 + 0.42 * z);
  float3 L = normalize(float3(-0.42, 0.52, 0.74));
  float spec = pow(max(0.0, dot(L, float3(q.x, -q.y, z))), 14.0);
  col += spec * 0.16;

  float edge = 1.0 - smoothstep(1.0 - 2.2 * px, 1.0, sqrt(rr));
  return half4(half3(col * edge), half(edge));
}
)";
    auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(src));
    if (!e)
      std::fprintf(stderr, "navball sksl: %s\n", err.c_str());
    return e;
  }();
  return fx;
}

/** Restrained bright-pass: luminance above a knee, nothing else. Paired with
 *  a small Gaussian and re-blended kPlus — the photographic half of the CRT
 *  recipe, deliberately without the scanline/backdrop half. */
inline sk_sp<SkRuntimeEffect> brightPassEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    const char *src = R"(
uniform shader content;
half4 main(float2 xy) {
  half4 s = content.eval(xy);
  float a = max(s.a, 1e-4);
  float3 straight = float3(s.rgb) / a;
  float l = max(max(straight.r, straight.g), straight.b);
  float k = smoothstep(0.68, 0.98, l) * float(a);
  return half4(half3(straight * k), half(k));
}
)";
    auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(src));
    if (!e)
      std::fprintf(stderr, "brightpass sksl: %s\n", err.c_str());
    return e;
  }();
  return fx;
}

} // namespace ksp

// ===========================================================================

struct KspMapView : sigil::compose::sketch::Sketch {
  using Out = ch::Output<float>;

  Out dashFast{0}, dashSlow{0};      // marching dots, two rates
  Out hubGlow{6.0f};                 // sdf alive glow, quantised
  Out armPulse{1.0f};                // gizmo idle breathe
  Out jitterX{0}, jitterY{0};        // the "dragged" prograde arm
  Out yaw{0}, pitchOut{0}, rollOut{0};
  Out ringSpin{0};
  Out throttle{0.72f};               // 0..1, tape needle
  Out gforce{0.34f};
  Out fuel0{1}, fuel1{1}, fuel2{1}, fuel3{1};
  Out rcsPulse{1}, goPulse{1};
  Out rollTape{0}, yawTape{0};
  Out planetSpin{0};
  Out dvSweep{0};

  std::shared_ptr<instancing::Atlas> starAtlas;
  std::shared_ptr<instancing::Pool> starPool;
  int burnTick = 0;
  double nextBurnAt = 0;

  // -------------------------------------------------------------------
  // Backdrop: space, nebula, stars

  Element backdrop(sketch::SketchContext &ctx) {
    using namespace ksp;
    const float W = ctx.size.width(), H = ctx.size.height();

    Element g = stack().absolute().inset(0);

    g.child(box().absolute().inset(0).fill(Material::radialUnit(
        {0.42f, 0.45f}, 1.15f, {{0.0f, kSpace}, {1.0f, kSpaceEdge}})));

    // The milky-way band the reference frame is dominated by: three soft
    // blob-shaped ramps on a diagonal, all at single-digit alpha. The first
    // cut ran patterns::grain over this at kOverlay and it came back a
    // white cloud — grain over a near-transparent base composites as its
    // own luminance, not as a modulation of the base. Grain moved to the
    // opaque panels below, which is where it reads as material anyway.
    auto wisp = [&](uint32_t seed, float x, float y, float w, float h,
                    SkColor4f c, float a, float rot) {
      return place(box()
                       .outline(shapes::blob(seed, 0.30f, 9))
                       .fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                                  {{0.0f, fade(c, a)},
                                                   {0.5f, fade(c, a * 0.45f)},
                                                   {1.0f, fade(c, 0.0f)}}))
                       .rotate(rot),
                   x, y, w, h)
          .cache(Cache::Texture)
          .bakeScale(0.4f);
    };
    // Two broad grounds at almost nothing, then a MOTTLE of small blobs
    // along the band. One big soft radial reads as fog; the reference's
    // milky way is mottled, and mottle is what overlapping small ones make.
    g.child(wisp(7u, -160, -70, 980, 400, kNebula, 0.16f, -12));
    g.child(wisp(13u, 340, 10, 1000, 350, kNebula2, 0.14f, -16));
    uint32_t ws = 0x2545F491u;
    auto wr = [&ws] {
      ws ^= ws << 13; ws ^= ws >> 17; ws ^= ws << 5;
      return (float)(ws & 0xffffff) / (float)0xffffff;
    };
    for (int i = 0; i < 16; ++i) {
      const float u = (float)i / 15.0f;
      const float bx = -140.0f + u * 1360.0f + (wr() - 0.5f) * 190.0f;
      const float by = 40.0f + u * 190.0f + (wr() - 0.5f) * 230.0f;
      const float bw = 150.0f + wr() * 260.0f;
      g.child(wisp(41u + (uint32_t)i * 7u, bx, by, bw, bw * (0.5f + wr() * 0.4f),
                   (i % 3 == 0) ? kNebula2 : kNebula, 0.11f + wr() * 0.11f,
                   -30.0f + wr() * 60.0f));
    }
    g.child(wisp(21u, 40, 430, 560, 380, kNebula2, 0.07f, 8));
    g.child(wisp(31u, 680, 380, 600, 440, kNebula, 0.06f, 22));
    (void)H;

    // ~240 stars as ONE atlas stamp, hashed (not a lattice), twinkling by
    // per-instance tint alpha mutated from the ticker.
    g.child(place(box().child(instancing::instances(starAtlas, starPool,
                                                    instancing::Mode::Live)),
                  0, 0, W, H));
    return g;
  }

  // -------------------------------------------------------------------
  // Kerbin — a shaded 2D disc, no 3D anywhere in this sketch.

  Element planet() {
    using namespace ksp;
    const float d = kKerbinR * 2;
    Element g = stack();

    g.child(at(box()
                   .outline(circleOutline())
                   // light offset toward the upper-left: the centre of the
                   // ramp is displaced, which is what fakes sphere shading.
                   .fill(Material::radial({kKerbinR * 0.60f, kKerbinR * 0.50f},
                                          kKerbinR * 1.35f,
                                          {{0.0f, lift(kOceanLit, 0.05f)},
                                           {0.34f, kOceanLit},
                                           {0.72f, C(0x1B4260)},
                                           {1.0f, kOceanDark}}))
                   .clip(),
               kKerbin, d, d)
                .child(box()
                           .absolute()
                           .inset(0)
                           .rotate(&planetSpin)
                           .child(place(box()
                                            .outline(shapes::blob(4u, 0.30f, 9))
                                            .fill(Material::solid(kLandMoss)),
                                        16, 34, 152, 120))
                           .child(place(box()
                                            .outline(shapes::blob(11u, 0.26f, 8))
                                            .fill(Material::solid(kLandTan)),
                                        126, 148, 122, 100))
                           .child(place(box()
                                            .outline(shapes::blob(19u, 0.34f, 7))
                                            .fill(Material::solid(C(0x53803A))),
                                        56, 172, 96, 78))
                           .child(place(box()
                                            .outline(shapes::blob(29u, 0.28f, 8))
                                            .fill(Material::solid(C(0x8E7C4E))),
                                        30, 178, 62, 56)))
                // The terminator: a dark ramp anchored past the lower-right
                // limb, multiplied over land AND ocean alike. Without it the
                // continents float on a flat blue coin.
                .child(box()
                           .absolute()
                           .inset(0)
                           // radiusUnit is a fraction of the HALF-DIAGONAL:
                           // 1.28 put the dark end of the ramp outside the
                           // disc entirely and the terminator vanished.
                           .fill(Material::radialUnit(
                               {0.34f, 0.28f}, 1.02f,
                               {{0.0f, C(0xFFFFFF, 0.0f)},
                                {0.38f, C(0x081420, 0.06f)},
                                {0.70f, C(0x061019, 0.42f)},
                                {1.0f, C(0x03070B, 0.92f)}})))
                // and one hot specular sliver where the star hits the ocean
                .child(box()
                           .absolute()
                           .inset(0)
                           .fill(Material::radialUnit(
                               {0.30f, 0.24f}, 0.42f,
                               {{0.0f, C(0xBFE4F5, 0.13f)},
                                {1.0f, C(0xBFE4F5, 0.0f)}}))
                           .blend(SkBlendMode::kPlus)));

    // Fresnel limb: one SDF pass, border + exponential glow.
    const sdf::Style rim{.fill = {0, 0, 0, 0},
                         .borderWidth = 1.6f,
                         .borderColor = fade(kAtmo, 0.68f),
                         .glowRadius = 8.0f,
                         .glowColor = fade(kAtmo, 0.26f)};
    const float boxSize = sdf::minBoxFor(rim, d);
    g.child(at(box().fill(sdf::material(sdf::circle(), rim)), kKerbin, boxSize,
               boxSize));
    return g;
  }

  // -------------------------------------------------------------------
  // Orbit lines — three real conics.

  Element orbits(sketch::SketchContext &ctx) {
    using namespace ksp;
    const float W = ctx.size.width(), H = ctx.size.height();
    const Conic cur = currentOrbit(), tgt = targetOrbit(), esc = escapeArc();

    Element g = stack().absolute().inset(0);
    auto full = [&](Element e) {
      e.absolute().inset(0).width(Dim(W)).height(Dim(H));
      return e;
    };

    // Target orbit: a huge ellipse whose periapsis arc is all that crosses
    // the frame — the reference's "distant orbit glimpsed as a flat band".
    g.child(full(box()
                     .outline(tgt.outline(-118, 118, 260))
                     .stroke(MarchingDots{.width = 1.4f,
                                          .color = fade(kTarget, 0.80f),
                                          .intervals = {1.6f, 5.4f},
                                          .phase = &dashSlow,
                                          .speed = 1.0f})));
    // …plus its bright solid near-edge, the way the reference frame shows
    // one lit segment of that same orbit crossing the top of the screen.
    g.child(full(box()
                     .outline(tgt.outline(-46, 46, 90))
                     .stroke(PathFormat{.width = 1.5f,
                                        .strokeFill = Fill::color(
                                            fade(kTarget, 0.95f))})));

    // Escape / flyby hyperbola — open, not a closed shape. The window is
    // hand-fitted to the part that crosses the frame: sampling out to the
    // asymptote makes path() drop off-canvas points, which splits the
    // result into several contours, and onPath only uses the FIRST one.
    const float nuA = -104.0f, nuB = 98.0f;
    (void)esc.nuInfinity();
    g.child(full(box()
                     .outline(esc.outline(nuA, nuB, 260))
                     .stroke(MarchingDots{.width = 1.2f,
                                          .color = fade(kEscape, 0.50f),
                                          .intervals = {1.4f, 5.8f},
                                          .phase = &dashFast,
                                          .speed = 0.55f})));
    g.child(full(t("ESCAPE  ·  KERBIN SOI EXIT  T+ 1h 12m",
                   body(8.5f, fade(kEscape, 0.6f), 1.3f))
                     .onPath(TextPath{.path = esc.outline(nuA, nuB, 260),
                                      .at = 0.80f,
                                      .align = TextPath::Align::Center,
                                      .offset = 8.0f,
                                      .autoFlip = true})));

    // The hero: the current orbit, drawn on with a trim reveal and dressed
    // in the organic 4-layer additive glow (a LayeredBrush, not a
    // frame-level effect()).
    g.child(full(box()
                     .outline(cur.outline(0, 360, 360))
                     .trim(0.0f, withFrom(0.0f, 1.0f, {900ms, ch::easeOutQuad}))
                     .stroke(brushes::filament(fade(kOrbit, 0.30f), kOrbitCore,
                                               0.26f))));

    // One arc label riding the orbit itself — shaped once, placed by arc
    // length, per-glyph tangent rotation. (Element::onPath.)
    g.child(full(t("KERBIN  ·  Ap 213,904 m  ·  Pe 88,012 m",
                   body(9.5f, fade(kOrbit, 0.9f), 1.6f))
                     .onPath(TextPath{.path = cur.outline(0, 360, 360),
                                      .at = 0.855f,
                                      .align = TextPath::Align::Center,
                                      .offset = 8.0f,
                                      .autoFlip = true})));
    g.child(full(t("TGT · MUN TRANSFER", body(8.5f, fade(kTarget, 0.85f), 1.4f))
                     .onPath(TextPath{.path = tgt.outline(-118, 118, 260),
                                      .at = 0.30f,
                                      .align = TextPath::Align::Center,
                                      .offset = -9.0f,
                                      .autoFlip = true})));
    return g;
  }

  /** Map marker: diamond + label, the Ap/Pe/AN/DN family. */
  Element marker(const char *label, SkPoint p, SkColor4f c, bool filled) {
    using namespace ksp;
    Element g = stack();
    Element d = at(box().outline(diamond()), p, 9, 9);
    if (filled)
      d.fill(Material::solid(c));
    else
      d.stroke(PathFormat{.width = 1.2f, .strokeFill = Fill::color(c)});
    g.child(d);
    g.child(at(t(label, bold(9.0f, c, 0.6f)), {p.fX + 12, p.fY - 9}, 30, 12));
    return g;
  }

  // -------------------------------------------------------------------
  // The manoeuvre gizmo — the centre of the study.

  Element gizmo() {
    using namespace ksp;
    const Conic cur = currentOrbit();
    const SkPoint hub = cur.at(kNodeNu);
    const SkVector pro = cur.prograde(kNodeNu);
    const SkVector rad = cur.radialOut(kNodeNu);

    const float aPro = std::atan2(pro.fY, pro.fX) * 57.29578f;
    const float aRad = std::atan2(rad.fY, rad.fX) * 57.29578f;
    // Flight-path angle: on a conic, radial and normal only coincide at the
    // apsides. Here the separation comes out ≈110.5°, which leaves two wide
    // gaps for the out-of-plane glyphs — no stylised fan needed.
    float sep = aRad - aPro;
    while (sep < 0) sep += 360;
    while (sep >= 360) sep -= 360;
    const float aNorm = aPro + sep * 0.5f;
    const float aAnti = aNorm + 180.0f;

    Element g = stack().absolute().inset(0).staggerChildren(55ms);

    // one shared builder, two lengths, two fill modes
    auto arm = [&](const char *k, float bearing, float len, SkColor4f c,
                   bool solid, bool jitter) {
      Element e =
          box().width(Dim(len)).height(Dim(20)).outline(paddle(4.5f, 15, 14));
      if (solid)
        e.fill(Material::solid(c));
      else
        e.stroke(PathFormat{.width = 1.8f, .strokeFill = Fill::color(c)})
            .fill(Material::solid(fade(c, 0.22f)));
      const float rad2 = bearing * 0.017453293f;
      e.centerAt({hub.fX + std::cos(rad2) * (len * 0.5f + 9),
                  hub.fY + std::sin(rad2) * (len * 0.5f + 9)})
          .rotate(bearing)
          .absolute()
          .key(k)
          .scale(&armPulse)
          .opacity(withFrom(0.0f, 1.0f, {380ms, ease::outBack()}));
      if (jitter)
        e.translateX(&jitterX).translateY(&jitterY);
      return e;
    };
    auto glyph = [&](const char *k, float bearing, SkColor4f c, bool solid) {
      const float rad2 = bearing * 0.017453293f;
      Element e = box()
                      .width(Dim(18))
                      .height(Dim(18))
                      .outline(solid ? ringDot(2.6f, 3.0f) : ringOnly(2.2f))
                      .fill(Material::solid(solid ? c : fade(c, 0.62f)))
                      .centerAt({hub.fX + std::cos(rad2) * 40,
                                 hub.fY + std::sin(rad2) * 40})
                      .absolute()
                      .key(k)
                      .scale(&armPulse)
                      .opacity(withFrom(0.0f, 1.0f, {380ms, ease::outBack()}));
      return e;
    };
    // the two out-of-plane glyphs ride a short spoke, so the fan reads as
    // six axes off one hub rather than two loose dots
    auto spoke = [&](float bearing, SkColor4f c) {
      const float r2 = bearing * 0.017453293f;
      const SkPoint a{hub.fX + std::cos(r2) * 11, hub.fY + std::sin(r2) * 11};
      const SkPoint b{hub.fX + std::cos(r2) * 31, hub.fY + std::sin(r2) * 31};
      return box()
          .absolute()
          .inset(0)
          .outline([a, b](SkSize) {
            SkPathBuilder p;
            p.moveTo(a);
            p.lineTo(b);
            return p.detach();
          })
          .stroke(PathFormat{.width = 2.0f,
                             .strokeFill = Fill::color(fade(c, 0.75f))});
    };

    g.child(arm("pro", aPro, 54, kProgradeC, true, true));
    g.child(arm("rout", aRad, 42, kRadialC, true, false));
    g.child(spoke(aNorm, kNormalC));
    g.child(glyph("nrm", aNorm, kNormalC, true));
    g.child(arm("ret", aPro + 180, 54, kProgradeC, false, false));
    g.child(arm("rin", aRad + 180, 42, kRadialC, false, false));
    g.child(spoke(aAnti, fade(kNormalC, 0.6f)));
    g.child(glyph("anrm", aAnti, kNormalC, false));

    // Hub: one SDF pass — fill, ring, and a breathing glow bound to a
    // quantised Output (instrument sampling, not a cinematic pulse).
    const sdf::Style hs{.fill = C(0x12181C, 0.72f),
                        .borderWidth = 1.6f,
                        .borderColor = C(0xF0F4F5),
                        .glowRadius = 9.0f,
                        .glowColor = fade(kProgradeC, 0.85f)};
    const float hbox = sdf::minBoxFor(hs, 19.0f);
    Material hm = sdf::material(sdf::circle(), hs);
    hm.uniform("uGlowR", &hubGlow);
    g.child(at(box().fill(std::move(hm)).key("hub"), hub, hbox, hbox));

    // Δv direction stub: the burn vector, drawn from the hub along prograde.
    g.child(box()
                .absolute()
                .inset(0)
                .outline([hub, pro](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(hub);
                  b.lineTo(hub.fX + pro.fX * 96, hub.fY + pro.fY * 96);
                  return b.detach();
                })
                .stroke(lines::Line{.width = 1.2f,
                                    .fill = Fill::color(fade(kProgradeC, 0.5f)),
                                    .dashIntervals = {4, 4}}));
    return g;
  }

  // -------------------------------------------------------------------
  // Δv / burn readout card (the flight-view frame's active-node readout).

  Element burnCard() {
    using namespace ksp;
    return place(box()
                     .column()
                     .padding(9, 8, 9, 8)
                     .gap(3)
                     .fill(Material::solid(C(0x12181C, 0.86f)))
                     .stroke(PathFormat{.width = 1.0f,
                                        .strokeFill = Fill::color(C(0x3A4148))})
                     .child(box()
                                .row()
                                .gap(6)
                                .alignItems(Align::Baseline)
                                .child(t("Δv", body(10.5f, C(0x9AA4AA))))
                                .child(t("164.9", lcd(16, kLcd)))
                                .child(t("m/s", body(10, fade(kLcd, 0.8f)))))
                     .child(slot("burn"))
                     .child(box()
                                .height(Dim(1))
                                .fill(Material::solid(C(0x2C3238)))
                                .margin(2)),
                 646, 566, 190, 88);
  }

  Element burnLines() {
    using namespace ksp;
    char a[64], b[64];
    std::snprintf(a, sizeof a, "Est. Burn: %ds", 38 + (burnTick % 5));
    const int total = 2672 - burnTick;
    std::snprintf(b, sizeof b, "Node in T + %dm, %02ds", total / 60, total % 60);
    return box()
        .column()
        .gap(1)
        .child(t(a, body(10.5f, kAmber)).key("burnA"))
        .child(t(b, body(10.5f, kAmber)).key("burnB"));
  }

  // -------------------------------------------------------------------
  // Vessel info card — flat square corners everywhere, values in ORANGE.

  Element infoRow(const char *label, const char *value) {
    using namespace ksp;
    return box()
        .row()
        .height(Dim(19))
        .alignItems(Align::Center)
        .padding(0, 0, 0, 8)
        .child(t(label, body(11, kCardInk)))
        .child(box().grow(1))
        .child(t(value, bold(11, kOrange)));
  }
  Element infoHead(const char *label) {
    using namespace ksp;
    return box()
        .height(Dim(19))
        .justify(Justify::Center)
        .padding(0, 0, 0, 8)
        .fill(Material::solid(ksp::kCardStrip))
        .child(t(label, bold(11, kOrange, 0.2f)));
  }

  Element infoCard() {
    using namespace ksp;
    return place(box()
                     .column()
                     .fill(Material::solid(kCardBody))
                     .clip()
                     .translateX(withFrom(46.0f, 0.0f, {380ms, ch::easeOutQuad}))
                     .opacity(withFrom(0.0f, 1.0f, {380ms}))
                     .child(box()
                                .height(Dim(26))
                                .justify(Justify::Center)
                                .padding(0, 0, 0, 9)
                                .fill(Material::solid(kOrange))
                                .child(t("Kerbal X", bold(14, C(0xFFFFFF)))))
                     .child(box()
                                .height(Dim(19))
                                .justify(Justify::Center)
                                .padding(0, 0, 0, 9)
                                .fill(Material::solid(kCardSub))
                                .child(t("Info", bold(11, C(0xE8E8EA)))))
                     .child(infoHead("Vessel classification"))
                     .child(box()
                                .row()
                                .padding(4, 0, 4, 8)
                                .gap(8)
                                .child(box()
                                           .width(Dim(34))
                                           .height(Dim(40))
                                           .outline(shapes::polygon(7, 12))
                                           .fill(Material::linearUnit(
                                               {0, 0}, {1, 1},
                                               {{0.0f, C(0xF7F7F8)},
                                                {1.0f, C(0xB9BCC1)}}))
                                           .stroke(PathFormat{
                                               .width = 1.0f,
                                               .strokeFill = Fill::color(
                                                   C(0x8A8E93))}))
                                .child(box()
                                           .column()
                                           .grow(1)
                                           .padding(2, 0, 0, 0)
                                           .child(infoRow("Ship:", "Rocket"))
                                           .child(infoRow("Partcount:", "71"))
                                           .child(infoRow("Total mass:",
                                                          "130.54 t"))))
                     .child(infoRow("Sphere of influence", "Kerbin"))
                     .child(infoRow("Situation", "ORBITING"))
                     .child(infoRow("Flight time", "T+ 00:05:10"))
                     .child(infoHead("Orbital Characteristics"))
                     .child(infoRow("Velocity", "2276.9 m/s"))
                     .child(infoRow("Altitude", "90,834 m"))
                     .child(infoRow("Apoapsis", "213,904 m"))
                     .child(infoRow("Periapsis", "88,012 m"))
                     .child(infoRow("Inclination", "6.4 °"))
                     .child(infoHead("Craft Stats"))
                     .child(infoRow("Max. Acceleration", "21.4 m/s²"))
                     .child(infoRow("Thrust / Weight", "1.63"))
                     .child(box().grow(1))
                     .child(box()
                                .height(Dim(6))
                                .fill(Material::solid(C(0x9DA1A6)))),
                 906, 40, 240, 318);
  }

  Element toolbar() {
    using namespace ksp;
    Element g = stack().absolute().inset(0).staggerChildren(45ms);
    static const char *kGlyphs[6] = {"◉", "◆", "▤", "✱", "▲", "◍"};
    for (int i = 0; i < 6; ++i) {
      const float y = 34.0f + (float)i * 46.0f;
      g.child(place(box()
                        .corners({5})
                        .fill(Material::linearUnit({0, 0}, {0, 1},
                                                   {{0.0f, lift(kGun, 0.10f)},
                                                    {1.0f, C(0x3E4750)}}))
                        .stroke(PathFormat{.width = 1.0f,
                                           .strokeFill = Fill::color(C(0x22282D)),
                                           .align = PathFormat::Align::Inner})
                        .alignItems(Align::Center)
                        .justify(Justify::Center)
                        .opacity(withFrom(0.0f, 1.0f, {260ms}))
                        .scale(withFrom(0.7f, 1.0f, {320ms, ease::outBack()}))
                        .child(t(kGlyphs[i], body(13, C(0xD3DBE0)))),
                    1156, y, 38, 38));
    }
    return g;
  }

  // -------------------------------------------------------------------
  // Mission clock (top-left), matching the reference's pill + MET + icons.

  Element missionClock() {
    using namespace ksp;
    Element g = stack().absolute().inset(0);
    g.child(place(box()
                      .corners({4})
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .fill(Material::solid(C(0x26282C, 0.94f)))
                      .stroke(PathFormat{.width = 1.0f,
                                         .strokeFill = Fill::color(C(0x4A5157))})
                      .child(t("T+ 0y, 0d, 00:05:10", lcd(13, kLcd)).key("met")),
                  18, 14, 200, 28));
    g.child(place(box()
                      .corners({4})
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, lift(kGun, 0.12f)},
                                                  {1.0f, C(0x3E4750)}}))
                      .child(t("MET", bold(11, C(0xE6EAEC)))),
                  222, 14, 40, 28));
    static const char *kIcons[5] = {"◉", "▮▮", "▼", "◍", "◈"};
    for (int i = 0; i < 5; ++i)
      g.child(place(box()
                        .corners({3})
                        .alignItems(Align::Center)
                        .justify(Justify::Center)
                        .fill(Material::solid(C(0x474F57)))
                        .child(t(kIcons[i], body(10, C(0x8CE07A)))),
                    272 + (float)i * 28, 16, 24, 24));
    return g;
  }

  // -------------------------------------------------------------------
  // Instrument cluster

  static constexpr SkPoint kBall{364, 668};
  static constexpr float kBallR = 84;
  static constexpr float kBezelR = 106;

  Element navball() {
    using namespace ksp;
    Element g = stack().absolute().inset(0);

    // Bezel: silver ring, gently lit from the top-left.
    g.child(at(box()
                   .outline(circleOutline())
                   .fill(Material::linearUnit({0.15f, 0}, {0.85f, 1},
                                              {{0.0f, C(0xC8CDD0)},
                                               {0.45f, C(0x8B9296)},
                                               {1.0f, C(0x5A6165)}}))
                   .stroke(PathFormat{.width = 1.2f,
                                      .strokeFill = Fill::color(C(0x2A3034))}),
               kBall, kBezelR * 2, kBezelR * 2));
    g.child(at(box()
                   .outline(circleOutline())
                   .fill(Material::solid(C(0x171B1E))),
               kBall, (kBallR + 5) * 2, (kBallR + 5) * 2));

    // The sphere itself.
    Material ball = Material::sksl(navballEffect(), {});
    ball.uniform("uSky", kSky);
    ball.uniform("uSkyHi", kSkyHi);
    ball.uniform("uGnd", kGround);
    ball.uniform("uGndLo", kGroundLo);
    ball.uniform("uYaw", &yaw);
    ball.uniform("uPitch", &pitchOut);
    ball.uniform("uRoll", &rollOut);
    g.child(at(box().fill(std::move(ball)).key("navball"), kBall, kBallR * 2,
               kBallR * 2));

    // Curved dial tapes on the bezel annulus — THROTTLE left, G FORCE right,
    // exactly as the reference draws them. sector() gives the closed,
    // fillable annular segment; onPath gives the curved lettering.
    auto tape = [&](float startDeg, float sweep, SkColor4f fillC) {
      return at(box()
                    .outline(shapes::sector(startDeg, sweep, 0.845f))
                    .fill(Material::solid(fillC))
                    .stroke(PathFormat{.width = 0.9f,
                                       .strokeFill = Fill::color(C(0x2A3034))}),
                kBall, kBezelR * 2, kBezelR * 2);
    };
    g.child(tape(150, 60, C(0x23282B)));            // throttle body
    g.child(tape(143, 8, C(0xC0392B)));             // hazard cap, top
    g.child(tape(210, 6, C(0x2E7D32)));             // green foot
    g.child(tape(-30, 60, C(0x23282B)));            // g-force body
    g.child(tape(-38, 9, C(0xC0392B)));
    g.child(tape(30, 7, C(0x2E7D32)));

    // Moving needles. The Outputs stay in their OWN units — throttle and
    // g-force are both 0..1 — and bind() maps each onto its tape's arc at
    // the property. Before bind() landed this needed a second Output per
    // gauge carrying degrees, updated in the tick loop.
    g.child(at(box()
                   .outline(shapes::sector(-3.0f, 6.0f, 0.80f))
                   .fill(Material::solid(C(0xF2F4F5)))
                   .rotate(bind(&throttle).to(207, 148))
                   .transformOrigin(0.5f, 0.5f),
               kBall, kBezelR * 2, kBezelR * 2));
    g.child(at(box()
                   .outline(shapes::sector(-3.0f, 6.0f, 0.80f))
                   .fill(Material::solid(C(0xF2F4F5)))
                   .rotate(bind(&gforce).to(28, -33))
                   .transformOrigin(0.5f, 0.5f),
               kBall, kBezelR * 2, kBezelR * 2));

    auto arcLabel = [&](const char *s, float atFrac, float sz, SkColor4f c,
                        float off) {
      return at(t(s, body(sz, c, 1.1f))
                    .onPath(TextPath{.path = circleOutline(),
                                     .at = atFrac,
                                     .align = TextPath::Align::Center,
                                     .offset = off,
                                     .autoFlip = true}),
                kBall, kBezelR * 1.88f, kBezelR * 1.88f);
    };
    g.child(arcLabel("THROTTLE", 0.5f, 8.5f, C(0xE8EDEF), 3));
    g.child(arcLabel("G FORCE", 0.0f, 8.5f, C(0xE8EDEF), 3));
    g.child(arcLabel("100", 0.425f, 7.0f, C(0xB9C2C6), -8));
    g.child(arcLabel("0", 0.575f, 7.0f, C(0xB9C2C6), -8));
    g.child(arcLabel("+15", 0.075f, 7.0f, C(0xB9C2C6), -8));
    g.child(arcLabel("0", 0.925f, 7.0f, C(0xB9C2C6), -8));

    // A heading ring of numerals inside the bezel, rotating with the ball —
    // eight onPath runs sharing one rotating container.
    Element ring = at(box().rotate(&ringSpin).transformOrigin(0.5f, 0.5f), kBall,
                      kBallR * 1.66f, kBallR * 1.66f);
    static const char *kHdg[4] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i)
      ring.child(t(kHdg[i], bold(9.0f, fade(C(0xEAF4F8), 0.85f), 0.6f))
                     .absolute()
                     .inset(0)
                     .onPath(TextPath{.path = circleOutline(),
                                      .at = 0.75f + (float)i / 4.0f,
                                      .align = TextPath::Align::Center,
                                      .offset = 2.0f,
                                      .autoFlip = true}));
    g.child(std::move(ring));

    // The gold level chevron — screen-locked while everything under it turns.
    g.child(at(box().outline(chevron()).fill(Material::solid(kGold)), kBall, 92,
               26));

    // Readouts above and below.
    g.child(place(box()
                      .column()
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .corners({4})
                      .fill(Material::solid(kLcdBg))
                      .stroke(PathFormat{.width = 1.2f,
                                         .strokeFill = Fill::color(C(0x9AA2A6))})
                      .child(t("Orbit", lcd(11, kLcd)))
                      .child(t("1140.0m/s", lcd(13, kLcdVal)).key("spd")),
                  kBall.fX - 68, kBall.fY - kBezelR - 6, 136, 38));
    g.child(place(box()
                      .row()
                      .gap(6)
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .corners({4})
                      .fill(Material::solid(kLcdBg))
                      .stroke(PathFormat{.width = 1.2f,
                                         .strokeFill = Fill::color(C(0x9AA2A6))})
                      .child(t("HDG", body(10, C(0xA9B4B8))))
                      .child(t("280°", lcd(12, kLcd)).key("hdg")),
                  kBall.fX - 52, kBall.fY + kBezelR - 8, 104, 24));

    // RCS / SAS toggles, flanking the ball's shoulders.
    auto toggle = [&](const char *label, SkColor4f c, float x) {
      return place(box()
                       .corners({3})
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .fill(Material::linearUnit({0, 0}, {0, 1},
                                                  {{0.0f, lift(c, 0.14f)},
                                                   {1.0f, c}}))
                       .stroke(PathFormat{.width = 1.0f,
                                          .strokeFill = Fill::color(C(0xE8EDEF, 0.5f)),
                                          .align = PathFormat::Align::Inner})
                       .opacity(&rcsPulse)
                       .child(t(label, bold(10, C(0xFFFFFF)))),
                   x, kBall.fY - kBezelR + 4, 40, 20);
    };
    g.child(toggle("RCS", kRcs, kBall.fX - kBezelR + 6));
    g.child(toggle("SAS", kSas, kBall.fX + kBezelR - 46));

    // The manoeuvre Δv arc riding outside the bezel, plus its tag — this is
    // the flight-view frame's own composition, verbatim.
    g.child(at(box()
                   .outline(shapes::arc(-72, 144))
                   .trim(0.0f, &dvSweep)
                   .stroke(brushes::filament(fade(kDvArc, 0.5f), C(0xEBFFDA),
                                             0.5f)),
               kBall, (kBezelR + 16) * 2, (kBezelR + 16) * 2));
    g.child(place(box()
                      .row()
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .corners({3})
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0xA0A6AA)},
                                                  {1.0f, C(0x6E767B)}}))
                      .gap(5)
                      .child(t("164.9m/s", body(11, C(0x14181A))))
                      .child(box()
                                 .width(Dim(13))
                                 .height(Dim(13))
                                 .corners({2})
                                 .alignItems(Align::Center)
                                 .justify(Justify::Center)
                                 .fill(Material::solid(kStageTab))
                                 .child(t("×", bold(10, C(0xFFFFFF))))),
                  kBall.fX + 92, kBall.fY - kBezelR - 2, 92, 20));
    // bezel index notch, top
    g.child(at(box()
                   .outline(shapes::polygon(3, 180))
                   .fill(Material::solid(C(0xD7DDE0))),
               {kBall.fX, kBall.fY - kBezelR - 4}, 16, 10));
    return g;
  }

  Element staging() {
    using namespace ksp;
    Element g = stack().absolute().inset(0);
    const float x = 14;

    // The hazard stripe is an Sk2D lattice hatch clipped to the tab, not a
    // drawn texture. It has to be a foreground (a background hatch paints
    // UNDER the node's own fill and disappears), so the digit rides a
    // SIBLING drawn after the striped plate rather than a child of it.
    auto stageTab = [&](const char *n, float y) {
      return place(stack(), x, y, 58, 25)
          .child(box()
                     .absolute()
                     .inset(0)
                     .corners({2})
                     .fill(Material::solid(kStageTab))
                     .foreground(lines::hatch(Fill::color(C(0x101010, 0.45f)),
                                              8.0f, 3.4f, -45.0f))
                     .stroke(PathFormat{.width = 1.0f,
                                        .strokeFill = Fill::color(C(0x7A3703)),
                                        .align = PathFormat::Align::Inner}))
          .child(box()
                     .absolute()
                     .inset(0)
                     .alignItems(Align::Center)
                     .justify(Justify::Center)
                     .child(t(n, bold(13, C(0xFFFFFF)))));
    };
    auto partIcon = [&](float py, const char *badge, const char *count) {
      return place(stack()
                       .corners({2})
                       .fill(Material::linearUnit({0, 0}, {0, 1},
                                                  {{0.0f, C(0x767F86)},
                                                   {0.5f, C(0x545D64)},
                                                   {1.0f, C(0x333A3F)}}))
                       .stroke(PathFormat{.width = 1.0f,
                                          .strokeFill = Fill::color(C(0x1D2226)),
                                          .align = PathFormat::Align::Inner})
                       .child(box()
                                  .absolute()
                                  .inset(0)
                                  .alignItems(Align::Center)
                                  .justify(Justify::Center)
                                  .child(t(badge, body(14, C(0xE3E9EC)))))
                       .child(box()
                                  .absolute()
                                  .right(Dim(1))
                                  .bottom(Dim(0))
                                  .child(t(count, bold(8, C(0xF6D488))))),
                   x + 4, py, 27, 27);
    };
    auto fuelBar = [&](float py, const ch::Output<float> *fill) {
      return place(box()
                       .fill(Material::solid(C(0x14181B)))
                       .stroke(PathFormat{.width = 1.0f,
                                          .strokeFill = Fill::color(C(0x3B4147)),
                                          .align = PathFormat::Align::Inner})
                       .clip()
                       // scaleX + transformOrigin: the drain is a transform,
                       // not a re-laid-out width
                       .child(box()
                                  .absolute()
                                  .inset(1)
                                  .fill(Material::linearUnit(
                                      {0, 0}, {0, 1},
                                      {{0.0f, lift(kFuel, 0.10f)},
                                       {1.0f, kFuel}}))
                                  .scaleX(fill)
                                  .transformOrigin(0.0f, 0.5f))
                       .child(box()
                                  .absolute()
                                  .inset(4, 0, 0, 0)
                                  .alignItems(Align::Center)
                                  .child(t("LiquidFuel", body(9, C(0xF0F3F0))))),
                   x + 37, py, 96, 13);
    };

    g.child(stageTab("0", 528));
    g.child(partIcon(557, "▤", "4"));
    g.child(stageTab("1", 592));
    for (int i = 0; i < 4; ++i) {
      const float y = 620.0f + (float)i * 31.0f;
      const ch::Output<float> *f =
          i == 0 ? &fuel0 : i == 1 ? &fuel1 : i == 2 ? &fuel2 : &fuel3;
      g.child(partIcon(y, "▥", i == 0 ? "4" : i == 1 ? "3" : i == 2 ? "2" : "1"));
      g.child(fuelBar(y + 7, f));
    }

    // STAGE cluster: hazard header, green go-button, the ONE inverted LCD.
    Element stage = place(box().column().corners({3}).clip(), x - 6, 756, 152, 38);
    stage.fill(Material::solid(C(0x2A2E31)))
        .stroke(PathFormat{.width = 1.0f,
                           .strokeFill = Fill::color(C(0x4A5157)),
                           .align = PathFormat::Align::Inner});
    stage.child(box()
                    .height(Dim(9))
                    .fill(Material::solid(C(0xE0B720)))
                    .foreground(lines::hatch(Fill::color(C(0x141414, 0.9f)),
                                             8.0f, 4.0f, -45.0f)));
    stage.child(box()
                    .row()
                    .grow(1)
                    .gap(5)
                    .padding(0, 6, 0, 6)
                    .alignItems(Align::Center)
                    .child(box()
                               .width(Dim(15))
                               .height(Dim(15))
                               .outline(circleOutline())
                               .fill(Material::radialUnit(
                                   {0.38f, 0.30f}, 1.0f,
                                   {{0.0f, C(0xE6FDD1)}, {0.5f, kGo},
                                    {1.0f, C(0x2E6E33)}}))
                               .opacity(&goPulse))
                    .child(t("STAGE", bold(9, C(0xE8ECEE))))
                    .child(box().grow(1))
                    .child(box()
                               .row()
                               .gap(2)
                               .children(std::vector<Element>{
                                   digitCell("0"), digitCell("0"),
                                   digitCell("1")})));
    g.child(std::move(stage));

    // ROLL / YAW linear tapes — a railway-tie track + a driven orange tick.
    auto tapeGauge = [&](const char *label, float py,
                         const ch::Output<float> *drive) {
      return place(stack()
                       .corners({2})
                       .fill(Material::solid(C(0x1B1F22)))
                       .stroke(PathFormat{.width = 1.0f,
                                          .strokeFill = Fill::color(C(0x454C51)),
                                          .align = PathFormat::Align::Inner})
                       .clip()
                       .child(box()
                                  .absolute()
                                  .inset(0)
                                  .outline([](SkSize s) {
                                    SkPathBuilder b;
                                    b.moveTo(4, s.height() * 0.5f);
                                    b.lineTo(s.width() - 4, s.height() * 0.5f);
                                    return b.detach();
                                  })
                                  .stroke(lines::Line{
                                      .width = 0.8f,
                                      .fill = Fill::color(C(0x6C767C)),
                                      .tickSpacing = 6.0f,
                                      .tickLength = 9.0f}))
                       .child(box()
                                  .absolute()
                                  .width(Dim(9))
                                  .height(Dim(8))
                                  .top(Dim(0))
                                  .left(Dim(48))
                                  .outline(shapes::polygon(3, 180))
                                  .fill(Material::solid(kStageTab))
                                  .translateX(bind(drive).to(-42, 42)))
                       .child(box()
                                  .absolute()
                                  .left(Dim(4))
                                  .top(Dim(1))
                                  .child(t(label, bold(8, C(0xC7D0D5))))),
                   x + 154, py, 106, 16);
    };
    g.child(tapeGauge("ROLL", 756, &rollTape));
    g.child(tapeGauge("YAW", 778, &yawTape));
    return g;
  }

  Element digitCell(const char *d) {
    using namespace ksp;
    return box()
        .width(Dim(13))
        .height(Dim(17))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.0f, C(0xF2F2F2)}, {1.0f, kStageLcd}}))
        .child(ksp::t(d, lcd(13, C(0x16181A))));
  }

  /** The top-centre altimeter block, straight off the flight-view frame:
   *  a hazard-striped left cheek, an odometer digit run ending in a red
   *  "K" cell, the blue ATMOSPHERE tape, and the round vertical-speed dial
   *  with its gold needle. Every part of it is a generator — hatch stripes,
   *  gradient plate, sector ticks, a rotated sector needle. */
  Element altimeter() {
    using namespace ksp;
    const float X = 430, Y = 6, W = 356, H = 82;
    Element g = place(stack().corners({4}).clip(), X, Y, W, H);
    g.fill(Material::blend(
         {{Material::linearUnit({0, 0}, {0, 1},
                                {{0.0f, C(0xA8AFB4)},
                                 {0.45f, C(0x848D93)},
                                 {1.0f, C(0x4E565C)}}),
           SkBlendMode::kSrcOver},
          // worn-metal luminance: the ONE place grain belongs here
          {patterns::grain(0.9f, 2, 9.0f, 0.3f, 1.0f), SkBlendMode::kSoftLight}}))
        .stroke(PathFormat{.width = 1.2f,
                           .strokeFill = Fill::color(C(0x22282C)),
                           .align = PathFormat::Align::Inner});

    g.child(place(box().fill(Material::solid(C(0xE0B720)))
                      .foreground(lines::hatch(Fill::color(C(0x141414, 0.9f)),
                                               8.0f, 4.0f, -45.0f)),
                  0, 0, 11, H));

    // odometer wheels
    auto wheel = [&](const char *d, float x, bool red) {
      return place(box()
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .fill(Material::linearUnit(
                           {0, 0}, {0, 1},
                           red ? std::vector<Stop>{{0.0f, C(0xE05B4A)},
                                                   {0.5f, C(0xC0392B)},
                                                   {1.0f, C(0x8E2A20)}}
                               : std::vector<Stop>{{0.0f, C(0xFAFAFA)},
                                                   {0.42f, C(0xFFFFFF)},
                                                   {1.0f, C(0xBFBFBF)}}))
                       .stroke(PathFormat{.width = 1.0f,
                                          .strokeFill = Fill::color(C(0x50585E)),
                                          .align = PathFormat::Align::Inner})
                       .child(t(d, lcd(20, red ? C(0xFFFFFF) : C(0x101214)))),
                   x, 8, 26, 34);
    };
    static const char *kDigits[6] = {"0", "0", "2", "1", "1", "3"};
    for (int i = 0; i < 6; ++i)
      g.child(wheel(kDigits[i], 18.0f + (float)i * 28.0f, false));
    g.child(wheel("K", 18.0f + 6 * 28.0f, true));

    // ATMOSPHERE tape
    g.child(place(stack()
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0x2E6E9E)},
                                                  {0.5f, C(0x4E9CC8)},
                                                  {1.0f, C(0x1E4E72)}}))
                      .stroke(PathFormat{.width = 1.0f,
                                         .strokeFill = Fill::color(C(0x18333F)),
                                         .align = PathFormat::Align::Inner})
                      .clip()
                      .child(box()
                                 .absolute()
                                 .inset(0)
                                 .outline([](SkSize s) {
                                   SkPathBuilder b;
                                   b.moveTo(2, s.height() * 0.62f);
                                   b.lineTo(s.width() - 2, s.height() * 0.62f);
                                   return b.detach();
                                 })
                                 .stroke(lines::Line{
                                     .width = 0.9f,
                                     .fill = Fill::color(C(0xE8F4FA, 0.85f)),
                                     .tickSpacing = 5.0f,
                                     .tickLength = 12.0f}))
                      .child(box()
                                 .absolute()
                                 .left(Dim(6))
                                 .top(Dim(1))
                                 .child(t("ATMOSPHERE", bold(8, C(0xEAF4FA), 1.4f))))
                      .child(box()
                                 .absolute()
                                 .width(Dim(9))
                                 .height(Dim(8))
                                 .left(Dim(30))
                                 .top(Dim(0))
                                 .outline(shapes::polygon(3, 180))
                                 .fill(Material::solid(C(0xFFFFFF)))
                                 .translateX(bind(&yawTape).to(0, 190))),
                  18, 48, 238, 22));

    // vertical-speed dial
    const SkPoint dc{300, 42};
    Element dial = stack().absolute().inset(0);
    dial.child(at(box()
                      .outline(circleOutline())
                      .fill(Material::radialUnit({0.4f, 0.32f}, 1.0f,
                                                 {{0.0f, C(0xF2F4F5)},
                                                  {0.7f, C(0xD3D8DB)},
                                                  {1.0f, C(0x9AA2A7)}}))
                      .stroke(PathFormat{.width = 1.4f,
                                         .strokeFill = Fill::color(C(0x33393E))}),
                  dc, 74, 74));
    for (int i = 0; i < 13; ++i)
      dial.child(at(box()
                        .outline(shapes::sector(-1.1f, 2.2f, i % 3 ? 0.82f
                                                                  : 0.72f))
                        .fill(Material::solid(C(0x3A4046)))
                        .rotate(-125.0f + (float)i * 20.8f),
                    dc, 68, 68));
    dial.child(at(t("VERT", bold(6.5f, C(0x4A5157), 0.6f)),
                  {dc.fX + 15, dc.fY - 6}, 26, 9));
    dial.child(at(t("SPD", bold(6.5f, C(0x4A5157), 0.6f)),
                  {dc.fX + 15, dc.fY + 3}, 26, 9));
    dial.child(at(t("100", body(6, C(0x5A6167))), {dc.fX - 5, dc.fY - 26}, 20, 8));
    dial.child(at(t("-100", body(6, C(0x5A6167))), {dc.fX - 5, dc.fY + 26}, 22, 8));
    dial.child(at(box()
                      .outline(shapes::sector(-2.2f, 4.4f, 0.0f))
                      .fill(Material::solid(kGold))
                      .rotate(bind(&gforce).to(-118, 118)),
                  dc, 62, 62));
    dial.child(at(box()
                      .outline(circleOutline())
                      .fill(Material::solid(C(0x33393E))),
                  dc, 7, 7));
    g.child(std::move(dial));
    return g;
  }

  /** The crew portrait plate, bottom-right in the flight-view frame. A
   *  deliberately abstract read of it: helmet sphere, visor sector, name
   *  bar — the composition, not the character art. */
  Element crewPlate() {
    using namespace ksp;
    const float X = 986, Y = 594, W = 178, H = 186;
    Element g = place(stack().corners({3}).clip(), X, Y, W, H);
    g.fill(Material::linearUnit({0, 0}, {0, 1},
                                {{0.0f, C(0x7F878C)}, {1.0f, C(0x454D53)}}))
        .stroke(PathFormat{.width = 1.2f,
                           .strokeFill = Fill::color(C(0x22282C)),
                           .align = PathFormat::Align::Inner});
    g.child(place(box()
                      .fill(Material::radialUnit({0.5f, 0.35f}, 1.1f,
                                                 {{0.0f, C(0x3E4A52)},
                                                  {1.0f, C(0x1A2126)}})),
                  5, 5, W - 10, H - 34));
    // helmet
    g.child(place(box()
                      .outline(circleOutline())
                      .fill(Material::radialUnit({0.36f, 0.28f}, 1.0f,
                                                 {{0.0f, C(0xFFFFFF)},
                                                  {0.5f, C(0xD3D8DB)},
                                                  {1.0f, C(0x7C858B)}})),
                  46, 34, 92, 92));
    // face under the glass: green, because that is the one thing about a
    // kerbal nobody gets wrong
    g.child(place(box()
                      .outline(circleOutline())
                      .fill(Material::radialUnit({0.4f, 0.32f}, 1.0f,
                                                 {{0.0f, C(0x9FC45C)},
                                                  {1.0f, C(0x5F8330)}})),
                  60, 48, 64, 64));
    g.child(place(box().outline(circleOutline()).fill(Material::solid(
                      C(0xF4F4F0))),
                  74, 62, 14, 17));
    g.child(place(box().outline(circleOutline()).fill(Material::solid(
                      C(0xF4F4F0))),
                  96, 62, 14, 17));
    g.child(place(box().outline(circleOutline()).fill(Material::solid(
                      C(0x141414))),
                  78, 68, 6, 7));
    g.child(place(box().outline(circleOutline()).fill(Material::solid(
                      C(0x141414))),
                  100, 68, 6, 7));
    g.child(place(box()
                      .outline(shapes::sector(20, 140, 0.0f))
                      .fill(Material::solid(C(0x2E3A18))),
                  80, 84, 24, 14));
    // the glass itself, over the face
    g.child(place(box()
                      .outline(shapes::sector(150, 240, 0.0f))
                      .fill(Material::linearUnit({0, 0}, {1, 1},
                                                 {{0.0f, C(0xBFE0D8, 0.34f)},
                                                  {0.55f, C(0x6E9A94, 0.10f)},
                                                  {1.0f, C(0x2E4A46, 0.26f)}}))
                      .stroke(PathFormat{.width = 1.4f,
                                         .strokeFill = Fill::color(C(0xE8ECEA))}),
                  54, 42, 76, 76));
    g.child(place(box()
                      .outline(shapes::blob(3u, 0.18f, 7))
                      .fill(Material::linearUnit({0, 0}, {1, 1},
                                                 {{0.0f, C(0xFFFFFF, 0.42f)},
                                                  {1.0f, C(0xFFFFFF, 0.0f)}}))
                      .blend(SkBlendMode::kPlus),
                  58, 44, 40, 34));
    // suit shoulders
    g.child(place(box()
                      .corners({26})
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0xE7E8E4)},
                                                  {1.0f, C(0x9AA0A2)}})),
                  40, 124, 104, 40));
    g.child(place(box()
                      .corners({2})
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0x9AA2A7)},
                                                  {1.0f, C(0x666E74)}}))
                      .child(t("Bill Kerman", body(11, C(0x14181A)))),
                  5, H - 26, W - 10, 21));
    return g;
  }

  Element cluster() {
    using namespace ksp;
    Element g = stack().absolute().inset(0);
    g.child(place(box()
                      .fill(Material::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, C(0x0A0C10, 0.30f)},
                                                  {0.35f, C(0x0A0C10, 0.62f)},
                                                  {1.0f, C(0x0A0C10, 0.74f)}})),
                  0, 512, 528, 288));
    g.child(staging());
    g.child(navball());
    return g;
  }

  // -------------------------------------------------------------------

  /** A vessel/body chip on a trajectory: circular gunmetal disc, glyph,
   *  short label — the reference's "As" marker and the craft icon. */
  Element chip(const char *glyph, const char *label, SkPoint p, SkColor4f ink,
               float r) {
    using namespace ksp;
    Element g = stack();
    g.child(at(box()
                   .outline(circleOutline())
                   .fill(Material::radialUnit({0.38f, 0.30f}, 1.0f,
                                              {{0.0f, C(0xB8C0C6)},
                                               {0.55f, C(0x66707A)},
                                               {1.0f, C(0x2C3238)}}))
                   .stroke(PathFormat{.width = 1.0f,
                                      .strokeFill = Fill::color(C(0x161A1E))})
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(t(glyph, body(r * 0.9f, C(0x0F1316)))),
               p, r * 2, r * 2));
    if (label[0])
      g.child(at(t(label, bold(9, ink, 0.5f)), {p.fX + r + 12, p.fY - 8}, 34, 12));
    return g;
  }

  Element mapLayer(sketch::SketchContext &ctx) {
    using namespace ksp;
    const Conic cur = currentOrbit();
    const Conic tgt = targetOrbit();
    Element g = stack().absolute().inset(0);
    g.child(planet());
    g.child(orbits(ctx));
    g.child(marker("Ap", cur.at(180), kApLabel, true));
    g.child(marker("Pe", cur.at(0), kPeLabel, true));
    g.child(marker("AN", cur.at(100), kAnLabel, false));
    g.child(marker("DN", cur.at(280), kApLabel, false));
    g.child(chip("◗", "Mun", tgt.at(28), kTarget, 12));
    g.child(chip("✦", "", tgt.at(-64), kTarget, 8));
    g.child(gizmo());
    // the craft itself, riding its orbit ahead of the node
    g.child(at(box()
                   .outline(shapes::polygon(3, 90))
                   .fill(Material::solid(C(0xE8F2F4)))
                   .rotate(std::atan2(cur.prograde(-40).fY,
                                      cur.prograde(-40).fX) *
                           57.29578f),
               cur.at(-40), 13, 11));
    return g;
  }

  Element describe(sketch::SketchContext &ctx) {
    using namespace ksp;
    const float W = ctx.size.width(), H = ctx.size.height();

    Element map = mapLayer(ctx);

    // Restrained photographic bloom: bright-pass → small Gaussian → kPlus,
    // over the map layer only (the LCD panels get their own local glow).
    // No scanlines, no backdrop distortion, no tiling.
    Element bloom =
        mapLayer(ctx)
            .effect(Effect::shader(brightPassEffect())
                        .then(Effect::filter(SkImageFilters::Blur(4, 4, nullptr))))
            .blend(SkBlendMode::kPlus)
            .opacity(0.34f);

    return stack()
        .width(Dim(W))
        .height(Dim(H))
        .child(backdrop(ctx))
        .child(std::move(map))
        .child(std::move(bloom))
        .child(burnCard())
        .child(infoCard())
        .child(toolbar())
        .child(missionClock())
        .child(altimeter())
        .child(crewPlate())
        .child(cluster())
        // corner vignette, last
        .child(box()
                   .absolute()
                   .inset(0)
                   .fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                              {{0.50f, C(0x000000, 0.0f)},
                                               {1.0f, C(0x000000, 0.30f)}})));
  }

  // -------------------------------------------------------------------

  void setup(sketch::SketchContext &ctx) override {
    using namespace ksp;
    ctx.canvas(1200, 800);
    ctx.background(kSpace);

    // Starfield: one soft-dot cell, ~240 hashed instances.
    starAtlas = std::make_shared<instancing::Atlas>(2.0f);
    const int dot = starAtlas->cell(
        box().fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                        {{0.0f, C(0xFFFFFF, 1.0f)},
                                         {0.42f, C(0xFFFFFF, 0.55f)},
                                         {1.0f, C(0xFFFFFF, 0.0f)}})),
        {7, 7});
    starPool = std::make_shared<instancing::Pool>();
    uint32_t s = 0x9E3779B9u;
    auto rnd = [&s] {
      s ^= s << 13; s ^= s >> 17; s ^= s << 5;
      return (float)(s & 0xffffff) / (float)0xffffff;
    };
    for (int i = 0; i < 360; ++i) {
      const float px = rnd() * 1200.0f, py = rnd() * 800.0f;
      const float sc = 0.18f + rnd() * rnd() * 0.75f;
      const float a = 0.25f + rnd() * 0.7f;
      SkColor4f tint = C(0xFFFFFF, a);
      const float hue = rnd();
      if (hue > 0.90f) tint = C(0xBFD4FF, a);
      else if (hue < 0.08f) tint = C(0xFFD8B8, a);
      starPool->add({px, py}, dot, 0.0f, sc, tint);
    }

    dashFast = 0; dashSlow = 0; ringSpin = 0; planetSpin = 0;
    burnTick = 0; nextBurnAt = 0;

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float ft = (float)t;
      dashFast = -ft * 22.0f;
      dashSlow = -ft * 14.0f;
      // discretised "instrument sampling": the hub glow steps at 8 Hz
      const float qt = std::floor(ft * 8.0f) / 8.0f;
      hubGlow = 6.0f + 3.4f * (0.5f + 0.5f * std::sin(qt * 4.4f));
      armPulse = 1.0f + 0.055f * std::sin(ft * 3.5f);
      // 6–9 Hz summed-sine jitter on the arm being "dragged"
      jitterX = 1.6f * (std::sin(ft * 41.0f) + 0.6f * std::sin(ft * 27.0f));
      jitterY = 1.6f * (std::sin(ft * 33.0f + 1.1f) + 0.6f * std::sin(ft * 51.0f));
      yaw = ft * 0.14f + 0.9f;
      pitchOut = 0.20f + 0.11f * std::sin(ft * 0.62f);
      rollOut = 0.06f * std::sin(ft * 0.43f);
      ringSpin = -ft * 8.0f * 0.5f;
      planetSpin = ft * 4.0f;
      // gauges stay in THEIR units (0..1); bind() maps them at the property
      throttle = 0.72f + 0.06f * std::sin(ft * 0.9f);
      gforce = 0.34f + 0.10f * std::sin(ft * 1.4f);
      const float drain = std::fmod(ft, 6.0f) / 6.0f;
      fuel0 = 1.00f - 0.15f * drain;
      fuel1 = 0.76f - 0.12f * drain;
      fuel2 = 0.51f - 0.09f * drain;
      fuel3 = 0.27f - 0.06f * drain;
      rcsPulse = 0.86f + 0.14f * (0.5f + 0.5f * std::sin(ft * 5.2f));
      goPulse = 0.82f + 0.18f * (0.5f + 0.5f * std::sin(ft * 4.2f));
      rollTape = 0.5f + 0.42f * std::sin(ft * 2.6f);
      yawTape = 0.5f + 0.42f * std::sin(ft * 2.17f + 2.0f);
      dvSweep = 0.34f + 0.30f * (0.5f + 0.5f * std::sin(ft * 0.8f));
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("burn", burnLines());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    if (elapsed < nextBurnAt)
      return;
    nextBurnAt = elapsed + 0.5;
    burnTick += 1;
    // Only the countdown re-describes; the rest of the tree keeps its caches.
    ctx.composer.renderSlot("burn", burnLines());
  }
};

SIGIL_SKETCH(KspMapView)
