// Automatic texture promotion, judged as pixels: a promoted node must paint
// what the live node painted, under fractional translations and host scales,
// at a new scale after a warm, and through a tile whose texels are partly
// transparent. The cost rule's refusals are named by the leaf that carries
// each condition, and a custom program counts as compositing with the canvas.

#include <sigilcompose/core/Pattern.h>

#include "support/CoreTestSupport.h"

namespace {

/** Expensive AND sensitive, which are two different demands on one shader.
 *  The forty-term sum is what puts a node over the promotion threshold; the
 *  cell test on the SAME coordinates is what reads back whether the bake
 *  sampled where the live paint sampled, since a cell boundary flips a
 *  whole channel where a smooth ramp moves one code value. */
sk_sp<SkRuntimeEffect> gridEffect() {
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
half4 main(float2 p) {
  float v = 0.0;
  for (int i = 0; i < 40; ++i) {
    float f = float(i) + 1.0;
    v += sin(p.x * 0.031 * f) * cos(p.y * 0.027 * f) / f;
  }
  float2 cell = floor(p / 6.0);
  float b = mod(cell.x + cell.y, 2.0);
  return half4(half(b), half(1.0 - b), half(clamp(v * 0.5 + 0.5, 0.0, 1.0)), 1.0);
}
)"));
    if (!effect) ADD_FAILURE() << err.c_str();
    return effect;
  }();
  return fx;
}

/** A page the promoter will actually promote: a per-pixel shader over the
 *  whole area, children that OVERFLOW it on every side, and a line of type.
 *  Child count alone is far under the promotion time threshold — the shader
 *  is what puts it over — and the overflow is the half the bake rect has to
 *  survive, since it is what carries the node's paint bounds outside the
 *  canvas the live paint is clipped to. */
Element promotablePage() {
  Element page = box().width(180).height(180).fill(
      material::skia::Paint::sksl(gridEffect()));
  for (int i = 0; i < 56; ++i)
    page.child(box()
                   .absolute()
                   .left(-6 + (float)i * 3.3f)
                   .top(-8)
                   .width(2)
                   .height(196)
                   .fill(i % 2 ? green() : red()));
  page.child(
      text(u8"JAM", whiteStyle(24)).absolute().left(8).top(70).width(160));
  return page;
}

/** The whole surface, row-major. */
std::vector<SkColor> surfaceOf(Host& host, int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve((size_t)w * (size_t)h);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) out.push_back(bm.getColor(x, y));
  return out;
}

/** WHAT PROMOTION IS ALLOWED TO CHANGE, as one number. A device-space bake
 *  is taken under the live matrix post-translated by an INTEGER, so nothing
 *  it draws may land on another pixel — but the matrix is inverted to find a
 *  shader's local coordinates, and at a scale whose reciprocal is inexact
 *  that integer does not cancel to the last bit. So a shaded pixel may land
 *  one code value away and nothing may land further: worst > 1 is a picture
 *  that moved, not a picture that rounded.
 *
 *  ONE VALUE IS THE BAKE OVER TRANSPARENT BLACK. A bake that lands on
 *  CONTENT carries a second: the node's own paint is composited twice
 *  where the live paint composited once — once into the bake, once when
 *  the bake is blitted — and Skia's blit of a raster image is not the
 *  arithmetic of its direct shader draw, so a partly transparent texel
 *  over a bright backdrop can settle a value further out. It is still a
 *  rounding and not a move: it appears only where the node's own alpha is
 *  between none and all, and taking the bake at higher precision does not
 *  remove it. Cases whose promoted node lands over other content say so
 *  and allow the two. */
int worstDrift(const std::vector<SkColor>& live,
               const std::vector<SkColor>& baked, size_t* differing) {
  int worst = 0;
  for (size_t i = 0; i < live.size(); ++i) {
    if (live[i] == baked[i]) continue;
    if (differing) ++*differing;
    for (int shift : {0, 8, 16, 24})
      worst = std::max(worst, std::abs((int)((live[i] >> shift) & 0xffu) -
                                       (int)((baked[i] >> shift) & 0xffu)));
  }
  return worst;
}

/** One scene drawn thirty times under a host matrix — long enough that the
 *  promoter's warmup is over — with promotion off and then on, and what the
 *  two pictures differ by. `promoted` says a node really was baked, so a
 *  promoter that stopped firing cannot pass this by comparing two identical
 *  live renders. */
struct PromotionDrift {
  int worstChannel = 0;
  size_t differingPixels = 0;
  bool promoted = false;
};

