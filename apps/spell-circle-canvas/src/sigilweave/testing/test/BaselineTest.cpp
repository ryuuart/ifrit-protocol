/** @file
 * A render held against a baseline file: missing until adopted, matched
 * once it is, and differed, resized or unreadable when the file says
 * otherwise, with the render written aside for each refusal; and a plate
 * that says which faces it was drawn in, so a render drawn in other faces
 * than its baseline was adopted on says so.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <sigilweave/layout/Flow.h>
#include <sigilweave/testing/Baseline.h>
#include <sigilweave/testing/Passage.h>
#include <sigilweave/testing/Plate.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "ScratchDir.h"
#include "support/Paragraphs.h"

using namespace sigil::weave;
using namespace sigil::weave::test;
namespace weave = sigil::weave;

namespace {

/// @p text in the instrument face, rendered alone on a white plate.
sk_sp<SkImage> rendered(std::u8string_view text, SkISize size = {200, 40}) {
  BlockFlow flow(SkRect::MakeXYWH(4, 4, 190, 30));
  return weave::testing::render(
      weave::testing::lay(sigil::test::fonts(), makeParagraph(text), flow),
      size);
}

SkPixmap pixelsOf(const sk_sp<SkImage>& image) {
  SkPixmap pixmap;
  image->peekPixels(&pixmap);
  return pixmap;
}

}  // namespace

TEST(WeaveBaseline, ARenderIsMissingThenAdoptedThenMatched) {
  const sigil::test::ScratchDir scratch("weave_baseline_adopted");
  const std::filesystem::path baseline = scratch.path / "plate.png";
  const sk_sp<SkImage> image = rendered(u8"baseline");
  using weave::testing::BaselineAction;
  using weave::testing::BaselineOutcome;

  EXPECT_EQ(
      weave::testing::compareToBaseline(pixelsOf(image), baseline).outcome,
      BaselineOutcome::kMissing);
  const weave::testing::BaselineComparison adopted =
      weave::testing::compareToBaseline(pixelsOf(image), baseline,
                                        BaselineAction::kAdopt);
  EXPECT_EQ(adopted.outcome, BaselineOutcome::kAdopted);
  EXPECT_TRUE(adopted.passed());
  const weave::testing::BaselineComparison matched =
      weave::testing::compareToBaseline(pixelsOf(image), baseline);
  EXPECT_EQ(matched.outcome, BaselineOutcome::kMatched);
  EXPECT_TRUE(matched.difference.identical());
}

TEST(WeaveBaseline, AMovedRenderDiffersAndIsWrittenAside) {
  const sigil::test::ScratchDir scratch("weave_baseline_differed");
  const std::filesystem::path baseline = scratch.path / "plate.png";
  const std::filesystem::path rejected = scratch.path / "renders/plate.png";
  using weave::testing::BaselineAction;
  using weave::testing::BaselineOutcome;
  ASSERT_TRUE(weave::testing::compareToBaseline(
                  pixelsOf(rendered(u8"one")), baseline, BaselineAction::kAdopt)
                  .passed());

  // The instrument's letters are one box each, so a longer word is what
  // moves the picture.
  const weave::testing::BaselineComparison moved =
      weave::testing::compareToBaseline(pixelsOf(rendered(u8"three")), baseline,
                                        BaselineAction::kJudge, rejected);
  EXPECT_EQ(moved.outcome, BaselineOutcome::kDiffered);
  EXPECT_FALSE(moved.passed());
  EXPECT_GT(moved.difference.differingPixels, 0);
  EXPECT_EQ(moved.rejected, rejected);
  EXPECT_TRUE(std::filesystem::exists(rejected));
  EXPECT_NE(weave::testing::describe(moved).find("differed"),
            std::string::npos);

  const weave::testing::BaselineComparison resized =
      weave::testing::compareToBaseline(pixelsOf(rendered(u8"one", {100, 40})),
                                        baseline);
  EXPECT_EQ(resized.outcome, BaselineOutcome::kResized);
  EXPECT_EQ(resized.baselineSize, (SkISize{200, 40}));

  scratch.write("junk.png", "not an image");
  EXPECT_EQ(weave::testing::compareToBaseline(pixelsOf(rendered(u8"one")),
                                              scratch.path / "junk.png")
                .outcome,
            BaselineOutcome::kUnreadable);
}

namespace {

/// "AVn" in @p face at 24 px, drawn on a white plate.
weave::testing::Plate plateIn(const sk_sp<SkTypeface>& face) {
  TextStyle style = basicStyle(24.0f);
  style.shaping.typeface = face;
  BlockFlow flow(SkRect::MakeXYWH(4, 4, 190, 30));
  weave::testing::Plate plate({200, 40}, SK_ColorWHITE);
  plate.draw(weave::testing::lay(sigil::test::fonts(),
                                 paragraphIn(u8"AVn", style), flow));
  return plate;
}

}  // namespace

TEST(WeaveBaseline, APlateListsTheFacesItWasDrawnIn) {
  const weave::testing::Plate plate = plateIn(sigil::test::instrument::sans());
  const std::vector<std::string> faces = plate.faces();
  ASSERT_EQ(faces.size(), 1u);
  EXPECT_NE(faces.front().find("revision"), std::string::npos);
  EXPECT_EQ(plate.faces(), faces) << "asking twice answers the same faces";
  EXPECT_TRUE(weave::testing::Plate({0, 0}, SK_ColorWHITE).faces().empty());
}

TEST(WeaveBaseline, ARenderInOtherFacesSaysTheFacesChanged) {
  const sigil::test::ScratchDir scratch("weave_baseline_faces");
  const std::filesystem::path baseline = scratch.path / "plate.png";
  using weave::testing::BaselineAction;
  using weave::testing::BaselineOutcome;
  const weave::testing::Plate adopted =
      plateIn(sigil::test::instrument::sans());
  ASSERT_TRUE(weave::testing::compareToBaseline(adopted, baseline,
                                                BaselineAction::kAdopt)
                  .passed());
  EXPECT_TRUE(std::filesystem::exists(weave::testing::facesBeside(baseline)));
  EXPECT_EQ(weave::testing::compareToBaseline(adopted, baseline).outcome,
            BaselineOutcome::kMatched);

  const weave::testing::BaselineComparison changed =
      weave::testing::compareToBaseline(
          plateIn(sigil::test::instrument::optical()), baseline);
  EXPECT_EQ(changed.outcome, BaselineOutcome::kFacesChanged);
  EXPECT_FALSE(changed.passed());
  EXPECT_EQ(changed.facesGone, adopted.faces());
  ASSERT_EQ(changed.facesNew.size(), 1u);
  EXPECT_NE(weave::testing::describe(changed).find("faces changed"),
            std::string::npos);
}

TEST(WeaveBaseline, APlateWithNoRoomReadsAsResizedRatherThanCrashing) {
  const sigil::test::ScratchDir scratch("weave_baseline_empty");
  const std::filesystem::path baseline = scratch.path / "plate.png";
  ASSERT_TRUE(weave::testing::compareToBaseline(
                  plateIn(sigil::test::instrument::sans()), baseline,
                  weave::testing::BaselineAction::kAdopt)
                  .passed());
  BlockFlow flow(SkRect::MakeWH(100, 40));
  weave::testing::Plate empty({0, 0}, SK_ColorWHITE);
  empty.draw(
      weave::testing::lay(sigil::test::fonts(), makeParagraph(u8"one"), flow));
  EXPECT_TRUE(empty.size().isEmpty());
  EXPECT_EQ(weave::testing::compareToBaseline(empty.pixels(), baseline).outcome,
            weave::testing::BaselineOutcome::kResized);
}
