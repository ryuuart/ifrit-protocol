// kit/Specimen.h — the captioned cell, the run of cells, the well they
// stand in and the sheet that heads and foots them.
//
// The claims are about WHERE THE PARTS LAND, so every case lays out through
// a composer and reads the keyed boxes back, rather than restating the
// margins the component spells.

#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/paint/Bases.h>

#include <optional>
#include <utility>

#include "support/KitType.h"
#include "support/Rows.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

namespace {

kit::Caption specimenVoice(kit::Caption::Where where) {
  return {.where = where, .gap = 6, .noteGap = 4};
}

/** The sheet the caption's two classes are registered on — the only place
 *  a cell's type comes from. */
sigil::compose::StyleSheet captionClasses() {
  return sigil::compose::StyleSheet{
      sigil::compose::rule("label, .label").font({.size = 12}),
      sigil::compose::rule("caption, .caption").font({.size = 10})};
}

}  // namespace

TEST(KitSpecimen, ASheetHeadsAndFootsItsPageWithLinesOfItsOwnText) {
  kit::Sheet page{.title = "TITLE",
                  .footer = "the footer",
                  .marginHorizontal = 30,
                  .marginTop = 16,
                  .marginBottom = 14,
                  .key = "page"};
  page.titleLine = [](const sigil::compose::Utf8& t) {
    return text(t).font({.size = 20});
  };
  Host host(400, 300);
  host.composer.render(
      kit::sheet(page, box().key("body"))
          .applyStyleSheet(sigil::compose::StyleSheet{
              sigil::compose::rule("h1").font({.size = 15}),
              sigil::compose::rule("footer").font({.size = 11})})
          .width(400)
          .height(300));
  host.frame();
  // The one line the sheet was handed stands where the register's would,
  // and the footer keeps its register, because no sheet moved.
  EXPECT_NEAR(require(host.composer.bounds("page-title")).height(),
              lineHeight(20), 1.5f);
  EXPECT_NEAR(require(host.composer.bounds("page-footer")).height(),
              lineHeight(11), 1.0f);
}

TEST(KitSpecimen, FormatReturnsTheWholeReading) {
  EXPECT_EQ(kit::formatted("plain"), "plain");
  EXPECT_EQ(kit::formatted("%s %d %.2f", "row", 17, 0.25), "row 17 0.25");
  const std::string payload(4096, 'x');
  EXPECT_EQ(kit::formatted("[%s]", payload.c_str()), "[" + payload + "]");
  EXPECT_TRUE(kit::formatted(nullptr).empty());
}

