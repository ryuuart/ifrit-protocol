// The layout schemes and the shape values a node is cut against: where a
// scheme puts its children, what a silhouette is as a value, and the
// derived routes that read a laid-out box back.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/core/Feed.h>

#include <numeric>

#include "support/ShapeTestSupport.h"

namespace {

/** How tall the paragraph came out. It is the one number that reads
 *  "how much room the geometry offered": the same words in the same box
 *  need fewer lines when the exclusion gives room back. */
float flowedHeight(Host& host, const char* key) {
  std::optional<SkRect> bounds = host.composer.bounds(key);
  return bounds ? bounds->height() : 0.0f;
}

/** Words placed on the line band the caller names — the direct reading of
 *  "this line's intervals were longer". */
int inkInBand(Host& host, SkIRect band) {
  int lit = 0;
  for (int y = band.top(); y < band.bottom(); ++y)
    for (int x = band.left(); x < band.right(); ++x)
      if (host.pixel(x, y) == SK_ColorWHITE) ++lit;
  return lit;
}

const std::u8string& flowBody() {
  // Long enough to run past the obstacle on every geometry, so a height
  // comparison reads room-per-line and not "the text stopped early"; and
  // set in words of one letter, so the rag is never further than one
  // word pitch from whatever edge the flow subtracted. At 15 px in the
  // instrument face that pitch is 13.5 px (a 9 px letter and a 4.5 px
  // space): three hundred words are about seventeen lines in the 360 px
  // box, which reaches well past the obstacle and stays well inside the
  // 460 px host with any silhouette on it, and a corner the flow gives
  // back takes a word wherever it is wider than the pitch.
  static const std::u8string body = [] {
    std::u8string all;
    for (int i = 0; i < 300; ++i) all += u8"o ";
    return all;
  }();
  return body;
}

/** One paragraph flowing around one keyed target of the caller's making. */
Element flowScene(Element target, float margin) {
  return stack()
      .child(std::move(target))
      .child(box()
                 .inset(0)
                 .child(text(flowBody(), whiteStyle(15))
                            .key("body")
                            .flowAround("obstacle", margin))
                 .zIndex(1));
}

Element obstacleBox(Shape silhouette) {
  Element el = box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(100)
                   .top(40)
                   .fill(Fill::color({0, 0.4f, 0, 1}));
  if (silhouette) el.shape(std::move(silhouette));
  return el;
}

}  // namespace

TEST(ComposeDerive, FlowAroundFollowsACircleSilhouette) {
  // A round target gives back the four corners its bounding box was
  // eating, so the same paragraph in the same box gets measurably more
  // room — and the corners themselves take type.
  Host boxed(360, 460), disc(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  disc.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 6));
  disc.frame();

  EXPECT_LT(flowedHeight(disc, "body"), flowedHeight(boxed, "body"));
  // The corner: inside the box, outside the circle. Type reaches it only
  // under the silhouette.
  const SkIRect corner = SkIRect::MakeLTRB(103, 43, 127, 67);
  EXPECT_FALSE(anyWhiteIn(boxed, corner));
  EXPECT_TRUE(anyWhiteIn(disc, corner));
  // The middle of the disc stays clear either way.
  EXPECT_FALSE(anyWhiteIn(disc, SkIRect::MakeLTRB(150, 100, 210, 140)));
}

TEST(ComposeDerive, FlowAroundFollowsAStarSilhouette) {
  // A concave silhouette is the case a bounding box cannot approximate:
  // text runs INTO the notches between the points.
  Host boxed(360, 460), star(360, 460);
  boxed.composer.render(flowScene(obstacleBox({}), 6));
  boxed.frame();
  star.composer.render(flowScene(obstacleBox(geometry::shapes::star(5)), 6));
  star.frame();

  EXPECT_LT(flowedHeight(star, "body"), flowedHeight(boxed, "body"));
  // The concave half: a star leaves far more of its bounding box open
  // than a disc does, so the notches and corners take more type than the
  // round silhouette can.
  const SkIRect band = SkIRect::MakeLTRB(103, 43, 257, 197);
  EXPECT_GT(inkInBand(star, band), inkInBand(boxed, band));
  // The star's own body still refuses type at its centre.
  EXPECT_FALSE(anyWhiteIn(star, SkIRect::MakeLTRB(165, 105, 195, 135)));
}

