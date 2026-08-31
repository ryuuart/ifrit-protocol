// SigilComposeCore whole-scene benchmarks at a single fixed size: what
// describe, reconcile, layout and draw cost on a realistic scoreboard, and
// what memoization, automatic picture caching and texture promotion save
// against the same scene without them. The scaling matrix is in
// ComposeCoreBench.cpp, the other file of this binary.

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkStream.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/encode/SkPngEncoder.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/instances/Instances.h>
#include <sigilimage/asset/ImageAsset.h>

#include <cmath>
#include <entt/entt.hpp>
#include <memory>
#include <random>
#include <string>
#include <utility>
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
      .corners({6})
      .fill(Fill::color({0.13f, 0.13f, 0.16f, 1}))
      .child(text(toU8(row.name), style).grow(1))
      .child(text(toU8(std::to_string(row.score)), style));
}

Element scoreboard(const std::vector<Row>& rows) {
  auto list = box().column().gap(4).padding(16);
  for (const Row& row : rows) list.child(memo(row, scoreRow).key(row.name));
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
    for (const Row& row : rows) list.child(memo(row, scoreRow).key(row.name));
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
    list.child(memo(row, scoreRow).key(row.name));
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
  for (const Row& row : rows) list.child(memo(row, scoreRow).key(row.name));
  host.composer.render(list);
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
}
BENCHMARK(BM_Draw_100Rows_TextureBlit);

// ---- Effects: a blurred headline, cached two ways --------------------------

namespace {

/** A blur-effected headline: picture replay re-runs the filter every
 *  draw; Cache::Texture bakes it — the effects payoff. */
Element bloomBlock(Cache mode) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 64.0f;
  style.paint.foreground.setColor(0xff7ee8ff);
  return box()
      .padding(24)
      .cache(mode)
      .effect(Effect::filter(SkImageFilters::Blur(12, 12, nullptr)))
      .child(text(u8"BLOOM PIPELINE", style));
}

void bloomArm(benchmark::State& state, Cache mode) {
  Host host(900, 300);
  host.composer.render(bloomBlock(mode));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
}

}  // namespace

static void BM_Draw_Bloom_PictureReplay(benchmark::State& state) {
  bloomArm(state, Cache::Picture);
}
BENCHMARK(BM_Draw_Bloom_PictureReplay);

static void BM_Draw_Bloom_TextureBaked(benchmark::State& state) {
  bloomArm(state, Cache::Texture);
}
BENCHMARK(BM_Draw_Bloom_TextureBaked);

// ---- A blur whose SIGMA VARIES across the node, three ways ----------------
//
// The question these arms answer: how does Effect::blur's pyramid scale in
// the sigma range, against writing the same effect by hand. The pyramid
// builds a fixed number of levels and mixes between them, so its cost does
// not follow sigma; a hand-written variable-sigma kernel cannot be made
// separable (the radius differs per pixel), so it pays (2R+1)² taps at every
// pixel.
//
// All three arms paint the SAME node — hard vertical stripes, so the blur
// has detail to destroy — driven by the SAME parameter map, and differ only
// in the effect:
//
//  Pyramid     Effect::blur(map, sigma) — fixed levels plus one mix pass.
//  Naive       the workaround that produces the same PICTURE: one SkSL pass
//              whose kernel is sized for the worst sigma anywhere in the
//              node.
//  ConstantMax Effect::filter(Blur(σ, σ)) — the floor. It does not produce
//              the picture (nothing varies across the node), but it is what
//              an author reaches for when they give up on varying it, so it
//              prices the feature against giving up.
//
// Each is run at two sigmas an octave-and-a-bit apart (6 and 24) because the
// claim under test is about scaling, not about any single sigma.

