/** @file
 * The library's one door to Skia, held to what it states: a face and a
 * Skia typeface are the same typeface shared, a face style and a Skia font
 * style the same three numbers, and a flow shape made from a Skia path
 * the shape its outline makes.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPath.h>
#include <include/core/SkTypeface.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilweave/layout/Flow.h>

#include <memory>

#include "sigilweave/advanced/Skia.h"
#include <Fonts.h>

using namespace sigil::weave;

TEST(SkiaDoor, ATypefaceCrossesIntoAFaceAndBackAsTheSameTypeface) {
  const sk_sp<SkTypeface> typeface = sigil::test::instrument::sans();
  ASSERT_TRUE(typeface);
  const Face face = fromSk(typeface);
  EXPECT_TRUE(face);
  EXPECT_EQ(face.identity(), typeface.get());
  EXPECT_EQ(toSk(face).get(), typeface.get());
  EXPECT_EQ(borrowSk(face), typeface.get());
  // The conversions the door lets stand in for a spelled one.
  const Face converted = typeface;
  const sk_sp<SkTypeface> back = converted;
  EXPECT_EQ(back.get(), typeface.get());
  EXPECT_EQ(converted, face);
}

TEST(SkiaDoor, AFaceSharesItsTypefaceRatherThanCopyingIt) {
  sk_sp<SkTypeface> typeface = sigil::test::instrument::sans();
  ASSERT_TRUE(typeface);
  const bool uniqueBefore = typeface->unique();
  {
    const Face face = fromSk(typeface);
    const Face copy = face;  // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_EQ(copy, face);
    EXPECT_FALSE(typeface->unique());
  }
  EXPECT_EQ(typeface->unique(), uniqueBefore)
      << "every reference a face took is given back when it goes";
}

TEST(SkiaDoor, NoTypefaceIsNoFace) {
  const Face face = fromSk(nullptr);
  EXPECT_FALSE(face);
  EXPECT_EQ(face, nullptr);
  EXPECT_EQ(toSk(face), nullptr);
  EXPECT_EQ(toSk(Face{}), nullptr);
  EXPECT_EQ(Face{}.familyName(), "");
}

TEST(SkiaDoor, AFaceReadsItsFamilyAndStyleAsItsTypefaceStatesThem) {
  const sk_sp<SkTypeface> typeface = sigil::test::instrument::sans();
  ASSERT_TRUE(typeface);
  const Face face = fromSk(typeface);
  SkString family;
  typeface->getFamilyName(&family);
  EXPECT_EQ(face.familyName(), family.c_str());
  EXPECT_EQ(toSk(face.style()), typeface->fontStyle());
}

TEST(SkiaDoor, AFaceStyleCrossesIntoSkiaAndBackUnchanged) {
  for (const FaceSlant slant :
       {FaceSlant::Upright, FaceSlant::Italic, FaceSlant::Oblique})
    for (const int weight : {100, 400, 650, 900})
      for (const int width : {1, 5, 9}) {
        const FaceStyle style{weight, width, slant};
        EXPECT_EQ(fromSk(toSk(style)), style);
      }
  EXPECT_EQ(toSk(FaceStyle{}), SkFontStyle::Normal());
  EXPECT_EQ(toSk(FaceStyle{.slant = FaceSlant::Italic}), SkFontStyle::Italic());
  EXPECT_EQ(fromSk(SkFontStyle::Bold()), (FaceStyle{.weight = 700}));
}

TEST(SkiaDoor, AFlowShapeFromASkiaPathIsTheShapeItsOutlineMakes) {
  const SkPath path = SkPath::Circle(120, 80, 50);
  const std::shared_ptr<FlowShape> fromSkia = flowshape::path(path);
  const std::shared_ptr<FlowShape> fromOutline =
      flowshape::path(sigil::geometry::path::fromSk(path));
  EXPECT_EQ(fromSkia->bounds(), fromOutline->bounds());
  std::vector<Span> skiaSpans, outlineSpans;
  fromSkia->bandSpans(FlowAxis::kLines, {70, 90}, 4, skiaSpans);
  fromOutline->bandSpans(FlowAxis::kLines, {70, 90}, 4, outlineSpans);
  ASSERT_EQ(skiaSpans.size(), outlineSpans.size());
  for (size_t index = 0; index < skiaSpans.size(); ++index) {
    EXPECT_EQ(skiaSpans[index].start, outlineSpans[index].start);
    EXPECT_EQ(skiaSpans[index].end, outlineSpans[index].end);
  }
}
