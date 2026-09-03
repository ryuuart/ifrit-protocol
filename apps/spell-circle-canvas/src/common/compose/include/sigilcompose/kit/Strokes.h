#pragma once

/** @file
 * The KIT's stroke-grammar values, each under the catalog it is a member
 * of: `kit::braid`, a strand set; `spans::brackets`, a span composition
 * standing beside the kernel's own span terms; and `brush::presets::`,
 * finished brushes with craft names.
 *
 * **This header is NOT reached by `sigilcompose/kit/Kit.h`.** The umbrella
 * include does not pull it in, so none of the names below exist unless you
 * include this file directly.
 *
 * These are VALUES, not machinery: each is a peer of something a caller
 * could write against the same public seam, and every one of them is
 * spelled over SigilCompose's public headers alone.
 *
 * PRESETS live at the bottom, under `brush::presets::`, and they are a
 * different KIND from everything above them: a shaper is a word of
 * vocabulary, a preset is a finished drawing with a craft name. They are
 * scoped apart so the difference is visible at the call site. A preset
 * whose name is craft jargon over a plain composition belongs among the
 * plain compositions instead.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Stroke.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// kit::braid — a strand SET

namespace kit {

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
inline std::vector<brush::Strand> braid(int n, float amplitude,
                                        float wavelength,
                                        const Decoration& ink) {
  std::vector<brush::Strand> out;
  const int count = std::max(1, n);
  out.reserve((size_t)count);
  for (int k = 0; k < count; ++k)
    out.push_back(
        brush::Strand{geometry::path::profile::wave(amplitude, wavelength,
                                                    (float)k / (float)count),
                      ink});
  return out;
}

/** THE ENGRAVED GROOVE across a circle's stroke, as the ramp it is
 *  painted with: a radial ramp centred on the circle's own centre, dark
 *  on the inner wall and lit on the outer, so the mark reads as a cut
 *  with a shadowed wall and a lit wall — a CROSS-SECTION, which is the
 *  one paint a stroke's own colour cannot carry. It is constant along the
 *  groove and varies across it for one reason: the ramp is concentric
 *  with the circle. On any path that is not a circle about the ramp's
 *  centre the trick falls apart.
 *
 *  NODE-LOCAL, in px: the centre is `{radius, radius}`, which is where
 *  `kit::disc(centre, radius)` puts the circle in its box, so the ramp is
 *  right on a disc and on nothing else. `shoulder` is how much of the
 *  width the two walls take to meet, as a fraction of it — 0 a hard step
 *  at the floor, 0.5 a ramp the whole width across. The tones carry their
 *  own alpha, which is what sets how deep the cut reads over the surface
 *  beneath. A comparable paint, so a plate of seventy grooves prunes;
 *  `toFill` turns it into the `Fill` a `lines::Rail` takes. */
inline material::skia::Paint grooveRamp(float radius, float width,
                                        SkColor4f dark, SkColor4f lite,
                                        float shoulder = 0.22f) {
  const float reach = radius + width;
  const float inner = (radius - width * 0.5f) / reach;
  const float outer = (radius + width * 0.5f) / reach;
  const float mid = (inner + outer) * 0.5f;
  const float half = (outer - inner) * std::clamp(shoulder, 0.0f, 0.5f);
  return material::skia::Paint::radial(
      {radius, radius}, reach,
      {{0.0f, dark}, {mid - half, dark}, {mid + half, lite}, {1.0f, lite}});
}

/** The groove as the stroke a disc's outline wears: @p width px centred
 *  on the outline, painted with `grooveRamp`. */
inline PathFormat groove(float radius, float width, SkColor4f dark,
                         SkColor4f lite, float shoulder = 0.22f) {
  PathFormat cut;
  cut.width = width;
  cut.strokeMaterial = grooveRamp(radius, width, dark, lite, shoulder);
  return cut;
}

}  // namespace kit

// ---------------------------------------------------------------------------
// spans::brackets — a span composition, beside the kernel's own terms

namespace spans {
/** The reticle: a window of `arm` px at every corner and nothing else.
 *  A composition of existing span terms rather than a new kind — `Spans`
 *  is a closed value, so a kit span can only ever be a composition. */
inline Spans brackets(float arm = 18.0f, float angleDeg = 30.0f) {
  return corners(arm, angleDeg);
}
}  // namespace spans

// ---------------------------------------------------------------------------
// brush::presets — finished compositions with craft names
//
// Peers of the shapers in MECHANICS — free functions over the public API,
// nothing reaching inside — and not peers of them in kind: a shaper is
// vocabulary, a preset is a finished drawing. They are scoped apart so the
// difference is visible at every call site: `geometry::shapers::wave` is
// a word, `brush::presets::rope` is a picture.

namespace brush::presets {

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

/** The cartographic railway: a dark line under a white dash overlay at
 *  about a third of its width, on a 50% duty cycle — the map convention,
 *  which uses no ties at all. Two decorations as one LayerStyle, so attach
 *  with `Element::style()`. */
inline LayerStyle railwayCarto(float scale = 1.0f,
                               SkColor4f dark = {0.439f, 0.439f, 0.439f, 1},
                               SkColor4f light = {1, 1, 1, 1}) {
  lines::Line base;
  base.width = 3.0f * scale;
  base.fill = Fill::color(dark);
  lines::Line dashes;
  dashes.width = 1.0f * scale;
  dashes.fill = Fill::color(light);
  dashes.dashIntervals = {8.0f * scale, 8.0f * scale};
  return LayerStyle{{}, {Decoration(base), Decoration(dashes)}};
}

/** The engraver's asymmetric parallel rule: HEAVY / hair / HEAVY — the
 *  commonest printed rule after the plain one. */
inline lines::Rails heavyHairHeavy(float heavy, float hair, const Fill& fill,
                                   float gap = 5.0f) {
  return lines::rails({{.across = -gap, .width = heavy, .fill = fill},
                       {.across = 0, .width = hair, .fill = fill},
                       {.across = gap, .width = heavy, .fill = fill}});
}

/** Solid casing with a DOTTED core — the map convention for a road under
 *  construction, a proposed route, a disused rail. `dotGap` is the spacing
 *  of the core's dots; the casing stays continuous. */
inline lines::Rails dottedCore(float outer, float core, const Fill& fill,
                               float gap = 5.0f, float dotGap = 6.0f) {
  return lines::rails({{.across = -gap, .width = outer, .fill = fill},
                       {.across = 0,
                        .width = core,
                        .fill = fill,
                        .dash = {0.01f, dotGap},
                        .cap = SkPaint::kRound_Cap},
                       {.across = gap, .width = outer, .fill = fill}});
}

}  // namespace brush::presets

}  // namespace sigil::compose
