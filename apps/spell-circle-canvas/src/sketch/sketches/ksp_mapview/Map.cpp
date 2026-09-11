#include "KspMapView.h"

auto KspMapView::backdrop(sketch::SketchContext& ctx) -> Element {
  using namespace ksp;
  const float W = ctx.size.width(), H = ctx.size.height();

  Element g = stack().inset(0);

  g.child(box().inset(0).fill(Paint::radialUnit(
      {0.42f, 0.45f}, 1.15f, {{0.0f, kSpace}, {1.0f, kSpaceEdge}})));

  // The milky-way band the reference frame is dominated by: soft
  // blob-shaped ramps on a diagonal, all at single-digit alpha. No grain
  // anywhere near it — over a near-transparent base, grain composites as
  // its own luminance rather than modulating the base, which turns the
  // whole band into a white cloud. Grain lives on the opaque panels
  // below, which is where it reads as material anyway.
  auto wisp = [&](uint32_t seed, float x, float y, float w, float h,
                  SkColor4f c, float a, float rot) {
    return at(box()
                  .shape(shapes::blob(seed, 0.30f, 9))
                  .fill(
                      Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                                        {{0.0f, mskia::withAlpha(c, a)},
                                         {0.5f, mskia::withAlpha(c, a * 0.45f)},
                                         {1.0f, mskia::withAlpha(c, 0.0f)}}))
                  .rotate(rot),
              x, y, w, h)
        .cache(Cache::Texture)
        .bakeScale(0.4f);
  };
  // Two broad grounds at almost nothing, then a MOTTLE of small blobs
  // along the band. One big soft radial reads as fog; the reference's
  // milky way is mottled, and mottle is what overlapping small ones make.
  g.child(wisp(7u, -160, -70, 980, 400, kNebula, 0.16f, -12));
  g.child(wisp(13u, 340, 10, 1000, 350, kNebula2, 0.14f, -16));
  uint32_t ws = 0x2545F491u;
  auto wr = [&ws] {
    return (float)(sigil::core::noise::xorshiftNext(ws) & 0xffffffu) /
           (float)0xffffff;
  };
  for (int i = 0; i < 16; ++i) {
    const float u = (float)i / 15.0f;
    const float bx = -140.0f + u * 1360.0f + (wr() - 0.5f) * 190.0f;
    const float by = 40.0f + u * 190.0f + (wr() - 0.5f) * 230.0f;
    const float bw = 150.0f + wr() * 260.0f;
    g.child(wisp(41u + (uint32_t)i * 7u, bx, by, bw, bw * (0.5f + wr() * 0.4f),
                 (i % 3 == 0) ? kNebula2 : kNebula, 0.11f + wr() * 0.11f,
                 -30.0f + wr() * 60.0f));
  }
  g.child(wisp(21u, 40, 430, 560, 380, kNebula2, 0.07f, 8));
  g.child(wisp(31u, 680, 380, 600, 440, kNebula, 0.06f, 22));
  (void)H;

  // The whole starfield as ONE atlas stamp, hashed (not a lattice),
  // twinkling by per-instance tint alpha mutated from the ticker.
  g.child(at(box().child(instancing::instances(starAtlas, starPool,
                                               instancing::Mode::Live)),
             0, 0, W, H));
  return g;
}

