// SigilMaterial values as a node's paint: fields, tiles, SDF surfaces and
// the layer styles this tier spells over them, plus the view transform a
// composer carries.

#include "support/BrushTestSupport.h"

TEST(ComposePatterns, GrainIsMonochromeAndVaries) {
  // Material::recipe(material::field::noise()) is fractal RGB noise — its
  // channels are independent fields, so overlaying it on a coloured surface
  // hue-shifts rather than shades. grain() is the luminance one: equal
  // channels, real variation.
  Host host(120, 120);
  host.composer.render(
      box().child(box().width(120).height(120).absolute().inset(0).fill(
          material::skia::Paint::recipe(
              material::field::grain(0.08f, 4, 3.0f)))));
  host.frame();
  int lo = 255, hi = 0;
  for (int y = 4; y < 116; y += 3)
    for (int x = 4; x < 116; x += 3) {
      const SkColor c = host.pixel(x, y);
      const int r = (int)SkColorGetR(c), g = (int)SkColorGetG(c),
                b = (int)SkColorGetB(c);
      ASSERT_LE(std::abs(r - g), 2) << "channel split at " << x << "," << y;
      ASSERT_LE(std::abs(g - b), 2) << "channel split at " << x << "," << y;
      lo = std::min(lo, r);
      hi = std::max(hi, r);
    }
  EXPECT_GT(hi - lo, 40) << "grain is flat — no field to overlay";
}

TEST(ComposeMaterial, BlendWithSdfLayerResolvesGeometry) {
  // A blend containing a geometry-dependent (SDF) layer must defer its
  // flatten to resolve time, when the node's size is known. Flattening at
  // build time bakes a zero resolution and renders a degenerate speck.
  material::skia::Paint m = material::skia::Paint::blend({
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {material::skia::Paint::recipe(material::sdf::material(
           material::sdf::circle(), {.fill = {1, 0, 0, 1}})),
       SkBlendMode::kPlus},
  });
  EXPECT_TRUE(m.geometryDependent());  // inherited from the SDF layer
  EXPECT_FALSE(m.isAnimated());        // still cacheable
  Host host;
  host.composer.render(box().child(
      box().width(100).height(100).inset(0, 0, 100, 100).absolute().fill(m)));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 150u);  // circle body visible
  EXPECT_LT(SkColorGetR(host.pixel(3, 3)), 40u);     // corner outside circle
}

TEST(ComposeSdf, AStarFillsItsCentreAndMissesTheBoxCorners) {
  // The analytic N-star: fill covers the body, the box corners lie outside
  // the arms. One shader pass, pixel-space distance.
  Host host;
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .inset(0, 0, 100, 100)
          .absolute()
          .fill(material::skia::Paint::recipe(material::sdf::material(
              material::sdf::star(5, 2.4f), {.fill = {1, 0, 0, 1}})))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 200u);  // body
  const SkColor corner = host.pixel(4, 4);           // outside the arms
  EXPECT_LT(SkColorGetR(corner), 30u);
  EXPECT_LT(SkColorGetG(corner), 30u);
}

TEST(ComposeSdf, GeometryStaticCachesAndPrunes) {
  // An SDF material reads uResolution (geometry-dependent) but binds no
  // Outputs: it must CACHE like static content (0 live paints, 0 re-records)
  // AND prune across an identical re-describe (recipe equality — same
  // per-kind effect pointer, equal constants).
  Host host;
  auto tree = [] {
    return box().child(box().width(80).height(60).fill(
        material::skia::Paint::recipe(material::sdf::material(
            material::sdf::roundBox(12),
            {.fill = {0, 1, 0, 1}, .borderWidth = 3}))));
  };
  host.composer.render(tree());
  host.frame();  // records
  host.frame();  // replays
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().nodesPainted, 0u);
  host.composer.render(tree());  // fresh describe, identical recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
}

TEST(ComposeSdf, ResizeReResolvesGeometry) {
  // uResolution bakes into the recording; a size change must re-resolve —
  // the materialSize invalidation, without any prop change.
  Host host;  // 200x200 surface
  host.composer.render(box().child(
      box().grow(1).fill(material::skia::Paint::recipe(material::sdf::material(
          material::sdf::circle(), {.fill = {1, 0, 0, 1}})))));
  host.frame();  // circle c=(100,100) r≈99
  host.composer.setSize({120, 120});
  host.frame();  // circle c=(60,60) r≈59
  // (15,110): inside the OLD circle (dist≈85.6<99) but outside the new one
  // (dist≈67.3>59) — red here means a stale bake replayed.
  EXPECT_LT(SkColorGetR(host.pixel(15, 110)), 30u);
  EXPECT_GT(SkColorGetR(host.pixel(60, 60)), 200u);  // new body
}

