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
