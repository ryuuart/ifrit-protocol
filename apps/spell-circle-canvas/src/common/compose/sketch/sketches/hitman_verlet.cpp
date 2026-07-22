// hitman_verlet.cpp — STUDY 04: the Hitman corpse (IO Interactive, 2000)
// and Thomas Jakobsen's verlet character physics.
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
//   4. FIGURE 9 RE-MEASURED, and the brief this sketch was built from was
//      wrong about it. Page 14 of the CMU scan rendered at 600 dpi,
//      thresholded, eroded by a disc of r = 8 px (which annihilates the
//      ~5 px sticks and every body-text stem) leaves exactly SIXTEEN
//      connected components of 620-657 px each - the particle dots, and
//      nothing else. Their centroids are the rest pose below. The
//      topology is confirmed: 16 particles, 24 sticks, the neck carrying
//      five of them. The LENGTHS are not: symmetrising each pair gives a
//      thigh ratio of 0.20569 of figure height, not 0.20184, so the
//      paper's own restlength = 100 on the thigh fixes the figure at
//      486.2 units, not 495. And thigh and shank differ by 1.91%, not by
//      0.41% - so the Drillis & Contini anthropometric cross-check the
//      brief wanted printed here would have FAILED, and in the opposite
//      direction (the diagram's thigh is longer than its shank; human
//      shanks are marginally longer than thighs). The [DC66] constants
//      could not be sourced from a primary scan either - the PSU page
//      carries the figure as an image and no transcription - so the
//      column is dropped, and this is said on the canvas.
//
// MEASURED WHILE BUILDING THIS (all reproducible from the sketch)
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
//   D. addFixed's REPRODUCIBILITY, and what alphaOut buys. Capturing
//      --at 3.10 at --fps 60 / 30 / 20 gives BYTE-IDENTICAL PNGs: 185
//      steps, alpha 1.00. --fps 10 gives 186 steps and alpha 0.00 - the
//      same instant on the other side of a step boundary - and the drawn
//      corpse differs by a mean of 1.03/255 per channel; --fps 15 lands
//      188 steps out and differs by 6.72/255, 6.5x more. --fps 5 needs 12
//      steps per frame against maxCatchUp = 8, the clamp trips exactly as
//      documented, and the capture lands at step 128 instead of 186. So
//      the clamp is not the source of the 10/15 Hz drift: float
//      accumulation is.
//
// RECONSTRUCTED (THE PAPER PUBLISHES NO SIMULATION CONSTANT BUT TWO)
//   - every rest length, from the re-measured Fig. 9 pose, ANCHORED by
//     assigning the paper's own restlength = 100 to the thigh
//   - 60 Hz fixed step; gravity 0.757 units/step^2, derived from a 1.75 m
//     stature (1 unit = 3.600 mm, so the paper's cube is a 3.60 m room)
//   - kFriction 0.14; capsule radius 14 u; knee inequality 100 u; blast
//     constant K = 179,200 u^3; hit displacement 45 u
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
// WHY IT IS A SigilCompose STRESS TEST
//   Every previous study in this program is FORWARD: frame N is a
//   function of frame N-1. A verlet body has no velocity variable at all,
//   so its shape at frame N is a function of what it TOUCHED at frame
//   N-1 - the solver projects, the integrator reads the projection as
//   velocity, and the next frame inherits it. That lands on the library's
//   weakest seam, geometry that is state, and on two things that landed
//   hours before this file was written:
//     * ticker.addFixed(hz, fn, maxCatchUp, &alphaOut) now publishes the
//       render interpolant. A verlet body's state IS the pair (x*, x), so
//       lerp(x*, x, alpha) is the integrator's OWN interpolant, free.
//       Every drawn particle here goes through it.
//     * decorations::paintOn(canvas, ctx, path, decoration) - so the
//       corpse's 24 capped-cylinder proxies are lines::Line values, its
//       centrelines are PathFormat values, and the drag leader wears a
//       PathFormat trim window riding its own head, all on geometry
//       recomputed sixty times a second inside custom().
//   Plus: instances(Mode::Live) for ~370 particle dots, a SECOND pool
//   driving the same 24 sticks through the new Pool::sizes() lane beside
//   the custom() path, shapes::parametric for the approximation-error
//   plot, lines::hatch for the drafting section on the bump,
//   patterns::grain on the floor, Material::glowUnit for the blast,
//   bind() with map/to/invert/clamp, slot()/renderSlot() for eight live
//   numbers, staggerChildren + withFrom + ease::outBack for the chrome.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/hitman_verlet.cpp \
//       --frame /tmp/hitman_verlet.png --at 3.35

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontTypes.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// Palette — this study's own chrome (a physics-debug register)

constexpr SkColor4f hex(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xFF) / 255.0f,
          (float)((rgb >> 8) & 0xFF) / 255.0f, (float)(rgb & 0xFF) / 255.0f, a};
}

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
      const float u = (e - kRampStop[i - 1]) / (kRampStop[i] - kRampStop[i - 1]);
      const SkColor4f &a = kRampCol[i - 1], &b = kRampCol[i];
      return {a.fR + (b.fR - a.fR) * u, a.fG + (b.fG - a.fG) * u,
              a.fB + (b.fB - a.fB) * u, alpha};
    }
  }
  return {kRampCol[4].fR, kRampCol[4].fG, kRampCol[4].fB, alpha};
}

// ---------------------------------------------------------------------------
// §6.1 — the frame. The STAGE IS SQUARE BECAUSE THE PAPER'S WORLD IS A CUBE.

constexpr float kCanvasW = 1560, kCanvasH = 920;
constexpr float kStage = 736;               // px, = the 1000-unit cube
constexpr float kUnit = kStage / 1000.0f;   // 0.736 px per world unit
constexpr float kColW = 352;

// §4.7 — the parameter table.
constexpr double kSimHz = 60.0;
constexpr float kDrag = 0.99f;         // documented "1.99", per VERIFIED 2
constexpr float kGravityStep = 0.757f; // a*dt^2, units/step^2 (derived)
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
constexpr float kFigureH = 100.0f / kThighRatio; // 486.17 world units

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

sk_sp<SkTypeface> face(const char *family, SkFontStyle style) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(family, style);
  return f ? f : mgr->matchFamilyStyle(nullptr, style);
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

sigil::weave::TextStyle type(sk_sp<SkTypeface> tf, float size, SkColor4f color,
                             float track = 0.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(tf);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor4f(color);
  s.paint.foreground.setAntiAlias(true);
  return s;
}
sigil::weave::TextStyle mono(float size, SkColor4f c, float track = 0.0f) {
  return type(monoFace(), size, c, track);
}
sigil::weave::TextStyle monoB(float size, SkColor4f c, float track = 0.0f) {
  return type(monoBoldFace(), size, c, track);
}
sigil::weave::TextStyle ui(float size, SkColor4f c, float track = 0.0f) {
  return type(uiFace(), size, c, track);
}
Element t(const char *s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

/** A sidebar panel shell. */
Element panel(float height, const char *heading, int order) {
  return box()
      .column()
      .width(Dim(kColW))
      .height(Dim(height))
      .shrink(0)
      .padding(14)
      .gap(7)
      .corners({5})
      .fill(kPanel)
      .clip(true)
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(withFrom(0.0f, 1.0f, {.duration = 300ms}))
      .translateX(withFrom(14.0f, 0.0f, {.duration = 300ms}))
      .key(std::string("panel") + std::to_string(order))
      .child(t(heading, ui(9.5f, kSteel, 1.9f)).height(Dim(12)).shrink(0));
}

/** A stage inset: the scrim over the world. */
Element stageInset(float x, float y, float w, float h, int order) {
  return box()
      .absolute()
      .left(Dim(x))
      .top(Dim(y))
      .width(Dim(w))
      .height(Dim(h))
      .padding(8)
      .column()
      .gap(3)
      .corners({5})
      .fill(hex(0x101116, 0.88f))
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(withFrom(0.0f, 1.0f, {.duration = 340ms, .delay = 900ms}))
      .scale(withFrom(0.94f, 1.0f,
                      {.duration = 340ms,
                       .ease = ease::outBack(1.70158f),
                       .delay = 900ms}))
      .key(std::string("inset") + std::to_string(order));
}

} // namespace

// ===========================================================================

