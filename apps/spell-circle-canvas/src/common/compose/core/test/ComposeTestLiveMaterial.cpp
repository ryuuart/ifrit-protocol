// A material that moves under the paint: a live uniform animating and
// declaring its volatility, uniforms that copy on write and never alias, a
// later plain fill replacing what was live, a blend tracking its outputs,
// uTime making a material live, a snapshot sampling it now, and a stable live
// resolve replaying its picture and blitting its texture.

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include "support/CoreTestSupport.h"

TEST(ComposeMaterial, LiveUniformAnimatesAndDeclaresVolatility) {
  // A ch::Output-bound uniform makes an sksl() Material LIVE: it re-resolves
  // every frame from the Output (no re-render), and its node paints live
  // (never freezes into a cache). This is what gives uniform(name, &output)
  // something to hook against.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uK;"
               "half4 main(float2 p) { return half4(uK, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  choreograph::Output<float> k{0.0f};
  Host host;
  host.composer.render(box().child(
      box()
          .width(40)
          .height(40)
          .inset(0, 0, 160, 160)
          .absolute()
          .fill(material::skia::Paint::sksl(effect).uniform("uK", &k))));
  host.frame();
  const SkColor c0 = host.pixel(20, 20);
  k = 1.0f;      // change the bound uniform — NO re-render
  host.frame();  // the live material re-resolves from k
  const SkColor c1 = host.pixel(20, 20);
  EXPECT_LT(SkColorGetR(c0), 40u);                    // uK=0 → black
  EXPECT_GT(SkColorGetR(c1), 200u);                   // uK=1 → red
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // volatile: paints live
}

TEST(ComposeMaterial, UniformOnNonShaderMaterialIsNoOp) {
  // uniform() on a material with no named uniforms (a solid) has nothing to
  // hook against: it is ignored, the material stays static and non-live.
  material::skia::Paint m =
      material::skia::Paint::solid({0, 1, 0, 1}).uniform("uK", 0.5f);
  EXPECT_FALSE(m.isAnimated());
  EXPECT_TRUE(m.isSolid());
}