PromotionDrift promotionDriftOf(const std::function<Element()>& page,
                                const SkMatrix& hostMatrix, int w, int h) {
  const auto render = [&](bool promotion, bool* promotedOut) {
    Host host(w, h);
    // EAGER on the promoted side, so the case is about the promoter and
    // never about the machine: the cost rule is a stopwatch, and an idle
    // runner crosses no bar and would compare two live renders.
    host.composer.setAutoTexturePromotion(promotion
                                              ? Composer::PromotionPolicy::Eager
                                              : Composer::PromotionPolicy::Off);
    host.composer.setProfiling(true);
    host.composer.render(profiledUnder(page().key("page")));
    for (int i = 0; i < 30; ++i) {
      SkCanvas& canvas = *host.surface->getCanvas();
      canvas.clear(SK_ColorBLACK);
      canvas.save();
      canvas.concat(hostMatrix);
      host.composer.draw(canvas);
      canvas.restore();
    }
    if (promotedOut)
      for (const Composer::NodeCost& row : host.composer.profile())
        *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
    return surfaceOf(host, w, h);
  };
  PromotionDrift out;
  const std::vector<SkColor> live = render(false, nullptr);
  const std::vector<SkColor> baked = render(true, &out.promoted);
  out.worstChannel = worstDrift(live, baked, &out.differingPixels);
  return out;
}

PromotionDrift promotionDrift(const SkMatrix& hostMatrix, int w, int h) {
  return promotionDriftOf(promotablePage, hostMatrix, w, h);
}

/** The still a plate is photographed as: a scene warmed at one scale on one
 *  surface — long enough that the promoter has baked what it is going to —
 *  and then drawn ONCE at another, larger, fractional scale onto a surface
 *  of its own. Everything a bake was pinned to moved between the two. */
std::vector<SkColor> warmThenStillOf(const std::function<Element()>& page,
                                     bool promotion, float stillScale, int w,
                                     int h, bool* promotedOut) {
  Host host(w, h);
  host.composer.setAutoTexturePromotion(promotion
                                            ? Composer::PromotionPolicy::Eager
                                            : Composer::PromotionPolicy::Off);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(page().key("page")));
  for (int i = 0; i < 30; ++i) host.frame();
  if (promotedOut)
    for (const Composer::NodeCost& row : host.composer.profile())
      *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
  const int sw = (int)((float)w * stillScale);
  const int sh = (int)((float)h * stillScale);
  sk_sp<SkSurface> still =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(sw, sh));
  still->getCanvas()->clear(SK_ColorBLACK);
  still->getCanvas()->scale(stillScale, stillScale);
  host.composer.draw(*still->getCanvas());
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(sw, sh));
  still->readPixels(bm.pixmap(), 0, 0);
  std::vector<SkColor> out;
  out.reserve((size_t)sw * (size_t)sh);
  for (int y = 0; y < sh; ++y)
    for (int x = 0; x < sw; ++x) out.push_back(bm.getColor(x, y));
  return out;
}

std::vector<SkColor> warmThenStill(bool promotion, float stillScale, int w,
                                   int h, bool* promotedOut) {
  return warmThenStillOf(promotablePage, promotion, stillScale, w, h,
                         promotedOut);
}

}  // namespace

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

/** Four leaves under a parent that caches nothing, so every one of them is
 *  visited — and profiled — on every frame. One is promotable and the
 *  other three each carry exactly one of the conditions under which a bake
 *  would paint different pixels, so the refusal a row reports names the
 *  thing the leaf was built to carry. Nothing here is expensive: the point
 *  is a page the COST rule would never promote. */
Element eagerPage() {
  return box()
      .cache(Cache::None)
      .child(box()
                 .key("plain")
                 .absolute()
                 .left(0)
                 .top(0)
                 .width(40)
                 .height(40)
                 .fill(red()))
      .child(box()
                 .key("faded")
                 .absolute()
                 .left(50)
                 .top(0)
                 .width(40)
                 .height(40)
                 .fill(green())
                 .opacity(0.5f))
      .child(box()
                 .key("turned")
                 .absolute()
                 .left(100)
                 .top(0)
                 .width(40)
                 .height(40)
                 .fill(blue())
                 .rotate(7))
      .child(box()
                 .key("recorded")
                 .absolute()
                 .left(0)
                 .top(50)
                 .width(40)
                 .height(40)
                 .fill(red())
                 .cache(Cache::Picture));
}

}  // namespace

