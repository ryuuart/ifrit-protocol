// kit/Legibility.h — what keeps type readable over a ground it does not
// control: the underlay a halo adds, the offset fill a shade is, the
// padding a scrim grows a run by, and the immediate-mode halo a custom
// leaf draws with.

#include <include/core/SkFont.h>
#include <sigilcompose/kit/Legibility.h>

#include <utility>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitLegibility, HaloedAddsExactlyOneUnderlayAndLeavesTheInputAlone) {
  sigil::weave::TextStyle base;
  base.shaping.fontSize = 12;
  const size_t before = base.paint.underlays.size();
  const sigil::weave::TextStyle out =
      kit::haloed(base, {.colour = {1, 1, 1, 1}, .width = 2.2f});
  EXPECT_EQ(base.paint.underlays.size(), before) << "input was mutated";
  ASSERT_EQ(out.paint.underlays.size(), before + 1);
  const SkPaint& p = out.paint.underlays.back().paint;
  EXPECT_EQ(p.getStyle(), SkPaint::kStroke_Style);
  EXPECT_FLOAT_EQ(p.getStrokeWidth(), 2.2f);
  EXPECT_EQ(p.getStrokeJoin(), SkPaint::kRound_Join);
}

TEST(KitLegibility, ShadeIsAnOffsetFillNotAStroke) {
  sigil::weave::TextStyle base;
  const sigil::weave::TextStyle out =
      kit::shaded(base, {.colour = {0, 0, 0, 0.9f}, .offset = {1, 1}});
  ASSERT_EQ(out.paint.underlays.size(), 1u);
  EXPECT_EQ(out.paint.underlays[0].paint.getStyle(), SkPaint::kFill_Style);
  EXPECT_FLOAT_EQ(out.paint.underlays[0].offset.fX, 1.0f);
}

TEST(KitLegibility, ScrimGrowsTheRunByItsPadding) {
  sigil::weave::TextStyle st;
  st.shaping.fontSize = 12;
  const SkSize bare =
      intrinsicSize(box().children({text(u8"NAVI", st)}), fonts());
  const SkSize plated = intrinsicSize(
      box().children(
          {kit::scrim(text(u8"NAVI", st), {.paddingX = 3, .paddingY = 4})}),
      fonts());
  EXPECT_FLOAT_EQ(plated.width(), bare.width() + 6);
  EXPECT_FLOAT_EQ(plated.height(), bare.height() + 8);
}

TEST(KitLegibility, DrawHaloedPutsGroundColourAroundTheInk) {
  // The immediate-mode spelling, which exists because a caption inside a
  // custom() leaf cannot reach addUnderlay.
  //
  // The comparison has to be DIFFERENTIAL — halo render against no-halo
  // render — because no absolute pixel count is a valid criterion here.
  // "More halo than ink" is false at ordinary sizes, since glyph interiors
  // outnumber a thin surrounding ring. What is true regardless of size is
  // that the halo colour appears only when a halo was asked for, and that
  // the ink survives it.
  auto render = [](bool halo) {
    sk_sp<SkSurface> s =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(160, 40));
    s->getCanvas()->clear(SkColorSetARGB(255, 128, 128, 128));
    SkFont font(sigil::weave::ports::pickTypeface({"Menlo", "Courier New"}),
                20.0f);
    if (halo)
      kit::drawHaloed(*s->getCanvas(), "HALO", {10, 28}, font,
                      SkColor4f{1, 1, 1, 1},
                      {.colour = {0, 0, 0, 1}, .width = 3.0f});
    else
      kit::drawHaloed(*s->getCanvas(), "HALO", {10, 28}, font,
                      SkColor4f{1, 1, 1, 1},
                      {.colour = {0, 0, 0, 0}, .width = 0.0f});
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(160, 40));
    s->readPixels(bm.pixmap(), 0, 0);
    int white = 0, black = 0;
    for (int y = 0; y < 40; ++y)
      for (int x = 0; x < 160; ++x) {
        const int r = (int)SkColorGetR(bm.getColor(x, y));
        white += r > 240;
        black += r < 16;
      }
    return std::pair<int, int>{white, black};
  };
  const auto plain = render(false);
  const auto haloed = render(true);
  EXPECT_GT(plain.first, 0) << "no ink at all";
  EXPECT_EQ(plain.second, 0) << "knockout appeared without being asked for";
  EXPECT_GT(haloed.second, 0) << "the halo did not paint";
  // A knockout ring surrounds the ink, so it must not eat it.
  EXPECT_GT(haloed.first, plain.first / 2)
      << "the halo painted over the ink instead of under it";
}
