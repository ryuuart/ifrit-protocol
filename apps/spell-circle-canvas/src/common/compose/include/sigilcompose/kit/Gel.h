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

/** The gel pill body, everything sized as a FRACTION of the node's height:
 *  a deep→light vertical ramp, a dark band recessed from the top, a
 *  screen-blended glow rising from the bottom edge, and a luminous halo of
 *  the tint underneath. Because it reads the size at paint, one value
 *  dresses a pill of any dimensions; because it is value-comparable, a
 *  static button wearing it prunes without a memo. */
struct AquaBody {
  SkColor4f tint = hex(0x1E8FFF);
  material::kit::AquaGelOptions opts;

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
 *  recess: `material::kit::AquaGelOptions{.topBand = 1.0f}` is the deep cut,
 * where the band under the top edge ends in a line and the lens reads as a
 * second object laid on the pill. */
LayerStyle aquaGel(SkColor4f tint = hex(0x1E8FFF),
                   material::kit::AquaGelOptions opts = {});

/** The sphere-tuned bundle: a domed lens inset further from the edges and
 *  confined to the upper half, over a hotter bottom glow — what reads as
 *  round rather than as a pill. Pass the diameter so the halo reserves the
 *  right cull reach. */
LayerStyle aquaOrb(SkColor4f tint = hex(0x1E8FFF),
                   float expectedDiameter = 128.0f);

}  // namespace sigil::compose::kit
