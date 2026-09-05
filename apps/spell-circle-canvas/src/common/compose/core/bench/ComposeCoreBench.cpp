// SigilComposeCore scaling benchmarks: the same operations at several node
// counts, so the reader gets a curve rather than a point. Arms are organized
// as a cold / warm / update / draw matrix. ComposeCoreSceneBench.cpp is the
// companion file in this binary, holding whole-scene arms at a fixed size.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Compose.h>
#include <sigilcore/reconcile/Env.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;

namespace core = sigil::core;
using sigil::compose::bench::cellFill;
using sigil::compose::bench::fonts;
using sigil::compose::bench::Host;
using sigil::compose::bench::nodeLadder;
using sigil::compose::bench::reportNodes;

namespace {

Element flexGrid(int count, int changed = -1, int phase = 0,
                 int orderShift = 0) {
  auto root = box().row().wrapLines().gap(1);
  for (int slot = 0; slot < count; ++slot) {
    const int id = (slot + orderShift) % count;
    root.child(box()
                   .key("n" + std::to_string(id))
                   .width(19)
                   .height(19)
                   .fill(cellFill(id, changed, phase)));
  }
  return root;
}

Element positionedGrid(int count) {
  auto root = positioned().inset(0, 0, 0, 0);
  constexpr int kColumns = 50;
  for (int id = 0; id < count; ++id) {
    const int row = id / kColumns;
    root.child(box()
                   .key("n" + std::to_string(id))
                   .left((float)(id % kColumns) * 20.0f)
                   .top((float)row * 20.0f)
                   .width(19)
                   .height(19)
                   .fill(cellFill(id)));
  }
  return root;
}

sk_sp<SkRuntimeEffect> groupShader() {
  static sk_sp<SkRuntimeEffect> effect = [] {
    auto [compiled, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float a = sin(p.x * 0.19) * cos(p.y * 0.23);"
                 "  float b = sin((p.x + p.y) * 0.07);"
                 "  return half4(half(0.30 + 0.25 * a), half(0.42 + 0.22 * b),"
                 "               half(0.58 + 0.20 * a), 1.0);"
                 "}"));
    if (!compiled)
      SkDebugf("compose_core_bench group shader failed: %s\n", error.c_str());
    return compiled;
  }();
  return effect;
}

Element groupScene(int count, Cache mode) {
  auto group = box().absolute().inset(12).key("group").cache(mode);
  constexpr int kColumns = 10;
  for (int id = 0; id < count; ++id) {
    const int row = id / kColumns;
    group.child(box()
                    .absolute()
                    .left(18.0f + (float)(id % kColumns) * 59.0f)
                    .top(18.0f + (float)row * 43.0f)
                    .width(64)
                    .height(13)
                    .rotate((float)(id % 7) * 6.0f - 18.0f)
                    .fill(sigil::material::skia::Paint::sksl(groupShader())));
  }
  // Keep the parent live so it calls into the group every frame. An Auto
  // parent would cache one picture containing the first-frame traversal and
  // make both arms bypass the group state machine entirely.
  return box().cache(Cache::None).absolute().inset(0).child(std::move(group));
}

}  // namespace

// ---- describe / reconcile -------------------------------------------------

static void BM_Mount_Cold(benchmark::State& state) {
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    sigil::motion::Ticker ticker;
    Composer composer(ticker, fonts());
    composer.setSize({1024, 1024});
    composer.render(flexGrid(count));
    benchmark::DoNotOptimize(composer.dirty());
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Mount_Cold)->Apply(nodeLadder);

