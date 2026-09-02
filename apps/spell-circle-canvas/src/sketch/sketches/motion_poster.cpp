/** @file
 * motion poster — EMBER GATE, the flagship living poster: type,
 * ornament and material moving together on one schedule.
 */

// EMBER GATE — a living typographic poster written with ZERO raw custom()
// lambdas. Every visual on it is a composable, cacheable, animatable VALUE,
// which is the constraint the scene exists to demonstrate:
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
//   film grain ...... patterns::grain, soft-light over the
//                     poster panel
//   entrance ........ Choreograph timeline ramps (Hold-staggered drop+fade
//                     per line, retarget-safe)
//   grade ........... carried in the palette's own numbers rather than in
//                     a view: a display-referred exponent is a per-channel
//                     power, and a palette of flat inks can hold it. A view
//                     is a full-canvas pass every frame, which on the raster
//                     backend costs more than the whole poster draws.
//
// The poster is authored in its own 4:5 portrait space and paint-scaled onto
// the canvas, WHICH IS THAT SHAPE. Scaling by transform rather than by
// re-authoring means one recording replayed under a matrix; declaring the
// canvas in the poster's own proportion means the plate is a poster rather
// than a photograph of one lying on a table.
//
// The grain overlay sits OUTSIDE that scaled subtree, at canvas-native
// resolution, so its noise stays pixel-sized instead of being magnified with
// everything else.

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/core/Patterns.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {512, 640};

namespace ember_poster {

// ---- stage geometry -------------------------------------------------------
constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kPanelW = kW, kPanelH = kH;  // the canvas IS the 4:5 poster
constexpr float kPW = 810, kPH = 1012;       // sketch author space
constexpr float kScale = kPanelW / kPW;      // paint-scale into panel

// ---- palette, authored WITH its own contrast --------------------------
// A display-referred exponent view is a full-canvas pass every frame, and
// on the raster backend that pass costs more than everything the poster
// draws put together. The curve is a per-channel power, so a palette of
// flat inks can carry it in its own numbers: these are the inks at
// exponent 1.12, which is what a view would have applied to them.
constexpr SkColor4f kInk{0.029f, 0.020f, 0.055f, 1};
constexpr SkColor4f kPlum{0.147f, 0.055f, 0.156f, 1};
constexpr SkColor4f kEmberDeep{0.378f, 0.093f, 0.055f, 1};
constexpr SkColor4f kEmber{0.955f, 0.434f, 0.147f, 1};
constexpr SkColor4f kEmberHot{1.000f, 0.779f, 0.378f, 1};
constexpr SkColor4f kBone{0.933f, 0.900f, 0.845f, 1};
constexpr SkColor4f kAsh{0.564f, 0.538f, 0.607f, 1};

/** This study's type colour reaches the paint as 8-bit sRGB, so a tint
 *  computed per frame lands on the same 256-step ladder as a quoted one.
 *  `compose::type` carries the float through instead, and the device
 *  raster resolves the two differently. */
inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  return sigil::compose::type(
      {.size = size, .color = color, .track = tracking, .color8 = true});
}

