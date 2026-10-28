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
#include "support/Rows.h"

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

TEST(ComposeFaces, ABakedLineKeepsTheInkAFaceDrawsOutsideItsOwnMetrics) {
  // ADeviceBakePaintsThePictureItsLivePaintPaints, on the machine's own
  // face: a line of type at a size a sheet's chrome is set at, on whatever
  // face the machine resolves for a style that names none. Its box is the band
  // of its lines — the tallest ascent over the deepest descent the face reports
  // — and a comma's tail reaches below that band, so the bottom row of the
  // comma is painted outside the box. The case is skipped on a face that keeps
  // its ink inside its own metrics, since there is then nothing for a bake to
  // lose.
  SkFont font(fonts().defaultTypeface(), 14);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  if (metrics.fBottom <= metrics.fDescent && metrics.fTop >= metrics.fAscent)
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
    SkPathBuilder builder;
    const SkPoint centre{size.width() * 0.5f, size.height() * 0.5f};
    for (int index = 0; index < 40; ++index) {
      const float angle = (float)index * 9.0f * 3.14159265f / 180.0f;
      builder.moveTo(centre.x() + std::cos(angle) * 30.0f,
                     centre.y() + std::sin(angle) * 30.0f);
      builder.lineTo(centre.x() + std::cos(angle) * 110.0f,
                     centre.y() + std::sin(angle) * 110.0f);
    }
    return builder.detach();
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
  constexpr int kWidth = 420, kHeight = 420;
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
    return SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
  };
  const auto pixels = [](SkSurface& from) {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWidth, kHeight));
    from.readPixels(bitmap.pixmap(), 0, 0);
    std::vector<SkColor> out;
    for (int y = 0; y < kHeight; ++y)
      for (int x = 0; x < kWidth; ++x) out.push_back(bitmap.getColor(x, y));
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

  const std::vector<SkColor> livePixels = pixels(*live),
                             layeredPixels = pixels(*layered),
                             blittedPixels = pixels(*blitted),
                             overGroundPixels = pixels(*overGround);
  return {worstDrift(layeredPixels, blittedPixels, nullptr),
          worstDrift(livePixels, overGroundPixels, nullptr),
          worstDrift(livePixels, blittedPixels, nullptr)};
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
SkIRect inkBoundsOf(const std::vector<SkColor>& pixels, int width, int height) {
  SkIRect ink = SkIRect::MakeEmpty();
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const SkColor color = pixels[(size_t)y * (size_t)width + (size_t)x];
      const int lit =
          std::max({(int)SkColorGetR(color), (int)SkColorGetG(color),
                    (int)SkColorGetB(color)});
      if (lit >= 24) ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return ink;
}

/** One scene drawn once, so what the pixels answer is about the layers a
 *  node opens and nothing about a bake. */
