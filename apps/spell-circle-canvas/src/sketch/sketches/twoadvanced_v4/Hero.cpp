#include "TwoAdvancedV4.h"

auto TwoAdvancedV4::cityBlock(sigil::geometry::mesh::Mesh& out, glm::vec3 lo,
                              glm::vec3 hi, glm::vec4 tint) -> void {
  static const glm::vec3 kN[5] = {
      {0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}};
  const glm::vec3 c[8] = {{lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
                          {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z},
                          {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
                          {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z}};
  static const int kFace[5][4] = {
      {0, 1, 2, 3}, {5, 4, 7, 6}, {1, 5, 6, 2}, {4, 0, 3, 7}, {3, 2, 6, 7}};
  for (int f = 0; f < 5; ++f) {
    const uint32_t base = (uint32_t)out.positions.size();
    const float side = f == 4 ? 1.0f : 0.74f;
    for (int k = 0; k < 4; ++k) {
      out.positions.push_back(c[kFace[f][k]]);
      out.normals.push_back(kN[f]);
      out.uvs.emplace_back(0.0f, 0.0f);
      out.colors.emplace_back(tint.r * side, tint.g * side, tint.b * side,
                              1.0f);
    }
    out.indices.insert(out.indices.end(),
                       {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

auto TwoAdvancedV4::cityMesh() -> sigil::geometry::mesh::Mesh {
  sigil::geometry::mesh::Mesh out;
  for (int rank = 0; rank < 4; ++rank) {
    const float z = -820.0f - (float)rank * 520.0f;
    const float fade = 1.0f - 0.20f * (float)rank;
    for (int i = 0; i < 26; ++i) {
      const float x = -1900.0f + (float)i * 150.0f + cityHash(i, rank) * 90.0f;
      const float wdt = 48.0f + cityHash(i, rank + 40) * 62.0f;
      const float hgt = 70.0f + cityHash(i, rank + 80) * 330.0f *
                                    (0.55f + 0.45f * (float)rank / 3.0f);
      const glm::vec4 tint{0.020f * fade, 0.128f * fade, 0.150f * fade, 1.0f};
      cityBlock(out, {x, 0.0f, z - 60.0f}, {x + wdt, hgt, z + 60.0f}, tint);
    }
  }
  return out;
}

auto TwoAdvancedV4::skyMesh() -> sigil::geometry::mesh::Mesh {
  sigil::geometry::mesh::Mesh m =
      sigil::geometry::mesh::quad(22000.0f, 7000.0f);
  m.colors.clear();
  for (const glm::vec3& pos : m.positions) {
    const float up = std::clamp(pos.y / 7000.0f + 0.5f, 0.0f, 1.0f);
    m.colors.emplace_back(0.010f + 0.030f * (1.0f - up),
                          0.056f + 0.174f * (1.0f - up),
                          0.070f + 0.196f * (1.0f - up), 1.0f);
  }
  return m;
}

auto TwoAdvancedV4::pod(const std::string& key, float x, float z, float scale)
    -> world::Element {
  namespace gm = sigil::geometry::mesh;
  world::Element p = world::Element().key(key).at({x, 0.0f, z}).scale(scale);
  p.child(world::Element()
              .key(key + "-shell")
              .at({0.0f, 66.0f, 0.0f})
              .mesh(gm::superellipsoid({52.0f, 74.0f, 52.0f}, 2.6f, 28, 18))
              .fill(sigil::material::kit::surface(
                  {.baseColor = {0.070f, 0.086f, 0.098f, 1.0f},
                   .metallic = 0.7f,
                   .roughness = 0.28f})));
  p.child(world::Element()
              .key(key + "-visor")
              .at({0.0f, 50.0f, 42.0f})
              .mesh(gm::superellipsoid({18.0f, 14.0f, 16.0f}, 1.8f, 20, 14))
              .fill(sigil::material::kit::unlit(
                  {.baseColor = {0.42f, 0.055f, 0.045f, 1.0f},
                   .emissive = {1.0f, 0.16f, 0.12f, 1.0f},
                   .emissiveStrength = 1.4f})));
  return p;
}

auto TwoAdvancedV4::bakeHero(int w, int h, sketch::SketchContext& ctx)
    -> sk_sp<SkImage> {
  namespace gm = sigil::geometry::mesh;
  using namespace tav;
  world::Element scene = world::Element().key("mainframe");

  // The lights: a cold key from behind the city, so the pods are read
  // as silhouettes against the halo, and a dim fill from the front so
  // their metal is not black.
  scene.child(world::Element().key("key").light(world::light::sun(
      {0.16f, -0.30f, 0.94f}, {0.36f, 0.86f, 0.92f, 1.0f}, 1.05f)));
  scene.child(world::Element().key("fill").light(world::light::sun(
      {-0.34f, -0.52f, -0.78f}, {0.44f, 0.72f, 0.82f, 1.0f}, 0.95f)));

  // THE SKY is a body: one unlit backdrop far behind everything, with
  // the ramp in its vertex colours. A gradient painted under the render
  // would be a second picture the scene knows nothing about.
  scene.child(
      world::Element()
          .key("sky")
          .at({0.0f, 900.0f, -5200.0f})
          .mesh(skyMesh())
          .fill(sigil::material::kit::unlit({.baseColor = {1, 1, 1, 1}})));
  scene.child(world::Element()
                  .key("water")
                  .rotateX(-90.0f)
                  .mesh(gm::quad(9000.0f, 9000.0f))
                  .fill(sigil::material::kit::surface(
                      {.baseColor = {0.012f, 0.070f, 0.082f, 1.0f},
                       .metallic = 0.9f,
                       .roughness = 0.10f})));
  scene.child(world::Element()
                  .key("city")
                  .mesh(cityMesh())
                  .fill(sigil::material::kit::surface(
                      {.baseColor = {1, 1, 1, 1}, .roughness = 0.85f})));
  // THE HALO: a real ring behind the pods, which is what the reference
  // reads as depth rather than as a drawn circle — the middle pod
  // occludes it.
  scene.child(world::Element()
                  .key("halo")
                  .at({0.0f, 150.0f, -760.0f})
                  .rotateX(90.0f)
                  .mesh(gm::torus(300.0f, 7.0f, 80, 8))
                  .fill(sigil::material::kit::unlit(
                      {.baseColor = {0.62f, 0.98f, 0.99f, 1.0f},
                       .emissive = {0.62f, 0.98f, 0.99f, 1.0f},
                       .emissiveStrength = 2.4f})));
  scene.child(pod("pod-mid", 0.0f, -430.0f, 1.60f));
  scene.child(pod("pod-left", -352.0f, -580.0f, 1.30f));
  scene.child(pod("pod-right", 358.0f, -600.0f, 1.26f));

  camera::Camera lens;
  lens.eye = {0.0f, 30.0f, 320.0f};
  lens.target = {0.0f, 57.0f, 0.0f};
  lens.up = {0.0f, 1.0f, 0.0f};
  lens.fovYDeg = 30.0f;
  lens.zNear = 4.0f;
  lens.zFar = 12000.0f;

  return ctx.bakeSet(world::Frame(std::move(scene)), lens, {w, h},
                     hexColor(0x02070A));
}

auto TwoAdvancedV4::heroScene(float w, float h, bool still) -> Element {
  using namespace tav;
  const float horizon = std::round(h * 0.66f);
  const float cx = w * 0.50f;

  Element scene = stack().width(Dim(w)).height(Dim(h)).clip();

  // THE BAKED RENDER: sky, city, water and pods in one image, sized to
  // the panel. A null bake (no raster surface) leaves the gradient
  // alone, which is the same forgiving contract every other bitmap on
  // this page keeps.
  if (heroPlate)
    scene.child(box().inset(0).fill(
        mskia::Paint::image(heroPlate, SkTileMode::kClamp, SkTileMode::kClamp,
                            SkMatrix::Scale(w / (float)heroPlate->width(),
                                            h / (float)heroPlate->height()),
                            SkSamplingOptions(SkFilterMode::kLinear))));
  else
    scene.child(box().inset(0).fill(
        mskia::Paint::linearUnit({0, 0}, {0, 0.66f},
                                 {{0.0f, hexColor(0x02070A)},
                                  {0.62f, hexColor(0x03181D)},
                                  {1.0f, hexColor(0x073038)}})));

  // the horizon haze band, full width. Without it the outer thirds are
  // black-on-black and the silhouettes have nothing to read against;
  // one kPlus ramp is the whole of the fix.
  scene.child(at(box().fill(mskia::Paint::linearUnit(
                     {0, 0}, {0, 1},
                     {{0.00f, mskia::withAlpha(kTealBar, 0.0f)},
                      {0.62f, mskia::withAlpha(kTealBar, 0.10f)},
                      {1.00f, mskia::withAlpha(kTealBar, 0.34f)}})),
                 0, horizon - 132, w, 132)
                  .blend(SkBlendMode::kPlus));

  // THE portal: one SDF circle. Its box must RESERVE sdf::pad() for the
  // glow — sdf::minBoxFor() is the only honest way to size it, since
  // pad eats into the half-size (a 300 box with glowRadius 54 leaves
  // almost no disc at all). 132 px of visible disc, reaching far.
  // The halo is a RING IN THE SCENE and the pods occlude it, so what
  // this adds is the atmosphere around it: a low-alpha core under a
  // reaching glow, screened over the render rather than pasted in front
  // of it.
  msdf::Style ps{.fill = mskia::toColor(mskia::withAlpha(kGlow, 0.07f)),
                 .borderWidth = 2,
                 .borderColor = mskia::toColor(
                     mskia::withAlpha({0.90f, 1.0f, 1.0f, 1.0f}, 0.35f)),
                 .glowRadius = 54,
                 .glowColor = mskia::toColor(mskia::withAlpha(kGlow, 0.42f))};
  const float pbox = msdf::minBoxFor(ps, 132);
  mskia::Paint pm = mskia::Paint::recipe(msdf::material(msdf::circle(), ps));
  if (!still) pm.uniform("uGlowR", &portalGlow);  // ±8 % sine, period 4 s
  Element portal = at(box().fill(pm), cx - pbox * 0.5f,
                      horizon - 108 - pbox * 0.5f, pbox, pbox)
                       .blend(SkBlendMode::kPlus);
  if (!still)
    // the one deliberately bouncy beat: the power core kicking on.
    // motion::ease::outBack() takes its overshoot as a parameter and converts
    // to an EaseFn, so the kick is one animate() call rather than a
    // hand-written keyframe path through the overshoot and back.
    portal.scale(animate(motion::from(0.80f).to(1.0f),
                         {620ms, motion::ease::outBack(2.1f), 2400ms}));
  scene.child(portal);

  // an orbital ring, trim-revealed with the panel
  Element ring = at(
      box()
          .shape(shapes::arc(-125, 310))
          .stroke(stroke(2, Fill::color(mskia::withAlpha(kCyanRing, 0.34f)))),
      cx - 118, horizon - 226, 236, 236);
  if (!still)
    ring.mask(by::spans(spans::upTo(animate(
        motion::from(0.0f).to(1.0f), {700ms, &ch::easeOutQuint, 2600ms}))));
  scene.child(ring);

  // water: streaks + a mirrored, blurred copy of the portal glow
  // The water is the RENDER's water; what compose adds over it is the
  // light on it. A fill here would paint out the one surface the bake
  // reflects the halo in.
  Element water = at(box().clip(), 0, horizon, w, h - horizon);
  if (!still)
    water.child(box()
                    .inset(0)
                    .fill(waterStreaks)
                    .opacity(0.55f)
                    .blend(SkBlendMode::kPlus));
  water.child(box()
                  .left(Dim(cx - 190))
                  .top(Dim(-72))
                  .width(380)
                  .height(300)
                  .fill(mskia::Paint::radialUnit(
                      {0.5f, 0.14f}, 1.05f,
                      {{0.0f, mskia::withAlpha(kGlow, 0.75f)},
                       {0.45f, mskia::withAlpha(kTealBar, 0.32f)},
                       {1.0f, mskia::withAlpha(kTealBar, 0.0f)}}))
                  // smear the reflection down into the water: sigma 26
                  // along the 90° axis (straight down), 14 across it
                  .effect(mskia::Effect::directionalBlur(26, 90, 14))
                  .opacity(0.78f)
                  .blend(SkBlendMode::kPlus));
  // the specular COLUMN — the vertical smear of a light in water, and
  // the single cue that reads "reflection" from across the room
  water.child(box()
                  .left(Dim(cx - 40))
                  .top(Dim(0))
                  .width(80)
                  .height(Dim(h - horizon))
                  .fill(mskia::Paint::linearUnit(
                      {0, 0}, {0, 1},
                      {{0.00f, mskia::withAlpha(kGlow, 0.55f)},
                       {0.35f, mskia::withAlpha(kGlow, 0.20f)},
                       {1.00f, mskia::withAlpha(kGlow, 0.0f)}}))
                  // soften the column's sides: sigma 10 along the 0° axis
                  // (horizontal), only 3 down its length
                  .effect(mskia::Effect::directionalBlur(10, 0, 3))
                  .blend(SkBlendMode::kPlus));
  scene.child(water);

  // THE horizon hairline. A hard, bright edge where the water starts
  // sells the reflection below it more than the blur itself does.
  scene.child(
      at(box().fill(mskia::withAlpha(kGlow, 0.62f)), 0, horizon - 1, w, 2));
  scene.child(
      at(box().fill(mskia::withAlpha(kCyanRing, 0.16f)), 0, horizon + 3, w, 1));
  return scene;
}

auto TwoAdvancedV4::hero(float w, float h) -> Element {
  using namespace tav;
  Element s = stack().width(Dim(w)).height(Dim(h)).clip();
  s.child(heroScene(w, h, false));
  // the atmospheric bloom pass: the same composite, blurred, screened
  // back over itself. Built `still` so it is provably static and the
  // Texture bake is paid once, not per frame.
  s.child(
      heroScene(w, h, true)
          .effect(mskia::Effect::filter(SkImageFilters::Blur(22, 22, nullptr)))
          .opacity(0.34f)
          .blend(SkBlendMode::kPlus)
          .cache(Cache::Texture)
          .bakeScale(0.5f));
  s.child(box().inset(0).fill(
      mskia::Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                               {{0.00f, {0, 0, 0, 0}},
                                {0.58f, {0, 0, 0, 0.10f}},
                                {1.00f, {0, 0, 0, 0.66f}}})));
  s.child(box().inset(0).foreground(styles::Scanlines{}));
  s.child(box().inset(0).foreground(styles::Brackets{
      mskia::withAlpha(kCyan, 0.7f), 22, 2, 8, shapes::Corner::All}));

  auto corner = [&](const char* a, const char* b, float l, float tp, bool end) {
    return box()
        .column()
        .gap(2)
        .alignItems(end ? Align::End : Align::Start)
        .left(Dim(l))
        .top(Dim(tp))
        .child(t(a, micro(10, mskia::withAlpha(kCyan, 0.8f), 220)))
        .child(t(b, micro(10, mskia::withAlpha(kCyanRing, 0.45f), 220)));
  };
  s.child(corner("REND / MAXON C4D R8", "PASS 04 \xc2\xb7 FRM 0142", 20, 18,
                 false));
  s.child(corner("38.2144 N", "121.4944 W", w - 132, 18, true));
  s.child(corner("DEPTH 00.42", "PRESS 1013 HPA", 20, h - 42, false));
  s.child(
      box()
          .left(Dim(w - 214))
          .top(Dim(h - 32))
          .row()
          .gap(6)
          .alignItems(Align::Center)
          .child(box().width(120).height(8).foreground(
              styles::TickRail{mskia::withAlpha(kCyan, 0.6f), 6, 3, 8, 1, 4,
                               0.5f, path::Edge::Top}))
          .child(t("SIG 88%", micro(10, mskia::withAlpha(kCyan, 0.85f), 200))));
  return s;
}

auto TwoAdvancedV4::mainframe() -> Element {
  using namespace tav;
  Element body = box().grow(1).clip().child(hero(1178, 316));
  // The transition shutters: six slats over the viewport, each one's
  // cover fraction a bound value — the hero underneath is never
  // re-described, so its bloom bake survives every section change.
  const float slatW = 1178.0f / 6.0f;
  for (int i = 0; i < 6; ++i)
    body.child(box()
                   .left(Dim((float)i * slatW))
                   .top(Dim(0))
                   .width(Dim(slatW + 1))
                   .height(316)
                   .fill(mskia::Paint::linearUnit({0, 0}, {1, 0},
                                                  {{0.0f, hexColor(0x2A0708)},
                                                   {1.0f, hexColor(0x1A0405)}}))
                   .foreground(onEdges(
                       path::Edge::Bottom,
                       stroke(3, Fill::color(mskia::withAlpha(kCyan, 0.5f)),
                              PathFormat::Align::Inner)))
                   .scaleY(&shutter[(size_t)i])
                   .transformOrigin(0.5f, 0.0f));
  // The ACCESSING readout that rides the closed shutters.
  body.child(box()
                 .left(Dim(1178.0f / 2 - 220))
                 .top(Dim(316.0f / 2 - 32))
                 .width(440)
                 .height(64)
                 .shape(shapes::chamfered(10, shapes::Corner::Diagonal))
                 .fill(mskia::withAlpha(hexColor(0x140404), 0.92f))
                 .stroke(stroke(1, Fill::color(mskia::withAlpha(kCyan, 0.6f)),
                                PathFormat::Align::Inner))
                 .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.7f), 10,
                                              2, 3, shapes::Corner::All})
                 .justify(Justify::Center)
                 .alignItems(Align::Center)
                 .child(slot("mfload"))
                 .opacity(&shutterInfo));

  Element panel = bevelPanel(box().column().padding(3), kChrome, 3);
  panel.key("mainframe")
      .area("mainframe")
      .translateY(animate(motion::from(70.0f).to(0.0f),
                          {520ms, &ch::easeOutQuint, 2400ms}))
      .opacity(animate(motion::from(0.0f).to(1.0f),
                       {300ms, &ch::easeOutQuad, 2400ms}))
      .child(panelHeader("MAIN", "FRAME",
                         "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 0))
      .child(body);
  return panel;
}

auto TwoAdvancedV4::mfLoadReadout(int section) -> Element {
  using namespace tav;
  return box()
      .row()
      .gap(10)
      .alignItems(Align::Center)
      .child(t("ACCESSING", micro(12, mskia::withAlpha(kCyan, 0.85f), 260)))
      .child(t("\xe2\x96\xb8", micro(11, kCyan, 0)))
      .child(t(kNavItems[section], heavy(17, kNear, 80)))
      .child(box().width(60).height(10).foreground(
          styles::TickRail{mskia::withAlpha(kCyan, 0.6f), 5, 3, 8, 1, 4, 0.5f,
                           path::Edge::Bottom}));
}

auto TwoAdvancedV4::monitorBody(float h) -> Element {
  using namespace tav;
  return box()
      .height(Dim(h))
      .fill(mskia::Paint::linearUnit({0, 0}, {0, 1},
                                     {{0.00f, kPanelHi},
                                      {0.15f, kPanel},
                                      {0.88f, kPanel},
                                      {1.00f, kPanelSh}}))
      .foreground(kit::gloss(mskia::withAlpha(kPanelHi, 0.5f), 40,
                             {0, -h * 0.34f}, 0.72f, 0.28f))
      .foreground(onEdges(
          path::Edge::Top,
          stroke(1, Fill::color(mskia::withAlpha(hexColor(0xCFEFEC), 0.7f)),
                 PathFormat::Align::Inner)));
}

auto TwoAdvancedV4::relatedStills() -> std::vector<Element> {
  using namespace tav;
  static const char* caps[4] = {"REEL 02", "BOARDS", "RIG TEST", "PLATE"};
  std::vector<Element> out;
  for (int i = 0; i < 4; ++i) {
    const float g = 0.30f + 0.18f * (float)i;
    Element cell =
        box()
            .grow(1)
            .column()
            .gap(3)
            .child(
                box()
                    .grow(1)
                    .shape(shapes::chamfered(7, shapes::Corner::Diagonal))
                    .fill(
                        mskia::Paint::linearUnit({0, 0}, {0, 1},
                                                 {{0.0f, hexColor(0x0A2C33)},
                                                  {1.0f, hexColor(0x02171B)}}))
                    .stroke(stroke(
                        1,
                        Fill::color(mskia::withAlpha(hexColor(0x0B3B40), 0.9f)),
                        PathFormat::Align::Inner))
                    .child(box().inset(0).fill(mskia::Paint::radialUnit(
                        {0.3f + 0.15f * (float)i, 0.8f}, 0.95f,
                        {{0.0f, mskia::withAlpha(kGlow, g)},
                         {1.0f, mskia::withAlpha(kGlow, 0.0f)}})))
                    .child(at(box().fill(hexColor(0x011114)), 6 + 4 * (float)i,
                              18, 12, 30))
                    .child(at(box().fill(hexColor(0x01191D)), 24 + 3 * (float)i,
                              8, 16, 40))
                    .child(at(box().fill(mskia::withAlpha(kGlow, 0.55f)), 0, 40,
                              200, 1))
                    .foreground(styles::Brackets{mskia::withAlpha(kCyan, 0.5f),
                                                 6, 1, 2, shapes::Corner::All})
                    .foreground(styles::Scanlines{{0, 0, 0, 0.24f}, 3, 1}))
            .child(t(caps[i], micro(9, hexColor(0x123B3D), 220)));
    out.push_back(std::move(cell));
  }
  return out;
}
