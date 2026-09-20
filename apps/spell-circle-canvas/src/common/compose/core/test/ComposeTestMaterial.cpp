// What a fill can be and what its recipe prunes on: unit ramps and linear,
// conical and sweep gradients, a null SkSL effect that is loud at build, a
// blend stack folded into one shader, a static material collapsing to a fill,
// a child slot sampling an index texture through a palette, a declared bleed
// growing the recording cull, a blend layer's amount, and a buffer that
// changes between commits without re-describing — and where a unit
// ramp's falloff reaches over however many stops it carries, and what
// a uniform name the effect never declared does.

#include "support/CoreTestSupport.h"

TEST(ComposeMaterial, UnitRampFollowsTheBoxItLandsIn) {
  // linear() is in node-local PIXELS, which an author cannot know for a
  // content-sized box. linearUnit() is in the unit square, so the SAME
  // material reads correctly at two different sizes.
  auto card = [](float w, float h) {
    return box().width(w).height(h).absolute().left(0).top(0).fill(
        material::skia::Paint::linearUnit(
            {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}));
  };
  Host small(80, 40);
  small.composer.render(box().children({card(80, 40)}));
  small.frame();
  EXPECT_GT(SkColorGetR(small.pixel(40, 2)), 180u);   // top is red…
  EXPECT_GT(SkColorGetB(small.pixel(40, 37)), 180u);  // …bottom is blue

  Host tall(80, 300);
  tall.composer.render(box().children({card(80, 300)}));
  tall.frame();
  EXPECT_GT(SkColorGetR(tall.pixel(40, 3)), 180u);
  EXPECT_GT(SkColorGetB(tall.pixel(40, 296)), 180u);
  // and the midpoint is the blend at BOTH sizes, which a pixel-space ramp
  // authored for one of them could not manage
  const SkColor midSmall = small.pixel(40, 20);
  const SkColor midTall = tall.pixel(40, 150);
  EXPECT_NEAR((int)SkColorGetR(midSmall), (int)SkColorGetR(midTall), 24);
  EXPECT_NEAR((int)SkColorGetB(midSmall), (int)SkColorGetB(midTall), 24);
}

TEST(ComposeMaterial, LinearGradientFillPaints) {
  Host host;
  host.composer.render(
      box().children({box()
                          .width(100)
                          .height(20)
                          .inset(0, 0, 100, 180)
                          .absolute()
                          .fill(material::skia::Paint::linear(
                              {0, 0}, {100, 0},
                              {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))}));
  host.frame();
  const SkColor left = host.pixel(2, 10);
  const SkColor right = host.pixel(98, 10);
  EXPECT_GT(SkColorGetR(left), 200u);  // red end
  EXPECT_LT(SkColorGetB(left), 70u);
  EXPECT_GT(SkColorGetB(right), 200u);  // blue end
  EXPECT_LT(SkColorGetR(right), 70u);
}

TEST(ComposeMaterial, ConicalMovesTheHighlightWithoutMovingTheFalloff) {
  // The offset-focus radial. Displacing a plain radial()'s CENTRE slides the
  // whole ramp, so falloff and highlight are one knob and a lit sphere is
  // impossible to spell. conical()
  // (SkShaders::TwoPointConicalGradient) keeps the outer circle put and
  // moves only the focus, which is the sphere-shading primitive.
  const auto sphere = [](SkPoint focus) {
    return box().children(
        {box()
             .width(120)
             .height(120)
             .inset(40, 40, 40, 40)
             .absolute()
             .fill(material::skia::Paint::conical(
                 focus, 0.0f, {60, 60}, 60.0f,
                 {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0.2f, 1}}}))});
  };
  Host centered, offset;
  centered.composer.render(sphere({60, 60}));
  offset.composer.render(sphere({35, 35}));
  centered.frame();
  offset.frame();
  // The highlight visibly MOVES to the focus: at the displaced focus
  // (node-local (35,35) = canvas (75,75)) the offset arm is near-white
  // and clearly brighter than the centered arm at that same pixel…
  EXPECT_GT(SkColorGetR(offset.pixel(75, 75)), 200u);
  EXPECT_GT((int)SkColorGetR(offset.pixel(75, 75)),
            (int)SkColorGetR(centered.pixel(75, 75)) + 40)
      << "the focus offset did not move the highlight";
  // …while at the old centre the ordering reverses.
  EXPECT_GT((int)SkColorGetR(centered.pixel(100, 100)),
            (int)SkColorGetR(offset.pixel(100, 100)) + 40);
  // And the recipe is comparable: identical conicals prune-equal, a moved
  // focus does not (it would freeze the highlight forever), and the
  // conical never aliases the radial it displaces.
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 1, 1, 1}},
                                                {1.0f, {0, 0, 0.2f, 1}}};
  EXPECT_TRUE(
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops));
  EXPECT_FALSE(
      material::skia::Paint::conical({35, 35}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::conical({36, 35}, 0, {60, 60}, 60, stops));
  EXPECT_FALSE(
      material::skia::Paint::conical({60, 60}, 0, {60, 60}, 60, stops) ==
      material::skia::Paint::radial({60, 60}, 60, stops));
}

