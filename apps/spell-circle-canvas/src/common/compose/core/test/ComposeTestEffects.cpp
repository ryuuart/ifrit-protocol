// The effect seam: a layer filter and a backdrop, an effect baked into a
// texture and never lifted off one, live uniforms animating without a
// re-describe, static chains pruning by recipe, directional blur at any
// angle, a parameter map varying the blur across the node, a second declared
// shader slot filled by a child, and the warnings an undeclared name earns.

#include <include/core/SkColorFilter.h>
#include <include/core/SkStream.h>
#include <include/effects/SkImageFilters.h>
#include <sigilimage/asset/ImageAsset.h>

#include "support/CoreTestSupport.h"

TEST(ComposeEffects, LayerEffectBlursNode) {
  Host host;
  host.composer.render(
      box().child(box()
                      .width(60)
                      .height(60)
                      .inset(70, 70, 70, 70)
                      .absolute()
                      .fill(red())
                      .effect(material::skia::Effect::filter(
                          SkImageFilters::Blur(8, 8, nullptr)))));
  host.frame();
  // Blur bleeds outside the crisp box bounds and softens the center edge.
  SkColor outside = host.pixel(64, 100);  // 6px outside the left edge
  EXPECT_NE(outside, SK_ColorBLACK);
  EXPECT_NE(host.pixel(100, 100), SK_ColorBLACK);  // center still red-ish
  // Far away stays untouched.
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLACK);
}

TEST(ComposeEffects, BackdropFiltersWhatIsBeneath) {
  Host host;
  // Invert color matrix as a deterministic backdrop filter.
  float invert[20] = {-1, 0, 0,  0, 1, 0, -1, 0, 0, 1,
                      0,  0, -1, 0, 1, 0, 0,  0, 1, 0};
  auto invertFilter =
      SkImageFilters::ColorFilter(SkColorFilters::Matrix(invert), nullptr);

  host.composer.render(
      stack()
          .child(box().inset(0).fill(red()))
          .child(box()
                     .width(80)
                     .height(80)
                     .inset(60, 60, 60, 60)
                     .absolute()
                     .backdrop(material::skia::Effect::filter(invertFilter))));
  host.frame();
  EXPECT_EQ(host.pixel(100, 100), SK_ColorCYAN);  // red inverted inside
  EXPECT_EQ(host.pixel(20, 100), SK_ColorRED);    // untouched outside
}

TEST(ComposeEffects, TextureBakesEffectOnce) {
  // ONCE is asserted literally, through `texturesBaked` — the per-draw
  // pixel-bake count: one on the frame that bakes, zero on every frame
  // after. A live texture count and a non-black pixel would look identical
  // on a node that re-bakes every single frame.
  //
  // profiledUnder(), not a plain parent: under a cacheable parent the second
  // frame replays the PARENT's picture and never visits this node at all, so
  // "0 bakes" would be true of a node that re-bakes every time it is asked.
  // Cache::None on the wrapper keeps the subject painted every frame, which
  // is what makes the second assertion a statement about the texture.
  Host host;
  host.composer.render(
      profiledUnder(box()
                        .key("bloomed")
                        .width(60)
                        .height(60)
                        .fill(green())
                        .effect(material::skia::Effect::filter(
                            SkImageFilters::Blur(4, 4, nullptr)))
                        .cache(Cache::Texture)));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the bake";
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "…and the second frame blits it rather than re-baking";
  EXPECT_GE(host.composer.stats().texturesLive, 1u);
  EXPECT_NE(host.pixel(30, 30), SK_ColorBLACK);  // filtered content present
}

namespace {
/** A sigma map: the blur's full range at the top edge, none at the bottom.
 *  Any map will do here — what these cases are about is the PARAMETER
 *  moving, and a map is what makes an effect carry one at all. */
material::skia::Paint sigmaMap() {
  return material::skia::Paint::linearUnit(
      {0, 0}, {0, 1}, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}});
}
}  // namespace

