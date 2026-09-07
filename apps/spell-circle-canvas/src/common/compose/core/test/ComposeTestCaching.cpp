// What a recording and a texture hold, and what stales them: a static
// subtree recorded once, a relayout and a reconcile invalidating what was
// recorded, an effect baked into the texture it sits on, a bound opacity
// kept out of the recording, a declared bake density holding one bake across
// view scales, and a texture-cached node blending on its blit.

#include "support/CoreTestSupport.h"

TEST(ComposeCaching, StaticSubtreeRecordsOnce) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(box().child(
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++programRuns;
        SkPaint p;
        p.setColor(SK_ColorCYAN);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(80)
          .height(80)));

  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 1);  // recorded once, replayed thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorCYAN);
  EXPECT_GE(host.composer.stats().picturesLive, 1u);
}

TEST(ComposeCaching, RelayoutInvalidatesStaleRecordings) {
  // setSize alone — no prop change, no re-render — resizes a pct-width child
  // whose geometry is already baked into cached recordings. Invalidation
  // driven only by patching never sees this, so relayout has to compare the
  // new rects against the baked ones and drop what moved.
  Host host;
  host.composer.render(
      box().child(box().width(pct(50)).height(40).fill(red())));
  host.frame();  // child spans x∈[0,100) at 200-wide viewport; recorded
  EXPECT_EQ(host.pixel(80, 20), SK_ColorRED);
  host.composer.setSize({120, 200});  // child now spans x∈[0,60)
  host.frame();
  EXPECT_EQ(host.pixel(80, 20), SK_ColorBLACK);  // red = stale bake replayed
  EXPECT_EQ(host.pixel(30, 20), SK_ColorRED);    // new geometry painted
}

TEST(ComposeCaching, CacheNoneRunsEveryFrame) {
  static int programRuns;
  programRuns = 0;
  Host host;
  host.composer.render(
      box().child(custom([](SkCanvas&, const PaintContext&) { ++programRuns; })
                      .width(10)
                      .height(10)
                      .cache(Cache::None)));
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 2);
}

TEST(ComposeCaching, ReconcileInvalidatesRecording) {
  Host host;
  auto tree = [](Fill f) {
    return box().child(box().key("x").width(60).height(60).fill(std::move(f)));
  };
  host.composer.render(tree(red()));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
  host.composer.render(tree(green()));
  EXPECT_TRUE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
}

TEST(ComposeCaching, APhosphorBloomOnATextureNodeIsBakedWithIt) {
  // A layer effect is captured by the node's bake, so a bloom over a
  // bounded glow-source layer under Cache::Texture is paid once: the
  // second frame blits the baked image and records nothing, and the
  // halo is in the blit.
  Host host;
  host.composer.render(
      box()
          .cache(Cache::None)
          .child(box()
                     .key("glow")
                     .width(80)
                     .height(80)
                     .cache(Cache::Texture)
                     .effect(material::skia::Effect::phosphorBloom(
                         9, 0.5f, 1.0f, 0.8f, -30.0f, 0.5f))
                     .child(box()
                                .absolute()
                                .left(28)
                                .top(28)
                                .width(24)
                                .height(24)
                                .fill(Fill::color({1, 0.7f, 0.1f, 1})))));
  host.frame();
  EXPECT_GE(host.composer.stats().texturesBaked, 1u);
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u);
  EXPECT_GT(SkColorGetR(host.pixel(57, 40)), 0u);  // the halo, 5 px out
  EXPECT_EQ(host.pixel(4, 4), SK_ColorBLACK);      // and no further
}

TEST(ComposeCaching, TextureCacheRasterizesOnceAndInvalidates) {
  static int programRuns;
  programRuns = 0;
  Host host;
  auto tree = [](SkColor color) {
    return box().child(
        custom([color](SkCanvas& c, const PaintContext& ctx) {
          ++programRuns;
          SkPaint p;
          p.setColor(color);
          c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
        })
            .key("tex")
            .width(80)
            .height(80)
            .cache(Cache::Texture));
  };
  host.composer.render(tree(SK_ColorMAGENTA));
  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(programRuns, 1);  // rasterized once, blitted thereafter
  EXPECT_EQ(host.pixel(40, 40), SK_ColorMAGENTA);
  EXPECT_GE(host.composer.stats().texturesLive, 1u);

  host.composer.render(tree(SK_ColorYELLOW));  // invalidate
  host.frame();
  EXPECT_EQ(programRuns, 2);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorYELLOW);
}