TEST(ComposeDerive, FlowAroundMarginHoldsOffTheSilhouette) {
  // The margin means one thing on a silhouette and on a box alike: a
  // standoff from whatever edge is being subtracted. A wider one buys the
  // paragraph less room, never more.
  Host tight(360, 460), wide(360, 460);
  tight.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 2));
  tight.frame();
  wide.composer.render(flowScene(obstacleBox(geometry::shapes::circle()), 26));
  wide.frame();
  EXPECT_LT(flowedHeight(tight, "body"), flowedHeight(wide, "body"));
}

TEST(ComposeDerive, FlowAroundSilhouetteTracksAMovingTarget) {
  // Moving targets already re-derive; a silhouette target must too.
  auto scene = [](float left) {
    return stack()
        .child(box()
                   .key("obstacle")
                   .width(160)
                   .height(160)
                   .left(left)
                   .top(40)
                   .shape(geometry::shapes::circle())
                   .fill(Fill::color({0, 0.4f, 0, 1})))
        .child(box()
                   .inset(0)
                   .child(text(flowBody(), whiteStyle(15))
                              .key("body")
                              .flowAround("obstacle", 6))
                   .zIndex(1));
  };
  Host host(360, 460);
  host.composer.render(scene(100));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
  host.composer.render(scene(20));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeLTRB(85, 105, 115, 135)));
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB(165, 105, 195, 135)));
}

TEST(ComposeShapeValues, CustomOutlineShapesFillAndClip) {
  Host host;
  // A diamond outline over a 100x100 box: the box's corner pixels sit
  // outside the shape, so fill and clipped children must not reach them.
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  host.composer.render(
      box().width(100).height(100).clip().shape(diamond).fill(red()).child(
          box().inset(0).absolute().fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorGREEN);  // clipped child inside
  EXPECT_EQ(host.pixel(3, 3), SK_ColorBLACK);    // box corner outside shape
}

TEST(ComposeShapeValues, RoundedOutlineCutsSharpCorners) {
  Host host;
  auto diamond = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(s.width() / 2, 0);
    b.lineTo(s.width(), s.height() / 2);
    b.lineTo(s.width() / 2, s.height());
    b.lineTo(0, s.height() / 2);
    b.close();
    return b.detach();
  };
  // Nested (the root always fills the viewport); radius 20 pulls the
  // 100x100 diamond's top vertex from y=0 down to y≈7.
  host.composer.render(
      box().child(box()
                      .width(100)
                      .height(100)
                      .shape(geometry::shapes::rounded(diamond, 20))
                      .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);   // body intact
  EXPECT_EQ(host.pixel(50, 3), SK_ColorBLACK);  // sharp tip rounded away
  EXPECT_EQ(host.pixel(50, 12), SK_ColorRED);   // rounded apex below y≈7

  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(100)
                                       .shape(geometry::shapes::star(5))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);    // star body
  EXPECT_NE(host.pixel(50, 3), SK_ColorBLACK);   // sharp top point present
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // gap between arms
}

