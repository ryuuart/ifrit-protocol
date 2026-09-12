/** @file
 * The environment map: a roughness reads its own bucket and each is
 * built once, the equirectangular convention round-trips, six faces and a cube
 * sheet resample into one panorama, the irradiance of a constant sky is
 * that constant, float survives the chain, and a ground colour replaces
 * the lower hemisphere.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkSurface.h>
#include <sigilimage/decode/Decode.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>

#include <cmath>
#include <vector>

#include "CubeContainers.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::solid;

namespace {

/** A sky with structure on both axes: a hot band at the horizon and a
 *  dark ground under it, so a blur has something to smear and a ground
 *  replacement has something to cover. The kit's named bakes are a
 *  layer above this feature; a test of the panorama itself writes its
 *  own radiance. */
EnvironmentMap bandedSky(int width) {
  return EnvironmentMap::baked(width, [](float u, float v) -> SkV3 {
    constexpr float kHorizon = 0.52f;
    if (v >= kHorizon) return {0.04f, 0.02f, 0.05f};
    const float t = v / kHorizon;
    const float band = 0.5f + 0.5f * std::sin(t * 40.0f);
    const float sun = std::exp(-((u - 0.5f) * (u - 0.5f)) / 0.002f);
    return {0.05f + t * band + 2.0f * sun, 0.10f + 0.4f * t, 0.30f - 0.2f * t};
  });
}

/** A panorama of one colour, in F32 so a value above 1 survives. */
sk_sp<SkImage> constantPanorama(int w, int h, SkColor4f color) {
  std::vector<float> px((size_t)w * h * 4);
  for (size_t i = 0; i < (size_t)w * h; ++i) {
    px[i * 4 + 0] = color.fR;
    px[i * 4 + 1] = color.fG;
    px[i * 4 + 2] = color.fB;
    px[i * 4 + 3] = color.fA;
  }
  const SkImageInfo info =
      SkImageInfo::Make(w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(
      {info, px.data(), (size_t)w * 4 * sizeof(float)});
}

/** One texel of a float image, read back as four floats. */
SkColor4f floatPixel(const sk_sp<SkImage>& image, int x, int y) {
  float px[4] = {0, 0, 0, 0};
  const SkImageInfo info =
      SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  EXPECT_TRUE(image->readPixels(nullptr, SkPixmap(info, px, sizeof(px)), x, y));
  return {px[0], px[1], px[2], px[3]};
}

}  // namespace

TEST(EnvironmentMap, RoughnessBlursAndEachBucketIsBuiltOnce) {
  const EnvironmentMap env = bandedSky(128);
  ASSERT_TRUE(env.valid());
  sk_sp<SkImage> sharp = env.image(0);
  sk_sp<SkImage> rough = env.image(0.6f);
  ASSERT_TRUE(sharp);
  ASSERT_TRUE(rough);
  EXPECT_NE(sharp.get(), rough.get());
  EXPECT_EQ(rough->width(), sharp->width());
  // Roughness is quantized into buckets and each bucket's blurred image is
  // built once and kept, so asking twice for the same roughness returns the
  // identical object rather than re-blurring the environment per draw.
  EXPECT_EQ(env.image(0.6f).get(), rough.get());
  EXPECT_EQ(env.size(), SkISize::Make(128, 64));
  const Texture t = env.texture(0.6f);
  EXPECT_EQ(t.tileX(), SkTileMode::kRepeat);
  EXPECT_EQ(t.tileY(), SkTileMode::kClamp);
  EXPECT_EQ(t.image().get(), rough.get());
}

TEST(EnvironmentMap, ASmallPanoramaKeepsItsOwnWidthInTheChain) {
  // The chain is bounded ABOVE so a 4K sky is not prefiltered nine times
  // at its full width. There is no lower bound: a 64-wide panorama
  // raised to 256 would be prefiltering pixels invented on the way up,
  // and the hand-set path has no lower bound either, so the two would
  // have disagreed about one picture.
  EXPECT_EQ(bandedSky(64).prefilterSize(), 64);
  EXPECT_EQ(bandedSky(128).prefilterSize(), 128);
  EXPECT_EQ(bandedSky(64).chain().front()->width(), 64);
  // …and above the bound it is the bound, whatever the source's width.
  EXPECT_EQ(bandedSky(2048).prefilterSize(), 1024);
  // A width set by hand is that width, either side of the bound.
  EXPECT_EQ(bandedSky(64).withPrefilterSize(256).prefilterSize(), 256);
  EXPECT_EQ(bandedSky(2048).withPrefilterSize(64).prefilterSize(), 64);
}