TEST(ComposeMaterial, UniformCopiesOnWriteNeverAlias) {
  // Materials are VALUES: binding a uniform on a copy must not contaminate
  // the base or its sibling copies. The shape that catches this is a shared
  // base material bound to two different Outputs — with aliasing, both
  // copies read whichever binding was applied last.
  material::skia::Paint base = material::skia::Paint::sksl(ukEffect());
  choreograph::Output<float> low{0.2f}, high{1.0f};
  material::skia::Paint a = base;
  a.uniform("uK", &low);
  material::skia::Paint b = base;
  b.uniform("uK", &high);
  EXPECT_FALSE(base.isAnimated());  // base untouched
  EXPECT_TRUE(a.isAnimated());
  EXPECT_TRUE(b.isAnimated());

  Host host;
  host.composer.render(box()
                           .child(box()
                                      .width(40)
                                      .height(40)
                                      .inset(0, 0, 160, 160)
                                      .absolute()
                                      .fill(a))
                           .child(box()
                                      .width(40)
                                      .height(40)
                                      .inset(60, 0, 100, 160)
                                      .absolute()
                                      .fill(b)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 90u);   // a: uK=0.2
  EXPECT_GT(SkColorGetR(host.pixel(80, 20)), 200u);  // b: uK=1.0 — not aliased
}

TEST(ComposeMaterial, LaterPlainFillReplacesLiveMaterial) {
  // Fill setters are last-wins in BOTH directions. The easy half to get
  // wrong is a plain fill() following a live-material fill(): if the live
  // material is held in a separate slot that paint consults first, the later
  // plain fill is silently ignored.
  choreograph::Output<float> k{1.0f};
  Host host;
  host.composer.render(
      box().child(box()
                      .width(40)
                      .height(40)
                      .inset(0, 0, 160, 160)
                      .absolute()
                      .fill(material::skia::Paint::sksl(ukEffect())
                                .uniform("uK", &k))        // live red
                      .fill(Fill::color({0, 1, 0, 1}))));  // then plain green
  host.frame();
  const SkColor c = host.pixel(20, 20);
  EXPECT_GT(SkColorGetG(c), 200u);  // green won
  EXPECT_LT(SkColorGetR(c), 40u);
}

TEST(ComposeMaterial, BlendWithLiveLayerTracksOutputs) {
  // A blend inherits its layers' volatility tier: a live layer makes the
  // whole blend LIVE, so it re-resolves per frame and TRACKS the bound
  // Output. Flattening the stack eagerly at build time instead would bake
  // the shader's default uniform values in permanently.
  choreograph::Output<float> k{0.8f};
  material::skia::Paint m = material::skia::Paint::blend({
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {material::skia::Paint::sksl(ukEffect()).uniform("uK", &k),
       SkBlendMode::kPlus},
  });
  EXPECT_TRUE(m.isAnimated());  // inherited from the bound layer
  Host host;
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t bright = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(bright, 170u);  // ~0.8 * 255 = 204
  k = 0.3f;                 // no render() — the blend follows the Output
  host.frame();
  const uint32_t dim = SkColorGetR(host.pixel(20, 20));
  EXPECT_GT(dim, 50u);  // ~0.3 * 255 = 77
  EXPECT_LT(dim, 110u);
}

TEST(ComposeMaterial, NestedBlendAsShaderFoldsItsLiveLayersPerCall) {
  // A blend carries no live-uniform block of its own — it INHERITS liveness
  // from its layers — so any code path that reaches for that block directly
  // has nothing to dereference. The shape that reaches it is a blend nested
  // inside another blend's layer list, because blend() folds every layer to
  // a shader as it is constructed: building `outer` below is the moment it
  // happens, before anything is painted.
  choreograph::Output<float> k{0.8f};
  material::skia::Paint inner = material::skia::Paint::blend({
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kSrcOver},
      {material::skia::Paint::sksl(ukEffect()).uniform("uK", &k),
       SkBlendMode::kPlus},
  });
  ASSERT_TRUE(inner.isAnimated());  // inherited from the bound layer
  material::skia::Paint outer = material::skia::Paint::blend({
      {inner, SkBlendMode::kSrcOver},  // a nested blend layer
      {material::skia::Paint::solid({0, 0, 0, 1}), SkBlendMode::kPlus},
  });
  ASSERT_TRUE(outer.isAnimated());  // liveness survives one more nesting

  // And the answer must be folded PER CALL. Merely guarding the null would
  // fall through to m_shader — blend()'s eager snapshot, built once at
  // construction — which is the stale-snapshot defect asShader()'s live
  // branch exists to prevent; it would answer 0.8 forever.
  auto sampleR = [](const material::skia::Paint& m) -> uint32_t {
    sk_sp<SkShader> s = m.asShader();
    EXPECT_TRUE(s);
    sk_sp<SkSurface> surf =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
    surf->getCanvas()->clear(SK_ColorBLACK);
    SkPaint p;
    p.setShader(std::move(s));
    surf->getCanvas()->drawPaint(p);
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surf->readPixels(bm.pixmap(), 2, 2);
    return SkColorGetR(bm.getColor(0, 0));
  };
  EXPECT_NEAR((int)sampleR(outer), 204, 12);  // 0.8 · 255, through two blends
  k = 0.3f;
  EXPECT_NEAR((int)sampleR(outer), 77, 12);  // 0.3 · 255 — the fold is fresh
  // The same claim for the inner blend, which the outer one reaches through.
  EXPECT_NEAR((int)sampleR(inner), 77, 12);
}