auto KspMapView::planet() -> Element {
  using namespace ksp;
  const float d = kKerbinR * 2;
  Element g = stack();

  g.child(at(box()
                 .shape(shapes::circle())
                 // light offset toward the upper-left: the centre of the
                 // ramp is displaced, which is what fakes sphere shading.
                 .fill(Paint::radial({kKerbinR * 0.60f, kKerbinR * 0.50f},
                                     kKerbinR * 1.35f,
                                     {{0.0f, mskia::lighten(kOceanLit, 0.05f)},
                                      {0.34f, kOceanLit},
                                      {0.72f, hexColor(0x1B4260)},
                                      {1.0f, kOceanDark}}))
                 .clip(),
             kKerbin, d, d)
              .child(box()
                         .inset(0)
                         .rotate(&planetSpin)
                         .child(at(box()
                                       .shape(shapes::blob(4u, 0.30f, 9))
                                       .fill(Paint::solid(kLandMoss)),
                                   16, 34, 152, 120))
                         .child(at(box()
                                       .shape(shapes::blob(11u, 0.26f, 8))
                                       .fill(Paint::solid(kLandTan)),
                                   126, 148, 122, 100))
                         .child(at(box()
                                       .shape(shapes::blob(19u, 0.34f, 7))
                                       .fill(Paint::solid(hexColor(0x53803A))),
                                   56, 172, 96, 78))
                         .child(at(box()
                                       .shape(shapes::blob(29u, 0.28f, 8))
                                       .fill(Paint::solid(hexColor(0x8E7C4E))),
                                   30, 178, 62, 56)))
              // The terminator: a dark ramp anchored past the lower-right
              // limb, multiplied over land AND ocean alike. Without it the
              // continents float on a flat blue coin.
              .child(box()
                         .inset(0)
                         // radiusUnit is a fraction of the HALF-DIAGONAL,
                         // so 1.02 is what finishes the ramp at the limb
                         // of the inscribed disc: past ~1.28 the dark end
                         // falls outside it and the terminator vanishes.
                         .fill(Paint::radialUnit(
                             {0.34f, 0.28f}, 1.02f,
                             {{0.0f, hexColor(0xFFFFFF, 0.0f)},
                              {0.38f, hexColor(0x081420, 0.06f)},
                              {0.70f, hexColor(0x061019, 0.42f)},
                              {1.0f, hexColor(0x03070B, 0.92f)}})))
              // and one hot specular sliver where the star hits the ocean
              .child(box()
                         .inset(0)
                         .fill(Paint::radialUnit(
                             {0.30f, 0.24f}, 0.42f,
                             {{0.0f, hexColor(0xBFE4F5, 0.13f)},
                              {1.0f, hexColor(0xBFE4F5, 0.0f)}}))
                         .blend(SkBlendMode::kPlus)));

  // Fresnel limb: one SDF pass, border + exponential glow.
  const sdf::Style rim{.fill = {0, 0, 0, 0},
                       .borderWidth = 1.6f,
                       .borderColor = toColor(mskia::withAlpha(kAtmo, 0.68f)),
                       .glowRadius = 8.0f,
                       .glowColor = toColor(mskia::withAlpha(kAtmo, 0.26f))};
  const float boxSize = sdf::minBoxFor(rim, d);
  g.child(at(box().fill(Paint::recipe(sdf::material(sdf::circle(), rim))),
             kKerbin, boxSize, boxSize));
  return g;
}

