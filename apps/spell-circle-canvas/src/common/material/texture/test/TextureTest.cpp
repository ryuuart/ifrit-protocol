/** @file
 * The texture leaf: sources compare by identity across the erasure and a
 * producer bakes once, sampling dials enter equality, a region cuts the
 * image and a placement moves it, and a texture fills a material's slot
 * as a leaf. Beside them, the tools' file names classify and a scratch
 * folder discovers into sets that decode into textures by role.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilimage/decode/Decode.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::solid;

namespace {

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
    {"OcclusionRoughnessMetallic", "Rock_OcclusionRoughnessMetallic.png",
     texture::Role::Packed, false},
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
  EXPECT_EQ(texture::roleForUsage("ambientOcclusion"),
            texture::Role::Occlusion);
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
