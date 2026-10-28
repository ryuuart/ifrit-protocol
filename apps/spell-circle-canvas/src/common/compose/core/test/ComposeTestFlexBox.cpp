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
#include "support/Rows.h"

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
  void arrange(Arrangement& arrangement) const {
    for (size_t index = 0; index < arrangement.children.size(); ++index)
      arrangement.children[index].place(geometry::path::Rect::of(
          {100.0f * (float)(index % 2), 40.0f * (float)(index / 2)},
          {100, 40}));
  }
};

}  // namespace

// `flexDirection` says which way the main axis runs and from which end;
// `row` and `column` are its shorthand, and the free factories are a box
// with its line said.
TEST(ComposeFlex, ADirectionRunsTheMainAxisFromEitherEnd) {
  struct DirectionRow {
    const char* name;
    FlexDirection direction;
    SkRect first;
    SkRect second;
  };
  // Reversed, the first child stands against the far end and the second
  // before it.
  const DirectionRow directions[] = {
      {"Row", FlexDirection::Row, SkRect::MakeXYWH(0, 0, 50, 50),
       SkRect::MakeXYWH(50, 0, 30, 30)},
      {"RowReverse", FlexDirection::RowReverse,
       SkRect::MakeXYWH(150, 0, 50, 50), SkRect::MakeXYWH(120, 0, 30, 30)},
      {"Column", FlexDirection::Column, SkRect::MakeXYWH(0, 0, 50, 50),
       SkRect::MakeXYWH(0, 50, 30, 30)},
      {"ColumnReverse", FlexDirection::ColumnReverse,
       SkRect::MakeXYWH(0, 50, 50, 50), SkRect::MakeXYWH(0, 20, 30, 30)},
  };
  sigil::compose::test::forEachRow(directions, [](const DirectionRow& row) {
    Host host(200, 200);
    host.composer.render(withTwoChildren(box()
                                             .width(200)
                                             .height(100)
                                             .alignItems(Align::Start)
                                             .flexDirection(row.direction)));
    host.frame();
    EXPECT_EQ(boundsOf(host, "first"), row.first);
    EXPECT_EQ(boundsOf(host, "second"), row.second);
  });
  sigil::compose::test::checkRows({
      {"RowAndColumnAreTheDirectionsShorthand",
       [] {
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
         restated.composer.render(withTwoChildren(
             box().flexDirection(FlexDirection::RowReverse).column()));
         restated.frame();
         EXPECT_EQ(boundsOf(restated, "second"),
                   SkRect::MakeXYWH(0, 50, 30, 30));
       }},
      {"TheRowAndColumnFactoriesAreABoxWithItsLineSet",
       [] {
         // The free factories are the box with the direction stated and the
         // children given in the call, so each describes the same tree the
         // longhand does, a list `each()` made included.
         const Element first = box().key("first").width(50).height(50);
         const Text label = text(u8"label", whiteStyle(12)).key("label");
         const std::vector<Element> tail = each(
             2, [](size_t index) { return box().width(10.0f + (float)index); });
         EXPECT_TRUE(
             sameDescription(row(first, label, tail),
                             box().row().children({first, label, tail})));
         EXPECT_TRUE(
             sameDescription(column(first, label, tail),
                             box().column().children({first, label, tail})));
         EXPECT_TRUE(sameDescription(row(), box().row()));
         EXPECT_TRUE(sameDescription(column(), box().column()));
         EXPECT_FALSE(sameDescription(row(first), column(first)));

         // ...and they lay out as the longhand does.
         Host factory, longhand;
         factory.composer.render(row(box().key("first").width(50).height(50),
                                     box().key("second").width(30).height(30)));
         longhand.composer.render(
             box().row().children({box().key("first").width(50).height(50),
                                   box().key("second").width(30).height(30)}));
         factory.frame();
         longhand.frame();
         EXPECT_EQ(boundsOf(factory, "second"), boundsOf(longhand, "second"));
         EXPECT_EQ(boundsOf(factory, "second").left(), 50.0f);
       }},
  });
}

