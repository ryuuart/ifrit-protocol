// motion_poster.cpp — "EMBER GATE", the Material-era motion poster.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/motion_poster.cpp
//
// The flagship thesis: a living typographic poster with ZERO raw custom()
// lambdas. Before the material layer, a scene like this was ~60% hand-rolled
// SkCanvas code (see RpgHudScene's bars/sigil/icons); here every visual is a
// composable, cacheable, animatable VALUE:
//
//   ground .......... Material::linear 4-stop ramp (recipe-prunes across
//                     re-renders — no memo, no re-records)
//   ember halo ...... Material::blend — radial core + kPlus sweep highlight
//                     + kScreen rim, flattened to ONE shader (no saveLayer)
//   breathing ring .. Material::sksl with uPulse BOUND to a ch::Output and
//                     uTime auto-injected — the material itself declares the
//                     node volatile; nothing else repaints
//   star sigil ...... shapes::star outline + sweep-ramp material, spinning
//                     via a bound rotate (transform-replay: content picture
//                     replays, zero re-records while it spins)
//   rules & chrome .. PathFormat dashes via shapes::onEdges (bottom edge
//                     only) — decorations compare by value, so they prune
//   film grain ...... Material::sksl reading uTime, soft-light over the
//                     whole frame
//   entrance ........ Choreograph timeline ramps (Hold-staggered drop+fade
//                     per line, retarget-safe)
//   grade ........... OCIO output view on the Composer (ACES SDR view from
//                     the ASWF built-in config, falling back to a gentle
//                     exponent grade) — one saveLayer, post-cache
//
// Headless captures (what the morning review PNGs are):
//   ComposeSketch <this> --frame poster.png --at 2.6

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>
#if defined(SIGILCOMPOSE_ENABLE_OCIO)
#include <sigilcompose/Ocio.h>
#endif

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

