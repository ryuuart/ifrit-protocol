#pragma once
// Scale scene: "UI as particles" — from confetti chips to whole POSTS, and
// none of them look instanced.
//
// TWO instancing::Atlas sheets, each stamped by one instances() leaf
// (Mode::Live) over an EnTT registry sim:
//   - CHIPS: five parameterized components (pill, spiky shout, scalloped
//     seal, carved nine-slice, dashed field note) baked THIRTY-TWO ways —
//     every atlas cell its own hue and content — scattered by the thousand.
//   - POSTS: six bigger multi-paragraph cards (a title + two paragraphs),
//     three border languages — the FLOURISH border we just built
//     (FlourishKit), a carved nine-slice, and a plain modern card — drifting
//     among the chips. Same instancing technique, from a badge to a feed.
//
// The EnTT registries stay on OUR side of the seam (velocities, drift,
// wrap); each frame the sim is copied into two instancing::Pool SoA stores
// through their spans, and the first-class layer does the baking (2x
// oversample built in) and the drawAtlas stamping.

#include "FlourishKit.h"
#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <sigilcompose/Instances.h>

#include <entt/entt.hpp>

#include <cmath>
#include <memory>
#include <random>

namespace compose_gallery {

struct UiParticleScene final : Scene {
  // ---- chips (the confetti tier) ------------------------------------------
  static constexpr size_t kChipCount = 820;
  static constexpr float kSprite = 64.0f;
  static constexpr int kVariants = 32;

  // ---- posts (the feed tier) ----------------------------------------------
  static constexpr size_t kPostCount = 30;
  static constexpr float kPostW = 232.0f, kPostH = 148.0f;
  static constexpr int kPostVariants = 6;

  entt::registry chips;
  entt::registry posts;
  std::shared_ptr<instancing::Atlas> chipAtlas, postAtlas;
  std::shared_ptr<instancing::Pool> chipPool, postPool;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };
  struct Look { uint8_t sprite; float scale; float spin; };

  const char *name() const override { return "ui_particles"; }

  // ---- chip components (one component, many skins) ------------------------

