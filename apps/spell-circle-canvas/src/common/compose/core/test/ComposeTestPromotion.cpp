// Automatic texture promotion, judged as pixels: a promoted node must paint
// what the live node painted, under fractional translations and host scales,
// at a new scale after a warm, and through a tile whose texels are partly
// transparent. The cost rule's refusals are named by the leaf that carries
// each condition, and a custom program counts as compositing with the canvas.

#include <include/core/SkFontMetrics.h>
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

namespace {

/** A LINE OF TYPE WHOSE GLYPHS STAND OUTSIDE ITS BOX, which is the shape
 *  of every text leaf and not a contrivance: a leaf is measured to the band
 *  of its lines — the tallest ascent over the deepest descent its faces
 *  report — and the outlines a face draws are not held to that band, so a
 *  tail, a swash or an accent hangs past the box.
 *
 *  The instrument face here is the one that makes the overhang whole pixels
 *  and the same on every machine: its combining marks carry no advance and
 *  are not composed into their base, so a stack of four of them adds ink
 *  above the ascent and no width at all. Nothing clips it — a leaf draws
 *  what it was given — so those pixels are the node's own paint. */
Element typeOutsideItsBox() {
  sigil::weave::TextStyle style;
  style.shaping.typeface = sigil::test::instrument::marks();
  style.shaping.fontSize = 24;
  style.paint.foreground.setColor(SK_ColorWHITE);
  Element page = box().width(180).height(80).fill(red());
  page.child(
      text(u8"a\u0308\u0304\u030a\u0302 e\u0308\u0304\u030a\u0302", style)
          .absolute()
          .left(6)
          .top(20)
          .width(168));
  return page;
}

}  // namespace

TEST(ComposeCache, APromotedLineKeepsTheInkThatStandsOutsideItsBox) {
  // A device bake is a surface sized to the node's paint bounds, and every
  // pixel outside it is lost rather than rounded — a whole coverage value,
  // tens of code values, wherever the live paint had ink there. So the
  // bounds a text leaf reports must hold the GLYPHS and not only the band
  // its lines were measured to.
  const PromotionDrift drift =
      promotionDriftOf(typeOutsideItsBox, SkMatrix::Scale(2, 2), 360, 160);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a line whose glyphs stand "
         "outside its box";
}

TEST(ComposeFaces, APromotedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics) {
  // The same claim about a REAL face, which is where it was found: a line
  // of type at a size a sheet's chrome is set at, on whatever face the
  // machine resolves for a style that names none. Its box is the band of
  // its lines — the tallest ascent over the deepest descent the face
  // reports — and a comma's tail reaches below that band, so the bottom row
  // of the comma is painted outside the box. The case is skipped on a face
  // that keeps its ink inside its own metrics, since there is then nothing
  // for a bake to lose.
  SkFont font(fonts().defaultTypeface(), 14);
  SkFontMetrics fm;
  font.getMetrics(&fm);
  if (fm.fBottom <= fm.fDescent && fm.fTop >= fm.fAscent)
    GTEST_SKIP() << "this machine's face draws no ink outside its metrics";
  const auto page = [] {
    Element sheet = box().width(300).height(60).fill(
        Fill::color(SkColor4f{0.07f, 0.07f, 0.09f, 1}));
    weave::TextStyle style = machineStyleAt(14);
    style.paint.foreground.setColor(SkColorSetRGB(232, 232, 236));
    sheet.child(text(u8"FloatImage, halfFloat gjpqy,", style)
                    .absolute()
                    .left(8)
                    .top(8)
                    .width(280));
    return sheet;
  };
  const PromotionDrift drift =
      promotionDriftOf(page, SkMatrix::Scale(2, 2), 600, 120);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a line of type";
}

namespace {

/** A stroke that declares what it is: a value-comparable scheme that
 *  paints only over what it covers, so the node wearing it is one the
 *  promoter admits. */
struct FlatStroke {
  float width = 22;
  bool operator==(const FlatStroke&) const = default;
  bool blends() const { return false; }
  float bleed() const { return width * 0.5f; }
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint p;
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    p.setColor(SK_ColorWHITE);
    p.setAntiAlias(true);
    canvas.drawPath(ctx.outline, p);
  }
};

