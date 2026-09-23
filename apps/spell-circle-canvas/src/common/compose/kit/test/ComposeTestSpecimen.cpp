// kit/Specimen.h — the captioned cell, the run of cells, the well they
// stand in and the sheet that heads and foots them.
//
// The claims are about WHERE THE PARTS LAND, so every case lays out through
// a composer and reads the keyed boxes back, rather than restating the
// margins the component spells.

#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Specimen.h>

#include <optional>
#include <utility>

#include "support/KitType.h"
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

TEST(KitSpecimen, TheCaptionsLinesStandWhereTheVoiceSays) {
  const float label = lineHeight(12);
  const float note = lineHeight(10);
  // The body's top and the cell's height, for one arrangement.
  const auto placed = [&](kit::Caption::Where where, bool withNote) {
    Host host(300, 300);
    host.composer.render(
        box()
            .width(300)
            .height(300)
            .applyStyleSheet(captionClasses())
            .children({kit::cell(specimenVoice(where), "LABEL",
                                 withNote ? "a note" : "",
                                 box().key("body").width(100).height(40))
                           .key("cell")}));
    host.frame();
    return std::pair{host.composer.bounds("body").value().top(),
                     host.composer.bounds("cell").value().height()};
  };
  const auto expectPlaced = [&](const char* name, kit::Caption::Where where,
                                bool withNote, float expectedTop,
                                float expectedHeight, float tolerance = 1.5f) {
    SCOPED_TRACE(name);
    const auto [bodyTop, cellHeight] = placed(where, withNote);
    EXPECT_NEAR(bodyTop, expectedTop, tolerance);
    EXPECT_NEAR(cellHeight, expectedHeight, tolerance);
  };
  // Split: the label over the body, the note under it.
  expectPlaced("split", kit::Caption::Where::Split, true, label + 6,
               label + 6 + 40 + 6 + note);
  // Above: both lines over the body, the note gap between them.
  expectPlaced("above", kit::Caption::Where::Above, true, label + 4 + note + 6,
               label + 4 + note + 6 + 40);
  // Below: the body first, then both lines.
  expectPlaced("below", kit::Caption::Where::Below, true, 0.0f,
               40 + 6 + label + 4 + note);
  // An absent note spends no gap: the cell ends at the body.
  expectPlaced("split without note", kit::Caption::Where::Split, false,
               label + 6, label + 6 + 40, 1.0f);
}

TEST(KitSpecimen, AMeasureKeepsALongLabelFromWideningItsCell) {
  // The point of a specimen sheet is that its cells line up. A label
  // wider than the body it captions widens the cell it is in and no
  // other, so a run of them stops lining up at whichever cell happens to
  // carry the longest call.
  const auto width = [](float labelMeasure) {
    kit::Caption voice = specimenVoice(kit::Caption::Where::Split);
    voice.labelMeasure = labelMeasure;
    return intrinsicSize(
               kit::cell(voice, "a label far wider than the body under it", "",
                         box().width(60).height(40))
                   .applyStyleSheet(captionClasses()),
               fonts())
        .width();
  };
  EXPECT_GT(width(0), 60.0f);         // unmeasured: the label decides
  EXPECT_FLOAT_EQ(width(60), 60.0f);  // measured: the body does
}

TEST(KitSpecimen, APartSetsOneLineWithTheParametersItNames) {
  // The label as a function of its text: the leaf it returns stands where
  // the register's would, and the body under it moves down by exactly the
  // taller line.
  kit::Caption voice = specimenVoice(kit::Caption::Where::Split);
  voice.label = [](const sigil::compose::Utf8& t) {
    return text(t).key("label").font({.size = 20});
  };
  Host host(300, 300);
  host.composer.render(
      box()
          .width(300)
          .height(300)
          .applyStyleSheet(captionClasses())
          .children({kit::cell(voice, "LABEL", "a note",
                               box().key("body").width(100).height(40))
                         .key("cell")}));
  host.frame();
  EXPECT_NEAR(host.composer.bounds("label").value().height(), lineHeight(20),
              1.5f);
  EXPECT_NEAR(host.composer.bounds("body").value().top(), lineHeight(20) + 6,
              1.5f);

  // A part takes a prefix of what the caption offers: nothing, the text,
  // or the text and the caption itself.
  int calls = 0;
  using CaptionPart = kit::Part<sigil::compose::Utf8, kit::Caption>;
  const CaptionPart none = [&] {
    ++calls;
    return box();
  };
  const CaptionPart one = [&](const sigil::compose::Utf8& t) {
    calls += t.empty() ? 100 : 1;
    return text(t);
  };
  const CaptionPart two = [&](const sigil::compose::Utf8&,
                              const kit::Caption& c) {
    calls += c.where == kit::Caption::Where::Split ? 1 : 100;
    return box();
  };
  none("x", voice);
  one("x", voice);
  two("x", voice);
  EXPECT_EQ(calls, 3);
  EXPECT_TRUE(static_cast<bool>(none));
  EXPECT_FALSE(static_cast<bool>(CaptionPart{}));
}