// `flexWrap` says what becomes of children that overflow the main axis: one
// line that gives room back, a second line below, or lines stacked from the
// far edge.
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
  // One line, and the three give room back.
  const auto oneLine = [&](std::optional<FlexWrap> wrap) {
    const auto [first, third] = thirdOf(wrap);
    EXPECT_EQ(first.top(), 0.0f);
    EXPECT_EQ(third.top(), 0.0f);
    EXPECT_LT(third.width(), 80.0f);
  };
  sigil::compose::test::checkRows({
      {"Unstated", [&] { oneLine(std::nullopt); }},
      {"NoWrap", [&] { oneLine(FlexWrap::NoWrap); }},
      {"WrapStartsASecondLineBelowTheFirst",
       [&] {
         const auto [first, third] = thirdOf(FlexWrap::Wrap);
         EXPECT_EQ(first, SkRect::MakeXYWH(0, 0, 80, 40));
         EXPECT_EQ(third, SkRect::MakeXYWH(0, 40, 80, 40));
       }},
      {"WrapReverseStacksTheLinesFromTheFarEdge",
       [&] {
         const auto [first, third] = thirdOf(FlexWrap::WrapReverse);
         EXPECT_EQ(first, SkRect::MakeXYWH(0, 60, 80, 40));
         EXPECT_EQ(third, SkRect::MakeXYWH(0, 20, 80, 40));
       }},
      {"AWrappingRowPaintsItsThirdChildOnTheSecondLine",
       [] {
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
         EXPECT_EQ(host.pixel(40, 60), SK_ColorBLUE);  // the second line
       }},
  });
}

// `Display::None` removes the node and everything under it from the layout,
// the picture and the hit test, and a scheme hands it no cell; the
// description keeps it, so saying it has a box again puts it back.
TEST(ComposeBox, ANodeWithNoBoxIsNotLaidOutDrawnOrHit) {
  sigil::compose::test::checkRows({
      {"InAFlexLine",
       [] {
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
         // The third child closes the gap, and nothing of the second is
         // anywhere.
         EXPECT_EQ(boundsOf(host, "right").left(), 50.0f);
         EXPECT_TRUE(boundsOf(host, "middle").isEmpty());
         EXPECT_EQ(host.pixel(75, 25), SK_ColorBLUE);
         EXPECT_EQ(host.pixel(125, 25), SK_ColorBLACK);
         EXPECT_EQ(host.composer.hitTest({75, 25}),
                   std::optional<std::string>("right"));

         // The description kept it: said to have a box again, it takes its
         // place.
         host.composer.render(line(Display::Flex));
         host.frame();
         EXPECT_EQ(boundsOf(host, "right").left(), 100.0f);
         EXPECT_EQ(host.pixel(75, 25), SK_ColorGREEN);
         EXPECT_EQ(host.composer.hitTest({55, 5}),
                   std::optional<std::string>("inside"));

         host.composer.render(line(Display::None));
         host.frame();
         EXPECT_EQ(host.pixel(75, 25), SK_ColorBLUE);
       }},
      {"UnderAScheme",
       [] {
         Host host(200, 200);
         host.composer.render(box().children(
             {layout(TwoColumns{})
                  .width(200)
                  .height(200)
                  .children({box().key("a"),
                             box().key("gone").display(Display::None),
                             box().key("b"), box().key("c")})}));
         host.frame();
         EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 100, 40));
         EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(100, 0, 100, 40));
         EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(0, 40, 100, 40));
         EXPECT_TRUE(boundsOf(host, "gone").isEmpty());
       }},
  });
}

