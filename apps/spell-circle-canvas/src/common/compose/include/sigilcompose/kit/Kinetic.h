#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose KIT — kinetic type: the entrance a track is made of — one
 * `motion::Tween` of a glyph's `Displaced` lanes, its timing the track's —
 * with the stock entrances as plain tween VALUES, and the loops and sweeps
 * for the kernel's multi-track `textFx()` seam as comparable `TextEffect`
 * values built from the same constructor any caller may use.
 *
 * One track:
 *
 *   text(u8"KINETIC", display)
 *       .textFx(textFx::entrance(
 *           {.from = textFx::Displaced{.dy = 26}, .duration = 480ms,
 *            .delay = motion::stagger(28ms), .ease = motion::ease::outExpo},
 *           {.progress = with(1.0f, {900ms, motion::ease::outQuad})}));
 *
 * Several tracks compose per glyph — offsets and rotations add, scale and
 * alpha multiply — and each carries its own selector, schedule and
 * progress:
 *
 *   text(u8"ONE LINE, TWO MOVES", display)
 *       .textFx(textFx::entrance(textFx::rise(20), {.unit = weave::Unit::Word}))
 *       .textFx({.where = weave::selectors::text(u8"TWO"),
 *            .effect = textFx::waveLoop(),
 *            .progress = &phase});
 *
 * The effects the runtime itself evaluates — `textFx::keys`,
 * `textFx::sequence`, `textFx::mix`, `textFx::hold`, `textFx::scramble`,
 * `textFx::pass` and the `textFx::effect` door — are the seam's, declared with
 * it in <sigilcompose/typography/TextEffect.h>, with the structural catalogue
 * beside it in <sigilcompose/typography/TextFx.h>; this header holds the
 * presets,
 * which are values over that seam and need nothing it does not expose.
 *
 * One-shot effects consume progress 0→1; loop effects (waveLoop) read a
 * WRAPPING bound phase (a live value stepped mod 1), and a looping SCHEDULE
 * (a track tween's `loop`) reads the same wrapping phase and re-opens
 * every unit's beat once per wrap. Everything renders through batched
 * RSXform draws — moving text is never per-glyph draw calls — and every
 * preset declares the reach its motion needs so the recording cull does
 * not truncate it.
 *
 * Every effect here also carries whether it MOVES its glyphs off the pen
 * positions the layout gave them (`TextEffect::displaces`), which is what
 * decides the grid a live run's origins are rounded to. An entrance that
 * displaces (`rise`, `slide`, `pop`, `spinIn`, `scatter`) and `waveLoop`
 * move them; `typeOn`,
 * `variableAxisSweep` and `tint` do not.
 */

#include <sigilmotion/ease/Ease.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Track.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilmotion/values/Tween.h>
#include <sigilweave/style/ShapingStyle.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

namespace sigil::compose::textFx {

// A scale-only effect's reach is read against `kNominalSizePx`, the
// display size the seam declares reaches at when no effect knows its font
// size at construction.

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

  bool operator==(const Displaced&) const = default;
};

/** THE LINE A GLYPH TRAVELS HOME ON, which the tween's `interpolate()`
 *  finds: every transform lane straight, the fade share held from the
 *  start. */
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
          start.fadeOver};
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
                       stop.scatterPx, stop.scatterLeanDeg, stop.fadeOver});
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
        m.alpha = from.fadeOver > 0 ? std::min(1.0f, t / from.fadeOver) : 1.0f;
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
 *  The stock entrances below are tween values: `entrance(textFx::pop())`.
 *  What differs between a rise, a slide and a tumble is only WHICH LANES
 *  carry the displacement, so they are values here and not six bodies. */
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

/** The track's own timing a stock entrance carries: 450ms per glyph, one
 *  glyph 30ms after the one before. */
inline constexpr std::chrono::milliseconds kEntranceDuration{450};
inline constexpr std::chrono::milliseconds kEntranceStep{30};

/** The stagger-reveal workhorse: glyphs rise from `distancePx` below their
 *  rest while fading in. Ease-out-expo motion; alpha completes over the
 *  first 35% of local progress, so a glyph is fully opaque while it is
 *  still moving rather than fading and settling together. */
[[nodiscard]] inline motion::Tween<Displaced> rise(float distancePx = 26) {
  return {.from = Displaced{.dy = distancePx},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outExpo};
}

