// SigilComposePaint benchmarks: what a pattern tile costs to bake and to
// paint as a repeating fill, what an SDF material costs per frame against
// the same silhouette as a path, and what a layer-style shadow adds to a
// card. Each arm walks a size ladder so the answer is a curve.

#include <include/core/SkCanvas.h>
#include <sigilcompose/Compose.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Through.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/sdf/Sdf.h>
#include <sigilmaterial/skia/Color.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;

namespace material = sigil::material;
using sigil::compose::bench::Host;

// ---- patterns: the bake, then the fill ------------------------------------

namespace {

/** The tile ladder: cell pitches an octave apart, so the bake's cost per
 *  tile and the fill's cost per screen can both be read against pitch. */
void pitchLadder(::benchmark::Benchmark* b) {
  b->Arg(4)->Arg(16)->Arg(64)->Unit(benchmark::kMicrosecond);
}

}  // namespace

/** Baking one halftone tile into a Material. The bake happens once per
 *  distinct Pattern; a fill that re-mints its pattern per describe pays
 *  this every frame, which is what the number is for. */
static void BM_Bake_Pattern_Halftone(benchmark::State& state) {
  const float pitch = (float)state.range(0);
  for ([[maybe_unused]] auto iteration : state) {
    material::Material material =
        Pattern(material::pattern::halftone(pitch, pitch * 0.35f,
                                            {0.1f, 0.1f, 0.12f, 1}))
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
  for ([[maybe_unused]] auto iteration : state) {
    material::Material material =
        Pattern(material::pattern::speckle(
                    128.0f, count, 0.5f, 1.5f,
                    {{0.9f, 0.9f, 0.85f, 1}, {0.6f, 0.6f, 0.55f, 1}}))
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
  material::Material halftone =
      Pattern(material::pattern::halftone(pitch, pitch * 0.35f,
                                          {0.1f, 0.1f, 0.12f, 1}))
          .material();
  host.composer.render(
      box().children({box().inset(0).fill(halftone).cache(Cache::None)}));
  host.draw();
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["pitch"] = pitch;
}
BENCHMARK(BM_Draw_Pattern_Fill_Live)->Apply(pitchLadder);

/** The same fill under a slowly rotating pattern: every frame remaps the
 *  tile without rebaking it, which is the property `Pattern::rotate`
 *  promises and this arm prices. */
static void BM_Draw_Pattern_Rotate_Live(benchmark::State& state) {
  Host host(800, 800);
  Pattern stripes =
      Pattern(material::pattern::stripes(6, 6, {0.2f, 0.2f, 0.25f, 1}));
  float angle = 0;
  auto describe = [&] {
    return box().children({box()
                               .inset(0)
                               .fill(stripes.rotate(angle).material())
                               .cache(Cache::None)});
  };
  host.composer.render(describe());
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
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
  auto root = box().row().flexWrap().gap(6).padding(6);
  material::sdf::Style style;
  style.fill = {0.18f, 0.22f, 0.32f, 1};
  style.borderWidth = 2;
  style.borderColor = {0.9f, 0.8f, 0.5f, 1};
  style.glowRadius = 6;
  style.glowColor = {0.4f, 0.7f, 1.0f, 0.6f};
  const float padded = material::sdf::minimumBoxFor(style, side);
  for (int id = 0; id < count; ++id) {
    Element card = box().key("card" + std::to_string(id));
    if (paint == CardPaint::Sdf) {
      card.width(padded).height(padded).fill(
          material::sdf::material(material::sdf::roundBox(side * 0.2f), style));
    } else {
      card.width(side)
          .height(side)
          .borderRadius({side * 0.2f})
          .fill(Fill::color(material::skia::toSkColor(style.fill)))
          .stroke(stroke(
              style.borderWidth,
              Fill::color(material::skia::toSkColor(style.borderColor))));
    }
    root.children({std::move(card)});
  }
  return root;
}

void cardArm(benchmark::State& state, CardPaint paint) {
  const int count = (int)state.range(0);
  const float side = (float)state.range(1);
  Host host(1024, 1024);
  host.composer.render(cardGrid(count, side, paint).cache(Cache::None));
  host.draw();  // warm the shader compile
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["cards"] = (double)count;
  state.counters["side"] = side;
  state.SetItemsProcessed(state.iterations() * count);
}

void cardLadder(::benchmark::Benchmark* b) {
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
  auto root = box().row().flexWrap().gap(10).padding(10);
  for (int id = 0; id < count; ++id)
    root.children({box()
                       .key("s" + std::to_string(id))
                       .width(120)
                       .height(72)
                       .borderRadius({8})
                       .fill(Fill::color(material::hexColor(0x2a3140)))
                       .background(shadow(
                           sigil::material::Color{0, 0, 0, 0.55f}, {0, 4}, 10))
                       .cache(mode)});
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
  for ([[maybe_unused]] auto iteration : state) host.draw();
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
  for ([[maybe_unused]] auto iteration : state) host.draw();
  state.counters["cards"] = (double)count;
  state.counters["picturesLive"] = (double)host.composer.stats().picturesLive;
}
BENCHMARK(BM_Draw_DropShadow_Cached)
    ->Arg(16)
    ->Arg(64)
    ->Unit(benchmark::kMicrosecond);

/** SIX LONG RIBBONS ON A LARGE PAGE, REDRAWN EVERY FRAME: three scrolling
 *  traces, each a soft wide pass under a sharp one, as a console page
 *  paints them. Every frame's ribbons are new geometry, so this is the
 *  cost of filling their coverage once a frame. */
static void BM_Draw_Ribbons_Scrolling(benchmark::State& state) {
  constexpr float kPageWidth = 1600, kPageHeight = 1000;
  Host host((int)kPageWidth, (int)kPageHeight);
  const auto trace = [](int channel, float scroll) {
    std::vector<glm::vec2> points;
    for (int index = 0; index <= 220; ++index) {
      const float across = (float)index / 220;
      const float turn = 6.2831853f * (across + scroll);
      points.push_back(
          {640 + across * 870, 250 + channel * 186.0f +
                                   50 * std::sin(turn * (3 + channel)) +
                                   12 * std::sin(turn * 11 + 1.3f)});
    }
    return points;
  };
  int frame = 0;
  const auto page = [&] {
    std::vector<Element> ribbons;
    const float scroll = (float)frame / 240;
    for (int channel = 0; channel < 3; ++channel)
      for (const float width : {9.0f, 2.4f}) {
        const auto points = trace(channel, scroll);
        ribbons.push_back(
            box()
                .absolute()
                .left(0)
                .top(0)
                .width(kPageWidth)
                .height(kPageHeight)
                .shape(heldPath(sigil::geometry::path::curveThrough(points)))
                .stroke(brush::ribbon(
                    sigil::geometry::path::Profile(width),
                    material::from(material::Color{0.3f, 0.8f, 0.8f, 1})
                        .surface({.unlit = true}))));
      }
    return box()
        .width(kPageWidth)
        .height(kPageHeight)
        .children(std::move(ribbons));
  };
  host.composer.render(page());
  host.draw();
  for ([[maybe_unused]] auto iteration : state) {
    ++frame;
    host.composer.render(page());
    host.draw();
  }
  state.counters["ribbons"] = 6;
}
BENCHMARK(BM_Draw_Ribbons_Scrolling)->Unit(benchmark::kMillisecond);
