#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include "GalleryCore.h"

#include <cmath>

namespace compose_gallery {

// ---- 2: choreographed headline (bindings + live custom leaf) -------------

struct HeadlineScene final : Scene {
  choreograph::Output<float> drop{-40.0f}, fade{0.0f}, spin{0.0f};

  const char *name() const override { return "headline"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    drop = -40.0f;
    fade = 0.0f;
    ticker.timeline().apply(&drop).then<choreograph::RampTo>(
        0.0f, 0.9f, &choreograph::easeOutQuint);
    ticker.timeline().apply(&fade).then<choreograph::RampTo>(1.0f, 0.7f);
    ticker.add([this](double dt) {
      spin = spin.value() + (float)(dt * 40.0);
      return true;
    });

    composer.render(
        box().column().padding(56).gap(24)
            .fill(Fill::color({0.05f, 0.08f, 0.14f, 1}))
            .child(text(u8"CHOREOGRAPH", styleAt(84, 0xff7ee8ff))
                       .key("headline")
                       .translateY(&drop).opacity(&fade))
            .child(text(u8"outputs bound as style values",
                        styleAt(22, 0xff9aa4bb)).opacity(&fade))
            .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                     SkPaint ring;
                     ring.setAntiAlias(true);
                     ring.setStyle(SkPaint::kStroke_Style);
                     ring.setStrokeWidth(3);
                     const float cx = ctx.size.width() / 2;
                     const float cy = ctx.size.height() / 2;
                     for (int i = 0; i < 6; ++i) {
                       const float a = spin.value() * 3.14159f / 180.0f +
                                       (float)i * 3.14159f / 3.0f;
                       ring.setColor(i % 2 ? 0x88b18cff : 0xcc7ee8ff);
                       c.drawCircle(cx + std::cos(a) * 90,
                                    cy + std::sin(a) * 90, 34, ring);
                     }
                   })
                       .height(280)
                       .cache(Cache::None)));
  }
};

// ---- 3: blend stack ------------------------------------------------------

struct BlendScene final : Scene {
  choreograph::Output<float> slide{0.0f};

  const char *name() const override { return "blend"; }

  static Element table(const char *title, SkColor4f base) {
    auto t = box().column().gap(6).padding(18).width(380).corners({12})
                 .fill(Fill::color(base));
    t.child(text(toU8(title), styleAt(22, 0xffffffff)));
    for (int i = 0; i < 4; ++i)
      t.child(box().row().gap(10).padding(8).corners({6})
                  .fill(Fill::color({0, 0, 0, 0.35f}))
                  .child(text(toU8("row " + std::to_string(i)),
                              styleAt(16, 0xffdde4f2)).grow(1))
                  .child(text(toU8(std::to_string(37 * (i + 1))),
                              styleAt(16, 0xff7ee8ff))));
    return t;
  }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      slide = (float)(std::sin(t * 0.8) * 140.0);
      return true;
    });
    composer.render(
        stack().fill(Fill::color({0.08f, 0.05f, 0.13f, 1}))
            .child(table("ALPHA", {0.16f, 0.10f, 0.28f, 1})
                       .inset(70, 90, 450, 130).zIndex(0))
            .child(table("BETA", {0.10f, 0.20f, 0.30f, 1})
                       .inset(300, 170, 220, 50).zIndex(1)
                       .translateX(&slide)
                       .opacity(0.85f)
                       .blend(SkBlendMode::kScreen)));
  }
};

// ---- 7: reflow + connector (derive phase, live) --------------------------

struct DeriveScene final : Scene {
  int slotIndex = 0;
  double nextMove = 0.0;

  const char *name() const override { return "derive"; }

  Element describe() {
    const float lefts[3] = {520, 300, 640};
    const float tops[3] = {80, 240, 380};
    PathFormat wire;
    wire.width = 3;
    wire.strokeFill = Fill::color({1.0f, 0.71f, 0.42f, 1});
    wire.dashIntervals = {8, 6};

    return stack().fill(Fill::color({0.06f, 0.07f, 0.12f, 1}))
        .child(box().inset(40, 40, 40, 40)
                   .child(text(
                       u8"the quick brown fox jumps over the lazy dog and "
                       u8"keeps running through the tall summer grass until "
                       u8"the river bend appears and the evening light "
                       u8"settles over the water in long amber bands while "
                       u8"the reeds keep time against the slow current and "
                       u8"the ferry rope hums under a traveler's hand",
                       styleAt(26, 0xffdde4f2))
                            .key("body")
                            .flowAround("float", 14))
                   .zIndex(1))
        .child(box().key("anchor").width(16).height(16)
                   .inset(60, 560, 824, 64).absolute()
                   .corners({8}).fill(Fill::color({0.49f, 0.91f, 1, 1})))
        .child(box().key("float").width(200).height(150)
                   .inset(lefts[slotIndex], tops[slotIndex],
                          kSceneSize.width() - lefts[slotIndex] - 200,
                          kSceneSize.height() - tops[slotIndex] - 150)
                   .absolute().corners({14})
                   .fill(Fill::color({0.20f, 0.13f, 0.33f, 1}))
                   .child(text(u8"float", styleAt(18, 0xff9aa4bb))
                              .absolute().inset(12, 12, 12, 110)))
        .child(connector("anchor", "float").inset(0).foreground(wire));
  }

  void setup(Composer &composer, sigil::motion::Ticker &) override {
    slotIndex = 0;
    nextMove = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextMove)
      return;
    nextMove = elapsed + 1.6;
    slotIndex = (slotIndex + 1) % 3;
    composer.render(describe());
  }
};


} // namespace compose_gallery
