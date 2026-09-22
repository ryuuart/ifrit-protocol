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

// TAGS: Motion/Transitions

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Lanes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Kit.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 690};
constexpr float kCell = 328;
constexpr float kPicture = 180;

constexpr float kSpan = 2.4f;         // seconds plotted
constexpr float kDt = 1.0f / 120.0f;  // the delta they are stepped at
constexpr float kAt = 0.55f;          // when the second description arrives
constexpr float kFirst = 0.92f;       // the first target
constexpr float kSecond = 0.24f;      // …and the one it is bent onto
constexpr int kDuration = 900;        // the transition both ask for, ms

constexpr material::Color kGrid{0.19f, 0.20f, 0.24f, 1};
constexpr material::Color kSecondInk{0.46f, 0.72f, 0.92f, 1};

/** The host's enumeration of its storages, declared here because a lane
 *  names no host type: one fixed slot array and one positional family. */
enum class Family : uint8_t { Slots, Points };
using Lane = motion::Lane<Family>;

motion::Transition ramp() { return {std::chrono::milliseconds(kDuration)}; }

using Trace = std::vector<float>;

/** THE CLASSES A CELL'S PLOT IS DRESSED BY, over the sheet the page
 *  already states: the moment the second description arrives, the flight
 *  that was left alone under the comparison, and the one whose family
 *  changed shape. */
weave::StyleSheet look() {
  weave::StyleSheet dressed = sketch::kit::theme().styleSheet();
  dressed.set("arrive", {.color = material::skia::toSkColor(kGrid)});
  dressed.set(
      "quiet",
      {.color = material::skia::toSkColor(sketch::kit::theme().palette.ash)});
  dressed.set("reshaped", {.color = material::skia::toSkColor(kSecondInk)});
  return dressed;
}

/** ONE CELL'S PLOT: the seconds across, the lane's value up, a rule where
 *  the second description arrives, and one trace per flight in the class
 *  it was named with. Each flight is a run of samples taken a tick apart,
 *  so the trace reads it at the moment the frame asks for. */
Element plot(const char* key,
             std::vector<std::pair<Trace, const char*>> lanes) {
  std::vector<sketch::kit::Layer> layers{
      sketch::kit::rules({.x = {kAt}, .styleClass = "arrive"}),
      sketch::kit::axis(
          {.of = sketch::kit::Axis::X, .reach = 0, .numbers = false})};
  for (auto& [flight, series] : lanes)
    layers.push_back(sketch::kit::trace(
        [taken = std::move(flight)](double seconds) {
          const double last = (double)taken.size() - 1;
          return (double)taken[(size_t)std::lround(seconds / kSpan * last)];
        },
        {.pen = {.width = 1.6f}, .styleClass = series}));
  return sketch::kit::plot(
             key,
             {.x = {.domain = {0, kSpan}}, .y = {.domain = {0, 1}}, .pad = 10},
             std::move(layers))
      .cover();
}

}  // namespace

struct LaneRetarget {
  Trace plain, slots, family, reshaped;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // the four flights have already been run
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    plain = run(Change::None);
    slots = run(Change::Slots);
    family = run(Change::Family);
    reshaped = run(Change::Reshaped);

    float disagreement = 0;
    for (size_t i = 0; i < slots.size(); ++i)
      disagreement = std::max(disagreement, std::abs(slots[i] - family[i]));
    const auto figure = [&](const char* key, const Trace& flight,
                            const char* series = "plotTrace") {
      return sketch::kit::well({.width = kCell, .height = kPicture})
          .children({plot(key, {{plain, "quiet"}, {flight, series}})});
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "A new target, mid-flight",
         .subtitle = "One transition, three responses to the same arrival · 0 "
                     "→ 0.92, then 0.24 at 0.55 s",
         .footer = "Every trace is sampled from a real ticker at 120 Hz. The "
                   "vertical rule marks the new description; grey preserves "
                   "the uninterrupted flight."},
        box()
            .column()
            .gap(22)
            .children(
                {sketch::kit::caption(
                     1020, "REFERENCE / NO INTERRUPTION",
                     "0–2.4 s · value 0–1 · transition duration 900 ms",
                     sketch::kit::well({.width = 1020, .height = 90})
                         .children({plot("plain", {{plain, "plotTrace"}})})),
                 sketch::kit::comparison(
                     {.cases = {{.title = "FIXED SLOT",
                                 .control = "retargetSlots · one property",
                                 .figure = figure("slots", slots),
                                 .note = "Keep the current value and bend "
                                         "toward the new endpoint."},
                                {.title = "SAME FAMILY",
                                 .control = "retargetFamily · 1 → 1 lane",
                                 .figure = figure("family", family),
                                 .note = "Equal shape preserves the running "
                                         "motion, just as a fixed slot does."},
                                {.title = "CHANGED FAMILY",
                                 .control = "retargetFamily · 1 → 2 lanes",
                                 .figure =
                                     figure("reshaped", reshaped, "reshaped"),
                                 .note = "Changing shape drops the old motion. "
                                         "The new value arrives immediately."}},
                      .measure = 1020,
                      .gap = 18}),
                 sketch::kit::readout(
                     {{.name = "Maximum sampled difference",
                       .value = kit::formatted("%.6f", disagreement),
                       .note = "fixed slots / equal family"}},
                     {.nameMeasure = 190})})
            .styleSheet(look())));
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

SIGIL_SKETCH(LaneRetarget, "Kit · API",
             "one flight interrupted at the same moment three ways — a fixed "
             "row bent onto a new endpoint, a positional family "
             "of equal shape, and one whose shape changed and dropped its "
             "motions")
