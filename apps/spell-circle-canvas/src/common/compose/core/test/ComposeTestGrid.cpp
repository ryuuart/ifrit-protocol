// The grid: the four track sizing functions, the picture of named areas a
// child claims a region of, the two auto-flows, and the content minima the
// rule floors a content track at.

#include <sigilcompose/core/Grid.h>

#include <array>
#include <limits>
#include <memory>

#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::layouts::Grid;
namespace layouts = sigil::compose::layouts;

/** What a scheme is handed, built by hand: the container, the children's
 *  measured sizes, and the cells they claimed. `childMinSizes` defaults to
 *  the measured sizes, which is what the composer fills in for every child
 *  that is not text. */
LayoutInput given(SkSize container, std::vector<SkSize> sizes,
                  std::vector<CellSpan> cells = {},
                  std::vector<std::string> areas = {},
                  std::vector<SkSize> minima = {}) {
  LayoutInput in;
  in.container = container;
  in.childSizes = std::move(sizes);
  in.childBaselines.assign(in.childSizes.size(),
                           std::numeric_limits<float>::quiet_NaN());
  in.childCells = std::move(cells);
  in.childCells.resize(in.childSizes.size());
  in.childAreas = std::move(areas);
  in.childAreas.resize(in.childSizes.size());
  in.childMinSizes = minima.empty() ? in.childSizes : std::move(minima);
  return in;
}

CellSpan claims(int column, int row, int columns = 1, int rows = 1) {
  return CellSpan{.column = column,
                  .row = row,
                  .columns = columns,
                  .rows = rows,
                  .declared = true};
}

std::vector<SkSize> boxes(size_t count, SkSize size) {
  return std::vector<SkSize>(count, size);
}

}  // namespace

// ---------------------------------------------------------------------------
// The track sizing functions

TEST(ComposeGrid, AFixedTrackIsItsLengthAndFreeSpaceBesideItStaysFree) {
  const Grid grid{
      .columns = {layouts::px(100), layouts::px(50), layouts::px(120)}};
  const std::vector<SkRect> at =
      grid.place(given({300, 100}, boxes(3, {10, 10})));
  ASSERT_EQ(at.size(), 3u);
  EXPECT_FLOAT_EQ(at[0].left(), 0);
  EXPECT_FLOAT_EQ(at[0].width(), 100);
  EXPECT_FLOAT_EQ(at[1].left(), 100);
  EXPECT_FLOAT_EQ(at[1].width(), 50);
  EXPECT_FLOAT_EQ(at[2].left(), 150);
  EXPECT_FLOAT_EQ(at[2].width(), 120);
  // 30 px of the container is spoken for by nothing, and stays that way.
  EXPECT_FLOAT_EQ(at[2].right(), 270);
}

TEST(ComposeGrid, AContentTrackIsTheWidestThingInItAndDoesNotStretch) {
  const Grid grid{
      .columns = {layouts::content(), layouts::content(), layouts::content()},
      .gap = {10, 0}};
  const std::vector<SkRect> at =
      grid.place(given({300, 50}, {{40, 10}, {60, 10}, {20, 10}}));
  ASSERT_EQ(at.size(), 3u);
  EXPECT_FLOAT_EQ(at[0].width(), 40);
  EXPECT_FLOAT_EQ(at[1].width(), 60);
  EXPECT_FLOAT_EQ(at[2].width(), 20);
  EXPECT_FLOAT_EQ(at[1].left(), 50);   // 40 + the gap
  EXPECT_FLOAT_EQ(at[2].left(), 120);  // + 60 + the gap
}