TEST(ComposeEffects, ALiveLayerEffectOverStaticContentFiltersOneBake) {
  // The node's ONLY volatility is its effect's bound parameter, so the
  // content under the effect is static: it is rasterized once with the
  // effect left out, and the effect runs over that one image at every
  // blit. ONCE is asserted literally through `texturesBaked` — a node
  // re-rasterizing its content into a fresh layer every frame would look
  // identical in any still.
  Host host;
  choreograph::Output<float> maxSigma{1.0f};
  host.composer.render(profiledUnder(
      box().key("racked").width(60).height(60).fill(green()).effect(
          material::skia::Effect::blur(sigmaMap(), 14.0f)
              .uniform("maxSigma", &maxSigma))));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u)
      << "the content, baked once with the effect left out of it";
  const std::vector<SkColor> sharp = grab(host);

  maxSigma = 14.0f;
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "…and the parameter moving re-filters that bake rather than "
         "re-baking it";
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u)
      << "…and re-records nothing either";
  EXPECT_NE(grab(host), sharp)
      << "the effect is live: the parameter moved and the picture did";

  maxSigma = 3.0f;
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
      << "the bake is held across every value the parameter takes";
}

TEST(ComposeEffects, ALiveBackdropEffectIsNeverLiftedOffABake) {
  // The exception, and the reason the tier is stated per effect rather
  // than per node: a backdrop effect reads what is already on the canvas,
  // and a bake holds none of that — filtering one would sample the bake's
  // own transparent black. So the node stays live however static its
  // content is.
  Host host;
  choreograph::Output<float> maxSigma{1.0f};
  // A ground with an EDGE in it: blurring a flat field changes no pixel,
  // so the parameter would look dead however live it was.
  host.composer.render(
      stack()
          .child(box().inset(0).fill(red()))
          .child(box()
                     .width(100)
                     .height(200)
                     .inset(0, 0, 0, 100)
                     .absolute()
                     .fill(green()))
          .child(box()
                     .key("well")
                     .width(80)
                     .height(80)
                     .inset(60, 60, 60, 60)
                     .absolute()
                     .backdrop(material::skia::Effect::blur(sigmaMap(), 14.0f)
                                   .uniform("maxSigma", &maxSigma))));
  host.frame();
  const unsigned baked = host.composer.stats().texturesBaked;
  const std::vector<SkColor> sharp = grab(host);
  maxSigma = 14.0f;
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, baked)
      << "a backdrop effect holds no bake to be re-filtered";
  EXPECT_NE(grab(host), sharp) << "and it still tracks its parameter";
}

// ---------------------------------------------------------------------------
// Effect live uniforms: the same uniform(name, &output) contract Material
// offers, on the effect seam.

TEST(ComposeEffects, ALiveUniformAnimatesWithoutRedescribe) {
  // With constant uniforms only, animating a ripple phase or a bloom
  // threshold costs a full re-describe every frame. A bound uniform resolves
  // per paint and declares the node volatile —
  // exactly the live-material contract.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(60)
          .height(60)
          .inset(0, 0, 140, 140)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::shader(effect).uniform("uK", &k))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(30, 30)), 200u);  // uK=1 → full green
  k = 0.25f;     // move the bound uniform — NO re-describe
  host.frame();  // the live effect re-resolves from k
  const SkColor dimmed = host.pixel(30, 30);
  EXPECT_LT(SkColorGetG(dimmed), 120u);
  EXPECT_GT(SkColorGetG(dimmed), 20u);               // dimmed, not gone
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)  // volatile: paints live
      << "a bound effect uniform must declare volatility";
}

