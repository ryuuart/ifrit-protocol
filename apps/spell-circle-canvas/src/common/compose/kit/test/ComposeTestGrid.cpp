// The grid: the four track sizing functions, the picture of named areas a
// child claims a region of, the two auto-flows, and the content minima the
// rule floors a content track at.

#include <sigilcompose/kit/Grid.h>

#include <limits>
#include <memory>

#include "support/ShapeTestSupport.h"

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

TEST(KitGrid, AFixedTrackIsItsLengthAndFreeSpaceBesideItStaysFree) {
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

TEST(KitGrid, AContentTrackIsTheWidestThingInItAndDoesNotStretch) {
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

TEST(KitGrid, SharesDivideWhatIsLeftAfterTheFixedTracks) {
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

TEST(KitGrid, WeightsSummingUnderOneTakeOnlyTheirOwnShareOfTheFreeSpace) {
  const Grid grid{.columns = {layouts::fr(0.5f), layouts::fr(0.25f)}};
  const std::vector<SkRect> at =
      grid.place(given({200, 50}, boxes(2, {0, 10})));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_FLOAT_EQ(at[0].width(), 100);  // half a share, not half the space
  EXPECT_FLOAT_EQ(at[1].width(), 50);
  EXPECT_FLOAT_EQ(at[1].right(), 150);  // and a quarter of it is left over
}

TEST(KitGrid, AShareThatWouldFallUnderItsFloorFreezesAndTheRestRedivides) {
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

TEST(KitGrid, MinmaxIsAFloorAndACeilingAndBothHold) {
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

TEST(KitGrid, ASpanTopsUpItsTracksInProportionToWhatTheyAlreadyHold) {
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

TEST(KitGrid, ARowSpanSharesItsDeficitAcrossItsRowsJustAsAColumnSpanDoes) {
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

TEST(KitGrid, ANameInThePicturePlacesAChildOnTheRectangleItCovers) {
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

TEST(KitGrid, ThePictureAloneSaysHowWideAndHowDeepTheGridIs) {
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

TEST(KitGrid, ANameThatCoversNoRectangleIsPlacedAtTheRectangleBoundingIt) {
  const Grid grid{
      .columns = {layouts::px(10), layouts::px(10), layouts::px(10)},
      .areas = {"a . a"}};
  const std::vector<SkRect> at =
      grid.place(given({30, 20}, boxes(1, {5, 5}), {}, {"a"}));
  ASSERT_EQ(at.size(), 1u);
  EXPECT_FLOAT_EQ(at[0].left(), 0);
  EXPECT_FLOAT_EQ(at[0].width(), 30);
}

TEST(KitGrid, ANameThePictureDoesNotCarryFlowsLikeAChildThatSaidNothing) {
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

TEST(KitGrid, SparseFlowNeverLooksBackAndDenseFillsTheHoleBehindIt) {
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

TEST(KitGrid, AFlowingChildNeverLandsOnACellSomethingElseClaimed) {
  const Grid grid{.columns = {layouts::px(10), layouts::px(10)},
                  .rows = {layouts::px(10), layouts::px(10)}};
  const std::vector<SkRect> at =
      grid.place(given({20, 20}, boxes(2, {5, 5}), {CellSpan{}, claims(0, 0)}));
  ASSERT_EQ(at.size(), 2u);
  EXPECT_EQ(at[1], SkRect::MakeXYWH(0, 0, 10, 10));
  EXPECT_EQ(at[0], SkRect::MakeXYWH(10, 0, 10, 10));
}

// ---------------------------------------------------------------------------
// Alignment

TEST(KitGrid, TheGridsOwnAlignmentHoldsUntilAChildStatesItsOwn) {
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

TEST(KitGrid, ASchemeThatAsksIsToldHowNarrowItsTextChildrenCanGo) {
  Host host(300, 200);
  const Recorder recorder;
  host.composer.render(box().absolute().inset(0).child(
      layout(recorder)
          .absolute()
          .left(Dim(0.0f))
          .top(Dim(0.0f))
          .width(Dim(200.0f))
          .child(text(u8"one two three four five six seven eight", styleAt(12))
                     .width(Dim(200.0f)))));
  host.frame();
  ASSERT_EQ(recorder.seen->childMinSizes.size(), 1u);
  // The narrowest the paragraph goes is one word, and it was measured at
  // two hundred — so the minimum is real and it is smaller.
  EXPECT_GT(recorder.seen->childMinSizes[0].width(), 0.0f);
  EXPECT_LT(recorder.seen->childMinSizes[0].width(),
            recorder.seen->childSizes[0].width());
}

TEST(KitGrid, ASchemeThatDoesNotAskIsToldNothingAndPaysForNothing) {
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

TEST(KitGrid, AChildClaimsItsRegionByNameThroughTheComposer) {
  Host host(400, 200);
  host.composer.render(
      layout(Grid{.columns = {layouts::px(100), layouts::fr(1)},
                  .rows = {layouts::px(40), layouts::fr(1)},
                  .areas = {"head head", "nav  main"}})
          .absolute()
          .inset(0)
          .child(box().key("head").area("head").fill(red()))
          .child(box().key("nav").area("nav").fill(green()))
          .child(box().key("main").area("main").fill(blue())));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("head")),
            SkRect::MakeXYWH(0, 0, 400, 40));
  EXPECT_EQ(require(host.composer.bounds("nav")),
            SkRect::MakeXYWH(0, 40, 100, 160));
  EXPECT_EQ(require(host.composer.bounds("main")),
            SkRect::MakeXYWH(100, 40, 300, 160));
}

TEST(KitGrid, AGridEmbeddedInAColumnTakesItsHeightFromWhatItPlaced) {
  // Every child of a scheme is placed absolutely, so nothing under the
  // container contributes to its height and flex would collapse it to
  // nothing. The extent it placed is the height instead.
  Host host(300, 300);
  host.composer.render(box().column().absolute().inset(0).child(
      layout(Grid{.columns = {layouts::fr(1), layouts::fr(1)},
                  .rows = {layouts::px(60)}})
          .key("grid")
          .child(box().fill(red()))
          .child(box().fill(green()))));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).height(), 60);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("grid")).width(), 300);
}
