#pragma once
// The grid-inventory study: Diablo II's 10x4 hoard, its equipment
// paperdoll, its rarity ladder, and the drag-validity feedback that
// FarrokhGames/Inventory (a D2-style grid inventory for Unity) reduces to
// three cell sprites — empty, selected, blocked.
//
// Why this scene exists: an inventory is the densest MATERIAL exercise in
// game UI. Nothing here is an image. Every surface is generated:
//
//   panel stone ......... slate ramp under patterns::noise, inside a
//                         bronze bevel with corner rivets — the frame is
//                         BevelEmboss + two keylines, not a nine-slice
//   slot wells .......... 40 identical cells through instances(): ONE
//                         atlas cell baked from an element tree, one
//                         drawAtlas stamp for the whole grid. The well
//                         look is a two-stop ramp under an inner shadow,
//                         which is what makes a rectangle read as a HOLE
//   equipment sockets ... the same well at ten D2 footprints (helm 2x2,
//                         armour 2x3, weapon 2x4, belt 2x1, rings 1x1...)
//                         with the silhouette of what belongs in it
//                         ghosted inside, the way an empty slot hints
//   item art ............ procedural silhouettes — every blade, flask and
//                         gauntlet is a path under a steel or glass ramp
//   the rarity ladder ... D2's item colours (white normal, #6969FF magic,
//                         #FFFF64 rare, #C7B377 unique, #00FF00 set) tint
//                         the cell behind the item, the name in the
//                         tooltip, and the glow uniques and sets carry
//   drag validity ....... the held item's footprint highlights GREEN over
//                         free cells and RED where it overlaps, which is
//                         FarrokhGames' canAdd -> blocked sprite, live
//   shimmer ............. uniques catch a gloss band that sweeps them
//
// Reference note: the geometry is Diablo II's (10x4 grid, 29 px cells at
// 640x480 — 38 px here), the colours are its documented item-quality
// codes. Names, art and copy are ours.