TEST(ComposeSdf, BoundGlowAnimatesWithinReserve) {
  // Alive chrome: bind uGlowR to a ch::Output — the material goes live and
  // the glow breathes with the Output, no render() calls. The style's
  // glowRadius reserves the pad; the binding animates within it.
  choreograph::Output<float> glow{0.01f};
  const material::sdf::Style style{
      .fill = {1, 0, 0, 1}, .glowRadius = 12, .glowColor = {1, 1, 1, 1}};
  Host host;
  host.composer.render(box().child(
      box()
          .width(100)
          .height(100)
          .inset(0, 0, 100, 100)
          .absolute()
          .fill(material::skia::Paint::recipe(
                    material::sdf::material(material::sdf::circle(), style))
                    .uniform("uGlowR", &glow))));
  host.frame();
  // Size the probe from the PUBLIC pad helper (no hand-copied formula):
  // circle radius = 50 − pad; sample 6px outside the edge.
  const int probeX = (int)(50.0f + (50.0f - material::sdf::pad(style)) + 6.0f);
  const uint32_t dim = SkColorGetR(host.pixel(probeX, 50));
  glow = 12.0f;  // brighten the falloff — no re-render
  host.frame();
  const uint32_t lit = SkColorGetR(host.pixel(probeX, 50));
  EXPECT_LT(dim, 25u);  // exp(-6/0.01) ≈ 0
  EXPECT_GT(lit, 80u);  // exp(-6/12) · edge cutoff ≈ 0.51 → ~130
}

TEST(ComposeSdf, PadSwallowingTheBoxWarnsOnceNamingMinBoxFor) {
  // material::sdf::pad() is reserved INSIDE the node's box, so a small box with
  // a large glow radius leaves almost no room for the shape and renders a
  // speck. material::sdf::minBoxFor() gives the size that would fit, but an
  // author who does not already know it exists has no way to find it — so the
  // warning fires where the two numbers first meet, at resolve, and names it.
  const material::sdf::Style style{
      .fill = {1, 0, 0, 1}, .glowRadius = 20, .glowColor = {1, 1, 1, 1}};
  ASSERT_GE(material::sdf::pad(style),
            30.0f);  // the premise: pad >= half of 60
  ::testing::internal::CaptureStderr();
  {
    Host host;
    host.composer.render(box().child(
        box().width(60).height(60).fill(material::skia::Paint::recipe(
            material::sdf::material(material::sdf::circle(), style)))));
    host.frame();
  }
  const std::string first = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(first.find("material::sdf::minBoxFor"), std::string::npos) << first;
  // Warned ONCE, process-wide: a second offender stays silent — the house
  // diagnostic contract (renderSlot's unknown-name warning), not a
  // per-frame log.
  ::testing::internal::CaptureStderr();
  {
    Host host;
    host.composer.render(box().child(
        box().width(50).height(50).fill(material::skia::Paint::recipe(
            material::sdf::material(material::sdf::circle(), style)))));
    host.frame();
  }
  EXPECT_EQ(
      ::testing::internal::GetCapturedStderr().find("material::sdf::minBoxFor"),
      std::string::npos);
}

TEST(ComposePattern, CheckerTilesSeamlessly) {
  // A stock generator baked once and repeated: cells land where the tile
  // math says, across tile boundaries.
  Pattern bg =
      Pattern(material::pattern::checker(10, {1, 0, 0, 1}, {0, 0, 1, 1}));
  Host host;
  host.composer.render(box().child(box()
                                       .width(60)
                                       .height(20)
                                       .inset(0, 0, 140, 180)
                                       .absolute()
                                       .fill(bg.material())));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);    // cell (0,0)
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE);  // cell (1,0)
  EXPECT_EQ(host.pixel(25, 5), SK_ColorRED);   // next tile repeats
  EXPECT_EQ(host.pixel(45, 5), SK_ColorRED);
}