TEST(KitSpecimen, AWellAppliesTheCallersSizeGroundAndPadding) {
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).children({kit::well(
      {.width = 100, .height = 80, .ground = red(), .padding = 10},
      box().key("well").children(
          {box().key("body").width(20).height(15).fill(green())}))}));
  host.frame();

  const auto well = host.composer.bounds("well");
  const auto body = host.composer.bounds("body");
  ASSERT_TRUE(well.has_value());
  ASSERT_TRUE(body.has_value());
  EXPECT_EQ(*well, SkRect::MakeWH(100, 80));
  EXPECT_FLOAT_EQ(body->left(), 10);
  EXPECT_FLOAT_EQ(body->top(), 10);
  EXPECT_EQ(host.pixel(1, 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(11, 11), SK_ColorGREEN);
}

TEST(KitSpecimen, AWellClipsByDefaultAndCanBeOpened) {
  const auto specimen = [](bool clip) {
    return kit::well(
        {.width = 100, .height = 80, .clip = clip},
        box().children(
            {box().absolute().left(90).top(20).width(30).height(20).fill(
                green())}));
  };
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).children({specimen(true)}));
  host.frame();
  EXPECT_EQ(host.pixel(95, 25), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(105, 25), SK_ColorBLACK);

  host.composer.render(
      box().width(160).height(120).children({specimen(false)}));
  host.frame();
  EXPECT_EQ(host.pixel(105, 25), SK_ColorGREEN);
}

TEST(KitSpecimen, AWellRoundsItsCornersAndRulesInsideItsOwnBox) {
  // A keyline centred on the boundary would put half its width outside,
  // and a plate that is not the width it was given is the one thing a
  // fixed surface may not be — so the rule is drawn inside the well.
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).children({kit::well(
      {.width = 100,
       .height = 80,
       .ground = red(),
       .padding = 10,
       .paddingY = 4,
       .corners = 0,
       .keyline = Fill::color({0, 1, 0, 1}),
       .keylineWidth = 4},
      box().key("well").children({box().key("body").width(20).height(15)}))}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("well")), SkRect::MakeWH(100, 80));
  // A plate is often set tighter down than across, which one distance
  // cannot say.
  const SkRect body = require(host.composer.bounds("body"));
  EXPECT_FLOAT_EQ(body.left(), 10);
  EXPECT_FLOAT_EQ(body.top(), 4);
  EXPECT_EQ(host.pixel(50, 1), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(50, 6), SK_ColorRED);
  EXPECT_EQ(host.pixel(101, 40), SK_ColorBLACK);
}

TEST(KitSpecimen, ACellStatesItsBodysOwnWell) {
  // A cell that states its body's cell is one call rather than two. With
  // no alignment the body IS the well; with one it is HELD by it, at the
  // measure it was built with.
  const auto placed = [](Align justify) {
    kit::Caption voice = specimenVoice(kit::Caption::Where::Split);
    voice.body = kit::Well{.width = 100, .height = 80, .ground = red()};
    voice.justify = justify;
    Host host(200, 200);
    host.composer.render(
        box()
            .width(200)
            .height(200)
            .applyStyleSheet(captionClasses())
            .children({kit::cell(
                voice, "", "",
                box().key("body").width(20).height(16).fill(green()))}));
    host.frame();
    return require(host.composer.bounds("body"));
  };
  // The spec is written onto the body: its own 20 x 16 is overruled.
  EXPECT_EQ(placed(Align::Auto), SkRect::MakeWH(100, 80));
  // Held: the body keeps its measure and stands in the middle of the
  // plate both ways.
  EXPECT_EQ(placed(Align::Center), SkRect::MakeXYWH(40, 32, 20, 16));
  EXPECT_EQ(placed(Align::Start), SkRect::MakeXYWH(0, 0, 20, 16));
}