// ---- palette (authored flat; the OCIO view grades the composite) ----------
constexpr SkColor4f kInk{0.043f, 0.031f, 0.075f, 1};
constexpr SkColor4f kPlum{0.180f, 0.075f, 0.190f, 1};
constexpr SkColor4f kEmberDeep{0.420f, 0.120f, 0.075f, 1};
constexpr SkColor4f kEmber{0.960f, 0.475f, 0.180f, 1};
constexpr SkColor4f kEmberHot{1.000f, 0.800f, 0.420f, 1};
constexpr SkColor4f kBone{0.940f, 0.910f, 0.860f, 1};
constexpr SkColor4f kAsh{0.600f, 0.575f, 0.640f, 1};

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// The breathing ring: pure SkSL over the node's box. uPulse is a live bound
// uniform; uTime/uResolution arrive from PaintContext automatically.
sk_sp<SkRuntimeEffect> ringEffect() {
  static const char *kSkSL = R"(
    uniform float uPulse;
    uniform float uTime;
    uniform float2 uResolution;
    half4 main(float2 p) {
      float2 c = uResolution * 0.5;
      float r = min(uResolution.x, uResolution.y) * 0.5;
      float d = distance(p, c) / r;
      float ringAt = 0.74 + 0.045 * uPulse;
      float band = abs(d - ringAt);
      float ring = smoothstep(0.016, 0.002, band);
      float glow = exp(-9.0 * band) * (0.55 + 0.30 * uPulse);
      // slow rune ticks riding the ring
      float ang = atan(p.y - c.y, p.x - c.x);
      float ticks = smoothstep(0.86, 0.995, sin(ang * 24.0 + uTime * 0.7))
                    * smoothstep(0.05, 0.012, band);
      float a = clamp(ring * 0.9 + glow * 0.5 + ticks * 0.65, 0.0, 1.0);
      float3 col = float3(1.00, 0.62, 0.26) * (ring + glow * 0.6)
                 + float3(1.00, 0.84, 0.48) * ticks;
      col = clamp(col, 0.0, 1.0);
      return half4(half3(col * a), half(a)); // premul
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx)
      SkDebugf("ring shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

// Film grain, soft-lighted over the frame. uTime keeps it alive.
sk_sp<SkRuntimeEffect> grainEffect() {
  static const char *kSkSL = R"(
    uniform float uTime;
    half4 main(float2 p) {
      float n = fract(sin(dot(p + fract(uTime) * 61.7,
                              float2(12.9898, 78.233))) * 43758.5453);
      half g = half(0.5 + 0.5 * n);
      return half4(g, g, g, 0.06);
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx)
      SkDebugf("grain shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

} // namespace

struct MotionPoster : sketch::Sketch {
  // Entrance choreography (drop + fade per line, Hold-staggered).
  ch::Output<float> dropTitle{54}, fadeTitle{0};
  ch::Output<float> dropSub{40}, fadeSub{0};
  ch::Output<float> dropInfo{28}, fadeInfo{0};
  // Living elements.
  ch::Output<float> pulse{0}, spin{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(810, 1012); // 4:5 poster
    ctx.background(kInk);

    auto &tl = ctx.ticker.timeline();
    auto enter = [&](ch::Output<float> &drop, ch::Output<float> &fade,
                     float from, float delay) {
      drop = from;
      fade = 0.0f;
      tl.apply(&drop).then<ch::Hold>(from, delay).then<ch::RampTo>(
          0.0f, 0.9f, &ch::easeOutQuint);
      tl.apply(&fade).then<ch::Hold>(0.0f, delay).then<ch::RampTo>(
          1.0f, 0.7f, &ch::easeOutQuad);
    };
    enter(dropTitle, fadeTitle, 54, 0.15f);
    enter(dropSub, fadeSub, 40, 0.38f);
    enter(dropInfo, fadeInfo, 28, 0.60f);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      pulse = (float)std::sin(t * 1.7);
      spin = (float)(t * 5.5); // slow degrees/sec
      return true;
    });

#if defined(SIGILCOMPOSE_ENABLE_OCIO)
    // The color-managed output stage. Two working grades:
    //  - default: a gentle exponent contrast grade (display-referred, matches
    //    how this palette is authored);
    //  - kUseAces: the real ACES 2.0 SDR view from the ASWF CG built-in
    //    config. It WORKS (bakes + applies), but ACES expects SCENE-LINEAR
    //    input — feeding it display-referred colors lifts/washes the frame.
    //    Flip it on after re-authoring the palette in linear (the honest
    //    ACES contract; F16 intermediates for >1.0 embers are the follow-up).
    constexpr bool kUseAces = false;
    Effect view = kUseAces
                      ? ocio::display("ocio://cg-config-latest",
                                      "sRGB - Display",
                                      "ACES 2.0 - SDR 100 nits (Rec.709)")
                      : ocio::exponent(1.12f);
    if (!view.imageFilter())
      view = ocio::exponent(1.12f); // fail-soft fallback
    ctx.composer.setView(std::move(view));
#endif

    ctx.composer.render(describe());
  }

  Element describe() {
    const float W = 810, H = 1012;
    const SkPoint focus = {W * 0.5f, H * 0.40f};

    // Ground: one 4-stop ramp — a value, cached, recipe-pruned.
    Material ground = Material::linear(
        {0, 0}, {0, H},
        {{0.00f, kInk}, {0.55f, kPlum}, {0.82f, kEmberDeep}, {1.00f, kInk}});

    // Ember halo: three layers flattened into ONE shader (no saveLayer):
    // radial core over transparent, kPlus sweep highlight, kScreen rim.
    Material halo = Material::blend({
        {Material::radial(focus, 300,
                          {{0.00f, {0.96f, 0.48f, 0.18f, 0.85f}},
                           {0.45f, {0.42f, 0.12f, 0.08f, 0.35f}},
                           {1.00f, {0, 0, 0, 0}}}),
         SkBlendMode::kSrcOver},
        {Material::sweep(focus,
                         {{0.00f, {0, 0, 0, 0}},
                          {0.18f, {1.0f, 0.80f, 0.42f, 0.30f}},
                          {0.36f, {0, 0, 0, 0}},
                          {1.00f, {0, 0, 0, 0}}},
                         -104, 256),
         SkBlendMode::kPlus},
        {Material::radial(focus, 340,
                          {{0.86f, {0, 0, 0, 0}},
                           {0.94f, {1.0f, 0.62f, 0.26f, 0.28f}},
                           {1.00f, {0, 0, 0, 0}}}),
         SkBlendMode::kScreen},
    });

    // The breathing ring: LIVE material — uPulse bound, uTime auto. The
    // material declares the volatility; no Cache::None, no custom().
    Material ring = Material::sksl(ringEffect()).uniform("uPulse", &pulse);

    // The star sigil: geometry from shapes::, paint from a sweep ramp,
    // motion from a bound rotate — transform-replay keeps it one recording.
    Material starFill = Material::sweep(
        {70, 70}, {{0.0f, kEmberHot}, {0.5f, kEmber}, {1.0f, kEmberHot}});

    // Dashed hairline rule under the info block: a value decoration on the
    // bottom edge only.
    PathFormat rule;
    rule.width = 1.2f;
    rule.strokeFill = Fill::color({0.94f, 0.91f, 0.86f, 0.55f});
    rule.dashIntervals = {10, 7};

    // Film grain: sksl reading uTime — declaring the clock makes it live
    // automatically, no cache annotation needed.
    Material grain = Material::sksl(grainEffect());

    return stack()
        .fill(ground)
        // halo + ring share the focus point
        .child(box().absolute().inset(0).fill(halo))
        .child(box().width(560).height(560)
                   .inset(focus.x() - 280, focus.y() - 280,
                          W - focus.x() - 280, H - focus.y() - 280)
                   .absolute()
                   .fill(ring))
        // spinning star sigil at the ring's heart
        .child(box().width(140).height(140)
                   .inset(focus.x() - 70, focus.y() - 70, W - focus.x() - 70,
                          H - focus.y() - 70)
                   .absolute()
                   .outline(shapes::rounded(shapes::star(9, 0.58f), 3))
                   .fill(starFill)
                   .rotate(&spin)
                   .opacity(0.92f))
        // the typographic block
        .child(
            box().column().absolute().inset(64, 0, 64, 0).zIndex(2)
                .child(box().grow(1)) // push type into the lower third
                .child(text(toU8("EMBER GATE"), type(108, kBone, 2))
                           .key("title")
                           .translateY(&dropTitle)
                           .opacity(&fadeTitle))
                .child(text(toU8("the ifrit protocol — movement II"),
                            type(30, kEmber, 1))
                           .key("sub")
                           .translateY(&dropSub)
                           .opacity(&fadeSub)
                           .margin(0, 10, 0, 0))
                .child(box().key("rules").height(26).margin(0, 26, 0, 0)
                           .foreground(shapes::onEdges(shapes::Edge::Bottom,
                                                       rule))
                           .translateY(&dropInfo)
                           .opacity(&fadeInfo))
                .child(box().row().alignItems(Align::Baseline).gap(18)
                           .margin(0, 14, 0, 88)
                           .translateY(&dropInfo)
                           .opacity(&fadeInfo)
                           .child(text(toU8("XXI"), type(40, kEmberHot)))
                           .child(text(toU8("midsummer"), type(21, kAsh, 3)))
                           .child(box().grow(1))
                           .child(text(toU8("the flooded causeway"),
                                       type(21, kAsh, 1)))
                           .child(text(toU8("§ vol. 4"), type(21, kEmber)))))
        // living film grain across everything beneath
        .child(box().absolute().inset(0).zIndex(3)
                   .fill(grain)
                   .blend(SkBlendMode::kSoftLight));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(MotionPoster)
