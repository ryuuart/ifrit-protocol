// Kit-tier tests: <sigilcompose/kit/*.h>.
//
// The kit sits ON TOP of the API and changes none of it, so a kit failure
// must never be reported as a kernel failure — hence a separate binary.
//
// Every case here is written to FAIL WITHOUT THE COMPONENT, and to measure
// what the component produced rather than to restate the arithmetic it was
// produced with — a test that recomputes the formula it is checking proves
// only that the compiler is deterministic.

#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/kit/Kit.h>

#include <cmath>
#include <utility>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;
namespace weave = sigil::weave;


namespace {

sigil::weave::TextStyle pixelStyle(float size) {
  return sigil::weave::textStyle(
      {.face = sigil::weave::ports::pickTypeface(
           {"Menlo", "DejaVu Sans Mono", "Courier New"}),
       .size = size,
       .color = {1, 1, 1, 1},
       .aliased = true});
}

}  // namespace

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
    const auto style = sigil::weave::textStyle(
        {.face = face, .size = 12.0f, .color = {1, 1, 1, 1}, .aliased = true});
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
      intrinsicSize(box().child(kit::masked(m, {.scale = 2.0f})), fonts());
  EXPECT_FLOAT_EQ(sz.width(), (float)m.w * 2.0f);
  EXPECT_FLOAT_EQ(sz.height(), (float)m.h * 2.0f);
}

