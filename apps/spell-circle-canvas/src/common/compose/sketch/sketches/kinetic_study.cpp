// kinetic_study.cpp — contemporary kinetic typography, ROUND 2 (REFERENCES.md
// §8, "the community grammar"). The whole card now enters via withFrom()
// MOUNT choreography — no hand-driven timeline ramps — so the §8 composition
// laws are enforced by construction:
//
//   ONE DOMINANT MOVE .. the hero MaskedRise (glyphfx::rise under line-box
//                        clips); everything else is supporting cast
//   ENTRANCE BUDGET .... every paragraph's spread uses Stagger::amountMs
//                        (amount-mode clamps the total, §8: <= 1.2s), and
//                        each withFrom duration = holdMs + virtual span so
//                        the researched per-glyph timings play in real time
//   SEQUENCE ........... Transition::delay (landed mid-round after this
//                        study's friction report): withFrom holds `from`
//                        through the delay, then ramps — real enter-after
//                        choreography with no ease hacks
//
// The moves, all mount-transitioned:
//   kicker ..... opacity + translateY withFrom (fade-drop, easeOutQuad) and
//                a Fill color sweep on the bullet chip (bone -> accent)
//   hero ....... two centered lines (textAlign kCenter — the poster lockup),
//                staggered rise from +1.26 x fontSize under a clip; master
//                progress LINEAR (distribution ease belongs to start times)
//   rule ....... a 380px center rule that DRAWS ON via trim-end withFrom
//                (§8's reveal curve ease-out-expo), then hosts a marching
//                comet: fixed trim window + wrapping offset (TrimMode::Wrap)
//   subline .... CharPop back.out(1.7) from CENTER (the lockup pops outward),
//                amount-mode 700ms + 420ms dur = 1.12s span; double textGlow
//                accent (the one neon element, kept under the type)
//   wave ....... WaveFloat loop on a wrapping phase Output — loops stay
//                subordinate: 0.10em amplitude, faded in after the entrance
//   ticker ..... util::marquee (the seamless two-copy strip) with the wrap
//                length taken from compose::measure() — no bounds() polling
//
// Headless captures (the entrance reads as a sequence):
//   ComposeSketch <this> --frame kinetic_early.png --at 0.25
//   ComposeSketch <this> --frame kinetic_mid.png   --at 0.6
//   ComposeSketch <this> --frame kinetic.png       --at 3.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Kinetic.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkPathBuilder.h>

#include <sigilweave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

// ---- restrained editorial palette: bone + ash + ONE accent ----------------
constexpr SkColor4f kInk{0.043f, 0.043f, 0.058f, 1};
constexpr SkColor4f kInkLift{0.058f, 0.053f, 0.072f, 1};
constexpr SkColor4f kInkFoot{0.086f, 0.063f, 0.066f, 1};
constexpr SkColor4f kBone{0.930f, 0.920f, 0.890f, 1};
constexpr SkColor4f kAsh{0.540f, 0.540f, 0.590f, 1};
constexpr SkColor4f kAccent{0.980f, 0.360f, 0.250f, 1};

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

constexpr float kW = 960, kH = 600;
constexpr float kHeroSize = 112;
constexpr float kTickerH = 48;
constexpr float kTickerSpeed = 90;   // px/s — the 50-120 circulating band
constexpr float kTickerGap = 56;     // between the two marquee copies
constexpr float kWavePeriod = 1.6f;  // the WaveFloat community constant
constexpr float kCometPeriod = 2.8f; // the marching-rule lap time
constexpr float kRuleW = 380;

/** The host's FontContext isn't exposed to sketches, so measure() gets its
 *  own (same system font manager — identical metrics to the render path). */
sigil::weave::FontContext &sketchFonts() {
  static auto *context = new sigil::weave::FontContext(
      sigil::weave::ports::systemFontManager());
  return *context;
}

} // namespace