TEST(KitSpecimen, AWellHoldsWhatItIsHandedWhereItsContentSaysTo) {
  // The two readings of one call: with no content the picture BECOMES the
  // plate, with content the plate holds it at its own measure.
  const auto shown = [](std::optional<kit::Well::Content> content) {
    Host host(200, 200);
    host.composer.render(box().width(200).height(200).children({kit::well(
        {.width = 100, .height = 80, .ground = red(), .content = content},
        box().key("picture").width(30).height(20).fill(green()))}));
    host.frame();
    return require(host.composer.bounds("picture"));
  };
  EXPECT_EQ(shown(std::nullopt), SkRect::MakeWH(100, 80));
  EXPECT_EQ(shown(kit::Well::Content{}), SkRect::MakeXYWH(35, 30, 30, 20));
  EXPECT_EQ(
      shown(kit::Well::Content{.across = Align::End, .down = Justify::End}),
      SkRect::MakeXYWH(70, 60, 30, 20));
  // The empty spelling is ranged too, so a plate whose children are added
  // fluently holds them in the middle.
  Host host(200, 200);
  host.composer.render(box().width(200).height(200).children(
      {kit::well({.width = 100, .height = 80, .content = kit::Well::Content{}})
           .children({box().key("late").width(30).height(20)})}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("late")),
            SkRect::MakeXYWH(35, 30, 30, 20));
}

TEST(KitSpecimen, AReadingStandsOverTheBodyOnAScrimOfItsGround) {
  kit::Caption voice = specimenVoice(kit::Caption::Where::Split);
  voice.gap = 8;
  voice.body = kit::Well{.width = 100, .height = 80, .ground = green()};
  voice.reading = u8"12";
  voice.readingLine = [](const sigil::compose::Utf8& t) {
    return text(t).key("figure").font({.size = 10});
  };
  Host host(200, 200);
  host.composer.render(
      box()
          .width(200)
          .height(200)
          .applyStyleSheet(captionClasses())
          .children({kit::cell(voice, "", "",
                               box().absolute().inset(0).fill(red()))}));
  host.frame();
  // The line stands inside the scrim's own air, one gap in from the
  // body's corner.
  const SkRect figure = require(host.composer.bounds("figure"));
  EXPECT_FLOAT_EQ(figure.left(), 16);
  EXPECT_FLOAT_EQ(figure.top(), 12);
  // The body fills the whole well, and the reading stands off it on a
  // scrim of the well's own ground.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(9, 9), SK_ColorGREEN);
  EXPECT_EQ(host.pixel((int)figure.right() + 4, (int)figure.bottom() + 2),
            SK_ColorGREEN);
}