TEST(ComposeCache, ABlendedLeafWithABoundOpacityPaintsWithoutALayer) {
  // A leaf that composites — a plus-blended glow, a fading overlay — used
  // to be handed a saveLayer whenever its opacity was BOUND rather than
  // plain, and that layer is the size of what the node PAINTS, so a
  // full-bleed one cost a canvas-sized intermediate every frame for one
  // fill. The alpha rides the fill paint instead, and this is the claim
  // that makes that legal: a lone fill drawn into a transparent layer and
  // composited at alpha is the same pixels as that fill drawn at alpha,
  // since there is nothing inside the layer for it to composite against
  // first. Asserted against the PLAIN opacity of the same value, which
  // took the direct route all along.
  const auto plate = [](const ch::Output<float>* bound, float plain) {
    auto host = std::make_unique<Host>();
    Element leaf = box()
                       .key("glow")
                       .width(120)
                       .height(120)
                       .absolute()
                       .left(30)
                       .top(30)
                       .corners({28.0f})
                       .hitTestable(false)
                       .fill(green())
                       .blend(SkBlendMode::kPlus);
    if (bound)
      leaf.opacity(bound);
    else
      leaf.opacity(plain);
    host->composer.render(profiledUnder(
        stack().child(box().inset(0).fill(red())).child(std::move(leaf))));
    host->frame();
    return host;
  };
  ch::Output<float> gain{0.5f};
  const std::unique_ptr<Host> live = plate(&gain, 0.0f);
  const std::unique_ptr<Host> plain = plate(nullptr, 0.5f);
  EXPECT_TRUE(identicalPixels(*live, *plain, 200, 200))
      << "the alpha on the fill paint is not the alpha through a layer";
  // …and it still tracks the binding, which a frozen fill paint would not.
  gain = 0.0f;
  live->frame();
  EXPECT_EQ(SkColorGetG(live->pixel(90, 90)), 0u);
}

TEST(ComposeCache, ARecordedLeafKeepsItsBoundOpacityOutOfTheRecording) {
  // …and the exclusion that survives. A leaf asked for `Cache::Picture`
  // records, and a recording that baked the fill paint would freeze this
  // frame's alpha and replay a fade that has moved on. Such a leaf keeps
  // the layer, so the opacity is applied over the replay.
  Host host;
  ch::Output<float> gain{1.0f};
  host.composer.render(profiledUnder(stack()
                                         .child(box().inset(0).fill(red()))
                                         .child(box()
                                                    .key("fading")
                                                    .width(80)
                                                    .height(80)
                                                    .absolute()
                                                    .left(10)
                                                    .top(10)
                                                    .fill(green())
                                                    .cache(Cache::Picture)
                                                    .opacity(&gain))));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(50, 50)), 200u) << "opaque at gain 1";
  gain = 0.0f;
  host.frame();
  EXPECT_LT(SkColorGetG(host.pixel(50, 50)), 20u)
      << "the recording froze the alpha it was taken at";
}

TEST(ComposeCache, ADeclaredBakeDensityHoldsOneBakeAcrossViewScales) {
  // A DECLARED DENSITY MAKES A BAKE A PICTURE OF THE CANVAS. Without one
  // a bake is a picture of the VIEW: a reader who zooms walks the coarse
  // ladder, and every rung re-rasterizes the node at the new resolution
  // while they wait. With one the node is baked at the density the host
  // named and blitted through whatever the view does afterwards, exactly
  // as an image node's pixels are — which is why three view scales cost
  // one bake and two blits.
  const auto drawAt = [](Host& host, float viewScale) {
    SkCanvas* canvas = host.surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    canvas->save();
    canvas->scale(viewScale, viewScale);
    host.composer.draw(*canvas);
    canvas->restore();
  };
  const auto panel = [] {
    return box().key("panel").width(60).height(60).fill(green()).cache(
        Cache::Texture);
  };
  // Three scales a wheel would pass through, none of them a rung apart
  // from the last by less than the ladder's own step.
  const float viewScales[] = {1.0f, 1.4f, 2.744f};

  Host pinned;
  pinned.composer.setBakeDensity(2.0f);
  pinned.composer.render(profiledUnder(panel()));
  drawAt(pinned, viewScales[0]);
  EXPECT_EQ(pinned.composer.stats().texturesBaked, 1u) << "the one bake";
  for (int step = 1; step < 3; ++step) {
    drawAt(pinned, viewScales[step]);
    EXPECT_EQ(pinned.composer.stats().texturesBaked, 0u)
        << "re-baked at view scale " << viewScales[step];
    EXPECT_EQ(pinned.composer.stats().texturesLive, 1u) << "still one image";
  }
  // …and it is still the panel that is on screen, blitted up.
  EXPECT_EQ(pinned.pixel(60, 60), SK_ColorGREEN);

  // The control: the ladder is exactly what the density opts out of.
  Host laddered;
  size_t ladderBakes = 0;
  laddered.composer.render(profiledUnder(panel()));
  for (float viewScale : viewScales) {
    drawAt(laddered, viewScale);
    ladderBakes += laddered.composer.stats().texturesBaked;
  }
  EXPECT_GT(ladderBakes, 1u);
}

