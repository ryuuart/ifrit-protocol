// kit/Board.h — the ground a placed drawing stands on and the titled
// region over it: what a board takes from its parent, what it leaves to
// the caller's own verbs, and where a panel rules its head off its
// content.

#include <sigilcompose/kit/Board.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilweave/layout/StyleSheet.h>

#include <utility>

#include "support/KitType.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitBoard, TakesWhateverItsParentGivesItWhereNoSizeIsStated) {
  Host host(200, 150);
  host.composer.render(box().width(200).height(150).children(
      {kit::board({}).key("board").children(
          {kit::at(0, 0, 10, 10).key("pin")})}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("board")), SkRect::MakeWH(200, 150));
  EXPECT_EQ(require(host.composer.bounds("pin")), SkRect::MakeWH(10, 10));
}

TEST(KitBoard, StandsAtItsOwnSizeOnItsOwnGroundAndPlacesEveryChildByItsRect) {
  Host host(200, 150);
  host.composer.render(box().width(200).height(150).children(
      {kit::board({.size = {120, 90}, .ground = red()})
           .key("board")
           .children({kit::at(20, 30, 10, 10).fill(green()).key("pin")})}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("board")), SkRect::MakeWH(120, 90));
  // Every child keeps the rect it was built with, against the board's
  // own edge.
  EXPECT_EQ(require(host.composer.bounds("pin")),
            SkRect::MakeXYWH(20, 30, 10, 10));
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(25, 35), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(130, 40), SK_ColorBLACK);
}

TEST(KitBoard, StatesNoSheetAndNoFontSoTheCallersVerbsDecide) {
  // A board is a box property throughout: the font, the ink and the sheet
  // are the caller's own verbs on what it returns, which is what a
  // page-less drawing states its classes with.
  Host host(200, 150);
  host.composer.render(
      box()
          .width(200)
          .height(150)
          .font({.size = 20})
          .styleSheet(weave::StyleSheet{{"caption", {.size = 9}}})
          .children({kit::board({}).children(
              {text(u8"Hg").key("inherited"),
               document::caption(u8"Hg").key("named")})}));
  host.frame();
  EXPECT_NEAR(require(host.composer.bounds("inherited")).height(),
              lineHeight(20), 1.5f);
  EXPECT_NEAR(require(host.composer.bounds("named")).height(), lineHeight(9),
              1.5f);
}

namespace {

/** The sheet a panel's three classes are registered on. */
weave::StyleSheet panelClasses() {
  return weave::StyleSheet{{"eyebrow", {.size = 9}},
                           {"h1", {.size = 14}},
                           {"caption", {.size = 10}}};
}

}  // namespace

TEST(KitPanel, RulesItsHeadOffItsContentAndStandsBothInItsOwnWell) {
  const float eyebrow = lineHeight(9);
  const float title = lineHeight(14);
  kit::Panel region{
      .eyebrow = "LOADOUT",
      .title = "SLOTS",
      .rule = red(),
      .ruleWidth = 2,
      .gap = 12,
      .titleGap = 4,
      .body = kit::Well{
          .width = 200, .height = 160, .ground = green(), .padding = 10}};
  Host host(300, 300);
  host.composer.render(
      box()
          .width(300)
          .height(300)
          .styleSheet(panelClasses())
          .children({kit::panel(region, box().key("content")).key("panel")}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("panel")), SkRect::MakeWH(200, 160));
  // The head stands inside the well's padding, and the content one gap
  // under it.
  const SkRect content = require(host.composer.bounds("content"));
  EXPECT_FLOAT_EQ(content.left(), 10);
  EXPECT_NEAR(content.top(), 10 + eyebrow + 4 + title + 12, 1.5f);
  // The rule bisects that gap rather than adding to it, so an unruled
  // panel puts its content in the same place.
  const int ruleTop = (int)(10 + eyebrow + 4 + title + 5);
  EXPECT_EQ(host.pixel(100, ruleTop + 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, ruleTop - 2), SK_ColorGREEN);
  kit::Panel plain = region;
  plain.rule = Fill::none();
  host.composer.render(
      box()
          .width(300)
          .height(300)
          .styleSheet(panelClasses())
          .children({kit::panel(plain, box().key("bare")).key("panel")}));
  host.frame();
  EXPECT_NEAR(require(host.composer.bounds("bare")).top(), content.top(), 1.0f);
}

TEST(KitPanel, RangesItsNoteAtTheFarEdgeOfTheHeadsLastLine) {
  const auto noteRect = [](bool withTitle) {
    kit::Panel region{.eyebrow = "LOADOUT",
                      .title = withTitle ? "SLOTS" : "",
                      .note = "3 / 8",
                      .body = kit::Well{.width = 200, .padding = 10}};
    region.noteLine = [](const sigil::compose::Utf8& t) {
      return text(t).key("note").font({.size = 10});
    };
    Host host(300, 300);
    host.composer.render(
        box()
            .width(300)
            .height(300)
            .styleSheet(panelClasses())
            .children({kit::panel(region, box().key("content"))}));
    host.frame();
    return std::pair{require(host.composer.bounds("note")),
                     require(host.composer.bounds("content"))};
  };
  // On the title's line where there is a title, and on the eyebrow's
  // where there is not — at the far edge of the head either way.
  const auto [besideTitle, under] = noteRect(true);
  EXPECT_FLOAT_EQ(besideTitle.right(), 190);
  EXPECT_NEAR(besideTitle.top(), lineHeight(9) + 4 + 10, 2.0f);
  const auto [besideEyebrow, alsoUnder] = noteRect(false);
  EXPECT_FLOAT_EQ(besideEyebrow.right(), 190);
  EXPECT_NEAR(besideEyebrow.top(), 10, 2.0f);
}

TEST(KitPanel, StandsBareWhereNoBodyIsStated) {
  // A caller that grounds the region itself gets the head and the
  // content and nothing around them.
  Host host(300, 300);
  host.composer.render(
      box()
          .width(300)
          .height(300)
          .styleSheet(panelClasses())
          .children({kit::panel({.eyebrow = "LOADOUT"},
                                box().key("content").height(40))
                         .key("panel")}));
  host.frame();
  const SkRect panel = require(host.composer.bounds("panel"));
  EXPECT_FLOAT_EQ(panel.left(), 0);
  EXPECT_NEAR(require(host.composer.bounds("content")).top(),
              lineHeight(9) + 12, 1.5f);
}
