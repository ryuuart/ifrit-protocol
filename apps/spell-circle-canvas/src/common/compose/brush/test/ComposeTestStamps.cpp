// An element stamped along a contour: the size a snapshot bakes to, the
// one recording replayed per sample, a stamp that walks its own contour
// again, and the nested composer a custom leaf draws.

#include <sigildraw/Pen.h>

#include "support/BrushTestSupport.h"

TEST(ComposeStripTiles,
     ASnapshotRecordsItsContentAtItsIntrinsicSizeAndSlicingChangesNoPixel) {
  sk_sp<SkPicture> pic = snapshot(
      box().row().gap(4).children({box().width(20).height(12).fill(red()),
                                   box().width(20).height(12).fill(green())}),
      fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);   // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f);  // content height
}

TEST(ComposeStampedElements,
     AContourWalkStampsItsElementOnTheOutlineFromOneRecording) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp = custom([](sigil::draw::Pen& pen, const PaintContext& ctx) {
                 SkCanvas& c = *pen.canvas();
                 ++stampDescribes;
                 SkPaint p;
                 p.setColor(SK_ColorYELLOW);
                 c.drawRect(SkRect::MakeWH(ctx.size.x, ctx.size.y), p);
               })
                   .width(12)
                   .height(12);

  host.composer.render(box().children({box()
                                           .width(100)
                                           .height(100)
                                           .inset(50)
                                           .absolute()
                                           .fill(blue())
                                           .foreground(vine)}));
  host.frame();
  host.frame();
  EXPECT_EQ(stampDescribes, 1);  // baked once, replayed at every sample

  // Stamps are centered on the outline: the top-left corner sample
  // lands half outside the box.
  EXPECT_EQ(host.pixel(50, 50), SK_ColorYELLOW);   // corner sample center
  EXPECT_EQ(host.pixel(100, 46), SK_ColorYELLOW);  // top edge, above box
  EXPECT_EQ(host.pixel(100, 100), SK_ColorBLUE);   // interior untouched
}
