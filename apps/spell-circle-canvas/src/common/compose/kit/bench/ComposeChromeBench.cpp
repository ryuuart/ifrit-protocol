// What a bevelled panel costs to paint: the four shapes a toolkit's edge
// takes, and a sheet of them at the density a reconstructed desktop has.

#include <sigilcompose/kit/Chrome.h>
#include <sigilcore/reconcile/Environment.h>

#include <string>

#include "BenchSupport.h"

using namespace sigil::compose;

namespace {

constexpr SkColor4f kFace{0.60f, 0.60f, 0.58f, 1};
constexpr SkColor4f kLit{0.86f, 0.86f, 0.84f, 1};
constexpr SkColor4f kShade{0.30f, 0.30f, 0.29f, 1};

/** The four edges an era asks for, in the order the arms name them. */
kit::Bevel era(int which) {
  switch (which) {
    case 0:
      return kit::bevels::motif(kLit, kShade, 2);
    case 1:
      return kit::bevels::motifEtched(kLit, kShade, 2);
    case 2:
      return kit::bevels::flash(kFace, 6);
    default:
      return kit::bevels::plate(kLit, kShade);
  }
}

Element panelOf(const kit::Bevel& b, int index) {
  return box()
      .width(Dimension(180))
      .height(Dimension(40))
      .fill(kFace)
      .overlay(b)
      .key("panel" + std::to_string(index));
}

}  // namespace

/** ONE PANEL, one edge, per arm: the mitred ring is two fills, the square
 *  pair four strokes and the moulded pair two blurs, and a look that
 *  dresses a whole window pays whichever it named on every panel in it. */
static void BM_Chrome_Bevel(benchmark::State& state) {
  const kit::Bevel b = era((int)state.range(0));
  bench::Host host(240, 80);
  host.composer.render(box().padding(20).child(panelOf(b, 0)));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.render(box().padding(20).child(panelOf(b, 0)));
    host.draw();
  }
}
BENCHMARK(BM_Chrome_Bevel)
    ->Arg(0)
    ->Arg(1)
    ->Arg(2)
    ->Arg(3)
    ->Unit(benchmark::kMicrosecond);

/** A DESKTOP OF THEM: sixty bevelled panels under one theme, which is the
 *  density a reconstructed interface actually has — a window's worth of
 *  buttons, wells and separators, every one reading the same tokens. */
static void BM_Chrome_Panels(benchmark::State& state) {
  const int count = (int)state.range(0);
  bench::Host host(240, 60 * 44 + 40);
  const auto tree = [&] {
    const sigil::core::environment::Provide<kit::Bevel> bound(
        kit::bevels::motif(kLit, kShade));
    Element page = box().padding(20).gap(4);
    for (int i = 0; i < count; ++i) page.child(panelOf(kit::ambientBevel(), i));
    return page;
  };
  host.composer.render(tree());
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.render(tree());
    host.draw();
  }
  bench::reportNodes(state, count);
}
BENCHMARK(BM_Chrome_Panels)->Arg(60)->Unit(benchmark::kMicrosecond);

/** The stipple: one colour through a repeating mask over a whole panel,
 *  which is what every insensitive control in a window costs. */
static void BM_Chrome_Stipple(benchmark::State& state) {
  bench::Host host(240, 80);
  const auto tree = [] {
    return box().padding(20).child(box()
                                       .width(Dimension(180))
                                       .height(Dimension(40))
                                       .fill(kFace)
                                       .overlay(styles::stipple(kShade))
                                       .key("stippled"));
  };
  host.composer.render(tree());
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    host.composer.render(tree());
    host.draw();
  }
}
BENCHMARK(BM_Chrome_Stipple)->Unit(benchmark::kMicrosecond);
