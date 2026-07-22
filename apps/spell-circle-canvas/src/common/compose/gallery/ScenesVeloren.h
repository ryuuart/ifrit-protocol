#pragma once
// The action-RPG HUD study, grounded in a real one: Veloren's
// (github.com/veloren/veloren, voxygen/src/hud/). Every dimension and
// every colour below is read out of that source rather than invented —
// which is the point, because a HUD's proportions ARE its design.
//
// From voxygen/src/hud/skillbar.rs:
//   health frame 484x24, content 480x18       energy frame 323x16,
//   content 319x10                            poise frame 323x16,
//   tick 3x10                                 hotbar slot 40x40 in a
//   42x42 frame, ten of them between the two mouse slots
//   selected-exp chip 34x38, hung off slot10
// From voxygen/src/hud/mod.rs (the palette, verbatim Rgba -> hex):
//   HP #54A100   LOW_HP #ED9608   CRITICAL_HP #C9302B   STAMINA #4A9EBF
//   XP #9669AB   POISE #B30099    POISE_TICK #B3E600    ENEMY_HP #ED1A4A
//   BUFF #10B01F DEBUFF #C9302B   quality ladder LOW #999999 /
//   COMMON #C9FFFF / MODERATE #10B01F / HIGH #2E52E6 / EPIC #944AED /
//   LEGENDARY #EBC200 / ARTIFACT #BD3D1C
//
// What it exercises: a bar STACK whose widths carry meaning (the health
// bar is 1.5x the energy bar because Veloren says so), the decay ghost
// Veloren paints in QUALITY_EPIC behind lost maximum health, the low-HP
// animation, a twelve-slot hotbar through instances() with per-slot
// cooldown sweeps, a framed minimap with a rotating compass rose over
// generated terrain, buff/debuff pips with drain rings, the loot
// scroller's quality-coloured feed, and an enemy nameplate.
//
// Veloren's own art is hand-painted wood and bone. Nothing here is an
// image: the frames are ramps under noise inside a bevel, the terrain is
// patterns::noise thresholded into bands, the item glyphs are paths.

