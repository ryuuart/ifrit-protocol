// Automatic texture promotion, judged as pixels: a promoted node must
// paint what the live node painted, under a fractional translation and a
// fractional host scale, at a new scale after a warm, through a tile
// whose texels are partly transparent, and under a phrase that adds
// light to the ground. The two arithmetic questions behind that are
// asked in plain Skia: which route the rasteriser takes, and what a
// stack of cached rasters costs the pixel it lands on.

#include <sigilcompose/core/Pattern.h>

#include <vector>

#include "support/PromotionTestSupport.h"

TEST(ComposeCache, AStillAtANewScaleIsUnchangedByWhatWasPromotedBeforeIt) {
  // A plate is photographed by warming a scene at its own size and then
  // drawing ONE more frame at a larger, fractional scale onto a surface of
  // its own — so every bake the promoter took during the warmup is pinned
  // to a device rect that no longer exists when the still is drawn. Nothing
  // held from before the scale change may reach that still: every device
  // bake is remade at the rect it is now blitted to, or dropped.
  bool promoted = false;
  const std::vector<SkColor> live =
      warmThenStill(false, 1.875f, 200, 200, nullptr);
  const std::vector<SkColor> baked =
      warmThenStill(true, 1.875f, 200, 200, &promoted);
  ASSERT_TRUE(promoted)
      << "nothing was promoted during the warmup, so this compared two live "
         "stills";
  ASSERT_EQ(live.size(), baked.size());
  size_t differing = 0;
  const int worst = worstDrift(live, baked, &differing);
  EXPECT_LE(worst, 1)
      << differing << " pixels of the still moved, worst " << worst
      << " code values, because the scene had been promoted at another scale";
}

TEST(ComposeCache, PromotionUnderAFractionalTranslationChangesNoPixels) {
  // The promoter's whole argument is that a device-space bake at an
  // integer-snapped rect, blitted with the matrix reset, cannot change
  // rasterisation under an axis-aligned matrix. A host translation with a
  // fraction in it is the case that argument has to survive: the bake rect
  // rounds OUT to whole device pixels, so the bake's matrix and the live
  // paint's differ by an integer, and an integer offset moves no sample.
  const PromotionDrift drift =
      promotionDrift(SkMatrix::Translate(0.37f, 0.61f), 200, 200);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a node under a fractional "
         "translation";
}

TEST(ComposeCache, PromotionUnderAFractionalHostScaleChangesNoPixels) {
  // …and the same under a host SCALE that is not a whole number, which is
  // what a page authored at one size and photographed at another stands
  // under. The bake rect still rounds out to whole device pixels, so the
  // offset between the two matrices is still an integer.
  SkMatrix host = SkMatrix::Scale(1.875f, 1.875f);
  host.postTranslate(0.37f, 0.61f);
  const PromotionDrift drift = promotionDrift(host, 400, 400);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a node under a fractional "
         "host scale";
}

namespace {

/** A repeating tile of hard-edged marks, magnified. A procedural ramp
 *  answers a coordinate that moved an epsilon with a code value; a
 *  MAGNIFIED TEXEL answers with whatever its two neighbours differ by,
 *  which is the whole range. So this is the paint that reads back where
 *  the bake sampled, rather than only how it rounded — and it is what a
 *  background of repeated art is. */
Pattern hardTile() {
  return Pattern::tile(
      {12, 12},
      box().children({box().absolute().left(0).top(0).width(6).height(6).fill(
                          Fill::color({1, 1, 1, 1})),
                      box().absolute().left(7).top(3).width(3).height(6).fill(
                          Fill::color({0.2f, 0.9f, 0.3f, 1}))}));
}

}  // namespace

TEST(ComposeCache, APromotedTileSamplesWhereTheLivePaintSampled) {
  // The pattern's tile is baked ONCE and held, so nothing about the image
  // can differ between the two runs: what differs is where the shader read
  // it. A tile shown four times its texel size turns any drift in that
  // read into whole levels rather than a rounding — a texel's worth,
  // wherever the sample crossed into its neighbour.
  Pattern pattern = hardTile();
  const auto page = [&] {
    Element out = promotablePage();
    out.children({box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts()))});
    return out;
  };
  SkMatrix host = SkMatrix::Scale(1.875f, 1.875f);
  host.postTranslate(0.37f, 0.61f);
  const PromotionDrift drift = promotionDriftOf(page, host, 400, 400);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  // TWO, because the tile is promoted over the page's own opaque paint: a
  // texel of partial coverage composited into a bake and blitted from it
  // settles up to a value further out than the same texel drawn once. A
  // sample that landed anywhere else moves a magnified texel's worth,
  // which is tens.
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when a promoted node's paint SAMPLED an image";
}

