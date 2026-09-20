/** @file
 * The fields: the halftone ramp swells downward and its band remaps,
 * grain is monochrome and varies, noise compares by its parameters and
 * shades, a ripple displaces the content it is handed, and the CRT
 * overlay stripes and vignettes in alpha alone. Then the screen: what a
 * tube's light reaches and where it lands, each of the three subjects
 * it is composed of on its own, and the composition read back as its
 * beam under its glass.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/field/Crt.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>

#include "ShaderTable.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::render;

namespace {

int coverage(const SkBitmap& bm, int y) {
  int n = 0;
  for (int x = 0; x < bm.width(); ++x)
    if (SkColorGetA(bm.getColor(x, y)) > 128) ++n;
  return n;
}

}  // namespace

TEST(Field, HalftoneRampSwellsDownwardAndBandRemaps) {
  const Material ramp = field::halftoneRamp(8, 0.5f, 3.5f, {0, 0, 0, 1});
  EXPECT_TRUE(ramp.geometryDependent());
  const SkBitmap bm = render(ramp, 64, 64);
  EXPECT_LT(coverage(bm, 4), coverage(bm, 60));
  const Material band =
      field::halftoneRamp(8, 0.5f, 3.5f, {0, 0, 0, 1}, 0, 0.9f, 1);
  const SkBitmap bb = render(band, 64, 64);
  // The swell is confined to the last tenth: the top reads as rMin.
  EXPECT_LE(coverage(bb, 30), coverage(bm, 30));
}

TEST(Field, GrainIsMonochromeAndVaries) {
  const Material g = field::grain(0.3f, 3, 4.0f, 1.0f);
  const SkBitmap bm = render(g, 32, 32);
  bool varies = false;
  for (int y = 0; y < 32; ++y)
    for (int x = 0; x < 32; ++x) {
      const SkColor c = bm.getColor(x, y);
      EXPECT_EQ(SkColorGetR(c), SkColorGetG(c));
      EXPECT_EQ(SkColorGetG(c), SkColorGetB(c));
      varies |= c != bm.getColor(0, 0);
    }
  EXPECT_TRUE(varies);
  EXPECT_EQ(g, field::grain(0.3f, 3, 4.0f, 1.0f));
  EXPECT_FALSE(g == field::grain(0.3f, 4, 4.0f, 1.0f));
  EXPECT_EQ(field::grainRecipe(3).get(), field::grainRecipe(3).get());
}

TEST(Field, NoiseComparesByParametersAndShades) {
  const Material a = field::noise(0.05f, 3, 1);
  EXPECT_EQ(a, field::noise(0.05f, 3, 1));
  EXPECT_FALSE(a == field::noise(0.05f, 3, 2));
  EXPECT_FALSE(a == field::noise(0.05f, 3, 1, true));
  const SkBitmap bm = render(a, 16, 16);
  bool varies = false;
  for (int i = 1; i < 16; ++i) varies |= bm.getColor(i, i) != bm.getColor(0, 0);
  EXPECT_TRUE(varies);
}

TEST(Field, RippleDisplacesTheContent) {
  // Content: a horizontal edge at y = 8. A vertical sine of x moves the
  // edge up and down along x.
  SkBitmap content;
  content.allocPixels(SkImageInfo::MakeN32Premul(32, 16));
  content.eraseColor(SK_ColorTRANSPARENT);
  content.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 8, 32, 8));
  content.setImmutable();
  Material r = field::ripple(3, 16);
  r.slot("content", Texture::of(content.asImage()));
  const SkBitmap bm = render(r, 32, 16);
  int firstRow[2] = {16, 16};
  for (int k = 0; k < 2; ++k) {
    const int x = k == 0 ? 4 : 12;  // a quarter wave apart
    for (int y = 0; y < 16; ++y)
      if (SkColorGetA(bm.getColor(x, y)) > 0) {
        firstRow[k] = y;
        break;
      }
  }
  EXPECT_NE(firstRow[0], firstRow[1]);
  EXPECT_EQ(r, r);
}

TEST(Field, CrtOverlayStripesEveryOtherHalfPitchAndDarkensTheCorners) {
  const Material crt = field::crtOverlay();
  EXPECT_TRUE(crt.geometryDependent());
  const SkBitmap bm = render(crt, 128, 128);
  // Black at every pixel; the whole picture is in the alpha.
  EXPECT_EQ(SkColorGetR(bm.getColor(64, 64)), 0u);
  // The default pitch is 4 px with the first half dark, so rows 0 and 1
  // carry the scanline and rows 2 and 3 do not.
  const unsigned lit = SkColorGetA(bm.getColor(64, 2));
  const unsigned dark = SkColorGetA(bm.getColor(64, 0));
  EXPECT_GT(dark, lit);
  EXPECT_EQ(SkColorGetA(bm.getColor(64, 1)), dark);
  EXPECT_EQ(SkColorGetA(bm.getColor(64, 3)), lit);
  // The corner falloff: a corner is further out than the centre row.
  EXPECT_GT(SkColorGetA(bm.getColor(1, 1)), SkColorGetA(bm.getColor(64, 64)));
}

TEST(Field, CrtOverlayScanStrengthAndVignetteAreTheCallersNumbers) {
  const SkBitmap none =
      render(field::crtOverlay(4.0f, 0.0f, 1.45f, 2.15f, 0.0f), 64, 64);
  // Every parameter off: the overlay is fully transparent and changes
  // nothing about what it sits over.
  for (int y = 0; y < 4; ++y) EXPECT_EQ(SkColorGetA(none.getColor(32, y)), 0u);
  const SkBitmap strong =
      render(field::crtOverlay(4.0f, 0.5f, 1.45f, 2.15f, 0.0f), 64, 64);
  EXPECT_GT(SkColorGetA(strong.getColor(32, 0)), 100u);
  EXPECT_EQ(SkColorGetA(strong.getColor(32, 2)), 0u);
}

TEST(Field, TheBeamTheBeatAndTheGrainAreAbsentUntilTheyAreGivenStrength) {
  // The tube's darkening is a sum, and the positional entry point is the
  // hard line alone: nothing the parameters added may reach a picture that
  // did not ask for it.
  const SkBitmap plain = render(field::crtOverlay(4.0f, 0.10f), 64, 64);
  const SkBitmap same = render(
      field::crtOverlay({.uScanPitch = 4.0f, .uScanStrength = 0.10f}), 64, 64);
  for (int y = 0; y < 16; ++y)
    EXPECT_EQ(SkColorGetA(plain.getColor(32, y)),
              SkColorGetA(same.getColor(32, y)));

  // A beam leaves its own centre alone and darkens away from it. The
  // centre falls half a pitch in, so a row there is lighter than one a
  // half pitch off it, and two rows a whole pitch apart read the same.
  const SkBitmap beam = render(field::crtOverlay({.uScanStrength = 0.0f,
                                                  .uVigStrength = 0.0f,
                                                  .uBeamPitch = 8.0f,
                                                  .uBeamFalloff = 2.0f,
                                                  .uBeamStrength = 0.6f}),
                               64, 64);
  EXPECT_EQ(SkColorGetA(beam.getColor(32, 4)),
            SkColorGetA(beam.getColor(32, 12)));
  EXPECT_LT(SkColorGetA(beam.getColor(32, 4)),
            SkColorGetA(beam.getColor(32, 8)));

  // The beat is a second period of the same shape, on a pitch of its
  // own: its centre is eight rows in where the beam's was four.
  const SkBitmap beat = render(field::crtOverlay({.uScanStrength = 0.0f,
                                                  .uVigStrength = 0.0f,
                                                  .uBeamStrength = 0.0f,
                                                  .uBeatPitch = 16.0f,
                                                  .uBeatFalloff = 2.0f,
                                                  .uBeatStrength = 0.6f}),
                               64, 64);
  EXPECT_LT(SkColorGetA(beat.getColor(32, 8)),
            SkColorGetA(beat.getColor(32, 16)));

  // The grain moves the darkening pixel to pixel, both ways, and a row
  // with nothing else on it is no longer one value.
  const SkBitmap grained = render(
      field::crtOverlay(
          {.uScanStrength = 0.20f, .uVigStrength = 0.0f, .uGrain = 0.30f}),
      64, 64);
  int lo = 256, hi = -1;
  for (int x = 0; x < 64; ++x) {
    const int a = (int)SkColorGetA(grained.getColor(x, 0));
    lo = std::min(lo, a);
    hi = std::max(hi, a);
  }
  EXPECT_GT(hi - lo, 8);
}

// ---- the embedded shader table --------------------------------------------

TEST(Field, EveryStockBodyCompiles) {
  for (const Material& m : field::everyRecipe()) {
    if (!m.recipe().has(Target::SkSL)) continue;
    EXPECT_TRUE(skia::shader(m, {.resolution = {64, 64}})) << m.recipe().name();
  }
}

TEST(Field, TheShaderTableHoldsEveryFileTheDirectoryDoes) {
  sigil::test::expectShaderTableIsWholeDirectory(
      field::shaderSources(), SIGIL_MATERIAL_FIELD_SHADER_DIR);
}

namespace {

/** @p source painted into a LAYER that carries @p effect. That is the
 *  path that gives a recipe an EXECUTOR, which is what fills a slot
 *  declared as one the executor fills — a fill has no layer and must
 *  bind such a slot itself. */
