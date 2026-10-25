// What a device bake has to reach that the node's own box does not: the
// ink a line, a face, a curve, a shape and a blurred child each draw
// outside it, the grid a curve far from the origin must still stand on, the
// rasteriser route a bake rests on, the declared shape that bounds every
// layer the node is given, and the dressed coverage that holds ink outside
// the box without moving the ink inside it.

#include <include/core/SkFontMetrics.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>

#include <cmath>
#include <vector>

#include "support/BakeTestSupport.h"

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

TEST(ComposeCache, ABakedLineKeepsTheInkThatStandsOutsideItsBox) {
  // A device bake is a surface sized to the node's paint bounds, and every
  // pixel outside it is lost rather than rounded — a whole coverage value,
  // tens of code values, wherever the live paint had ink there. So the
  // bounds a text leaf reports must hold the GLYPHS and not only the band
  // its lines were measured to.
  const BakeDrift drift =
      bakeDriftOf(typeOutsideItsBox, SkMatrix::Scale(2, 2), 360, 160);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a line whose glyphs "
         "stand "
         "outside its box";
}

TEST(ComposeFaces, ABakedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics) {
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
  const BakeDrift drift = bakeDriftOf(page, SkMatrix::Scale(2, 2), 600, 120);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a line of type";
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

TEST(ComposeCache, ABakedCurveFarFromTheOriginStandsOnTheLivePaintsGrid) {
  // A bake taken on a surface allocated at the node's own corner maps a
  // point through the live matrix with its translation reduced by an
  // integer. The integer is exact; the SUM it enters is not. The live paint
  // rounds at the magnitude of the device coordinate and the offset one
  // rounds at the magnitude of the offset coordinate, so the two land up to
  // half a float step apart — nothing along an edge that meets the grid
  // squarely, a whole supersample bucket where a curve runs nearly tangent
  // to it. The distance from the origin is the whole of the fixture: the
  // same arc at the origin is exact, because there the offset cancels.
  const BakeDrift drift =
      bakeDriftOf(strokedArcFarFromTheOrigin, SkMatrix::I(), 1500, 1500);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a curve standing 1100 "
         "pixels from the canvas origin";
}

TEST(ComposeCache, ABakedCurveKeepsTheCoverageItsLivePaintComputes) {
  // Skia decides whether a path needs its clipped rasterisation from the
  // path's CONTROL-POINT bounds, and the clipped and unclipped routes do
  // not answer the same antialiased coverage. A bake allocated to exactly
  // what the node paints cuts inside those bounds for every curve, so the
  // baked picture of a stroked arc moves by tens of code values along
  // its whole length. The bake's margin is what keeps the two on one route.
  const BakeDrift drift = bakeDriftOf(strokedArc, SkMatrix::I(), 420, 420);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a stroked curve";
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
  return Shape(skiaShape([](SkSize size) {
    SkPathBuilder b;
    const SkPoint c{size.width() * 0.5f, size.height() * 0.5f};
    for (int i = 0; i < 40; ++i) {
      const float a = (float)i * 9.0f * 3.14159265f / 180.0f;
      b.moveTo(c.x() + std::cos(a) * 30.0f, c.y() + std::sin(a) * 30.0f);
      b.lineTo(c.x() + std::cos(a) * 110.0f, c.y() + std::sin(a) * 110.0f);
    }
    return b.detach();
  }));
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
 *  own, so what holds the skirt is the reach its child's filter answers. */
Element blurredChildUnderAGroup() {
  Element page = blackPage();
  Element group = box().absolute().left(100).top(100).width(40).height(40);
  group.children({box()
                      .width(40)
                      .height(40)
                      .fill(Fill::color({1, 1, 1, 1}))
                      .filter(material::skia::filter(
                          SkImageFilters::Blur(10, 10, nullptr)))});
  page.children({std::move(group)});
  return page;
}

}  // namespace

TEST(ComposeCache, ABakedNodeKeepsTheSkirtABlurredChildFilters) {
  // What a layer effect paints is not what the content under it covers: a
  // blur, a glow, a shadow put ink outside the box the filtered node was
  // measured to, and that ink is drawn INTO whatever surface the node above
  // was allocated. Allocated to the unfiltered content, the surface cuts
  // the skirt off square — a mark that is gone, not a rounding — so the
  // rect a bake is sized to holds every effect below it at the reach its
  // own filter answers. The LAYERS keep the unfiltered rect: Skia grows a
  // filtered saveLayer for its filter already.
  const BakeDrift drift =
      bakeDriftOf(blurredChildUnderAGroup, SkMatrix::I(), 240, 240);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 2)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a node holding a child "
         "whose blur reaches past its box";
}

