// kit/PixelType.h — the bitmap face baked from a vector one: the pad that
// keeps the last glyph whole, what a mask carries back, and where a run of
// blitted cells lands.

#include <sigilcompose/kit/PixelType.h>
#include <sigilmedia/advanced/Skia.h>

#include "sigilweave/advanced/Skia.h"
#include "support/KitType.h"
#include "support/Rows.h"
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
    ASSERT_FALSE(cov.ink.empty());
    EXPECT_LT(cov.ink.right(), cov.width())
        << "ink touches the right edge — the surface is too small";
    EXPECT_LT(cov.ink.bottom(), cov.height());
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
    sk_sp<SkTypeface> face = sigil::weave::ports::pickTypeface(
        {family}, {.slant = sigil::weave::FaceSlant::Italic});
    if (!face) continue;
    const auto style = sigil::weave::textStyle({.face = face,
                                                .size = 12.0f,
                                                .ink = SkColor4f{1, 1, 1, 1},
                                                .aliased = true});
    for (const char8_t* s : {u8"Wf", u8"of", u8"lift", u8"Ay"}) {
      const kit::Coverage tight =
          kit::coverage(s, fonts(), style, {.x = 0, .y = 0});
      if (!tight.valid() || tight.ink.empty() ||
          tight.ink.right() < tight.width())
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
        ASSERT_FALSE(grown.ink.empty());
        EXPECT_GT(grown.ink.left(), 0) << family << " / " << (const char*)s;
        EXPECT_GT(grown.ink.top(), 0);
        EXPECT_LT(grown.ink.right(), grown.width())
            << family << " / " << (const char*)s;
        EXPECT_LT(grown.ink.bottom(), grown.height());
        // The clipped bake LOST ink; the grown one recovered it.
        EXPECT_GE(grown.ink.width(), tight.ink.width());
      }
    }
  }
  EXPECT_GT(overhangs, 0)
      << "no face on this machine overhangs its advance, so this control "
         "proved nothing — the pad test above is unguarded here";
}

TEST(KitPixelType, AMaskIsOneBitCroppedToItsInk) {
  sigil::compose::test::checkRows({
      {"MaskIsCroppedToItsInkAndCarriesTheOffsetBack",
       [&] {
         const kit::Coverage cov =
             kit::coverage(u8"MM", fonts(), pixelStyle(12.0f));
         ASSERT_TRUE(cov.valid());
         const kit::Mask m = kit::threshold(cov);
         ASSERT_TRUE(m);
         EXPECT_EQ(m.w, cov.ink.width());
         EXPECT_EQ(m.h, cov.ink.height());
         EXPECT_EQ(m.inkX, cov.ink.left());
         EXPECT_GT(m.advance, 0.0f);
         // Uncropped keeps the whole padded plane.
         const kit::Mask full = kit::threshold(cov, 0.5f, /*cropToInk=*/false);
         EXPECT_EQ(full.w, cov.width());
         EXPECT_GT(full.w, m.w);
       }},
      {"SpaceHasAnAdvanceAndNoMask",
       [&] {
         const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
         const kit::Cell& sp = f.cell(' ');
         EXPECT_EQ(sp.mask, nullptr);
         EXPECT_GT(sp.advance, 0);
         EXPECT_GT(kit::widthOf(f, "a a"), kit::widthOf(f, "aa"));
       }},
  });
}

TEST(KitPixelType, ABlitSetsARunOfCellsOnOneBaselineAtTheWidthItMeasures) {
  const kit::PixFont f = kit::bakeFont(fonts(), pixelStyle(10.0f));
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 40));
  ASSERT_TRUE(s);
  // Snapping rounds EVERY pen step, not just the origin, so it changes the
  // advance a run occupies. A measure that adds the raw advances reports a
  // width the drawing never uses, and a layout laid out on it overlaps the
  // next thing.
  const kit::Blit b{.track = 1.0f, .snap = 3.0f};
  const float drawn = kit::blit(PenOn(*s->getCanvas()).pen(), f, {3.0f, 3.0f},
                                "1234", {1, 1, 1, 1}, b);
  EXPECT_FLOAT_EQ(drawn, kit::widthOf(f, "1234", b));
  EXPECT_NEAR(std::fmod(drawn, 3.0f), 0.0f, 1e-3f);
  // The origin's own fraction is not part of the run's width: it is snapped
  // once, before the walk.
  EXPECT_FLOAT_EQ(kit::blit(PenOn(*s->getCanvas()).pen(), f, {4.9f, 4.1f},
                            "1234", {1, 1, 1, 1}, b),
                  drawn);
}

TEST(KitPixelType, MaskedIsANodeTheSizeOfTheMask) {
  const kit::Mask m = kit::bakeRun(u8"88", fonts(), pixelStyle(10.0f));
  ASSERT_TRUE(m);
  const SkSize sz =
      intrinsicSize(box().children({kit::masked(m, {.scale = 2.0f})}), fonts());
  EXPECT_FLOAT_EQ(sz.width(), (float)m.w * 2.0f);
  EXPECT_FLOAT_EQ(sz.height(), (float)m.h * 2.0f);
}
