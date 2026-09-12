// chevreul circle: scene assembly and animation.

// TAGS: Materials/Color

#include "ChevreulCircle.h"

auto ChevreulCircle::describe(sketch::SketchContext& ctx) -> Element {
  // The plate's sheet and its classes stand for everything described below
  // it, so a kit component four levels down is set in the plate's ink
  // without being handed it; the root states the voice every line inherits.
  const sketch::kit::Provide look(sheet(), classes());
  Element root = stack()
                     .width(Dimension(kW))
                     .height(Dimension(kH))
                     .font({.face = mono()})
                     .ink(kInk2);

  // the leaf: measured paper, its tooth, and the platemark
  root.child(at(0, 0, kW, kH).fill(Fill::color(kPaper)));
  if (kPaperGrain)
    root.child(at(0, 0, kW, kH)
                   .fill(paperGrain)
                   .blend(SkBlendMode::kMultiply)
                   .opacity(0.085f)
                   .cache(Cache::Texture));  // 1800x1200 of generated material
  root.child(
      at(28, 28, kW - 56, kH - 56)
          .fill(Fill::none())
          .foreground(stroke(1.0f, Fill::color(hexColor(0x8C8578, 0.55f)))));

  root.child(theHeader());
  root.child(theWheel(ctx));
  root.child(theQuadrant());
  root.child(theLabPlot());
  root.child(theObservations());
  root.child(theIllusion());
  root.child(theContrast());
  root.child(theVerification());

  root.child(
      label(
          "COLOURS MEASURED FROM SCIENCE HISTORY INSTITUTE ND1280 .C497 1864, "
          "PL. V, 2880×3789 · PAPER WHITE #EFE8D9 DIVIDED OUT IN LINEAR "
          "LIGHT · CONSTRUCTION AFTER CHEVREUL §6, §16, §160–§165 · TRANS. "
          "C. MARTEL · NO OUTPUT VIEW TRANSFORM IS SET, DELIBERATELY",
          56, 1168, 1690)
          .font({.size = 8, .track = 0.55f}));
  return root;
}

auto ChevreulCircle::setup(sketch::SketchContext& ctx) -> void {
  // The still has to name its moment: this is a 14 s loop (13 s reveal +
  // 1 s hold), and 12.6 s is fully settled with 1.4 s of margin before the
  // reset. An undeclared capture catches the plate roughly half-built,
  // with most verification rows and later panels unrevealed.
  sketch::kit::stage(
      ctx,
      {.size = SkSize::Make(kW, kH), .captureAt = 12.6, .background = kPaper});

  computeColours();

  // materials held as members so their identity survives re-describes
  paperGrain = Paint::recipe(field::grain(0.013f, 4, 11.0f, 0.32f));
  plateTone = Paint::recipe(field::grain(0.085f, 3, 5.0f, 0.45f));

  // the 72 measured values as ONE gradient: 144 stops, doubled so the
  // steps stay franches rather than blending into each other.
  {
    // SkShaders::SweepGradient measures from +x clockwise over
    // [startDeg, endDeg] and CLAMPS outside it, so the ring is authored in
    // the gradient's own 0..360 frame rather than by rotating the range.
    // Sector n spans screen angles [90 − 5(n+0.5), 90 − 5(n−0.5)], so the
    // band boundaries land at (2.5 + 5j)/360 and the band below boundary j
    // is sector (18 − j) mod 72.
    std::vector<sigil::material::skia::Stop> stops;
    stops.reserve(146);
    auto C = [&](int n) { return corrected[(size_t)(((n % 72) + 72) % 72)]; };
    stops.push_back({0.0f, C(18)});
    for (int j = 0; j < 72; ++j) {
      const float p = (2.5f + 5.0f * (float)j) / 360.0f;
      stops.push_back({p, C(18 - j)});
      stops.push_back({p, C(17 - j)});
    }
    stops.push_back({1.0f, C(18)});
    sweepRing =
        Paint::sweep({kRSweepOut, kRSweepOut}, std::move(stops), 0.0f, 360.0f);
  }

  verify(ctx);
  buildVerifyTable();
  buildLaw();

  // one Output, 0 -> 1 over 13.0 s, then a 1.0 s hold, then loop.
  ctx.ticker.add([this, &ticker = ctx.ticker](double) {
    const double t = ticker.elapsed();
    const double u = std::fmod(t, 14.0);
    demo = (float)std::clamp(u / 13.0, 0.0, 1.0);
    // §163's quadrant turning on the circle's axis: one column every
    // 90 ms through beat 6. Mode::Live reads the pool every frame.
    if (quadPool) {
      const float d = demo.value();
      auto tints = quadPool->tints();
      for (int k = 0; k < 10; ++k) {
        const float lo = 0.80f + 0.012f * (float)k;
        const float a = std::clamp((d - lo) / 0.010f, 0.0f, 1.0f);
        for (int r = 0; r < 20; ++r) tints[(size_t)k * 20 + (size_t)r].fA = a;
      }
    }
    return true;
  });

  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("chordcount", chordCounter());
}

auto ChevreulCircle::update(double, sketch::SketchContext& ctx) -> void {
  // Cost, measured rather than guessed: CHEVREUL_STATS=1 dumps the
  // composer's own per-phase timings for a few frames. The two numbers
  // this piece cares about are picturesLive (72 static flat fills plus
  // 78 onPath runs) and paintMs, since a TextPath carries no operator==
  // and therefore cannot prune.
  // kRedescribe re-describes the whole plate every frame, which is what
  // prices the un-prunable nodes: TextPath has no operator== by design,
  // so the 78 limb runs re-record on every render(). Set kLimb false for
  // the other half of the comparison.
  if (kRedescribe) ctx.composer.render(describe(ctx));
  ++frames;

  // The counting numbers of beat 2. Animatable covers floats, colours and
  // fills but not text, so a counter is a renderSlot() — done here rather
  // than by re-describing the plate, so every other cache stays valid.
  const float d = demo.value();
  const float u = std::clamp((d - 0.20f) / 0.09f, 0.0f, 1.0f);
  const std::string next = kit::formatted(
      "36 chords miss the ORIGIN by %.2f · the CENTROID by %.2f  "
      "(%.1f%%)",
      v.missOrigin * u, v.missCentroid * u, v.missPercent * u);
  if (next != counterText) {
    counterText = next;
    ctx.composer.renderSlot("chordcount", chordCounter());
  }
}

SIGIL_SKETCH(
    ChevreulCircle, "Study \xc2\xb7 Science",
    "Chevreul's 1er cercle chromatique, Plate V, 1864 \xe2\x80\x94 a study "
    "whose content is a palette")
