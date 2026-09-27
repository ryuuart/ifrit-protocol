#pragma once

/** @file
 * @ingroup compose-typography
 *
 * SigilCompose typography — THE ENTRANCE: the lanes a glyph is displaced on
 * (`textFx::Displaced`), the path home one `motion::Tween` of them makes
 * (`textFx::enter`, an effect over one unit of local progress), and that
 * tween whole on a track (`textFx::entrance`, its timing the track's
 * schedule).
 *
 *   text(u8"KINETIC", display)
 *       .textFx(textFx::entrance(
 *           {.from = textFx::Displaced{.dy = 26}, .duration = 480ms,
 *            .delay = motion::stagger(28ms), .ease = motion::ease::outExpo},
 *           {.progress = with(1.0f, {900ms, motion::ease::outQuad})}));
 *
 * The stock entrances over it — rise, slide, pop, spinIn, scatter, typeOn,
 * variableAxisSweep — are plain tween values in
 * <sigilcompose/typography/Presets.h>. Every effect built here carries
 * whether it MOVES its glyphs off the pen positions the layout gave them
 * (`TextEffect::displaces`), read off the stops: offsets, leans, growth
 * and scatter do; opacity and an axis coordinate do not.
 */

#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/compute/Noise.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Tween.h>
#include <sigilweave/style/ShapingStyle.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::compose::textFx {

/** WHERE A GLYPH STANDS on its way home: the lanes it is displaced on,
 *  which an entrance tween moves from its `.from` to home (`Displaced{}`,
 *  its `.to` unless one is named). `scatterPx` and `scatterLeanDeg` are
 *  the lanes that are not a constant: each glyph draws its own offset
 *  inside a `scatterPx` disc, and its own lean up to `scatterLeanDeg`, from
 *  the stream seeded on the glyph's own identity — so the draw is stable
 *  across frames and relayouts, which is what lets a settled scatter cache
 *  instead of jittering forever. */
struct Displaced {
  float dx = 0;              ///< px to the side the glyph comes in from
  float dy = 0;              ///< px below (positive) or above its rest
  float rotateDeg = 0;       ///< the lean it straightens out of
  float scale = 1;           ///< the size it grows from (1 = no growth)
  float scatterPx = 0;       ///< seeded per-glyph disc, added to dx/dy
  float scatterLeanDeg = 0;  ///< seeded per-glyph lean, added to rotateDeg
  /** The share of the unit's pass its opacity takes to come up, straight
   *  and whatever the curve; 0 enters opaque. A glyph at home is opaque,
   *  so this is read from the tween's `.from` alone. */
  float fadeOver = 0.35f;
  /** The opacity the glyph stands at, multiplied over the fade above — the
   *  lane a coverage-only entrance moves, the fade share being a ramp over
   *  time rather than a place on the path. 1 is fully there. */
  float opacity = 1;
  /** A variable-font axis coordinate the glyph is drawn at, applied at draw
   *  time as `GlyphModifier::axis` is and under its rule: only an
   *  ADVANCE-INVARIANT axis is honoured, because the glyph keeps the pen
   *  position shaping gave it. Unset: the shaped face. */
  std::optional<sigil::weave::FontVariation> axis;

  bool operator==(const Displaced&) const = default;
};

/** THE LINE A GLYPH TRAVELS HOME ON, which the tween's `interpolate()`
 *  finds: every transform lane and the opacity straight, the fade share
 *  held from the start, and the axis under `GlyphModifier`'s rule — lerped
 *  between two stops naming the same tag, cut at the middle of the segment
 *  otherwise. */
[[nodiscard]] inline Displaced interpolate(const Displaced& start,
                                           const Displaced& end, float amount) {
  const auto lane = [amount](float from, float to) {
    return from + (to - from) * amount;
  };
  return {lane(start.dx, end.dx),
          lane(start.dy, end.dy),
          lane(start.rotateDeg, end.rotateDeg),
          lane(start.scale, end.scale),
          lane(start.scatterPx, end.scatterPx),
          lane(start.scatterLeanDeg, end.scatterLeanDeg),
          start.fadeOver,
          lane(start.opacity, end.opacity),
          compose::detail::interpolateAxis(start.axis, end.axis, amount)};
}