TEST(EnvironmentMap, TheEquirectangularConventionRoundTrips) {
  // A direction and a panorama coordinate are the same thing said twice,
  // and every consumer of the value depends on them agreeing.
  for (float u : {0.02f, 0.17f, 0.5f, 0.83f}) {
    for (float v : {0.05f, 0.3f, 0.5f, 0.95f}) {
      const SkV2 back = equirectangularUv(equirectangularDirection({u, v}));
      EXPECT_NEAR(back.x, u, 1e-4f) << u << "," << v;
      EXPECT_NEAR(back.y, v, 1e-4f) << u << "," << v;
    }
  }
  // The azimuth is periodic: u = 0 and u = 1 are one direction, and the
  // inverse answers whichever end of the turn it landed on.
  const SkV2 seam = equirectangularUv(equirectangularDirection({0.0f, 0.5f}));
  EXPECT_NEAR(std::min(seam.x, 1.0f - seam.x), 0.0f, 1e-4f);
  // v = 0 is the zenith and u = 0.5 looks along -z.
  const SkV3 up = equirectangularDirection({0.5f, 0.0f});
  EXPECT_NEAR(up.y, 1.0f, 1e-5f);
  const SkV3 forward = equirectangularDirection({0.5f, 0.5f});
  EXPECT_NEAR(forward.z, -1.0f, 1e-5f);
}