std::vector<SkColor> drawnLive(Element page, int width, int height) {
  Host host(width, height);
  host.composer.render(std::move(page));
  host.frame();
  return surfaceOf(host, width, height);
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
    SkCanvas& canvas = *pen.canvas();
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor4f(color, nullptr);
    paint.setStyle(SkPaint::kStrokeAndFill_Style);
    paint.setStrokeWidth(spread * 2);
    canvas.drawPath(geometry::path::toSk(ctx.outline), paint);
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
    SkPathBuilder builder;
    builder.addCircle(size.width() * 0.5f, size.height() * 0.5f, 100.0f);
    return builder.detach();
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
SkIRect channelBoundsOf(const std::vector<SkColor>& pixels, int width,
                        int height, bool blue) {
  SkIRect ink = SkIRect::MakeEmpty();
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const SkColor color = pixels[(size_t)y * (size_t)width + (size_t)x];
      const int redValue = (int)SkColorGetR(color),
                blueValue = (int)SkColorGetB(color);
      const bool mine =
          blue ? blueValue > redValue + 24 : redValue > blueValue + 24;
      if (mine) ink.join(SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return ink;
}

}  // namespace

// A coverage boundary is traced where the node's ink stands, outside its box
// as well, on a grid that a bleed drawing nothing does not move.
TEST(ComposeCoverageBounds, ACoverageTraceFollowsTheInkOnAGridOfWholeSteps) {
  sigil::compose::test::checkRows({
      {"ADressedCoverageHoldsTheInkOutsideTheBox",
       [] {
         // THE SILHOUETTE IS THE INK, AND THE INK IS NOT THE BOX. A coverage
         // boundary is traced off an alpha raster of what the node drew, and
         // the node draws wherever its carriers put it — a declared shape
         // resolved past the box it was handed, a decoration's bleed, a glyph's
         // overhang, a routed path. A raster allocated at the box traces a
         // silhouette cut square at the box's edges and hands every decoration
         // an outline the node never drew.
         const std::vector<SkColor> pixels =
             drawnLive(haloedNode(true, false), 240, 240);
         const SkIRect figure = channelBoundsOf(pixels, 240, 240, false);
         const SkIRect halo = channelBoundsOf(pixels, 240, 240, true);
         // The fixture has to pose the problem: the disc stands sixty pixels
         // clear of the 80×80 box on every side.
         ASSERT_LT(figure.left(), 40)
             << "the fixture's shape stays inside its box";
         ASSERT_GT(figure.right(), 200)
             << "the fixture's shape stays inside its box";

         ASSERT_FALSE(halo.isEmpty()) << "the halo dressed a boundary cut at "
                                         "the node's box, which stands "
                                         "wholly under the disc the node "
                                         "filled, so none of it is visible";
         EXPECT_LE(halo.left(), figure.left() - 6)
             << "the halo stops at " << halo.left()
             << " where the ink it dresses "
             << "reaches " << figure.left();
         EXPECT_LE(halo.top(), figure.top() - 6)
             << "the halo stops at " << halo.top()
             << " where the ink it dresses "
             << "reaches " << figure.top();
         EXPECT_GE(halo.right(), figure.right() + 6)
             << "the halo stops at " << halo.right()
             << " where the ink it dresses "
             << "reaches " << figure.right();
         EXPECT_GE(halo.bottom(), figure.bottom() + 6)
             << "the halo stops at " << halo.bottom()
             << " where the ink it dresses "
             << "reaches " << figure.bottom();
       }},
      {"InkInsideTheBoxTracesTheBox",
       [] {
         // A WIDER RASTER IS NOT A DIFFERENT ANSWER. The trace covers the
         // node's paint bounds, so a carrier standing outside the box moves the
         // rect the raster is allocated at — and the boundary is a staircase of
         // whole steps, so a rect that moved the grid by a fraction of one
         // would restep every edge in the picture for a carrier that changed no
         // pixel. The grid is placed on whole steps instead, and a node whose
         // ink stays inside its box traces the path it always did.
         const std::vector<SkColor> pixels =
             drawnLive(haloedNode(false, false), 240, 240);
         const SkIRect halo = channelBoundsOf(pixels, 240, 240, true);
         // The node fills its 80×80 box at (80, 80) and the halo widens
         // whatever outline it is handed by eight pixels, so the boundary being
         // the box itself is one exact rect and nothing else.
         EXPECT_EQ(halo, SkIRect::MakeLTRB(72, 72, 168, 168))
             << "the traced boundary is not the node's own box: the halo "
                "around it "
                "spans "
             << halo.width() << "x" << halo.height() << " at (" << halo.left()
             << ", " << halo.top() << ")";

         const std::vector<SkColor> reserved =
             drawnLive(haloedNode(false, true), 240, 240);
         ASSERT_EQ(pixels.size(), reserved.size());
         size_t differing = 0;
         for (size_t index = 0; index < pixels.size(); ++index)
           if (pixels[index] != reserved[index]) ++differing;
         EXPECT_EQ(differing, 0u) << differing
                                  << " pixels moved when a decoration reserved "
                                     "13.5 px and drew nothing, "
                                     "so the raster's grid follows the paint "
                                     "bounds' own fraction rather "
                                     "than the node's steps";
       }},
  });
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

namespace {

/** A page whose root is baked in device space, the matrix the host draws it
 *  under, the canvas it is drawn on, and how many code values a channel of
 *  the baked page may stand from the live one. */
struct DeviceBakeRow {
  const char* name;
  Element (*page)();
  SkMatrix hostMatrix;
  int width, height;
  int worstChannel;
};

}  // namespace

// A device bake is the live paint's picture within one code value a channel,
// whatever the node draws outside its box and wherever on the canvas it
// stands: the bake holds the glyphs past a line's band, a shape's path, a
// blurred child's skirt, a mitre's tips and a walk's stamps, carries a
// margin clear of a curve's control points, and stands on the canvas's own
// grid.
TEST(ComposeCache, ADeviceBakePaintsThePictureItsLivePaintPaints) {
  const DeviceBakeRow rows[] = {
      // A leaf is measured to the band of its lines, and a face's marks
      // hang past it; the bounds a text leaf reports hold the glyphs.
      {"ALineWhoseGlyphsStandOutsideItsBox", typeOutsideItsBox,
       SkMatrix::Scale(2, 2), 360, 160, 1},
      // The live paint rounds at the magnitude of the device coordinate;
      // a bake standing on the canvas's own grid rounds the same, where a
      // bake offset to its corner lands up to half a float step away.
      {"ACurveFarFromTheOrigin", strokedArcFarFromTheOrigin, SkMatrix::I(),
       1500, 1500, 2},
      // Skia picks a curve's rasterisation route from its control-point
      // bounds; the bake's margin keeps it on the live paint's route.
      {"AStrokedCurve", strokedArc, SkMatrix::I(), 420, 420, 1},
      // A child's filter paints outside the box it was measured to, into
      // whatever its parent was given room in.
      {"ANodeOverABlurredChild", blurredChildUnderAGroup, SkMatrix::I(), 240,
       240, 2},
      // A shape resolved against the box may return a path anywhere.
      {"AShapeReachingPastItsBox", shapeOutsideItsBox, SkMatrix::I(), 240, 240,
       1},
      // The stock stroke's bleed holds its whole mitre.
      {"AStarWhoseMitredTipsStandPastItsBox", starWhoseTipsTouchItsBox,
       SkMatrix::Translate(20, 20), 240, 240, 1},
      // A walk declares the half of each stamp that stands outside the box.
      {"AWalkWhoseStampsStandPastItsBox", walkedPastItsBox,
       SkMatrix::Translate(20, 20), 240, 240, 1},
  };
  sigil::compose::test::forEachRow(rows, [](const DeviceBakeRow& row) {
    const BakeDrift drift =
        bakeDriftOf(row.page, row.hostMatrix, row.width, row.height);
    ASSERT_TRUE(drift.baked)
        << "nothing was baked, so this compared two live renders";
    EXPECT_LE(drift.worstChannel, row.worstChannel)
        << drift.differingPixels << " pixels moved, worst "
        << drift.worstChannel << " code values";
  });

  // A declared shape whose bounds stand far past any integer device
  // coordinate: the rect a device bake is measured to is cropped to the
  // clip before it is narrowed, so the bake paints what the live paint does.
  SCOPED_TRACE("AShapeBeyondEveryIntegerCoordinate");
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