TEST(ComposeGrid, SharesDivideWhatIsLeftAfterTheFixedTracks) {
  const Grid grid{
      .columns = {layouts::px(100), layouts::fr(1), layouts::fr(3)}};
  const std::vector<SkRect> at =
      grid.place(given({320, 50}, boxes(3, {10, 10})));
  ASSERT_EQ(at.size(), 3u);
  EXPECT_FLOAT_EQ(at[0].width(), 100);
  EXPECT_FLOAT_EQ(at[1].width(), 55);   // one of four shares of 220
  EXPECT_FLOAT_EQ(at[2].width(), 165);  // three of them
  EXPECT_FLOAT_EQ(at[2].right(), 320);
}

TEST(ComposeGrid, WeightsSummingUnderOneTakeOnlyTheirOwnShareOfTheFreeSpace) {
  const Grid grid{.columns = {layouts::fr(0.5f), layouts::fr(0.25f)}};
  const std::vector<SkRect> at =
      grid.place(given({200, 50}, boxes(2, {0, 10})));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_FLOAT_EQ(at[0].width(), 100);  // half a share, not half the space
  EXPECT_FLOAT_EQ(at[1].width(), 50);
  EXPECT_FLOAT_EQ(at[1].right(), 150);  // and a quarter of it is left over
}

TEST(ComposeGrid, AShareThatWouldFallUnderItsFloorFreezesAndTheRestRedivides) {
  // Without the freeze the two tracks would take 100 each, the first would
  // be pushed back up to its 180 floor, and the row would resolve 80 px
  // wider than the container it was given.
  const Grid grid{.columns = {layouts::minmax(layouts::px(180), layouts::fr(1)),
                              layouts::fr(1)}};
  const std::vector<SkRect> at =
      grid.place(given({200, 50}, boxes(2, {0, 10})));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_FLOAT_EQ(at[0].width(), 180);
  EXPECT_FLOAT_EQ(at[1].width(), 20);
  EXPECT_FLOAT_EQ(at[1].right(), 200);
}

TEST(ComposeGrid, MinmaxIsAFloorAndACeilingAndBothHold) {
  const Grid ceilinged{
      .columns = {layouts::minmax(layouts::px(0), layouts::px(60))}};
  const std::vector<SkRect> clamped =
      ceilinged.place(given({300, 50}, {{200, 10}}));
  ASSERT_EQ(clamped.size(), 1u);
  EXPECT_FLOAT_EQ(clamped[0].width(), 60);

  const Grid floored{
      .columns = {layouts::minmax(layouts::px(120), layouts::content())}};
  const std::vector<SkRect> held = floored.place(given({300, 50}, {{40, 10}}));
  ASSERT_EQ(held.size(), 1u);
  EXPECT_FLOAT_EQ(held[0].width(), 120);
}

TEST(ComposeGrid, ASpanTopsUpItsTracksInProportionToWhatTheyAlreadyHold) {
  const Grid grid{.columns = {layouts::content(), layouts::content()}};
  const std::vector<SkRect> at =
      grid.place(given({500, 50}, {{100, 10}, {40, 10}, {200, 10}},
                       {claims(0, 0), claims(1, 0), claims(0, 0, 2, 1)}));
  ASSERT_EQ(at.size(), 3u);
  // 140 px were already asked for; the 60 px the span still needs are split
  // 100:40, not down the middle.
  EXPECT_NEAR(at[0].width(), 100 + 60 * 100.0f / 140.0f, 0.01f);
  EXPECT_NEAR(at[1].width(), 40 + 60 * 40.0f / 140.0f, 0.01f);
  EXPECT_NEAR(at[2].width(), 200, 0.01f);
}

TEST(ComposeGrid, ARowSpanSharesItsDeficitAcrossItsRowsJustAsAColumnSpanDoes) {
  const Grid grid{.columns = {layouts::px(50)},
                  .rows = {layouts::content(), layouts::content()}};
  const std::vector<SkRect> at =
      grid.place(given({50, 400}, {{10, 30}, {10, 20}, {10, 100}},
                       {claims(0, 0), claims(0, 1), claims(0, 0, 1, 2)}));
  ASSERT_EQ(at.size(), 3u);
  EXPECT_NEAR(at[0].height(), 60, 0.01f);  // 30 + 50·30/50
  EXPECT_NEAR(at[1].top(), 60, 0.01f);
  EXPECT_NEAR(at[1].height(), 40, 0.01f);  // 20 + 50·20/50
  EXPECT_NEAR(at[2].height(), 100, 0.01f);
}

