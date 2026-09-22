/** @file
 * The two lines that announce something: the title card and the section
 * header.
 */

#include <gtest/gtest.h>
#include <include/core/SkShader.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
using compose::Element;
using compose::Fill;
using sigil::sketch::kit::test::Drawn;
using sigil::sketch::kit::test::kTall;
using sigil::sketch::kit::test::kWide;
using sigil::sketch::kit::test::sameDrawing;
using sigil::sketch::kit::test::subject;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;

// The headings

/** The card is the header half of a page, standing alone: three lines in
 *  the theme's registers, spaced by its subtitle gap. */
TEST(SketchKitHeading, TitleCardDrawsTheHandSpelledColumn) {
  const kit::Theme& house = kit::houseTheme();
  const auto line = [&](const kit::Register& r, sigil::material::Color c) {
    return house.style(r, c);
  };
  Element byHand =
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .children({compose::text(u8"SIGIL · COMPOSE",
                                   line(house.type.eyebrow, house.palette.ash)),
                     compose::text(u8"THE STROKE ATLAS",
                                   line(house.type.title, house.palette.ink))
                         .margin(house.spacing.subtitleGap, 0, 0, 0),
                     compose::text(u8"every rail, at one width",
                                   line(house.type.subtitle, house.palette.ash))
                         .margin(house.spacing.subtitleGap, 0, 0, 0)});
  Element byKit = kit::titleCard({.eyebrow = {u8"SIGIL · COMPOSE"},
                                  .title = {u8"THE STROKE ATLAS"},
                                  .subtitle = {u8"every rail, at one width"}});
  EXPECT_TRUE(sameDrawing(std::move(byHand), std::move(byKit)));
}

/** A missing line is absent and spends no gap behind it, so a card of two
 *  lines is not a card of three with one blank. */
TEST(SketchKitHeading, AMissingLineSpendsNoGap) {
  EXPECT_TRUE(sameDrawing(
      kit::titleCard({.title = {u8"T"}, .subtitle = {u8"S"}}),
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .children({compose::text(u8"T", kit::houseTheme().style(
                                              kit::houseTheme().type.title,
                                              kit::houseTheme().palette.ink)),
                     compose::text(u8"S", kit::houseTheme().style(
                                              kit::houseTheme().type.subtitle,
                                              kit::houseTheme().palette.ash))
                         .margin(kit::houseTheme().spacing.subtitleGap, 0, 0, 0)})));
}

/** THE MASTHEAD: a card at the left and a stack of ranged notes at the
 *  right, the two ranged against each other at their ENDS so the last
 *  note sits on the card's last line. */
TEST(SketchKitHeading, ACardWithNotesIsTheHandSpelledRow) {
  const kit::Theme& house = kit::houseTheme();
  const auto line = [&](const kit::Register& reg, sigil::material::Color ink) {
    return house.style(reg, ink);
  };
  Element byHand =
      compose::box()
          .row()
          .alignItems(compose::Align::End)
          .children(
              {compose::box()
                   .column()
                   .alignItems(compose::Align::Stretch)
                   .children(
                       {compose::text(u8"MET OFFICE", line(house.type.eyebrow,
                                                           house.palette.ash))})
                   .children(
                       {compose::text(u8"THE SHIPPING FORECAST",
                                      line(house.type.title, house.palette.ink))
                            .margin(house.spacing.subtitleGap, 0, 0, 0)})
                   .flexGrow(1),
               compose::box()
                   .column()
                   .gap(house.spacing.rowGap)
                   .alignItems(compose::Align::End)
                   .children({compose::text(
                       u8"ISSUED 0015 UTC",
                       line(house.type.captionNote, house.palette.ash))})
                   .children({compose::text(
                       u8"VALID TO 0600 UTC",
                       line(house.type.captionNote, house.palette.ash))})});
  EXPECT_TRUE(sameDrawing(
      std::move(byHand),
      kit::titleCard({.eyebrow = {u8"MET OFFICE"},
                      .title = {u8"THE SHIPPING FORECAST"},
                      .notes = {{u8"ISSUED 0015 UTC"}, {u8"VALID TO 0600 UTC"}},
                      .align = compose::Align::Stretch})));
}

/** A ranged note is often a step quieter than the subtitle beside it,
 *  which one ash cannot say — so a line names its own ink. */
