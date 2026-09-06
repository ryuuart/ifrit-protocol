/** @file
 * The atlas: a grid cuts equal cells row-major and names them by index,
 * both sprite tools' JSON is read into regions and sequences, and loose
 * images pack without overlap and keep their pixels.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/texture/Atlas.h>
#include <sigilmaterial/texture/Texture.h>

#include <optional>
#include <string>
#include <vector>

#include "support/Shade.h"

using namespace sigil::material;
using sigil::material::test::solid;

namespace {

/** One pixel of a shader, painted over a surface big enough to hold it. */
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

TEST(Atlas, ATagNamingNoFrameOfTheSheetIsNotASequence) {
  // A tag range that does not meet the frames — a sheet re-exported
  // shorter, a hand-edited JSON — used to register an EMPTY sequence
  // under a name a caller can ask for, and counted as a tag, so the
  // fallback that walks every frame was skipped too: nothing to play
  // under either name.
  const char* json = R"({"frames": [
    {"filename": "hero 0.aseprite", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}},
    {"filename": "hero 1.aseprite", "frame": {"x": 8, "y": 0, "w": 8, "h": 8}}
  ], "meta": {"app": "Aseprite", "frameTags": [
    {"name": "gone", "from": 7, "to": 9, "direction": "forward"},
    {"name": "negative", "from": -4, "to": -1, "direction": "forward"}]}})";
  const std::optional<Atlas> atlas =
      Atlas::fromAseprite(Texture::of(solid(SK_ColorRED, 16, 8)), json);
  ASSERT_TRUE(atlas);
  EXPECT_EQ(atlas->sequence("gone"), nullptr);
  EXPECT_EQ(atlas->sequence("negative"), nullptr);
  ASSERT_NE(atlas->sequence("all"), nullptr);
  EXPECT_EQ(atlas->sequence("all")->size(), 2u);

  // A range that only overhangs is clamped onto the frames it does
  // reach, rather than thrown away with them.
  const char* overhang = R"({"frames": [
    {"filename": "hero 0.aseprite", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}},
    {"filename": "hero 1.aseprite", "frame": {"x": 8, "y": 0, "w": 8, "h": 8}}
  ], "meta": {"app": "Aseprite", "frameTags": [
    {"name": "run", "from": -1, "to": 5, "direction": "forward"}]}})";
  const std::optional<Atlas> clamped =
      Atlas::fromAseprite(Texture::of(solid(SK_ColorRED, 16, 8)), overhang);
  ASSERT_TRUE(clamped);
  ASSERT_NE(clamped->sequence("run"), nullptr);
  EXPECT_EQ(*clamped->sequence("run"), (std::vector<size_t>{0, 1}));
  EXPECT_EQ(clamped->sequence("all"), nullptr);
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