TEST(ComposeMaterial, SweepWarnsWhenTheWindowLeavesTheCircle) {
  // Skia's sweep CLAMPS outside [startDeg, endDeg] rather than wrapping, so
  // a window wider than the circle — sweep(c, stops, 90, 450), the obvious
  // way to spell a hue wheel starting at red — paints part of the ring in
  // the first stop's flat colour with no error. The factory warns instead.
  //
  // Control first (a legal window must stay silent), then the trap arm. The
  // warning fires once per process, so this must be the only place that
  // triggers it and the order within the test matters.
  const std::vector<material::skia::Stop> stops{{0.0f, {1, 0, 0, 1}},
                                                {1.0f, {0, 0, 1, 1}}};
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sweep({50, 50}, stops, 0.0f, 360.0f);
  (void)material::skia::Paint::sweep({50, 50}, stops, 90.0f, 270.0f);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a window inside the circle must not warn";
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sweep({50, 50}, stops, 90.0f, 450.0f);
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("skia::Paint::sweep"), std::string::npos) << log;
  EXPECT_NE(log.find("wrap"), std::string::npos) << log;
}

TEST(ComposeMaterial, ANullSkslEffectIsLoudAtBuild) {
  // Host & tooling: "a material that fails to build should be loud." The
  // silent half of that failure is MakeForShader returning null and the
  // caller passing it straight in — the node then paints NOTHING with the
  // compile error long scrolled away. Control first: a valid effect stays
  // silent; the null build warns once, at build, not at draw.
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sksl(ukEffect(), {{"uK", 1.0f}});
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a valid effect must not warn";
  ::testing::internal::CaptureStderr();
  (void)material::skia::Paint::sksl(sk_sp<SkRuntimeEffect>(nullptr));
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("skia::Paint::sksl"), std::string::npos) << log;
  EXPECT_NE(log.find("nothing"), std::string::npos) << log;
}

TEST(ComposeMaterial, BlendStackCompositesToOneShader) {
  // Two solids blended kPlus → additive brighten in ONE flattened shader
  // (no saveLayer). red + green = yellow.
  Host host;
  host.composer.render(box().children(
      {box()
           .width(40)
           .height(40)
           .inset(0, 0, 160, 160)
           .absolute()
           .fill(material::skia::Paint::blend({
               {material::skia::Paint::solid({1, 0, 0, 1}),
                SkBlendMode::kSrcOver},
               {material::skia::Paint::solid({0, 1, 0, 1}), SkBlendMode::kPlus},
           }))}));
  host.frame();
  const SkColor c = host.pixel(20, 20);
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_GT(SkColorGetG(c), 200u);
  EXPECT_LT(SkColorGetB(c), 70u);
}

