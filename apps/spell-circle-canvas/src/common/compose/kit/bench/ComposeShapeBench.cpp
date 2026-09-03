// The kit tier's silhouette benchmarks: what a comparable silhouette buys
// the reconciler over a raw callable, and what shaped, rotated and blended
// leaves cost the hit test and the painter.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <cstdint>
#include <string>
#include <utility>

#include "BenchSupport.h"

using namespace sigil::compose;

namespace geometry = sigil::geometry;
using sigil::compose::bench::cellFill;
using sigil::compose::bench::Host;
using sigil::compose::bench::nodeLadder;
using sigil::compose::bench::reportNodes;

namespace {

enum class ShapeIdentity { Comparable, RawCallable };

/** A grid of shaped leaves whose silhouette is spelled either as a
 *  comparable `geometry::shapes::` value or as a bare callable, which the
 * reconciler can never prove equal to the one it replaces. */
Element shapedGrid(int count, ShapeIdentity identity) {
  auto root = box().row().wrapLines();
  for (int id = 0; id < count; ++id) {
    Element leaf = box()
                       .key("s" + std::to_string(id))
                       .width(24)
                       .height(24)
                       .fill(cellFill(id));
    if (identity == ShapeIdentity::Comparable) {
      leaf.shape(geometry::shapes::star(5 + id % 3, 0.45f, 0.08f));
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

void reconcileShapesArm(benchmark::State& state, ShapeIdentity identity) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(shapedGrid(count, identity));
  host.draw();
  for ([[maybe_unused]] auto iteration : state)
    host.composer.render(shapedGrid(count, identity));
  state.counters["patched"] = (double)host.composer.stats().patchedNodes;
  reportNodes(state, count);
}

}  // namespace

static void BM_Reconcile_Shapes_Comparable(benchmark::State& state) {
  reconcileShapesArm(state, ShapeIdentity::Comparable);
}
BENCHMARK(BM_Reconcile_Shapes_Comparable)->Apply(nodeLadder);

static void BM_Reconcile_Shapes_RawCallable(benchmark::State& state) {
  reconcileShapesArm(state, ShapeIdentity::RawCallable);
}
BENCHMARK(BM_Reconcile_Shapes_RawCallable)->Apply(nodeLadder);

/** hitTest through a 3-level tree with shaped and rotated nodes. */
static void BM_HitTest_ShapedTree(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(900, 640);
  auto scatter = layout(layouts::Scatter{.seed = 3}).inset(0);
  for (int i = 0; i < count; ++i)
    scatter.child(box()
                      .key("blob" + std::to_string(i))
                      .width(60)
                      .height(60)
                      .shape(geometry::shapes::blob((uint32_t)i, 0.3f, 7))
                      .rotate((float)i * 7.0f)
                      .fill(Fill::color({0.5f, 0.3f, 0.4f, 1})));
  host.composer.render(box().child(scatter));
  host.draw();
  int step = 0;
  for ([[maybe_unused]] auto iteration : state) {
    const SkPoint pt{(float)(step * 37 % 900), (float)(step * 53 % 640)};
    benchmark::DoNotOptimize(host.composer.hitTest(pt));
    ++step;
  }
  reportNodes(state, count);
}
BENCHMARK(BM_HitTest_ShapedTree)
    ->Arg(50)
    ->Arg(200)
    ->Unit(benchmark::kNanosecond);

/** Plus-blended blob leaves. A leaf whose blend mode can be pushed onto
 *  its own paint needs no isolation layer; the alternative is a
 *  device-sized saveLayer per leaf, so this arm is where that path's
 *  absence shows up. */
static void BM_Draw_BlendField_Blobs(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(900, 640);
  auto scatter = layout(layouts::Scatter{.seed = 9, .jitter = 0.8f}).inset(0);
  for (int i = 0; i < count; ++i)
    scatter.child(box()
                      .width(70)
                      .height(60)
                      .shape(geometry::shapes::blob((uint32_t)(i + 1), 0.3f, 6))
                      .fill(Fill::color({0.4f, 0.2f, 0.4f, 0.5f}))
                      .blend(SkBlendMode::kPlus));
  host.composer.render(box().child(scatter));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  reportNodes(state, count);
}
BENCHMARK(BM_Draw_BlendField_Blobs)
    ->Arg(100)
    ->Arg(400)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
