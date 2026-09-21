// What a decoration dresses: on the default boundary the node's whole
// box, and on the glyph boundary only where the letters are.

#include "support/ParagraphTestSupport.h"

TEST(ComposeBoundary, GlyphsHandTheDecorationsTheLettersInsteadOfTheBox) {
  // A decoration that fills the outline it is handed: on the default
  // boundary it covers the node's whole box, and on the glyph boundary it
  // covers only where letters are — so a point inside the box but between
  // two lines is painted by one and not by the other.
  const auto describe = [](Boundary boundary) {
    Element leaf = text(u8"HH HH", whiteStyle(40))
                       .key("t")
                       .absolute()
                       .left(10.0f)
                       .top(10.0f)
                       .width(240.0f)
                       .foreground(Decoration(PaintProgram(
                           [](SkCanvas& canvas, const PaintContext& ctx) {
                             SkPaint paint;
                             paint.setColor(SK_ColorGREEN);
                             paint.setAntiAlias(false);
                             canvas.drawPath(ctx.outline, paint);
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