SkBitmap throughEffect(const skia::Effect& effect, const sk_sp<SkImage>& source,
                       int side) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(side, side));
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint layer;
  layer.setImageFilter(effect.resolvedImageFilter(nullptr));
  canvas.saveLayer(nullptr, &layer);
  canvas.drawImage(source, 0, 0);
  canvas.restore();
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  EXPECT_TRUE(surface->makeImageSnapshot()->readPixels(nullptr,
                                                       bitmap.pixmap(), 0, 0));
  return bitmap;
}

/** The same, for a material run over the layer at @p reach. */
SkBitmap throughLayer(const Material& material, float reach,
                      const sk_sp<SkImage>& source, int side) {
  return throughEffect(skia::Effect::recipe(material, reach), source, side);
}

/** The whole screen over a rendered layer. */
SkBitmap throughScreen(const field::CrtParameters& parameters,
                       const sk_sp<SkImage>& source, int side) {
  return throughLayer(field::crt(parameters),
                      field::crtSampleRadius(parameters), source, side);
}

/** A black field of @p side pixels with one bright vertical band @p wide
 *  pixels across whose left edge is at @p at — the thin bright line a
 *  tube's light is judged by. */
sk_sp<SkImage> bandAt(int side, int at, int wide) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(side, side));
  bitmap.eraseColor(SK_ColorBLACK);
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeXYWH((float)at, 0, (float)wide, (float)side),
                  paint);
  bitmap.setImmutable();
  return bitmap.asImage();
}

