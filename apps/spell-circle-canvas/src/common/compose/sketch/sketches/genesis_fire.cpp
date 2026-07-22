// genesis_fire.cpp — STUDY 03: the Genesis Demo wall of fire (Lucasfilm
// Ltd, 1982) and the first particle system.
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
// WHY IT IS A SigilCompose STRESS TEST
//   Reeves' §2.2 attribute list IS the Pool schema the roadmap keeps
//   asking for, published in 1983. Five of its seven attributes fit:
//   position, colour, transparency and initial size land on
//   positions()/tints()/scales(), and per-sprite blend landed in
//   d1ae3db, so the additive-and-clamp colour model finally reaches the
//   destination — the sidebar's three-path bench shows the SAME pool at
//   kSrcOver and kPlus side by side. Two do not:
//     * `shape: streaked spherical` is per-instance NON-UNIFORM scale (a
//       quad 0.5*|v| long by `size` wide, swinging ~2.4:1 at ejection to
//       under 1:1 at apogee). SkRSXform is uniform by construction, so
//       the 30,000-body field drops to custom() + one SkVertices list
//       per 8,000 streaks. That is not a defeat — it is Reeves' own
//       renderer, "merely antialiased lines" — but it is 69 lines the
//       library should have owned.
//     * `lifetime`, measured in frames, has no delay/progress lane, and
//       the wall of fire IS a stagger. It lives in a parallel Site::t0.
//   Everything else is the library: two CONTROL pools through
//   instances() (the star field, Fig. 2's 149 ring marks), Materials for
//   the regolith and every soft light, shapes:: outlines, onPath for the
//   rim caption, bind() shaping ONE phase Output into four units,
//   slot() for the two live readouts, and ticker.addFixed for the 24 Hz
//   film clock.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/genesis_fire.cpp \
//       --frame /tmp/genesis_fire.png --at 4.6

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkVertices.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// Palette (study §5.3 — this study's own chrome, not sourced)

constexpr SkColor4f hex(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xFF) / 255.0f,
          (float)((rgb >> 8) & 0xFF) / 255.0f, (float)(rgb & 0xFF) / 255.0f, a};
}

constexpr SkColor4f kInk = hex(0x06070B);
constexpr SkColor4f kPanel = hex(0x0B0D14);
constexpr SkColor4f kBone = hex(0xE9ECF3);
constexpr SkColor4f kSteel = hex(0x77819A);
constexpr SkColor4f kSteelDim = hex(0x545E74);
constexpr SkColor4f kKeyline = hex(0x242A36);
constexpr SkColor4f kCyan = hex(0x4FB8D8);

// §5.1 — THE EMISSION SEED. Everything about the fire's colour is a
// consequence of this triple plus "light adds and clamps" [R83 §2.5].
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
// §6 — geometry

constexpr float kCanvasW = 1440, kCanvasH = 864;
constexpr float kStageW = 888, kStageH = 666; // 4:3 — the 500-line raster
constexpr float kSideW = 448;

constexpr float kLimbCx = 444.0f, kLimbCy = 1620.0f, kLimbR = 1150.0f;

float limbY(float x) {
  const float dx = x - kLimbCx;
  const float k = kLimbR * kLimbR - dx * dx;
  return kLimbCy - (k > 0 ? std::sqrt(k) : 0.0f);
}

// §4.4 — the reconstructed parameter table. All rates are per SIMULATION
// FRAME at 24 Hz, because [R83] counts lifetimes in frames and the frames
// in question are film frames.
constexpr double kSimHz = 24.0;
constexpr double kSimStep = 1.0 / kSimHz;
constexpr double kLoopSeconds = 10.0;

constexpr float kSpread = 168.0f;   // px/s  (7.0 px/frame)
constexpr float kSiteStep = 18.5f;  // px    (< R_gen, so systems overlap)
constexpr float kX0 = -80.0f;       // impact point, off-frame left
constexpr int kSiteCount = 53;      // covers -80 .. 882
constexpr float kGenWindow = 52.8f; // frames (2.20 s)
constexpr float kRGen = 26.0f;      // generation segment half-width, px
constexpr float kPsiMax = 34.0f * 0.0174532925f; // ejection cone, rad

// The birth rate is the one constant the census PINS. A limb view stacks
// the ring in depth (Fig. 6 photographs ~200 systems at once; a vertical
// slice anchors 53 columns), so each column carries kDepth systems' worth
// of births — see the census panel's derivation, which is printed on the
// canvas and measured against the live count.
constexpr int kDepth = 3;
// The A/B bench's population — enough bodies to accumulate in a 130x52
// cell, which 240 is not.
constexpr size_t kAbCount = 700;
constexpr float kInitialMeanParts = 41.0f * (float)kDepth;
constexpr float kDeltaMeanParts =
    -kInitialMeanParts / kGenWindow; // rate hits 0 exactly at the window
constexpr float kVarParts = 13.0f * (float)kDepth;

constexpr float kMeanSpeed = 15.0f, kVarSpeed = 4.8f; // px/frame
constexpr float kGravity = 1.02f;                     // px/frame^2
constexpr float kMeanLife = 34.0f, kVarLife = 12.0f;  // FRAMES
constexpr float kMeanSize = 4.0f, kVarSize = 1.6f;    // px diameter
constexpr float kSizeRate = 0.05f; // px/frame — [R83 §2.3] size changes at
                                   // a rate global to the system
constexpr float kColorVar = 0.35f;
// Documented ORDERING ("green and blue dropped off quickly, and the red
// followed at a slower rate"); the rates are reconstruction.
constexpr float kDecR = kE0r / 34.0f, kDecG = kE0g / 12.0f,
                kDecB = kE0b / 8.5f;
