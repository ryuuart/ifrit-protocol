// hit_slots.cpp — TWO UPDATE DOMAINS OVER ONE SCENE.
// =============================================================================
// A slot is a named mount point in a described tree, and `renderSlot`
// replaces what is under it WITHOUT re-describing anything else. That is
// the whole subject: the world below is described ONCE and its caches
// stay warm for the life of the sketch, while two small regions above it
// churn at completely different rates.
//
//   THE WORLD   nine keyed targets, described once in setup and never
//               again. Nothing in it is a rectangle, so the hit test has
//               to answer against real outlines rather than bounds.
//   THE PROBE   a slot re-rendered EVERY frame, because a marker that
//               follows a moving point is new content every frame and
//               there is nothing to compare.
//   THE READOUT a slot re-rendered ONLY when the answer changes. Most
//               frames the probe is still over what it was over last
//               frame, and re-describing the label then would be work
//               with no different pixels at the end of it.
//
// The two together are the point. A slot is not a cheap re-render; it is
// a BOUNDARY, and what it buys is that the expensive tree behind it never
// hears about either of them. Whether a given slot re-renders per frame
// or per change is then an ordinary question about that slot's content,
// answered twice here with two different answers.
//
// `Composer::hitTest` walks the described tree back to front and returns
// the KEY of the topmost node whose outline contains the point — the
// outline, so a blob's concave bay reads as a miss and its arm reads as a
// hit. A node with no key is transparent to it.
//
// EDIT THESE FIRST
//   the Lissajous rates in walk() — where the probe goes, and therefore
//                                   how often the readout changes.
//   kTargets — more keyed outlines under the same one-pass hit test.

#include <sigilcompose/kit/Silhouettes.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <string>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {860, 520};
constexpr int kTargets = 9;
/** When the probe stands on the first target, and how long it takes to
 *  reach the next one. */
constexpr double kFirstStation = 0.8;
constexpr double kStationGap = 1.05;

/** Where the probe is at scene time @p seconds: a Lissajous figure, so
 *  it crosses the field at an angle that never repeats over the run. */
SkPoint walk(double seconds) {
  const float w = kCanvas.width(), h = kCanvas.height();
  return {w * 0.5f + w * 0.36f * (float)std::sin(seconds * 0.9),
          h * 0.5f + h * 0.33f * (float)std::sin(seconds * 0.53 + 1.0)};
}

/** THE TARGETS STAND ON THE PROBE'S OWN PATH, one per station. A hit
 *  test only says something when there is something under the point, so
 *  the crossings are placed rather than hoped for: at station i the
 *  probe is exactly on target i's centre, and between stations it is
 *  somewhere in between and usually on nothing. */
double stationTime(int i) { return kFirstStation + (double)i * kStationGap; }

}  // namespace

struct HitSlots final : sketch::Sketch {
  std::string hitLabel = "\xe2\x80\x94";
  SkPoint probe{0, 0};

  /** THE WORLD: nine keyed outlines standing on the probe's path, plus
   *  the two slots the churn happens in. Described once. */
  Element describe() const {
    auto targets = stack().inset(0).zIndex(0);
    for (int i = 0; i < kTargets; ++i) {
      const float hue = (float)i / (float)kTargets;
      const SkPoint at = walk(stationTime(i));
      const float w = 108.0f + (float)(i % 3) * 24.0f;
      const float h = 100.0f + (float)(i % 4) * 20.0f;
      Shape outline = shapes::blob((uint32_t)(60 + i), 0.32f, 6);
      if (i % 2 == 1) outline = shapes::star(5 + i % 3, 0.5f);
      targets.child(
          box()
              .key("target-" + std::to_string(i))
              .width(w)
              .height(h)
              .inset(at.x() - w * 0.5f, at.y() - h * 0.5f, 0, 0)
              .shape(std::move(outline))
              .fill(Fill::color(
                  {0.22f + 0.45f * hue, 0.30f + 0.22f * hue, 0.62f, 0.85f}))
              .foreground(stroke(1.6f, Fill::color({0.85f, 0.95f, 1, 0.45f}))));
    }
    return stack()
        .fill(linearGradient(
            {0, 0}, {0, kCanvas.height()},
            {{0.07f, 0.06f, 0.13f, 1}, {0.16f, 0.09f, 0.20f, 1}}, {0.0f, 1.0f}))
        .child(std::move(targets))
        // Both slots opt OUT of the hit test. A slot's name is its key,
        // and a full-canvas marker slot would otherwise be the topmost
        // keyed node under every point the probe visits — the readout
        // would answer with the probe's own name, for ever.
        .child(slot("probe").inset(0).zIndex(6).hitTestable(false))
        .child(slot("readout")
                   .inset(18, kCanvas.height() - 42, 18, 12)
                   .zIndex(6)
                   .hitTestable(false));
  }

  /** The probe marker: new content on every frame, which is why its
   *  slot is re-rendered on every frame. */
  Element probeDot() const {
    const SkPoint p = probe;
    return custom([p](SkCanvas& c, const PaintContext&) {
             SkPaint paint;
             paint.setAntiAlias(true);
             paint.setColor(0xffffffff);
             c.drawCircle(p.x(), p.y(), 5, paint);
             paint.setStyle(SkPaint::kStroke_Style);
             paint.setStrokeWidth(1.5f);
             c.drawCircle(p.x(), p.y(), 10, paint);
           })
        .inset(0)
        .hitTestable(false)
        .cache(Cache::None);
  }

  Element readout() const {
    return box().hitTestable(false).child(text(
        toU8("hit: " + hitLabel), type({.size = 16, .color = hex(0xffe0b0)})));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(stationTime(4));  // standing on target-4
    hitLabel = "\xe2\x80\x94";
    probe = walk(0.0);
    ctx.composer.render(describe());
    ctx.composer.renderSlot("probe", probeDot());
    ctx.composer.renderSlot("readout", readout());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    Composer& composer = ctx.composer;
    probe = walk(elapsed);
    // Per frame: the marker moved, so its content is different.
    composer.renderSlot("probe", probeDot());
    // Per change: most frames the answer is the one already on screen.
    std::string label = composer.hitTest(probe).value_or("\xe2\x80\x94");
    if (label != hitLabel) {
      hitLabel = std::move(label);
      composer.renderSlot("readout", readout());
    }
  }
};

SIGIL_SKETCH(HitSlots, "Kit \xc2\xb7 API",
             "one scene described once under two slots that churn "
             "\xe2\x80\x94 a probe re-rendered every frame and a hitTest "
             "readout re-rendered only when the answer changes")
