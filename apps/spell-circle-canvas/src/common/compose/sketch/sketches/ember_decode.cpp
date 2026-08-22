// ember_decode.cpp — ONE ROUTE: a shader per letter, without a shader per
// letter.
// =============================================================================
// "Each letter phases in under its own SkSL" sounds like one runtime effect
// per glyph. It is not: a glyph is not a draw the author owns — the text
// engine batches a line into one drawGlyphsRSXform call per (font, paint
// pass), and cutting that up to give every letter its own paint is the one
// thing that makes typography expensive.
//
// THE ROUTE PROVED HERE. The line is rendered ONCE into pixels, and ONE SkSL
// pass reads those pixels as a sampler. Per-pixel, the shader asks which UNIT
// it is standing in — a letter on the display line, a word on the small one —
// and burns that unit in on that unit's own clock:
//
//     uniform shader uArt      the line, already rendered
//     uniform float4 uRect[N]  each unit's box, in this node's px
//     uniform float2 uCell[N]  x = that unit's 0->1, y = its noise seed
//
// So the cost is one draw and one pass over the line's own box, whatever N
// is, and "per letter" is uniform data rather than scene structure. The
// dissolve threshold is noise plus a left-to-right bias inside each unit, so
// the letter is eaten from seeded edges; the band where the threshold is
// being crossed is drawn hot, which is the ember rim.
//
// WHAT THIS ROUTE ASKS FOR AND DOES NOT GET. The door that hands a shader a
// node's own rendered pixels is `Element::effect(Effect::shader(...))`, and it
// carries FLOAT uniforms only — `Effect::shader(fx, {{name, float}})` and
// `Effect::uniform(name, &output)`. There is no array form and no vector form
// on that seam (nor on `Material::uniform`, which checks the declared size and
// rejects anything wider than a float), and no seam at all that hands a
// track's per-unit stagger to a shader. That is the whole reason the burn
// below is a custom() leaf at Cache::None driving SkRuntimeShaderBuilder by
// hand: the arrays are the point, and the array-capable door — a hand-built
// SkImageFilters::RuntimeShader passed to Effect::filter — is exactly the one
// that carries no live uniform and declares no volatility, so its progress
// could never move.
//
// Two things about that door are worth knowing before reaching for it: its
// main(xy) is in the NODE's own px, not the canvas's, and its output is NOT
// bounded by the node's box — a pass that returns colour where the content is
// transparent paints over the whole clip. Multiplying by the sampled coverage,
// as the burn does, is what keeps the marks on the letters.
//
// The rects are the other hand-walk, and it leaves compose entirely. Nothing
// on compose's seam answers "the rect of unit k": `measureRun` reports
// advances with no positions and drops the gaps between words, and
// `Composer::paragraphLayout` answers only for a keyed text node that is in
// the tree and only after a layout, which the sampler's baked line is not. So
// the units below are walked out of SigilWeave's own laid-out paragraph, and
// a unit that is a WORD is a merge the caller performs on that walk.
//
// EDIT THESE FIRST
//   kEach     — start-to-start between units. 0 decodes the whole line at
//               once and the per-unit uniforms stop being visible; past
//               kUnitDur the line reads one letter at a time.
//   kSpeckle / kPatch / kSweep — the three weights of the dissolve threshold.
//               They sum to 1. All on kSweep is a clean left-to-right wipe
//               with no burn; all on kSpeckle is television static.
//   kSuper    — the raster scale of the baked line. Drop it to 1 to see what
//               the route costs in sharpness when the sampler is baked at
//               logical size and the host draws at 2x.
//
// The three ways things move (hello.cpp): door 2. The burn is a paint program
// at Cache::None reading PaintContext::elapsedSeconds, wrapped into one
// kLoop-second cycle so the whole decode reads on repeat.
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/ember_decode.cpp \
//       --frame /tmp/ember_decode.png --at 2.4

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Studio.h>
#include <sigilsketch/Sketch.h>
#include <sigilweave/Choreograph.h>
#include <sigilweave/Flow.h>
#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr float kW = 1000.0f;
constexpr float kH = 430.0f;