auto KspMapView::orbits(sketch::SketchContext& ctx) -> Element {
  using namespace ksp;
  const float W = ctx.size.width(), H = ctx.size.height();
  const Conic cur = currentOrbit(), tgt = targetOrbit(), esc = escapeArc();

  Element g = stack().inset(0);
  auto full = [&](Element e) {
    e.inset(0).width(Dim(W)).height(Dim(H));
    return e;
  };

  // Target orbit: a huge ellipse whose periapsis arc is all that crosses
  // the frame — the reference's "distant orbit glimpsed as a flat band".
  g.child(
      full(box()
               .shape(trajectory(tgt, {-118, 118, 260}))
               .stroke(MarchingDots{.width = 1.4f,
                                    .color = mskia::withAlpha(kTarget, 0.80f),
                                    .intervals = {1.6f, 5.4f},
                                    .phase = &dashSlow,
                                    .speed = 1.0f})));
  // …plus its bright solid near-edge, the way the reference frame shows
  // one lit segment of that same orbit crossing the top of the screen.
  g.child(full(box()
                   .shape(trajectory(tgt, {-46, 46, 90}))
                   .stroke(PathFormat{.width = 1.5f,
                                      .strokeFill = Fill::color(
                                          mskia::withAlpha(kTarget, 0.95f))})));

  // Escape / flyby hyperbola — open, not a closed shape. The window is
  // hand-fitted to the part that crosses the frame rather than run out
  // to the asymptote at esc.asymptoteDeg(): a span held to a reach
  // BREAKS where it leaves it, and onPath treats each contour as its own
  // stretch of baseline that a word may not straddle.
  const float nuA = -104.0f, nuB = 98.0f;
  g.child(
      full(box()
               .shape(trajectory(esc, {nuA, nuB, 260}))
               .stroke(MarchingDots{.width = 1.2f,
                                    .color = mskia::withAlpha(kEscape, 0.50f),
                                    .intervals = {1.4f, 5.8f},
                                    .phase = &dashFast,
                                    .speed = 0.55f})));
  g.child(full(t("ESCAPE  ·  KERBIN SOI EXIT  T+ 1h 12m",
                 body(8.5f, mskia::withAlpha(kEscape, 0.6f), 1.3f))
                   .onPath(TextPath{.path = trajectory(esc, {nuA, nuB, 260}),
                                    .at = 0.80f,
                                    .align = TextPath::Align::Center,
                                    .offset = 8.0f,
                                    .autoFlip = true})));

  // The hero: the current orbit, drawn on with a trim reveal and dressed
  // in the organic 4-layer additive glow (a LayeredBrush, not a
  // frame-level effect()).
  g.child(
      full(box()
               .shape(trajectory(cur, {0, 360, 360}))
               .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                           {900ms, ch::easeOutQuad})),
                       brush::presets::filament(mskia::withAlpha(kOrbit, 0.30f),
                                                kOrbitCore, 0.26f))));

  // One arc label riding the orbit itself — shaped once, placed by arc
  // length, per-glyph tangent rotation. (Element::onPath.)
  g.child(full(t("KERBIN  ·  Ap 213,904 m  ·  Pe 88,012 m",
                 body(9.5f, mskia::withAlpha(kOrbit, 0.9f), 1.6f))
                   .onPath(TextPath{.path = trajectory(cur, {0, 360, 360}),
                                    .at = 0.855f,
                                    .align = TextPath::Align::Center,
                                    .offset = 8.0f,
                                    .autoFlip = true})));
  return g;
}

auto KspMapView::targetLabel(sketch::SketchContext& ctx) -> Element {
  using namespace ksp;
  const Conic tgt = targetOrbit();
  return t("TGT · MUN TRANSFER",
           body(8.5f, mskia::withAlpha(kTarget, 0.85f), 1.4f))
      .onPath(TextPath{.path = trajectory(tgt, {-118, 118, 260}),
                       .at = 0.30f,
                       .align = TextPath::Align::Center,
                       .offset = -9.0f,
                       .autoFlip = true})
      .inset(0)
      .width(Dim(ctx.size.width()))
      .height(Dim(ctx.size.height()));
}

auto KspMapView::marker(const char* label, SkPoint p, SkColor4f c, bool filled,
                        SkVector lift) -> Element {
  using namespace ksp;
  Element g = stack();
  Element d = at(box().shape(shapes::polygon(4)), p, 9, 9);
  if (filled)
    d.fill(Paint::solid(c));
  else
    d.stroke(PathFormat{.width = 1.2f, .strokeFill = Fill::color(c)});
  g.child(d);
  g.child(at(t(label, bold(9.0f, c, 0.6f)), {p.fX + lift.fX, p.fY + lift.fY},
             30, 12));
  return g;
}