/** The same band, centred. */
sk_sp<SkImage> brightBand(int side, int wide) {
  return bandAt(side, (side - wide) / 2, wide);
}

}  // namespace

TEST(Field, CrtZeroStrengthPreservesColourAndHonoursBounds) {
  field::CrtParameters p{.uBounds = {8, 8, 16, 16}};
  const auto red = Texture::of(test::solid(SK_ColorRED, 32, 32));
  Material screen = field::crt(p);
  screen.slot("content", red);
  screen.slot("bloom", red);
  ASSERT_NE(skia::shader(screen, {}), nullptr);
  const auto pixels = render(screen, 32, 32);
  EXPECT_EQ(pixels.getColor(16, 16), SK_ColorRED);
  EXPECT_EQ(pixels.getColor(7, 16), SK_ColorTRANSPARENT);
  EXPECT_EQ(pixels.getColor(24, 16), SK_ColorTRANSPARENT);
  p.uCurvature = 1;
  Material curved = field::crt(p);
  curved.slot("content", red);
  curved.slot("bloom", red);
  const auto glass = render(curved, 32, 32);
  EXPECT_EQ(glass.getColor(8, 8), SK_ColorBLACK);
  EXPECT_EQ(glass.getColor(16, 16), SK_ColorRED);
}

TEST(Field, CrtBloomSpreadsLightAndExplicitTimeIsRepeatable) {
  const sk_sp<SkImage> band = brightBand(32, 4);
  field::CrtParameters p{.uBounds = {0, 0, 32, 32}, .uBloomRadius = 2};
  const auto plain = throughScreen(p, band, 32);
  p.uBloom = 1;
  const auto glow = throughScreen(p, band, 32);
  EXPECT_GT(SkColorGetG(glow.getColor(12, 16)),
            SkColorGetG(plain.getColor(12, 16)));
  p.uBloomRadius = 4.8f;
  p.uBloom = 1.6f;
  const auto wide = throughScreen(p, band, 32);
  // Light fills the gap between the band and the edge, thinning with
  // distance rather than repeating the band at the gather's radii.
  EXPECT_GT(SkColorGetG(wide.getColor(6, 16)), 0);
  EXPECT_GT(SkColorGetG(wide.getColor(10, 16)),
            SkColorGetG(wide.getColor(6, 16)));
  p.uNoise = 0.3f;
  p.uTime = 1;
  const auto first = throughScreen(p, band, 32);
  EXPECT_TRUE(test::identical(first, throughScreen(p, band, 32)));
  p.uTime = 2;
  EXPECT_GT(test::differing(first, throughScreen(p, band, 32)), 0);
  // THE REACH IS NOT THE BLOOM'S ANY MORE. Nothing gathers, so the
  // sampling radius covers the warp and the shift alone and does not
  // grow when the light does — the executor's blur grows its own input
  // bounds.
  const float reach = field::crtSampleRadius(p);
  p.uBloomRadius *= 8;
  EXPECT_EQ(field::crtSampleRadius(p), reach);
}