struct KineticStudy : sketch::Sketch {
  // Loop clocks only — every ONE-SHOT is a withFrom mount transition now.
  ch::Output<float> wavePhase{0}, cometPhase{0}, tickX{0};
  float unitW = 0;   // the content's intrinsic width, via compose::measure()
  float wrapLen = 1; // marquee wrap length = unitW + gap

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kInk);

    // The marquee wrap length comes from compose::measure() — the round-1
    // bounds()-polling dance (paint a frame, read bounds, then wrap) is gone.
    // The measured width is then PINNED onto the content: inside the marquee
    // the strip's auto width resolves against the clip viewport, and an
    // unpinned text child wraps to it.
    const SkSize unit = sigil::compose::measure(tickerContent(), sketchFonts());
    unitW = std::ceil(unit.width());
    wrapLen = unitW + kTickerGap;

    // Loop clock: wrapping wave/comet phases + the LINEAR ticker offset.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      wavePhase = (float)std::fmod(t / kWavePeriod, 1.0);
      cometPhase = (float)std::fmod(t / kCometPeriod, 1.0);
      tickX = -(float)std::fmod(t * kTickerSpeed, (double)wrapLen);
      return true;
    });

    ctx.composer.render(describe());
  }

  Element tickerContent() {
    const char *unit = "WITHFROM MOUNT CHOREOGRAPHY   ●   EASE-OUT-EXPO "
                       "0.16 / 1 / 0.3 / 1   ●   AMOUNT-MODE 700 MS   ●   "
                       "BACK.OUT(1.7)   ●   WAVE 1.6 S · 0.10 EM   ●   "
                       "TRIM WRAP COMET   ●   90 PX/S LINEAR";
    // Keyless by contract — marquee mounts it twice. Once measured, the
    // intrinsic width is pinned so the strip can't wrap the line.
    Element content =
        box().row().alignItems(Align::Center).height(Dim(kTickerH))
            .child(text(toU8(unit), type(15, kAsh, 2)).shrink(0));
    if (unitW > 0)
      content.width(Dim(unitW)).shrink(0);
    return content;
  }

  Element describe() {
    Material ground = Material::linear(
        {0, 0}, {0, kH},
        {{0.00f, kInk}, {0.62f, kInkLift}, {1.00f, kInkFoot}});

    // -- hero: staggered MaskedRise, centered lockup -----------------------
    // Stagger amount-mode: 180ms TOTAL spread + 500ms per-glyph dur = 680ms
    // virtual span, regardless of glyph count. withFrom duration is exactly
    // that span (played 1:1, LINEAR — per-glyph easing lives inside
    // glyphfx::rise); Transition::delay sequences the lines.
    auto heroLine = [&](const char *s, const char *key, int delayMs) {
      GlyphFx fx;
      fx.effect = glyphfx::rise(kHeroSize * 1.26f);
      fx.stagger = {.amountMs = 180, .durationMs = 500};
      fx.progress = withFrom(
          0.0f, 1.0f,
          {680ms, &ch::easeNone, std::chrono::milliseconds(delayMs)});
      return box().clip() // the line-box mask
          .child(text(toU8(s), type(kHeroSize, kBone, 2))
                     .key(key)
                     .width(pct(100))
                     .textAlign(sigil::weave::TextAlignment::kCenter)
                     .glyphFx(std::move(fx)));
    };

    // -- subline: CharPop from CENTER, double textGlow accent --------------
    // Amount-mode budget: 700ms spread + 420ms dur = 1.12s <= the 1.2s law.
    GlyphFx popFx;
    popFx.effect = glyphfx::pop(0.35f, 1.70158f); // back.out(1.7)
    popFx.stagger = {.amountMs = 700, .durationMs = 420,
                     .from = Stagger::From::Center};
    popFx.progress = withFrom(0.0f, 1.0f, {1120ms, &ch::easeNone, 450ms});

    // -- wave line: endless float on the wrapping phase --------------------
    GlyphFx waveFx;
    waveFx.effect = glyphfx::waveLoop(0.10f, 0.5f); // §8: 0.10em, 0.5 rad
    waveFx.stagger = {.eachMs = 0, .durationMs = 450}; // one master phase
    waveFx.progress = &wavePhase;

    // -- the center rule: draws on via trim withFrom, then hosts the comet -
    auto lineOutline = [](SkSize s) {
      SkPathBuilder b;
      b.moveTo(0, s.height() * 0.5f);
      b.lineTo(s.width(), s.height() * 0.5f);
      return b.detach();
    };
    PathFormat ruleFmt;
    ruleFmt.width = 2;
    ruleFmt.strokeFill = Fill::color({kAccent.fR, kAccent.fG, kAccent.fB, 0.8f});
    PathFormat cometFmt;
    cometFmt.width = 2;
    cometFmt.strokeFill = Fill::color(kBone);

    PathFormat hairline;
    hairline.width = 1;
    hairline.strokeFill = Fill::color({0.93f, 0.92f, 0.89f, 0.22f});

    // One neon accent, kept under the type: tight core + wide halo.
    Effect glow = styles::textGlow({kAccent.fR, kAccent.fG, kAccent.fB, 0.55f}, 3)
                      .then(styles::textGlow(
                          {kAccent.fR, kAccent.fG, kAccent.fB, 0.30f}, 10));

    return stack()
        .fill(ground)
        // the editorial column
        .child(
            box().column().absolute().inset(64, 44, 64, kTickerH)
                // kicker: fade-drop withFrom + the chip's color sweep
                .child(box().row().alignItems(Align::Baseline)
                           .opacity(withFrom(0.0f, 1.0f,
                                             {450ms, &ch::easeOutQuad}))
                           .translateY(withFrom(-14.0f, 0.0f,
                                                {450ms, &ch::easeOutCubic}))
                           .child(box().width(Dim(10.0f)).height(Dim(10.0f))
                                      .alignSelf(Align::Center)
                                      .margin(0, 0, 12, 0)
                                      .fill(PropValue<Fill>(withFrom(
                                          Fill::color(kBone),
                                          Fill::color(kAccent),
                                          {900ms, &ch::easeOutQuad}))))
                           .child(text(toU8("SIGIL — MOTION STUDY II"),
                                       type(15, kAsh, 3)))
                           .child(box().grow(1))
                           .child(text(toU8("REFERENCES §8 — ENTRANCES"),
                                       type(15, kAccent, 3))))
                .child(box().grow(1))
                .child(heroLine("KINETIC", "hero1", 100))
                .child(heroLine("GRAMMAR", "hero2", 220))
                .child(box().width(Dim(kRuleW)).height(Dim(2.0f))
                           .alignSelf(Align::Center)
                           .margin(0, 30, 0, 0)
                           .outline(lineOutline)
                           .stroke(ruleFmt)
                           // DRAW ON: trim end 0 -> 1, §8's reveal curve
                           .trim(0.0f,
                                 withFrom(0.0f, 1.0f,
                                          {500ms, &ch::easeOutExpo, 360ms}))
                           // the marching comet: fixed window, wrapping offset
                           .child(box().absolute().inset(0, 0, 0, 0)
                                      .outline(lineOutline)
                                      .stroke(cometFmt)
                                      .trim(0.0f, 0.06f, &cometPhase,
                                            TrimMode::Wrap)
                                      .opacity(withFrom(
                                          0.0f, 1.0f,
                                          {500ms, &ch::easeOutQuad, 1500ms}))))
                .child(text(toU8("STAGGERED · POPPED · WAVED"),
                            type(26, kAccent, 4))
                           .key("popline")
                           .width(pct(100))
                           .textAlign(sigil::weave::TextAlignment::kCenter)
                           .glyphFx(std::move(popFx))
                           .effect(glow)
                           .margin(0, 26, 0, 0))
                .child(text(toU8("floating on a 1.6 s sine — amplitude "
                                 "0.10 em · phase 0.5 rad per glyph"),
                            type(19, kAsh, 1))
                           .key("waveline")
                           .width(pct(100))
                           .textAlign(sigil::weave::TextAlignment::kCenter)
                           .glyphFx(std::move(waveFx))
                           .opacity(withFrom(0.0f, 1.0f,
                                             {560ms, &ch::easeOutQuad, 840ms}))
                           .margin(0, 22, 0, 0))
                .child(box().grow(1)))
        // the seamless ticker: util::marquee over the measured wrap length
        .child(util::marquee(tickerContent(), &tickX, kTickerGap)
                   .absolute().inset(0, kH - kTickerH, 0, 0)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {560ms, &ch::easeOutQuad, 1040ms}))
                   .foreground(shapes::onEdges(shapes::Edge::Top, hairline)));
  }
};

SIGIL_SKETCH(KineticStudy)
