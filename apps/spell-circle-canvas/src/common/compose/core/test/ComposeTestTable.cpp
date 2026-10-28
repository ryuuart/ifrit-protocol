// The auto table: how a column takes its width from what is in it, how a
// span tops up the tracks it covers, and where a child that claimed no
// cells lands. The layout is the kernel's — a scheme the kit configures
// but does not own — so its cases stand here, with the seam a child
// claims its cells through.

#include <limits>
#include <utility>
#include <vector>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::layouts::Table;

namespace {

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
            std::vector<glm::vec2> minima = {}) {
  Given in{geometry::path::Rect::of({0, 0}, container), {}};
  for (size_t i = 0; i < sizes.size(); ++i) {
    Arrangement::Child child;
    child.size = sizes[i];
    child.minimumSize = minima.empty() ? sizes[i] : minima[i];
    if (i < cells.size()) child.cells = cells[i];
    in.children.push_back(std::move(child));
  }
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

static_assert(sigil::compose::ReadsChildMinimumSizes<Table>,
              "the auto rule is solved from the content minima");

namespace {

/** One solve of the auto rule from hand-built records: the table, the room
 *  it is given, what each column's child wants and needs, and every column
 *  width the rule resolves, with one column origin where the row reads it. */
struct SolveRow {
  const char* name;
  Table table;
  glm::vec2 container;
  std::vector<glm::vec2> sizes;
  std::vector<glm::vec2> minima;
  std::vector<float> widths;
  int originColumn = -1;
  float origin = 0;
};

/** Every child claims its own column of the first row. */
std::vector<CellSpan> oneColumnEach(size_t count) {
  std::vector<CellSpan> spans;
  for (size_t column = 0; column < count; ++column)
    spans.push_back(claims((int)column, 0));
  return spans;
}

void expectSolved(const SolveRow& row) {
  const Table::Grid grid = row.table.solve(given(
      row.container, row.sizes, oneColumnEach(row.sizes.size()), row.minima));
  ASSERT_EQ(grid.columnWidths.size(), row.widths.size());
  for (size_t column = 0; column < row.widths.size(); ++column)
    EXPECT_NEAR(grid.columnWidths[column], row.widths[column], 0.01f)
        << "column " << column;
  if (row.originColumn >= 0)
    EXPECT_NEAR(grid.columnX[(size_t)row.originColumn], row.origin, 0.01f)
        << "the origin of column " << row.originColumn;
}

}  // namespace

// Each column starts at the widest child that sits in it alone, a column
// nothing fills collapses, and the surplus is shared in proportion.
TEST(ComposeTable, AColumnStartsAtWhatIsInItAndSharesTheSurplusInProportion) {
  sigil::compose::test::checkRows({
      {"ThroughTheComposer",
       [] {
         // Three columns of unequal content in a 300-wide table with no
         // spacing: each column starts at its widest child, and the 300 − 180
         // left over is shared out IN PROPORTION, so the widest column takes
         // the most of it.
         Host host;
         Table table{.width = 300};
         host.composer.render(box().children(
             {layout(table)
                  .width(percent(100))
                  .flexGrow(1)
                  .children(
                      {box().key("a").width(30).height(20).gridCells(0, 0)})
                  .children(
                      {box().key("b").width(60).height(20).gridCells(1, 0)})
                  .children(
                      {box().key("c").width(90).height(20).gridCells(2, 0)})}));
         host.frame();
         const auto a = host.composer.bounds("a");
         const auto b = host.composer.bounds("b");
         const auto c = host.composer.bounds("c");
         ASSERT_TRUE(a && b && c);
         // 30 : 60 : 90 of 180, scaled to 300 — the proportional share.
         EXPECT_NEAR(a->left(), 0, 0.01f);
         EXPECT_NEAR(b->left(), 50, 0.01f);
         EXPECT_NEAR(c->left(), 150, 0.01f);
         // The children keep their measured size; the COLUMN grew, not the
         // child.
         EXPECT_NEAR(a->width(), 30, 0.01f);
       }},
      {"APublishedPageOfFiveByFiveWithSpans",
       [] {
         // A published page set on <TABLE WIDTH=500 CELLSPACING=2
         // CELLPADDING=1>, five columns by five rows, with a colspan=2, a
         // colspan=3 rowspan=2 and a rowspan=2 in it. Each cell measures the
         // <img> in it plus 18 px for every <br> above the image, which is what
         // the page's quirks-mode line box comes to. The expected numbers are
         // what headless Chrome reports for the same markup at a 640 px
         // viewport.
         struct Cell {
           float width, height;
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
         std::vector<glm::vec2> sizes;
         std::vector<CellSpan> spans;
         for (const Cell& cell : cells) {
           sizes.push_back({cell.width, cell.height});
           spans.push_back(cell.at);
         }

         const Table table{
             .columns = 5, .rows = 5, .width = 500, .spacing = 2, .padding = 1};
         const Table::Grid grid = table.solve(given({500, 435}, sizes, spans));

         // The surplus lands on fractional pixels, so the columns agree to a
         // tenth; the rows are whole content heights and agree exactly.
         const float browserColumns[5] = {71.42f, 97.70f, 122.33f, 78.95f,
                                          107.59f};
         ASSERT_EQ(grid.columnWidths.size(), 5u);
         for (size_t column = 0; column < 5; ++column)
           EXPECT_NEAR(grid.columnWidths[column], browserColumns[column], 0.15f)
               << "column " << column;
         const float browserRows[5] = {0, 113, 88, 73, 139};
         ASSERT_EQ(grid.rowHeights.size(), 5u);
         for (size_t row = 0; row < 5; ++row)
           EXPECT_NEAR(grid.rowHeights[row], browserRows[row], 0.01f)
               << "row " << row;
         // Row 3 is the one the logotype's rowspan pays for: 57 of content and
         // the whole 16 of the span's deficit, which is the asymmetry.
         EXPECT_NEAR(grid.rowHeights[3], 73, 0.01f);
       }},
      {"AColumnNothingFillsCollapses",
       [] {
         // An empty <TD> is a real cell with nothing in it. Its
         // column takes no width and no share of the surplus, and the
         // columns beside it resolve as though it were not there.
         expectSolved({.name = "",
                       .table = Table{.columns = 3, .width = 300},
                       .container = {300, 100},
                       .sizes = {{50, 20}, {0, 0}, {100, 20}},
                       .widths = {100, 0, 200},
                       .originColumn = 2,
                       .origin = 100});
       }},
      {"AnEmptyTableResolvesItsTracksAndPlacesNothing",
       [] {
         // A table with no children at all resolves its empty tracks and
         // places nothing.
         const Table table{.columns = 3, .width = 300};
         const Table::Grid nothing = table.solve(given({300, 100}, {}));
         EXPECT_EQ(nothing.columnWidths.size(), 3u);
         Arrangement empty{.box = geometry::path::Rect::of({0, 0}, {300, 100})};
         table.arrange(empty);
         EXPECT_TRUE(empty.children.empty());
       }},
  });
}

// A colspan's deficit is shared across the columns it covers; a rowspan's
// lands on the last row it covers.
TEST(ComposeTable, ASpanTopsUpColumnsAndRowsDifferently) {
  // The asymmetry that is the browsers' and not a slip: a colspan's
  // deficit is shared across the columns it covers, a rowspan's lands
  // entirely on the LAST row it covers.
  Host host;
  Table table{.columns = 2, .rows = 2, .width = 100};
  host.composer.render(box().children(
      {layout(table)
           .width(200)
           .height(200)
           .children(
               {box().key("wide").width(100).height(10).gridCells(0, 0, 2, 1)})
           .children(
               {box().key("tall").width(10).height(100).gridCells(0, 1, 1, 2)})
           .children(
               {box().key("small").width(10).height(10).gridCells(1, 1)})}));
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

// A child that claims no cell takes the next cell no declared child
// claimed, and sits in its cell as its alignment says.
TEST(ComposeTable, WhatNoChildClaimedFlowsAndAlignsInsideItsCell) {
  // A child that says nothing takes the next cell NO declared child
  // claimed — never one already spoken for, which is the failure of a
  // scheme that counts its flow from zero.
  Host host;
  Table table{.columns = 2, .width = 200};
  host.composer.render(box().children(
      {layout(table)
           .width(200)
           .height(100)
           .children({box().key("pinned").width(20).height(20).gridCells(1, 0)})
           .children({box().key("flowed").width(20).height(20)})
           .children({box()
                          .key("right")
                          .width(20)
                          .height(20)
                          .gridCells(1, 1)
                          .gridCellAlign(Align::End, Align::Start)})}));
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

// Given less room than they want, the columns each give up the same
// fraction of what they can; narrower still, they stand at what they need.
TEST(ComposeTable, TooNarrowAndEveryColumnGivesUpTheSameFractionOfItsRoom) {
  // Three columns wanting 200 between them: each falls the same fraction of
  // the way from what it wants to what it needs, and the one that needs
  // what it wants does not move at all.
  const SolveRow rows[] = {
      {.name = "ThirtyShortOfWhatTheyWant",
       .table = Table{.width = 170},
       .container = {170, 100},
       .sizes = {{100, 20}, {60, 20}, {40, 20}},
       .minima = {{40, 20}, {60, 20}, {40, 20}},
       // 40 + half of the 60 it can give; the other two have nothing to give.
       .widths = {70, 60, 40}},
      {.name = "NarrowerThanTheContentCanBeSet",
       .table = Table{.width = 60},
       .container = {60, 100},
       .sizes = {{100, 20}, {60, 20}, {40, 20}},
       .minima = {{40, 20}, {60, 20}, {40, 20}},
       // They stand at what they need and the table overflows rather than
       // dropping content.
       .widths = {40, 60, 40}},
  };
  sigil::compose::test::forEachRow(rows, expectSolved);
}

// `Fill` shares the surplus and `Shrink` stops at the content.
TEST(ComposeTable, AShrinkToFitTableStopsAtWhatIsInIt) {
  // The same three columns in the same 300-wide container, the one with a
  // width in its markup and the one without.
  const std::vector<glm::vec2> sizes{{30, 20}, {60, 20}, {90, 20}};
  const std::vector<CellSpan> spans{claims(0, 0), claims(1, 0), claims(2, 0)};
  const Table shrink{.fit = Table::ColumnSizing::Shrink};
  const Table::Grid tight = shrink.solve(given({300, 100}, sizes, spans));
  EXPECT_NEAR(tight.columnWidths[0], 30, 0.01f);
  EXPECT_NEAR(tight.columnWidths[2], 90, 0.01f);
  EXPECT_NEAR(tight.columnX[2], 90, 0.01f) << "packed at the start";

  const Table fill{.fit = Table::ColumnSizing::Fill};
  const Table::Grid spread = fill.solve(given({300, 100}, sizes, spans));
  EXPECT_NEAR(spread.columnWidths[0], 50, 0.01f);
  EXPECT_NEAR(spread.columnWidths[2], 150, 0.01f);

  // Shrink says nothing about a table with too little room: the columns
  // fall toward what they need either way.
  const Table::Grid pinched = shrink.solve(
      given({120, 100}, sizes, spans,
            {glm::vec2{30, 20}, glm::vec2{30, 20}, glm::vec2{30, 20}}));
  EXPECT_LT(pinched.columnWidths[2], 90.0f);
  EXPECT_GE(pinched.columnWidths[2], 30.0f);
}

// A declared width holds its column out of the division, a percentage of
// the room the columns divide, and content that will not fit widens it; the
// other columns divide what is left.
TEST(ComposeTable, ADeclaredWidthHoldsItsColumnOutOfTheDivision) {
  const SolveRow rows[] = {
      // <COL WIDTH=50> on the middle column of three: it neither takes a
      // share of the 150 left over nor gives one up.
      {.name = "APixelColumn",
       .table = Table{.width = 200, .declaredWidths = {0, 50, 0}},
       .container = {200, 100},
       .sizes = {{30, 20}, {20, 20}, {60, 20}},
       .widths = {50, 50, 100},
       .originColumn = 1,
       .origin = 50},
      // No column is narrower than the narrowest thing in it.
      {.name = "APixelColumnTheContentWidens",
       .table = Table{.width = 200, .declaredWidths = {10}},
       .container = {200, 100},
       .sizes = {{40, 20}, {20, 20}},
       .widths = {40, 160}},
      // <COL WIDTH="30%"> in a <TABLE WIDTH=500>: 150 whatever is in it, and
      // the two columns the markup left alone divide the 350 left by the
      // auto rule, 30 and 90 of the 120 they want.
      {.name = "APercentageColumn",
       .table = Table{.width = 500, .declaredWidths = {percent(30), {}, {}}},
       .container = {500, 100},
       .sizes = {{40, 20}, {30, 20}, {90, 20}},
       .widths = {150, 87.5f, 262.5f},
       .originColumn = 1,
       .origin = 150},
      // The share is of the ROOM the columns divide and not of the table's
      // outer width: 500 less a 2 px gap at four places and 1 px of padding
      // either side of three columns is 486, and 30 % of that is 145.8.
      {.name = "APercentageOfTheRoomInsideSpacingAndPadding",
       .table = Table{.width = 500,
                      .spacing = 2,
                      .padding = 1,
                      .declaredWidths = {percent(30)}},
       .container = {500, 100},
       .sizes = {{40, 20}, {30, 20}, {90, 20}},
       .widths = {145.8f, 85.05f, 255.15f}},
      // A quarter and a half of a 400-wide table stand out of the division,
      // and the last column has the remaining 100 to itself.
      {.name = "TwoPercentagesAndTheRestTakesWhatIsLeft",
       .table = Table{.width = 400,
                      .declaredWidths = {percent(25), percent(50), {}}},
       .container = {400, 100},
       .sizes = {{20, 20}, {20, 20}, {40, 20}},
       .widths = {100, 200, 100},
       .originColumn = 2,
       .origin = 300},
      // 10 % of 500 is 50 and the cell in the column is 80 wide: it resolves
      // 80, the rule a stated pixel width falls under, and the column beside
      // it takes the whole 420 that is left.
      {.name = "APercentageTheContentWidens",
       .table = Table{.width = 500, .declaredWidths = {percent(10), {}}},
       .container = {500, 100},
       .sizes = {{80, 20}, {60, 20}},
       .widths = {80, 420}},
  };
  sigil::compose::test::forEachRow(rows, expectSolved);
}
