#pragma once

#include <include/core/SkBitmap.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Ramp.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/Animation.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace measure = sigil::measure;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace std::chrono_literals;
using sigil::material::skia::Paint;
using sigil::material::skia::toColor;

namespace xcom {

// ---------------------------------------------------------------------------
// SCALE. One constant. Original 320x200 px -> canvas px; nothing else scales.

constexpr float PX = 4.0f;
// The unit map as a value. Only the scale is used here; the type also carries
// an origin and a snap. It is constexpr, which is why it rounds by hand
// instead of calling std::round — that would make n() unusable in the
// constant expressions below.
constexpr path::Grid kGrid{.scale = PX};
constexpr float n(float v) { return kGrid.s(v); }
constexpr float kCanvasW = n(320), kCanvasH = n(200);
constexpr float kPanelY = n(144), kPanelH = n(56);  // 200 - iconsHeight

// ---------------------------------------------------------------------------
// THE PALETTE. Sixteen blocks of sixteen, each block one hue ramping light ->
// dark. Palette::blockOffset(b) = b*16, and the whole interface addresses
// colour as a BLOCK NUMBER, never as a raw index. Index 0 is transparent
// (loadDat sets _colors[0].unused = 0); the dump writes it 00FF00 as a chroma
// key and it is never drawn. The background everything sits on is index 15,
// #000000 — Map.cpp:194 literally begins with clear(blockOffset(0) + 15).
//
// This table is typed in once and there is NO hex literal anywhere below it.

// clang-format off
constexpr uint32_t kPal[256] = {
/* 0 grey  */ 0x000000,0xFCFCFC,0xE8E8E8,0xD8D8D8,0xC4C4C4,0xB0B0B0,0xA0A0A0,0x8C8C8C,
              0x7C7C7C,0x686868,0x585858,0x444444,0x343434,0x202020,0x0C0C0C,0x000000,
/* 1 amber */ 0xFCD000,0xECB400,0xE0A000,0xD08800,0xC47400,0xB46000,0xA85000,0x984000,
              0x8C3400,0x7C2400,0x701C00,0x601000,0x540800,0x480400,0x380000,0x2C0000,
/* 2 red   */ 0xFC7878,0xEC6868,0xE05C5C,0xD04C4C,0xC44444,0xB83838,0xA82C2C,0x9C2424,
              0x8C1C1C,0x801414,0x740C0C,0x640808,0x580404,0x480000,0x3C0000,0x300000,
/* 3 green */ 0xA0E084,0x88D074,0x74C464,0x5CB854,0x48AC48,0x3CA040,0x30903C,0x288438,
              0x1C7838,0x146C34,0x106030,0x08502C,0x044428,0x003824,0x002C1C,0x002018,
/* 4 y-grn */ 0xD0E460,0xB8D454,0xA4C448,0x90B83C,0x7CA830,0x689C28,0x588C20,0x487C18,
              0x387014,0x28600C,0x1C5408,0x104404,0x083400,0x042800,0x001800,0x000C00,
/* 5 stone */ 0xF8F8F8,0xE8E0E0,0xDCD0CC,0xCCBCB8,0xC0ACA8,0xB09C94,0xA48C84,0x947C74,
              0x886C64,0x785C58,0x6C5048,0x5C443C,0x503830,0x442C28,0x34201C,0x281814,
/* 6 tan   */ 0xF0C448,0xE0B040,0xD49C34,0xC88830,0xBC7828,0xB06820,0xA05418,0x944814,
              0x883810,0x7C2C0C,0x702008,0x601404,0x540C04,0x480400,0x3C0000,0x300000,
/* 7 steel */ 0x8CB0C8,0x80A0BC,0x7090B0,0x6880A4,0x5C7098,0x506490,0x485484,0x3C4878,
              0x343C6C,0x2C3060,0x242458,0x201C4C,0x1C1840,0x181034,0x140C28,0x100820,
/* 8 sky   */ 0xA8D0F0,0x94BCE0,0x80ACD4,0x7098C8,0x6088BC,0x5078AC,0x4068A0,0x345894,
              0x284884,0x1C3C78,0x14306C,0x0C2460,0x081850,0x041044,0x000838,0x00042C,
/* 9 yellw */ 0xFCFC78,0xECE468,0xE0CC5C,0xD0B84C,0xC4A044,0xB88C38,0xA8782C,0x9C6424,
              0x8C501C,0x804014,0x74300C,0x642008,0x581404,0x480800,0x3C0400,0x300000,
/*10 olive */ 0xB8A058,0xAC904C,0xA08040,0x947038,0x88602C,0x7C5024,0x70441C,0x683818,
              0x5C2C10,0x50200C,0x441408,0x380C04,0x2C0800,0x200400,0x140000,0x0C0000,
/*11 mauve */ 0xF8DCD4,0xE8C4C0,0xDCB0AC,0xD09898,0xC0888C,0xB47880,0xA86878,0x985C6C,
              0x8C4C64,0x80405C,0x703454,0x642C4C,0x582044,0x48183C,0x3C1034,0x300C2C,
/*12 violt */ 0xD8C4FC,0xC8ACEC,0xB894E0,0xAC80D0,0xA06CC4,0x985CB4,0x8C4CA8,0x843C98,
              0x7C308C,0x74247C,0x6C1870,0x601060,0x540850,0x48043C,0x38002C,0x2C0020,
/*13 cyan  */ 0x40C4FC,0x38ACEC,0x3094E0,0x2880D0,0x206CC4,0x1C58B8,0x1848A8,0x10349C,
              0x0C288C,0x081880,0x041074,0x040464,0x040058,0x040048,0x08003C,0x080030,
/*14 lav-g */ 0xECECF8,0xD8D8E8,0xC4C4DC,0xB4B4D0,0xA4A4C0,0x9490B4,0x8884A8,0x787498,
              0x6C648C,0x605880,0x544C70,0x484064,0x3C3458,0x342848,0x28203C,0x201830,
/*15 cold  */ 0x8C9894,0x84888C,0x747C84,0x6C747C,0x5C686C,0x545C64,0x4C505C,0x384454,
              0x303844,0x283038,0x202430,0x181C20,0x101418,0x080C10,0x030408,0x030306,
};
// clang-format on

/** Palette index -> colour. Index 0 is the chroma key and returns alpha 0. */
inline SkColor4f C(int idx) noexcept {
  return hexColor(kPal[(unsigned)idx & 255u], idx == 0 ? 0.0f : 1.0f);
}
constexpr int blk(int block, int step) { return block * 16 + step; }

/** StandardShade, Surface.cpp:898. THE mechanism: add to the low nibble, stay
 *  inside the 16-entry ramp, and snap to absolute black on overflow. No
 *  multiply, no lerp, no colour space — one add and one compare. */
constexpr int shd(int src, int shade) {
  const auto bits = (unsigned)src;
  const int ns = (int)(bits & 15u) + shade;
  return ns > 15 ? 15 : (int)((bits & 0xF0u) | (unsigned)ns);
}
/** ColorReplace, same file: the block is REPLACED by a 1-based block number.
 *  This is how one arrow sprite and one TU numeral recolour three ways. */
constexpr int replaceBlock(int src, int shade, int block1) {
  const int ns = (int)((unsigned)src & 15u) + shade;
  return ns > 15 ? 15 : (int)(((unsigned)(block1 - 1) << 4u) | (unsigned)ns);
}

// ---------------------------------------------------------------------------
// THE PROJECTION — Camera.cpp, with _spriteWidth = 32 and _spriteHeight = 40
// substituted. Map.cpp's own header diagram: origin is the TOP corner, x runs
// down-right, y runs down-left, z runs up. North therefore points at the upper
// RIGHT of the screen.
//
//   +1 map x -> (+16, +8)   |  +1 map y -> (-16, +8)
//   +1 map z -> (  0, -24)  |  (+1,-1)  -> (+32,  0)   pure horizontal
//
// At 4x those are (+64,+32) / (-64,+32) / (0,-96) / (+128,0), and the sprite
// cell is 128 x 160. Framing constants chosen so map tile (9,9,0)'s cell centre
// lands on the viewport centre (640, 288): OX = 576, OY = -368.

// The map has to be 26 x 26 for this framing to fill the viewport. The visible
// band's CORNERS need tiles the array must actually contain — mx-my = 10 with
// mx+my = 29 wants mx = 19.5 — so a 20 x 20 array leaves a black sawtooth
// along the bottom-left edge. A full-bleed 320x144 viewport needs 26.
constexpr float kOX = 576.0f, kOY = -368.0f;
constexpr int kMapSize = 26;
constexpr float kCellW = n(32), kCellH = n(40);
constexpr float kMapViewW = kCanvasW, kMapViewH = kPanelY;

/** map (mx,my,mz) -> the TOP-LEFT of the 128x160 sprite cell. */
inline SkPoint mapToScreen(int mx, int my, int mz) {
  return {kOX + (float)(mx - my) * n(16),
          kOY + (float)(mx + my) * n(8) - (float)mz * n(24)};
}
/** Pool positions are the cell's CENTRE, not its top-left — a stamp is placed
 *  by its middle, so every mapToScreen result has to be re-centred before it
 *  goes into a Pool. */
inline SkPoint cellCentre(int mx, int my, int mz) {
  const SkPoint p = mapToScreen(mx, my, mz);
  return {p.fX + kCellW * 0.5f, p.fY + kCellH * 0.5f};
}

/** Camera::convertScreenToMap:438, transcribed. Integer division is C
 *  truncation toward zero, so negative screen positions land one tile off —
 *  that is the original behaviour and the reason for the Clamp to -1. */
inline void screenToMap(float sxf, float syf, int offsetZ, int* mapX,
                        int* mapY) {
  // Work in ORIGINAL pixels: the arithmetic below is the 1994 arithmetic.
  int screenX = (int)std::floor(sxf / PX);
  int screenY = (int)std::floor(syf / PX);
  screenY += (-32 / 2) + offsetZ * ((40 + 32 / 4) / 2);  // -16 + offsetZ*24
  const int offX = (int)(kOX / PX), offY = (int)(kOY / PX);
  int my = -screenX + offX + 2 * screenY - 2 * offY;
  int mx = screenY - offY - my / 4 - (32 / 4);  // - 8
  mx /= (32 / 4);                               // /= 8
  my /= 32;                                     // /= 32
  *mapX = std::clamp(mx, -1, kMapSize);
  *mapY = std::clamp(my, -1, kMapSize);
}

// ---------------------------------------------------------------------------
// THE 3x5 BITMAP FONT — NumberText.cpp, transcribed verbatim, MSB = leftmost
// column. Advance is 4 px: `x += _chars[d]->getWidth() + 1`, no kerning, no
// proportional width, and _bordered (the map markers) uses the same advance
// deliberately — "no this isn't a typo, i want to use the same spacing
// regardless." NumberText::draw ends with offset(_color), so a lit pixel lands
// at _color + 1: numTUs declares 64, therefore the TU digits are PAL[65].

constexpr uint8_t kDigit[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};

// ---------------------------------------------------------------------------
// PIXEL DRAWING. Every mark in this file is an ORIGINAL pixel, drawn as a PX-
// sized axis-aligned rect with antialiasing off. Nothing else can put a colour
// on the canvas that is not in the table above.

/** The pen a mark is made with, in INDICES rather than colours, because
 *  indices are this file's whole vocabulary: `kit::PixelInk` at the tile
 *  scale with the table applied on the way through. Index 0 is `C(0)`,
 *  whose alpha is zero, so the chroma key is a hole here exactly as it is
 *  in the index bake.
 *
 *  A drawing headed for the ATLAS fills a `kit::Sprite` instead, which
 *  speaks the same three verbs and RECORDS the indices rather than
 *  resolving them — see `indexCell`. */
struct Ink {
  kit::PixelInk ink;
  explicit Ink(SkCanvas& c) : ink{c, PX} {}
  void rect(float x, float y, float w, float h, int idx) const {
    ink.rect(x, y, w, h, C(idx));
  }
  void px(float x, float y, int idx) const { ink.px(x, y, C(idx)); }
  /** Run-length row: [x0, x0+w) at row y. */
  void row(float x0, float y, float w, int idx) const {
    ink.row(x0, y, w, C(idx));
  }
};

// ---------------------------------------------------------------------------
// THE PALETTE, IN THE SHADER. Every atlas cell's art is authored ONCE, in
// indices, and the 256-entry table reaches SkSL as a 256 x 1 CHILD IMAGE:
// `Paint::sksl(...).child("uPalette", Paint::image(strip, ..., kNearest))`.
// The lookup has to be a child rather than a uniform array because the index
// is a PIXEL VALUE — SkSL indexes a uniform array only by a constant — and it
// has to be kNearest because the sampled value is data, not colour: a linear
// tap between two palette entries is a blend of two unrelated hues.
//
// The body below is shd() and replaceBlock() transcribed, and like them it
// does not multiply: one add, one compare, one snap to absolute black.
// `uShade` is StandardShade's addend and `uBlock1` is ColorReplace's 1-based
// block, zero meaning keep the index's own.

inline sk_sp<SkRuntimeEffect> paletteEffect() {
  auto [effect, error] = SkRuntimeEffect::MakeForShader(
      SkString("uniform shader uIndex;\n"
               "uniform shader uPalette;\n"
               "uniform float uShade;\n"
               "uniform float uBlock1;\n"
               "half4 main(float2 p) {\n"
               "  half4 s = uIndex.eval(p);\n"
               "  if (s.a < 0.5) { return half4(0); }\n"
               "  float i = floor(float(s.r) * 255.0 + 0.5);\n"
               "  float ns = mod(i, 16.0) + uShade;\n"
               "  float base = uBlock1 > 0.0 ? (uBlock1 - 1.0) * 16.0\n"
               "                             : floor(i / 16.0) * 16.0;\n"
               "  float dst = ns > 15.0 ? 15.0 : base + ns;\n"
               "  return uPalette.eval(float2(dst + 0.5, 0.5));\n"
               "}"));
  if (!effect) std::fprintf(stderr, "palette effect: %s\n", error.c_str());
  return effect;
}

/** The 256 entries as the table a shader samples.
 *
 *  `material::Palette` is the seam and `skia::paletteLookup` is its
 *  crossing: a palette says there is nothing BETWEEN its entries, so the
 *  tap is nearest at texel centres and index n is index n. Index 0 keeps
 *  its alpha 0, so the chroma key stays the hole `C(0)` leaves. */
inline sigil::material::Palette palette() {
  sigil::material::Palette table;
  table.entries.reserve(256);
  for (int i = 0; i < 256; ++i) table.entries.emplace_back(C(i));
  return table;
}

/** One cell's art, plotted once as INDICES and baked into a 128 x 160
 *  index raster: the value in each pixel is the palette entry, not the
 *  colour it names, so one drawing serves every shade and every marker
 *  block and the table is applied afterwards in the shader. That is what
 *  an 8-bit sprite sheet always was. */
inline sk_sp<SkImage> indexCell(const std::function<void(kit::Sprite&)>& art) {
  kit::Sprite cell;
  cell.grid = {(int)(kCellW / PX), (int)(kCellH / PX)};
  art(cell);
  return kit::indexImage(cell, PX);
}

/** @p indices read through @p table at @p shade, optionally with the block
 *  replaced by the 1-based @p block1. Identity local matrix on the index
 *  child: the cell is baked at 1:1, so a fragment centre lands on a texel
 *  centre and the sample is the authored byte.
 *
 *  THE EFFECT AND THE TABLE ARE THE CALLER'S, held on the sketch for the
 *  length of a declaration. A `static` here would outlive the dylib a
 *  hot-reloaded sketch is compiled into. */
inline Paint paletteLut(const sk_sp<SkRuntimeEffect>& effect,
                        const Paint& table, const sk_sp<SkImage>& indices,
                        int shade, int block1 = 0) {
  Paint p = Paint::sksl(effect);
  p.uniform("uShade", (float)shade);
  p.uniform("uBlock1", (float)block1);
  p.child("uIndex", Paint::image(indices, SkTileMode::kClamp,
                                 SkTileMode::kClamp, SkMatrix::I(),
                                 SkSamplingOptions{SkFilterMode::kNearest}));
  p.child("uPalette", table);
  return p;
}

/** A deterministic 32-bit hash. Tile dither, speckle placement, the terrain
 *  scenario — everything random in this file comes through here, so the map is
 *  the same map on every run. */
inline uint32_t hash3(int a, int b, int c) {
  return sigil::core::noise::lattice(0, a, b, c);
}

// ---------------------------------------------------------------------------
// THE DIAMOND. Do NOT draw this as a rotated polygon — a rotated polygon
// antialiases its edges and forty colours appear that are not in the palette.
// It is a six-line run-length rasteriser, which is how the sprite was drawn in
// the first place: 32 wide, 16 tall, row r running [x0, x0+w).
//
// The widths are forced by the projection, not chosen. A neighbour at (+16,+8)
// must start exactly where this row ends, which gives w(r) = 2 + 4*min(r,15-r)
// and x0 = (32-w)/2 — check: x0(r) + 16 == x0(r+8) + w(r+8) for every r. The
// row widths sum to 256 = 32*16/2, exactly the rhombus area, so the tiling is
// gapless and overlap-free by construction.

constexpr int kDiamondTop = 24;  // rows 24..39 of the 32x40 cell

inline void diamondRow(int r, int& x0, int& w) {
  w = 2 + 4 * std::min(r, 15 - r);
  x0 = (32 - w) / 2;
}

// ---------------------------------------------------------------------------
// THE SCENARIO. A forest crash-recovery at dusk, globalShade 8.
//
// RECONSTRUCTED, and the only part of the file that is: the terrain layout, the
// tree placement and the discovered boundary. The projection that places them
// and the lighting that shades them are not.

enum Floor : uint8_t { kGrass = 0, kDirt = 1, kHull = 2 };
enum Obj : uint8_t { kNone = 0, kBush = 1, kTree = 2, kHullWall = 3 };

struct Soldier {
  int mx, my;
};
// Placement is constrained by the projection, not by taste: the viewport shows
// screen diagonals mx+my in [7, 26] — twenty of them — and a soldier drawn
// WHOLE costs four at the top. So the selected unit sits on diagonal 12 and the
// path preview is fourteen tiles rather than fifteen: a fifteenth marker would
// land on diagonal 27, whose diamond starts at y = 592 and is under the panel.
constexpr Soldier kSoldierA{8, 4};   // selected; Anders Holmgren
constexpr Soldier kSoldierB{15, 7};  // second of the squad, eight tiles away —
                                     // the two nine-step pools MEET rather than
                                     // add (Tile::addLight keeps the maximum),
                                     // and the join is a visible hard seam
constexpr int kAlienX = 12, kAlienY = 9;  // half-revealed at the light's edge
constexpr int kGlobalShade = 8;
constexpr int kPersonalLight = 15;  // TileEngine::personalLightPower

/** Tile::getShade() = max(0, 15 - maxLight), with the sun at
 *  power = 15 - globalShade and each living player unit carrying a personal
 *  light of 15 propagated as `power - Round(euclidean distance)`.
 *
 *  At globalShade 8 the ambient is 7, so bare terrain sits at shade 8 and each
 *  soldier stands in a nine-step pool reaching ambient at EXACTLY eight tiles.
 *  Round(), not floor() — verification #3 is the off-by-one that catches it. */
inline int tileShade(int mx, int my) {
  int light = 15 - kGlobalShade;  // the sun
  const Soldier units[2] = {kSoldierA, kSoldierB};
  for (const Soldier& u : units) {
    const float dx = (float)(mx - u.mx), dy = (float)(my - u.my);
    const int d = (int)std::lround(std::sqrt(dx * dx + dy * dy));
    light = std::max(light, kPersonalLight - d);
  }
  return std::max(0, 15 - light);
}

struct TileData {
  Floor floor = kGrass;
  Obj object = kNone;
  bool seen = false;
};

/** The path: fourteen steps from the selected soldier, down-left toward the
 *  cursor. Cardinal steps only, so each one advances (mx+my) by exactly 1 and
 *  the run walks fifteen screen diagonals — the visible band is 7..26. */
inline std::vector<SkIPoint> pathTiles() {
  // 11x (0,+1) down-left, 3x (+1,0) down-right, interleaved.
  static const int kStep[14] = {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  std::vector<SkIPoint> out;
  int mx = kSoldierA.mx, my = kSoldierA.my;
  for (int step : kStep) {
    if (step)
      ++mx;
    else
      ++my;
    out.push_back({mx, my});
  }
  return out;
}

inline std::array<TileData, (size_t)kMapSize * kMapSize> buildMap() {
  std::array<TileData, (size_t)kMapSize * kMapSize> m{};
  for (int mx = 0; mx < kMapSize; ++mx)
    for (int my = 0; my < kMapSize; ++my) {
      TileData& t = m[(size_t)my * (size_t)kMapSize + (size_t)mx];
      const int sum = mx + my, diff = mx - my;
      const uint32_t h = hash3(mx, my, 7);
      // The discovered boundary. Each half is a MONOTONE function of the
      // coordinate it does not cut: the top edge wanders with `diff`, the left
      // edge wanders with `sum`. Hashing both per tile — even on a coarse
      // lattice — left isolated undrawn tiles scattered inside the lit area,
      // which reads as damage rather than as a frontier.
      const int j1 = (int)(hash3(diff / 3, 0, 11) % 5u) - 2;
      const int j2 = (int)(hash3(sum / 3, 0, 23) % 5u) - 2;
      t.seen = sum >= 11 + j1 && diff >= -6 + j2;
      // The crash site: bare dirt to the north-east (screen upper right).
      const int dirtEdge = 3 + (int)((hash3(mx / 2, my / 2, 3)) % 3u);
      t.floor = (diff >= dirtEdge && sum <= 22) ? kDirt : kGrass;
      // Hull plates, a wrecked section of the UFO — the one two-level
      // structure on the map, and therefore the only z = 1 content.
      if (mx >= 12 && mx <= 13 && my >= 3 && my <= 4) t.floor = kHull;
      // Forest. Denser on grass than on the scorched dirt.
      const uint32_t r = (h >> 18u) % 100u;
      if (t.floor == kGrass)
        t.object = r < 16u ? kTree : (r < 34u ? kBush : kNone);
      else if (t.floor == kDirt)
        t.object = r < 6u ? kTree : (r < 16u ? kBush : kNone);
      else
        t.object = kHullWall;
    }
  // A dirt track, and the walked route is always discovered and always clear.
  for (const SkIPoint& p : pathTiles()) {
    TileData& t = m[(size_t)p.y() * (size_t)kMapSize + (size_t)p.x()];
    t.seen = true;
    t.object = kNone;
  }
  for (const Soldier& u : {kSoldierA, kSoldierB}) {
    TileData& t = m[(size_t)u.my * (size_t)kMapSize + (size_t)u.mx];
    t.seen = true;
    t.object = kNone;
  }
  m[(size_t)kAlienY * (size_t)kMapSize + (size_t)kAlienX].object = kNone;
  m[(size_t)kAlienY * (size_t)kMapSize + (size_t)kAlienX].seen = true;
  return m;
}

// ---------------------------------------------------------------------------
// THE CELL ART. Each drawing below is authored ONCE, in indices, into a
// 128 x 160 raster at FINAL scale, so the stamp is 1:1 and no magnification
// happens. It carries no shade and no marker colour: those are the LUT's two
// uniforms, and the atlas frame for (type, shade) is that one drawing read
// through the table.
//
// The frames are still `types x shades`, because a 16-step palette ramp is NOT
// a scalar multiple of its top entry and Pool::tints() therefore cannot shade a
// tile (block 3 at shade 8 needs per-channel multipliers R 0.17 / G 0.54 /
// B 0.42, and no single scalar comes close — matching green leaves red far too
// bright). What the LUT removes is the RE-RASTERISATION behind each frame.

/** Floor: the diamond, dithered two palette steps apart with a scatter of a
 *  third. X-COM's terrain has no smooth shading inside a tile — two steps in a
 *  dither is the entire vocabulary; there is no gradient anywhere.
 *
 *  TWO VARIANTS per (type, shade), keyed off (mx+my) parity. With a single
 *  variant the dither repeats identically under the (+-16, +8) lattice and a
 *  coherent moire appears across the whole map — the flyweight's own failure
 *  mode, and one that only shows up in the picture, never in the arithmetic. */
inline void paintFloor(kit::Sprite& ink, Floor kind, int variant) {
  int base = blk(3, 5), alt = blk(3, 7), speck = blk(3, 9), rare = blk(2, 4);
  if (kind == kDirt) {
    base = blk(6, 7);
    alt = blk(10, 6);
    speck = blk(10, 9);
    rare = blk(6, 11);
  } else if (kind == kHull) {
    base = blk(14, 3);
    alt = blk(14, 5);
    speck = blk(14, 7);
    rare = blk(7, 4);
  }
  for (int r = 0; r < 16; ++r) {
    int x0, w;
    diamondRow(r, x0, w);
    for (int x = x0; x < x0 + w; ++x) {
      const uint32_t h = hash3(x, r, (int)kind * 31 + variant * 977);
      int idx = (h & 1u) ? base : alt;
      if ((h % 9u) == 0u)
        idx = speck;
      else if ((h % 61u) == 0u)
        idx = rare;
      ink.px((float)x, (float)(kDiamondTop + r), idx);
    }
  }
  if (kind == kHull)  // plate seams
    for (int r = 3; r < 16; r += 6) {
      int x0, w;
      diamondRow(r, x0, w);
      ink.row((float)x0, (float)(kDiamondTop + r), (float)w, blk(14, 9));
    }
}

/** The diamond's row span for a given column — the extrusion silhouette. A
 *  column x is inside row r iff min(r, 15-r) >= ceil((|x-15.5| - 0.5)/2), which
 *  falls straight out of the run-length rule above. */
inline void diamondColumn(int x, int& rTop, int& rBot) {
  const float dx = std::abs((float)x - 15.5f);
  const int k = (int)std::ceil((dx - 0.5f) / 2.0f);
  rTop = std::clamp(k, 0, 7);
  rBot = 15 - rTop;
}

/** The wrecked UFO: one tile's worth of hull extruded a full level (24 px),
 *  which is what makes the crash site the only two-level structure here — the
 *  wall face blits at z = 0 and the deck plate at z = 1, exactly the
 *  O_OBJECT / O_FLOOR split Map::drawTerrain walks. */
inline void paintHullWall(kit::Sprite& ink) {
  for (int x = 0; x < 32; ++x) {
    int rTop, rBot;
    diamondColumn(x, rTop, rBot);
    const int y0 = kDiamondTop - 24 + rTop, y1 = kDiamondTop + rBot;
    for (int y = y0; y <= y1; ++y) {
      const uint32_t h = hash3(x, y, 55);
      const bool lit = x > 15;
      int step = (lit ? 6 : 9) + (int)(h % 2u);
      if ((h % 23u) == 0u) step += 3;      // rivets and scoring
      if (((y - y0) % 7) == 0) step += 2;  // hull plate seams, one per 7 rows
      ink.px((float)x, (float)y, blk(14, step));
    }
    ink.px((float)x, (float)y0, blk(14, 4));  // top lip
  }
}
inline void paintHullDeck(kit::Sprite& ink) {
  paintFloor(ink, kHull, 0);
  for (int r = 0; r < 16; ++r) {  // a bright rim so the deck reads as raised
    int x0, w;
    diamondRow(r, x0, w);
    ink.px((float)x0, (float)(kDiamondTop + r), blk(14, 1));
    ink.px((float)(x0 + w - 1), (float)(kDiamondTop + r), blk(14, 6));
  }
}

/** A lumpy canopy: an ellipse with a per-row hash wobble, emitted as whole
 *  pixels so nothing lands off the grid. The lit steps and the shaded flank are
 *  three apart on ONE hue ramp, which at ambient (shade 8) pushes the flank's
 *  last step past 15 — so the terminator on a dusk tree is literally
 *  PAL[15] #000000, not the ramp's darkest green. That is the overflow branch
 *  in shd(), and it is why X-COM's night terrain looks the way it does. */
inline void paintCanopy(kit::Sprite& ink, float cx, float cy, float rx,
                        float ry, int litStep, uint32_t seed) {
  const int r0 = (int)std::floor(cy - ry), r1 = (int)std::ceil(cy + ry);
  for (int r = r0; r <= r1; ++r) {
    const float t = ((float)r + 0.5f - cy) / ry;
    float hw = rx * std::sqrt(std::max(0.0f, 1.0f - t * t));
    hw += (float)((int)(hash3(r, (int)seed, 3) % 5u) - 2) * 0.5f;
    const int w = (int)std::lround(hw * 2.0f);
    if (w <= 0) continue;
    const int x0 = (int)std::lround(cx - (float)w * 0.5f);
    for (int x = x0; x < x0 + w; ++x) {
      const uint32_t h = hash3(x, r, (int)seed + 91);
      const bool flank = (float)x > cx + hw * 0.15f;
      int idx = blk(3, litStep + (flank ? 3 : 0) + (int)(h % 3u));
      if (!flank && (h % 89u) == 0u)
        idx = blk(2, 2);  // fruit — block 2 red, one pixel
      ink.px((float)x, (float)r, idx);
    }
  }
}

/** One cell per tree: canopy rows 4..24, trunk 22..32, standing on the
 *  diamond's centre (row 32). 28 original px tall, which is what the reference
 *  measures. It has to fit in ONE cell: a tree drawn across two levels comes
 *  out around 48 px and reads as a column rather than as a tree. */
inline void paintTree(kit::Sprite& ink) {
  for (int r = 23; r <= 32; ++r)
    ink.row(15.0f, (float)r, r > 29 ? 3.0f : 2.0f, blk(10, 6));
  ink.row(13.0f, 32.0f, 6.0f, blk(10, 8));
  paintCanopy(ink, 16.0f, 16.0f, 9.5f, 8.5f, 6, 5u);
}
inline void paintBush(kit::Sprite& ink) {
  paintCanopy(ink, 16.0f, 27.0f, 8.0f, 5.5f, 7, 17u);
}

/** The path arrow. ColorReplace recolours ONE sprite three ways, so the art is
 *  authored in block 0 and the LUT replaces the block per cell — which is
 *  exactly what blitNShade(..., newBaseColor) does at runtime.
 *  dir: 0 = down-left (map +y), 1 = down-right (map +x). */
inline void paintArrow(kit::Sprite& ink, int dir) {
  const auto put = [&](int x, int y, int step) {
    ink.px((float)(dir == 0 ? x : 31 - x), (float)y, blk(0, step));
  };
  // A left-pointing arrow, 11 x 7 original px: head then shaft. Sized off
  // the reference, and the size is the point — a 32-px tile is only 32 px,
  // so an arrow much larger than this covers half the ground it is
  // marking.
  //
  // AND IT IS FLAT. ColorReplace recolours a block; it does not shade one,
  // and the reference's markers are three flat blocks — green 4, yellow
  // 10, red 3 — with no keyline and no lit leading edge. A bright rim and
  // a darker shaft turn a marker into an icon, which at eleven pixels is
  // the loudest thing on the ground it is marking.
  for (int k = 0; k < 4; ++k)
    for (int y = 8 - k; y <= 8 + k; ++y) put(8 + k, y, 2);
  for (int x = 12; x < 19; ++x)
    for (int y = 6; y <= 10; ++y) put(x, y, 2);
}

/** The 3D box selector — a 32x40 wireframe cube, one tile, block 2. Drawn at
 *  the tile the HONEST inverse returns; the game's own picker biases the mouse
 *  ten pixels down first (Map.cpp:1314), and runAudit reports both answers so
 *  the difference is visible. */
inline void paintCursor(kit::Sprite& ink) {
  const int lit = blk(2, 0), dim = blk(2, 2);
  const int lift = 12;  // the box is half a level tall
  for (int r = 0; r < 16; ++r) {
    int x0, w;
    diamondRow(r, x0, w);
    const float xr = (float)(x0 + w - 1);
    ink.px((float)x0, (float)(kDiamondTop + r), lit);
    ink.px(xr, (float)(kDiamondTop + r), lit);
    ink.px((float)x0, (float)(kDiamondTop - lift + r), dim);
    ink.px(xr, (float)(kDiamondTop - lift + r), dim);
  }
  // Four vertical edges at the diamond's corners: left, right, back, front.
  for (int k = 0; k < lift; ++k) {
    ink.px(1.0f, (float)(kDiamondTop - lift + 7 + k), dim);
    ink.px(30.0f, (float)(kDiamondTop - lift + 7 + k), dim);
    ink.px(15.0f, (float)(kDiamondTop - lift + k), dim);
    ink.px(16.0f, (float)(kDiamondTop - lift + k), dim);
    ink.px(15.0f, (float)(kDiamondTop - lift + 15 + k), lit);
    ink.px(16.0f, (float)(kDiamondTop - lift + 15 + k), lit);
  }
}

/** Two soldiers and one alien, as real Elements rather than pool instances —
 *  they need keys so hitTest can reach them (verification #2). Block 8 armour,
 *  block 11 face; the alien is block 12. */
inline void paintUnit(SkCanvas& canvas, int shade, bool alien) {
  const Ink ink{canvas};
  const int body = alien ? blk(12, 2) : blk(8, 2);
  const int dark = alien ? blk(12, 7) : blk(8, 7);
  const int face = alien ? blk(3, 2) : blk(11, 2);
  const int boot = blk(0, 11);
  // Head
  ink.rect(13, 8, 6, 5, shd(body, shade));
  ink.rect(14, 10, 4, 3, shd(face, shade));
  ink.px(13, 12, shd(dark, shade));
  ink.px(18, 12, shd(dark, shade));
  // Torso + arms
  ink.rect(12, 13, 8, 9, shd(body, shade));
  ink.rect(19, 14, 2, 8, shd(dark, shade));
  ink.rect(11, 14, 2, 7, shd(dark, shade));
  ink.rect(13, 16, 5, 2, shd(dark, shade));
  // Legs
  ink.rect(13, 22, 3, 7, shd(dark, shade));
  ink.rect(17, 22, 3, 7, shd(body, shade));
  ink.rect(12, 29, 5, 2, shd(boot, shade));
  ink.rect(17, 29, 5, 2, shd(boot, shade));
  if (!alien) {  // the rifle, held across
    ink.rect(9, 17, 12, 1, shd(blk(15, 2), shade));
    ink.rect(14, 18, 4, 2, shd(blk(15, 5), shade));
  }
}

/** The selected-soldier arrow. arrowBob[8] = {0,1,2,1,0,1,2,1} — three integer
 *  positions, the only continuous-looking motion on the whole screen, and it
 *  is quantised. */
constexpr int kArrowBob[8] = {0, 1, 2, 1, 0, 1, 2, 1};
inline void paintBobArrow(SkCanvas& canvas, int frame) {
  const Ink ink{canvas};
  const int dy = kArrowBob[(unsigned)frame & 7u];
  for (int k = 0; k < 5; ++k)
    ink.row((float)(12 + k), (float)(dy + 4 + k), (float)(10 - 2 * k),
            blk(1, k == 0 ? 0 : 1));
}

// ---------------------------------------------------------------------------
// PIXEL TYPE. FONT_BIG and FONT_SMALL are .DAT glyph bitmaps and are not in any
// repo, and there is no bitmap-font path to load them into. So the substitute
// face is shaped at 1x — the 1994 pixel — rendered into a raster the size of
// its own ink, and every pixel QUANTISED into a palette ramp by coverage: two
// steps, the way an 8-bit artist would have antialiased a face by hand. Then
// it is presented at 4x with kNearest. The result is a bitmap font again, on
// the grid, in the palette, and the colour census stays clean.

struct PixelText {
  sk_sp<SkImage> image;
  int w = 0, h = 0;
};

inline PixelText pixelText(const std::u8string& s, weave::TextStyle style,
                           weave::FontContext& fonts, int litIdx, int dimIdx) {
  // The measure -> raster -> read-back -> crop-to-ink core is kit::coverage();
  // what belongs to this file is the TWO-LEVEL quantisation into palette
  // indices below, which is why the kit hands back coverage rather than a
  // finished 1-bit mask.
  //
  // The padding around the raster has to grow on EVERY side, which is what
  // kit::coverage does: a glyph with a negative left side-bearing puts ink at
  // negative x, so a pad applied only to the right and bottom clips it at
  // x = 0 no matter how large the pad is.
  style.paint.foreground.setColor(SK_ColorWHITE);
  const kit::Coverage cov = kit::coverage(s, fonts, style);
  if (!cov.valid() || cov.ink.isEmpty()) return {};
  const int cw = cov.ink.width(), chh = cov.ink.height();
  SkBitmap out;
  out.allocPixels(SkImageInfo::Make(cw, chh, kRGBA_8888_SkColorType,
                                    kUnpremul_SkAlphaType));
  out.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < chh; ++y)
    for (int x = 0; x < cw; ++x) {
      // Two levels, not one: the way an 8-bit artist hand-antialiased a
      // face, and the reason a 1-bit threshold() would not serve here.
      const float a = cov.alphaAt(cov.ink.fLeft + x, cov.ink.fTop + y);
      const uint8_t lv = a >= 0.55f ? 2 : (a >= 0.22f ? 1 : 0);
      if (!lv) continue;
      const uint32_t v = kPal[(unsigned)(lv == 2 ? litIdx : dimIdx) & 255u];
      // kRGBA_8888 unpremul: R, G, B, A in memory order.
      *out.getAddr32(x, y) = (0xFFu << 24u) | ((v & 255u) << 16u) |
                             (((v >> 8u) & 255u) << 8u) | ((v >> 16u) & 255u);
    }
  out.setImmutable();
  return {out.asImage(), cw, chh};
}

/** The pixel-text element: an image fill at exactly 4x, nearest-sampled, on a
 *  box whose size is the ink. Paint::image takes the sampling options, which
 *  is what keeps the 4x magnification from filtering. */
inline Element pixelTextEl(const PixelText& t, float x, float y) {
  if (!t.image) return box().width(0).height(0);
  return box()
      .left(x)
      .top(y)
      .width((float)t.w * PX)
      .height((float)t.h * PX)
      .fill(Paint::image(t.image, SkTileMode::kDecal, SkTileMode::kDecal,
                         SkMatrix::Scale(PX, PX),
                         SkSamplingOptions(SkFilterMode::kNearest)));
}

// ---------------------------------------------------------------------------
// THE CONTROL PANEL. Every rect below is a literal constructor argument from
// BattlescapeState.cpp:86-161. iconsWidth 320, iconsHeight 56, so
// visibleMapHeight = 144 and the panel is (0,144)-(320,200).

/** Absolute placement in the GAME'S OWN 320x200 pixels, scaled to canvas
 *  px on the way through. */
inline Element at(float x, float y, float w, float h) {
  return kit::at(n(x), n(y), n(w), n(h));
}

/** A 32x16 button plate: rounded by RUN LENGTH, not by a corner radius. A
 *  radius antialiases, and every antialiased edge puts colours on the canvas
 *  that VGA never had. Lit edge 81-85, face 86-87, shadow 91-93 — read off the
 *  capture's flat runs, which are the part of it that survived rescaling. */
inline void paintPlate(SkCanvas& canvas, int w, int h) {
  const Ink ink{canvas};
  for (int r = 0; r < h; ++r) {
    const int inset =
        (r == 0 || r == h - 1) ? 3 : ((r == 1 || r == h - 2) ? 1 : 0);
    const int x0 = inset, run = w - 2 * inset;
    const bool top = r < h / 2;
    ink.row((float)x0, (float)r, (float)run, blk(5, top ? 6 : 8));
    if (r == 0 || r == 1)
      ink.row((float)x0, (float)r, (float)run, blk(5, 1 + r));
    if (r == h - 1) ink.row((float)x0, (float)r, (float)run, blk(5, 12));
    if (r == h - 2) ink.row((float)x0, (float)r, (float)run, blk(5, 11));
    ink.px((float)x0, (float)r, blk(5, r == 0 ? 1 : 3));
    ink.px((float)(x0 + run - 1), (float)r, blk(5, r == h - 1 ? 12 : 10));
  }
}

/** Fourteen glyphs, in columns: UnitUp/UnitDown, MapUp/MapDown, ShowMap/Kneel,
 *  Inventory/Center, NextSoldier/NextStop, ShowLayers/Help, EndTurn/Abort. The
 *  capture's glyphs match that mapping exactly, which is a small verification
 *  in itself. Ink is block 5 step 14 with a block 5 step 2 highlight below. */
inline void paintButtonGlyph(SkCanvas& canvas, int id) {
  const Ink ink{canvas};
  const int d = blk(5, 14), l = blk(5, 2);
  const auto tri = [&](int cx, int cy, int size, bool up) {
    for (int k = 0; k < size; ++k) {
      const int wdt = 2 * (up ? k : size - 1 - k) + 1;
      const int x0 = cx - wdt / 2;
      ink.row((float)x0, (float)(cy + k), (float)wdt, d);
    }
  };
  const auto bars = [&](int x, int y, int rows, int wdt) {
    for (int k = 0; k < rows; ++k)
      ink.row((float)x, (float)(y + k * 2), (float)wdt, d);
  };
  const auto figure = [&](int x, int y) {
    ink.rect((float)(x + 2), (float)y, 2, 2, d);
    ink.rect((float)(x + 1), (float)(y + 2), 4, 4, d);
    ink.rect((float)x, (float)(y + 3), 1, 3, d);
    ink.rect((float)(x + 5), (float)(y + 3), 1, 3, d);
    ink.rect((float)(x + 1), (float)(y + 6), 1, 4, d);
    ink.rect((float)(x + 4), (float)(y + 6), 1, 4, d);
  };
  switch (id) {
    case 0:
      tri(10, 4, 4, true);
      bars(17, 4, 4, 8);
      break;  // unit up
    case 1:
      tri(10, 4, 4, false);
      bars(17, 4, 4, 8);
      break;  // unit down
    case 2:
      tri(10, 4, 4, true);
      bars(17, 4, 4, 6);
      ink.rect(24, 4, 2, 7, d);
      break;
    case 3:
      tri(10, 4, 4, false);
      bars(17, 4, 4, 6);
      ink.rect(24, 4, 2, 7, d);
      break;
    case 4:  // show map: a small grid
      for (int gy = 0; gy < 4; ++gy)
        for (int gx = 0; gx < 7; ++gx)
          ink.rect((float)(9 + gx * 2), (float)(4 + gy * 2), 1, 1,
                   ((unsigned)(gx + gy) & 1u) ? d : l);
      break;
    case 5:
      figure(11, 3);
      ink.rect(17, 9, 5, 1, d);
      break;  // kneel
    case 6:
      figure(13, 3);
      break;  // inventory
    case 7:   // center reticle
      ink.rect(12, 7, 9, 1, d);
      ink.rect(16, 3, 1, 9, d);
      ink.rect(13, 4, 1, 1, d);
      ink.rect(19, 4, 1, 1, d);
      ink.rect(13, 10, 1, 1, d);
      ink.rect(19, 10, 1, 1, d);
      break;
    case 8:
      figure(9, 3);
      ink.rect(17, 7, 6, 1, d);
      ink.rect(21, 5, 1, 5, d);
      figure(23, 3);
      break;  // next soldier
    case 9:
      figure(9, 3);
      ink.rect(17, 7, 6, 1, d);
      ink.rect(21, 5, 1, 5, d);
      ink.rect(24, 3, 1, 9, d);
      break;  // next / stop
    case 10:  // layers: three stacked plates, the top one lit
      for (int k = 0; k < 3; ++k) {
        const int y = 4 + k * 3;
        ink.row(8.0f, (float)y, 10.0f, k == 0 ? l : d);
        ink.row(9.0f, (float)(y + 1), 8.0f, d);
      }
      break;
    case 11:  // help: a question mark
      ink.row(13.0f, 3.0f, 5.0f, d);
      ink.rect(17, 4, 2, 2, d);
      ink.rect(15, 6, 2, 2, d);
      ink.rect(15, 9, 2, 2, d);
      break;
    case 12:  // end turn: slashed circle
      for (int k = 0; k < 9; ++k) {
        const float t = ((float)k - 4.0f) / 4.5f;
        const int hw =
            (int)std::lround(4.5f * std::sqrt(std::max(0.0f, 1 - t * t)));
        ink.px((float)(16 - hw), (float)(3 + k), d);
        ink.px((float)(16 + hw), (float)(3 + k), d);
      }
      for (int k = 0; k < 9; ++k) ink.px((float)(12 + k), (float)(11 - k), d);
      break;
    default:  // abort: a bird
      for (int k = 0; k < 6; ++k) {
        const int y = 8 - k / 2, w = 3 - k / 3, x1 = 18 - k / 2 + k;
        ink.row((float)(10 + k), (float)y, (float)w, d);
        ink.row((float)x1, (float)y, (float)w, d);
      }
      ink.rect(15, 7, 3, 2, d);
      break;
  }
}

/** Bar::draw, Interface/Bar.cpp — eleven lines that define the whole reading:
 *    square = {0,0, scale*max + 1, height};  drawRect(borderColor ?: color+4)
 *    square.y++; square.w--; square.h -= 2;  drawRect(0)      // TRANSPARENT
 *    square.w = scale*value;                 drawRect(color)
 *  With setScale(1.0) the outline is max+1 px long, the fill is `value` px, and
 *  the gap between them is TRANSPARENT so the panel's blue lattice shows
 *  through. That gap is what makes it read as a gauge and not a rectangle. */
inline Element statBar(float x, float y, int value, int maxValue, int colorIdx,
                       const char* key) {
  const int outline = colorIdx + 4;
  // A transparent full-canvas shell, so the four rects keep SCREEN
  // coordinates. The shell answers hitTest whether or not it has a fill, so
  // four of them stacked over the whole frame would make every probe return
  // the topmost one: `hitTestable(false)` excludes the shell's own box and
  // leaves its children tested, which is exactly what a coordinate carrier
  // wants. The KEY still goes on the fill rect rather than the shell.
  // The top outline row carries "<key>-max" so the audit can measure the
  // DRAWN outline length the same way it measures the drawn fill.
  Element g = box().inset(0).hitTestable(false);
  g.child(at(x, y, (float)(maxValue + 1), 1)
              .fill(C(outline))
              .key(std::string(key) + "-max"));
  g.child(at(x, y + 2, (float)(maxValue + 1), 1).fill(C(outline)));
  g.child(at(x + (float)maxValue, y + 1, 1, 1).fill(C(outline)));
  if (value > 0)
    g.child(at(x, y + 1, (float)value, 1).fill(C(colorIdx)).key(key));
  return g;
}

/** A stat recess: a seven-row single-step ramp in one hue block. Three of the
 *  four are blockOffset + 5..11; the TU one is the odd one out and uses the
 *  GREEN block rather than its own yellow-green, at +7..+13. Copy that — it is
 *  what the screen looks like. */
inline Element recess(float x, float y, int firstIdx) {
  Element g = box().inset(0).hitTestable(false);
  for (int r = 0; r < 7; ++r)
    g.child(at(x, y + (float)r, 17, 1).fill(C(firstIdx + r)));
  return g;
}

}  // namespace xcom

using namespace xcom;

// ===========================================================================
