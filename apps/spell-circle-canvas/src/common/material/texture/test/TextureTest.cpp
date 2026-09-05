/** @file
 * The texture feature: sources compare by identity across the erasure
 * and a producer bakes once; sampling dials enter equality; a region
 * cuts the image; a texture fills a material's slot as a leaf; the tools'
 * file names classify and a scratch folder discovers into sets that
 * decode into textures by role; the environment blurs and caches; bevel
 * normals are flat inside and tilted at the rim; an atlas grids, reads
 * both sprite tools' JSON, and packs loose images without overlap.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/texture/Atlas.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>
#include <sigilimage/decode/Decode.h>

#include <algorithm>
#include <boost/container/map.hpp>
#include <filesystem>
#include <string>
#include <vector>

#include "CubeContainers.h"
#include "ScratchDir.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::solid;

namespace {

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

SkColor pixelOf(const sk_sp<SkShader>& shader, int x, int y) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(x + 1, y + 1));
  SkCanvas canvas(bm);
  canvas.clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setShader(shader);
  canvas.drawPaint(paint);
  return bm.getColor(x, y);
}

}  // namespace

TEST(Texture, SourcesCompareByIdentityAcrossTheErasure) {
  const sk_sp<SkImage> a = solid(SK_ColorRED, 2, 2);
  const sk_sp<SkImage> b = solid(SK_ColorRED, 2, 2);
  EXPECT_EQ(Texture::of(a), Texture::of(a));
  EXPECT_FALSE(Texture::of(a) == Texture::of(b));
  // A different source kind is never equal, whatever it yields.
  // the callable is invoked on every layout, so its capture must survive each
  // return
  // NOLINTNEXTLINE(performance-no-automatic-move)
  const Texture produced = Texture::produce("red", [a] { return a; });
  EXPECT_FALSE(produced == Texture::of(a));
  // the callable is invoked on every layout, so its capture must survive each
  // return
  // NOLINTNEXTLINE(performance-no-automatic-move)
  EXPECT_EQ(produced, Texture::produce("red", [b] { return b; }));
  EXPECT_FALSE(Texture().valid());
  EXPECT_EQ(Texture(), Texture());
}

TEST(Texture, ProducerBakesOnceAndShares) {
  int bakes = 0;
  const Texture t = Texture::produce("counted", [&] {
    ++bakes;
    return solid(SK_ColorGREEN, 3, 3);
  });
  // the copy is what the test exercises
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  const Texture copy = t;
  EXPECT_EQ(bakes, 0);
  const sk_sp<SkImage> first = t.image();
  EXPECT_EQ(bakes, 1);
  EXPECT_EQ(copy.image().get(), first.get());
  EXPECT_EQ(bakes, 1);
}

TEST(Texture, SamplingDialsEnterEquality) {
  const sk_sp<SkImage> img = solid(SK_ColorBLUE, 4, 4);
  const Texture base = Texture::of(img);
  EXPECT_FALSE(base == Texture(base).tile(SkTileMode::kRepeat));
  EXPECT_FALSE(base == Texture(base).at({3, 0}));
  EXPECT_FALSE(base == Texture(base).region(SkIRect::MakeWH(2, 2)));
  EXPECT_FALSE(base == Texture(base).filter(SkFilterMode::kNearest));
  EXPECT_EQ(Texture(base).tile(SkTileMode::kRepeat),
            Texture(base).tile(SkTileMode::kRepeat, SkTileMode::kRepeat));
}

TEST(Texture, RegionCutsAndPlacementMoves) {
  // A 4x2 sheet: left half red, right half blue.
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(4, 2));
  bm.eraseColor(SK_ColorRED);
  bm.erase(SK_ColorBLUE, SkIRect::MakeXYWH(2, 0, 2, 2));
  bm.setImmutable();
  const sk_sp<SkImage> sheet = bm.asImage();

  const Texture right =
      Texture::of(sheet).region(SkIRect::MakeXYWH(2, 0, 2, 2));
  EXPECT_EQ(right.size(), SkISize::Make(2, 2));
  EXPECT_EQ(pixelOf(right.shader(), 0, 0), SK_ColorBLUE);
  // The cut is kept: the same image comes back for the same source.
  EXPECT_EQ(right.image().get(), right.image().get());

  const Texture moved = Texture::of(sheet).at({-2, 0});
  EXPECT_EQ(pixelOf(moved.shader(), 0, 0), SK_ColorBLUE);
  EXPECT_EQ(pixelOf(Texture::of(sheet).shader(), 0, 0), SK_ColorRED);
}

TEST(Texture, FillsAMaterialSlotAsALeaf) {
  struct NoParams {
    float uUnused;
  };
  auto recipe = std::make_shared<const Recipe>(
      Recipe::of<NoParams>("sampler").child("uImage"));
  const sk_sp<SkImage> img = solid(SK_ColorRED, 2, 2);
  Material a(recipe, NoParams{0});
  a.child("uImage", Texture::of(img));
  Material b(recipe, NoParams{0});
  b.child("uImage", Texture::of(img));
  EXPECT_EQ(a, b);
  ASSERT_NE(a.leaf("uImage"), nullptr);
  EXPECT_EQ(a.child("uImage"), nullptr);
  EXPECT_FALSE(a.isAnimated());
  b.child("uImage", Texture::of(img).tile(SkTileMode::kRepeat));
  EXPECT_FALSE(a == b);
  // A slot holding a leaf and one holding a material are unequal.
  Material c(recipe, NoParams{0});
  c.child("uImage", Material(recipe, NoParams{0}));
  EXPECT_FALSE(a == c);
}

namespace {

/** ONE FILE NAME AS A TOOL WRITES IT, and what this library reads out of
 *  it: which map it is, and — for a normal map — which way round its
 *  green channel runs, which is the one thing a name can say that a
 *  picture cannot. */
