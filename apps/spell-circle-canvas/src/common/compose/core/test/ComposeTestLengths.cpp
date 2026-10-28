// The units a length is written in and what each measures against, the
// point that is a fixed count of pixels, the arithmetic that sums them, and
// the one grammar that reads a length written as text.

#include <sigilcompose/core/Layout.h>
#include <sigilweave/style/Length.h>

#include <optional>
#include <string_view>
#include <utility>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

using sigil::compose::test::checkRows;
using sigil::compose::test::forEachRow;

using namespace sigil::weave::literals;

namespace {

/** A page that sets the instrument face at @p size and the property
 *  `gutter` to ten pixels, with one keyed child. */
Element pageWith(Element child, float size) {
  return box()
      .font({.face = sigil::test::instrument::sans(), .size = size})
      .var("gutter", 10_px)
      .children({std::move(child).key("measured")});
}

/** The width the keyed child laid out to. */
float widthOf(Host& host) {
  return require(host.composer.bounds("measured")).width();
}

/** The advance of "0" in the instrument face, as a fraction of its em.
 *  The face gives every figure its own advance and the zero the
 *  narrowest, so a `ch` there is a number no other glyph would give. */
constexpr float kInstrumentZeroEm = 0.46f;

}  // namespace

// Each unit is a fraction or a multiple of one thing: an em of the node's
// own font size (of the parent's, on a type size), a ch of the advance of
// "0" in the face in force, a point of four pixels to every three whatever
// the font, an lh of the line height in force, a property of what it holds,
// a vw of the canvas; and a sum measures each term against its own unit.
// Every row is a width on a 400 by 200 canvas, under a page that sets the
// instrument face at the row's size.
TEST(ComposeLengths, ALengthResolvesAgainstWhatItsUnitMeasures) {
  struct UnitRow {
    const char* name;
    float pageSize;
    Element (*measured)();
    float width;
  };
  const UnitRow units[] = {
      // CSS's `ch`: not the em, not the average character, the figure zero,
      // which is why a column of digits written in it lines up.
      {"AChIsTheAdvanceOfZeroInTheFaceInForce", 100,
       [] { return box().width(2_ch).height(10); },
       2.0f * 100.0f * kInstrumentZeroEm},
      {"AChFollowsTheFontItIsMeasuredAgainstAtFifty", 50,
       [] { return box().width(4_ch).height(10); },
       4.0f * 50.0f * kInstrumentZeroEm},
      {"AChFollowsTheFontItIsMeasuredAgainstAtAHundred", 100,
       [] { return box().width(4_ch).height(10); },
       4.0f * 100.0f * kInstrumentZeroEm},
      // Absolute: the face in force has nothing to say about it.
      {"APointIsFourPixelsToEveryThreeUnderASmallFont", 20,
       [] { return box().width(12_pt).height(10); }, 16.0f},
      {"APointIsFourPixelsToEveryThreeUnderALargeFont", 80,
       [] { return box().width(12_pt).height(10); }, 16.0f},
      // A type size stated in points stays in points once resolved, so an
      // em measured against it is a multiple of the pixels those points come
      // to: twelve points are sixteen pixels, and an em is one of them.
      {"ARelativeLengthUnderASizeInPointsMeasuresThePixels", 20,
       [] {
         return box()
             .fontSize(sigil::weave::pt(12))
             .width(1_em)
             .flexShrink(0)
             .height(10);
       },
       16.0f},
      {"AnEmLengthResolvesAgainstTheNodesOwnFontSizeAtTwenty", 20,
       [] { return box().width(1_em).height(10); }, 20.0f},
      {"AnEmLengthResolvesAgainstTheNodesOwnFontSizeAtForty", 40,
       [] { return box().width(1_em).height(10); }, 40.0f},
      // A type size of 2 em under a 12 px parent is 24 px, which an em on
      // the node itself then reads.
      {"AnEmFontSizeResolvesAgainstTheParentsSize", 12,
       [] { return box().fontSize(2_em).width(1_em).height(10); }, 24.0f},
      {"ASumInSeveralUnitsResolvesWithTheFontInForceAtTwenty", 20,
       [] { return box().width(2 * Dimension(1_em) + 12_px).height(10); },
       2.0f * 20.0f + 12.0f},
      {"ASumInSeveralUnitsResolvesWithTheFontInForceAtThirty", 30,
       [] { return box().width(2 * Dimension(1_em) + 12_px).height(10); },
       2.0f * 30.0f + 12.0f},
      {"ASumReadsACustomPropertyAndTheCanvas", 20,
       [] {
         return box().width(Dimension(var("gutter")) * 3 + 10_vw).height(10);
       },
       3.0f * 10.0f + 0.1f * 400.0f},
      // CSS's lh is the computed line height: under a stated pitch of 30 px
      // one lh is 30 px, and under a number it is that multiple of the type
      // size.
      {"AnLhIsTheLineHeightInForceUnderAStatedPitch", 20,
       [] { return box().lineHeight(Dimension(30)).width(1_lh).height(10); },
       30.0f},
      {"AnLhIsTheLineHeightInForceUnderANumber", 20,
       [] { return box().lineHeight(1.5f).width(2_lh).height(10); }, 60.0f},
  };
  forEachRow(units, [](const UnitRow& unit) {
    Host host(400, 200);
    host.composer.render(pageWith(unit.measured(), unit.pageSize));
    host.frame();
    EXPECT_FLOAT_EQ(widthOf(host), unit.width);
  });
}

