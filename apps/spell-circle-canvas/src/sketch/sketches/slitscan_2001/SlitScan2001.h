#pragma once

#include "Settings.h"

struct SlitScan2001 : sketch::Sketch {
  // ---- the film clock: 24 Hz because the film runs at 24 fps -------------
  ch::Output<float> frameAlpha{0};
  sigil::motion::Ticker::FixedStatus fixedStatus;
  long long filmNo = 0;
  bool everClamped = false;

  // ---- the demonstration clock: one sweep per 3.0 s ----------------------
  double tau = 0.0, elapsed = 0.0;

  // ---- the machine -------------------------------------------------------
  /** THE TRANSFER CURVE, COMPILED ONCE AND HELD. describe() runs again on
   *  every shot cut, and a program compiled inside it is a compile per
   *  cut; a function-local static would be worse still, since it outlives
   *  the dylib a hot-reloaded sketch is unloaded with. */
  sk_sp<SkRuntimeEffect> transfer;
  std::shared_ptr<instancing::Atlas> atlas, flatAtlas;
  std::shared_ptr<instancing::Pool> wallA, wallB, monA, monB;
  std::array<std::shared_ptr<instancing::Pool>, 6> s4;
  std::array<slit::Strip, 3> strips;
  Pattern gridPat, spekPat;
  float artOffset = 0.0f;
  float plateDeg = 0.0f;
  int shot = 0;

  // ---- what the sketch measured about itself -----------------------------
  float fitP = 0, fitR2 = 0, fitResid = 0, fitP95 = 0;
  std::array<float, 120> profX{}, profY{};  // the measured profile
  int profN = 0;
  float fitPMin = 0, fitResidMin = 0;  // the same fit at K = K_min
  int fitRays = 0, fitPts = 0;
  float rtMaxErr = -1, rtCorr = 0, rtMismatch = 0;
  int rtCols = 0;
  int sheetW = 0, sheetH = 0;
  /** The bake's cost, pinned through `ctx.measured` at setup: a plate is
   *  a function of the declaration, and a number the sketch measured
   *  about its own execution is not. */
  double bakeMs = 0;
  // Set from the host's --deterministic flag: suppress any number this
  // study timed about its own execution, so two headless renders of the
  // same code are byte-identical. Left in, a wall-clock digit changes the
  // plate by itself and a pixel comparison blames the code under test.

  struct Shot {
    const char* name;
    SkColor4f gelA, gelB;
    float phi0, dPhi;
    int cell;
    const char* art;
  };
  /** 96 film frames = 4.0 s at 24 fps. The shot index is derived from the
   *  FRAME COUNTER, never from the wall clock: `addFixed` runs an exact
   *  number of steps whatever the draw rate, so anything accumulated inside
   *  that step must also be a function of the frame counter. Reading a
   *  value that update() writes at the DRAW rate instead would charge
   *  different fixed steps to different shots, and the plate's rotation
   *  would depend on how fast the host happened to be drawing. */
  static int shotFor(long long frame) {
    return (int)((uint64_t)(frame / 96) & 3u);
  }

  static const Shot& shotAt(int i);

  // The corridor banks; it does not sit still. Within +-96 x +-44 px.
  ch::Output<float> coreX{0}, coreY{0};  ///< the convergence, bound
  SkPoint vp() const {
    return {
        slit::kFilmW * 0.5f + 96.0f * (float)std::sin(elapsed * 0.41),
        slit::kFilmH * 0.5f + 44.0f * (float)std::sin(elapsed * 0.67 + 1.1)};
  }
  static float displayGain() { return 30.0f; }
  static float transferK() { return 1.35f; }

  // ------------------------------------------------------------- the walls
  void rebuildWalls();

  // ==================================================================== 12-D
  // MEASURE the exposure profile; do not assume it. Nothing in the library
  // can read back what it drew -- Debug.h is entirely path-level, every
  // check it offers is a check on geometry you already hold -- so this
  // builds the raster by hand: snapshot the accumulation subtree, replay it
  // into an F16 surface (F16 because the outer streak is 1/120 of the apex
  // and eight bits would quantise it to two levels), and readPixels.
  //
  // Measured at UNIT GAIN through a UNIFORM slit cell: the LAW, not the
  // picture. The gain the frame uses clips the apex on purpose -- that IS
  // the film's shoulder -- and the artwork modulates the envelope, which is
  // verification E's business. Measuring the drawn frame would measure all
  // three at once and prove none of them.
  struct Fit {
    float p = 0, r2 = 0, worst = 0, p95 = 0;
    int rays = 0, pts = 0;
    std::array<double, 120> sum{};
    std::array<int, 120> cnt{};
  };

  Fit fitAtK(sigil::weave::FontContext& fonts, int K);

  void measureExposure(sigil::weave::FontContext& fonts);

  // ==================================================================== 12-E
  // ERCOLANO'S UNWRAP, RUN FORWARDS. He accumulated one scanline per DVD
  // frame and got the artwork back for four production shots. This feeds a
  // known strip in, accumulates one film column per frame across 96 frames,
  // concatenates them, and compares against what went in.
  //
  // The radius is u* = X0*ppi = 504.3 px, where the stamp is EXACTLY the
  // window's baked size -- 6 x 492 px, 1:1 on both axes. At any other radius
  // the residual would be measuring nearest-neighbour resampling phase; at
  // this one it measures TRANSPORT, which is the claim under test.
  void roundTrip(sigil::weave::FontContext& fonts);

  // =====================================================================
  // Header

  Element header();

  // ---------------------------------------------------------- the film frame
  Element filmFrame();

  // ------------------------------------------------------------- the rig
  void drawRig(SkCanvas& c, const PaintContext& ctx);
  void drawArtworkPanel(SkCanvas& c, const PaintContext& ctx);
  void drawMeasuredPoints(SkCanvas& c, const PaintContext& ctx);

  Element rigStrip();

  // ------------------------------------------------------------- sidebar
  /** Every text child inside a panel is shrink(0): a flex column will
   *  otherwise squash a text leaf below its measured height and the run
   *  silently overlaps its neighbour. This is the height-axis form of the
   *  same rule that lets a fixed width() flex child still shrink. */
  Element pl(const std::string& str, weave::TextStyle st) {
    return slit::t(str, std::move(st)).shrink(0);
  }

  Element panelShell(const char* heading, int order);

  Element s1Quote();

  Element s2Lens();

  Element s3Law();

  Element s4Sampling();

  Element sidebar();

  // ---------------------------------------------------------- live readouts
  Element readoutEl();
  Element expoEl();
  Element fitEl();
  Element rippleEl();

  // ---------------------------------------------------------------- describe
  Element describe(sketch::SketchContext& ctx);

  void setup(sketch::SketchContext& ctx) override;
  void update(double e, sketch::SketchContext& ctx) override;
};
