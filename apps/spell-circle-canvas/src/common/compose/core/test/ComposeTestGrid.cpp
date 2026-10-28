// The grid: the four track sizing functions, the picture of named areas a
// child claims a region of, the two auto-flows, and the content minima the
// rule floors a content track at.

#include <sigilcompose/core/Grid.h>

#include <array>
#include <limits>
#include <memory>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

namespace {

using sigil::compose::layouts::Grid;
namespace layouts = sigil::compose::layouts;

/** Owned records for direct layout tests; the public arrangement only
 *  borrows them for a call. Non-text minima equal their measured sizes. */
struct Given {
  geometry::path::Rect box;
  std::vector<Arrangement::Child> children;
  operator Arrangement() {
    return {.box = box, .children = children, .minimumSizesMeasured = true};
  }
};

Given given(glm::vec2 container, std::vector<glm::vec2> sizes,
            std::vector<CellSpan> cells = {},
            std::vector<std::string> areas = {},
            std::vector<glm::vec2> minima = {}) {
  Given records{geometry::path::Rect::of({0, 0}, container), {}};
  for (size_t index = 0; index < sizes.size(); ++index) {
    Arrangement::Child child;
    child.size = sizes[index];
    child.minimumSize = minima.empty() ? sizes[index] : minima[index];
    if (index < cells.size()) child.cells = cells[index];
    if (index < areas.size()) child.area = areas[index];
    records.children.push_back(std::move(child));
  }
  return records;
}

SkiaRects placed(const Grid& grid, Given input) {
  Arrangement arrangement = input;
  grid.arrange(arrangement);
  std::vector<geometry::path::Rect> rects;
  for (const auto& child : input.children) rects.push_back(child.rect);
  return rects;
}

CellSpan claims(int column, int row, int columns = 1, int rows = 1) {
  return CellSpan{.column = column,
                  .row = row,
                  .columns = columns,
                  .rows = rows,
                  .declared = true};
}

std::vector<glm::vec2> boxes(size_t count, glm::vec2 size) {
  return std::vector<glm::vec2>(count, size);
}

}  // namespace

// ---------------------------------------------------------------------------
// The track sizing functions

// A track carries a sizing function: a length neither grows nor shrinks,
// a content track is the widest thing in it and no wider, a share divides
// what is left by its weight and freezes at its floor, minmax is one under
// the other, and what is still free stays free. A span tops its tracks up
// in proportion to what they already hold, down as across.
TEST(ComposeGrid, EachTrackSizesByItsOwnSizingFunction) {
  /** Where a child starts along the axis read, and how far it runs. */
  struct Extent {
    float start;
    float length;
  };
  struct TrackRow {
    const char* name;
    Grid grid;
    glm::vec2 container;
    std::vector<glm::vec2> sizes;
    std::vector<CellSpan> cells;
    std::vector<Extent> placed;
    bool down = false;
  };
  const TrackRow rows[] = {
      // 30 px of the container is spoken for by nothing, and stays that way.
      {.name = "AFixedTrackIsItsLengthAndFreeSpaceBesideItStaysFree",
       .grid = Grid{.columns = {layouts::px(100), layouts::px(50),
                                layouts::px(120)}},
       .container = {300, 100},
       .sizes = boxes(3, {10, 10}),
       .placed = {{0, 100}, {100, 50}, {150, 120}}},
      {.name = "AContentTrackIsTheWidestThingInItAndDoesNotStretch",
       .grid = Grid{.columns = {layouts::content(), layouts::content(),
                                layouts::content()},
                    .gap = {10, 0}},
       .container = {300, 50},
       .sizes = {{40, 10}, {60, 10}, {20, 10}},
       .placed = {{0, 40}, {50, 60}, {120, 20}}},
      // One of four shares of 220, and three of them.
      {.name = "SharesDivideWhatIsLeftAfterTheFixedTracks",
       .grid =
           Grid{.columns = {layouts::px(100), layouts::fr(1), layouts::fr(3)}},
       .container = {320, 50},
       .sizes = boxes(3, {10, 10}),
       .placed = {{0, 100}, {100, 55}, {155, 165}}},
      // Half a share, not half the space, and a quarter of it is left over.
      {.name = "WeightsSummingUnderOneTakeOnlyTheirOwnShare",
       .grid = Grid{.columns = {layouts::fr(0.5f), layouts::fr(0.25f)}},
       .container = {200, 50},
       .sizes = boxes(2, {0, 10}),
       .placed = {{0, 100}, {100, 50}}},
      // Without the freeze the two tracks would take 100 each, the first
      // would be pushed back up to its 180 floor, and the row would resolve
      // 80 px wider than the container it was given.
      {.name = "AShareUnderItsFloorFreezesAndTheRestRedivides",
       .grid =
           Grid{.columns = {layouts::minmax(layouts::px(180), layouts::fr(1)),
                            layouts::fr(1)}},
       .container = {200, 50},
       .sizes = boxes(2, {0, 10}),
       .placed = {{0, 180}, {180, 20}}},
      {.name = "MinmaxCeilingHolds",
       .grid =
           Grid{.columns = {layouts::minmax(layouts::px(0), layouts::px(60))}},
       .container = {300, 50},
       .sizes = {{200, 10}},
       .placed = {{0, 60}}},
      {.name = "MinmaxFloorHolds",
       .grid = Grid{.columns = {layouts::minmax(layouts::px(120),
                                                layouts::content())}},
       .container = {300, 50},
       .sizes = {{40, 10}},
       .placed = {{0, 120}}},
      // 140 px were already asked for; the 60 px the span still needs are
      // split 100:40, not down the middle.
      {.name = "AColumnSpanTopsUpItsTracksInProportion",
       .grid = Grid{.columns = {layouts::content(), layouts::content()}},
       .container = {500, 50},
       .sizes = {{100, 10}, {40, 10}, {200, 10}},
       .cells = {claims(0, 0), claims(1, 0), claims(0, 0, 2, 1)},
       .placed = {{0, 100 + 60 * 100.0f / 140.0f},
                  {100 + 60 * 100.0f / 140.0f, 40 + 60 * 40.0f / 140.0f},
                  {0, 200}}},
      // 30 + 50 * 30 / 50 and 20 + 50 * 20 / 50.
      {.name = "ARowSpanTopsUpItsTracksInProportion",
       .grid = Grid{.columns = {layouts::px(50)},
                    .rows = {layouts::content(), layouts::content()}},
       .container = {50, 400},
       .sizes = {{10, 30}, {10, 20}, {10, 100}},
       .cells = {claims(0, 0), claims(0, 1), claims(0, 0, 1, 2)},
       .placed = {{0, 60}, {60, 40}, {0, 100}},
       .down = true},
      {.name = "FourRepeatedSharesDivideTheContainerFourWays",
       .grid = Grid{.columns = layouts::repeatTrack(4, layouts::fr(1))},
       .container = {400, 100},
       .sizes = boxes(4, {10, 10}),
       .placed = {{0, 100}, {100, 100}, {200, 100}, {300, 100}}},
  };
  sigil::compose::test::forEachRow(rows, [](const TrackRow& row) {
    const SkiaRects at =
        placed(row.grid, given(row.container, row.sizes, row.cells));
    ASSERT_EQ(at.size(), row.placed.size());
    for (size_t child = 0; child < at.size(); ++child) {
      const float start = row.down ? at[child].top() : at[child].left();
      const float length = row.down ? at[child].height() : at[child].width();
      EXPECT_NEAR(start, row.placed[child].start, 0.01f) << "child " << child;
      EXPECT_NEAR(length, row.placed[child].length, 0.01f) << "child " << child;
    }
  });
}

