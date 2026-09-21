// The four edges around a node, in each spelling the box and placement
// verbs accept: the shorthands, the named sides of an `Edges`, and the
// per-side verbs that write one side and leave the other three.

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "support/CoreTestSupport.h"

namespace {

SkRect boundsOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key));
}

/** A 200 by 200 host holding @p padded around one keyed child, laid out
 *  once; the child's bounds are where the padding put it. */
SkRect childUnder(Element padded) {
  static Host host{200, 200};
  host.composer.render(
      padded.width(200).height(200).children({box().key("child").flexGrow(1)}));
  host.frame();
  return boundsOf(host, "child");
}

}  // namespace

TEST(ComposeEdges, PaddingTakesOneTwoOrFourLengths) {
  // One length is all four sides; two are one across and one down; four
  // are a length per side, clockwise from the left.
  EXPECT_EQ(childUnder(box().padding(10)), SkRect::MakeXYWH(10, 10, 180, 180));
  EXPECT_EQ(childUnder(box().padding(10, 20)),
            SkRect::MakeXYWH(10, 20, 180, 160));
  EXPECT_EQ(childUnder(box().padding(1, 2, 3, 4)),
            SkRect::MakeXYWH(1, 2, 196, 194));
}

TEST(ComposeEdges, PaddingNamesItsSides) {
  // The same four lengths, each saying which side it is. A designated
  // initialiser follows the declaration order, which is CSS's.
  EXPECT_EQ(childUnder(box().padding({.top = 2, .right = 3, .bottom = 4,
                                      .left = 1})),
            SkRect::MakeXYWH(1, 2, 196, 194));
  // A side left unnamed is zero, not the side beside it.
  EXPECT_EQ(childUnder(box().padding({.left = 12})),
            SkRect::MakeXYWH(12, 0, 188, 200));
}

TEST(ComposeEdges, APerSidePaddingVerbLeavesTheOtherThree) {
  EXPECT_EQ(childUnder(box().padding(5).paddingLeft(40)),
            SkRect::MakeXYWH(40, 5, 155, 190));
  EXPECT_EQ(childUnder(box()
                           .paddingTop(1)
                           .paddingRight(2)
                           .paddingBottom(3)
                           .paddingLeft(4)),
            SkRect::MakeXYWH(4, 1, 194, 196));
}

TEST(ComposeEdges, MarginTakesTheSameThreeSpellings) {
  Host host{200, 200};
  const auto placed = [&host](Element child) {
    host.composer.render(box().width(200).height(200).children(
        {child.key("child").width(50).height(50)}));
    host.frame();
    return boundsOf(host, "child");
  };
  EXPECT_EQ(placed(box().margin(10)), SkRect::MakeXYWH(10, 10, 50, 50));
  EXPECT_EQ(placed(box().margin(10, 20)), SkRect::MakeXYWH(10, 20, 50, 50));
  EXPECT_EQ(placed(box().margin(1, 2, 3, 4)), SkRect::MakeXYWH(1, 2, 50, 50));
  EXPECT_EQ(placed(box().margin({.top = 2, .left = 1})),
            SkRect::MakeXYWH(1, 2, 50, 50));
  EXPECT_EQ(placed(box().marginTop(7).marginLeft(9)),
            SkRect::MakeXYWH(9, 7, 50, 50));
}

TEST(ComposeEdges, InsetTakesOneLengthFourLengthsOrNamedSides) {
  Host host{200, 200};
  const auto placed = [&host](Element child) {
    host.composer.render(
        box().width(200).height(200).children({child.key("child")}));
    host.frame();
    return boundsOf(host, "child");
  };
  EXPECT_EQ(placed(box().inset(10)), SkRect::MakeXYWH(10, 10, 180, 180));
  EXPECT_EQ(placed(box().inset(1, 2, 3, 4)), SkRect::MakeXYWH(1, 2, 196, 194));
  EXPECT_EQ(placed(box().inset({.top = 2, .right = 3, .bottom = 4, .left = 1})),
            SkRect::MakeXYWH(1, 2, 196, 194));
  // A side an inset leaves unnamed is UNPINNED, not zero, so the node's
  // own size stands there rather than stretching to the parent's edge.
  EXPECT_EQ(placed(box().width(50).height(40).inset({.top = 6, .left = 8})),
            SkRect::MakeXYWH(8, 6, 50, 40));
}