TEST(ComposeLengths, OnlyWhatNeedsSomethingElseToResolveIsRelative) {
  // The question the cascade pass asks to decide whether a node's layout
  // has to be written again when the font moves. A point carries
  // everything it needs; a ch does not.
  EXPECT_TRUE(Dimension(1_ch).relative());
  EXPECT_FALSE(Dimension(1_pt).relative());
  EXPECT_FALSE(Dimension(1.0f).relative());
  EXPECT_FALSE(percent(50.0f).relative());
}

// `parseDimension` is the one place text becomes a length. It reads each
// unit, ignores case and the space around the length, reads a custom
// property as `var("gutter")` would, takes a leading sign either way and
// reads calc() text as the arithmetic builds it. Nothing is answered rather
// than a plausible zero for text it does not cover, and for the words for
// infinity and for no number, which name no distance a layout could use.
TEST(ComposeLengths, ALengthWrittenAsTextIsReadByOneGrammar) {
  struct TextRow {
    const char* name;
    std::string_view text;
    std::optional<Dimension> length;
  };
  const TextRow texts[] = {
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "12", Dimension(12.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "12px", Dimension(12.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "50%", percent(50.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "10vw", vw(10.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "10vh", vh(10.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "10vmin", vmin(10.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "10VMAX", vmax(10.0f)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "1.5em",
       Dimension(1.5_em)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "2rem", Dimension(2_rem)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "0.5lh",
       Dimension(0.5_lh)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "3ch", Dimension(3_ch)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "9pt", Dimension(9_pt)},
      {"ALengthWrittenAsTextIsReadInTheUnitItNames", "auto", autoDimension()},
      {"TheGrammarIgnoresCaseAndSurroundingSpace", "  1.5EM ",
       Dimension(1.5_em)},
      {"TheGrammarIgnoresCaseAndSurroundingSpace", "\t-4PX", Dimension(-4.0f)},
      {"TheGrammarIgnoresCaseAndSurroundingSpace", " AUTO ", autoDimension()},
      // CSS's leading dashes name the same property as the bare word.
      {"ALengthWrittenAsTextMayNameACustomProperty", "var(gutter)",
       Dimension(var("gutter"))},
      {"ALengthWrittenAsTextMayNameACustomProperty", "var(--gutter)",
       Dimension(var("gutter"))},
      {"ALengthWrittenAsTextMayNameACustomProperty", "var( gutter )",
       Dimension(var("gutter"))},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "   ", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "wide", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "12pw", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "12cqw", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "12 px", std::nullopt},
      {"TextTheGrammarDoesNotCoverAnswersNothing", "var()", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "inf", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "infinity", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "inf%", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "-inf", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "nan", std::nullopt},
      {"ANumberThatNamesNoDistanceIsNotALength", "nanpx", std::nullopt},
      {"ALeadingPlusIsTheNumbersSign", "+12px", Dimension(12.0f)},
      {"ALeadingPlusIsTheNumbersSign", "+1.5em", Dimension(1.5_em)},
      {"ALeadingPlusIsTheNumbersSign", " +50% ", percent(50.0f)},
      {"ALeadingPlusIsTheNumbersSign", "+", std::nullopt},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(2em + 12px)",
       2 * Dimension(1_em) + 12_px},
      {"CalcTextReadsAsTheArithmeticDoes", "CALC( (1em + 2px) * 2 )",
       (Dimension(1_em) + 2_px) * 2},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(var(--gutter) / 2 + 1ch)",
       Dimension(var("gutter")) / 2 + Dimension(1_ch)},
      // A bare number is pixels.
      {"CalcTextReadsAsTheArithmeticDoes", "calc(3 * 4)", 12_px},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(100% + 12px)",
       100_percent + 12_px},
      // A length times a length, a division by nothing or by a length, and
      // a sum with a gap in it are none of them lengths.
      {"CalcTextReadsAsTheArithmeticDoes", "calc(1em * 2px)", std::nullopt},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(1em / 0)", std::nullopt},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(1em / 1px)", std::nullopt},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(1em +)", std::nullopt},
      {"CalcTextReadsAsTheArithmeticDoes", "calc(auto + 1em)", std::nullopt},
  };
  forEachRow(texts, [](const TextRow& row) {
    EXPECT_EQ(parseDimension(row.text), row.length)
        << "read from \"" << row.text << "\"";
  });
}