TEST(ComposeShapeValues, PerCornerRadiiIndependent) {
  Host host;
  // Sharp top-left, heavily rounded top-right.
  host.composer.render(box().child(
      box().width(100).height(100).corners({0, 40, 0, 0}).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(2, 2), SK_ColorRED);     // sharp TL corner filled
  EXPECT_EQ(host.pixel(97, 2), SK_ColorBLACK);  // rounded TR corner empty
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
}

// ---------------------------------------------------------------------------
// Shape kit (Shapes.h): organic generators, per-edge extraction.

TEST(ComposeShapeValues, PolygonAndSquircleSilhouettes) {
  Host host;
  host.composer.render(box()
                           .row()
                           .child(box()
                                      .width(90)
                                      .height(90)
                                      .shape(geometry::shapes::polygon(6))
                                      .fill(red()))
                           .child(box()
                                      .width(90)
                                      .height(90)
                                      .shape(geometry::shapes::squircle(4))
                                      .fill(green())));
  host.frame();
  EXPECT_EQ(host.pixel(45, 45), SK_ColorRED);     // hexagon body
  EXPECT_EQ(host.pixel(2, 2), SK_ColorBLACK);     // hexagon corner cut
  EXPECT_EQ(host.pixel(135, 45), SK_ColorGREEN);  // squircle body
  EXPECT_EQ(host.pixel(92, 2), SK_ColorBLACK);    // squircle corner soft
  EXPECT_EQ(host.pixel(135, 3), SK_ColorGREEN);   // but edge midpoints full
}

// ---------------------------------------------------------------------------
// Shape VALUES: an outline generator is a comparable scheme, so a shaped
// node can prune. This matters more than it looks — a node whose outline
// cannot compare re-patches on every describe, which throws away its
// recording and its bake, so a single such node in an otherwise static tree
// puts the whole tree back on the live-paint path.

TEST(ComposeShapeValues, AStockGeneratorShapePrunes) {
  // The core case: a shaped node re-described identically must patch nothing
  // and re-record nothing. If structural equality refuses shaped nodes
  // outright, this tree re-records every frame for its entire life and
  // nothing reports it.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape(geometry::shapes::star(5, 0.5f, 0.12f))
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);  // the star is really there

  host.composer.render(tree());  // brand-new Elements, identical values
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeShapeValues, AChangedParameterPatchesAndMovesPixels) {
  // The other half of the prune contract: equality must be HONEST. A
  // different parameter is a different value, patches, and redraws.
  Host host;
  auto tree = [](int sides) {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape(geometry::shapes::polygon(sides))
                           .fill(red()));
  };
  host.composer.render(tree(4));  // diamond: box corners empty
  host.frame();
  EXPECT_EQ(host.pixel(6, 6), SK_ColorBLACK);
  host.composer.render(tree(40));  // ~circle: still empty corners, more ink
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
  host.frame();
  // A 40-gon covers (25, 12); a diamond does not.
  EXPECT_EQ(host.pixel(25, 12), SK_ColorRED);
}

TEST(ComposeShapeValues, ARawCallableIsTheEscapeHatchAndStaysConservative) {
  // A hand-rolled OutlineFn cannot compare, so its node re-patches on every
  // describe. That is the conservative answer and it is deliberate: claiming
  // equality for two callables would prune a node whose outline had in fact
  // changed. An author who needs the prune wraps the node in memo().
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(100)
                           .height(100)
                           .shape([](SkSize s) {
                             SkPathBuilder b;
                             b.addOval(SkRect::MakeWH(s.width(), s.height()));
                             return b.detach();
                           })
                           .fill(red()));
  };
  host.composer.render(tree());
  host.frame();
  host.composer.render(tree());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "an incomparable callable pruned — equality is lying";
}

TEST(ComposeShapeValues, CopiesOfOneShapeCompareEqualEvenWhenRaw) {
  // Two copies of one Shape share state, and shared state IS identity — so
  // a caller who builds a raw callable once and holds it gets a real prune,
  // where one who re-mints an equivalent lambda each describe does not.
  const Shape raw = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  // the copy is what the test compares
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const Shape copy = raw;
  EXPECT_TRUE(raw == copy);
  // But two separate constructions from equivalent lambdas cannot know
  // they agree, and must not claim to.
  const Shape other = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(raw == other);
}

TEST(ComposeShapeValues, WrappersAreComparableWhenTheirInnerIs) {
  // rounded() composes: value in, value out. Wrapping the escape hatch
  // stays the escape hatch.
  EXPECT_TRUE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
              Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)));
  EXPECT_FALSE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
               Shape(geometry::shapes::rounded(geometry::shapes::star(5), 9)));
  EXPECT_FALSE(Shape(geometry::shapes::rounded(geometry::shapes::star(5), 8)) ==
               Shape(geometry::shapes::rounded(geometry::shapes::star(6), 8)));
  auto lambda = [](SkSize s) {
    SkPathBuilder b;
    b.addRect(SkRect::MakeWH(s.width(), s.height()));
    return b.detach();
  };
  EXPECT_FALSE(Shape(geometry::shapes::rounded(lambda, 8)) ==
               Shape(geometry::shapes::rounded(lambda, 8)));
}

