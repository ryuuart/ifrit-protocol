#pragma once
// Scale scene: "UI as particles" — the atlas sprites are REAL UI
// rendered once by IfritCompose itself, and not just pills: the mix
// includes ornate posts wearing different chrome (spiky outline() shout,
// scalloped seal, carved nine-slice frame, dashed field note), each
// parameterized by color. Then instanced by the thousand through one
// drawAtlas call over an EnTT SoA registry.

#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <entt/entt.hpp>

#include <include/core/SkRSXform.h>

#include <cmath>
#include <random>

namespace compose_gallery {

struct UiParticleScene final : Scene {
  static constexpr size_t kCount = 1500;
  static constexpr float kSprite = 64.0f; // atlas cell size
  static constexpr int kSpriteCount = 8;
  entt::registry registry;
  sk_sp<SkImage> atlas;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };
  struct Look { uint8_t sprite; float scale; float spin; };

  const char *name() const override { return "ui_particles"; }

  /** Eight UI sprites described with the ordinary element API and
   *  rendered into the atlas by a throwaway Composer: four pill chips
   *  and four ornate posts, every one color-parameterized. */
  void buildAtlas() {
    ifrit::tick::Ticker atlasTicker;
    Composer sprites(atlasTicker, fonts());
    sprites.setSize({kSprite * kSpriteCount, kSprite});

    // One 64px atlas cell, content centered.
    auto cell = [&](Element content) {
      return box().width(kSprite).height(kSprite)
          .alignItems(Align::Center).justify(Justify::Center)
          .child(std::move(content));
    };

    auto pill = [&](Element content, SkColor4f fillColor,
                    SkColor4f strokeColor) {
      return cell(box().width(kSprite - 10).height(kSprite - 26)
                      .corners({14})
                      .fill(Fill::color(fillColor))
                      .foreground(ifrit::compose::util::stroke(
                          2, Fill::color(strokeColor)))
                      .alignItems(Align::Center).justify(Justify::Center)
                      .child(std::move(content)));
    };

    const Palette oak = oakPalette(), emerald = emeraldPalette();

    // Post 1: spiky outline() shout.
    auto shout = cell(
        box().width(kSprite - 4).height(kSprite - 4)
            .outline(starburstOutline(10, 0.32f))
            .fill(ifrit::compose::util::radialGradient(
                {kSprite / 2 - 2, kSprite / 2 - 2}, kSprite / 2,
                {{1.0f, 0.88f, 0.40f, 1}, {0.88f, 0.26f, 0.10f, 1}}))
            .foreground(ifrit::compose::util::stroke(
                2, Fill::color({0.45f, 0.07f, 0.05f, 1})))
            .alignItems(Align::Center).justify(Justify::Center)
            .child(text(u8"POW", styleAt(14, 0xff471003))));

    // Post 2: scalloped wax-seal silhouette, emerald parchment.
    auto seal = cell(
        box().width(kSprite - 8).height(kSprite - 8)
            .outline(scallopOutline(9))
            .fill(parchmentFill(emerald.parchment, 0.12f))
            .foreground(ifrit::compose::util::stroke(
                2, Fill::color(emerald.stem)))
            .alignItems(Align::Center).justify(Justify::Center)
            .child(text(u8"act I", styleAt(13, 0xff10361f))));

    // Post 3: the carved nine-slice frame, generated on an
    // intermediate canvas at 48px and sliced into this cell.
    auto framed = cell(
        box().width(kSprite - 8).height(kSprite - 12)
            .background(carvedFrameSlice(
                std::make_shared<ifrit::image::ImageAsset>(
                    ifrit::image::ImageAsset::wrap(
                        makeCarvedFrame(oak, 48)))))
            .alignItems(Align::Center).justify(Justify::Center)
            .child(text(u8"+1", styleAt(16, 0xff2b1c0b))));

    // Post 4: dashed field note, two lines.
    auto note = [&] {
      PathFormat dashed;
      dashed.width = 1.6f;
      dashed.strokeFill = Fill::color({0.30f, 0.56f, 0.95f, 1});
      dashed.dashIntervals = {5, 4};
      return cell(box().width(kSprite - 10).height(kSprite - 18)
                      .corners({8})
                      .fill(Fill::color({0.90f, 0.94f, 1.0f, 1}))
                      .foreground(dashed)
                      .column().gap(2).padding(6)
                      .child(text(u8"run!", styleAt(12, 0xff14243a)))
                      .child(text(u8"north", styleAt(10, 0xff3a4a63))));
    }();

    sprites.render(
        box().row()
            .child(pill(text(u8"+250", styleAt(15, 0xff2b1e08)),
                        {0.98f, 0.78f, 0.30f, 1},
                        {0.66f, 0.46f, 0.10f, 1}))
            .child(pill(text(u8"♥", styleAt(20, 0xffffffff)),
                        {0.90f, 0.28f, 0.42f, 1},
                        {0.55f, 0.10f, 0.22f, 1}))
            .child(pill(text(u8"xp", styleAt(15, 0xff08303a)),
                        {0.49f, 0.91f, 1.0f, 1},
                        {0.10f, 0.45f, 0.55f, 1}))
            .child(pill(text(u8"-87", styleAt(15, 0xffffffff)),
                        {0.42f, 0.24f, 0.62f, 1},
                        {0.72f, 0.55f, 0.95f, 1}))
            .child(std::move(shout))
            .child(std::move(seal))
            .child(std::move(framed))
            .child(std::move(note)));

    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)(kSprite * kSpriteCount), (int)kSprite));
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
      registry.emplace<Look>(e, (uint8_t)(rng() % kSpriteCount),
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
            .child(text(u8"1,500 compose-rendered UI sprites — pills and "
                        u8"ornate posts (spiky, scalloped, nine-sliced, "
                        u8"dashed) from one atlas, one drawAtlas, EnTT SoA",
                        styleAt(16, 0xffdde4f2))
                       .absolute().inset(24, 24, 24, 590).zIndex(1)));
  }
};

} // namespace compose_gallery