// `repeatTrack` is n copies of one track, and a count of none is no tracks.
TEST(ComposeGrid, RepeatTrackIsCopiesOfOneTrack) {
  const std::vector<layouts::Track> four =
      layouts::repeatTrack(4, layouts::fr(1));
  ASSERT_EQ(four.size(), 4u);
  EXPECT_EQ(four[0], layouts::fr(1));
  EXPECT_EQ(four[3], layouts::fr(1));
  EXPECT_TRUE(layouts::repeatTrack(0, layouts::px(10)).empty());
  EXPECT_TRUE(layouts::repeatTrack(-3, layouts::px(10)).empty());
}

// ---------------------------------------------------------------------------
// The picture

// The picture of names sets the grid's counts, and a child claims the
// rectangle its name covers: the rectangle bounding a name that covers none,
// and a free cell for a name the picture does not carry.
TEST(ComposeGrid, AChildClaimsTheRectangleItsNameCoversInThePicture) {
  sigil::compose::test::checkRows({
      {"ANameInThePicturePlacesAChildOnTheRectangleItCovers",
       [] {
         const Grid grid{.columns = {layouts::px(100), layouts::px(100)},
                         .rows = {layouts::px(50), layouts::px(50)},
                         .areas = {"head head", "nav  main"}};
         const SkiaRects at = placed(grid, given({200, 100}, boxes(3, {10, 10}),
                                                 {}, {"head", "nav", "main"}));
         ASSERT_EQ(at.size(), 3u);
         EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 200, 50));
         EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 50, 100, 50));
         EXPECT_EQ(at[2], SkRect::MakeXYWH(100, 50, 100, 50));
       }},
      {"ThePictureAloneSaysHowWideAndHowDeepTheGridIs",
       [] {
         const Grid grid{.areas = {"a a b", "c c b"}};
         const SkiaRects at = placed(
             grid, given({300, 100}, boxes(3, {10, 10}), {}, {"a", "b", "c"}));
         ASSERT_EQ(at.size(), 3u);
         // Three equal columns because no column list was given, two rows
         // because the picture has two lines.
         EXPECT_FLOAT_EQ(at[0].width(), 200);
         EXPECT_FLOAT_EQ(at[1].left(), 200);
         EXPECT_FLOAT_EQ(at[1].width(), 100);
         EXPECT_FLOAT_EQ(at[2].top(), at[0].bottom());
       }},
      {"ANameThatCoversNoRectangleIsPlacedAtTheRectangleBoundingIt",
       [] {
         const Grid grid{
             .columns = {layouts::px(10), layouts::px(10), layouts::px(10)},
             .areas = {"a . a"}};
         const SkiaRects at =
             placed(grid, given({30, 20}, boxes(1, {5, 5}), {}, {"a"}));
         ASSERT_EQ(at.size(), 1u);
         EXPECT_FLOAT_EQ(at[0].left(), 0);
         EXPECT_FLOAT_EQ(at[0].width(), 30);
       }},
      {"ANameThePictureDoesNotCarryFlowsLikeAChildThatSaidNothing",
       [] {
         const Grid grid{.columns = {layouts::px(10), layouts::px(10)},
                         .areas = {"a b"}};
         const SkiaRects at = placed(
             grid, given({20, 20}, boxes(2, {5, 5}), {}, {"nowhere", "b"}));
         ASSERT_EQ(at.size(), 2u);
         EXPECT_FLOAT_EQ(at[1].left(), 10);  // "b" is where the picture says
         EXPECT_FLOAT_EQ(at[0].left(),
                         0);  // and the unknown name took a free cell
       }},
      {"AChildClaimsItsRegionByNameThroughTheComposer",
       [] {
         Host host(400, 200);
         host.composer.render(
             layout(Grid{.columns = {layouts::px(100), layouts::fr(1)},
                         .rows = {layouts::px(40), layouts::fr(1)},
                         .areas = {"head head", "nav  main"}})
                 .absolute()
                 .inset(0)
                 .children({box().key("head").gridArea("head").fill(red()),
                            box().key("nav").gridArea("nav").fill(green()),
                            box().key("main").gridArea("main").fill(blue())}));
         host.frame();
         EXPECT_EQ(require(host.composer.bounds("head")),
                   SkRect::MakeXYWH(0, 0, 400, 40));
         EXPECT_EQ(require(host.composer.bounds("nav")),
                   SkRect::MakeXYWH(0, 40, 100, 160));
         EXPECT_EQ(require(host.composer.bounds("main")),
                   SkRect::MakeXYWH(100, 40, 300, 160));
       }},
  });
}

