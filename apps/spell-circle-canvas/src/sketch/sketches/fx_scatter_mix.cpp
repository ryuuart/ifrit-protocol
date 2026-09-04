/** @file
 * fx_scatter_mix — two glyph effects nobody had drawn, and the three
 * schedule dials that decide who moves first.
 *
 * `fx::scatter` flies every glyph in from its own random offset inside a
 * disc, with its own random lean. The stream is seeded from the GLYPH'S
 * IDENTITY rather than from the frame, so the draw is stable across
 * frames and relayouts — which is what lets a settled scatter cache
 * instead of jittering forever.
 *
 * `fx::mix` evaluates every operand at the same local t and composes the
 * results by the algebra stacked tracks use: dx, dy and rotation ADD,
 * scale and alpha MULTIPLY. It is not a sequence — `fx::seq` re-clocks
 * its phases over windows, this one runs them all at once — and it is
 * comparable when its operands are, so a mixed track prunes like any
 * other.
 *
 * The other three cells change nothing about the effect and only WHO GETS
 * A BEAT WHEN. `Spread::From` picks the origin — Start, Center, End,
 * Random or Edges, where Edges starts at both ends and meets in the
 * middle — and `distribution` passes the linear ramp of delays through a
 * curve, so an ease-in crowds the early units together and lets the tail
 * spread out. The per-unit motion is untouched by it.
 *
 * Every cell holds one constant progress, so the sheet is a photograph of
 * one instant of the cascade and the ladder is legible as a ladder.
 *
 * EDIT THESE FIRST
 *   kProgress — where in the master 0→1 the photograph is taken.
 *   kRadius, kLean — the scatter's disc and its lean.
 *   kEach, kDuration — the spread's per-unit spacing and motion, ms.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 200;
constexpr float kPicture = 176;

constexpr float kProgress = 0.5f;  // where the photograph is taken
constexpr float kRadius = 34;      // the scatter's disc, px
constexpr float kLean = 26;        // …and its lean, degrees
constexpr float kEach = 60;        // per-unit spacing, ms
constexpr float kDuration = 420;   // one unit's own motion, ms

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kHot{0.95f, 0.36f, 0.28f,
                         1};  // what the mixed tint wipes FROM
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle specimen() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  return weave::textStyle(
      {.face = face, .size = 27, .color = kFigure, .track = 1.5f});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

Element cell(const char* call, const char* note, const char* key, Track track) {
  track.progress = kProgress;
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround)})
                       .child(text(toU8("DISPLACEMENT"), specimen())
                                  .key(key)
                                  .width(Dim(kCell - 28))
                                  .absolute()
                                  .inset(14, 60, 14, 14)
                                  .fx(std::move(track))));
}

/** The one spread every cell starts from — the origin and the
 *  distribution are the only fields the cells change. */
motion::Spread ladder(motion::Spread::From from,
                      choreograph::EaseFn distribution = nullptr) {
  return motion::Spread{.eachMs = kEach,
                        .durationMs = kDuration,
                        .from = from,
                        .distribution = std::move(distribution)};
}

}  // namespace

struct FxScatterMix final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    // Every track holds one constant progress: the sheet is one instant
    // of the cascade, not a moment of an animation.
    ctx.captureAt(0.05);

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("SCATTER, MIX AND THE LADDER \xc2\xb7 fx::"
                           "scatter, fx::mix, Spread::from, distribution"),
             .subtitle = toU8("dials \xc2\xb7 the progress the photograph is "
                              "taken at (0.50) \xc2\xb7 the scatter's radius "
                              "(34 px) and lean (26\xc2\xb0) \xc2\xb7 the "
                              "origin \xc2\xb7 the distribution curve"),
             .footer = toU8("mix composes by the algebra stacked tracks use "
                            "\xe2\x80\x94 dx, dy and rotation add, scale and "
                            "alpha multiply \xe2\x80\x94 and the scatter's "
                            "randomness is seeded from each glyph's own "
                            "identity, so it is the same scatter every "
                            "frame"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("fx::scatter(34, 26)",
                           "each glyph flies in from its own offset in a "
                           "disc, with its own lean \xc2\xb7 From::Start, so "
                           "the head has landed",
                           "sc",
                           {.effect = fx::scatter(kRadius, kLean),
                            .stagger = ladder(motion::Spread::From::Start)}),
                      cell("fx::mix(scatter, tint)",
                           "both at once at one local t \xc2\xb7 the "
                           "offsets are the scatter's and the colour the "
                           "tint's, composed and not sequenced",
                           "mx",
                           {.effect = fx::mix(fx::scatter(kRadius, kLean),
                                              fx::tint(kHot, kFigure)),
                            .stagger = ladder(motion::Spread::From::Start)}),
                      cell("Spread::From::End",
                           "the same effect, the cascade run backwards "
                           "\xc2\xb7 the LAST glyph is the one that has "
                           "landed",
                           "en",
                           {.effect = fx::scatter(kRadius, kLean),
                            .stagger = ladder(motion::Spread::From::End)}),
                      cell("Spread::From::Edges",
                           "both ends start together and meet in the middle "
                           "\xc2\xb7 the centre of the word is still in "
                           "flight",
                           "ed",
                           {.effect = fx::scatter(kRadius, kLean),
                            .stagger = ladder(motion::Spread::From::Edges)}),
                      cell("\xe2\x80\xa6"
                           ", .distribution = t\xc2\xb2",
                           "the ramp of DELAYS passed through a curve "
                           "\xc2\xb7 an ease-in crowds the early units and "
                           "lets the tail spread out",
                           "di",
                           {.effect = fx::scatter(kRadius, kLean),
                            .stagger = ladder(motion::Spread::From::Start,
                                              [](float t) { return t * t; })})},
                 .gap = 12}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(FxScatterMix, "Kit \xc2\xb7 API",
             "one word photographed half way through a scatter, then mixed "
             "with a tint, then run from the end, from both edges, and "
             "through an eased distribution")
