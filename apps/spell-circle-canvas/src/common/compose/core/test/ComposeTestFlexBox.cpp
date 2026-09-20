// The flex container and the box, where CSS has a keyword for them: which
// way the main axis runs and from which end, what becomes of children that
// overflow it, whether a node has a box at all, and what its stated size
// measures.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

/** Two keyed boxes, 50 and 30 along both axes, under @p container. */
Element withTwoChildren(Element container) {
  return box().children(
      {container.children({box().key("first").width(50).height(50),
                           box().key("second").width(30).height(30)})});
}

SkRect boundsOf(Host& host, std::string_view key) {
  return require(host.composer.bounds(key));
}

/** A scheme of two columns of 100 by 40 cells, filled in reading order by
 *  however many children it is handed. */
struct TwoColumns {
  std::vector<SkRect> place(const LayoutInput& input) const {
    std::vector<SkRect> cells;
    for (size_t index = 0; index < input.childSizes.size(); ++index)
      cells.push_back(SkRect::MakeXYWH(100.0f * (float)(index % 2),
                                       40.0f * (float)(index / 2), 100, 40));
    return cells;
  }
};

}  // namespace

TEST(ComposeFlex, ADirectionRunsTheMainAxisFromEitherEnd) {
  const auto laidOut = [](FlexDirection direction) {
    auto host = std::make_unique<Host>(200, 200);
    host->composer.render(withTwoChildren(box()
                                              .width(200)
                                              .height(100)
                                              .alignItems(Align::Start)
                                              .flexDirection(direction)));
    host->frame();
    return host;
  };
  const auto row = laidOut(FlexDirection::Row);
  EXPECT_EQ(boundsOf(*row, "first"), SkRect::MakeXYWH(0, 0, 50, 50));
  EXPECT_EQ(boundsOf(*row, "second"), SkRect::MakeXYWH(50, 0, 30, 30));

  // Reversed, the first child stands against the far end and the second
  // before it.
  const auto rowReverse = laidOut(FlexDirection::RowReverse);
  EXPECT_EQ(boundsOf(*rowReverse, "first"), SkRect::MakeXYWH(150, 0, 50, 50));
  EXPECT_EQ(boundsOf(*rowReverse, "second"), SkRect::MakeXYWH(120, 0, 30, 30));

  const auto column = laidOut(FlexDirection::Column);
  EXPECT_EQ(boundsOf(*column, "first"), SkRect::MakeXYWH(0, 0, 50, 50));
  EXPECT_EQ(boundsOf(*column, "second"), SkRect::MakeXYWH(0, 50, 30, 30));

  const auto columnReverse = laidOut(FlexDirection::ColumnReverse);
  EXPECT_EQ(boundsOf(*columnReverse, "first"), SkRect::MakeXYWH(0, 50, 50, 50));
  EXPECT_EQ(boundsOf(*columnReverse, "second"),
            SkRect::MakeXYWH(0, 20, 30, 30));
}

TEST(ComposeFlex, RowAndColumnAreTheDirectionsShorthand) {
  Host shorthand, longhand;
  shorthand.composer.render(withTwoChildren(box().row()));
  longhand.composer.render(
      withTwoChildren(box().flexDirection(FlexDirection::Row)));
  shorthand.frame();
  longhand.frame();
  EXPECT_EQ(boundsOf(shorthand, "second"), boundsOf(longhand, "second"));
  EXPECT_EQ(boundsOf(shorthand, "second").left(), 50.0f);

  // The later word wins, whichever spelling said it.
  Host restated;
  restated.composer.render(
      withTwoChildren(box().flexDirection(FlexDirection::RowReverse).column()));
  restated.frame();
  EXPECT_EQ(boundsOf(restated, "second"), SkRect::MakeXYWH(0, 50, 30, 30));
}

