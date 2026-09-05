/** @file
 * lane_retarget — bending a running motion onto the endpoints the next
 * description asks for, instead of restarting it.
 *
 * A LANE is one transitionable float on a node, addressed by WHERE the
 * motion that serves it is held. `Family` is the host's own enumeration
 * of its storages — one fixed slot array whose rows are a property of the
 * host, and any number of positional families whose length is a property
 * of the description — and everything else in the lane vocabulary is an
 * animatable, a held motion and a ticker. That is why it sits with the
 * values it retargets and names no host type at all: this sketch declares
 * its own `Family` and its own storage, and the calls are the same ones a
 * reconciler makes.
 *
 * `retargetSlots` walks the fixed rows: a row both descriptions carry is
 * retargeted, a row neither carries is skipped, and a row one side lacks
 * ramps from — or to — the lane's own STANDING value, which is the field's
 * default and the reason a slot can appear and disappear without a jump.
 *
 * `retargetFamily` walks a positional family, and its rule is the sharp
 * one: a description that changes the SHAPE of the family DROPS the
 * running motions rather than carrying them onto endpoints that now mean
 * something else. That is the same rule keys enforce for whole nodes. A
 * family of equal shape retargets lane by lane.
 *
 * Each cell steps a ticker of its own at a fixed delta and records the
 * value every tick, so the curves are the motions' own answers.
 *
 * EDIT THESE FIRST
 *   kAt — the moment the second description arrives, seconds.
 *   kFirst, kSecond — the two targets.
 *   kDuration — the transition both descriptions ask for.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Lanes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 254;
constexpr float kPicture = 176;

constexpr float kSpan = 2.4f;         // seconds plotted
constexpr float kDt = 1.0f / 120.0f;  // the delta they are stepped at
constexpr float kAt = 0.55f;          // when the second description arrives
constexpr float kFirst = 0.92f;       // the first target
constexpr float kSecond = 0.24f;      // …and the one it is bent onto
constexpr int kDuration = 900;        // the transition both ask for, ms

constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kGrid{0.19f, 0.20f, 0.24f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kSecondInk{0.46f, 0.72f, 0.92f, 1};

/** The host's enumeration of its storages, declared here because a lane
 *  names no host type: one fixed slot array and one positional family. */
enum class Family : uint8_t { Slots, Points };
using Lane = motion::Lane<Family>;

motion::Transition ramp() { return {std::chrono::milliseconds(kDuration)}; }

using Trace = std::vector<float>;

Element plot(const char* key, std::vector<std::pair<Trace, SkColor4f>> lanes) {
  return custom(key,
                [lanes = std::move(lanes)](SkCanvas& canvas,
                                           const PaintContext& pc) {
                  constexpr float kPad = 10;
                  const float w = pc.size.width() - 2 * kPad;
                  const float h = pc.size.height() - 2 * kPad;
                  SkPaint paint;
                  paint.setAntiAlias(true);
                  // The moment the second description arrives, marked on
                  // every cell at the same place.
                  paint.setColor4f(kGrid);
                  const float x = kPad + w * (kAt / kSpan);
                  canvas.drawRect({x, kPad, x + 1, kPad + h}, paint);
                  paint.setColor4f(kRule);
                  canvas.drawRect({kPad, kPad + h, kPad + w, kPad + h + 1},
                                  paint);
                  paint.setStyle(SkPaint::kStroke_Style);
                  paint.setStrokeWidth(1.6f);
                  for (const auto& [trace, colour] : lanes) {
                    if (trace.size() < 2) continue;
                    SkPathBuilder path;
                    for (size_t i = 0; i < trace.size(); ++i) {
                      const float px =
                          kPad + w * (float)i / (float)(trace.size() - 1);
                      const float py = kPad + h * (1.0f - trace[i]);
                      if (i == 0)
                        path.moveTo(px, py);
                      else
                        path.lineTo(px, py);
                    }
                    paint.setColor4f(colour);
                    canvas.drawPath(path.detach(), paint);
                  }
                })
      .absolute()
      .inset(0);
}

Element cell(const char* call, const char* note, Element body,
             const std::string& readout) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture})
          .child(std::move(body))
          .child(text(toU8(readout), sketch::kit::theme().mono(10, kFigure))
                     .absolute()
                     .left(Dim(8.0f))
                     .top(Dim(6.0f))
                     .padding(4, 2)
                     .fill(Fill::color(kCellGround))));
}

}  // namespace

struct LaneRetarget final : sketch::Sketch {
  Trace plain, slots, family, reshaped;
  std::string readouts[4];

