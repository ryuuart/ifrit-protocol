#pragma once

/** @file
 * The KIT's stroke-grammar values: shapers, profiles, strand sets, spans
 * and shapes, under the concept scopes they belong to.
 *
 * **This header is NOT reached by `sigilcompose/kit/Kit.h`.** The umbrella
 * include does not pull it in, so none of the names below exist unless you
 * include this file directly.
 *
 * These are VALUES, not machinery: each is a peer of something a caller
 * could write against the same public seam. That is enforced by the build
 * rather than by convention — the kit is its own CMake library whose only
 * include path is SigilCompose's public headers, so nothing here can reach
 * a library internal even by accident.
 *
 * PRESETS live at the bottom, under `kit::brush::presets::`, and they are a
 * different KIND from everything above them: a shaper is a word of
 * vocabulary, a preset is a finished drawing with a craft name. They are
 * scoped apart so the difference is visible at the call site. A preset
 * whose name is craft jargon over a plain composition belongs among the
 * plain compositions instead.
 */

#include <include/core/SkPathBuilder.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/core/Derive.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Silhouettes.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Contour.h"

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// kit::brush::shapers — stock values for the ONE geometry seam
//
// A shaper bends the one continuous mark. Each of these is a comparable
// struct with the seam's required `shape()` member; user-written shapers
// are indistinguishable from them at the call site, which is the point.

namespace brush {
namespace shapers {

/** A smooth oscillation across the mark — the wave every wavy rule,
 *  scalloped frame and ribbon edge is made of. Also the BRAID primitive:
 *  strands that oscillate trade sides, and where they trade sides they
 *  cross (see strands::braid).
 *
 *  **As a PROFILE it is ZERO-MEAN**, which makes it a CENTRELINE — a strand
 *  path that swings either side of the boundary. It is NOT a band width: a
 *  band asks its profile for a width and this one goes negative half the
 *  time, which inverts the rails wherever it does. An undulating band is
 *  the composition — a positive offset PLUS an oscillation:
 *
 *      struct Undulating {                     // width 20, wobbling by 6
 *        kit::brush::shapers::Wave wobble{6, 40};
 *        bool operator==(const Undulating &) const = default;
 *        float max() const { return 20.0f + wobble.max(); }
 *        float across(float along) const { return 20.0f + wobble.across(along);
 * }
 *      };
 *      band(spine, across(Profile(Undulating{}))).centered();
 *
 *  `wavelength` is PX, and the profile seam is asked in FRACTIONS of arc
 *  length. There is no contour length available at that seam to convert
 *  with, so the PROFILE reading treats `wavelength` as px-per-cycle on a
 *  nominal 1000 px contour: on a spine much shorter or much longer than
 *  that, the wavelength you get is not the one you asked for. That is also
 *  why `strands::braid` takes its own phase count instead of deriving one.
 *  The SHAPER reading (`shape()`) has a real path and is exact.
 *
 *  The third member is `phase`, not a zigzag flag — the cornered
 *  oscillation is `Zigzag` below, a separate value. */
struct Wave {
  float amplitude = 4.0f, wavelength = 24.0f, phase = 0.0f;
  bool operator==(const Wave&) const = default;
  float bleed() const { return std::abs(amplitude); }
  float max() const { return std::abs(amplitude); }
  /** As a PROFILE: the same value read as a width across a spine, which
   *  is what makes a braid strand and a wavy band one vocabulary. */
  float across(float along) const {
    return amplitude * std::sin(2.0f * 3.14159265f *
                                (along / wavelengthFraction() + phase));
  }
  /** As a SHAPER: displace the path itself. */
  SkPath shape(const SkPath& p) const {
    return geometry::path::displace(p, amplitude, wavelength, false);
  }