// ===========================================================================
// Legibility.

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
  const SkSize bare = intrinsicSize(box().child(text(u8"NAVI", st)), fonts());
  const SkSize plated =
      intrinsicSize(box().child(kit::scrim(text(u8"NAVI", st),
                                           {.paddingX = 3, .paddingY = 4})),
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

// ---------------------------------------------------------------------------
// kit/Strokes.h — the kit's stroke values.
//
// The claim these check is that a kit value is a PEER of one you write
// yourself: every case below goes through the same public seam a
// user-written value would, and kit/Kit.cpp static_asserts the concepts.
// The other half of the boundary — that the kit compiles against public
// headers only — is the kit target's include path, which carries no
// compose source directory; a kit source that reached for an internal
// header fails to compile.

#include <sigilcompose/typography/Typography.h>

// ---------------------------------------------------------------------------
// kit/Frame.h — the pinned box.
//
// The claim is about where INK LANDS, so these render: restating
// `left/top/width/height` in the assertion would check the spelling
// against itself.

TEST(KitAt, PinsInkAtTheAbsoluteRectAndNowhereElse) {
  Host host;
  host.composer.render(box().width(200).height(200).child(
      kit::at(20, 30, 40, 50).fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(21, 31), SK_ColorRED);
  EXPECT_EQ(host.pixel(59, 79), SK_ColorRED);
  EXPECT_EQ(host.pixel(19, 31), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(21, 29), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(61, 81), SK_ColorBLACK);
}

TEST(KitAt, TheElementOverloadPlacesANodeItDidNotBuild) {
  Host host;
  // The node carries its own paint and knows nothing about the plate; the
  // plate says where it goes. That split is the overload's whole reason.
  Element painted = box().fill(green());
  host.composer.render(box().width(200).height(200).child(
      kit::at(std::move(painted), 100, 10, 30, 20)));
  host.frame();
  EXPECT_EQ(host.pixel(101, 11), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(129, 29), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(99, 11), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(131, 31), SK_ColorBLACK);
}

// ---------------------------------------------------------------------------
// kit/Typeset.h — the dropped initial and the opening run.

TEST(KitDropCap, AnOrnamentKeepsItsSilhouetteAsTheOpeningExclusion) {
  const std::u8string passage =
      u8"small words keep moving through the opening measure until the "
      u8"ornament has passed and the full line becomes available again "
      u8"below it, with enough copy to make that recovery visible";
  const auto scene = [&](bool silhouette) {
    Element ornament =
        box().width(90).height(90).fill(Fill::color({0, 1, 0, 1}));
    if (silhouette) ornament.shape(geometry::shapes::circle());
    kit::DroppedCap made = kit::dropCap(std::move(ornament), passage,
                                        pixelStyle(12), "ornament", 4);
    return box()
        .width(220)
        .height(260)
        .child(std::move(made.initial))
        .child(std::move(made.body).key("body").width(220));
  };

  Host boxed(220, 260), round(220, 260);
  boxed.composer.render(scene(false));
  boxed.frame();
  round.composer.render(scene(true));
  round.frame();

  ASSERT_TRUE(round.composer.bounds("ornament").has_value());
  EXPECT_FLOAT_EQ(round.composer.bounds("ornament")->width(), 90);
  EXPECT_FLOAT_EQ(round.composer.bounds("ornament")->height(), 90);
  const auto firstLineStart = [](const Host& host) {
    float start = 10000;
    for (const weave::PositionedRun& run :
         host.composer.paragraphLayout("body")->runs)
      if (run.lineIndex == 0) start = std::min(start, run.origin.x());
    return start;
  };
  EXPECT_LT(firstLineStart(round), firstLineStart(boxed));
}

// ---------------------------------------------------------------------------
// kit/Sprites.h — the stamp a point sink draws with.

TEST(KitSprites, DotIsOpaqueWhiteAtTheCentreAndClearOutsideTheDisc) {
  const sk_sp<SkImage> dot = kit::dotSprite();
  ASSERT_TRUE(dot);
  EXPECT_EQ(dot->width(), 32);
  EXPECT_EQ(dot->height(), 32);
  SkBitmap bm;
  ASSERT_TRUE(bm.tryAllocPixels(SkImageInfo::MakeN32Premul(32, 32)));
  ASSERT_TRUE(dot->readPixels(nullptr, bm.pixmap(), 0, 0));
  EXPECT_EQ(bm.getColor(16, 16), SK_ColorWHITE);
  EXPECT_EQ(SkColorGetA(bm.getColor(0, 0)), 0u);
  // The margin is the point: the last row of the image is clear, so the
  // antialiased edge is inside the stamp rather than cut off by it.
  for (int x = 0; x < 32; ++x) EXPECT_EQ(SkColorGetA(bm.getColor(x, 31)), 0u);
}

TEST(KitSprites, MarginAndSizeAreTheCallersNumbers) {
  const sk_sp<SkImage> dot = kit::dotSprite(64, 0.0f);
  ASSERT_TRUE(dot);
  EXPECT_EQ(dot->width(), 64);
  SkBitmap bm;
  ASSERT_TRUE(bm.tryAllocPixels(SkImageInfo::MakeN32Premul(64, 64)));
  ASSERT_TRUE(dot->readPixels(nullptr, bm.pixmap(), 0, 0));
  // With no margin the disc reaches the edge, which is exactly the
  // clipped edge the default avoids.
  EXPECT_GT(SkColorGetA(bm.getColor(32, 63)), 0u);
}

// ---------------------------------------------------------------------------
// kit/Specimen.h — the captioned cell, the run of cells and the sheet.
//
// The claims are about WHERE THE PARTS LAND, so every case lays out through
// a composer and reads the keyed boxes back, rather than restating the
// margins the component spells.

namespace {

kit::Caption specimenVoice(kit::Caption::Where where) {
  return {.where = where,
          .label = weave::textStyle({.size = 12}),
          .note = weave::textStyle({.size = 10}),
          .gap = 6,
          .noteGap = 4};
}

/** One line of type at @p size, as the layout will size it. */
float lineHeight(float size) {
  return intrinsicSize(
             box().child(text(u8"Hg", weave::textStyle({.size = size}))),
             fonts())
      .height();
}

}  // namespace

TEST(KitSpecimen, TheCaptionsLinesStandWhereTheVoiceSays) {
  const float label = lineHeight(12);
  const float note = lineHeight(10);
  // The body's top and the cell's height, for one arrangement.
  const auto placed = [&](kit::Caption::Where where, bool withNote) {
    Host host(300, 300);
    host.composer.render(box().width(300).height(300).child(
        kit::cell(specimenVoice(where), u8"LABEL", withNote ? u8"a note" : u8"",
                  box().key("body").width(100).height(40))
            .key("cell")));
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
               kit::cell(voice, u8"a label far wider than the body under it",
                         u8"", box().width(60).height(40)),
               fonts())
        .width();
  };
  EXPECT_GT(width(0), 60.0f);         // unmeasured: the label decides
  EXPECT_FLOAT_EQ(width(60), 60.0f);  // measured: the body does
}

TEST(KitSpecimen, AWellAppliesTheCallersSizeGroundAndPadding) {
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).child(kit::well(
      {.width = 100, .height = 80, .ground = red(), .padding = 10},
      box().key("well").child(
          box().key("body").width(20).height(15).fill(green())))));
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
    return kit::well({.width = 100, .height = 80, .clip = clip},
                     box().child(box()
                                     .absolute()
                                     .left(Dim(90))
                                     .top(Dim(20))
                                     .width(30)
                                     .height(20)
                                     .fill(green())));
  };
  Host host(160, 120);
  host.composer.render(box().width(160).height(120).child(specimen(true)));
  host.frame();
  EXPECT_EQ(host.pixel(95, 25), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(105, 25), SK_ColorBLACK);

  host.composer.render(box().width(160).height(120).child(specimen(false)));
  host.frame();
  EXPECT_EQ(host.pixel(105, 25), SK_ColorGREEN);
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
  host.composer.render(box().width(300).height(300).child(run(false)));
  host.frame();
  // cell, gap, rule, gap, cell — and the rule spans the taller cell.
  EXPECT_FLOAT_EQ(host.composer.bounds("b").value().left(), 50 + 10 + 2 + 10);
  EXPECT_EQ(host.pixel(61, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(61, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(55, 5), SK_ColorBLACK);

  host.composer.render(box().width(300).height(300).child(run(true)));
  host.frame();
  EXPECT_FLOAT_EQ(host.composer.bounds("b").value().top(), 20 + 10 + 2 + 10);
  EXPECT_EQ(host.pixel(5, 31), SK_ColorRED);
  EXPECT_EQ(host.pixel(49, 31), SK_ColorRED);
}

TEST(KitSpecimen, ASheetRulesOffItsHeaderAndFooterAndFootsThePage) {
  const float title = lineHeight(15);
  const float footer = lineHeight(11);
  kit::Sheet page{.title = u8"TITLE",
                  .footer = u8"the footer",
                  .titleStyle = weave::textStyle({.size = 15}),
                  .footerStyle = weave::textStyle({.size = 11}),
                  .marginX = 30,
                  .marginTop = 16,
                  .marginBottom = 14,
                  .contentGap = 18,
                  .rule = red(),
                  .ruleWidth = 2,
                  .key = "page"};
  Host host(400, 300);
  host.composer.render(
      kit::sheet(page, box().key("body")).width(400).height(300));
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
  host.composer.render(
      kit::sheet(plain, box().key("body")).width(400).height(300));
  host.frame();
  EXPECT_NEAR(host.composer.bounds("plain-content").value().top(),
              content.top(), 1.0f);
  EXPECT_FALSE(host.composer.bounds("plain-head-rule").has_value());
}
