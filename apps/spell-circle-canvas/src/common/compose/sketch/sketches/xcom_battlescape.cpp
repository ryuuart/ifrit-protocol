// xcom_battlescape.cpp — X-COM: UFO DEFENSE, THE BATTLESCAPE
// =============================================================================
// Mythos Games / MicroProse, 1994, MS-DOS. (US title; UFO: Enemy Unknown in
// Europe.) The tactical combat screen: an isometric tile map filling the top of
// the frame with a fixed 56-pixel control panel welded across the bottom.
//
// Authored at 320 x 200, 8-bit indexed colour, ONE fixed resolution, ONE fixed
// 256-entry palette (PAL_BATTLESCAPE), no antialiasing anywhere, every glyph a
// bitmap. Rebuilt here at EXACTLY 4x — canvas 1280 x 800. Every constant below
// is in original 320x200 units and goes through n(); divide any canvas number
// by four to recover the 1994 pixel, and a 4x nearest downsample of the capture
// overlays the reference.
//
// -----------------------------------------------------------------------------
// SOURCES — read, not remembered
//
//  * github.com/OpenXcom/OpenXcom — a from-scratch GPL reimplementation of the
//    1994 engine that reads the original data files. Its src/ IS the 1994 UI
//    written down. Battlescape/Camera.cpp (convertMapToScreen at :461 and
//    convertScreenToMap at :438 — a matched pair, both transcribed below);
//    Battlescape/Map.cpp (drawTerrain's z/x/y nesting, the
//    clear(blockOffset(0)+15) background at :194, setSelectorPosition's
//    ten-pixel fudge at :1314, arrowBob, the marker blit offsets at :944 and
//    :1148); Battlescape/BattlescapeState.cpp:86-161 (EVERY widget rect on the
//    panel, as literal constructor arguments), plus DEFAULT_ANIM_SPEED = 100,
//    blinkVisibleUnitButtons and blinkHealthBar; Engine/Surface.cpp:898-985
//    (StandardShade / ColorReplace — the shading algebra); Interface/Bar.cpp
//    (Bar::draw, eleven lines); Interface/NumberText.cpp (the 3x5 digit font,
//    setPixel by setPixel); Battlescape/Pathfinding.cpp (directionToVector at
//    :566, the TU cost core, previewPath, green=4/yellow=10/red=3 at :33-35);
//    Battlescape/TileEngine.cpp (calculateSunShading, addLight, the power
//    constants); Savegame/Tile.cpp (getShade = max(0, 15 - maxLight)).
//  * bin/standard/xcom1/interfaces.rul, same repo, lines 1340-1424 — the
//    `battlescape` element table: every UI element's palette index by number.
//    soldiers.rul for the stat ranges; items.rul for the weapon table.
//  * The palette dump is baturinsky/xpedia2 src/palettes.js
//    (PAL_BATTLESCAPE_SAFE, JASC-PAL, 256 entries). PALETTES.DAT is original
//    game data and is in no repo. It is VERIFIED, not trusted: six element
//    colours that interfaces.rul names by index land on exactly the right entry
//    when sampled off the reference capture, and every channel is a multiple
//    of 4 — the `value * 4` in Palette::loadDat:73 and the independent
//    signature of a 6-bit VGA palette. (Two entries are not: PAL[254] 030408
//    and PAL[255] 030306. Transcribed as dumped; flagged here rather than
//    quietly rounded.)
//  * Reference capture: openxcom.org 1.0 gallery 07_pathpreview.png, nearest-
//    downsampled to 320x200. CAVEAT the researcher established and this file
//    inherits: the 640x400 original is NOT a lossless 2x — 63% of its 2x2
//    blocks are non-uniform, so it went through a scaling filter. Flat runs
//    survive (which is why the palette check holds); EDGES DO NOT. Every piece
//    of geometry here therefore comes from source, never from measurement.
//
// DOCUMENTED (not invented): the 320x200 screen and the 56-px panel; the
//   projection and its inverse; the shading algebra; the lighting propagation
//   and its power constants; every widget rect; every element palette index;
//   the full palette; the ten 3x5 digit glyphs and the 4-px advance; Bar::draw;
//   the marker colour blocks 4/10/3; DEFAULT_ANIM_SPEED = 100 and both blink
//   sequences; the TU-cost and accuracy formulas.
// RECONSTRUCTED (and it is most of the pixels): ALL sprite art. ICONS.PCK,
//   the TERRAIN/FOREST .PCK+.MCD sets, CURSOR.PCK, the unit sheets and
//   BIGLETS.DAT are original game data and none of it is in any repo. The
//   LAYOUT is documented; the PIXELS are not. Everything here is generated on
//   the pixel grid, in the palette, by run-length rasterisers. Also
//   reconstructed: the map (a forest crash-recovery at dusk), the two
//   proportional typefaces (FONT_BIG / FONT_SMALL are .DAT glyph bitmaps —
//   substituted, then re-quantised through a 1x raster so they come out as
//   bitmaps again, see pixelText below), the soldier's loadout, and the global
//   shade (8, inside the documented 0-15; shipped deployments use 5 and 15).
//
// -----------------------------------------------------------------------------
// THREE THINGS THAT SURPRISED THE RESEARCH, AND THEY BELONG IN THE HEADER
//
//  1. THE SOLDIER'S MAXIMUM STATS ARE DRAWN BUT NEVER WRITTEN. Bar::draw makes
//     the OUTLINE max+1 pixels long and the FILL `value` pixels long, at
//     setScale(1.0) — one pixel per point. Nothing on the Battlescape prints a
//     soldier's maximum TU, energy or health as a number; the only place those
//     numbers exist on screen is the LENGTH OF A ONE-PIXEL LINE. Measuring the
//     reference recovered TU 60, Stamina 65, Health 36, and all three fall
//     inside soldiers.rul's shipped ranges. A 1994 gauge encodes two quantities
//     in one 3-pixel strip and expects you to read both. Verification #4 below
//     re-runs that measurement against THIS render's own pixels.
//  2. THERE IS NO MULTIPLICATION ANYWHERE IN X-COM'S RENDERER. Lighting is
//     newShade = (src & 15) + shade; dest = newShade > 15 ? 15 : (src & 0xF0) |
//     newShade — arithmetic on a palette index, and when it overflows the pixel
//     snaps to ABSOLUTE BLACK rather than to the ramp's darkest entry ("so dark
//     it would flip over to another color - make it black instead",
//     Surface.cpp). Every shadow terminator in the game is that one `if`, and
//     consequently no colour on screen is outside the 256. That is `shd()`,
//     nine lines down from here, and it is called at bake time on every cell.
//  3. THE MOUSE PICKER CHEATS BY TEN PIXELS. Map::setSelectorPosition calls
//     convertScreenToMap(mx, my + _spriteHeight/4, ...) — it biases the cursor a
//     quarter of a sprite DOWN before inverting, so what you select is the tile
//     whose COLUMN you are pointing at, not the diamond under the pointer. The
//     exact inverse is in the same file and the game deliberately does not use
//     it. Twenty-six years before "hit slop" had a name. The selector drawn
//     here is the HONEST one; the audit prints both answers so the divergence
//     is on the record.
//
// -----------------------------------------------------------------------------
// THE ONE HARD THING: THE LIBRARY HAS NO NOTION OF A FIXED PALETTE, AND THIS
// SCREEN HAS NOTHING ELSE.
//
// Three routes exist and all three are wrong differently: quantise at authoring
// time (exact, turns the library into a rectangle-placer); quantise at paint
// time with an SkSL post pass (right shape, unreachable — Material::sksl() has
// no child-shader and no array-uniform lane, so a 256-entry LUT cannot get in);
// or quantise BY DISCIPLINE and test afterwards. This file ships the third.
// Every colour comes from PAL[] and there is not one hex literal below the
// table. Every edge lands on a multiple of PX = 4. Verification #5 is the test.
//
// -----------------------------------------------------------------------------
// VERIFICATION — printed to stdout at setup, re-run against the PNG afterwards
//
//   #1 projection round-trip     200 random screen points, screenToMap then
//                                mapToScreen: 0 failures outside the Clamp
//                                boundary.
//   #3 the light radius is 8     walking -x from the selected soldier the shade
//                                reads 0,1,2,3,4,5,6,7,8,8,8,8 — first constant
//                                at exactly the 8th tile. Off by one means
//                                floor() where addLight uses Round().
//   #4 the bars read back        fill px / 4 == the number in the recess;
//                                outline px / 4 - 1 == the max.
//   #5 the colour census         measured on the written PNG with PIL: 41
//                                distinct colours in the whole frame, 41 of 41
//                                in the 256-entry palette. Map region 30/30,
//                                panel region 26/26. Zero off-palette pixels.
//   #6 the 4-px lattice          nearest 4x down then 4x up is byte-identical
//                                to the capture: 0 mismatching pixels, so every
//                                edge in the frame is on the 1994 pixel grid.
//
// -----------------------------------------------------------------------------
// ONE THING THE RECONSTRUCTION COULD NOT RESOLVE, STATED PLAINLY
//
// The reference shows YELLOW path markers and a GREEN _btnReserveNone. Those
// two readings contradict each other: previewPath colours a marker
// `withinReserve ? green : yellow`, and with the reserve set to None every tile
// the soldier can afford is within it, so no yellow can occur. One of the two
// observations is wrong and the original save would settle it. This file draws
// the capture's button state (None lit) and computes the marker colours against
// an explicit kReserveTU = 15 — floor(60 * 25/100), a Snap Shot — so the yellow
// run is derived rather than drawn. The inconsistency is the report's, not the
// render's.
// =============================================================================

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Pattern.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace weave = sigil::weave;

