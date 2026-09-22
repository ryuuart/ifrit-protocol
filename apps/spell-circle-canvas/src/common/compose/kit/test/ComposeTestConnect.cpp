// kit/Connect.h — the connecting operators: a wire between two keyed nodes
// stated in the operator, a wire per pairing the nodes state under a lane,
// both routed as a connector routes and dressed by what the operator
// carries, and silent about a key the scope does not hold.

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Routers.h>

#include <string>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace {

Element node(const char* key, float x, float y) {
  return box().key(key).left(x).top(y).width(20).height(20).fill(green());
}

}  // namespace

TEST(ComposeConnect, BetweenWiresTwoKeyedNodesAndDressesTheWire) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("a", 20, 90), node("b", 160, 90)})
          .operators({connect::Between{.from = "a",
                                       .to = "b",
                                       .router = routers::straight(),
                                       .wire = stroke(4.0f, red())}}));
  host.frame();
  auto wire = host.composer.bounds("a->b");
  ASSERT_TRUE(wire.has_value());
  EXPECT_NEAR(wire->centerY(), 100, 1);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
}

TEST(ComposeConnect, ByLaneWiresEveryPairingTheNodesState) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("hub", 90, 90),
                     node("a", 20, 20).attribute("feeds", "hub"),
                     node("b", 160, 160)
                         .attribute("feeds", std::vector<std::string>{
                                                 "hub", "a", "nowhere"})})
          .operators({connect::ByLane{.lane = "feeds",
                                      .wire = stroke(3.0f, red())}}));
  host.frame();
  EXPECT_TRUE(host.composer.bounds("a->hub").has_value());
  EXPECT_TRUE(host.composer.bounds("b->hub").has_value());
  EXPECT_TRUE(host.composer.bounds("b->a").has_value());
  EXPECT_FALSE(host.composer.bounds("b->nowhere").has_value());  // silent
  // The diagonal from a to hub passes through (65, 65).
  EXPECT_EQ(host.pixel(65, 65), SK_ColorRED);
}

TEST(ComposeConnect, TheGapHoldsTheWireClearOfItsEnds) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("a", 20, 90), node("b", 160, 90)})
          .operators({connect::Between{.from = "a",
                                       .to = "b",
                                       .gap = 30,
                                       .wire = stroke(4.0f, red())}}));
  host.frame();
  // Centre to centre runs 30..170; pulled back 30 at each end it runs
  // 60..140, so the pixel just past each box is bare.
  EXPECT_EQ(host.pixel(45, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(155, 100), SK_ColorBLACK);
}

TEST(ComposeConnect, TheOperatorsZIndexPutsTheWiresBehindTheNodes) {
  Host host;
  host.composer.render(
      box().width(200).height(200)
          .children({node("a", 20, 90), node("b", 160, 90)})
          .operators({Operator(connect::Between{.from = "a",
                                                .to = "b",
                                                .wire = stroke(30.0f, red())})
                          .zIndex(-1)}));
  host.frame();
  EXPECT_EQ(host.pixel(30, 100), SK_ColorGREEN);  // the node, over the wire
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
}