  struct ChipTheme { SkColor4f fill, edge, ink; };
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
    return {hsv(hueDegrees, 0.62f, 0.94f), hsv(hueDegrees, 0.80f, 0.45f),
            darkInk ? hsv(hueDegrees, 0.85f, 0.22f) : SkColor4f{1, 1, 1, 1}};
  }
  static SkColor ink(const ChipTheme &t) { return t.ink.toSkColor(); }

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
        .background(carvedFrameSlice(std::make_shared<sigil::image::ImageAsset>(
            sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 96)))))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(std::move(label), styleAt(15, pal.ink.toSkColor())));
  }
  Element note(const ChipTheme &t, std::u8string line1, std::u8string line2) {
    PathFormat dashed;
    dashed.width = 1.6f;
    dashed.strokeFill = Fill::color(t.edge);
    dashed.dashIntervals = {5, 4};
    SkColor4f paper = t.fill;
    paper.fR = 0.75f + paper.fR * 0.25f;
    paper.fG = 0.75f + paper.fG * 0.25f;
    paper.fB = 0.75f + paper.fB * 0.25f;
    return box().width(kSprite - 10).height(kSprite - 18).corners({8})
        .fill(Fill::color(paper)).foreground(dashed)
        .column().gap(2).padding(6)
        .child(text(std::move(line1), styleAt(12, ink(t))))
        .child(text(std::move(line2), styleAt(10, t.edge.toSkColor())));
  }

  void buildChipAtlas() {
    chipAtlas = std::make_shared<instancing::Atlas>(); // 2x oversample built in

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
    for (int i = 0; i < kVariants; ++i) {
      const float hue = std::fmod((float)i * 137.5f, 360.0f);
      const ChipTheme theme = chipTheme(hue, (i % 3) != 0);
      Element content = [&]() -> Element {
        switch (i % 5) {
        case 0: return pill(theme, kPillLabels[rng() % 10]);
        case 1: return shout(theme, kShoutLabels[rng() % 4],
                             8 + (int)(rng() % 5));
        case 2: return seal(theme, kSealLabels[rng() % 4],
                            7.0f + (float)(rng() % 4));
        case 3: return framed(framePals[rng() % 4], kFrameLabels[rng() % 4]);
        default: {
          const auto &lines = kNoteLines[rng() % 4];
          return note(theme, lines[0], lines[1]);
        }
        }
      }();
      chipAtlas->cell(box().alignItems(Align::Center).justify(Justify::Center)
                          .child(std::move(content)),
                      {kSprite, kSprite});
    }
  }

  // ---- post components (the feed tier) ------------------------------------

  enum class PostKind { Flourish, Carved, Plain };
  struct PostConfig {
    PostKind kind;
    int paletteIndex;               // carved: OrnamentKit palette; plain: accent
    const char8_t *title;
    const char8_t *body1;
    const char8_t *body2;
  };

  Element flourishPost(const PostConfig &cfg) {
    FlourishStyle s; // gilt-on-parchment
    return flourishCard(s, kPostW - 6, kPostH - 6)
        .child(text(cfg.title, styleAt(15, toSk(s.ink))))
        .child(text(cfg.body1, styleAt(10.5f, toSk(s.ink))))
        .child(text(cfg.body2, styleAt(10.5f,
                                       toSk({s.bronze.fR, s.bronze.fG,
                                             s.bronze.fB, 1}))));
  }
  Element carvedPost(const PostConfig &cfg) {
    const Palette pals[4] = {oakPalette(), azurePalette(), crimsonPalette(),
                             emeraldPalette()};
    const Palette &pal = pals[cfg.paletteIndex & 3];
    return box().width(kPostW - 6).height(kPostH - 6)
        .background(carvedFrameSlice(std::make_shared<sigil::image::ImageAsset>(
            sigil::image::ImageAsset::wrap(makeCarvedFrame(pal, 128)))))
        .column().padding(30, 26).gap(5)
        .child(text(cfg.title, styleAt(15, pal.stem.toSkColor())))
        .child(text(cfg.body1, styleAt(10.5f, pal.ink.toSkColor())))
        .child(text(cfg.body2, styleAt(10.5f, pal.ink.toSkColor())));
  }
  Element plainPost(const PostConfig &cfg) {
    // A modern dark UI card — the counterpoint to the ornate borders.
    const SkColor4f accents[2] = {{0.42f, 0.66f, 0.98f, 1},   // cobalt
                                  {0.98f, 0.72f, 0.34f, 1}};  // amber
    const SkColor4f accent = accents[cfg.paletteIndex & 1];
    return box().width(kPostW - 6).height(kPostH - 6).corners({12})
        .fill(Fill::color({0.10f, 0.11f, 0.15f, 1}))
        .foreground(sigil::compose::util::stroke(1.4f, Fill::color(accent)))
        .column().padding(16, 14).gap(6)
        .child(text(cfg.title, styleAt(15, accent.toSkColor())))
        .child(box().width(pct(38)).height(2).corners({1})
                   .fill(Fill::color(accent)))
        .child(text(cfg.body1, styleAt(10.5f, 0xffcdd3df)))
        .child(text(cfg.body2, styleAt(10.5f, 0xff9aa3b4)));
  }

  Element postVariant(const PostConfig &cfg) {
    switch (cfg.kind) {
    case PostKind::Flourish: return flourishPost(cfg);
    case PostKind::Carved: return carvedPost(cfg);
    default: return plainPost(cfg);
    }
  }

  void buildPostAtlas() {
    static constexpr PostConfig kPosts[kPostVariants] = {
        {PostKind::Flourish, 0, u8"Codex I",
         u8"The causeway held through the night; we counted the lanterns "
         u8"twice, once for the living channel and once for the drowned road.",
         u8"Feed the vine its silver hour and the gate stands open one "
         u8"crossing more."},
        {PostKind::Carved, 0, u8"Quartermaster",
         u8"Six barrels of pitch, the copper bowls, and forty fathoms of "
         u8"good rope coiled against the cellar wall.",
         u8"One coal, still warm. Signed at the water line."},
        {PostKind::Plain, 0, u8"BUILD #482",
         u8"reconciler pruned every unchanged subtree; 0 pictures "
         u8"re-recorded, dirty() stayed false.",
         u8"frame 0.41 ms · 100 rows cached · headroom to spare"},
        {PostKind::Flourish, 2, u8"Rubric II",
         u8"The gate takes no coin but memory, and gives back the shape of "
         u8"every hand that held the rope.",
         u8"Draw the circle wide; keep the margin free for the vine."},
        {PostKind::Carved, 3, u8"Warden's Note",
         u8"The reeds keep time against the slow current where the ferry "
         u8"rope hums under a traveler's hand.",
         u8"Twelve nights the lanterns answered; on the last, the gate "
         u8"stood open."},
        {PostKind::Plain, 1, u8"RELEASE",
         u8"texture-baked bloom pays the filter once — 92x over per-frame "
         u8"picture replay on raster.",
         u8"ship it; the numbers held on Graphite too"},
    };

    postAtlas = std::make_shared<instancing::Atlas>(); // 2x: crisp paragraphs
    for (int i = 0; i < kPostVariants; ++i)
      postAtlas->cell(box().alignItems(Align::Center).justify(Justify::Center)
                          .child(postVariant(kPosts[i])),
                      {kPostW, kPostH});
  }

  // ---- seeding + stepping -------------------------------------------------

  static void seed(entt::registry &reg, size_t count, int variants,
                   uint32_t rngSeed, float scaleLo, float scaleHi,
                   float velUp) {
    std::mt19937 rng{rngSeed};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < count; ++i) {
      entt::entity e = reg.create();
      reg.emplace<Pos>(e, unit() * kSceneSize.width(),
                       unit() * kSceneSize.height());
      reg.emplace<Vel>(e, unit() * 40 - 20, -velUp * (0.4f + unit()));
      reg.emplace<Look>(e, (uint8_t)(rng() % variants),
                        scaleLo + unit() * (scaleHi - scaleLo),
                        unit() * 0.3f - 0.15f);
    }
  }

  static void step(entt::registry &reg, double dt, float margin) {
    reg.view<Pos, const Vel>().each([dt, margin](Pos &p, const Vel &v) {
      p.x += v.dx * (float)dt;
      p.y += v.dy * (float)dt;
      if (p.y < -margin) p.y += kSceneSize.height() + margin;
      if (p.x < -margin) p.x += kSceneSize.width() + margin;
      else if (p.x > kSceneSize.width()) p.x -= kSceneSize.width() + margin;
    });
  }

  // The EnTT → Pool copy-in: the registry stays the sim, the pool spans
  // are the seam the instances() leaf reads (Mode::Live, every frame).
  static void syncPool(entt::registry &reg, instancing::Pool &pool, double t) {
    auto positions = pool.positions();
    auto rotations = pool.rotations();
    auto scales = pool.scales();
    auto frames = pool.frames();
    size_t i = 0;
    reg.view<const Pos, const Look>().each([&](const Pos &p, const Look &l) {
      positions[i] = {p.x, p.y};
      rotations[i] = l.spin * (float)std::sin(t * 1.6 + p.x * 0.01);
      scales[i] = l.scale;
      frames[i] = l.sprite;
      ++i;
    });
  }

  void update(double t, Composer &) override {
    syncPool(chips, *chipPool, t);
    syncPool(posts, *postPool, t);
  }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    buildChipAtlas();
    buildPostAtlas();
    chips.clear();
    posts.clear();
    seed(chips, kChipCount, kVariants, 11, 0.35f, 0.9f, 70.0f);
    seed(posts, kPostCount, kPostVariants, 71, 0.42f, 0.62f, 24.0f);
    chipPool = std::make_shared<instancing::Pool>();
    postPool = std::make_shared<instancing::Pool>();
    chipPool->resize(kChipCount);
    postPool->resize(kPostCount);
    syncPool(chips, *chipPool, 0.0);
    syncPool(posts, *postPool, 0.0);

    ticker.add([this](double dt) {
      step(chips, dt, kSprite);
      step(posts, dt, kPostW);
      return true;
    });

    // instances() fills its parent; each tier gets a full-canvas box so
    // pool positions are canvas pixels. Chips behind, posts in front.
    composer.render(
        stack()
            .fill(sigil::compose::util::linearGradient(
                {0, 0}, {0, 640},
                {{0.05f, 0.04f, 0.12f, 1}, {0.12f, 0.05f, 0.14f, 1}}))
            .child(box().inset(0).child(instancing::instances(
                chipAtlas, chipPool, instancing::Mode::Live)))
            .child(box().inset(0).child(instancing::instances(
                postAtlas, postPool, instancing::Mode::Live)))
            .child(text(u8"UI as particles — 820 chips + 30 multi-paragraph "
                        u8"posts (flourish, carved & plain borders), each tier "
                        u8"one instances() stamp over an EnTT SoA registry",
                        styleAt(16, 0xffdde4f2))
                       .inset(24, 24, 24, 590).zIndex(1)));
  }
};

} // namespace compose_gallery