/** How far past its ends a curve reaches over [0, 1] — the overshoot of a
 *  back or elastic ease, zero for one that stays inside. Sampled, because a
 *  curve is any function. */
[[nodiscard]] inline float overshootOf(const motion::Easing& ease) {
  if (!ease) return 0.0f;
  float excess = 0.0f;
  constexpr int kSamples = 256;
  for (int index = 0; index <= kSamples; ++index) {
    const float value = ease((float)index / (float)kSamples);
    excess = std::max({excess, value - 1.0f, -value});
  }
  // Below this a curve is inside to its own rounding, not overshooting.
  constexpr float kRounding = 1e-4f;
  return excess > kRounding ? excess : 0.0f;
}

/** THE PATH A GLYPH TAKES HOME, as an effect: the tween's `.from` (a
 *  `Displaced`), its `.to` (home, `Displaced{}`, unless one is named), its
 *  `.keyframes` and its `.ease`, run over one unit of local progress. This
 *  is the form the combinators take — `textFx::sequence`, `textFx::mix`,
 *  `textFx::hold`, `.until()` — where the track around it says when a unit
 *  runs; `entrance()` below is the whole tween on a track, its timing the
 *  track's schedule. A keyframe's own duration is a share of the tween's.
 *
 *      textFx::sequence(textFx::enter(textFx::rise(20)).until(0.5f),
 *                       textFx::enter(textFx::pop())) */
