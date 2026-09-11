#pragma once

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Legend.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;
namespace field = sigil::material::field;
namespace mpattern = sigil::material::pattern;

using namespace sigil::compose;
using sigil::compose::toU8;
using sigil::material::skia::Paint;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 880};

namespace loot {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;

/** D2's cell is 29 px at 640x480; this stage is wider, so the grid keeps
 *  the 10x4 shape at 38 px and everything else measures in cells. */
constexpr float kCell = 38, kGap = 2;
constexpr int kCols = 10, kRows = 4;
// Not arrange::moduleSize: these measure the CONTAINER back from a fixed
// module and its gaps, which is that function run backwards.
constexpr float kGridW = kCols * kCell + (kCols - 1) * kGap;
constexpr float kGridH = kRows * kCell + (kRows - 1) * kGap;

// The panel: cold slate under bronze.
constexpr SkColor4f kStoneHi = hexColor(0x2A2723);
constexpr SkColor4f kStoneLo = hexColor(0x14120F);
constexpr SkColor4f kWellHi = hexColor(0x1C1A17);
constexpr SkColor4f kWellLo = hexColor(0x0B0A08);
constexpr SkColor4f kBronze = hexColor(0x8C7247);
constexpr SkColor4f kBronzeLit = hexColor(0xC9A96A);
constexpr SkColor4f kBronzeDim = hexColor(0x4A3B23);
constexpr SkColor4f kParch = hexColor(0xC8BC9A);
constexpr SkColor4f kAsh = hexColor(0x7A705C);

/** Diablo II's item-quality colours. */
enum class Rarity { Normal, Magic, Rare, Unique, Set };
inline SkColor4f rarityColor(Rarity r) {
  switch (r) {
    case Rarity::Magic:
      return hexColor(0x6969FF);
    case Rarity::Rare:
      return hexColor(0xFFFF64);
    case Rarity::Unique:
      return hexColor(0xC7B377);
    case Rarity::Set:
      return hexColor(0x00FF00);
    case Rarity::Normal:
      break;
  }
  return hexColor(0xE8E4DC);
}

// ---------------------------------------------------------------------------
// item silhouettes — every one a path, none an image

enum class Art {
  Sword,
  Armour,
  Wand,
  Helm,
  Gloves,
  Ring,
  Amulet,
  Potion,
  Shield,
  Boots,
  Belt,
  Bow
};

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
        b.moveTo(cx, h * 0.03f);  // point
        b.lineTo(cx + w * 0.15f, h * 0.14f);
        b.lineTo(cx + w * 0.13f, h * 0.62f);  // blade
        b.lineTo(cx - w * 0.13f, h * 0.62f);
        b.lineTo(cx - w * 0.15f, h * 0.14f);
        b.close();
        rect(w * 0.10f, h * 0.62f, w * 0.80f, h * 0.055f);        // crossguard
        rect(cx - w * 0.055f, h * 0.675f, w * 0.11f, h * 0.22f);  // grip
        b.addCircle(cx, h * 0.925f, w * 0.10f);                   // pommel
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
        b.moveTo(w * 0.42f, h * 0.10f);  // collar notch
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
        rect(w * 0.06f, h * 0.70f, w * 0.88f, h * 0.09f);  // brim
        rect(w * 0.30f, h * 0.46f, w * 0.40f, h * 0.07f);  // eye slit
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
        b.moveTo(w * 0.5f, h * 0.10f);  // stone
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
        rect(w * 0.42f, h * 0.10f, w * 0.16f, h * 0.16f);  // neck
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
        b.addCircle(w * 0.5f, h * 0.44f, w * 0.16f);  // boss
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
  const SkColor4f hi =
      glassy ? SkColor4f{tint.fR, tint.fG, tint.fB, 0.95f}
             : SkColor4f{std::min(1.0f, tint.fR * 1.55f + 0.20f),
                         std::min(1.0f, tint.fG * 1.55f + 0.20f),
                         std::min(1.0f, tint.fB * 1.55f + 0.20f), 1};
  const SkColor4f lo{tint.fR * 0.32f, tint.fG * 0.32f, tint.fB * 0.32f, 1};
  return box()
      .width(Dim(w))
      .height(Dim(h))
      .shape(artPath(art))
      .fill(Paint::linear({0, 0}, {w * 0.35f, h},
                          {{0.0f, hi}, {0.55f, tint}, {1.0f, lo}}))
      .stroke(stroke(1.1f, Fill::color({0.03f, 0.03f, 0.03f, 0.85f})));
}

// ---------------------------------------------------------------------------
// the grid model

struct Item {
  const char* name;
  Art art;
  int col, row, w, h;  // cell footprint
  Rarity rarity;
  SkColor4f tint;  // the metal/glass the art is made of
};

/** The hoard. Footprints are D2's: two-hand sword 2x4, body armour 2x3,
 *  wand 1x3, helm 2x2, gloves 2x2, rings and potions 1x1.
 *
 *  The layout leaves ONE free 2x3 pocket, at columns 7-8 of rows 1-3: the
 *  dragged shield's footprint is 2x3, and a "fits" pose has to land on a
 *  run of cells that is really free or the green highlight is a lie. */