struct ToolName {
  const char* what;
  const char* file;
  texture::Role role;
  bool directX;
};

class ToolFileName : public testing::TestWithParam<ToolName> {};

std::string toolNameOf(const testing::TestParamInfo<ToolName>& info) {
  return info.param.what;
}

const ToolName kSubstance[] = {
    {"BaseColor", "Rock_BaseColor.png", texture::Role::BaseColor, false},
    {"Normal", "Rock_Normal.png", texture::Role::Normal, false},
    {"NormalDX", "Rock_NormalDX.png", texture::Role::Normal, true},
    {"NormalDirectX", "Rock_Normal_DirectX.png", texture::Role::Normal, true},
    {"Roughness", "Rock_Roughness.png", texture::Role::Roughness, false},
    {"Metallic", "Rock_Metallic.png", texture::Role::Metallic, false},
    {"Height", "Rock_Height.png", texture::Role::Height, false},
    {"Emissive", "Rock_Emissive.png", texture::Role::Emissive, false},
    {"OcclusionRoughnessMetallic",
     "Rock_OcclusionRoughnessMetallic.png", texture::Role::Packed, false},
};

const ToolName kPolyHaven[] = {
    {"Diff", "metal_plate_diff_1k.png", texture::Role::BaseColor, false},
    {"NorGl", "metal_plate_nor_gl_1k.png", texture::Role::Normal, false},
    {"NorDx", "metal_plate_nor_dx_2k.png", texture::Role::Normal, true},
    {"Rough", "metal_plate_rough_1k.png", texture::Role::Roughness, false},
    {"Metal", "metal_plate_metal_1k.png", texture::Role::Metallic, false},
    {"Ao", "metal_plate_ao_1k.png", texture::Role::Occlusion, false},
    {"Arm", "metal_plate_arm_1k.png", texture::Role::Packed, false},
    {"Disp", "metal_plate_disp_1k.png", texture::Role::Height, false},
};