// ---------------------------------------------------------------------------
// The picture

TEST(ComposeGrid, ANameInThePicturePlacesAChildOnTheRectangleItCovers) {
  const Grid grid{.columns = {layouts::px(100), layouts::px(100)},
                  .rows = {layouts::px(50), layouts::px(50)},
                  .areas = {"head head", "nav  main"}};
  const std::vector<SkRect> at = grid.place(
      given({200, 100}, boxes(3, {10, 10}), {}, {"head", "nav", "main"}));
  ASSERT_EQ(at.size(), 3u);
  EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 200, 50));
  EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 50, 100, 50));
  EXPECT_EQ(at[2], SkRect::MakeXYWH(100, 50, 100, 50));
}

TEST(ComposeGrid, ThePictureAloneSaysHowWideAndHowDeepTheGridIs) {
  const Grid grid{.areas = {"a a b", "c c b"}};
  const std::vector<SkRect> at =
      grid.place(given({300, 100}, boxes(3, {10, 10}), {}, {"a", "b", "c"}));
  ASSERT_EQ(at.size(), 3u);
  // Three equal columns because no column list was given, two rows because
  // the picture has two lines.
  EXPECT_FLOAT_EQ(at[0].width(), 200);
  EXPECT_FLOAT_EQ(at[1].left(), 200);
  EXPECT_FLOAT_EQ(at[1].width(), 100);
  EXPECT_FLOAT_EQ(at[2].top(), at[0].bottom());
}

TEST(ComposeGrid, ANameThatCoversNoRectangleIsPlacedAtTheRectangleBoundingIt) {
  const Grid grid{
      .columns = {layouts::px(10), layouts::px(10), layouts::px(10)},
      .areas = {"a . a"}};
  const std::vector<SkRect> at =
      grid.place(given({30, 20}, boxes(1, {5, 5}), {}, {"a"}));
  ASSERT_EQ(at.size(), 1u);
  EXPECT_FLOAT_EQ(at[0].left(), 0);
  EXPECT_FLOAT_EQ(at[0].width(), 30);
}

TEST(ComposeGrid, ANameThePictureDoesNotCarryFlowsLikeAChildThatSaidNothing) {
  const Grid grid{.columns = {layouts::px(10), layouts::px(10)},
                  .areas = {"a b"}};
  const std::vector<SkRect> at =
      grid.place(given({20, 20}, boxes(2, {5, 5}), {}, {"nowhere", "b"}));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_FLOAT_EQ(at[1].left(), 10);  // "b" is where the picture says
  EXPECT_FLOAT_EQ(at[0].left(), 0);   // and the unknown name took a free cell
}

// ---------------------------------------------------------------------------
// The flow

TEST(ComposeGrid, SparseFlowNeverLooksBackAndDenseFillsTheHoleBehindIt) {
  // Three columns, two children two wide and one single: the second wide
  // child cannot start at column two, so it drops to the next row and
  // leaves a one-cell hole at the end of the first.
  const std::vector<SkSize> sizes = boxes(3, {5, 5});
  const std::vector<CellSpan> spans = {CellSpan{.columns = 2},
                                       CellSpan{.columns = 2}, CellSpan{}};
  const std::vector<layouts::Track> three = {layouts::px(10), layouts::px(10),
                                             layouts::px(10)};

  const Grid sparse{.columns = three,
                    .rows = {layouts::px(10), layouts::px(10)}};
  const std::vector<SkRect> flowed =
      sparse.place(given({30, 20}, sizes, spans));
  ASSERT_EQ(flowed.size(), 3u);
  EXPECT_EQ(flowed[1], SkRect::MakeXYWH(0, 10, 20, 10));
  EXPECT_EQ(flowed[2], SkRect::MakeXYWH(20, 10, 10, 10));

  const Grid dense{.columns = three,
                   .rows = {layouts::px(10), layouts::px(10)},
                   .dense = true};
  const std::vector<SkRect> packed = dense.place(given({30, 20}, sizes, spans));
  ASSERT_EQ(packed.size(), 3u);
  EXPECT_EQ(packed[1], SkRect::MakeXYWH(0, 10, 20, 10));
  EXPECT_EQ(packed[2], SkRect::MakeXYWH(20, 0, 10, 10));  // back into the hole
}