TEST(ComposeCache, ACustomProgramIsCountedAsCompositingWithTheCanvas) {
  // A custom() leaf is handed the canvas and may draw with ANY blend mode.
  // Nothing in the library can look inside the callable, so the only sound
  // reading is that it composites against what is already there — and a
  // bake would hand it transparent black instead. The cost of getting this
  // wrong is not a rounding: the plus-blended wash below lands more than a
  // hundred code values off when its node is baked, which is a picture
  // that changed rather than one that rounded.
  const auto page = [] {
    return box()
        .cache(Cache::None)
        .children({box()
                       .key("ground")
                       .absolute()
                       .left(0)
                       .top(0)
                       .width(180)
                       .height(180)
                       .fill(Fill::color({0.45f, 0.35f, 0.15f, 1})),
                   custom([](SkCanvas& canvas, const PaintContext& ctx) {
                     SkPaint paint;
                     paint.setColor4f({0.4f, 0.4f, 0.4f, 1});
                     paint.setBlendMode(SkBlendMode::kPlus);
                     canvas.drawRect(
                         SkRect::MakeWH(ctx.size.width(), ctx.size.height()),
                         paint);
                   })
                       .key("plus")
                       .absolute()
                       .left(20)
                       .top(20)
                       .width(120)
                       .height(120)});
  };
  const std::function<Element()> fn = page;
  const PromotionDrift drift =
      promotionDriftOf(fn, SkMatrix::Scale(1.875f, 1.875f), 400, 400);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted at all, so this compared two live renders";
  EXPECT_EQ(drift.worstChannel, 0)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values: a node holding a paint program was baked, and the "
         "program's blend resolved against transparent black";

  // …and the refusal SAYS so, since a refusal an author cannot read is a
  // node that is silently slow.
  Host host(400, 400);
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  host.composer.render(page());
  host.frame();
  const Composer::NodeCost* row = requireRow(host.composer, "plus");
  ASSERT_NE(row, nullptr);
  EXPECT_TRUE(row->refused(Composer::Promotion::ReadsBackdrop));
}

namespace {

/** A tile most of whose texels are PARTIALLY TRANSPARENT: a flat wash at a
 *  third alpha, and a turned bar whose antialiased edges carry every
 *  coverage between none and all. A tile of opaque marks reads back where
 *  a sample landed; this one reads back how the sample was COMPOSITED,
 *  which is the half a bake changes — it composites into transparent black
 *  and is blitted, where the live paint composites straight onto the
 *  backdrop. */
Pattern softTile() {
  return Pattern::tile(
      {16, 16},
      box().children({box().absolute().left(1).top(1).width(14).height(14).fill(
                          Fill::color({1, 1, 1, 0.35f})),
                      box()
                          .absolute()
                          .left(3)
                          .top(3)
                          .width(10)
                          .height(3)
                          .fill(Fill::color({0.2f, 0.9f, 0.3f, 0.6f}))
                          .rotate(24)}));
}

}  // namespace

TEST(ComposeCache, APromotedNodeCompositesAPartlyTransparentTileAsItDidLive) {
  // Every star in a field is an opaque core inside a soft ring, repeated
  // from one tile, and a bake that reads the ring differently draws the
  // same wrong ring in every repeat. So the claim under test is not where
  // the sample landed but what happened to it after: a bake composites the
  // tile into transparent black and blits the result, and that must reach
  // the same pixels as compositing it onto the backdrop directly.
  Pattern pattern = softTile();
  const auto page = [&] {
    Element out = promotablePage();
    out.children({box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts()))});
    return out;
  };
  SkMatrix host = SkMatrix::Scale(1.875f, 1.875f);
  host.postTranslate(0.37f, 0.61f);
  const PromotionDrift drift = promotionDriftOf(page, host, 400, 400);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, where a promoted node composited a partly "
         "transparent tile";
}