TEST(ComposeShapeValues, AnSvgSilhouetteComparesByItsGeometry) {
  // svg() parses its d-string once into an SkPath, and SkPath has
  // structural equality, so an svg() silhouette compares by geometry and
  // prunes where the raw-callable hatch above cannot.
  EXPECT_TRUE(Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")) ==
              Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")));
  EXPECT_FALSE(Shape(geometry::shapes::svg("M0 0L10 0L10 10Z")) ==
               Shape(geometry::shapes::svg("M0 0L10 0L5 10Z")));
}

TEST(ComposeShapeValues, KeyedParametricIsAValueUnkeyedIsNot) {
  auto fig8 = [](float t) { return SkPoint{std::sin(2 * t), std::sin(t)}; };
  // Unkeyed: the callable is the identity and cannot compare.
  EXPECT_FALSE(Shape(geometry::shapes::parametric(fig8, 0, 6.2832f, 720)) ==
               Shape(geometry::shapes::parametric(fig8, 0, 6.2832f, 720)));
  // Keyed: (key, window, samples) is the identity — the author's contract
  // that one key names one curve.
  EXPECT_TRUE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)));
  EXPECT_FALSE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 360)));
  EXPECT_FALSE(
      Shape(geometry::shapes::parametric("fig8", fig8, 0, 6.2832f, 720)) ==
      Shape(geometry::shapes::parametric("orbit", fig8, 0, 6.2832f, 720)));
  // The named families carry their identity in their parameters.
  EXPECT_TRUE(Shape(geometry::shapes::lissajous(3, 2)) ==
              Shape(geometry::shapes::lissajous(3, 2)));
  EXPECT_FALSE(Shape(geometry::shapes::lissajous(3, 2)) ==
               Shape(geometry::shapes::lissajous(5, 4)));
  EXPECT_TRUE(Shape(geometry::shapes::rose(3)) ==
              Shape(geometry::shapes::rose(3)));
  EXPECT_FALSE(Shape(geometry::shapes::spiral(3.0f)) ==
               Shape(geometry::shapes::spiral(4.0f)));
}

TEST(ComposeShapeValues, AGeneratedSilhouetteReDescribedEveryFrameKeepsItsBake) {
  // The steady state the prune exists for: a texture-cached node whose shape
  // is a generator, re-described every frame. If the shape does not compare,
  // the node patches, the patch drops the bake, and the node re-rasterizes
  // every frame — a single such node dominates a frame that is otherwise
  // entirely cached.
  Host host;
  auto tree = [] {
    return box().child(box()
                           .width(120)
                           .height(120)
                           .shape(geometry::shapes::circle())
                           .fill(red())
                           .cache(Cache::Texture));
  };
  host.composer.render(tree());
  host.frame();  // records + bakes once
  for (int i = 0; i < 3; ++i) {
    host.composer.render(tree());
    host.frame();
    EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "the bake was thrown away by an identical re-describe";
  }
}

TEST(ComposeQueries, HitTestHonorsShapeAndRotation) {
  Host host;
  host.composer.render(box()
                           .child(box()
                                      .key("star")
                                      .width(100)
                                      .height(100)
                                      .shape(geometry::shapes::star(5))
                                      .fill(red()))
                           .child(box()
                                      .key("spun")
                                      .width(80)
                                      .height(20)
                                      .inset(60, 140, 60, 40)
                                      .absolute()
                                      .rotate(90.0f)
                                      .fill(green())));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({50, 50}).value_or(""), "star");
  // Between the star's arms: inside the box, outside the silhouette.
  EXPECT_FALSE(host.composer.hitTest({20, 20}).has_value());

  // The 80x20 bar at (60,140) rotated 90° about its center paints as a
  // 20x80 bar centered at (100,150): x∈[90,110], y∈[110,190].
  EXPECT_EQ(host.composer.hitTest({100, 115}).value_or(""), "spun");
  EXPECT_FALSE(host.composer.hitTest({70, 150}).has_value());  // unrotated
                                                               // footprint
}

