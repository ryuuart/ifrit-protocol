// genesis_fire.cpp — the Genesis Demo wall of fire (Lucasfilm
// Ltd, 1982) and the first particle system, drawn by a pen.
//
// SUBJECT  The wall-of-fire element of the ~67-second Genesis Demo in
//          Star Trek II: The Wrath of Khan (Paramount, 4 June 1982),
//          computed by the Computer Graphics Project of Lucasfilm Ltd
//          for a 500-line video raster (~250,000 px/frame) and shot to
//          VistaVision by ILM. Fires by William T. Reeves; sequence
//          directed by Alvy Ray Smith.
//
// SOURCES  [R83] W. T. Reeves, "Particle Systems - A Technique for
//                Modeling a Class of Fuzzy Objects", Computer Graphics
//                17(3), July 1983, beginning p. 359 (SIGGRAPH '83; repr.
//                ACM TOG 2(2), April 1983, pp. 91-108).
//                https://www.lri.fr/~mbl/ENS/IG2/devoir2/files/docs/fuzzyParticles.pdf
//          [S82] A. R. Smith, "Special Effects for Star Trek II: The
//                Genesis Demo", American Cinematographer 63(10),
//                October 1982, pp. 1038-1039, 1048-1050.
//                https://alvyray.com/Papers/CG/StarTrekII_GenesisDemo.pdf
//          [S-W] https://alvyray.com/Art/GenesisDemo.htm (credits; the
//                sequence was reused in Star Trek III and IV).
//
// DOCUMENTED (from [R83] unless noted)
//   - NParts_f    = MeanParts_f + Rand() * VarParts_f, Rand in [-1,+1]
//   - MeanParts_f = InitialMeanParts + DeltaMeanParts * (f - f0)
//   - InitialSpeed = MeanSpeed + Rand() * VarSpeed
//   - the seven particle attributes; generation shapes; ejection cone;
//     gravity as an acceleration -> parabolic arcs
//   - lifetimes measured in FRAMES; extinction on lifetime, on
//     intensity below a threshold, or below the planet surface
//   - rendering: point light sources, light ADDS, channels CLAMP, no
//     depth sort, no shadows, everything antialiased
//   - colour: predominantly red + a touch of green + a small blue;
//     red saturates first -> orange -> yellow; blue last -> white.
//     Over life: green and blue decay fast, red slower (cooling)
//   - motion blur: 24 fps, 1/50 s shutter ~ half the inter-frame
//     motion; a straight antialiased line pos(f) -> pos(f + 1/2)
//     ("the trajectory is actually parabolic, but the straight-line
//     approximation has so far proved sufficient" - fn. 4)
//   - two-level hierarchy: systems in concentric rings from the impact
//     point; count per ring = circumference * density; RANDOM angular
//     placement; overlap gives a continuous ring
//   - THE WALL IS A STAGGER: "the second-level particle systems began
//     generating particles at varying times on the basis of their
//     distance from the point of impact"
//   - census: Fig.4 ~21 systems / 25,000 particles; Fig.5 ~200 /
//     75,000; Fig.6 ~200 / 85,000; Figs.7-8 ~400 / >750,000
//   - Tom Duff added one local light above the ring when rendering the
//     planet surface (the only hand-placed light in the shot)
//   - [S82] 67-second shot; 250,000 px/frame on a 500-line monitor;
//     ~2 man-years over an 80-second piece; frames 5 min to 5 hr;
//     ~1 month of VAX time for the fractals; delivered 19 March 1982;
//     stars from the Yale Bright Star Catalogue (9,100 stars to mag
//     6.6) with per-star colours; planet placed at Epsilon Indi so the
//     Big Dipper reads and "our sun would appear as an extra star";
//     in-house planet name "Keti Bandar"
//
// RECONSTRUCTED (mine - NEITHER SOURCE PUBLISHES ONE PARAMETER VALUE)
//   - every constant in the parameter table, DERIVED from the census:
//     Fig.6's 85,000/200 = 425 particles alive per explosion, and
//     population = birth rate x lifetime. Choose MeanLife = 34 frames
//     and the rest follows. That arithmetic is printed on the canvas.
//   - the emission triple e0 = (0.220, 0.050, 0.009) that generates
//     the overlap ramp (the ADD-AND-CLAMP RULE is documented; this
//     seed is not)
//   - the colour-decay rates, the ejection angle, the sweep rate, the
//     10 s loop period, MeanSize/VarSize (the paper gives the model,
//     not the millimetres)
//   - the 2D reduction: a vertical slice through the paper's
//     axisymmetric Fig. 3 explosion, which is what its own Figs. 6-7
//     photograph
//   - all chrome colours, all layout geometry, the 4:3 stage
//
// UNVERIFIABLE / FLAGGED
//   - no frame-accurate colorimetry of the release print exists in any
//     source read
//   - [S82] CONTRADICTS ITSELF on length: its caption says "the
//     67-second shot", its body text says "60 seconds for the Genesis
//     Demo". Both are printed on the panel.
//   - "250,000 pixels on a 500-line monitor" is the documented figure;
//     512x486 (248,832 px) is MY inference as the nearest standard
//     raster, not a citation.
//
// VERIFIED INDEPENDENTLY BY THIS STUDY
//   Smith's claim that the Sun appears "as an extra star" in the Big
//   Dipper from the Genesis planet checks out to the degree. eps Indi
//   is at RA 22h03m Dec -56d47'; the antipode - the direction the Sun
//   lies in from there - is RA 10h03m Dec +56d47', inside Ursa Major
//   near the Dipper's bowl. At 3.64 pc and M_V = 4.83 the Sun reads
//   m = 4.83 + 5*log10(3.64) - 5 = 2.63, between Megrez (3.31) and
//   Merak (2.37). The number is printed on the star field.
//
// HOW IT IS DRAWN
//   A particle system is a LOOP: births, an integration, three
//   extinction rules and one additive pass, sixty times a minute of
//   film. So the piece is a pen program, and the parts divide by what
//   each is:
//     * the field is the pen's. 8,000 streaks per SkVertices list, six
//       triangles each with a colour ramp across the cross-section, put
//       down by `pen.vertices()` under `blendMode(ADD)` - light ADDS and
//       CLAMPS [R83 §2.5] - inside a `clip()` of the stage. The
//       magnified motion-blur callout beside it is the same quad drawn
//       in the pen's own words - `beginShape(TRIANGLE_STRIP)` with a
//       `fill()` between the vertices, so the corners either side of a
//       fill carry it and the ramp costs one shape rather than one per
//       band.
//     * every panel is a retained tree, painted through `pen.element`:
//       the census with its live row, the generation law, the overlap
//       ramp, the three-path bench over two instancing pools, the
//       production panel, the header with its cascading title. What a
//       described tree is good at - typography, layout, the pools - it
//       keeps.
//     * the stage's own furniture is two guests with the field drawn
//       between them: the star field, the Dipper, the regolith and the
//       shockwave under it, Fig. 2's live plan inset over it. One
//       canvas, so ordering is just the order the calls are made in.
//   `ticker.addFixed(24 Hz, ..., &simAlpha)` is the film clock, and its
//   leftover fraction rides the loop phase, so the wavefront, Duff's
//   light and the plan ring sweep smoothly at any draw rate while the
//   particles still step in whole film frames - which is what [R83]
//   counts lifetimes in.
//
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/genesis_fire.cpp \
//       --frame /tmp/genesis_fire.png
//
//   4.6 s into the 10 s loop is the one instant that shows the whole
//   stagger at once: the wavefront is close to the right edge (it clears
//   the stage at kFrontCrossSeconds), the leftmost systems have finished
//   generating and are burning out, and the rightmost have just ignited.

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkVertices.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
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
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::draw::Pen;
using sigil::material::skia::Paint;
using sigil::material::skia::toColor;
using sigil::weave::ports::pickTypeface;
namespace draw = sigil::draw;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// Palette — this study's own chrome, not sourced

constexpr SkColor4f kInk = hex(0x06070B);
constexpr SkColor4f kPanel = hex(0x0B0D14);
constexpr SkColor4f kBone = hex(0xE9ECF3);
constexpr SkColor4f kSteel = hex(0x77819A);
constexpr SkColor4f kSteelDim = hex(0x545E74);
constexpr SkColor4f kKeyline = hex(0x242A36);
constexpr SkColor4f kCyan = hex(0x4FB8D8);

// THE EMISSION SEED. Everything about the fire's colour is a consequence
// of this triple plus "light adds and clamps" [R83 §2.5].
constexpr float kE0r = 0.220f, kE0g = 0.050f, kE0b = 0.009f;

/** clamp(n * e0) — the colour of a pixel covered by n particles. This is
 *  not a palette; it is an overlap count. Red saturates at n=5, green at
 *  n=20, blue at n=111. */