/** A STROKED CURVE THAT FILLS ITS NODE, which is what a ring, an arc table,
 *  a dial and a border ornament all are. The stroke's own offset curves are
 *  approximated by cubics whose control points stand outside the ink they
 *  draw, and the page is large enough that the stand-off is several
 *  pixels. */
Element strokedArc() {
  Element page = box().width(400).height(400).fill(Fill::color({0, 0, 0, 1}));
  page.child(box()
                 .absolute()
                 .left(31)
                 .top(31)
                 .width(338)
                 .height(338)
                 .shape(sigil::geometry::shapes::arc(-30.0f))
                 .stroke(FlatStroke{22}));
  return page;
}

}  // namespace

TEST(ComposeCache, APromotedCurveKeepsTheCoverageItsLivePaintComputes) {
  // Skia decides whether a path needs its clipped rasterisation from the
  // path's CONTROL-POINT bounds, and the clipped and unclipped routes do
  // not answer the same antialiased coverage. A bake allocated to exactly
  // what the node paints cuts inside those bounds for every curve, so the
  // promoted picture of a stroked arc moves by tens of code values along
  // its whole length. The bake's margin is what keeps the two on one route.
  const PromotionDrift drift =
      promotionDriftOf(strokedArc, SkMatrix::I(), 420, 420);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a stroked curve";
}

namespace {

/** A SHAPE THAT REACHES PAST THE BOX IT WAS RESOLVED AGAINST, which is what
 *  a ring of rules around a dial, a rule that crosses its cell and any
 *  generator anchored on a centre of its own all are. A Shape is handed the
 *  node's size and nothing holds what it returns to that size, so the ink
 *  is where the path is — and the node's box says nothing about it.
 *
 *  Forty rules on a centre of the node's own, from a radius inside the box
 *  to one well past every edge of it. */
Shape rulesPastTheBox() {
  return Shape([](SkSize size) {
    SkPathBuilder b;
    const SkPoint c{size.width() * 0.5f, size.height() * 0.5f};
    for (int i = 0; i < 40; ++i) {
      const float a = (float)i * 9.0f * 3.14159265f / 180.0f;
      b.moveTo(c.x() + std::cos(a) * 30.0f, c.y() + std::sin(a) * 30.0f);
      b.lineTo(c.x() + std::cos(a) * 110.0f, c.y() + std::sin(a) * 110.0f);
    }
    return b.detach();
  });
}

/** That shape on an 80×80 node in the middle of a 240×240 page, so the ink
 *  stands seventy pixels clear of the box on every side. */
Element ruledNode() {
  return box()
      .absolute()
      .left(80)
      .top(80)
      .width(80)
      .height(80)
      .shape(rulesPastTheBox())
      .fill(Fill::none())
      .stroke(FlatStroke{3});
}

Element blackPage() {
  return box().width(240).height(240).fill(Fill::color({0, 0, 0, 1}));
}

Element shapeOutsideItsBox() {
  Element page = blackPage();
  page.child(ruledNode());
  return page;
}

}  // namespace

namespace {

/** A BLURRED CHILD INSIDE A NODE SIZED TO IT. The child's own filtered
 *  layer draws a skirt well outside the child's box — Skia grows a
 *  filtered saveLayer for its filter, so the halo is painted — and the
 *  group above it is sized to the box alone, so a surface allocated to
 *  that box cuts the skirt off square. The group carries no effect of its
 *  own, which is what keeps it promotable: a filtered node is refused. */
Element blurredChildUnderAGroup() {
  Element page = blackPage();
  Element group = box().absolute().left(100).top(100).width(40).height(40);
  group.child(box()
                  .width(40)
                  .height(40)
                  .fill(Fill::color({1, 1, 1, 1}))
                  .effect(material::skia::Effect::filter(
                      SkImageFilters::Blur(10, 10, nullptr))));
  page.child(std::move(group));
  return page;
}

}  // namespace