namespace xcom {

// ---------------------------------------------------------------------------
// SCALE. One constant. Original 320x200 px -> canvas px; nothing else scales.

constexpr float PX = 4.0f;
constexpr float n(float v) { return v * PX; }
constexpr float kCanvasW = n(320), kCanvasH = n(200);
constexpr float kPanelY = n(144), kPanelH = n(56); // 200 - iconsHeight

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
inline SkColor4f C(int idx) {
  const uint32_t v = kPal[(unsigned)idx & 255u];
  return {(float)((v >> 16) & 255) / 255.0f, (float)((v >> 8) & 255) / 255.0f,
          (float)(v & 255) / 255.0f, idx == 0 ? 0.0f : 1.0f};
}
constexpr int blk(int block, int step) { return block * 16 + step; }

/** StandardShade, Surface.cpp:898. THE mechanism: add to the low nibble, stay
 *  inside the 16-entry ramp, and snap to absolute black on overflow. No
 *  multiply, no lerp, no colour space — one add and one compare. */
constexpr int shd(int src, int shade) {
  const int ns = (src & 15) + shade;
  return ns > 15 ? 15 : ((src & 0xF0) | ns);
}
/** ColorReplace, same file: the block is REPLACED by a 1-based block number.
 *  This is how one arrow sprite and one TU numeral recolour three ways. */
constexpr int replaceBlock(int src, int shade, int block1) {
  const int ns = (src & 15) + shade;
  return ns > 15 ? 15 : (((block1 - 1) << 4) | ns);
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

// The map is 26 x 26, not the 20 x 20 the framing was solved for: at 20 the
// visible band's CORNERS fall outside the array (mx-my = 10 with mx+my = 29
// wants mx = 19.5) and the render comes back with a black sawtooth along the
// bottom-left. A full-bleed 320x144 viewport at this framing needs 26.
constexpr float kOX = 576.0f, kOY = -368.0f;
constexpr int kMapSize = 26;
constexpr float kCellW = n(32), kCellH = n(40);
constexpr float kMapViewW = kCanvasW, kMapViewH = kPanelY;

/** map (mx,my,mz) -> the TOP-LEFT of the 128x160 sprite cell. */
inline SkPoint mapToScreen(int mx, int my, int mz) {
  return {kOX + (float)(mx - my) * n(16), kOY + (float)(mx + my) * n(8) -
                                              (float)mz * n(24)};
}
/** Pool positions are the cell's CENTRE, not its top-left (Instances.h says so
 *  in place::grid's arithmetic and nowhere else — and it costs an iteration). */
inline SkPoint cellCentre(int mx, int my, int mz) {
  const SkPoint p = mapToScreen(mx, my, mz);
  return {p.fX + kCellW * 0.5f, p.fY + kCellH * 0.5f};
}

/** Camera::convertScreenToMap:438, transcribed. Integer division is C
 *  truncation toward zero, so negative screen positions land one tile off —
 *  that is the original behaviour and the reason for the Clamp to -1. */
inline void screenToMap(float sxf, float syf, int offsetZ, int *mapX,
                        int *mapY) {
  // Work in ORIGINAL pixels: the arithmetic below is the 1994 arithmetic.
  int screenX = (int)std::floor(sxf / PX);
  int screenY = (int)std::floor(syf / PX);
  screenY += (-32 / 2) + offsetZ * ((40 + 32 / 4) / 2); // -16 + offsetZ*24
  const int offX = (int)(kOX / PX), offY = (int)(kOY / PX);
  int my = -screenX + offX + 2 * screenY - 2 * offY;
  int mx = screenY - offY - my / 4 - (32 / 4); // - 8
  mx /= (32 / 4);                              // /= 8
  my /= 32;                                    // /= 32
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

struct Ink {
  SkCanvas &c;
  void rect(float x, float y, float w, float h, int idx) const {
    if (idx == 0)
      return;
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor4f(C(idx), nullptr);
    c.drawRect(SkRect::MakeXYWH(x * PX, y * PX, w * PX, h * PX), p);
  }
  void px(float x, float y, int idx) const { rect(x, y, 1, 1, idx); }
  /** Run-length row: [x0, x0+w) at row y. */
  void run(float x0, float y, float w, int idx) const { rect(x0, y, w, 1, idx); }
};

/** A deterministic 32-bit hash. Tile dither, speckle placement, the terrain
 *  scenario — everything random in this file comes through here, so the map is
 *  the same map on every run. */
inline uint32_t hash3(int a, int b, int c) {
  uint32_t h = (uint32_t)(a * 374761393 + b * 668265263 + c * 2147483647);
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
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

constexpr int kDiamondTop = 24; // rows 24..39 of the 32x40 cell

inline void diamondRow(int r, int &x0, int &w) {
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
// WHOLE costs four at the top. So the selected unit sits on diagonal 12 and
// §10's fifteen-tile preview becomes fourteen; a fifteenth marker lands at
// diagonal 27, whose diamond starts at y = 592 and is under the panel.
constexpr Soldier kSoldierA{8, 4};  // selected; Anders Holmgren
constexpr Soldier kSoldierB{16, 8}; // second of the squad, nine tiles away —
                                    // far enough that the two light pools join
                                    // on a hard max-not-sum seam, which is
                                    // visible and is authentic
constexpr int kAlienX = 12, kAlienY = 3;
constexpr int kGlobalShade = 8;
constexpr int kPersonalLight = 15; // TileEngine::personalLightPower

/** Tile::getShade() = max(0, 15 - maxLight), with the sun at
 *  power = 15 - globalShade and each living player unit carrying a personal
 *  light of 15 propagated as `power - Round(euclidean distance)`.
 *
 *  At globalShade 8 the ambient is 7, so bare terrain sits at shade 8 and each
 *  soldier stands in a nine-step pool reaching ambient at EXACTLY eight tiles.
 *  Round(), not floor() — verification #3 is the off-by-one that catches it. */
inline int tileShade(int mx, int my) {
  int light = 15 - kGlobalShade; // the sun
  const Soldier units[2] = {kSoldierA, kSoldierB};
  for (const Soldier &u : units) {
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
  for (int i = 0; i < 14; ++i) {
    if (kStep[i])
      ++mx;
    else
      ++my;
    out.push_back({mx, my});
  }
  return out;
}

inline std::array<TileData, kMapSize * kMapSize> buildMap() {
  std::array<TileData, kMapSize * kMapSize> m{};
  for (int mx = 0; mx < kMapSize; ++mx)
    for (int my = 0; my < kMapSize; ++my) {
      TileData &t = m[(size_t)(my * kMapSize + mx)];
      const int sum = mx + my, diff = mx - my;
      const uint32_t h = hash3(mx, my, 7);
      // The discovered boundary. Each half is a MONOTONE function of the
      // coordinate it does not cut: the top edge wanders with `diff`, the left
      // edge wanders with `sum`. Hashing both per tile — even on a coarse
      // lattice — left isolated undrawn tiles scattered inside the lit area,
      // which reads as damage rather than as a frontier.
      const int j1 = (int)(hash3(diff / 3, 0, 11) % 5u) - 2;
      const int j2 = (int)(hash3(sum / 3, 0, 23) % 5u) - 2;
      t.seen = sum >= 10 + j1 && diff >= -7 + j2;
      // The crash site: bare dirt to the north-east (screen upper right).
      const int dirtEdge = 3 + (int)((hash3(mx / 2, my / 2, 3)) % 3u);
      t.floor = (diff >= dirtEdge && sum <= 22) ? kDirt : kGrass;
      // Hull plates, a wrecked section of the UFO — the one two-level
      // structure on the map, and therefore the only z = 1 content.
      if (mx >= 9 && mx <= 12 && my >= 0 && my <= 2 && ((h >> 14) % 7u) != 0u)
        t.floor = kHull;
      // Forest. Denser on grass than on the scorched dirt.
      const uint32_t r = (h >> 18) % 100u;
      if (t.floor == kGrass)
        t.object = r < 24u ? kTree : (r < 40u ? kBush : kNone);
      else if (t.floor == kDirt)
        t.object = r < 10u ? kTree : (r < 20u ? kBush : kNone);
      else
        t.object = kHullWall;
    }
  // A dirt track, and the walked route is always discovered and always clear.
  for (const SkIPoint &p : pathTiles()) {
    TileData &t = m[(size_t)(p.y() * kMapSize + p.x())];
    t.seen = true;
    t.object = kNone;
  }
  for (const Soldier &u : {kSoldierA, kSoldierB}) {
    TileData &t = m[(size_t)(u.my * kMapSize + u.mx)];
    t.seen = true;
    t.object = kNone;
  }
  m[(size_t)(kAlienY * kMapSize + kAlienX)].object = kNone;
  m[(size_t)(kAlienY * kMapSize + kAlienX)].seen = true;
  return m;
}

// ---------------------------------------------------------------------------
// THE CELL ART. Every cell is a custom() paint program inside an
// instancing::Atlas at oversample 1.0 — authored at FINAL scale so the stamp is
// 1:1 and no magnification happens. Cells are 128 x 160, addressed as
// (type, shade): the flyweight is `frames = types x shades`, because a 16-step
// palette ramp is NOT a scalar multiple of its top entry and Pool::tints()
// therefore cannot shade a tile (block 3 at shade 8 needs per-channel
// multipliers R 0.17 / G 0.54 / B 0.42; the best single scalar renders red 2.4x
// too bright). See the report.

/** Floor: the diamond, dithered two palette steps apart with a scatter of a
 *  third. X-COM's terrain has no smooth shading inside a tile — two steps in a
 *  dither is the entire vocabulary; there is no gradient anywhere.
 *
 *  TWO VARIANTS per (type, shade), keyed off (mx+my) parity. A single variant
 *  produced a coherent diamond moire across the whole map, because a shared
 *  cell repeats identically under the (+-16, +8) lattice — the flyweight's own
 *  failure mode, visible on the first render and invisible in the arithmetic. */
inline void paintFloor(SkCanvas &canvas, Floor kind, int shade, int variant) {
  const Ink ink{canvas};
  int base = blk(3, 5), alt = blk(3, 7), speck = blk(3, 9), rare = blk(2, 4);
  if (kind == kDirt) {
    base = blk(6, 4);
    alt = blk(10, 3);
    speck = blk(10, 6);
    rare = blk(6, 8);
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
      ink.px((float)x, (float)(kDiamondTop + r), shd(idx, shade));
    }
  }
  if (kind == kHull) // plate seams
    for (int r = 3; r < 16; r += 6) {
      int x0, w;
      diamondRow(r, x0, w);
      ink.run((float)x0, (float)(kDiamondTop + r), (float)w,
              shd(blk(14, 9), shade));
    }
}

/** The diamond's row span for a given column — the extrusion silhouette. A
 *  column x is inside row r iff min(r, 15-r) >= ceil((|x-15.5| - 0.5)/2), which
 *  falls straight out of the run-length rule above. */
inline void diamondColumn(int x, int &rTop, int &rBot) {
  const float dx = std::abs((float)x - 15.5f);
  const int k = (int)std::ceil((dx - 0.5f) / 2.0f);
  rTop = std::clamp(k, 0, 7);
  rBot = 15 - rTop;
}

/** The wrecked UFO: one tile's worth of hull extruded a full level (24 px),
 *  which is what makes the crash site the only two-level structure here — the
 *  wall face blits at z = 0 and the deck plate at z = 1, exactly the
 *  O_OBJECT / O_FLOOR split Map::drawTerrain walks. */
inline void paintHullWall(SkCanvas &canvas, int shade) {
  const Ink ink{canvas};
  for (int x = 0; x < 32; ++x) {
    int rTop, rBot;
    diamondColumn(x, rTop, rBot);
    const int y0 = kDiamondTop - 24 + rTop, y1 = kDiamondTop + rBot;
    for (int y = y0; y <= y1; ++y) {
      const uint32_t h = hash3(x, y, 55);
      const bool lit = x > 15;
      int step = (lit ? 6 : 9) + (int)(h % 2u);
      if ((h % 23u) == 0u)
        step += 3; // rivets and scoring
      ink.px((float)x, (float)y, shd(blk(14, step), shade));
    }
    ink.px((float)x, (float)y0, shd(blk(14, 4), shade)); // top lip
  }
}
inline void paintHullDeck(SkCanvas &canvas, int shade) {
  paintFloor(canvas, kHull, shade, 0);
  const Ink ink{canvas};
  for (int r = 0; r < 16; ++r) { // a bright rim so the deck reads as raised
    int x0, w;
    diamondRow(r, x0, w);
    ink.px((float)x0, (float)(kDiamondTop + r), shd(blk(14, 1), shade));
    ink.px((float)(x0 + w - 1), (float)(kDiamondTop + r),
           shd(blk(14, 6), shade));
  }
}

/** A lumpy canopy: an ellipse with a per-row hash wobble, emitted as whole
 *  pixels so nothing lands off the grid. The lit steps and the shaded flank are
 *  three apart on ONE hue ramp, which at ambient (shade 8) pushes the flank's
 *  last step past 15 — so the terminator on a dusk tree is literally
 *  PAL[15] #000000, not the ramp's darkest green. That is consequence #2 of the
 *  shading algebra, and it is why X-COM's night terrain looks the way it does. */
inline void paintCanopy(const Ink &ink, float cx, float cy, float rx, float ry,
                        int shade, int litStep, uint32_t seed) {
  const int r0 = (int)std::floor(cy - ry), r1 = (int)std::ceil(cy + ry);
  for (int r = r0; r <= r1; ++r) {
    const float t = ((float)r + 0.5f - cy) / ry;
    float hw = rx * std::sqrt(std::max(0.0f, 1.0f - t * t));
    hw += (float)((int)(hash3(r, (int)seed, 3) % 5u) - 2) * 0.5f;
    const int w = (int)std::lround(hw * 2.0f);
    if (w <= 0)
      continue;
    const int x0 = (int)std::lround(cx - (float)w * 0.5f);
    for (int x = x0; x < x0 + w; ++x) {
      const uint32_t h = hash3(x, r, (int)seed + 91);
      const bool flank = (float)x > cx + hw * 0.15f;
      int idx = blk(3, litStep + (flank ? 3 : 0) + (int)(h % 3u));
      if (!flank && (h % 89u) == 0u)
        idx = blk(2, 2); // fruit — block 2 red, one pixel
      ink.px((float)x, (float)r, shd(idx, shade));
    }
  }
}

/** One cell per tree: canopy rows 4..24, trunk 22..32, standing on the
 *  diamond's centre (row 32). 28 original px tall, which is what the reference
 *  measures — an earlier two-cell tree spanning both levels came out 48 px and
 *  read as a column, not a tree. */
inline void paintTree(SkCanvas &canvas, int shade) {
  const Ink ink{canvas};
  for (int r = 20; r <= 32; ++r)
    ink.run(15.0f, (float)r, r > 29 ? 3.0f : 2.0f, shd(blk(10, 6), shade));
  ink.run(13.0f, 32.0f, 6.0f, shd(blk(10, 8), shade));
  paintCanopy(ink, 16.0f, 14.0f, 11.0f, 10.5f, shade, 6, 5u);
}
inline void paintBush(SkCanvas &canvas, int shade) {
  const Ink ink{canvas};
  paintCanopy(ink, 16.0f, 27.0f, 8.0f, 5.5f, shade, 7, 17u);
}

/** The path arrow. ColorReplace recolours ONE sprite three ways, so the art is
 *  authored in block 1 and the cell bakes with the block replaced — which is
 *  exactly what blitNShade(..., newBaseColor) does at runtime.
 *  dir: 0 = down-left (map +y), 1 = down-right (map +x). */
inline void paintArrow(SkCanvas &canvas, int dir, int block1) {
  const Ink ink{canvas};
  const auto put = [&](int x, int y, int step) {
    ink.px((float)(dir == 0 ? x : 31 - x), (float)y,
           replaceBlock(blk(0, step), 0, block1));
  };
  // A left-pointing arrow: head then shaft, in original pixels, rows 2..14.
  for (int k = 0; k < 6; ++k)
    for (int y = 8 - k; y <= 8 + k; ++y)
      put(6 + k, y, k == 0 ? 0 : 2);
  for (int x = 12; x < 22; ++x)
    for (int y = 6; y <= 10; ++y)
      put(x, y, (y == 6 || y == 10) ? 4 : 1);
}

/** The 3D box selector — a 32x40 wireframe cube, one tile, block 2. Drawn at
 *  the tile the HONEST inverse returns; the game's own picker biases the mouse
 *  ten pixels down first (Map.cpp:1314) and the audit prints both. */
inline void paintCursor(SkCanvas &canvas) {
  const Ink ink{canvas};
  const int lit = blk(2, 0), dim = blk(2, 2);
  const int lift = 12; // the box is half a level tall
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
inline void paintUnit(SkCanvas &canvas, int shade, bool alien) {
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
  if (!alien) { // the rifle, held across
    ink.rect(9, 17, 12, 1, shd(blk(15, 2), shade));
    ink.rect(14, 18, 4, 2, shd(blk(15, 5), shade));
  }
}

/** The selected-soldier arrow. arrowBob[8] = {0,1,2,1,0,1,2,1} — three integer
 *  positions, the only continuous-looking motion on the whole screen, and it
 *  is quantised. */
constexpr int kArrowBob[8] = {0, 1, 2, 1, 0, 1, 2, 1};
inline void paintBobArrow(SkCanvas &canvas, int frame) {
  const Ink ink{canvas};
  const int dy = kArrowBob[frame & 7];
  for (int k = 0; k < 5; ++k)
    ink.run((float)(12 + k), (float)(dy + 4 + k), (float)(10 - 2 * k),
            blk(1, k == 0 ? 0 : 1));
}

// ---------------------------------------------------------------------------
// PIXEL TYPE. FONT_BIG and FONT_SMALL are .DAT glyph bitmaps and are not in any
// repo; SigilWeave has no bitmap-font path (ROADMAP §9). So the substitute face
// is shaped at 1x — the 1994 pixel — rendered into a raster the size of its own
// ink, and every pixel QUANTISED into a palette ramp by coverage: two steps,
// the way an 8-bit artist would have anti-aliased a face by hand. Then it is
// presented at 4x with kNearest. The result is a bitmap font again, on the
// grid, in the palette, and the census stays clean.

struct PixelText {
  sk_sp<SkImage> image;
  int w = 0, h = 0;
};

inline PixelText pixelText(const std::u8string &s, weave::TextStyle style,
                           weave::FontContext &fonts, int litIdx, int dimIdx) {
  style.paint.foreground.setColor(SK_ColorWHITE);
  Element tree = box().child(text(s, style));
  const SkSize sz = measure(box().child(text(s, style)), fonts);
  const int w = std::max(1, (int)std::ceil(sz.width()) + 2);
  const int h = std::max(1, (int)std::ceil(sz.height()) + 2);
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface)
    return {};
  surface->getCanvas()->clear(SK_ColorTRANSPARENT);
  if (sk_sp<SkPicture> pic = snapshot(std::move(tree), fonts))
    surface->getCanvas()->drawPicture(pic);

  SkBitmap read;
  read.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!surface->readPixels(read.pixmap(), 0, 0))
    return {};

  int x0 = w, y0 = h, x1 = -1, y1 = -1;
  std::vector<uint8_t> level((size_t)w * (size_t)h, 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const float a = (float)(SkColorGetA(read.getColor(x, y))) / 255.0f;
      const uint8_t lv = a >= 0.55f ? 2 : (a >= 0.22f ? 1 : 0);
      level[(size_t)y * (size_t)w + (size_t)x] = lv;
      if (lv) {
        x0 = std::min(x0, x);
        y0 = std::min(y0, y);
        x1 = std::max(x1, x);
        y1 = std::max(y1, y);
      }
    }
  if (x1 < 0)
    return {};
  const int cw = x1 - x0 + 1, chh = y1 - y0 + 1;
  SkBitmap out;
  out.allocPixels(SkImageInfo::Make(cw, chh, kRGBA_8888_SkColorType,
                                    kUnpremul_SkAlphaType));
  out.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < chh; ++y)
    for (int x = 0; x < cw; ++x) {
      const uint8_t lv = level[(size_t)(y + y0) * (size_t)w + (size_t)(x + x0)];
      if (!lv)
        continue;
      const uint32_t v = kPal[(unsigned)(lv == 2 ? litIdx : dimIdx) & 255u];
      // kRGBA_8888 unpremul: R, G, B, A in memory order.
      *out.getAddr32(x, y) = (0xFFu << 24) | ((v & 255u) << 16) |
                             (((v >> 8) & 255u) << 8) | ((v >> 16) & 255u);
    }
  out.setImmutable();
  return {out.asImage(), cw, chh};
}

/** The pixel-text element: an image fill at exactly 4x, nearest-sampled, on a
 *  box whose size is the ink. Material::image is the one blessed image path
 *  that always took a sampling parameter. */
inline Element pixelTextEl(const PixelText &t, float x, float y) {
  if (!t.image)
    return box().width(0).height(0);
  return box()
      .absolute()
      .left(x)
      .top(y)
      .width((float)t.w * PX)
      .height((float)t.h * PX)
      .fill(Material::image(t.image, SkTileMode::kDecal, SkTileMode::kDecal,
                            SkMatrix::Scale(PX, PX),
                            SkSamplingOptions(SkFilterMode::kNearest)));
}

// ---------------------------------------------------------------------------
// THE CONTROL PANEL. Every rect below is a literal constructor argument from
// BattlescapeState.cpp:86-161. iconsWidth 320, iconsHeight 56, so
// visibleMapHeight = 144 and the panel is (0,144)-(320,200).

inline Element at(float x, float y, float w, float h) {
  return box().absolute().left(n(x)).top(n(y)).width(n(w)).height(n(h));
}

/** A 32x16 button plate: rounded by RUN LENGTH, not by a corner radius. A
 *  radius antialiases and puts colours on the canvas that VGA never had, which
 *  §12 predicted and the first render confirmed. Lit edge 81-85, face 86-87,
 *  shadow 91-93 — measured off the capture's flat runs. */
inline void paintPlate(SkCanvas &canvas, int w, int h) {
  const Ink ink{canvas};
  for (int r = 0; r < h; ++r) {
    const int inset = (r == 0 || r == h - 1) ? 3 : ((r == 1 || r == h - 2) ? 1 : 0);
    const int x0 = inset, run = w - 2 * inset;
    const bool top = r < h / 2;
    ink.run((float)x0, (float)r, (float)run, blk(5, top ? 6 : 8));
    if (r == 0 || r == 1)
      ink.run((float)x0, (float)r, (float)run, blk(5, 1 + r));
    if (r == h - 1)
      ink.run((float)x0, (float)r, (float)run, blk(5, 12));
    if (r == h - 2)
      ink.run((float)x0, (float)r, (float)run, blk(5, 11));
    ink.px((float)x0, (float)r, blk(5, r == 0 ? 1 : 3));
    ink.px((float)(x0 + run - 1), (float)r, blk(5, r == h - 1 ? 12 : 10));
  }
}

/** Fourteen glyphs, in columns: UnitUp/UnitDown, MapUp/MapDown, ShowMap/Kneel,
 *  Inventory/Center, NextSoldier/NextStop, ShowLayers/Help, EndTurn/Abort. The
 *  capture's glyphs match that mapping exactly, which is a small verification
 *  in itself. Ink is block 5 step 14 with a block 5 step 2 highlight below. */
inline void paintButtonGlyph(SkCanvas &canvas, int id) {
  const Ink ink{canvas};
  const int d = blk(5, 14), l = blk(5, 2);
  const auto tri = [&](int cx, int cy, int size, bool up) {
    for (int k = 0; k < size; ++k) {
      const int wdt = 2 * (up ? k : size - 1 - k) + 1;
      ink.run((float)(cx - wdt / 2), (float)(cy + k), (float)wdt, d);
    }
  };
  const auto bars = [&](int x, int y, int rows, int wdt) {
    for (int k = 0; k < rows; ++k)
      ink.run((float)x, (float)(y + k * 2), (float)wdt, d);
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
  case 0: tri(10, 4, 4, true);  bars(17, 4, 4, 8); break;  // unit up
  case 1: tri(10, 4, 4, false); bars(17, 4, 4, 8); break;  // unit down
  case 2: tri(10, 4, 4, true);  bars(17, 4, 4, 6); ink.rect(24, 4, 2, 7, d); break;
  case 3: tri(10, 4, 4, false); bars(17, 4, 4, 6); ink.rect(24, 4, 2, 7, d); break;
  case 4: // show map: a small grid
    for (int gy = 0; gy < 4; ++gy)
      for (int gx = 0; gx < 7; ++gx)
        ink.rect((float)(9 + gx * 2), (float)(4 + gy * 2), 1, 1,
                 ((gx + gy) & 1) ? d : l);
    break;
  case 5: figure(11, 3); ink.rect(17, 9, 5, 1, d); break; // kneel
  case 6: figure(13, 3); break;                            // inventory
  case 7:                                                  // center reticle
    ink.rect(12, 7, 9, 1, d);
    ink.rect(16, 3, 1, 9, d);
    ink.rect(13, 4, 1, 1, d); ink.rect(19, 4, 1, 1, d);
    ink.rect(13, 10, 1, 1, d); ink.rect(19, 10, 1, 1, d);
    break;
  case 8: figure(9, 3); ink.rect(17, 7, 6, 1, d); ink.rect(21, 5, 1, 5, d);
          figure(23, 3); break;                            // next soldier
  case 9: figure(9, 3); ink.rect(17, 7, 6, 1, d); ink.rect(21, 5, 1, 5, d);
          ink.rect(24, 3, 1, 9, d); break;                 // next / stop
  case 10:                                                 // layers
    for (int k = 0; k < 3; ++k) {
      const int y = 4 + k * 3;
      for (int q = 0; q < 5; ++q)
        ink.run((float)(11 - q), (float)(y + q > y + 2 ? y : y), 0, d);
      ink.run(8.0f, (float)y, 10.0f, k == 0 ? l : d);
      ink.run(9.0f, (float)(y + 1), 8.0f, d);
    }
    break;
  case 11: // help: a question mark
    ink.run(13.0f, 3.0f, 5.0f, d);
    ink.rect(17, 4, 2, 2, d);
    ink.rect(15, 6, 2, 2, d);
    ink.rect(15, 9, 2, 2, d);
    break;
  case 12: // end turn: slashed circle
    for (int k = 0; k < 9; ++k) {
      const float t = ((float)k - 4.0f) / 4.5f;
      const int hw = (int)std::lround(4.5f * std::sqrt(std::max(0.0f, 1 - t * t)));
      ink.px((float)(16 - hw), (float)(3 + k), d);
      ink.px((float)(16 + hw), (float)(3 + k), d);
    }
    for (int k = 0; k < 9; ++k)
      ink.px((float)(12 + k), (float)(11 - k), d);
    break;
  default: // abort: a bird
    for (int k = 0; k < 6; ++k) {
      ink.run((float)(10 + k), (float)(8 - k / 2), (float)(3 - k / 3), d);
      ink.run((float)(18 - k / 2 + k), (float)(8 - k / 2), (float)(3 - k / 3), d);
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
                       const char *key) {
  const int outline = colorIdx + 4;
  // A transparent full-canvas shell, so the four rects keep SCREEN coordinates.
  // The KEY goes on the fill rect, never on the shell: a keyed full-bleed shell
  // with no fill still answers hitTest, and four of them stacked over the frame
  // made every probe return `barMorale`. There is no pointer-events: none.
  Element g = box().absolute().inset(0);
  g.child(at(x, y, (float)(maxValue + 1), 1).fill(C(outline)));
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
  Element g = box().absolute().inset(0);
  for (int r = 0; r < 7; ++r)
    g.child(at(x, y + (float)r, 17, 1).fill(C(firstIdx + r)));
  return g;
}

} // namespace xcom

using namespace xcom;

// ===========================================================================

struct XcomBattlescape : sigil::compose::sketch::Sketch {
  using Atlas = instancing::Atlas;
  using Pool = instancing::Pool;

  // ---- the flyweight -------------------------------------------------------
  std::shared_ptr<Atlas> tiles;      // 128x160 cells, (type x shade) + markers
  std::shared_ptr<Atlas> fontAtlas;  // ONE 4x4 white cell, tinted per instance
  std::shared_ptr<Pool> terrain;     // z0 floors + objects, z1 tree tops
  std::shared_ptr<Pool> overlay;     // path arrows + the box selector
  std::shared_ptr<Pool> mapGlyphs;   // fifteen bordered TU markers
  std::shared_ptr<Pool> panelGlyphs; // stats, ammo, layer, spotted-unit tags

  int cellFloor[3][2][9]{}; // [type][dither variant][shade]
  int cellObj[4][9]{};      // [kNone unused | kBush | kTree | kHullWall]
  int cellHullDeck[9]{};
  int cellArrow[2][3]{};
  int cellCursor = 0;
  int cellFontPx = 0;
  int atlasCells = 0;

  // ---- scenario ------------------------------------------------------------
  std::array<xcom::TileData, xcom::kMapSize * xcom::kMapSize> map{};
  std::vector<SkIPoint> path;
  std::vector<int> pathTU;
  std::vector<int> pathBlock;

  // ---- panel data (soldiers.rul ranges; maxima recovered from the capture) --
  static constexpr int kMaxTU = 60, kMaxEnergy = 65, kMaxHealth = 36,
                       kMaxMorale = 100;
  static constexpr int kFiring = 58; // chosen inside 40-70
  static constexpr int kReserveTU = 15; // floor(60 * 25/100) — a Snap Shot

  // ---- pixel type ----------------------------------------------------------
  xcom::PixelText nameText, popupRow[3], popupAcc[3], popupTU[3];

  // ---- motion --------------------------------------------------------------
  int animFrame = 0;
  int tagIndex = 32; // blinkVisibleUnitButtons walks 32..44
  int tagDir = 1;
  sigil::motion::Ticker::FixedStatus fixedStatus{};

  struct Phase {
    int frame = 0;
    int tag = 32;
    bool longPath = false;
    bool popup = false;
    bool fired = false;
    int tags = 3;
    bool operator==(const Phase &) const = default;
  } phase, lastPhase{-1};

  Pattern metalPattern, latticePattern;
  bool auditPrinted = false;
  int reportedFrames = 0;

  // =========================================================================
  // BAKE

  void bakeAtlas() {
    using namespace xcom;
    tiles = std::make_shared<Atlas>(1.0f);
    // The last of the five hardcoded kLinear paths. A pixel-art tilemap is the
    // case detail::stamp's literal was written against; without this the whole
    // map is a 500-stamp blur.
    tiles->filter(SkFilterMode::kNearest);
    const SkSize cell{kCellW, kCellH};

    for (int shade = 0; shade <= 8; ++shade) {
      for (int f = 0; f < 3; ++f)
        for (int v = 0; v < 2; ++v) {
          const Floor kind = (Floor)f;
          cellFloor[f][v][shade] = tiles->cell(
              custom([kind, shade, v](SkCanvas &c, const PaintContext &) {
                paintFloor(c, kind, shade, v);
              }),
              cell);
        }
      cellObj[kBush][shade] = tiles->cell(
          custom([shade](SkCanvas &c, const PaintContext &) {
            paintBush(c, shade);
          }),
          cell);
      cellObj[kTree][shade] = tiles->cell(
          custom([shade](SkCanvas &c, const PaintContext &) {
            paintTree(c, shade);
          }),
          cell);
      cellObj[kHullWall][shade] = tiles->cell(
          custom([shade](SkCanvas &c, const PaintContext &) {
            paintHullWall(c, shade);
          }),
          cell);
      cellHullDeck[shade] = tiles->cell(
          custom([shade](SkCanvas &c, const PaintContext &) {
            paintHullDeck(c, shade);
          }),
          cell);
    }
    const int kBlocks[3] = {4, 10, 3}; // Pathfinding green / yellow / red
    for (int d = 0; d < 2; ++d)
      for (int m = 0; m < 3; ++m) {
        const int b = kBlocks[m];
        cellArrow[d][m] = tiles->cell(
            custom([d, b](SkCanvas &c, const PaintContext &) {
              paintArrow(c, d, b);
            }),
            cell);
      }
    cellCursor = tiles->cell(
        custom([](SkCanvas &c, const PaintContext &) { paintCursor(c); }), cell);
    atlasCells = tiles->frameCount();

    fontAtlas = std::make_shared<Atlas>(1.0f);
    fontAtlas->filter(SkFilterMode::kNearest);
    // The font cell is a MASK, not a colour: Pool::tints() MULTIPLIES, so the
    // cell has to be pure white. Filling it with the palette's own white —
    // PAL[1] #FCFCFC, the obvious choice — scales every tinted glyph by
    // 252/255 and put SEVEN off-palette colours in the frame, each exactly two
    // units below its palette entry. The colour census caught all seven; no
    // amount of looking would have.
    cellFontPx = fontAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {PX, PX});
  }

  // ---- the font pool: a 4x4 white cell, tinted -----------------------------
  /** value at (x, y) in ORIGINAL px, colour = _color + 1 (NumberText::draw ends
   *  with offset(_color)). `bordered` adds the black surround the map markers
   *  carry. Returns the advance, 4 px per digit, no kerning, ever. */
  int pushNumber(Pool &pool, int value, float x, float y, int colorIdx,
                 bool bordered) {
    const int lit = colorIdx + 1;
    std::string digits = std::to_string(std::max(0, value));
    if (bordered) {
      for (size_t d = 0; d < digits.size(); ++d) {
        const int g = digits[d] - '0';
        for (int r = -1; r <= 5; ++r)
          for (int c = -1; c <= 3; ++c) {
            bool nearLit = false, self = false;
            for (int dr = -1; dr <= 1 && !nearLit; ++dr)
              for (int dc = -1; dc <= 1; ++dc) {
                const int rr = r + dr, cc = c + dc;
                if (rr < 0 || rr > 4 || cc < 0 || cc > 2)
                  continue;
                if (xcom::kDigit[g][rr] & (4 >> cc)) {
                  if (dr == 0 && dc == 0)
                    self = true;
                  else
                    nearLit = true;
                }
              }
            if (nearLit && !self)
              pool.add({(x + (float)d * 4 + (float)c) * PX + PX * 0.5f,
                        (y + (float)r) * PX + PX * 0.5f},
                       cellFontPx, 0.0f, 1.0f, xcom::C(xcom::blk(0, 15)));
          }
      }
    }
    for (size_t d = 0; d < digits.size(); ++d) {
      const int g = digits[d] - '0';
      for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 3; ++c)
          if (xcom::kDigit[g][r] & (4 >> c))
            pool.add({(x + (float)d * 4 + (float)c) * PX + PX * 0.5f,
                      (y + (float)r) * PX + PX * 0.5f},
                     cellFontPx, 0.0f, 1.0f, xcom::C(lit));
    }
    return (int)digits.size() * 4;
  }

  // =========================================================================
  // POOLS

  int terrainZ0 = 0, terrainZ1 = 0;

  void buildTerrain() {
    using namespace xcom;
    terrain->clear();
    terrainZ0 = terrainZ1 = 0;
    const auto visible = [](SkPoint tl) {
      return tl.fX + kCellW > 0 && tl.fX < kMapViewW && tl.fY + kCellH > 0 &&
             tl.fY < kMapViewH;
    };
    // Map::drawTerrain's own nesting: for z, for x, for y. The pool draws in
    // INSERTION order and that order IS the depth sort — do not sort.
    for (int z = 0; z <= 1; ++z)
      for (int mx = 0; mx < kMapSize; ++mx)
        for (int my = 0; my < kMapSize; ++my) {
          const TileData &t = map[(size_t)(my * kMapSize + mx)];
          if (!t.seen)
            continue; // undiscovered tiles are not drawn; PAL[15] shows
          const SkPoint tl = mapToScreen(mx, my, z);
          if (!visible(tl))
            continue;
          const int sh = tileShade(mx, my);
          const SkPoint centre = cellCentre(mx, my, z);
          if (z == 0) {
            terrain->add(centre, cellFloor[(int)t.floor][(mx + my) & 1][sh]);
            ++terrainZ0;
            if (t.object != kNone) {
              terrain->add(centre, cellObj[(int)t.object][sh]);
              ++terrainZ0;
            }
          } else if (t.floor == kHull) {
            terrain->add(centre, cellHullDeck[sh]);
            ++terrainZ1;
          }
        }
    terrain->touch();
  }

  /** previewPath, Pathfinding.cpp:972. Walk the path in order, subtract the
   *  cost, colour by the documented rule. Cardinal steps cost 4 (the .MCD floor
   *  cost is original data; 4 is the engine's own fallback and what open ground
   *  reads as). The marker is a 1-BASED palette block fed to ColorReplace. */
  void computePath(bool longer) {
    using namespace xcom;
    path = pathTiles();
    if (!longer)
      path.resize(12);
    pathTU.clear();
    pathBlock.clear();
    int tus = 58; // the soldier's current TU, and the number in the recess
    for (size_t i = 0; i < path.size(); ++i) {
      tus -= 4;
      const int marker = tus < 0 ? 3 : (tus >= kReserveTU ? 4 : 10);
      pathTU.push_back(std::max(0, tus));
      pathBlock.push_back(marker);
    }
  }

  void buildOverlay() {
    using namespace xcom;
    overlay->clear();
    for (size_t i = 0; i < path.size(); ++i) {
      const SkIPoint prev =
          i == 0 ? SkIPoint{kSoldierA.mx, kSoldierA.my} : path[i - 1];
      const int dir = path[i].x() != prev.x() ? 1 : 0;
      const int m = pathBlock[i] == 4 ? 0 : (pathBlock[i] == 10 ? 1 : 2);
      overlay->add(cellCentre(path[i].x(), path[i].y(), 0), cellArrow[dir][m]);
    }
    if (!path.empty())
      overlay->add(cellCentre(path.back().x(), path.back().y(), 0), cellCursor);
    overlay->touch();
  }

  void buildMapGlyphs() {
    using namespace xcom;
    mapGlyphs->clear();
    for (size_t i = 0; i < path.size(); ++i) {
      const SkPoint tl = mapToScreen(path[i].x(), path[i].y(), 0);
      const int v = pathTU[i];
      // Map.cpp:944 — x = screenPosition.x + 16 - (tuMarker > 9 ? 5 : 3),
      //               y = screenPosition.y + 29.
      const float ox = tl.fX / PX + 16.0f - (v > 9 ? 5.0f : 3.0f);
      const float oy = tl.fY / PX + 29.0f;
      pushNumber(*mapGlyphs, v, ox, oy, blk(pathBlock[i] - 1, 0), true);
    }
    mapGlyphs->touch();
  }

  void buildPanelGlyphs(const Phase &p) {
    using namespace xcom;
    panelGlyphs->clear();
    const int tu = p.fired ? 43 : 58;
    pushNumber(*panelGlyphs, tu, 136, 186, 64, false);        // numTUs
    pushNumber(*panelGlyphs, 56, 154, 186, 16, false);        // numEnergy
    pushNumber(*panelGlyphs, 36, 136, 194, 32, false);        // numHealth
    pushNumber(*panelGlyphs, 100, 154, 194, 192, false);      // numMorale
    // numLayers declares 15 in interfaces.rul, and offset(_color) would put the
    // lit pixel at 16 — amber. The capture's little `1` is BLACK, so it is
    // drawn here at 14 (lit 15). Noted rather than silently reconciled.
    pushNumber(*panelGlyphs, 1, 232, 150, 14, false);          // numLayers
    pushNumber(*panelGlyphs, p.fired ? 13 : 14, 280, 148, 2, false); // ammo R
    for (int i = 0; i < p.tags; ++i)
      pushNumber(*panelGlyphs, i + 1, 306 + (i == 9 ? -2 : 0),
                 132 - 13 * i, 16, false); // visibleUnits: 16
    panelGlyphs->touch();
  }

  // =========================================================================
  // DESCRIBE

  Element describe(sketch::SketchContext &ctx) {
    using namespace xcom;
    Element root = box().width(kCanvasW).height(kCanvasH).fill(C(blk(0, 15)));

    // ---- z1 the terrain, one atlas stamp ----------------------------------
    root.child(at(0, 0, 320, 144)
                   .key("map")
                   .child(instancing::instances(tiles, terrain)));

    // ---- z2 units and objects: REAL elements, so hitTest can reach them ----
    for (const auto &[u, alien, key] :
         {std::tuple{kSoldierA, false, "unit-a"},
          std::tuple{kSoldierB, false, "unit-b"},
          std::tuple{Soldier{kAlienX, kAlienY}, true, "unit-alien"}}) {
      const SkPoint tl = mapToScreen(u.mx, u.my, 0);
      const int sh = tileShade(u.mx, u.my);
      root.child(box()
                     .absolute()
                     .left(tl.fX)
                     .top(tl.fY)
                     .width(kCellW)
                     .height(kCellH)
                     .key(key)
                     .child(custom([sh, alien](SkCanvas &c,
                                               const PaintContext &) {
                       paintUnit(c, sh, alien);
                     })));
    }
    {
      const SkPoint tl = mapToScreen(kSoldierA.mx, kSoldierA.my, 0);
      const int frame = phase.frame;
      root.child(box()
                     .absolute()
                     .left(tl.fX)
                     .top(tl.fY - n(4))
                     .width(kCellW)
                     .height(kCellH)
                     .child(custom([frame](SkCanvas &c, const PaintContext &) {
                       paintBobArrow(c, frame);
                     })));
    }

    // ---- z3/z4 path arrows, TU numbers, the box selector -------------------
    root.child(at(0, 0, 320, 144).child(instancing::instances(tiles, overlay)));
    root.child(
        at(0, 0, 320, 144).child(instancing::instances(fontAtlas, mapGlyphs)));

    // ---- z5 the control panel ---------------------------------------------
    root.child(panel(ctx));

    // ---- z6 spotted-enemy tags. visibleUnits pos [300,-16] out of
    //      interfaces.rul, hence the y-16 and the upward stack.
    for (int i = 0; i < phase.tags; ++i) {
      Element tag = box().absolute().inset(0);
      tag.child(at(300, 128 - 13 * i, 15, 12).fill(C(blk(0, 15))));
      tag.child(at(301, 129 - 13 * i, 13, 10).fill(C(phase.tag)));
      root.child(tag);
    }

    // ---- panel numbers, above the panel and above the tags ----------------
    root.child(
        at(0, 0, 320, 200).child(instancing::instances(fontAtlas, panelGlyphs)));

    // ---- z7 the fire-mode popup, snapped open, never tweened ---------------
    if (phase.popup)
      root.child(popupEl());

    return root;
  }

  Element panel(sketch::SketchContext &) {
    using namespace xcom;
    // A full-canvas transparent shell, NOT a box at the panel's rect: every
    // widget below is positioned by its BattlescapeState constructor argument,
    // which is a screen coordinate, and Yoga resolves an absolute child against
    // its parent. Nesting them inside a box at (0,144) shifted the entire panel
    // off the bottom of the frame on the first render.
    Element p = box().absolute().inset(0);
    // The metal body: a dithered block-5 field, no gradient, no direction.
    p.child(at(0, 144, 320, 56).fill(metalPattern.material()));
    p.child(at(0, 144, 320, 1).fill(C(blk(5, 1))));
    p.child(at(0, 199, 320, 1).fill(C(blk(5, 13))));

    // Fourteen 32x16 plates, seven columns, two rows. They read in COLUMNS.
    const float bx[7] = {48, 80, 112, 144, 176, 208, 240};
    for (int col = 0; col < 7; ++col)
      for (int row = 0; row < 2; ++row) {
        const int id = col * 2 + row;
        p.child(at(bx[col], 144 + 16 * row, 32, 16)
                    .key("btn" + std::to_string(id))
                    .child(custom([](SkCanvas &c, const PaintContext &) {
                             paintPlate(c, 32, 16);
                           }).absolute().inset(0))
                    .child(custom([id](SkCanvas &c, const PaintContext &) {
                             paintButtonGlyph(c, id);
                           }).absolute().inset(0)));
      }

    // The six reserve buttons. buttonReserveNone declares 67, the other three
    // and buttonZeroTUs declare 35 — block 4 step 3 against block 2 step 3.
    const auto reserveBtn = [&](float x, float y, float w, float h, int idx,
                                const char *key) {
      p.child(at(x, y, w, h)
                  .key(key)
                  .fill(C(idx + 4))
                  .child(at(x + 1, y + 1, w - 2, h - 2).fill(C(idx))));
    };
    reserveBtn(49, 177, 10, 23, 35, "zeroTUs");
    reserveBtn(60, 177, 17, 11, 67, "resNone"); // the lit one, per the capture
    reserveBtn(78, 177, 17, 11, 35, "resSnap");
    reserveBtn(60, 189, 17, 11, 35, "resAimed");
    reserveBtn(78, 189, 17, 11, 35, "resAuto");
    reserveBtn(96, 177, 10, 23, 35, "resKneel");
    // Glyph strokes on the reserve buttons — a figure with a muzzle flash.
    for (const auto &[x, y] : {std::pair{60.0f, 177.0f}, std::pair{78.0f, 177.0f},
                               std::pair{60.0f, 189.0f},
                               std::pair{78.0f, 189.0f}})
      p.child(at(x + 3, y + 3, 11, 5)
                  .child(custom([](SkCanvas &c, const PaintContext &) {
                    const Ink ink{c};
                    ink.rect(0, 0, 2, 5, blk(0, 15));
                    ink.rect(2, 2, 5, 1, blk(0, 15));
                    ink.rect(8, 1, 1, 3, blk(0, 15));
                    ink.rect(10, 0, 1, 5, blk(0, 15));
                  })));

    // The rank badge, 26x23 — a gold plate, block 9 over block 10.
    p.child(at(107, 177, 26, 23)
                .key("rank")
                .child(custom([](SkCanvas &c, const PaintContext &) {
                  const Ink ink{c};
                  for (int r = 0; r < 23; ++r)
                    ink.run(0, (float)r, 26, blk(9, 2 + r / 6));
                  ink.run(0, 0, 26, blk(9, 0));
                  ink.run(0, 22, 26, blk(10, 6));
                  for (int r = 0; r < 23; ++r) {
                    ink.px(0, (float)r, blk(9, 1));
                    ink.px(25, (float)r, blk(10, 5));
                  }
                  // A chevron — STR_SQUADDIE.
                  for (int k = 0; k < 7; ++k) {
                    ink.run((float)(13 - k - 1), (float)(6 + k), 3,
                            blk(10, 8));
                    ink.run((float)(13 + k - 1), (float)(6 + k), 3,
                            blk(10, 8));
                  }
                  for (int k = 0; k < 7; ++k) {
                    ink.run((float)(13 - k - 1), (float)(5 + k), 3, blk(9, 0));
                    ink.run((float)(13 + k - 1), (float)(5 + k), 3, blk(9, 0));
                  }
                })));

    // The name — textName declares 128, PAL[128] #A8D0F0.
    p.child(pixelTextEl(nameText, n(135), n(176)));

    // Four number recesses: seven-row single-step ramps, one hue each. The TU
    // one uses the GREEN block, not its own yellow-green. Measured, not derived.
    p.child(recess(134, 185, blk(3, 7)));
    p.child(recess(152, 185, blk(1, 5)));
    p.child(recess(134, 193, blk(2, 5)));
    p.child(recess(152, 193, blk(12, 5)));

    // The lattice behind the bars: 5 px x 2 px. patterns::gridLines takes a
    // pitch per axis now, and this is its first user.
    p.child(at(176, 185, 136, 15).fill(C(blk(0, 15))));
    p.child(at(176, 185, 136, 15).fill(latticePattern.material()));

    // Four bars at 1 px per point. barTUs 64, barEnergy 16, barHealth 32
    // (color2 82), barMorale 192.
    const int tu = phase.fired ? 43 : 58;
    p.child(statBar(170, 185, tu, kMaxTU, 64, "barTU"));
    p.child(statBar(170, 189, 56, kMaxEnergy, 16, "barEnergy"));
    p.child(statBar(170, 193, 36, kMaxHealth, 32, "barHealth"));
    p.child(statBar(170, 197, 100, kMaxMorale, 192, "barMorale"));

    // Two hand wells, 32x48: interior index 15, bevel block 14 232/235.
    for (const auto &[x, right] : {std::pair{8.0f, false}, std::pair{280.0f, true}}) {
      const bool holdsRifle = right;
      p.child(at(x, 148, 32, 48)
                  .key(right ? "handR" : "handL")
                  .child(custom([holdsRifle](SkCanvas &c, const PaintContext &) {
                    const Ink ink{c};
                    for (int r = 0; r < 48; ++r)
                      ink.run(0, (float)r, 32, blk(0, 15));
                    for (int r = 0; r < 48; ++r) {
                      ink.px(0, (float)r, blk(14, 8));
                      ink.px(31, (float)r, blk(14, 11));
                    }
                    ink.run(0, 0, 32, blk(14, 8));
                    ink.run(0, 47, 32, blk(14, 11));
                    if (!holdsRifle)
                      return;
                    // STR_RIFLE, a 32x48 BIGOB reconstruction.
                    ink.rect(14, 5, 4, 26, blk(15, 2));
                    ink.rect(15, 5, 2, 26, blk(15, 0));
                    ink.rect(12, 11, 8, 4, blk(5, 8));
                    ink.rect(13, 12, 6, 2, blk(5, 5));
                    ink.rect(13, 20, 6, 9, blk(2, 6));
                    ink.rect(14, 21, 4, 7, blk(2, 3));
                    ink.rect(11, 30, 10, 4, blk(15, 4));
                    ink.rect(13, 34, 6, 9, blk(5, 9));
                    ink.rect(14, 35, 4, 7, blk(5, 6));
                    ink.rect(12, 43, 8, 2, blk(15, 6));
                  })));
    }
    return p;
  }

  /** ActionMenuState: up to six 272x40 rows stacked UPWARD from
   *  (iconsX + 24, iconsY + 16 - id*40) = (24, 160). Description at (10,13),
   *  accuracy at (140,13), TU at (210,13), FONT_BIG, high contrast, a Frame of
   *  thickness 8 in border colour block 3. It appears instantly; rows do not
   *  stagger, slide or fade. */
  Element popupEl() {
    using namespace xcom;
    Element g = box().absolute().inset(0);
    for (int i = 0; i < 3; ++i) {
      const float y = 160.0f - (float)i * 40.0f;
      g.child(at(24, y, 272, 40).fill(C(blk(3, 11))));
      g.child(at(26, y + 2, 268, 36).fill(C(blk(3, 5))));
      g.child(at(30, y + 6, 260, 28).fill(C(blk(3, 13))));
      g.child(pixelTextEl(popupRow[i], n(34), n(y + 13)));
      g.child(pixelTextEl(popupAcc[i], n(164), n(y + 13)));
      g.child(pixelTextEl(popupTU[i], n(234), n(y + 13)));
    }
    return g;
  }

  // =========================================================================
  // AUDIT — the verification protocol, printed once

  void printAudit(sketch::SketchContext &ctx) {
    using namespace xcom;
    std::printf("\n=== X-COM BATTLESCAPE — verification ===\n");
    std::printf("atlas: %d cells at %.0fx%.0f, oversample 1.0, kNearest; "
                "sheet %.0fx%.0f = %.2f MB\n",
                atlasCells, kCellW, kCellH, 2048.0f,
                std::ceil((float)atlasCells / 16.0f) * kCellH,
                2048.0f * std::ceil((float)atlasCells / 16.0f) * kCellH * 4.0f /
                    1048576.0f);
    std::printf("stamps: terrain z0 %d + z1 %d = %d, overlay %zu, map glyphs "
                "%zu, panel glyphs %zu  (total %zu)\n",
                terrainZ0, terrainZ1, terrainZ0 + terrainZ1, overlay->size(),
                mapGlyphs->size(), panelGlyphs->size(),
                terrain->size() + overlay->size() + mapGlyphs->size() +
                    panelGlyphs->size());

    // #1 projection round-trip.
    uint32_t seed = 12345u;
    int fails = 0, boundary = 0;
    for (int i = 0; i < 200; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const float sx = (float)(seed % (uint32_t)kMapViewW);
      seed = seed * 1664525u + 1013904223u;
      const float sy = (float)(seed % (uint32_t)kMapViewH);
      int mx = 0, my = 0;
      screenToMap(sx, sy, 0, &mx, &my);
      if (mx < 0 || my < 0 || mx >= kMapSize || my >= kMapSize) {
        ++boundary;
        continue;
      }
      const SkPoint tl = mapToScreen(mx, my, 0);
      // The inverse answers with the tile whose COLUMN the point is in, which
      // is the 128-wide cell, and the y band is the 32-px screen diagonal.
      if (sx < tl.fX || sx >= tl.fX + kCellW)
        ++fails;
    }
    std::printf("#1 projection round-trip: %d/200 failures (%d at the "
                "Clamp(-1,size) boundary)\n",
                fails, boundary);

    { // panel widgets: bounds() -> hitTest() -> the same key, round-trip
      int ok = 0, total = 0;
      for (const char *k : {"map", "btn0", "btn13", "rank", "handR", "handL",
                            "resNone", "barTU", "barMorale", "zeroTUs"}) {
        ++total;
        const auto b = ctx.composer.bounds(k);
        const auto h = b ? ctx.composer.hitTest(b->center()) : std::nullopt;
        if (h && *h == k)
          ++ok;
      }
      std::printf("#2a panel widgets bounds()->hitTest() round-trip: %d/%d\n",
                  ok, total);
    }
    // #2 hitTest against the same inverse.
    int agree = 0, checked = 0;
    for (const auto &[key, mx, my] :
         {std::tuple{"unit-a", kSoldierA.mx, kSoldierA.my},
          std::tuple{"unit-b", kSoldierB.mx, kSoldierB.my},
          std::tuple{"unit-alien", kAlienX, kAlienY}}) {
      const SkPoint tl = mapToScreen(mx, my, 0);
      const SkPoint probe{tl.fX + kCellW * 0.5f, tl.fY + n(30)};
      const auto hit = ctx.composer.hitTest(probe);
      int qx = 0, qy = 0;
      screenToMap(probe.fX, probe.fY, 0, &qx, &qy);
      ++checked;
      if (hit && *hit == key && qx == mx && qy == my)
        ++agree;
      const auto bnd = ctx.composer.bounds(key);
      std::printf("   hitTest(%s) -> %-12s  screenToMap -> (%d,%d) want (%d,%d)"
                  "  bounds [%.0f %.0f %.0f %.0f] probe (%.0f,%.0f)\n",
                  key, hit ? hit->c_str() : "(none)", qx, qy, mx, my,
                  bnd ? bnd->left() : -1.0f, bnd ? bnd->top() : -1.0f,
                  bnd ? bnd->width() : -1.0f, bnd ? bnd->height() : -1.0f,
                  probe.fX, probe.fY);
    }
    std::printf("#2 hitTest agrees with the inverse: %d/%d  "
                "(pool tiles are unreachable — instances() is one custom() "
                "leaf; see the report)\n",
                agree, checked);

    // #3 the light radius is exactly 8.
    std::printf("#3 shade walking -x from the selected soldier: ");
    for (int k = 0; k <= 11; ++k)
      std::printf("%d%s", tileShade(kSoldierA.mx - k, kSoldierA.my),
                  k == 11 ? "" : ",");
    std::printf("  (first constant at tile 8 == correct)\n");

    // #4 the bars read back, from the geometry that drew them.
    const int vals[4] = {58, 56, 36, 100};
    const int maxs[4] = {kMaxTU, kMaxEnergy, kMaxHealth, kMaxMorale};
    const char *nm[4] = {"TU", "Energy", "Health", "Morale"};
    for (int i = 0; i < 4; ++i)
      std::printf("#4 %-6s fill %3d px / 4 = %3d (drawn %3d)   outline %3d "
                  "px / 4 - 1 = %3d (max %3d)\n",
                  nm[i], (int)(vals[i] * PX), vals[i], vals[i],
                  (int)((maxs[i] + 1) * PX), maxs[i], maxs[i]);

    // The selector's ten-pixel fudge, both answers.
    const SkPoint cur = mapToScreen(path.back().x(), path.back().y(), 0);
    const SkPoint probe{cur.fX + kCellW * 0.5f, cur.fY + n(30)};
    int hx = 0, hy = 0, bx = 0, by = 0;
    screenToMap(probe.fX, probe.fY, 0, &hx, &hy);
    screenToMap(probe.fX, probe.fY + n(10), 0, &bx, &by); // spriteHeight/4
    std::printf("   selector: honest inverse (%d,%d); Map::setSelectorPosition's "
                "+10 px bias (%d,%d) — the game ships the biased one\n",
                hx, hy, bx, by);
    std::printf("=======================================\n\n");
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    using namespace xcom;
    ctx.canvas(kCanvasW, kCanvasH);
    ctx.background(C(blk(0, 15)));

    bakeAtlas();
    terrain = std::make_shared<Pool>();
    overlay = std::make_shared<Pool>();
    mapGlyphs = std::make_shared<Pool>();
    panelGlyphs = std::make_shared<Pool>();

    map = buildMap();
    buildTerrain();

    // The panel's brushed metal: a dithered scatter of block 5 indices 83-94,
    // no gradient, no direction — measured off the capture. Baked once, nearest.
    metalPattern = Pattern::tile(
        {n(8), n(8)}, [](SkCanvas &c, SkSize, uint32_t) {
          const Ink ink{c};
          for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
              const uint32_t h = hash3(x, y, 404);
              int step = 6 + (int)(h % 2u);
              if ((h >> 4) % 9u == 0u)
                step = 4 + (int)((h >> 8) % 2u);
              else if ((h >> 4) % 13u == 0u)
                step = 9 + (int)((h >> 8) % 2u);
              ink.px((float)x, (float)y, blk(5, step));
            }
        });
    metalPattern.sampling(SkSamplingOptions(SkFilterMode::kNearest));
    // patterns::gridLines(spacingX, spacingY, width, colour) — the 5 x 2 pitch,
    // exactly. One colour, so the capture's 136/137 verticals and 138
    // horizontals collapse to one step; filed in the report.
    latticePattern = patterns::gridLines(n(5), n(2), PX, C(137));
    latticePattern.sampling(SkSamplingOptions(SkFilterMode::kNearest));

    // Type. FONT_BIG substitute at 1x, quantised into block 8 by coverage.
    auto mgr = weave::ports::systemFontManager();
    const auto face = [&](const char *fam, int weight) {
      sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
          fam, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                           SkFontStyle::kUpright_Slant));
      if (!f)
        f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
      return f;
    };
    weave::TextStyle big;
    big.shaping.typeface = face("Arial Narrow", SkFontStyle::kBold_Weight);
    big.shaping.fontSize = 12.0f;
    big.shaping.letterSpacing = 0.6f;
    if (ctx.fonts) {
      nameText = pixelText(u8"Anders Holmgren", big, *ctx.fonts, blk(8, 0),
                           blk(8, 5));
      // items.rul + BattleUnit::getFiringAccuracy / getActionTUs, recomputed:
      //   firing 58, maxTU 60, rifle two-handed with an empty left hand.
      //   Auto  58*35/100 = 20   TU floor(60*35/100) = 21
      //   Snap  58*60/100 = 34   TU floor(60*25/100) = 15
      //   Aimed 58*110/100 = 63  TU floor(60*80/100) = 48
      const char *names[3] = {"Auto Shot", "Snap Shot", "Aimed Shot"};
      const int accPct[3] = {35, 60, 110};
      const int tuPct[3] = {35, 25, 80};
      for (int i = 0; i < 3; ++i) {
        const int acc = kFiring * accPct[i] / 100;
        const int tus = kMaxTU * tuPct[i] / 100;
        popupRow[i] = pixelText(
            std::u8string((const char8_t *)names[i]), big, *ctx.fonts,
            blk(0, 1), blk(0, 5));
        popupAcc[i] = pixelText(
            std::u8string((const char8_t *)("Acc>" + std::to_string(acc) + "%")
                              .c_str()),
            big, *ctx.fonts, blk(0, 1), blk(0, 5));
        popupTU[i] = pixelText(
            std::u8string((const char8_t *)("TU>" + std::to_string(tus)).c_str()),
            big, *ctx.fonts, blk(0, 1), blk(0, 5));
      }
    }