TEST(ComposeFlex, AWrappingRowFlowsOntoASecondLine) {
  Host host;
  host.composer.render(box().children(
      {box()
           .row()
           .flexWrap()
           .width(200)
           .children({box().width(80).height(40).fill(red())})
           .children({box().width(80).height(40).fill(green())})
           .children({box().width(80).height(40).fill(blue())})}));
  host.frame();
  EXPECT_EQ(host.pixel(40, 20), SK_ColorRED);
  EXPECT_EQ(host.pixel(120, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(40, 60), SK_ColorBLUE);  // wrapped to the next line
}

TEST(ComposeFlex, AWrapSaysWhichWayTheLinesStack) {
  const auto thirdOf = [](std::optional<FlexWrap> wrap) {
    Host host;
    Element line = box().row().width(200).height(100).alignItems(Align::Start);
    if (wrap) line.flexWrap(*wrap);
    host.composer.render(box().children(
        {line.children({box().key("first").width(80).height(40),
                        box().key("second").width(80).height(40),
                        box().key("third").width(80).height(40)})}));
    host.frame();
    return std::pair{boundsOf(host, "first"), boundsOf(host, "third")};
  };
  // Unstated, and said outright: one line, and the three give room back.
  for (const auto& [first, third] :
       {thirdOf(std::nullopt), thirdOf(FlexWrap::NoWrap)}) {
    EXPECT_EQ(first.top(), 0.0f);
    EXPECT_EQ(third.top(), 0.0f);
    EXPECT_LT(third.width(), 80.0f);
  }
  // Wrapped, the third starts a second line below the first…
  {
    const auto [first, third] = thirdOf(FlexWrap::Wrap);
    EXPECT_EQ(first, SkRect::MakeXYWH(0, 0, 80, 40));
    EXPECT_EQ(third, SkRect::MakeXYWH(0, 40, 80, 40));
  }
  // …and reversed, the lines stack from the far edge of the cross axis.
  {
    const auto [first, third] = thirdOf(FlexWrap::WrapReverse);
    EXPECT_EQ(first, SkRect::MakeXYWH(0, 60, 80, 40));
    EXPECT_EQ(third, SkRect::MakeXYWH(0, 20, 80, 40));
  }
}

TEST(ComposeBox, ANodeWithNoBoxIsNotLaidOutDrawnOrHit) {
  Host host(200, 100);
  const auto line = [](Display middle) {
    return box().row().children(
        {box().key("left").width(50).height(50).fill(red()),
         box()
             .key("middle")
             .width(50)
             .height(50)
             .fill(green())
             .display(middle)
             .children({box().key("inside").width(10).height(10)}),
         box().key("right").width(50).height(50).fill(blue())});
  };
  host.composer.render(line(Display::None));
  host.frame();
  // The third child closes the gap, and nothing of the second is anywhere.
  EXPECT_EQ(boundsOf(host, "right").left(), 50.0f);
  EXPECT_TRUE(boundsOf(host, "middle").isEmpty());
  EXPECT_EQ(host.pixel(75, 25), SK_ColorBLUE);
  EXPECT_EQ(host.pixel(125, 25), SK_ColorBLACK);
  EXPECT_EQ(host.composer.hitTest({75, 25}),
            std::optional<std::string>("right"));

  // The description kept it: said to have a box again, it takes its place.
  host.composer.render(line(Display::Flex));
  host.frame();
  EXPECT_EQ(boundsOf(host, "right").left(), 100.0f);
  EXPECT_EQ(host.pixel(75, 25), SK_ColorGREEN);
  EXPECT_EQ(host.composer.hitTest({55, 5}),
            std::optional<std::string>("inside"));

  host.composer.render(line(Display::None));
  host.frame();
  EXPECT_EQ(host.pixel(75, 25), SK_ColorBLUE);
}

TEST(ComposeBox, ANodeWithNoBoxTakesNoCellOfAScheme) {
  Host host(200, 200);
  host.composer.render(box().children(
      {layout(TwoColumns{})
           .width(200)
           .height(200)
           .children({box().key("a"), box().key("gone").display(Display::None),
                      box().key("b"), box().key("c")})}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 100, 40));
  EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(100, 0, 100, 40));
  EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(0, 40, 100, 40));
  EXPECT_TRUE(boundsOf(host, "gone").isEmpty());
}

