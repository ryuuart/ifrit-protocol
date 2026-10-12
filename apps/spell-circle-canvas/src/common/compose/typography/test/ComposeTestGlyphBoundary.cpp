// What a decoration dresses: on the default boundary the node's whole
// box, and on the glyph boundary only where the letters are.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>

#include "support/ParagraphTestSupport.h"

TEST(ComposeBoundary, GlyphsHandTheDecorationsTheLettersInsteadOfTheBox) {
  // A decoration that fills the outline it is handed: on the default
  // boundary it covers the node's whole box, and on the glyph boundary it
  // covers only where letters are — so a point inside the box but between
  // two lines is painted by one and not by the other.
  const auto describe = [](Boundary boundary) {
    Element leaf =
        text(u8"HH HH", whiteStyle(40))
            .key("t")
            .absolute()
            .left(10.0f)
            .top(10.0f)
            .width(240.0f)
            .foreground(Decoration(PaintProgram(
                [](sigil::draw::Pen& pen, const PaintContext& ctx) {
                  SkCanvas& canvas = *pen.canvas();
                  SkPaint paint;
                  paint.setColor(SK_ColorGREEN);
                  paint.setAntiAlias(false);
                  canvas.drawPath(geometry::path::toSk(ctx.outline), paint);
                })));
    if (boundary != Boundary::Auto) leaf.decorationOutline(boundary);
    return box().children({std::move(leaf)});
  };
  Host boxed(300, 200), lettered(300, 200);
  boxed.composer.render(describe(Boundary::Auto));
  boxed.frame();
  lettered.composer.render(describe(Boundary::Glyphs));
  lettered.frame();

  // The node's top-left corner: inside its box, and above every letter.
  EXPECT_EQ(boxed.pixel(12, 12), SK_ColorGREEN);
  EXPECT_NE(lettered.pixel(12, 12), SK_ColorGREEN);
  // …and the letters themselves are painted under both.
  EXPECT_TRUE(anyGreenIn(lettered, SkIRect::MakeXYWH(10, 10, 240, 60)));
}

namespace {

const std::u8string& aroundTheLetter() {
  static const std::u8string body = [] {
    std::u8string all;
    for (int i = 0; i < 12; ++i) all += u8"wave around the letter and on ";
    return all;
  }();
  return body;
}

/** A paragraph flowing round a keyed letter "O" set at 150 px: in a box of
 *  @p size when one is given, measured when not, its glyphs or its box
 *  the edge, lined up as @p alignment says. */
Element flowRoundLetter(std::optional<SkSize> size, Boundary boundary,
                        sigil::weave::TextAlignment alignment =
                            sigil::weave::TextAlignment::kStart) {
  Text letter = text(u8"O", whiteStyle(150))
                    .key("obstacle")
                    .absolute()
                    .left(10.0f)
                    .top(10.0f)
                    .textAlign(alignment);
  if (size) letter.width(size->width()).height(size->height());
  if (boundary != Boundary::Auto) letter.decorationOutline(boundary);
  return stack().children(
      {std::move(letter), box()
                              .inset(0)
                              .children({text(aroundTheLetter(), whiteStyle(14))
                                             .key("body")
                                             .contentFlowAround("obstacle", 2)})
                              .zIndex(1)});
}

/** Where the flowing paragraph's runs stand in @p host. */
std::vector<glm::vec2> bodyRuns(Host& host) {
  std::vector<glm::vec2> origins;
  if (const auto* layout = host.composer.paragraphLayout("body"))
    for (const sigil::weave::PositionedRun& run : layout->runs)
      origins.push_back(run.origin);
  return origins;
}

/** Where the flowing paragraph's runs stand after one frame of @p tree. */
std::vector<glm::vec2> flowedRuns(const Element& tree) {
  Host host(420, 300);
  host.composer.render(tree);
  host.frame();
  return bodyRuns(host);
}

}  // namespace

TEST(ComposeBoundary, AFixedSizeLetterExcludesItsGlyphsOnItsFirstFrame) {
  // A box of a stated size is never measured, so its glyphs are placed
  // only when something reads them. The flow reads them before the
  // painter does: it must see the letter, not the empty placement left
  // before the first layout, which would exclude the box.
  const auto fixed =
      flowedRuns(flowRoundLetter(SkSize::Make(150, 190), Boundary::Glyphs));
  const auto measured =
      flowedRuns(flowRoundLetter(std::nullopt, Boundary::Glyphs));
  const auto boxed =
      flowedRuns(flowRoundLetter(SkSize::Make(150, 190), Boundary::Auto));
  ASSERT_FALSE(fixed.empty());
  EXPECT_EQ(fixed, measured)
      << "a stated size changed where the letter's glyphs stand";
  EXPECT_NE(fixed, boxed) << "the letter excluded its box, not its glyphs";
}