TEST(ComposeCache, AStillAtANewScaleResamplesATileWhereTheLivePaintDoes) {
  // The same paint on the plate path: warmed at one scale, where the
  // promoter takes its bakes, then photographed ONCE at another. A bake
  // held over that change is a picture of the tile read at the old sample
  // positions, and a magnified texel makes that whole levels rather than a
  // rounding — so this is where a stale bake shows up as the picture
  // rather than as noise.
  Pattern pattern = hardTile();
  const auto page = [&] {
    Element out = promotablePage();
    out.children({box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts()))});
    return out;
  };
  bool promoted = false;
  const std::vector<SkColor> live =
      warmThenStillOf(page, false, 1.875f, 200, 200, nullptr);
  const std::vector<SkColor> baked =
      warmThenStillOf(page, true, 1.875f, 200, 200, &promoted);
  ASSERT_TRUE(promoted)
      << "nothing was promoted during the warmup, so this compared two live "
         "stills";
  ASSERT_EQ(live.size(), baked.size());
  size_t differing = 0;
  const int worst = worstDrift(live, baked, &differing);
  // Two for the same reason as the case above: the tile is promoted over
  // the page's own paint, so its coverage is composited twice.
  EXPECT_LE(worst, 2) << differing << " pixels of the still moved, worst "
                      << worst << " code values";
}

TEST(ComposeCache, APromotedEdgeThatLeavesTheCanvasLandsWhereItLandedLive) {
  // A BAKE CARRIES THE CANVAS'S OWN CLIP, and a turned edge running off the
  // canvas is how you read that back. Skia rasterizes an antialiased edge
  // against the clip it is given: the live paint's edge is cut at the canvas
  // and a bake spanning the node's full paint bounds leaves it whole, so the
  // coverage the two compute for the pixels either side differs by TENS of
  // code values — not the one an integer offset costs. Anything with bleed
  // leaves its canvas on some side, so this is most of what a bake is ever
  // taken over, and every outline on the page moves with it.
  const auto turned = [] {
    return profiledUnder(
        box().key("page").children({promotablePage(), box()
                                                          .absolute()
                                                          .left(-30)
                                                          .top(-14)
                                                          .width(260)
                                                          .height(40)
                                                          .fill(blue())
                                                          .rotate(7)}));
  };
  const auto render = [&](bool promotion, bool* promotedOut) {
    Host host(200, 200);
    host.composer.setAutoTexturePromotion(promotion);
    host.composer.setProfiling(true);
    host.composer.render(turned());
    for (int i = 0; i < 30; ++i) {
      SkCanvas& canvas = *host.surface->getCanvas();
      canvas.clear(SK_ColorBLACK);
      canvas.save();
      canvas.translate(0.37f, 0.61f);
      host.composer.draw(canvas);
      canvas.restore();
    }
    for (const Composer::NodeCost& row : host.composer.profile())
      *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
    return surfaceOf(host, 200, 200);
  };
  bool livePromoted = false, bakedPromoted = false;
  const std::vector<SkColor> live = render(false, &livePromoted);
  const std::vector<SkColor> baked = render(true, &bakedPromoted);
  ASSERT_TRUE(bakedPromoted)
      << "nothing was promoted, so this compared two live renders";
  size_t differing = 0;
  const int worst = worstDrift(live, baked, &differing);
  EXPECT_LE(worst, 1)
      << differing << " pixels moved, worst " << worst
      << " code values, when a promoted subtree's edge left the canvas";
}

namespace {

/** TYPE SET TO ADD RATHER THAN TO COVER. The blend is on the glyph PAINT,
 *  not on the node — which is where an additive phrase belongs, since a
 *  node-level blend opens a layer per frame — and it lands on a ground the
 *  node itself does not paint. The heavy panel beside it carries no blend
 *  and is there so the promoter has something left to bake: a page whose
 *  type declares a blend refuses the page, and a case that compared two
 *  live renders would pass whatever the promoter did. */
Element additiveType() {
  Element page = box()
                     .width(300)
                     .height(200)
                     .cache(Cache::None)
                     .fill(Fill::color({0.22f, 0.26f, 0.34f, 1}));
  page.children({promotablePage().absolute().left(0).top(10).key("panel")});
  sigil::weave::TextStyle lit = whiteStyle(40);
  lit.paint.foreground.setColor(SkColorSetRGB(120, 200, 255));
  lit.paint.foreground.setBlendMode(SkBlendMode::kPlus);
  page.children({text(u8"LIT", lit).absolute().left(140).top(70).width(150)});
  return page;
}

}  // namespace

