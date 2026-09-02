/** @file
 * kinetic card — a title card in the vocabulary motion designers
 * actually use: one dominant move the eye follows, everything else
 * supporting it, and stagger budgeted as a total rather than a per-glyph
 * delay.
 */

// A kinetic-typography card, written in the vocabulary motion designers
// actually use for title sequences and lyric videos: one dominant move that
// the eye follows, everything else supporting it, and stagger budgeted as a
// total duration rather than a per-glyph delay.
//
// The whole card is MOUNT choreography — animate(from().to()) values that run
// once when the node mounts — assembled from:
//  - a masked rise on the hero lines, the dominant move
//  - amount-mode stagger, so adding glyphs shortens each glyph's delay
//    instead of lengthening the whole entrance
//  - Transition delays to sequence the elements against each other
//  - a rule that draws itself on, with a wrapping comet riding it
//  - a per-character pop on the subline, under a double text glow
//  - a looping sine float, and a marquee ticker along the bottom edge
//
// Loop phases are Outputs driven from a ticker lambda and re-zeroed in
// setup(), because a scene can be activated more than once.
//
// THE ENTRANCES OVERLAP ON PURPOSE. Sequenced end to end, every element
// is either not yet in or already landed at any one instant, and a still
// of a piece of choreography then shows no choreography. The delays here
// are set so the hero lines are still rising while the pop is mid-cascade
// and the ticker is fading in, which is also what the declared moment
// photographs.
//
// EDIT THESE FIRST
//   kHeroSize          — the dominant move's size; the masked rise travels
//                        1.26 of it.
//   heroStagger        — amountMs is the whole cascade's spread and
//                        durationMs one glyph's own rise, which is what
//                        amount mode means.
//   popStagger / fx::pop — the subline's overshoot: 0.35 of the glyph's
//                        size, backed out at 1.70158.
//   kTickerSpeed       — the crawl, in px/s. 50 to 120 stays readable.

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
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
constexpr SkSize kSceneSize = {900, 640};

namespace kinetic_card {

// ---- restrained editorial palette: bone + ash + ONE accent ----------------
constexpr SkColor4f kInk{0.043f, 0.043f, 0.058f, 1};
constexpr SkColor4f kInkLift{0.058f, 0.053f, 0.072f, 1};
constexpr SkColor4f kInkFoot{0.086f, 0.063f, 0.066f, 1};
constexpr SkColor4f kBone{0.930f, 0.920f, 0.890f, 1};
constexpr SkColor4f kAsh{0.540f, 0.540f, 0.590f, 1};
constexpr SkColor4f kAccent{0.980f, 0.360f, 0.250f, 1};

/** This study's type colour reaches the paint as 8-bit sRGB, so a tint
 *  computed per frame lands on the same 256-step ladder as a quoted one.
 *  `compose::type` carries the float through instead, and the device
 *  raster resolves the two differently. */
inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0) {
  return sigil::compose::type(
      {.size = size, .color = color, .track = tracking, .color8 = true});
}

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kHeroSize = 112;
constexpr float kTickerH = 48;
constexpr float kTickerSpeed = 90;    // px/s — readable-crawl range is 50-120
constexpr float kTickerGap = 56;      // between the two marquee copies
constexpr float kWavePeriod = 1.6f;   // seconds per sine float cycle
constexpr float kCometPeriod = 2.8f;  // seconds per comet lap of the rule
constexpr float kRuleW = 380;
constexpr float kFadeW = 64;  // the ticker's dissolve at each frame edge

/// The same colour at zero alpha, for a ramp that fades to the ground.
constexpr SkColor4f transparent(SkColor4f c) { return {c.fR, c.fG, c.fB, 0}; }

}  // namespace kinetic_card

struct KineticCard final : sketch::Sketch {
  choreograph::Output<float> wavePhase{0}, cometPhase{0}, tickX{0};
  float unitW = 0;    // ticker content's intrinsic width (compose::measure)
  float wrapLen = 1;  // marquee wrap length = unitW + gap

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    // MID-ENTRANCE. The piece is choreography, and a still taken after
    // the choreography has landed shows none of it: at 0.78 s the first
    // hero line is settling while the second is still rising out of its
    // mask, the per-character pop is a third of the way through its
    // cascade, and the rule is still drawing itself on.
    ctx.captureAt(0.78);
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    namespace kc = kinetic_card;
    wavePhase = 0;
    cometPhase = 0;
    tickX = 0;

