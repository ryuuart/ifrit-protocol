// kit/Rows.h — a name and the figure that answers it: the classes a cell
// is set in, where a reading puts its figure, how a table heads its
// columns and rules under them, and what a bar is drawn in proportion to.

#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilweave/layout/StyleSheet.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "support/KitType.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

namespace {

/** The sheet the three classes a row names are registered on. */
weave::StyleSheet rowClasses() {
  return weave::StyleSheet{{"caption", {.size = 10}},
                           {"readout", {.size = 12}},
                           {"h2", {.size = 11}}};
}

}  // namespace

TEST(KitRows, ACellNamesTheRowItStandsInAsWellAsItsColumn) {
  Host host(220, 90);
  // A part takes the parameters it names: four of them here, so the
  // second row's cells are the ones lit.
  kit::Table how{.columns = {{u8"KEY", 60}, {u8"VALUE", 60}}};
  how.cellLine = [](const Utf8& words, const kit::Table&, size_t column,
                    size_t row) {
    return text(words).key("r" + std::to_string(row) + "c" +
                           std::to_string(column));
  };
  const Utf8 cells[4] = {u8"a", u8"b", u8"c", u8"d"};
  host.composer.render(box().width(220).height(90).children(
      {kit::table(std::span<const Utf8>(cells), how)}));
  host.frame();
  EXPECT_TRUE(host.composer.bounds("r0c0").has_value());
  EXPECT_TRUE(host.composer.bounds("r1c1").has_value());
  EXPECT_FALSE(host.composer.bounds("r2c0").has_value());
}

TEST(KitRows, ANameIsSetInCaptionNoteAndAFigureInReadout) {
  const auto height = [](const kit::Reading& one) {
    return intrinsicSize(
               box().styleSheet(rowClasses()).children({kit::reading(one)}),
               fonts())
        .height();
  };
  EXPECT_NEAR(height({.name = u8"nodes"}), lineHeight(10), 1.0f);
  EXPECT_NEAR(height({.value = u8"1 248"}), lineHeight(12), 1.0f);
  EXPECT_NEAR(height({.note = u8"ms"}), lineHeight(10), 1.0f);
}

TEST(KitRows, AReadingPutsItsFigureAtTheFarEdgeOfTheMeasure) {
  kit::Rows how{
      .measure = 220, .nameMeasure = 120, .labelGap = 10, .swatchSide = 9};
  how.nameLine = [](const sigil::compose::Utf8& t) {
    return text(t).key("name").font({.size = 10});
  };
  how.valueLine = [](const sigil::compose::Utf8& t) {
    return text(t).key("value").font({.size = 12});
  };
  Host host(300, 300);
  host.composer.render(box()
                           .width(300)
                           .height(300)
                           .styleSheet(rowClasses())
                           .children({kit::reading({.name = u8"nodes",
                                                    .value = u8"1 248",
                                                    .swatch = green()},
                                                   how)
                                          .key("row")}));
  host.frame();
  EXPECT_FLOAT_EQ(require(host.composer.bounds("row")).width(), 220);
  // The mark stands before the name, the name takes its measure, and the
  // space between is what grows — so the figure is on the far edge.
  const SkRect name = require(host.composer.bounds("name"));
  EXPECT_FLOAT_EQ(name.left(), 9 + 10);
  EXPECT_FLOAT_EQ(name.width(), 120);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("value")).right(), 220);
}

TEST(KitRows, AReadoutStacksItsRowsAndARuleStandsBetweenThem) {
  const std::vector<kit::Reading> two = {
      {.name = u8"nodes", .value = u8"7"},
      {.name = u8"instances", .value = u8"9"}};
  const auto height = [&](std::span<const kit::Reading> rows, bool ruled) {
    kit::Rows how{.measure = 220, .gap = 6};
    if (ruled) {
      how.divider = red();
      how.dividerWidth = 2;
    }
    return intrinsicSize(box()
                             .styleSheet(rowClasses())
                             .children({kit::readout(rows, how)}),
                         fonts())
        .height();
  };
  const float one = height(std::span(two).first(1), false);
  EXPECT_NEAR(height(two, false), one * 2 + 6, 1.0f);
  // The rule is a row of its own: it adds its width and a second gap.
  EXPECT_NEAR(height(two, true), one * 2 + 6 + 6 + 2, 1.0f);
}