TEST(KitSpecimen, ASheetsLinesArePartsOfTheirOwnText) {
  kit::Sheet page{.title = "TITLE",
                  .footer = "the footer",
                  .marginX = 30,
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

TEST(KitSpecimen, ASheetRulesOffItsHeaderAndFooterAndFootsThePage) {
  const float title = lineHeight(15);
  const float footer = lineHeight(11);
  kit::Sheet page{.title = "TITLE",
                  .footer = "the footer",
                  .marginX = 30,
                  .marginTop = 16,
                  .marginBottom = 14,
                  .contentGap = 18,
                  .rule = red(),
                  .ruleWidth = 2,
                  .key = "page"};
  // Both arrangements use the same document typography, so toggling the
  // rules is the only change that can move the content.
  const sigil::compose::StyleSheet typography{
      sigil::compose::rule("h1").font({.size = 15}),
      sigil::compose::rule("footer").font({.size = 11})};
  Host host(400, 300);
  host.composer.render(kit::sheet(page, box().key("body"))
                           .applyStyleSheet(typography)
                           .width(400)
                           .height(300));
  host.frame();
  const SkRect content = host.composer.bounds("page-content").value();
  // The content stands one content gap under the title, inside the side
  // margins, and one content gap over the footer, which sits on the bottom
  // margin.
  EXPECT_NEAR(content.top(), 16 + title + 18, 1.5f);
  EXPECT_FLOAT_EQ(content.left(), 30.0f);
  EXPECT_FLOAT_EQ(content.right(), 370.0f);
  const SkRect foot = host.composer.bounds("page-footer").value();
  EXPECT_NEAR(foot.bottom(), 300 - 14, 1.5f);
  EXPECT_NEAR(foot.height(), footer, 1.0f);
  EXPECT_NEAR(content.bottom(), foot.top() - 18, 1.5f);
  // The rule bisects that gap and is drawn full width.
  const SkRect rule = host.composer.bounds("page-head-rule").value();
  EXPECT_FLOAT_EQ(rule.height(), 2.0f);
  EXPECT_NEAR(rule.top(), 16 + title + 8, 1.5f);
  EXPECT_EQ(host.pixel(200, (int)rule.top() + 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(31, (int)rule.top() + 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(200, (int)rule.top() - 2), SK_ColorBLACK);

  // Unruled, the content lands at the same place: a rule bisects the gap
  // rather than adding to it.
  kit::Sheet plain = page;
  plain.rule = Fill::none();
  plain.key = "plain";
  host.composer.render(kit::sheet(plain, box().key("body"))
                           .applyStyleSheet(typography)
                           .width(400)
                           .height(300));
  host.frame();
  EXPECT_NEAR(host.composer.bounds("plain-content").value().top(),
              content.top(), 1.0f);
  EXPECT_FALSE(host.composer.bounds("plain-head-rule").has_value());
}

TEST(KitSpecimen, PanelsShareWidthAndKeepTheLastRowAligned) {
  Host host(320, 200);
  host.composer.render(box().children({kit::panelGrid(
      {.cells = {box().key("a").width(240).height(20),
                 box().key("b").width(10).height(30), box().key("c").height(10),
                 box().key("d").height(15)},
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
}

TEST(KitSpecimen, AGridToldItsMeasureCutsTheSharesFromThatWidth) {
  // A grid whose parent sizes from its CONTENT has no width to divide,
  // and its cells would be dealt nothing and drawn over each other.
  Host host(400, 200);
  const auto shares = [&](Dimension measure) {
    host.composer.render(
        box().width(400).height(200).row().children({kit::panelGrid(
            {.cells = {box().key("a").height(10), box().key("b").height(10)},
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
}

TEST(KitSpecimen, PanelDividersKeepTheirWidthAcrossWrappedRows) {
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
}

TEST(KitSpecimen, SurfacePaintPreservesBindingsAndMaterialResolution) {
  choreograph::Output<Fill> ink(Fill::color({1, 0, 0, 1}));
  Host host(200, 100);
  host.composer.render(box().children(
      {kit::well({.width = 100, .height = 60, .ground = &ink})}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
  ink = Fill::color({0, 1, 0, 1});
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
  EXPECT_EQ(SurfacePaint(&ink), SurfacePaint(&ink));
  const SurfacePaint paint = material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 1}}});
  for (float width : {80.0f, 160.0f}) {
    host.composer.render(box().children(
        {kit::well({.width = width, .height = 60, .ground = paint})}));
    host.frame();
    const SkColor middle = host.pixel((int)(width / 2), 20);
    EXPECT_NEAR(SkColorGetR(middle), SkColorGetB(middle), 5);
  }
}

TEST(KitSpecimen, APlacedWellHoldsChildrenThatKeepTheirOwnRects) {
  Host host(120, 90);
  // Every child of a placed well is absolute, so the two below share the
  // well's box instead of stacking down it.
  host.composer.render(box().width(120).height(90).children(
      {kit::well({.width = 100, .height = 60, .ground = red(), .placed = true})
           .key("plate")
           .children({box().key("a").width(30).height(20),
                      box().key("b").width(30).height(20)})}));
  host.frame();
  EXPECT_EQ(require(host.composer.bounds("a")).top(),
            require(host.composer.bounds("b")).top());
  EXPECT_EQ(require(host.composer.bounds("plate")),
            SkRect::MakeXYWH(0, 0, 100, 60));
}
