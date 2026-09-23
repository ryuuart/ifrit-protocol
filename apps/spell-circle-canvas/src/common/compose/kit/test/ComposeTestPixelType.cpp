// kit/PixelType.h — the bitmap face baked from a vector one: the pad that
// keeps the last glyph whole, what a mask carries back, and where a run of
// blitted cells lands.

#include <sigilcompose/kit/PixelType.h>

#include "support/KitType.h"
#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

TEST(KitPixelType, PadsWideEnoughThatTheLastGlyphIsNotClipped) {
  // Sizing the bake plane from intrinsicSize() plus a small fixed margin ends
  // the surface inside the final letter: intrinsicSize() returns the ADVANCE,
  // and a glyph's ink can sit outside its advance. The assertion is that with
  // the default pad the ink never reaches the right or bottom edge of the plane
  // — if it touches an edge, something was cut off.
  const auto style = pixelStyle(10.0f);
  for (const char8_t* s :
       {u8"Centrifuge", u8"WAV", u8"research", u8"1234567890"}) {
    const kit::Coverage cov = kit::coverage(s, fonts(), style);
    ASSERT_TRUE(cov.valid());
    ASSERT_FALSE(cov.ink.isEmpty());
    EXPECT_LT(cov.ink.fRight, cov.width())
        << "ink touches the right edge — the surface is too small";
    EXPECT_LT(cov.ink.fBottom, cov.height());
  }
}

TEST(KitPixelType, InkReallyDoesOverhangTheAdvanceSoThePadIsLoadBearing) {
  // The positive control for the case above, and the choice of face is
  // load-bearing. A monospace face is boxed by construction and its ink
  // never leaves its advance, so with Menlo "pad 0 clips" is simply false
  // and the pad test above would be proving nothing. Italic and script
  // faces do overhang, so the control uses those, requires at least one to
  // reach the edge at pad 0, and requires the SAME string to be clear of it
  // at the default pad.
  int overhangs = 0;
  for (const char* family : {"Helvetica", "Times New Roman", "Zapfino",
                             "Apple Chancery", "Snell Roundhand"}) {
    sk_sp<SkTypeface> face =
        sigil::weave::ports::pickTypeface({family}, SkFontStyle::Italic());
    if (!face) continue;
    const auto style = sigil::weave::textStyle({.face = face,
                                                .size = 12.0f,
                                                .color = SkColor4f{1, 1, 1, 1},
                                                .aliased = true});
    for (const char8_t* s : {u8"Wf", u8"of", u8"lift", u8"Ay"}) {
      const kit::Coverage tight =
          kit::coverage(s, fonts(), style, {.x = 0, .y = 0});
      if (!tight.valid() || tight.ink.isEmpty() ||
          tight.ink.fRight < tight.width())
        continue;
      ++overhangs;
      // The same string with ANY pad must come back unclipped, because
      // coverage() grows the pad and re-bakes until nothing touches an
      // edge. One pixel is enough to arm that retry. No fixed pad is
      // correct for every face — script faces overhang further than any
      // plausible default — which is why the component retries rather than
      // documenting a number for callers to pass.
      for (kit::Pad pad : {kit::Pad{1, 1}, kit::Pad{}}) {
        const kit::Coverage grown = kit::coverage(s, fonts(), style, pad);
        ASSERT_TRUE(grown.valid());
        ASSERT_FALSE(grown.ink.isEmpty());
        EXPECT_GT(grown.ink.fLeft, 0) << family << " / " << (const char*)s;
        EXPECT_GT(grown.ink.fTop, 0);
        EXPECT_LT(grown.ink.fRight, grown.width())
            << family << " / " << (const char*)s;
        EXPECT_LT(grown.ink.fBottom, grown.height());
        // The clipped bake LOST ink; the grown one recovered it.
        EXPECT_GE(grown.ink.width(), tight.ink.width());
      }
    }
  }
  EXPECT_GT(overhangs, 0)
      << "no face on this machine overhangs its advance, so this control "
         "proved nothing — the pad test above is unguarded here";
}

TEST(KitPixelType, MaskIsCroppedToItsInkAndCarriesTheOffsetBack) {
  const kit::Coverage cov = kit::coverage(u8"MM", fonts(), pixelStyle(12.0f));
  ASSERT_TRUE(cov.valid());
  const kit::Mask m = kit::threshold(cov);
  ASSERT_TRUE(m);
  EXPECT_EQ(m.w, cov.ink.width());
  EXPECT_EQ(m.h, cov.ink.height());
  EXPECT_EQ(m.inkX, cov.ink.fLeft);
  EXPECT_GT(m.advance, 0.0f);
  // Uncropped keeps the whole padded plane.
  const kit::Mask full = kit::threshold(cov, 0.5f, /*cropToInk=*/false);
  EXPECT_EQ(full.w, cov.width());
  EXPECT_GT(full.w, m.w);
}

TEST(KitPixelType, TheMaskIsOneBit) {
  const kit::Coverage cov = kit::coverage(u8"Ag", fonts(), pixelStyle(11.0f));
  ASSERT_TRUE(cov.valid());
  const kit::Mask m = kit::threshold(cov);
  ASSERT_TRUE(m);
  SkBitmap read;
  read.allocPixels(SkImageInfo::MakeA8(m.w, m.h));
  ASSERT_TRUE(m.image->readPixels(read.pixmap(), 0, 0));
  for (int y = 0; y < m.h; ++y)
    for (int x = 0; x < m.w; ++x) {
      const uint8_t v = *read.getAddr8(x, y);
      ASSERT_TRUE(v == 0 || v == 255)
          << "grey " << (int)v << " at " << x << "," << y;
    }
}