TEST(KitRows, ATableSetsEachCellInItsColumnsClass) {
  // A cell of a figure column is set in `readout` and every other in
  // `captionNote`, so the row that carries a figure is the taller.
  const std::vector<sigil::compose::Utf8> quiet = {u8"alpha"};
  const std::vector<sigil::compose::Utf8> measured = {u8"beta", u8"12"};
  const std::vector<std::span<const sigil::compose::Utf8>> rows = {quiet,
                                                                   measured};
  const std::vector<std::string> keys = {"r0", "r1"};
  kit::Table how{.columns = {{.width = 120}, {.width = 60, .figure = true}},
                 .gap = 10,
                 .rowGap = 5,
                 .keys = keys};
  Host host(400, 300);
  host.composer.render(box()
                           .width(400)
                           .height(300)
                           .styleSheet(rowClasses())
                           .children({kit::table(rows, how)}));
  host.frame();
  const SkRect first = require(host.composer.bounds("r0"));
  const SkRect second = require(host.composer.bounds("r1"));
  EXPECT_NEAR(first.height(), lineHeight(10), 1.0f);
  EXPECT_NEAR(second.height(), lineHeight(12), 1.0f);
  EXPECT_NEAR(second.top(), first.height() + 5, 1.0f);
}

TEST(KitRows, ARowIsItsOwnRunOfCellsSoAShortRowStaysShort) {
  // Row 0 carries one word where the table has two columns, and row 1 one
  // word more than it has: the short row stays short, and the surplus
  // takes the last column's class at its own width.
  const std::vector<sigil::compose::Utf8> first = {u8"alpha"};
  const std::vector<sigil::compose::Utf8> second = {u8"beta", u8"12", u8"held"};
  const std::vector<std::span<const sigil::compose::Utf8>> rows = {first,
                                                                   second};
  const std::vector<SurfacePaint> swatches = {SurfacePaint{}, green()};
  kit::Table how{.columns = {{.width = 120}, {.width = 60, .figure = true}},
                 .gap = 10,
                 .rowGap = 5,
                 .swatches = swatches,
                 .swatchSide = 9};
  // Every word is its own key, so where each cell lands can be read back.
  how.cellLine = [](const sigil::compose::Utf8& t) {
    return text(t)
        .key(std::string(reinterpret_cast<const char*>(t.bytes().c_str())))
        .font({.size = 10});
  };
  Host host(400, 300);
  host.composer.render(box()
                           .width(400)
                           .height(300)
                           .styleSheet(rowClasses())
                           .children({kit::table(rows, how)}));
  host.frame();
  // The unmarked row leaves the mark's column empty, and its one cell
  // takes its column's width.
  const SkRect alpha = require(host.composer.bounds("alpha"));
  EXPECT_FLOAT_EQ(alpha.left(), 9 + 10);
  EXPECT_FLOAT_EQ(alpha.width(), 120);
  // The marked row stands its mark before the first column.
  EXPECT_FLOAT_EQ(require(host.composer.bounds("beta")).left(), 9 + 10);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("12")).left(),
                  9 + 10 + 120 + 10);
  const SkRect surplus = require(host.composer.bounds("held"));
  EXPECT_FLOAT_EQ(surplus.left(), 9 + 10 + 120 + 10 + 60 + 10);
  EXPECT_LT(surplus.width(), 60);
}

TEST(KitRows, ATableHeadsItsColumnsInTheSectionClassAndRulesUnderThem) {
  kit::Table how{.columns = {{.head = u8"KEY", .width = 120},
                             {.head = u8"COST", .width = 60, .figure = true}},
                 .gap = 10,
                 .rowGap = 5,
                 .divider = red(),
                 .dividerWidth = 2,
                 .headRuled = true};
  how.headLine = [](const sigil::compose::Utf8& t) {
    return text(t).key("head").font({.size = 11});
  };
  const std::vector<sigil::compose::Utf8> only = {u8"alpha", u8"12"};
  const std::vector<std::span<const sigil::compose::Utf8>> rows = {only};
  Host host(400, 300);
  host.composer.render(box()
                           .width(400)
                           .height(300)
                           .styleSheet(rowClasses())
                           .children({kit::table(rows, how).key("table")}));
  host.frame();
  const SkRect head = require(host.composer.bounds("head"));
  EXPECT_FLOAT_EQ(head.top(), 0);
  EXPECT_NEAR(head.height(), lineHeight(11), 1.0f);
  // The rule under the head is a row of its own, in the divider's fill.
  EXPECT_EQ(host.pixel(50, (int)(head.height() + 5) + 1), SK_ColorRED);
}

