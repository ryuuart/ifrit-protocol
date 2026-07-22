#pragma once
// The botanical scene: procedural branches, stamped leaves, randomized
// flower fields.

#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <random>

namespace compose_gallery {

// The generic RPG HUD that used to live here was replaced by
// ScenesVeloren.h, which takes its dimensions and palette out of a
// real game's source instead of inventing them.

struct BotanicalScene final : Scene {
  uint32_t seed = 7;
  double nextReseed = 0.0;
  choreograph::Output<float> sway{0.0f};

  const char *name() const override { return "botanical"; }

  static void drawBranch(SkCanvas &c, std::mt19937 &rng, SkPoint base,
                         float angle, float length, int depth) {
    if (depth == 0 || length < 8)
      return;
    const SkPoint tip = {base.x() + std::cos(angle) * length,
                         base.y() + std::sin(angle) * length};
    SkPaint p;
    p.setAntiAlias(true);
    p.setStrokeCap(SkPaint::kRound_Cap);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth((float)depth * 1.6f);
    p.setColor(SkColorSetARGB(0xff, 0x6a, 0x4a + depth * 8, 0x38));
    c.drawLine(base.x(), base.y(), tip.x(), tip.y(), p);

    // Leaves along this segment.
    SkPaint leaf;
    leaf.setAntiAlias(true);
    for (int i = 1; i < 3; ++i) {
      const float t = (float)i / 3.0f;
      const SkPoint at = {base.x() + (tip.x() - base.x()) * t,
                          base.y() + (tip.y() - base.y()) * t};
      leaf.setColor(SkColorSetARGB(
          0xdd, 0x4a, (uint8_t)(0x9a + rng() % 60), 0x58));
      c.save();
      c.translate(at.x(), at.y());
      c.rotate((float)(rng() % 360));
      c.drawOval(SkRect::MakeXYWH(0, -3, 14, 6), leaf);
      c.restore();
    }

    if (depth == 1) { // flower at the tip
      const float hue = (float)(rng() % 3);
      SkPaint petal;
      petal.setAntiAlias(true);
      petal.setColor(hue < 1 ? 0xffff8ab5 : hue < 2 ? 0xffb18cff
                                                    : 0xffffb46b);
      c.save();
      c.translate(tip.x(), tip.y());
      for (int i = 0; i < 6; ++i) {
        c.rotate(60);
        c.drawOval(SkRect::MakeXYWH(3, -3.5f, 12, 7), petal);
      }
      SkPaint eye;
      eye.setAntiAlias(true);
      eye.setColor(0xfffff2c0);
      c.drawCircle(0, 0, 4.5f, eye);
      c.restore();
      return;
    }
    const int splits = 2 + (int)(rng() % 2);
    for (int i = 0; i < splits; ++i) {
      const float spread =
          ((float)(rng() % 1000) / 1000.0f - 0.5f) * 1.3f;
      drawBranch(c, rng, tip, angle + spread, length * 0.72f, depth - 1);
    }
  }

  Element describe() {
    const uint32_t currentSeed = seed;
    // Randomized flower-field pattern: describe-phase generation.
    std::mt19937 fieldRng{currentSeed * 977u};
    auto field = stack().inset(0);
    for (int i = 0; i < 26; ++i) {
      const float x = (float)(fieldRng() % 860);
      const float y = 430.0f + (float)(fieldRng() % 170);
      const float s = 8.0f + (float)(fieldRng() % 12);
      const uint32_t hue = fieldRng() % 3;
      field.child(
          custom([s, hue](SkCanvas &c, const PaintContext &) {
            SkPaint petal;
            petal.setAntiAlias(true);
            petal.setColor(hue == 0   ? 0xffff8ab5
                           : hue == 1 ? 0xff9adcf0
                                      : 0xffffd9a0);
            for (int k = 0; k < 5; ++k) {
              c.save();
              c.translate(s, s);
              c.rotate(72.0f * (float)k);
              c.drawOval(SkRect::MakeXYWH(s * 0.2f, -s * 0.22f, s * 0.9f,
                                          s * 0.44f), petal);
              c.restore();
            }
            SkPaint eye;
            eye.setAntiAlias(true);
            eye.setColor(0xff6a4a38);
            c.drawCircle(s, s, s * 0.28f, eye);
          })
              .width(2 * s).height(2 * s)
              .inset(x, y, 0, 0).absolute()
              .rotate(&sway).transformOrigin(0.5f, 1.0f));
    }

    return stack()
        .fill(sigil::compose::util::linearGradient(
            {0, 0}, {0, 640},
            {{0.13f, 0.09f, 0.26f, 1},
             {0.72f, 0.36f, 0.34f, 1},
             {0.98f, 0.75f, 0.45f, 1}},
            {0.0f, 0.72f, 1.0f}))
        // Ground
        .child(custom([](SkCanvas &c, const PaintContext &ctx) {
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setColor(0xff23301f);
                 SkPathBuilder b;
                 b.moveTo(0, ctx.size.height() * 0.72f);
                 b.quadTo(ctx.size.width() * 0.5f,
                          ctx.size.height() * 0.62f, ctx.size.width(),
                          ctx.size.height() * 0.75f);
                 b.lineTo(ctx.size.width(), ctx.size.height());
                 b.lineTo(0, ctx.size.height());
                 b.close();
                 c.drawPath(b.detach(), p);
               })
                   .inset(0))
        // Two procedural trees (seeded — reseed regrows them).
        .child(custom([currentSeed](SkCanvas &c, const PaintContext &) {
                 std::mt19937 rng{currentSeed};
                 drawBranch(c, rng, {180, 470}, -1.57f, 92, 5);
                 std::mt19937 rng2{currentSeed * 31u};
                 drawBranch(c, rng2, {680, 490}, -1.72f, 78, 5);
               })
                   .inset(0))
        .child(std::move(field))
        .child(text(u8"seeded regrowth every few seconds",
                    styleAt(15, 0xfff6e7d8))
                   .absolute().inset(24, 20, 24, 590));
  }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    seed = 7;
    nextReseed = 0.0;
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      sway = (float)std::sin(t * 1.4) * 6.0f;
      return true;
    });
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextReseed)
      return;
    nextReseed = elapsed + 3.0;
    seed = seed * 1664525u + 1013904223u;
    composer.render(describe());
  }
};

} // namespace compose_gallery
