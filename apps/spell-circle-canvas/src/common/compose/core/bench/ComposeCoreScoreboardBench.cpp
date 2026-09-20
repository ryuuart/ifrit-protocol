// WHAT A REALISTIC LIST COSTS: a hundred memo'd rows described,
// reconciled, laid out and drawn. The steady state where every memo hits,
// the frame where one row's data changed, the cold mount of the whole
// tree, the same tree forced volatile by one bound transform, and the
// list held as one texture — then the cached case again on a Graphite
// Metal surface, so the two targets can be compared directly.

#include <sigilcompose/Compose.h>

#include <memory>
#include <string>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using namespace std::chrono_literals;
using sigil::compose::bench::Host;

// ---- The scoreboard: 100 memo'd rows ----------------------------------------

namespace {

struct Row {
  std::string name;
  int score = 0;
  bool operator==(const Row&) const = default;
};

Element scoreRow(const Row& row) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14.0f;
  return box()
      .row()
      .gap(12)
      .padding(8)
      .borderRadius({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .children({text(row.name, style).flexGrow(1),
                 text(std::to_string(row.score), style)});
}

Element scoreboard(const std::vector<Row>& rows) {
  auto list = box().column().gap(4).padding(16);
  for (const Row& row : rows)
    list.children({memo(row, scoreRow).key(row.name)});
  return list;
}

std::vector<Row> makeRows(int count) {
  std::vector<Row> rows;
  rows.reserve((size_t)count);
  for (int i = 0; i < count; ++i)
    rows.push_back({"player_" + std::to_string(i), i * 7});
  return rows;
}

}  // namespace

/** The scoreboard rendered and drawn once, so every arm starts from the
 *  warm state a host reaches after its first frame. */
class Scoreboard : public benchmark::Fixture {
 public:
  void SetUp(const benchmark::State&) override {
    rows = makeRows(100);
    host = std::make_unique<Host>();
    host->composer.render(scoreboard(rows));
    host->draw();
  }
  void TearDown(const benchmark::State&) override { host.reset(); }

  std::vector<Row> rows;
  std::unique_ptr<Host> host;
};

/** Full describe + reconcile, nothing changed — the steady-state
 *  data-refresh cost (all memo hits). */
BENCHMARK_F(Scoreboard, Render_Unchanged)(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state)
    host->composer.render(scoreboard(rows));
  state.counters["memoHits"] = (double)host->composer.stats().memoHits;
}

/** One row's data changed: one memo miss re-describes + patches. */
BENCHMARK_F(Scoreboard, Render_OneChanged)(benchmark::State& state) {
  int tick = 0;
  for ([[maybe_unused]] auto iteration : state) {
    rows[50].score = ++tick;
    host->composer.render(scoreboard(rows));
  }
}

/** Drawing the fully static scoreboard: automatic picture replay. */
BENCHMARK_F(Scoreboard, Draw_Cached)(benchmark::State& state) {
  for ([[maybe_unused]] auto iteration : state) host->draw();
  state.counters["picturesLive"] = (double)host->composer.stats().picturesLive;
  state.counters["nodesPainted"] = (double)host->composer.stats().nodesPainted;
}

/** Text-heavy relayout: width change re-measures every paragraph. */
BENCHMARK_F(Scoreboard, Layout_WidthChange)(benchmark::State& state) {
  float width = 800;
  for ([[maybe_unused]] auto iteration : state) {
    width = width == 800 ? 640 : 800;
    host->composer.setSize({width, 2400});
    host->draw();
  }
}

/** A transition step: ticker + one animating node repainting over a
 *  static cached background of 99 rows. */
BENCHMARK_F(Scoreboard, Frame_OneTransitionActive)(benchmark::State& state) {
  int flip = 0;
  for ([[maybe_unused]] auto iteration : state) {
    state.PauseTiming();
    rows[10].score = ++flip;  // re-describe row 10 with a transition
    auto list = box().column().gap(4).padding(16).transition({16000ms});
    for (const Row& row : rows)
      list.children({memo(row, scoreRow).key(row.name)});
    host->composer.render(list);
    state.ResumeTiming();
    host->ticker.tick(1.0 / 120.0);
    host->draw();
  }
}

/** Cold describe + mount of the full 100-row tree (worst case). */
static void BM_Render_100Rows_Cold(benchmark::State& state) {
  auto rows = makeRows(100);
  for ([[maybe_unused]] auto iteration : state) {
    Host host;
    host.composer.render(scoreboard(rows));
    size_t instances = host.composer.stats().instances;
    benchmark::DoNotOptimize(instances);
  }
}
BENCHMARK(BM_Render_100Rows_Cold);

/** The same tree forced volatile by one bound root transform — live
 *  stacking paint of every node, the no-cache ceiling. */
static void BM_Draw_100Rows_Volatile(benchmark::State& state) {
  Host host;
  choreograph::Output<float> x = 0.0f;
  auto list = box().translateX(&x).column().gap(4).padding(16);
  for (const Row& row : makeRows(100))
    list.children({memo(row, scoreRow).key(row.name)});
  host.composer.render(list);
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    x = x.value() + 0.25f;
    host.draw();
  }
  state.counters["nodesPainted"] = (double)host.composer.stats().nodesPainted;
}
BENCHMARK(BM_Draw_100Rows_Volatile);

/** The sparse case: the 100-row list texture-cached whole — blitting
 *  its mostly-empty full area vs replaying only the rows. */
static void BM_Draw_100Rows_TextureBlit(benchmark::State& state) {
  Host host;
  auto rows = makeRows(100);
  auto list = box().column().gap(4).padding(16).cache(Cache::Texture);
  for (const Row& row : rows)
    list.children({memo(row, scoreRow).key(row.name)});
  host.composer.render(list);
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
}
BENCHMARK(BM_Draw_100Rows_TextureBlit);

#ifdef SIGIL_BENCH_GPU
// ---- The same scene against a Graphite Metal surface ----
// Cache tiers trade re-recording against re-rasterizing, and which side
// wins depends on the target.

using sigil::compose::bench::GraphiteTarget;

static void BM_Draw_100Rows_Cached_Graphite(benchmark::State& state) {
  GraphiteTarget target(state, 800, 2400);
  if (!target.ok()) return;
  Host host;
  host.composer.render(scoreboard(makeRows(100)));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submit();
  }
}
BENCHMARK(BM_Draw_100Rows_Cached_Graphite);

#endif  // SIGIL_BENCH_GPU
