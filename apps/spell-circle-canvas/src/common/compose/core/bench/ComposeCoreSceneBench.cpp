// WHAT A BAKE SAVES AGAINST DRAWING THE SAME PICTURE AGAIN, on two
// scenes made of many small draws: a tile map of image regions, memo'd
// by chunk, replayed whole and with one chunk changed and against the
// one SkSL fill that draws the same field with no tile anybody can key;
// and a charged disc, where every lit band is a stack of additive
// grades, held as its own bake or rasterized again every frame.
//
// The scaling matrix is in ComposeCoreBench.cpp, and the scoreboard, the
// effects and the batches are in the three files beside this one.

#include <include/core/SkBitmap.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Compose.h>
#include <sigilimage/asset/ImageAsset.h>

#include <memory>
#include <string>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

// ---- Image regions: a tile map three ways ---------------------------------

namespace {

std::shared_ptr<sigil::image::ImageAsset> benchAtlas() {
  static std::shared_ptr<sigil::image::ImageAsset> asset = [] {
    SkBitmap src;
    src.allocN32Pixels(64, 16);
    for (int i = 0; i < 4; ++i)
      src.erase(SkColorSetRGB((U8CPU)(60 + i * 40), 40, 90),
                SkIRect::MakeXYWH(i * 16, 0, 16, 16));
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(src.asImage()));
  }();
  return asset;
}

struct ChunkProps {
  std::vector<int> ids;
  bool operator==(const ChunkProps&) const = default;
};

Element benchChunk(const ChunkProps& p) {
  constexpr float kTile = 16.0f;
  auto tiles = box().width(10 * kTile).height(10 * kTile);
  for (int i = 0; i < (int)p.ids.size(); ++i) {
    const int row = i / 10;
    tiles.children(
        {image(benchAtlas())
             .imageRegion(SkRect::MakeXYWH((float)(p.ids[(size_t)i] % 4) * 16,
                                           0, 16, 16))
             .absolute()
             .inset((float)(i % 10) * kTile, (float)row * kTile, 0, 0)
             .width(kTile)
             .height(kTile)});
  }
  return tiles;
}

/** 6x4 chunks of 10x10 region tiles (2400 tiles), each chunk memo'd. */
struct TileGrid {
  std::vector<ChunkProps> chunks = std::vector<ChunkProps>(24);

  TileGrid() {
    for (int c = 0; c < 24; ++c)
      for (int i = 0; i < 100; ++i)
        chunks[(size_t)c].ids.push_back((i * 31 + c) % 4);
  }

  Element describe() const {
    auto grid = box().row().flexWrap().width(6 * 160.0f);
    for (int c = 0; c < 24; ++c)
      grid.children(
          {memo(chunks[(size_t)c], benchChunk).key("c" + std::to_string(c))});
    return box().children({grid});
  }
};

}  // namespace

/** Steady-state redraw, everything picture-cached (the tile-map
 *  baseline). */
static void BM_Draw_TileGrid_Region_Cached(benchmark::State& state) {
  Host host(960, 640);
  TileGrid grid;
  host.composer.render(grid.describe());
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.render(grid.describe());
    host.draw();
  }
}
BENCHMARK(BM_Draw_TileGrid_Region_Cached);

/** Same grid, one chunk's data mutated per iteration. The pair with the arm
 *  above isolates incremental cost: the changed chunk's memo misses and its
 *  recording is rebuilt, while the other 23 replay untouched. */
static void BM_Draw_TileGrid_Region_OneChunkChanged(benchmark::State& state) {
  Host host(960, 640);
  TileGrid grid;
  host.composer.render(grid.describe());
  host.draw();
  int flip = 0;
  for ([[maybe_unused]] auto iteration : state) {
    int& id = grid.chunks[7].ids[(size_t)(flip++ % 100)];
    id = (int)((unsigned)id ^ 1u);
    host.composer.render(grid.describe());
    host.draw();
  }
}
BENCHMARK(BM_Draw_TileGrid_Region_OneChunkChanged);

/** The same 2400-tile field expressed as ONE SkSL fill that samples the
 *  atlas procedurally: a single draw, but the tiles have no individual
 *  identity, so nothing can be keyed, hit-tested or animated per tile. That
 *  is the trade this arm prices against the two region-tile arms above. */