TEST(ComposeCache, APromotedPhraseThatAddsLightKeepsTheGroundUnderIt) {
  // A glyph pass carries an SkPaint of its own, so a phrase set to ADD
  // resolves against what is under the node exactly as a blended decoration
  // does — and a bake would offer it transparent black instead, which is
  // not a rounding: the light comes back flat, over a ground the blit then
  // covers rather than adds to. Type declares the backdrop read the same
  // way a decoration does, so the node is refused the automatic bake.
  const PromotionDrift drift =
      promotionDriftOf(additiveType, SkMatrix::I(), 300, 200);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted type that adds light";
}

namespace {

/** THE SAME STROKED CURVE, DRAWN THREE WAYS IN PLAIN SKIA — no compose in
 *  it at all, because what is asked here is a question about the
 *  rasteriser and a device bake is only the caller of it.
 *
 *  A bake paints its node into a fresh premultiplied surface and blits the
 *  result; the live paint puts the same draw straight onto the
 *  destination. If those two routes answered different antialiased
 *  coverage — a surface property, an alpha type, a coverage mode, the way
 *  a layer's bounds are snapped — then every promoted curve would stand a
 *  fraction of a pixel from its live paint and no bake could ever agree.
 *  They do not: the offscreen and the saveLayer are the same pixels as the
 *  direct draw, and what separates the BLITTED offscreen from the direct
 *  draw is one code value of arithmetic, the node's own coverage
 *  composited twice where the live paint composited once. */
struct RouteProbe {
  int layerVsOffscreen = 0;
  int liveVsOffscreenOverTheGround = 0;
  int liveVsBlittedOffscreen = 0;
};

RouteProbe rasterisationRoutes() {
  constexpr int kW = 420, kH = 420;
  const SkColor ground = SkColorSetARGB(255, 40, 90, 140);
  SkPathBuilder builder;
  builder.addArc(SkRect::MakeLTRB(31, 31, 369, 369), -30.0f, 300.0f);
  const SkPath path = builder.detach();
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(22);
  paint.setColor(SK_ColorWHITE);
  paint.setAntiAlias(true);
  const SkMatrix under = SkMatrix::Translate(10, 10);

  const auto surface = [] {
    return SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kW, kH));
  };
  const auto pixels = [](SkSurface& from) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kW, kH));
    from.readPixels(bm.pixmap(), 0, 0);
    std::vector<SkColor> out;
    for (int y = 0; y < kH; ++y)
      for (int x = 0; x < kW; ++x) out.push_back(bm.getColor(x, y));
    return out;
  };

  // Straight onto the destination, which is what a live paint is.
  sk_sp<SkSurface> live = surface();
  live->getCanvas()->clear(ground);
  live->getCanvas()->concat(under);
  live->getCanvas()->drawPath(path, paint);

  // Into a layer opened over that same destination.
  sk_sp<SkSurface> layered = surface();
  layered->getCanvas()->clear(ground);
  layered->getCanvas()->saveLayer(nullptr, nullptr);
  layered->getCanvas()->concat(under);
  layered->getCanvas()->drawPath(path, paint);
  layered->getCanvas()->restore();

  // Into a fresh premultiplied surface — the route a bake takes — and
  // blitted back over the ground.
  sk_sp<SkSurface> offscreen = surface();
  offscreen->getCanvas()->clear(SK_ColorTRANSPARENT);
  offscreen->getCanvas()->concat(under);
  offscreen->getCanvas()->drawPath(path, paint);
  sk_sp<SkSurface> blitted = surface();
  blitted->getCanvas()->clear(ground);
  blitted->getCanvas()->drawImage(offscreen->makeImageSnapshot(), 0, 0);

  // …and the same offscreen with the ground already in it, which takes
  // the bake's route through the rasteriser and the live paint's
  // arithmetic through the blend.
  sk_sp<SkSurface> overGround = surface();
  overGround->getCanvas()->clear(ground);
  overGround->getCanvas()->concat(under);
  overGround->getCanvas()->drawPath(path, paint);

  const std::vector<SkColor> pl = pixels(*live), pa = pixels(*layered),
                             pb = pixels(*blitted), pg = pixels(*overGround);
  return {worstDrift(pa, pb, nullptr), worstDrift(pl, pg, nullptr),
          worstDrift(pl, pb, nullptr)};
}

}  // namespace