const ToolName kAmbientCg[] = {
    {"Color", "Metal049A_1K-PNG_Color.png", texture::Role::BaseColor, false},
    {"NormalGL", "Metal049A_1K-PNG_NormalGL.png", texture::Role::Normal, false},
    {"NormalDX", "Metal049A_1K-PNG_NormalDX.png", texture::Role::Normal, true},
    {"Metalness", "Metal049A_1K-PNG_Metalness.png", texture::Role::Metallic,
     false},
    {"AmbientOcclusion", "Metal049A_1K-PNG_AmbientOcclusion.png",
     texture::Role::Occlusion, false},
    {"Displacement", "Metal049A_1K-PNG_Displacement.png", texture::Role::Height,
     false},
};

const ToolName kOtherwise[] = {
    {"GltfOrm", "thing_orm.png", texture::Role::Packed, false},
    {"GltfAlbedo", "thing_albedo.jpg", texture::Role::BaseColor, false},
    {"NoRoleWordAtAll", "photo.png", texture::Role::Unknown, false},
    {"ACameraSerial", "IMG_2048.png", texture::Role::Unknown, false},
};

}  // namespace

TEST_P(ToolFileName, NamesTheMapItHoldsAndItsNormalConvention) {
  const texture::Classified classified = texture::classify(GetParam().file);
  EXPECT_EQ(classified.role, GetParam().role);
  EXPECT_EQ(classified.directX, GetParam().directX);
}

INSTANTIATE_TEST_SUITE_P(SubstanceWrites, ToolFileName,
                         testing::ValuesIn(kSubstance), toolNameOf);
INSTANTIATE_TEST_SUITE_P(PolyHavenWrites, ToolFileName,
                         testing::ValuesIn(kPolyHaven), toolNameOf);
INSTANTIATE_TEST_SUITE_P(AmbientCgWrites, ToolFileName,
                         testing::ValuesIn(kAmbientCg), toolNameOf);
INSTANTIATE_TEST_SUITE_P(AnyoneElseWrites, ToolFileName,
                         testing::ValuesIn(kOtherwise), toolNameOf);

TEST(TextureSet, TheSetNameIsWhatStandsBeforeTheRoleWord) {
  // Which files belong together is the whole of what a set is, so the
  // name has to survive the role word and the size token being cut off
  // it — otherwise two maps of one material land in two sets.
  using texture::classify;
  EXPECT_EQ(classify("Rock_BaseColor.png").set, "Rock");
  EXPECT_EQ(classify("metal_plate_diff_1k.png").set, "metal_plate");
  EXPECT_EQ(classify("metal_plate_nor_gl_1k.png").set, "metal_plate");
  EXPECT_EQ(classify("Metal049A_1K-PNG_Color.png").set, "Metal049A_1K_PNG");
}

TEST(TextureSet, AUsageWordNamesARoleAndAnUnrecognisedOneIsUnknown) {
  EXPECT_EQ(texture::roleForUsage("ambientOcclusion"), texture::Role::Occlusion);
  EXPECT_EQ(texture::roleForUsage("baseColor"), texture::Role::BaseColor);
  EXPECT_EQ(texture::roleForUsage("wibble"), texture::Role::Unknown);
  EXPECT_EQ(texture::name(texture::Role::Packed), "packed");
}

