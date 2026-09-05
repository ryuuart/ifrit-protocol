// hitman_verlet.cpp — the Hitman corpse (IO Interactive, 2000)
// and Thomas Jakobsen's verlet character physics, drawn by a pen.
//
// SUBJECT  The corpse / cloth / plant simulation of Hitman: Codename 47
//          (IO Interactive / Eidos Interactive, Microsoft Windows,
//          19 Nov 2000 NA / 1 Dec 2000 EU; Glacier engine; DirectX
//          7.0a; min. Pentium II 300 / 64 MB / 12 MB VRAM), as
//          published by its author four months after ship.
//
// SOURCE   [J01] Thomas Jakobsen, "Advanced Character Physics", GDC 2001
//                Proceedings (San Jose, 20-24 March 2001), IO
//                Interactive, Farvergade 2, DK-1463 Copenhagen K.
//                https://www.cs.cmu.edu/afs/cs/academic/class/15462-s13/www/lec_slides/Jakobsen.pdf
//                Read in full from that PDF (19 pp.); every listing and
//                every quotation below was pulled out of it with
//                pdftotext, not from a secondary re-typing.
//
// DOCUMENTED (all [J01])
//   - x' = 2x - x* + a*dt^2, x* = x; "Keeping the time step fixed"
//   - "By lowering the value 2 to something like 1.99 a small amount of
//     drag can also be introduced to the system" (see VERIFIED 2)
//   - TimeStep() = AccumulateForces(); Verlet(); SatisfyConstraints()
//   - collision by PROJECTION, restitution ZERO, "no need to directly
//     cancel the velocity in the normal direction"
//   - the world is the cube (0,0,0)-(1000,1000,1000); restlength = 100
//   - relaxation: "If we stop the iterations early, the result might not
//     end up being quite valid but because of the Verlet scheme, in next
//     frame it will probably be better, next frame even more so"
//   - cloth: a mesh of triangles, one particle per vertex, one stick per
//     edge, rest length = the initial distance, ONE iteration, exactly
//     ONE particle pinned (m_x[0] = origo)
//   - the sqrt approximation = 1st-order Taylor at r = one Newton-Raphson
//     step; "removes the stiffness that appears otherwise"
//   - plants = cloth + support sticks between vertices sharing a
//     neighbour; ONE iteration "gave the plants exactly the right amount
//     of bending behavior"
//   - rigid body = 4 particles + 6 constraints (4*3-6 = 6 DOF), 3-4 iters
//   - penetration: p = c1*x1 + c2*x2, c1+c2 = 1; D = q-p;
//     lambda = ((q-p).D)/((c1^2+c2^2)*D^2); xi' = xi + ci*lambda*D
//   - HITMAN CORPSES ARE STICK FIGURES, not tetrahedra (Fig. 9). Angular
//     limits are INEQUALITY distance constraints - "for example between
//     the two knees - making sure that the legs never cross". Each stick
//     collides as a CAPPED CYLINDER against world triangles.
//   - motion control: a hit displaces ONE particle; a bomb displaces
//     EVERY particle away from the centre by a distance INVERSELY
//     PROPORTIONAL TO THE SQUARE DISTANCE. Verlet turns both into
//     velocity for free.
//   - IK: pin a particle INSIDE the relaxation loop; invmass = 0 makes it
//     immovable. Used in Hitman for dragging corpses by the hand.
//   - friction: measure d_p BEFORE projecting, then reduce the tangential
//     velocity by k*d_p by modifying x*; never let v_t reverse
//   - "The number of relaxation iterations used in Hitman vary between 1
//     and 10 with the kind of object simulated."
//   - soft constraints repair half the deviation per frame (the paper's
//     own series: 60 -> 80 -> 90 -> 95 -> 97.5)
//   - Hitman.ini: "enableconsole 1" + "consolecmd ip_debug 1";
//     shift+F9 bombs an NPC; K toggles free-cam
//   - "the press oxymoron 'lifelike death animations'"
//
// VERIFIED INDEPENDENTLY BY THIS STUDY (all four printed on the canvas)
//   1. THE STICK CODE IS SIGN-INVERTED in 4 of the paper's 5 stick
//      listings (the standalone (C2) block, stick-in-a-box, cloth, and
//      the mass-weighted variant). With r = 100 and |x2-x1| = 120, the
//      printed "x1 -= delta*0.5*diff; x2 += delta*0.5*diff" gives d = 140
//      and DIVERGES; flipping the two operators gives d = 100 exactly.
//      AND THE REASON IS VISIBLE IN THE PAPER: the fifth listing - the
//      sqrt approximation, the one that shipped in Hitman - carries the
//      SAME two assignment lines and is CORRECT, because its factor
//      "r*r/(d*d+r*r) - 0.5" is already NEGATIVE under tension. The
//      exposition form was written by removing that approximation from
//      the shipped code and the sign went with it.
//   2. "Lowering the value 2 to 1.99" is ORIGIN-DEPENDENT as written:
//      x' = 1.99x - x* + a*dt^2 subtracts 1% of the POSITION, so a
//      particle at rest at x = 500 drifts 5 units per step toward the
//      origin. The intended form lowers both coefficients:
//      x' = 1.99x - 0.99x* + a*dt^2  ==  x + 0.99(x - x*) + a*dt^2.
//      This sketch uses the latter.
//   3. The approximation's denominator is d^2 + r^2 >= r^2 > 0, so it
//      NEVER divides by zero - coincident particles stall instead of
//      producing NaN. The paper's own singularity note (Sec. 7) therefore
//      applies only to the form Hitman did NOT ship. It also
//      under-corrects compression (0.60x at d = r/2) and over-corrects
//      tension (1.20x at d = 2r) - which IS the "removed stiffness".
//   4. FIGURE 9 MEASURED OFF THE SCAN. Page 14 of the CMU scan rendered at
//      600 dpi, thresholded, eroded by a disc of r = 8 px (which
//      annihilates the ~5 px sticks and every body-text stem) leaves
//      exactly SIXTEEN connected components of 620-657 px each - the
//      particle dots, and nothing else. Their centroids are the rest pose
//      below. The topology that confirms: 16 particles, 24 sticks, the
//      neck carrying five of them. The lengths it gives: symmetrising each
//      pair puts the thigh at 0.20569 of figure height, so the paper's own
//      restlength = 100 on the thigh fixes the figure at 486.2 units.
//      Thigh and shank come out 1.91% apart, and the diagram's thigh is
//      the LONGER of the two - the reverse of a human skeleton, whose
//      shank is marginally the longer - so an anthropometric cross-check
//      against Drillis & Contini fails on the diagram's own proportions.
//      [DC66] could not be sourced from a primary scan either (the PSU
//      page carries the figure as an image and no transcription), so that
//      column is dropped, and this is said on the canvas.
//
// WHAT THIS RECONSTRUCTION SHOWS (all reproducible from the sketch)
//   A. ONE RELAXATION ITERATION HAS A SLENDERNESS LIMIT. At this study's
//      gravity a 3-wide braced truss of 24-unit cells, base row pinned,
//      stands at 5 rows (97.4% of nominal height, 15.8% peak constraint
//      error) and FOLDS FLAT at 6 (23.5%, 93%). The plants are sized to
//      that limit. Also: a ONE-dimensional chain of distance constraints
//      cannot be a plant at all, however many skip-one supports it
//      carries - a straight chain hanging DOWNWARD satisfies every one of
//      them, so gravity simply inverts it. "Cloth extended with support
//      sticks" has to mean a strip of cloth, not a line of it.
//   B. THE CLOTH'S SAG IS THE ITERATION COUNT. On ONE pin at ONE
//      iteration (both documented), a 7x7 sheet of 26-unit cells hangs
//      255 units on a 221-unit diagonal - 15% over - at 61% peak
//      constraint error; a 5x5 of 32-unit cells hangs 198 on 181, 9%
//      over, 28% peak. The patch here is sized to the solver.
//   C. ORDER MATTERS AS MUCH AS COUNT. Listed FROM THE PIN, a chain
//      converges in a single Gauss-Seidel sweep and 1, 4 and 10
//      iterations are indistinguishable - the A/B panel measured a
//      NON-monotone mean error that way. Listed from the free end (the
//      generic case; the paper's own cloth listing is row-major over a 2D
//      mesh, which is not dependency-sorted either) the claim holds
//      cleanly: mean e(1) > mean e(4) > mean e(10) on every frame
//      sampled.
//   D. THE FIXED STEP MAKES A CAPTURE REPRODUCIBLE ACROSS FRAME RATES.
//      What is drawn is lerp(x*, x, alpha), and both the step count and
//      alpha follow from accumulated time rather than from the render
//      rate, so one capture instant gives one image at 60, 30 and 20 fps
//      alike. Rates whose frame boundaries straddle a step boundary land
//      the capture one step to the other side of it and differ by a
//      fraction of a level per channel - that is float accumulation in the
//      time budget, not the catch-up clamp. The clamp only engages below
//      kSimHz / maxCatchUp, where a frame owes more steps than it may run
//      and the simulation deliberately falls behind wall-clock time.
//
// RECONSTRUCTED (THE PAPER PUBLISHES NO SIMULATION CONSTANT BUT TWO)
//   - every rest length, from the re-measured Fig. 9 pose, ANCHORED by
//     assigning the paper's own restlength = 100 to the thigh
//   - 60 Hz fixed step; gravity 0.757 units/step^2, derived from a 1.75 m
//     stature (1 unit = 3.600 mm, so the paper's cube is a 3.60 m room)
//   - kFriction 0.14; capsule radius 14 u; knee inequality 100 u; blast
//     constant K = 130,000 u^3; hit displacement 45 u
//   - the 2D reduction: the scheme is dimension-agnostic, only the DOF
//     count changes, and it is printed
//   - the bump, the cloth patch, the plants, all loop timings, all chrome
//     colours, all layout geometry
//
// UNVERIFIABLE / FLAGGED
//   - no source gives Glacier's physics tick rate or its dt
//   - no source gives the corpse's particle count; Fig. 9 is the only
//     evidence and it is a diagram
//   - [DC66] Drillis & Contini 1966 could not be checked against a
//     primary scan; see VERIFIED 4
//
// HOW IT IS DRAWN
//   A verlet body has no velocity variable at all: its shape at frame N
//   is a function of what it TOUCHED at frame N-1, geometry IS the state,
//   and the state is rebuilt sixty times a second. That is a loop, so
//   this is a pen program.
//     * `ctx.ticker.addFixed(60, ..., 8, &alpha)` is the integrator's
//       clock and publishes the render interpolant. A verlet body's state
//       IS the pair (x*, x), so `lerp(x*, x, alpha)` is its OWN
//       interpolant, free; every drawn particle here goes through it.
//     * the corpse's 24 capped-cylinder proxies are the pen's own words -
//       `strokeCap(ROUND)` at twice the capsule radius IS a capsule -
//       and so are the centrelines coloured by live constraint error, the
//       dotted knee inequality and the marching cased drag leader - both
//       `strokeDash`, the pen's own dash - the cloth, the plants and
//       every contact marker.
//     * every panel is a retained tree painted through `pen.element`:
//       the six sidebar panels, the two instancing pools. Three of them
//       are laid out with a HOLE - the anatomy
//       diagram, the relaxation A/B and the bench's second half - which
//       the pen draws into at the box the panel's own arithmetic gives.
//       The tree sets type and lays out; the pen draws what the solver
//       just moved.
//   Nothing goes through `pen.canvas()`: the drafting hatch inside the
//   bump is a `pen.clip()` of the section's own triangle, and the blast's
//   additive glow is the pen's own rect under `fill(paint, SHAPE)`,
//   which gives a falloff authored in a unit square the box it is a unit
//   of - the shape's own bounds - without a leaf standing in to supply
//   one.
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/hitman_verlet.cpp \
//       --frame /tmp/hitman_verlet.png

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/draw/Draw.h>
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

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::draw::Pen;
using sigil::material::skia::Paint;
using sigil::weave::ports::pickTypeface;
namespace draw = sigil::draw;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// Palette — this study's own chrome (a physics-debug register)