SkColor4f overlap(int n) {
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
constexpr float kBodyY = kPad + kHeaderH + 24;   // 162
constexpr float kStageX = kPad;                  // 36
constexpr float kSideX = kPad + kStageW + 32;    // 956
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

float limbY(float x) {
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

// A positional shorthand over the library's designated-init `textStyle()`,
// for the one display line that names its own face.
weave::TextStyle faced(sk_sp<SkTypeface> tf, float size, SkColor4f color,
                       float track = 0.0f) {
  return weave::textStyle(
      {.face = std::move(tf), .size = size, .color = color, .track = track});
}
// The three registers the panel is set in.
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

/** The same register on the PEN: a pen carries one type and one fill, so
 *  a register is set rather than described. */
void penMono(Pen& pen, float size, SkColor4f c, float track = 0.0f) {
  pen.textFont(weave::Type{.face = monoFace(), .size = size, .track = track});
  pen.fill(c);
}

// The planet's silhouette: the limb arc, closed down to the stage floor.
std::function<SkPath(SkSize)> limbOutline() {
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
 *  own guest at its own box, so the entrance the column used to stagger
 *  is the panel's own delay. */
Element panel(float height, int order) {
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
      .opacity(animate(from(0.0f).to(1.0f),
                       {.duration = 300ms, .delay = delay}))
      .translateX(animate(from(14.0f).to(0.0f),
                          {.duration = 300ms, .delay = delay}))
      .key(std::string("panel") + std::to_string(order));
}

Element panelHead(const char* s) {
  return t(s, ui(9.5f, kSteel, 1.9f)).height(13).shrink(0);
}

}  // namespace

// ===========================================================================

struct GenesisFire final : sketch::DrawSketch {
  // --- the two levels ------------------------------------------------------
  struct Site {
    SkPoint p;      // surface point
    SkVector n, u;  // outward normal, surface tangent
    float t0 = 0;   // ignition time, s  (THE documented mechanism)
    // [R83 §3]: "Varying the mean velocity parameter caused the
    // explosions to be different heights." This is why the wall has a
    // ragged silhouette instead of a uniform arc.
    float vScale = 1.0f;
  };
  struct Particle {
    SkPoint pos{0, 0}, vel{0, 0};
    float r = 0, g = 0, b = 0;  // emission ([R83 §2.2]'s seven attributes)
    float size = 0, grow = 0;   // diameter, px + its per-frame rate
    float age = 0, life = 0;    // FRAMES
    uint16_t site = 0;
  };

  std::vector<Site> sites;
  std::vector<Particle> parts;
  std::vector<sk_sp<SkVertices>> fieldChunks;

  // --- the A/B bench: one explosion, three renderers -----------------------
  std::vector<Particle> abParts;
  std::vector<sk_sp<SkVertices>> abChunks;
  std::shared_ptr<instancing::Atlas> abAtlas;
  std::shared_ptr<instancing::Pool> abPool;

  // --- the two CONTROL pools (textbook instancing) -------------------------
  std::shared_ptr<instancing::Atlas> starAtlas;
  std::shared_ptr<instancing::Pool> starPool;
  std::shared_ptr<instancing::Atlas> planAtlas;
  std::shared_ptr<instancing::Pool> planPool;
  struct PlanMark {
    SkPoint p;
    float dist = 0;  // from the impact point, in plan px
  };
  std::vector<PlanMark> planMarks;

  // --- clocks --------------------------------------------------------------
  double loopT = 0;
  bool stepped = false;  // set by the fixed-timestep steppable
  uint64_t simSteps = 0;
  uint32_t rng = 0x9E3779B9u;
  bool pinned = false;  // the host is capturing for a diff

  /** THE FILM CLOCK'S LEFTOVER FRACTION, published by addFixed. The
   *  particles step in whole FILM frames — that is what [R83] counts
   *  lifetimes in, and a half-frame position is what the motion-blur
   *  streak already draws — so what this interpolates is the loop
   *  PHASE: the wavefront, Duff's light and the plan ring sweep
   *  smoothly at any draw rate off one Output. */
  ch::Output<float> simAlpha{0.0f};

  // --- ONE phase Output; bind() derives every consumer from it -------------
  ch::Output<float> loopU{0.0f};     // loop fraction, [0,1)
  ch::Output<float> liveFrac{0.0f};  // census bar, [0,1]

  /** THE GUESTS THAT DO NOT CHANGE, built once. A guest is retained —
   *  the pen keeps a composer per call site — so a description that says
   *  the same thing every frame is work nobody asked for. Only the
   *  census panel is rebuilt, because its live row carries numbers that
   *  tick, and the two stage guests hold pools whose lanes are rewritten
   *  in place rather than re-described. */
  Element headerEl, belowEl, aboveEl, genEl, rampEl, benchEl, prodEl;

  // --- measured ------------------------------------------------------------
  size_t liveCount = 0;
  double buildUs = 0;
  size_t vertCount = 0;

  // =========================================================================
  // RNG — [R83 §2.1] "Rand is a procedure returning a uniformly distributed
  // random number between -1.0 and +1.0".

  float rand01() { return sigil::core::noise::xorshiftUnitNext(rng); }
  float rand11() { return rand01() * 2.0f - 1.0f; }

  // =========================================================================
  // One second-level system

  void emitAt(std::vector<Particle>& into, const Site& s, uint16_t si,
              float speedScale, float sizeScale, float genScale) {
    Particle p;
    const float off = kRGen * genScale * rand11();
    p.pos = {s.p.fX + s.u.fX * off, s.p.fY + s.u.fY * off};
    // In-plane ejection angle. A slice through the paper's axisymmetric
    // inverted cone reads as uniform in psi, not area-uniform on a disc.
    const float psi = kPsiMax * rand01();
    const float sgn = rand01() < 0.5f ? -1.0f : 1.0f;
    const float c = std::cos(psi), sn = std::sin(psi) * sgn;
    const SkVector dir{s.n.fX * c + s.u.fX * sn, s.n.fY * c + s.u.fY * sn};
    const float speed =
        (kMeanSpeed + rand11() * kVarSpeed) * speedScale * s.vScale;
    p.vel = {dir.fX * speed, dir.fY * speed};
    p.size = std::max(0.7f, (kMeanSize + rand11() * kVarSize) * sizeScale);
    p.grow = kSizeRate * sizeScale;
    p.life = std::max(6.0f, kMeanLife + rand11() * kVarLife);
    p.r = kE0r * (1.0f + rand11() * kColorVar);
    p.g = kE0g * (1.0f + rand11() * kColorVar);
    p.b = kE0b * (1.0f + rand11() * kColorVar);
    p.site = si;
    into.push_back(p);
  }

  /** Per-frame integration + the three documented extinction rules. */
  void advance(std::vector<Particle>& v, const std::vector<Site>& ss,
               float gravity, bool killBelowSurface) {
    for (size_t i = 0; i < v.size();) {
      Particle& p = v[i];
      const Site& s = ss[p.site];
      p.vel.fX -= s.n.fX * gravity;  // acceleration -> parabolic arcs
      p.vel.fY -= s.n.fY * gravity;
      p.pos.fX += p.vel.fX;
      p.pos.fY += p.vel.fY;
      p.age += 1.0f;
      p.size += p.grow;  // size change at a rate global to the system
      // colour change: LINEAR RATES, red slowest — [R83 §2.3, §3]
      p.r = std::max(0.0f, p.r - kDecR);
      p.g = std::max(0.0f, p.g - kDecG);
      p.b = std::max(0.0f, p.b - kDecB);
      const bool dead =
          p.age >= p.life || (p.r + p.g + p.b) < kMinIntensity ||
          (killBelowSurface && p.pos.fY > limbY(p.pos.fX) + 1.5f) ||
          (!killBelowSurface && p.pos.fY > s.p.fY + 2.0f);
      if (dead) {
        v[i] = v.back();
        v.pop_back();
      } else {
        ++i;
      }
    }
  }

  void stepSim() {
    loopT += kSimStep;
    if (loopT >= kLoopSeconds) {  // the sequence had a cut; a hard reset reads
      loopT -= kLoopSeconds;      // as one
      parts.clear();
    }
    // Generation. THE WALL IS A STAGGER: site i ignites at
    // t_i = (x_i - X0) / SPREAD — distance from the point of impact.
    for (uint16_t i = 0; i < (uint16_t)sites.size(); ++i) {
      const float f = (float)(loopT - sites[i].t0) * (float)kSimHz;
      if (f < 0.0f || f >= kGenWindow) continue;
      const float rate = kInitialMeanParts + kDeltaMeanParts * f;
      const int n =
          (int)std::lround(std::max(0.0f, rate + rand11() * kVarParts));
      for (int k = 0; k < n; ++k) emitAt(parts, sites[i], i, 1.0f, 1.0f, 1.0f);
    }
    advance(parts, sites, kGravity, true);

    // The A/B bench: one steady-state explosion on a fixed recipe, births
    // stopping at the pool's kAbCount slots.
    const int nb = (int)std::lround(std::max(0.0f, 25.0f + rand11() * 5.0f));
    for (int k = 0; k < nb && abParts.size() < kAbCount; ++k)
      emitAt(abParts, abSite(), 0, 0.55f, 0.72f, 0.35f);
    advance(abParts, abSites(), 1.06f, false);

    liveCount = parts.size();
    ++simSteps;
  }

  // The bench's single system, in the local px of its 130x52 cell — the
  // emitter sits 2 px above the cell's bottom edge, so the whole arc stays
  // inside the clip and the below-emitter cull (2 px under the emitter, in
  // advance()) lands exactly on the cell's edge.
  static const std::vector<Site>& abSites() {
    static const std::vector<Site> s = {
        Site{{65.0f, 50.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, 0.0f}};
    return s;
  }
  static const Site& abSite() { return abSites()[0]; }

  // =========================================================================
  // The renderer that defines the look.
  //
  // ONE additive pass over every living particle, no depth sort, no
  // occlusion. Each streak is a quad from pos(f + 1/2) to pos(f) — a
  // 1/50 s shutter at 24 fps is half the inter-frame motion — six
  // vertices wide so the cross-section falls off (drawVertices does not
  // antialias its own edges, so the falloff lives in the vertex colours).
  // ADD is the whole colour model: light ADDS and the buffer CLAMPS.

  void buildStreaks(const std::vector<Particle>& v,
                    std::vector<sk_sp<SkVertices>>& out) {
    out.clear();
    if (v.empty()) return;
    // SkVertices indices are uint16, so one list holds 65,535 vertices ->
    // 8,191 streaks at eight each. Chunk the way drawSpriteAtlas does.
    constexpr size_t kChunk = 8000;
    static thread_local std::vector<SkPoint> pos;
    static thread_local std::vector<SkColor> col;
    static thread_local std::vector<uint16_t> idx;
    // Six triangles: a FLAT core with an antialiased shoulder either side,
    // which is what an antialiased line actually is. (A three-band strip
    // whose core collapses to a single line reads dimmer for the same
    // coverage, because the flat interior is where the light lives.)
    static constexpr uint16_t kTri[18] = {0, 1, 2, 0, 2, 3, 3, 2, 4,
                                          3, 4, 5, 5, 4, 6, 5, 6, 7};
    for (size_t base = 0; base < v.size(); base += kChunk) {
      const size_t cnt = std::min(kChunk, v.size() - base);
      pos.clear();
      col.clear();
      idx.clear();
      pos.reserve(cnt * 8);
      col.reserve(cnt * 8);
      idx.reserve(cnt * 18);
      for (size_t i = 0; i < cnt; ++i) {
        const Particle& p = v[base + i];
        const float speed = p.vel.length();
        const SkVector d = speed > 1e-4f
                               ? SkVector{p.vel.fX / speed, p.vel.fY / speed}
                               : SkVector{0.0f, -1.0f};
        // streaked spherical: length 0.5*|v| but never less than half the
        // diameter, width `size`. So the quad is elongated at ejection and
        // ends up wider than it is long once the particle slows at apogee.
        // The instancing path expresses that too, through Pool::sizes() —
        // the sidebar's bench shows it — and the field is built by hand
        // here for the other reason: Reeves' own renderer was "merely
        // antialiased lines", and one SkVertices list per 8,000 streaks is
        // that, with a per-vertex alpha ramp no atlas cell carries.
        const float len = std::max(p.size * 0.5f, speed * 0.5f);
        const SkPoint head = p.pos;
        const SkPoint tail{p.pos.fX - d.fX * len, p.pos.fY - d.fY * len};
        const float hw = p.size * 0.5f;
        const SkVector nn{-d.fY * hw, d.fX * hw};
        const SkVector ni{nn.fX * 0.42f, nn.fY * 0.42f};
        const SkColor c = SkColorSetARGB(
            255, (uint8_t)std::lround(std::min(1.0f, p.r) * 255.0f),
            (uint8_t)std::lround(std::min(1.0f, p.g) * 255.0f),
            (uint8_t)std::lround(std::min(1.0f, p.b) * 255.0f));
        constexpr SkColor kEdge = 0x00000000;
        const uint16_t v0 = (uint16_t)pos.size();
        pos.push_back({tail.fX + nn.fX, tail.fY + nn.fY});
        col.push_back(kEdge);
        pos.push_back({head.fX + nn.fX, head.fY + nn.fY});
        col.push_back(kEdge);
        pos.push_back({head.fX + ni.fX, head.fY + ni.fY});
        col.push_back(c);
        pos.push_back({tail.fX + ni.fX, tail.fY + ni.fY});
        col.push_back(c);
        pos.push_back({head.fX - ni.fX, head.fY - ni.fY});
        col.push_back(c);
        pos.push_back({tail.fX - ni.fX, tail.fY - ni.fY});
        col.push_back(c);
        pos.push_back({head.fX - nn.fX, head.fY - nn.fY});
        col.push_back(kEdge);
        pos.push_back({tail.fX - nn.fX, tail.fY - nn.fY});
        col.push_back(kEdge);
        for (uint16_t tri : kTri) idx.push_back((uint16_t)(v0 + tri));
      }
      out.push_back(SkVertices::MakeCopy(
          SkVertices::kTriangles_VertexMode, (int)pos.size(), pos.data(),
          nullptr, col.data(), (int)idx.size(), idx.data()));
    }
  }

  /** THE ADDITIVE PASS, IN THE PEN'S OWN WORDS. A streak list is an
   *  `SkVertices` and `pen.vertices` takes one: it lands in the pen's
   *  space and in the pen's order, wearing the pen's fill under the
   *  `blendMode(ADD)` the caller's push stands at, so light adds where
   *  every other verb in this frame adds too. The fill carries no colour
   *  of its own — the vertices carry theirs, and a plain fill is where
   *  the corner colours govern.
   *
   *  THE FADE AT EITHER END OF THE LOOP IS THE FILL'S ALPHA. A layer would
   *  gather the streaks additively and then composite the gathered picture
   *  over the stage src-over, which is not what a dimming light does;
   *  scaling the source keeps the pass additive from the first streak all
   *  the way to the canvas. */
  static void paintField(Pen& pen, const std::vector<sk_sp<SkVertices>>& chunks,
                         float alpha) {
    if (chunks.empty() || alpha <= 0.001f) return;
    pen.push();
    pen.fill(255, 255, 255, alpha * 255.0f);
    for (const sk_sp<SkVertices>& v : chunks) pen.vertices(v);
    pen.pop();
  }

  /** The bench pool: ONE pool, read by two instances() leaves at two
   *  blend modes. Same data, two renderers — that is what makes it
   *  evidence rather than an assertion. */
  /** THE BENCH POOL, WITH THE NON-UNIFORM LANE. Reeves' `shape: streaked
   *  spherical` is a quad 0.5*|v| long by `size` wide: elongated at
   *  ejection, wider than it is long once the particle slows at apogee.
   *  An `SkRSXform` carries a rotation and ONE scale, so a pool asking for
   *  two takes `sizes()` — an opt-in (x, y) multiplier on top of
   *  `scales()` — and the stamp goes down the wider path. The cell is one
   *  baked aspect and the lane stretches it per instance, which is the
   *  whole of expressing a streak whose length tracks speed while its
   *  width does not. */
  void writeBenchPool() {
    abPool->resize(kAbCount);
    auto p = abPool->positions();
    auto r = abPool->rotations();
    auto s = abPool->scales();
    auto sz = abPool->sizes();
    auto tn = abPool->tints();
    auto fr = abPool->frames();
    for (size_t i = 0; i < kAbCount; ++i) {
      if (i < abParts.size()) {
        const Particle& q = abParts[i];
        const float speed = q.vel.length();
        // The same two numbers the field's own quads are built from, so
        // the bench is a picture of the field's shape rather than of a
        // second one.
        const float len = std::max(q.size * 0.5f, speed * 0.5f);
        p[i] = q.pos;
        r[i] = std::atan2(q.vel.fY, q.vel.fX);
        s[i] = 1.0f;
        // The cell's own ink is 4.4 x 2.3, and the lane is a multiplier on
        // it: the long axis takes the streak's length, the short one its
        // diameter.
        sz[i] = {std::max(0.12f, len / 4.4f), std::max(0.12f, q.size / 2.3f)};
        tn[i] = {std::min(1.0f, q.r), std::min(1.0f, q.g), std::min(1.0f, q.b),
                 1.0f};
        fr[i] = 0;
      } else {
        p[i] = {-999, -999};
        sz[i] = {0.0f, 0.0f};
        tn[i] = {0, 0, 0, 0};
      }
    }
    abPool->commit();
  }

  /** Fig. 2's 149 ring marks: per-instance frame + tint rewritten every
   *  tick as the wavefront passes. The textbook Mode::Live case. */
  void writePlanPool() {
    const float front = (float)(loopT / kFrontCrossSeconds) * 124.0f;
    auto tn = planPool->tints();
    auto fr = planPool->frames();
    auto sc = planPool->scales();
    for (size_t i = 0; i < planMarks.size(); ++i) {
      const float d = planMarks[i].dist;
      const float since = front - d;
      if (since < 0.0f) {
        fr[i] = 0;
        tn[i] = {1, 1, 1, 0.55f};
        sc[i] = 1.0f;
      } else {
        const float k = std::clamp(1.0f - since / 96.0f, 0.0f, 1.0f);
        fr[i] = 1;
        const SkColor4f hot = overlap(12);
        tn[i] = {hot.fR, hot.fG, hot.fB, 0.35f + 0.65f * k};
        sc[i] = 1.0f + 0.5f * k;
      }
    }
    planPool->commit();
  }

  // =========================================================================
  // The two control pools, laid out once

  void seedStars() {
    starAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // Five magnitude cells: a soft disc each, 10/7/5/4/3 px logical.
    const float sizes[5] = {7.0f, 5.4f, 4.2f, 3.2f, 2.4f};
    for (float s : sizes)
      starAtlas->cell(box().width(s).height(s).fill(
                          Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                            {{0.0f, {1, 1, 1, 1}},
                                             {0.22f, {1, 1, 1, 0.78f}},
                                             {0.58f, {1, 1, 1, 0.14f}},
                                             {1.0f, {1, 1, 1, 0.0f}}})),
                      {s, s});
    // N(m) ~ 10^(0.6m): 1 / 5 / 19 / 76 / 319 = 420 stars.
    const int bin[5] = {1, 5, 19, 76, 319};
    // B-V ramp [S82]: Carpenter "deduced the colors of the individual
    // stars" from the Yale Bright Star Catalogue.
    const SkColor4f bv[6] = {hex(0xAEC6FF), hex(0xD6E2FF), hex(0xFFFFFF),
                             hex(0xFFE9B8), hex(0xFFC48A), hex(0xFF9E6E)};
    const int bvWeight[6] = {6, 12, 20, 26, 24, 12};
    starPool = std::make_shared<instancing::Pool>();
    rng = 0x5EED1982u;
    for (int m = 0; m < 5; ++m) {
      for (int k = 0; k < bin[m]; ++k) {
        float x = 0, y = 0;
        for (int guard = 0; guard < 24; ++guard) {
          x = rand01() * kStageW;
          y = rand01() * (kStageH * 0.86f);
          if (y < limbY(x) - 8.0f) break;
        }
        int pick = (int)(rand01() * 100.0f), c = 0, acc = 0;
        for (; c < 6; ++c) {
          acc += bvWeight[c];
          if (pick < acc) break;
        }
        SkColor4f col = bv[std::min(c, 5)];
        col.fA = 0.30f + 0.55f * rand01();
        starPool->add({x, y}, m, 0.0f, 0.60f + 0.45f * rand01(), col);
      }
    }
  }

  void seedPlan() {
    planAtlas = std::make_shared<instancing::Atlas>(3.0f);
    // cell 0: unlit open ring; cell 1: lit dot.
    planAtlas->cell(box()
                        .width(3.2f)
                        .height(3.2f)
                        .shape(shapes::circle())
                        .stroke(stroke(0.7f, Fill::color(hex(0x2E3A46)),
                                       PathFormat::Align::Inner)),
                    {4, 4});
    planAtlas->cell(box().width(4.0f).height(4.0f).fill(
                        Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                          {{0.0f, {1, 1, 1, 1}},
                                           {0.45f, {1, 1, 1, 0.8f}},
                                           {1.0f, {1, 1, 1, 0}}})),
                    {4, 4});
    // Seven rings from the impact point; marks per ring =
    // round(0.055 * 2*pi*r) — [R83 §3]'s circumference x density rule.
    // Random angular placement, also documented.
    planPool = std::make_shared<instancing::Pool>();
    planMarks.clear();
    const SkPoint impact{34.0f, 106.0f};  // disc-local (disc centre 92,92)
    rng = 0x51A7C0DEu;
    const float radii[7] = {14, 30, 46, 62, 78, 94, 110};
    for (float r : radii) {
      const int n = (int)std::lround(0.055f * 2.0f * 3.14159265f * r);
      for (int i = 0; i < n; ++i) {
        const float a = rand01() * 6.2831853f;
        const SkPoint p{impact.fX + std::cos(a) * r,
                        impact.fY + std::sin(a) * r};
        planMarks.push_back({p, r});
        planPool->add(p, 0, 0.0f, 1.0f, {1, 1, 1, 0.55f});
      }
    }
  }

  void seedBench() {
    abAtlas = std::make_shared<instancing::Atlas>(4.0f);
    // ONE baked aspect. The paper's shape goes from elongated at ejection to
    // stubby at apogee; an atlas cell is one size and a Pool scale is one
    // float, so this cell is the compromise the middle two panels show.
    abAtlas->cell(box().width(4.4f).height(2.3f).corners({1.0f}).fill(
                      Paint::radialUnit({0.5f, 0.5f}, 1.05f,
                                        {{0.0f, {1, 1, 1, 1}},
                                         {0.42f, {1, 1, 1, 0.9f}},
                                         {1.0f, {1, 1, 1, 0}}})),
                  {4.8f, 2.6f});
    abPool = std::make_shared<instancing::Pool>();
    abPool->resize(kAbCount);
  }

  // =========================================================================
  // Stage — the retained furniture, under the field and over it

  Element starField() {
    return box()
        .inset(0)
        .opacity(
            animate(from(0.0f).to(1.0f), {.duration = 700ms, .delay = 340ms}))
        .child(instancing::instances(
            starAtlas, starPool, instancing::Mode::Data, SkBlendMode::kPlus));
  }

  struct DipStar {
    const char* name;
    float u, v, mag;
  };

  Element dipper() {
    // Real relative geometry, framed into x[430,860] y[40,250].
    static const DipStar kStars[8] = {
        {"ALKAID", 0.05f, 0.10f, 1.85f}, {"MIZAR", 0.24f, 0.26f, 2.23f},
        {"ALIOTH", 0.42f, 0.30f, 1.77f}, {"MEGREZ", 0.58f, 0.36f, 3.31f},
        {"PHECDA", 0.62f, 0.52f, 2.44f}, {"MERAK", 0.86f, 0.44f, 2.37f},
        {"DUBHE", 0.86f, 0.18f, 1.79f},  {"SOL", 0.40f, 0.14f, 2.63f}};
    constexpr float bx = 430, by = 40, bw = 430, bh = 210;
    auto at = [&](int i) {
      return SkPoint{bx + kStars[i].u * bw, by + kStars[i].v * bh};
    };

    Element g = box().inset(0);

    // the asterism, drawn on
    g.child(
        box()
            .inset(0)
            .shape([&, bx, by, bw, bh](SkSize) {
              SkPathBuilder b;
              auto P = [&](int i) {
                return SkPoint{bx + kStars[i].u * bw, by + kStars[i].v * bh};
              };
              b.moveTo(P(0));
              b.lineTo(P(1));
              b.lineTo(P(2));
              b.lineTo(P(3));
              b.lineTo(P(4));
              b.lineTo(P(5));
              b.lineTo(P(6));
              b.lineTo(P(3));
              return b.detach();
            })
            .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                        {.duration = 620ms, .delay = 1300ms})),
                    stroke(1.0f, Fill::color(hex(0x4FB8D8, 0.35f))))
            .key("asterism"));

    for (int i = 0; i < 8; ++i) {
      const SkPoint p = at(i);
      const float rad = std::max(1.4f, 4.6f - 0.85f * kStars[i].mag);
      const bool sol = i == 7;
      g.child(
          kit::disc(p, rad * 2.0f)
              .fill(Paint::radialUnit(
                  {0.5f, 0.5f}, 0.707f,
                  {{0.0f, sol ? hex(0xFFFFFF) : hex(0xEFF3FF)},
                   {0.22f, sol ? hex(0xFFF4D8, 0.9f) : hex(0xD9E4FF, 0.85f)},
                   {1.0f, {1, 1, 1, 0}}}))
              .blend(SkBlendMode::kPlus)
              .opacity(animate(from(0.0f).to(1.0f),
                               {.duration = 500ms, .delay = 1200ms})));
      g.child(t(kStars[i].name,
                mono(7.0f, sol ? kCyan : hex(0x9FB0CC, 0.85f), 1.1f))
                  .left(p.fX + rad + 5.0f)
                  .top(p.fY - 5.0f)
                  .opacity(animate(from(0.0f).to(1.0f),
                                   {.duration = 400ms, .delay = 1500ms})));
    }
    // Smith's joke, verified in the header block.
    const SkPoint s = at(7);
    g.child(box().left(s.fX + 4).top(s.fY + 6).width(1).height(16).fill(
        hex(0x4FB8D8, 0.5f)));
    g.child(box()
                .left(s.fX + 9)
                .top(s.fY + 12)
                .column()
                .gap(1)
                .opacity(animate(from(0.0f).to(1.0f),
                                 {.duration = 400ms, .delay = 1600ms}))
                .child(t("m = 2.63 FROM \xce\xb5 INDI (3.64 pc)",
                         mono(7.0f, kCyan, 0.9f)))
                .child(t("\"OUR SUN WOULD APPEAR AS AN EXTRA STAR\"",
                         mono(7.0f, hex(0x4FB8D8, 0.7f), 0.9f))));
    return g;
  }

  Element regolith() {
    // A generated surface, plus the ONE hand-added light in the shot
    // (Tom Duff's), riding the wavefront.
    Paint ground = Paint::blend(
        {{Paint::radialUnit({0.5f, 0.723f}, 0.50f,
                            {{0.0f, hex(0x3B3933)},
                             {0.42f, hex(0x232119)},
                             {1.0f, hex(0x0A0A0C)}}),
          SkBlendMode::kSrc},
         {Paint::recipe(field::grain(0.022f, 4, 7.0f, 0.5f, 1.0f)),
          SkBlendMode::kSoftLight},
         {Pattern(patterns::speckle(
                      170, 17, 0.9f, 3.4f,
                      {toColor(hex(0x6A655B)), toColor(hex(0x171512))}))
              .material(),
          SkBlendMode::kOverlay}});

    return box()
        .inset(0)
        .shape(limbOutline())
        .clip(true)
        .fill(std::move(ground))
        .opacity(
            animate(from(0.0f).to(1.0f), {.duration = 520ms, .delay = 420ms}))
        .translateY(animate(
            from(12.0f).to(0.0f),
            {.duration = 520ms, .ease = &ch::easeOutCubic, .delay = 420ms}))
        // Duff's local light. ONE Output (loopU) shaped into px.
        .child(kit::disc(SkPoint{0, 0}, 132)
                   .fill(Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                           {{0.0f, hex(0xFF8A3A, 0.62f)},
                                            {0.38f, hex(0xC24E14, 0.24f)},
                                            {1.0f, hex(0xFF8A3A, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .translateX(bind(&loopU).scale(1680.0f).offset(-80.0f))
                   .translateY(limbY(444.0f) + 26.0f)
                   .opacity(bind(&loopU).map([](float v) {
                     const float t = v * 10.0f;
                     return std::clamp(t / 0.4f, 0.0f, 1.0f) *
                            std::clamp((9.6f - t) / 0.8f, 0.0f, 1.0f);
                   })));
  }

  Element shockwave() {
    // The impact flash and Carpenter/Cole's shockwave, at the impact point
    // (off-frame left). Documented as elements; timing is reconstruction.
    const SkPoint impact{kX0, limbY(kX0 < 0 ? 0.0f : kX0) + 8.0f};
    Element g = box().inset(0);
    g.child(kit::disc(impact, 170)
                .fill(Paint::radialUnit({0.5f, 0.5f}, 0.707f,
                                        {{0.0f, {1, 1, 1, 0.95f}},
                                         {0.25f, hex(0xFFE7B0, 0.6f)},
                                         {1.0f, hex(0xFF7A20, 0.0f)}}))
                .blend(SkBlendMode::kPlus)
                .opacity(bind(&loopU).map([](float v) {
                  const float t = v * 10.0f;
                  if (t < 0.06f) return t / 0.06f;
                  if (t < 0.45f) {
                    const float k = 1.0f - (t - 0.06f) / 0.39f;
                    return k * k;
                  }
                  return 0.0f;
                })));
    g.child(kit::disc(impact, 520)
                .shape(shapes::circle())
                .stroke(stroke(2.0f, Fill::color(hex(0xFFB070, 0.85f))))
                .blend(SkBlendMode::kPlus)
                .scale(bind(&loopU)
                           .map([](float v) {
                             return choreograph::easeOutCubic(
                                 std::clamp(v * 10.0f / 1.1f, 0.0f, 1.0f));
                           })
                           .clamp(0.001f, 1.0f))
                .opacity(bind(&loopU).map([](float v) {
                  const float t = v * 10.0f;
                  if (t > 1.1f) return 0.0f;
                  const float k = 1.0f - t / 1.1f;
                  return k * k;
                })));
    return g;
  }

  /** Everything under the field: the sky, the stars, the Dipper, the
   *  regolith and the shockwave, in one guest the size of the stage. */
  Element stageBelow() {
    return stack()
        .width(kStageW)
        .height(kStageH)
        .clip()
        .fill(Paint::linearUnit({0.5f, 0.0f}, {0.5f, 0.85f},
                                {{0.0f, hex(0x03040A)},
                                 {0.55f, hex(0x05060D)},
                                 {1.0f, hex(0x0A0B13)}}))
        .child(starField().zIndex(1))
        .child(dipper().zIndex(2))
        .child(regolith().zIndex(3))
        .child(shockwave().zIndex(4));
  }

  Element planInset() {
    // Fig. 2: the distribution of second-level systems on the planet's
    // surface, live.
    Element inner =
        box()
            .left(12)
            .top(12)
            .width(184)
            .height(184)
            .shape(shapes::circle())
            .clip(true)
            .stroke(stroke(1.0f, Fill::color(hex(0x4FB8D8, 0.55f)),
                           PathFormat::Align::Inner))
            // the expanding wavefront ring — same Output, unit scale
            .child(kit::disc(SkPoint{34, 106}, 124)
                       .shape(shapes::circle())
                       .stroke(stroke(1.0f, Fill::color(hex(0x4FB8D8, 0.75f))))
                       .scale(bind(&loopU)
                                  .scale(10.0f / (float)kFrontCrossSeconds)
                                  .clamp(0.004f, 1.0f)))
            .child(box().inset(0).child(instancing::instances(
                planAtlas, planPool, instancing::Mode::Live,
                SkBlendMode::kPlus)));

    return box()
        .left(24)
        .top(24)
        .width(208)
        .height(208)
        .corners({6})
        .fill(hex(0x0B0D14, 0.86f))
        .stroke(stroke(1.5f, Fill::color(kKeyline), PathFormat::Align::Inner))
        .opacity(
            animate(from(0.0f).to(1.0f), {.duration = 340ms, .delay = 900ms}))
        .scale(animate(from(0.94f).to(1.0f), {.duration = 340ms,
                                              .ease = ease::outBack(1.70158f),
                                              .delay = 900ms}))
        .child(std::move(inner))
        // the impact point itself
        .child(box()
                   .left(12 + 34 - 2)
                   .top(12 + 106 - 2)
                   .width(4)
                   .height(4)
                   .shape(shapes::circle())
                   .fill(hex(0xFFFFFF, 0.95f)))
        // rim caption on a curved baseline
        .child(t("IMPACT \xc2\xb7 KETI BANDAR \xc2\xb7 \xce\xb5 INDI",
                 mono(8.0f, kCyan, 1.4f))
                   .left(12)
                   .top(12)
                   .width(184)
                   .height(184)
                   .onPath(TextPath{.path = shapes::circle(),
                                    .at = 0.75f,
                                    .align = TextPath::Align::Center,
                                    .offset = 8.0f}));
  }

  /** Everything over the field: Fig. 2's live plan inset and its caption.
   *  Both are retained — a pool written every tick and a line of type on
   *  a curved baseline are a described tree's business. */
  Element stageAbove() {
    return stack()
        .width(kStageW)
        .height(kStageH)
        .clip()
        .child(planInset())
        .child(t("FIG. 2 \xe2\x80\x94 DISTRIBUTION OF PARTICLE SYSTEMS ON THE "
                 "PLANET'S SURFACE",
                 mono(8.5f, kSteel, 0.6f))
                   .left(24)
                   .top(236)
                   .width(300)
                   .opacity(animate(from(0.0f).to(1.0f),
                                    {.duration = 300ms, .delay = 1050ms})));
  }

  // =========================================================================
  // The motion-blur callout — the one place the pen draws a streak in its
  // own words.

  /** [R83 §3]'s motion-blur construction, magnified 3x, in pen verbs.
   *  ONE shape: `beginShape(TRIANGLE_STRIP)` with a `fill()` between the
   *  vertex pairs, so the corners either side of that call carry it and
   *  the cross-section's falloff is interpolated across the mesh. The
   *  edge colour is the streak's own colour at zero alpha rather than
   *  transparent BLACK, because the corners are interpolated before they
   *  are composited and a black edge would darken the ramp's middle. */
  void blurCallout(Pen& pen, float x0, float y0, float w, float h, float a) {
    // the panel
    pen.noStroke();
    pen.fill(hex(0x0B0D14, 0.86f * a));
    pen.rect(x0, y0, w, h, 6);
    pen.noFill();
    pen.stroke(hex(0x242A36, a));
    pen.strokeWeight(1.5f);
    pen.rect(x0 + 0.75f, y0 + 0.75f, w - 1.5f, h - 1.5f, 6);
    pen.noStroke();

    float cy = y0 + 11;
    pen.textFont(weave::Type{.face = uiFace(), .size = 8.5f, .track = 1.7f});
    pen.fill(hex(0x4FB8D8, a));
    pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
    pen.text("MOTION BLUR \xe2\x80\x94 REEVES 1983 \xc2\xa7"
             "3",
             x0 + 11, cy);
    cy += 14;

    // the streak itself: a 3x-magnified quad, one shape, colour ramped
    // across the cross-section by the fills between its vertices
    const float sx = x0 + 11, sy = cy + h * 0.30f;
    const float x1 = sx + 5, x2 = sx + 141;
    const SkColor4f hot = overlap(9);
    const SkColor4f edge = {hot.fR, hot.fG, hot.fB, 0.0f};
    pen.noStroke();
    pen.beginShape(sigil::draw::TRIANGLE_STRIP);
    pen.fill(edge);
    pen.vertex(x1, sy - 5);
    pen.vertex(x2, sy - 5);
    pen.fill(hex(hot.toSkColor() & 0xFFFFFFu, a));
    pen.vertex(x1, sy);
    pen.vertex(x2, sy);
    pen.fill(edge);
    pen.vertex(x1, sy + 5);
    pen.vertex(x2, sy + 5);
    pen.endShape();

    // the two sample positions
    pen.fill(hex(0xFFFFFF, 0.95f * a));
    pen.circle(x2, sy, 5.2f);
    pen.fill(hex(0xFFFFFF, 0.45f * a));
    pen.circle(x1, sy, 4.0f);
    // cyan dimension bracket
    pen.noFill();
    pen.stroke(hex(0x4FB8D8, 0.9f * a));
    pen.strokeWeight(1.0f);
    const float by = sy + 13;
    pen.line(x1, by - 3, x1, by + 3);
    pen.line(x2, by - 3, x2, by + 3);
    pen.line(x1, by, x2, by);
    pen.noStroke();
    // labels
    penMono(pen, 7.0f, hex(0x9FB0CC, a));
    pen.text("pos(f + 1/2)", x1 - 8, sy - 18);
    pen.text("pos(f)", x2 - 14, sy - 18);
    penMono(pen, 7.0f, hex(0x4FB8D8, a));
    pen.text("0.5 \xc2\xb7 |v|", (x1 + x2) * 0.5f - 16, by + 4);

    cy = y0 + h - 46;
    penMono(pen, 7.5f, fadeTo(kBone, a), 0.4f);
    pen.text("SHUTTER 1/50 s @ 24 fps \xe2\x89\x88 \xc2\xbd FRAME OF MOTION",
             x0 + 11, cy);
    cy += 11;
    penMono(pen, 7.5f, fadeTo(kSteel, a), 0.4f);
    pen.text("STREAK = pos(f) \xe2\x86\x92 pos(f+\xc2\xbd), ANTIALIASED, "
             "ADDITIVE",
             x0 + 11, cy);
    cy += 11;
    penMono(pen, 6.5f, fadeTo(kSteelDim, a), 0.2f);
    pen.text("FN.4: \"A PARTICLE'S TRAJECTORY IS ACTUALLY PARABOLIC, BUT\n"
             "THE STRAIGHT-LINE APPROXIMATION HAS SO FAR PROVED SUFFICIENT\"",
             x0 + 11, cy);
  }

  static SkColor4f fadeTo(SkColor4f c, float a) {
    return {c.fR, c.fG, c.fB, c.fA * a};
  }

  // =========================================================================
  // Sidebar

  Element eqn(const char* s) {
    return t(s, mono(11.0f, kBone, 0.1f)).height(16).shrink(0);
  }

  Element generationPanel() {
    return panel(kPanelH[0], 1)
        .gap(3)
        .child(panelHead("GENERATION LAW"))
        .child(eqn("NParts_f    = MeanParts_f + Rand() \xc3\x97 VarParts_f"))
        .child(eqn("MeanParts_f = InitialMeanParts + \xce\x94Mean \xc3\x97 "
                   "(f \xe2\x88\x92 f\xe2\x82\x80)"))
        .child(eqn("InitialSpeed = MeanSpeed + Rand() \xc3\x97 VarSpeed"))
        .child(box().grow(1))
        .child(t("Rand() \xe2\x86\x92 UNIFORM [\xe2\x88\x92"
                 "1.0, +1.0] "
                 "\xe2\x80\x94 REEVES 1983 \xc2\xa7"
                 "2.1\xe2\x80\x93"
                 "2.2",
                 mono(7.5f, kSteelDim, 0.4f))
                   .shrink(0));
  }

  Element censusCell(const char* s, float w, weave::TextStyle st) {
    return t(s, std::move(st)).width(w).shrink(0);
  }

  Element censusBar(float frac, SkColor4f c, const char* key) {
    Element fill =
        box()
            .left(0)
            .top(0)
            .width(96)
            .height(7)
            .fill(c)
            .transformOrigin(0.0f, 0.5f)
            .scaleX(animate(from(0.0f).to(frac), {.duration = 420ms,
                                                  .ease = ease::outBack(1.2f),
                                                  .delay = 1200ms}));
    if (key) fill.scaleX(bind(&liveFrac).clamp(0.02f, 1.0f)).key(key);
    return box()
        .width(96)
        .height(7)
        .shrink(0)
        .fill(hex(0x171B24))
        .child(std::move(fill));
  }

  Element censusRow(const char* fig, const char* sys, const char* particles,
                    const char* per, float frac, bool live) {
    const SkColor4f c = live ? kCyan : kBone;
    const SkColor4f cd = live ? kCyan : kSteel;
    return box()
        .row()
        .height(14)
        .shrink(0)
        .alignItems(Align::Center)
        .child(censusCell(fig, 46, mono(9.5f, cd, 0.4f)))
        .child(censusCell(sys, 62, mono(9.5f, c, 0.4f)))
        .child(censusCell(particles, 108, monoB(9.5f, c, 0.4f)))
        .child(censusCell(per, 76, mono(9.5f, cd, 0.4f)))
        .child(censusBar(frac, live ? kCyan : hex(0x6D5A3F),
                         live ? "livebar" : nullptr));
  }

  /** The live census row. The numbers tick, so the row is re-described
   *  every frame and reconciled against what the guest's own composer
   *  already holds — which is what a slot used to buy. */
  Element liveRow() {
    char parts_[32], per_[24], sys_[16];
    std::snprintf(parts_, sizeof parts_, "%zu,%03zu", liveCount / 1000,
                  liveCount % 1000);
    std::snprintf(per_, sizeof per_, "%d",
                  (int)std::lround((double)liveCount / (33.0 * kDepth)));
    std::snprintf(sys_, sizeof sys_,
                  "33\xc3\x97"
                  "%d",
                  kDepth);
    return box()
        .row()
        .height(14)
        .shrink(0)
        .alignItems(Align::Center)
        .child(censusCell("THIS", 46, monoB(9.5f, kCyan, 0.4f)))
        .child(censusCell(sys_, 62, mono(9.5f, kCyan, 0.4f)))
        .child(censusCell(parts_, 108, monoB(9.5f, kCyan, 0.4f)))
        .child(censusCell(per_, 76, mono(9.5f, kCyan, 0.4f)))
        .child(censusBar(0.0f, kCyan, "livebar"));
  }

  Element censusPanel() {
    return panel(kPanelH[1], 2)
        .gap(4)
        .child(panelHead("PARTICLE CENSUS \xe2\x80\x94 REEVES 1983 \xc2\xa7"
                         "3"))
        .child(
            box()
                .row()
                .height(11)
                .shrink(0)
                .child(censusCell("FIG", 46, mono(7.5f, kSteelDim, 0.9f)))
                .child(censusCell("SYSTEMS", 62, mono(7.5f, kSteelDim, 0.9f)))
                .child(
                    censusCell("PARTICLES", 108, mono(7.5f, kSteelDim, 0.9f)))
                .child(censusCell("PER SYS", 76, mono(7.5f, kSteelDim, 0.9f)))
                .child(
                    censusCell("LOG SCALE", 96, mono(7.5f, kSteelDim, 0.9f))))
        .child(
            box()
                .column()
                .gap(3)
                .shrink(0)
                .staggerChildren(70ms)
                .child(censusRow("4", "~21", "25,000", "1,190*", 0.273f, false))
                .child(censusRow("5", "~200", "75,000", "375", 0.491f, false))
                .child(censusRow("6", "~200", "85,000", "425", 0.514f, false))
                .child(censusRow("7\xe2\x80\x93"
                                 "8",
                                 "~400", ">750,000", ">1,875", 0.945f, false))
                .child(liveRow()))
        .child(box().grow(1))
        .child(t("* FIG. 4 IS \"ONE VERY LARGE PARTICLE SYSTEM AND ABOUT 20 "
                 "SMALLER ONES\" \xe2\x80\x94 THAT MEAN IS MEANINGLESS.",
                 mono(6.5f, kSteelDim, 0.2f))
                   .shrink(0))
        .child(t("NO PARAMETER VALUE IS PUBLISHED ANYWHERE. EVERY CONSTANT "
                 "HERE IS ARITHMETIC ON TWO PUBLISHED INTEGERS: 85,000 "
                 "\xc3\xb7 200 = 425 ALIVE PER EXPLOSION (FIG. 6), AND "
                 "POPULATION = BIRTH RATE \xc3\x97 LIFETIME \xe2\x80\x94 "
                 "PICK MeanLife = 34 f AND THE RATE FOLLOWS. SYSTEMS IGNITE "
                 "EVERY 18.5/168 = 0.110 s: 20 GENERATING + 13 BURNING OUT "
                 "= 24 FULLY-LIT EQUIVALENTS. A LIMB VIEW STACKS THE RING "
                 "IN DEPTH (FIG. 6 IS ~200 SYSTEMS; THIS SLICE ANCHORS 53 "
                 "COLUMNS), SO EACH COLUMN CARRIES 3: 72 \xc3\x97 425 = "
                 "30,600 PREDICTED. THE \"THIS\" ROW IS MEASURED.",
                 mono(6.5f, kSteel, 0.2f))
                   .shrink(0));
  }

  Element rampPanel() {
    std::vector<Element> swatches, labels;
    swatches.reserve(14);
    labels.reserve(14);
    for (int n : kRampN) {
      swatches.push_back(box()
                             .width(28)
                             .height(26)
                             .shrink(0)
                             .fill(Paint::solid(overlap(n)))
                             .transformOrigin(0.5f, 1.0f)
                             .scaleY(animate(from(0.0f).to(1.0f),
                                             {.duration = 220ms,
                                              .ease = ease::outBack(1.70158f),
                                              .delay = 1500ms})));
      const bool key = n == 5 || n == 20 || n == 111;
      labels.push_back(t(std::to_string(n).c_str(),
                         mono(7.0f, key ? kBone : kSteelDim, 0.2f))
                           .width(28)
                           .shrink(0)
                           .textAlign(sigil::weave::TextAlignment::kCenter));
    }
    return panel(kPanelH[2], 3)
        .gap(3)
        .child(panelHead("COLOUR IS OVERLAP COUNT"))
        .child(box().row().gap(2).shrink(0).staggerChildren(26ms).children(
            std::move(swatches)))
        .child(box().row().gap(2).shrink(0).children(std::move(labels)))
        .child(box().grow(1))
        .child(t("LIGHT ADDS AND CLAMPS (\xc2\xa7"
                 "2.5) \xe2\x80\x94 RED "
                 "SATURATES AT n=5, GREEN AT n=20, BLUE AT n=111. "
                 "e\xe2\x82\x80 = (0.220, 0.050, 0.009) IS THE ONE "
                 "RECONSTRUCTED SEED.",
                 mono(6.5f, kSteel, 0.2f))
                   .shrink(0));
  }

  Element benchCell(Element content, const char* caption, SkColor4f cc) {
    return box()
        .column()
        .gap(3)
        .width(130)
        .shrink(0)
        .child(box()
                   .width(130)
                   .height(52)
                   .shrink(0)
                   .clip(true)
                   .fill(hex(0x05060A))
                   .stroke(stroke(1.0f, Fill::color(hex(0x1B2029)),
                                  PathFormat::Align::Inner))
                   .child(std::move(content)))
        .child(t(caption, mono(7.0f, cc, 0.2f))
                   .width(130)
                   .textAlign(sigil::weave::TextAlignment::kCenter));
  }

  /** The bench's third cell is EMPTY in the tree: the pen draws the quads
   *  into it afterwards, at the cell's own box, because those quads are
   *  the field's renderer and the field is the pen's. */
  Element renderModelPanel() {
    return panel(kPanelH[3], 4)
        .gap(4)
        .child(panelHead("RENDER MODEL \xe2\x80\x94 THREE PATHS, ONE POOL"))
        .child(
            box()
                .row()
                .gap(15)
                .shrink(0)
                .child(benchCell(box().inset(0).child(instancing::instances(
                                     abAtlas, abPool, instancing::Mode::Live,
                                     SkBlendMode::kSrcOver)),
                                 "instances() \xc2\xb7 kSrcOver",
                                 hex(0x8A93A8)))
                .child(benchCell(box().inset(0).child(instancing::instances(
                                     abAtlas, abPool, instancing::Mode::Live,
                                     SkBlendMode::kPlus)),
                                 "instances() \xc2\xb7 kPlus", hex(0xFFB672)))
                .child(benchCell(box().inset(0), "pen quads \xc2\xb7 kPlus",
                                 hex(0xFFB672))))
        .child(box().grow(1))
        .child(t("SAME 700 PARTICLES, ONE POOL. LEFT AND CENTRE DIFFER ONLY "
                 "IN BLEND: kSrcOver CANNOT ACCUMULATE, SO ITS WHOLE PALETTE "
                 "IS LUT ENTRY n=1. ALL THREE ARE STREAKED SPHERICAL "
                 "\xe2\x80\x94 LENGTH 0.5\xc2\xb7|v|, WIDTH size. THE TWO "
                 "POOLS TAKE IT FROM Pool::sizes(), THE OPT-IN NON-UNIFORM "
                 "LANE THAT STRETCHES ONE BAKED CELL PER INSTANCE.",
                 mono(6.5f, kSteel, 0.2f))
                   .shrink(0));
  }

  Element prodLine(const char* s, SkColor4f c) {
    return t(s, mono(8.0f, c, 0.2f)).height(10).shrink(0);
  }

  Element productionPanel() {
    return panel(kPanelH[4], 5)
        .gap(1)
        .child(panelHead("PRODUCTION \xe2\x80\x94 SMITH 1982"))
        .child(prodLine("67-SECOND SHOT \xc2\xb7 250,000 PX/FRAME \xc2\xb7 "
                        "500-LINE VIDEO MONITOR",
                        kBone))
        .child(prodLine("2 MAN-YEARS OVER AN 80-SECOND PIECE (60 s GENESIS + "
                        "20 s RETINA ID)",
                        kBone))
        .child(prodLine("FRAMES: 5 MINUTES TO 5 HOURS \xc2\xb7 ~1 MONTH OF "
                        "VAX TIME FOR THE FRACTALS",
                        kSteel))
        .child(prodLine("E&S PICTURE SYSTEM II \xc2\xb7 2\xc3\x97 IKONAS "
                        "\xc2\xb7 BARCO \xc2\xb7 HITACHI TABLET",
                        kSteel))
        .child(prodLine("DELIVERED MARCH 19, 1982 \xc2\xb7 SHOT TO "
                        "VISTAVISION BY ILM",
                        kSteel))
        .child(box().grow(1))
        .child(t("Am. Cinematographer 63(10) \xe2\x80\x94 caption: 67 s; "
                 "body text: 60 s. Both printed.",
                 mono(6.5f, kSteelDim, 0.2f))
                   .shrink(0));
  }

  // =========================================================================

  Element header() {
    Track rise{.effect = fx::rise(22),
               .stagger = {.eachMs = 26, .durationMs = 460},
               .progress = animate(
                   from(0.0f).to(1.0f),
                   {.duration = 850ms, .ease = &ch::easeNone, .delay = 120ms})};
    return box()
        .column()
        .height(kHeaderH)
        .shrink(0)
        .gap(4)
        .child(
            t("STOCHASTIC PARTICLE SYSTEMS", ui(11.5f, kSteel, 2.7f))
                .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
                .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})))
        .child(t("THE GENESIS DEMO, 1982", faced(heavyFace(), 46, kBone, -0.4f))
                   .key("title")
                   .fx(std::move(rise)))
        .child(t("W. T. Reeves, Lucasfilm Ltd \xe2\x80\x94 \"Particle "
                 "Systems: A Technique for Modeling a Class of Fuzzy "
                 "Objects\", SIGGRAPH '83 / ACM TOG 2(2) \xc2\xb7 sequence "
                 "dir. Alvy Ray Smith \xc2\xb7 Star Trek II, Paramount, "
                 "June 4, 1982",
                 ui(11.0f, kSteel, 0.1f))
                   .opacity(animate(from(0.0f).to(1.0f),
                                    {.duration = 240ms, .delay = 420ms})))
        .child(box().grow(1))
        .child(box().height(1).shrink(0).fill(kKeyline).opacity(
            animate(from(0.0f).to(1.0f), {.duration = 400ms, .delay = 320ms})));
  }

  // =========================================================================

  void setup(sketch::DrawContext& ctx) override {
    // 4.6 s into the 10 s loop: the wavefront is near the right edge, the
    // leftmost systems are burning out and the rightmost have just ignited.
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(kInk);
    ctx.captureAt(4.6);
    pinned = ctx.deterministic;

    loopT = 0;
    stepped = false;
    simSteps = 0;
    liveCount = 0;
    buildUs = 0;
    vertCount = 0;
    parts.clear();
    abParts.clear();
    fieldChunks.clear();
    abChunks.clear();

    // The 53 second-level systems, and THE STAGGER.
    rng = 0x0F1E2D3Cu;
    sites.clear();
    for (int i = 0; i < kSiteCount; ++i) {
      const float x = kX0 + kSiteStep * (float)i;
      const float y = limbY(std::clamp(x, 0.0f, kStageW));
      SkVector n{x - kLimbCx, y - kLimbCy};
      const float l = n.length();
      n = {n.fX / l, n.fY / l};
      // one mean-velocity draw per system, fixed for its whole life
      const float vs = 0.80f + 0.42f * rand01();
      sites.push_back(Site{{x, y}, n, {-n.fY, n.fX}, (x - kX0) / kSpread, vs});
    }

    seedStars();
    seedPlan();
    // (seedStars/seedPlan reseed rng; the site velocities were drawn
    //  above from the sketch's own stream.)
    seedBench();
    rng = 0x9E3779B9u;

    // The clock. [R83] counts lifetimes in FRAMES and the frames in
    // question are film frames, so the whole simulation runs at a fixed
    // 24 Hz whatever rate the host draws at. addFixed's catch-up clamp
    // matters here and is visible: a headless pre-roll at `--fps 1` hands
    // the ticker dt = 1.0, addFixed runs its 8 steps and DROPS the other
    // 16, so the sim lags the wall clock — which is the correct failure,
    // not a bug.
    ctx.ticker.addFixed(
        kSimHz,
        [this] {
          stepSim();
          stepped = true;
          return true;
        },
        8, &simAlpha);

    headerEl = header();
    belowEl = stageBelow();
    aboveEl = stageAbove();
    genEl = generationPanel();
    rampEl = rampPanel();
    benchEl = renderModelPanel();
    prodEl = productionPanel();

    ctx.pen.noStroke();
    ctx.pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
  }

  void draw(Pen& pen) override {
    // The renderers follow the SIM clock: the streak lists, the two pools
    // and the census row are rebuilt when — and only when — a film frame
    // has passed.
    if (stepped) {
      stepped = false;
      const auto t0 = std::chrono::steady_clock::now();
      buildStreaks(parts, fieldChunks);
      const auto t1 = std::chrono::steady_clock::now();
      const double us =
          std::chrono::duration<double, std::micro>(t1 - t0).count();
      buildUs = buildUs > 0 ? buildUs * 0.85 + us * 0.15 : us;
      vertCount = parts.size() * 8;
      buildStreaks(abParts, abChunks);
      writeBenchPool();
      writePlanPool();
    }

    // ONE phase Output, carrying the fixed step's leftover fraction so the
    // sweep is smooth at any draw rate. bind() derives the wavefront in
    // px, the plan ring's unit scale, and two piecewise alphas from it;
    // without that shaping each consumer would need an Output of its own,
    // kept in step by hand.
    const double phaseT = loopT + (double)simAlpha.value() * kSimStep;
    loopU = (float)(std::fmod(phaseT, kLoopSeconds) / kLoopSeconds);
    liveFrac = std::clamp(
        (float)(std::log10(std::max(1.0, (double)liveCount)) - 3.8) / 2.2f,
        0.0f, 1.0f);

    pen.background(kInk);
    pen.element(headerEl,
                SkRect::MakeXYWH(kPad, kPad, kCanvasW - 2 * kPad, kHeaderH));

    // --- the stage: two guests with the field drawn between them --------
    const SkRect stageBox =
        SkRect::MakeXYWH(kStageX, kBodyY, kStageW, kStageH);
    pen.element(belowEl, stageBox);
    {
      const float t = loopU.value() * 10.0f;
      const float a = std::clamp(t / 0.30f, 0.0f, 1.0f) *
                      std::clamp((9.55f - t) / 0.5f, 0.0f, 1.0f);
      // The stage is the mask and ADD is the colour model, both the
      // pen's and both held by this push: the field is drawn BETWEEN two
      // guests, into the box the stage declares.
      pen.push();
      pen.clip([&] {
        pen.rect(stageBox.x(), stageBox.y(), stageBox.width(),
                 stageBox.height());
      });
      pen.blendMode(sigil::draw::ADD);
      pen.translate(kStageX, kBodyY);
      paintField(pen, fieldChunks, a);
      pen.pop();
    }
    pen.element(aboveEl, stageBox);
    blurCallout(pen, kStageX + 24, kBodyY + kStageH - 24 - 142, 268, 142,
                cue(pen.millis(), 1150, 340));
    // the bezel
    pen.noFill();
    pen.stroke(hex(0x242A36, cue(pen.millis(), 260, 520, &ch::easeOutCubic)));
    pen.strokeWeight(1.5f);
    pen.rect(kStageX + 0.75f, kBodyY + 0.75f, kStageW - 1.5f, kStageH - 1.5f);
    pen.noStroke();

    // --- the caption band, declared OUTSIDE the artefact ----------------
    stageCaption(pen);

    // --- the sidebar: five panels, each its own guest --------------------
    pen.element(genEl,
                SkRect::MakeXYWH(kSideX, panelTop(0), kSideW, kPanelH[0]));
    pen.element(censusPanel(),
                SkRect::MakeXYWH(kSideX, panelTop(1), kSideW, kPanelH[1]));
    pen.element(rampEl,
                SkRect::MakeXYWH(kSideX, panelTop(2), kSideW, kPanelH[2]));
    pen.element(benchEl,
                SkRect::MakeXYWH(kSideX, panelTop(3), kSideW, kPanelH[3]));
    pen.element(prodEl,
                SkRect::MakeXYWH(kSideX, panelTop(4), kSideW, kPanelH[4]));

    // The bench's third cell: the SAME particles as the two instanced
    // cells, through the field's own quads.
    {
      const float cellX = kSideX + 12 + 2 * (130 + 15);
      const float cellY = panelTop(3) + 12 + 13 + 4;
      pen.push();
      pen.clip([&] { pen.rect(cellX, cellY, 130, 52); });
      pen.blendMode(sigil::draw::ADD);
      pen.translate(cellX, cellY);
      paintField(pen, abChunks, 1.0f);
      pen.pop();
    }
  }

  /** THE CAPTION BAND. It states what the study proves — how many streaks
   *  are alive, the raster the demo was computed for, and which light in
   *  the picture is hand-placed — and it is drawn OUTSIDE the stage,
   *  because a plate of an artefact is a plate of the artefact. Nothing
   *  here is a mark on the frame; it is a band under it. */
  void stageCaption(Pen& pen) {
    const float a = cue(pen.millis(), 1250, 300);
    if (a <= 0.001f) return;
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "FIELD: %zu,%03zu STREAKS \xc2\xb7 %zu,%03zu VERTS "
                  "\xc2\xb7 %zu drawVertices \xc2\xb7 BUILD %.2f ms / SIM "
                  "FRAME",
                  liveCount / 1000, liveCount % 1000, vertCount / 1000,
                  vertCount % 1000, fieldChunks.size(),
                  // The one number on this canvas that measures the host
                  // rather than the artefact, so it differs between two
                  // renders of the same frame. It is pinned to zero when
                  // the host is capturing for a diff, which is what makes
                  // a captured still comparable byte for byte.
                  pinned ? 0.0 : buildUs / 1000.0);
    const float right = kStageX + kStageW;
    pen.textAlign(sigil::draw::RIGHT, sigil::draw::TOP);
    penMono(pen, 8.5f, fadeTo(hex(0xFFB672, 0.85f), a), 0.5f);
    pen.text(buf, right, kCaptionY);
    penMono(pen, 8.5f, fadeTo(kSteel, a), 0.5f);
    pen.text("888\xc3\x97"
             "666 = 4:3 \xe2\x80\x94 THE 500-LINE VIDEO RASTER THE DEMO WAS "
             "COMPUTED FOR",
             right, kCaptionY + 12);
    penMono(pen, 8.5f, fadeTo(hex(0xFF8A3A, 0.75f), a), 0.5f);
    pen.text("WARM GROUND LIGHT RIDING THE FRONT = TOM DUFF'S LOCAL LIGHT, "
             "THE ONLY HAND-PLACED LIGHT IN THE SHOT",
             right, kCaptionY + 24);
    pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
  }

  /** A part's entrance as time arithmetic: what a described tree spells
   *  as `animate(from(0).to(1), {duration, delay})`, in a loop that has
   *  the clock in its hand. */
  static float cue(double ms, float delayMs, float durationMs,
                   const ch::EaseFn& ease = nullptr) {
    const float u = std::clamp(
        (float)((ms - (double)delayMs) / (double)durationMs), 0.0f, 1.0f);
    return ease ? ease(u) : u;
  }
};

SIGIL_SKETCH(
    GenesisFire, "Study \xc2\xb7 Motion",
    "The Genesis Demo wall of fire (Lucasfilm, 1982) \xe2\x80\x94 the first "
    "particle system")
