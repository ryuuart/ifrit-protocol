#pragma once

#include "Settings.h"

struct GenesisFire final : sketch::Sketch {
  /** The host is capturing for a diff, so a figure this study took off
   *  its own execution is pinned. Read from the context while declaring
   *  and kept, because the caption is drawn by a pen program and a pen
   *  program is handed no context. */
  bool deterministic = false;
  /** A number measured about this study's own execution: @p value
   *  normally, @p pinned under a capture that will be compared, since a
   *  plate carrying a build time no two runs agree on differs from
   *  itself. */
  [[nodiscard]] double measured(double value, double pinned = 0.0) const {
    return deterministic ? pinned : value;
  }

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
  std::vector<Site> sites;
  // One emitter per second-level system, and the cloud they all throw
  // into. The extinction rules below are this study's own.
  std::vector<physics::Emitter> mouths;
  physics::Particles parts;
  std::vector<sk_sp<SkVertices>> fieldChunks;

  // --- the A/B bench: one explosion, three renderers -----------------------
  physics::Emitter abMouth;
  physics::Particles abParts;
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
  /** [R83 §2.1]: "Rand is a procedure returning a uniformly distributed
   *  random number between -1.0 and +1.0" — `signedUnit`, and `unit` for
   *  the half of it an angle and a coin toss want. */
  chance::Stream rng = chance::Stream::xorshift(0x9E3779B9u);

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
  // One second-level system, as an emitter

  /** THE GENERATION SHAPE AND THE SEVEN ATTRIBUTES, as one value: the
   *  mouth is [R83 §2.2]'s generation segment about the surface point,
   *  the aim is the outward normal and the cone is the ejection cone,
   *  which a slice through the paper's axisymmetric inverted cone reads
   *  as uniform in psi rather than area-uniform on a disc. The three
   *  scales are the dial a smaller copy of the same explosion turns. */
  static physics::Emitter mouthFor(const Site& s, int index, float speedScale,
                                   float sizeScale, float genScale);

  /** The rates a cloud's attributes change at: [R83 §2.3]'s size change,
   *  global to the system, and the colour decay whose ORDERING is
   *  documented ("green and blue dropped off quickly, and the red
   *  followed at a slower rate") while the rates are reconstruction. */
  static void setRates(physics::Particles& cloud, float sizeScale);

  /** Per-frame integration + the three documented extinction rules. The
   *  acceleration is the system's own outward normal reversed, which is
   *  what makes the arcs parabolic about a curved surface rather than
   *  about one flat down. */
  void advance(physics::Particles& cloud, const std::vector<Site>& ss,
               float gravity, bool killBelowSurface);

  void stepSim();

  // The bench's single system, in the local px of its 130x52 cell — the
  // emitter sits 2 px above the cell's bottom edge, so the whole arc stays
  // inside the clip and the below-emitter cull (2 px under the emitter, in
  // advance()) lands exactly on the cell's edge.
  /** HELD ON THE SKETCH, not in a function-local static: a static with a
   *  dynamic initialiser registers its destructor with the process, and a
   *  hot-reloaded sketch's dylib is unloaded out from under it. */
  const std::vector<Site> abSites{
      Site{{65.0f, 50.0f}, {0.0f, -1.0f}, {1.0f, 0.0f}, 0.0f}};
  const Site& abSite() const { return abSites[0]; }

  // =========================================================================
  // The renderer that defines the look.
  //
  // ONE additive pass over every living particle, no depth sort, no
  // occlusion. Each streak is a quad from pos(f + 1/2) to pos(f) — a
  // 1/50 s shutter at 24 fps is half the inter-frame motion — six
  // vertices wide so the cross-section falls off (drawVertices does not
  // antialias its own edges, so the falloff lives in the vertex colours).
  // ADD is the whole colour model: light ADDS and the buffer CLAMPS.

  void buildStreaks(physics::Particles& cloud,
                    std::vector<sk_sp<SkVertices>>& out);

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
                         float alpha);

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
  void writeBenchPool();

  /** Fig. 2's 149 ring marks: per-instance frame + tint rewritten every
   *  tick as the wavefront passes. The textbook Mode::Live case. */
  void writePlanPool();

  // =========================================================================
  // The two control pools, laid out once

  void seedStars();

  void seedPlan();

  void seedBench();

  // =========================================================================
  // Stage — the retained furniture, under the field and over it

  Element starField();

  struct DipStar {
    const char* name;
    float u, v, mag;
  };

  Element dipper();

  Element regolith();

  Element shockwave();

  /** Everything under the field: the sky, the stars, the Dipper, the
   *  regolith and the shockwave, in one guest the size of the stage. */
  Element stageBelow();

  Element planInset();

  /** Everything over the field: Fig. 2's live plan inset and its caption.
   *  Both are retained — a pool written every tick and a line of type on
   *  a curved baseline are a described tree's business. */
  Element stageAbove();

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
  void blurCallout(Pen& pen, float x0, float y0, float w, float h, float a);

  static SkColor4f fadeTo(SkColor4f c, float a) {
    return {c.fR, c.fG, c.fB, c.fA * a};
  }

  // =========================================================================
  // Sidebar

  Element eqn(const char* s) {
    return t(s, mono(11.0f, kBone, 0.1f)).height(16).shrink(0);
  }

  Element generationPanel();

  Element censusCell(const char* s, float w, weave::TextStyle st) {
    return t(s, std::move(st)).width(w).shrink(0);
  }

  /** A ROW'S SHARE OF THE LARGEST CENSUS, as a bar SCALED from its left
   *  edge rather than sized: the sidebar's bed keeps its recording, and
   *  the live row's bar moves every frame off one bound Output where a
   *  width would be layout every frame. The entrance eases past its own
   *  fraction and the rail clips, which is what keeps a bar that
   *  overshoots inside its own track. The live row names its bar, since
   *  that row is described again every frame. */
  Element censusBar(float frac, SkColor4f c, const char* key);

  Element censusRow(const char* fig, const char* sys, const char* particles,
                    const char* per, float frac, bool live);

  /** The live census row. The numbers tick, so the row is re-described
   *  every frame and reconciled against what the guest's own composer
   *  already holds: the reconcile is what keeps a ticking number from
   *  re-recording the panel around it. */
  Element liveRow();

  Element censusPanel();

  Element rampPanel();

  Element benchCell(Element content, const char* caption, SkColor4f cc);

  /** The bench's third cell is EMPTY in the tree: the pen draws the quads
   *  into it afterwards, at the cell's own box, because those quads are
   *  the field's renderer and the field is the pen's. */
  Element renderModelPanel();

  Element prodLine(const char* s, SkColor4f c) {
    return t(s, mono(8.0f, c, 0.2f)).height(10).shrink(0);
  }

  Element productionPanel();

  // =========================================================================

  Element header();

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override;

  /** THE LOOP, run by the pen of the node the study stands in: one frame
   *  of the canvas that node keeps. */
  void draw(Pen& pen);

  /** THE CAPTION BAND. It states what the study proves — how many streaks
   *  are alive, the raster the demo was computed for, and which light in
   *  the picture is hand-placed — and it is drawn OUTSIDE the stage,
   *  because a plate of an artefact is a plate of the artefact. Nothing
   *  here is a mark on the frame; it is a band under it. */
  void stageCaption(Pen& pen);

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