TEST(ComposeEffects, AStaticShaderEffectPrunesByRecipe) {
  // The other half of the seam: a STATIC shader effect compares by RECIPE —
  // the runtime-effect pointer plus its constant uniforms — so a caller that
  // holds one SkRuntimeEffect and re-describes around it prunes. Comparing
  // the built filter pointer instead would re-patch every frame, since a
  // fresh filter is built each time.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  Host host;
  auto tree = [&](float uK) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::shader(effect, {{"uK", uK}})));
  };
  host.composer.render(tree(0.5f));
  host.frame();
  host.composer.render(
      tree(0.5f));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical shader-effect recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // …and the equality is honest: a different constant IS a change.
  host.composer.render(tree(0.9f));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeEffects, LiveChainsRecomposeAndStaticChainsStayCheap) {
  // then() precomposes static sides once (unchanged behaviour); a chain
  // with a live side re-composes per paint and stays honest about it.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform float uK;"
               "half4 main(float2 p) { return content.eval(p) * uK; }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{1.0f};
  const material::skia::Effect liveChain =
      material::skia::Effect::shader(effect).uniform("uK", &k).then(
          material::skia::Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_TRUE(liveChain.isAnimated());
  ASSERT_TRUE(liveChain.resolvedImageFilter() != nullptr);
  const material::skia::Effect staticChain =
      material::skia::Effect::shader(effect, {{"uK", 0.5f}})
          .then(material::skia::Effect::shader(effect, {{"uK", 0.5f}}));
  EXPECT_FALSE(staticChain.isAnimated());
  EXPECT_TRUE(staticChain.imageFilter() != nullptr);  // precomposed once

  // The chain applies BOTH stages: 1.0 * 0.5 through the live chain dims
  // a green fill to about half.
  Host host;
  host.composer.render(box().child(box()
                                       .width(60)
                                       .height(60)
                                       .inset(0, 0, 140, 140)
                                       .absolute()
                                       .fill(green())
                                       .effect(liveChain)));
  host.frame();
  const unsigned g = SkColorGetG(host.pixel(30, 30));
  EXPECT_GT(g, 90u);
  EXPECT_LT(g, 170u);
}

// ---------------------------------------------------------------------------
// Effect::directionalBlur — one spelling for an anisotropic blur at any
// angle, built entirely from filters Skia already has.

TEST(ComposeEffects, ADirectionalBlurAtAnAxisAngleIsBlurBitwise) {
  // At an axis-aligned angle directionalBlur must BE the plain
  // SkImageFilters::Blur call it replaces — same factory, same arguments —
  // so a caller who already wrote the Blur by hand gets identical pixels.
  // Compared pixel-for-pixel over the whole plate.
  auto plate = [](Host& host, material::skia::Effect e) {
    host.composer.render(box().child(box()
                                         .width(60)
                                         .height(60)
                                         .inset(70, 70, 70, 70)
                                         .absolute()
                                         .fill(green())
                                         .effect(std::move(e))));
    host.frame();
  };
  Host ported, hand, swapped;
  plate(ported, material::skia::Effect::directionalBlur(26, 90, 14));
  plate(hand,
        material::skia::Effect::filter(SkImageFilters::Blur(14, 26, nullptr)));
  EXPECT_TRUE(identicalPixels(ported, hand, 200, 200))
      << "directionalBlur(26, 90, 14) must BE Blur(14, 26)";
  // The control that keeps the pin honest: swapped sigmas are a
  // different picture, and this comparison can see it.
  plate(swapped,
        material::skia::Effect::filter(SkImageFilters::Blur(26, 14, nullptr)));
  EXPECT_FALSE(identicalPixels(ported, swapped, 200, 200));
}

TEST(ComposeEffects, ADirectionalBlurAtAnArbitraryAngleSmearsAlongIt) {
  // Any other angle is a rotate → Blur → unrotate sandwich — three filter
  // nodes Skia already provides, no new SkSL. A 45° streak on a centred
  // square throws ink down-right along the smear axis and none the same
  // distance across it, which is what the two probes below read.
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(80, 80, 80, 80)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::directionalBlur(18, 45))));
  host.frame();
  const unsigned along = SkColorGetG(host.pixel(125, 125));
  const unsigned acrossAxis = SkColorGetG(host.pixel(75, 125));
  EXPECT_GT(along, 40u);       // the streak reaches down-right
  EXPECT_LT(acrossAxis, 10u);  // nothing rides across the axis
  EXPECT_GT(along, acrossAxis * 4 + 8);
}

TEST(ComposeEffects, AStaticDirectionalBlurPrunesByRecipe) {
  // filter() compares by pointer, so the hand-built sites re-patched on
  // every describe. The recipe compares structurally: a re-described
  // equal directionalBlur prunes, and the equality is honest about a
  // changed angle.
  Host host;
  auto tree = [&](float angle) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::directionalBlur(12, angle, 4)));
  };
  host.composer.render(tree(30));
  host.frame();
  host.composer.render(tree(30));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical directionalBlur recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(75));  // a different angle IS a change
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(ComposeEffects, ABoundDirectionalBlurAngleAnimatesWithoutRedescribe) {
  // Live parameters ride the same uniform channel a Material uses: the
  // recipe's named parameters accept a bound Output, and the rotate/blur/
  // unrotate sandwich is rebuilt per paint. So an animated smear angle needs
  // no new mechanism and no re-describe — which is the difference between
  // animating it and faking it with a stack of pre-baked gradients.
  choreograph::Output<float> angle{0.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(80, 80, 80, 80)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::directionalBlur(18, 0).uniform(
              "angle", &angle))));
  host.frame();
  // angle 0: the streak runs horizontally — ink right of the box, a
  // sharp edge below it.
  EXPECT_GT(SkColorGetG(host.pixel(125, 100)), 60u);
  EXPECT_LT(SkColorGetG(host.pixel(100, 125)), 20u);
  angle = 90.0f;  // move the bound parameter — NO re-describe
  host.frame();   // the live effect re-resolves and the streak turns
  EXPECT_GT(SkColorGetG(host.pixel(100, 125)), 60u);
  EXPECT_LT(SkColorGetG(host.pixel(125, 100)), 20u);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a bound directionalBlur parameter must declare volatility";
}