// ---------------------------------------------------------------------------
// Organic layout schemes (Layouts.h).

TEST(ComposeLayouts, RadialPlacesChildrenOnTheRing) {
  Host host;
  std::vector<Element> dots;
  dots.reserve(4);
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("d" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::Radial{.radiusFraction = 0.8f})
                      .width(200)
                      .height(200)
                      .children(dots)));
  host.frame();
  // Radius 80 from center (100,100), starting up, clockwise quarters.
  auto center = [&](const char* k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("d0").x(), 100, 1);
  EXPECT_NEAR(center("d0").y(), 20, 1);   // top
  EXPECT_NEAR(center("d1").x(), 180, 1);  // right
  EXPECT_NEAR(center("d1").y(), 100, 1);
  EXPECT_NEAR(center("d2").y(), 180, 1);  // bottom
  EXPECT_NEAR(center("d3").x(), 20, 1);   // left
}

TEST(ComposeLayouts, AlongPathFollowsAStarContour) {
  Host host;
  std::vector<Element> beads;
  beads.reserve(10);
  for (int i = 0; i < 10; ++i)
    beads.push_back(
        box().width(6).height(6).fill(green()).key("b" + std::to_string(i)));
  host.composer.render(
      box().child(layout(layouts::AlongPath{.path = geometry::shapes::star(5)})
                      .width(180)
                      .height(180)
                      .children(beads)));
  host.frame();
  // First bead sits on the star's top point (contour start).
  auto b0 = host.composer.bounds("b0");
  ASSERT_TRUE(b0.has_value());
  EXPECT_NEAR(b0->centerX(), 90, 1.5);
  EXPECT_NEAR(b0->centerY(), 0, 1.5);
  // All beads land ON the star outline: distance from center between
  // inner and outer radius.
  for (int i = 0; i < 10; ++i) {
    auto r = host.composer.bounds("b" + std::to_string(i));
    ASSERT_TRUE(r.has_value());
    const float dx = r->centerX() - 90, dy = r->centerY() - 90;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_GE(dist, 0.4f * 90 - 2);
    EXPECT_LE(dist, 90 + 2);
  }
}

