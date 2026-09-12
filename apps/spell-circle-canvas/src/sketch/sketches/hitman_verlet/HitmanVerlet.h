#pragma once

#include "Settings.h"

struct HitmanVerlet final : sketch::Sketch {
  // -------------------------------------------------------------------------
  // §4 — the mechanism

  struct Body {
    /** The point set: `position` is x, `previous` is x*, and a particle
     *  the paper constrains to origo is one this set holds pinned. */
    physics::Points pts;
    /** The sticks, and the one inequality. Both are the distance band
     *  solved by the square-root-free form — the arithmetic this study is
     *  about, stated once where every simulation can reach it. */
    std::vector<physics::Constraint> sticks;
    std::vector<physics::Constraint> ineqs;
    int iterations = 1;
    bool worldCollide = false;
    float radius = 0;           // capsule radius, world units
    bool stickContact = false;  // also run the Sec.5 barycentric fix-up

    // The two lanes read and written in this file's own units, which are
    // Skia's points; the simulation carries them in the motion library's.
    [[nodiscard]] size_t count() const { return pts.size(); }
    [[nodiscard]] SkPoint at(size_t i) const {
      return {pts.position[i].x, pts.position[i].y};
    }
    [[nodiscard]] SkPoint was(size_t i) const {
      return {pts.previous[i].x, pts.previous[i].y};
    }
    void moveTo(size_t i, SkPoint p) { pts.position[i] = p; }
    void wasAt(size_t i, SkPoint p) { pts.previous[i] = p; }
    [[nodiscard]] float invm(size_t i) const { return pts.inverseMass(i); }
    /** A particle at rest at @p p; `held` is the paper's constraint to
     *  origo, an infinite mass by another road. */
    void add(SkPoint p, bool held = false) { pts.add(p, {}, 1.0f, held); }
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

  // The stepper beside this one in SigilMotion carries a velocity lane
  // and walks the constraint list itself. This study's subject is the
  // position-only form and a relaxation loop with the world projection
  // interleaved, so the step is written here; the sticks and the
  // inequality are the library's.
  static void verlet(Body& b, SkPoint gravityStep);

  // The world: the paper's cube plus one triangle. Returns the nearest
  // legal position for a point of radius `r`, or the point itself.
  static bool projectWorld(SkPoint p, float r, SkPoint* q);

  static SkPoint nearestOnSegment(SkPoint p, SkPoint a, SkPoint b);
  static SkPoint nearestOnTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c);
  static bool pointInTriangle(SkPoint p, SkPoint a, SkPoint b, SkPoint c);
  static float cross2(SkPoint a, SkPoint b) {
    return a.fX * b.fY - a.fY * b.fX;
  }

  /** §4.3(a) — projection, with §7 friction. The penetration depth is
   *  measured BEFORE the projection, or friction has nothing to scale by. */
  void collideWorld(Body& b, bool record);

  /** §5 — the capped cylinder against the world. The contact point lies ON
   *  the stick, so it is a barycentric blend and the correction
   *  distributes by the paper's own formula. Written as printed. */
  void collideSticks(Body& b, bool record);

  /** §4.3 — SatisfyConstraints. Projection lives INSIDE the relaxation
   *  loop, so a stick fix-up that shoves a particle back into the floor is
   *  projected out again on the next iteration. That interleaving IS the
   *  algorithm. */
  void satisfy(Body& b, bool record);

  // =========================================================================
  // Construction

  static SkPoint world(Norm n, float side, SkPoint origin) {
    return {origin.fX + n.x * side * kFigureH, origin.fY + n.y * kFigureH};
  }

  void addStick(Body& b, int i, int j) {
    const float r = len(b.at((size_t)j) - b.at((size_t)i));
    b.sticks.push_back(physics::stick((size_t)i, (size_t)j, r));
  }

  void buildRig(SkPoint feet);

  void buildCloth();

  /** §4 — "By placing support sticks between strategically chosen couples
   *  of vertices sharing a neighbor, the cloth algorithm can be extended to
   *  simulate plants." Read literally as a ONE-dimensional chain it cannot
   *  work, and this is worth stating: a straight chain hanging DOWNWARD
   *  satisfies every distance constraint it has, including every skip-one
   *  support, so gravity simply inverts it. A plant is a narrow strip of
   *  the CLOTH — two columns, every quad carrying a diagonal — pinned at
   *  its base row. That is a truss, and it stands. */
  void buildPlants();

  // The A/B chains live in PANEL pixels — their gravity is a panel
  // constant, not the world's.
  static constexpr float kChainRest = 12.0f;
  static constexpr float kChainG = 0.11f;
  void buildChains();

  // =========================================================================
  // §7 — motion control, documented as laws, reconstructed as numbers.

  void applyBlast(Body& b, SkPoint c);

