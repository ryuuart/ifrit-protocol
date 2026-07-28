#pragma once

/** @file
 * The KIT's stroke-grammar values: convenient shapers, spans, shapes,
 * profiles and strand sets, under the concept scopes they belong to.
 *
 * These are VALUES, not machinery. Every one is a peer of something you
 * could write yourself against the same seam — that is the tier rule, and
 * it is enforced structurally: SigilComposeKit is its own CMake library
 * whose only include path is compose's PUBLIC headers, so a kit value
 * cannot reach ComposeInternal.h even by accident.
 *
 * PRESETS live at the bottom, under `kit::brush::presets::`, and they are
 * a DIFFERENT tier from everything above: a shaper is a peer of something
 * you would write against a seam, a preset is a finished composition with
 * a craft name. The end state §33 rules for them is an EXTERNAL loadable
 * kit; no such mechanism is built, and the four that came out of core with
 * R2 (`filament`, `circuit`, `rope`, `pulse`) had to leave `brushes::`,
 * because that namespace dies with R3. So they sit here, in their own
 * scope, saying what they are — one move, not two, and the second move is
 * a change of home rather than a change of name. Standing check unchanged:
 * a preset whose name is craft jargon over a plain composition gets
 * demoted (the `cased` treatment).
 */

#include "sigilcompose/Brushes.h"
#include "sigilcompose/Compose.h"
#include "sigilcompose/Lines.h"
#include "sigilcompose/Routers.h"
#include "sigilcompose/Shapes.h"

#include <include/core/SkPathBuilder.h>
#include <include/core/SkStrokeRec.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>

#include <cmath>
#include <vector>

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
 *        float across(float along) const { return 20.0f + wobble.across(along); }
 *      };
 *      band(spine, across(Profile(Undulating{}))).centered();
 *
 *  `wavelength` is PX, and the profile seam is asked in FRACTIONS of arc
 *  length — there is no length at that seam to convert with, so the profile
 *  reading treats `wavelength` as px-per-cycle on a nominal 1000 px
 *  contour. That is the reason `strands::braid` takes its own phase count
 *  rather than deriving one, and the reason a wave profile on a very short
 *  or very long spine will not have the wavelength you asked for. The
 *  SHAPER reading (`shape()`) has a real path and is exact. */
/** THE THIRD MEMBER IS `phase`. The `ops::Wave` this replaced spelled a
 *  `bool zigzag` there, so `Wave{4, 28, true}` meant two different
 *  drawings in the two spellings and compiled both ways. `ops::` is gone
 *  (R3) and the trap with it, but it is the reason `Zigzag` below is its
 *  own value rather than a flag here. */
