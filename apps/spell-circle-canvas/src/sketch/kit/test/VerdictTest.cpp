/** @file
 * A run of checks drawn: which rows stand, and the colour a verdict is
 * painted in coming from the sheet.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/check/Check.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <utility>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
namespace measure = sigil::measure;
using compose::Element;
using sigil::sketch::kit::test::Drawn;
using sigil::sketch::kit::test::kTall;
using sigil::sketch::kit::test::kWide;
using sigil::sketch::kit::test::sameDrawing;

/** @p content under the house sheet, as a page would stand it. */
Element dressed(Element content) {
  return compose::box()
      .applyStyleSheet(kit::houseTheme().styleSheet())
      .children({std::move(content)});
}

/** How many pixels are within a small distance of @p colour. */
int pixelsNear(Element tree, sigil::material::Color colour) {
  SkBitmap drawn = Drawn(std::move(tree)).pixels();
  int count = 0;
  for (int y = 0; y < kTall; ++y)
    for (int x = 0; x < kWide; ++x) {
      const sigil::material::Color pixel = drawn.getColor4f(x, y);
      if (std::abs(pixel.r - colour.r) < 0.03f &&
          std::abs(pixel.g - colour.g) < 0.03f &&
          std::abs(pixel.b - colour.b) < 0.03f)
        ++count;
    }
  return count;
}

measure::CheckTable proof() {
  measure::CheckTable table;
  table.add(measure::heading("THE RULE"))
      .add(measure::check("pieces", 12, 12))
      .add(measure::reading("residual", 5.6e-16))
      .add(measure::check("closing gap", 0.0, 0.25, 0.01))
      .add(measure::finding(measure::check("legend holds", true)));
  return table;
}

TEST(SketchKitVerdict, AFailingCheckIsPaintedInTheFailColour) {
  const kit::Palette& palette = kit::houseTheme().palette;
  EXPECT_GT(pixelsNear(dressed(kit::verdict(proof())), palette.fail), 0);
  measure::CheckTable held;
  held.add(measure::check("pieces", 12, 12));
  EXPECT_EQ(pixelsNear(dressed(kit::verdict(held)), palette.fail), 0);
  EXPECT_GT(pixelsNear(dressed(kit::verdict(held)), palette.pass), 0);
}

TEST(SketchKitVerdict, TheFailuresPanelDrawsOnlyTheChecksThatFailed) {
  measure::CheckTable failed;
  failed.add(measure::check("closing gap", 0.0, 0.25, 0.01));
  EXPECT_TRUE(sameDrawing(
      dressed(kit::verdict(proof(), {.rows = kit::VerdictRows::Failures})),
      dressed(kit::verdict(failed))));
}

TEST(SketchKitVerdict, TheJudgedRowsLeaveOutHeadingsAndReadings) {
  measure::CheckTable judged;
  judged.add(measure::check("pieces", 12, 12))
      .add(measure::check("closing gap", 0.0, 0.25, 0.01))
      .add(measure::finding(measure::check("legend holds", true)));
  EXPECT_TRUE(sameDrawing(
      dressed(kit::verdict(proof(), {.rows = kit::VerdictRows::Judged})),
      dressed(kit::verdict(judged))));
  EXPECT_FALSE(sameDrawing(dressed(kit::verdict(proof())),
                           dressed(kit::verdict(judged))));
}

/** The colour is the sheet's: a rule over `.checkFail` repaints the
 *  verdict with no prop changed. */
TEST(SketchKitVerdict, ASheetRuleRestylesTheVerdict) {
  const sigil::material::Color cyan{0, 1, 1, 1};
  measure::CheckTable failed;
  failed.add(measure::check("closing gap", 0.0, 0.25, 0.01));
  Element restyled =
      compose::box()
          .applyStyleSheet(kit::houseTheme().styleSheet() +
                           compose::StyleSheet{compose::rule(".checkFail")
                                                   .font({.color = cyan})})
          .children({kit::verdict(failed, {.swatches = false})});
  EXPECT_GT(pixelsNear(std::move(restyled), cyan), 0);
}

TEST(SketchKitVerdict, TheSummaryCountsTheClaims) {
  EXPECT_FALSE(sameDrawing(dressed(kit::verdict(proof())),
                           dressed(kit::verdict(proof(), {.summary = true}))));
}

}  // namespace
