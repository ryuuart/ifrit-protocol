// SigilComposePaint benchmarks: what a pattern tile costs to bake and to
// paint as a repeating fill, what an SDF material costs per frame against
// the same silhouette as a path, and what a layer-style shadow adds to a
// card. Each arm walks a size ladder so the answer is a curve.

#include <include/core/SkCanvas.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Sdf.h>

#include <string>
#include <utility>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

// ---- patterns: the bake, then the fill ------------------------------------

namespace {

/** The tile ladder: cell pitches an octave apart, so the bake's cost per
 *  tile and the fill's cost per screen can both be read against pitch. */
void pitchLadder(benchmark::internal::Benchmark* b) {
  b->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);
}

}  // namespace

/** Baking one halftone tile into a Material. The bake happens once per
 *  distinct Pattern; a fill that re-mints its pattern per describe pays
 *  this every frame, which is what the number is for. */
static void BM_Bake_Pattern_Halftone(benchmark::State& state) {
  const float pitch = (float)state.range(0);
  for (auto _ : state) {
    Material material =
        patterns::halftone(pitch, pitch * 0.35f, {0.1f, 0.1f, 0.12f, 1})
            .material();
    benchmark::DoNotOptimize(material);
  }
  state.counters["pitch"] = pitch;
}
BENCHMARK(BM_Bake_Pattern_Halftone)->Apply(pitchLadder);

/** Baking a seeded speckle tile: the generator draws `count` marks, so the
 *  bake scales with the mark count rather than the pitch. */
static void BM_Bake_Pattern_Speckle(benchmark::State& state) {
  const int count = (int)state.range(0);
  for (auto _ : state) {
    Material material =
        patterns::speckle(128.0f, count, 0.5f, 1.5f,
                          {{0.9f, 0.9f, 0.85f, 1}, {0.6f, 0.6f, 0.55f, 1}})
            .material();
    benchmark::DoNotOptimize(material);
  }
  state.counters["marks"] = (double)count;
  state.SetItemsProcessed(state.iterations() * count);
}
BENCHMARK(BM_Bake_Pattern_Speckle)
    ->RangeMultiplier(4)
    ->Range(64, 4096)
    ->Unit(benchmark::kMicrosecond);

/** A full-panel fill with a baked halftone, repainted live: the per-frame
 *  price of a repeating shader over 800x800 px at each pitch. A settled
 *  node would cache this; Cache::None keeps the fill in the frame. */
static void BM_Draw_Pattern_Fill_Live(benchmark::State& state) {
  const float pitch = (float)state.range(0);
  Host host(800, 800);
  Material halftone =
      patterns::halftone(pitch, pitch * 0.35f, {0.1f, 0.1f, 0.12f, 1})
          .material();
  host.composer.render(
      box().child(box().inset(0).fill(halftone).cache(Cache::None)));
  host.draw();
  for (auto _ : state) host.draw();
  state.counters["pitch"] = pitch;
}
BENCHMARK(BM_Draw_Pattern_Fill_Live)->Apply(pitchLadder);

/** The same fill under a slowly rotating pattern: every frame remaps the
 *  tile without rebaking it, which is the property `Pattern::rotate`
 *  promises and this arm prices. */
static void BM_Draw_Pattern_Rotate_Live(benchmark::State& state) {
  Host host(800, 800);
  Pattern stripes = patterns::stripes(6, 6, {0.2f, 0.2f, 0.25f, 1});
  float angle = 0;
  auto describe = [&] {
    return box().child(box()
                           .inset(0)
                           .fill(stripes.rotate(angle).material())
                           .cache(Cache::None));
  };
  host.composer.render(describe());
  host.draw();
  for (auto _ : state) {
    angle += 0.5f;
    host.composer.render(describe());
    host.draw();
  }
}
BENCHMARK(BM_Draw_Pattern_Rotate_Live);

// ---- SDF materials against the path they replace ---------------------------

