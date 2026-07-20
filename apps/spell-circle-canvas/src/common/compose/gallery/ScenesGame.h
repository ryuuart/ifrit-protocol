#pragma once
// Expressive game-style scenes: an RPG HUD (vibrant, layered, animated
// game UI built entirely from the public API) and a botanical scene
// (procedural branches, stamped leaves, randomized flower fields).

#include "GalleryCore.h"
#include "OrnamentKit.h"

#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <random>

namespace compose_gallery {

// ---- RPG HUD — vibrant game UI, the practical-usage showcase --------------

struct RpgHudScene final : Scene {
  choreograph::Output<float> hp{0.82f}, mp{0.55f}, pulse{0.0f};
  choreograph::Output<float> critSpin{0.0f};
  bool firstEvent = true;
  bool critVisible = false;
  int critAmount = 0;
  int gold = 12874;
  double nextEvent = 0.0;
  std::mt19937 rng{42};
  struct Slot {
    int rarity; // 0 gray..4 orange
    int icon;   // 0 sword, 1 potion, 2 gem, 3 scroll
    bool operator==(const Slot &) const = default;
  };
  std::vector<Slot> bag;

  const char *name() const override { return "rpg_hud"; }

  static SkColor4f rarityColor(int r) {
    switch (r) {
    case 1: return {0.35f, 0.78f, 0.36f, 1};
    case 2: return {0.30f, 0.56f, 0.95f, 1};
    case 3: return {0.66f, 0.40f, 0.93f, 1};
    case 4: return {0.98f, 0.62f, 0.20f, 1};
    default: return {0.45f, 0.47f, 0.52f, 1};
    }
  }

  static void drawIcon(SkCanvas &c, int icon, SkSize size) {
    SkPaint p;
    p.setAntiAlias(true);
    const float w = size.width(), h = size.height();
    switch (icon) {
    case 0: { // sword
      p.setColor(0xffdde4f2);
      SkPathBuilder b;
      b.moveTo(w * 0.5f, h * 0.12f);
      b.lineTo(w * 0.62f, h * 0.62f);
      b.lineTo(w * 0.5f, h * 0.56f);
      b.lineTo(w * 0.38f, h * 0.62f);
      b.close();
      c.drawPath(b.detach(), p);
      p.setColor(0xffb08a4f);
      c.drawRect(SkRect::MakeXYWH(w * 0.34f, h * 0.62f, w * 0.32f,
                                  h * 0.07f), p);
      c.drawRect(SkRect::MakeXYWH(w * 0.46f, h * 0.66f, w * 0.08f,
                                  h * 0.22f), p);
      break;
    }
    case 1: // potion
      p.setColor(0xffe45a7c);
      c.drawCircle(w * 0.5f, h * 0.62f, w * 0.24f, p);
      p.setColor(0xff9adcf0);
      c.drawRect(SkRect::MakeXYWH(w * 0.44f, h * 0.18f, w * 0.12f,
                                  h * 0.26f), p);
      break;
    case 2: { // gem
      p.setColor(0xff7ee8ff);
      SkPathBuilder b;
      b.moveTo(w * 0.5f, h * 0.16f);
      b.lineTo(w * 0.78f, h * 0.5f);
      b.lineTo(w * 0.5f, h * 0.84f);
      b.lineTo(w * 0.22f, h * 0.5f);
      b.close();
      c.drawPath(b.detach(), p);
      break;
    }
    default: // scroll
      p.setColor(0xffe8d9a8);
      c.drawRoundRect(SkRect::MakeXYWH(w * 0.24f, h * 0.2f, w * 0.52f,
                                       h * 0.6f), 4, 4, p);
      p.setColor(0xffb08a4f);
      for (int i = 0; i < 3; ++i)
        c.drawRect(SkRect::MakeXYWH(w * 0.32f, h * (0.32f + 0.14f * (float)i),
                                    w * 0.36f, h * 0.05f), p);
    }
  }

  static Element resourceBar(const char *label, float fraction,
                             const choreograph::Output<float> *bound,
                             SkColor4f from, SkColor4f to) {
    (void)fraction;
    return box().row().gap(8).height(22)
        .child(text(toU8(label), styleAt(14, 0xff9aa4bb)).width(28))
        .child(box().grow(1).corners({9}).clip()
                   .fill(Fill::color({0, 0, 0, 0.45f}))
                   .child(custom([bound, from, to](SkCanvas &c,
                                                   const PaintContext &ctx) {
                            const float f =
                                std::clamp(bound->value(), 0.0f, 1.0f);
                            SkPoint pts[2] = {{0, 0},
                                              {ctx.size.width(), 0}};
                            SkColor4f colors[2] = {from, to};
                            SkPaint p;
                            p.setShader(SkShaders::LinearGradient(
                                pts, SkGradient({{colors, 2},
                                                 {},
                                                 SkTileMode::kClamp},
                                                {})));
                            c.drawRoundRect(
                                SkRect::MakeWH(ctx.size.width() * f,
                                               ctx.size.height()),
                                9, 9, p);
                          })
                              .inset(0)
                              .cache(Cache::None)));
  }