TEST(ComposeCache, ABakedShapeKeepsTheInkItDrawsOutsideItsBox) {
  // A device bake is a SURFACE, and ink outside it is lost rather than
  // rounded. The node's box bounds the layers it opens, but it does not
  // bound the path it declares: a Shape resolved against that box may
  // return a curve anywhere, and the surface is filled with that path while
  // every decoration dresses it. So what a bake is allocated to holds the
  // shape as well, and what bounds the drawing stays the clip the bake
  // carries in rather than the allocation.
  const BakeDrift drift =
      bakeDriftOf(shapeOutsideItsBox, SkMatrix::I(), 240, 240);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when the page was baked holding a node whose shape "
         "reaches "
         "past its box";
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
 *  a layer's bounds are snapped — then every baked curve would stand a
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
    rules.filter(material::skia::filter(
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

/** One scene drawn once, so what the pixels answer is about the layers a
 *  node opens and nothing about a bake. */
std::vector<SkColor> drawnLive(Element page, int w, int h) {
  Host host(w, h);
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

  void paint(sigil::draw::Pen& pen, const PaintContext& ctx) const {
    SkCanvas& c = *pen.canvas();
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    p.setStyle(SkPaint::kStrokeAndFill_Style);
    p.setStrokeWidth(spread * 2);
    c.drawPath(geometry::path::toSk(ctx.outline), p);
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
  void paint(sigil::draw::Pen& pen, const PaintContext&) const {}
};

/** A DISC THAT REACHES WELL PAST THE BOX IT WAS RESOLVED AGAINST, and
 *  fills — where `rulesPastTheBox` draws lines, this encloses an area, so
 *  the pixels it covers are a silhouette a coverage trace can answer for. */
Shape discPastTheBox() {
  return Shape(skiaShape([](SkSize size) {
    SkPathBuilder b;
    b.addCircle(size.width() * 0.5f, size.height() * 0.5f, 100.0f);
    return b.detach();
  }));
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
                     .decorationOutline(Boundary::Coverage)
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

TEST(ComposeCache, PaintBoundsBeyondIntegerCoordinatesPaintTheLivePicture) {
  // A declared shape whose bounds stand far past any integer device
  // coordinate: the rect a device bake is measured to is cropped to the
  // clip before it is narrowed, so the bake paints what the live paint does
  // rather than overflowing into a rect that stands nowhere.
  const auto scene = [](Cache caching) {
    return box()
        .cache(Cache::None)
        .children({box()
                       .key("largeShape")
                       .width(80)
                       .height(80)
                       .cache(caching)
                       .shape(skiaShape([] {
                         SkPathBuilder path;
                         path.addRect(SkRect::MakeLTRB(-1.0e20f, -1.0e20f,
                                                       1.0e20f, 1.0e20f));
                         return path.detach();
                       }))
                       .fill(red())});
  };
  Host plain, baked;
  plain.composer.render(scene(Cache::Auto));
  baked.composer.render(scene(Cache::Texture));
  plain.frame();
  baked.frame();
  EXPECT_TRUE(identicalPixels(plain, baked, 200, 200));
  EXPECT_EQ(baked.pixel(40, 40), SK_ColorRED);
}

namespace {

/** A FIVE-POINTED STAR WHOSE TIPS TOUCH ITS BOX, stroked by the stock
 *  brush under its defaults: a mitred join at the renderer's limit of four
 *  half-widths. A star's tip turns by 144 degrees, so its mitre stands
 *  about three and a quarter half-widths past the tip, which is past the
 *  box and past half the width both. */
Element starWhoseTipsTouchItsBox() {
  return box()
      .width(200)
      .height(200)
      .shape(skiaShape([] {
        SkPathBuilder star;
        for (int point = 0; point < 10; ++point) {
          const float radius = point % 2 ? 40.0f : 100.0f;
          const float angle = -1.5707963f + 3.14159265f * (float)point / 5.0f;
          const SkPoint at{100 + radius * std::cos(angle),
                           100 + radius * std::sin(angle)};
          if (point == 0)
            star.moveTo(at);
          else
            star.lineTo(at);
        }
        star.close();
        return star.detach();
      }))
      .fill(Fill::none())
      .stroke(brush::solid(6, Fill::color({1, 1, 1, 1})));
}

}  // namespace

TEST(ComposeCache, ABakedStarKeepsTheMitredTipsPastItsBox) {
  // The stock stroke meets its corners mitred, and its bleed is the whole
  // mitre, so a bake sized from it holds every tip the live paint draws.
  const BakeDrift drift = bakeDriftOf(starWhoseTipsTouchItsBox,
                                      SkMatrix::Translate(20, 20), 240, 240);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when a star whose mitred tips stand past its box "
         "was baked";
}

namespace {

/** A BOX WALKED BY SQUARES WIDER THAN ITS MARGIN: every sample on its
 *  outline stamps a square thirty units across, centred on the outline,
 *  so half of each stands outside the box. The walk declares that half. */
Element walkedPastItsBox() {
  ContourWalk walk;
  walk.spacing = 40;
  walk.stamp = box().width(30).height(30).fill(Fill::color({1, 1, 1, 1}));
  walk.bleedPx = 15;
  return box().width(200).height(200).foreground(std::move(walk));
}

}  // namespace

TEST(ComposeCache, ABakedWalkKeepsTheStampsPastItsBox) {
  const BakeDrift drift =
      bakeDriftOf(walkedPastItsBox, SkMatrix::Translate(20, 20), 240, 240);
  ASSERT_TRUE(drift.baked)
      << "nothing was baked, so this compared two live renders";
  EXPECT_LE(drift.worstChannel, 1)
      << drift.differingPixels << " pixels moved, worst " << drift.worstChannel
      << " code values, when a box walked by stamps wider than its margin "
         "was baked";
}