auto KspMapView::gizmo() -> Element {
  using namespace ksp;
  const Conic cur = currentOrbit();
  const SkPoint hub = pointAt(cur, kNodeNu);
  const glm::vec2 pro = cur.alongAt(kNodeNu);
  const float aPro = bearingDeg(pro);
  const float aRad = bearingDeg(cur.outwardAt(kNodeNu));
  // Prograde and radial-out are ≈113.8° apart at this node, so `sep` —
  // measured from prograde round to radial-out in screen-angle order — is
  // the explement, ≈246.2°. Bisecting THAT is what drops the out-of-plane
  // glyphs into the wide side rather than on top of an in-plane arm; no
  // stylised fan needed.
  float sep = aRad - aPro;
  while (sep < 0) sep += 360;
  while (sep >= 360) sep -= 360;
  const float aNorm = aPro + sep * 0.5f;
  const float aAnti = aNorm + 180.0f;

  Element g = stack().inset(0).staggerChildren(55ms);

  // one shared builder, two lengths, two fill modes
  auto arm = [&](const char* k, float bearing, float len, SkColor4f c,
                 bool solid, bool jitter) {
    Element e = box().width(Dim(len)).height(Dim(20)).shape(paddle(len));
    if (solid)
      e.fill(Paint::solid(c));
    else
      e.stroke(PathFormat{.width = 1.8f, .strokeFill = Fill::color(c)})
          .fill(Paint::solid(mskia::withAlpha(c, 0.22f)));
    const float rad2 = bearing * 0.017453293f;
    e.centerAt(arrange::onEllipse(hub, {len * 0.5f + 9, len * 0.5f + 9}, rad2))
        .rotate(bearing)
        .key(k)
        .scale(&armPulse)
        .opacity(animate(from(0.0f).to(1.0f), {380ms, ease::outBack()}));
    if (jitter) e.translateX(&jitterX).translateY(&jitterY);
    return e;
  };
  auto glyph = [&](const char* k, float bearing, SkColor4f c, bool solid) {
    const float rad2 = bearing * 0.017453293f;
    Element e =
        box()
            .width(Dim(18))
            .height(Dim(18))
            .shape(solid ? shapes::ring(2.6f, 3.0f) : shapes::ring(2.2f))
            .fill(Paint::solid(solid ? c : mskia::withAlpha(c, 0.62f)))
            .centerAt(arrange::onEllipse(hub, {40, 40}, rad2))
            .key(k)
            .scale(&armPulse)
            .opacity(animate(from(0.0f).to(1.0f), {380ms, ease::outBack()}));
    return e;
  };
  // the two out-of-plane glyphs ride a short spoke, so the fan reads as
  // six axes off one hub rather than two loose dots
  auto spoke = [&](float bearing, SkColor4f c) {
    const float r2 = bearing * 0.017453293f;
    const SkPoint a = arrange::onEllipse(hub, {11, 11}, r2);
    const SkPoint b = arrange::onEllipse(hub, {31, 31}, r2);
    return box()
        .inset(0)
        .shape(keyedShape(std::tuple{a.fX, a.fY, b.fX, b.fY},
                          [a, b](SkSize) {
                            SkPathBuilder p;
                            p.moveTo(a);
                            p.lineTo(b);
                            return p.detach();
                          }))
        .stroke(
            PathFormat{.width = 2.0f,
                       .strokeFill = Fill::color(mskia::withAlpha(c, 0.75f))});
  };

  g.child(arm("pro", aPro, 54, kProgradeC, true, true));
  g.child(arm("rout", aRad, 42, kRadialC, true, false));
  g.child(spoke(aNorm, kNormalC));
  g.child(glyph("nrm", aNorm, kNormalC, true));
  g.child(arm("ret", aPro + 180, 54, kProgradeC, false, false));
  g.child(arm("rin", aRad + 180, 42, kRadialC, false, false));
  g.child(spoke(aAnti, mskia::withAlpha(kNormalC, 0.6f)));
  g.child(glyph("anrm", aAnti, kNormalC, false));

  // Hub: one SDF pass — fill, ring, and a breathing glow bound to a
  // quantised Output (instrument sampling, not a cinematic pulse).
  const sdf::Style hs{
      .fill = toColor(hexColor(0x12181C, 0.72f)),
      .borderWidth = 1.6f,
      .borderColor = toColor(hexColor(0xF0F4F5)),
      .glowRadius = 9.0f,
      .glowColor = toColor(mskia::withAlpha(kProgradeC, 0.85f))};
  const float hbox = sdf::minBoxFor(hs, 19.0f);
  Paint hm = Paint::recipe(sdf::material(sdf::circle(), hs))
                 .uniform("uGlowR", &hubGlow);
  g.child(at(box().fill(std::move(hm)).key("hub"), hub, hbox, hbox));

  // Δv direction stub: the burn vector, drawn from the hub along prograde.
  g.child(box()
              .inset(0)
              .shape(keyedShape(std::tuple{hub.fX, hub.fY, pro.x, pro.y},
                                [hub, pro](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(hub);
                                  b.lineTo(hub.fX + pro.x * 96,
                                           hub.fY + pro.y * 96);
                                  return b.detach();
                                }))
              .stroke(lines::Line{
                  .width = 1.2f,
                  .fill = Fill::color(mskia::withAlpha(kProgradeC, 0.5f)),
                  .dashIntervals = {4, 4}}));
  return g;
}

