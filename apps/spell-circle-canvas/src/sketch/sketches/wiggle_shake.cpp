// wiggle_shake.cpp — ONE API: wiggle(&out, amount, frequency, seed,
// octaves, falloff).
// =============================================================================
// `wiggle()` reads NO CLOCK: it is a pure function of the normalised input,
// so `BoundFloat::apply(p)` can be PLOTTED — which is the only way to see
// what the five parameters actually do. Every panel here is the same noise
// the chips are shaking with, drawn as a graph over a two-second window of
// `seconds`.
//
// EDIT THESE FIRST
//   kSeedY   — top row. Set it to kSeedX (= 1) and the 2-D locus collapses
//              to a straight diagonal: that is the SHARED-SEED failure the
//              docs warn about, and the left panel already shows it.
//   kOctaves — bottom row's three curves. 1 is drift, 6 is flicker.
//   kAmount  — px. The red rails are at +-kAmount, and the curve never
//              crosses them AT ANY OCTAVE COUNT: the noise is normalised
//              to [-1,1] first, so `amount` is a BOUND, not a scale factor.
//
// The three ways things move (hello.cpp): the two chips are door 1 — the
// shake is DECLARED as a bound property and the runtime resolves it every
// frame. The graphs are static leaves; nothing here re-describes.

#include <include/core/SkPathBuilder.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kAmount = 60.0f;    // px of shake, and the graph's rails
constexpr float kFrequency = 3.0f;  // cycles per second of `seconds`
constexpr uint32_t kSeedX = 1;
constexpr uint32_t kSeedY = 2;  // <- make this 1 to see the diagonal
constexpr int kOctaves[3] = {1, 3, 6};
constexpr float kFalloff = 0.5f;
constexpr float kWindow = 2.0f;  // seconds of `seconds` on the x axis

sigil::weave::TextStyle type(float size, SkColor4f color) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.paint.foreground.setColor4f(color, nullptr);
  style.paint.foreground.setAntiAlias(true);
  return style;
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
const SkColor4f kRail{0.85f, 0.30f, 0.36f, 0.75f};
const SkColor4f kTrace{0.36f, 0.82f, 0.72f, 1};
const SkColor4f kTraceB{1.00f, 0.72f, 0.28f, 1};

void strokePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color, nullptr);
  canvas.drawPath(path, paint);
}

/** THE 2-D LOCUS: (x(p), y(p)) traced over the window, which is the path
 *  a two-axis shake actually walks. Shared seeds put x == y, so the locus
 *  IS the line y = x — the layer slides on a diagonal and never shakes. */
Element locus(const BoundFloat& wx, const BoundFloat& wy, SkColor4f color) {
  return custom([wx, wy, color](SkCanvas& canvas, const PaintContext& paint) {
           const float w = paint.size.width(), h = paint.size.height();
           const float cx = w * 0.5f, cy = h * 0.5f;
           const float k = std::min(w, h) * 0.5f / (kAmount * 1.15f);
           // The +-amount box: the locus can touch it, never leave it.
           SkPaint box;
           box.setStyle(SkPaint::kStroke_Style);
           box.setStrokeWidth(1);
           box.setColor4f(kRail, nullptr);
           canvas.drawRect(SkRect::MakeLTRB(cx - kAmount * k, cy - kAmount * k,
                                            cx + kAmount * k, cy + kAmount * k),
                           box);
           SkPathBuilder trace;
           for (int i = 0; i <= 900; ++i) {
             const float p = kWindow * (float)i / 900.0f;
             const SkPoint at{cx + wx.apply(p) * k, cy + wy.apply(p) * k};
             i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
           }
           strokePath(canvas, trace.detach(), color, 1.3f);
         })
      .cache(Cache::None);
}

/** ONE LANE, GRAPHED: p on x, the wiggled value on y, with the +-amount
 *  rails. This is exactly what the runtime feeds a bound property. */
Element graph(const BoundFloat& w, SkColor4f color) {
  return custom([w, color](SkCanvas& canvas, const PaintContext& paint) {
           const float pw = paint.size.width(), ph = paint.size.height();
           const float cy = ph * 0.5f;
           const float k = (ph * 0.5f - 6.0f) / kAmount;
           SkPaint rule;
           rule.setStyle(SkPaint::kStroke_Style);
           rule.setStrokeWidth(1);
           rule.setColor4f(kRail, nullptr);
           canvas.drawLine(0, cy - kAmount * k, pw, cy - kAmount * k, rule);
           canvas.drawLine(0, cy + kAmount * k, pw, cy + kAmount * k, rule);
           rule.setColor4f(kFrame, nullptr);
           canvas.drawLine(0, cy, pw, cy, rule);
           SkPathBuilder trace;
           for (int i = 0; i <= 1200; ++i) {
             const float p = kWindow * (float)i / 1200.0f;
             const SkPoint at{pw * (float)i / 1200.0f, cy - w.apply(p) * k};
             i == 0 ? (void)trace.moveTo(at) : (void)trace.lineTo(at);
           }
           strokePath(canvas, trace.detach(), color, 1.4f);
         })
      .cache(Cache::None);
}

