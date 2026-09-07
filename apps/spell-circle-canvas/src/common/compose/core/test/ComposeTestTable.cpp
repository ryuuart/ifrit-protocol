// The auto table: how a column takes its width from what is in it, how a
// span tops up the tracks it covers, and where a child that claimed no
// cells lands. The layout is the kernel's — a scheme the kit configures
// but does not own — so its cases stand here, with the seam a child
// claims its cells through.

#include <limits>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"

TEST(ComposeTable, AColumnIsAsWideAsWhatIsInItAndSharesTheSurplus) {
  // Three columns of unequal content in a 300-wide table with no spacing:
  // each column starts at its widest child, and the 300 − 180 left over is
  // shared out IN PROPORTION, so the widest column takes the most of it.
  Host host;
  Table table{.width = 300};
  host.composer.render(
      box().child(layout(table)
                      .width(pct(100))
                      .grow(1)
                      .child(box().key("a").width(30).height(20).cells(0, 0))
                      .child(box().key("b").width(60).height(20).cells(1, 0))
                      .child(box().key("c").width(90).height(20).cells(2, 0))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  const auto c = host.composer.bounds("c");
  ASSERT_TRUE(a && b && c);
  // 30 : 60 : 90 of 180, scaled to 300 — the proportional share.
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), 50, 0.01f);
  EXPECT_NEAR(c->left(), 150, 0.01f);
  // The children keep their measured size; the COLUMN grew, not the child.
  EXPECT_NEAR(a->width(), 30, 0.01f);
}

TEST(ComposeTable, ASpanTopsUpColumnsAndRowsDifferently) {
  // The asymmetry that is the browsers' and not a slip: a colspan's
  // deficit is shared across the columns it covers, a rowspan's lands
  // entirely on the LAST row it covers.
  Host host;
  Table table{.columns = 2, .rows = 2, .width = 100};
  host.composer.render(box().child(
      layout(table)
          .width(200)
          .height(200)
          .child(box().key("wide").width(100).height(10).cells(0, 0, 2, 1))
          .child(box().key("tall").width(10).height(100).cells(0, 1, 1, 2))
          .child(box().key("small").width(10).height(10).cells(1, 1))));
  host.frame();
  const auto wide = host.composer.bounds("wide");
  const auto tall = host.composer.bounds("tall");
  const auto small = host.composer.bounds("small");
  ASSERT_TRUE(wide && tall && small);
  // The two columns start at 10 (the tall child) and 10 (the small one),
  // and the 100-wide span tops both up equally — 50 each, which is the
  // whole 100-wide table, so nothing is left over to share.
  EXPECT_NEAR(small->left(), 50, 0.01f);
  // Row 1 holds a 10-tall child and the first row of a 100-tall span; the
  // span's deficit goes to row 2, so row 1 stays at its own content.
  EXPECT_NEAR(small->top(), 10, 0.01f);
  EXPECT_NEAR(tall->top(), 10, 0.01f);
}

TEST(ComposeTable, WhatNoChildClaimedFlowsAndAlignsInsideItsCell) {
  // A child that says nothing takes the next cell NO declared child
  // claimed — never one already spoken for, which is the failure of a
  // scheme that counts its flow from zero.
  Host host;
  Table table{.columns = 2, .width = 200};
  host.composer.render(box().child(
      layout(table)
          .width(200)
          .height(100)
          .child(box().key("pinned").width(20).height(20).cells(1, 0))
          .child(box().key("flowed").width(20).height(20))
          .child(box().key("right").width(20).height(20).cells(1, 1).cellAlign(
              Align::End, Align::Start))));
  host.frame();
  const auto pinned = host.composer.bounds("pinned");
  const auto flowed = host.composer.bounds("flowed");
  const auto right = host.composer.bounds("right");
  ASSERT_TRUE(pinned && flowed && right);
  EXPECT_NEAR(flowed->left(), 0, 0.01f) << "cell (1,0) was taken";
  EXPECT_NEAR(flowed->top(), 0, 0.01f);
  EXPECT_GT(pinned->left(), flowed->left());
  // Both are in column 1, which the surplus grew to 100 wide. The pinned
  // child sits at its start; the end-aligned one is flush with the far
  // side of the same cell.
  EXPECT_NEAR(pinned->right(), 120, 0.01f);
  EXPECT_NEAR(right->right(), 200, 0.01f);
  EXPECT_GT(right->top(), 0.0f) << "the second row";
}

// ---------------------------------------------------------------------------
// The scheme solved directly, from a LayoutInput built by hand: the numbers
// a column resolves to are what a study of a published page diffs against,
// and no placed rect carries them.

