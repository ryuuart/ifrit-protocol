// The units a length is written in: the two that measure against the face
// in force beside the three that were already there, the point that is a
// fixed count of pixels, and the one grammar that reads a length written
// as text.

#include <sigilcompose/core/Layout.h>
#include <sigilweave/style/Length.h>

#include "support/CoreTestSupport.h"

using namespace sigil::weave::literals;

namespace {

/** A page that sets the instrument face at @p size, with one keyed child. */
Element pageWith(Element child, float size) {
  return box()
      .font({.face = sigil::test::instrument::sans(), .size = size})
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

TEST(ComposeLengths, AChIsTheAdvanceOfZeroInTheFaceInForce) {
  // CSS's `ch`: not the em, not the average character, the figure zero —
  // which is why a column of digits written in it lines up.
  Host host(400, 200);
  host.composer.render(pageWith(box().width(2_ch).height(10), 100.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 2.0f * 100.0f * kInstrumentZeroEm);
}

TEST(ComposeLengths, AChFollowsTheFontItIsMeasuredAgainst) {
  // The same statement under a larger face is a larger length, with
  // nothing but the ancestor's font changed — which is what makes it a
  // relative unit rather than a number written once.
  Host host(400, 200);
  host.composer.render(pageWith(box().width(4_ch).height(10), 50.0f));
  host.frame();
  const float small = widthOf(host);
  host.composer.render(pageWith(box().width(4_ch).height(10), 100.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), small * 2.0f);
}

TEST(ComposeLengths, APointIsFourPixelsToEveryThree) {
  // Absolute: the face in force has nothing to say about it, so the same
  // statement under two different fonts is the same number of pixels.
  Host host(400, 200);
  host.composer.render(pageWith(box().width(12_pt).height(10), 20.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 16.0f);
  host.composer.render(pageWith(box().width(12_pt).height(10), 80.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 16.0f);
}

TEST(ComposeLengths, ARelativeLengthUnderASizeInPointsMeasuresThePixels) {
  // A type size STATED in points stays in points once resolved — points
  // are a fixed count of pixels and nothing is owed to convert them — so
  // the em a box beneath it is written in must be a multiple of the
  // pixels those points come to, not of the point count. Twelve points
  // are sixteen pixels, and an em is one of them.
  Host host(400, 200);
  host.composer.render(box()
                           .font({.face = sigil::test::instrument::sans(),
                                  .size = sigil::weave::pt(12)})
                           .children({box()
                                          .key("measured")
                                          .width(1_em)
                                          .flexShrink(0)
                                          .height(10)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 16.0f);
}

TEST(ComposeLengths, OnlyWhatNeedsSomethingElseToResolveIsRelative) {
  // The question the cascade pass asks to decide whether a node's layout
  // has to be written again when the font moves. A point carries
  // everything it needs; a ch does not.
  EXPECT_TRUE(Dimension(1_ch).relative());
  EXPECT_FALSE(Dimension(1_pt).relative());
  EXPECT_FALSE(Dimension(1.0f).relative());
  EXPECT_FALSE(pct(50.0f).relative());
}

TEST(ComposeLengths, ALengthWrittenAsTextIsReadInTheUnitItNames) {
  EXPECT_EQ(parseDimension("12"), Dimension(12.0f));
  EXPECT_EQ(parseDimension("12px"), Dimension(12.0f));
  EXPECT_EQ(parseDimension("50%"), pct(50.0f));
  EXPECT_EQ(parseDimension("10pw"), pw(10.0f));
  EXPECT_EQ(parseDimension("10ph"), ph(10.0f));
  EXPECT_EQ(parseDimension("1.5em"), Dimension(1.5_em));
  EXPECT_EQ(parseDimension("2rem"), Dimension(2_rem));
  EXPECT_EQ(parseDimension("0.5lh"), Dimension(0.5_lh));
  EXPECT_EQ(parseDimension("3ch"), Dimension(3_ch));
  EXPECT_EQ(parseDimension("9pt"), Dimension(9_pt));
  EXPECT_EQ(parseDimension("auto"), autoDimension());
}

TEST(ComposeLengths, TheGrammarIgnoresCaseAndSurroundingSpace) {
  EXPECT_EQ(parseDimension("  1.5EM "), Dimension(1.5_em));
  EXPECT_EQ(parseDimension("\t-4PX"), Dimension(-4.0f));
  EXPECT_EQ(parseDimension(" AUTO "), autoDimension());
}

TEST(ComposeLengths, ALengthWrittenAsTextMayNameACustomProperty) {
  // The same reference `var("gutter")` answers, so a length read from
  // text resolves against the nearest ancestor that set the property
  // exactly as one written in code does. CSS's leading dashes name the
  // same property as the bare word.
  EXPECT_EQ(parseDimension("var(gutter)"), Dimension(var("gutter")));
  EXPECT_EQ(parseDimension("var(--gutter)"), Dimension(var("gutter")));
  EXPECT_EQ(parseDimension("var( gutter )"), Dimension(var("gutter")));
}

TEST(ComposeLengths, TextTheGrammarDoesNotCoverAnswersNothing) {
  // Nothing is answered rather than a plausible zero: a caller that
  // cannot tell "unreadable" from "no length" writes the wrong box and
  // reports nothing.
  EXPECT_FALSE(parseDimension("").has_value());
  EXPECT_FALSE(parseDimension("   ").has_value());
  EXPECT_FALSE(parseDimension("wide").has_value());
  EXPECT_FALSE(parseDimension("12vh").has_value());
  EXPECT_FALSE(parseDimension("12 px").has_value());
  EXPECT_FALSE(parseDimension("var()").has_value());
}

TEST(ComposeLengths, ANumberThatNamesNoDistanceIsNotALength) {
  // The number reader accepts the words for infinity and for no number
  // at all, which name no distance: a layout handed one lays out
  // nothing, and every box under it is wrong with nothing reported.
  EXPECT_FALSE(parseDimension("inf").has_value());
  EXPECT_FALSE(parseDimension("infinity").has_value());
  EXPECT_FALSE(parseDimension("inf%").has_value());
  EXPECT_FALSE(parseDimension("-inf").has_value());
  EXPECT_FALSE(parseDimension("nan").has_value());
  EXPECT_FALSE(parseDimension("nanpx").has_value());
}

TEST(ComposeLengths, ALeadingPlusIsTheNumbersSign) {
  // CSS writes a positive length either way, and a reader that took only
  // the minus would refuse half of what an author copies in.
  EXPECT_EQ(parseDimension("+12px"), Dimension(12.0f));
  EXPECT_EQ(parseDimension("+1.5em"), Dimension(1.5_em));
  EXPECT_EQ(parseDimension(" +50% "), pct(50.0f));
  EXPECT_FALSE(parseDimension("+").has_value());
}

TEST(ComposeLengths, ArithmeticInOneUnitStaysInThatUnit) {
  EXPECT_EQ(2 * Dimension(1_em), Dimension(2_em));
  EXPECT_EQ(12_px + 4_px, 16_px);
  EXPECT_EQ(Dimension(3_em) / 3, Dimension(1_em));
  EXPECT_EQ(50_pct * 2, 100_pct);
  EXPECT_EQ(-(10_px), Dimension(-10.0f));
  // A sum that cancels back to one unit is that unit again, so nothing
  // downstream can tell it was ever a sum.
  EXPECT_EQ(Dimension(1_em) + 12_px - 12_px, Dimension(1_em));
  EXPECT_EQ((Dimension(1_em) + 12_px).unit, Dimension::Unit::Calc);
  EXPECT_EQ(Dimension(1_em) + 12_px, 12_px + Dimension(1_em));
}

TEST(ComposeLengths, ASumInSeveralUnitsResolvesWithTheFontInForce) {
  Host host(400, 200);
  const Dimension sum = 2 * Dimension(1_em) + 12_px;
  host.composer.render(pageWith(box().width(sum).height(10), 20.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 2.0f * 20.0f + 12.0f);
  // The em moved, so the sum moves with it.
  host.composer.render(pageWith(box().width(sum).height(10), 30.0f));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 2.0f * 30.0f + 12.0f);
}

TEST(ComposeLengths, ASumReadsACustomPropertyAndTheCanvas) {
  Host host(400, 200);
  host.composer.render(box().var("gutter", 10_px).children(
      {box()
           .key("measured")
           .width(Dimension(var("gutter")) * 3 + 10_pw)
           .height(10)}));
  host.frame();
  EXPECT_FLOAT_EQ(widthOf(host), 3.0f * 10.0f + 0.1f * 400.0f);
}

TEST(ComposeLengths, APercentageMixesWithNothingAndStandsAsAuto) {
  // Yoga resolves a percentage of the parent itself and holds no sum, so
  // the arithmetic refuses rather than guessing, and says so once.
  EXPECT_EQ(50_pct + Dimension(1_em), autoDimension());
  EXPECT_EQ(12_px - 50_pct, autoDimension());
  EXPECT_EQ(autoDimension() * 2, autoDimension());
  // A division by zero is no length, refused as the calc() text refuses it.
  EXPECT_EQ(Dimension(1_em) / 0.0f, autoDimension());
  EXPECT_EQ((Dimension(1_em) + 12_px) / 0.0f, autoDimension());
  // A zero of pixels adds nothing, so it may stand beside one.
  EXPECT_EQ(50_pct + 0_px, 50_pct);
  EXPECT_EQ(50_pct + 25_pct, 75_pct);
}

TEST(ComposeLengths, CalcTextReadsAsTheArithmeticDoes) {
  EXPECT_EQ(parseDimension("calc(2em + 12px)"), 2 * Dimension(1_em) + 12_px);
  EXPECT_EQ(parseDimension("CALC( (1em + 2px) * 2 )"),
            (Dimension(1_em) + 2_px) * 2);
  EXPECT_EQ(parseDimension("calc(var(--gutter) / 2 + 1ch)"),
            Dimension(var("gutter")) / 2 + Dimension(1_ch));
  EXPECT_EQ(parseDimension("calc(3 * 4)"), 12_px) << "a bare number is px";
  // A length times a length, a division by nothing or by a length, a sum
  // with a gap in it, and a percentage in a sum are none of them lengths.
  EXPECT_FALSE(parseDimension("calc(1em * 2px)"));
  EXPECT_FALSE(parseDimension("calc(1em / 0)"));
  EXPECT_FALSE(parseDimension("calc(1em / 1px)"));
  EXPECT_FALSE(parseDimension("calc(1em +)"));
  EXPECT_FALSE(parseDimension("calc(50% + 1em)"));
}