TEST(ComposeBoundary, ACentredLetterExcludesWhereItsResizedBoxPutsIt) {
  // A centred letter stands in the middle of whatever box it is given, so
  // a resize moves its glyphs and the exclusion with them, exactly as a
  // first frame at the new size places them.
  Host host(420, 300);
  const auto centred = [](float width) {
    return flowRoundLetter(SkSize::Make(width, 190), Boundary::Glyphs,
                           sigil::weave::TextAlignment::kCenter);
  };
  host.composer.render(centred(150));
  host.frame();
  const auto narrow = bodyRuns(host);
  host.composer.render(centred(260));
  host.frame();
  const auto resized = bodyRuns(host);
  EXPECT_NE(resized, narrow) << "the exclusion stayed where the letter stood";
  EXPECT_EQ(resized, flowedRuns(centred(260)));
}

TEST(ComposeBoundary, ABevelOnAnOverlaidCopyStaysOnTheGlyphs) {
  // The ink and a stationary bevel composed apart: the word in its ink,
  // and over it the same word in a clear ink whose material carries only
  // the bevel. The copy's bevel stands on the letters, so nothing it adds
  // lands off them.
  const auto word = [](const char* key, material::Material ink) {
    return text(u8"HO", whiteStyle(110))
        .key(key)
        .absolute()
        .left(20.0f)
        .top(20.0f)
        .width(300.0f)
        .height(150.0f)
        .decorationOutline(Boundary::Glyphs)
        .ink(std::move(ink));
  };
  const material::Material inked = material::from(material::Color{1, 1, 1, 1});
  const material::Material bevelOnly =
      material::from(material::Color{0, 0, 0, 0})
          .effects(material::Filter::bevel({.depth = 3,
                                            .size = 4,
                                            .highlight = {1, 0, 0, 1},
                                            .shadow = {0, 0, 1, 1}}));
  Host alone(360, 200), dressed(360, 200);
  alone.composer.render(
      box().fill(Fill::color({0, 0, 0, 1})).children({word("word", inked)}));
  alone.frame();
  dressed.composer.render(
      box()
          .fill(Fill::color({0, 0, 0, 1}))
          .children({word("word", inked), word("bevel", bevelOnly)}));
  dressed.frame();
  int offTheLetters = 0, onTheLetters = 0;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 360; ++x) {
      const bool letter = alone.pixel(x, y) != SK_ColorBLACK;
      const bool changed = dressed.pixel(x, y) != alone.pixel(x, y);
      offTheLetters += changed && !letter;
      onTheLetters += changed && letter;
    }
  EXPECT_GT(onTheLetters, 100) << "the copy's bevel drew nothing";
  EXPECT_EQ(offTheLetters, 0) << "the copy's bevel left the glyphs";
}

namespace {

/** "I H" in a clear ink whose material carries only a bevel in red and
 *  blue, on the glyph boundary; the H, the last word, under
 *  a track widening it to twice its width at @p progress, where one is
 *  asked for. */
Element bevelledWord(bool tracked, float progress) {
  Text word =
      text(u8"I H", whiteStyle(90))
          .key("word")
          .absolute()
          .left(20.0f)
          .top(20.0f)
          .decorationOutline(Boundary::Glyphs)
          .ink(material::from(material::Color{0, 0, 0, 0})
                   .effects(material::Filter::bevel({.depth = 3,
                                                     .size = 3,
                                                     .highlight = {1, 0, 0, 1},
                                                     .shadow = {0, 0, 1, 1}})));
  if (tracked)
    word.textFx({.where = sigil::weave::selectors::word(1),
                 .effect = textFx::tween({.to = GlyphModifier{.scaleX = 2.0f}}),
                 .progress = progress});
  return box().fill(Fill::color({0, 0, 0, 1})).children({std::move(word)});
}

/** The rightmost column the bevel's red or blue stands in, whatever
 *  else is painted. */
int rightmostPainted(Host& host, int width, int height) {
  int right = -1;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const SkColor colour = host.pixel(x, y);
      const int red = (int)SkColorGetR(colour),
                green = (int)SkColorGetG(colour),
                blue = (int)SkColorGetB(colour);
      if (red - green > 60 || blue - green > 60) right = std::max(right, x);
    }
  return right;
}

}  // namespace

TEST(ComposeBoundary, ABevelFollowsATrackThatWidensItsGlyph) {
  // The bevel dresses the glyphs where they are drawn: the H widened to
  // twice its width carries its bevel out to its widened edge, where the
  // letter at rest ends half a letter sooner.
  Host rest(400, 160), widened(400, 160);
  rest.composer.render(bevelledWord(false, 0));
  rest.frame();
  widened.composer.render(bevelledWord(true, 1));
  widened.frame();
  const int restEdge = rightmostPainted(rest, 400, 160);
  ASSERT_GT(restEdge, 0) << "the bevel drew nothing";
  EXPECT_GT(rightmostPainted(widened, 400, 160), restEdge + 10)
      << "the bevel stayed on the letter at rest";
}

TEST(ComposeBoundary, ABevelUnderATrackAtRestIsTheBevelOfStillText) {
  // A track whose glyphs stand at rest moves nothing, so the outline is
  // the one the layout cut and the picture is still text's, pixel for
  // pixel.
  Host still(400, 160), resting(400, 160);
  still.composer.render(bevelledWord(false, 0));
  still.frame();
  resting.composer.render(bevelledWord(true, 0));
  resting.frame();
  EXPECT_TRUE(identicalPixels(still, resting, 400, 160));
}