TEST(ComposeBox, ASchemePlacesItsDirectChildrenAndNothingDeeper) {
  // A wrapper with no box of its own is still one child of the scheme: the
  // cell it is handed has no box to take, and what it wraps is laid out by
  // the container's flex flow rather than by the scheme.
  Host host(200, 200);
  host.composer.render(box().children(
      {layout(TwoColumns{})
           .width(200)
           .height(200)
           .children(
               {box().key("a"),
                box()
                    .key("wrapper")
                    .display(Display::Contents)
                    .children({box().key("wrapped").width(30).height(30)}),
                box().key("b")})}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 100, 40));
  // The wrapper counted for the second cell, so the third child has the
  // third…
  EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(0, 40, 100, 40));
  // …and what it wraps stands where a column's first item does.
  EXPECT_EQ(boundsOf(host, "wrapped"), SkRect::MakeXYWH(0, 0, 30, 30));
}

TEST(ComposeBox, ContentsHandsANodesChildrenToItsParentsLine) {
  Host host(200, 100);
  const auto line = [](Display wrapper) {
    return box().children(
        {box()
             .row()
             .width(200)
             .justify(Justify::SpaceBetween)
             .children(
                 {box().key("a").width(40).height(40).fill(red()),
                  box().key("wrapper").display(wrapper).children(
                      {box().key("b").width(40).height(40).fill(green()),
                       box().key("c").width(40).height(40).fill(blue())})})});
  };
  // As a box of its own the wrapper is one item, a column at the far end.
  host.composer.render(line(Display::Flex));
  host.frame();
  EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(160, 0, 40, 40));
  EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(160, 40, 40, 40));

  // With none, its two children are the line's second and third items and
  // share the leftover room with the first.
  host.composer.render(line(Display::Contents));
  host.frame();
  EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 40, 40));
  EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(80, 0, 40, 40));
  EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(160, 0, 40, 40));
  EXPECT_EQ(host.pixel(100, 20), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(180, 20), SK_ColorBLUE);
  EXPECT_EQ(host.composer.hitTest({100, 20}), std::optional<std::string>("b"));

  // And back, so the flex engine's own count of such children is exercised
  // in both directions.
  host.composer.render(line(Display::Flex));
  host.frame();
  EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(160, 0, 40, 40));
}

TEST(ComposeBox, BoxSizingSaysWhatAStatedSizeMeasures) {
  Host host;
  const auto sized = [](std::string key) {
    return box().key(std::move(key)).width(100).height(40).padding(10);
  };
  host.composer.render(
      box()
          .alignItems(Align::Start)
          .children({sized("unstated"),
                     sized("border").boxSizing(BoxSizing::BorderBox),
                     sized("content").boxSizing(BoxSizing::ContentBox)}));
  host.frame();
  // The whole box unless said otherwise; the content alone when it is, with
  // the padding outside it.
  EXPECT_EQ(boundsOf(host, "unstated").width(), 100.0f);
  EXPECT_EQ(boundsOf(host, "unstated").height(), 40.0f);
  EXPECT_EQ(boundsOf(host, "border").width(), 100.0f);
  EXPECT_EQ(boundsOf(host, "content").width(), 120.0f);
  EXPECT_EQ(boundsOf(host, "content").height(), 60.0f);
}

TEST(ComposeBox, APositionedChildIsSizedByTheSameRule) {
  // A positioned subtree has no flex engine behind it and reads its rects
  // from the description, so the rule is stated twice and must agree.
  Host host;
  host.composer.render(
      positioned().children({box()
                                 .key("content")
                                 .left(10)
                                 .top(10)
                                 .width(100)
                                 .height(40)
                                 .padding(10)
                                 .boxSizing(BoxSizing::ContentBox),
                             box()
                                 .key("gone")
                                 .left(10)
                                 .top(10)
                                 .width(100)
                                 .height(40)
                                 .fill(red())
                                 .display(Display::None)}));
  host.frame();
  EXPECT_EQ(boundsOf(host, "content"), SkRect::MakeXYWH(10, 10, 120, 60));
  EXPECT_TRUE(boundsOf(host, "gone").isEmpty());
  EXPECT_EQ(host.pixel(50, 30), SK_ColorBLACK);
}