// ---------------------------------------------------------------------------
// The flow

// A child that claims nothing flows into the next free cell, never back past
// the cursor unless the grid is dense and never onto a claimed cell.
TEST(ComposeGrid, AChildThatClaimsNothingFlowsIntoTheNextFreeCell) {
  sigil::compose::test::checkRows({
      {"SparseFlowNeverLooksBackAndDenseFillsTheHoleBehindIt",
       [] {
         // Three columns, two children two wide and one single: the second wide
         // child cannot start at column two, so it drops to the next row and
         // leaves a one-cell hole at the end of the first.
         const std::vector<glm::vec2> sizes = boxes(3, {5, 5});
         const std::vector<CellSpan> spans = {
             CellSpan{.columns = 2}, CellSpan{.columns = 2}, CellSpan{}};
         const std::vector<layouts::Track> three = {
             layouts::px(10), layouts::px(10), layouts::px(10)};

         const Grid sparse{.columns = three,
                           .rows = {layouts::px(10), layouts::px(10)}};
         const SkiaRects flowed = placed(sparse, given({30, 20}, sizes, spans));
         ASSERT_EQ(flowed.size(), 3u);
         EXPECT_EQ(flowed[1], SkRect::MakeXYWH(0, 10, 20, 10));
         EXPECT_EQ(flowed[2], SkRect::MakeXYWH(20, 10, 10, 10));

         const Grid dense{.columns = three,
                          .rows = {layouts::px(10), layouts::px(10)},
                          .dense = true};
         const SkiaRects packed = placed(dense, given({30, 20}, sizes, spans));
         ASSERT_EQ(packed.size(), 3u);
         EXPECT_EQ(packed[1], SkRect::MakeXYWH(0, 10, 20, 10));
         EXPECT_EQ(packed[2],
                   SkRect::MakeXYWH(20, 0, 10, 10));  // back into the hole
       }},
      {"AFlowingChildNeverLandsOnACellSomethingElseClaimed",
       [] {
         const Grid grid{.columns = {layouts::px(10), layouts::px(10)},
                         .rows = {layouts::px(10), layouts::px(10)}};
         const SkiaRects at = placed(grid, given({20, 20}, boxes(2, {5, 5}),
                                                 {CellSpan{}, claims(0, 0)}));
         ASSERT_EQ(at.size(), 2u);
         EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 0, 10, 10));
         EXPECT_EQ(at[0], SkRect::MakeXYWH(10, 0, 10, 10));
       }},
      {"AChildWiderThanTheGridTakesTheWholeOfIt",
       [] {
         // A span no arrangement of the grid could hold: there is no cell three
         // columns wide in a three-column grid at which a five-wide child is
         // free, so the search for one is the grid it has, not a search without
         // an end.
         const Grid grid{
             .columns = {layouts::px(10), layouts::px(10), layouts::px(10)},
             .rows = {layouts::px(10), layouts::px(10)}};
         std::vector<CellSpan> spans(2);
         spans[0].columns = 5;
         const SkiaRects at =
             placed(grid, given({30, 20}, boxes(2, {5, 5}), spans));
         ASSERT_EQ(at.size(), 2u);
         EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 30, 10));
         // And the one behind it flows onto the row below, the whole first row
         // being spoken for.
         EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 10, 10, 10));
       }},
      {"EqualModulesRespectSpansAndFlowIntoFreeCells",
       [] {
         // 4×4 modules, gutter 8, container 200×200 → module 44×44. Child 0
         // spans 2×1 from (0,0); child 1 spans 1×3 from (3,0); children 2..3
         // auto-flow.
         Host host;
         const layouts::Grid grid{
             .columns = layouts::repeatTrack(4, layouts::fr()),
             .rows = layouts::repeatTrack(4, layouts::fr()),
             .gap = {8, 8}};
         host.composer.render(box().children(
             {layout(grid)
                  .width(percent(100))
                  .flexGrow(1)
                  .children(
                      {box().key("wide").gridCells(0, 0, 2, 1).fill(red())})
                  .children(
                      {box().key("tall").gridCells(3, 0, 1, 3).fill(blue())})
                  .children({box().key("third").fill(green())})
                  .children({box().key("fourth").fill(red())})}));
         host.frame();
         auto wide = host.composer.bounds("wide");
         auto tall = host.composer.bounds("tall");
         auto third = host.composer.bounds("third");
         auto fourth = host.composer.bounds("fourth");
         ASSERT_TRUE(wide && tall && third && fourth);
         EXPECT_NEAR(wide->width(), 44 * 2 + 8,
                     0.01f);  // 2-module span + gutter
         EXPECT_NEAR(wide->left(), 0, 0.01f);
         EXPECT_NEAR(tall->left(), (44 + 8) * 3, 0.01f);   // 4th column
         EXPECT_NEAR(tall->height(), 44 * 3 + 16, 0.01f);  // 3 rows + 2 gutters
         EXPECT_NEAR(third->left(), 104,
                     0.01f);  // first free cell: (2, 0)
         EXPECT_NEAR(third->top(), 0, 0.01f);
         EXPECT_NEAR(third->width(), 44, 0.01f);
         EXPECT_NEAR(fourth->left(), 0, 0.01f);  // next free cell: (0, 1)
         EXPECT_NEAR(fourth->top(), 52, 0.01f);
       }},
  });
}

