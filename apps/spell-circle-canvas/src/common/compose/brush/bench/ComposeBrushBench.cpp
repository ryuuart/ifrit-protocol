// SigilComposeBrush benchmarks: the per-frame price of everything that
// resolves geometry at paint time — masks, brushes, profiled strokes, woven
// strands, span strokes, stamped borders, art warps and hatches — and what a
// value decoration costs the structural prune.
//
// The live arms run with Cache::None on purpose: they price the per-frame
// resolve, and a recording would hide exactly that.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/shape/Shapes.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::cellFill;
using sigil::compose::bench::Host;
using sigil::compose::bench::reportNodes;

// ---- masks, brushes, profiled strokes, spans -------------------------------

namespace {

enum class MaskKind { Spans, Edge };

Element maskedGrid(int count, MaskKind kind,
                   choreograph::Output<float>* reveal) {
  auto root = positioned().inset(0, 0, 0, 0);
  constexpr int kColumns = 32;
  for (int id = 0; id < count; ++id) {
    Element leaf =
        box()
            .left((float)(id % kColumns) * 25.0f)
            .top((float)(id / kColumns) * 25.0f)
            .width(22)
            .height(22)
            .shape(shapes::circle())
            .fill(Fill::color({0.22f, 0.72f, 0.92f, 0.9f}))
            .stroke(brush::solid(2.0f, Fill::color({1.0f, 0.9f, 0.4f, 1.0f})))
            .cache(Cache::None);
    if (kind == MaskKind::Spans)
      leaf.mask(by::spans(spans::upTo(reveal)));
    else
      leaf.mask(by::edge(15.0f, reveal));
    root.child(std::move(leaf));
  }
  return root;
}

void maskArm(benchmark::State& state, MaskKind kind) {
  const int count = (int)state.range(0);
  choreograph::Output<float> reveal{0.0f};
  Host host(800, 800);
  host.composer.render(maskedGrid(count, kind, &reveal));
  host.draw();
  int tick = 0;
  for (auto _ : state) {
    reveal = (float)(++tick % 100) / 100.0f;
    host.draw();
  }
  reportNodes(state, count);
}

Element weaveScene(int strandCount) {
  std::vector<brush::Strand> strands;
  strands.reserve((size_t)strandCount);
  constexpr float kCenter = 320.0f;
  constexpr float kRadius = 285.0f;
  for (int i = 0; i < strandCount; ++i) {
    const float angle = SK_FloatPI * (float)i / (float)strandCount;
    const float dx = std::cos(angle) * kRadius;
    const float dy = std::sin(angle) * kRadius;
    SkPathBuilder path;
    path.moveTo(kCenter - dx, kCenter - dy);
    path.lineTo(kCenter + dx, kCenter + dy);
    const SkColor4f color = i % 2 == 0 ? SkColor4f{0.95f, 0.35f, 0.25f, 1.0f}
                                       : SkColor4f{0.25f, 0.75f, 0.95f, 1.0f};
    strands.push_back(
        {strand::path(path.detach()), brush::solid(5.0f, Fill::color(color))});
  }
  return stack().child(
      box()
          .inset(0)
          .stroke(brush::weave(std::move(strands), crossing::alternate()))
          .cache(Cache::None));
}

struct WaveWidth {
  float amplitude = 9.0f;
  float across(float along) const {
    return amplitude * (0.65f + 0.35f * std::sin(along * 8.0f * SK_FloatPI));
  }
  float max() const { return amplitude; }
  bool operator==(const WaveWidth&) const = default;
};

Element profiledRibbonGrid(int count) {
  auto root = positioned().inset(0, 0, 0, 0);
  constexpr int kColumns = 16;
  for (int id = 0; id < count; ++id) {
    root.child(
        box()
            .left((float)(id % kColumns) * 48.0f)
            .top((float)(id / kColumns) * 48.0f)
            .width(40)
            .height(40)
            .shape(shapes::circle())
            .fill(Fill::none())
            .stroke(brush::ribbon(Profile(WaveWidth{5.0f + (float)(id % 3)}),
                                  Fill::color({0.92f, 0.45f, 0.22f, 1.0f})))
            .cache(Cache::None));
  }
  return root;
}

/** Span strokes hold nothing between frames: the contour walk that turns a
 *  span's endpoints into a sub-path is redone on every paint, several times
 *  per painted node, and the count scales with the number of span passes on
 *  that node.
 *
 *  The fixture makes that the only variable: a 16-node grid where each node
 *  carries `passes` marching `spans::wrap` passes — disjoint windows driven
 *  off one shared Output, the marching-ants idiom — so every endpoint
 *  resolves and every boundary re-walks each frame. The low pass counts are
 *  what real scenes use; the high ones exist so the growth is visible and so
 *  a per-Instance span cache, if one is ever added, has something to be
 *  measured against. */
Element spanStrokeGrid(int passCount, choreograph::Output<float>& phase) {
  auto root = positioned().inset(0, 0, 0, 0);
  constexpr int kColumns = 4;
  constexpr int kNodes = 16;
  const float slot = 1.0f / (float)passCount;
  for (int id = 0; id < kNodes; ++id) {
    Element leaf = box()
                       .left((float)(id % kColumns) * 156.0f + 4.0f)
                       .top((float)(id / kColumns) * 156.0f + 4.0f)
                       .width(140)
                       .height(140)
                       .fill(Fill::none())
                       .cache(Cache::None);
    for (int p = 0; p < passCount; ++p) {
      const float base = (float)p * slot;
      const SkColor4f color = p % 2 == 0 ? SkColor4f{0.95f, 0.55f, 0.25f, 1.0f}
                                         : SkColor4f{0.25f, 0.65f, 0.95f, 1.0f};
      leaf.stroke(spans::wrap(bind(&phase).offset(base),
                              bind(&phase).offset(base + 0.6f * slot)),
                  brush::solid(3.0f, Fill::color(color)));
    }
    root.child(std::move(leaf));
  }
  return root;
}

}  // namespace