// ---- the cycle -------------------------------------------------------------
constexpr double kLoop = 9.6;     // one full decode + hold + burn-off
constexpr double kInAt = 0.35;    // first unit starts here
constexpr double kEach = 0.26;    // start-to-start, left to right
constexpr double kUnitDur = 1.0;  // one unit's own 0 -> 1
constexpr double kWordsAt = 1.4;  // the small line runs its own clock, late
constexpr double kOutAt = 7.30;   // the burn-off, right to left
constexpr double kOutEach = 0.09;
constexpr double kOutDur = 0.50;

// ---- the burn --------------------------------------------------------------
constexpr float kSuper = 2.0f;   // the line is rastered at this scale
constexpr float kPad = 1.5f;     // px around each unit's advance box, so a
                                 // letter's overshoot still finds its unit
constexpr float kSweep = 0.28f;  // the three threshold weights: they sum to 1
constexpr float kSpeckle = 0.30f;
constexpr float kPatch = 0.42f;

const SkColor4f kPlate{0.027f, 0.024f, 0.031f, 1};
const SkColor4f kInk{0.96f, 0.91f, 0.82f, 1};    // the resolved letter
const SkColor4f kEmber{1.00f, 0.47f, 0.13f, 1};  // the crossing band
const SkColor4f kLabel{0.62f, 0.55f, 0.50f, 1};
const SkColor4f kFaint{0.38f, 0.33f, 0.31f, 1};

// ---------------------------------------------------------------------------
// The units

/** One unit of the burn: the box its pixels live in, and the number that
 *  makes its noise its own. */
struct Unit {
  SkRect rect;
  float seed = 0;
};

/** A line ready to burn: its pixels, its size in logical px, and its units. */
struct Line {
  sk_sp<SkImage> art;
  SkSize size{0, 0};
  std::vector<Unit> units;
};

/** THE HAND WALK, and it goes past compose's own seam to make it.
 *
 *  A unit's box is where the layout PUT the glyphs, so the walk needs
 *  positions and a word index, which is SigilWeave's `forEachPlacedGlyph`
 *  over a laid-out paragraph. Compose's own one-shot answer, `measureRun`,
 *  reports the glyphs' advances and no positions — and an inter-word space
 *  is a gap the flow leaves rather than a glyph, so it is absent from that
 *  list and prefix sums of it walk a multi-word line off to the left.
 *
 *  Each unit spans the line box's full height: a letter's own ink box is
 *  narrower than that, but neighbouring units are separated horizontally and
 *  a full-height box is what lets an overshoot or a descender still find the
 *  unit it belongs to. */
std::vector<Unit> unitsOf(std::u8string_view utf8,
                          const sigil::weave::TextStyle& style,
                          sigil::weave::FontContext& fonts, float band,
                          bool byWord) {
  sigil::weave::Paragraph paragraph;
  paragraph.appendText(utf8, style);
  sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
  const sigil::weave::ParagraphLayout layout =
      sigil::weave::layoutParagraph(fonts, paragraph, flow);

  std::vector<Unit> units;
  uint32_t openWord = ~0u;
  sigil::weave::forEachPlacedGlyph(
      layout, paragraph, [&](const sigil::weave::PlacedGlyph& glyph) {
        const SkRect box = SkRect::MakeXYWH(glyph.rest.x() - kPad, 0,
                                            glyph.advance + 2 * kPad, band);
        if (byWord && glyph.wordIndex == openWord && !units.empty()) {
          units.back().rect.fRight = box.fRight;
          return;
        }
        openWord = glyph.wordIndex;
        // The seed only has to differ per unit and be the same every frame.
        units.push_back({box, 1.0f + (float)units.size() * 7.3f});
      });
  return units;
}