TEST(ComposeMaterial, StaticMaterialCollapsesToFillAndCaches) {
  // A gradient Material is static → collapses to Fill::shader → the parent
  // picture-caches like any static subtree: records once, replays on later
  // draws. (Reconcile-side pruning across re-render is pinned separately by
  // StaticMaterialPrunesAcrossRerender.)
  Host host;
  host.composer.render(box().children(
      {box().width(60).height(60).fill(material::skia::Paint::radial(
          {30, 30}, 30, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}}))}));
  host.frame();  // records
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
  host.frame();  // no re-render — replays the cached picture
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeMaterial, StaticMaterialPrunesAcrossRerender) {
  // Re-describing the SAME material recipe prunes even though every describe
  // builds a fresh SkShader: gradients and blend stacks compare by RECIPE,
  // not by shader pointer. Compare by pointer and a tree like this one
  // re-patches and re-records on every render() with nothing to show for it.
  Host host;
  auto tree = [] {
    return box().children(
        {box().width(60).height(60).fill(material::skia::Paint::linear(
             {0, 0}, {60, 0}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}})),
         box().width(40).height(40).fill(material::skia::Paint::blend({
             {material::skia::Paint::solid({0, 0, 0, 1}),
              SkBlendMode::kSrcOver},
             {material::skia::Paint::radial(
                  {20, 20}, 20, {{0.0f, {0, 1, 0, 1}}, {1.0f, {0, 0, 0, 1}}}),
              SkBlendMode::kPlus},
         }))});
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());  // brand-new shaders, identical recipes
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeMaterial, ChangedRecipeStillInvalidates) {
  // Over-prune guard: a changed ramp color is a different recipe — the node
  // patches and repaints.
  Host host;
  auto tree = [](SkColor4f c) {
    return box().children(
        {box().key("g").width(60).height(60).fill(material::skia::Paint::linear(
            {0, 0}, {60, 0}, {{0.0f, c}, {1.0f, c}}))});
  };
  host.composer.render(tree({1, 0, 0, 1}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree({0, 1, 0, 1}));
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
}

namespace {

/** A 1-row image whose pixels are the given colors (N32, no color space —
 *  the Host surface has none either, so nothing converts and a byte written
 *  here is the byte the shader reads). */
sk_sp<SkImage> rowImage(const std::vector<SkColor>& pixels) {
  SkBitmap bm;
  bm.allocN32Pixels((int)pixels.size(), 1);
  for (size_t i = 0; i < pixels.size(); ++i)
    *bm.getAddr32((int)i, 0) = SkPreMultiplyColor(pixels[i]);
  bm.setImmutable();
  return bm.asImage();
}

/** THE PALETTED SHADER. `uIndex`'s red channel is a palette INDEX, not a
 *  colour; `uPalette` is the 4-entry LUT it selects from. */
sk_sp<SkRuntimeEffect> paletteEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uIndex;"
                 "uniform shader uPalette;"
                 "uniform float uShade;"
                 "half4 main(float2 xy) {"
                 "  float i = floor(uIndex.eval(xy).r * 255.0 + 0.5);"
                 "  i = min(i + uShade, 3.0);"  // X-COM's ramp arithmetic
                 "  return uPalette.eval(float2(i + 0.5, 0.5));"
                 "}"));
    if (!effect) ADD_FAILURE() << err.c_str();
    return effect;
  }();
  return fx;
}

/** The IMAGES are process-wide, the way a decoded asset is: Material::image
 *  compares by image POINTER (the documented recipe rule), so a helper that
 *  minted a fresh SkImage per call would make every material unequal to
 *  every other and the prune question below unaskable. */
const sk_sp<SkImage>& indexImage() {
  static sk_sp<SkImage> img =
      rowImage({SkColorSetARGB(255, 0, 0, 0), SkColorSetARGB(255, 1, 0, 0),
                SkColorSetARGB(255, 2, 0, 0), SkColorSetARGB(255, 3, 0, 0)});
  return img;
}

const sk_sp<SkImage>& rampPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE, SK_ColorWHITE});
  return img;
}

const sk_sp<SkImage>& reversedPalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorWHITE, SK_ColorBLUE, SK_ColorGREEN, SK_ColorRED});
  return img;
}

const sk_sp<SkImage>& flatWhitePalette() {
  static sk_sp<SkImage> img =
      rowImage({SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE, SK_ColorWHITE});
  return img;
}

/** The index texture: four 1-px cells carrying indices 0..3, blown up to
 *  20 px each so a node pixel lands unambiguously inside one cell. NEAREST
 *  everywhere — an index sampled at kLinear is a blend of two unrelated
 *  palette entries, which is the trap this whole texture kind carries. */
material::skia::Paint indexSource() {
  return material::skia::Paint::image(
      indexImage(), SkTileMode::kClamp, SkTileMode::kClamp,
      SkMatrix::Scale(20, 20), SkSamplingOptions(SkFilterMode::kNearest));
}

material::skia::Paint paletteSource(const sk_sp<SkImage>& lut) {
  return material::skia::Paint::image(
      lut, SkTileMode::kClamp, SkTileMode::kClamp, SkMatrix::I(),
      SkSamplingOptions(SkFilterMode::kNearest));
}

}  // namespace

