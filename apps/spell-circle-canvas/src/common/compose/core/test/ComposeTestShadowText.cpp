// CSS's `box-shadow` and `text-shadow` read from their text, against the
// examples the CSS specifications give: each reads to the shadows the
// typed doors build, and text no shadow holds reads as nothing.

#include <gtest/gtest.h>
#include <sigilcompose/core/Shadow.h>
#include <sigilcompose/core/verbs/Font.h>
#include <sigilmaterial/color/Color.h>

#include <optional>
#include <vector>

#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

namespace {

using sigil::compose::ColorTerm;
using sigil::compose::Fill;
using sigil::compose::readShadow;
using sigil::compose::readTextShadow;
using sigil::compose::Shadow;
using sigil::compose::TextShadow;
using sigil::material::readColor;

}  // namespace

TEST(ComposeShadowText, CssShadowTextReadsOntoTheShadowValues) {
  checkRows({
      {"TheSpecificationsBoxShadowsReadAsTheirFields",
       [] {
         // CSS Backgrounds: two shadows, the second inset.
         const std::optional<std::vector<Shadow>> read = readShadow(
             "64px 64px 12px 40px rgba(0,0,0,0.4), "
             "12px 12px 0px 8px rgba(0,0,0,0.4) inset");
         ASSERT_TRUE(read);
         const Fill shade = Fill::color(*readColor("rgba(0,0,0,0.4)"));
         EXPECT_EQ(
             *read,
             (std::vector<Shadow>{
                 {.offset = {64, 64}, .blur = 12, .spread = 40, .color = shade},
                 {.offset = {12, 12},
                  .blur = 0,
                  .spread = 8,
                  .color = shade,
                  .inset = true}}));
       }},
      {"TheColourAndInsetStandBeforeOrAfterTheLengths",
       [] {
         const std::optional<std::vector<Shadow>> after =
             readShadow("0 4px 12px 2px rgba(0 0 0 / .3) inset");
         const std::optional<std::vector<Shadow>> before =
             readShadow("inset rgba(0 0 0 / .3) 0 4px 12px 2px");
         ASSERT_TRUE(after && before);
         EXPECT_EQ(*after, *before);
         ASSERT_EQ(after->size(), 1u);
         EXPECT_TRUE(after->front().inset);
         EXPECT_EQ(after->front().spread, 2.0f);
         // An unnamed colour, and currentcolor, are the ink in force.
         const std::optional<std::vector<Shadow>> unnamed =
             readShadow("3px 3px");
         ASSERT_TRUE(unnamed);
         EXPECT_EQ(unnamed->front().color, Fill::currentInk());
         EXPECT_EQ(*readShadow("3px 3px currentColor"), *unnamed);
         EXPECT_EQ(readShadow("none"), std::vector<Shadow>{});
       }},
      {"TheSpecificationsTextShadowsReadAsTheirFields",
       [] {
         // CSS Text Decoration: three shadows, the colour first, last or
         // unnamed.
         const std::optional<std::vector<TextShadow>> read =
             readTextShadow("3px 3px red, yellow -3px 3px 2px, 3px -3px");
         ASSERT_TRUE(read);
         ASSERT_EQ(read->size(), 3u);
         EXPECT_EQ((*read)[0],
                   (TextShadow{.offset = {3, 3},
                               .color = ColorTerm(*readColor("red"))}));
         EXPECT_EQ((*read)[1],
                   (TextShadow{.offset = {-3, 3},
                               .blur = 2,
                               .color = ColorTerm(*readColor("yellow"))}));
         EXPECT_EQ((*read)[2], (TextShadow{.offset = {3, -3}}));
       }},
      {"TextNoShadowHoldsReadsAsNothing",
       [] {
         for (const char* text :
              {"4px", "1px 2px 3px 4px 5px", "1px 2px -3px", "0.1em 0.1em #333",
               "1px 2px red blue", "1px red 2px", "inset inset 1px 1px",
               "1px 1px nonsense", ""}) {
           SCOPED_TRACE(text);
           EXPECT_FALSE(readShadow(text));
         }
         EXPECT_FALSE(readTextShadow("1px 1px inset"));
         EXPECT_FALSE(readTextShadow("1px 1px 2px 3px"));
       }},
  });
}