TEST(ComposeLayouts, ModularGridSpansAndAutoFlow) {
  // 4×4 modules, gutter 8, container 200×200 → module 44×44. Child 0 spans
  // 2×1 from (0,0); child 1 spans 1×3 from (3,0); children 2..3 auto-flow.
  Host host;
  layouts::ModularGrid grid;
  grid.columns = 4;
  grid.rows = 4;
  grid.gutter = 8;
  grid.spans = {{0, 0, 2, 1}, {3, 0, 1, 3}};
  host.composer.render(box().child(layout(grid)
                                       .width(pct(100))
                                       .grow(1)
                                       .child(box().key("a").fill(red()))
                                       .child(box().key("b").fill(blue()))
                                       .child(box().key("c").fill(green()))
                                       .child(box().key("d").fill(red()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  auto c = host.composer.bounds("c");
  auto d = host.composer.bounds("d");
  ASSERT_TRUE(a && b && c && d);
  EXPECT_NEAR(a->width(), 44 * 2 + 8, 0.01f);  // 2-module span + gutter
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), (44 + 8) * 3, 0.01f);   // 4th column
  EXPECT_NEAR(b->height(), 44 * 3 + 16, 0.01f);  // 3 rows + 2 gutters
  EXPECT_NEAR(c->left(), 0, 0.01f);  // auto-flow starts at (0,0)… of the flow
  EXPECT_NEAR(c->width(), 44, 0.01f);
  EXPECT_NEAR(d->left(), 44 + 8, 0.01f);  // next module across
}

TEST(ComposeLayouts, BaselineGridSnapsBottomsAndBaselines) {
  // Non-text children anchor by BOTTOM: heights 15 & 27 on rhythm 20 land
  // their bottoms on grid lines 20 and 60 (flow 20+27=47 rounds up).
  Host host;
  host.composer.render(box().child(
      layout(layouts::BaselineGrid{.rhythm = 20})
          .width(pct(100))
          .grow(1)
          .child(box().key("a").width(40).height(15).fill(red()))
          .child(box().key("b").width(40).height(27).fill(blue()))));
  host.frame();
  auto a = host.composer.bounds("a");
  auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_NEAR(a->bottom(), 20.0f, 0.01f);
  EXPECT_NEAR(b->bottom(), 60.0f, 0.01f);

  // A text child anchors by its FIRST BASELINE: with the baseline on the
  // 200 grid line, (200 - top) equals the baseline offset — strictly LESS
  // than the child's height (bottom-anchoring would make them equal).
  // Font-metric independent.
  host.composer.render(
      box().child(layout(layouts::BaselineGrid{.rhythm = 200})
                      .width(pct(100))
                      .grow(1)
                      .child(text(u8"Xylograph", styleAt(40)).key("t"))));
  host.frame();
  auto t = host.composer.bounds("t");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(200.0f - t->top(), 10.0f);               // sane baseline
  EXPECT_LT(200.0f - t->top(), t->height() - 0.5f);  // baseline, not bottom
}

TEST(ComposeLayouts, ScatterIsDeterministicAndContained) {
  auto centers = [&](uint32_t seed) {
    Host host;
    std::vector<Element> bits;
    bits.reserve(9);
    for (int i = 0; i < 9; ++i)
      bits.push_back(
          box().width(12).height(12).fill(blue()).key("s" + std::to_string(i)));
    host.composer.render(box().child(layout(layouts::Scatter{.seed = seed})
                                         .width(200)
                                         .height(200)
                                         .children(bits)));
    host.frame();
    std::vector<SkPoint> out;
    for (int i = 0; i < 9; ++i) {
      auto r = host.composer.bounds("s" + std::to_string(i));
      out.push_back({r->centerX(), r->centerY()});
      EXPECT_GE(r->left(), -0.01f);
      EXPECT_GE(r->top(), -0.01f);
      EXPECT_LE(r->right(), 200.01f);
      EXPECT_LE(r->bottom(), 200.01f);
    }
    return out;
  };
  auto a1 = centers(5), a2 = centers(5), b = centers(6);
  EXPECT_EQ(a1, a2);  // same seed → same scatter
  EXPECT_NE(a1, b);   // new seed → new chaos
}

namespace {

/** 4-tile atlas, 8px cells: [red | green] / [blue | yellow]. */
std::shared_ptr<sigil::image::ImageAsset> fourTileAtlas() {
  SkBitmap src;
  src.allocN32Pixels(16, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(8, 0, 8, 8));
  src.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 8, 8, 8));
  src.erase(SK_ColorYELLOW, SkIRect::MakeXYWH(8, 8, 8, 8));
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(src.asImage()));
}

struct ChunkProps {
  std::vector<int> tiles;  // 4x4 tile ids
  int chunkX = 0, chunkY = 0;
  bool operator==(const ChunkProps&) const = default;
};

constexpr float kTilePx = 12.0f;

Element tileChunk(const ChunkProps& p) {
  static auto atlas = fourTileAtlas();
  auto chunk = box().width(4 * kTilePx).height(4 * kTilePx);
  for (int i = 0; i < (int)p.tiles.size(); ++i) {
    const int id = p.tiles[(size_t)i];
    const int atlasRow = id / 2, row = i / 4;
    const float sx = (float)(id % 2) * 8, sy = (float)atlasRow * 8;
    chunk.child(image(atlas)
                    .region(SkRect::MakeXYWH(sx, sy, 8, 8))
                    .absolute()
                    .inset((float)(i % 4) * kTilePx, (float)row * kTilePx, 0, 0)
                    .width(kTilePx)
                    .height(kTilePx));
  }
  return chunk;
}

}  // namespace