static void BM_Draw_Mask_Spans_Live(benchmark::State& state) {
  maskArm(state, MaskKind::Spans);
}
BENCHMARK(BM_Draw_Mask_Spans_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_Mask_Edge_Live(benchmark::State& state) {
  maskArm(state, MaskKind::Edge);
}
BENCHMARK(BM_Draw_Mask_Edge_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_ProfiledRibbon_Live(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(800, 800);
  host.composer.render(profiledRibbonGrid(count));
  host.draw();
  for (auto _ : state) host.draw();
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_ProfiledRibbon_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

/** Woven strokes: crossing discovery re-flattens every strand and compares
 *  every segment pair on every live paint, so cost grows with the square of
 *  the strand count. The `pairs` counter reports that count directly, which
 *  is what the arm's timings should be divided by. */
static void BM_Draw_BrushWeave_Live(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(640, 640);
  host.composer.render(weaveScene(count));
  host.draw();
  for (auto _ : state) host.draw();
  state.counters["pairs"] = (double)(count * (count - 1) / 2);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_BrushWeave_Live)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_StrokeSpans_Live(benchmark::State& state) {
  const int passes = (int)state.range(0);
  choreograph::Output<float> phase{0.0f};
  Host host(640, 640);
  host.composer.render(spanStrokeGrid(passes, phase));
  host.draw();
  int tick = 0;
  for (auto _ : state) {
    phase = (float)(++tick % 100) / 100.0f;
    host.draw();
  }
  state.counters["passes"] = (double)passes;
  reportNodes(state, 16);
}
BENCHMARK(BM_Draw_StrokeSpans_Live)
    ->RangeMultiplier(2)
    ->Range(1, 16)
    ->Unit(benchmark::kMicrosecond);

// ---- decorated rows: the structural prune through value decorations -------

namespace {

struct Row {
  std::string name;
  int score = 0;
  bool operator==(const Row&) const = default;
};

std::vector<Row> makeRows(int count) {
  std::vector<Row> rows;
  for (int i = 0; i < count; ++i)
    rows.push_back({"player_" + std::to_string(i), i * 7});
  return rows;
}

/** A static decorated row: fill + drop shadow + stroked border. Deliberately
 *  built without memo, so what is being exercised is the structural prune's
 *  ability to see through value decorations (Shadow, PathFormat) and declare
 *  two describes equal. */
Element decoratedRow(const Row& row) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14.0f;
  return box()
      .row()
      .gap(12)
      .padding(8)
      .corners({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .background(shadow({0, 0, 0, 0.5f}, {0, 2}, 6))
      .foreground(stroke(1.5f, Fill::color({0.5f, 0.5f, 0.6f, 1})))
      .child(text(toU8(row.name), style).grow(1))
      .child(text(toU8(std::to_string(row.score)), style));
}

Element decoratedBoard(const std::vector<Row>& rows) {
  auto list = box().column().gap(4).padding(16);
  for (const Row& row : rows)
    list.child(
        decoratedRow(row).key(row.name));  // no memo — prune must cover it
  return list;
}

}  // namespace

/** 100 decorated rows rendered and drawn once — the warm state every
 *  decorated-board arm starts from. */
class DecoratedBoard : public benchmark::Fixture {
 public:
  void SetUp(const benchmark::State&) override {
    rows = makeRows(100);
    host = std::make_unique<Host>();
    host->composer.render(decoratedBoard(rows));
    host->draw();
  }
  void TearDown(const benchmark::State&) override { host.reset(); }

  std::vector<Row> rows;
  std::unique_ptr<Host> host;
};

/** Re-render, nothing changed. The reported patchedNodes counter is the
 *  point: if the prune sees the decorations as equal, no row is patched and
 *  no recording is dropped. Render-only cost here is dominated by describing
 *  the tree (there is no memo, so the rows are rebuilt either way), which is
 *  why the saving shows up on the draw side rather than in this arm's wall
 *  time. */
BENCHMARK_F(DecoratedBoard, Render_Unchanged)(benchmark::State& state) {
  for (auto _ : state) host->composer.render(decoratedBoard(rows));
  state.counters["patchedNodes"] = (double)host->composer.stats().patchedNodes;
}

/** The realistic frame loop for static decorated chrome: re-render every
 *  frame without a memo, then draw only when dirty() says something moved.
 *  This is the contract a host is expected to follow, and it is what turns
 *  the prune into a saving: an unchanged decorated render leaves dirty()
 *  false, the draw is skipped entirely, and the blurred shadow is never
 *  rasterized again. The draws% counter reports how often the guard let a
 *  draw through; the remaining wall time is describe cost alone. */
BENCHMARK_F(DecoratedBoard, Frame_Static)(benchmark::State& state) {
  double draws = 0, frames = 0;
  for (auto _ : state) {
    host->composer.render(decoratedBoard(rows));
    if (host->composer.dirty()) {  // host skips clean frames
      host->draw();
      ++draws;
    }
    ++frames;
  }
  state.counters["draws%"] = 100.0 * draws / frames;
}

// ---- stamps, art warps and hatches -----------------------------------------

namespace {

ContourWalk starVine() {
  ContourWalk vine;
  vine.spacing = 24.0f;
  vine.stamp = box()
                   .width(14)
                   .height(14)
                   .shape(shapes::star(4, 0.45f))
                   .fill(Fill::color({1, 0.7f, 0.4f, 1}));
  return vine;
}

}  // namespace

/** Element-stamp border: a ContourWalk stamps a composed ornament every
 *  24 px around a card's outline. The stamp is an Element, so it is
 *  described and recorded once and then replayed at each station rather
 *  than rebuilt per station. */
static void BM_Draw_StampBorder_Cached(benchmark::State& state) {
  Host host(800, 600);
  host.composer.render(box().child(box()
                                       .width(400)
                                       .height(280)
                                       .inset(100, 100, 300, 220)
                                       .absolute()
                                       .corners({20})
                                       .fill(Fill::color({0.1f, 0.1f, 0.2f, 1}))
                                       .foreground(starVine())));
  host.draw();
  for (auto _ : state) host.draw();
}
BENCHMARK(BM_Draw_StampBorder_Cached);

/** A transform-bound ornament (rotating star with a stamped border):
 *  paint-only volatility — content picture replays under the animated
 *  matrix instead of re-walking the border every frame. */
static void BM_Draw_SpinningStamped_TransformReplay(benchmark::State& state) {
  Host host(800, 600);
  choreograph::Output<float> spin{0.0f};
  host.composer.render(
      box().child(box()
                      .width(300)
                      .height(300)
                      .inset(250, 150, 250, 150)
                      .absolute()
                      .shape(shapes::rounded(shapes::star(7, 0.6f), 10))
                      .fill(Fill::color({0.9f, 0.4f, 0.3f, 1}))
                      .rotate(&spin)
                      .foreground(starVine())));
  host.draw();
  float angle = 0;
  for (auto _ : state) {
    spin = (angle += 0.7f);
    host.draw();
  }
}
BENCHMARK(BM_Draw_SpinningStamped_TransformReplay);

/** SkVertices art warp along a ~1500 px S-curve at 6 px stations
 *  (~500 strip verts), repainted per frame (Cache::None) — the honest
 *  worst case; a settled artline caches like any decoration. */
static void BM_Draw_ArtWarp_Live(benchmark::State& state) {
  Host host(900, 640);
  brush::Art vine =
      brush::artAlong(box().width(48).height(16).corners({8}).fill(
                          Fill::color({0.5f, 0.8f, 0.5f, 1})),
                      14, 6);
  host.composer.render(box().child(box()
                                       .absolute()
                                       .inset(20, 20, 20, 20)
                                       .shape([](SkSize s) {
                                         SkPathBuilder b;
                                         b.moveTo(0, s.height() / 2);
                                         b.cubicTo(s.width() * 0.3f, 0,
                                                   s.width() * 0.5f, s.height(),
                                                   s.width(), s.height() / 2);
                                         return b.detach();
                                       })
                                       .foreground(vine)
                                       .cache(Cache::None)));
  host.draw();
  for (auto _ : state) host.draw();
}
BENCHMARK(BM_Draw_ArtWarp_Live);

/** Sk2D lattice hatch filling a 400x400 blob per frame. */
static void BM_Draw_Hatch_Live(benchmark::State& state) {
  Host host(900, 640);
  host.composer.render(box().child(
      box()
          .width(400)
          .height(400)
          .centerAt({450, 320})
          .shape(shapes::blob(5, 0.2f))
          .background(lines::hatch(Fill::color({1, 1, 1, 0.5f}), 7, 1.2f))
          .cache(Cache::None)));
  host.draw();
  for (auto _ : state) host.draw();
}
BENCHMARK(BM_Draw_Hatch_Live);

// ---- Volatility is declared, not observed ---------------------------------
//
// A bound property marks its node volatile for as long as the binding
// exists, regardless of whether the value changed this frame. Volatility
// then propagates upward, so the ancestors of a bound node also drop their
// recordings and lose texture promotion.
//
// The fixture isolates that: a panel of `count` stroked, shaped cells, each
// recording its own picture, plus ONE accent cell in the same row whose fill
// is spelled either as a binding to an external Output or as a plain value.
// Everything else about the arms is identical, so the difference is entirely
// what declaring the binding costs the row, the frame and the root above it.
//
// PAIR ONE — the property does not move AT ALL. Whatever separates the two
// arms is paid purely for declaring a binding on a node that is provably
// holding still.
//
// PAIR TWO — the same colour actually moves, once every kSlowPeriod frames
// (three seconds at 60 Hz: slow enough that re-describing on the change is
// clearly an option). Each arm does the minimum work its spelling allows:
// the bound arm assigns the Output and never re-describes; the plain arm
// re-describes only on the frame the colour changes, and prunes everything
// except the one node that moved.

namespace {

enum class AccentFill { Bound, Plain };

Element slowThemedPanel(int count, AccentFill mode,
                        const choreograph::Output<Fill>* bound,
                        SkColor4f plain) {
  auto row = box().key("row").row().wrapLines().gap(2);
  for (int id = 0; id < count; ++id)
    row.child(box()
                  .key("c" + std::to_string(id))
                  .width(26)
                  .height(26)
                  .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
                  .fill(cellFill(id))
                  .stroke(brush::solid(
                      1.5f, Fill::color({0.95f, 0.86f, 0.55f, 1.0f}))));
  Element accent =
      box()
          .key("accent")
          .width(26)
          .height(26)
          .shape(shapes::star(7, 0.45f, 0.08f))
          .stroke(brush::solid(1.5f, Fill::color({0.10f, 0.10f, 0.12f, 1.0f})));
  if (mode == AccentFill::Bound)
    accent.fill(Animatable<Fill>(bound));
  else
    accent.fill(Fill::color(plain));
  row.child(std::move(accent));
  // Two container levels above the row, so the ancestor chain the poison
  // climbs is a realistic panel/frame/root, not a single node.
  return box().key("root").column().padding(6).child(
      box().key("frame").column().padding(4).child(std::move(row)));
}

/** Per-frame cache work, averaged, so the arms are comparable in NUMBERS
 *  and not only in wall time. */
struct CacheTally {
  double recorded = 0, baked = 0, painted = 0, live = 0, tex = 0, frames = 0;
  void add(const Composer::Stats& s) {
    recorded += (double)s.picturesRecorded;
    baked += (double)s.texturesBaked;
    painted += (double)s.nodesPainted;
    live += (double)s.picturesLive;
    tex += (double)s.texturesLive;
    frames += 1;
  }
  void report(benchmark::State& state) const {
    const double f = frames > 0 ? frames : 1;
    state.counters["rec/frame"] = recorded / f;
    state.counters["bake/frame"] = baked / f;
    state.counters["livePaint/frame"] = painted / f;
    state.counters["pics"] = live / f;
    state.counters["textures"] = tex / f;
  }
};

SkColor4f accentColor(int step) {
  const float t = (float)(step % 5) / 5.0f;
  return {0.20f + 0.70f * t, 0.55f - 0.30f * t, 0.85f - 0.40f * t, 1.0f};
}

constexpr int kSlowPeriod = 180;

/** Warms the panel past the settle window and any promotion warmup, and
 *  prints the profile rows when COMPOSE_BENCH_WHY is set so a surprising
 *  number can be read against what the cache state machine did. */
void warmPanel(Host& host) {
  for (int warm = 0; warm < 16; ++warm) host.draw();
  if (std::getenv("COMPOSE_BENCH_WHY")) {
    host.composer.setProfiling(true);
    host.draw();
    for (const auto& row : host.composer.profile())
      if (row.depth <= 3)
        std::printf("  [why] %-28s self=%7.3f ms cache=%d promotion=%d\n",
                    row.label.c_str(), row.selfMs, (int)row.cacheState,
                    (int)row.promotion);
    host.composer.setProfiling(false);
  }
}

void accentLadder(benchmark::internal::Benchmark* b) {
  b->Arg(32)->Arg(128)->Arg(512)->Unit(benchmark::kMicrosecond);
}

}  // namespace

static void BM_Draw_StillAccent_Bound(benchmark::State& state) {
  const int count = (int)state.range(0);
  choreograph::Output<Fill> tint{Fill::color(accentColor(0))};
  Host host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Bound, &tint, accentColor(0)));
  warmPanel(host);
  CacheTally tally;
  for (auto _ : state) {
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_StillAccent_Bound)->Apply(accentLadder);

static void BM_Draw_StillAccent_Plain(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Plain, nullptr, accentColor(0)));
  warmPanel(host);
  CacheTally tally;
  for (auto _ : state) {
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_StillAccent_Plain)->Apply(accentLadder);

static void BM_Draw_SlowAccent_Bound(benchmark::State& state) {
  const int count = (int)state.range(0);
  choreograph::Output<Fill> tint{Fill::color(accentColor(0))};
  Host host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Bound, &tint, accentColor(0)));
  warmPanel(host);
  CacheTally tally;
  int frame = 0;
  for (auto _ : state) {
    if (++frame % kSlowPeriod == 0)
      tint = Fill::color(accentColor(frame / kSlowPeriod));
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_SlowAccent_Bound)->Apply(accentLadder);

static void BM_Draw_SlowAccent_Plain(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Plain, nullptr, accentColor(0)));
  warmPanel(host);
  CacheTally tally;
  int frame = 0;
  for (auto _ : state) {
    if (++frame % kSlowPeriod == 0)
      host.composer.render(slowThemedPanel(count, AccentFill::Plain, nullptr,
                                           accentColor(frame / kSlowPeriod)));
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_SlowAccent_Plain)->Apply(accentLadder);

BENCHMARK_MAIN();
