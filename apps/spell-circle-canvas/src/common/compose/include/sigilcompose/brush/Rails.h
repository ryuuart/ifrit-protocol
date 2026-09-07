#pragma once

/** @file
 * SigilCompose rails — N-rail strokes, where every rail of a parallel rule
 * is its own line with its own width, fill, dash and phase.
 */

#include "sigilcompose/brush/Lines.h"

namespace sigil::compose::lines {

// ---------------------------------------------------------------------------
// N-rail strokes — a parallel rule where EVERY rail is its own line
//
// `Line::parallels` gives N rails sharing one width, one fill, one dash and
// one phase; its only per-rail knob is `coreWidthFactor`, which applies to
// the centre rail and only when `parallels` is odd. `Rails` is for
// everything that needs more than that:
//
//   heavy outer + hairline inner at two rails  (the engraver's rule)
//   solid outer + DOTTED inner                 (a road under construction)
//   unequal gaps                               (road + kerb + lane)
//   per-rail colour
//
// Neither workaround covers it. Stacked strokes with different insets only
// work on concentric shapes, because an inset is not a displacement. A
// `Brush` with a per-layer offset shaper works on any path, but each layer
// dashes ITS OWN offset curve, whose arc length differs from its
// neighbour's on every bend, so the dashes shear apart.
//
// `Rails` is built on Line's dashed-parallel construction instead:
//
//     DASH IN CENTRELINE ARC-SPACE, THEN DISPLACE THE DASHES.
//
// Every rail's pattern is measured along the SAME curve, so rails sharing
// intervals stay in register through any curvature, and a rail with a
// different pattern still registers against the centreline — which is what
// makes an inner tick fall reliably between two outer ones.

/** One rail of a `Rails` stroke: its own displacement from the route, its
 *  own width, fill, dash pattern and phase. `across` is px **LEFT of
 *  travel** — the one convention (see `geometry::path::parallel`), shared with
 *  `Line::across`, `strand::offset` and `Profile::across` — so a
 *  symmetric pair is {-gap/2, +gap/2}. */
struct Rail {
  float across = 0.0f;
  float width = 2.0f;
  Fill fill = Fill::color({1, 1, 1, 1});
  /** Empty → solid. Measured along the CENTRELINE, not this rail. */
  std::vector<SkScalar> dash;
  /** Added to the stroke's shared phase — the knob that slides ONE rail
   *  against its neighbours (staggered ties, a counter-dashed strand). */
  float dashPhase = 0.0f;
  SkPaint::Cap cap = SkPaint::kRound_Cap;
  SkPaint::Join join = SkPaint::kRound_Join;

  bool operator==(const Rail&) const = default;
};

/** The general parallel rule: an ordered set of `Rail`s sharing one route,
 *  one wave/zigzag displacement and one marching phase.
 *
 *      element.stroke(lines::Rails{.rails = {
 *          {.across = -4, .width = 2.4f, .fill = ink},
 *          {.across =  0, .width = 0.6f, .fill = ink, .dash = {1, 4}},
 *          {.across =  4, .width = 2.4f, .fill = ink}}});
 *
 *  A value like every other decoration: defaulted equality, so a static
 *  quad rail prunes and caches without a memo. */
struct Rails {
  std::vector<Rail> rails;

  /** Shared displacement of the route before any rail is offset — the
   *  whole set waves together (`Line::waveAmplitude` semantics). */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Shared phase, added to every rail's own `dashPhase`. Bind it and the
   *  whole set marches in register (PathFormat::dashPhaseBinding). */
  float dashPhase = 0.0f;
  std::optional<motion::Animatable<float>> dashPhaseBinding;

  /** Resample stride for the offset construction, px. 2 follows tight
   *  metro curves; loosen on long gentle routes. */
  float offsetStep = 2.0f;

  bool operator==(const Rails&) const = default;

  bool isAnimated() const {
    return dashPhaseBinding && motion::isLive(nullptr, *dashPhaseBinding);
  }
  float phase() const {
    return dashPhaseBinding ? motion::resolveFloatAt(nullptr, *dashPhaseBinding)
                            : dashPhase;
  }

  float bleed() const;

  /** Total centre-to-centre span of the set — the number a caller needs to
   *  reserve room, and the one `Line` never exposed. */
  float span() const;

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
};

/** Explicit rails, displacements and all. */
Rails rails(std::vector<Rail> set);

}  // namespace sigil::compose::lines
