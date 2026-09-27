// ember_decode.cpp — ONE ROUTE: a shader per letter, without a shader per
// letter.
// =============================================================================
// "Each letter phases in under its own SkSL" sounds like one runtime effect
// per glyph. It is not: a glyph is not a draw the author owns — the text
// engine batches a line into one drawGlyphsRSXform call per (font, paint
// pass), and cutting that up to give every letter its own paint is the one
// thing that makes typography expensive.
//
// THE ROUTE. The line is rendered ONCE into a layer, and ONE SkSL pass reads
// that layer as a sampler. Per-pixel, the shader asks which UNIT it is
// standing in — a letter on the display line, a word on the small one — and
// burns that unit in on that unit's own clock. All of it is ONE DECLARATION:
//
//     text(u8"EMBER DECODE", display)
//         .textFx({.effect = textFx::pass(burn),
//              .stagger = stagger(weave::Unit::Cluster, {.eachMs = 260})});
//
// `textFx::pass` makes the track's effect a PASS rather than a per-glyph
// deviation. The runtime renders the track's units into a layer, hands it to
// the material as `uContent`, and hands the track's own schedule as uniform
// data — `uUnitRect[N]` (each unit's box, node-local px) and `uUnitPhase[N]`
// (x: that unit's cascade-local 0→1, y: its stable seed), with N baked into
// the compiled source (`kUnitCount`) and one variant cached per distinct
// count. So the cost is one draw and one pass over the line's own box,
// whatever N is, and "per letter" is uniform data rather than scene
// structure. The dissolve threshold is noise plus a left-to-right bias
// inside each unit, so the letter is eaten from seeded edges; the band where
// the threshold is being crossed is drawn hot, which is the ember rim.
//
// WHAT THE ENGINE SUPPLIES:
//  - the unit boxes and per-unit clocks arrive as `uUnitRect`/`uUnitPhase`,
//    resolved from the SAME cascade `Composer::beatsOf` reports — the meter
//    bars under the display line are drawn from that query, so the bars and
//    the burn read one schedule by construction;
//  - the material is `material::Paint::recipe(...)` over a SigilMaterial recipe,
//    and the runtime owns the per-count specialization and its cache;
//  - the layer is sampled at the device's resolution, so a 2x host stays
//    sharp with no supersampled bake;
//  - the pass is BOUNDED to the node's box plus the track's reach, unlike a
//    raw Element::filter shader pass.
//
// WHAT REMAINS OUTSIDE THE ENGINE, stated because it shaped this file: a
// cascade opens each unit once — there is no "in, hold, burn off" as one
// stagger — so the loop below drives the track's PROGRESS up, holds it, and
// drives it back DOWN, which replays the cascade in reverse and burns the
// line off right to left (the last unit to arrive is the first to lose
// progress). And a pass is a whole-track statement: textFx::sequence/mix/hold
// do not consult it, so a pass that wants phases writes them in its own SkSL.
//
// EDIT THESE FIRST
//   kEachMs   — start-to-start between units. 0 decodes the whole line at
//               once and the per-unit uniforms stop being visible; past
//               kUnitMs the line reads one letter at a time.
//   kSpeckle / kPatch / kSweep — the three weights of the dissolve
//               threshold. They sum to 1. All on kSweep is a clean
//               left-to-right wipe with no burn; all on kSpeckle is
//               television static.
//
// Run:
//   ./build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
//       src/sketch/sketches/ember_decode/ember_decode.cpp \
//       --frame /tmp/ember_decode.png

// TAGS: Typography/Effects, Materials/Shaders

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Meter.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kW = 1000.0f;
constexpr float kH = 500.0f;

// ---- the cycle -------------------------------------------------------------
constexpr double kLoop = 9.6;     // one full decode + hold + burn-off
constexpr double kInAt = 0.35;    // the display line starts here
constexpr double kWordsAt = 1.4;  // the small line runs its own clock, late
constexpr double kOutAt = 7.30;   // progress runs back down from here
constexpr double kOutSecs = 1.6;  // …over this long (reversed cascade)