TEST(ComposeMaterial, AChildSlotSamplesAnIndexTextureThroughAPalette) {
  // THE DRIVING CASE, end to end: two images, one shader, one draw. Neither
  // source is the node's own painted content (that is Effect's `content`
  // child) — they are sources the material brings with it.
  Host host(80, 20);
  host.composer.render(stack().children({box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .slot("uIndex", indexSource())
          .slot("uPalette", paletteSource(rampPalette())))}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED) << "index 0";
  EXPECT_EQ(host.pixel(30, 10), SK_ColorGREEN) << "index 1";
  EXPECT_EQ(host.pixel(50, 10), SK_ColorBLUE) << "index 2";
  EXPECT_EQ(host.pixel(70, 10), SK_ColorWHITE) << "index 3";

  // The LUT is the point: re-authoring the palette re-colours the picture
  // without touching the index texture — the paletted-shading trick itself.
  Host swapped(80, 20);
  swapped.composer.render(stack().children({box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .slot("uIndex", indexSource())
          .slot("uPalette", paletteSource(reversedPalette())))}));
  swapped.frame();
  EXPECT_EQ(swapped.pixel(10, 10), SK_ColorWHITE) << "same indices, new LUT";
  EXPECT_EQ(swapped.pixel(70, 10), SK_ColorRED);

  // And the shade step is index ARITHMETIC, clamped at the ramp's end —
  // every cell moves one entry down the palette and the last one sticks.
  Host shaded(80, 20);
  shaded.composer.render(stack().children({box().absolute().inset(0).fill(
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 1.0f}})
          .slot("uIndex", indexSource())
          .slot("uPalette", paletteSource(rampPalette())))}));
  shaded.frame();
  EXPECT_EQ(shaded.pixel(10, 10), SK_ColorGREEN) << "0 + 1";
  EXPECT_EQ(shaded.pixel(50, 10), SK_ColorWHITE) << "2 + 1";
  EXPECT_EQ(shaded.pixel(70, 10), SK_ColorWHITE) << "3 + 1, clamped";
}

TEST(ComposeMaterial, TheChildRidesThePruneSignature) {
  // THE CACHE CONDITION. Anything read at paint time must participate in
  // reconciler equality. A child that does not leaves a pruned node sampling
  // the OLD palette forever, with no diagnostic and a picture that looks
  // deliberate.
  const material::skia::Paint a =
      material::skia::Paint::sksl(paletteEffect())
          .slot("uPalette", paletteSource(rampPalette()));
  const material::skia::Paint b =
      material::skia::Paint::sksl(paletteEffect())
          .slot("uPalette", paletteSource(rampPalette()));
  const material::skia::Paint c =
      material::skia::Paint::sksl(paletteEffect())
          .slot("uPalette", paletteSource(flatWhitePalette()));
  const material::skia::Paint bare =
      material::skia::Paint::sksl(paletteEffect());
  EXPECT_TRUE(a == b) << "same effect, same child recipe → prunes";
  EXPECT_FALSE(a == c) << "a different palette is a different material";
  EXPECT_FALSE(a == bare) << "a filled slot is not an empty one";

  // …and the reconciler agrees: identical describe prunes, a swapped
  // palette patches and repaints.
  Host host(80, 20);
  auto tree = [](const sk_sp<SkImage>& lut) {
    return stack().children({box().key("lut").absolute().inset(0).fill(
        material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
            .slot("uIndex", indexSource())
            .slot("uPalette", paletteSource(lut)))});
  };
  host.composer.render(tree(rampPalette()));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED);
  host.composer.render(
      tree(rampPalette()));  // identical recipe, fresh Materials…
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(flatWhitePalette()));
  EXPECT_TRUE(host.composer.dirty()) << "over-prune guard";
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorWHITE);
}

TEST(ComposeMaterial, ALiveChildMakesTheParentLive) {
  // TIER INHERITANCE, upward. The parent effect declares no uniform of its
  // own and no clock: everything volatile about it belongs to the child.
  // If the tier did not propagate, the parent would collapse to a Fill and
  // freeze the child at whatever the Output read on the frame it recorded.
  static const sk_sp<SkRuntimeEffect> passthrough = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uSrc;"
                 "half4 main(float2 p) { return uSrc.eval(p); }"));
    return fx;
  }();
  ASSERT_TRUE(passthrough);
  choreograph::Output<float> k{0.0f};
  const material::skia::Paint live =
      material::skia::Paint::sksl(passthrough)
          .slot("uSrc",
                material::skia::Paint::sksl(ukEffect()).uniform("uK", &k));
  EXPECT_TRUE(live.isAnimated()) << "the child's volatility is the parent's";
  EXPECT_FALSE(material::skia::Paint::sksl(passthrough)
                   .slot("uSrc", material::skia::Paint::solid({0, 1, 0, 1}))
                   .isAnimated())
      << "…and a static child leaves the parent static";

  Host host;
  host.composer.render(stack().children(
      {box().absolute().inset(0).width(40).height(40).fill(live)}));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);  // uK = 0 → black
  k = 1.0f;                                         // no render()
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u) << "uK = 1 → red";
}