TEST(TextureSet, DiscoversAndDecodesByRole) {
  namespace fs = std::filesystem;
  const sigil::test::ScratchDir scratch("sigilmaterial_texset");
  for (const char* name :
       {"tiles_diff_1k.png", "tiles_nor_dx_1k.png", "tiles_arm_1k.png",
        "tiles_rough_1k.png", "other_BaseColor.png", "notes.txt"})
    scratch.write(name, "x");
  const std::vector<texture::TextureSet> sets = texture::discover(scratch.path);
  ASSERT_EQ(sets.size(), 2u);
  EXPECT_EQ(sets[0].name, "other");
  EXPECT_EQ(sets[1].name, "tiles");
  const texture::TextureSet& tiles = sets[1];
  EXPECT_TRUE(tiles.normalDirectX);
  EXPECT_EQ(tiles.files.size(), 4u);

  boost::container::map<std::string, sk_sp<SkImage>> decoded;
  const auto decode = [&](const fs::path& p) {
    sk_sp<SkImage>& img = decoded[p.filename().string()];
    if (!img) img = solid(SK_ColorWHITE, 2, 2);
    return img;
  };
  const texture::TextureMaps maps = texture::fromFiles(tiles, decode);
  EXPECT_EQ(maps.name, "tiles");
  EXPECT_TRUE(maps.normalDirectX);
  ASSERT_NE(maps.map(texture::Role::BaseColor), nullptr);
  EXPECT_EQ(maps.map(texture::Role::BaseColor)->image().get(),
            decoded["tiles_diff_1k.png"].get());
  // A scanned material is meant to repeat.
  EXPECT_EQ(maps.map(texture::Role::BaseColor)->tileX(), SkTileMode::kRepeat);
  EXPECT_EQ(maps.map(texture::Role::Packed)->image().get(),
            decoded["tiles_arm_1k.png"].get());
  EXPECT_EQ(maps.map(texture::Role::Emissive), nullptr);

  // The usage door: the first word naming a role wins, in key order.
  const sk_sp<SkImage> a = solid(SK_ColorWHITE, 2, 2);
  const sk_sp<SkImage> b = solid(SK_ColorWHITE, 2, 2);
  const texture::TextureMaps u = texture::fromUsageMap(
      {{"diffuse", a}, {"baseColor", b}, {"normal", b}, {"height", a}});
  EXPECT_TRUE(u.normalDirectX);
  EXPECT_EQ(u.map(texture::Role::BaseColor)->image().get(), b.get());
  EXPECT_EQ(u.map(texture::Role::Normal)->image().get(), b.get());
  EXPECT_EQ(u.map(texture::Role::Height)->image().get(), a.get());
}