struct Wave {
  float amplitude = 4.0f, wavelength = 24.0f, phase = 0.0f;
  bool operator==(const Wave &) const = default;
  float bleed() const { return std::abs(amplitude); }
  float max() const { return std::abs(amplitude); }
  /** As a PROFILE: the same value read as a width across a spine, which
   *  is what makes a braid strand and a wavy band one vocabulary. */
  float across(float along) const {
    return amplitude *
           std::sin(2.0f * 3.14159265f * (along / wavelengthFraction() + phase));
  }
  /** As a SHAPER: displace the path itself. */
  SkPath shape(const SkPath &p) const {
    return lines::displace(p, amplitude, wavelength, false);
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
 *  ONE pass of SkDiscretePathEffect (grounded params in REFERENCES.md §9:
 *  deviation ~ 2). rough.js draws TWO — full and half deviation at
 *  different seeds — so the sketchy double-line is two brush layers, or
 *  two restyles, never one call. */
struct Jitter {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Jitter &) const = default;
  float bleed() const { return deviation * 2.0f; }
  SkPath shape(const SkPath &p) const {
    SkPathBuilder out;
    // HAIRLINE rec: under a fill rec SkDiscretePathEffect force-CLOSES
    // open contours — the transit study's phantom river channel (the
    // return chord braided the real run under jitter divergence).
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** A parallel displacement — the rail. Parallels never cross, which is
 *  why `layers` plus this is the double/triple line and a braid needs
 *  Wave instead.
 *
 *  **Positive is LEFT of travel**, the one convention (DESIGN.md; ROADMAP
 *  §33 ruling 5). It agrees with `strand::offset(px)` exactly — the two
 *  used to mean opposite sides, which is what R3's sign port ended. */
struct Offset {
  float px = 0.0f;
  float step = 4.0f;
  bool operator==(const Offset &) const = default;
  float bleed() const { return std::abs(px); }
  SkPath shape(const SkPath &p) const {
    return lines::offsetAcross(p, px, step);
  }
};

/** ROUND EVERY CORNER of the mark (SkCornerPathEffect). Not
 *  `shapes::rounded()`, which rounds an OUTLINE GENERATOR's result: this
 *  rounds whatever path the brush pipeline is carrying, so it softens a
 *  displaced zigzag or an offset rail, not just a silhouette.
 *
 *  Was the twin of `ops::Rounded`, and the reason it exists: §33 wanted
 *  the `ops::` family gone, and could not have it while `Rounded` and
 *  `Square` had no taught spelling — deleting them would have removed two
 *  capabilities with nothing to say instead. R3 deleted the twins; this
 *  now holds the body. */
struct Rounded {
  float radius = 6.0f;
  bool operator==(const Rounded &) const = default;
  SkPath shape(const SkPath &p) const {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  }
};

/** CUT EVERY CORNER of the mark at 45° (`routers::chamfer`) — Rounded's
 *  machined sibling, the game-UI corner (ROADMAP §8: SkCornerPathEffect
 *  only rounds). Not `shapes::chamfered()`, which cuts an OUTLINE
 *  GENERATOR's box: this cuts whatever polyline the brush pipeline is
 *  carrying — a routed wire, a displaced zigzag, an offset rail. Curved
 *  contours pass through untouched. */
struct Chamfer {
  float cut = 6.0f;
  bool operator==(const Chamfer &) const = default;
  SkPath shape(const SkPath &p) const { return routers::chamfer(p, cut); }
};

/** THE BOXY DISPLACEMENT: a square wave across the mark — battlements,
 *  the Greek meander key, a stepped circuit trace. Wave's sibling, and the
 *  other half of what unblocked the `ops::` deletion. */
struct Square {
  float amplitude = 5.0f, wavelength = 32.0f;
  bool operator==(const Square &) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath &p) const {
    return lines::displaceSquare(p, amplitude, wavelength);
  }
};

/** THE SAME OSCILLATION WITH CORNERS: `Wave` sampled as straight runs
 *  between its extremes rather than a curve — the drawn zigzag, the
 *  saw edge, the seismograph line.
 *
 *  Its own value rather than a `zigzag` flag on `Wave`, because Wave is
 *  ALSO read as a profile (`across()`) and a flag that the profile reading
 *  ignored would be a silent asymmetry. Found by the R2 port: the
 *  `ops::` deletion turned out to need THREE twins, not the two §33
 *  named — `ops::Wave{.zigzag = true}` was the third gap, and it had a
 *  live corpus site (the gallery's pipeline trio). */
struct Zigzag {
  float amplitude = 4.0f, wavelength = 24.0f;
  bool operator==(const Zigzag &) const = default;
  float bleed() const { return std::abs(amplitude); }
  SkPath shape(const SkPath &p) const {
    return lines::displace(p, amplitude, wavelength, true);
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
inline Offset offset(float px, float step = 4.0f) {
  return Offset{px, step};
}

} // namespace shapers
} // namespace brush

// ---------------------------------------------------------------------------
// kit::profile — the oscillating profile, kept out of core per the tier rule

namespace profile {
/** The wave as a PROFILE value (`across`/`max`): a band that undulates, a
 *  strand that trades sides. Core ships only `strand::self()` and
 *  `strand::offset()`; everything that oscillates lives here. */
inline Profile wave(float amplitude, float wavelength, float phase = 0.0f) {
  return Profile(brush::shapers::Wave{amplitude, wavelength, phase});
}
/** ZERO-MEAN, so this is a strand CENTRELINE, not a band width — see
 *  brush::shapers::Wave for why, and for the band spelling. */
} // namespace profile

// ---------------------------------------------------------------------------
// kit::strands — strand SETS

namespace strands {

/** A BRAID: `n` wave strands at phase k/n, all sharing one brush.
 *
 *  Crossings by CONSTRUCTION — n oscillations of equal amplitude and
 *  wavelength at evenly spread phases must trade sides, and where they
 *  trade sides the discovery pass finds a crossing. That is why the braid
 *  primitive is the wave and not the offset: `strands::parallel` was
 *  removed precisely because parallels are rails and cannot braid.
 *
 *  Pair it with a crossing rule to say who passes over whom —
 *  `crossing::alternate()` for plain weave, `crossing::pairs(...)` with a
 *  cycle for the impossible braid. */
inline std::vector<sigil::compose::brush::Strand>
braid(int n, float amplitude, float wavelength, Decoration ink) {
  // Fully qualified, and the brush parameter is `ink`: inside kit,
  // `brush::` means kit::brush (the shapers scope), so the composite's
  // namespace has to be spelled out. Same friction family as `band`
  // shadowing locals — a short good noun collides.
  std::vector<sigil::compose::brush::Strand> out;
  const int count = std::max(1, n);
  out.reserve((size_t)count);
  for (int k = 0; k < count; ++k)
    out.push_back(sigil::compose::brush::Strand{
        profile::wave(amplitude, wavelength, (float)k / (float)count), ink});
  return out;
}

} // namespace strands

// ---------------------------------------------------------------------------
// kit::spans — span values

namespace spans {
/** The reticle: a window of `arm` px at every corner and nothing else.
 *  A COMPOSITION of core terms, not a new kind — which is what a kit span
 *  can be, and the reason `Spans` is a closed value. */
inline Spans brackets(float arm = 18.0f, float angleDeg = 30.0f) {
  return sigil::compose::spans::corners(arm, angleDeg);
}
} // namespace spans

// ---------------------------------------------------------------------------
// kit::shapes — silhouette values

namespace shapes {
/** A RING: the area between two concentric circles. "Annulus" was
 *  rejected as jargon for exactly the shape everybody calls a ring.
 *  A comparable value (it IS the annulus value), so a ring node prunes. */
inline sigil::compose::shapes::Annulus ring(float innerRatio = 0.6f) {
  return sigil::compose::shapes::annulus(innerRatio);
}
} // namespace shapes

// ---------------------------------------------------------------------------
// kit::brush::presets — finished compositions with craft names
//
// Peers of the shapers in TIER MECHANICS (free functions over the public
// API, nothing reaches inside) and NOT peers of them in kind: a shaper is
// vocabulary, a preset is a finished drawing. They are scoped apart so the
// difference is visible at every call site — `kit::brush::shapers::wave`
// is a word, `kit::brush::presets::rope` is a picture of Path of Exile's
// rope. All four moved here from core's `brushes::` in R2 (ROADMAP §33),
// unchanged: same layers, same numbers, same references; R3 deleted the
// old namespace and with it the last spelling that was not this one.

namespace brush {
namespace presets {

/** Ori-style organic filament (REFERENCES.md §5): four strokes bottom-up —
 *  wide additive glow, mid glow, bright core, white center. Scale sets the
 *  envelope (1.0 → 14px envelope over a 2.5px core). */
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

/** FUI circuit trace (REFERENCES.md §5). Tiers: 0 = data (1px, 55%),
 *  1 = main (2px, 85%), 2 = power (4px + 8px under-glow). Pair with an
 *  octilinear-ish router with 45° chamfers for the full look. */
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

/** Path of Exile's rope connector, 3-state (REFERENCES.md §5 — palette
 *  ladder verified against Path of Building): counter-dashed strand layers
 *  read as rope; Active adds the warm halo and specular ridge. state:
 *  0 Normal, 1 Intermediate (hover-path), 2 Active.
 *
 *  `scale` is the zoom the rope is drawn at — every width, dash and blur
 *  moves together, the way the game's own line art does. The default is
 *  the widely-spaced study; a dense cluster wants ~0.6. */
inline LayeredBrush rope(int state, float scale = 1.0f) {
  struct P { SkColor4f body, ridge; };
  static constexpr P kStates[3] = {
      {{0.227f, 0.200f, 0.165f, 1}, {0.341f, 0.286f, 0.227f, 1}}, // #3A332A/#57493A
      {{0.420f, 0.353f, 0.251f, 1}, {0.553f, 0.459f, 0.314f, 1}}, // #6B5A40/#8D7550
      {{0.541f, 0.447f, 0.282f, 1}, {0.780f, 0.659f, 0.420f, 1}}, // #8A7248/#C7A86B
  };
  const P &p = kStates[state < 0 ? 0 : state > 2 ? 2 : state];
  SkColor4f bodyLit = {p.body.fR * 1.15f, p.body.fG * 1.15f,
                       p.body.fB * 1.15f, 1};
  SkColor4f ridgeLit = {p.ridge.fR * 1.3f, p.ridge.fG * 1.3f,
                        p.ridge.fB * 1.3f, 0.6f};
  const float k = scale <= 0 ? 1.0f : scale;
  LayeredBrush b;
  if (state >= 2)
    b.layers.push_back({18 * k, {1.0f, 0.788f, 0.439f, 0.13f}, 6 * k}); // halo
  b.layers.push_back(
      {11 * k, p.body, 0, {}, 0, SkBlendMode::kSrcOver, false});
  b.layers.push_back({7 * k, p.ridge, 0, {7 * k, 5 * k}, 0});   // strand
  b.layers.push_back({7 * k, bodyLit, 0, {7 * k, 5 * k}, 6 * k}); // counter
  b.layers.push_back({2 * k, ridgeLit, 0, {7 * k, 5 * k}, 3 * k}); // ridge
  return b;
}

/** The §5 pulse-travel profile as a brush: plus-blended halo, colored
 *  body, white-hot core. Claim a SHORT window of a rail
 *  (`spans::wrap(&phase, &phaseEnd)`) and march the window along it —
 *  the energy packet on any connector. */
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

} // namespace presets
} // namespace brush

} // namespace sigil::compose::kit