TEST(SketchKitHeading, ALineSetsItsOwnInk) {
  EXPECT_FALSE(sameDrawing(
      kit::titleCard({.title = {u8"T"}, .notes = {{u8"n"}}}),
      kit::titleCard({.title = {u8"T"},
                      .notes = {{.words = u8"n",
                                 .ink = Fill::color({1, 0.3f, 0.1f, 1})}}})));
}

/** A register names the face its own line is set in, for the line neither
 *  of the theme's two is. */
TEST(SketchKitHeading, ARegisterNamesItsOwnFace) {
  kit::Theme paper = kit::houseTheme();
  paper.type.title.face = paper.type.mono;
  const kit::Provide look(paper);
  Element byHand =
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .children({compose::text(
              u8"THE STROKE ATLAS",
              sigil::weave::textStyle(
                  {.face = paper.type.mono,
                   .size = paper.type.title.size,
                   .color = sigil::material::skia::toSkColor(paper.palette.ink),
                   .track = paper.type.title.track}))});
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::titleCard({.title = {u8"THE STROKE ATLAS"}})));
}

/** The rule follows the label; the supporting note stays below the heading. */
TEST(SketchKitHeading, TheSectionRuleFollowsItsLabel) {
  EXPECT_FALSE(sameDrawing(
      kit::sectionHeader({.label = u8"DYNAMICS", .note = u8"6 presets"})
          .width(360),
      kit::sectionHeader(
          {.label = u8"DYNAMICS", .note = u8"6 presets", .ruled = false})
          .width(360)));
}

TEST(SketchKitHeading, DocumentRulesStyleAPreviouslyConstructedCard) {
  const Element card = kit::titleCard({.title = {"AAAA"}, .key = "card"});
  Drawn original(compose::box().children({card}));
  Drawn styled(
      compose::box().styleSheet({{"h1", {.size = 32}}}).children({card}));
  const auto before = original.composer.bounds("card-title");
  const auto after = styled.composer.bounds("card-title");
  ASSERT_TRUE(before);
  ASSERT_TRUE(after);
  EXPECT_GT(after->width(), before->width() * 1.5f);
}

TEST(SketchKitHeading, AnAuthoredShaderKeepsDocumentTypography) {
  const Element card = kit::titleCard(
      {.title = {.words = "AAAA",
                 .ink = Fill::shader(SkShaders::Color(SK_ColorGREEN))},
       .key = "card"});
  Drawn original(compose::box().children({card}));
  Drawn styled(compose::box()
                   .styleSheet({{"h1", {.size = 32, .color = SkColors::kRed}}})
                   .children({card}));
  const auto before = original.composer.bounds("card-title");
  const auto after = styled.composer.bounds("card-title");
  ASSERT_TRUE(before);
  ASSERT_TRUE(after);
  EXPECT_GT(after->width(), before->width() * 1.5f);
  const SkBitmap pixels = styled.pixels();
  int green = 0;
  int red = 0;
  for (int y = 0; y < pixels.height(); ++y)
    for (int x = 0; x < pixels.width(); ++x) {
      const SkColor color = pixels.getColor(x, y);
      green += SkColorGetG(color) > 100;
      red += SkColorGetR(color) > 100;
    }
  EXPECT_GT(green, 0);
  EXPECT_EQ(red, 0);
}

TEST(SketchKitHeading, DocumentRulesSetASectionHeadingAndItsCaption) {
  const kit::Theme& house = kit::houseTheme();
  kit::Register heading = house.type.section;
  heading.size = 24;
  kit::Register note = house.type.captionNote;
  note.size = 18;
  const Element header =
      kit::sectionHeader({.label = "AAAA", .note = "BBBB", .ruled = false});
  Element byHand = compose::box().column().flexShrink(0).children(
      {compose::box()
           .row()
           .alignItems(compose::Align::Center)
           .gap(house.spacing.labelGap)
           .children(
               {compose::text("AAAA", house.style(heading, SkColors::kRed))}),
       compose::text("BBBB", house.style(note, SkColors::kGreen))
           .maxWidth(house.type.captionNote.size * 36)
           .margin(house.spacing.captionNoteGap, 0, 0, 0)});
  EXPECT_TRUE(sameDrawing(
      compose::box()
          .styleSheet({{"h2", {.size = 24, .color = SkColors::kRed}},
                       {"caption", {.size = 18, .color = SkColors::kGreen}}})
          .children({header}),
      compose::box().children({std::move(byHand)})));
}

}  // namespace
