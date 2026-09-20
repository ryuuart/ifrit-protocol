// MANY ITEMS IN ONE DRAW: the flyweight repeat layer, where a pool of
// placements is stamped from an atlas of cells, and the particle field,
// where an EnTT registry stepped as a ticker steppable is batched by one
// Cache::None leaf into a single drawAtlas — beside the per-item draw
// loop each of them exists to replace. Both are run again on a Graphite
// Metal surface, which is where a batch of this size belongs.

#include <include/core/SkCanvas.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/core/Instances.h>

#include <entt/entt.hpp>
#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

// ---- instances(): the flyweight repeat layer at scale --------------------

namespace {

std::pair<std::shared_ptr<instancing::CellSheet>,
          std::shared_ptr<instancing::Pool>>
makeInstanceScene(size_t count) {
  using namespace instancing;
  auto atlas = std::make_shared<CellSheet>();
  for (int i = 0; i < 4; ++i)
    atlas->cell(box().corners({6}).fill(
                    Fill::color({0.2f + 0.2f * (float)i, 0.5f, 0.8f, 1})),
                {24, 24});
  auto pool = std::make_shared<Pool>();
  uint32_t rng = 12345;
  auto next = [&rng] {
    rng = rng * 1664525u + 1013904223u;
    return (float)(rng >> 8u) / (float)(1u << 24u);
  };
  for (size_t i = 0; i < count; ++i)
    pool->add({next() * 800.0f, next() * 2400.0f}, (int)(i % 4), next() * 6.28f,
              0.5f + next());
  return {atlas, pool};
}

void instancesArm(benchmark::State& state, instancing::Mode mode) {
  const size_t count = (size_t)state.range(0);
  Host host;
  auto [atlas, pool] = makeInstanceScene(count);
  host.composer.render(box().children({instances(atlas, pool, mode)}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["instances"] = (double)count;
  state.SetItemsProcessed(state.iterations() * (int64_t)count);
}

}  // namespace

/** Live mode: full per-frame stamp cost (array build + one drawAtlas) on
 *  CPU raster. */
static void BM_Draw_Instances_Live(benchmark::State& state) {
  instancesArm(state, instancing::Mode::Live);
}
BENCHMARK(BM_Draw_Instances_Live)->Arg(10000);

/** Same pool, Data mode, untouched: the cached-picture replay price. */
static void BM_Draw_Instances_DataCached(benchmark::State& state) {
  instancesArm(state, instancing::Mode::Data);
}
BENCHMARK(BM_Draw_Instances_DataCached)->Arg(10000);

// ---- "UI as particles": the scale answer ----------------------------------
// Millions of visual items are ONE element, not a million elements: an
// EnTT registry (SoA component pools, cache-friendly iteration) stepped
// as a Ticker steppable, rendered by a single Cache::None custom leaf
// batching everything into one SkCanvas::drawAtlas call — the same
// GlyphRSXformBatches pattern the glyph engine uses for text.

namespace {

struct Particle {
  entt::registry registry;
  sk_sp<SkImage> sprite;
  std::vector<SkRSXform> xforms;
  std::vector<SkRect> texRects;

  struct Pos {
    float x, y;
  };
  struct Vel {
    float dx, dy;
  };

  explicit Particle(size_t count) {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
    s->getCanvas()->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor(0xff7ee8ff);
    s->getCanvas()->drawCircle(4, 4, 3.5f, p);
    sprite = s->makeImageSnapshot();

    // a fixed seed; the scene must render the same on every run
    // NOLINTNEXTLINE(bugprone-random-generator-seed)
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
    registry.view<Pos, const Vel>().each([dt](Pos& p, const Vel& v) {
      p.x += v.dx * dt;
      p.y += v.dy * dt;
      if (p.x < 0)
        p.x += 800;
      else if (p.x > 800)
        p.x -= 800;
      if (p.y < 0)
        p.y += 800;
      else if (p.y > 800)
        p.y -= 800;
    });
  }

  void draw(SkCanvas& c) {
    xforms.clear();
    registry.view<const Pos>().each([this](const Pos& p) {
      xforms.push_back(SkRSXform::Make(1, 0, p.x, p.y));
    });
    c.drawAtlas(sprite.get(), SkSpan(xforms.data(), xforms.size()),
                SkSpan(texRects.data(), texRects.size()), {},
                SkBlendMode::kPlus, SkSamplingOptions(SkFilterMode::kNearest),
                nullptr, nullptr);
  }
};

void reportPerParticle(benchmark::State& state, size_t count) {
  state.counters["perParticleNs"] = benchmark::Counter(
      (double)count, benchmark::Counter::kIsIterationInvariantRate |
                         benchmark::Counter::kInvert);
}

}  // namespace

/** Full frame at N particles: EnTT SoA step + one drawAtlas leaf. */
static void BM_Particles_EnttAtlasLeaf(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(
      box().children({custom([particles](SkCanvas& c) {
                        particles->draw(c);
                      })
                          .inset(0)
                          .cache(Cache::None)}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    particles->step(1.0f / 120.0f);
    host.draw();
  }
  reportPerParticle(state, count);
}
BENCHMARK(BM_Particles_EnttAtlasLeaf)->Arg(10000)->Arg(100000)->Arg(1000000);

/** The anti-pattern for contrast: per-particle draw calls. */
static void BM_Particles_DrawCircleLoop(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(
      box().children({custom([particles](SkCanvas& c) {
                        SkPaint p;
                        p.setAntiAlias(true);
                        p.setColor(0xff7ee8ff);
                        particles->registry.view<const Particle::Pos>().each(
                            [&](const Particle::Pos& pos) {
                              c.drawCircle(pos.x, pos.y, 3.5f, p);
                            });
                      })
                          .inset(0)
                          .cache(Cache::None)}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    particles->step(1.0f / 120.0f);
    host.draw();
  }
  reportPerParticle(state, count);
}
BENCHMARK(BM_Particles_DrawCircleLoop)->Arg(10000);

#ifdef SIGIL_BENCH_GPU
// ---- The same batches against a Graphite Metal surface ----
// A batch of this size is a GPU play, and these arms are what says so:
// the same pool and the same field, one submit per frame.

using sigil::compose::bench::GraphiteTarget;

/** The design claim: instanced masses are a GPU play. Same 10k pool,
 *  Live mode, Graphite target, per-frame submit. */
static void BM_Draw_Instances_Live_Graphite(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  GraphiteTarget target(state, 800, 2400);
  if (!target.ok()) return;
  Host host;
  auto [atlas, pool] = makeInstanceScene(count);
  host.composer.render(
      box().children({instances(atlas, pool, instancing::Mode::Live)}));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submit();
  }
  state.counters["instances"] = (double)count;
}
BENCHMARK(BM_Draw_Instances_Live_Graphite)->Arg(10000);

/** The same particle frame against a Graphite Metal target: drawAtlas
 *  becomes an instanced GPU batch; the CPU cost is building RSXforms. */
static void BM_Particles_EnttAtlasLeaf_Graphite(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  GraphiteTarget target(state, 800, 800);
  if (!target.ok()) return;
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(
      box().children({custom([particles](SkCanvas& c) {
                        particles->draw(c);
                      })
                          .inset(0)
                          .cache(Cache::None)}));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    particles->step(1.0f / 120.0f);
    host.composer.draw(target.canvas());
    target.submit();
  }
  reportPerParticle(state, count);
}
BENCHMARK(BM_Particles_EnttAtlasLeaf_Graphite)->Arg(100000)->Arg(1000000);

#endif  // SIGIL_BENCH_GPU
