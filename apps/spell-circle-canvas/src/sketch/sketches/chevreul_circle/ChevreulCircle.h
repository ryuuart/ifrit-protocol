#pragma once

#include "Settings.h"

struct ChevreulCircle : sketch::Sketch {
  static constexpr float kW = 1800.0f;
  static constexpr float kH = 1200.0f;

  // the wheel
  static constexpr SkPoint kC{436, 500};
  static constexpr float kRColour = 292.0f;  // outer edge of the colour band
  static constexpr float kInner = 0.34f;     // the paper medallion
  // The paper between two blades, in degrees of the 5 deg sector pitch.
  static constexpr float kBladeGapDeg = 1.25f;
  // How far the reconstruction is lifted toward the plate ON A WALL. The
  // medians are read off one photograph with the paper divided out in
  // linear light, which is a defensible reconstruction and reads a stop
  // duller than the engraving does under gallery light; this is a chroma
  // gain about each blade's own luminance, and it is the only place the
  // measured numbers are departed from.
  static constexpr float kWallLift = 1.22f;
  // The three surface passes, as dials rather than as environment reads: a
  // scene that describes itself differently depending on what is in the
  // environment cannot be photographed reproducibly.
  static constexpr bool kPlateTone = true;
  static constexpr bool kLimb = true;
  static constexpr bool kPaperGrain = true;
  /// Re-describe every frame, to price what cannot prune.
  static constexpr bool kRedescribe = false;
  static constexpr float kRLimbIn = 296.0f;
  static constexpr float kRLimbOut = 328.0f;
  static constexpr float kRSweepIn = 333.0f;  // the continuous-sweep ring
  static constexpr float kRSweepOut = 344.0f;

  // the staircases
  static constexpr float kBandW = 44.0f;
  static constexpr int kBandN = 20;
  static constexpr float kStairX = 860.0f;  // 20 x 44 = 880, integer, exact
  static constexpr float kStairYA = 566.0f;
  static constexpr float kStairYB = 646.0f;
  static constexpr float kStairYC = 726.0f;
  static constexpr float kStairH = 66.0f;

  // the quadrant
  static constexpr float kQX = 96.0f, kQY = 950.0f;
  static constexpr float kQCellW = 68.0f, kQCellH = 6.0f;
  static constexpr float kQGapX = 2.0f, kQGapY = 1.0f;

  std::array<SkColor4f, 72> corrected{}, scanned{};
  std::array<Lab, 72> lab{};
  std::array<SkColor4f, 20> gamme{}, gammeCode{};

  ch::Output<float> demo{0};
  Verdict v;
  measure::Table verdict;
  std::string derivation1, derivation2;
  std::string counterText;

  Paint paperGrain, plateTone, sweepRing, medallionGlow;
  std::shared_ptr<weave::Paragraph> lawPara;

  std::shared_ptr<instancing::Atlas> quadAtlas;
  std::shared_ptr<instancing::Pool> quadPool;
  int quadFrame = 0;

  // ==================================================================
  // verification

  /** An unpremultiplied channel as the 8-bit code a screen is handed —
   *  what every read-back claim on this plate is stated in. */
  static int code(float v) {
    return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
  }

  /** The measured colour, lifted toward what the engraving looks like on
   *  a wall: chroma scaled about the colour's own luminance, so the hue
   *  and the value the medians establish are both left alone and only the
   *  distance from grey changes. */
  static SkColor4f wallLift(SkColor4f c);

  void computeColours();

  void verify(sketch::SketchContext& ctx);

  void buildQuadrantPool();

  /** THE VERIFICATION, as one table. Every row's printed line is COMPUTED
   *  from the two values it reports, so a claim and its evidence cannot
   *  drift apart the way a hand-typed "OK" can. The rows that state
   *  something about CHEVREUL rather than about this reconstruction — his
   *  §160 luminosity claim, his §6 complementaries against his own §161
   *  construction — are findings: their verdict is printed and never
   *  counted against the run, because a finding that fails is the result.
   *  The one row that is a measurement with nothing to judge is a
   *  reading. */
  void buildVerifyTable();

  void buildLaw();

  // ==================================================================
  // the plate

  Element theHeader();

  Element theWheel(sketch::SketchContext& ctx);

  // ------------------------------------------------------------------
  Element theQuadrant();

  // ------------------------------------------------------------------
  Element theLabPlot();

  Element chordCounter();

  // ------------------------------------------------------------------
  Element theObservations();

  // ------------------------------------------------------------------
  /** One twenty-band ramp. @p graded puts the OCIO view on EACH BAND
   *  rather than on the group: a group of absolutely-placed children has
   *  no size of its own, so an effect there opens a layer the size of the
   *  canvas and runs the LUT over every pixel of the plate every frame.
   *  Twenty bounded layers of one band each are the same picture. */
  Element aStaircase(const std::array<SkColor4f, 20>& ramp, float y, float h,
                     const char* keyBase, bool withGap, bool graded = false);

  Element theIllusion();

  // ------------------------------------------------------------------
  Element theContrast();

  // ------------------------------------------------------------------
  Element theVerification();

  // ==================================================================
  Element describe(sketch::SketchContext& ctx);

  // ==================================================================
  void setup(sketch::SketchContext& ctx) override;

  int frames = 0;

  void update(double, sketch::SketchContext& ctx) override;
};