TEST(ComposeTiling, OnlyTouchedChunkRerecords) {
  Host host;
  // 2x2 chunks of 4x4 tiles; a checker-ish rule fills the ids.
  std::vector<ChunkProps> chunks(4);
  for (int c = 0; c < 4; ++c) {
    chunks[(size_t)c].chunkX = c % 2;
    chunks[(size_t)c].chunkY = c / 2;
    for (int i = 0; i < 16; ++i) chunks[(size_t)c].tiles.push_back((i + c) % 4);
  }
  auto maze = [&] {
    auto grid = box().row().wrapLines().width(2 * 4 * kTilePx);
    for (int c = 0; c < 4; ++c)
      grid.child(
          memo(chunks[(size_t)c], tileChunk).key("chunk" + std::to_string(c)));
    return box().child(grid);
  };

  host.composer.render(maze());
  host.frame();
  const size_t coldRecords = host.composer.stats().picturesRecorded;
  EXPECT_GE(coldRecords, 4u);  // every chunk baked (plus ancestors)

  // Pixel sanity: chunk 0 tile 0 is id 0 (red); chunk 1 tile 0 is id 1
  // (green) at x = 48.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);

  host.composer.render(maze());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);  // all memo-warm

  chunks[0].tiles[0] = 3;  // mutate ONE tile in ONE chunk
  host.composer.render(maze());
  host.frame();
  // Only chunk 0 and its ancestor chain re-record; the other three
  // chunks' pictures replay untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 3u);
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorYELLOW);  // the mutated tile
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);  // neighbors intact
}

TEST(ComposeLayouts, RadialRadiusAtGivesEachChildItsOwnRing) {
  // `radiusAt` gives each child its own ring radius, so one Radial can draw
  // nested orbits rather than a single circle. The list may be shorter than
  // the child count: the tail falls back to `radiusFraction`, which is what
  // the second half of this case checks.
  Host host;
  std::vector<Element> dots;
  dots.reserve(4);
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("r" + std::to_string(i)));
  host.composer.render(box().child(
      layout(layouts::Radial{.radiusFraction = 0.8f, .radiusAt = {0.4f, 0.8f}})
          .width(200)
          .height(200)
          .children(dots)));
  host.frame();
  auto center = [&](const char* k) {
    auto r = host.composer.bounds(k);
    return SkPoint{r->centerX(), r->centerY()};
  };
  EXPECT_NEAR(center("r0").y(), 60, 1);   // top, INNER ring (0.4 → r=40)
  EXPECT_NEAR(center("r1").x(), 180, 1);  // right, outer (0.8 → r=80)
  EXPECT_NEAR(center("r2").y(), 180, 1);  // bottom, fallback 0.8
  EXPECT_NEAR(center("r3").x(), 20, 1);   // left, fallback 0.8
}

// ---- The auto table, and the cells a child declares for itself -----------

TEST(ComposeLayouts, TableSizesColumnsByTheirContentAndSharesTheSurplus) {
  // Three columns of unequal content in a 300-wide table with no spacing:
  // each column starts at its widest child, and the 300 − 180 left over is
  // shared out IN PROPORTION, so the widest column takes the most of it.
  Host host;
  Table table{.width = 300};
  host.composer.render(
      box().child(layout(table)
                      .width(pct(100))
                      .grow(1)
                      .child(box().key("a").width(30).height(20).cells(0, 0))
                      .child(box().key("b").width(60).height(20).cells(1, 0))
                      .child(box().key("c").width(90).height(20).cells(2, 0))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  const auto c = host.composer.bounds("c");
  ASSERT_TRUE(a && b && c);
  // 30 : 60 : 90 of 180, scaled to 300 — the proportional share.
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), 50, 0.01f);
  EXPECT_NEAR(c->left(), 150, 0.01f);
  // The children keep their measured size; the COLUMN grew, not the child.
  EXPECT_NEAR(a->width(), 30, 0.01f);
}