// ---------------------------------------------------------------------------
// Alignment

TEST(ComposeGrid, TheGridsOwnAlignmentHoldsUntilAChildStatesItsOwn) {
  std::vector<CellSpan> spans(2);
  spans[1].across = Align::Center;
  spans[1].down = Align::End;
  spans[1].alignDeclared = true;
  const Grid grid{.columns = {layouts::px(40), layouts::px(40)},
                  .rows = {layouts::px(40)}};
  const SkiaRects at = placed(grid, given({80, 40}, boxes(2, {10, 10}), spans));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 40, 40));  // stretched, the default
  EXPECT_EQ(at[1], SkRect::MakeXYWH(55, 30, 10, 10));
}

// Solving hands back the tracks and places nothing; placing writes each
// borrowed record's rect and leaves its measurements and claims standing.
TEST(ComposeGrid, SolvingAndPlacingWriteOnlyTheBorrowedRecordsRect) {
  std::array<Arrangement::Child, 1> children;
  auto& child = children[0];
  child.size = {80, 20};
  child.minimumSize = {40, 20};
  child.cells = claims(0, 0);
  child.area = "main";
  child.attributes.set("tier", 2);
  child.statedSize = {
      .width = {.kind = StatedLength::Kind::Fraction, .value = 0.5f},
      .aspectRatio = 2};
  Arrangement arrangement{.box = geometry::path::Rect::of({0, 0}, {200, 100}),
                          .children = children,
                          .minimumSizesMeasured = true,
                          .percentagesResolved = true};
  const Grid grid{.columns = {layouts::px(90), layouts::px(90)},
                  .areas = {"main main"},
                  .gap = {20, 5}};
  EXPECT_EQ(arrangement.children.data(), children.data());
  const auto tracks = grid.solve(arrangement);
  EXPECT_EQ(child.rect, geometry::path::Rect{});
  ASSERT_EQ(tracks.rowHeights.size(), 1u);
  EXPECT_FLOAT_EQ(tracks.rowHeights[0], 50);
  grid.arrange(arrangement);
  EXPECT_EQ(child.rect, geometry::path::Rect::of({0, 0}, {100, 50}));
  EXPECT_EQ(child.size, glm::vec2(80, 20));
  EXPECT_EQ(child.minimumSize, glm::vec2(40, 20));
  EXPECT_EQ(child.cells, claims(0, 0));
  EXPECT_EQ(child.area, "main");
  EXPECT_EQ(child.attribute<int>("tier"), 2);
}

// ---------------------------------------------------------------------------
// The content minima, through the composer

namespace {

/** A scheme that keeps what it was handed, so a case can read the input the
 *  composer built rather than the rects a scheme made of it. */
struct Recorder {
  struct Seen {
    std::vector<Arrangement::Child> children;
    bool minimumSizesMeasured = false;
  };
  std::shared_ptr<Seen> seen = std::make_shared<Seen>();
  bool operator==(const Recorder& other) const { return seen == other.seen; }
  static constexpr bool readsChildMinimumSizes = true;
  void arrange(Arrangement& arrangement) const {
    seen->children.assign(arrangement.children.begin(),
                          arrangement.children.end());
    seen->minimumSizesMeasured = arrangement.minimumSizesMeasured;
    for (size_t index = 0; index < arrangement.children.size(); ++index) {
      auto& child = arrangement.children[index];
      child.place(geometry::path::Rect::of({0, (float)index * 20}, child.size));
    }
  }
};

}  // namespace

