// The IfritCompose perf gate (run in Release): what describe, reconcile,
// layout, and draw cost — and what memo and automatic picture caching
// actually save. Reference numbers live in STRESS_TESTS.md.

#include <ifritcompose/Compose.h>

#include <include/effects/SkImageFilters.h>

#include <textflow/FontContext.h>
#include <textflow/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <benchmark/benchmark.h>

#include <string>
#include <vector>

using namespace ifrit::compose;

namespace {
std::u8string toU8(const std::string &s) {
  return std::u8string(s.begin(), s.end());
}
} // namespace
using namespace std::chrono_literals;

namespace {

textflow::FontContext &fonts() {
  static auto *context =
      new textflow::FontContext(textflow::ports::systemFontManager());
  return *context;
}

struct Row {
  std::string name;
  int score = 0;
  bool operator==(const Row &) const = default;
};

Element scoreRow(const Row &row) {
  textflow::TextStyle style;
  style.shaping.fontSize = 14.0f;
  return box().row().gap(12).padding(8).corners({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .child(text(toU8(row.name), style).grow(1))
      .child(text(toU8(std::to_string(row.score)), style));
}

Element scoreboard(const std::vector<Row> &rows) {
  auto list = box().column().gap(4).padding(16);
  for (const Row &row : rows)
    list.child(memo(row, scoreRow).key(row.name));
  return list;
}

std::vector<Row> makeRows(int count) {
  std::vector<Row> rows;
  for (int i = 0; i < count; ++i)
    rows.push_back({"player_" + std::to_string(i), i * 7});
  return rows;
}

struct Host {
  ifrit::tick::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit Host(int w = 800, int h = 2400) {
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }
};

} // namespace

/** Full describe + reconcile of 100 memo'd rows, nothing changed —
 *  the steady-state data-refresh cost (all memo hits). */
static void BM_Render_100Rows_Unchanged(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  host.composer.render(scoreboard(rows));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.render(scoreboard(rows));
  state.counters["memoHits"] = (double)host.composer.stats().memoHits;
}
BENCHMARK(BM_Render_100Rows_Unchanged);

/** One row's data changed: one memo miss re-describes + patches. */
static void BM_Render_100Rows_OneChanged(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  host.composer.render(scoreboard(rows));
  host.composer.draw(*host.surface->getCanvas());
  int tick = 0;
  for (auto _ : state) {
    rows[50].score = ++tick;
    host.composer.render(scoreboard(rows));
  }
}
BENCHMARK(BM_Render_100Rows_OneChanged);

/** Cold describe + mount of the full 100-row tree (worst case). */
static void BM_Render_100Rows_Cold(benchmark::State &state) {
  auto rows = makeRows(100);
  for (auto _ : state) {
    Host host;
    host.composer.render(scoreboard(rows));
    benchmark::DoNotOptimize(host.composer.stats().instances);
  }
}
BENCHMARK(BM_Render_100Rows_Cold);

/** Drawing the fully static scoreboard: automatic picture replay. */
static void BM_Draw_100Rows_Cached(benchmark::State &state) {
  Host host;
  host.composer.render(scoreboard(makeRows(100)));
  host.composer.draw(*host.surface->getCanvas()); // record
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
  state.counters["picturesLive"] =
      (double)host.composer.stats().picturesLive;
  state.counters["nodesPainted"] =
      (double)host.composer.stats().nodesPainted;
}
BENCHMARK(BM_Draw_100Rows_Cached);

/** The same tree forced volatile by one bound root transform — live
 *  stacking paint of every node, the no-cache ceiling. */
static void BM_Draw_100Rows_Volatile(benchmark::State &state) {
  Host host;
  choreograph::Output<float> x = 0.0f;
  auto list = box().translateX(&x).column().gap(4).padding(16);
  for (const Row &row : makeRows(100))
    list.child(memo(row, scoreRow).key(row.name));
  host.composer.render(std::move(list));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state) {
    x = x.value() + 0.25f;
    host.composer.draw(*host.surface->getCanvas());
  }
  state.counters["nodesPainted"] =
      (double)host.composer.stats().nodesPainted;
}
BENCHMARK(BM_Draw_100Rows_Volatile);

/** Text-heavy relayout: width change re-measures every paragraph. */
static void BM_Layout_100Rows_WidthChange(benchmark::State &state) {
  Host host;
  host.composer.render(scoreboard(makeRows(100)));
  host.composer.draw(*host.surface->getCanvas());
  float width = 800;
  for (auto _ : state) {
    width = width == 800 ? 640 : 800;
    host.composer.setSize({width, 2400});
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Layout_100Rows_WidthChange);

/** A transition step: ticker + one animating node repainting over a
 *  static cached background of 99 rows. */
static void BM_Frame_OneTransitionActive(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  host.composer.render(scoreboard(rows));
  host.composer.draw(*host.surface->getCanvas());
  int flip = 0;
  for (auto _ : state) {
    state.PauseTiming();
    rows[10].score = ++flip; // re-describe row 10 with a transition
    auto list = box().column().gap(4).padding(16).transition({16000ms});
    for (const Row &row : rows)
      list.child(memo(row, scoreRow).key(row.name));
    host.composer.render(std::move(list));
    state.ResumeTiming();
    host.ticker.tick(1.0 / 120.0);
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Frame_OneTransitionActive);


/** Dense text block, picture replay per draw: the raster re-raster
 *  cost that Cache::Texture exists to eliminate. */
static Element denseBlock(Cache mode) {
  textflow::TextStyle style;
  style.shaping.fontSize = 15.0f;
  std::u8string para;
  for (int i = 0; i < 60; ++i)
    para += u8"the quick brown fox jumps over the lazy dog ";
  auto block = box().padding(12).width(780).cache(mode)
                   .fill(Fill::color({0.1f, 0.1f, 0.12f, 1}));
  for (int i = 0; i < 6; ++i)
    block.child(text(para, style));
  return block;
}

static void BM_Draw_DenseText_PictureReplay(benchmark::State &state) {
  Host host(800, 2400);
  host.composer.render(denseBlock(Cache::Picture));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_DenseText_PictureReplay);

static void BM_Draw_DenseText_TextureBlit(benchmark::State &state) {
  Host host(800, 2400);
  host.composer.render(denseBlock(Cache::Texture));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
  state.counters["texturesLive"] =
      (double)host.composer.stats().texturesLive;
}
BENCHMARK(BM_Draw_DenseText_TextureBlit);

/** The sparse case: the 100-row list texture-cached whole — blitting
 *  its mostly-empty full area vs replaying only the rows. */
static void BM_Draw_100Rows_TextureBlit(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  auto list = box().column().gap(4).padding(16).cache(Cache::Texture);
  for (const Row &row : rows)
    list.child(memo(row, scoreRow).key(row.name));
  host.composer.render(std::move(list));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_100Rows_TextureBlit);


/** A blur-effected headline: picture replay re-runs the filter every
 *  draw; Cache::Texture bakes it — the effects payoff. */
static Element bloomBlock(Cache mode) {
  textflow::TextStyle style;
  style.shaping.fontSize = 64.0f;
  style.paint.foreground.setColor(0xff7ee8ff);
  return box().padding(24).cache(mode)
      .effect(Effect::filter(SkImageFilters::Blur(12, 12, nullptr)))
      .child(text(u8"BLOOM PIPELINE", style));
}

static void BM_Draw_Bloom_PictureReplay(benchmark::State &state) {
  Host host(900, 300);
  host.composer.render(bloomBlock(Cache::Picture));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_Bloom_PictureReplay);

static void BM_Draw_Bloom_TextureBaked(benchmark::State &state) {
  Host host(900, 300);
  host.composer.render(bloomBlock(Cache::Texture));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_Bloom_TextureBaked);


#ifdef COMPOSE_BENCH_GRAPHITE
// ---- Item 21: the Graphite re-measure — does picture replay finally
// beat rasterization when the target is a GPU surface? ----

#include "ComposeBenchGpu.h"
#include "SkiaGraphiteContext.h"

#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>

namespace {

SkiaGraphiteContext &graphite() {
  static std::unique_ptr<SkiaGraphiteContext> ctx =
      SkiaGraphiteContext::createMetal(ifrit::compose::bench::gpuDevice(),
                                       ifrit::compose::bench::gpuQueue());
  return *ctx;
}

void submitGraphite() {
  auto recording = graphite().recorder()->snap();
  if (!recording)
    return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphite().context()->insertRecording(info);
  graphite().context()->submit();
}

} // namespace

static void BM_Draw_100Rows_Cached_Graphite(benchmark::State &state) {
  Host host;
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(), SkImageInfo::MakeN32Premul(800, 2400));
  host.composer.render(scoreboard(makeRows(100)));
  host.composer.draw(*surface->getCanvas());
  submitGraphite();
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite();
  }
}
BENCHMARK(BM_Draw_100Rows_Cached_Graphite);

static void BM_Draw_Bloom_PictureReplay_Graphite(benchmark::State &state) {
  Host host(900, 300);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(), SkImageInfo::MakeN32Premul(900, 300));
  host.composer.render(bloomBlock(Cache::Picture));
  host.composer.draw(*surface->getCanvas());
  submitGraphite();
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite();
  }
}
BENCHMARK(BM_Draw_Bloom_PictureReplay_Graphite);

static void BM_Draw_Bloom_TextureBaked_Graphite(benchmark::State &state) {
  Host host(900, 300);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphite().recorder(), SkImageInfo::MakeN32Premul(900, 300));
  host.composer.render(bloomBlock(Cache::Texture));
  host.composer.draw(*surface->getCanvas());
  submitGraphite();
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite();
  }
}
BENCHMARK(BM_Draw_Bloom_TextureBaked_Graphite);

#endif // COMPOSE_BENCH_GRAPHITE

BENCHMARK_MAIN();
