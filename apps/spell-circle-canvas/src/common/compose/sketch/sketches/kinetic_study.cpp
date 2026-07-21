// kinetic_study.cpp — contemporary kinetic typography study (REFERENCES.md §8,
// "the community grammar"). A motion title card cycling the canonical moves
// with the researched parameters:
//
//   hero ....... staggered MaskedRise — glyphfx::rise under line-box clips,
//                chars 30ms each / 500ms dur (the fluid band), master
//                progress a LINEAR timeline ramp sized to the virtual span
//                (distribution ease belongs to start times, not motion);
//                lines staggered 0.12s, rise distance 1.05 x line-height
//   subline .... CharPop — glyphfx::pop back.out(1.7) from 'start'; alpha
//                completes early (composition law: opaque while moving,
//                never overshoot alpha); entrance total kept <= 1.2s budget
//   wave ....... WaveFloat loop — glyphfx::waveLoop bound to a WRAPPING
//                phase Output (steppable t += dt, phase = fract(t / 1.6));
//                dy = 0.1em * sin(2*pi*t/1.6 - 0.5i)  ->  amplitude 0.1em,
//                phasePerGlyph 0.5/(2*pi) ~= 0.08; loops run under the
//                entrance amplitude (60-80% law)
//   ticker ..... seamless strip — duplicated text row, translateX bound to
//                -(v*t mod stripW), v = 90 px/s, LINEAR always (never eased,
//                never hard-stopped); stripW measured live via
//                composer.bounds() on the first unit
//
// Headless captures:
//   ComposeSketch <this> --frame kinetic_mid.png --at 0.5   (reveal mid-flight)
//   ComposeSketch <this> --frame kinetic.png     --at 3.0   (settled + loops)

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Kinetic.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Shapes.h>

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
constexpr float kHeroSize = 116;
constexpr float kTickerH = 48;
constexpr float kTickerSpeed = 90;   // px/s — the 50-120 circulating band
constexpr float kTickerGap = 56;     // built into the unit via padding-right
constexpr float kWavePeriod = 1.6f;  // the WaveFloat community constant

} // namespace

struct KineticStudy : sketch::Sketch {
  // Master progresses for the one-shot moves (LINEAR ramps — per-glyph
  // easing lives inside the effects, GSAP semantics).
  ch::Output<float> heroA{0}, heroB{0}, popP{0};
  // Wrapping phase for the loop + the ticker offset.
  ch::Output<float> wavePhase{0}, tickX{0};
  // Chrome fades.
  ch::Output<float> metaFade{0}, waveFade{0}, tickFade{0};

