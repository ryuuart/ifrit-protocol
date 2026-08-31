#pragma once

/** @file
 * SigilCompose layered brush — the stroke STACK: several passes over the
 * same path with their own widths, colours, blurs, dashes and blend modes,
 * painted bottom-up.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample
#include <sigilcompose/brush/Lines.h>        // lines::displace (the wave op)
#include <sigilcompose/shape/Shapes.h>

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

/** One stroke pass of a layered brush. */
struct StrokeLayer {
  float width = 2.0f;
  SkColor4f color = {1, 1, 1, 1};
  float blurSigma = 0;         // soft halo layers
  std::vector<SkScalar> dash;  // empty → solid
  float dashPhase = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool roundCap = true;

  bool operator==(const StrokeLayer&) const = default;
};

/** The layered stroke stack — painted bottom-up along the outline. */
struct LayeredBrush {
  std::vector<StrokeLayer> layers;

  bool operator==(const LayeredBrush&) const = default;

  /** Extra paint reach past the outline, so a cached recording's cull does
   *  not truncate the halo — the point of an additive stack is that it
   *  paints WIDE of the path, and a node culling at its own bounds loses
   *  exactly that.
   *
   *  Taken per layer and then maximised, not summed: a wide hard core and a
   *  narrow soft halo do not compound, since each layer reaches from the
   *  path independently. Blur counts as 3σ, which covers over 99% of a
   *  Gaussian. */
  float bleed() const {
    float reach = 0;
    for (const StrokeLayer& layer : layers)
      reach = std::max(reach, layer.width * 0.5f + layer.blurSigma * 3.0f);
    return reach;
  }
  /** The widest layer's full mark (see PathFormat::reach). */
  float reach() const {
    float widest = 0;
    for (const StrokeLayer& layer : layers)
      widest = std::max(widest, layer.width + layer.blurSigma * 3.0f);
    return widest;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

// Ready-made stroke stacks — an additive filament glow, circuit traces, a
// counter-dashed rope — are `kit::brush::presets::`, in
// <sigilcompose/kit/Strokes.h>. They are compositions of the values here
// and need nothing this header does not already expose.

// ---------------------------------------------------------------------------
// The brush model — a brush is a PIPELINE: geometry shapers over the path,
// then paint LAYERS over the result, some of which instance whole elements
// along it with a programmatic per-instance twist. The four leaf kinds:
//   brush::Scatter  jittered instances of one element, plus a mod function
//   brush::Pattern  side/corner/start/end tiles, fitted an integer number
//                   of times per run and stretched to close the gap
//   brush::Ribbon   a variable-width filled band; taper or calligraphic nib
//   brush::Art      one element bent continuously along the contour
//                   through SkVertices (`artAlong()`)

}  // namespace sigil::compose