TEST(ComposeEffects, AnUnknownDirectionalBlurUniformIsIgnoredNotLive) {
  // The same guardrail Material applies: a name that is not
  // "sigma"/"angle"/"across" warns and is IGNORED. Two things must follow —
  // it does not bind, and it does not silently declare the node volatile,
  // which would repaint every frame forever over a typo.
  choreograph::Output<float> v{1.0f};
  const material::skia::Effect typo =
      material::skia::Effect::directionalBlur(10, 0).uniform("sgima", &v);
  EXPECT_FALSE(typo.isAnimated());
  // The control: a real parameter name does bind.
  const material::skia::Effect bound =
      material::skia::Effect::directionalBlur(10, 0).uniform("sigma", &v);
  EXPECT_TRUE(bound.isAnimated());
}

namespace {

/** A hard-edged test target: 8px vertical stripes in the node's OWN local
 *  space. Blur is measured as the loss of stripe contrast, and stripes
 *  make that loss readable at a pixel pair instead of over an edge
 *  profile. Static (no uniforms), so it never perturbs volatility. */
material::skia::Paint stripeFill() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float band = mod(floor(p.x / 8.0), 2.0);"
                 "  return band < 1.0 ? half4(1) : half4(0, 0, 0, 1);"
                 "}"));
    return effect;
  }();
  return material::skia::Paint::sksl(fx);
}

/** THE PARAMETER: 0 at the node's left edge, 1 at its right, authored in
 *  the UNIT SQUARE — which is the point of using a Material as the
 *  carrier, because the box here is decided by the layout. */
material::skia::Paint focalRamp() {
  return material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

/** Local stripe contrast at canvas x (a stripe centre) — 0 is fully
 *  washed out, 255 fully sharp. The pair straddles one stripe boundary. */
int contrastAt(Host& host, int x, int y) {
  const SkColor a = host.pixel(x, y);
  const SkColor b = host.pixel(x + 8, y);
  return std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b));
}

/** The fixture every arm below shares: a 120x120 striped node at canvas
 *  (40, 40) — deliberately NOT at the origin, because a parameter
 *  Material must resolve in the NODE's space, and a map that read layer
 *  or canvas coordinates would shift its falloff by a third of the box. */
void stripePlate(Host& host, material::skia::Effect e) {
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .inset(40, 40, 40, 40)
                                       .absolute()
                                       .fill(stripeFill())
                                       .effect(std::move(e))));
  host.frame();
}

}  // namespace