TEST(EnvironmentMap, RoughnessBlursAndEachBucketIsBuiltOnce) {
  const EnvironmentMap env = EnvironmentMap::sunset(128);
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

TEST(EnvironmentMap, TheEquirectConventionRoundTrips) {
  // A direction and a panorama coordinate are the same thing said twice,
  // and every consumer of the value depends on them agreeing.
  for (float u : {0.02f, 0.17f, 0.5f, 0.83f}) {
    for (float v : {0.05f, 0.3f, 0.5f, 0.95f}) {
      const SkV2 back = equirectUv(equirectDirection({u, v}));
      EXPECT_NEAR(back.x, u, 1e-4f) << u << "," << v;
      EXPECT_NEAR(back.y, v, 1e-4f) << u << "," << v;
    }
  }
  // The azimuth is periodic: u = 0 and u = 1 are one direction, and the
  // inverse answers whichever end of the turn it landed on.
  const SkV2 seam = equirectUv(equirectDirection({0.0f, 0.5f}));
  EXPECT_NEAR(std::min(seam.x, 1.0f - seam.x), 0.0f, 1e-4f);
  // v = 0 is the zenith and u = 0.5 looks along -z.
  const SkV3 up = equirectDirection({0.5f, 0.0f});
  EXPECT_NEAR(up.y, 1.0f, 1e-5f);
  const SkV3 forward = equirectDirection({0.5f, 0.5f});
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
    const SkV2 uv = equirectUv(direction);
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
  const sigil::image::test::CubeFaces kFace = {
      SK_ColorRED,    SK_ColorGREEN, SK_ColorBLUE,
      SK_ColorYELLOW, SK_ColorCYAN,  SK_ColorMAGENTA};
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
  const EnvironmentMap fromSheet = EnvironmentMap::fromCubeMap(column.asImage());
  ASSERT_TRUE(fromSheet.valid());

  const SkV3 axes[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  const auto expectSameAsSheet = [&](const std::vector<std::byte>& bytes,
                                     const char* name) {
    auto asset = sigil::image::decodeImage(bytes.data(), bytes.size(), {}, name);
    ASSERT_TRUE(asset.has_value()) << name;
    const EnvironmentMap env =
        EnvironmentMap::fromCubeMap(asset->frames()[0].image);
    ASSERT_TRUE(env.valid()) << name;
    ASSERT_EQ(env.size(), fromSheet.size()) << name;
    const sk_sp<SkImage> pano = env.image(0);
    const sk_sp<SkImage> sheet = fromSheet.image(0);
    for (int i = 0; i < 6; ++i) {
      const SkV2 uv = equirectUv(axes[i]);
      const int x = std::min((int)(uv.x * (float)pano->width()), pano->width() - 1);
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
      EnvironmentMap::fromEquirect(constantPanorama(64, 32, sky));
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
      EnvironmentMap::fromEquirect(constantPanorama(64, 32, bright));
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
  const EnvironmentMap sky = EnvironmentMap::sunset(128);
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

TEST(Atlas, AGridCutsEqualCellsRowMajorAndNamesThemByIndex) {
  const Atlas atlas = Atlas::grid(Texture::of(solid(SK_ColorRED, 8, 4)), 4, 2);
  ASSERT_EQ(atlas.regions().size(), 8u);
  EXPECT_EQ(atlas.regions()[5].rect, SkIRect::MakeXYWH(2, 2, 2, 2));
  EXPECT_EQ(atlas.regions()[5].name, "5");
  ASSERT_NE(atlas.sequence("all"), nullptr);
  EXPECT_EQ(atlas.sequence("all")->size(), 8u);
  EXPECT_EQ(atlas.frame("all", 9).region(), atlas.region(1).region());
  EXPECT_EQ(atlas.region(5).size(), SkISize::Make(2, 2));
  EXPECT_FALSE(atlas.region(8).valid());
}

TEST(Atlas, ReadsTexturePackerAndDerivesSequences) {
  const char* json = R"({"frames": {
    "walk_02.png": {"frame": {"x": 10, "y": 0, "w": 10, "h": 10}, "rotated": false,
      "trimmed": true, "spriteSourceSize": {"x": 2, "y": 3, "w": 10, "h": 10},
      "sourceSize": {"w": 16, "h": 16}},
    "walk_01.png": {"frame": {"x": 0, "y": 0, "w": 10, "h": 10}, "rotated": true},
    "idle.png": {"frame": {"x": 20, "y": 0, "w": 5, "h": 5}}
  }, "meta": {"app": "TexturePacker"}})";
  const std::optional<Atlas> atlas =
      Atlas::fromTexturePacker(Texture::of(solid(SK_ColorRED, 32, 16)), json);
  ASSERT_TRUE(atlas);
  ASSERT_EQ(atlas->regions().size(), 3u);
  const AtlasRegion* walk2 = atlas->find("walk_02");
  ASSERT_NE(walk2, nullptr);
  EXPECT_EQ(walk2->rect, SkIRect::MakeXYWH(10, 0, 10, 10));
  EXPECT_EQ(walk2->sourceSize, SkISize::Make(16, 16));
  EXPECT_EQ(walk2->sourceOffset, SkIPoint::Make(2, 3));
  EXPECT_TRUE(atlas->find("walk_01")->rotated);
  const std::vector<size_t>* walk = atlas->sequence("walk");
  ASSERT_NE(walk, nullptr);
  ASSERT_EQ(walk->size(), 2u);
  EXPECT_EQ(atlas->regions()[(*walk)[0]].name, "walk_01");
  EXPECT_EQ(atlas->regions()[(*walk)[1]].name, "walk_02");
  EXPECT_EQ(atlas->sequence("idle")->size(), 1u);
  EXPECT_FALSE(Atlas::fromTexturePacker(Texture(), "not json"));
  EXPECT_FALSE(Atlas::fromTexturePacker(Texture(), R"({"meta": {}})"));
  // The array form reads the same.
  const char* array = R"({"frames": [
    {"filename": "a.png", "frame": {"x": 0, "y": 0, "w": 4, "h": 4}},
    {"filename": "b.png", "frame": {"x": 4, "y": 0, "w": 4, "h": 4}}]})";
  const std::optional<Atlas> fromArray =
      Atlas::fromTexturePacker(Texture(), array);
  ASSERT_TRUE(fromArray);
  EXPECT_EQ(fromArray->regions()[1].name, "b");
}