TEST(ComposeMaterial, AGeometryChildPropagatesTheGeometryTier) {
  // TIER INHERITANCE, the cheaper half: a child reading uResolution needs
  // the PaintContext at RECORD time but not every frame, and it must read
  // the parent NODE's box (there is one box here, not two).
  static const sk_sp<SkRuntimeEffect> passthrough = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uSrc;"
                 "half4 main(float2 p) { return uSrc.eval(p); }"));
    return fx;
  }();
  static const sk_sp<SkRuntimeEffect> unitRamp = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float2 uResolution;"
                 "half4 main(float2 p) {"
                 "  return half4(half(p.x / max(uResolution.x, 1.0)), 0, 0, 1);"
                 "}"));
    return fx;
  }();
  ASSERT_TRUE(passthrough && unitRamp);
  const material::skia::Paint m =
      material::skia::Paint::sksl(passthrough)
          .slot("uSrc", material::skia::Paint::sksl(unitRamp));
  EXPECT_TRUE(m.geometryDependent()) << "the child's tier is the parent's";
  EXPECT_FALSE(m.isAnimated()) << "geometry is not live";

  Host host(100, 20);
  host.composer.render(stack().children({box().absolute().inset(0).fill(m)}));
  host.frame();
  // The ramp spans the node's own width: dark at the left edge, bright at
  // the right. A child resolved with a null context would read uResolution
  // as 0 and clamp to full red everywhere.
  EXPECT_LT(SkColorGetR(host.pixel(2, 10)), 40u);
  EXPECT_GT(SkColorGetR(host.pixel(97, 10)), 200u);
}

TEST(ComposeMaterial, AnUndeclaredChildNameIsIgnored) {
  // uniform()'s guardrail, verbatim: assigning a child the effect does not
  // declare SkDEBUGFAILs, which would kill the hot-reload host over one
  // typo. Warn, ignore, keep painting.
  Host host(80, 20);
  material::skia::Paint m =
      material::skia::Paint::sksl(paletteEffect(), {{"uShade", 0.0f}})
          .slot("uIndex", indexSource())
          .slot("uPalette", paletteSource(rampPalette()))
          .slot("uNoSuchSlot", material::skia::Paint::solid({1, 1, 1, 1}));
  EXPECT_FALSE(m.isAnimated());
  host.composer.render(stack().children({box().absolute().inset(0).fill(m)}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 10), SK_ColorRED) << "the declared slots still ran";

  // And on a material with no slots at all it is a no-op, like uniform().
  material::skia::Paint solid =
      material::skia::Paint::solid({0, 1, 0, 1}).slot("uSrc", indexSource());
  EXPECT_TRUE(solid.isSolid());
  EXPECT_FALSE(solid.isAnimated());
}

// ---- Pattern: runtime-procedural regenerable tiles --------------------------