TEST(ComposeCache, AnEagerComposerPromotesTheEligibleNodeOnItsFirstFrame) {
  // The cost rule is a stopwatch, so what it promotes is a fact about the
  // machine: a run that means to TEST promotion asks for the eager policy
  // and gets the whole promotable set, everywhere, from frame one.
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  host.composer.render(eagerPage());
  host.frame();

  const Composer::NodeCost* plain = requireRow(host.composer, "plain");
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->promotion, Composer::Promotion::Promoted)
      << "eager promotion waited for a stopwatch it is not supposed to read";
  EXPECT_EQ(plain->cacheState, Composer::CacheState::Promoted);
  EXPECT_EQ(host.composer.stats().texturesBaked, 1u);
  // The bake is the picture the live paint paints, which is the whole of
  // what promotion may cost — eager or not.
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
}

TEST(ComposeCache, AnEagerComposerRefusesWhatTheCostRuleRefuses) {
  // Eager skips the cost question and NOTHING else. Every refusal is a
  // condition under which a bake would paint different pixels, so the two
  // policies must report the identical set of them for every node — a
  // policy that promoted its way past one would be promoting a picture
  // that moves.
  const auto refusalsUnder = [](Composer::PromotionPolicy policy) {
    Host host;
    host.composer.setAutoTexturePromotion(policy);
    host.composer.setProfiling(true);
    host.composer.render(eagerPage());
    host.frame();
    std::vector<std::pair<std::string, uint16_t>> out;
    for (const char* key : {"plain", "faded", "turned", "recorded"}) {
      const Composer::NodeCost* row = requireRow(host.composer, key);
      out.emplace_back(key, row ? row->refusals : 0xffffu);
    }
    return out;
  };
  const auto byCost = refusalsUnder(Composer::PromotionPolicy::ByCost);
  const auto eager = refusalsUnder(Composer::PromotionPolicy::Eager);
  ASSERT_EQ(byCost.size(), eager.size());
  for (size_t i = 0; i < byCost.size(); ++i)
    EXPECT_EQ(byCost[i].second, eager[i].second)
        << byCost[i].first << " is refused differently by the two policies";

  // And the refusals are the ones each leaf was built to carry, so the
  // comparison above is not two empty sets agreeing.
  Host host;
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Eager);
  host.composer.setProfiling(true);
  host.composer.render(eagerPage());
  host.frame();
  const Composer::NodeCost* faded = requireRow(host.composer, "faded");
  const Composer::NodeCost* turned = requireRow(host.composer, "turned");
  const Composer::NodeCost* recorded = requireRow(host.composer, "recorded");
  ASSERT_NE(faded, nullptr);
  ASSERT_NE(turned, nullptr);
  ASSERT_NE(recorded, nullptr);
  EXPECT_TRUE(faded->refused(Composer::Promotion::Composited));
  EXPECT_TRUE(turned->refused(Composer::Promotion::Transformed));
  EXPECT_TRUE(recorded->refused(Composer::Promotion::OptedOut));
  EXPECT_NE(faded->cacheState, Composer::CacheState::Promoted);
  EXPECT_NE(turned->cacheState, Composer::CacheState::Promoted);
  EXPECT_NE(recorded->cacheState, Composer::CacheState::Promoted);
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
      box()
          .child(box().absolute().left(0).top(0).width(6).height(6).fill(
              Fill::color({1, 1, 1, 1})))
          .child(box().absolute().left(7).top(3).width(3).height(6).fill(
              Fill::color({0.2f, 0.9f, 0.3f, 1}))));
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
    out.child(box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts())));
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
        .child(box()
                   .key("ground")
                   .absolute()
                   .left(0)
                   .top(0)
                   .width(180)
                   .height(180)
                   .fill(Fill::color({0.45f, 0.35f, 0.15f, 1})))
        .child(custom([](SkCanvas& canvas, const PaintContext& ctx) {
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
                   .height(120));
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
      box()
          .child(box().absolute().left(1).top(1).width(14).height(14).fill(
              Fill::color({1, 1, 1, 0.35f})))
          .child(box()
                     .absolute()
                     .left(3)
                     .top(3)
                     .width(10)
                     .height(3)
                     .fill(Fill::color({0.2f, 0.9f, 0.3f, 0.6f}))
                     .rotate(24)));
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
    out.child(box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts())));
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
    out.child(box().absolute().left(10).top(10).width(160).height(160).fill(
        pattern.material(fonts())));
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
    return profiledUnder(box()
                             .key("page")
                             .child(promotablePage())
                             .child(box()
                                        .absolute()
                                        .left(-30)
                                        .top(-14)
                                        .width(260)
                                        .height(40)
                                        .fill(blue())
                                        .rotate(7)));
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