// The breathing ring: pure SkSL over the node's box. uPulse is a live bound
// uniform; uTime/uResolution arrive from PaintContext automatically.
inline sk_sp<SkRuntimeEffect> ringEffect() {
  static const char* kSkSL = R"(
    uniform float uPulse;
    uniform float uTime;
    uniform float2 uResolution;
    half4 main(float2 p) {
      float2 c = uResolution * 0.5;
      float r = min(uResolution.x, uResolution.y) * 0.5;
      float d = distance(p, c) / r;
      float ringAt = 0.74 + 0.045 * uPulse;
      float band = abs(d - ringAt);
      // Everything below is transparent past a quarter of the radius from
      // the band, and the box this runs over is mostly that: leaving early
      // skips the exp, the atan and the sin for the four pixels in five
      // that could only ever return zero.
      if (band > 0.25) return half4(0);
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
    if (!fx) SkDebugf("ring shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

}  // namespace ember_poster

struct MotionPoster final : sketch::Sketch {
  // Entrance choreography (drop + fade per line, Hold-staggered).
  choreograph::Output<float> dropTitle{54}, fadeTitle{0};
  choreograph::Output<float> dropSub{40}, fadeSub{0};
  choreograph::Output<float> dropInfo{28}, fadeInfo{0};
  // Living elements.
  choreograph::Output<float> pulse{0}, spin{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.captureAt(6.0);
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    pulse = 0.0f;
    spin = 0.0f;

    auto& tl = ticker.timeline();
    auto enter = [&](choreograph::Output<float>& drop,
                     choreograph::Output<float>& fade, float from,
                     float delay) {
      drop = from;
      fade = 0.0f;
      tl.apply(&drop)
          .then<choreograph::Hold>(from, delay)
          .then<choreograph::RampTo>(0.0f, 0.9f, &choreograph::easeOutQuint);
      tl.apply(&fade)
          .then<choreograph::Hold>(0.0f, delay)
          .then<choreograph::RampTo>(1.0f, 0.7f, &choreograph::easeOutQuad);
    };
    enter(dropTitle, fadeTitle, 54, 0.15f);
    enter(dropSub, fadeSub, 40, 0.38f);
    enter(dropInfo, fadeInfo, 28, 0.60f);

    ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      pulse = (float)std::sin(t * 1.7);
      spin = (float)(t * 5.5);  // slow degrees/sec
      return true;
    });

    composer.render(describe());
  }

  /** The poster itself, verbatim in the sketch's 810x1012 author space
   *  (minus the grain overlay, which lives at panel-native resolution in
   *  describe()). Everything static here is one cached recording; the
   *  spinning star and dropping lines are transform-replay. */
  Element poster() {
    namespace ep = ember_poster;
    const float W = ep::kPW, H = ep::kPH;
    // The ring's lower arc used to cross the title at cap height, with no
    // exclusion and no knockout, so the overlap read as an accident rather
    // than as a device. Raised, it closes above the type.
    const SkPoint focus = {W * 0.5f, H * 0.335f};

    // Ground: one 4-stop ramp — a value, cached, recipe-pruned.
    Material ground = Material::linear({0, 0}, {0, H},
                                       {{0.00f, ep::kInk},
                                        {0.55f, ep::kPlum},
                                        {0.82f, ep::kEmberDeep},
                                        {1.00f, ep::kInk}});

    // Ember halo: three layers flattened into ONE shader (no saveLayer):
    // radial core over transparent, kPlus sweep highlight, kScreen rim.
    Material halo = Material::blend({
        {Material::radial(focus, 300,
                          {{0.00f, {0.96f, 0.48f, 0.18f, 0.85f}},
                           {0.45f, {0.42f, 0.12f, 0.08f, 0.35f}},
                           {1.00f, {0, 0, 0, 0}}}),
         SkBlendMode::kSrcOver},
        // The sweep highlight, EXPRESSED IN THE FULL 0..360 FRAME. Keep it
        // that way.
        //
        // A sweep's parameter comes from atan2, whose branch cut lies along
        // the +x ray. Narrowing the start/end angles does not rotate that
        // cut — it only narrows the reachable range of t, and any stop
        // outside the reachable range clamps. The visible result is not a
        // missing highlight: it is a hard step running the full width of the
        // scene through the sweep's centre, along the +x ray, where the
        // clamped value meets the live one.
        //
        // So the band is written across 0..360 instead. It runs transparent
        // at 256 deg, peaks near 321, and returns to transparent by 25.6,
        // which means it straddles 0 deg — hence the matching alpha at t=0
        // and t=1, which is what makes the wrap continuous.
        {Material::sweep(focus,
                         {{0.0000f, {1.0f, 0.80f, 0.42f, 0.1185f}},
                          {0.0711f, {0, 0, 0, 0}},
                          {0.7111f, {0, 0, 0, 0}},
                          {0.8911f, {1.0f, 0.80f, 0.42f, 0.30f}},
                          {1.0000f, {1.0f, 0.80f, 0.42f, 0.1185f}}},
                         0, 360),
         SkBlendMode::kPlus},
        {Material::radial(focus, 340,
                          {{0.86f, {0, 0, 0, 0}},
                           {0.94f, {1.0f, 0.62f, 0.26f, 0.28f}},
                           {1.00f, {0, 0, 0, 0}}}),
         SkBlendMode::kScreen},
    });

    // The breathing ring: LIVE material — uPulse bound, uTime auto. The
    // material declares the volatility; no Cache::None, no custom().
    Material ring = Material::sksl(ep::ringEffect()).uniform("uPulse", &pulse);

    // The star sigil: geometry from shapes::, paint from a sweep ramp,
    // motion from a bound rotate — transform-replay keeps it one recording.
    Material starFill = Material::sweep(
        {70, 70},
        {{0.0f, ep::kEmberHot}, {0.5f, ep::kEmber}, {1.0f, ep::kEmberHot}});

    // Dashed hairline rule under the info block: a value decoration on the
    // bottom edge only.
    PathFormat rule;
    rule.width = 1.2f;
    rule.strokeFill = Fill::color({0.94f, 0.91f, 0.86f, 0.55f});
    rule.dashIntervals = {10, 7};

    return stack()
        .fill(ground)
        // halo + ring share the focus point
        .child(box().inset(0).fill(halo))
        .child(box()
                   .width(560)
                   .height(560)
                   .inset(focus.x() - 280, focus.y() - 280, W - focus.x() - 280,
                          H - focus.y() - 280)
                   .fill(ring))
        // spinning star sigil at the ring's heart
        .child(box()
                   .width(140)
                   .height(140)
                   .inset(focus.x() - 70, focus.y() - 70, W - focus.x() - 70,
                          H - focus.y() - 70)
                   .shape(shapes::rounded(shapes::star(9, 0.58f), 3))
                   .fill(starFill)
                   .rotate(&spin)
                   .opacity(0.92f))
        // the typographic block
        .child(
            box()
                .column()
                .inset(64, 0, 64, 0)
                .zIndex(2)
                .child(box().grow(1))  // push type into the lower third
                .child(text(toU8("EMBER GATE"), ep::type(108, ep::kBone, 2))
                           .key("title")
                           .translateY(&dropTitle)
                           .opacity(&fadeTitle))
                .child(text(toU8("the ifrit protocol \xe2\x80\x94 movement II"),
                            ep::type(30, ep::kEmber, 1))
                           .key("sub")
                           .translateY(&dropSub)
                           .opacity(&fadeSub)
                           .margin(0, 10, 0, 0))
                .child(
                    box()
                        .key("rules")
                        .height(26)
                        .margin(0, 26, 0, 0)
                        .foreground(shapes::onEdges(shapes::Edge::Bottom, rule))
                        .translateY(&dropInfo)
                        .opacity(&fadeInfo))
                .child(
                    box()
                        .row()
                        .alignItems(Align::Baseline)
                        .gap(18)
                        .margin(0, 14, 0, 88)
                        .translateY(&dropInfo)
                        .opacity(&fadeInfo)
                        .child(text(toU8("XXI"), ep::type(40, ep::kEmberHot)))
                        .child(
                            text(toU8("midsummer"), ep::type(21, ep::kAsh, 3)))
                        .child(box().grow(1))
                        .child(text(toU8("the flooded causeway"),
                                    ep::type(21, ep::kAsh, 1)))
                        .child(text(toU8("\xc2\xa7 vol. 4"),
                                    ep::type(21, ep::kEmber)))));
  }

  Element describe() {
    namespace ep = ember_poster;
    return stack()
        .fill(Fill::color(ep::kInk))
        // The poster paints in its own space and the scale transform
        // replays its pictures — no re-authoring, no re-records.
        .child(box()
                   .inset(0)
                   .fill(Fill::color(ep::kInk))
                   .clip()
                   .child(poster()
                              .width(ep::kPW)
                              .height(ep::kPH)
                              .left(0)
                              .top(0)
                              .transformOrigin(0, 0)
                              .scale(ep::kScale))
                   // living film grain at panel-native resolution (outside the
                   // scaled subtree, so the noise stays pixel-sized)
                   .child(box()
                              .inset(0)
                              .zIndex(3)
                              // The library's own grain field. It was a raw
                              // runtime effect reseeded from uTime, which is
                              // a full-canvas hash every frame and the
                              // sketch's single largest cost; the field is a
                              // recipe the runtime can hold still, and a
                              // still grain is what a printed poster has.
                              .fill(patterns::grain(0.9f, 3, 7.0f, 1.0f))
                              .opacity(0.20f)
                              .blend(SkBlendMode::kSoftLight)
                              .cache(Cache::Texture)));
  }
};

}  // namespace

SIGIL_SKETCH_AS(MotionPoster, "motion poster", "Catalog \xc2\xb7 Type & grid",
                "EMBER GATE \xe2\x80\x94 the flagship living poster")
