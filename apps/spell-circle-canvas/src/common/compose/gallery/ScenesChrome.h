#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include "GalleryCore.h"

#include <include/core/SkPathBuilder.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

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

  static Element chunkElement(const Chunk &chunk) {
    // 8x5 tiles per chunk, "atlas index" from a hash of position+rev.
    auto rows = box().column();
    for (int y = 0; y < 5; ++y) {
      auto row = box().row();
      for (int x = 0; x < 8; ++x) {
        const uint32_t h = (uint32_t)(x * 73856093 ^ y * 19349663 ^
                                      chunk.index * 83492791 ^
                                      chunk.revision * 2654435761u);
        const float v = 0.08f + 0.10f * (float)(h % 5);
        row.child(box().width(27).height(27).margin(0.5f)
                      .fill(Fill::color({v, v * 0.9f, v * 1.4f, 1})));
      }
      rows.child(std::move(row));
    }
    return rows;
  }

  Element describe() {
    auto grid = box().row();
    for (int i = 0; i < 4; ++i)
      grid.child(memo(Chunk{i, revisions[(size_t)i]}, chunkElement)
                     .key("chunk" + std::to_string(i)));
    return box().padding(40).fill(Fill::color({0.03f, 0.03f, 0.07f, 1}))
        .child(text(u8"tile map — one chunk re-records per mutation",
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