  void setup(sketch::SketchContext& ctx) override {
    // the four flights have already been run
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    plain = run(Change::None);
    slots = run(Change::Slots);
    family = run(Change::Family);
    reshaped = run(Change::Reshaped);

    readouts[0] =
        kit::format("one description \xc2\xb7 0 \xe2\x86\x92 %.2f", kFirst);
    readouts[1] =
        kit::format("retargetSlots at %.2f s \xe2\x86\x92 %.2f", kAt, kSecond);
    readouts[2] = kit::format("retargetFamily \xc2\xb7 same shape");
    readouts[3] = kit::format("retargetFamily \xc2\xb7 shape 1 \xe2\x86\x92 2");

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("RETARGETING A LANE \xc2\xb7 motion::"
                       "retargetSlots, motion::retargetFamily"),
         .subtitle = toU8("dials \xc2\xb7 the moment the second "
                          "description arrives (0.55 s, the rule on "
                          "every plot) \xc2\xb7 the two targets \xc2\xb7 "
                          "the transition both ask for (900 ms)"),
         .footer = toU8("a description that changes the SHAPE of a "
                        "positional family DROPS its running motions "
                        "rather than carrying them onto endpoints that "
                        "now mean something else \xe2\x80\x94 the same "
                        "rule keys enforce for whole nodes")},
        kit::cells(
            {.cells = {cell("one description, left alone",
                            "the flight the other three interrupt \xc2\xb7 "
                            "one transition from the standing value to the "
                            "first target",
                            plot("plain", {{plain, kFigure}}), readouts[0]),
                       cell("retargetSlots(ticker, anims, prev, next, spec)",
                            "the fixed row bent onto the second target "
                            "mid-flight \xc2\xb7 the plain flight is under it "
                            "for comparison",
                            plot("slots", {{plain, kAsh}, {slots, kFigure}}),
                            readouts[1]),
                       cell("retargetFamily \xc2\xb7 equal shape",
                            "a positional family of the same length "
                            "retargets lane by lane, exactly as the fixed "
                            "rows do",
                            plot("family", {{plain, kAsh}, {family, kFigure}}),
                            readouts[2]),
                       cell("retargetFamily \xc2\xb7 the shape changed",
                            "one lane became two \xc2\xb7 the motions are "
                            "dropped and the new lanes start where the "
                            "storage starts, which is the jump this rule "
                            "chooses over a wrong carry",
                            plot("reshaped",
                                 {{plain, kAsh}, {reshaped, kSecondInk}}),
                            readouts[3])},
             .gap = 14})));
  }

  enum class Change { None, Slots, Family, Reshaped };

  /** One flight: a ticker stepped at a fixed delta, with the second
   *  description applied at `kAt` in whichever way the cell is about. */
  Trace run(Change change) {
    motion::Ticker ticker;
    motion::AnimatedFloats anims;
    anims.resize(1);
    const motion::Animatable<float> standing = 0.0f;
    const motion::Animatable<float> first = kFirst;
    const motion::Animatable<float> second = kSecond;
    const std::optional<motion::Transition> spec = ramp();

    // The mount: the first description's endpoint, from the lane's own
    // standing value.
    motion::transitionFloatAt(ticker, anims[0], standing, first, spec);

    Trace trace;
    bool applied = false;
    const int steps = (int)(kSpan / kDt);
    for (int i = 0; i < steps; ++i) {
      const float t = (float)i * kDt;
      if (!applied && t >= kAt && change != Change::None) {
        applied = true;
        const Lane before{&first, {Family::Slots, 0}, 0.0f};
        const Lane after{&second, {Family::Slots, 0}, 0.0f};
        if (change == Change::Slots) {
          motion::retargetSlots<Family>(ticker, anims, {&before, 1},
                                        {&after, 1}, spec);
        } else if (change == Change::Family) {
          const Lane beforeP{&first, {Family::Points, 0}, 0.0f};
          const Lane afterP{&second, {Family::Points, 0}, 0.0f};
          motion::retargetFamily<Family>(ticker, anims, {&beforeP, 1},
                                         {&afterP, 1}, spec);
        } else {
          // Two lanes where there was one: the family's shape changed.
          const Lane beforeP{&first, {Family::Points, 0}, 0.0f};
          const Lane afterP[2] = {{&second, {Family::Points, 0}, 0.0f},
                                  {&second, {Family::Points, 1}, 0.0f}};
          motion::retargetFamily<Family>(ticker, anims, {&beforeP, 1},
                                         {afterP, 2}, spec);
        }
      }
      ticker.tick(kDt);
      const motion::Animatable<float>& reading =
          applied && change != Change::None ? second : first;
      trace.push_back(motion::resolveFloatAt(
          anims.empty() ? nullptr : anims[0].get(), reading));
    }
    return trace;
  }
};

SIGIL_SKETCH(LaneRetarget, "Kit \xc2\xb7 API",
             "one flight interrupted at the same moment three ways \xe2\x80"
             "\x94 a fixed row bent onto a new endpoint, a positional family "
             "of equal shape, and one whose shape changed and dropped its "
             "motions")