namespace {

constexpr int kVaryPanelRaster = 96;  // the naive kernel is O(σ²) on the CPU
constexpr int kVaryPanelGpu = 256;

/** Hard 8px stripes in node-local space — detail for the blur to destroy. */
Material stripeTarget() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float band = mod(floor(p.x / 8.0), 2.0);"
                 "  return band < 1.0 ? half4(1) : half4(0, 0, 0, 1);"
                 "}"));
    return effect;
  }();
  return Material::sksl(fx);
}

/** The parameter: 0 at the node's left edge, 1 at its right. */
Material sigmaRamp() {
  return Material::linearUnit({0, 0}, {1, 0},
                              {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

/** THE WORKAROUND, written the way an author has to write it: the loop
 *  bound is a COMPILE-TIME constant (SkSL has no cheap dynamic bound), so
 *  it is the worst radius in the node, paid at every pixel. One effect
 *  cached per radius — minting one per call would only measure the
 *  compiler. */
sk_sp<SkRuntimeEffect> naiveVaryingBlur(int radius) {
  static std::vector<std::pair<int, sk_sp<SkRuntimeEffect>>> cache;
  for (const auto& [r, fx] : cache)
    if (r == radius) return fx;
  const std::string r = std::to_string(radius);
  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(
      ("uniform shader content;"
       "uniform shader param;"
       "uniform float uMaxSigma;"
       "half4 main(float2 p) {"
       "  float sigma = max(param.eval(p).r * uMaxSigma, 0.01);"
       "  float inv = -0.5 / (sigma * sigma);"
       "  half4 sum = half4(0);"
       "  float wsum = 0.0;"
       "  for (int dy = -" +
       r + "; dy <= " + r +
       "; ++dy) {"
       "    for (int dx = -" +
       r + "; dx <= " + r +
       "; ++dx) {"
       "      float w = exp(float(dx * dx + dy * dy) * inv);"
       "      sum += content.eval(p + float2(float(dx), float(dy))) * half(w);"
       "      wsum += w;"
       "    }"
       "  }"
       "  return sum / half(wsum);"
       "}")
          .c_str()));
  if (!effect)
    SkDebugf("[bench] naive varying blur failed: %s\n", error.c_str());
  cache.emplace_back(radius, effect);
  return effect;
}

Element varyingPanel(int side, Effect e) {
  return box()
      .width((float)side)
      .height((float)side)
      .fill(stripeTarget())
      .effect(std::move(e));
}

enum class BlurArm { Pyramid, Naive, ConstantMax };

Effect blurEffect(BlurArm arm, float sigma) {
  switch (arm) {
    case BlurArm::Pyramid:
      return Effect::blur(sigmaRamp(), sigma);
    case BlurArm::Naive: {
      // A Gaussian is negligible past three standard deviations, so R = 3σ
      // is the radius the worst pixel in the node needs — and every pixel
      // pays it.
      const int radius = (int)std::lround(3.0f * sigma);
      return Effect::shader(naiveVaryingBlur(radius), {{"uMaxSigma", sigma}})
          .child("param", sigmaRamp());
    }
    case BlurArm::ConstantMax:
      return Effect::filter(SkImageFilters::Blur(sigma, sigma, nullptr));
  }
  return {};
}

/** One draw per iteration on a raster surface, effect re-resolved each
 *  time (Cache::None keeps the filter out of a picture so the arms measure
 *  the FILTER, not the replay). */
void rasterVaryingArm(benchmark::State& state, BlurArm arm) {
  const float sigma = (float)state.range(0);
  Host host(kVaryPanelRaster, kVaryPanelRaster);
  host.composer.render(varyingPanel(kVaryPanelRaster, blurEffect(arm, sigma))
                           .cache(Cache::None));
  host.draw();  // warm the SkSL compile
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["sigma"] = sigma;
}

}  // namespace

static void BM_Draw_VaryingBlur_Pyramid(benchmark::State& state) {
  rasterVaryingArm(state, BlurArm::Pyramid);
}
BENCHMARK(BM_Draw_VaryingBlur_Pyramid)
    ->Arg(6)
    ->Arg(24)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_VaryingBlur_Naive(benchmark::State& state) {
  rasterVaryingArm(state, BlurArm::Naive);
}
// The naive kernel at σ = 24 is a 145² tap loop per pixel on the CPU: one
// iteration is the whole budget, and three at σ = 6 are already generous.
BENCHMARK(BM_Draw_VaryingBlur_Naive)
    ->Arg(6)
    ->Iterations(3)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Draw_VaryingBlur_Naive)
    ->Arg(24)
    ->Iterations(1)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_VaryingBlur_ConstantMax(benchmark::State& state) {
  rasterVaryingArm(state, BlurArm::ConstantMax);
}
BENCHMARK(BM_Draw_VaryingBlur_ConstantMax)
    ->Arg(24)
    ->Unit(benchmark::kMillisecond);

