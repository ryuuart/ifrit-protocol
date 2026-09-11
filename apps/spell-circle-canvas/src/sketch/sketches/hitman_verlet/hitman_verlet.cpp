// hitman verlet: scene assembly and animation.

#include "HitmanVerlet.h"

auto HitmanVerlet::setup(sketch::DrawContext& ctx) -> void {
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
      kSimHz,
      [this] {
        stepPhysics();
        return true;
      },
      8, &alpha);

  headerEl = header();
  overlayEl = stageOverlay();
  barsEl = instancedHalf();
  colAEl = {panelA1(), panelA2(), panelA3()};
  colBEl = {panelB1(), panelB2(), panelB3()};

  ctx.pen.noStroke();
  ctx.pen.textAlign(draw::LEFT, draw::TOP);
}

auto HitmanVerlet::draw(sketch::DrawContext& ctx) -> void {
  Pen& pen = ctx.pen;
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
  pen.element(colAEl[0], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 0), kColW,
                                          kPanelAH[0]));
  pen.element(colAEl[1], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 1), kColW,
                                          kPanelAH[1]));
  pen.element(colAEl[2], SkRect::MakeXYWH(kColAX, panelTop(kPanelAH, 2), kColW,
                                          kPanelAH[2]));
  pen.element(colBEl[0], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 0), kColW,
                                          kPanelBH[0]));
  pen.element(colBEl[1], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 1), kColW,
                                          kPanelBH[1]));
  pen.element(colBEl[2], SkRect::MakeXYWH(kColBX, panelTop(kPanelBH, 2), kColW,
                                          kPanelBH[2]));
  // The holes: B1's heading is 12 tall over a 4 px gap, so the diagram
  // starts 30 px into the panel's padding box.
  const float holeX = kColBX + kPanelPad, holeW = kColW - 2 * kPanelPad;
  paintAnatomy(pen, holeX, panelTop(kPanelBH, 0) + kPanelPad + 12 + 4, holeW);
  paintChains(pen, holeX, panelTop(kPanelBH, 1) + kPanelPad + 12 + 4);
}

SIGIL_SKETCH(
    HitmanVerlet, "Study \xc2\xb7 Motion",
    "Jakobsen's Advanced Character Physics (GDC 2001) \xe2\x80\x94 motion "
    "with state and contact")