constexpr SkColor4f kInk = hex(0x0A0A0C);
constexpr SkColor4f kPanel = hex(0x101116);
constexpr SkColor4f kKeyline = hex(0x191B22);
constexpr SkColor4f kBone = hex(0xE8E6E1);
constexpr SkColor4f kSteel = hex(0x8A8F9C);
constexpr SkColor4f kBlue = hex(0x6FA8DC);
constexpr SkColor4f kRed = hex(0xC8402F);
constexpr SkColor4f kSolid = hex(0x2A2E38);
constexpr SkColor4f kTick = hex(0x5A6070);

// The constraint-error ramp — the study's whole visual thesis.
constexpr float kRampStop[5] = {0.000f, 0.004f, 0.010f, 0.020f, 0.035f};
constexpr SkColor4f kRampCol[5] = {hex(0x4FC79E), hex(0x93C866), hex(0xF2A73B),
                                   hex(0xE2673A), hex(0xC8402F)};

SkColor4f errColor(float e, float alpha = 1.0f) {
  if (e <= kRampStop[0])
    return {kRampCol[0].fR, kRampCol[0].fG, kRampCol[0].fB, alpha};
  for (int i = 1; i < 5; ++i) {
    if (e <= kRampStop[i]) {
      const float u =
          (e - kRampStop[i - 1]) / (kRampStop[i] - kRampStop[i - 1]);
      const SkColor4f &a = kRampCol[i - 1], &b = kRampCol[i];
      return {a.fR + (b.fR - a.fR) * u, a.fG + (b.fG - a.fG) * u,
              a.fB + (b.fB - a.fB) * u, alpha};
    }
  }
  return {kRampCol[4].fR, kRampCol[4].fG, kRampCol[4].fB, alpha};
}

SkColor4f fadeTo(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, c.fA * a}; }

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

sk_sp<SkTypeface> face(const char* family, SkFontStyle style) {
  return pickTypeface({family}, style);
}
sk_sp<SkTypeface> monoFace() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Normal());
  return f;
}
sk_sp<SkTypeface> monoBoldFace() {
  static sk_sp<SkTypeface> f = face("Menlo", SkFontStyle::Bold());
  return f;
}
sk_sp<SkTypeface> uiFace() {
  static sk_sp<SkTypeface> f = face("Helvetica Neue", SkFontStyle::Normal());
  return f;
}
sk_sp<SkTypeface> heavyFace() {
  static sk_sp<SkTypeface> f = [] {
    auto mgr = sigil::weave::ports::systemFontManager();
    sk_sp<SkTypeface> t = mgr->matchFamilyStyle(
        "Helvetica Neue",
        SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kNormal_Width,
                    SkFontStyle::kUpright_Slant));
    return t ? t : face("Helvetica", SkFontStyle::Bold());
  }();
  return f;
}

weave::TextStyle faced(sk_sp<SkTypeface> tf, float size, SkColor4f color,
                       float track = 0.0f) {
  return weave::textStyle(
      {.face = std::move(tf), .size = size, .color = color, .track = track});
}
weave::TextStyle mono(float size, SkColor4f c, float track = 0.0f) {
  return faced(monoFace(), size, c, track);
}
weave::TextStyle monoB(float size, SkColor4f c, float track = 0.0f) {
  return faced(monoBoldFace(), size, c, track);
}
weave::TextStyle ui(float size, SkColor4f c, float track = 0.0f) {
  return faced(uiFace(), size, c, track);
}
Element t(const char* s, weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

/** The same three registers on the PEN: a pen carries one type and one
 *  fill, so a register is set rather than described. */
void penMono(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(weave::Type{.face = monoFace(), .size = size, .track = track});
  pen.fill(c);
}
void penMonoB(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(
      weave::Type{.face = monoBoldFace(), .size = size, .track = track});
  pen.fill(c);
}
void penUi(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(weave::Type{.face = uiFace(), .size = size, .track = track});
  pen.fill(c);
}

/** A part's entrance as time arithmetic: what a described tree spells as
 *  `animate(from(0).to(1), {duration, delay})`, in a loop that has the
 *  clock in its hand. */
float cue(double ms, float delayMs, float durationMs,
          const ch::EaseFn& ease = nullptr) {
  const float u = std::clamp(
      (float)((ms - (double)delayMs) / (double)durationMs), 0.0f, 1.0f);
  return ease ? ease(u) : u;
}

/** A sidebar panel shell. Each panel is its own guest at its own box, so
 *  the entrance the column used to stagger is the panel's own delay. */
Element panel(float height, const char* heading, int order) {
  const auto delay = std::chrono::milliseconds(85 * order);
  return box()
      .column()
      .width(Dim(kColW))
      .height(Dim(height))
      .shrink(0)
      .padding(kPanelPad)
      .gap(7)
      .corners({5})
      .fill(kPanel)
      .clip(true)
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(animate(from(0.0f).to(1.0f),
                       {.duration = 300ms, .delay = delay}))
      .translateX(animate(from(14.0f).to(0.0f),
                          {.duration = 300ms, .delay = delay}))
      .key(std::string("panel") + std::to_string(order))
      .child(t(heading, ui(9.5f, kSteel, 1.9f)).height(Dim(12)).shrink(0));
}

}  // namespace

// ===========================================================================

struct HitmanVerlet final : sketch::DrawSketch {
  // -------------------------------------------------------------------------
  // §4 — the mechanism

  struct Stick {
    int a = 0, b = 0;
    float r = 0, r2 = 0;
  };
  struct Ineq {
    int a = 0, b = 0;
    float minLen = 0;
  };

  struct Body {
    std::vector<SkPoint> x, xo;
    std::vector<float> invm;
    std::vector<Stick> sticks;
    std::vector<Ineq> ineqs;
    int iterations = 1;
    bool worldCollide = false;
    float radius = 0;           // capsule radius, world units
    bool stickContact = false;  // also run the Sec.5 barycentric fix-up
  };

  struct Contact {
    SkPoint p{0, 0}, n{0, 1};
  };

  Body rig, cloth;
  std::array<Body, 4> plants;
  std::array<Body, 3> chains;  // the 1 / 4 / 10 A/B, in PANEL px
  std::vector<Contact> contacts;

  // Named rig indices (order = the enumeration below).
  enum RigIdx {
    HEAD = 0,
    NECK,
    LSH,
    RSH,
    LEL,
    REL,
    LHA,
    RHA,
    LWA,
    RWA,
    LHI,
    RHI,
    LKN,
    RKN,
    LFO,
    RFO,
    NRIG
  };

  // -------------------------------------------------------------------------
  // Clocks and phase

  double loopT = 0;
  uint64_t simSteps = 0;
  bool didHit = false, didBomb = false;
  int phase = 0;  // 0 SPAWN 1 HIT 2 BOMB 3 SETTLE 4 DRAG 5 RELEASE
  SkPoint dragTarget{0, 0}, dragFrom{0, 0};
  bool dragging = false;
  float chainPhase = 0;

  /** THE GUESTS, BUILT ONCE. A guest is retained — the pen keeps a
   *  composer per call site — so a description that says the same thing
   *  every frame is work nobody asked for. Every live number on this
   *  canvas is drawn by the pen, and every pool lane is rewritten in
   *  place, so not one of these trees has to be described twice. */
  Element headerEl, overlayEl, barsEl;
  std::array<Element, 3> colAEl, colBEl;

  // Measured
  float stageMaxErr = 0;
  std::array<float, 3> chainMean{{0, 0, 0}}, chainMax{{0, 0, 0}};
  size_t contactCount = 0;

  // Bindings
  ch::Output<float> alpha{0.0f};       // addFixed's render interpolant
  ch::Output<float> blastPhase{0.0f};  // 1 at detonation, decaying
  ch::Output<float> bodyFade{1.0f};

  // Instancing: the particle dots (the control case) and the sticks (the
  // Pool::sizes() lane).
  std::shared_ptr<instancing::Atlas> dotAtlas;
  std::shared_ptr<instancing::Pool> dotPool;
  std::shared_ptr<instancing::Atlas> barAtlas;
  std::shared_ptr<instancing::Pool> barPool;
  int cellDot = 0, cellPin = 0, cellBar = 0;

  // =========================================================================
  // §4.2 — Verlet. There is no velocity anywhere in this function.

  static void verlet(Body& b, SkPoint gravityStep) {
    for (size_t i = 0; i < b.x.size(); ++i) {
      if (b.invm[i] <= 0.0f) {
        b.xo[i] = b.x[i];
        continue;
      }
      const SkPoint prev = b.x[i];
      // VERIFIED 2: the paper's "2 -> 1.99" lowers ONLY the first
      // coefficient, which subtracts 1% of the POSITION. Both drop here.
      b.x[i] = b.x[i] + (b.x[i] - b.xo[i]) * kDrag + gravityStep;
      b.xo[i] = prev;
    }
  }

  // §4.3(b) — the stick, in the SHIPPED form: the square-root
  // approximation, which is the one listing the paper prints correctly.
  static void satisfyStick(Body& b, const Stick& s) {
    SkPoint d = b.x[s.b] - b.x[s.a];
    // f is NEGATIVE under tension and POSITIVE under compression: the sign
    // the exposition form lost (VERIFIED 1). Denominator >= r2 > 0, so no
    // division by zero is possible (VERIFIED 3).
    const float f = s.r2 / (dot(d, d) + s.r2) - 0.5f;
    d = d * f;
    const float w1 = b.invm[s.a], w2 = b.invm[s.b], sum = w1 + w2;
    if (sum <= 0.0f) return;
    b.x[s.a] = b.x[s.a] - d * (2.0f * w1 / sum);
    b.x[s.b] = b.x[s.b] + d * (2.0f * w2 / sum);
  }

  // §4.3(c) — the inequality constraint: enforced ONLY when too close.
  static void satisfyIneq(Body& b, const Ineq& q) {
    SkPoint d = b.x[q.b] - b.x[q.a];
    const float dd = dot(d, d);
    if (dd >= q.minLen * q.minLen) return;
    const float r2 = q.minLen * q.minLen;
    const float f = r2 / (dd + r2) - 0.5f;
    d = d * f;
    const float w1 = b.invm[q.a], w2 = b.invm[q.b], sum = w1 + w2;
    if (sum <= 0.0f) return;
    b.x[q.a] = b.x[q.a] - d * (2.0f * w1 / sum);
    b.x[q.b] = b.x[q.b] + d * (2.0f * w2 / sum);
  }

  // The world: the paper's cube plus one triangle. Returns the nearest
  // legal position for a point of radius `r`, or the point itself.
  static bool projectWorld(SkPoint p, float r, SkPoint* q) {
    SkPoint out = p;
    bool hit = false;
    if (out.fY < r) {
      out.fY = r;
      hit = true;
    }
    if (out.fX < r) {
      out.fX = r;
      hit = true;
    }
    if (out.fX > 1000.0f - r) {
      out.fX = 1000.0f - r;
      hit = true;
    }
    if (out.fY > 1000.0f - r) {
      out.fY = 1000.0f - r;
      hit = true;
    }
    // The bump, inflated by r: nearest point on the triangle.
    SkPoint onTri = nearestOnTriangle(out, kBumpA, kBumpB, kBumpC);
    SkPoint away = out - onTri;
    const float dist = len(away);
    const bool inside = pointInTriangle(out, kBumpA, kBumpB, kBumpC);
    if (inside || dist < r) {
      SkPoint n = dist > 1e-4f ? away * (1.0f / dist) : SkPoint{0, 1};
      if (inside) n = n * -1.0f;
      out = onTri + n * r;
      hit = true;
    }
    *q = out;
    return hit;
  }

