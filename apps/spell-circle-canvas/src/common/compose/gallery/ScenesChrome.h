#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include "GalleryCore.h"

#include <sigilimage/ImageAsset.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkStream.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/encode/SkPngEncoder.h>

#include <cmath>

namespace compose_gallery {

// ---- 11: tile map with chunked caching (#15) ------------------------------

struct TileScene final : Scene {
  struct Chunk {
    int index;
    int revision;
    bool operator==(const Chunk &) const = default;
  };
  std::vector<int> revisions = std::vector<int>(4, 0);
  double nextMutation = 0.0;

  const char *name() const override { return "tilemap"; }

  /** The maze tileset: a procedural 4-cell atlas (16px tiles: floor,
   *  brick wall, moss floor, ember) built once — the stress-item-15
   *  shape: image(atlas).region(cell) selects per tile. */
  static std::shared_ptr<sigil::image::ImageAsset> atlas() {
    static std::shared_ptr<sigil::image::ImageAsset> asset = [] {
      sk_sp<SkSurface> s =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
      SkCanvas &c = *s->getCanvas();
      SkPaint p;
      // 0: floor
      p.setColor(SkColorSetRGB(0x18, 0x16, 0x24));
      c.drawRect(SkRect::MakeXYWH(0, 0, 16, 16), p);
      p.setColor(SkColorSetRGB(0x24, 0x20, 0x34));
      c.drawRect(SkRect::MakeXYWH(2, 2, 2, 2), p);
      c.drawRect(SkRect::MakeXYWH(10, 9, 2, 2), p);
      // 1: brick wall
      p.setColor(SkColorSetRGB(0x5a, 0x33, 0x2c));
      c.drawRect(SkRect::MakeXYWH(16, 0, 16, 16), p);
      p.setColor(SkColorSetRGB(0x3a, 0x1f, 0x1c));
      for (int row = 0; row < 4; ++row)
        c.drawRect(SkRect::MakeXYWH(16, (float)row * 4 + 3, 16, 1), p);
      c.drawRect(SkRect::MakeXYWH(16 + 7, 0, 1, 16), p);
      // 2: moss floor
      p.setColor(SkColorSetRGB(0x1c, 0x2a, 0x1e));
      c.drawRect(SkRect::MakeXYWH(32, 0, 16, 16), p);
      p.setColor(SkColorSetRGB(0x2f, 0x49, 0x2c));
      c.drawRect(SkRect::MakeXYWH(35, 4, 3, 2), p);
      c.drawRect(SkRect::MakeXYWH(42, 10, 4, 3), p);
      // 3: ember
      p.setColor(SkColorSetRGB(0x2a, 0x12, 0x18));
      c.drawRect(SkRect::MakeXYWH(48, 0, 16, 16), p);
      p.setColor(SkColorSetRGB(0xff, 0x7a, 0x33));
      c.drawRect(SkRect::MakeXYWH(54, 6, 4, 4), p);
      p.setColor(SkColorSetRGB(0xff, 0xc4, 0x6b));
      c.drawRect(SkRect::MakeXYWH(55, 7, 2, 2), p);

      SkBitmap bm;
      bm.allocPixels(s->imageInfo());
      s->readPixels(bm.pixmap(), 0, 0);
      SkDynamicMemoryWStream stream;
      SkPngEncoder::Encode(&stream, bm.pixmap(), {});
      return std::make_shared<sigil::image::ImageAsset>(
          *sigil::image::ImageAsset::decode(stream.detachAsData()));
    }();
    return asset;
  }

  static Element chunkElement(const Chunk &chunk) {
    // 8x5 tiles per chunk; a seeded rule picks the atlas region — a
    // maze-ish wall pattern with moss and embers scattered in.
    constexpr float kTile = 27.0f;
    auto tiles = box().width(8 * kTile).height(5 * kTile);
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 8; ++x) {
        const uint32_t h = (uint32_t)(x * 73856093 ^ y * 19349663 ^
                                      chunk.index * 83492791 ^
                                      chunk.revision * 2654435761u);
        int id = 0; // floor
        if (x % 3 == 1 && (h & 5) != 0)
          id = 1; // maze walls in broken columns
        else if ((h % 11) == 3)
          id = 2; // moss
        else if ((h % 23) == 7)
          id = 3; // ember
        tiles.child(image(atlas())
                        .region(SkRect::MakeXYWH((float)id * 16, 0, 16, 16))
                        .absolute()
                        .inset((float)x * kTile, (float)y * kTile, 0, 0)
                        .width(kTile).height(kTile));
      }
    return tiles;
  }

  Element describe() {
    auto grid = box().row();
    for (int i = 0; i < 4; ++i)
      grid.child(memo(Chunk{i, revisions[(size_t)i]}, chunkElement)
                     .key("chunk" + std::to_string(i)));
    return box().padding(40).fill(Fill::color({0.03f, 0.03f, 0.07f, 1}))
        .child(text(u8"tile maze — atlas regions; one chunk re-records "
                    u8"per mutation",
                    styleAt(18, 0xff9aa4bb)))
        .child(box().height(16))
        .child(std::move(grid));
  }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    revisions.assign(4, 0);
    nextMutation = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextMutation)
      return;
    nextMutation = elapsed + 0.7;
    revisions[(size_t)(elapsed * 13.0) % 4]++;
    composer.render(describe());
  }
};


} // namespace compose_gallery
