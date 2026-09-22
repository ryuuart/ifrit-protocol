// kit/Pin.h, kit/Outline.h, kit/Stamp.h — the stock adding operators: an
// element hung off a node where its request says and at the first
// fallback that fits, a band along one node's outline attached to it, the
// hull of a set of nodes attached to the scope, and one element made per
// node stating a lane.

#include <sigilcompose/kit/Outline.h>
#include <sigilcompose/kit/Pin.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <string>

#include "support/ShapeTestSupport.h"

namespace {

Element node(const char* key, float x, float y) {
  return box().key(key).left(x).top(y).width(20).height(20).fill(green());
}

}  // namespace

TEST(ComposeAdders, PinHangsTheElementWhereTheRequestSaysAndFallsBack) {
  Host host;
  const pin::Request request{
      .element = box().fill(red()),
      .size = {40, 20},
      .where = {.on = {1, 0.5f},
                .at = {0, 0.5f},
                .offset = {10, 0},
                .fallbacks = {{.on = {0, 0.5f}, .at = {1, 0.5f}, .offset = {-10, 0}}}}};
  host.composer.render(box().width(200).height(200)
                           .children({node("a", 20, 90).attribute("label", request),
                                      node("b", 170, 90).attribute("label", request)})
                           .operators({pin::ByLane{.lane = "label"}}));
  host.frame();
  // a's callout hangs to its right: 40..90 wide, level with it.
  auto a = host.composer.bounds("a-pin");
  ASSERT_TRUE(a.has_value());
  EXPECT_NEAR(a->left(), 50, 0.5f);
  EXPECT_NEAR(a->centerY(), 100, 0.5f);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED);
  // b's would leave the box to the right, so it takes the fallback and
  // hangs to the left: its right edge 10 px short of b.
  auto b = host.composer.bounds("b-pin");
  ASSERT_TRUE(b.has_value());
  EXPECT_NEAR(b->right(), 160, 0.5f);
  EXPECT_EQ(host.pixel(140, 100), SK_ColorRED);
}

TEST(ComposeAdders, AroundAttachesABandAlongTheNodesOutline) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({box().key("dial").left(50).top(50).width(100).height(100)
                         .shape(geometry::shapes::circle())})
          .operators({outline::Around{
              .key = "dial",
              .across = across(10),
              .formation = geometry::path::Formation::Outer,
              .fill = red()}}));
  host.frame();
  ASSERT_TRUE(host.composer.bounds("dial-outline").has_value());
  EXPECT_EQ(host.pixel(100, 45), SK_ColorRED);    // 55 from the centre: on the band
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // inside the dial, unfilled
  EXPECT_EQ(host.pixel(100, 30), SK_ColorBLACK);   // past the band
}

TEST(ComposeAdders, HullEnclosesTheClassedNodesAndNoOther) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("a", 20, 20).styleClass("chosen"),
                     node("b", 150, 20).styleClass("chosen"),
                     node("c", 80, 150).styleClass("chosen"),
                     node("d", 170, 170)})
          .operators({Operator(outline::Hull{.styleClass = "chosen", .fill = red()})
                          .zIndex(-1)}));
  host.frame();
  auto hull = host.composer.bounds("hull:chosen");
  ASSERT_TRUE(hull.has_value());
  EXPECT_NEAR(hull->left(), 20, 1.5f);
  EXPECT_NEAR(hull->right(), 170, 1.5f);
  EXPECT_NEAR(hull->bottom(), 170, 1.5f);
  EXPECT_EQ(host.pixel(85, 60), SK_ColorRED);      // inside the triangle
  EXPECT_EQ(host.pixel(180, 180), SK_ColorGREEN);  // d, outside it
  EXPECT_EQ(host.pixel(10, 190), SK_ColorBLACK);
}

TEST(ComposeAdders, StampMakesOneElementPerNodeAttachedToIt) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("a", 20, 20).attribute("n", 1),
                     node("b", 150, 150).attribute("n", 2), node("c", 90, 90)})
          .operators({stamp::ByLane{
              .lane = "n",
              .key = "dots",
              .make = [](const Scope::Node& at) {
                return box().width(6).height(6).fill(red()).centerAt(
                    {at.bounds.width() / 2, at.bounds.height() / 2});
              }}}));
  host.frame();
  auto a = host.composer.bounds("a-stamp");
  ASSERT_TRUE(a.has_value());
  EXPECT_NEAR(a->centerX(), 30, 0.5f);
  EXPECT_NEAR(a->centerY(), 30, 0.5f);
  EXPECT_TRUE(host.composer.bounds("b-stamp").has_value());
  EXPECT_FALSE(host.composer.bounds("c-stamp").has_value());
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);
}