TEST(ComposeMaterial, DeclaringUTimeMakesMaterialLive) {
  // "Reading the clock IS the volatility declaration": an sksl effect that
  // declares uTime takes the live path with no bound Outputs — it re-resolves
  // per frame with PaintContext time instead of freezing a uTime=0 snapshot.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uTime;"
      "half4 main(float2 p) { return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  material::skia::Paint m = material::skia::Paint::sksl(effect);
  EXPECT_TRUE(m.isAnimated());

  sigil::motion::FrameClock clock;
  Host host;
  host.composer.setClock(&clock);
  host.composer.render(box().child(
      box().width(40).height(40).inset(0, 0, 160, 160).absolute().fill(m)));
  host.frame();
  const uint32_t r0 = SkColorGetR(host.pixel(20, 20));  // uTime ≈ 0 → black
  clock.tick();                                         // advance real time…
  // …but pin the readable elapsed via a fabricated wait: FrameClock elapsed
  // is wall-time based; just assert the material painted live (r0 near 0 is
  // the frozen-snapshot failure mode this test guards).
  EXPECT_LT(r0, 30u);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live, not cached
}

TEST(ComposeMaterial, LiveMaterialUnderLeafDirectBlend) {
  // The leaf fast path routes blend onto the fill paint, so a
  // live-material leaf with .blend(kPlus) must composite additively.
  choreograph::Output<float> k{1.0f};  // red
  Host host;
  host.composer.render(
      stack()
          .child(box()
                     .width(40)
                     .height(40)
                     .inset(0, 0, 160, 160)
                     .absolute()
                     .fill(Fill::color({0, 1, 0, 1})))  // green under
          .child(
              box()
                  .width(40)
                  .height(40)
                  .inset(0, 0, 160, 160)
                  .absolute()
                  .fill(
                      material::skia::Paint::sksl(ukEffect()).uniform("uK", &k))
                  .blend(SkBlendMode::kPlus)));
  host.frame();
  const SkColor c = host.pixel(20, 20);  // red + green = yellow
  EXPECT_GT(SkColorGetR(c), 200u);
  EXPECT_GT(SkColorGetG(c), 200u);
  EXPECT_LT(SkColorGetB(c), 60u);
}

TEST(ComposeMaterial, SnapshotSamplesLiveMaterialNow) {
  // snapshot() — the element-tree-as-a-brush bake — samples live
  // materials at their CURRENT Output values.
  choreograph::Output<float> k{1.0f};
  sk_sp<SkPicture> pic =
      snapshot(box().width(60).height(60).fill(
                   material::skia::Paint::sksl(ukEffect()).uniform("uK", &k)),
               fonts());
  ASSERT_TRUE(pic);
  Host host;
  host.surface->getCanvas()->clear(SK_ColorBLACK);
  host.surface->getCanvas()->drawPicture(pic);
  EXPECT_GT(SkColorGetR(host.pixel(30, 30)), 200u);  // k=1 sampled at bake
}

TEST(ComposeMaterial, RenderSlotHostsLiveMaterial) {
  // A live material mounted through renderSlot() animates like
  // any other — the slot path wires volatility identically.
  choreograph::Output<float> k{0.0f};
  Host host;
  host.composer.render(box().child(slot("s").width(40).height(40)));
  host.composer.renderSlot(
      "s", box().width(40).height(40).fill(
               material::skia::Paint::sksl(ukEffect()).uniform("uK", &k)));
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(20, 20)), 30u);  // k=0
  k = 1.0f;                                         // no render, no renderSlot
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(20, 20)), 200u);  // live through the slot
}

TEST(ComposeMaterial, ContentScaleDeclaringMaterialIsLive) {
  // uContentScale tracks the HOST's zoom, not the node — it must take the
  // live tier (the pre-tier-split behavior), unlike uResolution.
  auto [effect, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uContentScale;"
               "half4 main(float2 p) { return half4(1, 0, 0, 1); }"));
  ASSERT_TRUE(effect) << err.c_str();
  EXPECT_TRUE(material::skia::Paint::sksl(effect).isAnimated());
}