/** The line, rendered once into pixels the shader samples. Baked at kSuper so
 *  it still has edges when the host draws at 2x; the shader's local matrix
 *  scales it back to logical px, which is the space the unit rects are in. */
Line bakeLine(std::u8string_view utf8, const sigil::weave::TextStyle& style,
              sigil::weave::FontContext& fonts, bool byWord) {
  Line line;
  // snapshot() and measure() take the INTRINSIC size from the root's
  // children, so the leaf is wrapped rather than handed over bare.
  Element leaf = text(std::u8string(utf8), style);
  line.size = measure(box().child(leaf), fonts);
  const int w = std::max(1, (int)std::ceil(line.size.width() * kSuper));
  const int h = std::max(1, (int)std::ceil(line.size.height() * kSuper));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface) return line;
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->scale(kSuper, kSuper);
  canvas->drawPicture(snapshot(box().child(leaf), fonts));
  line.art = surface->makeImageSnapshot();
  line.units = unitsOf(utf8, style, fonts, line.size.height(), byWord);
  return line;
}

// ---------------------------------------------------------------------------
// The pass

/** THE BURN, as one monolithic main(). A sketch dylib carries its own Skia,
 *  and an SkSL helper function called from one faults in the host's inliner —
 *  so the noise, the threshold and the rim are all written inline.
 *
 *  ONE EFFECT PER UNIT COUNT. The loop bound is baked into the source because
 *  a runtime effect has no loop a uniform can bound, and the arrays are sized
 *  to the count for the same reason a fixed ceiling would be worse: the
 *  builder refuses a partial array write, so a ceiling of 64 would upload 64
 *  units' worth of uniforms and walk 64 iterations per pixel to burn 12. */
sk_sp<SkRuntimeEffect> burnEffect(int units) {
  struct Cached {
    int units;
    sk_sp<SkRuntimeEffect> effect;
  };
  static std::vector<Cached> cache;
  for (const Cached& c : cache)
    if (c.units == units) return c.effect;

  const std::string n = std::to_string(units);
  const std::string src =
      "uniform shader uArt;"
      "uniform float4 uRect[" +
      n +
      "];"
      "uniform float2 uCell[" +
      n +
      "];"
      "uniform float4 uInk;"
      "uniform float4 uEmber;"
      "half4 main(float2 xy) {"
      "  float cover = float(uArt.eval(xy).a);"
      // Which unit owns this pixel. step()/mix() rather than a branch: the
      // last matching unit wins, which is what padded boxes that touch want.
      "  float p = 0.0;"
      "  float seed = 0.0;"
      "  float u = 0.0;"
      "  for (int i = 0; i < " +
      n +
      "; ++i) {"
      "    float4 r = uRect[i];"
      "    float inside = step(r.x, xy.x) * step(xy.x, r.x + r.z) *"
      "                   step(r.y, xy.y) * step(xy.y, r.y + r.w);"
      "    p = mix(p, uCell[i].x, inside);"
      "    seed = mix(seed, uCell[i].y, inside);"
      "    u = mix(u, (xy.x - r.x) / max(r.z, 1.0), inside);"
      "  }"
      // The threshold this pixel has to clear: a fine speckle, a coarser
      // blotch, and a bias along the unit, all seeded so the churn is the
      // same churn on every frame.
      "  float2 grain = floor(xy * 0.5) + seed;"
      "  float speck = fract(sin(dot(grain, float2(12.9898, 78.233))) *"
      "                      43758.5453);"
      "  float2 blot = floor(xy * 0.09) + seed;"
      "  float patch = fract(sin(dot(blot, float2(39.3468, 11.135))) *"
      "                      24634.6345);"
      "  float thr = " +
      std::to_string(kSweep) + " * u + " + std::to_string(kSpeckle) +
      " * speck + " + std::to_string(kPatch) +
      " * patch;"
      // d is how far this pixel's unit has run past the pixel's own
      // threshold: negative is unburnt, 0 is the crossing, positive resolved.
      "  float d = p - thr;"
      "  float body = smoothstep(0.0, 0.055, d);"
      "  float front = 1.25 * exp(-abs(d) * 9.0) * smoothstep(0.0, 0.03, p) *"
      "                (1.0 - 0.35 * body);"
      // A tighter band inside the ember one: the crossing itself, white-hot.
      "  float core = exp(-abs(d) * 30.0) * smoothstep(0.0, 0.03, p);"
      "  float a = cover * clamp(body + front + core, 0.0, 1.0);"
      "  float3 emit = min(uInk.rgb * body + uEmber.rgb * front +"
      "                    float3(1.0, 0.92, 0.72) * core * 0.55,"
      "                    float3(1.0));"
      "  return half4(half3(emit * a), half(a));"
      "}";

  auto [effect, error] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!effect) SkDebugf("ember_decode burn shader: %s\n", error.c_str());
  cache.push_back({units, effect});
  return effect;
}