// ---- instances(): the flyweight repeat layer at scale --------------------

namespace {

std::pair<std::shared_ptr<instancing::Atlas>, std::shared_ptr<instancing::Pool>>
makeInstanceScene(size_t count) {
  using namespace instancing;
  auto atlas = std::make_shared<Atlas>();
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
  host.composer.render(box().child(instances(atlas, pool, mode)));
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
      box().child(custom([particles](SkCanvas& c, const PaintContext&) {
                    particles->draw(c);
                  })
                      .inset(0)
                      .cache(Cache::None)));
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
      box().child(custom([particles](SkCanvas& c, const PaintContext&) {
                    SkPaint p;
                    p.setAntiAlias(true);
                    p.setColor(0xff7ee8ff);
                    particles->registry.view<const Particle::Pos>().each(
                        [&](const Particle::Pos& pos) {
                          c.drawCircle(pos.x, pos.y, 3.5f, p);
                        });
                  })
                      .inset(0)
                      .cache(Cache::None)));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    particles->step(1.0f / 120.0f);
    host.draw();
  }
  reportPerParticle(state, count);
}
BENCHMARK(BM_Particles_DrawCircleLoop)->Arg(10000);

// ---- Image regions: a tile map three ways ---------------------------------

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
  bool operator==(const ChunkProps&) const = default;
};

Element benchChunk(const ChunkProps& p) {
  constexpr float kTile = 16.0f;
  auto tiles = box().width(10 * kTile).height(10 * kTile);
  for (int i = 0; i < (int)p.ids.size(); ++i) {
    const int row = i / 10;
    tiles.child(image(benchAtlas())
                    .region(SkRect::MakeXYWH((float)(p.ids[(size_t)i] % 4) * 16,
                                             0, 16, 16))
                    .absolute()
                    .inset((float)(i % 10) * kTile, (float)row * kTile, 0, 0)
                    .width(kTile)
                    .height(kTile));
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
    auto grid = box().row().wrapLines().width(6 * 160.0f);
    for (int c = 0; c < 24; ++c)
      grid.child(
          memo(chunks[(size_t)c], benchChunk).key("c" + std::to_string(c)));
    return box().child(grid);
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

  host.composer.render(box().child(box()
                                       .width(960)
                                       .height(640)
                                       .fill(Fill::shader(field))
                                       .cache(Cache::None)));
  for ([[maybe_unused]] auto iteration : state) host.draw();
}
BENCHMARK(BM_Draw_TileGrid_SkSLFill);

#ifdef COMPOSE_BENCH_GRAPHITE
// ---- The same arms against a Graphite Metal surface ----
// Cache tiers trade re-recording against re-rasterizing, and which side wins
// depends on the target. These arms repeat the raster measurements above
// with a GPU surface underneath so the two can be compared directly.

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

/** The design claim: instanced masses are a GPU play. Same 10k pool,
 *  Live mode, Graphite target, per-frame submit. */
static void BM_Draw_Instances_Live_Graphite(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  GraphiteTarget target(state, 800, 2400);
  if (!target.ok()) return;
  Host host;
  auto [atlas, pool] = makeInstanceScene(count);
  host.composer.render(
      box().child(instances(atlas, pool, instancing::Mode::Live)));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submit();
  }
  state.counters["instances"] = (double)count;
}
BENCHMARK(BM_Draw_Instances_Live_Graphite)->Arg(10000);

