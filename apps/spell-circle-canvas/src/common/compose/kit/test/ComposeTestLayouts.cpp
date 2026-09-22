// kit/Layouts.h — the organic layout schemes: children on a ring and on
// each child's own ring, along a contour and along the closure the
// selected contour has, snapped to a baseline grid, and jittered
// deterministically inside their box.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Grid.h>

#include <string>
#include <vector>

#include "support/ShapeTestSupport.h"

TEST(ComposeLayouts, RadialPlacesChildrenOnTheRing) {
  Host host;
  std::vector<Element> dots;
  dots.reserve(4);
  for (int i = 0; i < 4; ++i)
    dots.push_back(
        box().width(10).height(10).fill(red()).key("d" + std::to_string(i)));
  host.composer.render(
      box().children({layout(layouts::Radial{.radiusFraction = 0.8f})
                          .width(200)
                          .height(200)
                          .children(dots)}));
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
  host.composer.render(box().children(
      {layout(layouts::AlongPath{.path = geometry::shapes::star(5)})
           .width(180)
           .height(180)
           .children(beads)}));
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

TEST(ComposeLayouts, BaselineGridSnapsBottomsAndBaselines) {
  // Non-text children anchor by BOTTOM: heights 15 & 27 on rhythm 20 land
  // their bottoms on grid lines 20 and 60 (flow 20+27=47 rounds up).
  Host host;
  host.composer.render(box().children(
      {layout(layouts::BaselineGrid{.rhythm = 20})
           .width(pct(100))
           .flexGrow(1)
           .children({box().key("a").width(40).height(15).fill(red())})
           .children({box().key("b").width(40).height(27).fill(blue())})}));
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
  host.composer.render(box().children(
      {layout(layouts::BaselineGrid{.rhythm = 200})
           .width(pct(100))
           .flexGrow(1)
           .children({text(u8"Xylograph", styleAt(40)).key("t")})}));
  host.frame();
  auto t = host.composer.bounds("t");
  ASSERT_TRUE(t.has_value());
  EXPECT_GT(200.0f - t->top(), 10.0f);               // sane baseline
  EXPECT_LT(200.0f - t->top(), t->height() - 0.5f);  // baseline, not bottom
}

TEST(ComposeLayouts, JitteredIsDeterministicAndContained) {
  auto centers = [&](uint32_t seed) {
    Host host;
    std::vector<Element> bits;
    bits.reserve(9);
    for (int i = 0; i < 9; ++i)
      bits.push_back(
          box().width(12).height(12).fill(blue()).key("s" + std::to_string(i)));
    host.composer.render(box().children({layout(layouts::Jittered{.seed = seed})
                                             .width(200)
                                             .height(200)
                                             .children(bits)}));
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
  host.composer.render(box().children(
      {layout(layouts::Radial{.radiusFraction = 0.8f, .radiusAt = {0.4f, 0.8f}})
           .width(200)
           .height(200)
           .children(dots)}));
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

TEST(ComposeLayouts, AlongPathUsesTheSelectedContoursClosure) {
  for (bool firstClosed : {false, true}) {
    SCOPED_TRACE(firstClosed);
    layouts::AlongPath scheme{.path = [firstClosed] {
      SkPathBuilder path;
      path.moveTo(0, 0).lineTo(100, 0);
      if (firstClosed) path.close();
      path.moveTo(0, 50).lineTo(100, 50);
      if (!firstClosed) path.close();
      return path.detach();
    }};
    Arrangement arrangement;
    arrangement.box = SkRect::MakeWH(100, 100);
    arrangement.children.resize(2);
    scheme.arrange(arrangement);
    const auto& placed = arrangement.children;
    EXPECT_FLOAT_EQ(placed[0].rect.centerX(), 0);
    EXPECT_FLOAT_EQ(placed[1].rect.centerX(), 100);
    EXPECT_FLOAT_EQ(placed[1].rect.centerY(), 0);
  }
}

TEST(ComposeLayouts, RadialPlacesByAFactWhenToldTheLane) {
  // Written out of order, with hours missing, under twelve divisions: each
  // numeral stands at its own hour, and a child stating no hour stands at
  // its index.
  Host host;
  auto hour = [](int number) {
    return box()
        .key("h" + std::to_string(number))
        .attribute("hour", number)
        .width(10)
        .height(10)
        .fill(red());
  };
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .operators({layouts::Radial{.lane = "hour", .divisions = 12}})
          .children({hour(9), hour(3)}));
  host.frame();
  auto centre = [&](const char* key) {
    auto rect = host.composer.bounds(key);
    return SkPoint{rect->centerX(), rect->centerY()};
  };
  EXPECT_NEAR(centre("h3").x(), 180, 1);
  EXPECT_NEAR(centre("h3").y(), 100, 1);
  EXPECT_NEAR(centre("h9").x(), 20, 1);
  EXPECT_NEAR(centre("h9").y(), 100, 1);
}

TEST(ComposeLayouts, RadialFacingTurnsEachChildAlongItsRadius) {
  // A tall bar at three o'clock, facing, is turned a quarter clockwise:
  // it paints wide, so the pixel above and below its centre is background
  // and the one beside it is the bar.
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .operators({layouts::Radial{
              .radiusFraction = 0.5f, .startDeg = 0.0f, .facing = true}})
          .children({box().key("bar").width(6).height(40).fill(red())}));
  host.frame();
  auto rect = host.composer.bounds("bar");
  ASSERT_TRUE(rect.has_value());
  EXPECT_NEAR(rect->centerX(), 150, 1);
  EXPECT_NEAR(rect->centerY(), 100, 1);
  EXPECT_EQ(host.pixel(150, 85), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(135, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(165, 100), SK_ColorRED);
}

TEST(ComposeLayouts, JitterNudgesWhatTheOperatorBeforeItPlaced) {
  Host host;
  auto tree = [](bool jittered) {
    std::vector<Operator> list = {layouts::Radial{}};
    if (jittered) list.push_back(layouts::Jitter{.seed = 7, .amount = 12});
    return box().width(200).height(200).operators(list).children(
        {box().key("a").width(10).height(10).fill(red()),
         box().key("b").width(10).height(10).fill(red())});
  };
  host.composer.render(tree(false));
  host.frame();
  const SkRect still = *host.composer.bounds("a");
  host.composer.render(tree(true));
  host.frame();
  const SkRect moved = *host.composer.bounds("a");
  // Moved off the ring, by no more than the amount, and still in the box.
  EXPECT_NE(still, moved);
  EXPECT_LE(std::abs(moved.left() - still.left()), 12.5f);
  EXPECT_LE(std::abs(moved.top() - still.top()), 12.5f);
  EXPECT_GE(moved.left(), 0);
  EXPECT_LE(moved.right(), 200);
  // Deterministic: the same seed lands in the same place again.
  host.composer.render(tree(true));
  host.frame();
  EXPECT_EQ(*host.composer.bounds("a"), moved);
}