TEST(ComposeCache, ADeviceBakeRasterisesOnItsLivePaintsRoute) {
  // WHAT A DEVICE BAKE RESTS ON. Skia holds more than one way to answer an
  // antialiased edge, and which one a draw takes is decided from the
  // path's bounds against the clip it stands in — which is why a bake is
  // allocated with a margin. What it must NOT depend on is the surface the
  // draw lands on: a bake paints into a fresh premultiplied surface where
  // the live paint puts the same draw onto the destination, and if those
  // answered different coverage no bake could ever agree with the paint it
  // replaces.
  const RouteProbe routes = rasterisationRoutes();
  EXPECT_EQ(routes.layerVsOffscreen, 0)
      << "a layer opened over the destination and a fresh offscreen "
         "rasterise the same curve differently";
  EXPECT_EQ(routes.liveVsOffscreenOverTheGround, 0)
      << "an offscreen surface holding the ground rasterises the same curve "
         "differently from the destination it stands for";
  EXPECT_LE(routes.liveVsBlittedOffscreen, 1)
      << "a blitted offscreen stands more than the one code value its "
         "second composite costs from the live paint";
}

namespace {

/** THE WORST A STACK OF CACHED RASTERS COSTS A FLAT WASH, measured rather
 *  than reasoned: every destination value at once along the strip, every
 *  source alpha, a spread of source colours, and the same marks painted
 *  twice — straight onto the ground, which is the live paint, and each
 *  into a bake of its own that is then blitted, which is what promotion
 *  makes of them. A wash carries no coverage of its own, so what is left
 *  is the composite's own arithmetic. */
int washThroughBakes(int marks) {
  constexpr int kW = 256;
  const auto strip = [] {
    return SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kW, 1));
  };
  const auto ground = [&](SkSurface& s) {
    SkPaint p;
    p.setBlendMode(SkBlendMode::kSrc);
    p.setAntiAlias(false);
    for (int x = 0; x < kW; ++x) {
      p.setColor(SkColorSetARGB(255, x, x, x));
      s.getCanvas()->drawIRect(SkIRect::MakeXYWH(x, 0, 1, 1), p);
    }
  };
  const auto readRow = [](SkSurface& s) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kW, 1));
    s.readPixels(bm.pixmap(), 0, 0);
    std::vector<SkColor> row;
    row.reserve(kW);
    for (int x = 0; x < kW; ++x) row.push_back(bm.getColor(x, 0));
    return row;
  };
  int worst = 0;
  for (int a = 1; a < 256; ++a)
    for (int c = 0; c < 256; c += 17) {
      SkPaint mark;
      mark.setAntiAlias(false);
      mark.setColor(SkColorSetARGB(a, c, c, c));
      sk_sp<SkSurface> live = strip();
      ground(*live);
      for (int m = 0; m < marks; ++m)
        live->getCanvas()->drawIRect(SkIRect::MakeWH(kW, 1), mark);
      sk_sp<SkSurface> promoted = strip();
      ground(*promoted);
      for (int m = 0; m < marks; ++m) {
        sk_sp<SkSurface> bake = strip();
        bake->getCanvas()->clear(SK_ColorTRANSPARENT);
        bake->getCanvas()->drawIRect(SkIRect::MakeWH(kW, 1), mark);
        promoted->getCanvas()->drawImage(bake->makeImageSnapshot(), 0, 0);
      }
      const std::vector<SkColor> l = readRow(*live), p = readRow(*promoted);
      for (int x = 0; x < kW; ++x)
        for (int shift : {0, 8, 16})
          worst = std::max(worst, std::abs((int)((l[x] >> shift) & 0xffu) -
                                           (int)((p[x] >> shift) & 0xffu)));
    }
  return worst;
}

}  // namespace

TEST(ComposeCache, ACachedRasterCostsThePixelItLandsOnOneCodeValue) {
  // WHAT THE CONTRACT'S SECOND CLAUSE IS PRICED FROM. A cached raster is
  // a composite the live paint does not make: the mark is rounded into
  // the bake's own eight bits and the bake is rounded again onto what it
  // lands on. On a flat wash — no coverage of the mark's own to round —
  // that costs exactly ONE code value per composite, and it does not
  // decay: a pixel under a stack of independent bakes pays one for each.
  //
  // So the clause is a bound PER COMPOSITE, and a picture is bounded by
  // that number times how many composites its pixels stood under. The
  // second value the clause carries over content is this one plus the
  // rounding of the mark's own coverage into the bake, which a wash does
  // not exercise and an antialiased edge does.
  for (int marks = 1; marks <= 4; ++marks)
    EXPECT_EQ(washThroughBakes(marks), marks)
        << marks << " bakes over one pixel";
}
