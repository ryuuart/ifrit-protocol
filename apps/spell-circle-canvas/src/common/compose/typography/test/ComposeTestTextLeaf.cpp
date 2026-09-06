// A TEXT LEAF PLACED BY A LAYOUT: absolute and centred placement, the
// paragraph overload's mixed spans, and a leaf inside a baseline grid.
//
// The text binary's share of the content suites, one file per subject.

#include <memory>

#include "DressedTypeProbes.h"

TEST(TextLayout, FullyConstrainedAbsoluteTextPaints) {
  // Yoga skips the measure callback when absolute insets determine both
  // dimensions; the kernel must lay the paragraph out at paint time.
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      stack().child(text(u8"WWWW", style).absolute().inset(10, 10, 10, 120)));
  host.frame();
  int lit = 0;
  for (int x = 10; x < 190; x += 4)
    for (int y = 10; y < 70; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 20);  // glyph coverage, not empty
}

TEST(TextLayout, AlignItemsCentersTextLeaf) {
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(box()
                           .width(200)
                           .height(60)
                           .alignItems(Align::Center)
                           .child(text(u8"W", style)));
  host.frame();

  int litLeft = 0, litMiddle = 0;
  for (int x = 0; x < 50; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litLeft++;
  for (int x = 75; x < 125; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litMiddle++;
  EXPECT_EQ(litLeft, 0);    // nothing hugging the start edge
  EXPECT_GT(litMiddle, 5);  // the glyph sits in the middle
}

TEST(TextLayout, ParagraphOverloadPaintsMixedSpans) {
  Host host(400, 200);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  sigil::weave::TextStyle big = styleAt(40);
  big.paint.foreground.setColor(SK_ColorWHITE);
  sigil::weave::TextStyle small = styleAt(16);
  small.paint.foreground.setColor(SK_ColorWHITE);
  para->appendText(u8"BIG", big);
  para->appendText(u8" and small", small);

  host.composer.render(box().padding(10).child(text(para).key("spans")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("spans");
  ASSERT_NE(layout, nullptr);
  int lit = 0;
  for (int x = 10; x < 390; x += 3)
    for (int y = 10; y < 70; y += 3)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 15);  // both spans shaped and painted
}

TEST(ComposeLayouts, BaselineGridRendersInsideStackedAbsoluteColumn) {
  // Text inside a BaselineGrid, nested in an absolute column, inside a
  // stack(). A custom layout scheme writes back into Yoga out of band, and
  // an absolute ancestor changes how its subtree is sized, so this is the
  // combination most likely to leave the text laid out at zero size and
  // therefore invisible.
  Host host;
  host.composer.render(stack().child(
      box()
          .column()
          .absolute()
          .inset(10, 10, 10, 10)
          .child(layout(layouts::BaselineGrid{.rhythm = 24})
                     .width(pct(100))
                     .child(text(u8"probe", whiteStyle(28)).key("p")))));
  host.frame();
  auto b = host.composer.bounds("p");
  ASSERT_TRUE(b.has_value());
  EXPECT_GT(b->width(), 5.0f) << "placed rect " << b->left() << "," << b->top()
                              << " " << b->width() << "x" << b->height();
  EXPECT_GT(b->height(), 5.0f);
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())))
      << "placed rect " << b->left() << "," << b->top() << " " << b->width()
      << "x" << b->height();
}