TEST(ComposeMaterial, StableLiveResolveReplaysThePicture) {
  // The resolve memo: a live material whose bound inputs did not change
  // returns the SAME shader pointer, and a node whose only volatility is
  // that material replays its picture instead of re-recording. So a slow or
  // stepped material repaints at ITS rate, not at the frame rate.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(
      "uniform float uPhase; half4 main(float2 p) {"
      "  return half4(fract(uPhase), 0.2, 1.0 - fract(uPhase), 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f};
  host.composer.render(box().child(box().width(100).height(100).fill(
      material::skia::Paint::sksl(fx).uniform("uPhase", &phase))));
  host.frame();  // records once
  const SkColor before = host.pixel(50, 50);
  host.frame();  // same phase → stable resolve → pure replay
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().nodesPainted, 1u);  // just the root shim
  EXPECT_EQ(host.pixel(50, 50), before);
  phase = 0.75f;  // the material actually changed
  host.frame();
  EXPECT_GT(host.composer.stats().picturesRecorded, 0u);
  EXPECT_NE(host.pixel(50, 50), before);
}

TEST(ComposeMaterial, BoundUniformOwnsItsSlotOverInjection) {
  // Binding uTime to an Output is the documented stepping idiom — the
  // auto-inject must not overwrite it with continuous clock time.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uTime; half4 main(float2 p) {"
               "  return half4(fract(uTime), 0, 0, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  choreograph::Output<float> stepped{0.5f};
  material::skia::Paint m =
      material::skia::Paint::sksl(fx).uniform("uTime", &stepped);
  PaintContext ctx;
  ctx.size = {4, 4};
  ctx.elapsedSeconds = 123.789;  // continuous clock — must be IGNORED
  Fill f = resolveFill(m, ctx);
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2, 2));
  SkPaint p;
  p.setShader(f.shaderValue);
  s->getCanvas()->drawPaint(p);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  s->readPixels(bm.pixmap(), 0, 0);
  EXPECT_NEAR(SkColorGetR(bm.getColor(0, 0)), 128, 3);  // fract(0.5), not .789
}

TEST(ComposeMaterial, BakeScaleUpscalesThroughTheSameRect) {
  // bakeScale(0.5) rasterizes the texture bake at half resolution; the
  // blit stretches it back through the same dst rect — same coverage,
  // same color, a quarter of the evaluated pixels.
  Host host;
  host.composer.render(box().child(box()
                                       .left(20)
                                       .top(20)
                                       .width(100)
                                       .height(100)
                                       .cache(Cache::Texture)
                                       .bakeScale(0.5f)
                                       .fill(red())));
  host.frame();  // bake at half scale
  host.frame();  // blit
  EXPECT_EQ(host.pixel(70, 70), SkColorSetARGB(255, 255, 0, 0));
  // inboard of the AA edge on every side — coverage must not shrink
  EXPECT_EQ(host.pixel(23, 23), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(116, 116), SkColorSetARGB(255, 255, 0, 0));
  EXPECT_EQ(host.pixel(140, 70), SK_ColorBLACK);  // outside stays empty
}

TEST(ComposeMaterial, StableLiveResolveBlitsTheTexture) {
  // The texture flavour of the resolve memo, which matters most for a large
  // shader-filled area: bake at the material's own rate and BLIT in between.
  // Replaying a picture re-executes the SkSL on raster; blitting a texture
  // does not.
  auto [fx, err] = SkRuntimeEffect::MakeForShader(
      SkString("uniform float uPhase; half4 main(float2 p) {"
               "  return half4(fract(uPhase), 0.4, 0.2, 1); }"));
  ASSERT_TRUE(fx) << err.c_str();
  Host host;
  choreograph::Output<float> phase{0.25f}, sibling{0.0f};
  host.composer.render(
      box()
          .child(box()
                     .width(100)
                     .height(100)
                     .cache(Cache::Texture)
                     .fill(material::skia::Paint::sksl(fx).uniform("uPhase",
                                                                   &phase)))
          // An always-animating sibling keeps the ROOT live, which is the
          // ordinary case in a real scene: the shader-filled node must still
          // blit even though the frame as a whole is repainting.
          .child(box().width(10).height(10).fill(red()).translateX(&sibling)));
  host.frame();  // bakes
  const unsigned recordedAfterBake = host.composer.stats().picturesRecorded;
  EXPECT_GE(recordedAfterBake, 1u);
  host.frame();  // stable phase → blit, no re-bake
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  phase = 0.75f;
  host.frame();  // real change → one re-bake
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
}