/** One unit's 0 -> 1: staggered left to right on the way in, right to left on
 *  the way out, so one loop carries the whole decode and its burn-off. */
float phaseOf(double t, int i, int n) {
  const double in =
      std::clamp((t - kInAt - (double)i * kEach) / kUnitDur, 0.0, 1.0);
  const double out = std::clamp(
      (t - kOutAt - (double)(n - 1 - i) * kOutEach) / kOutDur, 0.0, 1.0);
  return (float)(in * (1.0 - out));
}

std::array<float, 4> rgba(SkColor4f c) { return {c.fR, c.fG, c.fB, c.fA}; }

/** The paint program: fill the uniform arrays from the clock, hand the baked
 *  line over as the sampler, and draw the node's whole box in one pass. */
PaintProgram burn(std::shared_ptr<const Line> line, double startAt) {
  return [line = std::move(line), startAt](SkCanvas& canvas,
                                           const PaintContext& ctx) {
    const int n = (int)line->units.size();
    if (!line->art || n == 0) return;
    sk_sp<SkRuntimeEffect> effect = burnEffect(n);
    if (!effect) return;

    const double t = studio::phase(ctx.elapsedSeconds - startAt, kLoop) * kLoop;
    std::vector<float> rects((size_t)n * 4), cells((size_t)n * 2);
    for (int i = 0; i < n; ++i) {
      const SkRect& r = line->units[(size_t)i].rect;
      rects[(size_t)i * 4 + 0] = r.x();
      rects[(size_t)i * 4 + 1] = r.y();
      rects[(size_t)i * 4 + 2] = r.width();
      rects[(size_t)i * 4 + 3] = r.height();
      cells[(size_t)i * 2 + 0] = phaseOf(t, i, n);
      cells[(size_t)i * 2 + 1] = line->units[(size_t)i].seed;
    }

    SkRuntimeShaderBuilder builder(effect);
    builder.uniform("uRect").set(rects.data(), n * 4);
    builder.uniform("uCell").set(cells.data(), n * 2);
    builder.uniform("uInk") = rgba(kInk);
    builder.uniform("uEmber") = rgba(kEmber);
    const SkMatrix toLogical = SkMatrix::Scale(1.0f / kSuper, 1.0f / kSuper);
    builder.child("uArt") = line->art->makeShader(
        SkTileMode::kDecal, SkTileMode::kDecal,
        SkSamplingOptions(SkFilterMode::kLinear), &toLogical);
    SkPaint paint;
    paint.setShader(builder.makeShader());
    canvas.drawRect(SkRect::MakeSize(ctx.size), paint);
  };
}

/** The uniform array, drawn: one bar per unit, under the unit it belongs to,
 *  filled to that unit's progress. It reads the same numbers the burn does. */