#include "GalleryCore.h"

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace compose_gallery {

namespace worldhud {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

// voxygen/src/hud/mod.rs, verbatim.
constexpr SkColor4f kHp = C(0x54A100);
constexpr SkColor4f kLowHp = C(0xED9608);
constexpr SkColor4f kCritHp = C(0xC9302B);
constexpr SkColor4f kStamina = C(0x4A9EBF);
constexpr SkColor4f kXp = C(0x9669AB);
constexpr SkColor4f kPoise = C(0xB30099);
constexpr SkColor4f kPoiseTick = C(0xB3E600);
constexpr SkColor4f kEnemyHp = C(0xED1A4A);
constexpr SkColor4f kBuff = C(0x10B01F);
constexpr SkColor4f kDebuff = C(0xC9302B);
constexpr SkColor4f kQualityLow = C(0x999999);
constexpr SkColor4f kQualityCommon = C(0xC9FFFF);
constexpr SkColor4f kQualityModerate = C(0x10B01F);
constexpr SkColor4f kQualityHigh = C(0x2E52E6);
constexpr SkColor4f kQualityEpic = C(0x944AED);
constexpr SkColor4f kQualityLegendary = C(0xEBC200);
constexpr SkColor4f kQualityArtifact = C(0xBD3D1C);

// The frame material: Veloren's UI is carved bone over dark wood.
constexpr SkColor4f kBoneHi = C(0xD8CBA8);
constexpr SkColor4f kBone = C(0xA2947A);
constexpr SkColor4f kBoneLo = C(0x584E3D);
constexpr SkColor4f kWood = C(0x2A2118);
constexpr SkColor4f kWoodLo = C(0x160F0A);
constexpr SkColor4f kTrack = C(0x0B0906);
constexpr SkColor4f kInk = C(0xEDE6D4);
constexpr SkColor4f kInkDim = C(0x8C8271);

// skillbar.rs dimensions, unscaled — the stage is wide enough to take
// them, and scaling them would be the one thing that loses the study.
constexpr float kHealthW = 484, kHealthH = 24;
constexpr float kHealthInnerW = 480, kHealthInnerH = 18;
constexpr float kEnergyW = 323, kEnergyH = 16;
constexpr float kEnergyInnerW = 319, kEnergyInnerH = 10;
constexpr float kSlot = 40, kSlotFrame = 42, kSlotGap = 2;
constexpr int kSlotCount = 12; // M1 + ten numbered + M2
constexpr float kBarX = (kW - kHealthW) * 0.5f;
constexpr float kBarY = 486;
constexpr float kEnergyY = 514;
constexpr float kPoiseY = 534;
constexpr float kSlotsY = 556;
constexpr float kSlotsW = kSlotCount * kSlotFrame + (kSlotCount - 1) * kSlotGap;
constexpr float kSlotsX = (kW - kSlotsW) * 0.5f;

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (weight > 0)
    s.shaping.variations = {sigil::weave::FontVariation("wght", weight)};
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  // Veloren draws every HUD string twice: black underneath, then the
  // colour on top. At 10px over terrain that is the whole legibility
  // budget, so it is not optional.
  sigil::weave::PaintLayer shade;
  shade.paint.setColor4f({0, 0, 0, 0.9f}, nullptr);
  shade.paint.setAntiAlias(true);
  shade.offset = {1, 1};
  s.paint.addUnderlay(shade);
  return s;
}

/** A sunk track: the black slot a bar's content sits in. */
inline Element track(float w, float h) {
  return box().width(Dim(w)).height(Dim(h))
      .fill(Material::solid(kTrack))
      .foreground(styles::InnerShadow{{0, 0, 0, 0.85f}, {0, 2}, 3});
}

/** The carved bone frame Veloren hangs on everything. */
inline Element boneFrame(float w, float h, float radius = 3) {
  return box().width(Dim(w)).height(Dim(h)).corners({radius})
      .fill(Material::linear({0, 0}, {0, h},
                             {{0.0f, kBoneHi}, {0.45f, kBone},
                              {1.0f, kBoneLo}}))
      .foreground(styles::BevelEmboss{1.6f, 2.4f, 120,
                                      {1, 1, 1, 0.35f}, {0, 0, 0, 0.65f}})
      .foreground(util::stroke(1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f}),
                               PathFormat::Align::Outer));
}

/** A bar: bone frame, sunk track, content, and Veloren's decay ghost —
 *  the QUALITY_EPIC band it paints over maximum health you have lost. */
inline Element bar(float frameW, float frameH, float innerW, float innerH,
                   float fraction, SkColor4f color, float decay = 0.0f) {
  const float padX = (frameW - innerW) * 0.5f;
  const float padY = (frameH - innerH) * 0.5f;
  Element e = boneFrame(frameW, frameH, 2)
                  .child(track(innerW, innerH)
                             .left(padX).top(padY));
  if (decay > 0.0f)
    e.child(box()
                .left(padX + innerW * (1.0f - decay)).top(padY)
                .width(Dim(innerW * decay)).height(Dim(innerH))
                .fill(Material::solid({kQualityEpic.fR, kQualityEpic.fG,
                                       kQualityEpic.fB, 0.55f})));
  e.child(box().left(padX).top(padY)
              .width(Dim(innerW * fraction)).height(Dim(innerH))
              .fill(Material::linear(
                  {0, 0}, {0, innerH},
                  {{0.0f, {std::min(1.0f, color.fR * 1.45f + 0.06f),
                           std::min(1.0f, color.fG * 1.45f + 0.06f),
                           std::min(1.0f, color.fB * 1.45f + 0.06f), 1}},
                   {0.5f, color},
                   {1.0f, {color.fR * 0.62f, color.fG * 0.62f,
                           color.fB * 0.62f, 1}}})));
  return e;
}

/** Hotbar item glyphs — paths, so a slot never needs a sprite. */
enum class Glyph { Sword, Bow, Fire, Frost, Heal, Shield, Dash, Bomb };

