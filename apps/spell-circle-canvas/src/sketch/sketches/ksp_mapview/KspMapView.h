#pragma once

#include "Settings.h"

struct KspMapView : sketch::Sketch {
  using Out = ch::Output<float>;

  Out dashFast{0}, dashSlow{0};  // marching dots, two rates
  Out hubGlow{6.0f};             // sdf alive glow, quantised
  Out armPulse{1.0f};            // gizmo idle breathe
  Out jitterX{0}, jitterY{0};    // the "dragged" prograde arm
  Out yaw{0}, pitchOut{0}, rollOut{0};
  Out ringSpin{0};
  Out throttle{0.72f};  // 0..1, tape needle
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

  Element backdrop(sketch::SketchContext& ctx);

  // -------------------------------------------------------------------
  // Kerbin — a shaded 2D disc, no 3D anywhere in this sketch.

  Element planet();

  // -------------------------------------------------------------------
  // Orbit lines — three real conics.

  Element orbits(sketch::SketchContext& ctx);

  /** The target trajectory's own readout. It rides the arc at the point
   *  where the target body's icon sits on that same arc, so it is drawn
   *  ABOVE the bodies rather than inside the orbit layer beneath them: a
   *  map readout is HUD text, and a HUD is never occluded by the thing it
   *  is annotating. */
  Element targetLabel(sketch::SketchContext& ctx);

  /** Map marker: diamond + label, the Ap/Pe/AN/DN family.
   *
   *  `lift` is the label's offset from the diamond. It is a parameter and not
   *  a constant because the Ap/Pe arc label rides the orbit on the OUTSIDE at
   *  offset +8, and a node whose label also sits above its diamond prints
   *  straight through that run — "Ap 213,904DN m". Nodes that land in the arc
   *  label's band hang their label the other way. */
  Element marker(const char* label, SkPoint p, SkColor4f c, bool filled,
                 SkVector lift = {12, -9});

  // -------------------------------------------------------------------
  // The manoeuvre gizmo — the centre of the study.

  Element gizmo();

  // -------------------------------------------------------------------
  // Δv / burn readout card (the flight-view frame's active-node readout).

  Element burnCard();

  Element burnLines();

  // -------------------------------------------------------------------
  // Vessel info card — flat square corners everywhere, values in ORANGE.

  Element infoRow(const char* label, const char* value);
  Element infoHead(const char* label);

  Element infoCard();

  Element toolbar();

  // -------------------------------------------------------------------
  // Mission clock (top-left), matching the reference's pill + MET + icons.

  Element missionClock();

  // -------------------------------------------------------------------
  // Instrument cluster

  static constexpr SkPoint kBall{364, 668};
  static constexpr float kBallR = 84;
  static constexpr float kBezelR = 106;

  Element navball();

  Element staging();

  Element digitCell(const char* d);

  /** The top-centre altimeter block, straight off the flight-view frame:
   *  a hazard-striped left cheek, an odometer digit run ending in a red
   *  "K" cell, the blue ATMOSPHERE tape, and the round vertical-speed dial
   *  with its gold needle. Every part of it is a generator — hatch stripes,
   *  gradient plate, sector ticks, a rotated sector needle. */
  Element altimeter();

  /** The crew portrait plate, bottom-right in the flight-view frame. A
   *  deliberately abstract read of it: helmet sphere, visor sector, name
   *  bar — the composition, not the character art. */
  Element crewPlate();

  Element cluster();

  // -------------------------------------------------------------------

  /** A vessel/body chip on a trajectory: circular gunmetal disc, glyph,
   *  short label — the reference's "As" marker and the craft icon. */
  Element chip(const char* glyph, const char* label, SkPoint p, SkColor4f ink,
               float r);

  Element mapLayer(sketch::SketchContext& ctx);

  Element describe(sketch::SketchContext& ctx);

  // -------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override;

  void update(double elapsed, sketch::SketchContext& ctx) override;
};
