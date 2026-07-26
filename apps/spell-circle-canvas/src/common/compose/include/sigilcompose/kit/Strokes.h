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
 * What is deliberately NOT here: PRESETS. `cased`, `railway`, `rope`,
 * `GlossContour` and their relatives are compositions with craft names,
 * and they belong in external loadable kits (`stroke_atlas` stays the
 * in-repo specimen page). Standing check: a preset whose name is craft
 * jargon over a plain composition gets demoted — the `cased` treatment.
 */

#include "sigilcompose/Brushes.h"
#include "sigilcompose/Compose.h"
#include "sigilcompose/Lines.h"
#include "sigilcompose/Shapes.h"

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
 *  pushed off true by a seeded amount (the rough.js line). */
struct Jitter {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Jitter &) const = default;
  float bleed() const { return deviation * 2.0f; }
  SkPath shape(const SkPath &p) const {
    return ops::Sketchy{segLength, deviation, seed}.apply(p);
  }
};

/** A parallel displacement — the rail. Parallels never cross, which is
 *  why `layers` plus this is the double/triple line and a braid needs
 *  Wave instead.
 *
 *  **Positive is RIGHT of travel**, because this wraps `lines::offsetAlong`
 *  unchanged (§27). That is the OPPOSITE of `strand::offset`, which is
 *  left-of-travel in the band's frame. The split predates both (see
 *  `bandPointAt`) and its reconciliation is the designer's call; until then
 *  the two `offset`s genuinely mean different sides, and this is the one
 *  place both are named together. */
struct Offset {
  float px = 0.0f;
  float step = 4.0f;
  bool operator==(const Offset &) const = default;
  float bleed() const { return std::abs(px); }
  SkPath shape(const SkPath &p) const {
    return lines::offsetAlong(p, px, step);
  }
};

inline Wave wave(float amplitude, float wavelength, float phase = 0.0f) {
  return Wave{amplitude, wavelength, phase};
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
 *  rejected as jargon for exactly the shape everybody calls a ring. */
inline sigil::compose::shapes::OutlineFn ring(float innerRatio = 0.6f) {
  return sigil::compose::shapes::annulus(innerRatio);
}
} // namespace shapes

} // namespace sigil::compose::kit
