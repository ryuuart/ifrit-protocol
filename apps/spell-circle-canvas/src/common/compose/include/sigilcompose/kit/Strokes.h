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
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/kit/Silhouettes.h>

#include <cmath>
#include <vector>

#include "sigilgeometry/path/Contour.h"

namespace sigil::compose::kit {



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
        geometry::path::profile::wave(amplitude, wavelength, (float)k / (float)count), ink});
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
// kit::brush::presets — finished compositions with craft names
//
// Peers of the shapers in MECHANICS — free functions over the public API,
// nothing reaching inside — and not peers of them in kind: a shaper is
// vocabulary, a preset is a finished drawing. They are scoped apart so the
// difference is visible at every call site: `geometry::shapes::wave` is
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