TEST(KitSpecimen, ARunSpacesItsCellsAndRulesBetweenThem) {
  const auto run = [](bool column) {
    return kit::cells({.cells = {box().key("a").width(50).height(20),
                                 box().key("b").width(50).height(30)},
                       .column = column,
                       .gap = 10,
                       .divider = red(),
                       .dividerWidth = 2});
  };
  Host host(300, 300);
  host.composer.render(box().width(300).height(300).children({run(false)}));
  host.frame();
  // cell, gap, rule, gap, cell — and the rule spans the taller cell.
  EXPECT_FLOAT_EQ(host.composer.bounds("b").value().left(), 50 + 10 + 2 + 10);
  EXPECT_EQ(host.pixel(61, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(61, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(55, 5), SK_ColorBLACK);

  host.composer.render(box().width(300).height(300).children({run(true)}));
  host.frame();
  EXPECT_FLOAT_EQ(host.composer.bounds("b").value().top(), 20 + 10 + 2 + 10);
  EXPECT_EQ(host.pixel(5, 31), SK_ColorRED);
  EXPECT_EQ(host.pixel(49, 31), SK_ColorRED);
}

TEST(KitSpecimen, PanelsShareTheWidthAcrossWrappedRows) {
  sigil::compose::test::checkRows({
      {"PanelsShareWidthAndKeepTheLastRowAligned",
       [&] {
         Host host(320, 200);
         host.composer.render(box().children({kit::panelGrid(
             {.cells = {box().key("a").width(240).height(20),
                        box().key("b").width(10).height(30),
                        box().key("c").height(10), box().key("d").height(15)},
              .columns = 3,
              .gap = 10,
              .rowGap = 12})}));
         host.frame();
         const auto a = host.composer.bounds("a");
         const auto b = host.composer.bounds("b");
         const auto d = host.composer.bounds("d");
         ASSERT_TRUE(a && b && d);
         EXPECT_FLOAT_EQ(a->width(), 100);
         EXPECT_FLOAT_EQ(a->width(), b->width());
         EXPECT_FLOAT_EQ(a->width(), d->width());
         EXPECT_FLOAT_EQ(a->height(), 30);
         EXPECT_FLOAT_EQ(d->left(), 0);
         EXPECT_FLOAT_EQ(d->top(), 42);
       }},
      {"AGridToldItsMeasureCutsTheSharesFromThatWidth",
       [&] {
         // A grid whose parent sizes from its CONTENT has no width to divide,
         // and its cells would be dealt nothing and drawn over each other.
         Host host(400, 200);
         const auto shares = [&](Dimension measure) {
           host.composer.render(box().width(400).height(200).row().children(
               {kit::panelGrid({.cells = {box().key("a").height(10),
                                          box().key("b").height(10)},
                                .columns = 2,
                                .gap = 20,
                                .measure = measure})}));
           host.frame();
           return std::pair{require(host.composer.bounds("a")),
                            require(host.composer.bounds("b"))};
         };
         const auto [none, alongside] = shares({});
         EXPECT_FLOAT_EQ(none.width(), 0);
         EXPECT_FLOAT_EQ(alongside.width(), 0);
         const auto [left, right] = shares(Dimension(220));
         EXPECT_FLOAT_EQ(left.width(), 100);
         EXPECT_FLOAT_EQ(right.width(), 100);
         EXPECT_FLOAT_EQ(right.left(), 120);
       }},
      {"PanelDividersKeepTheirWidthAcrossWrappedRows",
       [&] {
         Host host(300, 200);
         host.composer.render(box().children({kit::panelGrid(
             {.cells = {box().key("a").height(20), box().key("b").height(30),
                        box().key("c").height(15), box().key("d").height(25)},
              .columns = 2,
              .gap = 10,
              .rowGap = 12,
              .divider = red(),
              .dividerWidth = 4})}));
         host.frame();
         const auto a = host.composer.bounds("a");
         const auto b = host.composer.bounds("b");
         const auto c = host.composer.bounds("c");
         const auto d = host.composer.bounds("d");
         ASSERT_TRUE(a && b && c && d);
         EXPECT_FLOAT_EQ(a->width(), 138);
         EXPECT_FLOAT_EQ(b->left(), 162);
         EXPECT_FLOAT_EQ(c->top(), 42);
         EXPECT_FLOAT_EQ(d->left(), b->left());
         EXPECT_EQ(host.pixel(150, 29), SK_ColorRED);
         EXPECT_EQ(host.pixel(150, 60), SK_ColorRED);
         EXPECT_EQ(host.pixel(150, 35), SK_ColorBLACK);
       }},
  });
}

TEST(KitSpecimen, AWellHoldsItsContentAsTheCallerStates) {
  // A ground is a Fill, and a box-unit paint in it is stretched over
  // whatever width the well is given.
  Host host(200, 100);
  const Fill paint = material::linearGradient(
      {0, 0}, {1, 0}, {{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 1}}});
  for (float width : {80.0f, 160.0f}) {
    host.composer.render(box().children(
        {kit::well({.width = width, .height = 60, .ground = paint})}));
    host.frame();
    const SkColor middle = host.pixel((int)(width / 2), 20);
    EXPECT_NEAR(SkColorGetR(middle), SkColorGetB(middle), 5);
  }
}
