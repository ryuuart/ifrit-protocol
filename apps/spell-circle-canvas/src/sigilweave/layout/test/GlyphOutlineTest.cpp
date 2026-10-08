/** @file
 * A string as glyph contours, stood where it was asked to stand: its
 * bounds centred on the point, or its baseline begun there.
 */

#include <gtest/gtest.h>
#include <sigilweave/layout/GlyphOutline.h>

#include "sigilweave/advanced/Skia.h"
#include "support/LayoutSupport.h"

using namespace sigil::weave;

TEST(GlyphOutline, OneGlyphIsCentredWhereItWasAsked) {
  const auto face = sigil::test::instrument::sans();
  ASSERT_TRUE(face);
  const auto outline =
      glyphOutline(sigil::test::fonts(), u8"H",
                   {.face = face, .size = 100, .at = {240, 130}});
  ASSERT_FALSE(outline.empty());
  const auto bounds = outline.bounds();
  EXPECT_GT(bounds.width(), 0);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_NEAR(bounds.centre().x, 240, 1e-3f);
  EXPECT_NEAR(bounds.centre().y, 130, 1e-3f);
}

TEST(GlyphOutline, ABaselineAnchorBeginsThePenThere) {
  const auto face = sigil::test::instrument::sans();
  ASSERT_TRUE(face);
  const auto outline = glyphOutline(sigil::test::fonts(), u8"H",
                                    {.face = face,
                                     .size = 100,
                                     .at = {40, 300},
                                     .anchor = GlyphOutlineAnchor::Baseline});
  ASSERT_FALSE(outline.empty());
  const auto bounds = outline.bounds();
  // The letter stands on the baseline, inside the advance the pen began.
  EXPECT_GE(bounds.left(), 40 - 1e-3f);
  EXPECT_LE(bounds.right(), 40 + 60 + 1e-3f);
  EXPECT_LE(bounds.bottom(), 300 + 1e-3f);
  EXPECT_LT(bounds.top(), 300);
}