/** Slide-in from the side (negative = from the left). */
[[nodiscard]] inline motion::Tween<Displaced> slide(float distancePx = -32) {
  return {.from = Displaced{.dx = distancePx, .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Scale-overshoot entrance (back.out(1.7) — the elastic pop). */
[[nodiscard]] inline motion::Tween<Displaced> pop(float fromScale = 0.35f,
                                                  float overshoot = 1.70158f) {
  return {.from = Displaced{.scale = fromScale, .fadeOver = 1.0f / 2.2f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outBack(overshoot)};
}

/** Tumble-in: glyphs spin from `degrees` while rising and fading. */
[[nodiscard]] inline motion::Tween<Displaced> spinIn(float degrees = 70,
                                                     float risePx = 14) {
  return {.from = Displaced{.dy = risePx, .rotateDeg = degrees,
                            .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Seeded scatter: every glyph flies in from its own random offset inside
 *  a `radiusPx` disc, with its own random lean. */
[[nodiscard]] inline motion::Tween<Displaced> scatter(float radiusPx = 40,
                                                      float leanDeg = 24) {
  return {.from = Displaced{.scatterPx = radiusPx, .scatterLeanDeg = leanDeg,
                            .fadeOver = 1.0f / 1.7f},
          .duration = kEntranceDuration,
          .delay = motion::stagger(kEntranceStep),
          .ease = motion::ease::outCubic};
}

/** Hard typewriter: a glyph is absent, then simply THERE (pair with a
 *  short duration and a first-to-last stagger). */
[[nodiscard]] inline TextEffect typeOn() {
  return TextEffect(
      "typeOn", {},
      [](const GlyphInfo&, float t, core::noise::Mix64Stream&) {
        GlyphModifier m;
        m.alpha = t >= 0.5f ? 1.0f : 0.0f;
        return m;
      },
      // Coverage only: the glyph appears where it already was.
      0.0f, {}, /*displaces=*/false);
}

/** Endless float: glyph i bobs on a sine, phase-shifted per glyph. Bind
 *  progress to a WRAPPING phase value (t = fract(seconds / period)) and
 *  give the track a tween with no delay (`.tween = {}`) so every glyph
 *  reads the same master phase.
 *  Amplitude is in EM — keep it at or under 0.15em, past which descenders
 *  of adjacent glyphs collide — and the phase shift is RADIANS per glyph,
 *  where roughly 0.4–0.6 gives one readable travelling wave. */
[[nodiscard]] inline TextEffect waveLoop(float amplitudeEm = 0.10f,
                                         float phaseRadPerGlyph = 0.5f) {
  return TextEffect(
      "waveLoop", {amplitudeEm, phaseRadPerGlyph},
      [amplitudeEm, phaseRadPerGlyph](const GlyphInfo& g, float t,
                                      core::noise::Mix64Stream&) {
        GlyphModifier m;
        m.dy = std::sin(t * geometry::path::kTau -
                        (float)g.index * phaseRadPerGlyph) *
               amplitudeEm * (g.fontSize > 0 ? g.fontSize : 16.0f);
        return m;
      },
      std::abs(amplitudeEm) * kNominalSizePx);
}

/** A variable-font axis SWEPT across local progress: `from` at t = 0,
 *  `to` at t = 1. Pair it with a stagger and a weight rolls along the
 *  line. The held coordinate is `TextEffect::variableAxis`, on the seam,
 *  because the span verb that holds an axis is built on it. */
[[nodiscard]] inline TextEffect variableAxisSweep(const char (&tag)[5],
                                                  float from, float to) {
  const sigil::weave::FontVariation coordinate(tag, from);
  return TextEffect(
      "variableAxisSweep",
      {(float)(unsigned char)tag[0], (float)(unsigned char)tag[1],
       (float)(unsigned char)tag[2], (float)(unsigned char)tag[3], from, to},
      [coordinate, from, to](const GlyphInfo&, float t,
                             core::noise::Mix64Stream&) {
        GlyphModifier m;
        sigil::weave::FontVariation driven = coordinate;
        driven.value = from + (to - from) * std::clamp(t, 0.0f, 1.0f);
        m.axis = driven;
        return m;
      },
      0.0f, {}, /*displaces=*/false);
}

/** A COLOUR REVEAL AS A SCHEDULE: the glyphs read @p from at local 0 and
 *  @p to at local 1 — a karaoke wipe, a highlight sweeping a word, an
 *  initial catching its colour as it lands.
 *
 *  THE ELEMENT IS SET IN `to`, AND THE EFFECT MULTIPLIES DOWN TOWARD
 *  `from`. That inversion is the one thing to get right here. A `GlyphModifier`
 *  carries `colorMultiplier`, a per-channel MULTIPLIER over every pass the
 * glyph's style draws, and a multiplier can only take a colour toward black —
 * so the DESTINATION is what the style paints, and the origin is reached by
 *  dividing. The arguments still read in time order and the division is
 *  done here: `textFx::tint(pale, sung)` on a line set in `sung` wipes it from
 *  pale to sung. Set the line in `from` and it draws pale throughout,
 *  which is the obvious first mistake and has no diagnostic.
 *
 *  Multiplying is also what lets this tint a gradient-filled or
 *  image-filled line without knowing what fills it. Its cost is that a
 *  DESTINATION CHANNEL OF ZERO cannot be departed from — nothing multiplies
 *  0 into anything else — so that channel holds at 0 for the whole ramp
 *  whatever @p from says there. The way UP is the other two colour terms:
 *  `GlyphModifier::colorAdd` is the hard flash over whatever the style paints,
 *  `GlyphModifier::colorScreen` the glow that brightens toward white without
 *  clipping — both usually spoken through a `textFx::keys` table.
 *
 *  Alpha is untouched: a reveal that also fades wants an alpha track, which
 *  composes with this one. The ramp is a smoothstep because a hard cut at
 *  display size flickers at any frame rate; the width of the edge is bought
 *  with the track's `duration`, not with the curve. */
[[nodiscard]] inline TextEffect tint(material::Color from, material::Color to) {
  const material::Color origin{to.r > 0 ? from.r / to.r : 1.0f,
                               to.g > 0 ? from.g / to.g : 1.0f,
                               to.b > 0 ? from.b / to.b : 1.0f, 1.0f};
  return TextEffect(
      "tint", {from.r, from.g, from.b, from.a, to.r, to.g, to.b, to.a},
      [origin](const GlyphInfo&, float t, core::noise::Mix64Stream&) {
        const float e = motion::ease::smoothstep(t);
        GlyphModifier m;
        m.colorMultiplier = {origin.r + (1.0f - origin.r) * e,
                             origin.g + (1.0f - origin.g) * e,
                             origin.b + (1.0f - origin.b) * e, 1.0f};
        return m;
      },
      // Colour only: a wipe repaints letters, it does not move them.
      0.0f, {}, /*displaces=*/false);
}

}  // namespace sigil::compose::textFx