inline std::function<SkPath(SkSize)> glyphPath(Glyph g) {
  return [g](SkSize s) {
    const float w = s.width(), h = s.height(), cx = w * 0.5f;
    SkPathBuilder b;
    switch (g) {
    case Glyph::Sword:
      b.moveTo(cx, h * 0.08f);
      b.lineTo(cx + w * 0.11f, h * 0.22f);
      b.lineTo(cx + w * 0.08f, h * 0.66f);
      b.lineTo(cx - w * 0.08f, h * 0.66f);
      b.lineTo(cx - w * 0.11f, h * 0.22f);
      b.close();
      b.addRect(SkRect::MakeXYWH(w * 0.20f, h * 0.66f, w * 0.60f, h * 0.06f));
      b.addRect(SkRect::MakeXYWH(cx - w * 0.05f, h * 0.72f, w * 0.10f,
                                 h * 0.20f));
      break;
    case Glyph::Bow:
      b.addArc(SkRect::MakeXYWH(w * 0.20f, h * 0.10f, w * 0.62f, h * 0.80f),
               120, 130);
      b.moveTo(w * 0.32f, h * 0.14f);
      b.lineTo(w * 0.32f, h * 0.86f);
      b.moveTo(w * 0.32f, h * 0.50f);
      b.lineTo(w * 0.82f, h * 0.50f);
      break;
    case Glyph::Fire:
      b.moveTo(cx, h * 0.08f);
      b.quadTo(w * 0.86f, h * 0.44f, w * 0.72f, h * 0.72f);
      b.quadTo(w * 0.60f, h * 0.94f, cx, h * 0.92f);
      b.quadTo(w * 0.40f, h * 0.94f, w * 0.28f, h * 0.72f);
      b.quadTo(w * 0.14f, h * 0.44f, cx, h * 0.08f);
      b.close();
      break;
    case Glyph::Frost:
      for (int i = 0; i < 3; ++i) {
        const float a = (float)i * 1.0471976f;
        const float dx = std::cos(a) * w * 0.36f, dy = std::sin(a) * h * 0.36f;
        b.moveTo(cx - dx, h * 0.5f - dy);
        b.lineTo(cx + dx, h * 0.5f + dy);
      }
      break;
    case Glyph::Heal:
      b.addRect(SkRect::MakeXYWH(cx - w * 0.09f, h * 0.16f, w * 0.18f,
                                 h * 0.68f));
      b.addRect(SkRect::MakeXYWH(w * 0.16f, h * 0.41f, w * 0.68f, h * 0.18f));
      break;
    case Glyph::Shield:
      b.moveTo(w * 0.18f, h * 0.16f);
      b.lineTo(w * 0.82f, h * 0.16f);
      b.lineTo(w * 0.82f, h * 0.54f);
      b.quadTo(w * 0.82f, h * 0.82f, cx, h * 0.90f);
      b.quadTo(w * 0.18f, h * 0.82f, w * 0.18f, h * 0.54f);
      b.close();
      break;
    case Glyph::Dash:
      for (int i = 0; i < 3; ++i) {
        const float x = w * (0.22f + 0.22f * (float)i);
        b.moveTo(x, h * 0.24f);
        b.lineTo(x + w * 0.16f, h * 0.50f);
        b.lineTo(x, h * 0.76f);
      }
      break;
    case Glyph::Bomb:
      b.addCircle(cx, h * 0.60f, w * 0.28f);
      b.moveTo(cx + w * 0.10f, h * 0.34f);
      b.quadTo(cx + w * 0.34f, h * 0.16f, cx + w * 0.22f, h * 0.06f);
      break;
    }
    return b.detach();
  };
}

} // namespace worldhud

