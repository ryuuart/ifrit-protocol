// SigilCompose scaling benchmarks. ComposeBench.cpp keeps the historical
// scene/perf-gate probes; this file is the systematic cold/warm/update/draw
// matrix, mirroring weave_bench's organization and parameterized sizes.

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <benchmark/benchmark.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using namespace sigil::compose;

namespace {

sigil::weave::FontContext &coreFonts() {
  static auto *context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

struct CoreHost {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, coreFonts()};
  sk_sp<SkSurface> surface;

  explicit CoreHost(int width = 1024, int height = 1024) {
    composer.setSize({(float)width, (float)height});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  }

  void draw() { composer.draw(*surface->getCanvas()); }
};

void reportNodes(benchmark::State &state, int count) {
  state.counters["nodes"] = (double)count;
  state.SetItemsProcessed(state.iterations() * (int64_t)count);
}

Fill cellFill(int id, int changed, int phase) {
  if (id == changed && phase != 0)
    return Fill::color({0.95f, 0.35f, 0.18f, 1.0f});
  const float tint = 0.20f + 0.04f * (float)(id % 6);
  return Fill::color({tint, 0.45f, 0.68f, 1.0f});
}

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
    root.child(box()
                   .key("n" + std::to_string(id))
                   .left((float)(id % kColumns) * 20.0f)
                   .top((float)(id / kColumns) * 20.0f)
                   .width(19)
                   .height(19)
                   .fill(cellFill(id, -1, 0)));
  }
  return root;
}

enum class ShapeIdentity { Comparable, RawCallable };

Element shapedGrid(int count, ShapeIdentity identity) {
  auto root = box().row().wrapLines();
  for (int id = 0; id < count; ++id) {
    Element leaf = box()
                       .key("s" + std::to_string(id))
                       .width(24)
                       .height(24)
                       .fill(cellFill(id, -1, 0));
    if (identity == ShapeIdentity::Comparable) {
      leaf.shape(shapes::star(5 + id % 3, 0.45f, 0.08f));
    } else {
      leaf.shape([](SkSize size) {
        SkPathBuilder path;
        path.addOval(SkRect::MakeWH(size.width(), size.height()));
        return path.detach();
      });
    }
    root.child(std::move(leaf));
  }
  return root;
}

enum class MaskKind { Spans, Edge };

Element maskedGrid(int count, MaskKind kind,
                   choreograph::Output<float> *reveal) {
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
  bool operator==(const WaveWidth &) const = default;
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
      SkDebugf("compose_bench group shader failed: %s\n", error.c_str());
    return compiled;
  }();
  return effect;
}

Element groupScene(int count, Cache mode) {
  auto group = box().absolute().inset(12).key("group").cache(mode);
  constexpr int kColumns = 10;
  for (int id = 0; id < count; ++id) {
    group.child(box()
                    .absolute()
                    .left(18.0f + (float)(id % kColumns) * 59.0f)
                    .top(18.0f + (float)(id / kColumns) * 43.0f)
                    .width(64)
                    .height(13)
                    .rotate((float)(id % 7) * 6.0f - 18.0f)
                    .fill(Material::sksl(groupShader())));
  }
  // Keep the parent live so it calls into the group every frame. An Auto
  // parent would cache one picture containing the first-frame traversal and
  // make both arms bypass the group state machine entirely.
  return box().cache(Cache::None).absolute().inset(0).child(std::move(group));
}

} // namespace

// ---- describe / reconcile -------------------------------------------------