namespace {

/** A grid of `count` cards, each `side` px, painted either as an SDF
 *  material (one shader draw carrying border, glow and shadow) or as the
 *  equivalent rounded-rect path with a stroke. */
enum class CardPaint { Sdf, Path };

Element cardGrid(int count, float side, CardPaint paint) {
  auto root = box().row().wrapLines().gap(6).padding(6);
  sdf::Style style;
  style.fill = {0.18f, 0.22f, 0.32f, 1};
  style.borderWidth = 2;
  style.borderColor = {0.9f, 0.8f, 0.5f, 1};
  style.glowRadius = 6;
  style.glowColor = {0.4f, 0.7f, 1.0f, 0.6f};
  const float padded = sdf::minBoxFor(style, side);
  for (int id = 0; id < count; ++id) {
    Element card = box().key("card" + std::to_string(id));
    if (paint == CardPaint::Sdf) {
      card.width(padded).height(padded).fill(
          sdf::material(sdf::roundBox(side * 0.2f), style));
    } else {
      card.width(side)
          .height(side)
          .corners({side * 0.2f})
          .fill(Fill::color(style.fill))
          .stroke(stroke(style.borderWidth, Fill::color(style.borderColor)));
    }
    root.child(std::move(card));
  }
  return root;
}

void cardArm(benchmark::State& state, CardPaint paint) {
  const int count = (int)state.range(0);
  const float side = (float)state.range(1);
  Host host(1024, 1024);
  host.composer.render(cardGrid(count, side, paint).cache(Cache::None));
  host.draw();  // warm the shader compile
  for (auto _ : state) host.draw();
  state.counters["cards"] = (double)count;
  state.counters["side"] = side;
  state.SetItemsProcessed(state.iterations() * count);
}

void cardLadder(benchmark::internal::Benchmark* b) {
  b->ArgsProduct({{16, 64}, {32, 96}})->Unit(benchmark::kMicrosecond);
}

}  // namespace

static void BM_Draw_Sdf_Cards_Live(benchmark::State& state) {
  cardArm(state, CardPaint::Sdf);
}
BENCHMARK(BM_Draw_Sdf_Cards_Live)->Apply(cardLadder);

static void BM_Draw_Path_Cards_Live(benchmark::State& state) {
  cardArm(state, CardPaint::Path);
}
BENCHMARK(BM_Draw_Path_Cards_Live)->Apply(cardLadder);

// ---- layer styles: a drop shadow on a card, live and cached ---------------

namespace {

Element shadowedCards(int count, Cache mode) {
  auto root = box().row().wrapLines().gap(10).padding(10);
  for (int id = 0; id < count; ++id)
    root.child(box()
                   .key("s" + std::to_string(id))
                   .width(120)
                   .height(72)
                   .corners({8})
                   .fill(Fill::color(hex(0x2a3140)))
                   .background(styles::dropShadow({0, 0, 0, 0.55f}, {0, 4}, 10))
                   .cache(mode));
  return root;
}

}  // namespace

/** The blurred shadow re-rasterized every frame — what a card pays when
 *  something keeps it volatile. */
static void BM_Draw_DropShadow_Live(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(shadowedCards(count, Cache::None));
  host.draw();
  for (auto _ : state) host.draw();
  state.counters["cards"] = (double)count;
}
BENCHMARK(BM_Draw_DropShadow_Live)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

/** The same cards settled: the shadow is in the recording and the frame is
 *  a replay. Read against the live arm for what the cache saves. */
static void BM_Draw_DropShadow_Cached(benchmark::State& state) {
  const int count = (int)state.range(0);
  Host host(1024, 1024);
  host.composer.render(shadowedCards(count, Cache::Auto));
  host.draw();
  for (auto _ : state) host.draw();
  state.counters["cards"] = (double)count;
  state.counters["picturesLive"] = (double)host.composer.stats().picturesLive;
}
BENCHMARK(BM_Draw_DropShadow_Cached)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
