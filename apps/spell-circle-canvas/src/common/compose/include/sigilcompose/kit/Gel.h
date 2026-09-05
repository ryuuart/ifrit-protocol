#pragma once

/** @file
 * THE GEL: one era's look, as a bundle of this library's mechanisms over
 * SigilMaterial's colour tables. A deep-to-light body ramp with a
 * recessed band under the top edge, a wet highlight lens across the top
 * half, a glow rising from the bottom, and a luminous halo of the tint
 * underneath.
 *
 * Everything is sized as a FRACTION of the node's height, read at paint,
 * so one value dresses a pill of any dimensions; and everything is
 * value-comparable, so a static button wearing it prunes without a memo.
 * `bleed()` runs before the node has a layout size, so the halo's cull
 * reserve is declared by `AquaGelOptions::expectedHeight` — the halo
 * reaches about 0.65 of that height beyond the box, and under-declaring
 * it truncates the halo at the cached picture's edge.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/skia/Paint.h>

namespace sigil::compose::kit {

/** Knobs the gel bundle exposes; the defaults dress a pill. */
struct AquaGelOptions {
  float lensAlphaTop = 0.72f;    ///< lens ramp: white at the top, clear below
  float lensBottomFrac = 0.52f;  ///< lens ends this far down the box
  float lensInsetXFrac = 0.05f;  ///< lens inset each side; ~0.16 on spheres
  /** Where down the lens its ramp has reached its bottom value, as a
   *  fraction of the lens's own height. Below 1 the lens's lower arc is
   *  painted at that value and its outline never shows, so the highlight
   *  ends in a fade; at 1 the ramp runs to the arc itself and the lens
   *  reads as a cut-out shape laid on the surface. */
  float lensFadeEnd = 0.82f;
  float bottomGlow = 0.85f;  ///< strength of the light from below
  /** How hard the recessed band under the top edge cuts, as a weight on
   *  `aquaTopBand`'s own alpha. At 1 the recess is a dark cap that ends in
   *  a visible line across the shape, and the lens above it reads as a
   *  second object; the default keeps the recess as shading on one
   *  surface. */
  float topBand = 0.55f;
  bool halo = true;  ///< luminous tint drop beneath the shape
  /** The tallest the gel will be: the halo's reach beyond the box is a
   *  fraction of it, and a renderer's cull reserve reads this before the
   *  box has a size. Under-declaring it truncates the halo. */
  float expectedHeight = 64.0f;
  bool operator==(const AquaGelOptions&) const = default;
};

/** The gel pill body, everything sized as a FRACTION of the node's height:
 *  a deep→light vertical ramp, a dark band recessed from the top, a
 *  screen-blended glow rising from the bottom edge, and a luminous halo of
 *  the tint underneath. Because it reads the size at paint, one value
 *  dresses a pill of any dimensions; because it is value-comparable, a
 *  static button wearing it prunes without a memo. */
struct AquaBody {
  SkColor4f tint = hex(0x1E8FFF);
  AquaGelOptions opts;

  bool operator==(const AquaBody&) const = default;
  float bleed() const { return opts.halo ? opts.expectedHeight * 0.65f : 0.0f; }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The highlight lens: a white ramp across the top half of the shape,
 *  clipped inside it — the wet-looking specular that sells the gel.
 *
 *  `fadeEnd` is where the ramp reaches `alphaBottom`, as a fraction of the
 *  lens. Below 1 the lens's own lower arc is drawn at that value and so
 *  leaves no visible outline: the highlight ends where the light ends
 *  rather than where the shape does. */
struct AquaGloss {
  float insetXFrac = 0.05f;
  float topFrac = 0.04f, bottomFrac = 0.52f;
  float alphaTop = 0.72f, alphaBottom = 0.0f;
  float fadeEnd = 0.82f;

  bool operator==(const AquaGloss&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The drop-in gel bundle: body, gloss and a hairline keyline. Use it on a
 *  pill — `box().corners({h/2}).style(kit::aquaGel(tint))` — and give
 *  the node NO fill: the body decoration paints the surface, and a fill
 *  would cover it. Pass options to retune the lens, the glow and the
 *  recess: `AquaGelOptions{.topBand = 1.0f}` is the deep cut,
 * where the band under the top edge ends in a line and the lens reads as a
 * second object laid on the pill. */
LayerStyle aquaGel(SkColor4f tint = hex(0x1E8FFF),
                   AquaGelOptions opts = {});

/** The sphere-tuned bundle: a domed lens inset further from the edges and
 *  confined to the upper half, over a hotter bottom glow — what reads as
 *  round rather than as a pill. Pass the diameter so the halo reserves the
 *  right cull reach. */
LayerStyle aquaOrb(SkColor4f tint = hex(0x1E8FFF),
                   float expectedDiameter = 128.0f);

}  // namespace sigil::compose::kit