TEST(ComposeCache, ADeclaredBakeDensityStillReBakesChangedContent) {
  // The density answers "at what resolution", never "is this still the
  // same picture". A node whose content changed is a different picture
  // and is taken again.
  Host host;
  host.composer.setBakeDensity(2.0f);
  const auto panel = [](Fill fill) {
    return profiledUnder(
        box().key("panel").width(60).height(60).fill(fill).cache(
            Cache::Texture));
  };
  host.composer.render(panel(green()));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 0u) << "nothing changed";
  host.composer.render(panel(red()));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u) << "the fill changed";
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
}

TEST(ComposeCache, ADeclaredScaleEntranceBakesOnceAtItsDestination) {
  // A `from(a).to(b)` on a scale lane NAMES b, so the coarse bake ladder
  // takes the bake there and the blit minifies through the entrance. The
  // ladder is for a scale nobody declared — a resize, a pinch zoom — where
  // one bake per step is the cheap answer; an entrance is a known scale
  // being travelled, and quantizing it bakes the node again at every rung
  // it passes.
  Host host;
  host.composer.render(
      profiledUnder(box()
                        .key("wedge")
                        .width(80)
                        .height(80)
                        .fill(green())
                        .cache(Cache::Texture)
                        .scale(animate(motion::from(0.2f).to(1.0f),
                                       {std::chrono::milliseconds(400)}))));
  host.frame();
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u)
      << "the bake, taken at the scale the motion names";
  // Inside the entrance throughout: the last tick lands at 350 ms of 400.
  for (int step = 0; step < 7; ++step) {
    host.frame(0.05);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "step " << step << ": the entrance passed a rung and re-baked";
  }
}

// ---------------------------------------------------------------------------
// A texture-cached node's blend rides its blit, not a saveLayer.

TEST(ComposeCaching, ATextureBlendCompositesOnTheBlitNotALayer) {
  // Cache::Texture plus a non-srcOver blend must not allocate a
  // device-clip-sized saveLayer just to composite ONE blit — the blend
  // belongs on the blit's own paint.
  //
  // That is safe because compositing an image through a layer and drawing
  // it with the same paint directly are one operation minus an
  // intermediate, so the deferred plate must match a hand-built layer
  // composite to within the 8-bit rounding residual. And kPlus over the
  // red base must actually ACCUMULATE, or the blend was dropped rather
  // than moved.
  auto plate = [](bool texture) {
    Host host;
    host.composer.render(box().fill(red()).child(
        box()
            .width(80)
            .height(80)
            .inset(20, 20, 100, 100)
            .absolute()
            .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
            .blend(SkBlendMode::kPlus)
            .cache(texture ? Cache::Texture : Cache::Picture)));
    for (int i = 0; i < 3; ++i)
      host.frame();  // settle: bake once, then replay/blit
    std::vector<SkColor> px;
    for (int y = 10; y < 110; y += 2)
      for (int x = 10; x < 110; x += 2) px.push_back(host.pixel(x, y));
    return px;
  };
  const auto deferred = plate(true), layered = plate(false);
  ASSERT_EQ(deferred.size(), layered.size());
  int peak = 0;
  for (size_t i = 0; i < deferred.size(); ++i)
    for (unsigned shift : {0u, 8u, 16u, 24u})
      peak = std::max(peak, std::abs((int)((deferred[i] >> shift) & 0xFFu) -
                                     (int)((layered[i] >> shift) & 0xFFu)));
  EXPECT_LE(peak, 2) << "deferred blit and layer composite disagree "
                        "beyond the 8-bit residual";
  // …and the blend is really live: plus over red saturates the red
  // channel where the child overlaps.
  Host host;
  host.composer.render(
      box().fill(red()).child(box()
                                  .width(80)
                                  .height(80)
                                  .inset(20, 20, 100, 100)
                                  .absolute()
                                  .fill(Fill::color({0.2f, 0.4f, 0.2f, 1}))
                                  .blend(SkBlendMode::kPlus)
                                  .cache(Cache::Texture)));
  for (int i = 0; i < 3; ++i) host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(50, 50)), 250u);  // 1.0 + 0.2 clamps
  EXPECT_GT(SkColorGetG(host.pixel(50, 50)), 90u);   // the child's green
}