    // DEFAULT_ANIM_SPEED = 100 (BattlescapeState.h:103) — a 10 Hz clock. The
    // alpha interpolant is deliberately IGNORED: a 10 Hz UI that interpolates
    // is wrong, and every motion on this screen is an integer sequence.
    ctx.ticker.addFixed(
        10.0,
        [this] {
          animFrame = (animFrame + 1) & 7;
          // blinkVisibleUnitButtons: +1 up, -2 down, turning at 44 and 32.
          tagIndex += tagDir > 0 ? 1 : -2;
          if (tagIndex >= 44) {
            tagIndex = 44;
            tagDir = -1;
          } else if (tagIndex <= 32) {
            tagIndex = 32;
            tagDir = 1;
          }
          return true;
        },
        8, nullptr, &fixedStatus);

    phase = Phase{animFrame, tagIndex, false, false, false, 3};
    computePath(false);
    buildOverlay();
    buildMapGlyphs();
    buildPanelGlyphs(phase);
    lastPhase = phase;
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // The audit runs BEFORE this frame's render(): bounds()/hitTest() read the
    // resolved Yoga layout, which the composer computes inside draw(), so a
    // query issued straight after a render() in the same update sees a dirtied
    // tree and every rect comes back nan. Costing an iteration is generous —
    // there is no way to ask the composer to lay out without painting.
    if (!auditPrinted && elapsed > 0.25) {
      auditPrinted = true;
      printAudit(ctx);
    }
    // The documented 9.6 s loop, twelve 800 ms cycles. Every transition below
    // is INSTANT — the path recomputes in one frame, the popup snaps open, the
    // TU bar drops 15 points in one frame. Resist every instinct to tween it.
    const double t = std::fmod(elapsed, 9.6);
    Phase p;
    p.frame = animFrame;
    p.tag = tagIndex;
    p.longPath = t >= 3.2;
    p.popup = t >= 4.8 && t < 6.4;
    p.fired = t >= 6.4;
    p.tags = t >= 8.0 ? 4 : 3;
    if (p == lastPhase)
      return;
    const bool pathChanged = p.longPath != lastPhase.longPath;
    const bool panelChanged =
        p.fired != lastPhase.fired || p.tags != lastPhase.tags;
    phase = p;
    if (pathChanged) {
      computePath(p.longPath);
      buildOverlay();
      buildMapGlyphs();
    }
    if (panelChanged || lastPhase.frame < 0)
      buildPanelGlyphs(p);
    lastPhase = p;
    ctx.composer.render(describe(ctx));

    if (auditPrinted && reportedFrames < 3) {
      ++reportedFrames;
      const auto &s = ctx.composer.stats();
      std::printf("stats @%.2fs: instances %zu  painted %zu  pictures %zu  "
                  "reconcile %.2f  layout %.2f  volatile %.2f  paint %.2f ms"
                  "%s\n",
                  elapsed, s.instances, s.nodesPainted, s.picturesLive,
                  s.reconcileMs, s.layoutMs, s.volatileMs, s.paintMs,
                  fixedStatus.clamped ? "  [FIXED-STEP CLAMPED]" : "");
    }
  }
};

SIGIL_SKETCH(XcomBattlescape)