// A scheme that declares it reads child minimum sizes is told how narrow its
// text children can go; one that does not is told nothing.
TEST(ComposeGrid, OnlyASchemeThatDeclaresItIsToldItsChildrensMinimumSizes) {
  sigil::compose::test::checkRows({
      {"ASchemeThatAsksIsToldHowNarrowItsTextChildrenCanGo",
       [] {
         Host host(300, 200);
         const Recorder recorder;
         host.composer.render(box().absolute().inset(0).children(
             {layout(recorder)
                  .absolute()
                  .left(0.0f)
                  .top(0.0f)
                  .width(200.0f)
                  .children({text(u8"one two three four five six seven eight",
                                  styleAt(12))
                                 .width(200.0f)})}));
         host.frame();
         ASSERT_EQ(recorder.seen->children.size(), 1u);
         EXPECT_TRUE(recorder.seen->minimumSizesMeasured);
         // The narrowest the paragraph goes is one word, and it was measured at
         // two hundred — so the minimum is real and it is smaller.
         EXPECT_GT(recorder.seen->children[0].minimumSize.x, 0.0f);
         EXPECT_LT(recorder.seen->children[0].minimumSize.x,
                   recorder.seen->children[0].size.x);
       }},
      {"ASchemeThatDoesNotAskIsToldNothing",
       [] {
         Host host(300, 200);
         struct Quiet {
           bool operator==(const Quiet&) const = default;
           void arrange(Arrangement& arrangement) const {
             for (auto& child : arrangement.children)
               child.place(geometry::path::Rect::of({0, 0}, {10, 10}));
           }
         };
         // The concept, not the runtime: a scheme without the declaration is
         // not one the composer measures minima for.
         static_assert(!sigil::compose::ReadsChildMinimumSizes<Quiet>);
         static_assert(sigil::compose::ReadsChildMinimumSizes<Grid>);
       }},
  });
}

// ---------------------------------------------------------------------------
// Through the composer

// A grid's gap is a length read where the grid lays out: pixels, the font,
// a share of the box, or the custom properties in force.
TEST(ComposeGrid, AGapIsALengthReadWhereTheGridLaysOut) {
  sigil::compose::test::checkRows({
      {"ACustomPropertyInForce",
       [] {
         // The gutter is the sheet's: a grid whose gap names a custom property
         // lays out at whatever the sheet in force states, and lays out again
         // when the sheet states another.
         Host host(400, 100);
         const auto under = [](float gutter) {
           return box()
               .absolute()
               .inset(0)
               .applyStyleSheet(StyleSheet{
                   rule(":root").var("spacing-cell-gap", Dimension(gutter))})
               .children(
                   {layout(Grid{.columns = {layouts::px(40), layouts::px(40)},
                                .rows = {layouts::px(40), layouts::px(40)},
                                .gap = layouts::Gap::both(
                                    var("spacing-cell-gap"))})
                        .key("grid")
                        .absolute()
                        .inset(0)
                        .children({box().key("first").fill(red()),
                                   box().key("second").fill(green()),
                                   box().key("third").fill(blue())})});
         };
         host.composer.render(under(10));
         host.frame();
         EXPECT_EQ(require(host.composer.bounds("second")),
                   SkRect::MakeXYWH(50, 0, 40, 40));
         EXPECT_EQ(require(host.composer.bounds("third")),
                   SkRect::MakeXYWH(0, 50, 40, 40));
         host.composer.render(under(30));
         host.frame();
         EXPECT_EQ(require(host.composer.bounds("second")),
                   SkRect::MakeXYWH(70, 0, 40, 40));
         EXPECT_EQ(require(host.composer.bounds("third")),
                   SkRect::MakeXYWH(0, 70, 40, 40));
       }},
      {"TheFontAndAShareOfTheBox",
       [] {
         // An em is the grid node's own font size; a percentage is of the
         // grid's extent along the gap's own axis; a bare number is pixels.
         Host host(400, 200);
         host.composer.render(
             layout(Grid{.columns = {layouts::px(40), layouts::px(40)},
                         .rows = {layouts::px(40), layouts::px(40)},
                         .gap = {Dimension(sigil::weave::em(2)), percent(10)}})
                 .font({.size = 10})
                 .absolute()
                 .inset(0)
                 .children({box().key("first"), box().key("second"),
                            box().key("third")}));
         host.frame();
         EXPECT_EQ(require(host.composer.bounds("second")),
                   SkRect::MakeXYWH(60, 0, 40, 40));
         EXPECT_EQ(require(host.composer.bounds("third")),
                   SkRect::MakeXYWH(0, 60, 40, 40));
       }},
  });
}