  float stripW = 0; // measured from composer.bounds("tickunit") once painted

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kInk);

    auto &tl = ctx.ticker.timeline();

    // Hero MaskedRise: "KINETIC" = 7 glyphs -> virtual span 500 + 30*6 =
    // 680ms. A linear master ramp of exactly that length plays the
    // researched per-glyph timings in real time. Lines each 0.12s.
    heroA = 0.0f;
    heroB = 0.0f;
    tl.apply(&heroA).then<ch::Hold>(0.0f, 0.10f).then<ch::RampTo>(
        1.0f, 0.68f, &ch::easeNone);
    tl.apply(&heroB).then<ch::Hold>(0.0f, 0.22f).then<ch::RampTo>(
        1.0f, 0.68f, &ch::easeNone);

    // CharPop subline: 26 glyphs at 30ms each + 420ms dur = 1.17s -- inside
    // the <=1.2s paragraph entrance budget.
    popP = 0.0f;
    tl.apply(&popP).then<ch::Hold>(0.0f, 0.42f).then<ch::RampTo>(
        1.0f, 1.17f, &ch::easeNone);

    metaFade = 0.0f;
    tl.apply(&metaFade).then<ch::RampTo>(1.0f, 0.45f, &ch::easeOutQuad);
    waveFade = 0.0f;
    tl.apply(&waveFade).then<ch::Hold>(0.0f, 0.85f).then<ch::RampTo>(
        1.0f, 0.55f, &ch::easeOutQuad);
    tickFade = 0.0f;
    tl.apply(&tickFade).then<ch::Hold>(0.0f, 1.05f).then<ch::RampTo>(
        1.0f, 0.50f, &ch::easeOutQuad);

    // The loop clock: wrapping wave phase + the LINEAR ticker offset.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      wavePhase = (float)std::fmod(t / kWavePeriod, 1.0);
      if (stripW > 1)
        tickX = -(float)std::fmod(t * kTickerSpeed, (double)stripW);
      return true;
    });

    ctx.composer.render(describe());
  }

  Element describe() {
    Material ground = Material::linear(
        {0, 0}, {0, kH},
        {{0.00f, kInk}, {0.62f, kInkLift}, {1.00f, kInkFoot}});

    // -- hero: staggered rise under a line-box clip (the MaskedRise read) --
    auto heroLine = [&](const char *s, const char *key,
                        const ch::Output<float> *progress) {
      GlyphFx fx;
      // 1.05 x line-height below rest, chars 30ms each / 500ms dur.
      fx.effect = glyphfx::rise(kHeroSize * 1.26f);
      fx.stagger = {.eachMs = 30, .durationMs = 500};
      fx.progress = progress;
      return box().clip() // the line-box mask
          .child(text(toU8(s), type(kHeroSize, kBone, 2))
                     .key(key)
                     .glyphFx(std::move(fx)));
    };

    // -- subline: CharPop from 'start' ------------------------------------
    GlyphFx popFx;
    popFx.effect = glyphfx::pop(0.35f, 1.70158f); // back.out(1.7)
    popFx.stagger = {.eachMs = 30, .durationMs = 420,
                     .from = Stagger::From::Start};
    popFx.progress = &popP;

    // -- wave line: endless float on the wrapping phase --------------------
    GlyphFx waveFx;
    // amplitude 0.1em of the 19px face; phasePerGlyph 0.5 rad / 2*pi ~= 0.08.
    waveFx.effect = glyphfx::waveLoop(1.9f, 0.08f);
    waveFx.stagger = {.eachMs = 0, .durationMs = 450}; // one master phase
    waveFx.progress = &wavePhase;

    // -- ticker: duplicated units, hairline above, LINEAR drift ------------
    const char *unit = "EASE-OUT-EXPO 0.16 / 1 / 0.3 / 1   ●   STAGGER "
                       "30 MS PER CHAR   ●   BACK.OUT(1.7)   ●   "
                       "WAVE 1.6 S   ●   90 PX/S LINEAR";
    auto tickerUnit = [&](const char *key) {
      return box().row().shrink(0).padding(0, 0, kTickerGap, 0).key(key)
          .child(text(toU8(unit), type(15, kAsh, 2)).shrink(0));
    };
    PathFormat hairline;
    hairline.width = 1;
    hairline.strokeFill = Fill::color({0.93f, 0.92f, 0.89f, 0.22f});

    return stack()
        .fill(ground)
        // the editorial column
        .child(
            box().column().absolute().inset(64, 44, 64, kTickerH)
                .child(box().row().alignItems(Align::Baseline)
                           .opacity(&metaFade)
                           .child(text(toU8("SIGIL — MOTION STUDY"),
                                       type(15, kAsh, 3)))
                           .child(box().grow(1))
                           .child(text(toU8("REFERENCES §8"),
                                       type(15, kAccent, 3))))
                .child(box().grow(1))
                .child(heroLine("KINETIC", "hero1", &heroA))
                .child(heroLine("GRAMMAR", "hero2", &heroB))
                .child(text(toU8("STAGGERED · POPPED · WAVED"),
                            type(26, kAccent, 4))
                           .key("popline")
                           .glyphFx(std::move(popFx))
                           .margin(0, 20, 0, 0))
                .child(text(toU8("floating on a 1.6 s sine — amplitude "
                                 "0.10 em · phase 0.5 rad per glyph"),
                            type(19, kAsh, 1))
                           .key("waveline")
                           .glyphFx(std::move(waveFx))
                           .opacity(&waveFade)
                           .margin(0, 18, 0, 34)))
        // the seamless ticker strip
        .child(
            box().absolute().inset(0, kH - kTickerH, 0, 0).clip()
                .opacity(&tickFade)
                .foreground(shapes::onEdges(shapes::Edge::Top, hairline))
                .child(box().row().absolute().inset(0, 0, -4200, 0)
                           .alignItems(Align::Center)
                           .translateX(&tickX)
                           .child(tickerUnit("tickunit"))
                           .child(tickerUnit("tick1"))
                           .child(tickerUnit("tick2"))));
  }

  void update(double, sketch::SketchContext &ctx) override {
    // Measure the ticker unit once it has painted; the steppable starts
    // wrapping the offset as soon as stripW lands.
    if (stripW <= 1) {
      if (auto b = ctx.composer.bounds("tickunit"))
        stripW = b->width();
    }
  }
};

SIGIL_SKETCH(KineticStudy)