TEST(ComposeCache, APromotedNodeKeepsTheSkirtABlurredChildFilters) {
  // What a layer effect paints is not what the content under it covers: a
  // blur, a glow, a shadow put ink outside the box the filtered node was
  // measured to, and that ink is drawn INTO whatever surface the node above
  // was allocated. Allocated to the unfiltered content, the surface cuts
  // the skirt off square — a mark that is gone, not a rounding — so the
  // rect a bake is sized to holds every effect below it at the reach its
  // own filter answers. The LAYERS keep the unfiltered rect: Skia grows a
  // filtered saveLayer for its filter already.
  const PromotionDrift drift =
      promotionDriftOf(blurredChildUnderAGroup, SkMatrix::I(), 240, 240);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a node holding a child "
         "whose blur reaches past its box";
}

TEST(ComposeCache, APromotedShapeKeepsTheInkItDrawsOutsideItsBox) {
  // A device bake is a SURFACE, and ink outside it is lost rather than
  // rounded. The node's box bounds the layers it opens, but it does not
  // bound the path it declares: a Shape resolved against that box may
  // return a curve anywhere, and the surface is filled with that path while
  // every decoration dresses it. So what a bake is allocated to holds the
  // shape as well, and what bounds the drawing stays the clip the bake
  // carries in rather than the allocation.
  const PromotionDrift drift =
      promotionDriftOf(shapeOutsideItsBox, SkMatrix::I(), 240, 240);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a node whose shape reaches "
         "past its box";
}

namespace {

/** THE TWO BOUNDED LAYERS A NODE OPENS OVER ITS OWN CONTENT — the one a
 *  group opacity or blend composites through, and the one a layer effect is
 *  run over. Both are sized from the node's paint bounds and saveLayer
 *  bounds ARE a clip, so a bound that misses the node's ink deletes it. */
enum class Layer { None, GroupOpacity, Effect };

Element ruledNodeUnder(Layer layer) {
  Element page = blackPage();
  Element rules = ruledNode();
  if (layer == Layer::GroupOpacity) {
    // A CHILD, so the opacity opens the group's layer rather than riding
    // the leaf's own fill paint: the fill-only leaf routes blend and
    // opacity onto the paint and never opens a layer at all, which would
    // be a fixture that poses nothing.
    rules.child(box().absolute().left(30).top(30).width(20).height(20).fill(
        Fill::color({0, 0, 1, 1})));
    rules.opacity(0.6f);
  }
  if (layer == Layer::Effect)
    // A COLOUR FILTER as the layer effect: it maps each pixel where it
    // stands and moves no ink at all, so what the layer's bounds did to the
    // picture is the only thing between the two renders.
    rules.effect(material::skia::Effect::filter(
        SkColorFilters::Blend(SK_ColorGREEN, SkBlendMode::kModulate)));
  page.child(std::move(rules));
  return page;
}

/** WHERE THE INK STANDS, as a box. A layer that cut the node's shape
 *  truncates this at the node's own edges whatever it did to the values
 *  inside it, and the tolerance is well under an antialiased edge's
 *  faintest step so no rounding decides the answer. */
SkIRect inkBoundsOf(const std::vector<SkColor>& pixels, int w, int h) {
  SkIRect ink = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor c = pixels[(size_t)y * (size_t)w + (size_t)x];
      const int lit = std::max(
          {(int)SkColorGetR(c), (int)SkColorGetG(c), (int)SkColorGetB(c)});
      if (lit >= 24) ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return ink;
}

/** One scene drawn once with promotion held off, so what the pixels answer
 *  is about the layers a node opens and nothing about a bake. */
std::vector<SkColor> drawnLive(Element page, int w, int h) {
  Host host(w, h);
  host.composer.setAutoTexturePromotion(Composer::PromotionPolicy::Off);
  host.composer.render(std::move(page));
  host.frame();
  return surfaceOf(host, w, h);
}

}  // namespace

