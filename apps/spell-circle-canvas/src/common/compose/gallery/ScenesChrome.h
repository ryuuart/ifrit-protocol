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

// ---- 4: chrome (decoration primitives) -----------------------------------

struct ChromeScene final : Scene {
  const char *name() const override { return "chrome"; }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    SkPathBuilder leaf;
    leaf.moveTo(0, 0);
    leaf.quadTo(6, -7, 14, 0);
    leaf.quadTo(6, 7, 0, 0);

    PathFormat stamped;
    stamped.width = 2;
    stamped.strokeFill = Fill::color({0.69f, 0.55f, 1.0f, 1});
    stamped.stampPath = leaf.detach();
    stamped.stampAdvance = 20;

    PathFormat dashed;
    dashed.width = 4;
    dashed.strokeFill = Fill::color({0.49f, 0.91f, 1.0f, 1});
    dashed.dashIntervals = {14, 8};

    ContourWalk orbit;
    orbit.spacing = 26;
    orbit.animatedWalk = true; // dots breathe with the clock
    orbit.draw = [](SkCanvas &c, const PathSample &s,
                    const PaintContext &ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setColor(SkColorSetARGB(0xff, 0xff, 0xb4, 0x6b));
      const float wave =
          std::sin(s.fraction * 6.28318f + (float)ctx.elapsedSeconds * 2);
      c.drawCircle(0, -10, 2.0f + 1.8f * wave, p);
    };

    composer.render(
        stack().fill(Fill::color({0.10f, 0.06f, 0.16f, 1}))
            .child(box().inset(60, 70, 60, 70).corners({24})
                       .fill(Fill::color({0.07f, 0.08f, 0.14f, 0.92f}))
                       .foreground(stamped)
                       .foreground(orbit)
                       .child(box().column().padding(40).gap(14)
                                  .child(text(u8"CHROME",
                                              styleAt(56, 0xffe8ecf8)))
                                  .child(text(u8"stamped leaves, breathing walk dots, dash data",
                                              styleAt(17, 0xff9aa4bb)))
                                  .child(box().height(30)
                                             .foreground(dashed)))));
  }
};

// ---- 10: procedural SkSL border (#11) -------------------------------------

struct SkslBorderScene final : Scene {
  sk_sp<SkRuntimeEffect> effect;

  const char *name() const override { return "sksl_border"; }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    auto result = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float t;
        half4 main(float2 p) {
          float wave = 0.5 + 0.5 * sin(p.x * 0.05 + p.y * 0.08 + t * 3.0);
          return half4(0.3 + 0.6 * wave, 0.55, 1.0 - 0.5 * wave, 1) *
                 half4(0.9);
        })"));
    effect = result.effect;

    composer.render(
        stack().fill(Fill::color({0.05f, 0.06f, 0.1f, 1}))
            .child(box().inset(90, 100, 90, 100).corners({26})
                       .fill(Fill::color({0.08f, 0.09f, 0.15f, 1}))
                       .foreground(Decoration(PaintProgram(
                           [this](SkCanvas &c, const PaintContext &ctx) {
                             if (!effect)
                               return;
                             SkRuntimeShaderBuilder b(effect);
                             b.uniform("t") =
                                 (float)ctx.elapsedSeconds;
                             SkPaint p;
                             p.setAntiAlias(true);
                             p.setStyle(SkPaint::kStroke_Style);
                             p.setStrokeWidth(10);
                             p.setShader(b.makeShader());
                             c.drawPath(ctx.outline, p);
                           })))
                       .cache(Cache::None) // reads the clock
                       .child(text(u8"SKSL BORDER",
                                   styleAt(52, 0xffe8ecf8))
                                  .absolute().inset(60, 80, 60, 60))));
  }
};

// ---- 5: CRT + bloom (effects) --------------------------------------------

struct CrtScene final : Scene {
  const char *name() const override { return "crt_bloom"; }

  static Element scanlines(float phase, SkColor4f tint) {
    return custom([phase, tint](SkCanvas &c, const PaintContext &ctx) {
             SkPaint p;
             p.setColor4f(tint, nullptr);
             const float shift =
                 phase + (float)std::fmod(ctx.elapsedSeconds * 8.0, 6.0);
             for (float y = shift; y < ctx.size.height(); y += 6.0f)
               c.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), 2.4f), p);
           })
        .absolute().inset(0)
        .cache(Cache::None) // reads the clock: declared volatility
        .blend(SkBlendMode::kPlus);
  }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    composer.render(
        stack().fill(Fill::color({0.02f, 0.03f, 0.05f, 1}))
            .child(box().column().padding(70).gap(10).inset(0).zIndex(2)
                       .child(text(u8"PHOSPHOR", styleAt(96, 0xff9df2ff))))
            .child(box().column().padding(70).inset(0).zIndex(1)
                       .effect(Effect::filter(
                           SkImageFilters::Blur(14, 14, nullptr)))
                       .blend(SkBlendMode::kPlus)
                       .cache(Cache::Texture) // bloom baked once
                       .child(text(u8"PHOSPHOR", styleAt(96, 0xff2a7f96))))
            .child(scanlines(0.0f, {0.10f, 0.22f, 0.16f, 1}).zIndex(0))
            .child(scanlines(2.0f, {0.16f, 0.10f, 0.20f, 1}).zIndex(0))
            .child(scanlines(4.0f, {0.05f, 0.12f, 0.24f, 1}).zIndex(0)));
  }
};

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
