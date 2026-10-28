// The shelf: boxes laid left to right, wrapping at a width, and what the
// packing guarantees a caller — every box inside the sheet, none of them
// overlapping, and the rectangles in the order the boxes arrived.

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

namespace {

/** Every pair of non-empty cells, tested for a shared pixel. */
bool anyOverlap(const std::vector<SkRect>& cells) {
  for (size_t index = 0; index < cells.size(); ++index)
    for (size_t other = index + 1; other < cells.size(); ++other)
      if (!cells[index].isEmpty() && !cells[other].isEmpty() &&
          SkRect::Intersects(cells[index], cells[other]))
        return true;
  return false;
}

}  // namespace

// Boxes lie left to right on shelves as deep as their deepest box, wrapping
// at the width, every cell inside the sheet, none overlapping, and the cells
// parallel to the boxes given.
TEST(ComposeShelf, ShelvesPackBoxesLeftToRightAsDeepAsTheirDeepestBox) {
  sigil::compose::test::checkRows({
      {"PacksEveryBoxInsideTheSheetWithNoneOverlapping",
       [] {
         // Sizes that do not divide the width evenly, so shelves break at
         // different columns and a row is never a tidy multiple.
         std::vector<SkSize> boxes;
         for (int index = 0; index < 64; ++index)
           boxes.push_back({17.0f + (float)(index % 7) * 11.0f,
                            13.0f + (float)(index % 5) * 9.0f});
         const Shelved packed = shelve(boxes, {.maxWidth = 200.0f});

         ASSERT_EQ(packed.cells.size(), boxes.size());
         EXPECT_FALSE(anyOverlap(packed.cells));
         const SkRect sheet = SkRect::MakeWH((float)packed.sheet.width(),
                                             (float)packed.sheet.height());
         for (size_t index = 0; index < boxes.size(); ++index) {
           // Each box keeps the size it was handed in at, at its own index.
           EXPECT_FLOAT_EQ(packed.cells[index].width(), boxes[index].width());
           EXPECT_FLOAT_EQ(packed.cells[index].height(), boxes[index].height());
           EXPECT_TRUE(sheet.contains(packed.cells[index]));
         }
         // The sheet is no wider than it was allowed to be, and it is not one
         // long row: sixty-four boxes of this size do not fit in 200 px.
         EXPECT_LE(packed.sheet.width(), 200);
         EXPECT_GT(packed.sheet.height(), 13);
       }},
      {"AShelfWrapsAtTheWidthAndTheNextOneClearsTheTallestOnIt",
       [] {
         const std::vector<SkSize> boxes = {
             {40, 10}, {40, 30}, {40, 10}, {40, 10}};
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
       }},
      {"ABoxWiderThanTheSheetGetsAShelfRatherThanBeingLost",
       [] {
         const std::vector<SkSize> boxes = {{20, 10}, {300, 10}, {20, 10}};
         const Shelved packed = shelve(boxes, {.maxWidth = 100.0f});
         for (const SkRect& cell : packed.cells) EXPECT_FALSE(cell.isEmpty());
         EXPECT_FALSE(anyOverlap(packed.cells));
         // It took the sheet wider than asked rather than being clipped away,
         // and the wrap did not spin: three boxes, three shelves at most.
         EXPECT_EQ(packed.sheet.width(), 300);
         EXPECT_LE(packed.sheet.height(), 30);
       }},
      {"PaddingIsAGutterOnEverySideAndBetweenNeighbours",
       [] {
         const std::vector<SkSize> boxes = {{10, 10}, {10, 10}};
         const Shelved packed = shelve(boxes, {.padding = 2.0f});
         EXPECT_FLOAT_EQ(packed.cells[0].left(), 2.0f);
         EXPECT_FLOAT_EQ(packed.cells[0].top(), 2.0f);
         EXPECT_FLOAT_EQ(packed.cells[1].left(), 14.0f);  // 2 + 10 + 2
         // 2 + 10 + 2 + 10 + 2 across, 2 + 10 + 2 down.
         EXPECT_EQ(packed.sheet.width(), 26);
         EXPECT_EQ(packed.sheet.height(), 14);
       }},
      {"AnEmptyBoxTakesNoRoomAndNothingIsPackedFromNothing",
       [] {
         const std::vector<SkSize> boxes = {{10, 10}, {0, 0}, {10, 10}};
         const Shelved packed = shelve(boxes);
         EXPECT_TRUE(packed.cells[1].isEmpty());
         EXPECT_FLOAT_EQ(packed.cells[2].left(), 10.0f);  // the empty one did
                                                          // not advance the pen
         const Shelved nothing = shelve({});
         EXPECT_TRUE(nothing.cells.empty());
         EXPECT_TRUE(nothing.sheet.isEmpty());
       }},
  });
}