TEST(ComposeEffects, AParameterMapVariesTheBlurAcrossTheNode) {
  // Sharp at one edge, soft at the other, from ONE effect on ONE node — a
  // picture no constant sigma can produce, and the reason the channel
  // exists at all (a depth-of-field falloff, a lens edge).
  Host varying;
  stripePlate(varying, material::skia::Effect::blur(focalRamp(), 16));
  const int y = 100;                             // the node's vertical middle
  const int sharp = contrastAt(varying, 51, y);  // local x 11 → sigma ~1.5
  const int mid = contrastAt(varying, 67, y);    // local x 27 → sigma ~3.6
  const int soft = contrastAt(varying, 139, y);  // local x 99 → sigma ~13
  EXPECT_GT(sharp, 150) << "the map's 0 end must stay legibly sharp";
  EXPECT_LT(soft, 40) << "the map's 1 end must be washed out";
  EXPECT_GT(sharp, mid) << "sharp " << sharp << " mid " << mid;
  EXPECT_GT(mid, soft) << "the falloff must be monotonic across the node"
                       << " (sharp " << sharp << " mid " << mid << " soft "
                       << soft << ")";

  // THE CONTROLS, both directions, because "a blur happened" is not the
  // claim — "the blur VARIES" is. A constant blur at the same sigma
  // washes the sharp end too; a constant blur at zero leaves the soft end
  // sharp. Neither can be the picture above.
  Host constantMax, unblurred;
  stripePlate(constantMax, material::skia::Effect::filter(
                               SkImageFilters::Blur(16, 16, nullptr)));
  EXPECT_LT(contrastAt(constantMax, 51, y), 40)
      << "a constant max-sigma blur cannot leave the left end sharp";
  stripePlate(unblurred, material::skia::Effect::filter(
                             SkImageFilters::Offset(0, 0, nullptr)));
  EXPECT_GT(contrastAt(unblurred, 139, y), 150)
      << "…and no blur at all cannot make the right end soft";
}
TEST(ComposeEffects, AStaticParamBlurPrunesByRecipeAndByItsMap) {
  // Carrying the sigma map as a Material rather than a callable is what
  // makes the effect comparable at all. The map has to be IN the equality
  // too: a parameter read live but excluded from the comparison leaves a
  // pruned node sampling last frame's map forever.
  Host host;
  auto tree = [&](float maxSigma, material::skia::Paint map) {
    return box().child(box().width(60).height(60).fill(green()).effect(
        material::skia::Effect::blur(std::move(map), maxSigma)));
  };
  host.composer.render(tree(10, focalRamp()));
  host.frame();
  host.composer.render(
      tree(10, focalRamp()));  // fresh material::skia::Effect, same recipe
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical blur recipe re-patched";
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(14, focalRamp()));  // a different range IS a change
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  // …and so is a different MAP at the same range.
  const material::skia::Paint flipped = material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {1, 1, 1, 1}}, {1.0f, {0, 0, 0, 1}}});
  host.composer.render(tree(14, flipped));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "the sigma map must ride the prune signature";
}

TEST(ComposeEffects, ALiveSigmaMapMakesTheWholeEffectLive) {
  // Liveness has to be INHERITED: a live parameter must lift the whole
  // effect to live, or a bake samples the map once and the effect freezes
  // at that sample while everything around it keeps moving. The recursion
  // is Material::isAnimated()'s own.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> k{0.0f};
  const material::skia::Paint liveMap =
      material::skia::Paint::sksl(fx).uniform("uK", &k);
  EXPECT_TRUE(material::skia::Effect::blur(liveMap, 16).isAnimated());
  EXPECT_FALSE(material::skia::Effect::blur(focalRamp(), 16).isAnimated())
      << "a static map must NOT declare volatility (the control)";

  // The pixels follow the live map with no re-describe: uK 0 is sharp
  // everywhere, uK 1 is blurred everywhere.
  Host host;
  stripePlate(host, material::skia::Effect::blur(liveMap, 16));
  EXPECT_GT(contrastAt(host, 99, 100), 150);
  k = 1.0f;      // move the map — NO re-describe
  host.frame();  // the live effect re-resolves the parameter
  EXPECT_LT(contrastAt(host, 99, 100), 40);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a live sigma map must declare volatility";
}

TEST(ComposeEffects, ABoundMaxSigmaAnimatesOnTheExistingChannel) {
  // The range rides the SAME uniform channel as every other live parameter,
  // so there is exactly one way to animate a blur rather than one per knob.
  choreograph::Output<float> range{0.0f};
  Host host;
  stripePlate(
      host,
      material::skia::Effect::blur(focalRamp(), 0).uniform("maxSigma", &range));
  EXPECT_GT(contrastAt(host, 139, 100), 150);  // range 0: no blur anywhere
  range = 16.0f;
  host.frame();
  EXPECT_LT(contrastAt(host, 139, 100), 40);  // the map's 1 end now washes
  EXPECT_GT(contrastAt(host, 51, 100), 150);  // …and its 0 end still does not
}

