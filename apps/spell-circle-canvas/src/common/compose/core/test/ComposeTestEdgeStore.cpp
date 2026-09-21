// The edge store: the node-to-routes back-index in tree order, what an
// anchor says about which of the two things it is, the free point that
// anchors to nothing and is on the route anyway, and the index clearing
// when the routes unmount.

#include "support/CoreTestSupport.h"

TEST(ComposeEdgeStore, RoutesAtReturnsAnchoredRoutesInTreeOrder) {
  Host host;
  auto describe = [] {
    return box().children(
        {box().key("a").width(30).height(30).absolute().inset(
             {.top = 10, .right = 160, .bottom = 160, .left = 10}),
         box().key("b").width(30).height(30).absolute().inset(
             {.top = 160, .right = 10, .bottom = 10, .left = 160}),
         connector("a", "b").key("edge1"),
         rail({{"a", {0.5f, 0.5f}}, {"b", {0.5f, 0.5f}}}).key("edge2"),
         connector("a", "b")});  // keyless: anchored but unaddressable
  };
  host.composer.render(describe());
  host.frame();
  const std::vector<std::string> atA = host.composer.routesAt("a");
  ASSERT_EQ(atA.size(), 2u);  // the keyless route is omitted
  EXPECT_EQ(atA[0], "edge1");
  EXPECT_EQ(atA[1], "edge2");
  EXPECT_EQ(host.composer.routesAt("b").size(), 2u);
  EXPECT_TRUE(host.composer.routesAt("nowhere").empty());
}

TEST(ComposeRail, AFreePointAnchorsToNothingAndIsStillOnTheRoute) {
  // A route through a PLACE rather than through a thing: the bend that
  // clears a corner is a real waypoint, and standing an invisible box up
  // to carry its coordinates mounts and lays out a node per bend for a
  // number the caller already had.
  Host host;
  host.composer.render(box().children(
      {box().key("a").absolute().rect(SkRect::MakeXYWH(10, 10, 20, 20)),
       box().key("b").absolute().rect(SkRect::MakeXYWH(150, 150, 20, 20)),
       rail({Anchor::on("a"), Anchor::at({20.0f, 160.0f}), Anchor::on("b")})
           .key("elbow")
           .absolute()
           .inset(0)}));
  host.frame();
  // The route turns at the free point: a hit at the elbow lands on the
  // rail, and the straight line between the two nodes does not pass
  // anywhere near it.
  EXPECT_EQ(host.composer.hitTest({20, 160}), "elbow");
  EXPECT_EQ(host.composer.hitTest({20, 100}), "elbow");  // down the first leg
  EXPECT_NE(host.composer.hitTest({90, 90}), "elbow");   // the chord it is not
  // It is still a route AT the nodes it does bind.
  EXPECT_EQ(host.composer.routesAt("a").size(), 1u);
  EXPECT_EQ(host.composer.routesAt("b").size(), 1u);
  // A rail of free points alone binds nothing and still draws.
  host.composer.render(box().children(
      {rail({Anchor::at({10.0f, 10.0f}), Anchor::at({10.0f, 180.0f})})
           .key("free")
           .absolute()
           .inset(0)}));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({10, 100}), "free");
}

TEST(ComposeRail, AnAnchorSaysWhichOfTheTwoThingsItIs) {
  // The discriminator is in the TYPE, so there is no field to fill that
  // the resolver will not read: a bound anchor names a node and carries a
  // normalized point on it; a free one names nothing and carries a point
  // in the rail's own coordinates. Neither can be mistaken for the other.
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

  // And the route the resolver draws follows the kind: the same numbers
  // read as a NORM land on the node, and read as a POINT land where they
  // say. A rail bound to a 20x20 box at (150, 150) with norm {1, 1}
  // arrives at its bottom-right corner, not at (1, 1).
  Host host;
  host.composer.render(box().children(
      {box().key("b").absolute().rect(
           SkRect::MakeXYWH(150.0f, 150.0f, 20.0f, 20.0f)),
       rail({Anchor::at({170.0f, 10.0f}), Anchor::on("b", {1.0f, 1.0f})})
           .key("corner")
           .absolute()
           .inset(0)}));
  host.frame();
  EXPECT_EQ(host.composer.hitTest({170, 100}), "corner");  // the vertical run
  EXPECT_EQ(host.composer.hitTest({170, 168}), "corner");  // down to {170,170}
}

TEST(ComposeEdgeStore, IndexClearsWhenRoutesUnmount) {
  Host host;
  bool withRoute = true;
  auto describe = [&] {
    auto tree = box().children(
        {box().key("a").width(30).height(30).absolute().inset(
             {.top = 10, .right = 160, .bottom = 160, .left = 10}),
         box().key("b").width(30).height(30).absolute().inset(
             {.top = 160, .right = 10, .bottom = 10, .left = 160})});
    if (withRoute) tree.children({connector("a", "b").key("edge")});
    return tree;
  };
  host.composer.render(describe());
  host.frame();
  ASSERT_EQ(host.composer.routesAt("a").size(), 1u);
  withRoute = false;
  host.composer.render(describe());
  host.frame();
  EXPECT_TRUE(host.composer.routesAt("a").empty());
}