TEST(ComposeMaterial, DeclaredBleedGrowsTheRecordingCull) {
  // A decoration declares how far it escapes the node with bleed(), and a
  // Material needs the same word: a fill on a shape() that overflows the box
  // — which is legal — is otherwise truncated at the cached bounds, leaving
  // the caller to pad the node by hand.
  //
  // Both carriers are checked, the static recipe and the live slot.
  // Cache::Texture makes the truncation hard rather than merely likely: the
  // bake surface is exactly recordBounds, so anything outside it cannot
  // survive by accident.
  auto overflowShape = [](SkSize s) {
    // A disc centered on the box, poking 20px beyond every edge.
    SkPathBuilder b;
    b.addOval(SkRect::MakeLTRB(-20, -20, s.width() + 20, s.height() + 20));
    return b.detach();
  };
  {
    Host host;  // recipe carrier: a static solid material
    host.composer.render(box().padding(40).children(
        {box()
             .width(60)
             .height(40)
             .cache(Cache::Texture)
             .shape(overflowShape)
             .fill(material::skia::Paint::solid({1, 0, 0, 1}).bleed(24))}));
    host.frame();
    host.frame();  // the cached replay is where a small cull would bite
    // Node spans y∈[40,80); 14px below is inside the disc's overflow.
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  {
    Host host;  // live carrier: a geometry-tier material (uResolution ramp)
    host.composer.render(box().padding(40).children(
        {box()
             .width(60)
             .height(40)
             .cache(Cache::Texture)
             .shape(overflowShape)
             .fill(material::skia::Paint::linearUnit(
                       {0, 0}, {1, 1}, {{0, {1, 0, 0, 1}}, {1, {1, 0, 0, 1}}})
                       .bleed(24))}));
    host.frame();
    host.frame();
    EXPECT_EQ(host.pixel(70, 94), SK_ColorRED);
  }
  // The reserve is recipe: it participates in equality, so a changed
  // bleed re-records instead of replaying a stale, smaller cull.
  material::skia::Paint a = material::skia::Paint::solid({1, 0, 0, 1});
  material::skia::Paint b = material::skia::Paint::solid({1, 0, 0, 1});
  b.bleed(24);
  EXPECT_FALSE(a == b);
  EXPECT_TRUE(a == material::skia::Paint::solid({1, 0, 0, 1}));
  EXPECT_FLOAT_EQ(b.bleed(), 24.0f);
}

// ---------------------------------------------------------------------------
// Material::amount(): a blend layer's strength.

TEST(ComposeMaterial, ABlendLayerCompositesAtItsAmount) {
  // "Soft-light this noise at 30%" had no expression — the only route was
  // baking the amplitude into a forked copy of the generator's SkSL.
  // amount() is Photoshop layer opacity: composite the layer in full,
  // then mix the RESULT back toward the accumulation — which on a
  // srcOver white-over-red at 0.5 lands on pink, and at 0 leaves red.
  auto plate = [](float amt) {
    Host host;
    host.composer.render(box().children(
        {box()
             .width(60)
             .height(60)
             .inset(0, 0, 140, 140)
             .absolute()
             .fill(material::skia::Paint::blend(
                 {{material::skia::Paint::solid({1, 0, 0, 1}),
                   SkBlendMode::kSrcOver},
                  {material::skia::Paint::solid({1, 1, 1, 1}).amount(amt),
                   SkBlendMode::kSrcOver}}))}));
    host.frame();
    return host.pixel(30, 30);
  };
  const SkColor full = plate(1.0f), half = plate(0.5f), none = plate(0.0f);
  EXPECT_GT(SkColorGetG(full), 240u);       // white wins outright
  EXPECT_NEAR(SkColorGetG(half), 128, 12);  // half toward white…
  EXPECT_GT(SkColorGetR(half), 240u);       // …with red intact
  EXPECT_LT(SkColorGetG(none), 12u);        // 0 leaves the base
  EXPECT_GT(SkColorGetR(none), 240u);

  // The amount is recipe: equal amounts prune, different amounts patch.
  const material::skia::Paint a =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.3f);
  const material::skia::Paint b =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.3f);
  const material::skia::Paint c =
      material::skia::Paint::solid({1, 1, 1, 1}).amount(0.7f);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
}

// ---------------------------------------------------------------------------
// Material::buffer: content that changes without re-describing.

TEST(ComposeMaterial, ABufferPrunesBetweenCommitsAndPatchesOnCommit) {
  // The Instances pruning rule, on pixels: identical re-describes prune
  // while the revision holds; one commit() patches exactly once. Before
  // this seam, anything with STATE — a simulation, a video frame, a
  // scrollback — fell to custom() + Cache::None and forfeited every
  // cache and decoration slot on the node.
  auto src = std::make_shared<sigil::material::skia::PixelBuffer>(40, 40);
  src->canvas().clear(SkColorSetARGB(255, 255, 0, 0));  // red frame
  src->commit();
  Host host;
  auto tree = [&] {
    return box().children({box()
                               .width(100)
                               .height(100)
                               .inset(0, 0, 100, 100)
                               .absolute()
                               .fill(material::skia::Paint::buffer(src))});
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);

  host.composer.render(tree());  // same revision: prune
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an uncommitted buffer re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  src->canvas().clear(SkColorSetARGB(255, 0, 0, 255));  // new frame…
  src->commit();                                        // …published
  host.composer.render(tree());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a commit must patch its node";
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorBLUE);

  // …and the new revision is itself stable.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

// ---------------------------------------------------------------------------
// SurfacePaint's two collapses — what a slot that stores ONE value keeps
// when it is handed the whole union.

