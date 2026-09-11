// ksp mapview: scene assembly and animation.

#include "KspMapView.h"

auto KspMapView::describe(sketch::SketchContext& ctx) -> Element {
  using namespace ksp;
  const float W = ctx.size.width(), H = ctx.size.height();

  Element map = mapLayer(ctx);

  // Restrained photographic bloom: the bright pass, a small Gaussian and
  // a kPlus composite, over the map layer only (the LCD panels get their
  // own local glow). No scanlines, no backdrop distortion, no tiling.
  //
  // THE SECOND `mapLayer(ctx)` IS THE COST OF THIS SEAM, and it is not
  // avoidable: a bright pass has to run over the finished layer and be
  // composited BACK over it, which is two nodes, and a node is described
  // by building it. `Effect::phosphorBloom` retains its own source and
  // would need only one, but it GATHERS — twenty-four taps a pixel over
  // the whole map, where this is one tap and a separable blur. Over a
  // canvas this size the cheap composite wins, and the duplicate
  // describe is what it costs.
  Element bloom =
      mapLayer(ctx)
          .effect(
              Effect::brightPass(0.68f, 0.30f)
                  .then(Effect::filter(SkImageFilters::Blur(4, 4, nullptr))))
          .blend(SkBlendMode::kPlus)
          .opacity(0.34f);

  return stack()
      .width(Dim(W))
      .height(Dim(H))
      .child(backdrop(ctx))
      .child(std::move(map))
      .child(std::move(bloom))
      .child(burnCard())
      .child(infoCard())
      .child(toolbar())
      .child(missionClock())
      .child(altimeter())
      .child(crewPlate())
      .child(cluster())
      // corner vignette, last
      .child(box().inset(0).fill(
          Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                            {{0.50f, hexColor(0x000000, 0.0f)},
                             {1.0f, hexColor(0x000000, 0.30f)}})));
}

auto KspMapView::setup(sketch::SketchContext& ctx) -> void {
  using namespace ksp;
  sketch::kit::stage(
      ctx, {.size = {1200, 800}, .captureAt = 6.0, .background = kSpace});

  // Starfield: one soft-dot cell, 360 hashed instances.
  starAtlas = std::make_shared<instancing::Atlas>(2.0f);
  const int dot = starAtlas->cell(
      box().fill(Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                                   {{0.0f, hexColor(0xFFFFFF, 1.0f)},
                                    {0.42f, hexColor(0xFFFFFF, 0.55f)},
                                    {1.0f, hexColor(0xFFFFFF, 0.0f)}})),
      {7, 7});
  starPool = std::make_shared<instancing::Pool>();
  uint32_t s = 0x9E3779B9u;
  auto rnd = [&s] {
    return (float)(sigil::core::noise::xorshiftNext(s) & 0xffffffu) /
           (float)0xffffff;
  };
  for (int i = 0; i < 360; ++i) {
    const float px = rnd() * 1200.0f, py = rnd() * 800.0f;
    const float sc = 0.18f + rnd() * rnd() * 0.75f;
    const float a = 0.25f + rnd() * 0.7f;
    SkColor4f tint = hexColor(0xFFFFFF, a);
    const float hue = rnd();
    if (hue > 0.90f)
      tint = hexColor(0xBFD4FF, a);
    else if (hue < 0.08f)
      tint = hexColor(0xFFD8B8, a);
    starPool->add({px, py}, dot, 0.0f, sc, tint);
  }

  dashFast = 0;
  dashSlow = 0;
  ringSpin = 0;
  planetSpin = 0;
  burnTick = 0;
  nextBurnAt = 0;

  ctx.ticker.add([this, &ticker = ctx.ticker](double) {
    const double t = ticker.elapsed();
    const float ft = (float)t;
    dashFast = -ft * 22.0f;
    dashSlow = -ft * 14.0f;
    // discretised "instrument sampling": the hub glow steps at 8 Hz
    const float qt = quantizeTime(ft, 8.0f);
    hubGlow = 6.0f + 3.4f * (0.5f + 0.5f * std::sin(qt * 4.4f));
    armPulse = 1.0f + 0.055f * std::sin(ft * 3.5f);
    // 6–9 Hz summed-sine jitter on the arm being "dragged"
    jitterX = 1.6f * (std::sin(ft * 41.0f) + 0.6f * std::sin(ft * 27.0f));
    jitterY =
        1.6f * (std::sin(ft * 33.0f + 1.1f) + 0.6f * std::sin(ft * 51.0f));
    yaw = ft * 0.14f + 0.9f;
    pitchOut = 0.20f + 0.11f * std::sin(ft * 0.62f);
    rollOut = 0.06f * std::sin(ft * 0.43f);
    ringSpin = -ft * 8.0f * 0.5f;
    planetSpin = ft * 4.0f;
    // gauges stay in THEIR units (0..1); bind() maps them at the property
    throttle = 0.72f + 0.06f * std::sin(ft * 0.9f);
    gforce = 0.34f + 0.10f * std::sin(ft * 1.4f);
    const float drain = std::fmod(ft, 6.0f) / 6.0f;
    fuel0 = 1.00f - 0.15f * drain;
    fuel1 = 0.76f - 0.12f * drain;
    fuel2 = 0.51f - 0.09f * drain;
    fuel3 = 0.27f - 0.06f * drain;
    rcsPulse = 0.86f + 0.14f * (0.5f + 0.5f * std::sin(ft * 5.2f));
    goPulse = 0.82f + 0.18f * (0.5f + 0.5f * std::sin(ft * 4.2f));
    rollTape = 0.5f + 0.42f * std::sin(ft * 2.6f);
    yawTape = 0.5f + 0.42f * std::sin(ft * 2.17f + 2.0f);
    dvSweep = 0.34f + 0.30f * (0.5f + 0.5f * std::sin(ft * 0.8f));
    return true;
  });

  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("burn", burnLines());
}

auto KspMapView::update(double elapsed, sketch::SketchContext& ctx) -> void {
  if (elapsed < nextBurnAt) return;
  nextBurnAt = elapsed + 0.5;
  burnTick += 1;
  // Only the countdown re-describes; the rest of the tree keeps its caches.
  ctx.composer.renderSlot("burn", burnLines());
}

SIGIL_SKETCH(
    KspMapView, "Study \xc2\xb7 Game UI",
    "Kerbal Space Program's map view \xe2\x80\x94 real conics, a navball in "
    "one SkSL pass")