constexpr float kMinIntensity = 0.020f;

// Derived timing, printed on the canvas.
constexpr double kFrontCrossSeconds = (kStageW - kX0) / kSpread; // 5.762 s

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

/** A panel shell: ground, keyline, corners, padding. */
Element panel(float height, int order) {
  return box()
      .column()
      .width(kSideW)
      .height(height)
      .shrink(0)
      .padding(12)
      .corners({5})
      .fill(kPanel)
      .stroke(stroke(1.0f, Fill::color(kKeyline), PathFormat::Align::Inner))
      .opacity(withFrom(0.0f, 1.0f, {.duration = 300ms}))
      .translateX(withFrom(14.0f, 0.0f, {.duration = 300ms}))
      .key(std::string("panel") + std::to_string(order));
}

Element panelHead(const char *s) {
  return t(s, ui(9.5f, kSteel, 1.9f)).height(13).shrink(0);
}

} // namespace

// ===========================================================================

struct GenesisFire : sigil::compose::sketch::Sketch {
  // --- §4.1 the two levels -------------------------------------------------
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
    float r = 0, g = 0, b = 0;     // emission (the seven attributes, §2.2)
    float size = 0, grow = 0;      // diameter, px + its per-frame rate
    float age = 0, life = 0;       // FRAMES
    uint16_t site = 0;
  };

  std::vector<Site> sites;
  std::vector<Particle> parts;
  std::vector<sk_sp<SkVertices>> fieldChunks;

  // --- the A/B bench: one explosion, three renderers -----------------------
  std::vector<Particle> abParts;
  std::shared_ptr<instancing::Atlas> abAtlas;
  std::shared_ptr<instancing::Pool> abPool;

  // --- the two CONTROL pools (textbook instancing) -------------------------
  std::shared_ptr<instancing::Atlas> starAtlas;
  std::shared_ptr<instancing::Pool> starPool;
  std::shared_ptr<instancing::Atlas> planAtlas;
  std::shared_ptr<instancing::Pool> planPool;
  struct PlanMark {
    SkPoint p;
    float dist = 0; // from the impact point, in plan px
  };
  std::vector<PlanMark> planMarks;

  // --- clocks --------------------------------------------------------------
  double loopT = 0, elapsed = 0;
  bool stepped = false; // set by the fixed-timestep steppable
  uint64_t simSteps = 0;
  uint32_t rng = 0x9E3779B9u;

  // --- ONE phase Output; bind() derives every consumer from it -------------
  ch::Output<float> loopU{0.0f};   // loop fraction, [0,1)
  ch::Output<float> liveFrac{0.0f}; // census bar, [0,1]

  // --- measured ------------------------------------------------------------
  size_t liveCount = 0, peakCount = 0;
  double buildUs = 0;
  size_t vertCount = 0;

  // =========================================================================
  // RNG — [R83 §2.1] "Rand is a procedure returning a uniformly distributed
  // random number between -1.0 and +1.0".

  float rand01() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return (float)(rng >> 8) * (1.0f / 16777216.0f);
  }
  float rand11() { return rand01() * 2.0f - 1.0f; }

  // =========================================================================
  // §4.2 — one second-level system

  void emitAt(std::vector<Particle> &into, const Site &s, uint16_t si,
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

  /** §4.2 per-frame integration + the three documented extinction rules. */
  void advance(std::vector<Particle> &v, const std::vector<Site> &ss,
               float gravity, bool killBelowSurface) {
    for (size_t i = 0; i < v.size();) {
      Particle &p = v[i];
      const Site &s = ss[p.site];
      p.vel.fX -= s.n.fX * gravity; // acceleration -> parabolic arcs
      p.vel.fY -= s.n.fY * gravity;
      p.pos.fX += p.vel.fX;
      p.pos.fY += p.vel.fY;
      p.age += 1.0f;
      p.size += p.grow; // size change at a rate global to the system
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
    if (loopT >= kLoopSeconds) { // the sequence had a cut; a hard reset reads
      loopT -= kLoopSeconds;     // as one
      parts.clear();
    }
    // Generation. THE WALL IS A STAGGER: site i ignites at
    // t_i = (x_i - X0) / SPREAD — distance from the point of impact.
    for (uint16_t i = 0; i < (uint16_t)sites.size(); ++i) {
      const float f = (float)(loopT - sites[i].t0) * (float)kSimHz;
      if (f < 0.0f || f >= kGenWindow)
        continue;
      const float rate = kInitialMeanParts + kDeltaMeanParts * f;
      const int n =
          (int)std::lround(std::max(0.0f, rate + rand11() * kVarParts));
      for (int k = 0; k < n; ++k)
        emitAt(parts, sites[i], i, 1.0f, 1.0f, 1.0f);
    }
    advance(parts, sites, kGravity, true);

    // The A/B bench: a steady-state explosion, ~240 alive, fixed recipe.
    const int nb = (int)std::lround(std::max(0.0f, 25.0f + rand11() * 5.0f));
    for (int k = 0; k < nb && abParts.size() < kAbCount; ++k)
      emitAt(abParts, abSite(), 0, 0.55f, 0.72f, 0.35f);
    advance(abParts, abSites(), 1.06f, false);

    liveCount = parts.size();
    peakCount = std::max(peakCount, liveCount);
    ++simSteps;
  }

  // The bench's single system, in its 130x56 cell's local px.
  static const std::vector<Site> &abSites() {
    static const std::vector<Site> s = {
        Site{{65.0f, 55.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, 0.0f}};
    return s;
  }
  static const Site &abSite() { return abSites()[0]; }

  // =========================================================================
  // §4.3 — the renderer that defines the look.
  //
  // ONE additive pass over every living particle, no depth sort, no
  // occlusion. Each streak is a quad from pos(f + 1/2) to pos(f) — a
  // 1/50 s shutter at 24 fps is half the inter-frame motion — six
  // vertices wide so the cross-section falls off (drawVertices does not
  // antialias its own edges, so the falloff lives in the vertex colours).
  // kPlus on the paint is the whole colour model: light ADDS and CLAMPS.

  void buildStreaks(const std::vector<Particle> &v,
                    std::vector<sk_sp<SkVertices>> &out) {
    out.clear();
    if (v.empty())
      return;
    // SkVertices indices are uint16, so one list holds 65,535 vertices ->
    // 8,191 streaks at eight each. Chunk the way drawSpriteAtlas does.
    constexpr size_t kChunk = 8000;
    static thread_local std::vector<SkPoint> pos;
    static thread_local std::vector<SkColor> col;
    static thread_local std::vector<uint16_t> idx;
    // Six triangles: a FLAT core with an antialiased shoulder either side,
    // which is what an antialiased line actually is. (A three-band strip
    // whose core is one line is 30% dimmer for the same coverage.)
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
        const Particle &p = v[base + i];
        const float speed = p.vel.length();
        const SkVector d = speed > 1e-4f
                               ? SkVector{p.vel.fX / speed, p.vel.fY / speed}
                               : SkVector{0.0f, -1.0f};
        // streaked spherical: length 0.5*|v|, width `size`. The aspect
        // swings ~3.5:1 at ejection to ~1:1 at apogee — which is exactly
        // what SkRSXform cannot express (see LIBRARY GAPS).
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
        for (int k = 0; k < 18; ++k)
          idx.push_back((uint16_t)(v0 + kTri[k]));
      }
      out.push_back(SkVertices::MakeCopy(
          SkVertices::kTriangles_VertexMode, (int)pos.size(), pos.data(),
          nullptr, col.data(), (int)idx.size(), idx.data()));
    }
  }

  static void drawField(SkCanvas &canvas,
                        const std::vector<sk_sp<SkVertices>> &chunks) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setBlendMode(SkBlendMode::kPlus); // light adds; the buffer clamps
    for (const sk_sp<SkVertices> &v : chunks)
      if (v)
        canvas.drawVertices(v, SkBlendMode::kDst, p); // no shader -> vertex
  }                                                   // colour is the source

  PaintProgram fieldProgram() {
    return [this](SkCanvas &canvas, const PaintContext &) {
      drawField(canvas, fieldChunks);
    };
  }

  /** The A/B bench's third cell: the same 240 particles as the two
   *  instanced cells, through §4.3's quads. */
  PaintProgram benchQuadProgram() {
    return [this](SkCanvas &canvas, const PaintContext &) {
      std::vector<sk_sp<SkVertices>> chunks;
      buildStreaks(abParts, chunks);
      drawField(canvas, chunks);
    };
  }

  /** The bench pool: ONE pool, read by two instances() leaves at two
   *  blend modes. Same data, two renderers — that is what makes it
   *  evidence rather than an assertion. */
  void writeBenchPool() {
    abPool->resize(kAbCount);
    auto p = abPool->positions();
    auto r = abPool->rotations();
    auto s = abPool->scales();
    auto tn = abPool->tints();
    auto fr = abPool->frames();
    for (size_t i = 0; i < kAbCount; ++i) {
      if (i < abParts.size()) {
        const Particle &q = abParts[i];
        p[i] = q.pos;
        r[i] = std::atan2(q.vel.fY, q.vel.fX);
        s[i] = std::max(0.25f, q.size / 2.3f); // ONE uniform float
        tn[i] = {std::min(1.0f, q.r), std::min(1.0f, q.g),
                 std::min(1.0f, q.b), 1.0f};
        fr[i] = 0;
      } else {
        p[i] = {-999, -999};
        tn[i] = {0, 0, 0, 0};
      }
    }
    abPool->touch();
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
    planPool->touch();
  }

  // =========================================================================
  // §9.5 / §9.2 — the two control pools, laid out once

  void seedStars() {
    starAtlas = std::make_shared<instancing::Atlas>(2.0f);
    // Five magnitude cells: a soft disc each, 10/7/5/4/3 px logical.
    const float sizes[5] = {7.0f, 5.4f, 4.2f, 3.2f, 2.4f};
    for (float s : sizes)
      starAtlas->cell(box()
                          .width(s)
                          .height(s)
                          .fill(Material::radialUnit(
                              {0.5f, 0.5f}, 0.707f,
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
          if (y < limbY(x) - 8.0f)
            break;
        }
        int pick = (int)(rand01() * 100.0f), c = 0, acc = 0;
        for (; c < 6; ++c) {
          acc += bvWeight[c];
          if (pick < acc)
            break;
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
                        .outline(shapes::circle())
                        .stroke(stroke(0.7f, Fill::color(hex(0x2E3A46)),
                                       PathFormat::Align::Inner)),
                    {4, 4});
    planAtlas->cell(box()
                        .width(4.0f)
                        .height(4.0f)
                        .fill(Material::radialUnit({0.5f, 0.5f}, 0.707f,
                                                   {{0.0f, {1, 1, 1, 1}},
                                                    {0.45f, {1, 1, 1, 0.8f}},
                                                    {1.0f, {1, 1, 1, 0}}})),
                    {4, 4});
    // Seven rings from the impact point; marks per ring =
    // round(0.055 * 2*pi*r) — [R83 §3]'s circumference x density rule.
    // Random angular placement, also documented.
    planPool = std::make_shared<instancing::Pool>();
    planMarks.clear();
    const SkPoint impact{34.0f, 106.0f}; // disc-local (disc centre 92,92)
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
    // ONE baked aspect. The paper's shape swings 3.5:1 -> 1:1 over a
    // particle's life; an atlas cell is one size and a Pool scale is one
    // float, so this cell is the compromise the middle two panels show.
    abAtlas->cell(box()
                      .width(4.4f)
                      .height(2.3f)
                      .corners({1.0f})
                      .fill(Material::radialUnit({0.5f, 0.5f}, 1.05f,
                                                 {{0.0f, {1, 1, 1, 1}},
                                                  {0.42f, {1, 1, 1, 0.9f}},
                                                  {1.0f, {1, 1, 1, 0}}})),
                  {4.8f, 2.6f});
    abPool = std::make_shared<instancing::Pool>();
    abPool->resize(kAbCount);
  }

  // =========================================================================
  // Stage

  Element starField() {
    return box()
        .inset(0)
        .opacity(withFrom(0.0f, 1.0f, {.duration = 700ms, .delay = 340ms}))
        .child(instancing::instances(starAtlas, starPool,
                                     instancing::Mode::Data,
                                     SkBlendMode::kPlus));
  }

  struct DipStar {
    const char *name;
    float u, v, mag;
  };

  Element dipper() {
    // §6.1 — real relative geometry, framed into x[430,860] y[40,250].
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
    g.child(box()
                .inset(0)
                .outline([&, bx, by, bw, bh](SkSize) {
                  SkPathBuilder b;
                  auto P = [&](int i) {
                    return SkPoint{bx + kStars[i].u * bw,
                                   by + kStars[i].v * bh};
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
                .stroke(stroke(1.0f, Fill::color(hex(0x4FB8D8, 0.35f))))
                .trim(0.0f, withFrom(0.0f, 1.0f,
                                     {.duration = 620ms, .delay = 1300ms}))
                .key("asterism"));

    for (int i = 0; i < 8; ++i) {
      const SkPoint p = at(i);
      const float rad = std::max(1.4f, 4.6f - 0.85f * kStars[i].mag);
      const bool sol = i == 7;
      g.child(disc(p, rad * 2.0f)
                  .fill(Material::radialUnit(
                      {0.5f, 0.5f}, 0.707f,
                      {{0.0f, sol ? hex(0xFFFFFF) : hex(0xEFF3FF)},
                       {0.22f, sol ? hex(0xFFF4D8, 0.9f) : hex(0xD9E4FF, 0.85f)},
                       {1.0f, {1, 1, 1, 0}}}))
                  .blend(SkBlendMode::kPlus)
                  .opacity(withFrom(0.0f, 1.0f,
                                    {.duration = 500ms, .delay = 1200ms})));
      g.child(t(kStars[i].name, mono(7.0f, sol ? kCyan : hex(0x9FB0CC, 0.85f),
                                     1.1f))
                  .left(p.fX + rad + 5.0f)
                  .top(p.fY - 5.0f)
                  .opacity(withFrom(0.0f, 1.0f,
                                    {.duration = 400ms, .delay = 1500ms})));
    }
    // Smith's joke, verified in the header block.
    const SkPoint s = at(7);
    g.child(box()
                .left(s.fX + 4)
                .top(s.fY + 6)
                .width(1)
                .height(16)
                .fill(hex(0x4FB8D8, 0.5f)));
    g.child(box()
                .left(s.fX + 9)
                .top(s.fY + 12)
                .column()
                .gap(1)
                .opacity(withFrom(0.0f, 1.0f,
                                  {.duration = 400ms, .delay = 1600ms}))
                .child(t("m = 2.63 FROM \xce\xb5 INDI (3.64 pc)",
                         mono(7.0f, kCyan, 0.9f)))
                .child(t("\"OUR SUN WOULD APPEAR AS AN EXTRA STAR\"",
                         mono(7.0f, hex(0x4FB8D8, 0.7f), 0.9f))));
    return g;
  }

  Element regolith() {
    // §9.3 — a generated surface, plus the ONE hand-added light in the
    // shot (Tom Duff's), riding the wavefront.
    Material ground = Material::blend(
        {{Material::radialUnit({0.5f, 0.723f}, 0.50f,
                               {{0.0f, hex(0x3B3933)},
                                {0.42f, hex(0x232119)},
                                {1.0f, hex(0x0A0A0C)}}),
          SkBlendMode::kSrc},
         {patterns::grain(0.022f, 4, 7.0f, 0.5f, 1.0f),
          SkBlendMode::kSoftLight},
         {patterns::speckle(170, 17, 0.9f, 3.4f,
                            {hex(0x6A655B), hex(0x171512)})
              .material(),
          SkBlendMode::kOverlay}});

    return box()
        .inset(0)
        .outline(limbOutline())
        .clip(true)
        .fill(std::move(ground))
        .opacity(withFrom(0.0f, 1.0f, {.duration = 520ms, .delay = 420ms}))
        .translateY(withFrom(12.0f, 0.0f,
                             {.duration = 520ms,
                              .ease = &ch::easeOutCubic,
                              .delay = 420ms}))
        // Duff's local light. ONE Output (loopU) shaped into px.
        .child(disc({0, 0}, 132)
                   .fill(Material::radialUnit({0.5f, 0.5f}, 0.707f,
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
    // §7.2 — the impact flash and Carpenter/Cole's shockwave, at the
    // impact point (off-frame left). Documented as elements; timing is
    // reconstruction.
    const SkPoint impact{kX0, limbY(kX0 < 0 ? 0.0f : kX0) + 8.0f};
    Element g = box().inset(0);
    g.child(disc(impact, 170)
                .fill(Material::radialUnit({0.5f, 0.5f}, 0.707f,
                                           {{0.0f, {1, 1, 1, 0.95f}},
                                            {0.25f, hex(0xFFE7B0, 0.6f)},
                                            {1.0f, hex(0xFF7A20, 0.0f)}}))
                .blend(SkBlendMode::kPlus)
                .opacity(bind(&loopU).map([](float v) {
                  const float t = v * 10.0f;
                  if (t < 0.06f)
                    return t / 0.06f;
                  if (t < 0.45f) {
                    const float k = 1.0f - (t - 0.06f) / 0.39f;
                    return k * k;
                  }
                  return 0.0f;
                })));
    g.child(disc(impact, 520)
                .outline(shapes::circle())
                .stroke(stroke(2.0f, Fill::color(hex(0xFFB070, 0.85f))))
                .blend(SkBlendMode::kPlus)
                .scale(bind(&loopU).map([](float v) {
                        const float t = std::clamp(v * 10.0f / 1.1f, 0.0f, 1.0f);
                        const float u = t - 1.0f;
                        return 1.0f + u * u * u; // easeOutCubic
                      }).clamp(0.001f, 1.0f))
                .opacity(bind(&loopU).map([](float v) {
                  const float t = v * 10.0f;
                  if (t > 1.1f)
                    return 0.0f;
                  const float k = 1.0f - t / 1.1f;
                  return k * k;
                })));
    return g;
  }

  Element planInset() {
    // §6.1 — Fig. 2: the distribution of second-level systems on the
    // planet's surface, live.
    Element inner =
        box()
            .left(12)
            .top(12)
            .width(184)
            .height(184)
            .outline(shapes::circle())
            .clip(true)
            .stroke(stroke(1.0f, Fill::color(hex(0x4FB8D8, 0.55f)),
                           PathFormat::Align::Inner))
            // the expanding wavefront ring — same Output, unit scale
            .child(disc({34, 106}, 124)
                       .outline(shapes::circle())
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
        .opacity(withFrom(0.0f, 1.0f, {.duration = 340ms, .delay = 900ms}))
        .scale(withFrom(0.94f, 1.0f, {.duration = 340ms,
                                      .ease = ease::outBack(1.70158f),
                                      .delay = 900ms}))
        .child(std::move(inner))
        // the impact point itself
        .child(box()
                   .left(12 + 34 - 2)
                   .top(12 + 106 - 2)
                   .width(4)
                   .height(4)
                   .outline(shapes::circle())
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

  Element planCaption() {
    return t("FIG. 2 \xe2\x80\x94 DISTRIBUTION OF PARTICLE SYSTEMS ON THE "
             "PLANET'S SURFACE",
             mono(8.5f, kSteel, 0.6f))
        .left(24)
        .top(236)
        .width(300)
        .opacity(withFrom(0.0f, 1.0f, {.duration = 300ms, .delay = 1050ms}));
  }

  /** §3's motion-blur construction, magnified 3x. */
  PaintProgram blurCalloutProgram() {
    return [](SkCanvas &canvas, const PaintContext &ctx) {
      const float h = ctx.size.height();
      const float y = h * 0.42f;
      const float x0 = 16, x1 = 152;
      SkPaint line;
      line.setAntiAlias(true);
      line.setBlendMode(SkBlendMode::kPlus);
      // the streak itself: a 3x-magnified quad, additive
      const SkColor4f hot = overlap(9);
      const SkColor c = hot.toSkColor();
      SkPoint verts[6] = {{x0, y - 5}, {x1, y - 5}, {x1, y},
                          {x0, y},     {x1, y + 5}, {x0, y + 5}};
      SkColor vc[6] = {0, 0, c, c, 0, 0};
      const uint16_t idx[12] = {0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5};
      auto v = SkVertices::MakeCopy(SkVertices::kTriangles_VertexMode, 6, verts,
                                    nullptr, vc, 12, idx);
      canvas.drawVertices(v, SkBlendMode::kDst, line);
      // the two sample positions
      SkPaint dot;
      dot.setAntiAlias(true);
      dot.setColor4f(hex(0xFFFFFF, 0.95f), nullptr);
      canvas.drawCircle(x1, y, 2.6f, dot);
      dot.setColor4f(hex(0xFFFFFF, 0.45f), nullptr);
      canvas.drawCircle(x0, y, 2.0f, dot);
      // cyan dimension bracket
      SkPaint br;
      br.setAntiAlias(true);
      br.setStyle(SkPaint::kStroke_Style);
      br.setStrokeWidth(1.0f);
      br.setColor4f(hex(0x4FB8D8, 0.9f), nullptr);
      const float by = y + 13;
      canvas.drawLine(x0, by - 3, x0, by + 3, br);
      canvas.drawLine(x1, by - 3, x1, by + 3, br);
      canvas.drawLine(x0, by, x1, by, br);
      // labels
      SkFont f(monoFace(), 7.0f);
      SkPaint tp;
      tp.setAntiAlias(true);
      tp.setColor4f(hex(0x9FB0CC), nullptr);
      canvas.drawString("pos(f + 1/2)", x0 - 8, y - 11, f, tp);
      canvas.drawString("pos(f)", x1 - 14, y - 11, f, tp);
      tp.setColor4f(hex(0x4FB8D8), nullptr);
      canvas.drawString("0.5 \xc2\xb7 |v|", (x0 + x1) * 0.5f - 16, by + 12, f,
                        tp);
    };
  }

  Element blurCallout() {
    return box()
        .left(24)
        .bottom(24)
        .width(268)
        .height(142)
        .column()
        .gap(3)
        .padding(11)
        .corners({6})
        .fill(hex(0x0B0D14, 0.86f))
        .stroke(stroke(1.5f, Fill::color(kKeyline), PathFormat::Align::Inner))
        .opacity(withFrom(0.0f, 1.0f, {.duration = 340ms, .delay = 1150ms}))
        .child(t("MOTION BLUR \xe2\x80\x94 REEVES 1983 \xc2\xa7" "3",
                 ui(8.5f, kCyan, 1.7f))
                   .shrink(0))
        .child(custom(blurCalloutProgram()).height(44).shrink(0))
        .child(t("SHUTTER 1/50 s @ 24 fps \xe2\x89\x88 \xc2\xbd FRAME OF "
                 "MOTION",
                 mono(7.5f, kBone, 0.4f))
                   .shrink(0))
        .child(t("STREAK = pos(f) \xe2\x86\x92 pos(f+\xc2\xbd), ANTIALIASED, "
                 "ADDITIVE",
                 mono(7.5f, kSteel, 0.4f))
                   .shrink(0))
        .child(t("FN.4: \"A PARTICLE'S TRAJECTORY IS ACTUALLY PARABOLIC, BUT "
                 "THE STRAIGHT-LINE APPROXIMATION HAS SO FAR PROVED "
                 "SUFFICIENT\"",
                 mono(6.5f, kSteelDim, 0.2f)));
  }

  Element stageCaption() {
    return box()
        .right(16)
        .bottom(14)
        .column()
        .gap(2)
        .opacity(withFrom(0.0f, 1.0f, {.duration = 300ms, .delay = 1250ms}))
        .child(slot("fieldStat"))
        .child(t("888\xc3\x97" "666 = 4:3 \xe2\x80\x94 THE 500-LINE VIDEO "
                 "RASTER THE DEMO WAS COMPUTED FOR",
                 mono(8.5f, kSteel, 0.5f))
                   .textAlign(sigil::weave::TextAlignment::kEnd))
        .child(t("WARM GROUND LIGHT RIDING THE FRONT = TOM DUFF'S LOCAL "
                 "LIGHT, THE ONLY HAND-PLACED LIGHT IN THE SHOT",
                 mono(8.5f, hex(0xFF8A3A, 0.75f), 0.5f))
                   .textAlign(sigil::weave::TextAlignment::kEnd));
  }

  Element stage() {
    return stack()
        .width(kStageW)
        .height(kStageH)
        .shrink(0)
        .clip()
        .fill(Material::linearUnit({0.5f, 0.0f}, {0.5f, 0.85f},
                                   {{0.0f, hex(0x03040A)},
                                    {0.55f, hex(0x05060D)},
                                    {1.0f, hex(0x0A0B13)}}))
        .child(starField().zIndex(1))
        .child(dipper().zIndex(2))
        .child(regolith().zIndex(3))
        .child(shockwave().zIndex(4))
        .child(custom(fieldProgram())
                   .inset(0)
                   .cache(Cache::None)
                   .zIndex(5)
                   .opacity(bind(&loopU).map([](float v) {
                     const float t = v * 10.0f;
                     return std::clamp(t / 0.30f, 0.0f, 1.0f) *
                            std::clamp((9.55f - t) / 0.5f, 0.0f, 1.0f);
                   })))
        .child(planInset().zIndex(6))
        .child(planCaption().zIndex(6))
        .child(blurCallout().zIndex(6))
        .child(stageCaption().zIndex(6))
        .child(box()
                   .inset(0)
                   .stroke(stroke(1.5f, Fill::color(kKeyline),
                                  PathFormat::Align::Inner))
                   .trim(0.0f, withFrom(0.0f, 1.0f,
                                        {.duration = 520ms,
                                         .ease = &ch::easeOutCubic,
                                         .delay = 260ms}))
                   .zIndex(9));
  }

  // =========================================================================
  // Sidebar

  Element eqn(const char *s) {
    return t(s, mono(11.0f, kBone, 0.1f)).height(16).shrink(0);
  }

  Element generationPanel() {
    return panel(104, 1)
        .gap(3)
        .child(panelHead("GENERATION LAW"))
        .child(eqn("NParts_f    = MeanParts_f + Rand() \xc3\x97 VarParts_f"))
        .child(eqn("MeanParts_f = InitialMeanParts + \xce\x94Mean \xc3\x97 "
                   "(f \xe2\x88\x92 f\xe2\x82\x80)"))
        .child(eqn("InitialSpeed = MeanSpeed + Rand() \xc3\x97 VarSpeed"))
        .child(box().grow(1))
        .child(t("Rand() \xe2\x86\x92 UNIFORM [\xe2\x88\x92" "1.0, +1.0] "
                 "\xe2\x80\x94 REEVES 1983 \xc2\xa7" "2.1\xe2\x80\x93" "2.2",
                 mono(7.5f, kSteelDim, 0.4f))
                   .shrink(0));
  }

  Element censusCell(const char *s, float w, sigil::weave::TextStyle st) {
    return t(s, std::move(st)).width(w).shrink(0);
  }

  Element censusBar(float frac, SkColor4f c, const char *key) {
    Element fill = box()
                       .left(0)
                       .top(0)
                       .width(96)
                       .height(7)
                       .fill(c)
                       .transformOrigin(0.0f, 0.5f)
                       .scaleX(withFrom(0.0f, frac,
                                        {.duration = 420ms,
                                         .ease = ease::outBack(1.2f),
                                         .delay = 1200ms}));
    if (key)
      fill.scaleX(bind(&liveFrac).clamp(0.02f, 1.0f)).key(key);
    return box()
        .width(96)
        .height(7)
        .shrink(0)
        .fill(hex(0x171B24))
        .child(std::move(fill));
  }

  Element censusRow(const char *fig, const char *sys, const char *particles,
                    const char *per, float frac, bool live) {
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

  Element censusPanel() {
    return panel(192, 2)
        .gap(4)
        .child(panelHead("PARTICLE CENSUS \xe2\x80\x94 REEVES 1983 \xc2\xa7" "3"))
        .child(box()
                   .row()
                   .height(11)
                   .shrink(0)
                   .child(censusCell("FIG", 46, mono(7.5f, kSteelDim, 0.9f)))
                   .child(censusCell("SYSTEMS", 62, mono(7.5f, kSteelDim, 0.9f)))
                   .child(censusCell("PARTICLES", 108,
                                     mono(7.5f, kSteelDim, 0.9f)))
                   .child(censusCell("PER SYS", 76, mono(7.5f, kSteelDim, 0.9f)))
                   .child(censusCell("LOG SCALE", 96,
                                     mono(7.5f, kSteelDim, 0.9f))))
        .child(box()
                   .column()
                   .gap(3)
                   .shrink(0)
                   .staggerChildren(70ms)
                   .child(censusRow("4", "~21", "25,000", "1,190*", 0.273f,
                                    false))
                   .child(censusRow("5", "~200", "75,000", "375", 0.491f, false))
                   .child(censusRow("6", "~200", "85,000", "425", 0.514f, false))
                   .child(censusRow("7\xe2\x80\x93" "8", "~400", ">750,000",
                                    ">1,875", 0.945f, false))
                   .child(slot("censusLive")))
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
    for (int i = 0; i < 14; ++i) {
      const int n = kRampN[i];
      swatches.push_back(box()
                             .width(28)
                             .height(26)
                             .shrink(0)
                             .fill(Material::solid(overlap(n)))
                             .transformOrigin(0.5f, 1.0f)
                             .scaleY(withFrom(0.0f, 1.0f,
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
    return panel(98, 3)
        .gap(3)
        .child(panelHead("COLOUR IS OVERLAP COUNT"))
        .child(box()
                   .row()
                   .gap(2)
                   .shrink(0)
                   .staggerChildren(26ms)
                   .children(std::move(swatches)))
        .child(box().row().gap(2).shrink(0).children(std::move(labels)))
        .child(box().grow(1))
        .child(t("LIGHT ADDS AND CLAMPS (\xc2\xa7" "2.5) \xe2\x80\x94 RED "
                 "SATURATES AT n=5, GREEN AT n=20, BLUE AT n=111. "
                 "e\xe2\x82\x80 = (0.220, 0.050, 0.009) IS THE ONE "
                 "RECONSTRUCTED SEED.",
                 mono(6.5f, kSteel, 0.2f))
                   .shrink(0));
  }

  Element benchCell(Element content, const char *caption, SkColor4f cc) {
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

  Element renderModelPanel() {
    return panel(144, 4)
        .gap(4)
        .child(panelHead("RENDER MODEL \xe2\x80\x94 THREE PATHS, ONE POOL"))
        .child(box()
                   .row()
                   .gap(15)
                   .shrink(0)
                   .child(benchCell(box().inset(0).child(
                                        instancing::instances(
                                            abAtlas, abPool,
                                            instancing::Mode::Live,
                                            SkBlendMode::kSrcOver)),
                                    "instances() \xc2\xb7 kSrcOver",
                                    hex(0x8A93A8)))
                   .child(benchCell(box().inset(0).child(
                                        instancing::instances(
                                            abAtlas, abPool,
                                            instancing::Mode::Live,
                                            SkBlendMode::kPlus)),
                                    "instances() \xc2\xb7 kPlus",
                                    hex(0xFFB672)))
                   .child(benchCell(custom(benchQuadProgram())
                                        .inset(0)
                                        .cache(Cache::None),
                                    "custom() quads \xc2\xb7 kPlus",
                                    hex(0xFFB672))))
        .child(box().grow(1))
        .child(t("SAME 700 PARTICLES, ONE POOL. LEFT AND CENTRE DIFFER ONLY "
                 "IN BLEND: kSrcOver CANNOT ACCUMULATE, SO ITS WHOLE PALETTE "
                 "IS LUT ENTRY n=1. ONLY THE RIGHT CELL IS STREAKED "
                 "SPHERICAL \xe2\x80\x94 LENGTH 0.5\xc2\xb7|v|, WIDTH size "
                 "\xe2\x80\x94 SO ITS SLOW PARTICLES COLLAPSE TO DOTS AT "
                 "APOGEE WHILE THE ATLAS HOLDS ONE BAKED ASPECT FOREVER. "
                 "SkRSXform IS UNIFORM BY CONSTRUCTION.",
                 mono(6.5f, kSteel, 0.2f))
                   .shrink(0));
  }

  Element prodLine(const char *s, SkColor4f c) {
    return t(s, mono(8.0f, c, 0.2f)).height(10).shrink(0);
  }

  Element productionPanel() {
    return panel(96, 5)
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
    GlyphFx fx;
    fx.effect = glyphfx::rise(22);
    fx.stagger = {.eachMs = 26, .durationMs = 460};
    fx.progress = withFrom(0.0f, 1.0f,
                           {.duration = 850ms,
                            .ease = &ch::easeNone,
                            .delay = 120ms});
    return box()
        .column()
        .height(102)
        .shrink(0)
        .gap(4)
        .child(t("TECHNIQUE STUDY 03 \xc2\xb7 STOCHASTIC PARTICLE SYSTEMS",
                 ui(11.5f, kSteel, 2.7f))
                   .opacity(withFrom(0.0f, 1.0f, {.duration = 260ms}))
                   .translateY(withFrom(8.0f, 0.0f, {.duration = 260ms})))
        .child(t("THE GENESIS DEMO, 1982",
                 type(heavyFace(), 46, kBone, -0.4f))
                   .key("title")
                   .glyphFx(std::move(fx)))
        .child(t("W. T. Reeves, Lucasfilm Ltd \xe2\x80\x94 \"Particle "
                 "Systems: A Technique for Modeling a Class of Fuzzy "
                 "Objects\", SIGGRAPH '83 / ACM TOG 2(2) \xc2\xb7 sequence "
                 "dir. Alvy Ray Smith \xc2\xb7 Star Trek II, Paramount, "
                 "June 4, 1982",
                 ui(11.0f, kSteel, 0.1f))
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 240ms, .delay = 420ms})))
        .child(box().grow(1))
        .child(box()
                   .height(1)
                   .shrink(0)
                   .fill(kKeyline)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {.duration = 400ms, .delay = 320ms})));
  }

  Element describe() {
    return box()
        .column()
        .padding(36)
        .gap(24)
        .fill(kInk)
        .child(header())
        .child(box()
                   .row()
                   .gap(32)
                   .grow(1)
                   .child(stage())
                   .child(box()
                              .column()
                              .width(kSideW)
                              .shrink(0)
                              .gap(8)
                              .staggerChildren(90ms)
                              .child(generationPanel())
                              .child(censusPanel())
                              .child(rampPanel())
                              .child(renderModelPanel())
                              .child(productionPanel())));
  }

  /** The live census row. PropValue has no u8string case (ROADMAP §9), so
   *  a number that ticks goes through a slot — which is the right answer
   *  and is cheap, but is not what the call site reaches for first. */
  Element liveRow() {
    char parts_[32], per_[24], sys_[16];
    std::snprintf(parts_, sizeof parts_, "%zu,%03zu", liveCount / 1000,
                  liveCount % 1000);
    std::snprintf(per_, sizeof per_, "%d",
                  (int)std::lround((double)liveCount / (33.0 * kDepth)));
    std::snprintf(sys_, sizeof sys_, "33\xc3\x97" "%d", kDepth);
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

  /** The field's own measured cost, printed next to API.md's "10k
   *  instances in 0.18 ms on Graphite". Same slot idiom as the census
   *  row: a number that ticks has no binding path (ROADMAP §9). */
  Element fieldStat() {
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "FIELD: %zu,%03zu STREAKS \xc2\xb7 %zu,%03zu VERTS "
                  "\xc2\xb7 %zu drawVertices \xc2\xb7 BUILD %.2f ms / SIM "
                  "FRAME",
                  liveCount / 1000, liveCount % 1000, vertCount / 1000,
                  vertCount % 1000, fieldChunks.size(), buildUs / 1000.0);
    return t(buf, mono(8.5f, hex(0xFFB672, 0.85f), 0.5f))
        .textAlign(sigil::weave::TextAlignment::kEnd);
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas((int)kCanvasW, (int)kCanvasH);
    ctx.background(kInk);

    loopT = 0;
    stepped = false;
    elapsed = 0;
    simSteps = 0;
    liveCount = peakCount = 0;
    buildUs = 0;
    vertCount = 0;
    parts.clear();
    abParts.clear();
    fieldChunks.clear();

    // §4.1 — the 53 second-level systems, and THE STAGGER.
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
      sites.push_back(Site{{x, y}, n, {-n.fY, n.fX},
                           (x - kX0) / kSpread, vs});
    }

    seedStars();
    seedPlan();
    // (seedStars/seedPlan reseed rng; the site velocities were drawn
    //  above from the sketch's own stream.)
    seedBench();
    rng = 0x9E3779B9u;

    Composer &composer = ctx.composer;
    // §7.1 — the clock. [R83] counts lifetimes in FRAMES and the frames
    // in question are film frames, so the whole simulation runs at a
    // fixed 24 Hz whatever the host draws at. This used to be a
    // hand-rolled accumulator plus its own spiral-of-death clamp;
    // ROADMAP §12 closed while this sketch was being written and it is
    // now one line. The clamp matters here and is visible: a headless
    // pre-roll at `--fps 1` hands the ticker dt = 1.0, addFixed runs its
    // 8 steps and DROPS the other 16, so the sim lags the wall clock —
    // which is the correct failure, not a bug.
    ctx.ticker.addFixed(kSimHz, [this] {
      stepSim();
      stepped = true;
      return true;
    });
    ctx.ticker.add([this, &composer](double dt) {
      elapsed += dt;
      if (stepped) {
        stepped = false;
        const auto t0 = std::chrono::steady_clock::now();
        buildStreaks(parts, fieldChunks);
        const auto t1 = std::chrono::steady_clock::now();
        const double us =
            std::chrono::duration<double, std::micro>(t1 - t0).count();
        buildUs = buildUs > 0 ? buildUs * 0.85 + us * 0.15 : us;
        vertCount = parts.size() * 8;
        writeBenchPool();
        writePlanPool();
        composer.renderSlot("censusLive", liveRow());
        composer.renderSlot("fieldStat", fieldStat());
      }
      // ONE phase Output. bind() derives the wavefront in px, the plan
      // ring's unit scale, and two piecewise alphas from it (ROADMAP §1,
      // closed — this study would have carried four Outputs).
      loopU = (float)(loopT / kLoopSeconds);
      liveFrac = std::clamp(
          (float)(std::log10(std::max(1.0, (double)liveCount)) - 3.8) / 2.2f,
          0.0f, 1.0f);
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("censusLive", liveRow());
    ctx.composer.renderSlot("fieldStat", fieldStat());
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(GenesisFire)
