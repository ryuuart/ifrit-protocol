/** @file
 * The escape hatch to Skia: every conversion it spells comes back to the
 * value it started from, and every Skia form of an operator answers the
 * path its outline form converts to — so a caller crossing at the
 * boundary draws exactly what a caller staying on outlines would.
 */

#include <gtest/gtest.h>
#include <include/core/SkPathBuilder.h>

#include <vector>

#include "sigilgeometry/advanced/Skia.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Operations.h"
#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Segments.h"
#include "sigilgeometry/path/Symmetry.h"
#include "sigilgeometry/path/Transform.h"

namespace {

using namespace sigil::geometry::path;

SkPath openWave() {
  SkPathBuilder builder;
  builder.moveTo(0, 0);
  builder.cubicTo(40, -30, 80, 30, 120, 0);
  builder.lineTo(160, 20);
  return builder.detach();
}

Outline ring() {
  return Outline::svg("M 0 0 L 100 0 L 100 60 L 0 60 Z M 20 20 L 20 40 "
                      "L 80 40 L 80 20 Z")
      .withFillRule(FillRule::EvenOdd);
}

TEST(Skia, AnOutlineComesBackFromItsPathUnchanged) {
  const Outline outline = ring();
  EXPECT_EQ(fromSk(toSk(outline)), outline);
  EXPECT_EQ(fromSk(toSk(outline)).fillRule(), FillRule::EvenOdd);
  EXPECT_EQ(toSk(fromSk(openWave())), openWave());
}

TEST(Skia, ARunOfOutlinesComesBackInOrder) {
  const std::vector<Outline> outlines{ring(), fromSk(openWave()), Outline()};
  EXPECT_EQ(fromSk(toSk(outlines)), outlines);
}

TEST(Skia, ARectAPointAndATransformComeBackUnchanged) {
  const Rect rect{{-3.5f, 2}, {40, 90.25f}};
  EXPECT_EQ(fromSk(toSk(rect)), rect);
  EXPECT_EQ(fromSk(toSk(glm::vec2(7.5f, -2))), glm::vec2(7.5f, -2));
  EXPECT_EQ(fromSk(toSkSize(glm::vec2(12, 30))), glm::vec2(12, 30));
  const Transform turned =
      Transform::translate({10, -4}) * Transform::rotate(30.0f);
  EXPECT_EQ(fromSk(toSk(turned)).matrix, turned.matrix);
  EXPECT_EQ(toSk(FillRule::EvenOdd), SkPathFillType::kEvenOdd);
  EXPECT_EQ(toSk(FillRule::NonZero), SkPathFillType::kWinding);
}

TEST(Skia, AContourReadOnAPathIsTheContourReadOnItsOutline) {
  const Outline outline = fromSk(openWave());
  const std::vector<Contour> fromPath = contoursOf(openWave());
  const std::vector<Contour> fromOutline = Contour::of(outline);
  ASSERT_EQ(fromPath.size(), fromOutline.size());
  EXPECT_FLOAT_EQ(lengthOf(openWave()), Contour::lengthOf(outline));
  const Contour& contour = fromOutline.front();
  EXPECT_EQ(fromSk(segmentOf(contour, 20, 90)), contour.segment(20, 90));
  const auto [before, after] = splitOf(contour, 50);
  EXPECT_EQ(fromSk(before), contour.split(50).first);
  EXPECT_EQ(fromSk(after), contour.split(50).second);
}

TEST(Skia, AnOperatorOnAPathAnswersWhatItAnswersOnTheOutline) {
  const Outline outline = ring();
  const SkPath path = toSk(outline);
  EXPECT_EQ(fromSk(operations::offset(path, 4)),
            operations::offset(outline, 4));
  EXPECT_EQ(fromSk(operations::unite(path, toSk(Outline::rectangle(
                                              {{50, 30}, {140, 90}})))),
            operations::unite(outline,
                              Outline::rectangle({{50, 30}, {140, 90}})));
  EXPECT_EQ(fromSk(operations::distort(operations::Zigzag{}, openWave())),
            operations::Zigzag{}.apply(fromSk(openWave())));
  EXPECT_EQ(fromSk(reverse(path)), reverse(outline));
  EXPECT_EQ(segments(path), segments(outline));
  const Symmetry symmetry{.order = 3};
  EXPECT_EQ(fromSk(copies(symmetry, path)), copies(symmetry, outline));
}

}  // namespace