// `Display::Contents` removes only the node's own box: its children are
// items of its parent's line, and under a scheme it is still one child.
TEST(ComposeBox, ContentsHandsANodesChildrenToItsParentsLine) {
  sigil::compose::test::checkRows({
      {"InAFlexLine",
       [] {
         Host host(200, 100);
         const auto line = [](Display wrapper) {
           return box().children(
               {box()
                    .row()
                    .width(200)
                    .justifyContent(Justify::SpaceBetween)
                    .children(
                        {box().key("a").width(40).height(40).fill(red()),
                         box().key("wrapper").display(wrapper).children(
                             {box().key("b").width(40).height(40).fill(green()),
                              box().key("c").width(40).height(40).fill(
                                  blue())})})});
         };
         // As a box of its own the wrapper is one item, a column at the far
         // end.
         host.composer.render(line(Display::Flex));
         host.frame();
         EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(160, 0, 40, 40));
         EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(160, 40, 40, 40));

         // With none, its two children are the line's second and third items
         // and share the leftover room with the first.
         host.composer.render(line(Display::Contents));
         host.frame();
         EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 40, 40));
         EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(80, 0, 40, 40));
         EXPECT_EQ(boundsOf(host, "c"), SkRect::MakeXYWH(160, 0, 40, 40));
         EXPECT_EQ(host.pixel(100, 20), SK_ColorGREEN);
         EXPECT_EQ(host.pixel(180, 20), SK_ColorBLUE);
         EXPECT_EQ(host.composer.hitTest({100, 20}),
                   std::optional<std::string>("b"));

         // And back, so the flex engine's own count of such children is
         // exercised in both directions.
         host.composer.render(line(Display::Flex));
         host.frame();
         EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(160, 0, 40, 40));
       }},
      {"UnderAScheme",
       [] {
         // A wrapper with no box of its own is still one child of the scheme:
         // the cell it is handed has no box to take, and what it wraps is laid
         // out by the container's flex flow rather than by the scheme.
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
                           .children(
                               {box().key("wrapped").width(30).height(30)}),
                       box().key("b")})}));
         host.frame();
         EXPECT_EQ(boundsOf(host, "a"), SkRect::MakeXYWH(0, 0, 100, 40));
         // The wrapper counted for the second cell, so the third child has the
         // third…
         EXPECT_EQ(boundsOf(host, "b"), SkRect::MakeXYWH(0, 40, 100, 40));
         // …and what it wraps stands where a column's first item does.
         EXPECT_EQ(boundsOf(host, "wrapped"), SkRect::MakeXYWH(0, 0, 30, 30));
       }},
  });
}

// `boxSizing` says what a stated size measures: the whole box unless
// `ContentBox` is said, which puts the padding outside it.
TEST(ComposeBox, BoxSizingSaysWhatAStatedSizeMeasures) {
  sigil::compose::test::checkRows({
      {"InAFlexLine",
       [] {
         Host host;
         const auto sized = [](std::string key) {
           return box().key(std::move(key)).width(100).height(40).padding(10);
         };
         host.composer.render(
             box()
                 .alignItems(Align::Start)
                 .children(
                     {sized("unstated"),
                      sized("border").boxSizing(BoxSizing::BorderBox),
                      sized("content").boxSizing(BoxSizing::ContentBox)}));
         host.frame();
         // The whole box unless said otherwise; the content alone when it is,
         // with the padding outside it.
         EXPECT_EQ(boundsOf(host, "unstated").width(), 100.0f);
         EXPECT_EQ(boundsOf(host, "unstated").height(), 40.0f);
         EXPECT_EQ(boundsOf(host, "border").width(), 100.0f);
         EXPECT_EQ(boundsOf(host, "content").width(), 120.0f);
         EXPECT_EQ(boundsOf(host, "content").height(), 60.0f);
       }},
      {"InAPositionedSubtree",
       [] {
         // A positioned subtree has no flex engine behind it and reads its
         // rects from the description, so the rule is stated twice and must
         // agree.
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
         EXPECT_EQ(boundsOf(host, "content"),
                   SkRect::MakeXYWH(10, 10, 120, 60));
         EXPECT_TRUE(boundsOf(host, "gone").isEmpty());
         EXPECT_EQ(host.pixel(50, 30), SK_ColorBLACK);
       }},
  });
}
