/** @file
 * The two lines that announce something: the title card and the section
 * header.
 */

#include <gtest/gtest.h>
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
  const auto line = [&](const kit::Register& r, SkColor4f c) {
    return house.style(r, c);
  };
  Element byHand =
      compose::box()
          .column()
          .alignItems(compose::Align::Start)
          .child(compose::text(u8"SIGIL · COMPOSE",
                               line(house.type.eyebrow, house.palette.ash)))
          .child(compose::text(u8"THE STROKE ATLAS",
                               line(house.type.title, house.palette.ink))
                     .margin(0, house.spacing.subtitleGap, 0, 0))
          .child(compose::text(u8"every rail, at one width",
                               line(house.type.subtitle, house.palette.ash))
                     .margin(0, house.spacing.subtitleGap, 0, 0));
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
          .child(compose::text(
              u8"T", kit::houseTheme().style(kit::houseTheme().type.title,
                                             kit::houseTheme().palette.ink)))
          .child(compose::text(u8"S", kit::houseTheme().style(
                                          kit::houseTheme().type.subtitle,
                                          kit::houseTheme().palette.ash))
                     .margin(0, kit::houseTheme().spacing.subtitleGap, 0, 0))));
}

/** THE MASTHEAD: a card at the left and a stack of ranged notes at the
 *  right, the two ranged against each other at their ENDS so the last
 *  note sits on the card's last line. */
TEST(SketchKitHeading, ACardWithNotesIsTheHandSpelledRow) {
  const kit::Theme& house = kit::houseTheme();
  const auto line = [&](const kit::Register& reg, SkColor4f ink) {
    return house.style(reg, ink);
  };
  Element byHand =
      compose::box()
          .row()
          .alignItems(compose::Align::End)
          .child(
              compose::box()
                  .column()
                  .alignItems(compose::Align::Stretch)
                  .child(compose::text(u8"MET OFFICE", line(house.type.eyebrow,
                                                            house.palette.ash)))
                  .child(
                      compose::text(u8"THE SHIPPING FORECAST",
                                    line(house.type.title, house.palette.ink))
                          .margin(0, house.spacing.subtitleGap, 0, 0))
                  .grow(1))
          .child(compose::box()
                     .column()
                     .gap(house.spacing.rowGap)
                     .alignItems(compose::Align::End)
                     .child(compose::text(
                         u8"ISSUED 0015 UTC",
                         line(house.type.captionNote, house.palette.ash)))
                     .child(compose::text(
                         u8"VALID TO 0600 UTC",
                         line(house.type.captionNote, house.palette.ash))));
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
          .child(compose::text(
              u8"THE STROKE ATLAS",
              sigil::weave::textStyle({.face = paper.type.mono,
                                       .size = paper.type.title.size,
                                       .color = paper.palette.ink,
                                       .track = paper.type.title.track})));
  EXPECT_TRUE(sameDrawing(std::move(byHand),
                          kit::titleCard({.title = {u8"THE STROKE ATLAS"}})));
}

/** The rule is what grows, so the note stands at the far edge however
 *  wide the header is — which is the whole reason it is a component. */
TEST(SketchKitHeading, TheSectionRuleFillsWhatTheTwoLinesLeave) {
  EXPECT_FALSE(sameDrawing(
      kit::sectionHeader({.label = u8"DYNAMICS", .note = u8"6 presets"})
          .width(compose::Dim(360)),
      kit::sectionHeader(
          {.label = u8"DYNAMICS", .note = u8"6 presets", .ruled = false})
          .width(compose::Dim(360))));
}

}  // namespace