  static Element bagSlot(const Slot &s) {
    auto cell = box().width(56).height(56).corners({10})
                    .background(sigil::compose::util::shadow(
                        rarityColor(s.rarity), {0, 0}, 14))
                    .fill(Fill::color({0.09f, 0.10f, 0.16f, 1}))
                    .foreground(sigil::compose::util::stroke(
                        2, Fill::color(rarityColor(s.rarity))));
    const int icon = s.icon;
    cell.child(custom([icon](SkCanvas &c, const PaintContext &ctx) {
                 drawIcon(c, icon, ctx.size);
               }).inset(0));
    return cell;
  }

  static Element anchored(Element e, float l, float t, float w, float h) {
    return std::move(e).width(w).height(h)
        .inset(l, t, kSceneSize.width() - l - w,
               kSceneSize.height() - t - h).absolute();
  }

  Element describe() {
    auto bagGrid = box().row().gap(10);
    for (size_t i = 0; i < bag.size(); ++i)
      bagGrid.child(memo(bag[i], bagSlot).key("slot" + std::to_string(i)));

    return stack()
        .fill(sigil::compose::util::linearGradient(
            {0, 0}, {900, 640},
            {{0.10f, 0.05f, 0.20f, 1}, {0.03f, 0.10f, 0.16f, 1}}))
        // Character card
        .child(anchored(box().column().gap(10).padding(16), 24, 24, 320,
                        178)
                   .corners({16})
                   .background(sigil::compose::util::shadow(
                       {0, 0, 0, 0.6f}, {4, 6}, 18))
                   .fill(Fill::color({0.08f, 0.07f, 0.14f, 0.94f}))
                   .foreground(sigil::compose::util::stroke(
                       1.5f, Fill::color({0.69f, 0.55f, 1.0f, 0.6f})))
                   .child(box().row().gap(12)
                              .child(custom([this](SkCanvas &c,
                                                   const PaintContext &ctx) {
                                       // Stylized knight portrait.
                                       SkPaint p;
                                       p.setAntiAlias(true);
                                       p.setColor(0xff2a2f45);
                                       c.drawCircle(ctx.size.width() / 2,
                                                    ctx.size.height() / 2,
                                                    26, p);
                                       p.setColor(0xff7ee8ff);
                                       c.drawRect(
                                           SkRect::MakeXYWH(
                                               ctx.size.width() / 2 - 18,
                                               ctx.size.height() / 2 - 4,
                                               36, 6),
                                           p);
                                       p.setColor(0xffb18cff);
                                       c.drawCircle(ctx.size.width() / 2,
                                                    ctx.size.height() / 2 -
                                                        16,
                                                    8, p);
                                     })
                                         .width(60).height(60))
                              .child(box().column().gap(4).grow(1)
                                         .child(text(u8"SER IGNIS",
                                                     styleAt(22,
                                                             0xffe8ecf8)))
                                         .child(text(u8"flame warden · lv 42",
                                                     styleAt(13,
                                                             0xff9aa4bb)))))
                   .child(resourceBar("HP", 0.82f, &hp,
                                      {0.95f, 0.35f, 0.35f, 1},
                                      {0.98f, 0.62f, 0.20f, 1}))
                   .child(resourceBar("MP", 0.55f, &mp,
                                      {0.30f, 0.56f, 0.95f, 1},
                                      {0.49f, 0.91f, 1.0f, 1})))
        // Center ember sigil — bound to the idle pulse Output
        .child(anchored(box(), 330, 200, 240, 240)
                   .cache(Cache::None)
                   .child(custom([this](SkCanvas &c,
                                        const PaintContext &ctx) {
                            const float cx = ctx.size.width() / 2;
                            const float cy = ctx.size.height() / 2;
                            const float glow =
                                0.5f + 0.5f * pulse.value();
                            SkPaint p;
                            p.setAntiAlias(true);
                            p.setStyle(SkPaint::kStroke_Style);
                            p.setStrokeWidth(3);
                            p.setColor4f(
                                {0.98f, 0.62f, 0.20f, 0.25f + 0.45f * glow});
                            p.setMaskFilter(SkMaskFilter::MakeBlur(
                                kNormal_SkBlurStyle, 6 + 10 * glow));
                            c.drawCircle(cx, cy, 78, p);
                            p.setMaskFilter(nullptr);
                            p.setStrokeWidth(1.5f);
                            p.setColor4f(
                                {0.69f, 0.55f, 1.0f, 0.3f + 0.4f * glow});
                            c.drawCircle(cx, cy, 62, p);
                            // Rotating rune ticks.
                            const float spin =
                                (float)ctx.elapsedSeconds * 0.6f;
                            p.setStyle(SkPaint::kFill_Style);
                            for (int i = 0; i < 9; ++i) {
                              const float a =
                                  spin + (float)i * 6.2832f / 9.0f;
                              const float r =
                                  70 + 6 * std::sin(spin * 3 + (float)i);
                              p.setColor4f({0.49f, 0.91f, 1.0f,
                                            0.35f + 0.5f * glow});
                              c.drawCircle(cx + std::cos(a) * r,
                                           cy + std::sin(a) * r,
                                           2.5f + 1.5f * glow, p);
                            }
                            // Inner flame kernel.
                            p.setColor4f(
                                {0.98f, 0.62f, 0.20f, 0.5f + 0.5f * glow});
                            c.drawCircle(cx, cy, 10 + 4 * glow, p);
                          })
                              .inset(0)
                              .cache(Cache::None)))
        // Gold + inventory dock
        .child(anchored(box().column().gap(10).padding(16), 24, 484, 600,
                        132)
                   .corners({16})
                   .fill(Fill::color({0.07f, 0.06f, 0.12f, 0.94f}))
                   .foreground(sigil::compose::util::stroke(
                       1.5f, Fill::color({0.98f, 0.62f, 0.20f, 0.5f})))
                   .child(box().row().gap(8)
                              .child(text(u8"◈", styleAt(18, 0xffffb46b)))
                              .child(text(toU8(std::to_string(gold) +
                                               " gold"),
                                          styleAt(16, 0xffffd9a0))
                                         .key("gold")))
                   .child(std::move(bagGrid)))
        // Quest scroll: parchment ground + vine chrome (the same
        // parameterized ornament components the manuscript page uses).
        .child([&] {
          const Palette pal = azurePalette();
          return anchored(box().column().gap(6).padding(16), 548, 24, 328,
                          112)
              .corners({12}).cache(Cache::Texture)
              .background(sigil::compose::util::shadow(
                  {0, 0, 0, 0.5f}, {3, 4}, 10))
              .fill(parchmentFill(pal.parchment))
              .foreground(sigil::compose::util::stroke(
                  2, Fill::color(pal.stem)))
              .foreground(SwirlCorners{pal, 18.0f, 1.6f})
              .child(text(u8"QUEST · The Ember Gate",
                          styleAt(16, 0xff1c3450)))
              .child(text(u8"Carry the last coal through the flooded "
                          u8"causeway before the tide turns.",
                          styleAt(13, 0xff2f2617)));
        }())
        // Spiky crit shout: a non-square dialog — custom outline() the
        // fill, stroke, and clip all follow; wobble bound to critSpin.
        .child([&] {
          if (!critVisible)
            return box().key("crit").width(0).height(0).absolute()
                .inset(0, 0, kSceneSize.width(), kSceneSize.height());
          return anchored(box().key("crit").column()
                              .alignItems(Align::Center)
                              .justify(Justify::Center),
                          370, 96, 170, 140)
              .outline(starburstOutline(12, 0.34f))
              .fill(sigil::compose::util::radialGradient(
                  {85, 70}, 90,
                  {{1.0f, 0.88f, 0.40f, 1}, {0.88f, 0.26f, 0.10f, 1}}))
              .foreground(sigil::compose::util::stroke(
                  3, Fill::color({0.45f, 0.07f, 0.05f, 1})))
              .rotate(&critSpin).zIndex(5)
              .child(text(u8"CRIT!", styleAt(21, 0xff471003)))
              .child(text(toU8(std::to_string(critAmount)),
                          styleAt(30, 0xff2e0a02)));
        }());
  }