TEST(EnvironmentMap, SixFacesResampleIntoOnePanorama) {
  // +x -x +y -y +z -z, each its own colour, so where a face landed in the
  // panorama is legible from the pixel.
  const SkColor kFace[6] = {SK_ColorRED,    SK_ColorGREEN, SK_ColorBLUE,
                            SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};
  EnvironmentMap::Faces faces;
  for (int i = 0; i < 6; ++i) faces[i] = solid(kFace[i], 32, 32);
  const EnvironmentMap env = EnvironmentMap::fromFaces(faces, 128);
  ASSERT_TRUE(env.valid());
  EXPECT_EQ(env.size(), SkISize::Make(128, 64));

  // Sample the panorama where each face's centre direction lands. The
  // faces are solid, so a bilinear tap well inside one is that colour
  // exactly.
  const sk_sp<SkImage> pano = env.image(0);
  const auto colourAt = [&](SkV3 direction) {
    const SkV2 uv = equirectangularUv(direction);
    const int x = std::min((int)(uv.x * 128.0f), 127);
    const int y = std::min((int)(uv.y * 64.0f), 63);
    const SkColor4f c = floatPixel(pano, x, y);
    return SkColor4f{c.fR, c.fG, c.fB, 1};
  };
  const SkV3 axes[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  for (int i = 0; i < 6; ++i) {
    const SkColor4f got = colourAt(axes[i]);
    const SkColor4f want = SkColor4f::FromColor(kFace[i]);
    EXPECT_NEAR(got.fR, want.fR, 0.02f) << "face " << i;
    EXPECT_NEAR(got.fG, want.fG, 0.02f) << "face " << i;
    EXPECT_NEAR(got.fB, want.fB, 0.02f) << "face " << i;
  }
}

TEST(EnvironmentMap, ACubeSheetIsUnpackedByItsLayout) {
  // A 6:1 row and a 1:6 column carry the faces in the order they are
  // named, and both resolve to the same panorama.
  SkBitmap row;
  row.allocPixels(SkImageInfo::MakeN32Premul(6 * 16, 16));
  SkBitmap column;
  column.allocPixels(SkImageInfo::MakeN32Premul(16, 6 * 16));
  const SkColor kFace[6] = {SK_ColorRED,    SK_ColorGREEN, SK_ColorBLUE,
                            SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};
  for (int i = 0; i < 6; ++i) {
    SkCanvas(row).clear(SK_ColorTRANSPARENT);
    SkCanvas(column).clear(SK_ColorTRANSPARENT);
  }
  for (int i = 0; i < 6; ++i) {
    SkPaint paint;
    paint.setColor(kFace[i]);
    SkCanvas(row).drawIRect(SkIRect::MakeXYWH(i * 16, 0, 16, 16), paint);
    SkCanvas(column).drawIRect(SkIRect::MakeXYWH(0, i * 16, 16, 16), paint);
  }
  row.setImmutable();
  column.setImmutable();
  const EnvironmentMap fromRow = EnvironmentMap::fromCubeMap(row.asImage());
  const EnvironmentMap fromColumn =
      EnvironmentMap::fromCubeMap(column.asImage());
  ASSERT_TRUE(fromRow.valid());
  ASSERT_TRUE(fromColumn.valid());
  EXPECT_EQ(fromRow.size(), fromColumn.size());
  const SkColor4f a = floatPixel(fromRow.image(0), 32, 16);
  const SkColor4f b = floatPixel(fromColumn.image(0), 32, 16);
  EXPECT_NEAR(a.fR, b.fR, 1e-5f);
  EXPECT_NEAR(a.fG, b.fG, 1e-5f);
  EXPECT_NEAR(a.fB, b.fB, 1e-5f);
}

TEST(EnvironmentMap, ACubeMapInAContainerIsTheSheetOfItsFaces) {
  // A DDS through OpenImageIO and a KTX 1 or 2 through the KTX reader
  // each decode to the six faces as one 1:6 column, which is a sheet
  // fromCubeMap already reads — so the panorama is the same texel the
  // sheet of the same faces gives at each face's centre direction.
  const sigil::image::test::CubeFaces kFace = {SK_ColorRED,  SK_ColorGREEN,
                                               SK_ColorBLUE, SK_ColorYELLOW,
                                               SK_ColorCYAN, SK_ColorMAGENTA};
  constexpr int kEdge = 16;
  SkBitmap column;
  column.allocPixels(SkImageInfo::MakeN32Premul(kEdge, 6 * kEdge));
  for (int i = 0; i < 6; ++i) {
    SkPaint paint;
    paint.setColor(kFace[(size_t)i]);
    SkCanvas(column).drawIRect(SkIRect::MakeXYWH(0, i * kEdge, kEdge, kEdge),
                               paint);
  }
  column.setImmutable();
  const EnvironmentMap fromSheet =
      EnvironmentMap::fromCubeMap(column.asImage());
  ASSERT_TRUE(fromSheet.valid());

  const SkV3 axes[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  const auto expectSameAsSheet = [&](const std::vector<std::byte>& bytes,
                                     const char* name) {
    auto asset =
        sigil::image::decodeImage(bytes.data(), bytes.size(), {}, name);
    ASSERT_TRUE(asset.has_value()) << name;
    const EnvironmentMap env =
        EnvironmentMap::fromCubeMap(asset->frames()[0].image);
    ASSERT_TRUE(env.valid()) << name;
    ASSERT_EQ(env.size(), fromSheet.size()) << name;
    const sk_sp<SkImage> pano = env.image(0);
    const sk_sp<SkImage> sheet = fromSheet.image(0);
    for (int i = 0; i < 6; ++i) {
      const SkV2 uv = equirectangularUv(axes[i]);
      const int x =
          std::min((int)(uv.x * (float)pano->width()), pano->width() - 1);
      const int y =
          std::min((int)(uv.y * (float)pano->height()), pano->height() - 1);
      const SkColor4f got = floatPixel(pano, x, y);
      const SkColor4f want = floatPixel(sheet, x, y);
      EXPECT_EQ(got.fR, want.fR) << name << " face " << i;
      EXPECT_EQ(got.fG, want.fG) << name << " face " << i;
      EXPECT_EQ(got.fB, want.fB) << name << " face " << i;
      // …and that texel IS the face, so the order the container names
      // its faces in is the order the sheet reads them.
      const SkColor4f face = SkColor4f::FromColor(kFace[(size_t)i]);
      EXPECT_NEAR(got.fR, face.fR, 0.02f) << name << " face " << i;
      EXPECT_NEAR(got.fG, face.fG, 0.02f) << name << " face " << i;
      EXPECT_NEAR(got.fB, face.fB, 0.02f) << name << " face " << i;
    }
  };
  expectSameAsSheet(sigil::image::test::cubeKtx1(kFace, kEdge), "cube.ktx");
  expectSameAsSheet(sigil::image::test::cubeKtx2(kFace, kEdge), "cube.ktx2");
  // The DDS reader is OpenImageIO's; without that backend the bytes
  // decode to nothing, which is the one outcome the case cannot judge.
  const auto dds = sigil::image::test::cubeDds(kFace, kEdge);
  if (sigil::image::probeImage(dds.data(), dds.size(), "cube.dds"))
    expectSameAsSheet(dds, "cube.dds");
}

TEST(EnvironmentMap, IrradianceOfAConstantPanoramaIsTheConstant) {
  // The cosine convolution is normalised by its own weights, so a sky of
  // one radiance answers that radiance from every normal — which is what
  // makes it the number a Lambertian body multiplies its albedo by.
  const SkColor4f sky{0.2f, 0.55f, 0.9f, 1};
  const EnvironmentMap env =
      EnvironmentMap::fromEquirectangular(constantPanorama(64, 32, sky));
  const sk_sp<SkImage> lobe = env.irradiance();
  ASSERT_TRUE(lobe);
  EXPECT_EQ(lobe->dimensions(), SkISize::Make(32, 16));
  for (int y : {0, 8, 15}) {
    for (int x : {0, 16, 31}) {
      const SkColor4f got = floatPixel(lobe, x, y);
      EXPECT_NEAR(got.fR, sky.fR, 1e-4f);
      EXPECT_NEAR(got.fG, sky.fG, 1e-4f);
      EXPECT_NEAR(got.fB, sky.fB, 1e-4f);
    }
  }
  // And the flat fallback is that same constant.
  const SkColor4f mean = env.average();
  EXPECT_NEAR(mean.fR, sky.fR, 1e-4f);
  EXPECT_NEAR(mean.fG, sky.fG, 1e-4f);
  EXPECT_NEAR(mean.fB, sky.fB, 1e-4f);
}

TEST(EnvironmentMap, FloatSurvivesTheBucketsAndTheChain) {
  // An HDRI's whole point is the values above one; a blur that clamped
  // them would turn a sun into a white disc of the same brightness as
  // the sky beside it.
  const SkColor4f bright{6.0f, 3.0f, 1.5f, 1};
  const EnvironmentMap env =
      EnvironmentMap::fromEquirectangular(constantPanorama(64, 32, bright));
  for (float roughness : {0.0f, 0.4f, 1.0f}) {
    const SkColor4f got = floatPixel(env.image(roughness), 12, 7);
    EXPECT_NEAR(got.fR, bright.fR, 1e-3f) << roughness;
    EXPECT_NEAR(got.fG, bright.fG, 1e-3f) << roughness;
    EXPECT_NEAR(got.fB, bright.fB, 1e-3f) << roughness;
  }
  const SkColor4f mean = env.average();
  EXPECT_NEAR(mean.fR, bright.fR, 1e-3f);

  // The chain is one mip pyramid: nine levels, each half the last, and
  // level 0 at the prefilter size.
  const EnvironmentMap sized = env.withPrefilterSize(256);
  EXPECT_EQ(sized.prefilterSize(), 256);
  const std::vector<sk_sp<SkImage>> levels = sized.chain();
  ASSERT_EQ((int)levels.size(), EnvironmentMap::kLevels);
  for (int i = 0; i < EnvironmentMap::kLevels; ++i) {
    ASSERT_TRUE(levels[i]);
    EXPECT_EQ(levels[i]->width(), std::max(256 >> i, 2)) << i;
    EXPECT_EQ(levels[i]->height(), std::max((256 >> i) / 2, 1)) << i;
  }
  EXPECT_NEAR(floatPixel(levels[4], 4, 2).fR, bright.fR, 1e-3f);
}

TEST(EnvironmentMap, GroundColourReplacesTheLowerHemisphere) {
  const EnvironmentMap sky = bandedSky(128);
  const EnvironmentMap floored = sky.withGround({0.05f, 0.05f, 0.05f, 1});
  ASSERT_TRUE(floored.valid());
  EXPECT_EQ(floored.size(), sky.size());
  // Below the horizon is the colour asked for; above it the sky stands.
  const SkColor4f below = floatPixel(floored.image(0), 64, 60);
  EXPECT_NEAR(below.fR, 0.05f, 1e-3f);
  EXPECT_NEAR(below.fB, 0.05f, 1e-3f);
  const SkColor4f above = floatPixel(floored.image(0), 64, 4);
  const SkColor4f original = floatPixel(sky.image(0), 64, 4);
  EXPECT_NEAR(above.fR, original.fR, 1e-5f);
  EXPECT_FALSE(floored == sky);
}

TEST(Bevel, TheNormalsAreFlatInsideAndTiltedAtTheRim) {
  const SkPath shape = SkPath::Circle(50, 50, 40);
  const Texture normals = bevelNormals(shape, SkIRect::MakeWH(100, 100), 10);
  sk_sp<SkImage> img = normals.image();
  ASSERT_TRUE(img);
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(100, 100));
  ASSERT_TRUE(img->readPixels(nullptr, bm.pixmap(), 0, 0));
  // Normal-map encoding: rgb = n * 0.5 + 0.5, so a flat normal pointing
  // straight out of the surface is (128, 128, 255) and the mid-grey 128 is
  // the zero of each axis. The interior of a bevel is flat.
  const SkColor center = bm.getColor(50, 50);
  EXPECT_GT(SkColorGetB(center), 240u);
  EXPECT_NEAR(SkColorGetR(center), 128, 6);
  // x runs to the right, so the LEFT rim tilts toward -x and its red
  // channel drops below the 128 zero point. A sign flip here would light
  // every bevelled shape from the wrong side.
  const SkColor rim = bm.getColor(13, 50);
  EXPECT_LT(SkColorGetR(rim), 110u);
  // The bounds-free overload places the map so device xy reads it: the
  // map's corner sits at the outset bounds' corner.
  const Texture placed = bevelNormals(SkPath::Circle(200, 200, 40), 10);
  EXPECT_FLOAT_EQ(placed.uv().getTranslateX(), 148);
  EXPECT_FLOAT_EQ(placed.uv().getTranslateY(), 148);
}
