// Pixel art: what a sheet of sprites costs to pack and rasterise, and how
// the three ways of presenting one drawing compare — the marks as nodes,
// the marks as one baked image, and the indices a shader recolours.

#include <sigilcompose/kit/Sprites.h>

#include <string>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;

namespace {

/** A 16 x 16 icon whose characters are scattered enough that the merge has
 *  real work to do: a grid of one entry would collapse to a single mark
 *  and measure the merge rather than the bake. */
std::vector<std::string> icon(int seed) {
  static constexpr char kChars[] = " abcd";
  std::vector<std::string> rows;
  for (int y = 0; y < 16; ++y) {
    std::string row(16, ' ');
    for (int x = 0; x < 16; ++x) {
      const int cell = (x / 2 + y / 3 + seed) % 7;
      row[(size_t)x] = kChars[cell < 5 ? cell : 0];
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

const std::vector<SkColor4f> kPalette{{0, 0, 0, 0},
                                      {0.9f, 0.2f, 0.2f, 1},
                                      {0.2f, 0.7f, 0.3f, 1},
                                      {0.2f, 0.3f, 0.9f, 1},
                                      {0.9f, 0.9f, 0.2f, 1}};

kit::Sprite spriteOf(int seed) {
  return kit::pixelMap(icon(seed), {" abcd", kPalette}).value_or(kit::Sprite{});
}

}  // namespace

/** THE SHEET: sixty-four icons packed and rasterised onto one image. This
 *  is the whole cost a drawing pays once at setup for a texture it then
 *  binds once per frame. */
static void BM_Sprites_SheetBake(benchmark::State& state) {
  const int count = (int)state.range(0);
  std::vector<kit::Sprite> sprites;
  for (int i = 0; i < count; ++i) sprites.push_back(spriteOf(i));
  for ([[maybe_unused]] auto iteration : state) {
    kit::SpriteSheet sheet;
    for (int i = 0; i < count; ++i)
      sheet.add("icon" + std::to_string(i), sprites[(size_t)i]);
    benchmark::DoNotOptimize(sheet.bake({.cell = 4.0f}));
  }
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_Sprites_SheetBake)->Arg(64)->Unit(benchmark::kMicrosecond);

/** Reading a character grid into merged marks — the part of the bake that
 *  is neither packing nor rasterising. */
static void BM_Sprites_ReadGrid(benchmark::State& state) {
  const std::vector<std::string> rows = icon(3);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(kit::pixelMap(rows, {" abcd", kPalette}));
}
BENCHMARK(BM_Sprites_ReadGrid)->Unit(benchmark::kMicrosecond);

/** One sprite, three presentations. The node form is what a drawing pays
 *  per describe; the two bakes are what it pays once. */
static void BM_Sprites_Present(benchmark::State& state) {
  const kit::Sprite sprite = spriteOf(1);
  const int form = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    if (form == 0)
      benchmark::DoNotOptimize(kit::pixelSprite(sprite, {.cell = 4.0f}));
    else if (form == 1)
      benchmark::DoNotOptimize(kit::spriteImage(sprite, {.cell = 4.0f}));
    else
      benchmark::DoNotOptimize(kit::indexImage(sprite, 4.0f));
  }
}
BENCHMARK(BM_Sprites_Present)
    ->Arg(0)
    ->Arg(1)
    ->Arg(2)
    ->Unit(benchmark::kMicrosecond);