TEST(Field, TheScreensLightReachesWellBeyondTheLineThatMadeIt) {
  // A fixed tap count is what caps how far a gather may reach before it
  // starts leaving copies of what it spread; a Gaussian has no such cap.
  // So light from a thin bright band stands far outside it and thins the
  // whole way.
  const sk_sp<SkImage> band = brightBand(64, 4);
  const field::CrtParameters p{
      .uBounds = {0, 0, 64, 64}, .uBloomRadius = 12, .uBloom = 1.6f};
  const SkBitmap lit = throughScreen(p, band, 64);
  // EVERY STEP of the run from the band's edge to the picture's, because
  // a copy of the band out there is a RISE and three points read wide
  // apart step straight over one.
  int previous = 256;
  for (int x = 34; x < 64; ++x) {
    const int green = SkColorGetG(lit.getColor(x, 32));
    EXPECT_LE(green, previous) << "x " << x;
    previous = green;
  }
  // And it is light that reaches, not light that stops: far out it
  // stands above nothing, and well below what it was beside the band.
  EXPECT_GT(SkColorGetG(lit.getColor(56, 32)), 0);
  EXPECT_LT(SkColorGetG(lit.getColor(56, 32)),
            SkColorGetG(lit.getColor(36, 32)));
}

TEST(Field, TheTubesLightIsDrawnFromTheWholeLayerAndLandsOnlyInsideIt) {
  // The blur the executor fills the bloom slot with is taken over the
  // LAYER, not over the tube, so a bright thing standing outside the
  // bounds lights the glass near that edge the way a lamp beside a
  // monitor does. Where the light LANDS is the tube's rectangle and
  // nothing else.
  const sk_sp<SkImage> outside = bandAt(64, 8, 4);
  const field::CrtParameters p{.uBounds = {24, 0, 16, 64},
                               .uBloomRadius = 8,
                               .uBloom = 1.6f};
  const SkBitmap lit = throughScreen(p, outside, 64);
  // Nothing at all outside the rectangle — not the band that was drawn
  // there, and not the light it gives off.
  EXPECT_EQ(SkColorGetA(lit.getColor(10, 32)), 0u);
  EXPECT_EQ(SkColorGetA(lit.getColor(20, 32)), 0u);
  EXPECT_EQ(SkColorGetA(lit.getColor(44, 32)), 0u);
  // The tube itself was handed black — the band is outside its bounds,
  // so `content` reads nothing there — and is lit all the same, from
  // the near edge towards the far one.
  EXPECT_GT(SkColorGetG(lit.getColor(25, 32)), 0);
  EXPECT_GT(SkColorGetG(lit.getColor(25, 32)),
            SkColorGetG(lit.getColor(38, 32)));
}

