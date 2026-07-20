#pragma once
// Scale scene: "UI as particles" — and none of them look instanced.
// FIVE parameterized components (pill chip, spiky shout, scalloped
// seal, carved nine-slice post, dashed field note) are instantiated
// THIRTY-TWO ways — every atlas cell gets its own hue and its own
// content — then scattered by the thousand through one drawAtlas call
// over an EnTT SoA registry. Same components, different data.

#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <entt/entt.hpp>

#include <include/core/SkRSXform.h>

#include <cmath>
#include <random>

namespace compose_gallery {

struct UiParticleScene final : Scene {
  static constexpr size_t kCount = 1500;
  static constexpr float kSprite = 64.0f;      // on-screen sprite size
  static constexpr float kAtlasScale = 2.0f;   // oversample: crisp when
  static constexpr float kCell = kSprite * kAtlasScale; // magnified
  static constexpr int kCols = 8, kRows = 4;
  static constexpr int kVariants = kCols * kRows;
  entt::registry registry;
  sk_sp<SkImage> atlas;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };
  struct Look { uint8_t sprite; float scale; float spin; };

  const char *name() const override { return "ui_particles"; }

  /** Pastel/deep color pair from a hue — the palette knob every chip
   *  component takes. */
  struct ChipTheme {
    SkColor4f fill, edge, ink;
  };
  static SkColor4f hsv(float h, float s, float v) {
    const float c = v * s;
    const float x = c * (1 - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    return {r + m, g + m, b + m, 1};
  }
  static ChipTheme chipTheme(float hueDegrees, bool darkInk) {
    return {hsv(hueDegrees, 0.62f, 0.94f),
            hsv(hueDegrees, 0.80f, 0.45f),
            darkInk ? hsv(hueDegrees, 0.85f, 0.22f)
                    : SkColor4f{1, 1, 1, 1}};
  }
  static SkColor ink(const ChipTheme &t) { return t.ink.toSkColor(); }

  /** The five components. Each takes (theme, content) — the whole
   *  point: one component, many skins. */
  Element pill(const ChipTheme &t, std::u8string label) {
    return box().width(kSprite - 10).height(kSprite - 26).corners({14})
        .fill(Fill::color(t.fill))
        .foreground(sigil::compose::util::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(std::move(label), styleAt(15, ink(t))));
  }
  Element shout(const ChipTheme &t, std::u8string label, int spikes) {
    return box().width(kSprite - 4).height(kSprite - 4)
        .outline(starburstOutline(spikes, 0.32f))
        .fill(sigil::compose::util::radialGradient(
            {kSprite / 2 - 2, kSprite / 2 - 2}, kSprite / 2,
            {{1.0f, 0.92f, 0.55f, 1}, t.fill}))
        .foreground(sigil::compose::util::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(std::move(label), styleAt(13, ink(t))));
  }
  Element seal(const ChipTheme &t, std::u8string label, float lobe) {
    return box().width(kSprite - 8).height(kSprite - 8)
        .outline(scallopOutline(lobe))
        .fill(Fill::color(t.fill))
        .foreground(sigil::compose::util::stroke(2, Fill::color(t.edge)))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(std::move(label), styleAt(13, ink(t))));
  }
  Element framed(const Palette &pal, std::u8string label) {
    return box().width(kSprite - 8).height(kSprite - 12)
        .background(carvedFrameSlice(
            std::make_shared<sigil::image::ImageAsset>(
                sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 96)))))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(std::move(label),
                    styleAt(15, pal.ink.toSkColor())));
  }
  Element note(const ChipTheme &t, std::u8string line1,
               std::u8string line2) {
    PathFormat dashed;
    dashed.width = 1.6f;
    dashed.strokeFill = Fill::color(t.edge);
    dashed.dashIntervals = {5, 4};
    SkColor4f paper = t.fill;
    paper.fR = 0.75f + paper.fR * 0.25f;
    paper.fG = 0.75f + paper.fG * 0.25f;
    paper.fB = 0.75f + paper.fB * 0.25f;
    return box().width(kSprite - 10).height(kSprite - 18).corners({8})
        .fill(Fill::color(paper))
        .foreground(dashed)
        .column().gap(2).padding(6)
        .child(text(std::move(line1), styleAt(12, ink(t))))
        .child(text(std::move(line2), styleAt(10, t.edge.toSkColor())));
  }

  /** 32 variants rendered by a throwaway Composer into an 8×4 atlas:
   *  the same five components cycling through seeded hues and content
   *  pools — no two cells identical. */
  void buildAtlas() {
    sigil::tick::Ticker atlasTicker;
    Composer sprites(atlasTicker, fonts());
    sprites.setSize({kSprite * kCols, kSprite * kRows});

    static constexpr const char8_t *kPillLabels[] = {
        u8"+250", u8"+120", u8"+45", u8"-87", u8"-12", u8"xp",
        u8"gg", u8"♥", u8"lv 9", u8"rare"};
    static constexpr const char8_t *kShoutLabels[] = {u8"POW", u8"BAM",
                                                      u8"ZOK", u8"CRIT"};
    static constexpr const char8_t *kSealLabels[] = {u8"act I", u8"act II",
                                                     u8"fin", u8"oath"};
    static constexpr const char8_t *kFrameLabels[] = {u8"+1", u8"+3",
                                                      u8"7", u8"key"};
    static constexpr const char8_t *kNoteLines[][2] = {
        {u8"run!", u8"north"}, {u8"hide!", u8"east"},
        {u8"loot!", u8"cave"}, {u8"rest", u8"camp"}};
    const Palette framePals[4] = {oakPalette(), azurePalette(),
                                  crimsonPalette(), emeraldPalette()};

    std::mt19937 rng{23};
    auto column = box().column();
    for (int row = 0; row < kRows; ++row) {
      auto rowBox = box().row();
      for (int col = 0; col < kCols; ++col) {
        const int i = row * kCols + col;
        const float hue = std::fmod((float)i * 137.5f, 360.0f);
        const ChipTheme theme = chipTheme(hue, (i % 3) != 0);
        Element content = [&]() -> Element {
          switch (i % 5) {
          case 0: return pill(theme, kPillLabels[rng() % 10]);
          case 1: return shout(theme, kShoutLabels[rng() % 4],
                               8 + (int)(rng() % 5));
          case 2: return seal(theme, kSealLabels[rng() % 4],
                              7.0f + (float)(rng() % 4));
          case 3: return framed(framePals[rng() % 4],
                                kFrameLabels[rng() % 4]);
          default: {
            const auto &lines = kNoteLines[rng() % 4];
            return note(theme, lines[0], lines[1]);
          }
          }
        }();
        rowBox.child(box().width(kSprite).height(kSprite)
                         .alignItems(Align::Center)
                         .justify(Justify::Center)
                         .child(std::move(content)));
      }
      column.child(std::move(rowBox));
    }
    sprites.render(std::move(column));

    // Bake at kAtlasScale so sprites stay sharp under RSXform scale and
    // HiDPI canvases — raster textures blur when magnified, so never
    // let drawAtlas magnify: oversample here, minify at draw time.
    sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        (int)(kCell * kCols), (int)(kCell * kRows)));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    surface->getCanvas()->scale(kAtlasScale, kAtlasScale);
    sprites.draw(*surface->getCanvas());
    atlas = surface->makeImageSnapshot();
  }

  void setup(Composer &composer, sigil::tick::Ticker &ticker) override {
    buildAtlas();
    registry.clear();
    std::mt19937 rng{11};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < kCount; ++i) {
      entt::entity e = registry.create();
      registry.emplace<Pos>(e, unit() * kSceneSize.width(),
                            unit() * kSceneSize.height());
      registry.emplace<Vel>(e, unit() * 60 - 30, -20.0f - unit() * 70);
      registry.emplace<Look>(e, (uint8_t)(rng() % kVariants),
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
            .fill(sigil::compose::util::linearGradient(
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
                               l.scale / kAtlasScale, a, p.x, p.y,
                               kCell / 2, kCell / 2));
                           texRects.push_back(SkRect::MakeXYWH(
                               kCell * (float)(l.sprite % kCols),
                               kCell * (float)(l.sprite / kCols),
                               kCell, kCell));
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
            .child(text(u8"1,500 sprites from 32 atlas variants — five "
                        u8"components, each instantiated with its own hue "
                        u8"and content; one drawAtlas, EnTT SoA",
                        styleAt(16, 0xffdde4f2))
                       .absolute().inset(24, 24, 24, 590).zIndex(1)));
  }
};

} // namespace compose_gallery
