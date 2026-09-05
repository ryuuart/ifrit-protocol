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
//         .fx({.effect = fx::pass(burn),
//              .stagger = stagger(weave::unit::Cluster, {.eachMs = 260})});
//
// `fx::pass` makes the track's effect a PASS rather than a per-glyph
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
//  - the material is `mskia::Paint::recipe(...)` over a SigilMaterial recipe,
//    and the runtime owns the per-count specialization and its cache;
//  - the layer is sampled at the device's resolution, so a 2x host stays
//    sharp with no supersampled bake;
//  - the pass is BOUNDED to the node's box plus the track's reach, unlike a
//    raw Element::effect shader pass.
//
// WHAT REMAINS OUTSIDE THE ENGINE, stated because it shaped this file: a
// cascade opens each unit once — there is no "in, hold, burn off" as one
// stagger — so the loop below drives the track's PROGRESS up, holds it, and
// drives it back DOWN, which replays the cascade in reverse and burns the
// line off right to left (the last unit to arrive is the first to lose
// progress). And a pass is a whole-track statement: fx::seq/mix/hold do not
// consult it, so a pass that wants phases writes them in its own SkSL.
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
//       src/sketch/sketches/ember_decode.cpp \
//       --frame /tmp/ember_decode.png

#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
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

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr float kW = 1000.0f;
constexpr float kH = 430.0f;

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

const SkColor4f kPlate{0.027f, 0.024f, 0.031f, 1};
const SkColor4f kInk{0.96f, 0.91f, 0.82f, 1};    // the resolved letter
const SkColor4f kEmber{1.00f, 0.47f, 0.13f, 1};  // the crossing band
const SkColor4f kLabel{0.62f, 0.55f, 0.50f, 1};
const SkColor4f kFaint{0.38f, 0.33f, 0.31f, 1};

// ---------------------------------------------------------------------------
// The pass

/** THE BURN, against the pass contract: `uContent`, `uUnitRect`,
 *  `uUnitPhase` and `kUnitCount` are declared by the runtime, everything
 *  else is this material's own. The letters are baked white and the pass
 *  reads only their coverage, supplying every colour itself. A sketch
 *  dylib carries its own Skia, and an SkSL helper function called from
 *  main() faults in the host's inliner — so the noise, the threshold and
 *  the rim are all written inline. */
constexpr const char* kBurnSksl = R"(
half4 main(float2 xy) {
  float cover = float(uContent.eval(xy).a);
  // Which unit owns this pixel. step()/mix() rather than a branch: the
  // last matching unit wins, which is what boxes that touch want.
  float p = 0.0;
  float seed = 0.0;
  float u = 0.0;
  for (int i = 0; i < kUnitCount; ++i) {
    float4 r = uUnitRect[i];
    float inside = step(r.x, xy.x) * step(xy.x, r.x + r.z) *
                   step(r.y, xy.y) * step(xy.y, r.y + r.w);
    p = mix(p, uUnitPhase[i].x, inside);
    seed = mix(seed, uUnitPhase[i].y, inside);
    u = mix(u, (xy.x - r.x) / max(r.z, 1.0), inside);
  }
  // The threshold this pixel has to clear: a fine speckle, a coarser
  // blotch, and a bias along the unit, all seeded so the churn is the
  // same churn on every frame.
  float2 grain = floor(xy * 0.5) + seed;
  float speck = fract(sin(dot(grain, float2(12.9898, 78.233))) * 43758.5453);
  float2 blot = floor(xy * 0.09) + seed;
  float patch = fract(sin(dot(blot, float2(39.3468, 11.135))) * 24634.6345);
  float thr = uWeights[0] * u + uWeights[1] * speck + uWeights[2] * patch;
  // d is how far this pixel's unit has run past the pixel's own
  // threshold: negative is unburnt, 0 is the crossing, positive resolved.
  float d = p - thr;
  float body = smoothstep(0.0, 0.055, d);
  float front = 1.25 * exp(-abs(d) * 9.0) * smoothstep(0.0, 0.03, p) *
                (1.0 - 0.35 * body);
  // A tighter band inside the ember one: the crossing itself, white-hot.
  float core = exp(-abs(d) * 30.0) * smoothstep(0.0, 0.03, p);
  float a = cover * clamp(body + front + core, 0.0, 1.0);
  float3 emit = min(uInk.rgb * body + uEmber.rgb * front +
                    float3(1.0, 0.92, 0.72) * core * 0.55,
                    float3(1.0));
  return half4(half3(emit * a), half(a));
})";

/** The burn's ABI. The three weights are one array rather than three
 *  floats because they are read as a set and the body indexes them. */
struct BurnParams {
  sigil::material::Color uInk;
  sigil::material::Color uEmber;
  std::array<float, 3> uWeights;  // sweep, speckle, patch
};

/** The definition, made once for the process: a recipe's identity is the
 *  object, so a fresh one per describe would compile a fresh program and
 *  never compare equal to itself. */