    const SkSize unit = ctx.measure(tickerContent());
    unitW = std::ceil(unit.width());
    wrapLen = unitW + kc::kTickerGap;

    ticker.add([this, t = 0.0](double dt) mutable {
      namespace kc = kinetic_card;
      t += dt;
      wavePhase = motion::phase(t, kc::kWavePeriod);
      cometPhase = motion::phase(t, kc::kCometPeriod);
      tickX = -wrapLen * motion::phase(t * kc::kTickerSpeed / wrapLen, 1.0);
      return true;
    });

    composer.render(describe());
  }

  Element tickerContent() {
    namespace kc = kinetic_card;
    const char* unit =
        "WITHFROM MOUNT CHOREOGRAPHY   \xe2\x97\x8f   "
        "EASE-OUT-EXPO 0.16 / 1 / 0.3 / 1   \xe2\x97\x8f   "
        "AMOUNT-MODE 700 MS   \xe2\x97\x8f   BACK.OUT(1.7)   "
        "\xe2\x97\x8f   WAVE 1.6 S \xc2\xb7 0.10 EM   "
        "\xe2\x97\x8f   TRIM WRAP COMET   \xe2\x97\x8f   "
        "90 PX/S LINEAR";
    Element content =
        box()
            .row()
            .alignItems(Align::Center)
            .height(Dim(kc::kTickerH))
            .child(text(toU8(unit), kc::type(15, kc::kAsh, 2)).shrink(0));
    if (unitW > 0) content.width(Dim(unitW)).shrink(0);
    return content;
  }

  Element describe() {
    namespace kc = kinetic_card;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    Material ground = Material::linear(
        {0, 0}, {0, kc::kH},
        {{0.00f, kc::kInk}, {0.62f, kc::kInkLift}, {1.00f, kc::kInkFoot}});

    // Each entrance's progress duration is its cascade's own span, so the
    // last glyph lands exactly as the master arrives at 1. Both cascades
    // are amount-mode, whose span is `durationMs + amountMs` for every
    // unit count past one — that count-independence is the point of the
    // mode, so the spelled count is nominal.
    const Stagger heroStagger{.amountMs = 180, .durationMs = 500};
    const auto spanOf = [](const Stagger& s) {
      return std::chrono::milliseconds(std::lround(s.spanMs(2)));
    };
    auto heroLine = [&](const char* s, const char* key, int delayMs) {
      return box()
          .clip()  // the line-box mask
          .child(text(toU8(s), kc::type(kc::kHeroSize, kc::kBone, 2))
                     .key(key)
                     .width(pct(100))
                     .textAlign(sigil::weave::TextAlignment::kCenter)
                     .fx({.effect = fx::rise(kc::kHeroSize * 1.26f),
                          .stagger = heroStagger,
                          .progress =
                              animate(from(0.0f).to(1.0f),
                                      {spanOf(heroStagger), &ch::easeNone,
                                       std::chrono::milliseconds(delayMs)})}));
    };

    const Stagger popStagger{
        .amountMs = 700, .durationMs = 420, .from = Stagger::From::Center};
    Track popFx{
        .effect = fx::pop(0.35f, 1.70158f),  // back.out(1.7)
        .stagger = popStagger,
        .progress = animate(from(0.0f).to(1.0f),
                            {spanOf(popStagger), &ch::easeNone, 450ms})};

    // Amplitude in em, phase offset per glyph in radians.
    Track waveFx{.effect = fx::waveLoop(0.10f, 0.5f),
                 .stagger = {.eachMs = 0, .durationMs = 450},  // one phase
                 .progress = &wavePhase};

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
        box()
            .width(pct(100))
            .padding(0, 32)
            .margin(0, -6, 0, -32)
            .cache(Cache::Texture)
            .child(text(toU8("STAGGERED \xc2\xb7 POPPED \xc2\xb7 WAVED"),
                        kc::type(26, kc::kAccent, 4))
                       .key("popline")
                       .width(pct(100))
                       .textAlign(sigil::weave::TextAlignment::kCenter)
                       .fx(std::move(popFx))
                       .effect(glow));

    return stack()
        .fill(ground)
        .child(
            box()
                .column()
                .inset(64, 44, 64, kc::kTickerH)
                .child(box()
                           .row()
                           .alignItems(Align::Baseline)
                           .opacity(animate(from(0.0f).to(1.0f),
                                            {450ms, &ch::easeOutQuad}))
                           .translateY(animate(from(-14.0f).to(0.0f),
                                               {450ms, &ch::easeOutCubic}))
                           .child(box()
                                      .width(Dim(10.0f))
                                      .height(Dim(10.0f))
                                      .alignSelf(Align::Center)
                                      .margin(0, 0, 12, 0)
                                      .fill(Animatable<Fill>(animate(
                                          from(Fill::color(kc::kBone))
                                              .to(Fill::color(kc::kAccent)),
                                          {900ms, &ch::easeOutQuad}))))
                           .child(text(toU8("SIGIL \xe2\x80\x94 MOTION STUDY"),
                                       kc::type(15, kc::kAsh, 3)))
                           .child(box().grow(1))
                           .child(text(toU8("ENTRANCES \xe2\x80\x94 RISE "
                                            "\xc2\xb7 POP \xc2\xb7 WAVE"),
                                       kc::type(15, kc::kAccent, 3))))
                .child(box().grow(1))
                .child(heroLine("KINETIC", "hero1", 100))
                .child(heroLine("GRAMMAR", "hero2", 220))
                .child(box()
                           .width(Dim(kc::kRuleW))
                           .height(Dim(2.0f))
                           .alignSelf(Align::Center)
                           .margin(0, 30, 0, 0)
                           .shape(lineOutline)
                           .stroke(spans::upTo(animate(
                                       from(0.0f).to(1.0f),
                                       {500ms, &ch::easeOutExpo, 360ms})),
                                   ruleFmt)
                           .child(box()
                                      .inset(0, 0, 0, 0)
                                      .shape(lineOutline)
                                      .stroke(spans::wrap(0.0f, 0.06f)
                                                  .offset(&cometPhase),
                                              cometFmt)
                                      .opacity(animate(
                                          from(0.0f).to(1.0f),
                                          {500ms, &ch::easeOutQuad, 1500ms}))))
                .child(std::move(popLine))
                .child(text(toU8("floating on a 1.6 s sine \xe2\x80\x94 "
                                 "amplitude 0.10 em \xc2\xb7 phase 0.5 rad "
                                 "per glyph"),
                            kc::type(19, kc::kAsh, 1))
                           .key("waveline")
                           .width(pct(100))
                           .textAlign(sigil::weave::TextAlignment::kCenter)
                           .fx(std::move(waveFx))
                           .opacity(animate(from(0.0f).to(1.0f),
                                            {560ms, &ch::easeOutQuad, 300ms}))
                           .margin(0, 22, 0, 0))
                .child(box().grow(1)))
        .child(marquee(tickerContent(), &tickX, kc::kTickerGap)
                   .inset(0, kc::kH - kc::kTickerH, 0, 0)
                   .opacity(animate(from(0.0f).to(1.0f),
                                    {560ms, &ch::easeOutQuad, 380ms}))
                   .foreground(shapes::onEdges(shapes::Edge::Top, hairline)))
        // A ticker CUT at the frame reads as a broken caption; a ticker
        // that dissolves into the edge reads as running past it. Two
        // ramps in the ground's own foot colour, over each end.
        .child(box()
                   .inset(0, kc::kH - kc::kTickerH + 1, kc::kW - kc::kFadeW, 0)
                   .fill(Material::linear(
                       {0, 0}, {kc::kFadeW, 0},
                       {{0.0f, kc::kInkFoot},
                        {1.0f, kc::transparent(kc::kInkFoot)}})))
        .child(
            box()
                .inset(kc::kW - kc::kFadeW, kc::kH - kc::kTickerH + 1, 0, 0)
                .fill(Material::linear({0, 0}, {kc::kFadeW, 0},
                                       {{0.0f, kc::transparent(kc::kInkFoot)},
                                        {1.0f, kc::kInkFoot}})));
  }
};

}  // namespace

SIGIL_SKETCH_AS(KineticCard, "kinetic card", "Catalog \xc2\xb7 Type & grid",
                "kinetic type grammar")