// A grid whose extent is left open takes the extent it placed, held within
// its own and its parent's stated minimum and maximum.
TEST(ComposeGrid, AnOpenGridTakesTheExtentItPlacedWithinItsBounds) {
  sigil::compose::test::checkRows({
      {"AGridInAColumnTakesItsHeightFromWhatItPlaced",
       [] {
         // Every child of a scheme is placed absolutely, so nothing under the
         // container contributes to its height and flex would collapse it to
         // nothing. The extent it placed is the height instead.
         Host host(300, 300);
         host.composer.render(box().column().absolute().inset(0).children(
             {layout(Grid{.columns = {layouts::fr(1), layouts::fr(1)},
                          .rows = {layouts::px(60)}})
                  .key("grid")
                  .children({box().fill(red())})
                  .children({box().fill(green())})}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 60);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).width(), 300);
       }},
      {"AStretchedGridContributesToItsContentSizedRow",
       [] {
         Host host(500, 600);
         const auto tree = [](float siblingHeight, float figureHeight) {
           return box().column().gap(20).children(
               {box().key("row").row().gap(20).children(
                    {layout(Grid{.columns = {layouts::fr()},
                                 .rows = {layouts::px(figureHeight)}})
                         .key("grid")
                         .width(300)
                         .children({box().key("figure").fill(red())}),
                     box().width(100).height(siblingHeight)}),
                box().key("following").height(20)});
         };
         for (const auto [siblingHeight, figureHeight, expected] :
              {std::array{80.0f, 240.0f, 240.0f},
               std::array{320.0f, 240.0f, 320.0f},
               std::array{80.0f, 120.0f, 120.0f}}) {
           host.composer.render(tree(siblingHeight, figureHeight));
           host.frame();
           EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(),
                           figureHeight);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                           expected + 20);
         }
       }},
      {"AStretchedGridKeepsItsParentsExplicitHeight",
       [] {
         Host host(500, 400);
         host.composer.render(box().column().gap(20).children(
             {box().row().height(80).children(
                  {layout(Grid{.columns = {layouts::fr()},
                               .rows = {layouts::px(240)}})
                       .key("grid")
                       .width(300)
                       .children({box().key("figure").fill(red())})}),
              box().key("following").height(20)}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 80);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(), 240);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 100);
       }},
      {"AStretchedGridsNaturalExtentRespectsItsOwnMaximum",
       [] {
         Host host(500, 600);
         const auto tree = [](float siblingHeight) {
           return box().column().gap(20).children(
               {box().key("row").row().gap(20).children(
                    {layout(Grid{.columns = {layouts::fr()},
                                 .rows = {layouts::px(240)}})
                         .key("grid")
                         .width(300)
                         .maxHeight(100)
                         .children({box().key("figure").fill(red())}),
                     box().width(100).height(siblingHeight)}),
                box().key("following").height(20)});
         };
         for (const float siblingHeight : {80.0f, 320.0f, 80.0f}) {
           host.composer.render(tree(siblingHeight));
           host.frame();
           const float rowHeight = std::max(100.0f, siblingHeight);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 100);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(),
                           rowHeight);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(),
                           240);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                           rowHeight + 20);
         }
       }},
      {"AMaximumBoundsAContentSizedRowWithoutFixingItsHeight",
       [] {
         Host host(500, 600);
         for (const float maximum : {500.0f, 100.0f, 500.0f}) {
           host.composer.render(box().column().gap(20).children(
               {box().key("row").row().maxHeight(maximum).children(
                    {layout(Grid{.columns = {layouts::fr()},
                                 .rows = {layouts::px(240)}})
                         .key("grid")
                         .width(300)
                         .children({box().key("figure").fill(red())}),
                     box().width(100).height(80)}),
                box().key("following").height(20)}));
           host.frame();
           const float expected = std::min(maximum, 240.0f);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(),
                           240);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                           expected + 20);
         }
       }},
      {"AContentSizedRowsMinimumCanStretchAShorterGrid",
       [] {
         Host host(500, 600);
         host.composer.render(box().column().gap(20).children(
             {box().key("row").row().minHeight(200).maxHeight(500).children(
                  {layout(Grid{.columns = {layouts::fr()},
                               .rows = {layouts::px(120)}})
                       .key("grid")
                       .width(300)
                       .children({box().key("figure").fill(red())}),
                   box().width(100).height(80)}),
              box().key("following").height(20)}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 200);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), 200);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(), 120);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 220);
       }},
      {"AParentsMaximumLeavesRoomForItsPaddingAndTheGridMargin",
       [] {
         Host host(500, 600);
         host.composer.render(box().column().gap(20).children(
             {box().key("row").row().maxHeight(100).padding(10, 0).children(
                  {layout(Grid{.columns = {layouts::fr()},
                               .rows = {layouts::px(240)}})
                       .key("grid")
                       .width(300)
                       .margin(5, 0)
                       .children({box().key("figure").fill(red())}),
                   box().width(100).height(60)}),
              box().key("following").height(20)}));
         host.frame();
         const SkRect grid = require(host.composer.bounds("grid"));
         EXPECT_FLOAT_EQ(grid.top(), 15);
         EXPECT_FLOAT_EQ(grid.height(), 70);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), 100);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 120);
       }},
      {"AMaximumBoundsAContentSizedColumnWithoutFixingItsWidth",
       [] {
         Host host(600, 500);
         for (const float maximum : {500.0f, 100.0f, 500.0f}) {
           host.composer.render(
               box()
                   .row()
                   .alignItems(Align::Start)
                   .gap(20)
                   .children(
                       {box().key("column").column().maxWidth(maximum).children(
                            {layout(Grid{.columns = {layouts::px(240)},
                                         .rows = {layouts::fr()}})
                                 .key("grid")
                                 .height(300)
                                 .children({box().key("figure").fill(red())}),
                             box().width(80).height(100)}),
                        box().key("following").width(20).height(20)}));
           host.frame();
           const float expected = std::min(maximum, 240.0f);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).width(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("column")).width(),
                           expected);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).width(),
                           240);
           EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).left(),
                           expected + 20);
         }
       }},
      {"AMaximumPassesThroughNestedStretchedRows",
       [] {
         Host host(500, 600);
         host.composer.render(box().column().gap(20).children(
             {box().key("outer").row().maxHeight(100).padding(10, 0).children(
                  {box().key("inner").row().padding(5, 0).children(
                      {layout(Grid{.columns = {layouts::fr()},
                                   .rows = {layouts::px(240)}})
                           .key("grid")
                           .width(300)
                           .children({box().fill(red())}),
                       box().width(100).height(20)})}),
              box().key("following").height(20)}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 70);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("inner")).height(), 80);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("outer")).height(), 100);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 120);
       }},
  });
}

