// WHAT AN EFFECT COSTS AND WHERE IT IS PAID: a blurred headline replayed
// against the same headline baked, a halo that never changes applied
// inside a held bake and over it, and a blur whose sigma varies across
// the node priced against writing the same picture by hand. Each set is
// run again on a Graphite Metal surface, which is where a runtime-effect
// kernel runs in production.

#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Compose.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "BenchSupport.h"

using namespace sigil::compose;
using sigil::compose::bench::Host;

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
      .effect(sigil::material::skia::Effect::filter(
          SkImageFilters::Blur(12, 12, nullptr)))
      .children({text(u8"BLOOM PIPELINE", style)});
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

// ---- A STATIC layer effect over a held bake, both ways ---------------------
//
// The shape: a band of marks held as ONE bake in the node's own space and
// blitted through a turn, wearing a halo that never changes. Two places the
// halo can be applied — INSIDE the content raster, where it opens a layer
// over the node's whole band on every bake, or OVER the finished bake, as
// one image draw. The arms price both, on the frames that hold the bake and
// on the frames that remake it.
//
// A declared COVERAGE boundary is what refuses the lifted tier. It changes
// nothing a band of plain boxes paints, so the two arms are the same picture.

namespace {

Element haloedBand(Boundary boundary, const choreograph::Output<float>* turn) {
  const float side = 640.0f, radius = 260.0f;
  Element band = box()
                     .key("band")
                     .absolute()
                     .left(40)
                     .top(40)
                     .width(side)
                     .height(side)
                     .cache(Cache::Texture)
                     .boundary(boundary)
                     .transformOrigin(0.5f, 0.5f)
                     .rotate(sigil::motion::bind(turn).target(0.0f, 360.0f))
                     .effect(sigil::material::skia::Effect::glow(
                         {0.35f, 0.85f, 1.0f, 1.0f}, 6.0f));
  for (int i = 0; i < 24; ++i) {
    const float a = (float)i * (float)(2 * M_PI) / 24.0f;
    band.children({box()
                       .absolute()
                       .left(side * 0.5f + radius * std::cos(a) - 9.0f)
                       .top(side * 0.5f + radius * std::sin(a) - 9.0f)
                       .width(18)
                       .height(18)
                       .fill(Fill::color({1.0f, 0.78f, 0.3f, 1.0f}))});
  }
  return band;
}

/** The bake is taken once and every frame after is a blit through the turn:
 *  what a held bake costs per frame with the effect where the arm puts it. */
void haloHeldArm(benchmark::State& state, Boundary boundary, bool turning) {
  choreograph::Output<float> turn{0.0f};
  Host host(800, 800);
  host.composer.render(box().children({haloedBand(boundary, &turn)}));
  host.draw();
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    if (turning) turn = (t += 0.004f);
    host.draw();
  }
}

/** …and the other half of the bargain: a frame that RE-BAKES. The content is
 *  re-described, so the bake is remade and the arm prices the bake itself. */
void haloRebakeArm(benchmark::State& state, Boundary boundary) {
  choreograph::Output<float> turn{0.0f};
  Host host(800, 800);
  host.composer.render(box().children({haloedBand(boundary, &turn)}));
  host.draw();
  float t = 0;
  for ([[maybe_unused]] auto iteration : state) {
    t += 0.004f;
    turn = t;
    host.composer.render(box()
                             .opacity(1.0f - 0.0001f * t)
                             .children({haloedBand(boundary, &turn)}));
    host.draw();
  }
}

}  // namespace

/** The halo inside the content raster: the layer is paid on every bake. */
static void BM_Draw_StaticGlow_Held_InLayer(benchmark::State& state) {
  haloHeldArm(state, Boundary::Coverage, true);
}
BENCHMARK(BM_Draw_StaticGlow_Held_InLayer);

/** The halo over the finished bake, under a turn. */
static void BM_Draw_StaticGlow_Held_OverBake(benchmark::State& state) {
  haloHeldArm(state, Boundary::Auto, true);
}
BENCHMARK(BM_Draw_StaticGlow_Held_OverBake);

/** …and both again with the node STANDING STILL: a held bake blitted
 *  without a resample, which is the cheapest frame either arm has. */
static void BM_Draw_StaticGlow_Still_InLayer(benchmark::State& state) {
  haloHeldArm(state, Boundary::Coverage, false);
}
BENCHMARK(BM_Draw_StaticGlow_Still_InLayer);

static void BM_Draw_StaticGlow_Still_OverBake(benchmark::State& state) {
  haloHeldArm(state, Boundary::Auto, false);
}
BENCHMARK(BM_Draw_StaticGlow_Still_OverBake);

/** The bake remade with the filter's own layer inside the content raster. */
static void BM_Draw_StaticGlow_Rebake_InLayer(benchmark::State& state) {
  haloRebakeArm(state, Boundary::Coverage);
}
BENCHMARK(BM_Draw_StaticGlow_Rebake_InLayer);

/** The bake remade with the filter lifted off the content raster: the
 *  content into one surface, the effect over it into the next. */
static void BM_Draw_StaticGlow_Rebake_OverBake(benchmark::State& state) {
  haloRebakeArm(state, Boundary::Auto);
}
BENCHMARK(BM_Draw_StaticGlow_Rebake_OverBake);

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
sigil::material::skia::Paint stripeTarget() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float band = mod(floor(p.x / 8.0), 2.0);"
                 "  return band < 1.0 ? half4(1) : half4(0, 0, 0, 1);"
                 "}"));
    return effect;
  }();
  return sigil::material::skia::Paint::sksl(fx);
}

/** The parameter: 0 at the node's left edge, 1 at its right. */
sigil::material::skia::Paint sigmaRamp() {
  return sigil::material::skia::Paint::linearUnit(
      {0, 0}, {1, 0}, {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
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

Element varyingPanel(int side, sigil::material::skia::Effect e) {
  return box()
      .width((float)side)
      .height((float)side)
      .fill(stripeTarget())
      .effect(std::move(e));
}

enum class BlurArm { Pyramid, Naive, ConstantMax };

sigil::material::skia::Effect blurEffect(BlurArm arm, float sigma) {
  switch (arm) {
    case BlurArm::Pyramid:
      return sigil::material::skia::Effect::blur(sigmaRamp(), sigma);
    case BlurArm::Naive: {
      // A Gaussian is negligible past three standard deviations, so R = 3σ
      // is the radius the worst pixel in the node needs — and every pixel
      // pays it.
      const int radius = (int)std::lround(3.0f * sigma);
      return sigil::material::skia::Effect::shader(naiveVaryingBlur(radius),
                                                   {{"uMaxSigma", sigma}})
          .slot("param", sigmaRamp());
    }
    case BlurArm::ConstantMax:
      return sigil::material::skia::Effect::filter(
          SkImageFilters::Blur(sigma, sigma, nullptr));
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

#ifdef SIGIL_BENCH_GPU
// ---- The same effects against a Graphite Metal surface ----
// Where a filter is paid depends on the target, so each raster arm above
// is repeated with a GPU surface underneath.

using sigil::compose::bench::GraphiteTarget;

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

#endif  // SIGIL_BENCH_GPU