TEST(ComposeEffects, AnEffectChildFillsASecondDeclaredShaderSlot) {
  // Effect::shader fills exactly ONE child — "content", the node's own
  // layer — so a second declared `uniform shader` has nothing to bind it.
  // child() fills it with a Material, resolved against THIS node's box, so
  // unit-space authoring works here exactly as it does on a fill.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform shader param;"
               "half4 main(float2 p) {"
               "  return content.eval(p) * param.eval(p).r;"
               "}"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  host.composer.render(
      box().child(box()
                      .width(120)
                      .height(120)
                      .inset(40, 40, 40, 40)
                      .absolute()
                      .fill(green())
                      .effect(material::skia::Effect::shader(fx).child(
                          "param", focalRamp()))));
  host.frame();
  // The ramp modulates the green layer left (0) to right (1) — and the
  // ramp is in the NODE's unit square, so the dark end is at the node's
  // left edge, not the canvas's.
  EXPECT_LT(SkColorGetG(host.pixel(45, 100)), 40u);
  EXPECT_GT(SkColorGetG(host.pixel(155, 100)), 200u);
  EXPECT_NEAR((int)SkColorGetG(host.pixel(100, 100)), 128, 40);

  // …AND A STATIC CHILD REACHES THE SNAPSHOT, which the arm above cannot
  // show: a unit ramp is geometry-tier, so the paint path re-resolves it and
  // a mistake at store time is invisible. A solid never needs a context, so
  // it appears in the filter only if child() actually rebuilt the snapshot.
  Host flat;
  flat.composer.render(box().child(
      box()
          .width(120)
          .height(120)
          .inset(40, 40, 40, 40)
          .absolute()
          .fill(green())
          .effect(material::skia::Effect::shader(fx).child(
              "param", material::skia::Paint::solid({0.5f, 0.5f, 0.5f, 1})))));
  flat.frame();
  EXPECT_NEAR((int)SkColorGetG(flat.pixel(60, 100)), 128, 24);
  EXPECT_NEAR((int)SkColorGetG(flat.pixel(140, 100)), 128, 24);
}

TEST(ComposeEffects, AnUndeclaredEffectChildIsIgnoredNotBound) {
  // Material::child's guardrail, verbatim: an undeclared name warns and is
  // IGNORED — and, the sharp half, an ignored child must not declare
  // volatility either (a node painting live for a child that does nothing
  // is the silent failure this pins).
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(half(uK), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> k{1.0f};
  const material::skia::Paint liveMap =
      material::skia::Paint::sksl(fx).uniform("uK", &k);

  // (a) filter() has no child to fill, exactly as it has no uniform.
  const sk_sp<SkImageFilter> raw = SkImageFilters::Blur(4, 4, nullptr);
  material::skia::Effect plain = material::skia::Effect::filter(raw);
  plain.child("param", liveMap);
  EXPECT_EQ(plain.imageFilter(), raw) << "filter()'s filter was replaced";
  EXPECT_FALSE(plain.isAnimated());

  // (b) a shader() effect that declares no such child.
  auto [oneChild, err2] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "half4 main(float2 p) { return content.eval(p); }"));
  ASSERT_TRUE(oneChild) << err2.c_str();
  material::skia::Effect narrow = material::skia::Effect::shader(oneChild);
  narrow.child("param", liveMap);
  EXPECT_FALSE(narrow.isAnimated());
  // …and "content" is the library's, never the author's to overwrite.
  material::skia::Effect content = material::skia::Effect::shader(oneChild);
  content.child("content", liveMap);
  EXPECT_FALSE(content.isAnimated());

  // (c) a blur()'s one child is "sigma"; a typo must not bind.
  material::skia::Effect typo = material::skia::Effect::blur(focalRamp(), 8);
  typo.child("sgima", liveMap);
  EXPECT_FALSE(typo.isAnimated());
  // THE CONTROL: the declared name does bind, and does go live.
  auto [twoChild, err3] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader content;"
               "uniform shader param;"
               "half4 main(float2 p) { return content.eval(p) * "
               "param.eval(p).r; }"));
  ASSERT_TRUE(twoChild) << err3.c_str();
  material::skia::Effect bound = material::skia::Effect::shader(twoChild);
  bound.child("param", liveMap);
  EXPECT_TRUE(bound.isAnimated());
  // …and blur()'s real name re-aims the map, which is what makes the
  // child vector one mechanism rather than two.
  material::skia::Effect reaimed = material::skia::Effect::blur(focalRamp(), 8);
  reaimed.child("sigma", liveMap);
  EXPECT_TRUE(reaimed.isAnimated());
}