TEST(ComposeGrid, AFlowingChildNeverLandsOnACellSomethingElseClaimed) {
  const Grid grid{.columns = {layouts::px(10), layouts::px(10)},
                  .rows = {layouts::px(10), layouts::px(10)}};
  const std::vector<SkRect> at =
      grid.place(given({20, 20}, boxes(2, {5, 5}), {CellSpan{}, claims(0, 0)}));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 0, 10, 10));
  EXPECT_EQ(at[0], SkRect::MakeXYWH(10, 0, 10, 10));
}

TEST(ComposeGrid, AChildWiderThanTheGridTakesTheWholeOfIt) {
  // A span no arrangement of the grid could hold: there is no cell three
  // columns wide in a three-column grid at which a five-wide child is
  // free, so the search for one is the grid it has, not a search without
  // an end.
  const Grid grid{
      .columns = {layouts::px(10), layouts::px(10), layouts::px(10)},
      .rows = {layouts::px(10), layouts::px(10)}};
  std::vector<CellSpan> spans(2);
  spans[0].columns = 5;
  const std::vector<SkRect> at =
      grid.place(given({30, 20}, boxes(2, {5, 5}), spans));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 30, 10));
  // And the one behind it flows onto the row below, the whole first row
  // being spoken for.
  EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 10, 10, 10));
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
  const std::vector<SkRect> at =
      grid.place(given({80, 40}, boxes(2, {10, 10}), spans));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_EQ(at[0], SkRect::MakeXYWH(0, 0, 40, 40));  // stretched, the default
  EXPECT_EQ(at[1], SkRect::MakeXYWH(55, 30, 10, 10));
}

// ---------------------------------------------------------------------------
// The content minima, through the composer

namespace {

/** A scheme that keeps what it was handed, so a case can read the input the
 *  composer built rather than the rects a scheme made of it. */
struct Recorder {
  std::shared_ptr<LayoutInput> seen = std::make_shared<LayoutInput>();
  bool operator==(const Recorder& o) const { return seen == o.seen; }
  static constexpr bool readsChildMinSizes = true;
  std::vector<SkRect> place(const LayoutInput& in) const {
    *seen = in;
    std::vector<SkRect> at(in.childSizes.size());
    for (size_t i = 0; i < at.size(); ++i)
      at[i] = SkRect::MakeXYWH(0, (float)i * 20, in.childSizes[i].width(),
                               in.childSizes[i].height());
    return at;
  }
};

}  // namespace

TEST(ComposeGrid, ASchemeThatAsksIsToldHowNarrowItsTextChildrenCanGo) {
  Host host(300, 200);
  const Recorder recorder;
  host.composer.render(box().absolute().inset(0).children(
      {layout(recorder).absolute().left(0.0f).top(0.0f).width(200.0f).children(
          {text(u8"one two three four five six seven eight", styleAt(12))
               .width(200.0f)})}));
  host.frame();
  ASSERT_EQ(recorder.seen->childMinSizes.size(), 1u);
  // The narrowest the paragraph goes is one word, and it was measured at
  // two hundred — so the minimum is real and it is smaller.
  EXPECT_GT(recorder.seen->childMinSizes[0].width(), 0.0f);
  EXPECT_LT(recorder.seen->childMinSizes[0].width(),
            recorder.seen->childSizes[0].width());
}