TEST(ComposeMaterial, ACollapsedFillAnswersNothingRatherThanAnEmptyFill) {
  using material::skia::Paint;
  const std::vector<material::skia::Stop> ramp{{0.0f, {1, 0, 0, 1}},
                                               {1.0f, {0, 0, 1, 1}}};
  // What a Fill slot can hold: a plain fill, references and all, and a
  // paint that is one colour or one shader whatever frame it lands in.
  EXPECT_EQ(SurfacePaint{green()}.collapsedFill(), green());
  EXPECT_EQ(SurfacePaint{Fill::currentInk()}.collapsedFill(),
            Fill::currentInk());
  EXPECT_EQ(SurfacePaint{Fill::var("accent")}.collapsedFill(),
            Fill::var("accent"));
  EXPECT_EQ(SurfacePaint{Fill::none()}.collapsedFill(), Fill::none());
  EXPECT_EQ(SurfacePaint{Paint::solid({0, 1, 0, 1})}.collapsedFill(), green());
  EXPECT_EQ(
      SurfacePaint{Paint::linear({0, 0}, {10, 0}, ramp)}.collapsedFill()->kind,
      Fill::Kind::Shader);

  // And what it cannot: the two tiers whose colour is the frame's. They
  // answer NOTHING, never `Fill::none()` — an empty fill is a colour of
  // its own at every painter that reads one, and answering it would
  // paint a rule or a glyph outline black where a gradient was asked
  // for. The caller says what it paints instead.
  EXPECT_FALSE(
      SurfacePaint{Paint::linearUnit({0, 0}, {1, 0}, ramp)}.collapsedFill());
  auto live = std::make_shared<choreograph::Output<Fill>>(green());
  EXPECT_FALSE(SurfacePaint{motion::Animatable<Fill>{live}}.collapsedFill());
}

TEST(ComposeMaterial, ACollapsedPaintSeparatesNothingFromWhatItCannotStore) {
  using material::skia::Paint;
  // A paint slot resolves without the tree and without a binding's
  // identity, so a colour and a paint pass…
  EXPECT_TRUE(SurfacePaint{green()}.collapsedPaint()->isSolid());
  EXPECT_EQ(SurfacePaint{Paint::solid({0, 1, 0, 1})}.collapsedPaint(),
            Paint::solid({0, 1, 0, 1}));
  // …while both of the cascade's references and a bound fill do not.
  EXPECT_FALSE(SurfacePaint{Fill::currentInk()}.collapsedPaint());
  EXPECT_FALSE(SurfacePaint{Fill::var("accent")}.collapsedPaint());
  auto live = std::make_shared<choreograph::Output<Fill>>(green());
  EXPECT_FALSE(SurfacePaint{motion::Animatable<Fill>{live}}.collapsedPaint());
  // An empty paint is the one spelling that MEANS nothing, and `none()`
  // is what tells a caller which of the two it was handed.
  EXPECT_FALSE(SurfacePaint{Fill::none()}.collapsedPaint());
  EXPECT_TRUE(SurfacePaint{Fill::none()}.none());
  EXPECT_FALSE(SurfacePaint{Fill::currentInk()}.none());
}

TEST(ComposeMaterial, AnEmptySurfacePaintLeavesAFillAndAnEmptyFillClearsIt) {
  // A component prop that was never given is an empty SurfacePaint, and
  // it leaves the element's own fill standing: that is what lets a
  // caller state a default and a component override it. Saying NO FILL
  // is a different sentence, spelled with the fill itself — so a
  // conversion that only ever APPLIES cannot carry both, and the verb
  // that takes "nothing" from an author has to state the empty one.
  Host standing(40, 40);
  standing.composer.render(box().children(
      {box().width(40).height(40).absolute().left(0).top(0).fill(red()).fill(
          SurfacePaint{})}));
  standing.frame();
  EXPECT_EQ(standing.pixel(20, 20), SK_ColorRED);

  Host cleared(40, 40);
  cleared.composer.render(box().children(
      {box().width(40).height(40).absolute().left(0).top(0).fill(red()).fill(
          Fill::none())}));
  cleared.frame();
  EXPECT_EQ(cleared.pixel(20, 20), SK_ColorBLACK);
}

// -------------------------------------------------------------------------
// The unit ramps as a fill takes them: where the falloff reaches, and
// how many stops one carries.