auto KspMapView::burnCard() -> Element {
  using namespace ksp;
  return at(
      box()
          .column()
          .padding(9, 8, 9, 8)
          .gap(3)
          .fill(Paint::solid(hexColor(0x12181C, 0.86f)))
          .stroke(PathFormat{.width = 1.0f,
                             .strokeFill = Fill::color(hexColor(0x3A4148))})
          .child(box()
                     .row()
                     .gap(6)
                     .alignItems(Align::Baseline)
                     .child(t("Δv", body(10.5f, hexColor(0x9AA4AA))))
                     .child(t("164.9", lcd(16, kLcd)))
                     .child(t("m/s", body(10, mskia::withAlpha(kLcd, 0.8f)))))
          .child(slot("burn"))
          .child(box()
                     .height(Dim(1))
                     .fill(Paint::solid(hexColor(0x2C3238)))
                     .margin(2)),
      646, 566, 190, 88);
}

auto KspMapView::burnLines() -> Element {
  using namespace ksp;
  char a[64], b[64];
  std::snprintf(a, sizeof a, "Est. Burn: %ds", 38 + (burnTick % 5));
  const int total = 2672 - burnTick;
  std::snprintf(b, sizeof b, "Node in T + %dm, %02ds", total / 60, total % 60);
  return box()
      .column()
      .gap(1)
      .child(t(a, body(10.5f, kAmber)).key("burnA"))
      .child(t(b, body(10.5f, kAmber)).key("burnB"));
}

auto KspMapView::mapLayer(sketch::SketchContext& ctx) -> Element {
  using namespace ksp;
  const Conic cur = currentOrbit();
  const Conic tgt = targetOrbit();
  Element g = stack().inset(0);
  g.child(planet());
  g.child(orbits(ctx));
  g.child(marker("Ap", pointAt(cur, 180), kApLabel, true));
  g.child(marker("Pe", pointAt(cur, 0), kPeLabel, true));
  g.child(marker("AN", pointAt(cur, 100), kAnLabel, false));
  g.child(marker("DN", pointAt(cur, 280), kApLabel, false, {12, 13}));
  g.child(chip("◗", "Mun", pointAt(tgt, 28), kTarget, 12));
  g.child(chip("✦", "", pointAt(tgt, -64), kTarget, 8));
  g.child(targetLabel(ctx));
  g.child(gizmo());
  // the craft itself, riding its orbit ahead of the node
  g.child(at(box()
                 .shape(shapes::polygon(3, 90))
                 .fill(Paint::solid(hexColor(0xE8F2F4)))
                 .rotate(bearingDeg(cur.alongAt(-40))),
             pointAt(cur, -40), 13, 11));
  return g;
}