TEST(ComposePattern, HeldPatternPrunesReseedRegenerates) {
  // The identity contract: a HELD pattern re-described is pointer-equal
  // (prunes, no rebake); .seed(n) drops the bake and shows up as exactly
  // one changed recipe.
  Pattern grain =
      Pattern(material::pattern::speckle(64, 40, 1, 3, {{1, 1, 1, 1}}));
  Host host;
  auto tree = [&] {
    return box().child(box().width(80).height(80).fill(grain.material()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());  // same bake → same recipe → prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  grain.seed(7);  // regenerate
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  EXPECT_TRUE(host.composer.dirty());
}

TEST(ComposePattern, ReseedingACopyLeavesTheOriginalAlone) {
  // A Pattern is a VALUE — scale, rotate, offset and sampling are all
  // per-object. seed() and retile() touch the shared recipe, so they must
  // copy on write: editing it in place would drop the original's bake and
  // regenerate every element still drawing the old tile. Same aliasing
  // hazard, and same answer, as binding a uniform on a copied Material.
  Pattern base =
      Pattern(material::pattern::speckle(64, 40, 1, 3, {{1, 1, 1, 1}}));
  base.seed(11);
  auto plate = [&] {
    Host host(64, 64);
    host.composer.render(
        box().child(box().width(64).height(64).fill(base.material())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 64; y += 3)
      for (int x = 0; x < 64; x += 3) px.push_back(host.pixel(x, y));
    return px;
  };
  const std::vector<SkColor> before = plate();

  Pattern copy = base;
  copy.seed(99);
  EXPECT_EQ(base.currentSeed(), 11u) << "the copy re-rolled the original";
  EXPECT_EQ(copy.currentSeed(), 99u);
  EXPECT_EQ(plate(), before) << "and dropped its bake with it";
}

TEST(ComposePattern, AnElementTreeIsATile) {
  // Patterns are compositions: an element tree (two boxes) as the tile.
  Pattern duo = Pattern::tile(
      {20, 10}, box()
                    .row()
                    .child(box().width(10).height(10).fill(red()))
                    .child(box().width(10).height(10).fill(blue())));
  Host host;
  host.composer.render(box().child(box()
                                       .width(40)
                                       .height(10)
                                       .inset(0, 0, 160, 190)
                                       .absolute()
                                       .fill(duo.material(fonts()))));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(25, 5), SK_ColorRED);  // the repeat
}

TEST(ComposePattern, TheGirihEightTileIsAStarAndACross) {
  // The construction is Hankin's polygons-in-contact method on a 4.8.8
  // tiling at θ=45: a khatam star at the tile centre in the star colour, the
  // cross ground at the flanks of each edge midpoint, and the strap ribbon
  // running along the khatam chord. The pixel probes below name those three
  // places, so a pattern that merely looks ornamental will not pass.
  material::kit::GirihPalette pal = material::kit::fezPalette();
  Pattern zellige = material::kit::girih8(24, pal);
  const float s = 24 * (1 + 1.41421356f);  // tile spacing ≈ 57.9
  Host host;
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(0, 0, 80, 80)
                                       .absolute()
                                       .fill(zellige.material())));
  host.frame();
  // Tile center = khatam star fill (blue).
  const SkColor center = host.pixel((int)(s / 2), (int)(s / 2));
  EXPECT_GT(SkColorGetB(center), 100u);
  EXPECT_LT(SkColorGetR(center), 80u);
  // Near the tile corner (inside the corner filler) = ground (teal).
  const SkColor corner = host.pixel(3, 3);
  EXPECT_GT(SkColorGetG(corner), 80u);
  EXPECT_LT(SkColorGetR(corner), 80u);
  EXPECT_LT(SkColorGetB(corner), SkColorGetG(corner));  // teal, not blue
}

// ---- layer styles: the Photoshop route --------------------------------------

TEST(ComposeStyles, BevelLightsAndShadesOpposedEdges) {
  // The fake bevel = two opposed inner shadows: with light from the upper
  // left, the top inner edge reads brighter than the body and the bottom
  // inner edge darker.
  Host host;
  host.composer.render(
      box().child(box()
                      .width(60)
                      .height(60)
                      .inset(0, 0, 140, 140)
                      .absolute()
                      .fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))
                      .foreground(styles::BevelEmboss{.depth = 4, .size = 3})));
  host.frame();
  const uint32_t top = SkColorGetR(host.pixel(30, 2));
  const uint32_t mid = SkColorGetR(host.pixel(30, 30));
  const uint32_t bot = SkColorGetR(host.pixel(30, 57));
  EXPECT_GT(top, mid + 20);  // lit edge
  EXPECT_LT(bot + 20, mid);  // shaded edge
}

TEST(ComposeStyles, AnOverlaySitsOverTheFillAndAStrokeOverBoth) {
  // colorOverlay tints the shape through its blend; .stroke() is fill's
  // ergonomic peer for dressing the outline.
  Host host;
  host.composer.render(
      box().child(box()
                      .width(60)
                      .height(60)
                      .inset(0, 0, 140, 140)
                      .absolute()
                      .fill(Fill::color({0, 0, 1, 1}))
                      .foreground(styles::colorOverlay(
                          {1, 0, 0, 1}, SkBlendMode::kSrcOver, 0.5f))
                      .stroke(sigil::compose::stroke(4, green()))));
  host.frame();
  const SkColor c = host.pixel(30, 30);  // 50% red over blue
  EXPECT_GT(SkColorGetR(c), 90u);
  EXPECT_GT(SkColorGetB(c), 90u);
  EXPECT_EQ(host.pixel(30, 1), SK_ColorGREEN);  // stroked edge
}