// ---- the cascade -----------------------------------------------------------
constexpr float kEachMs = 260;   // start-to-start, left to right
constexpr float kUnitMs = 1000;  // one unit's own 0 -> 1

// ---- the burn --------------------------------------------------------------
constexpr float kSweep = 0.28f;  // the three threshold weights: they sum to 1
constexpr float kSpeckle = 0.30f;
constexpr float kPatch = 0.42f;

const material::Color kPlate{0.027f, 0.024f, 0.031f, 1};
const material::Color kInk{0.96f, 0.91f, 0.82f, 1};    // the resolved letter
const material::Color kEmber{1.00f, 0.47f, 0.13f, 1};  // the crossing band
const material::Color kLabel{0.62f, 0.55f, 0.50f, 1};
const material::Color kFaint{0.66f, 0.60f, 0.55f, 1};

// ---------------------------------------------------------------------------
// The pass

/** The burn's ABI. The three weights are one array rather than three
 *  floats because they are read as a set and the body indexes them. */
struct BurnParameters {
  sigil::material::Color uInk;
  sigil::material::Color uEmber;
  std::array<float, 3> uWeights;  // sweep, speckle, patch
};

/** THE DEFINITION, made once and held by whoever draws with it: a recipe's
 *  identity IS the object, so a fresh one per describe compiles a fresh
 *  program and never compares equal to itself. The sketch holds it, rather
 *  than a static in this dylib, which a reload unloads. @p body is the pass,
 *  `burn.sksl` beside this file. */
std::shared_ptr<const sigil::material::Recipe> burnRecipe(std::string body) {
  return std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<BurnParameters>("ember.burn")
          .body(sigil::material::Target::SkSL, std::move(body)));
}

material::Paint burnMaterial(
    const std::shared_ptr<const sigil::material::Recipe>& recipe) {
  return material::Paint::recipe(sigil::material::Material(recipe))
      .set("uInk", kInk)
      .set("uEmber", kEmber)
      .set("uWeights", std::vector<float>{kSweep, kSpeckle, kPatch});
}

/** One track's master progress across the loop: a linear ramp up from
 *  @p startAt (so `master * totalMs` advances at wall speed and each unit
 *  crosses its own beat exactly as the cascade schedules it), a hold at 1,
 *  then a faster ramp DOWN from kOutAt — reversing progress replays the
 *  cascade backwards, so the burn-off runs right to left. */
float masterAt(double t, double startAt, float totalMs) {
  const double up = std::clamp((t - startAt) * 1000.0 / totalMs, 0.0, 1.0);
  const double down = std::clamp((t - kOutAt) / kOutSecs, 0.0, 1.0);
  return (float)(up * (1.0 - down));
}

}  // namespace

// ===========================================================================

struct EmberDecode {
  std::shared_ptr<const sigil::material::Recipe> recipe;
  sigil::motion::Animatable<float> display = sigil::motion::animatable(0.0f), words = sigil::motion::animatable(0.0f);
  float displayTotalMs = 1;  // the cascades' spans, read back from beatsOf
  float wordsTotalMs = 1;