// Text is measured again at the width the grid places it at, within its own
// stated bounds, before the content-sized rows settle.
TEST(ComposeGrid, TextIsMeasuredAtItsPlacedWidthBeforeContentRowsSettle) {
  sigil::compose::test::checkRows({
      {"TextWrapsAtItsResolvedTrackWidth",
       [] {
         Host host(500, 600);
         const auto paragraph = [] {
           return text(
               u8"One measured paragraph should wrap within the track that "
               "holds it and leave the following row below all of its lines.",
               styleAt(16));
         };
         host.composer.render(box().column().gap(20).children(
             {layout(Grid{.columns = {layouts::fr(), layouts::fr()},
                          .rows = {layouts::content(), layouts::content()},
                          .gap = {20, 12}})
                  .key("grid")
                  .row()
                  .alignItems(Align::Start)
                  .children({paragraph()
                                 .key("paragraph")
                                 .flexShrink(0)
                                 .gridCells(0, 0),
                             box().height(18).gridCells(1, 0),
                             box().key("nextRow").height(20).gridCells(0, 1)}),
              box().key("following").height(20)}));
         float previousHeight = 0;
         for (const float viewport : {500.0f, 300.0f}) {
           host.composer.setSize({viewport, 600});
           host.frame();
           const SkRect textBounds = require(host.composer.bounds("paragraph"));
           const float measure = (viewport - 20) / 2;
           EXPECT_FLOAT_EQ(textBounds.width(), measure);
           EXPECT_GT(textBounds.height(), previousHeight);
           EXPECT_GE(require(host.composer.bounds("nextRow")).top(),
                     textBounds.bottom() + 12);
           EXPECT_GE(require(host.composer.bounds("following")).top(),
                     require(host.composer.bounds("grid")).bottom() + 20);
           Host reference(500, 600);
           reference.composer.render(
               box().children({paragraph().key("reference").width(measure)}));
           reference.frame();
           EXPECT_NEAR(textBounds.height(),
                       require(reference.composer.bounds("reference")).height(),
                       0.25f);
           previousHeight = textBounds.height();
         }
       }},
      {"TextWrapsAtItsOwnStatedBoundsWithinTheTrack",
       [] {
         const auto paragraph = [] {
           return text(
               "One paragraph wraps at its own bounds even when the grid track "
               "offers a different width. Every line belongs above the next "
               "row.",
               styleAt(16));
         };
         for (const bool maximum : {true, false}) {
           const float trackWidth = maximum ? 240 : 80;
           const float expectedWidth = maximum ? 100 : 140;
           Element bounded =
               paragraph().key("paragraph").flexShrink(0).gridCells(0, 0);
           if (maximum)
             bounded.maxWidth(expectedWidth);
           else
             bounded.minWidth(expectedWidth);
           Host host(500, 600);
           host.composer.render(
               layout(Grid{.columns = {layouts::px(trackWidth)},
                           .rows = {layouts::content(), layouts::content()},
                           .gap = {0, 12}})
                   .row()
                   .alignItems(Align::Start)
                   .children(
                       {std::move(bounded),
                        box().key("following").height(20).gridCells(0, 1)}));
           host.frame();
           Host reference(500, 600);
           reference.composer.render(box().children(
               {paragraph().key("reference").width(expectedWidth)}));
           reference.frame();
           const SkRect actual = require(host.composer.bounds("paragraph"));
           EXPECT_FLOAT_EQ(actual.width(), expectedWidth);
           EXPECT_NEAR(actual.height(),
                       require(reference.composer.bounds("reference")).height(),
                       0.25f);
           EXPECT_NEAR(require(host.composer.bounds("following")).top(),
                       actual.bottom() + 12, 0.25f);
         }
       }},
      {"ACustomSchemeSeesTheReflowedTextAndMayLeaveItUnplaced",
       [] {
         struct ReflowSensitive {
           std::shared_ptr<bool> shortened = std::make_shared<bool>(false);
           void arrange(Arrangement& arrangement) const {
             auto& child = arrangement.children.front();
             if (child.size.y > 40) {
               *shortened = true;
               return;
             }
             child.place(geometry::path::Rect::of({0, 0}, {60, child.size.y}));
           }
         };
         Host host(500, 400);
         const ReflowSensitive scheme;
         host.composer.render(
             layout(scheme)
                 .row()
                 .alignItems(Align::Start)
                 .children({text(u8"A sentence becomes several lines at the "
                                 u8"proposed width.",
                                 styleAt(16))
                                .flexShrink(0)}));
         host.frame();
         EXPECT_TRUE(*scheme.shortened);
       }},
      {"WrappedTextKeepsItsStatedMinimumAndMaximumHeight",
       [] {
         Host host(400, 400);
         host.composer.render(
             layout(Grid{.columns = {layouts::px(160), layouts::px(160)},
                         .rows = {layouts::content(), layouts::content()},
                         .gap = {20, 12}})
                 .row()
                 .alignItems(Align::Start)
                 .children(
                     {text("Short", styleAt(16))
                          .key("minimum")
                          .minHeight(90)
                          .gridCells(0, 0),
                      text("A long paragraph whose many words would need more "
                           "than "
                           "one line when placed within a narrow track.",
                           styleAt(16))
                          .key("maximum")
                          .maxHeight(30)
                          .gridCells(1, 0),
                      box().key("following").height(20).gridCells(0, 1)}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("minimum")).height(), 90);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("maximum")).height(), 30);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 102);
       }},
  });
}