  float maxError(const Body& b) const;
  void chainStats(const Body& b, float* mean, float* mx) const;

  // =========================================================================
  // The fixed step. Every constant above is per-60-Hz-step.

  void stepPhysics();

  // =========================================================================
  // Drawing. World y is UP, stage y is DOWN.

  static SkPoint toStage(SkPoint w) {
    return {w.fX * kUnit, (1000.0f - w.fY) * kUnit};
  }
  /** The integrator's OWN interpolant: a verlet body's state IS (x*, x),
   *  so lerp(x*, x, alpha) is the position at t_prev + alpha*dt. Free. */
  SkPoint drawnWorld(const Body& b, size_t i) const {
    const float a = alpha.value();
    return b.was(i) + (b.at(i) - b.was(i)) * a;
  }
  SkPoint drawn(const Body& b, size_t i) const {
    return toStage(drawnWorld(b, i));
  }

  /** The rig's stage-space bounds, so the two A/B thumbnails can FIT it
   *  rather than assume where it is. */
  SkRect rigBounds() const;
  /** Fit-to-cell: the transform that maps rigBounds() into a w x h cell. */
  void rigFit(float w, float h, float* scale, SkPoint* offset) const;

  /** The corpse's 24 sticks, three layers, all in the pen's own words on
   *  geometry recomputed this frame. A ROUND CAP AT TWICE THE CAPSULE
   *  RADIUS IS A CAPPED CYLINDER — the collision proxy the paper
   *  describes, made visible by the stroke that already draws it. */
  void paintRig(Pen& pen, float scale, SkPoint offset, float fade,
                bool proxies) const;

  // =========================================================================
  // The particle pool (Mode::Live) — the control case.

  void writeDotPool();

  /** The SECOND path for the SAME 24 sticks: one atlas cell (a bar with
   *  BUTT ends), rotations() = atan2(dy, dx), and the sizes() lane carrying
   *  length and thickness independently — which one uniform per-instance
   *  scale cannot express, since every stick needs a different length at
   *  the same width. */
  void writeBarPool(SkPoint origin, float scale);

  // =========================================================================
  // The stage, in pen verbs

  void stageChrome(Pen& pen, double ms);

  void worldBox(Pen& pen, double ms);

  /** The corpse, the cloth, the plants and every contact this frame. */
  void simulation(Pen& pen);

  /** The retained overlay: every particle dot through instances(Mode::Live)
   *  — the control case beside the pen's own stroking. */
  Element stageOverlay();

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
  void blastGlow(Pen& pen);

  /** The stage's own labels, and the note block over the world box. */
  void stageLabels(Pen& pen);

  /** Fig. 4b/5b: the paper's own worked case, c1 = 0.75, c2 = 0.25, and
   *  Fig. 10's friction diagram beside it. Both are drawings with four
   *  lines of type under them, so both are the pen's. */
  void figPenetration(Pen& pen, float x0, float y0, float a);

  void figFriction(Pen& pen, float x0, float y0, float a);

  /** A stage inset: the scrim over the world. */
  void inset(Pen& pen, float x, float y, float w, float h, float a);

  /** The A/B strip's chrome, with the instanced half riding in as a guest
   *  and the pen drawing the same 24 sticks beside it. */
  Element instancedHalf() {
    return box().inset(0).child(
        instancing::instances(barAtlas, barPool, instancing::Mode::Live));
  }

  void instancingStrip(Pen& pen, float x0, float y0, float a);

  /** The ramp's five steps and what they stand for, IN THE PEN'S WORDS.
   *  Every key on this canvas is drawn rather than composed: the stage is
   *  one pen program over a live solver, and a component hands back an
   *  Element, which would want a second surface over the one being
   *  painted. */
  void errorLegend(Pen& pen, float x0, float y0, float a);

  // =========================================================================
  // Sidebar — the six panels. Three of them are laid out with a HOLE the
  // pen draws into afterwards.

  Element codeLine(const char* s, SkColor4f c, bool caret = false);

  Element panelA1();

  Element panelA2();

  Element panelA3();

  /** B1's tree leaves a 118 px hole under its heading; the anatomy
   *  diagram is drawn into it by the pen, because it is the rest pose of
   *  the same rig the stage is simulating. */
  Element panelB1();

  void paintAnatomy(Pen& pen, float x0, float y0, float w);

  /** B2's tree leaves a 156 px hole for the three chains and 34 px for
   *  their live numbers; both are the solver's, so both are the pen's. */
  Element panelB2();

  void paintChains(Pen& pen, float x0, float y0);

  Element panelB3();

  // =========================================================================

  Element header();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  /** THE LOOP, run by the pen of the node the study stands in: one frame
   *  of the canvas that node keeps. */
  void draw(Pen& pen);
};
