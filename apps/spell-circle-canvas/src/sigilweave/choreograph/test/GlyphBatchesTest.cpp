/** @file
 * The paint-complete batched draw: what mints a bucket and what does not,
 * the order the passes reach the canvas in, the alpha scale, the tint, a
 * driven face, the matrix lane a glyph rides when no RSXform can carry it,
 * the pivot a centre offset moves, and what clear() keeps.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkGradient.h>
#include <sigilweave/kit/PaintLayers.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "support/ChoreographSupport.h"
#include "support/Paints.h"
#include "support/Pixels.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// A style whose foreground is a red-to-green gradient running from `left`
/// to `right` in canvas space, under a thick blue outline pass.
PaintStyle outlinedGradient(float left, float right) {
  PaintStyle style;
  style.foreground.setAntiAlias(true);
  style.foreground.setShader(
      horizontalGradient(left, right, SK_ColorRED, SK_ColorGREEN));
  style.addUnderlay(sigil::weave::kit::outline(SK_ColorBLUE, 6.0f));
  return style;
}

/// Accumulates every glyph of `layout` at its rest pose.
GlyphRSXformBatches batchAtRest(const ParagraphLayout& layout,
                                const Paragraph& paragraph,
                                float alphaScale = 1.0f) {
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    batches.addGlyph(glyph, glyph.rest + SkVector{glyph.advance * 0.5f, 0},
                     1.0f, 0.0f, alphaScale);
  });
  return batches;
}

}  // namespace

TEST(GlyphBatches, EveryPaintPassOfTheSpanDraws) {
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  auto [paragraph, layout] = laidOut(u8"HALO", 64.0f, flow);
  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);
  // The ramp spans exactly the placed text, so both ends of it are on the
  // glyphs and a flat fill could not pass for the shader.
  paragraph.setPaint(0, 4, outlinedGradient(lines[0].left, lines[0].right));

  GlyphRSXformBatches batches = batchAtRest(layout, paragraph);
  // One font, two passes: the outline underlay and the gradient foreground.
  ASSERT_EQ(batches.batches.size(), 2u);
  EXPECT_EQ(batches.batches[0].paint.getStyle(), SkPaint::kStroke_Style)
      << "underlays draw before the foreground";
  EXPECT_NE(batches.batches[1].paint.getShader(), nullptr);
  EXPECT_EQ(batches.batches[0].glyphs.size(), batches.batches[1].glyphs.size());

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 120));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorWHITE);
  const int drawn = batches.draw(canvas);
  EXPECT_EQ(drawn, static_cast<int>(batches.batches[0].glyphs.size() * 2));

  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawOutline = false, sawGradientStart = false, sawGradientEnd = false;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      const SkColor pixel = pixmap.getColor(x, y);
      const U8CPU red = SkColorGetR(pixel), green = SkColorGetG(pixel),
                  blue = SkColorGetB(pixel);
      if (blue > 200 && red < 60 && green < 60) sawOutline = true;
      if (red > 150 && green < 90 && blue < 60) sawGradientStart = true;
      if (green > 120 && red < 90 && blue < 60) sawGradientEnd = true;
    }
  EXPECT_TRUE(sawOutline) << "the underlay pass must reach the canvas";
  EXPECT_TRUE(sawGradientStart) << "the shader foreground must reach it too";
  EXPECT_TRUE(sawGradientEnd) << "and it must be the shader, not a flat fill";
}

TEST(GlyphBatches, BucketsSplitOnPassAndFontButNotOnGlyph) {
  BlockFlow flow(SkRect::MakeWH(400, 200));
  auto [paragraph, layout] = laidOut(u8"many many letters here", 24.0f, flow);

  // A plain single-pass style is one bucket, however many glyphs it draws.
  GlyphRSXformBatches flat = batchAtRest(layout, paragraph);
  ASSERT_EQ(flat.batches.size(), 1u);
  EXPECT_GT(flat.batches[0].glyphs.size(), 10u);

  // Adding an offset pass adds exactly one bucket, and the offset is the
  // bucket's, not the glyph's — the transforms stay identical.
  PaintStyle shadowed(SK_ColorBLACK);
  shadowed.addUnderlay(PaintLayer(SK_ColorRED, {4, 4}));
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()),
                     shadowed);
  GlyphRSXformBatches layered = batchAtRest(layout, paragraph);
  ASSERT_EQ(layered.batches.size(), 2u);
  EXPECT_EQ(layered.batches[0].offset, (SkVector{4, 4}));
  EXPECT_EQ(layered.batches[1].offset, (SkVector{0, 0}));
  ASSERT_EQ(layered.batches[0].transforms.size(),
            layered.batches[1].transforms.size());
  for (size_t index = 0; index < layered.batches[0].transforms.size(); ++index)
    EXPECT_EQ(layered.batches[0].transforms[index].fTx,
              layered.batches[1].transforms[index].fTx);
}

TEST(GlyphBatches, AlphaScaleFadesEveryPassAndDropsInvisibleOnes) {
  BlockFlow flow(SkRect::MakeWH(400, 100));
  auto [paragraph, layout] = laidOut(u8"fade", 32.0f, flow);
  PaintStyle style(SK_ColorBLACK);
  style.addUnderlay(PaintLayer(0x80FF0000, {2, 2}));
  paragraph.setPaint(0, 4, style);

  GlyphRSXformBatches half = batchAtRest(layout, paragraph, 0.5f);
  ASSERT_EQ(half.batches.size(), 2u);
  EXPECT_NEAR(half.batches[0].paint.getAlphaf(), 0.5f * 128.0f / 255.0f, 0.01f);
  EXPECT_NEAR(half.batches[1].paint.getAlphaf(), 0.5f, 0.01f);

  // A fully faded glyph mints no bucket at all.
  GlyphRSXformBatches gone = batchAtRest(layout, paragraph, 0.0f);
  EXPECT_TRUE(gone.batches.empty());
}

TEST(GlyphBatches, UnderlaysDrawBeneathForegroundsAcrossFadeClasses) {
  // Distinct per-glyph fades mint distinct buckets. The draw must still put
  // EVERY underlay beneath EVERY foreground: a blurred halo reaches past its
  // own glyph, and a cascade mid-flight (each letter at its own fade) must
  // not lay a later letter's halo over an earlier letter's stroke.
  BlockFlow flow(SkRect::MakeWH(300, 120));
  auto [paragraph, layout] = laidOut(u8"OO", 64.0f, flow);

  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(4.0f);
  stroke.setColor(0xFFDED8CC);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(8.0f);
  halo.setColor(0xFF000000);
  const PaintLayer blurredHalo = PaintLayer::blurred(halo, 5.0f);

  PaintStyle hollow;
  hollow.foreground = stroke;
  hollow.underlays.push_back(blurredHalo);
  paragraph.setPaint(0, 2, hollow);

  // One fade class per glyph — the second differs just enough to be its own
  // bucket pair while staying visually opaque.
  const auto batchFaded = [&](const PaintStyle* override) {
    GlyphRSXformBatches batches;
    uint32_t index = 0;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      const float alpha = index++ == 0 ? 0.995f : 1.0f;
      batches.addGlyph(glyph.shaped, override ? *override : *glyph.paint,
                       glyph.glyph, glyph.advance * 0.5f,
                       glyph.rest + SkVector{glyph.advance * 0.5f, 0}, 1.0f,
                       0.0f, alpha);
    });
    return batches;
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(300, 120);
  sk_sp<SkSurface> actualSurface = SkSurfaces::Raster(info);
  actualSurface->getCanvas()->clear(SK_ColorWHITE);
  batchFaded(nullptr).draw(actualSurface->getCanvas());

  // Ground truth: the same glyphs as two single-pass styles, every halo
  // drawn before any stroke.
  PaintStyle haloOnly;
  haloOnly.foreground = blurredHalo.paint;
  PaintStyle strokeOnly;
  strokeOnly.foreground = stroke;
  sk_sp<SkSurface> expectedSurface = SkSurfaces::Raster(info);
  expectedSurface->getCanvas()->clear(SK_ColorWHITE);
  batchFaded(&haloOnly).draw(expectedSurface->getCanvas());
  batchFaded(&strokeOnly).draw(expectedSurface->getCanvas());

  SkPixmap actual, expected;
  ASSERT_TRUE(actualSurface->peekPixels(&actual));
  ASSERT_TRUE(expectedSurface->peekPixels(&expected));
  const PixelDifference apart = worstPixelDifference(actual, expected);
  // The two fade classes differ by 1/255 at most, so anything past a couple
  // of counts is a compositing-order divergence, not the fade.
  EXPECT_LE(apart.worst, 4) << "batched draw diverges from underlays-then-"
                               "foregrounds at ("
                            << apart.x << ", " << apart.y << ")";
}

TEST(GlyphBatches, TintMultipliesAFlatPassAndModulatesAShaderOne) {
  // The colour multiplier has to reach EVERY pass, and the two kinds of
  // pass take it differently: a flat pass multiplies the colour it already
  // carries, a shader pass cannot (its colour is decided downstream) and
  // takes an equivalent modulating filter instead. Either way the glyph
  // keeps the pass — a tinted letter is not a re-styled letter.
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  auto [paragraph, layout] = laidOut(u8"HALO", 64.0f, flow);
  const std::vector<LineMetrics> lines = layout.lineMetrics(paragraph);
  ASSERT_EQ(lines.size(), 1u);
  paragraph.setPaint(0, 4, outlinedGradient(lines[0].left, lines[0].right));

  // Green only: the blue outline underlay must go black, and the red end of
  // the gradient foreground must go black too, while its green end holds.
  GlyphDress dress;
  dress.colorMul = {0, 1, 0, 1};
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress placed = dress;
    placed.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    batches.addGlyph(glyph, placed);
  });
  ASSERT_EQ(batches.batches.size(), 2u) << "the tint dropped a pass";
  EXPECT_EQ(batches.batches[0].paint.getStyle(), SkPaint::kStroke_Style);
  EXPECT_EQ(batches.batches[0].paint.getColor4f().fB, 0.0f)
      << "a flat pass takes the tint in its own colour";
  EXPECT_NE(batches.batches[1].paint.getColorFilter(), nullptr)
      << "a shader pass takes the tint as a modulating filter";

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 120));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  batches.draw(canvas);
  SkPixmap pixmap;
  ASSERT_TRUE(surface->peekPixels(&pixmap));
  bool sawGreen = false;
  for (int y = 0; y < pixmap.height(); ++y)
    for (int x = 0; x < pixmap.width(); ++x) {
      const SkColor pixel = pixmap.getColor(x, y);
      EXPECT_LT(SkColorGetB(pixel), 40u)
          << "blue survived a tint that multiplies it by zero, at " << x << ","
          << y;
      EXPECT_LT(SkColorGetR(pixel), 40u) << "red survived it too";
      if (SkColorGetG(pixel) > 120) sawGreen = true;
    }
  EXPECT_TRUE(sawGreen) << "the tint painted nothing at all";
}

TEST(GlyphBatches, OneTintIsOneBucketHoweverManyGlyphsWearIt) {
  // A bucket's key is a whole SkPaint and SkPaint compares its colour
  // filter by POINTER, so the modulating filter has to be memoized: a fresh
  // one per glyph would mint a bucket per glyph and undo the batching that
  // is the entire point of this file.
  BlockFlow flow(SkRect::MakeWH(400, 200));
  auto [paragraph, layout] = laidOut(u8"many many letters here", 24.0f, flow);
  PaintStyle shaded;
  const SkPoint ends[2] = {{0, 0}, {400, 0}};
  const SkColor4f ramp[2] = {SkColor4f::FromColor(SK_ColorRED),
                             SkColor4f::FromColor(SK_ColorGREEN)};
  shaded.foreground.setShader(SkShaders::LinearGradient(
      ends, SkGradient(SkGradient::Colors({ramp, 2}, SkTileMode::kClamp),
                       SkGradient::Interpolation())));
  paragraph.setPaint(0, static_cast<uint32_t>(paragraph.text().size()), shaded);

  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    dress.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    dress.colorMul = {0.5f, 0.75f, 1.0f, 1.0f};
    batches.addGlyph(glyph, dress);
  });
  EXPECT_EQ(batches.batches.size(), 1u)
      << "one tint over one style must stay one bucket";
  EXPECT_GT(batches.batches[0].glyphs.size(), 10u);
}

TEST(GlyphBatches, ADrivenFaceIsItsOwnBucket) {
  // A glyph drawn through a varied clone cannot share a bucket with one
  // drawn through the base face: the face is what the draw call carries.
  BlockFlow flow(SkRect::MakeWH(400, 100));
  auto [paragraph, layout] = laidOut(u8"AB", 32.0f, flow);

  const SkTypeface* shapedFace = nullptr;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    if (!shapedFace && glyph.shaped) shapedFace = glyph.shaped->typeface.get();
  });
  ASSERT_NE(shapedFace, nullptr);
  // The committed instrument stands in for a varied clone: the bucket key
  // cares that the typeface differs, not how it was made, and a face this
  // machine happens to have is not needed to say so.
  const sk_sp<SkTypeface> other = verticalFeaturesFace();
  ASSERT_TRUE(other);
  ASSERT_NE(other.get(), shapedFace);
  bool first = true;
  GlyphRSXformBatches batches;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    dress.center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    if (!first) dress.face = other;
    first = false;
    batches.addGlyph(glyph, dress);
  });
  ASSERT_EQ(batches.batches.size(), 2u);
  EXPECT_NE(batches.batches[0].typeface.get(),
            batches.batches[1].typeface.get());
}

TEST(GlyphBatches, AMatrixGlyphRidesItsOwnLaneInsideItsBucket) {
  // A shear cannot be an RSXform, so that glyph draws under its own matrix
  // — in the SAME bucket, so it keeps its pass order and its paint, and
  // without disturbing the shared transform array its neighbours ride.
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 80));
  auto [paragraph, layout] = laidOut(u8"HH", 48.0f, flow);

  SkMatrix sheared;
  GlyphRSXformBatches batches;
  bool first = true;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    GlyphDress dress;
    const SkPoint center = glyph.rest + SkVector{glyph.advance * 0.5f, 0};
    if (first) {
      dress.center = center;
    } else {
      sheared = SkMatrix::Translate(center.x(), center.y());
      sheared.preConcat(SkMatrix::MakeAll(1, -0.5f, 0, 0, 1, 0, 0, 0, 1));
      sheared.preTranslate(-glyph.advance * 0.5f, 0);
      dress.matrix = &sheared;
    }
    first = false;
    batches.addGlyph(glyph, dress);
  });
  ASSERT_EQ(batches.batches.size(), 1u) << "the matrix lane split the bucket";
  EXPECT_EQ(batches.batches[0].glyphs.size(), 1u);
  EXPECT_EQ(batches.batches[0].matrixGlyphs.size(), 1u);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(400, 100));
  surface->getCanvas()->clear(SK_ColorWHITE);
  EXPECT_EQ(batches.draw(surface->getCanvas()), 2)
      << "both lanes must reach the canvas";
  batches.clear();
  EXPECT_TRUE(batches.batches[0].matrixGlyphs.empty())
      << "clear() left the matrix lane behind";
}

TEST(GlyphBatches, ClearKeepsBucketsButReleasesGlyphs) {
  BlockFlow flow(SkRect::MakeWH(400, 100));
  auto [paragraph, layout] = laidOut(u8"reuse", 20.0f, flow);

  GlyphRSXformBatches batches = batchAtRest(layout, paragraph);
  ASSERT_EQ(batches.batches.size(), 1u);
  batches.clear();
  ASSERT_EQ(batches.batches.size(), 1u) << "allocations are kept for reuse";
  EXPECT_TRUE(batches.batches[0].glyphs.empty());
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 32));
  EXPECT_EQ(batches.draw(surface->getCanvas()), 0)
      << "an emptied batch issues no draw";
}

TEST(GlyphBatches, ACentreOffsetMovesThePivotOffTheAdvanceAxis) {
  // The RSXform convention backs a glyph out from its pose centre by half
  // its advance ALONG ITS OWN X. A vertical column's advance is not on x,
  // so the dress carries the back-out instead — and it turns with the
  // glyph, exactly as the default one does.
  BlockFlow flow(SkRect::MakeWH(200, 60));
  auto [paragraph, layout] = laidOut(u8"H", 40.0f, flow);

  const SkPoint centre{100, 30};
  const SkVector offset{0, 12};
  const auto placedAt = [&](float cosine, float sine, const SkVector* off) {
    GlyphRSXformBatches batches;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      GlyphDress dress;
      dress.center = centre;
      dress.cosine = cosine;
      dress.sine = sine;
      dress.centreOffset = off;
      batches.addGlyph(glyph, dress);
    });
    return batches.batches.at(0).transforms.at(0);
  };

  const SkRSXform upright = placedAt(1, 0, &offset);
  EXPECT_FLOAT_EQ(upright.fTx, centre.x());
  EXPECT_FLOAT_EQ(upright.fTy, centre.y() - offset.y())
      << "an unrotated glyph backs out along the offset itself";

  // A quarter turn takes (0, 12) to (-12, 0).
  const SkRSXform turned = placedAt(0, 1, &offset);
  EXPECT_NEAR(turned.fTx, centre.x() + offset.y(), 1e-4f);
  EXPECT_NEAR(turned.fTy, centre.y(), 1e-4f);

  // Null keeps the horizontal convention.
  const SkRSXform plain = placedAt(1, 0, nullptr);
  EXPECT_LT(plain.fTx, centre.x()) << "backed out by half its advance";
  EXPECT_FLOAT_EQ(plain.fTy, centre.y());
}

TEST(GlyphBatches, SubpixelDecidesWhetherAFractionOfAPixelMovesAnything) {
  // The declaration a moving run makes, and its whole observable meaning.
  // Two device origins a fraction of a pixel apart INSIDE one pixel cell:
  // without the subpixel grid the glyph is rasterized at the rounded
  // origin both times and the two frames are the same picture; with it
  // they are not.
  BlockFlow flow(SkRect::MakeXYWH(10, 10, 380, 100));
  auto [paragraph, layout] = laidOut(u8"H", 44.0f, flow);
  PaintStyle white;
  white.foreground.setColor(SK_ColorWHITE);
  paragraph.setPaint(0, 1, white);

  // A pose is the glyph's CENTRE, and the transform backs out half its
  // advance, so the device origin a pose asks for is stated here rather
  // than guessed at.
  float advance = 0;
  forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
    advance = glyph.advance;
  });
  ASSERT_GT(advance, 0.0f);

  // One frame, reduced to a value: two frames are the same rasterization
  // exactly when their pixels are.
  const auto renderHash = [&](bool subpixel, float deviceX) {
    GlyphRSXformBatches batches;
    batches.subpixel = subpixel;
    forEachPlacedGlyph(layout, paragraph, [&](const PlacedGlyph& glyph) {
      batches.addGlyph(glyph,
                       SkPoint{deviceX + advance * 0.5f, glyph.rest.y()});
    });
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 100));
    surface->getCanvas()->clear(SK_ColorBLACK);
    batches.draw(surface->getCanvas());
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(200, 100));
    EXPECT_TRUE(surface->readPixels(bitmap.pixmap(), 0, 0));
    uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 200; ++x) {
        hash ^= bitmap.getColor(x, y);
        hash *= 1099511628211ull;
      }
    return hash;
  };

  // Both origins sit inside the pixel cell that starts at 100, whichever
  // way a rounding rule breaks it.
  EXPECT_EQ(renderHash(false, 100.0f), renderHash(false, 100.4f))
      << "whole-pixel origins moved on a fraction of a pixel";
  EXPECT_NE(renderHash(true, 100.0f), renderHash(true, 100.4f))
      << "the subpixel grid ignored a fraction of a pixel";
}
