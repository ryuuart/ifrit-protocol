// What a bake has to reach that the node's own box does not: the ink a
// line, a face, a curve, a shape and a blurred child each draw outside
// it, the grid a curve far from the origin must still stand on, the
// declared shape that bounds every layer the node is given, and the
// dressed coverage that holds ink outside the box without moving the ink
// inside it.

#include <include/core/SkFontMetrics.h>

#include <vector>

#include "support/PromotionTestSupport.h"

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
  page.children(
      {text(u8"a\u0308\u0304\u030a\u0302 e\u0308\u0304\u030a\u0302", style)
           .absolute()
           .left(6)
           .top(20)
           .width(168)});
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
    sheet.children({text(u8"FloatImage, halfFloat gjpqy,", style)
                        .absolute()
                        .left(8)
                        .top(8)
                        .width(280)});
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

/** A STROKED CURVE THAT FILLS ITS NODE, which is what a ring, an arc table,
 *  a dial and a border ornament all are. The stroke's own offset curves are
 *  approximated by cubics whose control points stand outside the ink they
 *  draw, and the page is large enough that the stand-off is several
 *  pixels. */
Element strokedArc() {
  Element page = box().width(400).height(400).fill(Fill::color({0, 0, 0, 1}));
  page.children({box()
                     .absolute()
                     .left(31)
                     .top(31)
                     .width(338)
                     .height(338)
                     .shape(sigil::geometry::shapes::arc(-30.0f))
                     .stroke(FlatStroke{22})});
  return page;
}

/** THE SAME CURVE, STANDING WELL AWAY FROM THE CANVAS ORIGIN. Nothing about
 *  the node changes — only the device coordinates its ink lands on, and the
 *  magnitude those coordinates are computed at. */
Element strokedArcFarFromTheOrigin() {
  Element page = box().width(1500).height(1500).fill(Fill::color({0, 0, 0, 1}));
  page.children({box()
                     .absolute()
                     .left(1100)
                     .top(1100)
                     .width(338)
                     .height(338)
                     .shape(sigil::geometry::shapes::arc(-30.0f))
                     .stroke(FlatStroke{22})});
  return page;
}

}  // namespace

TEST(ComposeCache, APromotedCurveFarFromTheOriginStandsOnTheLivePaintsGrid) {
  // A bake taken on a surface allocated at the node's own corner maps a
  // point through the live matrix with its translation reduced by an
  // integer. The integer is exact; the SUM it enters is not. The live paint
  // rounds at the magnitude of the device coordinate and the offset one
  // rounds at the magnitude of the offset coordinate, so the two land up to
  // half a float step apart — nothing along an edge that meets the grid
  // squarely, a whole supersample bucket where a curve runs nearly tangent
  // to it. The distance from the origin is the whole of the fixture: the
  // same arc at the origin is exact, because there the offset cancels.
  const PromotionDrift drift =
      promotionDriftOf(strokedArcFarFromTheOrigin, SkMatrix::I(), 1500, 1500);
  ASSERT_TRUE(drift.promoted)
      << "nothing was promoted, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the library promoted a curve standing 1100 "
         "pixels from the canvas origin";
}

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
  page.children({ruledNode()});
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
  group.children({box()
                      .width(40)
                      .height(40)
                      .fill(Fill::color({1, 1, 1, 1}))
                      .effect(material::skia::Effect::filter(
                          SkImageFilters::Blur(10, 10, nullptr)))});
  page.children({std::move(group)});
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
    rules.children({box().absolute().left(30).top(30).width(20).height(20).fill(
        Fill::color({0, 0, 1, 1}))});
    rules.opacity(0.6f);
  }
  if (layer == Layer::Effect)
    // A COLOUR FILTER as the layer effect: it maps each pixel where it
    // stands and moves no ink at all, so what the layer's bounds did to the
    // picture is the only thing between the two renders.
    rules.effect(material::skia::Effect::filter(
        SkColorFilters::Blend(SK_ColorGREEN, SkBlendMode::kModulate)));
  page.children({std::move(rules)});
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
  page.children({std::move(node)});
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
