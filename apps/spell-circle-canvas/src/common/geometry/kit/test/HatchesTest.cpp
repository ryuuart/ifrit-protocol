/** @file
 * The hatch: the lattice and the offset behind one door that takes an
 * outline and gives one back.
 */

#include <gtest/gtest.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/kit/Hatches.h>
#include <sigilgeometry/path/Segments.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <vector>

using namespace sigil::geometry;

namespace {

size_t markCount(const SkPath& path) {
  return path::segments(path).size();
}

TEST(Hatch, FillsAShapeWithLinesInsideIt) {
  const SkPath square = SkPath::Rect(SkRect::MakeWH(100, 100));
  const SkPath hatched = shapes::hatchOutline(square, {.spacing = 10.0f});
  EXPECT_EQ(markCount(hatched), 10u);
  const SkRect bounds = hatched.computeTightBounds();
  EXPECT_GE(bounds.left(), -1e-3f);
  EXPECT_LE(bounds.right(), 100 + 1e-3f);
  EXPECT_GE(bounds.top(), -1e-3f);
  EXPECT_LE(bounds.bottom(), 100 + 1e-3f);
}

// A ring's hole is a hole: the even-odd rule the lattice reads is the
// rule the shape is filled by, so no mark crosses the middle.
TEST(Hatch, LeavesTheHoleOfARingEmpty) {
  SkPathBuilder b;
  b.addCircle(0, 0, 100);
  b.addCircle(0, 0, 50);
  const SkPath ring = b.detach();
  for (const path::SegmentContour& mark :
       path::segments(shapes::hatchOutline(ring, {.spacing = 8.0f}))) {
    ASSERT_EQ(mark.segments.size(), 1u);
    const glm::vec2 middle =
        (mark.segments[0].start() + mark.segments[0].end()) * 0.5f;
    EXPECT_GE(glm::length(middle), 49.0f);
  }
}

// The ladder is measured from the origin, so a shape that moves keeps
// its lines where they were instead of dragging them along.
TEST(Hatch, AnOriginHoldsTheLinesStillWhileTheShapeMoves) {
  const SkPath here = SkPath::Rect(SkRect::MakeXYWH(0, 0, 100, 100));
  const SkPath there = SkPath::Rect(SkRect::MakeXYWH(0, 3, 100, 100));
  const shapes::Hatch pinned{.spacing = 10.0f, .origin = glm::vec2{0, 0}};
  const std::vector<path::SegmentContour> a =
      path::segments(shapes::hatchOutline(here, pinned));
  const std::vector<path::SegmentContour> b =
      path::segments(shapes::hatchOutline(there, pinned));
  ASSERT_FALSE(a.empty());
  ASSERT_FALSE(b.empty());
  // Every line of the moved shape stands on a rung of the same ladder.
  for (const path::SegmentContour& mark : b)
    EXPECT_NEAR(std::fmod(mark.segments[0].start().y + 1000.0f, 10.0f), 0.0f,
                1e-3f);
  EXPECT_NEAR(std::fmod(a.front().segments[0].start().y + 1000.0f, 10.0f),
              0.0f, 1e-3f);
}

TEST(Hatch, AnInsetKeepsTheMarksInsideTheEdge) {
  const SkPath square = SkPath::Rect(SkRect::MakeWH(100, 100));
  const SkPath hatched =
      shapes::hatchOutline(square, {.spacing = 10.0f, .inset = 12.0f});
  const SkRect bounds = hatched.computeTightBounds();
  EXPECT_GE(bounds.left(), 11.0f);
  EXPECT_LE(bounds.right(), 89.0f);
}

}  // namespace
