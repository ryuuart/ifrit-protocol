#pragma once

/** @file
 * @ingroup compose-brush
 *
 * SigilCompose style marks — the drawn pieces an ornament is built of:
 * an inner shadow, an outer glow and a bevel as decorations a kit places
 * on a chosen slot, made of gradients and blurs rather than shaders. A
 * LOOK — a glow, a shadow, a bevel or an overlay dressing a node — is a
 * material's effects and layers, stated with `fill(material)`; these marks
 * are for a component that places one mark itself, beside its own fill.
 *
 * Every mark here is a VALUE decoration with defaulted equality, so a
 * node carrying one prunes and caches like any other static decoration.
 * `.background()` paints beneath the node's fill, `.foreground()` above
 * the fill, the content and the children.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/brush/Decorations.h>  // PathFormat keylines in the presets
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <array>

#include "sigilcompose/Compose.h"

/** THE STYLE MARKS: drawn pieces a kit component places on one slot of
 *  its own, beside the fill it paints. */
namespace sigil::compose::styles {

/** Inner shadow: a blurred band hugging the inner edges — the recessed,
 *  punched-in look, and one half of every fake bevel. `offset` is the
 *  direction the shadow is CAST, so (0, 3) casts downward and the band
 *  therefore hugs the TOP inner edge.
 *
 *  It is built as a FINITE stroked band clipped inside the outline, and
 *  must stay that way. The obvious alternative — blurring an inverse fill
 *  through a mask filter — has device-dependent bounds, so it floods the
 *  whole interior when the node is cached at a non-origin offset. */
struct InnerShadow {
  material::Color color = {0, 0, 0, 0.5f};
  SkVector offset = {0, 3};
  float size = 5;  ///< blur extent, px

  bool operator==(const InnerShadow&) const = default;

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** Outer Glow: the shape re-drawn blurred (optionally spread wider) —
 *  attach as a background; the fill covers the center. */
struct OuterGlow {
  material::Color color = {1, 1, 1, 0.8f};
  float size = 8;    ///< blur extent, px
  float spread = 0;  ///< hard expansion before the blur, px

  bool operator==(const OuterGlow&) const = default;
  /** Paint reach beyond the node's bounds (recording cull grows by this). */
  float bleed() const { return size * 2 + spread; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** Bevel and emboss, the fake-3D workhorse: two OPPOSED inner shadows — a
 *  highlight plane hugging the lit edges and a shadow plane hugging the far
 *  edges. There is no lighting model here; the depth is entirely in those
 *  two bands. `angleDeg` is the light angle, counter-clockwise from +x and
 *  naming the direction the light COMES FROM, so 120° is upper-left. */
struct BevelEmboss {
  float depth = 3;  ///< plane offset, px
  float size = 4;   ///< soften blur, px
  float angleDeg = 120;
  material::Color highlight = {1, 1, 1, 0.65f};
  material::Color shadow = {0, 0, 0, 0.45f};

  bool operator==(const BevelEmboss&) const = default;

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** The water/heat warp: the node's rendered layer resampled through a sine
 *  displacement field — y shifted by a sine of x, or with `vertical`, x by
 *  a sine of y. Water reads convincingly at an amplitude of a few percent
 *  of the node's height with only a couple of waves across it. Attach with
 *  `.filter()` to warp the node's own layer, or `.backdropFilter()` to
 *  warp what is beneath it.
 *
 *  An Effect is a STATIC value, so animating this means re-describing with
 *  a moving `phase`, and the node re-records on every change. Keep it for
 *  moments that earn it, or pair it with Cache::None so the node is not
 *  paying to invalidate a cache it never keeps. */
material::Filter ripple(float amplitudePx, float wavelengthPx,
                              float phase = 0.0f, bool vertical = false);

}  // namespace sigil::compose::styles