static void BM_Reconcile_Flex_Warm(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(flexGrid(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(flexGrid(count));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Flex_Warm)->Apply(nodeLadder);

static void BM_Reconcile_Positioned_Warm(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(positionedGrid(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(positionedGrid(count));
  state.counters["yogaNodes"] = (double)host.composer.stats().yogaNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Positioned_Warm)->Apply(nodeLadder);

static void BM_Reconcile_OneChanged(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(flexGrid(count));
  host.draw();
  int phase = 0;
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(
        flexGrid(count, count / 2, ((unsigned)++phase & 1u) != 0));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_OneChanged)->Apply(nodeLadder);

static void BM_Reconcile_KeyedReorder(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(flexGrid(count));
  host.draw();
  int shift = 0;
  for ([[maybe_unused]] auto iteration : state) {
    shift = shift == 0 ? 1 : 0;
    host.composer.render(flexGrid(count, -1, 0, shift));
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_KeyedReorder)->Apply(nodeLadder);

// ---- layout ---------------------------------------------------------------

static void BM_Layout_Flex_ViewportToggle(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(flexGrid(count));
  host.draw();
  bool narrow = false;
  for ([[maybe_unused]] auto iteration : state) {
    narrow = !narrow;
    host.composer.setSize({narrow ? 768.0f : 1024.0f, 1024.0f});
    host.draw();
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Layout_Flex_ViewportToggle)->Apply(nodeLadder);

static void BM_Layout_Positioned_ViewportToggle(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(positionedGrid(count));
  host.draw();
  bool narrow = false;
  for ([[maybe_unused]] auto iteration : state) {
    narrow = !narrow;
    host.composer.setSize({narrow ? 768.0f : 1024.0f, 1024.0f});
    host.draw();
  }
  state.counters["yogaNodes"] = (double)host.composer.stats().yogaNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Layout_Positioned_ViewportToggle)->Apply(nodeLadder);

// ---- queries --------------------------------------------------------------

static void BM_Query_Bounds(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(positionedGrid(count));
  host.draw();
  int id = 0;
  for ([[maybe_unused]] auto iteration : state) {
    const std::string key = "n" + std::to_string(id++ % count);
    benchmark::DoNotOptimize(host.composer.bounds(key));
  }
  state.counters["treeNodes"] = (double)count;
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Query_Bounds)
    ->Arg(100)
    ->Arg(2000)
    ->Arg(10000)
    ->Unit(benchmark::kNanosecond);

// ---- The cost of an environment change under memos that never read it ----
//
// A memo captures the ambient environment stack when it is constructed and
// compares that stack BEFORE it compares its own props. So changing anything
// in the environment misses every memo below the provider, including memos
// that never read the environment at all.
//
// These two arms bracket that cost. `_ThemeHeld` is the all-hits steady
// state. `_ThemeChange` flips one environment value each iteration, which
// forces every memo to re-describe; the describes then all compare equal, so
// the `patched` counter stays at zero and the difference between the arms is
// pure wasted describe work. That difference is what tracking which memos
// actually read the environment would be able to recover.

namespace {

struct BenchPalette {
  SkColor4f surface{0.20f, 0.40f, 0.60f, 1.0f};
  bool operator==(const BenchPalette&) const = default;
};

struct MemoCellProps {
  int id = 0;
  bool operator==(const MemoCellProps&) const = default;
};

Element memoGridUnder(int count, const BenchPalette& palette) {
  core::env::Provide<BenchPalette> theme(palette);
  auto root = box().row().wrapLines().gap(1);
  for (int id = 0; id < count; ++id)
    root.child(memo(MemoCellProps{id}, [](const MemoCellProps& props) {
                 // Deliberately never reads core::env::inherited: this memo
                 // has no reason to miss when the theme changes.
                 return box().width(19).height(19).fill(cellFill(props.id));
               }).key("m" + std::to_string(id)));
  return root;
}

}  // namespace

static void BM_Env_ThemeChange_MemosNeverRead(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(memoGridUnder(count, BenchPalette{}));
  host.draw();
  bool dark = false;
  for ([[maybe_unused]] auto iteration : state) {
    dark = !dark;
    BenchPalette palette;
    palette.surface = dark ? SkColor4f{0.10f, 0.12f, 0.16f, 1.0f}
                           : SkColor4f{0.20f, 0.40f, 0.60f, 1.0f};
    host.composer.render(memoGridUnder(count, palette));
  }
  state.counters["memoHits"] = (double)host.composer.stats().memoHits;
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Env_ThemeChange_MemosNeverRead)->Apply(nodeLadder);

static void BM_Env_ThemeHeld_MemosNeverRead(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(memoGridUnder(count, BenchPalette{}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(memoGridUnder(count, BenchPalette{}));
  state.counters["memoHits"] = (double)host.composer.stats().memoHits;
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Env_ThemeHeld_MemosNeverRead)->Apply(nodeLadder);

// ---- Cache::Group ---------------------------------------------------------

static void BM_Draw_GroupCache_LivePictures(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(640, 640);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(groupScene(count, Cache::Auto));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_GroupCache_LivePictures)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_GroupCache_Blit(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(640, 640);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(groupScene(count, Cache::Group));
  host.draw();  // establish the subtree-value memo
  host.draw();  // settle and take the one group bake
  if (host.composer.stats().texturesLive == 0) {
    state.SkipWithError("Cache::Group did not reach the blit state");
    return;
  }
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["textures"] = (double)host.composer.stats().texturesLive;
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_GroupCache_Blit)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

// ---- Slicing one very long picture into tile rasters ---------------------
// A strip far longer than any single texture is baked ONCE as a vector
// picture and then cut into tile-sized rasters. How much does the cutting
// cost, and how much of it is avoidable?
//
// The arms bracket the answer. `FullReplay` is the straightforward way:
// every tile replays the WHOLE picture behind a clip. `PerTilePicture`
// extracts one picture per tile ahead of the timed loop and replays only
// that, so it has already paid all op-selection cost and is the floor no
// windowing mechanism can beat. `RTreeReplay` is the cheapest real thing in
// between — one extra argument to beginRecording — with `RTreeBuild` pricing
// what that costs to set up, and `SurfacesOnly` giving the raster floor
// underneath all of them. Any proposal to add windowed baking has to fit in
// the gap between FullReplay and PerTilePicture.

namespace {

Element marqueeStrip(float acrossPx, float alongPx) {
  auto label = [&](const std::string& text, float size, SkColor4f color) {
    sigil::weave::TextStyle style;
    style.shaping.fontSize = size;
    style.paint.foreground.setColor(color.toSkColor());
    auto paragraph = std::make_shared<sigil::weave::Paragraph>();
    paragraph->appendText(
        std::u8string_view((const char8_t*)text.c_str(), text.size()), style);
    sigil::weave::ParagraphLayoutOptions centered;
    centered.alignment = sigil::weave::TextAlignment::kCenter;
    return sigil::compose::text(paragraph, centered);
  };
  auto root = box()
                  .column()
                  .width(acrossPx)
                  .height(alongPx)
                  .padding(52, 110)
                  .child(box().left(10).top(0).bottom(0).width(6).fill(
                      Fill::color({0.455f, 0.878f, 0.745f, 0.95f})))
                  .child(box().right(10).top(0).bottom(0).width(4).fill(
                      Fill::color({0.455f, 0.878f, 0.745f, 0.5f})));
  const int sectors = (int)(alongPx / 930.0f);  // the marquee's own density
  for (int s = 0; s < sectors; ++s) {
    root.child(box().grow());
    root.child(label("— " + std::to_string(s + 1) + " —", 64.0f,
                     {0.62f, 0.69f, 0.79f, 1.0f}));
    root.child(
        label("nothing here tiles and nothing repeats: each sector "
              "is a different neighborhood of the same element tree, "
              "numbered as it passes.",
              44.0f, {0.93f, 0.96f, 1.0f, 1.0f}));
    auto row = box().row().gap(6).alignItems(Align::End).height(170);
    for (int i = 0; i < 44; ++i) {
      const float beat =
          0.5f + 0.35f * std::sin((float)i * 0.29f + (float)s * 1.7f);
      row.child(box()
                    .width(6)
                    .height(28.0f + 134.0f * beat)
                    .corners({3})
                    .fill(Fill::color(
                        {0.455f, 0.878f, 0.745f, 0.45f + 0.5f * beat})));
    }
    root.child(std::move(row));
  }
  return root;
}

/** The strip's picture and the tile geometry it is cut into. */
struct StripBake {
  sk_sp<SkPicture> art;
  int tiles = 0;
  int acrossPx = 0;
  int alongPx = 0;
};

StripBake bakeStrip(int tiles, int acrossPx, int tileAlongPx) {
  StripBake out;
  out.tiles = tiles;
  out.acrossPx = acrossPx;
  out.alongPx = tileAlongPx;
  out.art = snapshot(
      marqueeStrip((float)acrossPx, (float)(tiles * tileAlongPx)), fonts());
  return out;
}

// One tile of the strip, drawn the way the marquee draws it: mirrored
// across the band so the wall's u-mapping restores unmirrored glyphs, then
// stepped to this tile's window.
void drawTileWindow(SkCanvas* canvas, const StripBake& bake, int k) {
  canvas->translate((float)bake.acrossPx, 0);
  canvas->scale(-1, 1);
  canvas->translate(0, -(float)(k * bake.alongPx));
}

sk_sp<SkSurface> tileSurface(const StripBake& bake) {
  return SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(bake.acrossPx, bake.alongPx));
}

/** The same picture re-recorded behind an R-tree, so playback into a tile
 *  visits only the ops whose bounds meet that tile. This is the cheapest
 *  mechanism a "windowed bake" could actually be: one extra argument to
 *  `beginRecording`. */
sk_sp<SkPicture> withRTree(const sk_sp<SkPicture>& art) {
  SkRTreeFactory rtree;
  SkPictureRecorder recorder;
  // playback(), not drawPicture(): drawPicture on a recording canvas stores
  // a nested reference, which the R-tree cannot see into.
  art->playback(recorder.beginRecording(art->cullRect(), &rtree));
  return recorder.finishRecordingAsPicture();
}

/** The tile ladder: a strip of two, ten and forty 4096 px tiles. */
void tileLadder(::benchmark::Benchmark* b) {
  b->Arg(2)->Arg(10)->Arg(40)->Unit(benchmark::kMillisecond);
}

}  // namespace

/** What the bake itself costs — the number the tiling saving has to be
 *  read against, since both happen once. */
static void BM_Bake_TiledStrip_Snapshot(benchmark::State& state) {
  const int tiles = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(
        snapshot(marqueeStrip(324.0f, (float)(tiles * 4096)), fonts()));
  state.counters["tiles"] = (double)tiles;
}
BENCHMARK(BM_Bake_TiledStrip_Snapshot)->Apply(tileLadder);

static void BM_Bake_TiledStrip_FullReplay(benchmark::State& state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas* canvas = surface->getCanvas();
      SkAutoCanvasRestore restore(canvas, true);
      canvas->clear(SK_ColorTRANSPARENT);
      drawTileWindow(canvas, bake, k);
      canvas->drawPicture(bake.art);
    }
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["ops"] = (double)bake.art->approximateOpCount(true);
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_FullReplay)->Apply(tileLadder);

static void BM_Bake_TiledStrip_RTreeReplay(benchmark::State& state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  const sk_sp<SkPicture> art = withRTree(bake.art);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas* canvas = surface->getCanvas();
      SkAutoCanvasRestore restore(canvas, true);
      canvas->clear(SK_ColorTRANSPARENT);
      drawTileWindow(canvas, bake, k);
      canvas->drawPicture(art);
    }
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["ops"] = (double)art->approximateOpCount(true);
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_RTreeReplay)->Apply(tileLadder);

/** What the R-tree recipe COSTS, so it can be told whether it pays: the
 *  re-record plus the tree build, once. */
static void BM_Bake_TiledStrip_RTreeBuild(benchmark::State& state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(withRTree(bake.art));
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_RTreeBuild)->Apply(tileLadder);

/** The FLOOR: each tile's ops extracted into their own picture ONCE,
 *  outside the timed loop, so the timed work is only what that tile
 *  actually draws. No region bake can beat this. */
static void BM_Bake_TiledStrip_PerTilePicture(benchmark::State& state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  const sk_sp<SkPicture> art = withRTree(bake.art);
  std::vector<sk_sp<SkPicture>> windows;
  double ops = 0;
  for (int k = 0; k < bake.tiles; ++k) {
    SkPictureRecorder recorder;
    SkCanvas* rec =
        recorder.beginRecording(SkRect::MakeIWH(bake.acrossPx, bake.alongPx));
    drawTileWindow(rec, bake, k);
    art->playback(rec);  // R-tree culls against the recorder's cull rect
    windows.push_back(recorder.finishRecordingAsPicture());
    ops += (double)windows.back()->approximateOpCount(true);
  }
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas* canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->drawPicture(windows[(size_t)k]);
    }
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["ops"] = ops / (double)bake.tiles;
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_PerTilePicture)->Apply(tileLadder);

/** How much of a tile's cost is the raster itself: same tile count, same
 *  surfaces, but nothing replayed. The floor under BOTH arms above. */
static void BM_Bake_TiledStrip_SurfacesOnly(benchmark::State& state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k)
      surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_SurfacesOnly)->Apply(tileLadder);
