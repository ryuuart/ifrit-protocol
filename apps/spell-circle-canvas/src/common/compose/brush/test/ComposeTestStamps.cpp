// An element stamped along a contour: the size a snapshot bakes to, the
// one recording replayed per sample, a stamp that walks its own contour
// again, and the nested composer a custom leaf draws.

#include "support/BrushTestSupport.h"

TEST(ComposeStamps, SnapshotBakesIntrinsicSize) {
  sk_sp<SkPicture> pic = snapshot(
      box().row().gap(4).children({box().width(20).height(12).fill(red()),
                                   box().width(20).height(12).fill(green())}),
      fonts());
  ASSERT_NE(pic, nullptr);
  EXPECT_FLOAT_EQ(pic->cullRect().width(), 44.0f);   // 20 + 4 + 20
  EXPECT_FLOAT_EQ(pic->cullRect().height(), 12.0f);  // content height
}

TEST(ComposeStamps, StampRecordsOnceReplaysPerSample) {
  static int stampDescribes;
  stampDescribes = 0;
  Host host;

  ContourWalk vine;
  vine.spacing = 25.0f;
  vine.stamp =
      custom([](SkCanvas& c, const PaintContext& ctx) {
        ++stampDescribes;
        SkPaint p;
        p.setColor(SK_ColorYELLOW);
        c.drawRect(SkRect::MakeWH(ctx.size.width(), ctx.size.height()), p);
      })
          .width(12)
          .height(12);

  host.composer.render(box().children(
      {box()
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

TEST(ComposeStamps, RecursiveStampWalksItsOwnContour) {
  // Two levels of recursion: the stamp is itself decorated by a ContourWalk
  // dotting its own outline. This terminates because a stamp may only
  // decorate art that is already baked, never itself — so the nesting is
  // finite by construction rather than by a depth limit.
  Host host;
  ContourWalk dots;
  dots.spacing = 6.0f;
  dots.draw = [](SkCanvas& c) {
    SkPaint p;
    p.setColor(SK_ColorCYAN);
    c.drawRect(SkRect::MakeXYWH(-1, -1, 2, 2), p);
  };

  ContourWalk outer;
  outer.spacing = 40.0f;
  outer.stamp = box().width(16).height(16).fill(red()).foreground(dots);

  host.composer.render(box().children(
      {box()
           .width(120)
           .height(120)
           .inset(40)
           .absolute()
           .foreground(outer)}));
  host.frame();
  int redPx = 0, cyanPx = 0;
  for (int x = 0; x < 200; x += 2)
    for (int y = 0; y < 200; y += 2) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorRED)
        redPx++;
      else if (c == SK_ColorCYAN)
        cyanPx++;
    }
  EXPECT_GT(redPx, 20);   // stamps landed
  EXPECT_GT(cyanPx, 10);  // and their own walked borders too
}

TEST(ComposeStamps, CustomLeafDrawsNestedComposer) {
  // A custom() leaf hosting an entire nested Composer: the recursion closes
  // at the paint phase, with the inner composer owning its own ticker and
  // size and drawing straight onto the outer canvas.
  Host host;
  auto nestedTicker = std::make_shared<sigil::motion::Ticker>();
  auto nested = std::make_shared<Composer>(*nestedTicker, fonts());
  nested->setSize({60, 60});
  nested->render(box().padding(10).fill(green()).children(
      {box().flexGrow(1).fill(red())}));

  host.composer.render(box().children(
      {custom([nested, nestedTicker](SkCanvas& c) {
         nested->draw(c);
       })
           .width(60)
           .height(60)
           .cache(Cache::None)}));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // nested padding ring
  EXPECT_EQ(host.pixel(30, 30), SK_ColorRED);  // nested content
}