namespace {

/** What a scheme is handed: the container, the children's measured sizes,
 *  the cells they claimed, and the narrowest each can be set — which
 *  defaults to the measured size, as the composer fills it for every child
 *  that is not text. */
LayoutInput given(SkSize container, std::vector<SkSize> sizes,
                  std::vector<CellSpan> cells = {},
                  std::vector<SkSize> minima = {}) {
  LayoutInput in;
  in.container = container;
  in.childSizes = std::move(sizes);
  in.childBaselines.assign(in.childSizes.size(),
                           std::numeric_limits<float>::quiet_NaN());
  in.childCells = std::move(cells);
  in.childCells.resize(in.childSizes.size());
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

}  // namespace

static_assert(sigil::compose::SizesFromContentMinima<Table>,
              "the auto rule is solved from the content minima");

TEST(ComposeTable, ItResolvesTheGridAPageOfTheTableEraWasSetOn) {
  // www.spacejam.com (1996): <TABLE WIDTH=500 CELLSPACING=2 CELLPADDING=1>,
  // five columns by five rows, with a colspan=2, a colspan=3 rowspan=2 and
  // a rowspan=2 in it. Each cell measures the <img> in it plus 18 px for
  // every <br> above the image, which is what the page's quirks-mode line
  // box comes to. The expected numbers are what headless Chrome reports
  // for the same markup at a 640 px viewport.
  struct Cell {
    float w, h;
    CellSpan at;
  };
  const Cell cells[] = {
      {0, 0, claims(0, 0, 5, 1)},      // the empty <TD> across the top
      {131, 110, claims(0, 1, 2, 1)},  // press box, three <br> above it
      {55, 67, claims(2, 1)},          // jam central
      {62, 62, claims(3, 1)},          // bball
      {95, 113, claims(4, 1)},         // lunar tunes, two <br>
      {63, 88, claims(0, 2)},          // line up, two <br>
      {272, 165, claims(1, 2, 3, 2)},  // the logotype
      {58, 52, claims(4, 2)},          // jump
      {49, 57, claims(0, 3)},          // junior
      {94, 108, claims(4, 3, 1, 2)},   // studio store, two <br>
      {0, 0, claims(0, 4)},            // the second empty <TD>
      {83, 83, claims(1, 4)},          // souvenirs
      {104, 139, claims(2, 4)},        // site map, four <br>
      {67, 63, claims(3, 4)},          // behind the jam
  };
  std::vector<SkSize> sizes;
  std::vector<CellSpan> spans;
  for (const Cell& c : cells) {
    sizes.push_back({c.w, c.h});
    spans.push_back(c.at);
  }

  const Table table{
      .columns = 5, .rows = 5, .width = 500, .spacing = 2, .padding = 1};
  const Table::Grid grid = table.solve(given({500, 435}, sizes, spans));

  // The surplus lands on fractional pixels, so the columns agree to a
  // tenth; the rows are whole content heights and agree exactly.
  const float browserColumns[5] = {71.42f, 97.70f, 122.33f, 78.95f, 107.59f};
  ASSERT_EQ(grid.columnWidths.size(), 5u);
  for (size_t i = 0; i < 5; ++i)
    EXPECT_NEAR(grid.columnWidths[i], browserColumns[i], 0.15f)
        << "column " << i;
  const float browserRows[5] = {0, 113, 88, 73, 139};
  ASSERT_EQ(grid.rowHeights.size(), 5u);
  for (size_t i = 0; i < 5; ++i)
    EXPECT_NEAR(grid.rowHeights[i], browserRows[i], 0.01f) << "row " << i;
  // Row 3 is the one the logotype's rowspan pays for: 57 of content and
  // the whole 16 of the span's deficit, which is the asymmetry.
  EXPECT_NEAR(grid.rowHeights[3], 73, 0.01f);
}

TEST(ComposeTable, TooNarrowAndEveryColumnGivesUpTheSameFractionOfItsRoom) {
  // Three columns wanting 200 between them in a table with 170: each
  // falls the same fraction of the way from what it wants to what it
  // needs, and the one that needs what it wants does not move at all.
  const Table table{.width = 170};
  const Table::Grid grid =
      table.solve(given({170, 100}, {{100, 20}, {60, 20}, {40, 20}},
                        {claims(0, 0), claims(1, 0), claims(2, 0)},
                        {SkSize{40, 20}, SkSize{60, 20}, SkSize{40, 20}}));
  ASSERT_EQ(grid.columnWidths.size(), 3u);
  EXPECT_NEAR(grid.columnWidths[0], 70, 0.01f)
      << "40 + half of the 60 it can give";
  EXPECT_NEAR(grid.columnWidths[1], 60, 0.01f) << "nothing to give";
  EXPECT_NEAR(grid.columnWidths[2], 40, 0.01f) << "nothing to give";

  // Narrower than the content can be set at all: the columns stand at what
  // they need and the table overflows rather than dropping content.
  const Table pinched{.width = 60};
  const Table::Grid tight =
      pinched.solve(given({60, 100}, {{100, 20}, {60, 20}, {40, 20}},
                          {claims(0, 0), claims(1, 0), claims(2, 0)},
                          {SkSize{40, 20}, SkSize{60, 20}, SkSize{40, 20}}));
  EXPECT_NEAR(tight.columnWidths[0], 40, 0.01f);
  EXPECT_NEAR(tight.columnWidths[1], 60, 0.01f);
  EXPECT_NEAR(tight.columnWidths[2], 40, 0.01f);
}

TEST(ComposeTable, AShrinkToFitTableStopsAtWhatIsInIt) {
  // The same three columns in the same 300-wide container, the one with a
  // width in its markup and the one without.
  const std::vector<SkSize> sizes{{30, 20}, {60, 20}, {90, 20}};
  const std::vector<CellSpan> spans{claims(0, 0), claims(1, 0), claims(2, 0)};
  const Table shrink{.fit = Table::Fit::Shrink};
  const Table::Grid tight = shrink.solve(given({300, 100}, sizes, spans));
  EXPECT_NEAR(tight.columnWidths[0], 30, 0.01f);
  EXPECT_NEAR(tight.columnWidths[2], 90, 0.01f);
  EXPECT_NEAR(tight.columnX[2], 90, 0.01f) << "packed at the start";

  const Table fill{.fit = Table::Fit::Fill};
  const Table::Grid spread = fill.solve(given({300, 100}, sizes, spans));
  EXPECT_NEAR(spread.columnWidths[0], 50, 0.01f);
  EXPECT_NEAR(spread.columnWidths[2], 150, 0.01f);

  // Shrink says nothing about a table with too little room: the columns
  // fall toward what they need either way.
  const Table::Grid pinched =
      shrink.solve(given({120, 100}, sizes, spans,
                         {SkSize{30, 20}, SkSize{30, 20}, SkSize{30, 20}}));
  EXPECT_LT(pinched.columnWidths[2], 90.0f);
  EXPECT_GE(pinched.columnWidths[2], 30.0f);
}

TEST(ComposeTable, ADeclaredColumnKeepsItsWidthAndTheRestShareTheSurplus) {
  // <COL WIDTH=50> on the middle column of three: it neither takes a share
  // of the 150 left over nor gives one up, so the rail the markup states
  // is the width it is stated at.
  const Table table{.width = 200, .declaredWidths = {0, 50, 0}};
  const Table::Grid grid =
      table.solve(given({200, 100}, {{30, 20}, {20, 20}, {60, 20}},
                        {claims(0, 0), claims(1, 0), claims(2, 0)}));
  EXPECT_NEAR(grid.columnWidths[1], 50, 0.01f);
  EXPECT_NEAR(grid.columnWidths[0], 50, 0.01f) << "30 of the 90 wanted";
  EXPECT_NEAR(grid.columnWidths[2], 100, 0.01f) << "60 of the 90 wanted";
  EXPECT_NEAR(grid.columnX[1], 50, 0.01f);

  // What is in a column still widens it past what the markup asked for:
  // no column is narrower than the narrowest thing in it.
  const Table cramped{.width = 200, .declaredWidths = {10}};
  const Table::Grid grown = cramped.solve(
      given({200, 100}, {{40, 20}, {20, 20}}, {claims(0, 0), claims(1, 0)}));
  EXPECT_NEAR(grown.columnWidths[0], 40, 0.01f);
}

TEST(ComposeTable, AColumnNothingFillsCollapsesAndLeavesTheRestAlone) {
  // The page's empty <TD> is a real cell with nothing in it. Its column
  // takes no width and no share of the surplus, and the columns beside it
  // resolve as though it were not there.
  const Table table{.columns = 3, .width = 300};
  const Table::Grid grid =
      table.solve(given({300, 100}, {{50, 20}, {0, 0}, {100, 20}},
                        {claims(0, 0), claims(1, 0), claims(2, 0)}));
  ASSERT_EQ(grid.columnWidths.size(), 3u);
  EXPECT_NEAR(grid.columnWidths[1], 0, 0.01f);
  EXPECT_NEAR(grid.columnWidths[0], 100, 0.01f);
  EXPECT_NEAR(grid.columnWidths[2], 200, 0.01f);
  EXPECT_NEAR(grid.columnX[2], 100, 0.01f) << "the collapsed column is passed";

  // A table with no children at all resolves one empty track rather than
  // none, and places nothing.
  const Table::Grid nothing = table.solve(given({300, 100}, {}));
  EXPECT_EQ(nothing.columnWidths.size(), 3u);
  EXPECT_TRUE(table.place(given({300, 100}, {})).empty());
}