TEST(ComposeGrid, ASchemeThatDoesNotAskIsToldNothingAndPaysForNothing) {
  Host host(300, 200);
  struct Quiet {
    bool operator==(const Quiet&) const = default;
    std::vector<SkRect> place(const LayoutInput& in) const {
      return std::vector<SkRect>(in.childSizes.size(), SkRect::MakeWH(10, 10));
    }
  };
  // The concept, not the runtime: a scheme without the declaration is not
  // one the composer measures minima for.
  static_assert(!sigil::compose::SizesFromContentMinima<Quiet>);
  static_assert(sigil::compose::SizesFromContentMinima<Grid>);
}

// ---------------------------------------------------------------------------
// Through the composer

TEST(ComposeGrid, AChildClaimsItsRegionByNameThroughTheComposer) {
  Host host(400, 200);
  host.composer.render(
      layout(Grid{.columns = {layouts::px(100), layouts::fr(1)},
                  .rows = {layouts::px(40), layouts::fr(1)},
                  .areas = {"head head", "nav  main"}})
          .absolute()
          .inset(0)
          .children({box().key("head").area("head").fill(red()),
                     box().key("nav").area("nav").fill(green()),
                     box().key("main").area("main").fill(blue())}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("head")),
            SkRect::MakeXYWH(0, 0, 400, 40));
  EXPECT_EQ(require(host.composer.bounds("nav")),
            SkRect::MakeXYWH(0, 40, 100, 160));
  EXPECT_EQ(require(host.composer.bounds("main")),
            SkRect::MakeXYWH(100, 40, 300, 160));
}

TEST(ComposeGrid, AGridEmbeddedInAColumnTakesItsHeightFromWhatItPlaced) {
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
}

TEST(ComposeGrid, AStretchedGridContributesToItsContentSizedRow) {
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
       {std::array{80.0f, 240.0f, 240.0f}, std::array{320.0f, 240.0f, 320.0f},
        std::array{80.0f, 120.0f, 120.0f}}) {
    host.composer.render(tree(siblingHeight, figureHeight));
    host.frame();
    EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(),
                    figureHeight);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                    expected + 20);
  }
}

TEST(ComposeGrid, AStretchedGridKeepsItsParentsExplicitHeight) {
  Host host(500, 400);
  host.composer.render(box().column().gap(20).children(
      {box().row().height(80).children(
           {layout(Grid{.columns = {layouts::fr()}, .rows = {layouts::px(240)}})
                .key("grid")
                .width(300)
                .children({box().key("figure").fill(red())})}),
       box().key("following").height(20)}));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 80);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(), 240);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 100);
}

TEST(ComposeGrid, AStretchedGridsNaturalExtentRespectsItsOwnMaximum) {
  Host host(500, 600);
  const auto tree = [](float siblingHeight) {
    return box().column().gap(20).children(
        {box().key("row").row().gap(20).children(
             {layout(
                  Grid{.columns = {layouts::fr()}, .rows = {layouts::px(240)}})
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
    EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), rowHeight);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(), 240);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                    rowHeight + 20);
  }
}

TEST(ComposeGrid, AMaximumBoundsAContentSizedRowWithoutFixingItsHeight) {
  Host host(500, 600);
  for (const float maximum : {500.0f, 100.0f, 500.0f}) {
    host.composer.render(box().column().gap(20).children(
        {box().key("row").row().maxHeight(maximum).children(
             {layout(
                  Grid{.columns = {layouts::fr()}, .rows = {layouts::px(240)}})
                  .key("grid")
                  .width(300)
                  .children({box().key("figure").fill(red())}),
              box().width(100).height(80)}),
         box().key("following").height(20)}));
    host.frame();
    const float expected = std::min(maximum, 240.0f);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).height(), 240);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(),
                    expected + 20);
  }
}