// ---- the screen's three subjects, each on its own ------------------------

TEST(Field, TheBeamAloneRastersAPictureAndBendsNothing) {
  // The scanlines over a flat surface, with no glass anywhere: the
  // raster is the same lines at the same pitch across the whole
  // picture, which is what says nothing is bending them.
  const field::CrtBeamParameters parameters{
      .uBounds = {0, 0, 64, 64}, .uScanPitch = 4, .uRaster = 1.0f};
  Material beam = field::crtBeam(parameters);
  beam.slot("content", Texture::of(test::solid(SK_ColorGRAY, 64, 64)));
  ASSERT_NE(skia::shader(beam, {}), nullptr);
  const SkBitmap lit = render(beam, 64, 64);
  int low = 256;
  int high = 0;
  for (int y = 0; y < 64; ++y) {
    EXPECT_EQ(lit.getColor(2, y), lit.getColor(32, y)) << "row " << y;
    if (y + 4 < 64)
      EXPECT_EQ(lit.getColor(32, y), lit.getColor(32, y + 4)) << "row " << y;
    low = std::min(low, test::luminance(lit.getColor(32, y)));
    high = std::max(high, test::luminance(lit.getColor(32, y)));
  }
  EXPECT_GT(high - low, 8);
  // The guns are the beam's: spread apart, the picture's edges carry
  // colour along them where the flat grey has none.
  field::CrtBeamParameters spread = parameters;
  spread.uRgbShift = 2.0f;
  Material converging = field::crtBeam(spread);
  const sk_sp<SkImage> band = bandAt(64, 40, 4);
  converging.slot("content", Texture::of(band));
  Material straight = field::crtBeam(parameters);
  straight.slot("content", Texture::of(band));
  EXPECT_GT(test::differing(render(converging, 64, 64), render(straight, 64, 64)),
            0);
  // And the beam asks for no reach of its own until a gun or the sweep
  // takes a reading off the pixel it was asked for.
  EXPECT_FLOAT_EQ(field::crtBeamSampleRadius(parameters), 0.0f);
  EXPECT_FLOAT_EQ(
      field::crtBeamSampleRadius({.uRgbShift = 2, .uJitter = 2, .uSync = 1}),
      5.0f);
}

TEST(Field, TheBloomSourceAloneIsTheLayerBlurredInsideTheTube) {
  // The light a picture throws, as a layer of its own: the executor's
  // blur of the layer, read once — the same pixels Effect::blur makes
  // of the same layer at the same sigma.
  const sk_sp<SkImage> band = brightBand(64, 4);
  const field::CrtBloomParameters whole{.uBounds = {0, 0, 64, 64},
                                        .uBloomRadius = 6};
  EXPECT_TRUE(test::identical(throughLayer(field::crtBloom(whole), 0, band, 64),
                              throughEffect(skia::Effect::blur(6), band, 64)));
  // And kept inside the tube: the same light with the bounds drawn in
  // stands where the tube is and nowhere else.
  const field::CrtBloomParameters inset{.uBounds = {24, 0, 16, 64},
                                        .uBloomRadius = 6};
  const SkBitmap kept = throughLayer(field::crtBloom(inset), 0, band, 64);
  EXPECT_EQ(SkColorGetA(kept.getColor(10, 32)), 0u);
  EXPECT_GT(SkColorGetG(kept.getColor(30, 32)), 0);
}