TEST(ComposePaintBounds, ADeclaredShapeBoundsEveryLayerTheNodeIsGiven) {
  // ONE RULE WHEREVER A NODE IS SIZED. The shape a node declares is where
  // its ink is, so it bounds the group's opacity layer and the effect's
  // layer exactly as it bounds the surface a bake is allocated to — and a
  // layer is the harsher of the two, since its bounds are a clip and cut
  // the drawing rather than merely holding less of it.
  const SkIRect whole =
      inkBoundsOf(drawnLive(ruledNodeUnder(Layer::None), 240, 240), 240, 240);
  // The fixture has to pose the problem: the rules stand well outside the
  // 80×80 box they were resolved against, on every side.
  ASSERT_LT(whole.left(), 40) << "the fixture's shape stays inside its box";
  ASSERT_GT(whole.right(), 200) << "the fixture's shape stays inside its box";

  const SkIRect faded = inkBoundsOf(
      drawnLive(ruledNodeUnder(Layer::GroupOpacity), 240, 240), 240, 240);
  EXPECT_EQ(faded, whole)
      << "the group's opacity layer cut the shape the node declared: its ink "
         "spans "
      << faded.width() << "x" << faded.height() << " where the same node "
      << "drawn with no layer spans " << whole.width() << "x" << whole.height();

  const SkIRect filtered =
      inkBoundsOf(drawnLive(ruledNodeUnder(Layer::Effect), 240, 240), 240, 240);
  EXPECT_EQ(filtered, whole)
      << "the layer effect's layer cut the shape the node declared: its ink "
         "spans "
      << filtered.width() << "x" << filtered.height() << " where the same "
      << "node drawn with no layer spans " << whole.width() << "x"
      << whole.height();
}

namespace {

/** A HARD-EDGED HALO ALONG WHATEVER OUTLINE THE NODE HANDS IT. A
 *  stroke-and-fill of the outline widens it by exactly `spread` with no
 *  blur, so where the halo's edge stands IS where the boundary stands,
 *  read to the pixel — which a blurred glow's ramp could only be read to
 *  within its falloff. Attached as a BACKGROUND, so it shows only where
 *  the outline reaches outside the node's own fill. */
struct Halo {
  SkColor4f color{0.25f, 0.55f, 1.0f, 1};
  float spread = 8;

  bool operator==(const Halo&) const = default;
  float bleed() const { return spread; }

  void paint(SkCanvas& c, const PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    p.setStyle(SkPaint::kStrokeAndFill_Style);
    p.setStrokeWidth(spread * 2);
    c.drawPath(ctx.outline, p);
  }
};

/** A DECORATION THAT RESERVES ROOM AND DRAWS NOTHING — the whole of what a
 *  bleed is, with the mark taken away. It moves the node's paint bounds by
 *  a fraction of a pixel and moves nothing else, which is the one thing
 *  that can tell a trace reading those bounds apart from a trace reading
 *  the box. */
struct Reserve {
  float extent = 13.5f;

  bool operator==(const Reserve&) const = default;
  float bleed() const { return extent; }
  void paint(SkCanvas&, const PaintContext&) const {}
};

/** A DISC THAT REACHES WELL PAST THE BOX IT WAS RESOLVED AGAINST, and
 *  fills — where `rulesPastTheBox` draws lines, this encloses an area, so
 *  the pixels it covers are a silhouette a coverage trace can answer for. */
Shape discPastTheBox() {
  return Shape([](SkSize size) {
    SkPathBuilder b;
    b.addCircle(size.width() * 0.5f, size.height() * 0.5f, 100.0f);
    return b.detach();
  });
}

/** An 80×80 node in the middle of a 240×240 page, dressed along the
 *  silhouette of what it drew. `past` fills a disc 100 px from the node's
 *  own centre — sixty pixels clear of the box on every side — where the
 *  plain node fills its box and nothing else. */
Element haloedNode(bool past, bool reserving) {
  Element node = box()
                     .absolute()
                     .left(80)
                     .top(80)
                     .width(80)
                     .height(80)
                     .fill(Fill::color({1, 0, 0, 1}))
                     .boundary(Boundary::Coverage)
                     .background(Halo{});
  if (past) node.shape(discPastTheBox());
  if (reserving) node.background(Reserve{});
  Element page = blackPage();
  page.child(std::move(node));
  return page;
}

/** WHERE ONE CARRIER'S INK STANDS, as a box: the pixels where this channel
 *  outweighs the others, so the halo's own extent is read apart from the
 *  fill it is drawn beneath. */
SkIRect channelBoundsOf(const std::vector<SkColor>& pixels, int w, int h,
                        bool blue) {
  SkIRect ink = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const SkColor c = pixels[(size_t)y * (size_t)w + (size_t)x];
      const int r = (int)SkColorGetR(c), b = (int)SkColorGetB(c);
      const bool mine = blue ? b > r + 24 : r > b + 24;
      if (mine) ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return ink;
}

}  // namespace