TEST(ComposeLengths, ArithmeticOnLengthsIsCssCalc) {
  checkRows({
      {"ArithmeticInOneUnitStaysInThatUnit",
       [] {
         EXPECT_EQ(2 * Dimension(1_em), Dimension(2_em));
         EXPECT_EQ(12_px + 4_px, 16_px);
         EXPECT_EQ(Dimension(3_em) / 3, Dimension(1_em));
         EXPECT_EQ(50_percent * 2, 100_percent);
         EXPECT_EQ(-(10_px), Dimension(-10.0f));
         // A sum that cancels back to one unit is that unit again, so nothing
         // downstream can tell it was ever a sum.
         EXPECT_EQ(Dimension(1_em) + 12_px - 12_px, Dimension(1_em));
         EXPECT_EQ((Dimension(1_em) + 12_px).unit, Dimension::Unit::Calc);
         EXPECT_EQ(Dimension(1_em) + 12_px, 12_px + Dimension(1_em));
       }},
      {"APercentageIsATermOfTheSumAndAutoIsRefused",
       [] {
         // A percentage is kept in the sum for the property that knows its
         // extent; auto is no length, and arithmetic on it is refused and said
         // once.
         EXPECT_EQ((50_percent + Dimension(1_em)).unit, Dimension::Unit::Calc);
         EXPECT_EQ(12_px - 50_percent, -(50_percent - 12_px));
         EXPECT_EQ(50_percent + Dimension(1_em) - Dimension(1_em), 50_percent)
             << "a sum that cancels back to its percentage is that percentage";
         EXPECT_EQ(autoDimension() * 2, autoDimension());
         EXPECT_EQ(autoDimension() + 50_percent, autoDimension());
         // A division by zero is no length, refused as the calc() text refuses
         // it.
         EXPECT_EQ(Dimension(1_em) / 0.0f, autoDimension());
         EXPECT_EQ((Dimension(1_em) + 12_px) / 0.0f, autoDimension());
         // A zero of pixels adds nothing, so it may stand beside one.
         EXPECT_EQ(50_percent + 0_px, 50_percent);
         EXPECT_EQ(50_percent + 25_percent, 75_percent);
       }},
      {"APercentageSumResolvesWhereItsExtentIsKnown",
       [] {
         // A positioned child knows its containing box, so `percent(50) + 10`
         // is half of it and ten more. Flex layout cannot hold the sum — Yoga's
         // percent is a unit of its own — so it says so and lays the property
         // out as auto, and the SAME declaration still resolves where the
         // extent is known.
         const Dimension halfAndTen = percent(50) + Dimension(10);
         Host host(400, 200);
         host.composer.render(positioned().width(300).height(100).children(
             {box()
                  .key("measured")
                  .left(0)
                  .top(0)
                  .width(halfAndTen)
                  .height(10)}));
         host.frame();
         EXPECT_FLOAT_EQ(widthOf(host), 0.5f * 300.0f + 10.0f);

         Host flex(400, 200);
         ::testing::internal::CaptureStderr();
         flex.composer.render(box().row().width(300).height(100).children(
             {box().key("measured").width(halfAndTen).height(10)}));
         flex.frame();
         const std::string report = ::testing::internal::GetCapturedStderr();
         EXPECT_NE(report.find("flex layout"), std::string::npos)
             << "flex layout dropped the sum without saying so: " << report;
         EXPECT_FLOAT_EQ(widthOf(flex), 0.0f) << "laid out as auto";
         EXPECT_EQ(halfAndTen, percent(50) + Dimension(10))
             << "the declaration kept the sum as written";
       }},
  });
}

