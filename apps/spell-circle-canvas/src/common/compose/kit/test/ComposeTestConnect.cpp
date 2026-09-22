// kit/Connect.h — the connecting operators: a wire between two keyed nodes
// stated in the operator, a wire through a run of stops, a wire per pairing
// the nodes state under a lane, each dressed by what the operator carries
// and silent about a key the scope does not hold.

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

TEST(ComposeConnect, AlongThreadsAWireThroughItsStops) {
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .children({node("a", 20, 20), node("b", 160, 160)})
          .operators({connect::Along{
              .stops = {Anchor::on("a"), Anchor::at({30.0f, 170.0f}),
                        Anchor::on("b")},
              .wire = stroke(4.0f, red())}}));
  host.frame();
  // The wire turns at the free point — down the left, then along the
  // bottom — rather than taking the chord between the two nodes, and it
  // is keyed by the stops it binds.
  EXPECT_TRUE(host.composer.bounds("a->b").has_value());
  EXPECT_EQ(host.pixel(30, 120), SK_ColorRED);     // down the first leg
  EXPECT_EQ(host.pixel(100, 170), SK_ColorRED);    // along the second
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);  // the chord it is not
}

TEST(ComposeConnect, AlongTakesARunOfFreePointsBoundToNothing) {
  // A route through places rather than through things: nothing is
  // resolved on the scope and the wire is drawn anyway.
  Host host;
  host.composer.render(
      box().width(200).height(200).operators(
          {connect::Along{.stops = {Anchor::at({10.0f, 10.0f}),
                                    Anchor::at({10.0f, 180.0f})},
                          .wire = stroke(4.0f, red())}}));
  host.frame();
  EXPECT_EQ(host.pixel(10, 100), SK_ColorRED);
}

TEST(ComposeConnect, AlongHoldsItsEndsClearByTheTerminalStopsGap) {
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .children({node("a", 20, 90), node("b", 160, 90)})
          .operators({connect::Along{
              .stops = {Anchor::on("a", {0.5f, 0.5f}, 30.0f),
                        Anchor::on("b", {0.5f, 0.5f}, 30.0f)},
              .wire = stroke(4.0f, red())}}));
  host.frame();
  // Centre to centre runs 30..170; each end pulled back 30 it runs
  // 60..140, so the pixel just past each box is bare.
  EXPECT_EQ(host.pixel(45, 100), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(155, 100), SK_ColorBLACK);
}

TEST(ComposeConnect, AlongIsSilentAboutAStopTheScopeDoesNotHold) {
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .children({node("a", 20, 90)})
          .operators({connect::Along{
              .stops = {Anchor::on("a"), Anchor::on("nowhere")},
              .wire = stroke(4.0f, red())}}));
  host.frame();
  EXPECT_FALSE(host.composer.bounds("a->nowhere").has_value());
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);
}

TEST(ComposeConnect, AStopSaysWhichOfTheTwoThingsItIs) {
  // The discriminator is in the TYPE, so there is no field to fill that
  // the operator will not read: a bound stop names a node and carries a
  // normalized point on it; a free one names nothing and carries a point
  // in the scope's coordinates. Neither can be mistaken for the other.
  const Anchor bound = Anchor::on("a", {1.0f, 0.5f}, 4.0f);
  const Anchor free = Anchor::at({20.0f, 160.0f});
  EXPECT_EQ(bound.key(), "a");
  EXPECT_TRUE(free.key().empty());
  EXPECT_EQ(std::get<Anchor::OnNode>(bound.where).norm, (SkPoint{1.0f, 0.5f}));
  EXPECT_EQ(std::get<Anchor::FreePoint>(free.where).point,
            (SkPoint{20.0f, 160.0f}));
  EXPECT_EQ(bound.gap, 4.0f);  // the gap belongs to neither half
  EXPECT_NE(bound, free);
  EXPECT_EQ(bound, Anchor("a", {1.0f, 0.5f}, 4.0f));  // the brace spelling

  // And the wire follows the kind: the same numbers read as a NORM land
  // on the node, and read as a POINT land where they say. A stop bound to
  // a 20x20 node at (150, 150) with norm {1, 1} arrives at its
  // bottom-right corner, not at (1, 1).
  Host host;
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .children({node("b", 150, 150)})
          .operators({connect::Along{
              .stops = {Anchor::at({170.0f, 10.0f}),
                        Anchor::on("b", {1.0f, 1.0f})},
              .wire = stroke(4.0f, red())}}));
  host.frame();
  EXPECT_EQ(host.pixel(170, 100), SK_ColorRED);  // the vertical run
  EXPECT_EQ(host.pixel(170, 165), SK_ColorRED);  // down to {170, 170}
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLACK);
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