TEST(ComposeLayouts, TableSpansTopUpColumnsAndRowsDifferently) {
  // The asymmetry that is the browsers' and not a slip: a colspan's
  // deficit is shared across the columns it covers, a rowspan's lands
  // entirely on the LAST row it covers.
  Host host;
  Table table{.columns = 2, .rows = 2, .width = 100};
  host.composer.render(box().child(
      layout(table)
          .width(200)
          .height(200)
          .child(box().key("wide").width(100).height(10).cells(0, 0, 2, 1))
          .child(box().key("tall").width(10).height(100).cells(0, 1, 1, 2))
          .child(box().key("small").width(10).height(10).cells(1, 1))));
  host.frame();
  const auto wide = host.composer.bounds("wide");
  const auto tall = host.composer.bounds("tall");
  const auto small = host.composer.bounds("small");
  ASSERT_TRUE(wide && tall && small);
  // The two columns start at 10 (the tall child) and 10 (the small one),
  // and the 100-wide span tops both up equally — 50 each, which is the
  // whole 100-wide table, so nothing is left over to share.
  EXPECT_NEAR(small->left(), 50, 0.01f);
  // Row 1 holds a 10-tall child and the first row of a 100-tall span; the
  // span's deficit goes to row 2, so row 1 stays at its own content.
  EXPECT_NEAR(small->top(), 10, 0.01f);
  EXPECT_NEAR(tall->top(), 10, 0.01f);
}

TEST(ComposeLayouts, TableFlowsWhatNoChildClaimedAndAlignsInsideTheCell) {
  // A child that says nothing takes the next cell NO declared child
  // claimed — never one already spoken for, which is the failure of a
  // scheme that counts its flow from zero.
  Host host;
  Table table{.columns = 2, .width = 200};
  host.composer.render(
      box().child(layout(table)
                      .width(200)
                      .height(100)
                      .child(box().key("pinned").width(20).height(20).cells(1, 0))
                      .child(box().key("flowed").width(20).height(20))
                      .child(box()
                                 .key("right")
                                 .width(20)
                                 .height(20)
                                 .cells(1, 1)
                                 .cellAlign(Align::End, Align::Start))));
  host.frame();
  const auto pinned = host.composer.bounds("pinned");
  const auto flowed = host.composer.bounds("flowed");
  const auto right = host.composer.bounds("right");
  ASSERT_TRUE(pinned && flowed && right);
  EXPECT_NEAR(flowed->left(), 0, 0.01f) << "cell (1,0) was taken";
  EXPECT_NEAR(flowed->top(), 0, 0.01f);
  EXPECT_GT(pinned->left(), flowed->left());
  // Both are in column 1, which the surplus grew to 100 wide. The pinned
  // child sits at its start; the end-aligned one is flush with the far
  // side of the same cell.
  EXPECT_NEAR(pinned->right(), 120, 0.01f);
  EXPECT_NEAR(right->right(), 200, 0.01f);
  EXPECT_GT(right->top(), 0.0f) << "the second row";
}

TEST(ComposeLayouts, AChildsOwnCellsOutrankAParallelList) {
  // The seam growth, stated as the behaviour it exists for: a child that
  // names its cells is placed there whatever a scheme's parallel list
  // says, so inserting a child cannot shift every placement after it.
  Host host;
  layouts::ModularGrid grid;
  grid.columns = 4;
  grid.rows = 4;
  grid.gutter = 8;
  grid.spans = {{0, 0, 1, 1}, {0, 0, 1, 1}};  // both at the origin
  host.composer.render(
      box().child(layout(grid)
                      .width(pct(100))
                      .grow(1)
                      .child(box().key("listed").fill(red()))
                      .child(box().key("declared").cells(3, 0).fill(blue()))));
  host.frame();
  const auto listed = host.composer.bounds("listed");
  const auto declared = host.composer.bounds("declared");
  ASSERT_TRUE(listed && declared);
  EXPECT_NEAR(listed->left(), 0, 0.01f) << "the list still places a child";
  EXPECT_NEAR(declared->left(), (44 + 8) * 3, 0.01f) << "…and is overruled";
}