TEST(ComposeLengths, ASumIsOneEntryReleasedWithItsLastHolder) {
  checkRows({
      {"ASumLeavesTheTableWithItsLastHolder",
       [] {
         // A sum in several units is one counted entry, shared by every length
         // equal to it, and it goes when the last of them does.
         const size_t before = sigil::compose::detail::calcEntries();
         {
           const Dimension sum = Dimension(3_em) + 17_px;
           EXPECT_EQ(sigil::compose::detail::calcEntries(), before + 1);
           const Dimension copy = sum;
           Dimension moved = Dimension(3_em) + 17_px;
           const Dimension taken = std::move(moved);
           EXPECT_EQ(sigil::compose::detail::calcEntries(), before + 1)
               << "one entry per sum";
           EXPECT_EQ(copy, taken);
           Dimension replaced = Dimension(5_em) + 3_px;
           EXPECT_EQ(sigil::compose::detail::calcEntries(), before + 2);
           replaced = sum;
           EXPECT_EQ(sigil::compose::detail::calcEntries(), before + 1)
               << "the overwritten sum had no other holder";
         }
         EXPECT_EQ(sigil::compose::detail::calcEntries(), before);
       }},
      {"ASumRebuiltEveryFrameDoesNotGrowTheTable",
       [] {
         // A sum built from a number that changes every frame is a new sum
         // every frame, and the one before it goes with the description that
         // held it.
         const size_t before = sigil::compose::detail::calcEntries();
         {
           Host host(400, 200);
           for (int frame = 0; frame < 120; ++frame) {
             host.composer.render(
                 pageWith(box()
                              .width(Dimension(1_em) + Dimension((float)frame))
                              .height(10),
                          20.0f));
             host.frame();
             ASSERT_FLOAT_EQ(widthOf(host), 20.0f + (float)frame);
           }
           EXPECT_LE(sigil::compose::detail::calcEntries(), before + 2)
               << "only what the composer still holds";
         }
         EXPECT_EQ(sigil::compose::detail::calcEntries(), before);
       }},
  });
}

TEST(ComposeLengths, AFractionIsANumberOrAPercentage) {
  // CSS's `<number> | <percentage>` where the two are one amount.
  EXPECT_EQ(parseFraction("0.5"), 0.5f);
  EXPECT_EQ(parseFraction(" 50% "), 0.5f);
  EXPECT_EQ(parseFraction("+150%"), 1.5f);
  EXPECT_EQ(parseFraction("-25%"), -0.25f);
  EXPECT_EQ(parseFraction("1"), 1.0f);
  EXPECT_FALSE(parseFraction("50px").has_value());
  EXPECT_FALSE(parseFraction("half").has_value());
  EXPECT_FALSE(parseFraction("").has_value());
  EXPECT_FALSE(parseFraction("nan%").has_value());
}