TEST(ComposeGrid, AContentSizedRowsMinimumCanStretchAShorterGrid) {
  Host host(500, 600);
  host.composer.render(box().column().gap(20).children(
      {box().key("row").row().minHeight(200).maxHeight(500).children(
           {layout(Grid{.columns = {layouts::fr()}, .rows = {layouts::px(120)}})
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
}

TEST(ComposeGrid, AParentsMaximumLeavesRoomForItsPaddingAndTheGridMargin) {
  Host host(500, 600);
  host.composer.render(box().column().gap(20).children(
      {box()
           .key("row")
           .row()
           .maxHeight(100)
           .padding(0, 10, 0, 10)
           .children({layout(Grid{.columns = {layouts::fr()},
                                  .rows = {layouts::px(240)}})
                          .key("grid")
                          .width(300)
                          .margin(0, 5, 0, 5)
                          .children({box().key("figure").fill(red())}),
                      box().width(100).height(60)}),
       box().key("following").height(20)}));
  host.frame();
  const SkRect grid = require(host.composer.bounds("grid"));
  EXPECT_FLOAT_EQ(grid.top(), 15);
  EXPECT_FLOAT_EQ(grid.height(), 70);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).height(), 100);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 120);
}

TEST(ComposeGrid, AMaximumBoundsAContentSizedColumnWithoutFixingItsWidth) {
  Host host(600, 500);
  for (const float maximum : {500.0f, 100.0f, 500.0f}) {
    host.composer.render(
        box()
            .row()
            .alignItems(Align::Start)
            .gap(20)
            .children({box().key("column").column().maxWidth(maximum).children(
                           {layout(Grid{.columns = {layouts::px(240)},
                                        .rows = {layouts::fr()}})
                                .key("grid")
                                .height(300)
                                .children({box().key("figure").fill(red())}),
                            box().width(80).height(100)}),
                       box().key("following").width(20).height(20)}));
    host.frame();
    const float expected = std::min(maximum, 240.0f);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).width(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("column")).width(), expected);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("figure")).width(), 240);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).left(),
                    expected + 20);
  }
}

TEST(ComposeGrid, AMaximumPassesThroughNestedStretchedRows) {
  Host host(500, 600);
  host.composer.render(box().column().gap(20).children(
      {box()
           .key("outer")
           .row()
           .maxHeight(100)
           .padding(0, 10, 0, 10)
           .children({box()
                          .key("inner")
                          .row()
                          .padding(0, 5, 0, 5)
                          .children({layout(Grid{.columns = {layouts::fr()},
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
}

TEST(ComposeGrid, TextWrapsAtItsResolvedTrackWidthBeforeRowsAreSized) {
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
           .children({paragraph().key("paragraph").shrink(0).cells(0, 0),
                      box().height(18).cells(1, 0),
                      box().key("nextRow").height(20).cells(0, 1)}),
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
}

TEST(ComposeGrid, TextWrapsAtItsBoundedWidthBeforeRowsAreSized) {
  const auto paragraph = [] {
    return text(
        "One paragraph wraps at its own bounds even when the grid track "
        "offers a different width. Every line belongs above the next row.",
        styleAt(16));
  };
  for (const bool maximum : {true, false}) {
    const float trackWidth = maximum ? 240 : 80;
    const float expectedWidth = maximum ? 100 : 140;
    Element bounded = paragraph().key("paragraph").shrink(0).cells(0, 0);
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
            .children({std::move(bounded),
                       box().key("following").height(20).cells(0, 1)}));
    host.frame();
    Host reference(500, 600);
    reference.composer.render(
        box().children({paragraph().key("reference").width(expectedWidth)}));
    reference.frame();
    const SkRect actual = require(host.composer.bounds("paragraph"));
    EXPECT_FLOAT_EQ(actual.width(), expectedWidth);
    EXPECT_NEAR(actual.height(),
                require(reference.composer.bounds("reference")).height(),
                0.25f);
    EXPECT_NEAR(require(host.composer.bounds("following")).top(),
                actual.bottom() + 12, 0.25f);
  }
}

TEST(ComposeGrid, ACustomSchemeCanReturnFewerRectsAfterTextReflow) {
  struct ReflowSensitive {
    std::shared_ptr<bool> shortened = std::make_shared<bool>(false);
    std::vector<SkRect> place(const LayoutInput& in) const {
      if (in.childSizes.front().height() > 40) {
        *shortened = true;
        return {};
      }
      return {SkRect::MakeWH(60, in.childSizes.front().height())};
    }
  };
  Host host(500, 400);
  const ReflowSensitive scheme;
  host.composer.render(
      layout(scheme)
          .row()
          .alignItems(Align::Start)
          .children(
              {text(u8"A sentence becomes several lines at the proposed width.",
                    styleAt(16))
                   .shrink(0)}));
  host.frame();
  EXPECT_TRUE(*scheme.shortened);
}

TEST(ComposeGrid, WrappedTextKeepsItsDeclaredExtentBounds) {
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
                   .cells(0, 0),
               text("A long paragraph whose many words would need more than "
                    "one line when placed within a narrow track.",
                    styleAt(16))
                   .key("maximum")
                   .maxHeight(30)
                   .cells(1, 0),
               box().key("following").height(20).cells(0, 1)}));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("minimum")).height(), 90);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("maximum")).height(), 30);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("following")).top(), 102);
}

