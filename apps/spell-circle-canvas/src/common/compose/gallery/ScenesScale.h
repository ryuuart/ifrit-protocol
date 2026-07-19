#pragma once
// Scale scene: "UI as particles" — the atlas sprites are REAL UI chips
// rendered once by IfritCompose itself (coin pill, heart badge, XP gem,
// damage number), then instanced 20,000 times through one drawAtlas
// call over an EnTT SoA registry. Millions-scale composition of actual
// interface elements, not dots.

#include "GalleryCore.h"

#include <entt/entt.hpp>

#include <include/core/SkRSXform.h>

#include <cmath>
#include <random>

namespace compose_gallery {

struct UiParticleScene final : Scene {
  static constexpr size_t kCount = 3000;
  static constexpr float kSprite = 48.0f; // atlas cell size
  entt::registry registry;
  sk_sp<SkImage> atlas;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };
  struct Look { uint8_t sprite; float scale; float spin; };

  const char *name() const override { return "ui_particles"; }

  /** The four UI chips, described with the ordinary element API and
   *  rendered into the atlas by a throwaway Composer: compose builds
   *  the sprites, EnTT + drawAtlas instance them. */
  void buildAtlas() {
    ifrit::tick::Ticker atlasTicker;
    Composer sprites(atlasTicker, fonts());
    sprites.setSize({kSprite * 4, kSprite});

    auto chip = [&](Element content, SkColor4f fillColor,
                    SkColor4f strokeColor) {
      return box().width(kSprite - 8).height(kSprite - 20).corners({12})
          .margin(4)
          .fill(Fill::color(fillColor))
          .foreground(ifrit::compose::util::stroke(
              2, Fill::color(strokeColor)))
          .alignItems(Align::Center).justify(Justify::Center)
          .child(std::move(content));
    };

    sprites.render(
        box().row()
            .child(chip(text(u8"+250", styleAt(15, 0xff2b1e08)),
                        {0.98f, 0.78f, 0.30f, 1},
                        {0.66f, 0.46f, 0.10f, 1}))
            .child(chip(text(u8"♥", styleAt(18, 0xffffffff)),
                        {0.90f, 0.28f, 0.42f, 1},
                        {0.55f, 0.10f, 0.22f, 1}))
            .child(chip(text(u8"xp", styleAt(15, 0xff08303a)),
                        {0.49f, 0.91f, 1.0f, 1},
                        {0.10f, 0.45f, 0.55f, 1}))
            .child(chip(text(u8"-87", styleAt(15, 0xffffffff)),
                        {0.42f, 0.24f, 0.62f, 1},
                        {0.72f, 0.55f, 0.95f, 1})));

    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)(kSprite * 4), (int)kSprite));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    sprites.draw(*surface->getCanvas());
    atlas = surface->makeImageSnapshot();
  }

  void setup(Composer &composer, ifrit::tick::Ticker &ticker) override {
    buildAtlas();
    registry.clear();
    std::mt19937 rng{11};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < kCount; ++i) {
      entt::entity e = registry.create();
      registry.emplace<Pos>(e, unit() * kSceneSize.width(),
                            unit() * kSceneSize.height());
      registry.emplace<Vel>(e, unit() * 60 - 30, -20.0f - unit() * 70);
      registry.emplace<Look>(e, (uint8_t)(rng() % 4),
                             0.35f + unit() * 0.55f,
                             unit() * 0.4f - 0.2f);
    }
    xforms.reserve(kCount);
    texRects.reserve(kCount);

    ticker.add([this](double dt) {
      registry.view<Pos, const Vel>().each([dt](Pos &p, const Vel &v) {
        p.x += v.dx * (float)dt;
        p.y += v.dy * (float)dt;
        if (p.y < -kSprite)
          p.y += kSceneSize.height() + kSprite; // loot fountain loops
        if (p.x < -kSprite) p.x += kSceneSize.width() + kSprite;
        else if (p.x > kSceneSize.width()) p.x -= kSceneSize.width() + kSprite;
      });
      return true;
    });

    composer.render(
        stack()
            .fill(ifrit::compose::util::linearGradient(
                {0, 0}, {0, 640},
                {{0.05f, 0.04f, 0.12f, 1}, {0.12f, 0.05f, 0.14f, 1}}))
            .child(custom([this](SkCanvas &c, const PaintContext &ctx) {
                     xforms.clear();
                     texRects.clear();
                     const double t = ctx.elapsedSeconds;
                     registry.view<const Pos, const Look>().each(
                         [&](const Pos &p, const Look &l) {
                           const float a =
                               l.spin * (float)std::sin(t * 2.0 + p.x);
                           xforms.push_back(SkRSXform::MakeFromRadians(
                               l.scale, a, p.x, p.y, kSprite / 2,
                               kSprite / 2));
                           texRects.push_back(SkRect::MakeXYWH(
                               kSprite * (float)l.sprite, 0, kSprite,
                               kSprite));
                         });
                     c.drawAtlas(atlas.get(),
                                 SkSpan(xforms.data(), xforms.size()),
                                 SkSpan(texRects.data(), texRects.size()),
                                 {}, SkBlendMode::kSrcOver,
                                 SkSamplingOptions(SkFilterMode::kLinear),
                                 nullptr, nullptr);
                   })
                       .inset(0)
                       .cache(Cache::None))
            .child(text(u8"3,000 compose-rendered UI chips on CPU raster (~5 us each) — "
                        u8"one atlas, one drawAtlas, EnTT SoA; the GPU path carries 100k+",
                        styleAt(17, 0xffdde4f2))
                       .absolute().inset(24, 24, 24, 590).zIndex(1)));
  }
};

} // namespace compose_gallery