inline constexpr Item kItems[] = {
    {"Doomslinger", Art::Sword, 0, 0, 2, 4, Rarity::Unique, hexColor(0xB9A06A)},
    {"Sigil Plate", Art::Armour, 2, 0, 2, 3, Rarity::Set, hexColor(0x6E8F63)},
    {"Ashen Wand", Art::Wand, 4, 0, 1, 3, Rarity::Magic, hexColor(0x6F79C4)},
    {"Grave Helm", Art::Helm, 5, 0, 2, 2, Rarity::Rare, hexColor(0x9AA0A6)},
    {"Cinder Grips", Art::Gloves, 5, 2, 2, 2, Rarity::Normal,
     hexColor(0x8A6A46)},
    {"Band of Soot", Art::Ring, 7, 0, 1, 1, Rarity::Rare, hexColor(0xC7A657)},
    {"Ember Charm", Art::Amulet, 8, 0, 1, 1, Rarity::Unique,
     hexColor(0xC7B377)},
    {"Healing Draught", Art::Potion, 9, 0, 1, 1, Rarity::Normal,
     hexColor(0xB03A3A)},
    {"Healing Draught", Art::Potion, 4, 3, 1, 1, Rarity::Normal,
     hexColor(0xB03A3A)},
    {"Mana Draught", Art::Potion, 9, 1, 1, 1, Rarity::Normal,
     hexColor(0x3A56B0)},
    {"Mana Draught", Art::Potion, 9, 2, 1, 1, Rarity::Normal,
     hexColor(0x3A56B0)},
    {"Warden's Sabatons", Art::Boots, 2, 3, 2, 1, Rarity::Magic,
     hexColor(0x7A6A55)},
};
inline constexpr int kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

/** The equipment paperdoll: D2's ten sockets, at D2's footprints. */
struct Socket {
  const char* label;
  Art ghost;
  float x, y;  // panel-local px
  int w, h;    // cell footprint
  const Item* worn;
};

// ---------------------------------------------------------------------------

/** THE HOARD'S LATTICE: 38 px cells with 2 px between them, and a
 *  footprint that swallows the gaps it crosses. `arrange::cellRect` is
 *  that arithmetic's origin — module, gap, origin and span — and the
 *  three readings below are the four numbers it answers, taken one at a
 *  time. */
inline SkRect cellRect(int col, int row, int cols = 1, int rows = 1) {
  return sigil::geometry::arrange::cellRect({col, row}, {kCell, kCell},
                                            {kGap, kGap}, {0, 0}, cols, rows);
}
inline float cellX(int c) { return cellRect(c, 0).fLeft; }
inline float cellY(int r) { return cellRect(0, r).fTop; }
inline float spanW(int cells) { return cellRect(0, 0, cells, 1).width(); }

/** A well: the two-stop ramp in the kit's recessed well, which is the
 *  shadow inside the edge and the hard sunken lip under it — the blur
 *  says the depth and the lip says where the surface breaks. */
inline Element well(float w, float h, float alpha = 1.0f) {
  return sketch::kit::well(
      {.width = Dim(w),
       .height = Dim(h),
       .ground =
           Paint::linear({0, 0}, {0, h},
                         {{0.0f, {kWellLo.fR, kWellLo.fG, kWellLo.fB, alpha}},
                          {1.0f, {kWellHi.fR, kWellHi.fG, kWellHi.fB, alpha}}}),
       .clip = false,
       .corners = 2,
       .recess = sketch::kit::Well::Recess{
           .lipLight = SkColor4f{0.42f, 0.38f, 0.31f, 0.30f},
           .lipDark = SkColor4f{0, 0, 0, 0.55f}}});
}

/** The bronze-framed stone panel every part of this UI sits in. */
inline Element panel(float w, float h) {
  return box()
      .width(Dim(w))
      .height(Dim(h))
      .corners({4})
      .background(styles::dropShadow({0, 0, 0, 0.65f}, {0, 7}, 16))
      .fill(Paint::linear({0, 0}, {0, h}, {{0.0f, kStoneHi}, {1.0f, kStoneLo}}))
      .clip()
      // quarried, not smooth: the grain is generated, never a texture file
      .child(box()
                 .inset(0)
                 .fill(Paint::recipe(field::noise(0.06f, 4, 7.0f)))
                 .opacity(0.16f)
                 .blend(SkBlendMode::kOverlay))
      .foreground(styles::BevelEmboss{
          2.5f,
          4.0f,
          120,
          {kBronzeLit.fR, kBronzeLit.fG, kBronzeLit.fB, 0.35f},
          {0, 0, 0, 0.7f}})
      .foreground(stroke(2.0f, Fill::color(kBronze)))
      .foreground(
          stroke(1.0f, Fill::color(kBronzeDim), PathFormat::Align::Inner))
      // A PANEL IS A STILL LIFE: a ramp, a generated grain and a bevel,
      // none of which ever change. Recorded rather than baked it re-runs
      // the grain over every pixel of a tall plate on every frame, which
      // is most of this scene's budget; asked for by name it is
      // rasterised once.
      .cache(Cache::Texture);
}

/** Four rivets, one per corner of a panel. */
inline Element rivets(float w, float h, float inset = 11) {
  auto stud = [](float x, float y) {
    return box()
        .width(Dim(6.0f))
        .height(Dim(6.0f))
        .corners({3})
        .left(x - 3)
        .top(y - 3)
        .fill(Paint::radial(
            {3, 3}, 3.4f,
            {{0.0f, kBronzeLit}, {0.7f, kBronze}, {1.0f, kBronzeDim}}));
  };
  return stack()
      .inset(0)
      .child(stud(inset, inset))
      .child(stud(w - inset, inset))
      .child(stud(inset, h - inset))
      .child(stud(w - inset, h - inset));
}

}  // namespace loot
