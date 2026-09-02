// blur_falloff.cpp — ONE API: Effect::blur(Material sigmaMap, float maxSigma).
// =============================================================================
// A blur whose sigma is a FUNCTION OF POSITION. Every panel holds the same
// content and the same maximum sigma; only the MAP differs, and the map is an
// ordinary Material — which is the whole point: unit-space ramps are already
// authored against whatever box the layout decides, so a focal falloff is one
// line and needs no pixel arithmetic.
//
//   1 a constant blur, for contrast: all legible or none of it.
//   2 DEPTH OF FIELD — a 3-stop linearUnit down the box.
//   3 A LENS EDGE — glowUnit from the centre: sharp on axis, soft at the rim.
//   4 RACK FOCUS — the same map, maxSigma BOUND: no re-describe.
//
// EDIT THESE FIRST
//   kMaxSigma  — the blur radius the map's 1.0 end reaches. Every panel
//                shares it, so raising it separates the panels further and
//                lowering it makes panel 1 and panel 2 converge.
//   kFocal     — where the sharp line sits down panel 2's box, 0..1. At 0 the
//                falloff becomes a plain top-to-bottom ramp.
//   kRackHz    — how fast panel 4 breathes.
//   the map in dofMap() — swap linearUnit for radialUnit, or add stops, and
//                only the FALLOFF changes: the effect, the content and the
//                node are untouched.
//
// Of the three ways a scene can move, panel 4 is the driven one: nothing
// re-describes, an Output does the moving. The bound Output IS the volatility
// declaration — a live map or a live maxSigma lifts the whole effect to live,
// so no bake can sample the parameter once and freeze it.

#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 240.0f;
constexpr float kMaxSigma = 14.0f;  // the map's 1.0 end, in px of sigma
constexpr float kFocal = 0.42f;     // panel 2's sharp line, 0..1 down the box
constexpr double kRackHz = 0.18;    // panel 4's breathing rate

const SkColor4f kInk{0.92f, 0.94f, 0.98f, 1};
const SkColor4f kDim{0.56f, 0.61f, 0.72f, 1};

/** THE CONTENT, one function so every panel blurs the SAME picture: fine
 *  horizontal rules (detail a blur destroys visibly) under three discs. */
Material rules() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("half4 main(float2 p) {"
                 "  float band = mod(floor(p.y / 7.0), 2.0);"
                 "  return band < 1.0 ? half4(0.16, 0.19, 0.28, 1)"
                 "                    : half4(0.62, 0.70, 0.86, 1);"
                 "}"));
    return effect;
  }();
  return Material::sksl(fx);
}

Element disc(float size, float left, float top, SkColor4f color) {
  return box()
      .width(size)
      .height(size)
      .left(left)
      .top(top)
      .corners({size * 0.5f})
      .fill(Material::solid(color));
}

Element subject() {
  return stack()
      .width(kPanel)
      .height(kPanel)
      .fill(rules())
      .child(disc(76, 26, 30, {0.98f, 0.44f, 0.34f, 1}))
      .child(disc(52, 96, 96, {0.42f, 0.86f, 0.72f, 1}))
      .child(disc(96, 118, 132, {0.96f, 0.82f, 0.36f, 1}));
}

/** DEPTH OF FIELD: three stops down the unit square — max sigma at the top
 *  edge, zero at the focal line, max again at the bottom. */
Material dofMap() {
  return Material::linearUnit(
      {0, 0}, {0, 1},
      {{0.0f, {1, 1, 1, 1}}, {kFocal, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

/** A LENS EDGE: zero on axis, max at the inscribed circle. glowUnit is the
 *  one that means "fills this box" (radialUnit reaches the corners). */
Material lensMap() {
  return Material::glowUnit({0.5f, 0.5f}, 1.0f,
                            {{0.0f, {0, 0, 0, 1}}, {1.0f, {1, 1, 1, 1}}});
}

Element panel(const char* title, const char* note, Effect e,
              const std::string& key) {
  return box()
      .width(kPanel)
      .column()
      .gap(6)
      .child(text(toU8(title), type({.size = 13, .color = kInk})))
      .child(subject().key(std::move(key)).effect(std::move(e)))
      .child(text(toU8(note), type({.size = 11, .color = kDim})));
}

}  // namespace

struct BlurFalloff : sketch::Sketch {
  choreograph::Output<float> rack{0.0f};  // panel 4's bound maxSigma

  Element describe() {
    return stack()
        .child(text(toU8("Effect::blur(Material sigmaMap, float maxSigma) "
                         "\xc2\xb7 one effect, four falloffs"),
                    type({.size = 15, .color = kInk}))
                   .left(30)
                   .top(16))
        .child(box()
                   .row()
                   .left(30)
                   .top(52)
                   .gap(20)
                   .child(panel("constant (before)",
                                "Blur(14, 14): all or "
                                "nothing",
                                Effect::filter(SkImageFilters::Blur(
                                    kMaxSigma, kMaxSigma, nullptr)),
                                "flat"))
                   .child(panel("depth of field",
                                "linearUnit, 3 stops down "
                                "the box",
                                Effect::blur(dofMap(), kMaxSigma), "dof"))
                   .child(panel("lens edge", "glowUnit from the centre",
                                Effect::blur(lensMap(), kMaxSigma), "lens"))
                   .child(panel(
                       "rack focus", "the SAME map, maxSigma bound",
                       Effect::blur(dofMap(), 0).uniform("maxSigma", &rack),
                       "rack")))
        .child(text(toU8("the parameter is a Material, so it prunes, it "
                         "animates on the one uniform channel, and its unit "
                         "square is whatever box the layout decided"),
                    type({.size = 11, .color = kDim}))
                   .left(30)
                   .bottom(14));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1080, 372);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(1.6);  // rack focus near its widest
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    (void)ctx;
    // Derived from `elapsed`, not accumulated: a still at a declared time is
    // then the same still every run. No re-describe — the bound parameter
    // makes the effect live all by itself.
    const double phase = elapsed * kRackHz * 6.2831853;
    rack = kMaxSigma * 0.5f * (float)(1.0 - std::cos(phase));
  }
};

SIGIL_SKETCH(BlurFalloff, "Kit \xc2\xb7 API",
             "Effect::blur(Material, maxSigma) \xe2\x80\x94 one effect, four "
             "falloffs: constant, depth of field, a lens edge, a rack focus")
