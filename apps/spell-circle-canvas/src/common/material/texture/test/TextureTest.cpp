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
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <cstdio>
#include <filesystem>
#include <map>

using namespace sigil::material;

namespace {

sk_sp<SkImage> solid(SkColor color, int w, int h) {
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  bm.eraseColor(color);
  bm.setImmutable();
  return bm.asImage();
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

TEST(TextureSet, ClassifiesTheToolsNames) {
  using textures::classify;
  using textures::Role;
  // Substance Painter / Designer.
  EXPECT_EQ(classify("Rock_BaseColor.png").role, Role::BaseColor);
  EXPECT_EQ(classify("Rock_BaseColor.png").set, "Rock");
  EXPECT_EQ(classify("Rock_Normal.png").role, Role::Normal);
  EXPECT_FALSE(classify("Rock_Normal.png").directX);
  EXPECT_TRUE(classify("Rock_NormalDX.png").directX);
  EXPECT_TRUE(classify("Rock_Normal_DirectX.png").directX);
  EXPECT_EQ(classify("Rock_Roughness.png").role, Role::Roughness);
  EXPECT_EQ(classify("Rock_Metallic.png").role, Role::Metallic);
  EXPECT_EQ(classify("Rock_Height.png").role, Role::Height);
  EXPECT_EQ(classify("Rock_Emissive.png").role, Role::Emissive);
  EXPECT_EQ(classify("Rock_OcclusionRoughnessMetallic.png").role, Role::Packed);
  // Poly Haven.
  EXPECT_EQ(classify("metal_plate_diff_1k.png").role, Role::BaseColor);
  EXPECT_EQ(classify("metal_plate_diff_1k.png").set, "metal_plate");
  EXPECT_EQ(classify("metal_plate_nor_gl_1k.png").role, Role::Normal);
  EXPECT_FALSE(classify("metal_plate_nor_gl_1k.png").directX);
  EXPECT_EQ(classify("metal_plate_nor_gl_1k.png").set, "metal_plate");
  EXPECT_TRUE(classify("metal_plate_nor_dx_2k.png").directX);
  EXPECT_EQ(classify("metal_plate_rough_1k.png").role, Role::Roughness);
  EXPECT_EQ(classify("metal_plate_metal_1k.png").role, Role::Metallic);
  EXPECT_EQ(classify("metal_plate_ao_1k.png").role, Role::Occlusion);
  EXPECT_EQ(classify("metal_plate_arm_1k.png").role, Role::Packed);
  EXPECT_EQ(classify("metal_plate_disp_1k.png").role, Role::Height);
  // ambientCG.
  EXPECT_EQ(classify("Metal049A_1K-PNG_Color.png").role, Role::BaseColor);
  EXPECT_EQ(classify("Metal049A_1K-PNG_NormalGL.png").role, Role::Normal);
  EXPECT_TRUE(classify("Metal049A_1K-PNG_NormalDX.png").directX);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Metalness.png").role, Role::Metallic);
  EXPECT_EQ(classify("Metal049A_1K-PNG_AmbientOcclusion.png").role,
            Role::Occlusion);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Displacement.png").role, Role::Height);
  EXPECT_EQ(classify("Metal049A_1K-PNG_Color.png").set, "Metal049A_1K_PNG");
  // glTF-ish.
  EXPECT_EQ(classify("thing_orm.png").role, Role::Packed);
  EXPECT_EQ(classify("thing_albedo.jpg").role, Role::BaseColor);
  // Nothing recognizable.
  EXPECT_EQ(classify("photo.png").role, Role::Unknown);
  EXPECT_EQ(classify("IMG_2048.png").role, Role::Unknown);
  EXPECT_EQ(textures::roleForUsage("ambientOcclusion"), Role::Occlusion);
  EXPECT_EQ(textures::roleForUsage("baseColor"), Role::BaseColor);
  EXPECT_EQ(textures::roleForUsage("wibble"), Role::Unknown);
  EXPECT_EQ(textures::name(Role::Packed), "packed");
}

TEST(TextureSet, DiscoversAndDecodesByRole) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "sigilmaterial_texset";
  fs::create_directories(dir);
  for (const char* name :
       {"tiles_diff_1k.png", "tiles_nor_dx_1k.png", "tiles_arm_1k.png",
        "tiles_rough_1k.png", "other_BaseColor.png", "notes.txt"}) {
    FILE* f = std::fopen((dir / name).string().c_str(), "wb");
    ASSERT_TRUE(f);
    std::fputs("x", f);
    std::fclose(f);
  }
  const std::vector<textures::TextureSet> sets = textures::discover(dir);
  ASSERT_EQ(sets.size(), 2u);
  EXPECT_EQ(sets[0].name, "other");
  EXPECT_EQ(sets[1].name, "tiles");
  const textures::TextureSet& tiles = sets[1];
  EXPECT_TRUE(tiles.normalDirectX);
  EXPECT_EQ(tiles.files.size(), 4u);

  std::map<std::string, sk_sp<SkImage>> decoded;
  const auto decode = [&](const fs::path& p) {
    sk_sp<SkImage>& img = decoded[p.filename().string()];
    if (!img) img = solid(SK_ColorWHITE, 2, 2);
    return img;
  };
  const textures::TextureMaps maps = textures::fromFiles(tiles, decode);
  EXPECT_EQ(maps.name, "tiles");
  EXPECT_TRUE(maps.normalDirectX);
  ASSERT_NE(maps.map(textures::Role::BaseColor), nullptr);
  EXPECT_EQ(maps.map(textures::Role::BaseColor)->image().get(),
            decoded["tiles_diff_1k.png"].get());
  // A scanned material is meant to repeat.
  EXPECT_EQ(maps.map(textures::Role::BaseColor)->tileX(), SkTileMode::kRepeat);
  EXPECT_EQ(maps.map(textures::Role::Packed)->image().get(),
            decoded["tiles_arm_1k.png"].get());
  EXPECT_EQ(maps.map(textures::Role::Emissive), nullptr);
  fs::remove_all(dir);

  // The usage door: the first word naming a role wins, in key order.
  const sk_sp<SkImage> a = solid(SK_ColorWHITE, 2, 2);
  const sk_sp<SkImage> b = solid(SK_ColorWHITE, 2, 2);
  const textures::TextureMaps u = textures::fromUsageMap(
      {{"diffuse", a}, {"baseColor", b}, {"normal", b}, {"height", a}});
  EXPECT_TRUE(u.normalDirectX);
  EXPECT_EQ(u.map(textures::Role::BaseColor)->image().get(), b.get());
  EXPECT_EQ(u.map(textures::Role::Normal)->image().get(), b.get());
  EXPECT_EQ(u.map(textures::Role::Height)->image().get(), a.get());
}

TEST(Surface, EnvironmentRoughnessBlursAndCaches) {
  const Environment env = Environment::sunset(128);
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

TEST(Surface, BevelNormalsFlatInteriorTiltedRim) {
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

TEST(Atlas, GridCutsEqualCellsRowMajor) {
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

TEST(Atlas, ReadsAsepriteTags) {
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
