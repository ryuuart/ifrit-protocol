#pragma once
// The kinetic-typography card (REFERENCES.md §8 — the community grammar),
// ported from sketch/sketches/kinetic_study.cpp: the whole card enters via
// withFrom() MOUNT choreography — one dominant move (the hero MaskedRise),
// amount-mode stagger budgets (≤ 1.2s), Transition::delay sequencing, a
// draw-on rule hosting a wrap-trim comet, a CharPop subline under a double
// textGlow, a WaveFloat loop, and a measured util::marquee ticker.
//
// This file is the PORTING EXEMPLAR for study→scene conversions:
//  - constants/helpers live in a scene-private nested namespace
//  - SketchContext dissolves: canvas() → the registry's kSceneSize
//    (adapt layout constants), background() → the root's fill,
//    ctx.composer/ticker → the setup() parameters, sketch-local
//    FontContexts → the gallery's fonts()
//  - loop clocks re-zero in setup() (scenes re-activate)

#include "GalleryCore.h"

#include <sigilcompose/Kinetic.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>

namespace compose_gallery {

namespace kinetic_card {

// ---- restrained editorial palette: bone + ash + ONE accent ----------------
constexpr SkColor4f kInk{0.043f, 0.043f, 0.058f, 1};
constexpr SkColor4f kInkLift{0.058f, 0.053f, 0.072f, 1};
constexpr SkColor4f kInkFoot{0.086f, 0.063f, 0.066f, 1};
constexpr SkColor4f kBone{0.930f, 0.920f, 0.890f, 1};
constexpr SkColor4f kAsh{0.540f, 0.540f, 0.590f, 1};
constexpr SkColor4f kAccent{0.980f, 0.360f, 0.250f, 1};

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kHeroSize = 112;
constexpr float kTickerH = 48;
constexpr float kTickerSpeed = 90;   // px/s — the 50-120 circulating band
constexpr float kTickerGap = 56;     // between the two marquee copies
constexpr float kWavePeriod = 1.6f;  // the WaveFloat community constant
constexpr float kCometPeriod = 2.8f; // the marching-rule lap time
constexpr float kRuleW = 380;

} // namespace kinetic_card

struct KineticCardScene final : Scene {
  choreograph::Output<float> wavePhase{0}, cometPhase{0}, tickX{0};
  float unitW = 0;   // ticker content's intrinsic width (compose::measure)
  float wrapLen = 1; // marquee wrap length = unitW + gap

  const char *name() const override { return "kinetic card"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace kc = kinetic_card;
    wavePhase = 0;
    cometPhase = 0;
    tickX = 0;

    const SkSize unit = sigil::compose::measure(tickerContent(), fonts());
    unitW = std::ceil(unit.width());
    wrapLen = unitW + kc::kTickerGap;

    ticker.add([this, t = 0.0](double dt) mutable {
      namespace kc = kinetic_card;
      t += dt;
      wavePhase = (float)std::fmod(t / kc::kWavePeriod, 1.0);
      cometPhase = (float)std::fmod(t / kc::kCometPeriod, 1.0);
      tickX = -(float)std::fmod(t * kc::kTickerSpeed, (double)wrapLen);
      return true;
    });

    composer.render(describe());
  }

  Element tickerContent() {
    namespace kc = kinetic_card;
    const char *unit = "WITHFROM MOUNT CHOREOGRAPHY   \xe2\x97\x8f   "
                       "EASE-OUT-EXPO 0.16 / 1 / 0.3 / 1   \xe2\x97\x8f   "
                       "AMOUNT-MODE 700 MS   \xe2\x97\x8f   BACK.OUT(1.7)   "
                       "\xe2\x97\x8f   WAVE 1.6 S \xc2\xb7 0.10 EM   "
                       "\xe2\x97\x8f   TRIM WRAP COMET   \xe2\x97\x8f   "
                       "90 PX/S LINEAR";
    Element content =
        box().row().alignItems(Align::Center).height(Dim(kc::kTickerH))
            .child(text(toU8(unit), kc::type(15, kc::kAsh, 2)).shrink(0));
    if (unitW > 0)
      content.width(Dim(unitW)).shrink(0);
    return content;
  }