TEST(ComposeStyles, BevelBandsEdgesWhenNested) {
  // A bevel must band the node's EDGES wherever the node sits. Implemented
  // as a blurred inverse fill it floods the whole shape once the node is at
  // a non-origin offset inside a cached tree — and a fixture anchored at the
  // origin cannot see that, which is why this one is deliberately nested and
  // offset.
  Host host;
  host.composer.render(box().padding(30).child(box().padding(10).child(
      box()
          .width(60)
          .height(60)
          .fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))
          .foreground(styles::BevelEmboss{.depth = 4, .size = 3}))));
  host.frame();
  host.frame();  // the CACHED replay is the bug's trigger
  const uint32_t top = SkColorGetR(host.pixel(70, 42));
  const uint32_t mid = SkColorGetR(host.pixel(70, 70));
  const uint32_t bot = SkColorGetR(host.pixel(70, 97));
  EXPECT_GT(top, mid + 15);  // lit band
  EXPECT_LT(bot + 15, mid);  // shaded band
  EXPECT_GT(mid, 100u);      // the flood bug washed the body toward white
  EXPECT_LT(mid, 160u);
}

TEST(ComposeStyles, BigSoftShadowSurvivesPictureCaching) {
  // A blurred shadow reaches well past its node, so the recording cull has
  // to grow by the decoration's declared bleed(). Otherwise the shadow draws
  // on the first frame and is truncated by every cached replay after it.
  Host host;
  host.composer.render(box().padding(40).child(
      box()
          .width(60)
          .height(40)
          .background(sigil::compose::shadow({1, 0, 0, 0.9f}, {0, 10}, 20))
          .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))));
  host.frame();
  host.frame();  // cached replay
  // Node spans y∈[40,80); sample 14px below it — the soft red reach.
  EXPECT_GT(SkColorGetR(host.pixel(70, 94)), 25u);
}

TEST(ComposeStyles, OuterGlowHalosOutsideTheShape) {
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(60, 60, 100, 100)
          .absolute()
          .corners({8})
          .background(styles::OuterGlow{.color = {1, 1, 1, 1}, .size = 10})
          .fill(Fill::color({0.2f, 0.2f, 0.2f, 1}))));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(56, 80)), 40u);  // halo 4px outside
  EXPECT_LT(SkColorGetR(host.pixel(30, 80)), 12u);  // fades with distance
}

TEST(ComposePatterns, SequencePaintsColouredRunsAndPhaseSlides) {
  // stripes() is single-colour and cannot be phased, so a multi-colour
  // repeating band — and any animated slide of one — would otherwise be a
  // hand-written pattern program each time.
  auto sample = [](float phase, int x) {
    Host host;
    host.composer.render(box().child(
        box()
            .width(120)
            .height(40)
            .inset(0, 0, 80, 160)
            .absolute()
            .fill(Pattern(material::pattern::sequence({{10, {1, 0, 0, 1}},
                                                       {10, {0, 1, 0, 1}},
                                                       {10, {0, 0, 1, 1}}},
                                                      phase))
                      .material())));
    host.frame();
    return host.pixel(x, 20);
  };
  EXPECT_EQ(sample(0.0f, 5), SK_ColorRED);     // run 1
  EXPECT_EQ(sample(0.0f, 15), SK_ColorGREEN);  // run 2
  EXPECT_EQ(sample(0.0f, 25), SK_ColorBLUE);   // run 3
  EXPECT_EQ(sample(0.0f, 35), SK_ColorRED);    // wraps
  EXPECT_EQ(sample(10.0f, 5), SK_ColorGREEN);  // slid one run: green leads
  EXPECT_EQ(sample(10.0f, 15), SK_ColorBLUE);
}

TEST(ComposeColor, OcioViewTransformsOutputAndClears) {
  // An exponent transform darkens mid-grey (0.5^2.2 ~ 0.218); clearing the
  // view restores pass-through. Exercises bake, response row, the
  // lowering the host's eight-bit surface asks for, and saveLayer.
#ifndef SIGILMATERIAL_ENABLE_OCIO
  GTEST_SKIP() << "built without OpenColorIO, so a view transform is a "
                  "no-op and this proves nothing";
#else
  ASSERT_TRUE(sigil::material::ocio::available());
  Host host;
  host.composer.setView(sigil::material::ocio::exponent(2.2f));
  host.composer.render(box().child(
      box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);
  EXPECT_LT(dark, 80u);
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);
  EXPECT_LT(plain, 138u);
#endif
}
