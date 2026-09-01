/** @file
 * tile map — a maze from a generated four-cell atlas, drawn as chunks:
 * one chunk re-records per mutation and the rest replay, so what a memo
 * is worth is visible in the frame.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkStream.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {

// ---- 11: tile map with chunked caching (#15) ------------------------------

/** The side every 16 px atlas cell is drawn at. It is a SQUARE only while
 *  the row of chunks gets the width it asks for. */
constexpr float kTile = 27.0f;
constexpr int kChunkCols = 8, kChunkRows = 5, kChunks = 4;
constexpr float kPad = 40.0f;
constexpr float kCaptionH = 26.0f;  // the caption's own row, one 18 px line
constexpr float kCaptionGap = 16.0f;

/** The canvas IS the content: four chunks wide and one chunk plus its
 *  caption tall. A narrower box would shrink the chunks — a row lays its
 *  children out in the space it has — and the atlas cells would stop being
 *  square; a taller one would photograph empty ground. */
constexpr float kCanvasW = kChunks * kChunkCols * kTile + 2 * kPad;
constexpr float kCanvasH =
    kChunkRows * kTile + kCaptionH + kCaptionGap + 2 * kPad;

struct TileMap final : sketch::Sketch {
  struct Chunk {
    int index;
    int revision;
    bool operator==(const Chunk&) const = default;
  };
  std::vector<int> revisions = std::vector<int>(kChunks, 0);
  double nextMutation = 0.0;

  /** The maze tileset: a procedural 4-cell atlas (16px tiles: floor,
   *  brick wall, moss floor, ember) built once — the stress-item-15
   *  shape: image(atlas).region(cell) selects per tile. */
  static std::shared_ptr<sigil::image::ImageAsset> atlas() {
    static std::shared_ptr<sigil::image::ImageAsset> asset = [] {
      sk_sp<SkSurface> s =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
      SkCanvas& c = *s->getCanvas();
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

  static Element chunkElement(const Chunk& chunk) {
    // 8x5 tiles per chunk; a seeded rule picks the atlas region — a
    // maze-ish wall pattern with moss and embers scattered in.
    auto tiles = box().width(kChunkCols * kTile).height(kChunkRows * kTile);
    for (int y = 0; y < kChunkRows; ++y)
      for (int x = 0; x < kChunkCols; ++x) {
        const uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^
                           (uint32_t)chunk.index * 83492791u ^
                           chunk.revision * 2654435761u;
        int id = 0;  // floor
        if (x % 3 == 1 && (h & 5u) != 0)
          id = 1;  // maze walls in broken columns
        else if ((h % 11) == 3)
          id = 2;  // moss
        else if ((h % 23) == 7)
          id = 3;  // ember
        tiles.child(image(atlas())
                        .region(SkRect::MakeXYWH((float)id * 16, 0, 16, 16))
                        .inset((float)x * kTile, (float)y * kTile, 0, 0)
                        .width(kTile)
                        .height(kTile));
      }
    return tiles;
  }

  Element describe() {
    auto grid = box().row().width(kChunks * kChunkCols * kTile);
    for (int i = 0; i < kChunks; ++i)
      grid.child(memo(Chunk{i, revisions[(size_t)i]}, chunkElement)
                     .key("chunk" + std::to_string(i)));
    return box()
        .padding(kPad)
        .fill(Fill::color({0.03f, 0.03f, 0.07f, 1}))
        .child(box()
                   .height(kCaptionH)
                   .row()
                   .alignItems(Align::Center)
                   .child(text(u8"tile maze — atlas regions; one chunk "
                               u8"re-records per mutation",
                               type({.size = 18, .color = hex(0x9aa4bb)}))))
        .child(box().height(kCaptionGap))
        .child(std::move(grid));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    revisions.assign(kChunks, 0);
    nextMutation = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    if (elapsed < nextMutation) return;
    nextMutation = elapsed + 0.7;
    revisions[(size_t)(elapsed * 13.0) % kChunks]++;
    composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(TileMap, "tile map", "Catalog \xc2\xb7 Tiling", "#15")
