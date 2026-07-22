// The SigilCompose perf gate (run in Release): what describe, reconcile,
// layout, and draw cost — and what memo and automatic picture caching
// actually save. Reference numbers live in STRESS_TESTS.md.

#include <sigilcompose/Compose.h>
#include <sigilcompose/Util.h>

#include <include/effects/SkImageFilters.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <benchmark/benchmark.h>

#include <random>
#include <string>
#include <vector>

using namespace sigil::compose;

using namespace std::chrono_literals;

namespace {

sigil::weave::FontContext &fonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

struct Row {
  std::string name;
  int score = 0;
  bool operator==(const Row &) const = default;
};

Element scoreRow(const Row &row) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14.0f;
  return box().row().gap(12).padding(8).corners({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .child(text(sigil::compose::util::toU8(row.name), style).grow(1))
      .child(text(sigil::compose::util::toU8(std::to_string(row.score)), style));
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
  sigil::motion::Ticker ticker;
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

/** A static decorated row: fill + drop shadow + stroked border — the chrome
 *  shape that pre-P0 defeated the structural prune (any decoration forced a
 *  re-patch + re-record every render). No memo: this exercises the no-memo
 *  prune over value decorations (Shadow, PathFormat). */
static Element decoratedRow(const Row &row) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14.0f;
  return box().row().gap(12).padding(8).corners({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .background(sigil::compose::util::shadow({0, 0, 0, 0.5f}, {0, 2}, 6))
      .foreground(
          sigil::compose::util::stroke(1.5f, Fill::color({0.5f, 0.5f, 0.6f, 1})))
      .child(text(sigil::compose::util::toU8(row.name), style).grow(1))
      .child(
          text(sigil::compose::util::toU8(std::to_string(row.score)), style));
}

static Element decoratedBoard(const std::vector<Row> &rows) {
  auto list = box().column().gap(4).padding(16);
  for (const Row &row : rows)
    list.child(decoratedRow(row).key(row.name)); // no memo — prune must cover it
  return list;
}

/** Re-render 100 static DECORATED rows, nothing changed. Pre-P0 every row
 *  re-patched + re-recorded (decorations defeated the prune); now the
 *  value-decoration prune skips them all (patchedNodes → 0). Render-only cost
 *  is describe-dominated (no memo re-builds the rows), so the patch savings
 *  are in the noise here — the win shows on the draw side below. */
static void BM_Render_100DecoratedRows_Unchanged(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  host.composer.render(decoratedBoard(rows));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.render(decoratedBoard(rows));
  state.counters["patchedNodes"] = (double)host.composer.stats().patchedNodes;
}
BENCHMARK(BM_Render_100DecoratedRows_Unchanged);

/** The realistic frame loop for static decorated chrome: re-render (no memo)
 *  then draw ONLY when dirty() — the standard host contract (API.md worked
 *  example 2 / util::Stage). Pre-P0, an unchanged decorated render still set
 *  contentDirty (every decoration defeated the prune), so dirty() stayed true
 *  and the host redrew the whole scene every frame; now the prune leaves
 *  dirty() false, so the host skips the draw entirely — the blurred-shadow
 *  rasterization is never paid. (draws% → 0, the frame collapses to describe
 *  cost.) */
static void BM_Frame_100DecoratedRows_Static(benchmark::State &state) {
  Host host;
  auto rows = makeRows(100);
  host.composer.render(decoratedBoard(rows));
  host.composer.draw(*host.surface->getCanvas());
  double draws = 0, frames = 0;
  for (auto _ : state) {
    host.composer.render(decoratedBoard(rows));
    if (host.composer.dirty()) { // host skips clean frames
      host.composer.draw(*host.surface->getCanvas());
      ++draws;
    }
    ++frames;
  }
  state.counters["draws%"] = 100.0 * draws / frames;
}
BENCHMARK(BM_Frame_100DecoratedRows_Static);

/** Cold describe + mount of the full 100-row tree (worst case). */
static void BM_Render_100Rows_Cold(benchmark::State &state) {
  auto rows = makeRows(100);
  for (auto _ : state) {
    Host host;
    host.composer.render(scoreboard(rows));
    size_t instances = host.composer.stats().instances;
    benchmark::DoNotOptimize(instances);
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
  sigil::weave::TextStyle style;
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
  sigil::weave::TextStyle style;
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

SkiaGraphiteContext *graphite() {
  static std::unique_ptr<SkiaGraphiteContext> ctx =
      SkiaGraphiteContext::createMetal(sigil::compose::bench::gpuDevice(),
                                       sigil::compose::bench::gpuQueue());
  return ctx.get();
}

void submitGraphite(SkiaGraphiteContext &graphiteContext) {
  auto recording = graphiteContext.recorder()->snap();
  if (!recording)
    return;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  graphiteContext.context()->insertRecording(info);
  graphiteContext.context()->submit();
}

} // namespace

static void BM_Draw_100Rows_Cached_Graphite(benchmark::State &state) {
  SkiaGraphiteContext *graphiteContext = graphite();
  if (!graphiteContext) {
    state.SkipWithError("Graphite Metal context is unavailable");
    return;
  }
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphiteContext->recorder(), SkImageInfo::MakeN32Premul(800, 2400));
  if (!surface) {
    state.SkipWithError("Graphite render target creation failed");
    return;
  }
  Host host;
  host.composer.render(scoreboard(makeRows(100)));
  host.composer.draw(*surface->getCanvas());
  submitGraphite(*graphiteContext);
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite(*graphiteContext);
  }
}
BENCHMARK(BM_Draw_100Rows_Cached_Graphite);

// ---- instances(): the flyweight repeat layer at scale --------------------

#include <sigilcompose/Instances.h>

namespace {

std::pair<std::shared_ptr<sigil::compose::instancing::Atlas>,
          std::shared_ptr<sigil::compose::instancing::Pool>>
makeInstanceScene(size_t count) {
  using namespace sigil::compose::instancing;
  auto atlas = std::make_shared<Atlas>();
  for (int i = 0; i < 4; ++i)
    atlas->cell(box()
                    .corners({6})
                    .fill(Fill::color({0.2f + 0.2f * (float)i, 0.5f, 0.8f, 1})),
                {24, 24});
  auto pool = std::make_shared<Pool>();
  uint32_t rng = 12345;
  auto next = [&rng] {
    rng = rng * 1664525u + 1013904223u;
    return (float)(rng >> 8) / (float)(1u << 24);
  };
  for (size_t i = 0; i < count; ++i)
    pool->add({next() * 800.0f, next() * 2400.0f}, (int)(i % 4),
              next() * 6.28f, 0.5f + next());
  return {atlas, pool};
}

} // namespace

/** 10k pool, Live mode: full per-frame stamp cost (array build + one
 *  drawAtlas) on CPU raster. */
static void BM_Draw_Instances10k_Live(benchmark::State &state) {
  using namespace sigil::compose::instancing;
  Host host;
  auto [atlas, pool] = makeInstanceScene(10000);
  host.composer.render(
      box().child(instances(atlas, pool, Mode::Live)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_Instances10k_Live);

/** Same pool, Data mode, untouched: the cached-picture replay price. */
static void BM_Draw_Instances10k_DataCached(benchmark::State &state) {
  using namespace sigil::compose::instancing;
  Host host;
  auto [atlas, pool] = makeInstanceScene(10000);
  host.composer.render(box().child(instances(atlas, pool)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_Instances10k_DataCached);

// ---- brush-arc tail: art warp + hatch, the per-frame paint price -------

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Lines.h>

/** SkVertices art warp along a ~1500 px S-curve at 6 px stations
 *  (~500 strip verts), repainted per frame (Cache::None) — the honest
 *  worst case; a settled artline caches like any decoration. */
static void BM_Draw_ArtWarp_Live(benchmark::State &state) {
  Host host(900, 640);
  brushes::ArtBrush vine = brushes::artAlong(
      box().width(48).height(16).corners({8})
          .fill(Fill::color({0.5f, 0.8f, 0.5f, 1})),
      14, 6);
  host.composer.render(box().child(
      box().absolute().inset(20, 20, 20, 20)
          .outline([](SkSize s) {
            SkPathBuilder b;
            b.moveTo(0, s.height() / 2);
            b.cubicTo(s.width() * 0.3f, 0, s.width() * 0.5f, s.height(),
                      s.width(), s.height() / 2);
            return b.detach();
          })
          .foreground(vine)
          .cache(Cache::None)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_ArtWarp_Live);

/** Sk2D lattice hatch filling a 400x400 blob per frame. */
static void BM_Draw_Hatch_Live(benchmark::State &state) {
  Host host(900, 640);
  host.composer.render(box().child(
      box().width(400).height(400).centerAt({450, 320})
          .outline(shapes::blob(5, 0.2f))
          .background(lines::hatch(Fill::color({1, 1, 1, 0.5f}), 7, 1.2f))
          .cache(Cache::None)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_Hatch_Live);

/** The design claim: instanced masses are a GPU play. Same 10k pool,
 *  Live mode, Graphite target, per-frame submit. */
static void BM_Draw_Instances10k_Live_Graphite(benchmark::State &state) {
  using namespace sigil::compose::instancing;
  SkiaGraphiteContext *graphiteContext = graphite();
  if (!graphiteContext) {
    state.SkipWithError("Graphite Metal context is unavailable");
    return;
  }
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphiteContext->recorder(), SkImageInfo::MakeN32Premul(800, 2400));
  if (!surface) {
    state.SkipWithError("Graphite render target creation failed");
    return;
  }
  Host host;
  auto [atlas, pool] = makeInstanceScene(10000);
  host.composer.render(
      box().child(instances(atlas, pool, Mode::Live)));
  host.composer.draw(*surface->getCanvas());
  submitGraphite(*graphiteContext);
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite(*graphiteContext);
  }
}
BENCHMARK(BM_Draw_Instances10k_Live_Graphite);

static void BM_Draw_Bloom_PictureReplay_Graphite(benchmark::State &state) {
  SkiaGraphiteContext *graphiteContext = graphite();
  if (!graphiteContext) {
    state.SkipWithError("Graphite Metal context is unavailable");
    return;
  }
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphiteContext->recorder(), SkImageInfo::MakeN32Premul(900, 300));
  if (!surface) {
    state.SkipWithError("Graphite render target creation failed");
    return;
  }
  Host host(900, 300);
  host.composer.render(bloomBlock(Cache::Picture));
  host.composer.draw(*surface->getCanvas());
  submitGraphite(*graphiteContext);
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite(*graphiteContext);
  }
}
BENCHMARK(BM_Draw_Bloom_PictureReplay_Graphite);

static void BM_Draw_Bloom_TextureBaked_Graphite(benchmark::State &state) {
  SkiaGraphiteContext *graphiteContext = graphite();
  if (!graphiteContext) {
    state.SkipWithError("Graphite Metal context is unavailable");
    return;
  }
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphiteContext->recorder(), SkImageInfo::MakeN32Premul(900, 300));
  if (!surface) {
    state.SkipWithError("Graphite render target creation failed");
    return;
  }
  Host host(900, 300);
  host.composer.render(bloomBlock(Cache::Texture));
  host.composer.draw(*surface->getCanvas());
  submitGraphite(*graphiteContext);
  for (auto _ : state) {
    host.composer.draw(*surface->getCanvas());
    submitGraphite(*graphiteContext);
  }
}
BENCHMARK(BM_Draw_Bloom_TextureBaked_Graphite);

#endif // COMPOSE_BENCH_GRAPHITE

// ---- "UI as particles": the scale answer ----------------------------------
// Millions of visual items are ONE element, not a million elements: an
// EnTT registry (SoA component pools, cache-friendly iteration) stepped
// as a Ticker steppable, rendered by a single Cache::None custom leaf
// batching everything into one SkCanvas::drawAtlas call — the same
// GlyphRSXformBatches pattern sigil::weave::Choreograph uses for glyphs.

#include <entt/entt.hpp>

#include <include/core/SkRSXform.h>

namespace {

struct Particle {
  entt::registry registry;
  sk_sp<SkImage> sprite;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos { float x, y; };
  struct Vel { float dx, dy; };

  explicit Particle(size_t count) {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    s->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(0xff7ee8ff);
    s->getCanvas()->drawCircle(4, 4, 3.5f, p);
    sprite = s->makeImageSnapshot();

    std::mt19937 rng{11};
    auto unit = [&] { return (float)(rng() % 10000) / 10000.0f; };
    for (size_t i = 0; i < count; ++i) {
      entt::entity e = registry.create();
      registry.emplace<Pos>(e, unit() * 800.0f, unit() * 800.0f);
      registry.emplace<Vel>(e, unit() * 80 - 40, unit() * 80 - 40);
    }
    xforms.reserve(count);
    texRects.assign(count, SkRect::MakeWH(8, 8));
  }

  void step(float dt) {
    registry.view<Pos, const Vel>().each([dt](Pos &p, const Vel &v) {
      p.x += v.dx * dt;
      p.y += v.dy * dt;
      if (p.x < 0) p.x += 800; else if (p.x > 800) p.x -= 800;
      if (p.y < 0) p.y += 800; else if (p.y > 800) p.y -= 800;
    });
  }

  void draw(SkCanvas &c) {
    xforms.clear();
    registry.view<const Pos>().each([this](const Pos &p) {
      xforms.push_back(SkRSXform::Make(1, 0, p.x, p.y));
    });
    c.drawAtlas(sprite.get(), SkSpan(xforms.data(), xforms.size()),
                SkSpan(texRects.data(), texRects.size()), {},
                SkBlendMode::kPlus, SkSamplingOptions(SkFilterMode::kNearest),
                nullptr, nullptr);
  }
};

} // namespace

/** Full frame at N particles: EnTT SoA step + one drawAtlas leaf. */
static void BM_Particles_EnttAtlasLeaf(benchmark::State &state) {
  const size_t count = (size_t)state.range(0);
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(box().child(
      custom([particles](SkCanvas &c, const PaintContext &) {
        particles->draw(c);
      }).inset(0).cache(Cache::None)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state) {
    particles->step(1.0f / 120.0f);
    host.composer.draw(*host.surface->getCanvas());
  }
  state.counters["perParticleNs"] =
      benchmark::Counter((double)count, benchmark::Counter::kIsIterationInvariantRate |
                                            benchmark::Counter::kInvert);
}
BENCHMARK(BM_Particles_EnttAtlasLeaf)->Arg(10000)->Arg(100000)->Arg(1000000);

/** The anti-pattern for contrast: per-particle draw calls. */
static void BM_Particles_DrawCircleLoop(benchmark::State &state) {
  const size_t count = (size_t)state.range(0);
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(box().child(
      custom([particles](SkCanvas &c, const PaintContext &) {
        SkPaint p;
        p.setAntiAlias(true);
        p.setColor(0xff7ee8ff);
        particles->registry.view<const Particle::Pos>().each(
            [&](const Particle::Pos &pos) {
              c.drawCircle(pos.x, pos.y, 3.5f, p);
            });
      }).inset(0).cache(Cache::None)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state) {
    particles->step(1.0f / 120.0f);
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Particles_DrawCircleLoop)->Arg(10000);

#ifdef COMPOSE_BENCH_GRAPHITE
/** The same particle frame against a Graphite Metal target: drawAtlas
 *  becomes an instanced GPU batch; the CPU cost is building RSXforms. */
static void BM_Particles_EnttAtlasLeaf_Graphite(benchmark::State &state) {
  const size_t count = (size_t)state.range(0);
  SkiaGraphiteContext *graphiteContext = graphite();
  if (!graphiteContext) {
    state.SkipWithError("Graphite Metal context is unavailable");
    return;
  }
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(
      graphiteContext->recorder(), SkImageInfo::MakeN32Premul(800, 800));
  if (!surface) {
    state.SkipWithError("Graphite render target creation failed");
    return;
  }
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(
      box().child(custom([particles](SkCanvas &c, const PaintContext &) {
                    particles->draw(c);
                  })
                      .inset(0)
                      .cache(Cache::None)));
  host.composer.draw(*surface->getCanvas());
  submitGraphite(*graphiteContext);
  for (auto _ : state) {
    particles->step(1.0f / 120.0f);
    host.composer.draw(*surface->getCanvas());
    submitGraphite(*graphiteContext);
  }
  state.counters["perParticleNs"] = benchmark::Counter(
      (double)count, benchmark::Counter::kIsIterationInvariantRate |
                         benchmark::Counter::kInvert);
}
BENCHMARK(BM_Particles_EnttAtlasLeaf_Graphite)->Arg(100000)->Arg(1000000);
#endif

BENCHMARK_MAIN();

// ---------------------------------------------------------------------------
// Completeness-round benches: stamps, regions vs SkSL tiling, hitTest,
// direct leaf blending, transform-replay caching.

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Layouts.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkStream.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/encode/SkPngEncoder.h>

#include <sigilimage/ImageAsset.h>

namespace {

std::shared_ptr<sigil::image::ImageAsset> benchAtlas() {
  static std::shared_ptr<sigil::image::ImageAsset> asset = [] {
    SkBitmap src;
    src.allocN32Pixels(64, 16);
    for (int i = 0; i < 4; ++i)
      src.erase(SkColorSetRGB((U8CPU)(60 + i * 40), 40, 90),
                SkIRect::MakeXYWH(i * 16, 0, 16, 16));
    SkDynamicMemoryWStream stream;
    SkPngEncoder::Encode(&stream, src.pixmap(), {});
    return std::make_shared<sigil::image::ImageAsset>(
        *sigil::image::ImageAsset::decode(stream.detachAsData()));
  }();
  return asset;
}

struct ChunkProps {
  std::vector<int> ids;
  bool operator==(const ChunkProps &) const = default;
};

Element benchChunk(const ChunkProps &p) {
  constexpr float kTile = 16.0f;
  auto tiles = box().width(10 * kTile).height(10 * kTile);
  for (int i = 0; i < (int)p.ids.size(); ++i)
    tiles.child(image(benchAtlas())
                    .region(SkRect::MakeXYWH(
                        (float)(p.ids[(size_t)i] % 4) * 16, 0, 16, 16))
                    .absolute()
                    .inset((float)(i % 10) * kTile, (float)(i / 10) * kTile,
                           0, 0)
                    .width(kTile).height(kTile));
  return tiles;
}

} // namespace

/** 6x4 chunks of 10x10 region tiles (2400 tiles): steady-state redraw,
 *  everything picture-cached (the tile-map baseline). */
static void BM_Draw_TileGrid_Region_Cached(benchmark::State &state) {
  Host host(960, 640);
  std::vector<ChunkProps> chunks(24);
  for (int c = 0; c < 24; ++c)
    for (int i = 0; i < 100; ++i)
      chunks[(size_t)c].ids.push_back((i * 31 + c) % 4);
  auto describe = [&] {
    auto grid = box().row().wrapLines().width(6 * 160.0f);
    for (int c = 0; c < 24; ++c)
      grid.child(memo(chunks[(size_t)c], benchChunk)
                     .key("c" + std::to_string(c)));
    return box().child(grid);
  };
  host.composer.render(describe());
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state) {
    host.composer.render(describe());
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Draw_TileGrid_Region_Cached);

/** Same grid, one chunk's data mutated per iteration — the incremental
 *  cost item 15 promises: one chunk re-records, 23 replay. */
static void BM_Draw_TileGrid_Region_OneChunkChanged(benchmark::State &state) {
  Host host(960, 640);
  std::vector<ChunkProps> chunks(24);
  for (int c = 0; c < 24; ++c)
    for (int i = 0; i < 100; ++i)
      chunks[(size_t)c].ids.push_back((i * 31 + c) % 4);
  auto describe = [&] {
    auto grid = box().row().wrapLines().width(6 * 160.0f);
    for (int c = 0; c < 24; ++c)
      grid.child(memo(chunks[(size_t)c], benchChunk)
                     .key("c" + std::to_string(c)));
    return box().child(grid);
  };
  host.composer.render(describe());
  host.composer.draw(*host.surface->getCanvas());
  int flip = 0;
  for (auto _ : state) {
    chunks[7].ids[(size_t)(flip++ % 100)] ^= 1;
    host.composer.render(describe());
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Draw_TileGrid_Region_OneChunkChanged);

/** Item 16: the same 2400-tile field as ONE SkSL fill sampling the
 *  atlas procedurally — a single draw, no per-tile identity. */
static void BM_Draw_TileGrid_SkSLFill(benchmark::State &state) {
  Host host(960, 640);
  static const char *kSkSL = R"(
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
  const auto &frame = benchAtlas()->frames().front();
  sk_sp<SkShader> atlasShader = frame.image->makeShader(
      SkTileMode::kClamp, SkTileMode::kClamp, SkSamplingOptions());
  SkRuntimeShaderBuilder builder(effect);
  builder.child("atlas") = atlasShader;
  sk_sp<SkShader> field = builder.makeShader();

  host.composer.render(box().child(
      box().width(960).height(640).fill(Fill::shader(field))
          .cache(Cache::None)));
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_TileGrid_SkSLFill);

/** Element-stamp border: a card whose ContourWalk stamps a composed
 *  ornament every 24px — recorded once, replayed ~46 times per draw. */
static void BM_Draw_StampBorder_Cached(benchmark::State &state) {
  Host host(800, 600);
  ContourWalk vine;
  vine.spacing = 24.0f;
  vine.stamp = box().width(14).height(14)
                   .outline(shapes::star(4, 0.45f))
                   .fill(Fill::color({1, 0.7f, 0.4f, 1}));
  host.composer.render(box().child(
      box().width(400).height(280).inset(100, 100, 300, 220).absolute()
          .corners({20}).fill(Fill::color({0.1f, 0.1f, 0.2f, 1}))
          .foreground(vine)));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_StampBorder_Cached);

/** hitTest through a 3-level tree with shaped and rotated nodes. */
static void BM_HitTest_ShapedTree(benchmark::State &state) {
  Host host(900, 640);
  auto scatter = layout(layouts::Scatter{.seed = 3}).inset(0);
  for (int i = 0; i < 50; ++i)
    scatter.child(box().key("blob" + std::to_string(i))
                      .width(60).height(60)
                      .outline(shapes::blob((uint32_t)i, 0.3f, 7))
                      .rotate((float)i * 7.0f)
                      .fill(Fill::color({0.5f, 0.3f, 0.4f, 1})));
  host.composer.render(box().child(scatter));
  host.composer.draw(*host.surface->getCanvas());
  int step = 0;
  for (auto _ : state) {
    const SkPoint pt{(float)(step * 37 % 900), (float)(step * 53 % 640)};
    benchmark::DoNotOptimize(host.composer.hitTest(pt));
    ++step;
  }
}
BENCHMARK(BM_HitTest_ShapedTree);

/** 100 plus-blended blob leaves — the direct-blend fast path (each
 *  used to cost a device-sized saveLayer). */
static void BM_Draw_BlendField_100Blobs(benchmark::State &state) {
  Host host(900, 640);
  auto scatter = layout(layouts::Scatter{.seed = 9, .jitter = 0.8f})
                     .inset(0);
  for (int i = 0; i < 100; ++i)
    scatter.child(box().width(70).height(60)
                      .outline(shapes::blob((uint32_t)(i + 1), 0.3f, 6))
                      .fill(Fill::color({0.4f, 0.2f, 0.4f, 0.5f}))
                      .blend(SkBlendMode::kPlus));
  host.composer.render(box().child(scatter));
  host.composer.draw(*host.surface->getCanvas());
  for (auto _ : state)
    host.composer.draw(*host.surface->getCanvas());
}
BENCHMARK(BM_Draw_BlendField_100Blobs);

/** A transform-bound ornament (rotating star with a stamped border):
 *  paint-only volatility — content picture replays under the animated
 *  matrix instead of re-walking the border every frame. */
static void BM_Draw_SpinningStamped_TransformReplay(benchmark::State &state) {
  Host host(800, 600);
  choreograph::Output<float> spin{0.0f};
  ContourWalk vine;
  vine.spacing = 24.0f;
  vine.stamp = box().width(14).height(14)
                   .outline(shapes::star(4, 0.45f))
                   .fill(Fill::color({1, 0.7f, 0.4f, 1}));
  host.composer.render(box().child(
      box().width(300).height(300).inset(250, 150, 250, 150).absolute()
          .outline(shapes::rounded(shapes::star(7, 0.6f), 10))
          .fill(Fill::color({0.9f, 0.4f, 0.3f, 1}))
          .rotate(&spin)
          .foreground(vine)));
  host.composer.draw(*host.surface->getCanvas());
  float angle = 0;
  for (auto _ : state) {
    spin = (angle += 0.7f);
    host.composer.draw(*host.surface->getCanvas());
  }
}
BENCHMARK(BM_Draw_SpinningStamped_TransformReplay);