PaintProgram meter(std::shared_ptr<const Line> line, double startAt) {
  return [line = std::move(line), startAt](SkCanvas& canvas,
                                           const PaintContext& ctx) {
    const int n = (int)line->units.size();
    if (n == 0) return;
    const double t = studio::phase(ctx.elapsedSeconds - startAt, kLoop) * kLoop;
    SkPaint bed, fill;
    bed.setColor4f(kFaint, nullptr);
    fill.setColor4f(kEmber, nullptr);
    for (int i = 0; i < n; ++i) {
      const SkRect& r = line->units[(size_t)i].rect;
      const float p = phaseOf(t, i, n);
      canvas.drawRect(SkRect::MakeXYWH(r.x() + kPad, 0, r.width() - 2 * kPad,
                                       ctx.size.height()),
                      bed);
      canvas.drawRect(
          SkRect::MakeXYWH(r.x() + kPad, 0, (r.width() - 2 * kPad) * p,
                           ctx.size.height()),
          fill);
    }
  };
}

/** A burning line and its meter, as one column sized to the baked art. */
Element burning(const std::string& key, const std::shared_ptr<const Line>& line,
                double startAt, bool withMeter) {
  Element column = box().column().gap(8).width(line->size.width());
  // The keyed custom() spelling: the key is the program's identity, so the
  // node prunes across describes even though a lambda cannot compare. The
  // clock is read inside the program, which is what Cache::None declares.
  column.child(custom(key, burn(line, startAt))
                   .width(line->size.width())
                   .height(line->size.height())
                   .cache(Cache::None));
  if (withMeter)
    column.child(custom(key + "-meter", meter(line, startAt))
                     .width(line->size.width())
                     .height(3)
                     .cache(Cache::None));
  return column;
}

}  // namespace

// ===========================================================================

struct EmberDecode : sigil::compose::sketch::Sketch {
  std::shared_ptr<const Line> display, words;

  Element describe() {
    const sigil::weave::TextStyle label =
        studio::type({.size = 11.5f, .color = kLabel, .track = 1.6f});
    const sigil::weave::TextStyle faint =
        studio::type({.size = 10.5f, .color = kFaint, .track = 0.8f});

    return box()
        .column()
        .padding(44)
        .gap(20)
        .fill(Material::solid(kPlate))
        .child(text(toU8("TEXT AS A SAMPLER \xc2\xb7 ONE SkSL PASS OVER ONE "
                         "RENDERED LINE"),
                    label))
        .child(burning("burn-display", display, 0.0, true))
        .child(text(
            toU8("uRect[" + std::to_string(display->units.size()) +
                 "] \xc2\xb7 uCell[" + std::to_string(display->units.size()) +
                 "] \xe2\x80\x94 a LETTER is a unit; the bar under "
                 "each one is the progress that unit's uniform "
                 "carries"),
            faint))
        .child(box().height(6))
        .child(burning("burn-words", words, kWordsAt, false))
        .child(text(toU8("the same pass, the same shader source at another "
                         "count \xe2\x80\x94 a WORD is a unit here, which is "
                         "a merge of advance boxes and nothing else"),
                    faint))
        .child(box().grow(1))
        .child(text(toU8("one draw and one pass over each line's own box, "
                         "whatever N is \xc2\xb7 per-unit progress is uniform "
                         "DATA, not scene structure"),
                    faint));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kPlate);
    ctx.captureAt(2.4);  // mid-decode: resolved, burning and unlit at once
    if (!ctx.fonts) return;

    const sk_sp<SkTypeface> face =
        studio::pickFace({"Helvetica Neue", "Arial", "Inter"}, 700);
    // The bake is WHITE: the pass reads the art's coverage and supplies every
    // colour itself, so the baked line's own colour never reaches a pixel.
    display = std::make_shared<const Line>(bakeLine(
        u8"EMBER DECODE",
        studio::type(
            {.face = face, .size = 78, .color = {1, 1, 1, 1}, .track = 5.0f}),
        *ctx.fonts, false));
    words = std::make_shared<const Line>(bakeLine(
        u8"ONE PASS PER WORD PHASE",
        studio::type(
            {.face = face, .size = 27, .color = {1, 1, 1, 1}, .track = 3.0f}),
        *ctx.fonts, true));

    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(EmberDecode)
