// The shelf: boxes laid left to right, wrapping at a width, and what the
// packing guarantees a caller — every box inside the sheet, none of them
// overlapping, and the rectangles in the order the boxes arrived.

#include "support/CoreTestSupport.h"

namespace {

/** Every pair of non-empty cells, tested for a shared pixel. */
bool anyOverlap(const std::vector<SkRect>& cells) {
  for (size_t i = 0; i < cells.size(); ++i)
    for (size_t j = i + 1; j < cells.size(); ++j)
      if (!cells[i].isEmpty() && !cells[j].isEmpty() &&
          SkRect::Intersects(cells[i], cells[j]))
        return true;
  return false;
}

}  // namespace

TEST(ComposeShelf, PacksEveryBoxInsideTheSheetWithNoneOverlapping) {
  // Sizes that do not divide the width evenly, so shelves break at
  // different columns and a row is never a tidy multiple.
  std::vector<SkSize> boxes;
  for (int i = 0; i < 64; ++i)
    boxes.push_back(
        {17.0f + (float)(i % 7) * 11.0f, 13.0f + (float)(i % 5) * 9.0f});
  const Shelved packed = shelve(boxes, {.maxWidth = 200.0f});

  ASSERT_EQ(packed.cells.size(), boxes.size());
  EXPECT_FALSE(anyOverlap(packed.cells));
  const SkRect sheet =
      SkRect::MakeWH((float)packed.sheet.width(), (float)packed.sheet.height());
  for (size_t i = 0; i < boxes.size(); ++i) {
    // Each box keeps the size it was handed in at, at its own index.
    EXPECT_FLOAT_EQ(packed.cells[i].width(), boxes[i].width());
    EXPECT_FLOAT_EQ(packed.cells[i].height(), boxes[i].height());
    EXPECT_TRUE(sheet.contains(packed.cells[i]));
  }
  // The sheet is no wider than it was allowed to be, and it is not one
  // long row: sixty-four boxes of this size do not fit in 200 px.
  EXPECT_LE(packed.sheet.width(), 200);
  EXPECT_GT(packed.sheet.height(), 13);
}

TEST(ComposeShelf, AShelfWrapsAtTheWidthAndTheNextOneClearsTheTallestOnIt) {
  const std::vector<SkSize> boxes = {{40, 10}, {40, 30}, {40, 10}, {40, 10}};
  const Shelved packed = shelve(boxes, {.maxWidth = 100.0f});
  // Two on the first shelf, then the wrap.
  EXPECT_FLOAT_EQ(packed.cells[0].left(), 0.0f);
  EXPECT_FLOAT_EQ(packed.cells[1].left(), 40.0f);
  EXPECT_FLOAT_EQ(packed.cells[0].top(), 0.0f);
  EXPECT_FLOAT_EQ(packed.cells[1].top(), 0.0f);
  EXPECT_FLOAT_EQ(packed.cells[2].left(), 0.0f);
  // The second shelf clears the TALLEST box on the first, not the last
  // one — a shelf is as deep as its deepest box.
  EXPECT_FLOAT_EQ(packed.cells[2].top(), 30.0f);
  EXPECT_FLOAT_EQ(packed.cells[3].left(), 40.0f);
  EXPECT_EQ(packed.sheet.height(), 40);
}

TEST(ComposeShelf, ABoxWiderThanTheSheetGetsAShelfRatherThanBeingLost) {
  const std::vector<SkSize> boxes = {{20, 10}, {300, 10}, {20, 10}};
  const Shelved packed = shelve(boxes, {.maxWidth = 100.0f});
  for (const SkRect& cell : packed.cells) EXPECT_FALSE(cell.isEmpty());
  EXPECT_FALSE(anyOverlap(packed.cells));
  // It took the sheet wider than asked rather than being clipped away,
  // and the wrap did not spin: three boxes, three shelves at most.
  EXPECT_EQ(packed.sheet.width(), 300);
  EXPECT_LE(packed.sheet.height(), 30);
}

TEST(ComposeShelf, PaddingIsAGutterOnEverySideAndBetweenNeighbours) {
  const std::vector<SkSize> boxes = {{10, 10}, {10, 10}};
  const Shelved packed = shelve(boxes, {.padding = 2.0f});
  EXPECT_FLOAT_EQ(packed.cells[0].left(), 2.0f);
  EXPECT_FLOAT_EQ(packed.cells[0].top(), 2.0f);
  EXPECT_FLOAT_EQ(packed.cells[1].left(), 14.0f);  // 2 + 10 + 2
  // 2 + 10 + 2 + 10 + 2 across, 2 + 10 + 2 down.
  EXPECT_EQ(packed.sheet.width(), 26);
  EXPECT_EQ(packed.sheet.height(), 14);
}

TEST(ComposeShelf, AnEmptyBoxTakesNoRoomAndNothingIsPackedFromNothing) {
  const std::vector<SkSize> boxes = {{10, 10}, {0, 0}, {10, 10}};
  const Shelved packed = shelve(boxes);
  EXPECT_TRUE(packed.cells[1].isEmpty());
  EXPECT_FLOAT_EQ(packed.cells[2].left(), 10.0f);  // the empty one did not
                                                   // advance the pen
  const Shelved nothing = shelve({});
  EXPECT_TRUE(nothing.cells.empty());
  EXPECT_TRUE(nothing.sheet.isEmpty());
}