namespace {

void bloomGraphiteArm(benchmark::State& state, Cache mode) {
  GraphiteTarget target(state, 900, 300);
  if (!target.ok()) return;
  Host host(900, 300);
  host.composer.render(bloomBlock(mode));
  host.composer.draw(target.canvas());
  target.submit();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submit();
  }
}

}  // namespace

static void BM_Draw_Bloom_PictureReplay_Graphite(benchmark::State& state) {
  bloomGraphiteArm(state, Cache::Picture);
}
BENCHMARK(BM_Draw_Bloom_PictureReplay_Graphite);

static void BM_Draw_Bloom_TextureBaked_Graphite(benchmark::State& state) {
  bloomGraphiteArm(state, Cache::Texture);
}
BENCHMARK(BM_Draw_Bloom_TextureBaked_Graphite);

// ---- The varying-blur arms on the GPU, where these shaders belong --------
// The raster set above is the portable measurement; this is the
// representative one, because a runtime-effect kernel in production runs as
// a fragment shader. Same fixture and same parameter map, on a larger panel
// so that per-frame submit overhead does not dominate what is being timed.
// Every frame is SYNCED: these arms compare shader cost.

namespace {

void graphiteVaryingArm(benchmark::State& state, BlurArm arm) {
  const float sigma = (float)state.range(0);
  GraphiteTarget target(state, kVaryPanelGpu, kVaryPanelGpu);
  if (!target.ok()) return;
  Host host(kVaryPanelGpu, kVaryPanelGpu);
  host.composer.render(
      varyingPanel(kVaryPanelGpu, blurEffect(arm, sigma)).cache(Cache::None));
  host.composer.draw(target.canvas());  // warm the pipeline compile
  target.submitSynced();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.draw(target.canvas());
    target.submitSynced();
  }
  state.counters["sigma"] = sigma;
}

}  // namespace

static void BM_Draw_VaryingBlur_Pyramid_Graphite(benchmark::State& state) {
  graphiteVaryingArm(state, BlurArm::Pyramid);
}
BENCHMARK(BM_Draw_VaryingBlur_Pyramid_Graphite)
    ->Arg(6)
    ->Arg(24)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_VaryingBlur_Naive_Graphite(benchmark::State& state) {
  graphiteVaryingArm(state, BlurArm::Naive);
}
BENCHMARK(BM_Draw_VaryingBlur_Naive_Graphite)
    ->Arg(6)
    ->Arg(24)
    ->Unit(benchmark::kMillisecond);

static void BM_Draw_VaryingBlur_ConstantMax_Graphite(benchmark::State& state) {
  graphiteVaryingArm(state, BlurArm::ConstantMax);
}
BENCHMARK(BM_Draw_VaryingBlur_ConstantMax_Graphite)
    ->Arg(24)
    ->Unit(benchmark::kMillisecond);

/** The same particle frame against a Graphite Metal target: drawAtlas
 *  becomes an instanced GPU batch; the CPU cost is building RSXforms. */
static void BM_Particles_EnttAtlasLeaf_Graphite(benchmark::State& state) {
  const size_t count = (size_t)state.range(0);
  GraphiteTarget target(state, 800, 800);
  if (!target.ok()) return;
  auto particles = std::make_shared<Particle>(count);
  Host host(800, 800);
  host.composer.render(
      box().child(custom([particles](SkCanvas& c, const PaintContext&) {
                    particles->draw(c);
                  })
                      .inset(0)
                      .cache(Cache::None)));
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

#endif  // COMPOSE_BENCH_GRAPHITE