TEST(ComposeCoverageBounds, ADressedCoverageHoldsTheInkOutsideTheBox) {
  // THE SILHOUETTE IS THE INK, AND THE INK IS NOT THE BOX. A coverage
  // boundary is traced off an alpha raster of what the node drew, and the
  // node draws wherever its carriers put it — a declared shape resolved
  // past the box it was handed, a decoration's bleed, a glyph's overhang, a
  // routed path. A raster allocated at the box traces a silhouette cut
  // square at the box's edges and hands every decoration an outline the
  // node never drew.
  const std::vector<SkColor> pixels =
      drawnLive(haloedNode(true, false), 240, 240);
  const SkIRect figure = channelBoundsOf(pixels, 240, 240, false);
  const SkIRect halo = channelBoundsOf(pixels, 240, 240, true);
  // The fixture has to pose the problem: the disc stands sixty pixels clear
  // of the 80×80 box on every side.
  ASSERT_LT(figure.left(), 40) << "the fixture's shape stays inside its box";
  ASSERT_GT(figure.right(), 200) << "the fixture's shape stays inside its box";

  ASSERT_FALSE(halo.isEmpty())
      << "the halo dressed a boundary cut at the node's box, which stands "
         "wholly under the disc the node filled, so none of it is visible";
  EXPECT_LE(halo.left(), figure.left() - 6)
      << "the halo stops at " << halo.left() << " where the ink it dresses "
      << "reaches " << figure.left();
  EXPECT_LE(halo.top(), figure.top() - 6)
      << "the halo stops at " << halo.top() << " where the ink it dresses "
      << "reaches " << figure.top();
  EXPECT_GE(halo.right(), figure.right() + 6)
      << "the halo stops at " << halo.right() << " where the ink it dresses "
      << "reaches " << figure.right();
  EXPECT_GE(halo.bottom(), figure.bottom() + 6)
      << "the halo stops at " << halo.bottom() << " where the ink it dresses "
      << "reaches " << figure.bottom();
}

TEST(ComposeCoverageBounds, InkInsideTheBoxTracesWhereItAlwaysDid) {
  // A WIDER RASTER IS NOT A DIFFERENT ANSWER. The trace covers the node's
  // paint bounds, so a carrier standing outside the box moves the rect the
  // raster is allocated at — and the boundary is a staircase of whole
  // steps, so a rect that moved the grid by a fraction of one would restep
  // every edge in the picture for a carrier that changed no pixel. The
  // grid is placed on whole steps instead, and a node whose ink stays
  // inside its box traces the path it always did.
  const std::vector<SkColor> pixels =
      drawnLive(haloedNode(false, false), 240, 240);
  const SkIRect halo = channelBoundsOf(pixels, 240, 240, true);
  // The node fills its 80×80 box at (80, 80) and the halo widens whatever
  // outline it is handed by eight pixels, so the boundary being the box
  // itself is one exact rect and nothing else.
  EXPECT_EQ(halo, SkIRect::MakeLTRB(72, 72, 168, 168))
      << "the traced boundary is not the node's own box: the halo around it "
         "spans "
      << halo.width() << "x" << halo.height() << " at (" << halo.left() << ", "
      << halo.top() << ")";

  const std::vector<SkColor> reserved =
      drawnLive(haloedNode(false, true), 240, 240);
  ASSERT_EQ(pixels.size(), reserved.size());
  size_t differing = 0;
  for (size_t i = 0; i < pixels.size(); ++i)
    if (pixels[i] != reserved[i]) ++differing;
  EXPECT_EQ(differing, 0u)
      << differing
      << " pixels moved when a decoration reserved 13.5 px and drew nothing, "
         "so the raster's grid follows the paint bounds' own fraction rather "
         "than the node's steps";
}