[[nodiscard]] inline TextEffect enter(motion::Tween<Displaced> tween) {
  const Displaced from = tween.from ? tween.from->value() : Displaced{};
  // Home is where an entrance rests unless the tween names another place.
  if (!tween.to && tween.keyframes.empty()) tween.to = Displaced{};
  // The glyph's own path, over one unit of local progress: the track's
  // schedule decides when that unit runs, so the shape starts at once,
  // plays once and lasts exactly the unit.
  motion::Tween<Displaced> shape = tween;
  shape.duration = std::chrono::seconds(1);
  // A stop's own length is a share of the unit's, in the same measure.
  const double unitSeconds = tween.duration.value().count();
  for (motion::Keyframe<Displaced>& step : shape.keyframes)
    if (step.duration && unitSeconds > 0.0)
      *step.duration = *step.duration / unitSeconds;
  shape.delay = motion::Duration{};
  shape.loop = 0;
  shape.loopDelay = {};
  shape.alternate = false;
  shape.composition = motion::Composition::Replace;
  const motion::Easing ease = shape.easing();

  // A rotated or scaled glyph swings its corners out of its advance box;
  // half the nominal size covers any angle. A curve that overshoots
  // carries the displacement, and the glyph itself, that share past home.
  std::vector<Displaced> stops{from};
  if (tween.to) stops.push_back(tween.to->value());
  for (const motion::Keyframe<Displaced>& step : tween.keyframes)
    stops.push_back(step.to);
  float travel = 0.0f, reach = 0.0f;
  bool leans = false, moves = false;
  for (const Displaced& stop : stops) {
    travel = std::max(travel, std::max(std::abs(stop.dx), std::abs(stop.dy)) +
                                  std::abs(stop.scatterPx));
    leans |= stop.rotateDeg != 0 || stop.scatterLeanDeg != 0;
    moves |= stop.dx != 0 || stop.dy != 0 || stop.rotateDeg != 0 ||
             stop.scale != 1 || stop.scatterPx != 0 || stop.scatterLeanDeg != 0;
  }
  reach = travel;
  if (leans) reach += kNominalSizePx * 0.5f;
  std::vector<motion::Easing> curves{ease};
  for (const motion::Keyframe<Displaced>& step : tween.keyframes)
    if (step.ease) {
      curves.push_back(step.ease);
      reach += overshootOf(step.ease) * (travel + kNominalSizePx);
    }
  reach += overshootOf(ease) * (travel + kNominalSizePx);

  // What two entrances compare by: every stop's lanes, where they fall,
  // and the curves beside them.
  std::vector<float> parameters;
  const auto describe = [&parameters](const Displaced& stop) {
    parameters.insert(parameters.end(),
                      {stop.dx, stop.dy, stop.rotateDeg, stop.scale,
                       stop.scatterPx, stop.scatterLeanDeg, stop.fadeOver,
                       stop.opacity, stop.axis ? 1.0f : 0.0f});
    const sigil::weave::FontVariation axis =
        stop.axis.value_or(sigil::weave::FontVariation());
    for (const char byte : axis.tag)
      parameters.push_back((float)(unsigned char)byte);
    parameters.push_back(axis.value);
  };
  for (const Displaced& stop : stops) describe(stop);
  for (const motion::Keyframe<Displaced>& step : shape.keyframes)
    parameters.push_back(step.duration ? (float)step.duration->count() : -1.0f);

  return TextEffect(
      "enter", std::move(parameters),
      [shape = std::move(shape), from](const GlyphInfo&, float t,
                                       core::noise::Mix64Stream& rng) {
        const Displaced now = shape.at(motion::Duration(t));
        float dx = now.dx, dy = now.dy, lean = now.rotateDeg;
        // The stream is drawn in one order whatever the progress, so a
        // glyph keeps its own scatter all the way home.
        if (from.scatterPx != 0) {
          dx += rng.signedUnit() * now.scatterPx;
          dy += rng.signedUnit() * now.scatterPx;
        }
        if (from.scatterLeanDeg != 0)
          lean += rng.signedUnit() * now.scatterLeanDeg;
        GlyphModifier m;
        m.dx = dx;
        m.dy = dy;
        m.rotateDeg = lean;
        if (from.scale != 1) m.scale = now.scale;
        m.alpha = now.opacity *
                  (from.fadeOver > 0 ? std::min(1.0f, t / from.fadeOver) : 1.0f);
        m.axis = now.axis;
        return m;
      },
      reach, std::move(curves), moves);
}

/** THE ENTRANCE, as a track: one `motion::Tween` says both what every
 *  glyph does and when — `.from` where it starts (a `Displaced`), `.ease`
 *  the curve it comes home on, `.keyframes` any stops on the way (the
 *  path `enter()` runs), and `.duration`, `.delay` (a `motion::stagger()`
 *  or `motion::cues()` over the glyphs as siblings), `.loop`,
 *  `.loopDelay` and `.alternate` the track's schedule. @p track supplies
 *  the rest of the track — which glyphs, what a unit is, the master
 *  progress — and its effect and tween are this entrance's.
 *
 *      text(u8"KINETIC", display)
 *          .textFx(textFx::entrance({.from = textFx::Displaced{.dy = 26},
 *                                    .duration = 480ms,
 *                                    .delay = motion::stagger(28ms),
 *                                    .ease = motion::ease::outBack()}))
 *
 *  The stock entrances are tween values, in
 *  <sigilcompose/typography/Presets.h>: `entrance(textFx::pop())`. What
 *  differs between a rise, a slide and a tumble is only WHICH LANES carry
 *  the displacement, so they are values and not six bodies. */
[[nodiscard]] inline Track entrance(const motion::Tween<Displaced>& tween,
                                    Track track = {}) {
  track.effect = enter(tween);
  track.tween = {.duration = tween.duration,
                 .delay = tween.delay,
                 .loop = tween.loop,
                 .loopDelay = tween.loopDelay,
                 .alternate = tween.alternate};
  return track;
}

}  // namespace sigil::compose::textFx