static void BM_Mount_Cold(benchmark::State &state) {
  const int count = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    sigil::motion::Ticker ticker;
    Composer composer(ticker, coreFonts());
    composer.setSize({1024, 1024});
    composer.render(flexGrid(count));
    benchmark::DoNotOptimize(composer.dirty());
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Mount_Cold)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Reconcile_Flex_Warm(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(flexGrid(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(flexGrid(count));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Flex_Warm)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Reconcile_Positioned_Warm(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(positionedGrid(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(positionedGrid(count));
  state.counters["yogaNodes"] = (double)host.composer.stats().yogaNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Positioned_Warm)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Reconcile_OneChanged(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(flexGrid(count));
  host.draw();
  int phase = 0;
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(flexGrid(count, count / 2, ++phase & 1));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_OneChanged)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Reconcile_KeyedReorder(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(flexGrid(count));
  host.draw();
  int shift = 0;
  for ([[maybe_unused]] auto iteration : state) {
    shift = shift == 0 ? 1 : 0;
    host.composer.render(flexGrid(count, -1, 0, shift));
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_KeyedReorder)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

// ---- layout ---------------------------------------------------------------

static void BM_Layout_Flex_ViewportToggle(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
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
BENCHMARK(BM_Layout_Flex_ViewportToggle)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Layout_Positioned_ViewportToggle(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
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
BENCHMARK(BM_Layout_Positioned_ViewportToggle)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

// ---- comparable values / queries -----------------------------------------

static void BM_Reconcile_Shapes_Comparable(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(shapedGrid(count, ShapeIdentity::Comparable));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(shapedGrid(count, ShapeIdentity::Comparable));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Shapes_Comparable)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Reconcile_Shapes_RawCallable(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(shapedGrid(count, ShapeIdentity::RawCallable));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(shapedGrid(count, ShapeIdentity::RawCallable));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Reconcile_Shapes_RawCallable)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Query_Bounds(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
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

// ---- paint mechanisms added after the original perf gate -----------------

static void BM_Draw_Mask_Spans_Live(benchmark::State &state) {
  const int count = (int)state.range(0);
  choreograph::Output<float> reveal{0.0f};
  CoreHost host(800, 800);
  host.composer.render(maskedGrid(count, MaskKind::Spans, &reveal));
  host.draw();
  int tick = 0;
  for ([[maybe_unused]] auto iteration : state) {
    reveal = (float)(++tick % 100) / 100.0f;
    host.draw();
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_Mask_Spans_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_Mask_Edge_Live(benchmark::State &state) {
  const int count = (int)state.range(0);
  choreograph::Output<float> reveal{0.0f};
  CoreHost host(800, 800);
  host.composer.render(maskedGrid(count, MaskKind::Edge, &reveal));
  host.draw();
  int tick = 0;
  for ([[maybe_unused]] auto iteration : state) {
    reveal = (float)(++tick % 100) / 100.0f;
    host.draw();
  }
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_Mask_Edge_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(256)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_ProfiledRibbon_Live(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(800, 800);
  host.composer.render(profiledRibbonGrid(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.draw();
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_ProfiledRibbon_Live)
    ->Arg(16)
    ->Arg(64)
    ->Arg(128)
    ->Unit(benchmark::kMicrosecond);

/** The ROADMAP's explicit missing arm: crossing discovery re-flattens every
 * strand and compares every segment pair on every live paint. */
static void BM_Draw_BrushWeave_Live(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(640, 640);
  host.composer.render(weaveScene(count));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.draw();
  state.counters["pairs"] = (double)(count * (count - 1) / 2);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_BrushWeave_Live)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_GroupCache_LivePictures(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(640, 640);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(groupScene(count, Cache::Auto));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.draw();
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_GroupCache_LivePictures)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_GroupCache_Blit(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(640, 640);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(groupScene(count, Cache::Group));
  host.draw(); // establish the subtree-value memo
  host.draw(); // settle and take the one group bake
  if (host.composer.stats().texturesLive == 0) {
    state.SkipWithError("Cache::Group did not reach the blit state");
    return;
  }
  for ([[maybe_unused]] auto iteration : state)
    host.draw();
  state.counters["textures"] = (double)host.composer.stats().texturesLive;
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_GroupCache_Blit)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);