TEST(KitRows, SwatchesReserveOneColumnAcrossTheHeadAndEveryRow) {
  const Utf8 cells[] = {"marked", "12", "empty", "34", "missing", "56"};
  const SurfacePaint swatches[] = {red(), Fill::none()};
  for (const bool marked : {false, true}) {
    kit::Table how{.columns = {{.head = "NAME", .width = 120},
                               {.head = "VALUE", .width = 60}},
                   .gap = 10,
                   .swatches = marked ? std::span<const SurfacePaint>(swatches)
                                      : std::span<const SurfacePaint>{},
                   .swatchSide = 12};
    how.headLine = [](const Utf8& words) {
      return text(words).key(words == "NAME" ? "nameHead" : "valueHead");
    };
    how.cellLine = [](const Utf8& words, const kit::Table&, size_t column,
                      size_t row) {
      return text(words).key("r" + std::to_string(row) + "c" +
                             std::to_string(column));
    };
    Host host(400, 200);
    host.composer.render(
        box().children({kit::table(std::span<const Utf8>(cells), how)}));
    host.frame();
    const float start = marked ? 22.0f : 0.0f;
    EXPECT_FLOAT_EQ(require(host.composer.bounds("nameHead")).left(), start);
    EXPECT_FLOAT_EQ(require(host.composer.bounds("valueHead")).left(),
                    start + 130);
    for (size_t row = 0; row < 3; ++row) {
      EXPECT_FLOAT_EQ(
          require(host.composer.bounds("r" + std::to_string(row) + "c0"))
              .left(),
          start);
      EXPECT_FLOAT_EQ(
          require(host.composer.bounds("r" + std::to_string(row) + "c1"))
              .left(),
          start + 130);
    }
  }
}

TEST(KitRows, ABarsOwnInkStandsOverTheRowsPaintAndItsLines) {
  // WHICH row is lit is the data's business, so the inks are a run beside
  // the values; a short run leaves the rows past its end as the props
  // say.
  const std::vector<sigil::compose::Utf8> labels = {u8"a", u8"b"};
  const std::vector<double> values = {40.0, 40.0};
  const std::array<sigil::material::Color, 1> lit{{{0, 1, 0, 1}}};
  Host host(400, 200);
  host.composer.render(
      box()
          .width(400)
          .height(200)
          .styleSheet(rowClasses())
          .children({kit::bars(labels, values,
                               {.length = 100,
                                .labelMeasure = 0,
                                .barHeight = 10,
                                .bar = Fill::color({1, 0, 0, 1}),
                                .inks = lit})}));
  host.frame();
  // The first row's bar is its own ink; the second keeps the paint the
  // props named.
  EXPECT_EQ(host.pixel(60, 5), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(60, 19), SK_ColorRED);
}

TEST(KitRows, ABarRunsInProportionToTheLargestValue) {
  kit::Bars how{.length = 150,
                .labelMeasure = 40,
                .barHeight = 10,
                .gap = 8,
                .rowGap = 4};
  how.figureLine = [](double value) {
    return text(kit::formatted("%.0f", value))
        .key(value > 20 ? "big" : "small")
        .font({.size = 12});
  };
  const std::vector<sigil::compose::Utf8> labels = {u8"a", u8"b"};
  const std::vector<double> values = {37.0, 15.0};
  Host host(400, 200);
  const auto figures = [&](const kit::Bars& bars) {
    host.composer.render(box()
                             .width(400)
                             .height(200)
                             .styleSheet(rowClasses())
                             .children({kit::bars(labels, values, bars)}));
    host.frame();
    return std::pair{require(host.composer.bounds("big")).left(),
                     require(host.composer.bounds("small")).left()};
  };
  // The figure stands after the bar, so where it lands IS the bar's run:
  // the largest value takes the whole length and the rest are in
  // proportion to it.
  const auto [big, small] = figures(how);
  EXPECT_FLOAT_EQ(big, 40 + 8 + 150 + 8);
  // A run is laid out on whole pixels, so the proportion is exact to one.
  EXPECT_NEAR(small, 40 + 8 + 150.0f * 15.0f / 37.0f + 8, 1.0f);
  // A stated extent overrides the derived one.
  kit::Bars stated = how;
  stated.largest = 74.0;
  const auto [half, quarter] = figures(stated);
  EXPECT_FLOAT_EQ(half, 40 + 8 + 75 + 8);
  EXPECT_NEAR(quarter, 40 + 8 + 150.0f * 15.0f / 74.0f + 8, 1.0f);
  // A track holds the room the longest bar takes, so every figure stands
  // on one edge however short its bar.
  kit::Bars tracked = how;
  tracked.rest = Fill::color({0.2f, 0.2f, 0.2f, 1});
  const auto [first, second] = figures(tracked);
  EXPECT_FLOAT_EQ(first, 40 + 8 + 150 + 8);
  EXPECT_FLOAT_EQ(second, 40 + 8 + 150 + 8);
}
