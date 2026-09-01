// query_probes.cpp — ONE API: Composer::bounds() and Composer::hitTest().
// =============================================================================
// A description says what a scene IS; layout decides where it landed.
// The query surface is how the host asks the second question, and this
// sheet answers it twice over the same two shapes.
//
//   bounds(key)  — the resolved rect of a keyed node. The ring around
//     the star is placed FROM that rect, not from the numbers that
//     described the star. Nothing here recomputes a layout by hand,
//     which is the point: a second copy of the arithmetic is a second
//     thing to keep in step.
//   hitTest(pt)  — which keyed node is under a point, shape and all. The
//     row of probes crosses both shapes at one height and each dot is
//     coloured by the answer: one colour for the star, one for the
//     rounded box, grey for nothing. The star's answer is what makes
//     the surface worth having — the gaps BETWEEN its arms miss, so the
//     probes flicker across it rather than covering its bounding box.
//
// THE ORDER OF OPERATIONS is the whole shape of this sketch. Queries
// answer against the RESOLVED tree, so the scene is described first, the
// answers are read on the next frame, and the overlay is described from
// them — the data path, not a paint-time peek. Reading a query while the
// tree is being painted would ask a question of the frame it is already
// answering.
//
// EDIT THESE FIRST
//   kProbeY / the probe stride — where the row cuts and how finely.
//   the star's innerRatio      — deeper arms, more misses between them.

#include <include/core/SkPaint.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>
#include <optional>
#include <string>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {720, 420};
constexpr float kProbeY = 190;
constexpr float kProbeFirstX = 20;
constexpr float kProbeStride = 24;
constexpr int kProbes = 30;  // kProbeFirstX + 29 strides is the last one in

/** What a probe landed on. The paint program reads these rather than the
 *  keys themselves: a fixed array of them copies without allocating, and
 *  a paint program that cannot throw is one the painter can call from
 *  anywhere. */
enum class Under : uint8_t { Nothing, Hero, Side };

}  // namespace

struct QueryProbes final : sketch::Sketch {
  /** What the last query answered, per probe. Meaningless until
   *  `answered`, which the first frame with a resolved layout sets. */
  std::array<Under, kProbes> hits{};
  std::optional<SkRect> heroBounds;
  bool answered = false;

  Element scene() const {
    auto root = stack()
                    .fill(Fill::color({0.05f, 0.06f, 0.1f, 1}))
                    .child(box()
                               .key("hero")
                               .width(180)
                               .height(180)
                               .inset(80, 100, 460, 140)
                               .absolute()
                               .shape(shapes::star(5, 0.42f))
                               .fill(Fill::color({0.95f, 0.42f, 0.28f, 1})))
                    .child(box()
                               .key("side")
                               .width(140)
                               .height(90)
                               .inset(420, 150, 160, 180)
                               .absolute()
                               .corners({16})
                               .fill(Fill::color({0.36f, 0.62f, 0.66f, 1})));
    if (!answered) return root;

    // The overlay is described FROM the answers, which is why it is a
    // child of the same tree rather than a pass over the canvas.
    const std::array<Under, kProbes> probes = hits;
    const std::optional<SkRect> hero = heroBounds;
    return root.child(
        custom([probes, hero](SkCanvas& canvas, const PaintContext&) {
          if (hero) {
            SkPaint ring;
            ring.setAntiAlias(true);
            ring.setStyle(SkPaint::kStroke_Style);
            ring.setStrokeWidth(3);
            ring.setColor4f({1.0f, 0.71f, 0.42f, 1});
            canvas.drawRoundRect(hero->makeOutset(10, 10), 18, 18, ring);
          }
          for (int i = 0; i < kProbes; ++i) {
            SkColor4f color = {0.23f, 0.25f, 0.32f, 1};  // nothing under it
            if (probes[(size_t)i] == Under::Hero)
              color = {1.0f, 0.37f, 0.54f, 1};
            else if (probes[(size_t)i] == Under::Side)
              color = {0.49f, 0.91f, 1.0f, 1};
            SkPaint dot;
            dot.setAntiAlias(true);
            dot.setColor4f(color);
            canvas.drawCircle(kProbeFirstX + (float)i * kProbeStride, kProbeY,
                              4, dot);
          }
        })
            .absolute()
            .inset(0)
            .zIndex(9));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.05f, 0.06f, 0.1f, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(scene());
  }

  void update(double, sketch::SketchContext& ctx) override {
    if (answered) return;  // the scene is still; one round of answers is all
    heroBounds = ctx.composer.bounds("hero");
    if (!heroBounds) return;  // no layout yet
    for (int i = 0; i < kProbes; ++i) {
      const float x = kProbeFirstX + (float)i * kProbeStride;
      const std::optional<std::string> hit = ctx.composer.hitTest({x, kProbeY});
      hits[(size_t)i] = !hit             ? Under::Nothing
                        : *hit == "hero" ? Under::Hero
                                         : Under::Side;
    }
    answered = true;
    ctx.composer.render(scene());
  }
};

SIGIL_SKETCH(QueryProbes, "Kit · API",
             "Composer::bounds() and hitTest() — a ring placed from a "
             "resolved rect, and a row of probes coloured by what each "
             "one lands on")
