// A tiled field in chunks: a chunk memoised on the tile ids it holds
// re-records only where an id changed, and the chunks beside it keep the
// recordings they already had.

#include <string>
#include <vector>

#include "support/ShapeTestSupport.h"

namespace {

/** 4-tile atlas, 8px cells: [red | green] / [blue | yellow]. */
std::shared_ptr<sigil::image::ImageAsset> fourTileAtlas() {
  SkBitmap src;
  src.allocN32Pixels(16, 16);
  src.erase(SK_ColorRED, SkIRect::MakeXYWH(0, 0, 8, 8));
  src.erase(SK_ColorGREEN, SkIRect::MakeXYWH(8, 0, 8, 8));
  src.erase(SK_ColorBLUE, SkIRect::MakeXYWH(0, 8, 8, 8));
  src.erase(SK_ColorYELLOW, SkIRect::MakeXYWH(8, 8, 8, 8));
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(src.asImage()));
}

struct ChunkProps {
  std::vector<int> tiles;  // 4x4 tile ids
  int chunkX = 0, chunkY = 0;
  bool operator==(const ChunkProps&) const = default;
};

constexpr float kTilePx = 12.0f;

Element tileChunk(const ChunkProps& p) {
  static auto atlas = fourTileAtlas();
  auto chunk = box().width(4 * kTilePx).height(4 * kTilePx);
  for (int i = 0; i < (int)p.tiles.size(); ++i) {
    const int id = p.tiles[(size_t)i];
    const int atlasRow = id / 2, row = i / 4;
    const float sx = (float)(id % 2) * 8, sy = (float)atlasRow * 8;
    chunk.children(
        {image(atlas)
             .imageRegion(SkRect::MakeXYWH(sx, sy, 8, 8))
             .absolute()
             .inset((float)(i % 4) * kTilePx, (float)row * kTilePx, 0, 0)
             .width(kTilePx)
             .height(kTilePx)});
  }
  return chunk;
}

}  // namespace

TEST(ComposeTiling, OnlyTouchedChunkRerecords) {
  Host host;
  // 2x2 chunks of 4x4 tiles; a checker-ish rule fills the ids.
  std::vector<ChunkProps> chunks(4);
  for (int c = 0; c < 4; ++c) {
    chunks[(size_t)c].chunkX = c % 2;
    chunks[(size_t)c].chunkY = c / 2;
    for (int i = 0; i < 16; ++i) chunks[(size_t)c].tiles.push_back((i + c) % 4);
  }
  auto maze = [&] {
    auto grid = box().row().flexWrap().width(2 * 4 * kTilePx);
    for (int c = 0; c < 4; ++c)
      grid.children({memo(chunks[(size_t)c], tileChunk)
                         .key("chunk" + std::to_string(c))});
    return box().children({grid});
  };

  host.composer.render(maze());
  host.frame();
  const size_t coldRecords = host.composer.stats().picturesRecorded;
  EXPECT_GE(coldRecords, 4u);  // every chunk baked (plus ancestors)

  // Pixel sanity: chunk 0 tile 0 is id 0 (red); chunk 1 tile 0 is id 1
  // (green) at x = 48.
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);

  host.composer.render(maze());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);  // all memo-warm

  chunks[0].tiles[0] = 3;  // mutate ONE tile in ONE chunk
  host.composer.render(maze());
  host.frame();
  // Only chunk 0 and its ancestor chain re-record; the other three
  // chunks' pictures replay untouched.
  EXPECT_LE(host.composer.stats().picturesRecorded, 3u);
  EXPECT_GE(host.composer.stats().picturesRecorded, 1u);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorYELLOW);  // the mutated tile
  EXPECT_EQ(host.pixel(53, 5), SK_ColorGREEN);  // neighbors intact
}