// A grid item's percentages resolve against the cell it is given, so a
// picture bounded at a percentage of its box paints inside its cell.
TEST(ComposeGrid, AGridItemsPercentagesAreOfTheCellItIsGiven) {
  sigil::compose::test::checkRows({
      {"AnImageLeafAsACellPaintsInsideTheCellItWasGiven",
       [] {
         // An image leaf keeps its proportions by bounding itself at a
         // percentage of the box it stands in. Every child of a grid leaves the
         // flow once the grid places it, so a grid whose height its children
         // gave it has no height of its own on the next pass unless it is sized
         // from what it placed at once, and a percentage of nothing bounds the
         // leaf to nothing.
         SkBitmap solid;
         solid.allocN32Pixels(16, 16);
         solid.eraseColor(SK_ColorGREEN);
         const auto picture = sigil::media::Image::of(solid.asImage());
         const auto leaf = [&] {
           return image(sigil::media::PixelSource(picture))
               .width(80)
               .height(80);
         };
         const auto inside = [](Host& host, const SkRect& cell) {
           return host.pixel((int)cell.centerX(), (int)cell.centerY());
         };
         // Two columns of 195 with a gap of 10: the picture in the first cell,
         // a box in the second, and a picture alone on the second row.
         const Grid grid{.columns = {layouts::fr(), layouts::fr()},
                         .gap = {10, 10}};
         Host host(400, 400);
         host.composer.render(box().column().children(
             {layout(grid)
                  .key("grid")
                  .row()
                  .alignItems(Align::Start)
                  .children({leaf().key("picture")})
                  .children({box().key("box").width(80).height(80).fill(red())})
                  .children({box().key("cell").column().children(
                      {leaf().key("framed"),
                       box().key("label").width(80).height(20).fill(
                           blue())})})}));
         host.frame();
         const SkRect cell = require(host.composer.bounds("picture"));
         EXPECT_EQ(cell, SkRect::MakeXYWH(0, 0, 195, 80));
         EXPECT_EQ(inside(host, cell), SK_ColorGREEN);
         // The box beside it stands in its own cell.
         EXPECT_EQ(require(host.composer.bounds("box")),
                   SkRect::MakeXYWH(205, 0, 195, 80));
         EXPECT_EQ(host.pixel(300, 40), SK_ColorRED);
         // The same leaf in a column in a cell: the picture above, its label
         // below it.
         const SkRect framed = require(host.composer.bounds("framed"));
         EXPECT_EQ(framed, SkRect::MakeXYWH(0, 90, 80, 80));
         EXPECT_EQ(inside(host, framed), SK_ColorGREEN);
         EXPECT_EQ(require(host.composer.bounds("label")).top(), 170);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 190);
       }},
      {"APercentageIsOfTheCellNotTheGrid",
       [] {
         // As CSS resolves a grid item's percentages against its grid area: a
         // picture bounded at a hundred percent of its box is as wide as its
         // cell, and its proportions give the row its height; a box half as
         // wide as its box is half its cell.
         SkBitmap solid;
         solid.allocN32Pixels(16, 16);
         solid.eraseColor(SK_ColorGREEN);
         const auto picture = sigil::media::Image::of(solid.asImage());
         // Two columns of 195 with a gap of 10.
         const Grid grid{.columns = {layouts::fr(), layouts::fr()},
                         .gap = {10, 10}};
         Host host(400, 400);
         host.composer.render(box().column().children(
             {layout(grid)
                  .key("grid")
                  .children({image(sigil::media::PixelSource(picture))
                                 .key("picture")})
                  .children({box()
                                 .key("half")
                                 .width(percent(50))
                                 .height(20)
                                 .gridCellAlign(Align::Start, Align::Start)
                                 .fill(red())})}));
         host.frame();
         EXPECT_EQ(require(host.composer.bounds("picture")),
                   SkRect::MakeXYWH(0, 0, 195, 195));
         EXPECT_EQ(host.pixel(190, 190), SK_ColorGREEN);
         // Half of 195, on the whole pixel the layout rounds it to.
         const SkRect half = require(host.composer.bounds("half"));
         EXPECT_EQ(half.left(), 205);
         EXPECT_NEAR(half.width(), 97.5f, 0.5f);
         EXPECT_EQ(half.height(), 20);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 195);
       }},
  });
}