TEST(Atlas, AsepritesTagsBecomeTheSequences) {
  const char* json = R"({"frames": [
    {"filename": "hero 0.aseprite", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}, "duration": 100},
    {"filename": "hero 1.aseprite", "frame": {"x": 8, "y": 0, "w": 8, "h": 8}, "duration": 100},
    {"filename": "hero 2.aseprite", "frame": {"x": 16, "y": 0, "w": 8, "h": 8}, "duration": 100}
  ], "meta": {"app": "Aseprite", "frameTags": [
    {"name": "run", "from": 0, "to": 1, "direction": "forward"},
    {"name": "jump", "from": 2, "to": 2, "direction": "forward"}]}})";
  const std::optional<Atlas> atlas =
      Atlas::fromAseprite(Texture::of(solid(SK_ColorRED, 24, 8)), json);
  ASSERT_TRUE(atlas);
  ASSERT_EQ(atlas->regions().size(), 3u);
  ASSERT_NE(atlas->sequence("run"), nullptr);
  EXPECT_EQ(*atlas->sequence("run"), (std::vector<size_t>{0, 1}));
  EXPECT_EQ(*atlas->sequence("jump"), (std::vector<size_t>{2}));
  EXPECT_EQ(atlas->sequence("all"), nullptr);
  EXPECT_EQ(atlas->frame("run", 3).region(), SkIRect::MakeXYWH(8, 0, 8, 8));
  // No tags: one sequence through every frame.
  const char* untagged = R"({"frames": [
    {"filename": "x", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {}})";
  const std::optional<Atlas> all = Atlas::fromAseprite(Texture(), untagged);
  ASSERT_TRUE(all);
  EXPECT_EQ(all->sequence("all")->size(), 1u);
}

TEST(Atlas, PacksWithoutOverlapAndKeepsPixels) {
  std::vector<std::pair<std::string, sk_sp<SkImage>>> images;
  const SkColor colors[] = {SK_ColorRED, SK_ColorGREEN, SK_ColorBLUE,
                            SK_ColorYELLOW, SK_ColorCYAN};
  images.reserve(5);
  for (int i = 0; i < 5; ++i)
    images.emplace_back("s" + std::to_string(i),
                        solid(colors[i], 6 + 3 * i, 5 + 2 * i));
  const Atlas atlas = Atlas::pack(images, 1);
  ASSERT_EQ(atlas.regions().size(), 5u);
  ASSERT_TRUE(atlas.sheet().valid());
  for (size_t i = 0; i < 5; ++i) {
    const AtlasRegion* r = atlas.find("s" + std::to_string(i));
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->rect.size(), SkISize::Make(6 + 3 * (int)i, 5 + 2 * (int)i));
    for (size_t j = 0; j < i; ++j)
      EXPECT_FALSE(SkIRect::Intersects(r->rect, atlas.regions()[j].rect));
    // The region reads back the image it was packed from.
    EXPECT_EQ(pixelOf(atlas.region(r->name).shader(), 1, 1), colors[i]);
  }
  // A packed sheet is a power of two on a side.
  const SkISize side = atlas.sheet().size();
  EXPECT_EQ(side.width(), side.height());
  const auto width = (uint32_t)side.width();
  EXPECT_EQ(width & (width - 1u), 0u);
}