  static SkPoint nearestOnSegment(SkPoint p, SkPoint a, SkPoint b) {
    const SkPoint ab = b - a;
    const float dd = dot(ab, ab);
    if (dd < 1e-6f) return a;
    const float t = std::clamp(dot(p - a, ab) / dd, 0.0f, 1.0f);
    return a + ab * t;
  }
  static SkPoint nearestOnTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c) {
    const SkPoint p0 = nearestOnSegment(p, a, b);
    const SkPoint p1 = nearestOnSegment(p, b, c);
    const SkPoint p2 = nearestOnSegment(p, c, a);
    const float d0 = len(p - p0), d1 = len(p - p1), d2 = len(p - p2);
    if (d0 <= d1 && d0 <= d2) return p0;
    return d1 <= d2 ? p1 : p2;
  }
  static bool pointInTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c) {
    const float s1 = cross2(b - a, p - a), s2 = cross2(c - b, p - b),
                s3 = cross2(a - c, p - c);
    const bool neg = s1 < 0 || s2 < 0 || s3 < 0;
    const bool pos = s1 > 0 || s2 > 0 || s3 > 0;
    return !(neg && pos);
  }
  static float cross2(SkPoint a, SkPoint b) {
    return a.fX * b.fY - a.fY * b.fX;
  }

  /** §4.3(a) — projection, with §7 friction. The penetration depth is
   *  measured BEFORE the projection, or friction has nothing to scale by. */
  void collideWorld(Body& b, bool record) {
    for (size_t i = 0; i < b.x.size(); ++i) {
      if (b.invm[i] <= 0.0f) continue;
      SkPoint q;
      if (!projectWorld(b.x[i], b.radius, &q)) continue;
      const SkPoint delta = q - b.x[i];
      const float dp = len(delta);
      if (dp < 1e-5f) continue;
      const SkPoint n = delta * (1.0f / dp);
      b.x[i] = q;  // restitution ZERO: clamp, never reflect
      // §7 friction: reduce the TANGENTIAL velocity by k*dp, by moving x*.
      const SkPoint v = b.x[i] - b.xo[i];
      const SkPoint vt = v - n * dot(v, n);
      const float m = len(vt);
      if (m > 1e-6f) {
        const float reduce = std::min(m, kFriction * dp);
        b.xo[i] = b.xo[i] + vt * (reduce / m);  // never reverses: clamped
      }
      if (record && contacts.size() < 40) contacts.push_back({q, n});
    }
  }

  /** §5 — the capped cylinder against the world. The contact point lies ON
   *  the stick, so it is a barycentric blend and the correction
   *  distributes by the paper's own formula. Written as printed. */
  void collideSticks(Body& b, bool record) {
    for (const Stick& s : b.sticks) {
      const float c1 = 0.5f, c2 = 0.5f;
      const SkPoint p = b.x[s.a] * c1 + b.x[s.b] * c2;
      SkPoint q;
      if (!projectWorld(p, b.radius, &q)) continue;
      const SkPoint D = q - p;
      const float dd = dot(D, D);
      if (dd < 1e-8f) continue;
      const float lambda = dot(q - p, D) / ((c1 * c1 + c2 * c2) * dd);
      const float w1 = b.invm[s.a], w2 = b.invm[s.b];
      if (w1 > 0) b.x[s.a] = b.x[s.a] + D * (c1 * lambda);
      if (w2 > 0) b.x[s.b] = b.x[s.b] + D * (c2 * lambda);
      if (record && contacts.size() < 40)
        contacts.push_back({q, D * (1.0f / std::sqrt(dd))});
    }
  }

  /** §4.3 — SatisfyConstraints. Projection lives INSIDE the relaxation
   *  loop, so a stick fix-up that shoves a particle back into the floor is
   *  projected out again on the next iteration. That interleaving IS the
   *  algorithm. */
  void satisfy(Body& b, bool record) {
    for (int it = 0; it < b.iterations; ++it) {
      const bool rec = record && it == b.iterations - 1;
      if (b.worldCollide) {
        collideWorld(b, rec);
        if (b.stickContact) collideSticks(b, rec);
      }
      for (const Stick& s : b.sticks) satisfyStick(b, s);
      for (const Ineq& q : b.ineqs) satisfyIneq(b, q);
      // §7 IK: keep setting the position INSIDE the loop.
      if (&b == &rig && dragging) b.x[LHA] = dragTarget;
    }
  }

  // =========================================================================
  // Construction

  static SkPoint world(Norm n, float side, SkPoint origin) {
    return {origin.fX + n.x * side * kFigureH, origin.fY + n.y * kFigureH};
  }

  void addStick(Body& b, int i, int j) {
    const float r = len(b.x[j] - b.x[i]);
    b.sticks.push_back({i, j, r, r * r});
  }

  void buildRig(SkPoint feet) {
    rig = Body{};
    rig.iterations = 4;  // documented range 1-10; 3-4 for rigid bodies
    rig.worldCollide = true;
    rig.stickContact = true;
    rig.radius = kCapsule;
    auto P = [&](Norm n, float side) { return world(n, side, feet); };
    rig.x = {P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1),
             P(kEl, -1),  P(kEl, +1),  P(kHa, -1), P(kHa, +1),
             P(kWa, -1),  P(kWa, +1),  P(kHi, -1), P(kHi, +1),
             P(kKn, -1),  P(kKn, +1),  P(kFo, -1), P(kFo, +1)};
    rig.xo = rig.x;  // x* = x  ->  zero velocity at spawn
    rig.invm.assign(NRIG, 1.0f);
    // The 24 sticks, in the paper's five groups (re-counted at 600 dpi).
    addStick(rig, HEAD, NECK);  // 1
    addStick(rig, NECK, LSH);
    addStick(rig, NECK, RSH);  // 2
    addStick(rig, LSH, RSH);   // 1  shoulder bar
    addStick(rig, NECK, LWA);
    addStick(rig, NECK, RWA);  // 2  long braces
    addStick(rig, LSH, LWA);
    addStick(rig, RSH, RWA);  // 2  side
    addStick(rig, LSH, RWA);
    addStick(rig, RSH, LWA);  // 2  crossed
    addStick(rig, LWA, RWA);  // 1  waist bar
    addStick(rig, LWA, LHI);
    addStick(rig, RWA, RHI);  // 2  side
    addStick(rig, LWA, RHI);
    addStick(rig, RWA, LHI);  // 2  crossed
    addStick(rig, LHI, RHI);  // 1  hip bar
    addStick(rig, LSH, LEL);
    addStick(rig, RSH, REL);  // 2
    addStick(rig, LEL, LHA);
    addStick(rig, REL, RHA);  // 2
    addStick(rig, LHI, LKN);
    addStick(rig, RHI, RKN);  // 2  THE ANCHOR
    addStick(rig, LKN, LFO);
    addStick(rig, RKN, RFO);  // 2
    // The documented inequality: "between the two knees - making sure that
    // the legs never cross".
    rig.ineqs.push_back({LKN, RKN, kKneeMin});
  }

  void buildCloth() {
    cloth = Body{};
    cloth.iterations = 1;  // documented
    // MEASURED, on ONE pin at ONE iteration (both documented): a 7x7 sheet
    // of 26-unit cells hangs 255 units on a 221-unit diagonal - 15% over -
    // with 61% peak constraint error. A 5x5 of 32-unit cells hangs 198 on
    // 181, 9% over, 28% peak. The load per stick is what one iteration
    // cannot carry, so the patch is sized to the solver, not the reverse.
    constexpr int C = 5, R = 5;
    constexpr float sp = 32.0f;
    const float x0 = 776.0f, y0 = 706.0f;
    cloth.worldCollide = true;
    cloth.radius = 3.0f;
    auto idx = [&](int c, int r) { return r * C + c; };
    for (int r = 0; r < R; ++r)
      for (int c = 0; c < C; ++c)
        cloth.x.push_back(
            {x0 + (float)c * sp + (((unsigned)r & 1u) ? sp * 0.5f : 0.0f),
             y0 - (float)r * sp});
    cloth.xo = cloth.x;
    cloth.invm.assign(cloth.x.size(), 1.0f);
    cloth.invm[0] = 0.0f;  // "Constrain one particle of the cloth to origo"
    for (int r = 0; r < R; ++r) {
      for (int c = 0; c < C; ++c) {
        if (c + 1 < C) addStick(cloth, idx(c, r), idx(c + 1, r));
        if (r + 1 < R) {
          addStick(cloth, idx(c, r), idx(c, r + 1));
          const int dc = ((unsigned)r & 1u) ? c + 1 : c - 1;
          if (dc >= 0 && dc < C) addStick(cloth, idx(c, r), idx(dc, r + 1));
        }
      }
    }
  }

  /** §4 — "By placing support sticks between strategically chosen couples
   *  of vertices sharing a neighbor, the cloth algorithm can be extended to
   *  simulate plants." Read literally as a ONE-dimensional chain it cannot
   *  work, and this is worth stating: a straight chain hanging DOWNWARD
   *  satisfies every distance constraint it has, including every skip-one
   *  support, so gravity simply inverts it. A plant is a narrow strip of
   *  the CLOTH — two columns, every quad carrying a diagonal — pinned at
   *  its base row. That is a truss, and it stands. */
  void buildPlants() {
    const float roots[4] = {664.0f, 736.0f, 808.0f, 880.0f};
    for (int p = 0; p < 4; ++p) {
      Body& b = plants[(size_t)p];
      b = Body{};
      b.iterations = 1;  // documented — "exactly the right amount of bending"
      b.worldCollide = true;
      b.radius = 3.0f;
      // A patch of cloth three wide and five tall, fully braced, base row
      // pinned. Slenderness is the whole design question here, because ONE
      // relaxation iteration has a slenderness limit: at this study's
      // gravity a 3-wide braced truss of 24-unit cells stands at five rows
      // (near its nominal height, and holding) and FOLDS FLAT at six. A
      // 2-wide, 7-tall strip is a wet noodle from the first second. That is
      // a fact about aspect ratio and iteration count, not about the paper.
      constexpr int R = 5, C = 3;
      constexpr float sp = 24.0f, w = 28.0f;
      const float lean = (p % 2 == 0) ? 2.2f : -2.8f;
      auto id = [](int r, int c) { return r * C + c; };
      for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
          b.x.push_back(
              {roots[p] + lean * (float)r + (float)c * w, (float)r * sp});
      b.xo = b.x;
      b.invm.assign(b.x.size(), 1.0f);
      for (int c = 0; c < C; ++c)
        b.invm[(size_t)id(0, c)] = 0.0f;  // the base row is the root
      for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
          if (c + 1 < C) addStick(b, id(r, c), id(r, c + 1));
          if (r + 1 < R) {
            addStick(b, id(r, c), id(r + 1, c));
            // the documented support sticks, "between strategically chosen
            // couples of vertices sharing a neighbor" — both diagonals, so
            // every cell is a braced truss and the plant stands up
            if (c + 1 < C) addStick(b, id(r, c), id(r + 1, c + 1));
            if (c > 0) addStick(b, id(r, c), id(r + 1, c - 1));
          }
        }
    }
  }

  // The A/B chains live in PANEL pixels — their gravity is a panel
  // constant, not the world's.
  static constexpr float kChainRest = 12.0f;
  static constexpr float kChainG = 0.11f;
  void buildChains() {
    const int iters[3] = {1, 4, 10};
    const float xs[3] = {60.0f, 162.0f, 264.0f};
    for (int k = 0; k < 3; ++k) {
      Body& b = chains[(size_t)k];
      b = Body{};
      b.iterations = iters[k];
      constexpr int N = 12;
      for (int i = 0; i < N; ++i)
        b.x.push_back({xs[k], 16.0f + (float)i * kChainRest});
      b.xo = b.x;
      b.invm.assign(N, 1.0f);
      b.invm[0] = 0.0f;
      // ORDER MATTERS, and it is the whole reason the paper talks about
      // iteration counts at all: listed FROM THE PIN a chain converges in a
      // single Gauss-Seidel sweep and 1, 4 and 10 are indistinguishable.
      // A real constraint list has no reason to be dependency-sorted -
      // the paper's own cloth listing is row-major over a 2D mesh, which
      // is not one - so these are listed from the FREE END.
      for (int i = N - 2; i >= 0; --i)
        b.sticks.push_back({i, i + 1, kChainRest, kChainRest * kChainRest});
    }
  }

  // =========================================================================
  // §7 — motion control, documented as laws, reconstructed as numbers.

  void applyBlast(Body& b, SkPoint c) {
    for (size_t i = 0; i < b.x.size(); ++i) {
      if (b.invm[i] <= 0.0f) continue;
      SkPoint d = b.x[i] - c;
      const float r = std::max(12.0f, len(d));
      // |dx| = K / r^2  ->  dx = K (x-c) / r^3
      SkPoint push = d * (kBlastK / (r * r * r));
      const float m = len(push);
      if (m > 45.0f) push = push * (45.0f / m);
      b.x[i] = b.x[i] + push;  // a POSITION displacement; verlet does the rest
    }
  }

  float maxError(const Body& b) const {
    float e = 0;
    for (const Stick& s : b.sticks)
      e = std::max(e, std::abs(len(b.x[s.b] - b.x[s.a]) - s.r) / s.r);
    return e;
  }
  void chainStats(const Body& b, float* mean, float* mx) const {
    float sum = 0, m = 0;
    for (const Stick& s : b.sticks) {
      const float e = std::abs(len(b.x[s.b] - b.x[s.a]) - s.r) / s.r;
      sum += e;
      m = std::max(m, e);
    }
    *mean = b.sticks.empty() ? 0 : sum / (float)b.sticks.size();
    *mx = m;
  }

  // =========================================================================
  // The fixed step. Every constant above is per-60-Hz-step.

  void stepPhysics() {
    loopT += 1.0 / kSimHz;
    if (loopT >= kLoop) {
      loopT -= kLoop;
      buildRig({300.0f, kCapsule});
      didHit = didBomb = false;
      dragging = false;
    }
    contacts.clear();

    // Phase machine: the §7 motion-control events, on this study's schedule.
    if (!didHit && loopT >= 1.10) {
      rig.x[RSH].fX -= kHitPush;  // documented: displace ONE particle
      didHit = true;
    }
    if (!didBomb && loopT >= 2.60) {
      const SkPoint c = kBlast;
      applyBlast(rig, c);
      applyBlast(cloth, c);
      for (Body& p : plants) applyBlast(p, c);
      didBomb = true;
      blastPhase = 1.0f;
    }
    const bool wantDrag = loopT >= 6.20 && loopT < 9.40;
    if (wantDrag && !dragging) {
      dragFrom = rig.x[LHA];
      dragging = true;
    }
    if (!wantDrag) dragging = false;
    if (dragging) {
      const float u = std::clamp((float)((loopT - 6.20) / 3.20), 0.0f, 1.0f);
      const float e = ch::easeInOutCubic(u);
      const SkPoint to{120.0f, 30.0f};
      dragTarget = dragFrom + (to - dragFrom) * e;
    }
    phase = loopT < 1.10   ? 0
            : loopT < 2.60 ? 1
            : loopT < 3.10 ? 2
            : loopT < 6.20 ? 3
            : loopT < 9.40 ? 4
                           : 3;

    const SkPoint g{0.0f, -kGravityStep};
    verlet(rig, g);
    satisfy(rig, true);
    verlet(cloth, g);
    satisfy(cloth, false);
    for (Body& p : plants) {
      verlet(p, g);
      satisfy(p, false);
    }
    // The A/B: same gravity, same drag, same initial condition, three
    // iteration counts. Drive the pins together so they never settle.
    chainPhase += (float)(1.0 / kSimHz);
    const float px = 26.0f * std::sin(chainPhase * 6.2831853f / 1.8f);
    const SkPoint cg{0.0f, kChainG};
    const float xs[3] = {60.0f, 162.0f, 264.0f};
    for (int k = 0; k < 3; ++k) {
      chains[(size_t)k].x[0] = {xs[k] + px, 16.0f};
      verlet(chains[(size_t)k], cg);
      satisfy(chains[(size_t)k], false);
      float mn = 0, mx = 0;
      chainStats(chains[(size_t)k], &mn, &mx);
      // Half-second exponential mean: the claim is about the solver, not
      // about which frame the capture happens to land on.
      chainMean[(size_t)k] = chainMean[(size_t)k] * 0.967f + mn * 0.033f;
      chainMax[(size_t)k] = std::max(chainMax[(size_t)k] * 0.967f, mx);
    }

    stageMaxErr = maxError(rig);
    contactCount = contacts.size();
    ++simSteps;
    blastPhase =
        std::max(0.0f, blastPhase.value() - (float)(1.0 / kSimHz) / 0.34f);
    const float fadeU = (float)((loopT - 10.40) / 0.60);
    bodyFade = loopT >= 10.40 ? std::clamp(1.0f - fadeU, 0.0f, 1.0f) : 1.0f;
  }

  // =========================================================================
  // Drawing. World y is UP, stage y is DOWN.

  static SkPoint toStage(SkPoint w) {
    return {w.fX * kUnit, (1000.0f - w.fY) * kUnit};
  }
  /** The integrator's OWN interpolant: a verlet body's state IS (x*, x),
   *  so lerp(x*, x, alpha) is the position at t_prev + alpha*dt. Free. */
  SkPoint drawnWorld(const Body& b, size_t i) const {
    const float a = alpha.value();
    return b.xo[i] + (b.x[i] - b.xo[i]) * a;
  }
  SkPoint drawn(const Body& b, size_t i) const {
    return toStage(drawnWorld(b, i));
  }

  /** The rig's stage-space bounds, so the two A/B thumbnails can FIT it
   *  rather than assume where it is. */
  SkRect rigBounds() const {
    SkRect r = SkRect::MakeLTRB(1e9f, 1e9f, -1e9f, -1e9f);
    for (size_t i = 0; i < rig.x.size(); ++i) {
      const SkPoint p = drawn(rig, i);
      r.fLeft = std::min(r.fLeft, p.fX);
      r.fTop = std::min(r.fTop, p.fY);
      r.fRight = std::max(r.fRight, p.fX);
      r.fBottom = std::max(r.fBottom, p.fY);
    }
    return r.makeOutset(12, 12);
  }
  /** Fit-to-cell: the transform that maps rigBounds() into a w x h cell. */
  void rigFit(float w, float h, float* scale, SkPoint* offset) const {
    const SkRect b = rigBounds();
    const float s =
        std::min(w / std::max(1.0f, b.width()), h / std::max(1.0f, b.height()));
    *scale = s;
    *offset = {(w - b.width() * s) * 0.5f - b.fLeft * s,
               (h - b.height() * s) * 0.5f - b.fTop * s};
  }

  /** The corpse's 24 sticks, three layers, all in the pen's own words on
   *  geometry recomputed this frame. A ROUND CAP AT TWICE THE CAPSULE
   *  RADIUS IS A CAPPED CYLINDER — the collision proxy the paper
   *  describes, made visible by the stroke that already draws it. */
  void paintRig(Pen& pen, float scale, SkPoint offset, float fade,
                bool proxies) const {
    auto at = [&](size_t i) {
      const SkPoint p = drawn(rig, i);
      return SkPoint{offset.fX + p.fX * scale, offset.fY + p.fY * scale};
    };
    pen.noFill();
    pen.strokeCap(draw::ROUND);
    if (proxies) {
      // 1. The capped-cylinder proxies — the collision geometry the paper
      //    describes, at the width the solver actually uses.
      pen.strokeWeight(2.0f * kCapsule * kUnit * scale);
      pen.stroke(hex(0x6FA8DC, 0.10f * fade));
      for (const Stick& s : rig.sticks) {
        const SkPoint a = at((size_t)s.a), b = at((size_t)s.b);
        pen.line(a.fX, a.fY, b.fX, b.fY);
      }
    }
    // 2. The centrelines, coloured by LIVE constraint error.
    pen.strokeWeight(std::max(4.6f, 2.5f * scale));
    for (const Stick& s : rig.sticks) {
      const float e = std::abs(len(rig.x[s.b] - rig.x[s.a]) - s.r) / s.r;
      pen.stroke(errColor(e, fade));
      const SkPoint a = at((size_t)s.a), b = at((size_t)s.b);
      pen.line(a.fX, a.fY, b.fX, b.fY);
    }
    // 3. The inequality constraint — dotted, as Figure 8 draws it.
    pen.strokeWeight(1.4f * scale);
    pen.stroke(hex(0xC8402F, 0.75f * fade));
    pen.strokeDash({2.5f, 3.5f});
    const SkPoint lk = at(LKN), rk = at(RKN);
    pen.line(lk.fX, lk.fY, rk.fX, rk.fY);
    pen.noDash();
    pen.noStroke();
  }

  // =========================================================================
  // The particle pool (Mode::Live) — the control case.

  void writeDotPool() {
    dotPool->clear();
    auto push = [&](const Body& b, int pinFrame) {
      for (size_t i = 0; i < b.x.size(); ++i)
        dotPool->add(drawn(b, i), b.invm[i] <= 0.0f ? pinFrame : cellDot);
    };
    push(rig, cellPin);
    push(cloth, cellPin);
    for (const Body& p : plants) push(p, cellPin);
    // Fade the whole field with the body.
    const float f = bodyFade.value();
    if (f < 1.0f) {
      auto tints = dotPool->tints();
      for (SkColor4f& t : tints) t.fA = f;
    }
  }

  /** The SECOND path for the SAME 24 sticks: one atlas cell (a bar with
   *  BUTT ends), rotations() = atan2(dy, dx), and the sizes() lane carrying
   *  length and thickness independently — which one uniform per-instance
   *  scale cannot express, since every stick needs a different length at
   *  the same width. */
  void writeBarPool(SkPoint origin, float scale) {
    barPool->resize(rig.sticks.size());
    auto pos = barPool->positions();
    auto rot = barPool->rotations();
    auto tint = barPool->tints();
    auto frame = barPool->frames();
    auto size = barPool->sizes();
    const float f = bodyFade.value();
    for (size_t i = 0; i < rig.sticks.size(); ++i) {
      const Stick& s = rig.sticks[i];
      SkPoint a = drawn(rig, (size_t)s.a), b = drawn(rig, (size_t)s.b);
      a = {origin.fX + a.fX * scale, origin.fY + a.fY * scale};
      b = {origin.fX + b.fX * scale, origin.fY + b.fY * scale};
      const SkPoint d = b - a;
      const float L = std::max(0.5f, len(d));
      pos[i] = a + d * 0.5f;
      rot[i] = std::atan2(d.fY, d.fX);
      frame[i] = cellBar;
      // cell is 32 x 8 logical, so the x multiplier is L/32 while y is
      // fixed: ONE cell length serving 24 different stick lengths remaps the
      // cell's aspect by itself.
      size[i] = {L / 32.0f, 4.6f / 8.0f};
      const float e = std::abs(len(rig.x[s.b] - rig.x[s.a]) - s.r) / s.r;
      tint[i] = errColor(e, f);
    }
    barPool->commit();
  }

  // =========================================================================
  // The stage, in pen verbs

  void stageChrome(Pen& pen, double ms) {
    const float a = cue(ms, 520, 400);
    // The floor: a band from world y = 0 down out of frame is the bottom
    // edge of the cube, so the solid reads as a plinth under it plus the
    // surface line the paper's projection actually clamps to. The grain is
    // a LAYER OF THE PAINT — Paint::blend soft-lights it onto the solid in
    // one fill, rather than costing a second pass over the same band.
    const float floorTop = kStage - kCapsule * kUnit;
    const Paint floor = Paint::blend(
        {{Paint::solid(fadeTo(kSolid, a)), SkBlendMode::kSrc},
         {Paint::recipe(field::grain(0.035f, 3, 11.0f, 0.5f)),
          SkBlendMode::kSoftLight}});
    pen.noStroke();
    pen.fill(floor);
    pen.rect(0, floorTop, kStage, kStage - floorTop);
    pen.fill(hex(0x6FA8DC, 0.5f * a));
    pen.rect(0, floorTop, kStage, 1);

    // The bump, drawn to the paper's own Fig. 4 scale, with a drafting
    // section hatched inside it — a generator, not a texture.
    const SkPoint a1 = toStage(kBumpA), b1 = toStage(kBumpB),
                  c1 = toStage(kBumpC);
    pen.fill(fadeTo(kSolid, a));
    pen.stroke(hex(0x6FA8DC, 0.40f * a));
    pen.strokeWeight(1.0f);
    pen.triangle(a1.fX, a1.fY, b1.fX, b1.fY, c1.fX, c1.fY);
    {
      // The hatch: 45° lines inside the section, masked by the section
      // itself — the same three points, drawn once as the shape and once
      // as the clip, so the hatch cannot outrun the bump it belongs to.
      pen.push();
      pen.clip([&] { pen.triangle(a1.fX, a1.fY, b1.fX, b1.fY, c1.fX, c1.fY); });
      pen.stroke(hex(0x6FA8DC, 0.20f * a));
      pen.strokeWeight(1.0f);
      const float span = c1.fX - a1.fX + (a1.fY - b1.fY);
      for (float s = 0; s < span; s += 6.0f)
        pen.line(a1.fX + s, a1.fY, a1.fX + s - span, a1.fY - span);
      pen.pop();
    }
    pen.noStroke();
  }

  void worldBox(Pen& pen, double ms) {
    // The cube's bezel, and its top wall — real, just not interesting.
    const float g = cue(ms, 240, 620, &ch::easeOutCubic);
    pen.noFill();
    pen.strokeCap(draw::SQUARE);
    pen.stroke(hex(0x6FA8DC, 0.45f));
    pen.strokeWeight(1.5f);
    // the bezel drawn UP TO a fraction of its perimeter, as a trim window
    // on a rect is
    const float per = 4.0f * kStage, run = per * g;
    const SkPoint corner[5] = {{0.75f, 0.75f},
                               {kStage - 0.75f, 0.75f},
                               {kStage - 0.75f, kStage - 0.75f},
                               {0.75f, kStage - 0.75f},
                               {0.75f, 0.75f}};
    float walked = 0;
    for (int i = 0; i < 4 && walked < run; ++i) {
      const SkPoint p0 = corner[i], p1 = corner[i + 1];
      const float side = len(p1 - p0);
      const float take = std::min(side, run - walked);
      const SkPoint u = (p1 - p0) * (1.0f / side);
      pen.line(p0.fX, p0.fY, p0.fX + u.fX * take, p0.fY + u.fY * take);
      walked += side;
    }
    pen.stroke(hex(0x6FA8DC, 0.22f));
    pen.strokeDash({4.0f, 5.0f});
    pen.line(0, 1, kStage, 1);
    pen.noDash();
    pen.strokeCap(draw::ROUND);
    pen.noStroke();
  }

  /** The corpse, the cloth, the plants and every contact this frame. */
  void simulation(Pen& pen) {
    const float f = bodyFade.value();
    // The cloth: one shape for the quad fill, then its edges.
    pen.noStroke();
    pen.fill(hex(0x6FA8DC, 0.07f * f));
    constexpr int C = 5, R = 5;
    pen.beginShape(draw::QUADS);
    for (int r = 0; r + 1 < R; ++r)
      for (int c = 0; c + 1 < C; ++c) {
        const int a = r * C + c;
        const SkPoint p0 = drawn(cloth, (size_t)a),
                      p1 = drawn(cloth, (size_t)a + 1),
                      p2 = drawn(cloth, (size_t)a + (size_t)C + 1),
                      p3 = drawn(cloth, (size_t)a + (size_t)C);
        pen.vertex(p0.fX, p0.fY);
        pen.vertex(p1.fX, p1.fY);
        pen.vertex(p2.fX, p2.fY);
        pen.vertex(p3.fX, p3.fY);
      }
    pen.endShape();

    pen.noFill();
    pen.strokeCap(draw::ROUND);
    pen.strokeWeight(1.0f);
    pen.stroke(hex(0x8A8F9C, 0.45f * f));
    for (const Stick& s : cloth.sticks) {
      const SkPoint a = drawn(cloth, (size_t)s.a), b = drawn(cloth, (size_t)s.b);
      pen.line(a.fX, a.fY, b.fX, b.fY);
    }
    pen.strokeWeight(2.0f);
    pen.stroke(hex(0x8A8F9C, 0.70f * f));
    for (const Body& p : plants)
      for (const Stick& st : p.sticks) {
        const SkPoint a = drawn(p, (size_t)st.a), b = drawn(p, (size_t)st.b);
        pen.line(a.fX, a.fY, b.fX, b.fY);
      }

    paintRig(pen, 1.0f, {0, 0}, f, true);

    // The contact markers: an open ring and a normal tick at every
    // projection this frame. These are the frames where the ramp fires.
    pen.noFill();
    pen.stroke(hex(0xC8402F, 0.95f * f));
    pen.strokeWeight(1.3f);
    for (const Contact& k : contacts) {
      const SkPoint p = toStage(k.p);
      pen.circle(p.fX, p.fY, 7.0f);
      pen.line(p.fX, p.fY, p.fX + k.n.fX * 14.0f, p.fY - k.n.fY * 14.0f);
    }

    // §7 IK: the target the hand is pinned to, "the hand of the player" —
    // a CASED leader (a wide casing under a narrow core) whose last eighth
    // is drawn in the accent, which is what a trim window on a path is
    // when the path is two points. The core MARCHES: a dash whose phase
    // runs backwards with the clock, so the leader reads as a pull toward
    // the target rather than a rod between two points. The casing under it
    // and the accent head stay solid, which is what makes the marching
    // legible.
    // FROM THE GRAB POINT, not from the hand. The pin puts the hand on the
    // target as the last operation of every constraint pass, so the hand
    // is never further from the target than one pass's stick residual — a
    // pixel or two — and a leader drawn from it is a point under the
    // target ring. The pull the leader shows is the drag's own: from where
    // the hand was taken hold of to where the target has moved it.
    if (dragging) {
      const SkPoint h = toStage(dragFrom), tgt = toStage(dragTarget);
      pen.strokeWeight(2.0f + 2.0f * 4.0f);
      pen.stroke(kInk);
      pen.line(h.fX, h.fY, tgt.fX, tgt.fY);
      pen.strokeWeight(2.0f);
      pen.stroke(hex(0x6FA8DC, 0.55f));
      pen.strokeDash({4.0f, 3.0f}, -(float)pen.millis() * 0.02f);
      pen.line(h.fX, h.fY, tgt.fX, tgt.fY);
      pen.noDash();
      const SkPoint head = h + (tgt - h) * 0.88f;
      pen.strokeWeight(2.6f);
      pen.stroke(kRed);
      pen.line(head.fX, head.fY, tgt.fX, tgt.fY);
      pen.strokeWeight(1.6f);
      pen.circle(tgt.fX, tgt.fY, 18.0f);
    }
    // The blast site, marked permanently: the law is documented, the
    // constant is not.
    {
      const SkPoint b = toStage(kBlast);
      pen.strokeWeight(1.0f);
      pen.stroke(hex(0xC8402F, 0.75f));
      pen.line(b.fX - 7, b.fY, b.fX + 7, b.fY);
      pen.line(b.fX, b.fY - 7, b.fX, b.fY + 7);
      pen.circle(b.fX, b.fY, 9.0f);
    }
    // The HIT chevron, 0.4 s on the displaced particle.
    if (loopT >= 1.10 && loopT < 1.50) {
      const SkPoint p = drawn(rig, RSH);
      pen.strokeWeight(2.0f);
      pen.stroke(kRed);
      pen.beginShape();
      pen.vertex(p.fX + 20, p.fY - 9);
      pen.vertex(p.fX + 9, p.fY);
      pen.vertex(p.fX + 20, p.fY + 9);
      pen.endShape();
    }
    pen.noStroke();
  }

  /** The retained overlay: every particle dot through instances(Mode::Live)
   *  — the control case beside the pen's own stroking. */
  Element stageOverlay() {
    return stack()
        .width(Dim(kStage))
        .height(Dim(kStage))
        .child(box().inset(0).child(
            instancing::instances(dotAtlas, dotPool, instancing::Mode::Live)));
  }

  /** THE BLAST'S ADDITIVE GLOW, in the pen's own words. Its falloff is
   *  authored in a UNIT SQUARE — nothing about it is in pixels — and
   *  `fill(paint, SHAPE)` is what gives that unit square a box: the
   *  240 x 240 the shape itself occupies, wherever the blast site is.
   *  Before that word existed the glow needed a compose leaf whose only
   *  job was to be a box for the fill to be a unit of, and the picture is
   *  the same one.
   *
   *  The decay is the STOPS' alpha rather than a node opacity, because
   *  the pass is additive and dst + a*src is what a dimming light does,
   *  with nothing gathered in between. Off entirely once the phase is
   *  spent, which is most of the loop. */
  void blastGlow(Pen& pen) {
    const float a =
        std::clamp(ch::easeOutQuad(blastPhase.value()), 0.0f, 1.0f);
    if (a <= 0.0f) return;
    const SkPoint c = toStage(kBlast);
    pen.push();
    pen.translate(kStageX, kBodyY);
    pen.noStroke();
    pen.blendMode(sigil::draw::ADD);
    pen.rectMode(sigil::draw::CENTER);
    pen.fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                             {{0.0f, hex(0xFFF3E2, 1.0f * a)},
                              {0.35f, hex(0xFFC98A, 0.55f * a)},
                              {1.0f, hex(0xC8402F, 0.0f)}}),
             sigil::draw::SHAPE);
    pen.rect(c.fX, c.fY, 240.0f, 240.0f);
    pen.pop();
  }

  /** The stage's own labels, and the note block over the world box. */
  void stageLabels(Pen& pen) {
    penMono(pen, 7.5f, kTick);
    pen.textAlign(draw::LEFT, draw::TOP);
    pen.text("(0, 0)", 7, kStage - 13);
    pen.text("(1000, 1000)", kStage - 62, 5);
    penUi(pen, 7.5f, kBone, 0.5f);
    pen.text("\xc2\xa7"
             "9 \xc2\xb7 THE CORPSE \xc2\xb7 16 PARTICLES, 24 STICKS, "
             "4 ITERATIONS \xc2\xb7 EVERY STICK COLOURED BY ITS LIVE "
             "CONSTRAINT ERROR",
             16, 548, 214, 60);
    penUi(pen, 7.0f, kTick, 0.5f);
    pen.textAlign(draw::RIGHT, draw::TOP);
    pen.text("\xc2\xa7"
             "4 \xc2\xb7 TRIANGULAR MESH \xc2\xb7 ONE PARTICLE PINNED "
             "\xc2\xb7 ONE ITERATION \xc2\xb7 THE SAG IS THE ITERATION "
             "COUNT",
             452, 412, 268, 40);
    pen.text("\xc2\xa7"
             "4 \xc2\xb7 PLANTS = CLOTH + SUPPORT STICKS \xc2\xb7 ONE "
             "ITERATION \xc2\xb7 BASE ROW PINNED",
             452, 596, 268, 40);
    pen.textAlign(draw::LEFT, draw::TOP);
    pen.text("736 \xc3\x97 736 px = THE PAPER'S CUBE "
             "(0,0,0)\xe2\x80\x93(1000,1000,1000) IN THE PLANE.",
             240, 18, 262, 24);
    pen.text("1 UNIT = 0.736 px = 3.60 mm \xc2\xb7 THE STAGE IS SQUARE "
             "BECAUSE THE WORLD IS A CUBE.",
             240, 40, 262, 24);
    penUi(pen, 7.0f, hex(0xC8402F, 0.8f), 0.4f);
    pen.text("\xc2\xa7"
             "7 BOMB \xe2\x8a\x95 \xc2\xb7 |\xce\x94x| = K / |x\xe2\x88\x92"
             "c|\xc2\xb2 \xc2\xb7 EVERY PARTICLE, ONCE \xc2\xb7 THE "
             "INTEGRATOR MAKES IT VELOCITY",
             240, 68, 262, 24);

    // The phase strip, and the live stage readout under the error legend.
    const char* names[5] = {"SPAWN", "HIT", "BOMB", "SETTLE", "DRAG"};
    float x = 16;
    for (int i = 0; i < 5; ++i) {
      penUi(pen, 8.0f, i == phase ? kRed : hex(0x8A8F9C, 0.45f), 1.7f);
      pen.text(names[i], x, 690);
      x += pen.textWidth(names[i]) + 9;
    }
  }

  /** Fig. 4b/5b: the paper's own worked case, c1 = 0.75, c2 = 0.25, and
   *  Fig. 10's friction diagram beside it. Both are drawings with four
   *  lines of type under them, so both are the pen's. */
  void figPenetration(Pen& pen, float x0, float y0, float a) {
    inset(pen, x0, y0, 208, 148, a);
    penUi(pen, 7.5f, fadeTo(kSteel, a), 1.2f);
    pen.text("FIG. 4b/5b \xc2\xb7 \xc2\xa7"
             "5 PENETRATION",
             x0 + 8, y0 + 8);
    pen.push();
    pen.translate(x0 + 8, y0 + 21);
    // the obstacle
    pen.noStroke();
    pen.fill(fadeTo(kSolid, a));
    pen.beginShape();
    pen.vertex(10, 56);
    pen.vertex(96, 14);
    pen.vertex(180, 56);
    pen.vertex(180, 62);
    pen.vertex(10, 62);
    pen.endShape(draw::CLOSE);
    // the stick, penetrating a quarter along
    const SkPoint x1{22, 44}, x2{162, 26};
    const SkPoint pp{x1.fX * 0.75f + x2.fX * 0.25f,
                     x1.fY * 0.75f + x2.fY * 0.25f};
    const SkPoint q{pp.fX + 2, pp.fY - 17};
    pen.noFill();
    pen.strokeWeight(2.0f);
    pen.stroke(fadeTo(kBlue, a));
    pen.line(x1.fX, x1.fY, x2.fX, x2.fY);
    pen.strokeWeight(1.4f);
    pen.stroke(fadeTo(kRed, a));
    pen.line(pp.fX, pp.fY, q.fX, q.fY);
    pen.noStroke();
    pen.fill(fadeTo(kBone, a));
    pen.circle(x1.fX, x1.fY, 5.2f);
    pen.circle(x2.fX, x2.fY, 5.2f);
    pen.fill(fadeTo(kRed, a));
    pen.circle(pp.fX, pp.fY, 4.4f);
    pen.circle(q.fX, q.fY, 4.4f);
    pen.pop();
    float y = y0 + 21 + 64 + 3;
    penMono(pen, 7.0f, fadeTo(kBlue, a), 0.1f);
    pen.text("p = c1\xc2\xb7x1 + c2\xc2\xb7x2,  c1 = 0.75, c2 = 0.25",
             x0 + 8, y);
    y += 11;
    pen.text("\xce\xbb = (q\xe2\x88\x92p)\xc2\xb7\xce\x94 / "
             "((c1\xc2\xb2+c2\xc2\xb2)\xc2\xb7\xce\x94\xc2\xb2)",
             x0 + 8, y);
    y += 11;
    pen.text("x1' = x1 + c1\xce\xbb\xce\x94    x2' = x2 + c2\xce\xbb\xce\x94",
             x0 + 8, y);
    y += 12;
    penUi(pen, 6.5f, fadeTo(kTick, a), 0.6f);
    pen.text("THE FIX-UP VIOLATES THE STICK. RELAX AGAIN.", x0 + 8, y);
  }

  void figFriction(Pen& pen, float x0, float y0, float a) {
    inset(pen, x0, y0, 208, 148, a);
    penUi(pen, 7.5f, fadeTo(kSteel, a), 1.2f);
    pen.text("FIG. 10 \xc2\xb7 \xc2\xa7"
             "7 FRICTION",
             x0 + 8, y0 + 8);
    pen.push();
    pen.translate(x0 + 8, y0 + 21);
    pen.noStroke();
    pen.fill(fadeTo(kSolid, a));
    pen.beginShape();
    pen.vertex(8, 58);
    pen.quadraticVertex(96, 34, 184, 58);
    pen.vertex(184, 70);
    pen.vertex(8, 70);
    pen.endShape(draw::CLOSE);
    pen.noFill();
    pen.strokeWeight(1.2f);
    // the box, sunk by d_p
    pen.stroke(hex(0x6FA8DC, 0.55f * a));
    pen.rect(58, 30, 34, 22);
    pen.stroke(fadeTo(kRed, a));
    pen.line(75, 52, 75, 44);  // d_p
    pen.stroke(fadeTo(kBlue, a));
    pen.line(96, 40, 146, 40);  // v_t before
    pen.line(146, 40, 140, 36);
    pen.line(146, 40, 140, 44);
    pen.stroke(fadeTo(kBone, a));
    pen.line(96, 52, 124, 52);  // v_t after
    pen.line(124, 52, 118, 48);
    pen.line(124, 52, 118, 56);
    pen.noStroke();
    pen.pop();
    float y = y0 + 21 + 64 + 3;
    penMono(pen, 7.0f, fadeTo(kBlue, a), 0.1f);
    pen.text("d_p MEASURED BEFORE THE PROJECTION,", x0 + 8, y);
    y += 11;
    pen.text("v_t REDUCED BY k\xc2\xb7"
             "d_p BY MOVING x*.",
             x0 + 8, y);
    y += 11;
    pen.text("NEVER LET v_t REVERSE \xe2\x80\x94 CLAMP TO ZERO.", x0 + 8, y);
    y += 12;
    penUi(pen, 6.5f, fadeTo(kTick, a), 0.6f);
    pen.text("RESTITUTION IS ZERO: PARTICLES DO NOT BOUNCE.", x0 + 8, y);
  }

  /** A stage inset: the scrim over the world. */
  void inset(Pen& pen, float x, float y, float w, float h, float a) {
    pen.noStroke();
    pen.fill(hex(0x101116, 0.88f * a));
    pen.rect(x, y, w, h, 5);
    pen.noFill();
    pen.stroke(fadeTo(kKeyline, a));
    pen.strokeWeight(1.0f);
    pen.rect(x + 0.5f, y + 0.5f, w - 1, h - 1, 5);
    pen.noStroke();
  }

  /** The A/B strip's chrome, with the instanced half riding in as a guest
   *  and the pen drawing the same 24 sticks beside it. */
  Element instancedHalf() {
    return box().inset(0).child(
        instancing::instances(barAtlas, barPool, instancing::Mode::Live));
  }

  void instancingStrip(Pen& pen, float x0, float y0, float a) {
    constexpr float w = 330, h = 108;
    inset(pen, x0, y0, w, h, a);
    penUi(pen, 7.5f, fadeTo(kSteel, a), 0.9f);
    pen.text("SAME 24 STICKS \xc2\xb7 instances()+sizes() vs the pen",
             x0 + 7, y0 + 7);
    pen.element(barsEl, SkRect::MakeXYWH(x0 + 7, y0 + 19, 150, 76));
    {
      float sc = 1;
      SkPoint off{0, 0};
      rigFit(150.0f, 74.0f, &sc, &off);
      paintRig(pen, sc, {x0 + 7 + off.fX + 166.0f, y0 + 19 + off.fY + 2.0f},
               bodyFade.value(), false);
    }
    pen.noStroke();
    pen.fill(hex(0x191B22, 0.9f * a));
    pen.rect(x0 + 7 + 158, y0 + 19, 1, 76);
    penMono(pen, 7.0f, fadeTo(kTick, a), 0.2f);
    pen.text("ONE POOL WITH sizes() \xc2\xb7 ONE PEN PROGRAM", x0 + 7,
             y0 + h - 17);
  }

  void errorLegend(Pen& pen, float x0, float y0, float a) {
    constexpr float w = 336, h = 48;
    pen.noStroke();
    pen.fill(hex(0x101116, 0.86f * a));
    pen.rect(x0, y0, w, h, 4);
    pen.noFill();
    pen.stroke(fadeTo(kKeyline, a));
    pen.strokeWeight(1);
    pen.rect(x0 + 0.5f, y0 + 0.5f, w - 1, h - 1, 4);
    pen.noStroke();
    const char* labels[5] = {"0.000", "0.004", "0.010", "0.020",
                             "\xe2\x89\xa5.035"};
    for (int i = 0; i < 5; ++i) {
      const float x = x0 + 7 + (float)i * 50;
      pen.fill(fadeTo(kRampCol[i], a));
      pen.rect(x, y0 + 7, 48, 7);
      penMono(pen, 6.5f, fadeTo(kTick, a), 0.2f);
      pen.text(labels[i], x, y0 + 17);
    }
    penUi(pen, 7.0f, fadeTo(kSteel, a), 0.3f);
    pen.textAlign(draw::RIGHT, draw::TOP);
    pen.text("e = ||x2\xe2\x88\x92x1|\xe2\x88\x92r| / r", x0 + w - 7, y0 + 7);
    pen.textAlign(draw::LEFT, draw::TOP);
    // the live readout: max error, contacts, step and the interpolant
    char buf[128];
    std::snprintf(buf, sizeof buf,
                  "MAX e %5.2f%%  \xc2\xb7  CONTACTS %2zu  \xc2\xb7  STEP "
                  "%llu  \xc2\xb7  \xce\xb1 %.2f",
                  stageMaxErr * 100, contactCount, (unsigned long long)simSteps,
                  (double)alpha.value());
    penMonoB(pen, 8.0f, errColor(stageMaxErr, a), 0.1f);
    pen.text(buf, x0 + 7, y0 + 30);
  }

  // =========================================================================
  // Sidebar — the six panels. Three of them are laid out with a HOLE the
  // pen draws into afterwards.

  Element codeLine(const char* s, SkColor4f c, bool caret = false) {
    auto row = box().row().gap(4).height(Dim(12)).shrink(0);
    row.child(t(caret ? "\xe2\x97\x84" : " ", mono(7.0f, caret ? kRed : kInk))
                  .width(Dim(8))
                  .shrink(0));
    row.child(t(s, mono(9.5f, c, 0.1f)));
    return row;
  }

  Element panelA1() {
    return panel(kPanelAH[0], "A1 \xc2\xb7 VERLET \xe2\x80\x94 NO VELOCITY "
                              "VARIABLE",
                 1)
        .gap(4)
        .child(t("x' = 2x \xe2\x88\x92 x* + a\xc2\xb7\xce\x94t\xc2\xb2      x* "
                 "= x",
                 monoB(12.0f, kBone, 0.2f))
                   .height(Dim(16))
                   .shrink(0))
        .child(codeLine("temp    = x[i];", kBlue))
        .child(codeLine("x[i]   += DRAG*(x[i]-oldx[i]) + g;", kBlue))
        .child(codeLine("oldx[i] = temp;", kBlue))
        .child(box().grow(1))
        .child(t("1.99 IS A VELOCITY DAMP, NOT A POSITION ONE",
                 ui(7.5f, kRed, 0.5f)))
        .child(t("x' = 1.99x \xe2\x88\x92 0.99x* + a\xce\x94t\xc2\xb2  ==  "
                 "x + 0.99(x\xe2\x88\x92x*) + a\xce\x94t\xc2\xb2",
                 mono(7.5f, kSteel, 0.1f)))
        .child(t("AS PRINTED, A PARTICLE AT REST AT x = 500 DRIFTS 5 u/STEP "
                 "TOWARD THE ORIGIN.",
                 ui(7.0f, kTick, 0.4f)));
  }

  Element panelA2() {
    return panel(kPanelAH[1], "A2 \xc2\xb7 THE STICK CONSTRAINT, AND A SIGN",
                 2)
        .child(codeLine("delta = x2-x1;", kBlue))
        .child(codeLine("deltalength = sqrt(delta*delta);", kBlue))
        .child(codeLine("diff = (deltalength-restlength)/deltalength;", kBlue))
        .child(codeLine("x1 -= delta*0.5*diff;", kRed, true))
        .child(codeLine("x2 += delta*0.5*diff;", kRed, true))
        .child(box().height(Dim(2)).shrink(0))
        .child(t("r = 100, |x2\xe2\x88\x92x1| = 120 \xe2\x86\x92 diff = 1/6, "
                 "delta\xc2\xb7"
                 "0.5\xc2\xb7"
                 "diff = (10, 0)",
                 mono(7.5f, kSteel, 0.1f)))
        .child(t("AS PRINTED : x1 = (\xe2\x88\x92"
                 "10,0)  x2 = (130,0)  "
                 "\xe2\x86\x92 d = 140  DIVERGES",
                 mono(8.0f, kRed, 0.1f)))
        .child(t("CORRECTED  : x1 = ( 10,0)  x2 = (110,0)  "
                 "\xe2\x86\x92 d = 100  EXACT",
                 mono(8.0f, hex(0x4FC79E), 0.1f)))
        .child(box().height(Dim(2)).shrink(0))
        .child(t("FOUR OF THE FIVE STICK LISTINGS CARRY IT: (C2), "
                 "STICK-IN-A-BOX, CLOTH, MASS-WEIGHTED.",
                 ui(7.5f, kSteel, 0.4f)))
        .child(t("THE FIFTH \xe2\x80\x94 THE SQRT APPROXIMATION, THE ONE THAT "
                 "SHIPPED IN HITMAN \xe2\x80\x94 IS CORRECT WITH THE SAME TWO "
                 "ASSIGNMENT LINES, BECAUSE ITS FACTOR IS ALREADY NEGATIVE "
                 "UNDER TENSION. THE EXPOSITION FORM WAS MADE BY REMOVING THE "
                 "APPROXIMATION, AND THE SIGN WENT WITH IT.",
                 ui(7.5f, kTick, 0.4f)))
        .child(box().grow(1))
        .child(t("THE PAPER'S OWN STICK CODE PUSHES WHEN IT SHOULD PULL.",
                 monoB(9.0f, kRed, 0.2f)));
  }

  Element panelA3() {
    // s_exact(u) = 0.5 - 1/(2u);  s_approx(u) = 0.5 - 1/(1+u^2)
    auto curve = [](bool approx) {
      return shapes::parametric(
          [approx](float u) {
            const float s = approx ? 0.5f - 1.0f / (1.0f + u * u)
                                   : 0.5f - 1.0f / (2.0f * u);
            const float x = (u - 1.25f) / 0.75f;  // u in [0.5, 2] -> [-1,1]
            const float y =
                -(s - (-0.1f)) / 0.45f;  // s in [-0.55,0.35], flipped
            return SkPoint{x, std::clamp(y, -1.0f, 1.0f)};
          },
          0.5f, 2.0f, 240);
    };
    auto plotCurve = [&](bool approx, SkColor4f c, float w) {
      PathFormat f = stroke(w, Fill::color(c));
      if (!approx) f.dashIntervals = {3.5f, 3.0f};
      return box()
          .inset(0)
          .shape(curve(approx))
          .stroke(spans::upTo(animate(to(1.0f), {.duration = 520ms,
                                                 .ease = ch::easeOutCubic,
                                                 .delay = 1400ms})),
                  f);
    };
    auto bar = [&](int i, const char* label, float h) {
      return box()
          .column()
          .gap(2)
          .width(Dim(52))
          .shrink(0)
          .alignItems(Align::Center)
          .child(box().grow(1))
          .child(box()
                     .width(Dim(30))
                     .height(Dim(h))
                     .fill(i == 4 ? kBlue : hex(0x6FA8DC, 0.42f))
                     .scaleY(animate(from(0.0f).to(1.0f),
                                     {.duration = 220ms,
                                      .ease = ease::outBack(1.70158f),
                                      .delay = 1600ms}))
                     .transformOrigin(0.5f, 1.0f))
          .child(t(label, mono(7.0f, kSteel)));
    };
    return panel(kPanelAH[2], "A3 \xc2\xb7 THE SQUARE-ROOT APPROXIMATION", 3)
        .child(codeLine("delta *= r*r/(delta*delta+r*r) - 0.5;", kBlue))
        .child(codeLine("x1 -= delta;   x2 += delta;", kBlue))
        .child(
            box()
                .height(Dim(64))
                .shrink(0)
                .child(box()  // s = 0
                           .left(Dim(0))
                           .top(Dim(39.1f))
                           .width(Dim(324))
                           .height(Dim(1))
                           .fill(hex(0x2A2E38)))
                .child(box()  // u = 1
                           .left(Dim(108))
                           .top(Dim(0))
                           .width(Dim(1))
                           .height(Dim(64))
                           .fill(hex(0x2A2E38)))
                .child(plotCurve(false, kSteel, 1.4f))
                .child(plotCurve(true, kBlue, 1.8f))
                .child(
                    t("s_exact", mono(7.0f, kSteel)).left(Dim(4)).top(Dim(2)))
                .child(
                    t("s_approx", mono(7.0f, kBlue)).left(Dim(4)).top(Dim(13)))
                .child(t("u = d/r   0.5 \xe2\x86\x92 2.0", mono(7.0f, kTick))
                           .left(Dim(244))
                           .top(Dim(52))))
        .child(t("approx/exact:  0.60\xc3\x97 at u=0.5 \xc2\xb7 0.88 \xc2\xb7 "
                 "1.08 \xc2\xb7 1.15 \xc2\xb7 1.20\xc3\x97 at u=2.0",
                 mono(7.5f, kSteel, 0.1f)))
        .child(t("AGREES IN VALUE AND SLOPE AT u = 1. DENOMINATOR "
                 "d\xc2\xb2+r\xc2\xb2 \xe2\x89\xa5 r\xc2\xb2 > 0, SO IT "
                 "CANNOT DIVIDE BY ZERO: \xc2\xa7"
                 "7's SINGULARITY NOTE "
                 "APPLIES ONLY TO THE EXACT FORM.",
                 ui(7.0f, kTick, 0.4f)))
        .child(box()
                   .row()
                   .gap(2)
                   .height(Dim(38))
                   .shrink(0)
                   .staggerChildren(60ms)
                   .child(bar(0, "60", 12))
                   .child(bar(1, "80", 16))
                   .child(bar(2, "90", 18))
                   .child(bar(3, "95", 19))
                   .child(bar(4, "97.5", 19.5f)))
        .child(t("\xc2\xa7"
                 "7 SOFT CONSTRAINTS: HALF THE DEVIATION PER FRAME.",
                 ui(7.0f, kTick, 0.4f)));
  }

  /** B1's tree leaves a 118 px hole under its heading; the anatomy
   *  diagram is drawn into it by the pen, because it is the rest pose of
   *  the same rig the stage is simulating. */
  Element panelB1() {
    return panel(kPanelBH[0], "B1 \xc2\xb7 FIGURE 9: THE ANATOMY", 4)
        .gap(4)
        .child(box().height(Dim(118)).shrink(0))
        .child(t("16 PARTICLES \xc2\xb7 24 STICKS \xc2\xb7 1 INEQUALITY "
                 "(KNEES, \xc2\xa7"
                 "6)",
                 monoB(8.5f, kBone, 0.1f)))
        .child(t("16\xc3\x97"
                 "2 \xe2\x88\x92 24 = 8 PLANAR DOF   "
                 "(16\xc3\x97"
                 "3 \xe2\x88\x92 24 = 24 IN THE PAPER'S 3D)",
                 mono(8.0f, kSteel, 0.1f)))
        .child(t("COMPARE \xc2\xa7"
                 "5's TETRAHEDRON: 4\xc3\x97"
                 "3 \xe2\x88\x92 6 = 6",
                 mono(8.0f, kSteel, 0.1f)))
        .child(t("RE-COUNTED AT 600 dpi: THRESHOLD, ERODE BY A DISC r = 8 px "
                 "\xe2\x80\x94 EVERY STICK AND EVERY BODY-TEXT STEM DIES AND "
                 "EXACTLY 16 COMPONENTS OF 620\xe2\x80\x93"
                 "657 px SURVIVE. "
                 "THE PAPER PUBLISHES NO COUNT.",
                 ui(7.0f, kTick, 0.4f)));
  }

  void paintAnatomy(Pen& pen, float x0, float y0, float w) {
    // The rest pose at 104 px tall, centred in the panel's hole.
    const float H = 104.0f;
    const float cx = x0 + w * 0.5f, base = y0 + 112.0f;
    auto P = [&](Norm n, float side) {
      return SkPoint{cx + n.x * side * H, base - n.y * H};
    };
    std::array<SkPoint, NRIG> p = {
        P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1),
        P(kEl, -1),  P(kEl, +1),  P(kHa, -1), P(kHa, +1),
        P(kWa, -1),  P(kWa, +1),  P(kHi, -1), P(kHi, +1),
        P(kKn, -1),  P(kKn, +1),  P(kFo, -1), P(kFo, +1)};
    pen.noFill();
    pen.strokeCap(draw::ROUND);
    pen.strokeWeight(1.5f);
    pen.stroke(hex(0x8A8F9C, 0.9f));
    for (const Stick& s : rig.sticks) {
      const SkPoint a = p[(size_t)s.a], b = p[(size_t)s.b];
      pen.line(a.fX, a.fY, b.fX, b.fY);
    }
    pen.strokeWeight(1.0f);
    pen.stroke(hex(0xC8402F, 0.9f));
    pen.strokeDash({2.0f, 3.0f});
    pen.line(p[LKN].fX, p[LKN].fY, p[RKN].fX, p[RKN].fY);
    pen.noDash();
    pen.noStroke();
    pen.fill(kBone);
    for (const SkPoint& q : p) pen.circle(q.fX, q.fY, 5.2f);
    penMono(pen, 7.0f, kTick);
    pen.textAlign(draw::LEFT, draw::CENTER);
    pen.text("NECK", p[NECK].fX + 7, p[NECK].fY);
    pen.text("WAIST", p[RWA].fX + 7, p[RWA].fY);
    pen.text("HIP", p[RHI].fX + 7, p[RHI].fY);
    penMono(pen, 7.0f, kRed);
    pen.text("|LK\xe2\x88\x92RK| \xe2\x89\xa5 100", p[LKN].fX - 66,
             p[LKN].fY);
    pen.textAlign(draw::LEFT, draw::TOP);
  }

  /** B2's tree leaves a 156 px hole for the three chains and 34 px for
   *  their live numbers; both are the solver's, so both are the pen's. */
  Element panelB2() {
    return panel(kPanelBH[1], "B2 \xc2\xb7 RELAXATION: 1 \xc2\xb7 4 \xc2\xb7 "
                              "10",
                 5)
        .gap(4)
        .child(box().height(Dim(156)).shrink(0))
        .child(box().height(Dim(34)).shrink(0))
        .child(t("\"ITERATIONS USED IN HITMAN VARY BETWEEN 1 AND 10 WITH THE "
                 "KIND OF OBJECT SIMULATED.\" \xe2\x80\x94 \xc2\xa7"
                 "7. "
                 "ORDER MATTERS AS MUCH AS COUNT: LISTED FROM THE PIN A CHAIN "
                 "CONVERGES IN ONE SWEEP AND ALL THREE ARE IDENTICAL. THESE "
                 "ARE LISTED FROM THE FREE END.",
                 ui(7.0f, kTick, 0.4f)));
  }

  void paintChains(Pen& pen, float x0, float y0) {
    pen.push();
    pen.translate(x0, y0);
    pen.noFill();
    pen.strokeCap(draw::ROUND);
    pen.strokeWeight(2.2f);
    for (int k = 0; k < 3; ++k) {
      const Body& b = chains[(size_t)k];
      for (const Stick& s : b.sticks) {
        const float e = std::abs(len(b.x[s.b] - b.x[s.a]) - s.r) / s.r;
        const SkPoint a = drawnWorld(b, (size_t)s.a);
        const SkPoint z = drawnWorld(b, (size_t)s.b);
        pen.stroke(errColor(e));
        pen.line(a.fX, a.fY, z.fX, z.fY);
      }
      pen.noStroke();
      for (size_t i = 0; i < b.x.size(); ++i) {
        const SkPoint q = drawnWorld(b, i);
        pen.fill(b.invm[i] <= 0 ? kRed : kBone);
        pen.circle(q.fX, q.fY, 4.0f);
      }
      pen.noFill();
    }
    pen.noStroke();
    penMonoB(pen, 9.0f, kBone);
    pen.textAlign(draw::LEFT, draw::BASELINE);
    const char* labels[3] = {"1", "4", "10"};
    const float xs[3] = {54, 156, 256};
    for (int k = 0; k < 3; ++k) pen.text(labels[k], xs[k], 12);
    pen.textAlign(draw::LEFT, draw::TOP);
    pen.pop();

    // The A/B's own numbers, and the monotonicity claim COMPUTED from the
    // three means rather than asserted beside them.
    char a[96], b[96];
    std::snprintf(a, sizeof a, "MEAN e   %5.2f%%    %5.2f%%    %5.2f%%",
                  chainMean[0] * 100, chainMean[1] * 100, chainMean[2] * 100);
    std::snprintf(b, sizeof b, "MAX  e   %5.2f%%    %5.2f%%    %5.2f%%",
                  chainMax[0] * 100, chainMax[1] * 100, chainMax[2] * 100);
    const bool monotone =
        chainMean[0] > chainMean[1] && chainMean[1] > chainMean[2];
    penMonoB(pen, 8.5f, kBone, 0.1f);
    pen.text(a, x0, y0 + 160);
    penMono(pen, 8.5f, kSteel, 0.1f);
    pen.text(b, x0, y0 + 171);
    penMono(pen, 7.5f, monotone ? hex(0x4FC79E) : kRed, 0.1f);
    pen.text(monotone ? "mean e(1) > mean e(4) > mean e(10)  \xe2\x9c\x93"
                      : "MONOTONICITY FAILED THIS FRAME",
             x0, y0 + 182);
  }

  Element panelB3() {
    auto restRow = [&](const char* name, const char* val, bool anchor) {
      return box()
          .row()
          .height(Dim(11))
          .shrink(0)
          .child(t(name, mono(8.0f, anchor ? kBlue : kSteel, 0.1f)).grow(1))
          .child(t(val, anchor ? monoB(8.0f, kBlue, 0.1f)
                               : mono(8.0f, kBone, 0.1f)));
    };
    return panel(kPanelBH[2], "B3 \xc2\xb7 REST LENGTHS & PRODUCTION", 6)
        .gap(3)
        .child(restRow("head \xe2\x80\x93 neck", "56.0", false))
        .child(restRow("shoulder bar", "168.0", false))
        .child(restRow("neck \xe2\x80\x93 waist (brace)", "176.0", false))
        .child(restRow("hip bar", "140.0", false))
        .child(restRow("hip \xe2\x80\x93 knee  (THE ANCHOR)", "100.0", true))
        .child(restRow("knee \xe2\x80\x93 foot", "98.0", false))
        .child(t("restlength = 100 ON THE THIGH FIXES THE FIGURE AT 486.2 "
                 "UNITS \xe2\x80\x94 48.6% OF THE PAPER'S OWN CUBE. THIGH "
                 "100.0 / SHANK 98.0 IS 1.91% APART, SO THE DRILLIS & CONTINI "
                 "CROSS-CHECK IS DROPPED: NO PRIMARY SCAN, AND THE DIAGRAM "
                 "WOULD HAVE FAILED IT.",
                 ui(7.0f, kTick, 0.4f)))
        .child(box().height(Dim(4)).shrink(0))
        .child(t("IO INTERACTIVE / EIDOS \xc2\xb7 19 NOV 2000 \xc2\xb7 GLACIER "
                 "\xc2\xb7 DirectX 7.0a \xc2\xb7 GDC 2001, SAN JOSE",
                 ui(7.0f, kSteel, 0.3f)))
        .child(t("\"THE PRESS OXYMORON: LIFELIKE DEATH ANIMATIONS\"",
                 ui(7.0f, kSteel, 0.3f)))
        .child(t("HITMAN.INI: \"enableconsole 1\" + \"consolecmd ip_debug 1\" "
                 "\xe2\x80\x94 SHIFT+F9 BOMBS AN NPC, K = FREE CAM",
                 ui(7.0f, kSteel, 0.3f)));
  }

  // =========================================================================

  Element header() {
    Track rise{
        .effect = fx::rise(22.0f),
        .stagger = {.eachMs = 24, .durationMs = 440},
        .progress = animate(
            from(0.0f).to(1.0f),
            {.duration = 1100ms, .ease = ch::easeOutQuad, .delay = 120ms})};
    return box()
        .column()
        .height(Dim(kHeaderH))
        .shrink(0)
        .gap(3)
        .child(
            t("STATE AND CONTACT", ui(10.0f, kSteel, 2.6f))
                .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
                .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})))
        .child(
            t("THE HITMAN RAGDOLL, 2000", faced(heavyFace(), 42, kBone, -0.3f))
                .key("title")
                .fx(std::move(rise)))
        .child(t("Thomas Jakobsen, IO Interactive \xe2\x80\x94 \"Advanced "
                 "Character Physics\", GDC 2001 \xc2\xb7 shipped in Hitman: "
                 "Codename 47 (Eidos, 19 Nov 2000, Glacier engine, DirectX "
                 "7.0a) \xc2\xb7 every stick coloured by its LIVE constraint "
                 "error",
                 ui(10.5f, kSteel, 0.1f))
                   .opacity(animate(from(0.0f).to(1.0f),
                                    {.duration = 240ms, .delay = 400ms})))
        .child(box().grow(1))
        .child(box().height(Dim(1)).shrink(0).fill(kKeyline).opacity(
            animate(from(0.0f).to(1.0f), {.duration = 400ms, .delay = 320ms})));
  }

  // =========================================================================

  void setup(sketch::DrawContext& ctx) override {
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(kInk);
    // This study brings its own canvas, background and capture instant
    // rather than inheriting any: 3.35 s is 0.75 s after the bomb fires at
    // loopT 2.60, with ragdoll, cloth and plants all in full inverse-square
    // flight. Any later and the SETTLE phase has the body motionless on the
    // floor, which is the one still a physics study must not ship.
    ctx.captureAt(3.35);

    loopT = 0;
    simSteps = 0;
    didHit = didBomb = dragging = false;
    phase = 0;
    chainPhase = 0;
    contacts.clear();

    buildRig({300.0f, kCapsule});
    buildCloth();
    buildPlants();
    buildChains();

    dotAtlas = std::make_shared<instancing::Atlas>(2.0f);
    cellDot = dotAtlas->cell(box().shape(shapes::circle()).fill(kBone), {5, 5});
    cellPin = dotAtlas->cell(
        box()
            .shape(shapes::circle())
            .fill(kBone)
            .stroke(stroke(1.2f, Fill::color(kRed), PathFormat::Align::Inner)),
        {7, 7});
    dotPool = std::make_shared<instancing::Pool>();

    // The stick cell: a bar with BUTT ends. Round caps would deform under
    // the sizes() lane — a satisfied constraint holds the stretch to a
    // percent or so, but the cell must not assume that.
    barAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // A PILL, not a bar, is what a stick's collision proxy really is — and
    // it is the case sizes() cannot hold: one cell length serving 24
    // different stick lengths scales x per instance against a fixed y, so
    // round ends stretch into ellipses of whatever aspect that instance
    // asked for. Rounded corners on a bar tolerate that; caps do not.
    cellBar = barAtlas->cell(box().corners({4}).fill(kBone), {32, 8});
    barPool = std::make_shared<instancing::Pool>();
    (void)barPool->sizes();  // materialise the lane

    // The clock. Verlet is only correct at a fixed Δt, and the alphaOut
    // parameter publishes the leftover fraction of a step: a verlet body's
    // state IS the pair (x*, x), so lerp(x*, x, alpha) is the integrator's
    // own interpolant, and drawing through it costs nothing extra.
    ctx.ticker.addFixed(
        kSimHz, [this] { stepPhysics(); return true; }, 8, &alpha);

    headerEl = header();
    overlayEl = stageOverlay();
    barsEl = instancedHalf();
    colAEl = {panelA1(), panelA2(), panelA3()};
    colBEl = {panelB1(), panelB2(), panelB3()};

    ctx.pen.noStroke();
    ctx.pen.textAlign(draw::LEFT, draw::TOP);
  }

  void draw(Pen& pen) override {
    const double ms = pen.millis();
    // The pools are written every frame, because what they carry is the
    // interpolated position and that moves between steps as well as
    // across them.
    writeDotPool();
    {
      // The pool's positions are LOCAL to the guest's own box, which is
      // where the instances leaf paints them.
      float sc = 1;
      SkPoint off{0, 0};
      rigFit(150.0f, 74.0f, &sc, &off);
      writeBarPool({off.fX, off.fY + 2.0f}, sc);
    }

    pen.background(kInk);
    pen.element(headerEl,
                SkRect::MakeXYWH(kPad, kPad, kCanvasW - 2 * kPad, kHeaderH));

    // --- the stage -------------------------------------------------------
    const SkRect stageBox = SkRect::MakeXYWH(kStageX, kBodyY, kStage, kStage);
    pen.push();
    pen.translate(kStageX, kBodyY);
    worldBox(pen, ms);
    stageChrome(pen, ms);
    simulation(pen);
    pen.pop();
    pen.element(overlayEl, stageBox);
    blastGlow(pen);
    pen.push();
    pen.translate(kStageX, kBodyY);
    stageLabels(pen);
    pen.pop();

    const float insetA = cue(ms, 900, 340);
    figPenetration(pen, kStageX + 16, kBodyY + 16, insetA);
    figFriction(pen, kStageX + 512, kBodyY + 16, insetA);
    instancingStrip(pen, kStageX + 16, kBodyY + 178, cue(ms, 980, 340));
    errorLegend(pen, kStageX + 16, kBodyY + 302, cue(ms, 1440, 300));

    // --- the sidebar: six panels, three of them with a hole --------------
    pen.element(colAEl[0], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 0),
                                            kColW, kPanelAH[0]));
    pen.element(colAEl[1], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 1),
                                            kColW, kPanelAH[1]));
    pen.element(colAEl[2], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 2),
                                            kColW, kPanelAH[2]));
    pen.element(colBEl[0], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 0),
                                            kColW, kPanelBH[0]));
    pen.element(colBEl[1], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 1),
                                            kColW, kPanelBH[1]));
    pen.element(colBEl[2], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 2),
                                            kColW, kPanelBH[2]));
    // The holes: B1's heading is 12 tall over a 4 px gap, so the diagram
    // starts 30 px into the panel's padding box.
    const float holeX = kColBX + kPanelPad, holeW = kColW - 2 * kPanelPad;
    paintAnatomy(pen, holeX, panelTop(kPanelBH, 0) + kPanelPad + 12 + 4, holeW);
    paintChains(pen, holeX, panelTop(kPanelBH, 1) + kPanelPad + 12 + 4);
  }
};

SIGIL_SKETCH(
    HitmanVerlet, "Study \xc2\xb7 Motion",
    "Jakobsen's Advanced Character Physics (GDC 2001) \xe2\x80\x94 motion "
    "with state and contact")