#include "GalleryCore.h"

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace compose_gallery {

namespace loot {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

/** D2's cell is 29 px at 640x480; this stage is wider, so the grid keeps
 *  the 10x4 shape at 38 px and everything else measures in cells. */
constexpr float kCell = 38, kGap = 2;
constexpr int kCols = 10, kRows = 4;
constexpr float kGridW = kCols * kCell + (kCols - 1) * kGap;
constexpr float kGridH = kRows * kCell + (kRows - 1) * kGap;

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

// The panel: cold slate under bronze.
constexpr SkColor4f kStoneHi = C(0x2A2723);
constexpr SkColor4f kStoneLo = C(0x14120F);
constexpr SkColor4f kWellHi = C(0x1C1A17);
constexpr SkColor4f kWellLo = C(0x0B0A08);
constexpr SkColor4f kBronze = C(0x8C7247);
constexpr SkColor4f kBronzeLit = C(0xC9A96A);
constexpr SkColor4f kBronzeDim = C(0x4A3B23);
constexpr SkColor4f kParch = C(0xC8BC9A);
constexpr SkColor4f kAsh = C(0x7A705C);

/** Diablo II's item-quality colours. */
enum class Rarity { Normal, Magic, Rare, Unique, Set };
inline SkColor4f rarityColor(Rarity r) {
  switch (r) {
  case Rarity::Magic:  return C(0x6969FF);
  case Rarity::Rare:   return C(0xFFFF64);
  case Rarity::Unique: return C(0xC7B377);
  case Rarity::Set:    return C(0x00FF00);
  case Rarity::Normal: break;
  }
  return C(0xE8E4DC);
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (weight > 0)
    s.shaping.variations = {sigil::weave::FontVariation("wght", weight)};
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// ---------------------------------------------------------------------------
// item silhouettes — every one a path, none an image

enum class Art { Sword, Armour, Wand, Helm, Gloves, Ring, Amulet, Potion,
                 Shield, Boots, Belt, Bow };

/** Normalised item silhouette: the path is authored in the box it is
 *  handed, so one function serves the grid cell and the tooltip swatch. */
inline std::function<SkPath(SkSize)> artPath(Art art) {
  return [art](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    auto rect = [&](float x, float y, float rw, float rh) {
      b.addRect(SkRect::MakeXYWH(x, y, rw, rh));
    };
    switch (art) {
    case Art::Sword: {
      const float cx = w * 0.5f;
      b.moveTo(cx, h * 0.03f);                       // point
      b.lineTo(cx + w * 0.15f, h * 0.14f);
      b.lineTo(cx + w * 0.13f, h * 0.62f);           // blade
      b.lineTo(cx - w * 0.13f, h * 0.62f);
      b.lineTo(cx - w * 0.15f, h * 0.14f);
      b.close();
      rect(w * 0.10f, h * 0.62f, w * 0.80f, h * 0.055f);   // crossguard
      rect(cx - w * 0.055f, h * 0.675f, w * 0.11f, h * 0.22f); // grip
      b.addCircle(cx, h * 0.925f, w * 0.10f);              // pommel
      break;
    }
    case Art::Armour: {
      b.moveTo(w * 0.30f, h * 0.10f);
      b.lineTo(w * 0.70f, h * 0.10f);
      b.lineTo(w * 0.92f, h * 0.26f);
      b.lineTo(w * 0.84f, h * 0.42f);
      b.lineTo(w * 0.82f, h * 0.90f);
      b.lineTo(w * 0.18f, h * 0.90f);
      b.lineTo(w * 0.16f, h * 0.42f);
      b.lineTo(w * 0.08f, h * 0.26f);
      b.close();
      b.moveTo(w * 0.42f, h * 0.10f);                // collar notch
      b.lineTo(w * 0.50f, h * 0.24f);
      b.lineTo(w * 0.58f, h * 0.10f);
      b.close();
      break;
    }
    case Art::Wand: {
      const float cx = w * 0.5f;
      rect(cx - w * 0.07f, h * 0.20f, w * 0.14f, h * 0.72f);
      b.addCircle(cx, h * 0.16f, w * 0.20f);
      break;
    }
    case Art::Helm: {
      b.addArc(SkRect::MakeXYWH(w * 0.14f, h * 0.16f, w * 0.72f, h * 0.72f),
               180, 180);
      b.lineTo(w * 0.86f, h * 0.74f);
      b.lineTo(w * 0.14f, h * 0.74f);
      b.close();
      rect(w * 0.06f, h * 0.70f, w * 0.88f, h * 0.09f); // brim
      rect(w * 0.30f, h * 0.46f, w * 0.40f, h * 0.07f); // eye slit
      break;
    }
    case Art::Gloves: {
      b.moveTo(w * 0.26f, h * 0.86f);
      b.lineTo(w * 0.26f, h * 0.40f);
      b.quadTo(w * 0.26f, h * 0.20f, w * 0.42f, h * 0.20f);
      b.quadTo(w * 0.56f, h * 0.20f, w * 0.56f, h * 0.40f);
      b.lineTo(w * 0.56f, h * 0.46f);
      b.lineTo(w * 0.72f, h * 0.36f);
      b.quadTo(w * 0.82f, h * 0.32f, w * 0.82f, h * 0.48f);
      b.lineTo(w * 0.74f, h * 0.86f);
      b.close();
      break;
    }
    case Art::Ring: {
      b.addCircle(w * 0.5f, h * 0.56f, w * 0.30f);
      b.addCircle(w * 0.5f, h * 0.56f, w * 0.17f);
      b.moveTo(w * 0.5f, h * 0.10f);                 // stone
      b.lineTo(w * 0.64f, h * 0.24f);
      b.lineTo(w * 0.5f, h * 0.38f);
      b.lineTo(w * 0.36f, h * 0.24f);
      b.close();
      break;
    }
    case Art::Amulet: {
      b.addArc(SkRect::MakeXYWH(w * 0.22f, h * 0.10f, w * 0.56f, h * 0.56f),
               200, 140);
      b.moveTo(w * 0.5f, h * 0.52f);
      b.lineTo(w * 0.68f, h * 0.72f);
      b.lineTo(w * 0.5f, h * 0.92f);
      b.lineTo(w * 0.32f, h * 0.72f);
      b.close();
      break;
    }
    case Art::Potion: {
      rect(w * 0.42f, h * 0.10f, w * 0.16f, h * 0.16f); // neck
      b.moveTo(w * 0.36f, h * 0.26f);
      b.quadTo(w * 0.18f, h * 0.46f, w * 0.22f, h * 0.72f);
      b.quadTo(w * 0.26f, h * 0.92f, w * 0.50f, h * 0.92f);
      b.quadTo(w * 0.74f, h * 0.92f, w * 0.78f, h * 0.72f);
      b.quadTo(w * 0.82f, h * 0.46f, w * 0.64f, h * 0.26f);
      b.close();
      break;
    }
    case Art::Shield: {
      b.moveTo(w * 0.12f, h * 0.10f);
      b.lineTo(w * 0.88f, h * 0.10f);
      b.lineTo(w * 0.88f, h * 0.52f);
      b.quadTo(w * 0.88f, h * 0.84f, w * 0.50f, h * 0.94f);
      b.quadTo(w * 0.12f, h * 0.84f, w * 0.12f, h * 0.52f);
      b.close();
      b.addCircle(w * 0.5f, h * 0.44f, w * 0.16f);   // boss
      break;
    }
    case Art::Boots: {
      b.moveTo(w * 0.30f, h * 0.14f);
      b.lineTo(w * 0.58f, h * 0.14f);
      b.lineTo(w * 0.58f, h * 0.62f);
      b.lineTo(w * 0.86f, h * 0.74f);
      b.lineTo(w * 0.86f, h * 0.88f);
      b.lineTo(w * 0.30f, h * 0.88f);
      b.close();
      break;
    }
    case Art::Belt: {
      rect(w * 0.04f, h * 0.30f, w * 0.92f, h * 0.40f);
      rect(w * 0.40f, h * 0.16f, w * 0.20f, h * 0.68f);
      break;
    }
    case Art::Bow: {
      b.addArc(SkRect::MakeXYWH(w * 0.22f, h * 0.06f, w * 0.72f, h * 0.88f),
               110, 140);
      b.moveTo(w * 0.34f, h * 0.10f);
      b.lineTo(w * 0.34f, h * 0.90f);
      break;
    }
    }
    return b.detach();
  };
}

/** One item's art, filled with a steel or tinted ramp and outlined. */
inline Element artwork(Art art, float w, float h, SkColor4f tint,
                       bool glassy = false) {
  const SkColor4f hi = glassy
                           ? SkColor4f{tint.fR, tint.fG, tint.fB, 0.95f}
                           : SkColor4f{std::min(1.0f, tint.fR * 1.55f + 0.20f),
                                       std::min(1.0f, tint.fG * 1.55f + 0.20f),
                                       std::min(1.0f, tint.fB * 1.55f + 0.20f),
                                       1};
  const SkColor4f lo{tint.fR * 0.32f, tint.fG * 0.32f, tint.fB * 0.32f, 1};
  return box().width(Dim(w)).height(Dim(h))
      .outline(artPath(art))
      .fill(Material::linear({0, 0}, {w * 0.35f, h},
                             {{0.0f, hi}, {0.55f, tint}, {1.0f, lo}}))
      .stroke(util::stroke(1.1f, Fill::color({0.03f, 0.03f, 0.03f, 0.85f})));
}

// ---------------------------------------------------------------------------
// the grid model

struct Item {
  const char *name;
  Art art;
  int col, row, w, h;   // cell footprint
  Rarity rarity;
  SkColor4f tint;       // the metal/glass the art is made of
};

/** The hoard. Footprints are D2's: two-hand sword 2x4, body armour 2x3,
 *  wand 1x3, helm 2x2, gloves 2x2, rings and potions 1x1. */
inline constexpr Item kItems[] = {
    {"Doomslinger", Art::Sword, 0, 0, 2, 4, Rarity::Unique, C(0xB9A06A)},
    {"Sigil Plate", Art::Armour, 2, 0, 2, 3, Rarity::Set, C(0x6E8F63)},
    {"Ashen Wand", Art::Wand, 4, 0, 1, 3, Rarity::Magic, C(0x6F79C4)},
    {"Grave Helm", Art::Helm, 5, 0, 2, 2, Rarity::Rare, C(0x9AA0A6)},
    {"Cinder Grips", Art::Gloves, 5, 2, 2, 2, Rarity::Normal, C(0x8A6A46)},
    {"Band of Soot", Art::Ring, 7, 0, 1, 1, Rarity::Rare, C(0xC7A657)},
    {"Ember Charm", Art::Amulet, 8, 0, 1, 1, Rarity::Unique, C(0xC7B377)},
    {"Healing Draught", Art::Potion, 7, 1, 1, 1, Rarity::Normal, C(0xB03A3A)},
    {"Healing Draught", Art::Potion, 8, 1, 1, 1, Rarity::Normal, C(0xB03A3A)},
    {"Mana Draught", Art::Potion, 9, 1, 1, 1, Rarity::Normal, C(0x3A56B0)},
    {"Mana Draught", Art::Potion, 9, 2, 1, 1, Rarity::Normal, C(0x3A56B0)},
    {"Warden's Sabatons", Art::Boots, 2, 3, 2, 1, Rarity::Magic, C(0x7A6A55)},
};
inline constexpr int kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

/** The equipment paperdoll: D2's ten sockets, at D2's footprints. */
struct Socket {
  const char *label;
  Art ghost;
  float x, y;      // panel-local px
  int w, h;        // cell footprint
  const Item *worn;
};

// ---------------------------------------------------------------------------

inline float cellX(int c) { return c * (kCell + kGap); }
inline float cellY(int r) { return r * (kCell + kGap); }
inline float spanW(int cells) { return cells * kCell + (cells - 1) * kGap; }

/** A well: the two-stop ramp under an inner shadow that makes a rectangle
 *  read as a hole punched in the panel. */
inline Element well(float w, float h, float alpha = 1.0f) {
  return box().width(Dim(w)).height(Dim(h)).corners({2})
      .fill(Material::linear({0, 0}, {0, h},
                             {{0.0f, {kWellLo.fR, kWellLo.fG, kWellLo.fB,
                                      alpha}},
                              {1.0f, {kWellHi.fR, kWellHi.fG, kWellHi.fB,
                                      alpha}}}))
      .foreground(styles::InnerShadow{{0, 0, 0, 0.75f}, {0, 2}, 3})
      .foreground(shapes::onEdges(
          shapes::Edge::Bottom,
          util::stroke(1.0f, Fill::color({0.42f, 0.38f, 0.31f, 0.30f}))));
}

/** The bronze-framed stone panel every part of this UI sits in. */
inline Element panel(float w, float h) {
  return box().width(Dim(w)).height(Dim(h)).corners({4})
      .background(styles::dropShadow({0, 0, 0, 0.65f}, {0, 7}, 16))
      .fill(Material::linear({0, 0}, {0, h},
                             {{0.0f, kStoneHi}, {1.0f, kStoneLo}}))
      .clip()
      // quarried, not smooth: the grain is generated, never a texture file
      .child(box().absolute().inset(0)
                 .fill(patterns::noise(0.06f, 4, 7.0f))
                 .opacity(0.16f)
                 .blend(SkBlendMode::kOverlay))
      .foreground(styles::BevelEmboss{2.5f, 4.0f, 120,
                                      {kBronzeLit.fR, kBronzeLit.fG,
                                       kBronzeLit.fB, 0.35f},
                                      {0, 0, 0, 0.7f}})
      .foreground(util::stroke(2.0f, Fill::color(kBronze)))
      .foreground(util::stroke(1.0f, Fill::color(kBronzeDim),
                              PathFormat::Align::Inner));
}

/** Four rivets, one per corner of a panel. */
inline Element rivets(float w, float h, float inset = 11) {
  auto stud = [](float x, float y) {
    return box().width(Dim(6.0f)).height(Dim(6.0f)).corners({3})
        .absolute().left(x - 3).top(y - 3)
        .fill(Material::radial({3, 3}, 3.4f,
                               {{0.0f, kBronzeLit},
                                {0.7f, kBronze},
                                {1.0f, kBronzeDim}}));
  };
  return stack().absolute().inset(0)
      .child(stud(inset, inset))
      .child(stud(w - inset, inset))
      .child(stud(inset, h - inset))
      .child(stud(w - inset, h - inset));
}

} // namespace loot

struct LootGridScene final : Scene {
  // The held item's travel, the validity flip, the unique shimmer, and the
  // gold counter's tick.
  choreograph::Output<float> dragX{0}, dragY{0};
  choreograph::Output<float> blockedMix{0}, fitsMix{0};
  choreograph::Output<float> shimmer{0};
  choreograph::Output<float> goldFrac{0};
  int gold = 0;

  std::shared_ptr<instancing::Atlas> cellAtlas;
  std::shared_ptr<instancing::Pool> cellPool;

  int shownGold = -1;

  const char *name() const override { return "loot grid"; }

  /** The counter ticks, so the tree re-renders — but only on the frames
   *  where the integer actually changed. That is the reconciler's job and
   *  this is the cheapest honest way to exercise it. */
  void update(double, Composer &composer) override {
    if (gold != shownGold) {
      shownGold = gold;
      composer.render(describe());
    }
  }

  // Where the dragged shield rests: over the wand+gloves (blocked), then
  // over the empty right-hand column (free).
  static constexpr int kDragW = 2, kDragH = 3;
  static SkPoint blockedAt() {
    return {loot::cellX(4), loot::cellY(1)};
  }
  static SkPoint freeAt() { return {loot::cellX(2) + 4, loot::cellY(4) + 8}; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace lt = loot;
    dragX = blockedAt().x();
    dragY = blockedAt().y();
    blockedMix = 1;
    fitsMix = 0;
    shimmer = 0;
    goldFrac = 0;
    gold = 0;

    // One atlas cell, forty instances: the empty grid is a single
    // drawAtlas stamp, which is the whole point of the flyweight layer.
    cellAtlas = std::make_shared<instancing::Atlas>(2.0f);
    cellAtlas->cell(lt::well(lt::kCell, lt::kCell),
                    {lt::kCell, lt::kCell});
    cellPool = std::make_shared<instancing::Pool>();
    for (int r = 0; r < lt::kRows; ++r)
      for (int c = 0; c < lt::kCols; ++c)
        cellPool->add({lt::cellX(c) + lt::kCell * 0.5f,
                       lt::cellY(r) + lt::kCell * 0.5f});

    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // 4.4 s round trip: rest blocked, slide, rest free, slide back.
      const double cycle = std::fmod(t, 4.4);
      double u = 0;      // 0 = blocked slot, 1 = free slot
      if (cycle < 1.4)
        u = 0;
      else if (cycle < 2.2)
        u = (cycle - 1.4) / 0.8;
      else if (cycle < 3.6)
        u = 1;
      else
        u = 1.0 - (cycle - 3.6) / 0.8;
      const float ease =
          (float)(u * u * (3.0 - 2.0 * u)); // smoothstep, the UI standard
      const SkPoint a = blockedAt(), b = freeAt();
      dragX = a.x() + (b.x() - a.x()) * ease;
      dragY = a.y() + (b.y() - a.y()) * ease;
      blockedMix = 1.0f - ease;
      fitsMix = ease;
      // pixels, not a fraction: translateX moves by the bound value
      shimmer = -70.0f + 240.0f * (float)std::fmod(t * 0.42, 1.0);
      const float g = (float)std::min(1.0, t / 1.6);
      goldFrac = g;
      gold = (int)(214860 * g);
      return true;
    });

    composer.render(describe());
  }

  // ------------------------------------------------------------------

  Element gridPanel() {
    namespace lt = loot;
    constexpr float pad = 15;
    Element grid = stack().width(Dim(lt::kGridW)).height(Dim(lt::kGridH))
                       .absolute().left(pad).top(pad + 26);

    // the empty wells — forty cells, one stamp
    grid.child(instances(cellAtlas, cellPool));

    // the items, each on its rarity-tinted cell
    for (int i = 0; i < lt::kItemCount; ++i) {
      const lt::Item &item = lt::kItems[i];
      const float w = lt::spanW(item.w), h = lt::spanW(item.h);
      const SkColor4f rc = lt::rarityColor(item.rarity);
      const bool lit = item.rarity == lt::Rarity::Unique ||
                       item.rarity == lt::Rarity::Set;
      Element cell =
          box().width(Dim(w)).height(Dim(h)).corners({2})
              .absolute().left(lt::cellX(item.col)).top(lt::cellY(item.row))
              .fill(Material::linear(
                  {0, 0}, {0, h},
                  {{0.0f, {rc.fR * 0.30f, rc.fG * 0.30f, rc.fB * 0.30f, 0.85f}},
                   {1.0f, {rc.fR * 0.13f, rc.fG * 0.13f, rc.fB * 0.13f,
                           0.85f}}}))
              .foreground(util::stroke(
                  1.0f, Fill::color({rc.fR, rc.fG, rc.fB, lit ? 0.7f : 0.3f})))
              .row().justify(Justify::Center).alignItems(Align::Center)
              .child(lt::artwork(item.art, w * 0.76f, h * 0.80f, item.tint,
                                 item.art == lt::Art::Potion));
      if (lit)
        cell.background(styles::dropShadow({rc.fR, rc.fG, rc.fB, 0.35f},
                                           {0, 0}, 9));
      // uniques catch a light band that sweeps them
      if (item.rarity == lt::Rarity::Unique)
        cell.child(box().absolute().inset(1).clip()
                       .child(box().width(Dim(w * 0.30f)).height(Dim(h * 1.8f))
                                  .absolute()
                                  .left(-w * 0.4f).top(-h * 0.4f)
                                  .translateX(&shimmer)
                                  .rotate(18.0f)
                                  .fill(Material::linear(
                                      {0, 0}, {w * 0.35f, 0},
                                      {{0.0f, {1, 1, 1, 0.0f}},
                                       {0.5f, {1, 1, 1, 0.30f}},
                                       {1.0f, {1, 1, 1, 0.0f}}}))
                                  .blend(SkBlendMode::kPlus)));
      grid.child(std::move(cell));
    }

    // the held item's footprint: green where it fits, red where it does not
    const float dw = lt::spanW(kDragW), dh = lt::spanW(kDragH);
    // FarrokhGames' two cell sprites, cross-faded: green while the
    // footprint is free, red the moment it overlaps something.
    grid.child(box().width(Dim(dw)).height(Dim(dh)).corners({2})
                   .absolute().left(0).top(0)
                   .translateX(&dragX).translateY(&dragY)
                   .fill(Material::solid({0.16f, 0.80f, 0.24f, 0.26f}))
                   .foreground(util::stroke(
                       1.4f, Fill::color({0.35f, 1.0f, 0.45f, 0.75f})))
                   .opacity(&fitsMix)
                   .zIndex(5));
    grid.child(box().width(Dim(dw)).height(Dim(dh)).corners({2})
                   .absolute().left(0).top(0)
                   .translateX(&dragX).translateY(&dragY)
                   .fill(Material::solid({0.90f, 0.16f, 0.14f, 0.30f}))
                   .foreground(util::stroke(
                       1.4f, Fill::color({1.0f, 0.35f, 0.30f, 0.8f})))
                   .opacity(&blockedMix)
                   .zIndex(6));
    // and the item riding it
    grid.child(box().width(Dim(dw)).height(Dim(dh))
                   .absolute().left(0).top(0)
                   .translateX(&dragX).translateY(&dragY)
                   .row().justify(Justify::Center).alignItems(Align::Center)
                   .zIndex(7)
                   .child(lt::artwork(lt::Art::Shield, dw * 0.78f, dh * 0.62f,
                                      lt::C(0x8895A2))));

    return stack().width(Dim(lt::kGridW + 2 * pad))
        .height(Dim(lt::kGridH + 2 * pad + 26))
        .absolute().left(452).top(96)
        .child(loot::panel(lt::kGridW + 2 * pad, lt::kGridH + 2 * pad + 26)
                   .absolute().inset(0))
        .child(loot::rivets(lt::kGridW + 2 * pad, lt::kGridH + 2 * pad + 26))
        .child(text(toU8("HOARD"), lt::type(12, lt::kBronzeLit, 4.5f, 650))
                   .absolute().left(pad).top(pad + 2))
        .child(text(toU8("10 \xc3\x97" " 4"), lt::type(11, lt::kAsh, 2.0f))
                   .absolute().right(pad).top(pad + 3))
        .child(std::move(grid));
  }

  Element paperdoll() {
    namespace lt = loot;
    constexpr float pw = 396, ph = 500, pad = 16;
    struct Slot {
      const char *label;
      lt::Art ghost;
      float x, y;
      int w, h;
    };
    // D2's footprints: helm 2x2, amulet 1x1, armour 2x3, weapon 2x4,
    // shield 2x4, belt 2x1, rings 1x1, gloves 2x2, boots 2x2.
    static const Slot kSlots[] = {
        {"HELM", lt::Art::Helm, 160, 0, 2, 2},
        {"AMULET", lt::Art::Amulet, 262, 20, 1, 1},
        {"WEAPON", lt::Art::Sword, 12, 92, 2, 4},
        {"ARMOUR", lt::Art::Armour, 160, 92, 2, 3},
        {"SHIELD", lt::Art::Shield, 284, 92, 2, 4},
        {"RING", lt::Art::Ring, 112, 216, 1, 1},
        {"BELT", lt::Art::Belt, 160, 216, 2, 1},
        {"RING", lt::Art::Ring, 262, 216, 1, 1},
        {"GLOVES", lt::Art::Gloves, 12, 262, 2, 2},
        {"BOOTS", lt::Art::Boots, 284, 262, 2, 2},
    };
    // Which sockets are filled, and with what.
    struct Worn { int slot; lt::Rarity rarity; SkColor4f tint; };
    static const Worn kWorn[] = {
        {0, lt::Rarity::Rare, lt::C(0x9AA0A6)},
        {2, lt::Rarity::Unique, lt::C(0xB9A06A)},
        {3, lt::Rarity::Set, lt::C(0x6E8F63)},
        {6, lt::Rarity::Magic, lt::C(0x7A6A55)},
        {9, lt::Rarity::Normal, lt::C(0x7A6A55)},
    };

    Element body = stack().absolute().inset(0);
    for (int i = 0; i < (int)(sizeof(kSlots) / sizeof(kSlots[0])); ++i) {
      const Slot &s = kSlots[i];
      const float w = lt::spanW(s.w), h = lt::spanW(s.h);
      const Worn *equipped = nullptr;
      for (const Worn &candidate : kWorn)
        if (candidate.slot == i)
          equipped = &candidate;

      Element socket =
          stack().width(Dim(w)).height(Dim(h))
              .absolute().left(pad + s.x).top(pad + 22 + s.y)
              .child(lt::well(w, h).absolute().inset(0));
      if (equipped) {
        const SkColor4f rc = lt::rarityColor(equipped->rarity);
        socket.child(box().absolute().inset(2).corners({2})
                         .fill(Material::linear(
                             {0, 0}, {0, h},
                             {{0.0f, {rc.fR * 0.28f, rc.fG * 0.28f,
                                      rc.fB * 0.28f, 0.9f}},
                              {1.0f, {rc.fR * 0.12f, rc.fG * 0.12f,
                                      rc.fB * 0.12f, 0.9f}}}))
                         .foreground(util::stroke(
                             1.0f,
                             Fill::color({rc.fR, rc.fG, rc.fB, 0.45f}))));
        socket.child(box().absolute().inset(0)
                         .row().justify(Justify::Center)
                         .alignItems(Align::Center)
                         .child(lt::artwork(s.ghost, w * 0.72f, h * 0.76f,
                                            equipped->tint)));
      } else {
        // empty sockets hint at what belongs in them
        socket.child(box().absolute().inset(0)
                         .row().justify(Justify::Center)
                         .alignItems(Align::Center)
                         .opacity(0.13f)
                         .child(lt::artwork(s.ghost, w * 0.64f, h * 0.68f,
                                            lt::kParch)));
        socket.child(text(toU8(s.label), lt::type(7.0f, lt::kAsh, 1.3f))
                         .absolute().left(0).right(0).bottom(3)
                         .textAlign(sigil::weave::TextAlignment::kCenter));
      }
      body.child(std::move(socket));
    }

    // the stat block D2 puts under the paperdoll: two columns of
    // label-dots-value rows, laid out rather than absolutely stacked
    auto statRow = [&](const char *label, const char *value,
                       SkColor4f valueColor) {
      return box().row().width(Dim(166.0f)).alignItems(Align::Center)
          .child(text(toU8(label), lt::type(10.5f, lt::kAsh, 1.1f)))
          .child(box().grow(1).height(Dim(1.0f)).margin(6, 0, 6, 0)
                     .fill(Material::solid({0.42f, 0.38f, 0.31f, 0.28f})))
          .child(text(toU8(value), lt::type(12, valueColor, 0.5f, 620)));
    };
    body.child(box().row().gap(20)
                   .absolute().left(pad + 10).bottom(pad + 8)
                   .child(box().column().gap(4)
                              .child(statRow("STRENGTH", "142", lt::kParch))
                              .child(statRow("DEXTERITY", "97", lt::kParch))
                              .child(statRow("VITALITY", "206",
                                             lt::rarityColor(
                                                 lt::Rarity::Magic)))
                              .child(statRow("ENERGY", "68", lt::kParch)))
                   .child(box().column().gap(4)
                              .child(statRow("DEFENCE", "1,884", lt::kParch))
                              .child(statRow("FIRE RES", "+65%",
                                             lt::C(0xE07A3C)))
                              .child(statRow("COLD RES", "+41%",
                                             lt::C(0x5AA6E0)))
                              .child(statRow("LIGHT RES",
                                             "\xe2\x88\x92" "35%",
                                             lt::C(0xD04040)))));

    return stack().width(Dim(pw)).height(Dim(ph))
        .absolute().left(30).top(96)
        .child(loot::panel(pw, ph).absolute().inset(0))
        .child(loot::rivets(pw, ph))
        .child(text(toU8("EQUIPPED"), lt::type(12, lt::kBronzeLit, 4.5f, 650))
                   .absolute().left(pad).top(pad))
        .child(std::move(body));
  }

  /** The hover tooltip, D2's stack: name in the rarity colour, base type,
   *  affix lines, then requirements — unmet ones in red. */
  Element tooltip() {
    namespace lt = loot;
    using namespace std::chrono_literals;
    const SkColor4f rc = lt::rarityColor(lt::Rarity::Unique);
    auto line = [&](const char *s, SkColor4f c) {
      return text(toU8(s), lt::type(11.5f, c, 0.2f));
    };
    return box().width(Dim(262.0f))
        .absolute().left(452).top(340)
        .column().alignItems(Align::Center).padding(14, 11).gap(2).corners({2})
        .fill(Material::solid({0.02f, 0.02f, 0.02f, 0.90f}))
        .foreground(util::stroke(1.0f, Fill::color({rc.fR, rc.fG, rc.fB,
                                                    0.45f})))
        .background(styles::dropShadow({0, 0, 0, 0.7f}, {0, 5}, 12))
        .key("tooltip")
        .opacity(withFrom(0.0f, 1.0f, {380ms}))
        .translateY(withFrom(8.0f, 0.0f, {460ms}))
        .zIndex(9)
        .child(text(toU8("Doomslinger"), lt::type(15, rc, 1.4f, 620)))
        .child(text(toU8("Colossus Blade"), lt::type(11.5f, lt::kParch, 0.8f))
                   .margin(0, 0, 0, 6))
        .child(line("189% Enhanced Damage", lt::rarityColor(lt::Rarity::Magic)))
        .child(line("+2 to Fire Skills", lt::rarityColor(lt::Rarity::Magic)))
        .child(line("Adds 40-92 Fire Damage",
                    lt::rarityColor(lt::Rarity::Magic)))
        .child(line("Ignores Target's Defence",
                    lt::rarityColor(lt::Rarity::Magic)))
        .child(box().width(Dim(180.0f)).height(Dim(1.0f)).margin(0, 7, 0, 5)
                   .fill(Material::linear(
                       {0, 0}, {180, 0},
                       {{0.0f, {rc.fR, rc.fG, rc.fB, 0.0f}},
                        {0.5f, {rc.fR, rc.fG, rc.fB, 0.5f}},
                        {1.0f, {rc.fR, rc.fG, rc.fB, 0.0f}}})))
        .child(line("Required Strength: 189", lt::kAsh))
        .child(line("Required Level: 63", lt::C(0xD04040)));
  }

  Element describe() {
    namespace lt = loot;
    char goldText[32];
    std::snprintf(goldText, sizeof(goldText), "%d", gold);

    auto root = stack().fill(Material::linear(
        {0, 0}, {0, lt::kH},
        {{0.0f, lt::C(0x0D0C0A)}, {0.5f, lt::C(0x14120F)},
         {1.0f, lt::C(0x080706)}}));

    // a faint speckled floor so the panels sit ON something
    root.child(box().absolute().inset(0)
                   .fill(patterns::speckle(140, 26, 0.4f, 1.4f,
                                           {{0.55f, 0.48f, 0.36f, 0.20f}})
                             .material())
                   .opacity(0.5f));

    root.child(box().column().absolute().left(30).top(34)
                   .child(text(toU8("HOARD OF THE HORADRIM"),
                               lt::type(23, lt::kParch, 3.4f, 640)))
                   .child(text(toU8("grid inventory \xe2\x80\x94 generated "
                                    "materials, no sprites"),
                               lt::type(12, lt::kAsh, 1.0f))
                              .margin(0, 5, 0, 0)));

    root.child(paperdoll());
    root.child(gridPanel());
    root.child(tooltip());

    // gold, on its own little plaque
    root.child(box().row().alignItems(Align::Center).gap(9)
                   .absolute().right(30).top(38)
                   .padding(13, 7).corners({3})
                   .fill(Material::linear({0, 0}, {0, 32},
                                          {{0.0f, lt::kStoneHi},
                                           {1.0f, lt::kStoneLo}}))
                   .foreground(util::stroke(1.0f, Fill::color(lt::kBronzeDim)))
                   .child(box().width(Dim(13.0f)).height(Dim(13.0f))
                              .corners({6.5f})
                              .fill(Material::radial(
                                  {5, 4}, 9,
                                  {{0.0f, lt::C(0xFFE9A8)},
                                   {0.6f, lt::C(0xD8A93C)},
                                   {1.0f, lt::C(0x7A5C15)}})))
                   .child(text(toU8(goldText),
                               lt::type(17, lt::C(0xD8B95C), 1.6f, 620)))
                   .child(text(toU8("GOLD"), lt::type(10, lt::kAsh, 2.2f))));

    // the legend for the rarity ladder, bottom right
    auto chip = [&](lt::Rarity r, const char *label) {
      const SkColor4f c = lt::rarityColor(r);
      return box().row().alignItems(Align::Center).gap(6)
          .child(box().width(Dim(9.0f)).height(Dim(9.0f)).corners({1.5f})
                     .fill(Material::solid({c.fR * 0.35f, c.fG * 0.35f,
                                            c.fB * 0.35f, 1}))
                     .foreground(util::stroke(1.0f, Fill::color(c))))
          .child(text(toU8(label), lt::type(10.5f, c, 0.8f)));
    };
    root.child(box().row().gap(15).alignItems(Align::Center)
                   .absolute().right(30).bottom(26)
                   .child(chip(lt::Rarity::Normal, "normal"))
                   .child(chip(lt::Rarity::Magic, "magic"))
                   .child(chip(lt::Rarity::Rare, "rare"))
                   .child(chip(lt::Rarity::Set, "set"))
                   .child(chip(lt::Rarity::Unique, "unique")));

    root.child(box().row().gap(8).alignItems(Align::Center)
                   .absolute().left(30).bottom(26)
                   .child(box().width(Dim(11.0f)).height(Dim(11.0f))
                              .corners({2})
                              .fill(Material::solid({0.16f, 0.80f, 0.24f,
                                                     0.30f}))
                              .foreground(util::stroke(
                                  1.0f,
                                  Fill::color({0.35f, 1.0f, 0.45f, 0.8f}))))
                   .child(text(toU8("fits"), lt::type(10.5f, lt::kAsh, 0.8f)))
                   .child(box().width(Dim(11.0f)).height(Dim(11.0f))
                              .corners({2}).margin(10, 0, 0, 0)
                              .fill(Material::solid({0.90f, 0.16f, 0.14f,
                                                     0.34f}))
                              .foreground(util::stroke(
                                  1.0f,
                                  Fill::color({1.0f, 0.35f, 0.30f, 0.8f}))))
                   .child(text(toU8("blocked"),
                               lt::type(10.5f, lt::kAsh, 0.8f))));
    return root;
  }
};

} // namespace compose_gallery