TEST(ComposeMaterials, GlowUnitReachesTheInscribedCircleNotTheCorners) {
  // radialUnit's radius is a fraction of the box's HALF-DIAGONAL, so a soft
  // round light authored at radius 1 has not finished falling off where the
  // INSCRIBED circle is — and on a node also carrying
  // geometry::shapes::circle() the remaining alpha becomes a visible hard rim.
  // glowUnit is radialUnit scaled to the inscribed circle instead, so radius 1
  // reaches zero exactly at the edge that gets clipped.
  const std::vector<material::skia::Stop> ramp = {{0.0f, {1, 1, 1, 1}},
                                                  {1.0f, {0, 0, 0, 1}}};
  auto edgeValue = [&](material::skia::Paint m) {
    Host host(200, 200);
    host.composer.render(
        box().children({box().absolute().inset(0).fill(std::move(m))}));
    host.frame();
    // Just inside the box edge, on the horizontal centre line — where the
    // inscribed circle touches.
    return SkColorGetR(host.pixel(197, 100));
  };

  // radialUnit(…, 1.0) is still bright at the inscribed circle, because
  // its ramp does not reach black until the corners.
  EXPECT_GT(
      edgeValue(material::skia::Paint::radialUnit({0.5f, 0.5f}, 1.0f, ramp)),
      40);
  // glowUnit(…, 1.0) has landed by then. That is the whole difference.
  EXPECT_LT(
      edgeValue(material::skia::Paint::glowUnit({0.5f, 0.5f}, 1.0f, ramp)), 8);
  // And the old spelling of the same thing still works, which is what
  // makes this a convenience rather than a behaviour change.
  EXPECT_LT(
      edgeValue(material::skia::Paint::radialUnit({0.5f, 0.5f}, 0.7071f, ramp)),
      8);
}

TEST(ComposeMaterials, UnitRampsTakeAnyNumberOfStops) {
  // A fixed stop count with the tail clamped runs out from both directions
  // — a many-run repeating sett, a long chromatic sweep — and the only way
  // out is a hand-written pattern program. The count is baked into the
  // shader source instead, with one effect cached per count, which is the
  // same rule the noise generators follow for octaves.
  auto sweep = [](int n) {
    std::vector<material::skia::Stop> stops;
    for (int i = 0; i < n; ++i) {
      const float t = (float)i / (float)(n - 1);
      // A sawtooth the six-stop version could not have represented:
      // alternating black and white at every step.
      const float v = (i % 2) ? 1.0f : 0.0f;
      stops.push_back({t, {v, v, v, 1}});
    }
    Host host(256, 32);
    host.composer.render(box().children({box().absolute().inset(0).fill(
        material::skia::Paint::linearUnit({0, 0}, {1, 0}, stops))}));
    host.frame();
    // Count the light/dark transitions across the middle scanline.
    int flips = 0;
    bool light = SkColorGetR(host.pixel(0, 16)) > 128;
    for (int x = 1; x < 256; ++x) {
      const bool now = SkColorGetR(host.pixel(x, 16)) > 128;
      flips += now != light;
      light = now;
    }
    return flips;
  };

  // Six stops = five alternations. Twenty-four and seventy-two scale with
  // the count, which is exactly what the fixed version could not do.
  EXPECT_NEAR(sweep(6), 5, 1);
  EXPECT_NEAR(sweep(24), 23, 2);
  EXPECT_NEAR(sweep(72), 71, 4);

  // Degenerate counts still behave.
  Host one(64, 64);
  one.composer.render(box().children(
      {box().absolute().inset(0).fill(material::skia::Paint::linearUnit(
          {0, 0}, {1, 0}, {{0.0f, {1, 0, 0, 1}}}))}));
  one.frame();
  EXPECT_GT(SkColorGetR(one.pixel(32, 32)), 200);
}

// -------------------------------------------------------------------------
// What a uniform name the effect does not declare does.

TEST(ComposeMaterial, UnknownUniformNamesWarnAndIgnore) {
  // A typo'd uniform name must never abort (SkDEBUGFAIL kills the sketch
  // host in debug): unknown names are warned and dropped, at sksl() and at
  // uniform(), constant and bound alike.
  material::skia::Paint m =
      material::skia::Paint::sksl(ukEffect(), {{"uTypo", 1.0f}});
  choreograph::Output<float> o{1.0f};
  m.uniform("uAlsoMissing", &o);  // dropped → still not live
  EXPECT_FALSE(m.isAnimated());
  Host host;
  host.composer.render(box().children(
      {box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)}));
  host.frame();  // paints with uK at its SkSL default (0) — and does not crash
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 40u);
}