TEST(Field, TheGlassAloneIsTheIdentityUntilItIsCurvedOrLit) {
  // The barrel over any surface: with nothing to bend, light or darken,
  // what comes out of the glass is what went into it, pixel for pixel.
  const sk_sp<SkImage> band = bandAt(64, 40, 4);
  SkBitmap drawn;
  drawn.allocPixels(SkImageInfo::MakeN32Premul(64, 64));
  ASSERT_TRUE(band->readPixels(nullptr, drawn.pixmap(), 0, 0));
  const field::CrtGlassParameters flat{.uBounds = {0, 0, 64, 64}};
  EXPECT_TRUE(test::identical(
      throughLayer(field::crtGlass(flat), field::crtGlassSampleRadius(flat),
                   band, 64),
      drawn));
  // Curved, the band is read from somewhere else; lit, the black beside
  // it carries the light it throws. Neither is the picture it was
  // handed, and the bend is the only one of the two that reaches.
  field::CrtGlassParameters curved = flat;
  curved.uCurvature = 0.4f;
  EXPECT_GT(test::differing(
                throughLayer(field::crtGlass(curved),
                             field::crtGlassSampleRadius(curved), band, 64),
                drawn),
            0);
  field::CrtGlassParameters lit = flat;
  lit.uBloomRadius = 6;
  lit.uBloom = 1.6f;
  EXPECT_GT(test::differing(
                throughLayer(field::crtGlass(lit),
                             field::crtGlassSampleRadius(lit), band, 64),
                drawn),
            0);
  EXPECT_GT(field::crtGlassSampleRadius(curved), 0);
  EXPECT_FLOAT_EQ(field::crtGlassSampleRadius(lit), 0.0f);
}

TEST(Field, TheWholeScreenIsItsBeamUnderItsGlass) {
  // The composition taken apart. Given nothing to bend, light or
  // darken, the whole screen IS the beam under it — the same picture
  // the scanlines alone make of the same layer, guns, sweep and grain
  // included — which is what says the glass reads the beam and adds
  // nothing of its own on the way.
  const sk_sp<SkImage> band = bandAt(64, 20, 6);
  const field::CrtParameters screen{.uBounds = {0, 0, 64, 64},
                                    .uRgbShift = 0.8f,
                                    .uScanPitch = 3,
                                    .uRaster = 0.45f,
                                    .uNoise = 0.012f,
                                    .uJitter = 1.5f,
                                    .uFlicker = 0.2f,
                                    .uSync = 0.8f,
                                    .uTime = 1.25f};
  const field::CrtBeamParameters beam{.uBounds = {0, 0, 64, 64},
                                      .uRgbShift = 0.8f,
                                      .uScanPitch = 3,
                                      .uRaster = 0.45f,
                                      .uNoise = 0.012f,
                                      .uJitter = 1.5f,
                                      .uFlicker = 0.2f,
                                      .uSync = 0.8f,
                                      .uTime = 1.25f};
  EXPECT_TRUE(test::identical(
      throughScreen(screen, band, 64),
      throughLayer(field::crtBeam(beam), field::crtBeamSampleRadius(beam), band,
                   64)));
  // And the whole screen's reach is its passes' reaches together.
  field::CrtParameters curved = screen;
  curved.uCurvature = 0.2f;
  EXPECT_FLOAT_EQ(
      field::crtSampleRadius(curved),
      field::crtBeamSampleRadius(beam) +
          field::crtGlassSampleRadius({.uBounds = curved.uBounds,
                                       .uCurvature = curved.uCurvature}));
}