struct WorldHudScene final : Scene {
  // Plain fractions. Every bar here is a full-size fill with its growing
  // edge pinned by transformOrigin() and its extent carried by scaleX —
  // which is what these Outputs used to encode as pixel slides inside a
  // clip, back when Element had only a uniform scale().
  choreograph::Output<float> hp{0.62f}, energy{0.78f}, poise{0.55f};
  choreograph::Output<float> xp{0}, enemyHp{0.4f};
  choreograph::Output<float> lowPulse{0}, compass{0};
  std::array<choreograph::Output<float>, 4> cooldown{};

  std::shared_ptr<instancing::Atlas> slotAtlas;
  std::shared_ptr<instancing::Pool> slotPool;

  const char *name() const override { return "world hud"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace wh = worldhud;
    hp = 0.62f;
    energy = 0.78f;
    poise = 0.55f;
    xp = 0;
    enemyHp = 0.4f;
    lowPulse = 0;
    compass = 0;
    for (auto &c : cooldown)
      c = 0.0f;

    // The empty slot frame is one atlas cell stamped twelve times.
    slotAtlas = std::make_shared<instancing::Atlas>(2.0f);
    slotAtlas->cell(wh::boneFrame(wh::kSlotFrame, wh::kSlotFrame, 3)
                        .child(wh::track(wh::kSlot - 4, wh::kSlot - 4)
                                   .left(5).top(5)),
                    {wh::kSlotFrame, wh::kSlotFrame});
    slotPool = std::make_shared<instancing::Pool>();
    for (int i = 0; i < wh::kSlotCount; ++i)
      slotPool->add({i * (wh::kSlotFrame + wh::kSlotGap) +
                         wh::kSlotFrame * 0.5f,
                     wh::kSlotFrame * 0.5f});

    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // Combat, on a loop: health drains, a heal lands, energy spends and
      // regenerates, poise breaks and recovers.
      const double cycle = std::fmod(t, 9.0);
      float h = 0.62f;
      if (cycle < 3.0)
        h = 0.62f - 0.42f * (float)(cycle / 3.0);
      else if (cycle < 3.5)
        h = 0.20f + 0.55f * (float)((cycle - 3.0) / 0.5);
      else
        h = 0.75f - 0.13f * (float)((cycle - 3.5) / 5.5);
      hp = h;
      // Veloren's hp_ani: below 20% the bar breathes.
      lowPulse = h < 0.25f
                     ? 0.5f + 0.5f * (float)std::sin(t * 9.0)
                     : 0.0f;
      energy = 0.35f + 0.45f * (float)(0.5 + 0.5 * std::sin(t * 0.9));
      poise = 0.30f + 0.60f * (float)(0.5 + 0.5 * std::sin(t * 0.55 + 1.7));
      compass = (float)std::fmod(t * 8.0, 360.0);
      xp = (float)std::fmod(t * 0.11, 1.0);
      enemyHp = std::max(0.0f, 0.85f - (float)std::fmod(t * 0.16, 1.0));
      for (size_t i = 0; i < cooldown.size(); ++i) {
        const double period = 2.4 + 0.9 * (double)i;
        // the dark cover keeps its top edge and its bottom edge rises
        cooldown[i] = 1.0f - (float)(std::fmod(t, period) / period);
      }
      return true;
    });

    composer.render(describe());
  }

  // ------------------------------------------------------------------

  Element barStack() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    Element stackEl = stack().inset(0);

    // health, with the decay ghost and the low-HP wash
    stackEl.child(box().left(wh::kBarX).top(wh::kBarY)
                      .child(wh::bar(wh::kHealthW, wh::kHealthH,
                                     wh::kHealthInnerW, wh::kHealthInnerH,
                                     0.62f, wh::kHp, 0.14f)));
    // the live fill rides on top of the static frame so only IT repaints
    stackEl.child(box()
                      .left(wh::kBarX + 2).top(wh::kBarY + 3)
                      .width(Dim(wh::kHealthInnerW))
                      .height(Dim(wh::kHealthInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&hp)
                      .fill(Material::linear(
                          {0, 0}, {0, wh::kHealthInnerH},
                          {{0.0f, worldhud::C(0x7FE000)},
                           {0.5f, wh::kHp},
                           {1.0f, worldhud::C(0x2F5C00)}})));
    stackEl.child(box()
                      .left(wh::kBarX).top(wh::kBarY)
                      .width(Dim(wh::kHealthW)).height(Dim(wh::kHealthH))
                      .corners({2})
                      .fill(Material::solid({wh::kCritHp.fR, wh::kCritHp.fG,
                                             wh::kCritHp.fB, 0.55f}))
                      .opacity(&lowPulse)
                      .blend(SkBlendMode::kPlus));
    stackEl.child(text(toU8("640 / 1030"), wh::type(11, wh::kInk, 0.8f))
                      .left(wh::kBarX + wh::kHealthW * 0.5f - 30)
                      .top(wh::kBarY + 5));

    // energy
    const float ex = (wh::kW - wh::kEnergyW) * 0.5f;
    stackEl.child(box().left(ex).top(wh::kEnergyY)
                      .child(wh::bar(wh::kEnergyW, wh::kEnergyH,
                                     wh::kEnergyInnerW, wh::kEnergyInnerH,
                                     1.0f, wh::kStamina)));
    stackEl.child(box().left(ex + 2).top(wh::kEnergyY + 3)
                      .width(Dim(wh::kEnergyInnerW))
                      .height(Dim(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&energy)
                      .fill(Material::solid(wh::kStamina)));

    // poise, with skillbar.rs's 3x10 ticks along it
    stackEl.child(box().left(ex).top(wh::kPoiseY)
                      .child(wh::bar(wh::kEnergyW, wh::kEnergyH,
                                     wh::kEnergyInnerW, wh::kEnergyInnerH,
                                     1.0f, wh::kPoise)));
    stackEl.child(box().left(ex + 2).top(wh::kPoiseY + 3)
                      .width(Dim(wh::kEnergyInnerW))
                      .height(Dim(wh::kEnergyInnerH))
                      .transformOrigin(0.0f, 0.5f)
                      .scaleX(&poise)
                      .fill(Material::solid(wh::kPoise)));
    for (int i = 1; i < 6; ++i)
      stackEl.child(box()
                        .left(ex + 2 + wh::kEnergyInnerW * (float)i / 6.0f)
                        .top(wh::kPoiseY + 3)
                        .width(Dim(3.0f)).height(Dim(10.0f))
                        .fill(Material::solid(wh::kPoiseTick)));
    return stackEl;
  }

  Element hotbar() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    static const struct { const char *key; wh::Glyph glyph; bool filled; }
    kSlots[] = {
        {"M1", wh::Glyph::Sword, true},   {"1", wh::Glyph::Fire, true},
        {"2", wh::Glyph::Frost, true},    {"3", wh::Glyph::Heal, true},
        {"4", wh::Glyph::Dash, true},     {"5", wh::Glyph::Shield, true},
        {"6", wh::Glyph::Bomb, true},     {"7", wh::Glyph::Bow, false},
        {"8", wh::Glyph::Sword, false},   {"9", wh::Glyph::Fire, false},
        {"0", wh::Glyph::Frost, false},   {"M2", wh::Glyph::Bow, true},
    };
    Element rail = stack().left(wh::kSlotsX).top(wh::kSlotsY)
                       .width(Dim(wh::kSlotsW)).height(Dim(wh::kSlotFrame));
    rail.child(instances(slotAtlas, slotPool));
    for (int i = 0; i < wh::kSlotCount; ++i) {
      const float x = i * (wh::kSlotFrame + wh::kSlotGap);
      if (kSlots[i].filled)
        rail.child(box().left(x + 9).top(wh::kSlotsY * 0 + 9)
                       .width(Dim(24.0f)).height(Dim(24.0f))
                       .outline(wh::glyphPath(kSlots[i].glyph))
                       .fill(Material::linear(
                           {0, 0}, {0, 24},
                           {{0.0f, wh::kBoneHi}, {1.0f, wh::kBone}}))
                       // several glyphs are line-only (frost, dash, bow):
                       // a fill alone leaves them invisible
                       .stroke(util::stroke(2.2f,
                                            Fill::color(wh::kBoneHi)))
                       .stroke(util::stroke(
                           3.4f, Fill::color({0.04f, 0.03f, 0.02f, 0.75f}),
                           PathFormat::Align::Outer)));
      // four of them are cooling down: the sweep Veloren draws as a dark
      // wipe over the icon
      if (i >= 1 && i <= 4)
        rail.child(box().left(x + 3).top(3)
                       .width(Dim(wh::kSlot - 4)).height(Dim(wh::kSlot - 4))
                       .transformOrigin(0.5f, 0.0f)
                       .scaleY(&cooldown[(size_t)i - 1])
                       .fill(Material::linear(
                           {0, 0}, {0, wh::kSlot - 4},
                           {{0.0f, {0.06f, 0.10f, 0.16f, 0.86f}},
                            {1.0f, {0.10f, 0.16f, 0.24f, 0.72f}}})));
      rail.child(text(toU8(kSlots[i].key), wh::type(9, wh::kInkDim, 0.6f))
                     .left(x + 4).top(wh::kSlotFrame - 13));
    }
    // the selected-exp chip skillbar.rs hangs off slot10
    rail.child(box().left(wh::kSlotsW + 3).top(2)
                   .width(Dim(34.0f)).height(Dim(38.0f))
                   .child(worldhud::boneFrame(34, 38, 3).inset(0))
                   .child(box().left(3).top(20)
                              .width(Dim(28.0f)).height(Dim(6.0f))
                              .fill(Material::solid(worldhud::kTrack))
                              .child(box().left(0).top(0)
                                         .width(Dim(28.0f)).height(Dim(6.0f))
                                         .transformOrigin(0.0f, 0.5f)
                                         .scaleX(&xp)
                                         .fill(Material::solid(
                                             worldhud::kXp))))
                   .child(text(toU8("34"), wh::type(13, wh::kInk, 0.4f, 640))
                              .left(9).top(3)));
    return rail;
  }

  /** The minimap: generated terrain under a bone ring, with a compass
   *  rose that counter-rotates and player/POI markers. */
  Element minimap() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    constexpr float d = 168;
    return stack().key("minimap").right(28).top(28)
        .width(Dim(d)).height(Dim(d))
        .opacity(withFrom(0.0f, 1.0f, {420ms}))
        .child(box().inset(0).corners({d * 0.5f}).clip()
                   .fill(Material::solid(worldhud::C(0x2E4A2A)))
                   .child(box().inset(0)
                              .fill(patterns::noise(0.014f, 5, 3.0f))
                              .opacity(0.85f)
                              .blend(SkBlendMode::kMultiply))
                   .child(box().inset(0)
                              .fill(Material::radial(
                                  {d * 0.5f, d * 0.5f}, d * 0.55f,
                                  {{0.0f, {0, 0, 0, 0}},
                                   {0.72f, {0, 0, 0, 0.25f}},
                                   {1.0f, {0, 0, 0, 0.75f}}})))
                   // the rivers Veloren's world always has
                   .child(box().inset(0)
                              .fill(patterns::stripes(2, 47,
                                                      worldhud::C(0x2F6FA8,
                                                                  0.30f))
                                        .material())
                              .rotate(24.0f)
                              .opacity(0.7f)))
        // compass rose, counter-rotating under the frame
        .child(box().inset(0).rotate(&compass)
                   .child(box().inset(0)
                              .outline(shapes::star(4, 0.12f))
                              .fill(Material::solid(
                                  {wh::kBoneHi.fR, wh::kBoneHi.fG,
                                   wh::kBoneHi.fB, 0.22f}))))
        .child(box().left(d * 0.5f - 4).top(d * 0.5f - 4)
                   .width(Dim(8.0f)).height(Dim(8.0f))
                   .outline(shapes::polygon(3))
                   .fill(Material::solid(worldhud::C(0xFFE9A8))))
        .child(box().left(d * 0.30f).top(d * 0.36f)
                   .width(Dim(6.0f)).height(Dim(6.0f)).corners({3})
                   .fill(Material::solid(wh::kQualityLegendary)))
        .child(box().left(d * 0.68f).top(d * 0.62f)
                   .width(Dim(6.0f)).height(Dim(6.0f)).corners({3})
                   .fill(Material::solid(wh::kEnemyHp)))
        // the ring
        .child(box().inset(0).corners({d * 0.5f})
                   .foreground(util::stroke(
                       5.0f,
                       util::linearGradient({0, 0}, {0, d},
                                            {wh::kBoneHi, wh::kBone,
                                             wh::kBoneLo}),
                       PathFormat::Align::Inner))
                   .foreground(util::stroke(
                       1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f}))))
        .child(text(toU8("N"), wh::type(11, wh::kInk, 1.0f, 640))
                   .left(d * 0.5f - 4).top(7))
        .child(box().row().left(0).right(0).bottom(-19)
                   .justify(Justify::Center)
                   .child(text(toU8("WELDRIN VALE  \xc2\xb7  1204, -388"),
                               wh::type(10, wh::kInkDim, 1.2f))));
  }

  /** Buff and debuff pips with their drain rings — buffs.rs colours. */
  Element buffRow() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Pip { const char *label; SkColor4f color; float left; };
    static const Pip kPips[] = {
        {"REG", wh::kBuff, 0.72f},   {"HST", wh::kBuff, 0.35f},
        {"PRT", wh::kBuff, 0.88f},   {"BRN", wh::kDebuff, 0.51f},
        {"BLD", wh::kDebuff, 0.19f},
    };
    Element row = box().key("buffs").row().gap(6)
                      .left(28).top(28).zIndex(6)
                      .staggerChildren(70ms);
    for (const Pip &p : kPips)
      row.child(box().width(Dim(30.0f)).height(Dim(30.0f)).corners({4})
                    .opacity(withFrom(0.0f, 1.0f, {320ms}))
                    .translateY(withFrom(-10.0f, 0.0f, {380ms}))
                    .fill(Material::linear({0, 0}, {0, 30},
                                           {{0.0f, worldhud::C(0x2A2118)},
                                            {1.0f, worldhud::C(0x120C08)}}))
                    .foreground(util::stroke(
                        1.4f, Fill::color({p.color.fR, p.color.fG, p.color.fB,
                                           0.9f})))
                    .alignItems(Align::Center).justify(Justify::Center)
                    // the drain: a dark wipe from the bottom, under the label
                    .child(box().left(0).bottom(0)
                               .width(Dim(30.0f))
                               .height(Dim(30.0f * (1.0f - p.left)))
                               .fill(Material::solid({0, 0, 0, 0.62f}))
                               .zIndex(1))
                    .child(text(toU8(p.label),
                                wh::type(9, p.color, 0.6f, 640))
                               .zIndex(2)));
    return row;
  }

  /** The loot scroller: recent pickups, each in its quality colour. */
  Element lootFeed() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    struct Line { const char *text; SkColor4f color; };
    static const Line kLines[] = {
        {"Sunsteel Greatsword", wh::kQualityLegendary},
        {"Cave Spider Silk x4", wh::kQualityLow},
        {"Velorite Fragment x2", wh::kQualityHigh},
        {"Rugged Hide", wh::kQualityModerate},
        {"Glowing Remains", wh::kQualityEpic},
    };
    Element feed = box().key("loot").column().gap(3)
                       .left(28).bottom(70).zIndex(6)
                       .staggerChildren(90ms);
    for (const Line &l : kLines)
      feed.child(box().row().alignItems(Align::Center).gap(7)
                     .opacity(withFrom(0.0f, 1.0f, {420ms}))
                     .translateX(withFrom(-24.0f, 0.0f, {480ms}))
                     .child(box().width(Dim(16.0f)).height(Dim(16.0f))
                                .corners({2})
                                .fill(Material::solid({l.color.fR * 0.28f,
                                                       l.color.fG * 0.28f,
                                                       l.color.fB * 0.28f, 1}))
                                .foreground(util::stroke(
                                    1.0f, Fill::color(l.color))))
                     .child(text(toU8(l.text), wh::type(11, l.color, 0.4f))));
    return feed;
  }

  /** The enemy nameplate: ENEMY_HP_COLOR over a black track. */
  Element targetPlate() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;
    return box().key("target").column().alignItems(Align::Center)
        .left(0).right(0).top(96).zIndex(6)
        .opacity(withFrom(0.0f, 1.0f, {360ms, &choreograph::easeOutQuad, 220ms}))
        .child(text(toU8("CAVE TROLL"), wh::type(15, wh::kInk, 1.6f, 640)))
        .child(text(toU8("Lv 27"), wh::type(10, wh::kInkDim, 1.4f))
                   .margin(0, 2, 0, 4))
        .child(box().width(Dim(168.0f)).height(Dim(9.0f))
                   .fill(Material::solid(worldhud::kTrack))
                   .foreground(util::stroke(
                       1.0f, Fill::color({0.05f, 0.04f, 0.03f, 0.9f})))
                   .child(box().left(1).top(1)
                              .width(Dim(166.0f)).height(Dim(7.0f))
                              .transformOrigin(0.0f, 0.5f)
                              .scaleX(&enemyHp)
                              .fill(Material::solid(wh::kEnemyHp))));
  }

  Element describe() {
    namespace wh = worldhud;
    using namespace std::chrono_literals;

    auto root = stack().fill(Material::linear(
        {0, 0}, {0, wh::kH},
        {{0.0f, worldhud::C(0x1B2B36)},
         {0.55f, worldhud::C(0x24331F)},
         {1.0f, worldhud::C(0x11170E)}}));

    // a coarse world under the HUD so the bars have something to be legible
    // against — the HUD is the subject, but a HUD over nothing is a lie
    root.child(box().inset(0)
                   .fill(patterns::noise(0.006f, 6, 12.0f))
                   .opacity(0.55f)
                   .blend(SkBlendMode::kOverlay));
    // a horizon and two ridgelines, so the HUD reads as an overlay on a
    // world rather than as a diagram floating in a void
    auto ridge = [&](float baseY, float amp, float freq, float phase,
                     SkColor4f color) {
      return box().inset(0)
          .outline([baseY, amp, freq, phase](SkSize s) {
            SkPathBuilder b;
            b.moveTo(0, s.height());
            b.lineTo(0, baseY);
            for (float x = 0; x <= s.width(); x += 6)
              b.lineTo(x, baseY + amp * std::sin(x * freq + phase) +
                              amp * 0.4f * std::sin(x * freq * 2.7f));
            b.lineTo(s.width(), s.height());
            b.close();
            return b.detach();
          })
          .fill(Material::solid(color));
    };
    root.child(ridge(268, 26, 0.0085f, 0.4f, worldhud::C(0x15201A)));
    root.child(ridge(322, 18, 0.0135f, 2.1f, worldhud::C(0x0E1712)));
    root.child(ridge(392, 12, 0.0210f, 4.4f, worldhud::C(0x080D09)));
    root.child(box().inset(0)
                   .fill(Material::radial({wh::kW * 0.5f, wh::kH * 0.42f},
                                          wh::kW * 0.72f,
                                          {{0.0f, {0, 0, 0, 0}},
                                           {1.0f, {0, 0, 0, 0.65f}}})));

    root.child(box().column().left(28).top(70).zIndex(6)
                   .child(text(toU8("WELDRIN VALE"),
                               wh::type(20, wh::kInk, 2.6f, 640)))
                   .child(text(toU8("action-RPG HUD \xe2\x80\x94 every "
                                    "dimension from voxygen/src/hud"),
                               wh::type(11, wh::kInkDim, 0.9f))
                              .margin(0, 5, 0, 0)));

    root.child(buffRow());
    root.child(minimap());
    root.child(targetPlate());
    root.child(lootFeed());
    root.child(barStack());
    root.child(hotbar());
    return root;
  }
};

} // namespace compose_gallery