namespace {

/** A PANEL THAT OPENS OVER WHAT IT HOLDS. The window clips its content to
 *  its own box and the box's height is the reveal, so the mark inside is
 *  cut on the frames the reveal is short and whole once it has run. The
 *  mark's OWN description never changes across the two: same content, same
 *  box, same place, same paint bounds — everything a bake's staleness rules
 *  compare stands still while the only thing that moved is the clip. */
Element revealedPage(float reveal) {
  Element page = box().width(240).height(240).fill(Fill::color({0, 0, 0, 1}));
  Element window =
      box().absolute().left(0).top(0).width(240).height(reveal).clip();
  window.child(box()
                   .absolute()
                   .left(20)
                   .top(20)
                   .width(200)
                   .height(200)
                   .fill(material::skia::Paint::sksl(gridEffect()))
                   .key("mark"));
  page.child(window.key("window"));
  return page;
}

/** The reveal, warmed short and then opened: thirty frames with the clip
 *  cutting the mark — long enough for the promoter to have baked whatever
 *  it is going to — and then the same tree with the window open. */
std::vector<SkColor> revealedThenOpen(bool promotion, int w, int h,
                                      bool* promotedOut) {
  Host host(w, h);
  host.composer.setAutoTexturePromotion(promotion
                                            ? Composer::PromotionPolicy::Eager
                                            : Composer::PromotionPolicy::Off);
  host.composer.setProfiling(true);
  host.composer.render(profiledUnder(revealedPage(60).key("page")));
  for (int i = 0; i < 30; ++i) host.frame();
  if (promotedOut)
    for (const Composer::NodeCost& row : host.composer.profile())
      *promotedOut |= row.cacheState == Composer::CacheState::Promoted;
  host.composer.render(profiledUnder(revealedPage((float)h).key("page")));
  for (int i = 0; i < 5; ++i) host.frame();
  return surfaceOf(host, w, h);
}

}  // namespace

TEST(ComposeCache, APromotedNodeIsRebakedWhenTheClipThatCutItOpens) {
  // A device bake carries the canvas's own clip into its layer, so what it
  // holds is the node's paint as that clip left it. A clip narrows and
  // widens for reasons the node's own bounds cannot see — a panel opening,
  // a window growing, an ancestor's layer standing over the box while it
  // fades in — and nothing else the staleness rules compare moves with it:
  // the paint bounds are the same bounds whatever the clip did to them. So
  // a bake taken under the narrow clip and held past it blits the CUT, and
  // the marks the clip removed never come back.
  bool promoted = false;
  const std::vector<SkColor> live = revealedThenOpen(false, 240, 240, nullptr);
  const std::vector<SkColor> baked =
      revealedThenOpen(true, 240, 240, &promoted);
  ASSERT_TRUE(promoted)
      << "nothing was promoted while the clip was narrow, so this compared "
         "two live renders";
  ASSERT_EQ(live.size(), baked.size());
  size_t differing = 0;
  const int worst = worstDrift(live, baked, &differing);
  EXPECT_LE(worst, 1) << differing << " pixels moved, worst " << worst
                      << " code values, after the clip that cut the bake "
                         "opened";
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
  page.child(promotablePage().absolute().left(0).top(10).key("panel"));
  sigil::weave::TextStyle lit = whiteStyle(40);
  lit.paint.foreground.setColor(SkColorSetRGB(120, 200, 255));
  lit.paint.foreground.setBlendMode(SkBlendMode::kPlus);
  page.child(text(u8"LIT", lit).absolute().left(140).top(70).width(150));
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