struct HitmanVerlet : sigil::compose::sketch::Sketch {

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
    float radius = 0;         // capsule radius, world units
    bool stickContact = false;// also run the Sec.5 barycentric fix-up
  };

  struct Contact {
    SkPoint p{0, 0}, n{0, 1};
  };

  Body rig, cloth;
  std::array<Body, 4> plants;
  std::array<Body, 3> chains; // the 1 / 4 / 10 A/B, in PANEL px
  std::vector<Contact> contacts;

  // Named rig indices (order = the enumeration below).
  enum RigIdx {
    HEAD = 0, NECK, LSH, RSH, LEL, REL, LHA, RHA,
    LWA, RWA, LHI, RHI, LKN, RKN, LFO, RFO, NRIG
  };

  // -------------------------------------------------------------------------
  // Clocks and phase

  double loopT = 0, elapsed = 0;
  uint64_t simSteps = 0;
  bool stepped = false;
  bool didHit = false, didBomb = false;
  int phase = 0; // 0 SPAWN 1 HIT 2 BOMB 3 SETTLE 4 DRAG 5 RELEASE
  SkPoint dragTarget{0, 0}, dragFrom{0, 0};
  bool dragging = false;
  float chainPhase = 0;

  // Measured
  float stageMaxErr = 0;
  std::array<float, 3> chainMean{{0, 0, 0}}, chainMax{{0, 0, 0}};
  size_t contactCount = 0;
  double customUs = 0, instUs = 0;

  // Bindings
  ch::Output<float> alpha{0.0f};      // addFixed's render interpolant
  ch::Output<float> blastPhase{0.0f}; // 1 at detonation, decaying
  ch::Output<float> bodyFade{1.0f};

  // Instancing: the particle dots (the control case) and the sticks (the
  // new Pool::sizes() lane).
  std::shared_ptr<instancing::Atlas> dotAtlas;
  std::shared_ptr<instancing::Pool> dotPool;
  std::shared_ptr<instancing::Atlas> barAtlas;
  std::shared_ptr<instancing::Pool> barPool;
  int cellDot = 0, cellPin = 0, cellBar = 0;

  // =========================================================================
  // §4.2 — Verlet. There is no velocity anywhere in this function.

  static void verlet(Body &b, SkPoint gravityStep) {
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
  static void satisfyStick(Body &b, const Stick &s) {
    SkPoint d = b.x[s.b] - b.x[s.a];
    // f is NEGATIVE under tension and POSITIVE under compression: the sign
    // the exposition form lost (VERIFIED 1). Denominator >= r2 > 0, so no
    // division by zero is possible (VERIFIED 3).
    const float f = s.r2 / (dot(d, d) + s.r2) - 0.5f;
    d = d * f;
    const float w1 = b.invm[s.a], w2 = b.invm[s.b], sum = w1 + w2;
    if (sum <= 0.0f)
      return;
    b.x[s.a] = b.x[s.a] - d * (2.0f * w1 / sum);
    b.x[s.b] = b.x[s.b] + d * (2.0f * w2 / sum);
  }

  // §4.3(c) — the inequality constraint: enforced ONLY when too close.
  static void satisfyIneq(Body &b, const Ineq &q) {
    SkPoint d = b.x[q.b] - b.x[q.a];
    const float dd = dot(d, d);
    if (dd >= q.minLen * q.minLen)
      return;
    const float r2 = q.minLen * q.minLen;
    const float f = r2 / (dd + r2) - 0.5f;
    d = d * f;
    const float w1 = b.invm[q.a], w2 = b.invm[q.b], sum = w1 + w2;
    if (sum <= 0.0f)
      return;
    b.x[q.a] = b.x[q.a] - d * (2.0f * w1 / sum);
    b.x[q.b] = b.x[q.b] + d * (2.0f * w2 / sum);
  }

  // The world: the paper's cube plus one triangle. Returns the nearest
  // legal position for a point of radius `r`, or the point itself.
  static bool projectWorld(SkPoint p, float r, SkPoint *q) {
    SkPoint out = p;
    bool hit = false;
    if (out.fY < r) { out.fY = r; hit = true; }
    if (out.fX < r) { out.fX = r; hit = true; }
    if (out.fX > 1000.0f - r) { out.fX = 1000.0f - r; hit = true; }
    if (out.fY > 1000.0f - r) { out.fY = 1000.0f - r; hit = true; }
    // The bump, inflated by r: nearest point on the triangle.
    SkPoint onTri = nearestOnTriangle(out, kBumpA, kBumpB, kBumpC);
    SkPoint away = out - onTri;
    const float dist = len(away);
    const bool inside = pointInTriangle(out, kBumpA, kBumpB, kBumpC);
    if (inside || dist < r) {
      SkPoint n = dist > 1e-4f ? away * (1.0f / dist) : SkPoint{0, 1};
      if (inside)
        n = n * -1.0f;
      out = onTri + n * r;
      hit = true;
    }
    *q = out;
    return hit;
  }

  static SkPoint nearestOnSegment(SkPoint p, SkPoint a, SkPoint b) {
    const SkPoint ab = b - a;
    const float dd = dot(ab, ab);
    if (dd < 1e-6f)
      return a;
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
  static float cross2(SkPoint a, SkPoint b) { return a.fX * b.fY - a.fY * b.fX; }

  /** §4.3(a) — projection, with §7 friction. The penetration depth is
   *  measured BEFORE the projection, or friction has nothing to scale by. */
  void collideWorld(Body &b, bool record) {
    for (size_t i = 0; i < b.x.size(); ++i) {
      if (b.invm[i] <= 0.0f)
        continue;
      SkPoint q;
      if (!projectWorld(b.x[i], b.radius, &q))
        continue;
      const SkPoint delta = q - b.x[i];
      const float dp = len(delta);
      if (dp < 1e-5f)
        continue;
      const SkPoint n = delta * (1.0f / dp);
      b.x[i] = q; // restitution ZERO: clamp, never reflect
      // §7 friction: reduce the TANGENTIAL velocity by k*dp, by moving x*.
      const SkPoint v = b.x[i] - b.xo[i];
      const SkPoint vt = v - n * dot(v, n);
      const float m = len(vt);
      if (m > 1e-6f) {
        const float reduce = std::min(m, kFriction * dp);
        b.xo[i] = b.xo[i] + vt * (reduce / m); // never reverses: clamped
      }
      if (record && contacts.size() < 40)
        contacts.push_back({q, n});
    }
  }

  /** §5 — the capped cylinder against the world. The contact point lies ON
   *  the stick, so it is a barycentric blend and the correction
   *  distributes by the paper's own formula. Written as printed. */
  void collideSticks(Body &b, bool record) {
    for (const Stick &s : b.sticks) {
      const float c1 = 0.5f, c2 = 0.5f;
      const SkPoint p = b.x[s.a] * c1 + b.x[s.b] * c2;
      SkPoint q;
      if (!projectWorld(p, b.radius, &q))
        continue;
      const SkPoint D = q - p;
      const float dd = dot(D, D);
      if (dd < 1e-8f)
        continue;
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
  void satisfy(Body &b, bool record) {
    for (int it = 0; it < b.iterations; ++it) {
      const bool rec = record && it == b.iterations - 1;
      if (b.worldCollide) {
        collideWorld(b, rec);
        if (b.stickContact)
          collideSticks(b, rec);
      }
      for (const Stick &s : b.sticks)
        satisfyStick(b, s);
      for (const Ineq &q : b.ineqs)
        satisfyIneq(b, q);
      // §7 IK: keep setting the position INSIDE the loop.
      if (&b == &rig && dragging)
        b.x[LHA] = dragTarget;
    }
  }

  // =========================================================================
  // Construction

  static SkPoint world(Norm n, float side, SkPoint origin) {
    return {origin.fX + n.x * side * kFigureH, origin.fY + n.y * kFigureH};
  }

  void addStick(Body &b, int i, int j) {
    const float r = len(b.x[j] - b.x[i]);
    b.sticks.push_back({i, j, r, r * r});
  }

  void buildRig(SkPoint feet) {
    rig = Body{};
    rig.iterations = 4; // documented range 1-10; 3-4 for rigid bodies
    rig.worldCollide = true;
    rig.stickContact = true;
    rig.radius = kCapsule;
    auto P = [&](Norm n, float side) { return world(n, side, feet); };
    rig.x = {P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1),
             P(kEl, -1),  P(kEl, +1),  P(kHa, -1), P(kHa, +1),
             P(kWa, -1),  P(kWa, +1),  P(kHi, -1), P(kHi, +1),
             P(kKn, -1),  P(kKn, +1),  P(kFo, -1), P(kFo, +1)};
    rig.xo = rig.x; // x* = x  ->  zero velocity at spawn
    rig.invm.assign(NRIG, 1.0f);
    // The 24 sticks, in the paper's five groups (re-counted at 600 dpi).
    addStick(rig, HEAD, NECK);                       // 1
    addStick(rig, NECK, LSH); addStick(rig, NECK, RSH); // 2
    addStick(rig, LSH, RSH);                         // 1  shoulder bar
    addStick(rig, NECK, LWA); addStick(rig, NECK, RWA); // 2  long braces
    addStick(rig, LSH, LWA);  addStick(rig, RSH, RWA);  // 2  side
    addStick(rig, LSH, RWA);  addStick(rig, RSH, LWA);  // 2  crossed
    addStick(rig, LWA, RWA);                         // 1  waist bar
    addStick(rig, LWA, LHI);  addStick(rig, RWA, RHI);  // 2  side
    addStick(rig, LWA, RHI);  addStick(rig, RWA, LHI);  // 2  crossed
    addStick(rig, LHI, RHI);                         // 1  hip bar
    addStick(rig, LSH, LEL);  addStick(rig, RSH, REL);  // 2
    addStick(rig, LEL, LHA);  addStick(rig, REL, RHA);  // 2
    addStick(rig, LHI, LKN);  addStick(rig, RHI, RKN);  // 2  THE ANCHOR
    addStick(rig, LKN, LFO);  addStick(rig, RKN, RFO);  // 2
    // The documented inequality: "between the two knees - making sure that
    // the legs never cross".
    rig.ineqs.push_back({LKN, RKN, kKneeMin});
  }

  void buildCloth() {
    cloth = Body{};
    cloth.iterations = 1; // documented
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
        cloth.x.push_back({x0 + (float)c * sp + (r & 1 ? sp * 0.5f : 0.0f),
                           y0 - (float)r * sp});
    cloth.xo = cloth.x;
    cloth.invm.assign(cloth.x.size(), 1.0f);
    cloth.invm[0] = 0.0f; // "Constrain one particle of the cloth to origo"
    for (int r = 0; r < R; ++r) {
      for (int c = 0; c < C; ++c) {
        if (c + 1 < C)
          addStick(cloth, idx(c, r), idx(c + 1, r));
        if (r + 1 < R) {
          addStick(cloth, idx(c, r), idx(c, r + 1));
          const int dc = (r & 1) ? c + 1 : c - 1;
          if (dc >= 0 && dc < C)
            addStick(cloth, idx(c, r), idx(dc, r + 1));
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
      Body &b = plants[(size_t)p];
      b = Body{};
      b.iterations = 1; // documented — "exactly the right amount of bending"
      b.worldCollide = true;
      b.radius = 3.0f;
      // A patch of cloth three wide and four tall, fully braced, base row
      // pinned. Slenderness is the whole design question here: a 2-wide,
      // 7-tall strip is a wet noodle at ONE iteration and folds flat under
      // this study's gravity within a second — which is a fact about
      // aspect ratio and iteration count, not about the paper.
      // MEASURED: at this study's gravity a 3-wide braced truss of 24-unit
      // cells stands at 5 rows (97.4% of nominal height, 15.8% peak
      // constraint error) and FOLDS FLAT at 6. One relaxation iteration has
      // a slenderness limit, and that is a fact about the iteration count,
      // not about the paper.
      constexpr int R = 5, C = 3;
      constexpr float sp = 24.0f, w = 28.0f;
      const float lean = (p % 2 == 0) ? 2.2f : -2.8f;
      auto id = [](int r, int c) { return r * C + c; };
      for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c)
          b.x.push_back({roots[p] + lean * (float)r + (float)c * w,
                         (float)r * sp});
      b.xo = b.x;
      b.invm.assign(b.x.size(), 1.0f);
      for (int c = 0; c < C; ++c)
        b.invm[(size_t)id(0, c)] = 0.0f; // the base row is the root
      for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
          if (c + 1 < C)
            addStick(b, id(r, c), id(r, c + 1));
          if (r + 1 < R) {
            addStick(b, id(r, c), id(r + 1, c));
            // the documented support sticks, "between strategically chosen
            // couples of vertices sharing a neighbor" — both diagonals, so
            // every cell is a braced truss and the plant stands up
            if (c + 1 < C)
              addStick(b, id(r, c), id(r + 1, c + 1));
            if (c > 0)
              addStick(b, id(r, c), id(r + 1, c - 1));
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
      Body &b = chains[(size_t)k];
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

  void applyBlast(Body &b, SkPoint c) {
    for (size_t i = 0; i < b.x.size(); ++i) {
      if (b.invm[i] <= 0.0f)
        continue;
      SkPoint d = b.x[i] - c;
      const float r = std::max(12.0f, len(d));
      // |dx| = K / r^2  ->  dx = K (x-c) / r^3
      SkPoint push = d * (kBlastK / (r * r * r));
      const float m = len(push);
      if (m > 45.0f)
        push = push * (45.0f / m);
      b.x[i] = b.x[i] + push; // a POSITION displacement; verlet does the rest
    }
  }

  float maxError(const Body &b) const {
    float e = 0;
    for (const Stick &s : b.sticks)
      e = std::max(e, std::abs(len(b.x[s.b] - b.x[s.a]) - s.r) / s.r);
    return e;
  }
  void chainStats(const Body &b, float *mean, float *mx) const {
    float sum = 0, m = 0;
    for (const Stick &s : b.sticks) {
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

    // Phase machine (§7.2).
    if (!didHit && loopT >= 1.10) {
      rig.x[RSH].fX -= kHitPush; // documented: displace ONE particle
      didHit = true;
    }
    if (!didBomb && loopT >= 2.60) {
      const SkPoint c = kBlast;
      applyBlast(rig, c);
      applyBlast(cloth, c);
      for (Body &p : plants)
        applyBlast(p, c);
      didBomb = true;
      blastPhase = 1.0f;
    }
    const bool wantDrag = loopT >= 6.20 && loopT < 9.40;
    if (wantDrag && !dragging) {
      dragFrom = rig.x[LHA];
      dragging = true;
    }
    if (!wantDrag)
      dragging = false;
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
    for (Body &p : plants) {
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
    blastPhase = std::max(0.0f, blastPhase.value() - (float)(1.0 / kSimHz) / 0.34f);
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
  SkPoint drawnWorld(const Body &b, size_t i) const {
    const float a = alpha.value();
    return b.xo[i] + (b.x[i] - b.xo[i]) * a;
  }
  SkPoint drawn(const Body &b, size_t i) const {
    return toStage(drawnWorld(b, i));
  }

  static SkPath segment(SkPoint a, SkPoint b) {
    SkPathBuilder p;
    p.moveTo(a);
    p.lineTo(b);
    return p.detach();
  }

  /** Every edge of a body as ONE path — ~260 cloth edges and 44 plant
   *  sticks are one stroke, not 300 elements. */
  SkPath bodyPath(const Body &b) const {
    SkPathBuilder p;
    for (const Stick &s : b.sticks) {
      p.moveTo(drawn(b, (size_t)s.a));
      p.lineTo(drawn(b, (size_t)s.b));
    }
    return p.detach();
  }

  /** The corpse's 24 sticks, three layers, all through the decoration
   *  vocabulary on geometry recomputed this frame (decorations::paintOn).
   *  Returns the microseconds spent, for the A/B against instances(). */
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
  void rigFit(float w, float h, float *scale, SkPoint *offset) const {
    const SkRect b = rigBounds();
    const float s = std::min(w / std::max(1.0f, b.width()),
                             h / std::max(1.0f, b.height()));
    *scale = s;
    *offset = {(w - b.width() * s) * 0.5f - b.fLeft * s,
               (h - b.height() * s) * 0.5f - b.fTop * s};
  }

  double paintRig(SkCanvas &c, const PaintContext &ctx, float scale,
                  SkPoint offset, float fade, bool proxies) const {
    const auto t0 = std::chrono::steady_clock::now();
    auto at = [&](size_t i) {
      const SkPoint p = drawn(rig, i);
      return SkPoint{offset.fX + p.fX * scale, offset.fY + p.fY * scale};
    };
    if (proxies) {
      // 1. The capped-cylinder proxies — the collision geometry the paper
      //    describes, made visible. lines::Line strokes with ROUND caps,
      //    which is exactly a capsule.
      SkPathBuilder all;
      for (const Stick &s : rig.sticks) {
        all.moveTo(at((size_t)s.a));
        all.lineTo(at((size_t)s.b));
      }
      lines::Line capsule;
      capsule.width = 2.0f * kCapsule * kUnit * scale;
      capsule.fill = Fill::color(hex(0x6FA8DC, 0.10f * fade));
      decorations::paintOn(c, ctx, all.detach(), capsule);
    }
    // 2. The centrelines, coloured by LIVE constraint error.
    for (const Stick &s : rig.sticks) {
      const float e =
          std::abs(len(rig.x[s.b] - rig.x[s.a]) - s.r) / s.r;
      decorations::paintOn(
          c, ctx, segment(at((size_t)s.a), at((size_t)s.b)),
          stroke(std::max(4.6f, 2.5f * scale),
                 Fill::color(errColor(e, fade))));
    }
    // 3. The inequality constraint — dotted, as Figure 8 draws it.
    PathFormat dotted = stroke(1.4f * scale, Fill::color(hex(0xC8402F, 0.75f * fade)));
    dotted.dashIntervals = {2.5f, 3.5f};
    decorations::paintOn(c, ctx, segment(at(LKN), at(RKN)), dotted);
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count();
  }

  // =========================================================================
  // The particle pool (Mode::Live) — the control case.

  void writeDotPool() {
    dotPool->clear();
    auto push = [&](const Body &b, int pinFrame) {
      for (size_t i = 0; i < b.x.size(); ++i)
        dotPool->add(drawn(b, i), b.invm[i] <= 0.0f ? pinFrame : cellDot);
    };
    push(rig, cellPin);
    push(cloth, cellPin);
    for (const Body &p : plants)
      push(p, cellPin);
    // Fade the whole field with the body.
    const float f = bodyFade.value();
    if (f < 1.0f) {
      auto tints = dotPool->tints();
      for (SkColor4f &t : tints)
        t.fA = f;
    }
  }

  /** The SECOND path for the SAME 24 sticks: one atlas cell (a bar with
   *  BUTT ends), rotations() = atan2(dy, dx), and the new sizes() lane
   *  carrying length and thickness independently — which RSXform's single
   *  uniform scale could not express at all. */
  void writeBarPool(SkPoint origin, float scale) {
    barPool->resize(rig.sticks.size());
    auto pos = barPool->positions();
    auto rot = barPool->rotations();
    auto tint = barPool->tints();
    auto frame = barPool->frames();
    auto size = barPool->sizes();
    const float f = bodyFade.value();
    for (size_t i = 0; i < rig.sticks.size(); ++i) {
      const Stick &s = rig.sticks[i];
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
      // cell's aspect by itself. See the report's §10.3 finding.
      size[i] = {L / 32.0f, 4.6f / 8.0f};
      const float e = std::abs(len(rig.x[s.b] - rig.x[s.a]) - s.r) / s.r;
      tint[i] = errColor(e, f);
    }
    barPool->touch();
  }

  // =========================================================================
  // The stage

  Element worldBox() {
    return box()
        .absolute()
        .inset(0)
        .stroke(stroke(1.5f, Fill::color(hex(0x6FA8DC, 0.45f)),
                       PathFormat::Align::Inner))
        .child(box() // the cube's top wall — real, just not interesting
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(0))
                   .width(Dim(kStage))
                   .height(Dim(1))
                   .fill(kInk)
                   .stroke([] {
                     PathFormat f =
                         stroke(1.5f, Fill::color(hex(0x6FA8DC, 0.22f)));
                     f.dashIntervals = {4.0f, 5.0f};
                     return f;
                   }()))
        .trim(0.0f, with(1.0f, {.duration = 620ms,
                                .ease = ch::easeOutCubic,
                                .delay = 240ms}))
        .key("worldbox");
  }

  Element stageChrome() {
    // The floor: a band from world y = 0 down out of frame is the bottom
    // edge of the cube, so the solid reads as a 26 px plinth under it plus
    // the surface line the paper's projection actually clamps to.
    const float floorTop = kStage - kCapsule * kUnit;
    return box()
        .absolute()
        .inset(0)
        .opacity(withFrom(0.0f, 1.0f, {.duration = 400ms, .delay = 520ms}))
        .child(box()
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(floorTop))
                   .width(Dim(kStage))
                   .height(Dim(kStage - floorTop))
                   .fill(kSolid)
                   // decorations::wash — the material-valued decoration,
                   // which landed WHILE this sketch was being written. The
                   // first spelling here was a sibling box with .fill(material)
                   // and .blend(), i.e. a whole extra node for a grain pass.
                   .overlay(decorations::wash(
                       patterns::grain(0.035f, 3, 11.0f, 0.5f),
                       SkBlendMode::kSoftLight, 0.6f)))
        .child(box()
                   .absolute()
                   .left(Dim(0))
                   .top(Dim(floorTop))
                   .width(Dim(kStage))
                   .height(Dim(1))
                   .fill(hex(0x6FA8DC, 0.5f)))
        // The bump, drawn to the paper's own Fig. 4 scale, with the
        // drafting-section hatch as a foreground — a generator, not a
        // texture. A box().outline(shapes::polygon(3)) INSCRIBES, which is
        // the wrong shape; the three points go in directly.
        .child(box()
                   .absolute()
                   .left(Dim(kBumpA.fX * kUnit))
                   .top(Dim(kStage - kBumpB.fY * kUnit))
                   .width(Dim((kBumpC.fX - kBumpA.fX) * kUnit))
                   .height(Dim(kBumpB.fY * kUnit))
                   .outline([](SkSize s) {
                     SkPathBuilder p;
                     p.moveTo(0, s.height());
                     p.lineTo(s.width() * 0.5f, 0);
                     p.lineTo(s.width(), s.height());
                     p.close();
                     return p.detach();
                   })
                   .fill(kSolid)
                   .foreground(lines::hatch(Fill::color(hex(0x6FA8DC, 0.20f)),
                                            6.0f, 1.0f, 45.0f))
                   .stroke(stroke(1.0f, Fill::color(hex(0x6FA8DC, 0.40f)))));
  }

  /** The corpse, the cloth, the plants, the contacts — one custom leaf.
   *  custom() measures ZERO on the main axis, so both dims are explicit. */
  Element simulation() {
    return custom([this](SkCanvas &c, const PaintContext &ctx) {
             const float f = bodyFade.value();
             // The cloth: one path for the triangle fill, one for the edges.
             {
               SkPathBuilder tri;
               constexpr int C = 5, R = 5;
               for (int r = 0; r + 1 < R; ++r)
                 for (int c = 0; c + 1 < C; ++c) {
                   const int a = r * C + c;
                   tri.moveTo(drawn(cloth, (size_t)a));
                   tri.lineTo(drawn(cloth, (size_t)(a + 1)));
                   tri.lineTo(drawn(cloth, (size_t)(a + C + 1)));
                   tri.lineTo(drawn(cloth, (size_t)(a + C)));
                   tri.close();
                 }
               SkPaint fp;
               fp.setAntiAlias(true);
               fp.setColor4f(hex(0x6FA8DC, 0.07f), nullptr);
               c.drawPath(tri.detach(), fp);
             }
             lines::Line clothEdge;
             clothEdge.width = 1.0f;
             clothEdge.fill = Fill::color(hex(0x8A8F9C, 0.45f));
             decorations::paintOn(c, ctx, bodyPath(cloth), clothEdge);
             lines::Line stem;
             stem.width = 2.0f;
             stem.fill = Fill::color(hex(0x8A8F9C, 0.70f));
             SkPathBuilder stems;
             for (const Body &p : plants)
               for (const Stick &st : p.sticks) {
                 stems.moveTo(drawn(p, (size_t)st.a));
                 stems.lineTo(drawn(p, (size_t)st.b));
               }
             decorations::paintOn(c, ctx, stems.detach(), stem);

             paintRig(c, ctx, 1.0f, {0, 0}, f, true);

             // The contact markers: an open ring and a normal tick at
             // every projection this frame. These are the frames where the
             // ramp fires.
             SkPaint mark;
             mark.setAntiAlias(true);
             mark.setStyle(SkPaint::kStroke_Style);
             mark.setStrokeWidth(1.3f);
             mark.setColor4f(hex(0xC8402F, 0.95f * f), nullptr);
             for (const Contact &k : contacts) {
               const SkPoint p = toStage(k.p);
               c.drawCircle(p.fX, p.fY, 3.5f, mark);
               c.drawLine(p.fX, p.fY, p.fX + k.n.fX * 14.0f,
                          p.fY - k.n.fY * 14.0f, mark);
             }

             // §7 IK: the target the hand is pinned to, "the hand of the
             // player". lines::cased for the leader, and a PathFormat with
             // its OWN trim window riding the leader's head — the seam
             // ROADMAP §7 had to correct once, on live geometry.
             if (dragging) {
               const SkPoint h = drawn(rig, LHA), tgt = toStage(dragTarget);
               const SkPath leader = segment(h, tgt);
               decorations::paintOn(
                   c, ctx, leader,
                   lines::cased(2.0f, Fill::color(hex(0x6FA8DC, 0.55f)), 4.0f));
               PathFormat head = stroke(2.6f, Fill::color(kRed));
               head.trimStart = 0.88f;
               head.trimEnd = 1.0f;
               decorations::paintOn(c, ctx, leader, head);
               SkPaint ring;
               ring.setAntiAlias(true);
               ring.setStyle(SkPaint::kStroke_Style);
               ring.setStrokeWidth(1.6f);
               ring.setColor4f(kRed, nullptr);
               c.drawCircle(tgt.fX, tgt.fY, 9.0f, ring);
             }
             // The blast site, marked permanently: the law is documented,
             // the constant is not.
             {
               const SkPoint b = toStage(kBlast);
               SkPaint x;
               x.setAntiAlias(true);
               x.setStyle(SkPaint::kStroke_Style);
               x.setStrokeWidth(1.0f);
               x.setColor4f(hex(0xC8402F, 0.75f), nullptr);
               c.drawLine(b.fX - 7, b.fY, b.fX + 7, b.fY, x);
               c.drawLine(b.fX, b.fY - 7, b.fX, b.fY + 7, x);
               c.drawCircle(b.fX, b.fY, 4.5f, x);
             }
             // The HIT chevron, 0.4 s on the displaced particle.
             if (loopT >= 1.10 && loopT < 1.50) {
               const SkPoint p = drawn(rig, RSH);
               SkPaint chev;
               chev.setAntiAlias(true);
               chev.setStyle(SkPaint::kStroke_Style);
               chev.setStrokeWidth(2.0f);
               chev.setColor4f(kRed, nullptr);
               SkPathBuilder b;
               b.moveTo(p.fX + 20, p.fY - 9);
               b.lineTo(p.fX + 9, p.fY);
               b.lineTo(p.fX + 20, p.fY + 9);
               c.drawPath(b.detach(), chev);
             }
           })
        .absolute()
        .inset(0)
        .width(Dim(kStage))
        .height(Dim(kStage))
        .cache(Cache::None);
  }

  Element blastFlash() {
    const SkPoint c = toStage(kBlast);
    return disc(c, 120.0f)
        .fill(Material::glowUnit({0.5f, 0.5f}, 1.0f,
                                 {{0.0f, hex(0xFFF3E2, 1.0f)},
                                  {0.35f, hex(0xFFC98A, 0.55f)},
                                  {1.0f, hex(0xC8402F, 0.0f)}}))
        .blend(SkBlendMode::kPlus)
        .opacity(bind(&blastPhase).map(ch::easeOutQuad).clamp(0.0f, 1.0f))
        .key("blast");
  }

  /** The A/B strip: the SAME 24 sticks through instances()+sizes() beside
   *  the custom() path. Study 03 shipped this gap as a picture; the lane
   *  landed because of it, so this one ships the fix as a picture. */
  Element instancingStrip() {
    constexpr float w = 330, h = 108;
    return box()
        .absolute()
        .left(Dim(16))
        .top(Dim(178))
        .width(Dim(w))
        .height(Dim(h))
        .column()
        .padding(7)
        .gap(2)
        .corners({5})
        .fill(hex(0x101116, 0.88f))
        .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
        .opacity(withFrom(0.0f, 1.0f, {.duration = 340ms, .delay = 980ms}))
        .key("strip")
        .child(t("SAME 24 STICKS \xc2\xb7 instances()+sizes() vs custom()",
                 ui(7.5f, kSteel, 0.9f))
                   .height(Dim(10))
                   .shrink(0))
        .child(box()
                   .grow(1)
                   .child(box() // the instanced half
                              .absolute()
                              .left(Dim(0))
                              .top(Dim(0))
                              .width(Dim(150))
                              .height(Dim(76))
                              .child(instancing::instances(
                                  barAtlas, barPool, instancing::Mode::Live)))
                   .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                            // The custom() half, fitted to the same cell.
                            float sc = 1;
                            SkPoint off{0, 0};
                            rigFit(150.0f, 74.0f, &sc, &off);
                            off.fX += 166.0f;
                            off.fY += 2.0f;
                            customUs = customUs * 0.9f +
                                       0.1f * paintRig(c, ctx, sc, off,
                                                       bodyFade.value(), false);
                            // The instanced path's own cost, measured through
                            // the SAME call instances(Mode::Live) makes (it is
                            // custom(){ detail::stamp(...) } and nothing else),
                            // issued into a 1 px clip so only the CPU-side
                            // array build and the draw call are timed.
                            const auto s0 = std::chrono::steady_clock::now();
                            c.save();
                            c.clipRect(SkRect::MakeWH(1, 1));
                            instancing::detail::stamp(c, ctx, *barAtlas,
                                                      *barPool,
                                                      SkBlendMode::kSrcOver);
                            c.restore();
                            const auto s1 = std::chrono::steady_clock::now();
                            instUs = instUs * 0.9 +
                                     0.1 * std::chrono::duration<double,
                                                                 std::micro>(
                                               s1 - s0)
                                               .count();
                          })
                              .absolute()
                              .left(Dim(0))
                              .top(Dim(0))
                              .width(Dim(w - 14))
                              .height(Dim(76))
                              .cache(Cache::None))
                   .child(box()
                              .absolute()
                              .left(Dim(158))
                              .top(Dim(0))
                              .width(Dim(1))
                              .height(Dim(76))
                              .fill(hex(0x191B22, 0.9f))))
        .child(slot("benchStat").height(Dim(10)).shrink(0));
  }

  Element errorLegend() {
    auto swatch = [&](int i, const char *label) {
      return box()
          .column()
          .gap(3)
          .width(Dim(48))
          .shrink(0)
          .child(box()
                     .height(Dim(7))
                     .fill(kRampCol[i])
                     .scaleX(withFrom(0.0f, 1.0f,
                                      {.duration = 200ms, .delay = 1500ms}))
                     .transformOrigin(0.0f, 0.5f))
          .child(t(label, mono(6.5f, kTick, 0.2f)));
    };
    return box()
        .absolute()
        .left(Dim(16))
        .top(Dim(302))
        .width(Dim(336))
        .height(Dim(48))
        .column()
        .gap(4)
        .padding(7)
        .corners({4})
        .fill(hex(0x101116, 0.86f))
        .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
        .opacity(withFrom(0.0f, 1.0f, {.duration = 300ms, .delay = 1440ms}))
        .key("legend")
        .child(box()
                   .row()
                   .gap(2)
                   .staggerChildren(40ms)
                   .child(swatch(0, "0.000"))
                   .child(swatch(1, "0.004"))
                   .child(swatch(2, "0.010"))
                   .child(swatch(3, "0.020"))
                   .child(swatch(4, "\xe2\x89\xa5.035"))
                   .child(box().grow(1))
                   .child(t("e = ||x2\xe2\x88\x92x1|\xe2\x88\x92r| / r",
                            ui(7.0f, kSteel, 0.3f))))
        .child(slot("stageStat").height(Dim(9)).shrink(0));
  }

  Element figPenetration() {
    // Fig. 4b/5b: the paper's own worked case, c1 = 0.75, c2 = 0.25.
    return stageInset(16, 16, 208, 148, 1)
        .child(t("FIG. 4b/5b \xc2\xb7 \xc2\xa7" "5 PENETRATION",
                 ui(7.5f, kSteel, 1.2f))
                   .height(Dim(10))
                   .shrink(0))
        .child(custom([](SkCanvas &c, const PaintContext &ctx) {
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setStyle(SkPaint::kStroke_Style);
                 // the obstacle
                 p.setStrokeWidth(1.0f);
                 p.setColor4f(hex(0x2A2E38), nullptr);
                 SkPaint fillp;
                 fillp.setAntiAlias(true);
                 fillp.setColor4f(hex(0x2A2E38), nullptr);
                 SkPathBuilder ob;
                 ob.moveTo(10, 56);
                 ob.lineTo(96, 14);
                 ob.lineTo(180, 56);
                 ob.lineTo(180, 62);
                 ob.lineTo(10, 62);
                 ob.close();
                 c.drawPath(ob.detach(), fillp);
                 // the stick, penetrating a quarter along
                 const SkPoint x1{22, 44}, x2{162, 26};
                 const SkPoint pp{x1.fX * 0.75f + x2.fX * 0.25f,
                                  x1.fY * 0.75f + x2.fY * 0.25f};
                 const SkPoint q{pp.fX + 2, pp.fY - 17};
                 p.setStrokeWidth(2.0f);
                 p.setColor4f(hex(0x6FA8DC), nullptr);
                 c.drawLine(x1.fX, x1.fY, x2.fX, x2.fY, p);
                 p.setStrokeWidth(1.4f);
                 p.setColor4f(kRed, nullptr);
                 c.drawLine(pp.fX, pp.fY, q.fX, q.fY, p);
                 SkPaint dotp;
                 dotp.setAntiAlias(true);
                 dotp.setColor4f(kBone, nullptr);
                 c.drawCircle(x1.fX, x1.fY, 2.6f, dotp);
                 c.drawCircle(x2.fX, x2.fY, 2.6f, dotp);
                 dotp.setColor4f(kRed, nullptr);
                 c.drawCircle(pp.fX, pp.fY, 2.2f, dotp);
                 c.drawCircle(q.fX, q.fY, 2.2f, dotp);
                 (void)ctx;
               })
                   .height(Dim(64))
                   .shrink(0))
        .child(t("p = c1\xc2\xb7x1 + c2\xc2\xb7x2,  c1 = 0.75, c2 = 0.25",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("\xce\xbb = (q\xe2\x88\x92p)\xc2\xb7\xce\x94 / ((c1\xc2\xb2+c2\xc2\xb2)\xc2\xb7\xce\x94\xc2\xb2)",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("x1' = x1 + c1\xce\xbb\xce\x94    x2' = x2 + c2\xce\xbb\xce\x94",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("THE FIX-UP VIOLATES THE STICK. RELAX AGAIN.",
                 ui(6.5f, kTick, 0.6f)));
  }

  Element figFriction() {
    return stageInset(512, 16, 208, 148, 2)
        .child(t("FIG. 10 \xc2\xb7 \xc2\xa7" "7 FRICTION", ui(7.5f, kSteel, 1.2f))
                   .height(Dim(10))
                   .shrink(0))
        .child(custom([](SkCanvas &c, const PaintContext &ctx) {
                 SkPaint fillp;
                 fillp.setAntiAlias(true);
                 fillp.setColor4f(hex(0x2A2E38), nullptr);
                 SkPathBuilder ground;
                 ground.moveTo(8, 58);
                 ground.quadTo(96, 34, 184, 58);
                 ground.lineTo(184, 70);
                 ground.lineTo(8, 70);
                 ground.close();
                 c.drawPath(ground.detach(), fillp);
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setStyle(SkPaint::kStroke_Style);
                 p.setStrokeWidth(1.2f);
                 // the box, sunk by d_p
                 p.setColor4f(hex(0x6FA8DC, 0.55f), nullptr);
                 c.drawRect(SkRect::MakeXYWH(58, 30, 34, 22), p);
                 p.setColor4f(kRed, nullptr);
                 c.drawLine(75, 52, 75, 44, p); // d_p
                 p.setColor4f(hex(0x6FA8DC), nullptr);
                 c.drawLine(96, 40, 146, 40, p); // v_t before
                 c.drawLine(146, 40, 140, 36, p);
                 c.drawLine(146, 40, 140, 44, p);
                 p.setColor4f(hex(0xE8E6E1), nullptr);
                 c.drawLine(96, 52, 124, 52, p); // v_t after
                 c.drawLine(124, 52, 118, 48, p);
                 c.drawLine(124, 52, 118, 56, p);
                 (void)ctx;
               })
                   .height(Dim(64))
                   .shrink(0))
        .child(t("d_p MEASURED BEFORE THE PROJECTION,",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("v_t REDUCED BY k\xc2\xb7" "d_p BY MOVING x*.",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("NEVER LET v_t REVERSE \xe2\x80\x94 CLAMP TO ZERO.",
                 mono(7.0f, kBlue, 0.1f)))
        .child(t("RESTITUTION IS ZERO: PARTICLES DO NOT BOUNCE.",
                 ui(6.5f, kTick, 0.6f)));
  }

  Element phaseStrip() {
    const char *names[5] = {"SPAWN", "HIT", "BOMB", "SETTLE", "DRAG"};
    auto row = box().row().gap(9);
    for (int i = 0; i < 5; ++i)
      row.child(t(names[i], ui(8.0f, i == phase ? kRed : hex(0x8A8F9C, 0.45f),
                               1.7f)));
    return row;
  }

  Element stage() {
    return box()
        .width(Dim(kStage))
        .height(Dim(kStage))
        .shrink(0)
        .child(worldBox())
        .child(stageChrome())
        .child(simulation())
        .child(box()
                   .absolute()
                   .inset(0)
                   .child(instancing::instances(dotAtlas, dotPool,
                                                instancing::Mode::Live)))
        .child(blastFlash())
        .child(t("(0, 0)", mono(7.5f, kTick))
                   .absolute()
                   .left(Dim(7))
                   .top(Dim(kStage - 13)))
        .child(t("(1000, 1000)", mono(7.5f, kTick))
                   .absolute()
                   .left(Dim(kStage - 62))
                   .top(Dim(5)))
        .child(t("\xc2\xa7" "4 \xc2\xb7 TRIANGULAR MESH \xc2\xb7 ONE PARTICLE "
                 "PINNED \xc2\xb7 ONE ITERATION \xc2\xb7 THE SAG IS THE "
                 "ITERATION COUNT",
                 ui(7.0f, kTick, 0.5f))
                   .absolute()
                   .left(Dim(452))
                   .top(Dim(412))
                   .width(Dim(268))
                   .textAlign(sigil::weave::TextAlignment::kEnd))
        .child(t("\xc2\xa7" "4 \xc2\xb7 PLANTS = CLOTH + SUPPORT STICKS \xc2\xb7 "
                 "ONE ITERATION \xc2\xb7 BASE ROW PINNED",
                 ui(7.0f, kTick, 0.5f))
                   .absolute()
                   .left(Dim(452))
                   .top(Dim(596))
                   .width(Dim(268))
                   .textAlign(sigil::weave::TextAlignment::kEnd))
        .child(figPenetration())
        .child(figFriction())
        .child(instancingStrip())
        .child(errorLegend())
        .child(slot("phase").absolute().left(Dim(16)).top(Dim(690)))
        .child(box()
                   .absolute()
                   .left(Dim(240))
                   .top(Dim(18))
                   .width(Dim(262))
                   .column()
                   .gap(2)
                   .child(t("736 \xc3\x97 736 px = THE PAPER'S CUBE "
                            "(0,0,0)\xe2\x80\x93(1000,1000,1000) IN THE PLANE.",
                            ui(7.0f, kTick, 0.4f)))
                   .child(t("1 UNIT = 0.736 px = 3.60 mm \xc2\xb7 THE STAGE IS "
                            "SQUARE BECAUSE THE WORLD IS A CUBE.",
                            ui(7.0f, kTick, 0.4f)))
                   .child(box().height(Dim(3)).shrink(0))
                   .child(t("\xc2\xa7" "7 BOMB \xe2\x8a\x95 \xc2\xb7 |\xce\x94x| = "
                            "K / |x\xe2\x88\x92" "c|\xc2\xb2 \xc2\xb7 EVERY "
                            "PARTICLE, ONCE \xc2\xb7 THE INTEGRATOR MAKES IT "
                            "VELOCITY",
                            ui(7.0f, hex(0xC8402F, 0.8f), 0.4f))));;
  }

  // =========================================================================
  // Sidebar — column A

  Element codeLine(const char *s, SkColor4f c, bool caret = false) {
    auto row = box().row().gap(4).height(Dim(12)).shrink(0);
    row.child(t(caret ? "\xe2\x97\x84" : " ", mono(7.0f, caret ? kRed : kInk))
                  .width(Dim(8))
                  .shrink(0));
    row.child(t(s, mono(9.5f, c, 0.1f)));
    return row;
  }

  Element panelA1() {
    return panel(156, "A1 \xc2\xb7 VERLET \xe2\x80\x94 NO VELOCITY VARIABLE", 1)
        .gap(4)
        .child(t("x' = 2x \xe2\x88\x92 x* + a\xc2\xb7\xce\x94t\xc2\xb2      x* = x",
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
    return panel(288,
                 "A2 \xc2\xb7 THE STICK CONSTRAINT, AND A SIGN", 2)
        .child(codeLine("delta = x2-x1;", kBlue))
        .child(codeLine("deltalength = sqrt(delta*delta);", kBlue))
        .child(codeLine("diff = (deltalength-restlength)/deltalength;", kBlue))
        .child(codeLine("x1 -= delta*0.5*diff;", kRed, true))
        .child(codeLine("x2 += delta*0.5*diff;", kRed, true))
        .child(box().height(Dim(2)).shrink(0))
        .child(t("r = 100, |x2\xe2\x88\x92x1| = 120 \xe2\x86\x92 diff = 1/6, "
                 "delta\xc2\xb7" "0.5\xc2\xb7" "diff = (10, 0)",
                 mono(7.5f, kSteel, 0.1f)))
        .child(t("AS PRINTED : x1 = (\xe2\x88\x92" "10,0)  x2 = (130,0)  "
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
            const float x = (u - 1.25f) / 0.75f;   // u in [0.5, 2] -> [-1,1]
            const float y = -(s - (-0.1f)) / 0.45f; // s in [-0.55,0.35], flipped
            return SkPoint{x, std::clamp(y, -1.0f, 1.0f)};
          },
          0.5f, 2.0f, 240);
    };
    auto plotCurve = [&](bool approx, SkColor4f c, float w) {
      PathFormat f = stroke(w, Fill::color(c));
      if (!approx)
        f.dashIntervals = {3.5f, 3.0f};
      return box()
          .absolute()
          .inset(0)
          .outline(curve(approx))
          .stroke(f)
          .trim(0.0f, with(1.0f, {.duration = 520ms,
                                  .ease = ch::easeOutCubic,
                                  .delay = 1400ms}));
    };
    auto bar = [&](int i, const char *label, float h) {
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
                     .scaleY(withFrom(0.0f, 1.0f,
                                      {.duration = 220ms,
                                       .ease = ease::outBack(1.70158f),
                                       .delay = 1600ms}))
                     .transformOrigin(0.5f, 1.0f))
          .child(t(label, mono(7.0f, kSteel)));
    };
    return panel(244, "A3 \xc2\xb7 THE SQUARE-ROOT APPROXIMATION", 3)
        .child(codeLine("delta *= r*r/(delta*delta+r*r) - 0.5;", kBlue))
        .child(codeLine("x1 -= delta;   x2 += delta;", kBlue))
        .child(box()
                   .height(Dim(64))
                   .shrink(0)
                   .child(box() // s = 0
                              .absolute()
                              .left(Dim(0))
                              .top(Dim(39.1f))
                              .width(Dim(324))
                              .height(Dim(1))
                              .fill(hex(0x2A2E38)))
                   .child(box() // u = 1
                              .absolute()
                              .left(Dim(108))
                              .top(Dim(0))
                              .width(Dim(1))
                              .height(Dim(64))
                              .fill(hex(0x2A2E38)))
                   .child(plotCurve(false, kSteel, 1.4f))
                   .child(plotCurve(true, kBlue, 1.8f))
                   .child(t("s_exact", mono(7.0f, kSteel))
                              .absolute()
                              .left(Dim(4))
                              .top(Dim(2)))
                   .child(t("s_approx", mono(7.0f, kBlue))
                              .absolute()
                              .left(Dim(4))
                              .top(Dim(13)))
                   .child(t("u = d/r   0.5 \xe2\x86\x92 2.0", mono(7.0f, kTick))
                              .absolute()
                              .left(Dim(244))
                              .top(Dim(52))))
        .child(t("approx/exact:  0.60\xc3\x97 at u=0.5 \xc2\xb7 0.88 \xc2\xb7 "
                 "1.08 \xc2\xb7 1.15 \xc2\xb7 1.20\xc3\x97 at u=2.0",
                 mono(7.5f, kSteel, 0.1f)))
        .child(t("AGREES IN VALUE AND SLOPE AT u = 1. DENOMINATOR "
                 "d\xc2\xb2+r\xc2\xb2 \xe2\x89\xa5 r\xc2\xb2 > 0, SO IT "
                 "CANNOT DIVIDE BY ZERO: \xc2\xa7" "7's SINGULARITY NOTE "
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
        .child(t("\xc2\xa7" "7 SOFT CONSTRAINTS: HALF THE DEVIATION PER FRAME.",
                 ui(7.0f, kTick, 0.4f)));
  }

  // Sidebar — column B

  Element panelB1() {
    return panel(236, "B1 \xc2\xb7 FIGURE 9: THE ANATOMY", 4)
        .gap(4)
        .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                 // The rest pose at 156 px tall, centred.
                 const float H = 104.0f;
                 const float cx = ctx.size.width() * 0.5f, base = 112.0f;
                 auto P = [&](Norm n, float side) {
                   return SkPoint{cx + n.x * side * H, base - n.y * H};
                 };
                 std::array<SkPoint, NRIG> p = {
                     P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1),
                     P(kEl, -1),  P(kEl, +1),  P(kHa, -1), P(kHa, +1),
                     P(kWa, -1),  P(kWa, +1),  P(kHi, -1), P(kHi, +1),
                     P(kKn, -1),  P(kKn, +1),  P(kFo, -1), P(kFo, +1)};
                 SkPathBuilder b;
                 for (const Stick &s : rig.sticks) {
                   b.moveTo(p[(size_t)s.a]);
                   b.lineTo(p[(size_t)s.b]);
                 }
                 lines::Line l;
                 l.width = 1.5f;
                 l.fill = Fill::color(hex(0x8A8F9C, 0.9f));
                 decorations::paintOn(c, ctx, b.detach(), l);
                 PathFormat dotted =
                     stroke(1.0f, Fill::color(hex(0xC8402F, 0.9f)));
                 dotted.dashIntervals = {2.0f, 3.0f};
                 SkPathBuilder k;
                 k.moveTo(p[LKN]);
                 k.lineTo(p[RKN]);
                 decorations::paintOn(c, ctx, k.detach(), dotted);
                 SkPaint dp;
                 dp.setAntiAlias(true);
                 dp.setColor4f(kBone, nullptr);
                 for (const SkPoint &q : p)
                   c.drawCircle(q.fX, q.fY, 2.6f, dp);
                 // four labels
                 SkFont f(monoFace(), 7.0f);
                 SkPaint tp;
                 tp.setAntiAlias(true);
                 tp.setColor4f(kTick, nullptr);
                 auto lab = [&](const char *s, SkPoint at, float dx) {
                   c.drawSimpleText(s, strlen(s), SkTextEncoding::kUTF8,
                                    at.fX + dx, at.fY + 2.5f, f, tp);
                 };
                 lab("NECK", p[NECK], 7);
                 lab("WAIST", p[RWA], 7);
                 lab("HIP", p[RHI], 7);
                 tp.setColor4f(kRed, nullptr);
                 lab("|LK\xe2\x88\x92RK| \xe2\x89\xa5 100", p[LKN], -66);
               })
                   .height(Dim(118))
                   .shrink(0)
                   .cache(Cache::None))
        .child(t("16 PARTICLES \xc2\xb7 24 STICKS \xc2\xb7 1 INEQUALITY "
                 "(KNEES, \xc2\xa7" "6)",
                 monoB(8.5f, kBone, 0.1f)))
        .child(t("16\xc3\x97" "2 \xe2\x88\x92 24 = 8 PLANAR DOF   "
                 "(16\xc3\x97" "3 \xe2\x88\x92 24 = 24 IN THE PAPER'S 3D)",
                 mono(8.0f, kSteel, 0.1f)))
        .child(t("COMPARE \xc2\xa7" "5's TETRAHEDRON: 4\xc3\x97" "3 \xe2\x88\x92 6 = 6",
                 mono(8.0f, kSteel, 0.1f)))
        .child(t("RE-COUNTED AT 600 dpi: THRESHOLD, ERODE BY A DISC r = 8 px "
                 "\xe2\x80\x94 EVERY STICK AND EVERY BODY-TEXT STEM DIES AND "
                 "EXACTLY 16 COMPONENTS OF 620\xe2\x80\x93" "657 px SURVIVE. "
                 "THE PAPER PUBLISHES NO COUNT.",
                 ui(7.0f, kTick, 0.4f)));
  }

  Element panelB2() {
    return panel(264, "B2 \xc2\xb7 RELAXATION: 1 \xc2\xb7 4 \xc2\xb7 10", 5)
        .gap(4)
        .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                 for (int k = 0; k < 3; ++k) {
                   const Body &b = chains[(size_t)k];
                   for (const Stick &s : b.sticks) {
                     const float e =
                         std::abs(len(b.x[s.b] - b.x[s.a]) - s.r) / s.r;
                     const SkPoint a = drawnWorld(b, (size_t)s.a);
                     const SkPoint z = drawnWorld(b, (size_t)s.b);
                     decorations::paintOn(c, ctx, segment(a, z),
                                          stroke(2.2f, Fill::color(errColor(e))));
                   }
                   SkPaint dp;
                   dp.setAntiAlias(true);
                   for (size_t i = 0; i < b.x.size(); ++i) {
                     const SkPoint q = drawnWorld(b, i);
                     dp.setColor4f(b.invm[i] <= 0 ? kRed : kBone, nullptr);
                     c.drawCircle(q.fX, q.fY, 2.0f, dp);
                   }
                 }
                 SkFont f(monoBoldFace(), 9.0f);
                 SkPaint tp;
                 tp.setAntiAlias(true);
                 tp.setColor4f(kBone, nullptr);
                 const char *labels[3] = {"1", "4", "10"};
                 const float xs[3] = {54, 156, 256};
                 for (int k = 0; k < 3; ++k)
                   c.drawSimpleText(labels[k], strlen(labels[k]),
                                    SkTextEncoding::kUTF8, xs[k], 12, f, tp);
               })
                   .height(Dim(156))
                   .shrink(0)
                   .cache(Cache::None))
        .child(slot("chainStat").height(Dim(34)).shrink(0))
        .child(t("\"ITERATIONS USED IN HITMAN VARY BETWEEN 1 AND 10 WITH THE "
                 "KIND OF OBJECT SIMULATED.\" \xe2\x80\x94 \xc2\xa7" "7. "
                 "ORDER MATTERS AS MUCH AS COUNT: LISTED FROM THE PIN A CHAIN "
                 "CONVERGES IN ONE SWEEP AND ALL THREE ARE IDENTICAL. THESE "
                 "ARE LISTED FROM THE FREE END.",
                 ui(7.0f, kTick, 0.4f)));
  }

  Element panelB3() {
    auto restRow = [&](const char *name, const char *val, bool anchor) {
      return box()
          .row()
          .height(Dim(11))
          .shrink(0)
          .child(t(name, mono(8.0f, anchor ? kBlue : kSteel, 0.1f)).grow(1))
          .child(t(val, anchor ? monoB(8.0f, kBlue, 0.1f)
                               : mono(8.0f, kBone, 0.1f)));
    };
    return panel(188, "B3 \xc2\xb7 REST LENGTHS & PRODUCTION", 6)
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
    GlyphFx fx;
    fx.effect = glyphfx::rise(22.0f);
    fx.stagger = {.eachMs = 24, .durationMs = 440};
    fx.progress = withFrom(0.0f, 1.0f, {.duration = 1100ms,
                                        .ease = ch::easeOutQuad,
                                        .delay = 120ms});
    return box()
        .column()
        .height(Dim(100))
        .shrink(0)
        .gap(3)
        .child(t("TECHNIQUE STUDY 04 \xc2\xb7 STATE AND CONTACT",
                 ui(10.0f, kSteel, 2.6f))
                   .opacity(withFrom(0.0f, 1.0f, {.duration = 260ms}))
                   .translateY(withFrom(8.0f, 0.0f, {.duration = 260ms})))
        .child(t("THE HITMAN RAGDOLL, 2000", type(heavyFace(), 42, kBone, -0.3f))
                   .key("title")
                   .glyphFx(std::move(fx)))
        .child(t("Thomas Jakobsen, IO Interactive \xe2\x80\x94 \"Advanced "
                 "Character Physics\", GDC 2001 \xc2\xb7 shipped in Hitman: "
                 "Codename 47 (Eidos, 19 Nov 2000, Glacier engine, DirectX "
                 "7.0a) \xc2\xb7 every stick coloured by its LIVE constraint "
                 "error",
                 ui(10.5f, kSteel, 0.1f))
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 240ms, .delay = 400ms})))
        .child(box().grow(1))
        .child(box()
                   .height(Dim(1))
                   .shrink(0)
                   .fill(kKeyline)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 400ms, .delay = 320ms})));
  }

  Element describe() {
    return box()
        .column()
        .padding(32)
        .gap(20)
        .fill(kInk)
        .child(header())
        .child(box()
                   .row()
                   .gap(28)
                   .grow(1)
                   .child(stage())
                   .child(box()
                              .row()
                              .gap(28)
                              .width(Dim(732))
                              .shrink(0)
                              .child(box()
                                         .column()
                                         .gap(24)
                                         .width(Dim(kColW))
                                         .shrink(0)
                                         .staggerChildren(85ms)
                                         .child(panelA1())
                                         .child(panelA2())
                                         .child(panelA3()))
                              .child(box()
                                         .column()
                                         .gap(24)
                                         .width(Dim(kColW))
                                         .shrink(0)
                                         .staggerChildren(85ms)
                                         .child(panelB1())
                                         .child(panelB2())
                                         .child(panelB3()))));
  }

  // -------------------------------------------------------------------------
  // Live numbers. PropValue has no u8string case (ROADMAP §9), so numbers
  // that tick go through slot()/renderSlot() — the fourth study to land here.

  Element chainStatEl() {
    char a[96], b[96];
    std::snprintf(a, sizeof a, "MEAN e   %5.2f%%    %5.2f%%    %5.2f%%",
                  chainMean[0] * 100, chainMean[1] * 100, chainMean[2] * 100);
    std::snprintf(b, sizeof b, "MAX  e   %5.2f%%    %5.2f%%    %5.2f%%",
                  chainMax[0] * 100, chainMax[1] * 100, chainMax[2] * 100);
    const bool monotone =
        chainMean[0] > chainMean[1] && chainMean[1] > chainMean[2];
    return box()
        .column()
        .child(t(a, monoB(8.5f, kBone, 0.1f)))
        .child(t(b, mono(8.5f, kSteel, 0.1f)))
        .child(t(monotone ? "mean e(1) > mean e(4) > mean e(10)  \xe2\x9c\x93"
                          : "MONOTONICITY FAILED THIS FRAME",
                 mono(7.5f, monotone ? hex(0x4FC79E) : kRed, 0.1f)));
  }

  Element stageStatEl() {
    char buf[128];
    std::snprintf(buf, sizeof buf,
                  "MAX e %5.2f%%  \xc2\xb7  CONTACTS %2zu  \xc2\xb7  STEP "
                  "%llu  \xc2\xb7  \xce\xb1 %.2f",
                  stageMaxErr * 100, contactCount,
                  (unsigned long long)simSteps, (double)alpha.value());
    return t(buf, monoB(8.0f, errColor(stageMaxErr), 0.1f));
  }

  Element benchStatEl() {
    char buf[128];
    std::snprintf(buf, sizeof buf,
                  "24 sticks: instances()+sizes() %.0f \xc2\xb5s  \xc2\xb7  "
                  "custom()+paintOn %.0f \xc2\xb5s",
                  instUs, customUs);
    return t(buf, mono(7.0f, kTick, 0.2f));
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas((int)kCanvasW, (int)kCanvasH);
    ctx.background(kInk);

    loopT = elapsed = 0;
    simSteps = 0;
    stepped = false;
    didHit = didBomb = dragging = false;
    phase = 0;
    chainPhase = 0;
    customUs = instUs = 0;
    contacts.clear();

    buildRig({300.0f, kCapsule});
    buildCloth();
    buildPlants();
    buildChains();

    dotAtlas = std::make_shared<instancing::Atlas>(2.0f);
    cellDot = dotAtlas->cell(
        box().outline(shapes::circle()).fill(kBone), {5, 5});
    cellPin = dotAtlas->cell(
        box()
            .outline(shapes::circle())
            .fill(kBone)
            .stroke(stroke(1.2f, Fill::color(kRed), PathFormat::Align::Inner)),
        {7, 7});
    dotPool = std::make_shared<instancing::Pool>();

    // The stick cell: a bar with BUTT ends. Round caps would deform under
    // the sizes() lane (see the report) — a satisfied constraint holds the
    // stretch within ~1%, but the cell must not assume that.
    barAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // A PILL, not a bar: a capped cylinder is what a stick's collision
    // proxy is, and it is the case the roadmap predicts sizes() cannot
    // hold — one cell length serving 24 different stick lengths remaps the
    // cell's aspect by 0.19x-0.63x in x against a fixed 0.40x in y, so the
    // round ends come out as ellipses of that aspect. See the report.
    cellBar = barAtlas->cell(box().corners({4}).fill(kBone), {32, 8});
    barPool = std::make_shared<instancing::Pool>();
    (void)barPool->sizes(); // materialise the lane

    Composer &composer = ctx.composer;

    // §7.1 — the clock. Verlet is only correct at a fixed Δt, and the
    // alphaOut parameter (which landed hours before this file) publishes
    // the leftover fraction of a step: a verlet body's state IS the pair
    // (x*, x), so lerp(x*, x, alpha) is the integrator's own interpolant.
    ctx.ticker.addFixed(
        kSimHz,
        [this] {
          stepPhysics();
          stepped = true;
          return true;
        },
        8, &alpha);

    ctx.ticker.add([this, &composer](double dt) {
      elapsed += dt;
      if (stepped) {
        stepped = false;
        composer.renderSlot("chainStat", chainStatEl());
        composer.renderSlot("stageStat", stageStatEl());
        composer.renderSlot("benchStat", benchStatEl());
        composer.renderSlot("phase", phaseStrip());
      }
      writeDotPool();
      {
        float sc = 1;
        SkPoint off{0, 0};
        rigFit(150.0f, 74.0f, &sc, &off);
        writeBarPool({off.fX, off.fY + 2.0f}, sc);
      }
      return true;
    });

    composer.render(describe());
    composer.renderSlot("chainStat", chainStatEl());
    composer.renderSlot("stageStat", stageStatEl());
    composer.renderSlot("benchStat", benchStatEl());
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(HitmanVerlet)