  void setup(Composer &composer, sigil::tick::Ticker &ticker) override {
    bag.clear();
    for (int i = 0; i < 8; ++i)
      bag.push_back({(int)(rng() % 5), (int)(rng() % 4)});
    gold = 12874;
    nextEvent = 0.0;
    // Idle pulse drives nothing yet; hp/mp jitter via events below.
    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      pulse = (float)std::sin(t * 2.0);
      critSpin = 4.0f * (float)std::sin(t * 9.0);
      return true;
    });
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextEvent)
      return;
    nextEvent = elapsed + 1.1;
    // Combat tick: damage or regen, loot shuffle, gold trickle.
    const bool hit = firstEvent || (rng() % 3) == 0;
    firstEvent = false;
    critVisible = hit;
    if (hit)
      critAmount = 80 + (int)(rng() % 160);
    // Retarget-from-current ramps via the composer's ticker isn't
    // needed here: bars are bound Outputs — animate them directly.
    // (Scene owns no ticker ref; jitter in setup steppable instead.)
    hp = std::clamp(hp.value() + (hit ? -0.12f : 0.04f), 0.08f, 1.0f);
    mp = std::clamp(mp.value() + ((rng() % 2) ? -0.07f : 0.06f), 0.05f,
                    1.0f);
    gold += (int)(rng() % 45);
    if ((rng() % 4) == 0)
      bag[rng() % bag.size()] = {(int)(rng() % 5), (int)(rng() % 4)};
    composer.render(describe());
  }
};

// ---- Botanical — branches, stamped leaves, randomized flower fields -------

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

  void setup(Composer &composer, sigil::tick::Ticker &ticker) override {
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