std::shared_ptr<const sigil::material::Recipe> burnRecipe() {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      std::make_shared<const sigil::material::Recipe>(
          sigil::material::Recipe::of<BurnParams>("ember.burn")
              .body(sigil::material::Target::SkSL, kBurnSksl));
  return recipe;
}

mskia::Paint burnMaterial() {
  return mskia::Paint::recipe(sigil::material::Material(burnRecipe()))
      .uniform("uInk", kInk)
      .uniform("uEmber", kEmber)
      .uniform("uWeights", std::vector<float>{kSweep, kSpeckle, kPatch});
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

struct EmberDecode : sketch::Sketch {
  choreograph::Output<float> display{0.0f}, words{0.0f};
  float displayTotalMs = 1;  // the cascades' spans, read back from beatsOf
  float wordsTotalMs = 1;

  Element describe(sketch::SketchContext& ctx) {
    const sigil::weave::TextStyle label =
        weave::textStyle({.size = 11.5f, .color = kLabel, .track = 1.6f});
    const sigil::weave::TextStyle faint =
        weave::textStyle({.size = 10.5f, .color = kFaint, .track = 0.8f});
    const sk_sp<SkTypeface> face =
        weave::ports::face({"Helvetica Neue", "Arial", "Inter"}, 700);
    // The letters are set WHITE: the pass reads the layer's coverage and
    // supplies every colour itself, so the type's own colour never lands.
    const sigil::weave::TextStyle big =
        weave::textStyle({.face = face, .size = 78, .color = {1, 1, 1, 1}, .track = 5.0f});
    const sigil::weave::TextStyle small =
        weave::textStyle({.face = face, .size = 27, .color = {1, 1, 1, 1}, .track = 3.0f});

    const mskia::Paint burn = burnMaterial();
    Element root =
        box().column().padding(44).gap(20).fill(mskia::Paint::solid(kPlate));
    root.child(text(toU8("TEXT AS A SAMPLER \xc2\xb7 ONE SkSL PASS OVER ONE "
                         "RENDERED LINE"),
                    label));
    root.child(text(u8"EMBER DECODE", big)
                   .key("burn-display")
                   .fx({.effect = fx::pass(burn),
                        .stagger = {.eachMs = kEachMs, .durationMs = kUnitMs},
                        .over = weave::unit::Cluster,
                        .progress = &display}));
    root.child(
        text(toU8("uUnitRect[N] \xc2\xb7 uUnitPhase[N] \xe2\x80\x94 a LETTER "
                  "is a unit; the bar under each one is the progress that "
                  "unit's uniform carries, read back from beatsOf"),
             faint));
    root.child(box().height(6));
    root.child(text(u8"ONE PASS PER WORD PHASE", small)
                   .key("burn-words")
                   .fx({.effect = fx::pass(burn),
                        .stagger = {.eachMs = kEachMs, .durationMs = kUnitMs},
                        .over = weave::unit::Word,
                        .progress = &words}));
    root.child(text(toU8("the same pass, the same source at another count "
                         "\xe2\x80\x94 a WORD is a unit here, and the "
                         "runtime compiled and cached one variant per "
                         "count"),
                    faint));
    root.child(box().grow(1));
    root.child(text(toU8("one draw and one pass over each line's own box, "
                         "whatever N is \xc2\xb7 per-unit progress is "
                         "uniform DATA, not scene structure"),
                    faint));

    // THE SCHEDULE, DRAWN, from the same query the pass agrees with: one
    // bar per beat of the display track, at that beat's laid-out rect,
    // filled to that beat's local time — no restated i * eachMs anywhere.
    // beatsOf answers in the composer's space and the overlay spans the
    // root from its origin, so the two frames line up.
    Element meter = positioned().absolute().inset(0).hitTestable(false);
    meter.key("meter");
    const std::vector<Beat> beats = ctx.composer.beatsOf("burn-display", 0);
    for (size_t i = 0; i < beats.size(); ++i) {
      const SkRect& r = beats[i].rect;
      const std::string cell = "beat" + std::to_string(i);
      meter.child(box()
                      .key(cell)
                      .left(r.left())
                      .top(r.bottom() + 6)
                      .width(r.width())
                      .height(3)
                      .fill(Fill::color(kFaint))
                      .child(box()
                                 .key(cell + "-t")
                                 .left(0)
                                 .top(0)
                                 .width(r.width() * beats[i].localT)
                                 .height(3)
                                 .fill(Fill::color(kEmber))));
    }
    root.child(std::move(meter));  // appended last, so it paints over
    return root;
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(
        ctx, {.size = SkSize::Make(kW, kH),
              .captureAt = 2.4,
              .background =
                  kPlate});  // mid-decode: resolved, burning and unlit at once
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
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

SIGIL_SKETCH(
    EmberDecode, "Kit \xc2\xb7 API",
    "text as a sampler \xe2\x80\x94 one SkSL pass burns a line in, reading "
    "each unit's rect, progress and seed out of uniform arrays")
