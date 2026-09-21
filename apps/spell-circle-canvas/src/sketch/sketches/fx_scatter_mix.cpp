/** @file
 * fx_scatter_mix — two composed glyph effects, and the three
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
 * scale and alpha MULTIPLY. It is not a sequence — `fx::sequence` re-clocks
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

// TAGS: Typography/Effects, Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 700};

constexpr float kProgress = 0.5f;  // where the photograph is taken
constexpr float kRadius = 34;      // the scatter's disc, px
constexpr float kLean = 26;        // …and its lean, degrees
constexpr float kEach = 60;        // per-unit spacing, ms
constexpr float kDuration = 420;   // one unit's own motion, ms

constexpr SkColor4f kHot{0.95f, 0.36f, 0.28f,
                         1};  // what the mixed tint wipes FROM

weave::TextStyle specimen() {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  return weave::textStyle({.face = face,
                           .size = 34,
                           .color = sketch::kit::theme().palette.figure,
                           .track = 1});
}

Element figure(float width, const char* key, Track track) {
  track.progress = kProgress;
  return sketch::kit::well({.width = width, .height = 146})
      .children({text("CASCADE", specimen())
                     .key(key)
                     .width(width - 88)
                     .absolute()
                     .inset(48, 44, 20, 44)
                     .fx(std::move(track))});
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

struct FxScatterMix {
  bool instrumented = false;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    instrumented = false;
    ctx.composer.render(describe(ctx));
    // Request frames only until layout can supply the static beat meters.
    ctx.ticker.add([this]() -> bool { return !instrumented; });
  }

  void update(double, sketch::SketchContext& ctx) {
    if (instrumented || ctx.composer.beatsOf("sc", 0).empty()) return;
    ctx.composer.render(describe(ctx));
    instrumented = true;
  }

  Element describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    const SkColor4f ink = sketch::kit::theme().palette.figure;

    Element page = sketch::kit::page(
        {.title = "Change the effect, or change who starts",
         .subtitle = "The same word at 50% master progress · 60 ms between "
                     "glyphs, 420 ms for each motion",
         .footer =
             "Meters read each glyph's actual local time. mix runs effects "
             "together: offsets and rotation add; scale and alpha multiply."},
        box().column().gap(22).children(
            {document::h2("01 / ONE SCHEDULE, TWO EFFECTS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SCATTER",
                        .control = "radius 34 px · lean 26° · from start",
                        .figure = figure(
                            498, "sc",
                            {.effect = fx::scatter(kRadius, kLean),
                             .stagger = ladder(motion::Spread::From::Start)}),
                        .note = "Each glyph gets a stable, seeded offset and "
                                "lean."},
                       {.title = "SCATTER + TINT",
                        .control = "mix(scatter, tint) · same local time",
                        .figure = figure(
                            498, "mx",
                            {.effect = fx::mix(fx::scatter(kRadius, kLean),
                                               fx::tint(kHot, ink)),
                             .stagger = ladder(motion::Spread::From::Start)}),
                        .note = "Position follows scatter while colour "
                                "changes alongside it."}},
                  .measure = 1020,
                  .gap = 24}),
             document::h2("02 / ONE EFFECT, THREE SCHEDULES"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "FROM THE END",
                        .control = "From::End",
                        .figure = figure(328, "en",
                                         {.effect = fx::scatter(kRadius, kLean),
                                          .stagger = ladder(
                                              motion::Spread::From::End)}),
                        .note = "The final glyph starts first; the cascade "
                                "travels backward."},
                       {.title = "FROM BOTH EDGES",
                        .control = "From::Edges",
                        .figure = figure(
                            328, "ed",
                            {.effect = fx::scatter(kRadius, kLean),
                             .stagger = ladder(motion::Spread::From::Edges)}),
                        .note = "Both ends arrive together. The centre "
                                "remains in flight."},
                       {.title = "EASE THE DELAYS",
                        .control = "From::Start · distribution t²",
                        .figure = figure(
                            328, "di",
                            {.effect = fx::scatter(kRadius, kLean),
                             .stagger = ladder(motion::Spread::From::Start,
                                               [](float t) { return t * t; })}),
                        .note = "Only the delay spacing changes; each glyph "
                                "keeps the same motion."}},
                  .measure = 1020,
                  .gap = 18})}));
    Element root = stack().inset(0).children({std::move(page)});
    for (const char* key : {"sc", "mx", "en", "ed", "di"})
      root.children(
          {kit::trackMeter(ctx.composer, key, 0,
                           sketch::kit::theme().palette.figure,
                           {0.4f, 0.4f, 0.5f, 0.2f},
                           {.where = kit::MeterPlacement::Where::Under,
                            .thickness = 3,
                            .gap = 12,
                            .trim = 1.5f})
               .inset(0)});
    return root;
  }
};

SIGIL_SKETCH(FxScatterMix, "Kit · API",
             "one word photographed half way through a scatter, then mixed "
             "with a tint, then run from the end, from both edges, and "
             "through an eased distribution")