TEST(KitPixelType, DigitsShareOneAdvanceSoAReadoutDoesNotShiver) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  EXPECT_GT(f.lineHeight, 0);
  EXPECT_GT(f.digitAdvance, 0);
  EXPECT_FLOAT_EQ(kit::widthOf(f, "111"), kit::widthOf(f, "888"));
}

TEST(KitPixelType, SpaceHasAnAdvanceAndNoMask) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  const kit::Cell& sp = f.cell(' ');
  EXPECT_EQ(sp.mask, nullptr);
  EXPECT_GT(sp.advance, 0);
  EXPECT_GT(kit::widthOf(f, "a a"), kit::widthOf(f, "aa"));
}

TEST(KitPixelType, BlitAdvancesByTheMeasuredWidthAndSnaps) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  ASSERT_TRUE(s);
  const kit::Blit b{.track = 1.0f};
  const float w =
      kit::blit(*s->getCanvas(), f, {4, 4}, "1234", {1, 1, 1, 1}, b);
  EXPECT_NEAR(w, kit::widthOf(f, "1234", b), 1e-3f);

  const kit::Blit snapped{.track = 1.0f, .snap = 4.0f};
  const float ws =
      kit::blit(*s->getCanvas(), f, {4.9f, 4.1f}, "1", {1, 1, 1, 1}, snapped);
  EXPECT_NEAR(std::fmod(ws, 4.0f), 0.0f, 1e-3f);
}

TEST(KitPixelType, ASnappedRunMeasuresTheWidthItDraws) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  ASSERT_TRUE(s);
  // Snapping rounds EVERY pen step, not just the origin, so it changes the
  // advance a run occupies. A measure that adds the raw advances reports a
  // width the drawing never uses, and a layout laid out on it overlaps the
  // next thing.
  const kit::Blit b{.track = 1.0f, .snap = 3.0f};
  const float drawn =
      kit::blit(*s->getCanvas(), f, {3.0f, 3.0f}, "1234", {1, 1, 1, 1}, b);
  EXPECT_FLOAT_EQ(drawn, kit::widthOf(f, "1234", b));
  EXPECT_NEAR(std::fmod(drawn, 3.0f), 0.0f, 1e-3f);
  // The origin's own fraction is not part of the run's width: it is snapped
  // once, before the walk.
  EXPECT_FLOAT_EQ(
      kit::blit(*s->getCanvas(), f, {4.9f, 4.1f}, "1234", {1, 1, 1, 1}, b),
      drawn);
}

TEST(KitPixelType, ARunOfCellsStandsOnOneBaseline) {
  // A cell is cropped to its ink, so where that ink sat inside the line
  // box is the cell's to carry: an `x` and an `l` cropped flush and drawn
  // at one y would stand on no common line at all. The drop tells them
  // apart, and the two inks END together — which is what a baseline is.
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(12.0f));
  const kit::Cell& tall = f.cell('l');
  const kit::Cell& shortOne = f.cell('x');
  ASSERT_NE(tall.mask, nullptr);
  ASSERT_NE(shortOne.mask, nullptr);
  EXPECT_GT(shortOne.inkY, tall.inkY)
      << "an x sits lower in the line box than an l";
  EXPECT_NEAR(tall.inkY + tall.h, shortOne.inkY + shortOne.h, 1)
      << "both rest on the same baseline";
  // A descender reaches BELOW that baseline, and the line box holds it.
  const kit::Cell& below = f.cell('p');
  ASSERT_NE(below.mask, nullptr);
  EXPECT_GT(below.inkY + below.h, shortOne.inkY + shortOne.h);
  EXPECT_GE(f.lineHeight, below.inkY + below.h);
}

TEST(KitPixelType, ABlitLandsEachCellAtItsOwnDropInTheLineBox) {
  // The pen walk is unchanged by the drop — the advance is the advance —
  // but the ink lands where the cell says, so a mixed run reads as type
  // rather than as a row of tops.
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(12.0f));
  const kit::Cell& shortOne = f.cell('x');
  ASSERT_NE(shortOne.mask, nullptr);
  ASSERT_GT(shortOne.inkY, 0);
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(80, 40));
  ASSERT_TRUE(s);
  s->getCanvas()->clear(SK_ColorBLACK);
  kit::blit(*s->getCanvas(), f, {4, 4}, "x", {1, 1, 1, 1});
  SkBitmap read;
  ASSERT_TRUE(read.tryAllocPixels(SkImageInfo::MakeN32Premul(80, 40)));
  ASSERT_TRUE(s->readPixels(read.pixmap(), 0, 0));
  int topmost = 40;
  for (int y = 0; y < 40; ++y)
    for (int x = 0; x < 80; ++x)
      if (SkColorGetR(read.getColor(x, y)) > 0) {
        topmost = std::min(topmost, y);
        break;
      }
  EXPECT_EQ(topmost, 4 + shortOne.inkY);
}

TEST(KitPixelType, MaskedIsANodeTheSizeOfTheMask) {
  const kit::Mask m = kit::bakeRun(u8"88", fonts(), pixelStyle(10.0f));
  ASSERT_TRUE(m);
  const SkSize sz =
      intrinsicSize(box().children({kit::masked(m, {.scale = 2.0f})}), fonts());
  EXPECT_FLOAT_EQ(sz.width(), (float)m.w * 2.0f);
  EXPECT_FLOAT_EQ(sz.height(), (float)m.h * 2.0f);
}