static void BM_Draw_TileGrid_SkSLFill(benchmark::State& state) {
  Host host(960, 640);
  static const char* kSkSL = R"(
    uniform shader atlas;
    half4 main(float2 xy) {
      float2 tile = floor(xy / 16.0);
      float id = mod(tile.x * 31.0 + tile.y * 7.0, 4.0);
      float2 local = xy - tile * 16.0;
      return atlas.eval(float2(id * 16.0, 0) + local);
    })";
  auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
  if (!effect) {
    state.SkipWithError(err.c_str());
    return;
  }
  const auto& frame = benchAtlas()->frames().front();
  sk_sp<SkShader> atlasShader = frame.image->makeShader(
      SkTileMode::kClamp, SkTileMode::kClamp, SkSamplingOptions());
  SkRuntimeShaderBuilder builder(effect);
  builder.child("atlas") = atlasShader;
  sk_sp<SkShader> field = builder.makeShader();

  host.composer.render(box().children({box()
                                           .width(960)
                                           .height(640)
                                           .fill(Fill::shader(field))
                                           .cache(Cache::None)}));
  for ([[maybe_unused]] auto iteration : state) host.draw();
}
BENCHMARK(BM_Draw_TileGrid_SkSLFill);

// ---- A CHARGED DISC: N emissive stacks over one shape ---------------------
//
// The shape a lit diagram takes: every lit band, seal and star is a stack of
// additive fills laid over the whole disc, each stack gated by a beat of its
// own. What the arms below separate is the cost of the LIGHT from the cost
// of the NODES — a stack that composites through a layer of its own pays a
// bounded intermediate per stack, where one that rides its blit pays a blit.

namespace {

/** One grade of a glow: a disc filled at a low alpha through kPlus, which
 *  is how a bloom is built out of nested discs. */
Element grade(float radius, float alpha) {
  return box()
      .absolute()
      .left(200 - radius)
      .top(200 - radius)
      .width(radius * 2)
      .height(radius * 2)
      .shape([](SkSize s) {
        return SkPath::Oval(SkRect::MakeWH(s.fWidth, s.fHeight));
      })
      .fill(Fill::color({1.0f, 0.72f, 0.31f, alpha}))
      .blendMode(SkBlendMode::kPlus);
}

/** One lit element: four grades over the same disc, held as one bake and
 *  composited at its own gain — the form a lit diagram repeats. */
Element emissiveStack(int index, Cache mode) {
  const float radius = 60.0f + (float)(index % 8) * 14.0f;
  return box()
      .key("lit" + std::to_string(index))
      .absolute()
      .left(0)
      .top(0)
      .width(400)
      .height(400)
      .cache(mode)
      .blendMode(SkBlendMode::kPlus)
      .opacity(0.55f + 0.04f * (float)(index % 8))
      .children({grade(radius, 0.085f), grade(radius * 0.72f, 0.16f),
                 grade(radius * 0.5f, 0.42f), grade(radius * 0.3f, 0.96f)});
}

void chargedDiscArm(benchmark::State& state, Cache mode) {
  const int count = (int)state.range(0);
  Host host(400, 400);
  Element disc =
      box().width(400).height(400).fill(Fill::color({0.05f, 0.04f, 0.06f, 1}));
  for (int i = 0; i < count; ++i) disc.children({emissiveStack(i, mode)});
  host.composer.render(disc);
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  sigil::compose::bench::reportNodes(state, count);
}

}  // namespace

/** Each stack held as its own bake: the draw is one blit per stack. */
static void BM_Draw_ChargedDisc_Baked(benchmark::State& state) {
  chargedDiscArm(state, Cache::Texture);
}
BENCHMARK(BM_Draw_ChargedDisc_Baked)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

/** The same picture with nothing held: every grade is rasterized again on
 *  every frame, which is what the cost of the light alone looks like. */
static void BM_Draw_ChargedDisc_Live(benchmark::State& state) {
  chargedDiscArm(state, Cache::None);
}
BENCHMARK(BM_Draw_ChargedDisc_Live)->Arg(1)->Arg(4)->Arg(16)->Arg(64);