TEST(ComposeEffects, ADroppedUniformBindingIsLoudNotSilent) {
  // The drop the recipe-name guardrails do not reach: a filter() has no
  // uniform to receive a binding at all. It must warn like the blur paths
  // do — an author animating a filter() uniform otherwise gets neither
  // motion nor diagnostic. Control first: a valid binding on a shader()
  // stays silent. (The other drop this once covered, a null Output, can no
  // longer be spelled: the parameter is an animatable, and the empty case
  // of one is a plain number.)
  choreograph::Output<float> k{0.5f};
  ::testing::internal::CaptureStderr();
  (void)material::skia::Effect::shader(ukEffect()).uniform("uK", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a valid binding must not warn";
  // uniform() on a filter(): warned and ignored, and still not live.
  ::testing::internal::CaptureStderr();
  material::skia::Effect plain =
      material::skia::Effect::filter(SkImageFilters::Blur(4, 4, nullptr));
  plain.uniform("uK", &k);
  const std::string filterLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(filterLog.find("skia::Effect::uniform"), std::string::npos)
      << filterLog;
  EXPECT_NE(filterLog.find("uK"), std::string::npos) << filterLog;
  EXPECT_FALSE(plain.isAnimated());
}

TEST(ComposeEffects, AnUndeclaredShaderUniformIsWarnedAndIgnored) {
  // The shader() path is the one that takes arbitrary names, and the builder
  // answers a name the effect does not declare — or one declared at another
  // type — with a debug abort and no write. This Skia has no SK_DEBUG, so
  // without a check the value is dropped, the effect paints with a zeroed
  // uniform, and nothing says so. Material's discipline is the standard:
  // validate at store time, warn, ignore.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform shader content;"
      "uniform float uK;"
      "uniform float2 uV;"
      "half4 main(float2 p) { return content.eval(p) * (uK + uV.x); }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{0.5f};

  // Control: the declared float binds, silently, on both doors.
  ::testing::internal::CaptureStderr();
  const material::skia::Effect good =
      material::skia::Effect::shader(effect, {{"uK", 0.5f}});
  material::skia::Effect goodBound = material::skia::Effect::shader(effect);
  goodBound.uniform("uK", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "a declared float uniform must bind without a word";
  EXPECT_TRUE(goodBound.isAnimated());
  EXPECT_TRUE(good.imageFilter() != nullptr);

  // (a) a typo'd constant on shader(): warned, and the filter it builds is
  // the one it would have built with no binding at all.
  ::testing::internal::CaptureStderr();
  const material::skia::Effect typoConst =
      material::skia::Effect::shader(effect, {{"noSuchConst", 1.0f}});
  const std::string constLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(constLog.find("skia::Effect::shader"), std::string::npos)
      << constLog;
  EXPECT_NE(constLog.find("noSuchConst"), std::string::npos) << constLog;
  EXPECT_EQ(typoConst, material::skia::Effect::shader(effect))
      << "a rejected constant must leave no trace in the recipe";

  // (b) a typo'd binding on uniform(): warned, ignored, and — the part that
  // costs a repaint every frame if it is got wrong — NOT declared live.
  ::testing::internal::CaptureStderr();
  material::skia::Effect typoBound = material::skia::Effect::shader(effect);
  typoBound.uniform("noSuchBinding", &k);
  const std::string boundLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(boundLog.find("skia::Effect::uniform"), std::string::npos)
      << boundLog;
  EXPECT_NE(boundLog.find("noSuchBinding"), std::string::npos) << boundLog;
  EXPECT_FALSE(typoBound.isAnimated())
      << "an ignored binding must not mark the node live forever";
  EXPECT_EQ(typoBound, material::skia::Effect::shader(effect));

  // (c) a name the effect DOES declare, at another type: a float2 is not a
  // float, and assigning it is the same abort.
  ::testing::internal::CaptureStderr();
  material::skia::Effect wrongType =
      material::skia::Effect::shader(effect, {{"uV", 1.0f}});
  wrongType.uniform("uV", &k);
  const std::string typeLog = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(typeLog.find("uV"), std::string::npos) << typeLog;
  EXPECT_FALSE(wrongType.isAnimated());
  EXPECT_EQ(wrongType, material::skia::Effect::shader(effect));

  // Once per name, not once per call: a description is rebuilt every frame
  // in a live-coding host and a per-call warning would bury the console.
  ::testing::internal::CaptureStderr();
  (void)material::skia::Effect::shader(effect, {{"noSuchConst", 1.0f}});
  material::skia::Effect again = material::skia::Effect::shader(effect);
  again.uniform("noSuchBinding", &k);
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "the same rejected name must not warn twice";
}