TEST(ComposeLengths, AFontHoldsTheFontUnitsAndAPercentageOfTheTypeSize) {
  checkRows({
      {"AFontHoldsTheFontUnitsAndAPercentageOfTheTypeSize",
       [] {
         // CSS's font-size and letter-spacing read a percentage as a multiple
         // of the type size, which is an em here; units a font cannot hold
         // answer nothing.
         EXPECT_EQ(fontLength(Dimension(12.0f)), sigil::weave::Length(12.0f));
         EXPECT_EQ(fontLength(9_pt), sigil::weave::pt(9));
         EXPECT_EQ(fontLength(1.5_em), sigil::weave::em(1.5f));
         EXPECT_EQ(fontLength(2_rem), sigil::weave::rem(2));
         EXPECT_EQ(fontLength(percent(150)), sigil::weave::em(1.5f));
         EXPECT_FALSE(fontLength(vw(10)).has_value());
         EXPECT_FALSE(fontLength(Dimension(var("size"))).has_value());
         EXPECT_FALSE(fontLength(autoDimension()).has_value());
       }},
      {"AFontSizeInPercentIsAMultipleOfTheParentsSize",
       [] {
         // `fontSize(150_percent)` under a 40 px parent is 60 px, which an em
         // measured on the node itself then reads.
         Host host(400, 200);
         host.composer.render(pageWith(
             box().fontSize(150_percent).width(1_em).height(10).flexShrink(0),
             40.0f));
         host.frame();
         EXPECT_FLOAT_EQ(widthOf(host), 60.0f);
       }},
  });
}

TEST(ComposeLengths, ALineHeightIsAPitchOrAMultipleOfTheTypeSize) {
  checkRows({
      {"ALineHeightInARelativeUnitIsAnAbsolutePitch",
       [] {
         // CSS's line-height as a length: 24 px, 1.5em and 150% of a 20 px type
         // size are all a 30 px pitch, the same one LineHeight::absolute
         // states.
         Host host(400, 400);
         const auto line = [](const char* key) {
           return text(u8"A").key(key).flexShrink(0);
         };
         host.composer.render(
             box()
                 .font({.face = sigil::test::instrument::sans(), .size = 20})
                 .column()
                 .children({line("pixels").lineHeight(Dimension(30.0f)),
                            line("em").lineHeight(1.5_em),
                            line("percent").lineHeight(150_percent),
                            line("leading").lineHeight(
                                sigil::weave::LineHeight::absolute(30))}));
         host.frame();
         const float pitch = require(host.composer.bounds("leading")).height();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("pixels")).height(),
                         pitch);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("em")).height(), pitch);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("percent")).height(),
                         pitch);
       }},
      {"ARelativeLineHeightIsInheritedAsThePitchItCameTo",
       [] {
         // CSS computes an em line height where it is stated and hands the
         // pixels down: a passage set twice as large below keeps the 30 px
         // pitch.
         Host host(400, 400);
         host.composer.render(
             box()
                 .font({.face = sigil::test::instrument::sans(), .size = 20})
                 .lineHeight(1.5_em)
                 .column()
                 .children(
                     {text(u8"A").key("inherited").fontSize(40).flexShrink(0),
                      text(u8"A")
                          .key("stated")
                          .fontSize(40)
                          .lineHeight(sigil::weave::LineHeight::absolute(30))
                          .flexShrink(0)}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("inherited")).height(),
                         require(host.composer.bounds("stated")).height());
       }},
      {"ANumberLineHeightIsAMultipleOfTheTypeSize",
       [] {
         // CSS's unitless line-height: twice a 20 px type size is a 40 px
         // pitch, whatever line height the face itself reports, and a passage
         // set at another size below inherits the number, not the pitch.
         Host host(400, 400);
         const auto line = [](const char* key) {
           return text(u8"A").key(key).flexShrink(0);
         };
         host.composer.render(
             box()
                 .font({.face = sigil::test::instrument::sans(), .size = 20})
                 .column()
                 .children({line("number").lineHeight(2.0f),
                            line("forty").lineHeight(
                                sigil::weave::LineHeight::absolute(40)),
                            box().lineHeight(2.0f).flexShrink(0).children(
                                {line("smaller").fontSize(10)}),
                            line("twenty").fontSize(10).lineHeight(
                                sigil::weave::LineHeight::absolute(20))}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("number")).height(),
                         require(host.composer.bounds("forty")).height());
         EXPECT_FLOAT_EQ(require(host.composer.bounds("smaller")).height(),
                         require(host.composer.bounds("twenty")).height());
       }},
  });
}