TEST(ComposeGrid, RepeatTrackIsNCopiesOfOneTrack) {
  const std::vector<layouts::Track> four =
      layouts::repeatTrack(4, layouts::fr(1));
  ASSERT_EQ(four.size(), 4u);
  EXPECT_EQ(four[0], layouts::fr(1));
  EXPECT_EQ(four[3], layouts::fr(1));
  // …and a grid built from it divides its container four ways.
  const Grid grid{.columns = four};
  const std::vector<SkRect> at =
      grid.place(given({400, 100}, boxes(4, {10, 10})));
  ASSERT_EQ(at.size(), 4u);
  for (int i = 0; i < 4; ++i)
    EXPECT_FLOAT_EQ(at[(size_t)i].left(), (float)i * 100.0f);
  // A count of none is no tracks at all, not one.
  EXPECT_TRUE(layouts::repeatTrack(0, layouts::px(10)).empty());
  EXPECT_TRUE(layouts::repeatTrack(-3, layouts::px(10)).empty());
}

TEST(ComposeGrid, EqualModulesRespectSpansAndFlowIntoFreeCells) {
  // 4×4 modules, gutter 8, container 200×200 → module 44×44. Child 0 spans
  // 2×1 from (0,0); child 1 spans 1×3 from (3,0); children 2..3 auto-flow.
  Host host;
  const layouts::Grid grid{.columns = layouts::repeatTrack(4, layouts::fr()),
                           .rows = layouts::repeatTrack(4, layouts::fr()),
                           .gap = {8, 8}};
  host.composer.render(box().children(
      {layout(grid)
           .width(pct(100))
           .grow(1)
           .children({box().key("a").cells(0, 0, 2, 1).fill(red())})
           .children({box().key("b").cells(3, 0, 1, 3).fill(blue())})
           .children({box().key("c").fill(green())})
           .children({box().key("d").fill(red())})}));
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
  EXPECT_NEAR(c->left(), 104, 0.01f);            // first free cell: (2, 0)
  EXPECT_NEAR(c->top(), 0, 0.01f);
  EXPECT_NEAR(c->width(), 44, 0.01f);
  EXPECT_NEAR(d->left(), 0, 0.01f);  // next free cell: (0, 1)
  EXPECT_NEAR(d->top(), 52, 0.01f);
}
