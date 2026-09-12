// The Genesis wall of fire: a fixed-step particle simulation drawn as luminous
// streaks.

#include "GenesisFire.h"

void GenesisFire::setup(sketch::SketchContext& ctx) {
  // 4.6 s into the 10 s loop: the wavefront is near the right edge, the
  // leftmost systems are burning out and the rightmost have just ignited.
  ctx.canvas(kCanvasW, kCanvasH);
  ctx.background(kInk);
  ctx.captureAt(4.6);

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
  rng.reseed(0x0F1E2D3Cu);
  sites.clear();
  for (int i = 0; i < kSiteCount; ++i) {
    const float x = kX0 + kSiteStep * (float)i;
    const float y = limbY(std::clamp(x, 0.0f, kStageW));
    SkVector n{x - kLimbCx, y - kLimbCy};
    const float l = n.length();
    n = {n.fX / l, n.fY / l};
    // one mean-velocity draw per system, fixed for its whole life
    const float vs = 0.80f + 0.42f * rng.unit();
    sites.push_back(Site{{x, y}, n, {-n.fY, n.fX}, (x - kX0) / kSpread, vs});
  }
  mouths.clear();
  for (int i = 0; i < kSiteCount; ++i)
    mouths.push_back(mouthFor(sites[(size_t)i], i, 1.0f, 1.0f, 1.0f));
  setRates(parts, 1.0f);
  abMouth = mouthFor(abSite(), 0, 0.55f, 0.72f, 0.35f);
  setRates(abParts, 0.72f);

  seedStars();
  seedPlan();
  // (seedStars/seedPlan reseed rng; the site velocities were drawn
  //  above from the sketch's own stream.)
  seedBench();
  rng.reseed(0x9E3779B9u);

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

  deterministic = ctx.deterministic;
  ctx.composer.render(
      graphics("genesis_fire.loop", [this](Pen& pen) { draw(pen); })
          .absolute()
          .inset(0));
}

void GenesisFire::draw(Pen& pen) {
  if (pen.frameCount == 1) {
    pen.noStroke();
    pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
  }
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
      (float)(std::log10(std::max(1.0, (double)liveCount)) - 3.8) / 2.2f, 0.0f,
      1.0f);

  pen.background(kInk);
  pen.element(headerEl,
              SkRect::MakeXYWH(kPad, kPad, kCanvasW - 2 * kPad, kHeaderH));

  // --- the stage: two guests with the field drawn between them --------
  const SkRect stageBox = SkRect::MakeXYWH(kStageX, kBodyY, kStageW, kStageH);
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
      pen.rect(stageBox.x(), stageBox.y(), stageBox.width(), stageBox.height());
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
  pen.stroke(
      hexColor(0x242A36, cue(pen.millis(), 260, 520, &ch::easeOutCubic)));
  pen.strokeWeight(1.5f);
  pen.rect(kStageX + 0.75f, kBodyY + 0.75f, kStageW - 1.5f, kStageH - 1.5f);
  pen.noStroke();

  // --- the caption band, declared OUTSIDE the artefact ----------------
  stageCaption(pen);

  // --- the sidebar: five panels, each its own guest --------------------
  pen.element(genEl, SkRect::MakeXYWH(kSideX, panelTop(0), kSideW, kPanelH[0]));
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

void GenesisFire::stageCaption(Pen& pen) {
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
                // renders of the same frame. `measured` reads zero
                // where the host is capturing for a diff, which is what
                // makes a captured still comparable byte for byte.
                measured(buildUs / 1000.0));
  const float right = kStageX + kStageW;
  pen.textAlign(sigil::draw::RIGHT, sigil::draw::TOP);
  penMono(pen, 8.5f, fadeTo(hexColor(0xFFB672, 0.85f), a), 0.5f);
  pen.text(buf, right, kCaptionY);
  penMono(pen, 8.5f, fadeTo(kSteel, a), 0.5f);
  pen.text(
      "888\xc3\x97"
      "666 = 4:3 \xe2\x80\x94 THE 500-LINE VIDEO RASTER THE DEMO WAS "
      "COMPUTED FOR",
      right, kCaptionY + 12);
  penMono(pen, 8.5f, fadeTo(hexColor(0xFF8A3A, 0.75f), a), 0.5f);
  pen.text(
      "WARM GROUND LIGHT RIDING THE FRONT = TOM DUFF'S LOCAL LIGHT, "
      "THE ONLY HAND-PLACED LIGHT IN THE SHOT",
      right, kCaptionY + 24);
  pen.textAlign(sigil::draw::LEFT, sigil::draw::TOP);
}

SIGIL_SKETCH(
    GenesisFire, "Study \xc2\xb7 Motion",
    "The Genesis Demo wall of fire (Lucasfilm, 1982) \xe2\x80\x94 the first "
    "particle system")