  Element describe() {
    namespace kc = kinetic_card;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    Material ground = Material::linear(
        {0, 0}, {0, kc::kH},
        {{0.00f, kc::kInk}, {0.62f, kc::kInkLift}, {1.00f, kc::kInkFoot}});

    auto heroLine = [&](const char *s, const char *key, int delayMs) {
      GlyphFx fx;
      fx.effect = glyphfx::rise(kc::kHeroSize * 1.26f);
      fx.stagger = {.amountMs = 180, .durationMs = 500};
      fx.progress = withFrom(
          0.0f, 1.0f,
          {680ms, &ch::easeNone, std::chrono::milliseconds(delayMs)});
      return box().clip() // the line-box mask
          .child(text(toU8(s), kc::type(kc::kHeroSize, kc::kBone, 2))
                     .key(key)
                     .width(pct(100))
                     .textAlign(sigil::weave::TextAlignment::kCenter)
                     .glyphFx(std::move(fx)));
    };

    GlyphFx popFx;
    popFx.effect = glyphfx::pop(0.35f, 1.70158f); // back.out(1.7)
    popFx.stagger = {.amountMs = 700, .durationMs = 420,
                     .from = Stagger::From::Center};
    popFx.progress = withFrom(0.0f, 1.0f, {1120ms, &ch::easeNone, 450ms});

    GlyphFx waveFx;
    waveFx.effect = glyphfx::waveLoop(0.10f, 0.5f); // §8: 0.10em, 0.5 rad
    waveFx.stagger = {.eachMs = 0, .durationMs = 450}; // one master phase
    waveFx.progress = &wavePhase;

    auto lineOutline = [](SkSize s) {
      SkPathBuilder b;
      b.moveTo(0, s.height() * 0.5f);
      b.lineTo(s.width(), s.height() * 0.5f);
      return b.detach();
    };
    PathFormat ruleFmt;
    ruleFmt.width = 2;
    ruleFmt.strokeFill =
        Fill::color({kc::kAccent.fR, kc::kAccent.fG, kc::kAccent.fB, 0.8f});
    PathFormat cometFmt;
    cometFmt.width = 2;
    cometFmt.strokeFill = Fill::color(kc::kBone);

    PathFormat hairline;
    hairline.width = 1;
    hairline.strokeFill = Fill::color({0.93f, 0.92f, 0.89f, 0.22f});

    Effect glow =
        styles::textGlow(
            {kc::kAccent.fR, kc::kAccent.fG, kc::kAccent.fB, 0.55f}, 3)
            .then(styles::textGlow(
                {kc::kAccent.fR, kc::kAccent.fG, kc::kAccent.fB, 0.30f}, 10));

    // Keep enough transparent raster space for the 10px double glow. The
    // negative margins exactly cancel this wrapper's vertical padding, so
    // the line keeps its original layout position/height. While the glyph
    // entrance is active subtree volatility bypasses the texture; once it
    // settles the expensive filter result becomes one cached blit.
    Element popLine =
        box().width(pct(100))
            .padding(0, 32)
            .margin(0, -6, 0, -32)
            .cache(Cache::Texture)
            .child(text(toU8("STAGGERED \xc2\xb7 POPPED \xc2\xb7 WAVED"),
                        kc::type(26, kc::kAccent, 4))
                       .key("popline")
                       .width(pct(100))
                       .textAlign(sigil::weave::TextAlignment::kCenter)
                       .glyphFx(std::move(popFx))
                       .effect(glow));

    return stack()
        .fill(ground)
        .child(
            box().column().absolute().inset(64, 44, 64, kc::kTickerH)
                .child(box().row().alignItems(Align::Baseline)
                           .opacity(withFrom(0.0f, 1.0f,
                                             {450ms, &ch::easeOutQuad}))
                           .translateY(withFrom(-14.0f, 0.0f,
                                                {450ms, &ch::easeOutCubic}))
                           .child(box().width(Dim(10.0f)).height(Dim(10.0f))
                                      .alignSelf(Align::Center)
                                      .margin(0, 0, 12, 0)
                                      .fill(PropValue<Fill>(withFrom(
                                          Fill::color(kc::kBone),
                                          Fill::color(kc::kAccent),
                                          {900ms, &ch::easeOutQuad}))))
                           .child(text(toU8("SIGIL \xe2\x80\x94 MOTION STUDY"),
                                       kc::type(15, kc::kAsh, 3)))
                           .child(box().grow(1))
                           .child(text(toU8("REFERENCES \xc2\xa7"
                                            "8 \xe2\x80\x94 ENTRANCES"),
                                       kc::type(15, kc::kAccent, 3))))
                .child(box().grow(1))
                .child(heroLine("KINETIC", "hero1", 100))
                .child(heroLine("GRAMMAR", "hero2", 220))
                .child(box().width(Dim(kc::kRuleW)).height(Dim(2.0f))
                           .alignSelf(Align::Center)
                           .margin(0, 30, 0, 0)
                           .outline(lineOutline)
                           .stroke(ruleFmt)
                           .trim(0.0f,
                                 withFrom(0.0f, 1.0f,
                                          {500ms, &ch::easeOutExpo, 360ms}))
                           .child(box().absolute().inset(0, 0, 0, 0)
                                      .outline(lineOutline)
                                      .stroke(cometFmt)
                                      .trim(0.0f, 0.06f, &cometPhase,
                                            TrimMode::Wrap)
                                      .opacity(withFrom(
                                          0.0f, 1.0f,
                                          {500ms, &ch::easeOutQuad, 1500ms}))))
                .child(std::move(popLine))
                .child(text(toU8("floating on a 1.6 s sine \xe2\x80\x94 "
                                 "amplitude 0.10 em \xc2\xb7 phase 0.5 rad "
                                 "per glyph"),
                            kc::type(19, kc::kAsh, 1))
                           .key("waveline")
                           .width(pct(100))
                           .textAlign(sigil::weave::TextAlignment::kCenter)
                           .glyphFx(std::move(waveFx))
                           .opacity(withFrom(0.0f, 1.0f,
                                             {560ms, &ch::easeOutQuad, 840ms}))
                           .margin(0, 22, 0, 0))
                .child(box().grow(1)))
        .child(util::marquee(tickerContent(), &tickX, kc::kTickerGap)
                   .absolute().inset(0, kc::kH - kc::kTickerH, 0, 0)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {560ms, &ch::easeOutQuad, 1040ms}))
                   .foreground(shapes::onEdges(shapes::Edge::Top, hairline)));
  }
};

} // namespace compose_gallery