Element panel(float width, float height, const char* title, const char* sub,
              Element inner) {
  inner.inset(0);  // the plot fills its frame
  return box()
      .width(width)
      .column()
      .child(box()
                 .width(width)
                 .height(height)
                 .stroke(stroke(1.0f, Fill::color(kFrame)))
                 .child(std::move(inner)))
      .child(text(toU8(title), type(13, kInk)).margin(0, 8, 0, 3))
      .child(text(toU8(sub), type(11, kDim)));
}

}  // namespace

struct WiggleShake : sketch::Sketch {
  choreograph::Output<float> seconds{0};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1120, 680);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

    // `seconds` is the SCHEDULE the shake is phased off. It ramps forever
    // rather than wrapping, so `frequency` reads as plain Hz and the noise
    // never steps at a seam.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      seconds = (float)t;
      return true;
    });

    // The chips' own lanes, and the graphs', are the SAME values: build
    // them once and hand the BoundFloat to both.
    const Bound shakeX = wiggle(&seconds, kAmount, kFrequency, kSeedX);
    const Bound shakeY = wiggle(&seconds, kAmount, kFrequency, kSeedY);
    const Bound sameY = wiggle(&seconds, kAmount, kFrequency, kSeedX);

    // A live chip: two lanes, noise around REST. `wiggle(&out, …)` is
    // `bind(&out).scale(0).wiggle(…)` — without the scale(0) the property
    // would track `seconds` itself and drift off the canvas. `left/top` are
    // the REST position; the wiggle is a paint-only transform on top of it.
    const auto chip = [](const Bound& x, const Bound& y, SkColor4f color,
                         float left) {
      return box()
          .width(26)
          .height(26)
          .corners({5})
          .fill(Fill::color(color))
          .left(left)
          .top(102)
          .translateX(x)
          .translateY(y);
    };

    ctx.composer.render(
        stack()
            .child(text(toU8("wiggle(&out, amount, frequency, seed, octaves, "
                             "falloff) \xc2\xb7 no clock, so it graphs"),
                        type(15, kInk))
                       .left(30)
                       .top(16))

            // ---- the failure, and the fix -------------------------------
            .child(
                box()
                    .row()
                    .left(30)
                    .top(50)
                    .gap(26)
                    .child(panel(250, 250, "SHARED SEED \xc2\xb7 broken",
                                 "x and y both seed 1 \xe2\x86\x92 y = x",
                                 locus(shakeX.value(), sameY.value(), kTraceB)))
                    .child(panel(250, 250, "SEEDS 1 / 2 \xc2\xb7 a shake",
                                 "two independent lanes",
                                 locus(shakeX.value(), shakeY.value(), kTrace)))
                    .child(
                        panel(250, 250, "the same lanes, LIVE",
                              "amber = shared seed, teal = 1 / 2",
                              stack()
                                  .child(chip(shakeX, sameY, kTraceB, 70))
                                  .child(chip(shakeX, shakeY, kTrace, 154)))))

            // ---- octaves change TEXTURE, not the bound ------------------
            .child(box()
                       .row()
                       .left(30)
                       .top(388)
                       .gap(26)
                       .child(panel(340, 200, "1 octave \xc2\xb7 drift",
                                    "smooth value noise",
                                    graph(wiggle(&seconds, kAmount, kFrequency,
                                                 kSeedX, kOctaves[0], kFalloff)
                                              .value(),
                                          kTrace)))
                       .child(panel(340, 200, "3 octaves \xc2\xb7 handheld",
                                    "falloff 0.5 per octave",
                                    graph(wiggle(&seconds, kAmount, kFrequency,
                                                 kSeedX, kOctaves[1], kFalloff)
                                              .value(),
                                          kTrace)))
                       .child(panel(340, 200, "6 octaves \xc2\xb7 flicker",
                                    "still inside the rails",
                                    graph(wiggle(&seconds, kAmount, kFrequency,
                                                 kSeedX, kOctaves[2], kFalloff)
                                              .value(),
                                          kTrace))))

            .child(text(toU8("red rails are +-amount \xc2\xb7 the noise is "
                             "normalised to [-1,1] BEFORE amount, so the "
                             "bound holds at any octave count"),
                        type(11, kDim))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(
    WiggleShake, "Kit \xc2\xb7 API",
    "wiggle() reads no clock, so it GRAPHS \xe2\x80\x94 the shared-seed "
    "diagonal beside the fix, and the amplitude bound at six octaves")