// ---------------------------------------------------------------------------
// A STATIC layer effect over content that has settled is run over the bake.

namespace {

/** The shape the tier is for: a node that holds one bake and rides its own
 *  transform, wearing a halo that never changes. The bound rotation is what
 *  keeps the bake in the node's OWN space — a node standing still bakes in
 *  device space, which is taken once and has no repeated bake to lift an
 *  effect out of.
 *
 *  @p boundary is the arm: a declared coverage boundary refuses the tier
 *  and changes nothing a box without decorations paints, so that arm is
 *  the same node with the filter inside its bake. */
Element haloedNode(Boundary boundary, const choreograph::Output<float>* turn) {
  return box()
      .key("halo")
      .absolute()
      .left(40)
      .top(40)
      .width(80)
      .height(80)
      .cache(Cache::Texture)
      .boundary(boundary)
      .transformOrigin(0.5f, 0.5f)
      .rotate(motion::bind(turn).target(0.0f, 360.0f))
      .effect(material::skia::Effect::glow({0.1f, 0.85f, 1.0f, 1}, 6))
      .child(box().absolute().left(20).top(20).width(40).height(40).fill(
          Fill::color({1, 0.72f, 0.15f, 1})));
}

}  // namespace

TEST(ComposeCaching, AStaticEffectOverSettledContentIsRunOverItsBake) {
  // The picture is the whole claim: a node whose content is rasterized
  // WITHOUT its effect, with the effect run over that image, must paint
  // what the same node painted with the effect inside the raster. What
  // moves is only what a re-bake costs — the filter's own band-sized layer
  // is gone from it.
  //
  // THE BLEED RULE is what makes the two the same. The lifted filter reads
  // the bake and nothing else, so the bake must carry the effect's reach as
  // transparent margin — here the halo's sigma against the 20 px the inner
  // box is inset by — exactly as it had to before, when that same margin
  // was what the filter's own layer spread into.
  choreograph::Output<float> still{0.0f};  // held at rest: no pixel moves
  const auto plate = [&still](Boundary boundary) {
    Host host;
    host.composer.setProfiling(true);
    host.composer.render(
        box().child(profiledUnder(haloedNode(boundary, &still))));
    for (int i = 0; i < 3; ++i) host.frame();  // bake once, then blit
    const Composer::NodeCost* row = requireRow(host.composer, "halo");
    return std::pair{grab(host), row && row->effectDeferred};
  };
  const auto [blitFiltered, rides] = plate(Boundary::Auto);
  const auto [bakeFiltered, sits] = plate(Boundary::Coverage);
  EXPECT_TRUE(rides) << "the effect stayed inside the bake — nothing below "
                        "this line compares the two tiers";
  EXPECT_FALSE(sits) << "the reference arm deferred too, so both plates are "
                        "the same path and the comparison proves nothing";
  ASSERT_EQ(blitFiltered.size(), bakeFiltered.size());
  int peak = 0;
  size_t lit = 0;
  for (size_t i = 0; i < blitFiltered.size(); ++i) {
    for (unsigned shift : {0u, 8u, 16u, 24u})
      peak =
          std::max(peak, std::abs((int)((blitFiltered[i] >> shift) & 0xFFu) -
                                  (int)((bakeFiltered[i] >> shift) & 0xFFu)));
    if (blitFiltered[i] != SK_ColorBLACK) ++lit;
  }
  EXPECT_LE(peak, 1) << "the filter at the blit and the filter inside the "
                        "bake paint different pixels";
  // …and there was a halo to compare: the content alone is 40x40, so a
  // plate agreeing on 1600 pixels agreed about the content and nothing the
  // effect made.
  EXPECT_GT(lit, 2600u) << "no halo outside the content in either plate";
}

TEST(ComposeCaching, ADeferredEffectSpreadsInsideTheBakeAndStopsAtIt) {
  // The bleed rule, read off the pixels. The halo stands in the margin the
  // node's paint bounds carry around its ink, and the node's own bake rect
  // ends it: the lifted filter has no pixels to read past the bake, and the
  // surface it draws into cuts the halo where the filter's own layer was
  // cut before.
  choreograph::Output<float> still{0.0f};
  Host host;
  host.composer.render(
      box().child(profiledUnder(haloedNode(Boundary::Auto, &still))));
  for (int i = 0; i < 3; ++i) host.frame();
  EXPECT_GT(SkColorGetB(host.pixel(56, 100)), 20u);  // 4 px out: the halo
  EXPECT_EQ(host.pixel(38, 100), SK_ColorBLACK);     // 2 px past the box
}
