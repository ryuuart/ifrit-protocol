#pragma once

/** @file
 * SigilCompose brushes — the LINE vocabulary between elements, grounded in
 * real game linework (REFERENCES.md §5): a brush is a LAYERED STROKE STACK
 * (widths, colors, blurs, dashes, blends, bottom-up) applied to the node's
 * outline — a rail's route, a connector's wire, a border. Every brush is a
 * comparable VALUE (defaulted equality → prunes), attached with `.stroke()`.
 *
 * The stock set transcribes measured grammars:
 *  - filament(): Ori's 4-layer additive glow (envelope 4–6× core — THE
 *    organic-glow signature); state via the whole stack's opacity.
 *  - circuit(): FUI trace tiers (1/2/4px data/main/power + under-glow).
 *  - rope(): Path of Exile's 3-state rope (counter-dashed strand layers;
 *    verified palette ladder Normal→Intermediate→Active).
 *
 * The width law from the research applies: state changes shift COLOR
 * dramatically but width by ≤1.3× — hierarchy encodes importance, state
 * encodes progress.
 */

#include "sigilcompose/Compose.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/effects/SkDashPathEffect.h>

#include <vector>

namespace sigil::compose {

/** One stroke pass of a layered brush. */
struct StrokeLayer {
  float width = 2.0f;
  SkColor4f color = {1, 1, 1, 1};
  float blurSigma = 0;                // soft halo layers
  std::vector<SkScalar> dash;         // empty → solid
  float dashPhase = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool roundCap = true;

  bool operator==(const StrokeLayer &) const = default;
};

/** The layered stroke stack — painted bottom-up along the outline. */
struct LayeredBrush {
  std::vector<StrokeLayer> layers;

  bool operator==(const LayeredBrush &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    for (const StrokeLayer &layer : layers) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(layer.width);
      p.setStrokeCap(layer.roundCap ? SkPaint::kRound_Cap
                                    : SkPaint::kButt_Cap);
      p.setColor4f(layer.color, nullptr);
      p.setBlendMode(layer.blend);
      if (layer.blurSigma > 0)
        p.setMaskFilter(
            SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, layer.blurSigma));
      if (!layer.dash.empty())
        p.setPathEffect(SkDashPathEffect::Make(
            SkSpan(layer.dash.data(), layer.dash.size()), layer.dashPhase));
      c.drawPath(ctx.outline, p);
    }
  }
};

namespace brushes {

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
 *  0 Normal, 1 Intermediate (hover-path), 2 Active. */
inline LayeredBrush rope(int state) {
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
  LayeredBrush b;
  if (state >= 2)
    b.layers.push_back({18, {1.0f, 0.788f, 0.439f, 0.13f}, 6}); // halo
  b.layers.push_back({11, p.body, 0, {}, 0, SkBlendMode::kSrcOver, false});
  b.layers.push_back({7, p.ridge, 0, {7, 5}, 0});   // strand
  b.layers.push_back({7, bodyLit, 0, {7, 5}, 6});   // counter-strand
  b.layers.push_back({2, ridgeLit, 0, {7, 5}, 3});  // ridge light
  return b;
}

/** The §5 pulse-travel profile as a brush: plus-blended halo, colored
 *  body, white-hot core. Stroke it on a SHORT trim window of a rail
 *  (trim(&phase, &phaseEnd)) and march the window along the route —
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

} // namespace brushes
} // namespace sigil::compose