 private:
  /** See the note on the struct: px per cycle on a nominal 1000 px contour. */
  float wavelengthFraction() const {
    return wavelength > 0 ? wavelength / 1000.0f : 0.024f;
  }
};

/** A hand-drawn wobble: the mark resampled into short segments, each
 *  pushed off true by a seeded amount (the rough.js line).
 *
 *  ONE pass of SkDiscretePathEffect. The sketchy double-line those tools
 *  draw is TWO passes — full and half deviation at different seeds — so it
 *  is two brush layers or two restyles here, never one call. */
struct Jitter {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Jitter&) const = default;
  float bleed() const { return deviation * 2.0f; }
  SkPath shape(const SkPath& p) const {
    SkPathBuilder out;
    // HAIRLINE rec is required: under a fill rec SkDiscretePathEffect
    // force-CLOSES open contours, so an open mark gains a return chord
    // from its end back to its start — which then jitters away from the
    // real run and draws as a second, phantom line.
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** A parallel displacement — the rail. Parallels never cross, which is
 *  why `layers` plus this is the double or triple line and a braid needs
 *  Wave instead.
 *
 *  **Positive is LEFT of travel.** That is the library-wide sign
 *  convention for an across-the-path offset, and it agrees exactly with
 *  `strand::offset(px)`; anything added here must match it, because a
 *  disagreement mirrors a drawing rather than erroring. */
struct Offset {
  float px = 0.0f;
  float step = 4.0f;
  bool operator==(const Offset&) const = default;
  float bleed() const { return std::abs(px); }
  SkPath shape(const SkPath& p) const {
    return geometry::path::parallel(p, px, step);
  }
};

/** ROUND EVERY CORNER of the mark (SkCornerPathEffect). Not
 *  `shapes::rounded()`, which rounds an OUTLINE GENERATOR's result: this
 *  rounds whatever path the brush pipeline is carrying, so it softens a
 *  displaced zigzag or an offset rail, not just a silhouette. */
struct Rounded {
  float radius = 6.0f;
  bool operator==(const Rounded&) const = default;
  SkPath shape(const SkPath& p) const {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** CUT EVERY CORNER of the mark at 45° (`routers::chamfer`) — Rounded's
 *  machined sibling, and the treatment SkCornerPathEffect cannot give you
 *  because it only rounds. Not `shapes::chamfered()`, which cuts an
 *  OUTLINE GENERATOR's box: this cuts whatever polyline the brush pipeline
 *  is carrying — a routed wire, a displaced zigzag, an offset rail.
 *
 *  A contour containing any curve segment passes through COMPLETELY
 *  UNTOUCHED, so this is a silent no-op over a curved mark. */
struct Chamfer {
  float cut = 6.0f;
  bool operator==(const Chamfer&) const = default;
  SkPath shape(const SkPath& p) const { return routers::chamfer(p, cut); }
};

/** THE BOXY DISPLACEMENT: a square wave across the mark — battlements,
 *  the Greek meander key, a stepped circuit trace. Wave's sibling; it has
 *  no profile reading, only a shaper one. */
struct Square {
  float amplitude = 5.0f, wavelength = 32.0f;
  bool operator==(const Square&) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath& p) const {
    return lines::displaceSquare(p, amplitude, wavelength);
  }
};

/** THE SAME OSCILLATION WITH CORNERS: `Wave` sampled as straight runs
 *  between its extremes rather than a curve — the drawn zigzag, the
 *  saw edge, the seismograph line.
 *
 *  Its own value rather than a `zigzag` flag on `Wave`, because `Wave` is
 *  ALSO read as a profile through `across()`, and a flag the profile
 *  reading had to ignore would be a silent asymmetry between the two
 *  readings of one value. */
struct Zigzag {
  float amplitude = 4.0f, wavelength = 24.0f;
  bool operator==(const Zigzag&) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath& p) const {
    return geometry::path::displace(p, amplitude, wavelength, true);
  }
};

inline Wave wave(float amplitude, float wavelength, float phase = 0.0f) {
  return Wave{amplitude, wavelength, phase};
}
inline Zigzag zigzag(float amplitude = 4.0f, float wavelength = 24.0f) {
  return Zigzag{amplitude, wavelength};
}
inline Rounded rounded(float radius = 6.0f) { return Rounded{radius}; }
inline Chamfer chamfered(float cut = 6.0f) { return Chamfer{cut}; }
inline Square square(float amplitude = 5.0f, float wavelength = 32.0f) {
  return Square{amplitude, wavelength};
}
inline Jitter jitter(float segLength = 8.0f, float deviation = 2.0f,
                     uint32_t seed = 7) {
  return Jitter{segLength, deviation, seed};
}
inline Offset offset(float px, float step = 4.0f) { return Offset{px, step}; }

}  // namespace shapers
}  // namespace brush

// ---------------------------------------------------------------------------
// kit::profile — the oscillating profile

namespace profile {
/** The wave as a PROFILE value (`across`/`max`): a strand that trades
 *  sides. The library itself ships only `strand::self()` and
 *  `strand::offset()`; everything that oscillates lives here.
 *
 *  ZERO-MEAN, so this is a strand CENTRELINE and not a band width — as a
 *  width it goes negative half the time and inverts the band's rails. See
 *  `brush::shapers::Wave` for the composition an undulating band wants
 *  instead. */
inline Profile wave(float amplitude, float wavelength, float phase = 0.0f) {
  return Profile(brush::shapers::Wave{amplitude, wavelength, phase});
}
}  // namespace profile

// ---------------------------------------------------------------------------
// kit::strands — strand SETS

namespace strands {

/** A BRAID: `n` wave strands at phase k/n, all sharing one brush.
 *
 *  Crossings by CONSTRUCTION: n oscillations of equal amplitude and
 *  wavelength at evenly spread phases must trade sides, and where they
 *  trade sides the crossing-discovery pass finds a crossing. That is why
 *  the braid primitive is the wave and not the offset — parallels are
 *  rails and never cross, so they cannot braid at all.
 *
 *  Pair it with a crossing rule to say who passes over whom:
 *  `crossing::alternate()` for a plain weave, `crossing::pairs(...)` with
 *  a cycle for an impossible braid. */
inline std::vector<sigil::compose::brush::Strand> braid(int n, float amplitude,
                                                        float wavelength,
                                                        const Decoration& ink) {
  // Fully qualified on purpose. Inside `sigil::compose::kit`, an
  // unqualified `brush::` resolves to kit::brush — the shapers scope
  // above — and NOT to the library's own brush namespace, so any name
  // from the latter has to be spelled out in full here. The parameter is
  // named `ink` rather than `brush` for the same reason.
  std::vector<sigil::compose::brush::Strand> out;
  const int count = std::max(1, n);
  out.reserve((size_t)count);
  for (int k = 0; k < count; ++k)
    out.push_back(sigil::compose::brush::Strand{
        profile::wave(amplitude, wavelength, (float)k / (float)count), ink});
  return out;
}

}  // namespace strands

// ---------------------------------------------------------------------------
// kit::spans — span values

namespace spans {
/** The reticle: a window of `arm` px at every corner and nothing else.
 *  A composition of existing span terms rather than a new kind — `Spans`
 *  is a closed value, so a kit span can only ever be a composition. */
inline Spans brackets(float arm = 18.0f, float angleDeg = 30.0f) {
  return sigil::compose::spans::corners(arm, angleDeg);
}
}  // namespace spans

// ---------------------------------------------------------------------------
// kit::shapes — silhouette values

namespace shapes {
/** A RING: the area between two concentric circles, under the plain name.
 *  Returns the annulus value itself, so it is comparable and a ring node
 *  prunes like any other shaped node. */
inline sigil::compose::shapes::Annulus ring(float innerRatio = 0.6f) {
  return sigil::compose::shapes::annulus(innerRatio);
}
}  // namespace shapes

// ---------------------------------------------------------------------------
// kit::brush::presets — finished compositions with craft names
//
// Peers of the shapers in MECHANICS — free functions over the public API,
// nothing reaching inside — and not peers of them in kind: a shaper is
// vocabulary, a preset is a finished drawing. They are scoped apart so the
// difference is visible at every call site: `kit::brush::shapers::wave` is
// a word, `kit::brush::presets::rope` is a picture.

namespace brush {
namespace presets {

/** An organic glowing filament: four strokes bottom-up — wide additive
 *  glow, mid glow, bright core, white centre. `scale` sets the envelope;
 *  at 1.0 that is a 14 px envelope over a 2.5 px core. */
inline LayeredBrush filament(SkColor4f glow = {0.435f, 0.847f, 1.0f, 1},
                             SkColor4f core = {0.875f, 0.965f, 1.0f, 1},
                             float scale = 1.0f) {
  SkColor4f g18 = glow, g45 = glow, c90 = core;
  g18.fA = 0.18f;
  g45.fA = 0.45f;
  c90.fA = 0.90f;
  return LayeredBrush{{
      {14 * scale, g18, 8 * scale, {}, 0, SkBlendMode::kPlus},
      {7 * scale, g45, 3 * scale, {}, 0, SkBlendMode::kPlus},
      {2.5f * scale, c90},
      {1 * scale, {1, 1, 1, 0.7f}},
  }};
}

/** A circuit trace. Tiers: 0 = data (1 px, 55% alpha), 1 = main (2 px,
 *  85%), 2 = power (4 px over an 8 px under-glow). Pair it with an
 *  orthogonal or octilinear router cutting its corners at 45° — see
 *  `routers::manhattan`'s `chamferCut` — for the full look. */
inline LayeredBrush circuit(SkColor4f color = {0.208f, 0.878f, 0.824f, 1},
                            int tier = 1) {
  SkColor4f c = color;
  LayeredBrush b;
  if (tier >= 2) {
    SkColor4f under = color;
    under.fA = 0.15f;
    b.layers.push_back({8, under, 4});
    c.fA = 1.0f;
    b.layers.push_back({4, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  } else if (tier == 1) {
    c.fA = 0.85f;
    b.layers.push_back({2, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  } else {
    c.fA = 0.55f;
    b.layers.push_back({1, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  }
  return b;
}

/** A three-state rope connector: counter-dashed strand layers read as
 *  twisted rope, and the Active state adds a warm halo and a specular
 *  ridge. `state` is 0 Normal, 1 Intermediate, 2 Active; out-of-range
 *  values clamp.
 *
 *  `scale` is the zoom the rope is drawn at, and every width, dash and
 *  blur moves with it together. The default suits widely-spaced nodes; a
 *  dense cluster wants around 0.6. */
inline LayeredBrush rope(int state, float scale = 1.0f) {
  struct P {
    SkColor4f body, ridge;
  };
  static constexpr P kStates[3] = {
      {{0.227f, 0.200f, 0.165f, 1},
       {0.341f, 0.286f, 0.227f, 1}},  // #3A332A/#57493A
      {{0.420f, 0.353f, 0.251f, 1},
       {0.553f, 0.459f, 0.314f, 1}},  // #6B5A40/#8D7550
      {{0.541f, 0.447f, 0.282f, 1},
       {0.780f, 0.659f, 0.420f, 1}},  // #8A7248/#C7A86B
  };
  const P& p = kStates[state < 0 ? 0 : state > 2 ? 2 : state];
  SkColor4f bodyLit = {p.body.fR * 1.15f, p.body.fG * 1.15f, p.body.fB * 1.15f,
                       1};
  SkColor4f ridgeLit = {p.ridge.fR * 1.3f, p.ridge.fG * 1.3f, p.ridge.fB * 1.3f,
                        0.6f};
  const float k = scale <= 0 ? 1.0f : scale;
  LayeredBrush b;
  if (state >= 2)
    b.layers.push_back({18 * k, {1.0f, 0.788f, 0.439f, 0.13f}, 6 * k});  // halo
  b.layers.push_back({11 * k, p.body, 0, {}, 0, SkBlendMode::kSrcOver, false});
  b.layers.push_back({7 * k, p.ridge, 0, {7 * k, 5 * k}, 0});       // strand
  b.layers.push_back({7 * k, bodyLit, 0, {7 * k, 5 * k}, 6 * k});   // counter
  b.layers.push_back({2 * k, ridgeLit, 0, {7 * k, 5 * k}, 3 * k});  // ridge
  return b;
}

/** The pulse-travel profile as a brush: plus-blended halo, coloured body,
 *  white-hot core. Claim a SHORT window of a rail
 *  (`spans::wrap(&phase, &phaseEnd)`) and march the window along it — the
 *  energy packet on any connector. */
inline LayeredBrush pulse(SkColor4f halo = {1.0f, 0.79f, 0.44f, 0.35f},
                          SkColor4f core = {1, 1, 1, 0.9f},
                          float scale = 1.0f) {
  SkColor4f body = halo;
  body.fA = std::min(1.0f, halo.fA * 2.2f);
  return LayeredBrush{{
      {12 * scale, halo, 5 * scale, {}, 0, SkBlendMode::kPlus},
      {5 * scale, body, 2 * scale, {}, 0, SkBlendMode::kPlus},
      {2 * scale, core},
  }};
}

}  // namespace presets
}  // namespace brush

}  // namespace sigil::compose::kit
