#pragma once

/** @file
 * SigilCompose layer styles — THE MECHANISMS an image editor builds a
 * rich surface out of: fake bevels, metallic sheens, inner shadows, glows
 * and overlays, made of gradients, blurs and blend modes and never of
 * shaders. This is the compositional peer to the SkSL route
 * (`material::sdf`, `material::skia::Paint::sksl`). It models no
 * lighting: a bevel is two opposed inner shadows, glass is a highlight
 * lens over a body ramp.
 *
 * The LOOKS these are bundled into are the kit's — `kit/Gel.h`,
 * `kit/Chrome.h`, `kit/Gloss.h` — because a look is one era and a
 * mechanism is not.
 *
 * Every style here is a VALUE decoration with defaulted equality, so styled
 * chrome prunes and caches like any other static decoration — which is the
 * reason to reach for these before writing a shader.
 *
 * ATTACHMENT IS THE CONTRACT. `.background()` paints BENEATH the node's
 * fill, so anything attached there is hidden by an opaque fill: it is
 * where shadows and outer glows belong, and where a body ramp belongs on a
 * node with no fill of its own. `.foreground()` paints above the fill, the
 * content and the children. The order that reads correctly is the familiar
 * one — drop shadow, outer glow, fill, colour/gradient overlay, inner glow,
 * inner shadow, bevel planes, stroke — and it is produced by which slot
 * each style is attached to plus the order within that slot.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/brush/Decorations.h>  // PathFormat keylines in the presets
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <array>

#include "sigilcompose/Compose.h"

namespace sigil::compose::styles {

/** Drop shadow — `shadow` under the name it has in this family.
 *  Attach as the FIRST background, so everything else paints over it. */
inline Shadow dropShadow(SkColor4f color = {0, 0, 0, 0.5f},
                         SkVector offset = {3, 3}, float size = 6) {
  return shadow(color, offset, size);
}

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
  SkColor4f color = {0, 0, 0, 0.5f};
  SkVector offset = {0, 3};
  float size = 5;  // blur extent, px

  bool operator==(const InnerShadow&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Inner Glow: an inner shadow with no offset — edges light up inward. */
inline InnerShadow innerGlow(SkColor4f color, float size) {
  return InnerShadow{color, {0, 0}, size};
}

/** Outer Glow: the shape re-drawn blurred (optionally spread wider) —
 *  attach as a background; the fill covers the center. */
struct OuterGlow {
  SkColor4f color = {1, 1, 1, 0.8f};
  float size = 8;    // blur extent, px
  float spread = 0;  // hard expansion before the blur, px

  bool operator==(const OuterGlow&) const = default;
  /** Paint reach beyond the node's bounds (recording cull grows by this). */
  float bleed() const { return size * 2 + spread; }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Bevel and emboss, the fake-3D workhorse: two OPPOSED inner shadows — a
 *  highlight plane hugging the lit edges and a shadow plane hugging the far
 *  edges. There is no lighting model here; the depth is entirely in those
 *  two bands. `angleDeg` is the light angle, counter-clockwise from +x and
 *  naming the direction the light COMES FROM, so 120° is upper-left. */
struct BevelEmboss {
  float depth = 3;  // plane offset, px
  float size = 4;   // soften blur, px
  float angleDeg = 120;
  SkColor4f highlight = {1, 1, 1, 0.65f};
  SkColor4f shadow = {0, 0, 0, 0.45f};

  bool operator==(const BevelEmboss&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Colour, gradient and pattern overlay in one value: any Material drawn
 *  over the shape with a blend mode and an opacity. They are not three
 *  styles here because a Material is already polymorphic.
 *
 *  **Overlay materials must be STATIC.** This scheme declares no volatility
 *  and resolves its material WITHOUT a paint context, and both halves of
 *  that bite silently: a LIVE material is sampled once, frozen into the
 *  cached picture, and never repaints; a geometry-dependent one resolves
 *  against a zero size, so anything scaled by the node's box degenerates.
 *  Use `decorations::wash` for either — it declares its animation and
 *  resolves against the real context. */
struct Overlay {
  material::skia::Paint material;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  float opacity = 1.0f;

  bool operator==(const Overlay&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Overlay colorOverlay(SkColor4f color,
                            SkBlendMode blend = SkBlendMode::kSrcOver,
                            float opacity = 1.0f) {
  return Overlay{material::skia::Paint::solid(color), blend, opacity};
}
inline Overlay gradientOverlay(material::skia::Paint gradient,
                               SkBlendMode blend = SkBlendMode::kSrcOver,
                               float opacity = 1.0f) {
  return Overlay{std::move(gradient), blend, opacity};
}

/** Text (or any layer) glow: the node's rendered layer re-emitted blurred
 *  beneath itself — a drop shadow at zero offset, which keeps the content
 *  on top. Attach with `.effect()`, and chain with `.then()` for a tighter
 *  core over a wider halo: `text(...).effect(styles::textGlow(cyan, 6))`.
 *  The kernel's `Effect::glow`, under the name this family gives it. */
inline material::skia::Effect textGlow(SkColor4f color, float sigma) {
  return material::skia::Effect::glow(color, sigma);
}

/** The water/heat warp: the node's rendered layer resampled through a sine
 *  displacement field — y shifted by a sine of x, or with `vertical`, x by
 *  a sine of y. Water reads convincingly at an amplitude of a few percent
 *  of the node's height with only a couple of waves across it. Attach with
 *  `.effect()` to warp the node's own layer, or `.backdrop()` to warp what
 *  is beneath it.
 *
 *  An Effect is a STATIC value, so animating this means re-describing with
 *  a moving `phase`, and the node re-records on every change. Keep it for
 *  moments that earn it, or pair it with Cache::None so the node is not
 *  paying to invalidate a cache it never keeps. */
material::skia::Effect ripple(float amplitudePx, float wavelengthPx,
                              float phase = 0.0f, bool vertical = false);

}  // namespace sigil::compose::styles