  Element describe(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    const sk_sp<SkTypeface> face =
        weave::ports::face({"Helvetica Neue", "Arial", "Inter"}, 700);
    // The letters are set WHITE: the pass reads the layer's coverage and
    // supplies every colour itself, so the type's own colour never lands.
    const auto burnt = [&](float size, float track) {
      return weave::Type{.face = face,
                         .size = size,
                         .color = material::Color{1, 1, 1, 1},
                         .track = track};
    };
    const material::Paint burn = burnMaterial(recipe);

    // THE SCHEDULE, DRAWN, from the same query the pass agrees with: one
    // meter per beat of the display track, at that beat's laid-out rect,
    // filled to that beat's local time — no restated i * eachMs anywhere.
    // beatsOf answers in the composer's space and the overlay spans the root
    // from its origin, so the two frames line up.
    const std::vector<Beat> beats = ctx.composer.beatsOf("burn-display", 0);
    const auto readBack = [](const Beat& beat, std::size_t i) {
      return sketch::kit::meter({.fraction = beat.localProgress,
                                 .width = Dimension(beat.rect.width()),
                                 .height = Dimension(3),
                                 .track = Fill::color(kFaint),
                                 .bar = Fill::color(kEmber)})
          .key("beat" + std::to_string(i))
          .at({beat.rect.left(), beat.rect.bottom() + 6});
    };

    Element content =
        box()
            .column()
            .gap(20)
            // The faint remark is the sheet's own voice: every line is set in
            // it unless it says otherwise.
            .font(sketch::kit::theme().font(
                sketch::kit::theme().type.captionNote))
            .children({
                text(u8"EMBER DECODE")
                    .font(burnt(78, 5.0f))
                    .key("burn-display")
                    .textFx(
                        {.effect = textFx::pass(burn),
                         .tween = {.duration = std::chrono::duration<double, std::milli>(kUnitMs), .delay = sigil::motion::stagger(std::chrono::duration<double, std::milli>(kEachMs))}, 
                         .unit = weave::Unit::Cluster,
                         .progress = display}),
                document::paragraph(
                    "Each letter has its own clock. The bars read "
                    "back the same schedule that drives the burn.")
                    .width(680),
                box().height(6),
                text(u8"ONE PASS PER WORD PHASE")
                    .font(burnt(27, 3.0f))
                    .key("burn-words")
                    .textFx(
                        {.effect = textFx::pass(burn),
                         .tween = {.duration = std::chrono::duration<double, std::milli>(kUnitMs), .delay = sigil::motion::stagger(std::chrono::duration<double, std::milli>(kEachMs))}, 
                         .unit = weave::Unit::Word,
                         .progress = words}),
                document::paragraph(
                    "Each word is a unit here. The shader stays "
                    "the same; its schedule changes.")
                    .width(680),
            });
    return stack().children(
        {sketch::kit::page(
             {.title = "Text as a sampler",
              .subtitle = "One material pass · a separate clock for each "
                          "letter or word",
              .footer =
                  "The bars read back the schedule that drives the burn."},
             std::move(content)),
         stack().key("meter").inset(0).hitTestable(false).children(
             {each(beats, readBack)})});
  }

  void setup(sketch::SketchContext& ctx) {
    // The pass is a recipe's body rather than a whole program, so it is
    // read as text and handed to the recipe.
    recipe = burnRecipe(
        ctx.assets.hub().text(ctx.local("burn.sksl")).value_or(std::string()));
    const sketch::kit::Provide presentation(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(
        ctx, {.size = SkSize::Make(kW, kH),
              .captureAt = 2.4,
              .background = sketch::kit::theme()
                                .palette.ground});  // mid-decode: resolved,
                                                    // burning and unlit at once
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    // The cascades' real spans, read off the schedule rather than
    // restated: the last beat's start plus one beat's length is the whole
    // ramp. beatsOf answers after the first draw has laid the text out,
    // so this fills in on the first updated frame and then holds.
    const auto span = [&](const char* key) {
      float total = 1;
      for (const Beat& b : ctx.composer.beatsOf(key, 0))
        total = std::max(total, b.startMs + kUnitMs);
      return total;
    };
    if (displayTotalMs <= 1.0f) displayTotalMs = span("burn-display");
    if (wordsTotalMs <= 1.0f) wordsTotalMs = span("burn-words");
    const double t = sigil::motion::phase(elapsed, kLoop) * kLoop;
    display = masterAt(t, kInAt, displayTotalMs);
    words = masterAt(t, kWordsAt, wordsTotalMs);
    // Re-described per frame for the meter, which reads beatsOf at
    // describe time; the text nodes themselves prune (equal values), and
    // the pass repaints because its bound progress moved.
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(EmberDecode, "Kit · API",
             "text as a sampler — one SkSL pass burns a line in, reading "
             "each unit's rect, progress and seed out of uniform arrays")
