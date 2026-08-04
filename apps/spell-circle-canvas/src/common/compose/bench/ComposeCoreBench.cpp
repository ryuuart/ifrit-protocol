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
#include <include/core/SkBBHFactory.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <sigilweave/Paragraph.h>

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

// ---- ROADMAP §Argument 3 / §10g(4): what the BINARY volatility
// ---- declaration costs a node whose bound property barely moves.
//
// A panel of `count` stroked, shaped cells — each records its own picture —
// plus ONE accent cell in the same row whose fill is either bound to an
// external Output or a plain value. The accent's ancestors (the row, the
// frame, the root) are what a bound fill poisons: `computeVolatile` walks
// the volatility UP, so the row and the root drop their pictures and lose
// texture promotion for as long as the binding exists, whether or not it
// has moved this frame. Everything else about the three arms is identical.

enum class AccentFill { Bound, Plain };

Element slowThemedPanel(int count, AccentFill mode,
                        const choreograph::Output<Fill> *bound,
                        SkColor4f plain) {
  auto row = box().key("row").row().wrapLines().gap(2);
  for (int id = 0; id < count; ++id)
    row.child(box()
                  .key("c" + std::to_string(id))
                  .width(26)
                  .height(26)
                  .shape(shapes::star(5 + id % 3, 0.45f, 0.08f))
                  .fill(cellFill(id, -1, 0))
                  .stroke(brush::solid(
                      1.5f, Fill::color({0.95f, 0.86f, 0.55f, 1.0f}))));
  Element accent = box()
                       .key("accent")
                       .width(26)
                       .height(26)
                       .shape(shapes::star(7, 0.45f, 0.08f))
                       .stroke(brush::solid(
                           1.5f, Fill::color({0.10f, 0.10f, 0.12f, 1.0f})));
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
  void add(const Composer::Stats &s) {
    recorded += (double)s.picturesRecorded;
    baked += (double)s.texturesBaked;
    painted += (double)s.nodesPainted;
    live += (double)s.picturesLive;
    tex += (double)s.texturesLive;
    frames += 1;
  }
  void report(benchmark::State &state) const {
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

/** ROADMAP §33 residue (resolveSpans): the span walk is re-run 3-4x per
 *  paint with nothing held between frames — "fine at the corpus's pass
 *  counts", where the corpus runs 1-3 passes per boundary. This arm is the
 *  measurement that claim was missing: a 16-node grid whose every node
 *  carries `passes` marching `spans::wrap` passes (disjoint windows off one
 *  Output, the marching-ants idiom), so every endpoint resolves and every
 *  boundary re-walks on every frame. 1-2 is corpus-representative; 8-16 is
 *  the spans-heavy stress the fix-shape (an Instance-side cache keyed like
 *  outlineCache) would exist for. */
Element spanStrokeGrid(int passCount, choreograph::Output<float> &phase) {
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

static void BM_Draw_StrokeSpans_Live(benchmark::State &state) {
  const int passes = (int)state.range(0);
  choreograph::Output<float> phase{0.0f};
  CoreHost host(640, 640);
  host.composer.render(spanStrokeGrid(passes, phase));
  host.draw();
  int tick = 0;
  for ([[maybe_unused]] auto iteration : state) {
    phase = (float)(++tick % 100) / 100.0f;
    host.draw();
  }
  state.counters["passes"] = (double)passes;
  reportNodes(state, 16);
}
BENCHMARK(BM_Draw_StrokeSpans_Live)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->Arg(16)
    ->Unit(benchmark::kMicrosecond);

// ---- ROADMAP §10g "scoped, not built" (1): env read tracking ------------
//
// A memo captures the ambient environment stack at construction and
// compares it BEFORE its props, so a theme change misses every memo below
// the provider — including memos that never read the environment at all.
// The scoped-but-unbuilt read flag would let exactly those memos keep
// hitting. These two arms bracket what it would save: `_ThemeHeld` is the
// all-hits steady state, `_ThemeChange` re-describes every memo each
// iteration and then prunes every one of them (patched stays 0 — the
// describe's result compares equal). The delta per iteration is the entire
// prize the read flag is competing for.

struct BenchPalette {
  SkColor4f surface{0.20f, 0.40f, 0.60f, 1.0f};
  bool operator==(const BenchPalette &) const = default;
};

struct MemoCellProps {
  int id = 0;
  bool operator==(const MemoCellProps &) const = default;
};

Element memoGridUnder(int count, const BenchPalette &palette) {
  env::Provide<BenchPalette> theme(palette);
  auto root = box().row().wrapLines().gap(1);
  for (int id = 0; id < count; ++id)
    root.child(memo(MemoCellProps{id},
                    [](const MemoCellProps &props) {
                      // Never reads env::inherited — the read flag's case.
                      return box().width(19).height(19).fill(
                          cellFill(props.id, -1, 0));
                    })
                   .key("m" + std::to_string(id)));
  return root;
}

static void BM_Env_ThemeChange_MemosNeverRead(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
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
BENCHMARK(BM_Env_ThemeChange_MemosNeverRead)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
    ->Unit(benchmark::kMicrosecond);

static void BM_Env_ThemeHeld_MemosNeverRead(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host;
  host.composer.render(memoGridUnder(count, BenchPalette{}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(memoGridUnder(count, BenchPalette{}));
  state.counters["memoHits"] = (double)host.composer.stats().memoHits;
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}
BENCHMARK(BM_Env_ThemeHeld_MemosNeverRead)
    ->Arg(100)
    ->Arg(500)
    ->Arg(2000)
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

// ---- the windowed/tiled bake question ------------------------------------
// A strip far wider than any texture is baked ONCE as a vector picture and
// then sliced into tile rasters. The candidate mechanism was a "region
// bake" that would hand back per-tile content directly; these three arms
// measure what such a mechanism could possibly save. `FullReplay` is the
// status quo (every tile replays the WHOLE picture behind a clip);
// `PerTilePicture` pre-extracts one picture per tile and replays only that,
// which is the FLOOR any region bake could reach — it has already paid the
// op-selection cost outside the timed loop. The gap between them is the
// entire budget a new mechanism has to spend.

Element marqueeStrip(float acrossPx, float alongPx) {
  auto label = [&](std::string text, float size, SkColor4f color) {
    sigil::weave::TextStyle style;
    style.shaping.fontSize = size;
    style.paint.foreground.setColor(color.toSkColor());
    auto paragraph = std::make_shared<sigil::weave::Paragraph>();
    paragraph->appendText(
        std::u8string_view((const char8_t *)text.c_str(), text.size()), style);
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
  const int sectors = (int)(alongPx / 930.0f); // the marquee's own density
  for (int s = 0; s < sectors; ++s) {
    root.child(box().grow());
    root.child(label("— " + std::to_string(s + 1) + " —", 64.0f,
                     {0.62f, 0.69f, 0.79f, 1.0f}));
    root.child(label("nothing here tiles and nothing repeats: each sector "
                     "is a different neighborhood of the same element tree, "
                     "numbered as it passes.",
                     44.0f, {0.93f, 0.96f, 1.0f, 1.0f}));
    auto row = box().row().gap(6).alignItems(Align::End).height(170);
    for (int i = 0; i < 44; ++i) {
      const float beat =
          0.5f + 0.35f * std::sin((float)i * 0.29f + (float)s * 1.7f);
      row.child(box().width(6).height(28.0f + 134.0f * beat).corners({3}).fill(
          Fill::color({0.455f, 0.878f, 0.745f, 0.45f + 0.5f * beat})));
    }
    root.child(std::move(row));
  }
  return root;
}

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
  out.art = snapshot(marqueeStrip((float)acrossPx,
                                  (float)(tiles * tileAlongPx)),
                     coreFonts());
  return out;
}

// One tile of the strip, drawn the way the marquee draws it: mirrored
// across the band so the wall's u-mapping restores unmirrored glyphs, then
// stepped to this tile's window.
void drawTileWindow(SkCanvas *canvas, const StripBake &bake, int k) {
  canvas->translate((float)bake.acrossPx, 0);
  canvas->scale(-1, 1);
  canvas->translate(0, -(float)(k * bake.alongPx));
}

sk_sp<SkSurface> tileSurface(const StripBake &bake) {
  return SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(bake.acrossPx, bake.alongPx));
}

/** What the bake itself costs — the number the tiling saving has to be
 *  read against, since both happen once. */
static void BM_Bake_TiledStrip_Snapshot(benchmark::State &state) {
  const int tiles = (int)state.range(0);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(
        snapshot(marqueeStrip(324.0f, (float)(tiles * 4096)), coreFonts()));
  state.counters["tiles"] = (double)tiles;
}
BENCHMARK(BM_Bake_TiledStrip_Snapshot)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

static void BM_Bake_TiledStrip_FullReplay(benchmark::State &state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas *canvas = surface->getCanvas();
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
BENCHMARK(BM_Bake_TiledStrip_FullReplay)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

/** The same picture re-recorded behind an R-tree, so playback into a tile
 *  visits only the ops whose bounds meet that tile. This is the cheapest
 *  mechanism a "windowed bake" could actually be: one extra argument to
 *  `beginRecording`. */
sk_sp<SkPicture> withRTree(const sk_sp<SkPicture> &art) {
  SkRTreeFactory rtree;
  SkPictureRecorder recorder;
  // playback(), not drawPicture(): drawPicture on a recording canvas stores
  // a nested reference, which the R-tree cannot see into.
  art->playback(recorder.beginRecording(art->cullRect(), &rtree));
  return recorder.finishRecordingAsPicture();
}

static void BM_Bake_TiledStrip_RTreeReplay(benchmark::State &state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  const sk_sp<SkPicture> art = withRTree(bake.art);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas *canvas = surface->getCanvas();
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
BENCHMARK(BM_Bake_TiledStrip_RTreeReplay)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

/** What the R-tree recipe COSTS, so it can be told whether it pays: the
 *  re-record plus the tree build, once. */
static void BM_Bake_TiledStrip_RTreeBuild(benchmark::State &state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  for ([[maybe_unused]] auto iteration : state)
    benchmark::DoNotOptimize(withRTree(bake.art));
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_RTreeBuild)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

/** The FLOOR: each tile's ops extracted into their own picture ONCE,
 *  outside the timed loop, so the timed work is only what that tile
 *  actually draws. No region bake can beat this. */
static void BM_Bake_TiledStrip_PerTilePicture(benchmark::State &state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  const sk_sp<SkPicture> art = withRTree(bake.art);
  std::vector<sk_sp<SkPicture>> windows;
  double ops = 0;
  for (int k = 0; k < bake.tiles; ++k) {
    SkPictureRecorder recorder;
    SkCanvas *rec =
        recorder.beginRecording(SkRect::MakeIWH(bake.acrossPx, bake.alongPx));
    drawTileWindow(rec, bake, k);
    art->playback(rec); // R-tree culls against the recorder's cull rect
    windows.push_back(recorder.finishRecordingAsPicture());
    ops += (double)windows.back()->approximateOpCount(true);
  }
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k) {
      SkCanvas *canvas = surface->getCanvas();
      canvas->clear(SK_ColorTRANSPARENT);
      canvas->drawPicture(windows[(size_t)k]);
    }
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["ops"] = ops / (double)bake.tiles;
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_PerTilePicture)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

/** How much of a tile's cost is the raster itself: same tile count, same
 *  surfaces, but nothing replayed. The floor under BOTH arms above. */
static void BM_Bake_TiledStrip_SurfacesOnly(benchmark::State &state) {
  const StripBake bake = bakeStrip((int)state.range(0), 324, 4096);
  sk_sp<SkSurface> surface = tileSurface(bake);
  for ([[maybe_unused]] auto iteration : state) {
    for (int k = 0; k < bake.tiles; ++k)
      surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    benchmark::DoNotOptimize(surface->makeImageSnapshot());
  }
  state.counters["tiles"] = (double)bake.tiles;
}
BENCHMARK(BM_Bake_TiledStrip_SurfacesOnly)
    ->Arg(2)
    ->Arg(10)
    ->Arg(40)
    ->Unit(benchmark::kMillisecond);

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

// ---- §Argument 3: the binary volatility declaration -----------------------
//
// PAIR ONE — the property does not move AT ALL. Same tree, same pixels; the
// only difference is that one arm spells the accent's colour as a binding.
// Whatever separates these two arms is paid by a node that is provably
// holding still, which is the defect stated as a measurement.

static void BM_Draw_StillAccent_Bound(benchmark::State &state) {
  const int count = (int)state.range(0);
  choreograph::Output<Fill> tint{Fill::color(accentColor(0))};
  CoreHost host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Bound, &tint, accentColor(0)));
  for (int warm = 0; warm < 16; ++warm)
    host.draw(); // past kScalarSettleFrames and any promotion warmup
  if (getenv("COMPOSE_BENCH_WHY")) {
    host.composer.setProfiling(true);
    host.draw();
    for (const auto &row : host.composer.profile())
      if (row.depth <= 3)
        printf("  [why] %-28s self=%7.3f ms cache=%d promotion=%d\n",
               row.label.c_str(), row.selfMs, (int)row.cacheState,
               (int)row.promotion);
    host.composer.setProfiling(false);
  }
  CacheTally tally;
  for ([[maybe_unused]] auto iteration : state) {
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_StillAccent_Bound)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_StillAccent_Plain(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Plain, nullptr, accentColor(0)));
  for (int warm = 0; warm < 16; ++warm)
    host.draw();
  if (getenv("COMPOSE_BENCH_WHY")) {
    host.composer.setProfiling(true);
    host.draw();
    for (const auto &row : host.composer.profile())
      if (row.depth <= 3)
        printf("  [why] %-28s self=%7.3f ms cache=%d promotion=%d\n",
               row.label.c_str(), row.selfMs, (int)row.cacheState,
               (int)row.promotion);
    host.composer.setProfiling(false);
  }
  CacheTally tally;
  for ([[maybe_unused]] auto iteration : state) {
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_StillAccent_Plain)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Unit(benchmark::kMicrosecond);

// PAIR TWO — the same colour actually moves, once every 180 frames (three
// seconds at 60 Hz, the entry's own example). Each arm does the minimum
// work its spelling requires: the bound arm assigns the Output and never
// re-describes; the plain arm re-describes only on the frame it changes,
// and prunes everything but the one node that moved.

constexpr int kSlowPeriod = 180;

static void BM_Draw_SlowAccent_Bound(benchmark::State &state) {
  const int count = (int)state.range(0);
  choreograph::Output<Fill> tint{Fill::color(accentColor(0))};
  CoreHost host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Bound, &tint, accentColor(0)));
  for (int warm = 0; warm < 16; ++warm)
    host.draw();
  CacheTally tally;
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    if (++frame % kSlowPeriod == 0)
      tint = Fill::color(accentColor(frame / kSlowPeriod));
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_SlowAccent_Bound)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Unit(benchmark::kMicrosecond);

static void BM_Draw_SlowAccent_Plain(benchmark::State &state) {
  const int count = (int)state.range(0);
  CoreHost host(900, 900);
  host.composer.render(
      slowThemedPanel(count, AccentFill::Plain, nullptr, accentColor(0)));
  for (int warm = 0; warm < 16; ++warm)
    host.draw();
  CacheTally tally;
  int frame = 0;
  for ([[maybe_unused]] auto iteration : state) {
    if (++frame % kSlowPeriod == 0)
      host.composer.render(slowThemedPanel(
          count, AccentFill::Plain, nullptr, accentColor(frame / kSlowPeriod)));
    host.draw();
    tally.add(host.composer.stats());
  }
  tally.report(state);
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_SlowAccent_Plain)
    ->Arg(32)
    ->Arg(128)
    ->Arg(512)
    ->Unit(benchmark::kMicrosecond);
