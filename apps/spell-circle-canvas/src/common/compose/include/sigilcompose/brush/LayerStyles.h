#pragma once

/** @file
 * SigilCompose layer styles — rich surfaces the way an image editor makes
 * them: fake bevels, metallic sheens, inner shadows, glows and overlays,
 * built from gradients, blurs and blend modes and never from shaders. This
 * is the compositional peer to the SkSL route (core/Sdf.h,
 * Material::sksl). It models no lighting: a chrome button is a gradient
 * ramp, a bevel is two opposed inner shadows, glass is a highlight lens
 * over a body ramp. The MECHANISMS — inner shadow, outer glow, bevel,
 * overlay, gloss contour — are decorations of this tier; the LOOKS they
 * are bundled into — the gel, the chromes, the chrome type — take their
 * colour tables and options from SigilMaterial's kit.
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
#include <sigilcompose/core/Material.h>
#include <sigilmaterial/kit/LayerStyles.h>

#include <array>

#include "sigilcompose/Compose.h"

namespace sigil::compose::styles {

namespace detail {
/** 0xRRGGBB → SkColor4f (straight alpha). */
inline SkColor4f rgb(uint32_t hex, float a = 1.0f) {
  return {(float)((hex >> 16u) & 0xffu) / 255.0f,
          (float)((hex >> 8u) & 0xffu) / 255.0f, (float)(hex & 0xffu) / 255.0f,
          a};
}
/** A SigilMaterial colour as Skia spells it. */
inline SkColor4f sk(sigil::material::Color c) { return {c.r, c.g, c.b, c.a}; }
/** A Skia colour as SigilMaterial spells it. */
inline sigil::material::Color mat(SkColor4f c) {
  return {c.fR, c.fG, c.fB, c.fA};
}
}  // namespace detail

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
  Material material;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  float opacity = 1.0f;

  bool operator==(const Overlay&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Overlay colorOverlay(SkColor4f color,
                            SkBlendMode blend = SkBlendMode::kSrcOver,
                            float opacity = 1.0f) {
  return Overlay{Material::solid(color), blend, opacity};
}
inline Overlay gradientOverlay(Material gradient,
                               SkBlendMode blend = SkBlendMode::kSrcOver,
                               float opacity = 1.0f) {
  return Overlay{std::move(gradient), blend, opacity};
}

/** Text (or any layer) glow: the node's rendered layer re-emitted blurred
 *  beneath itself — a drop shadow at zero offset, which keeps the content
 *  on top. Attach with `.effect()`, and chain with `.then()` for a tighter
 *  core over a wider halo: `text(...).effect(styles::textGlow(cyan, 6))`.
 *  The kernel's `Effect::glow`, under the name this family gives it. */
inline Effect textGlow(SkColor4f color, float sigma) {
  return Effect::glow(color, sigma);
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
Effect ripple(float amplitudePx, float wavelengthPx, float phase = 0.0f,
              bool vertical = false);

// ---------------------------------------------------------------------------
// Preset bundles — LayerStyle values for Element::style()

/** Knobs the gel bundle exposes; the defaults dress a pill. `bleed()`
 *  runs before the node has a layout size, so the halo's cull reserve is
 *  declared by `expectedHeight`: the halo reaches about 0.65 of that
 *  height beyond the box, and under-declaring it truncates the halo at
 *  the cached picture's edge. */
using AquaGelOptions = sigil::material::kit::AquaGelOptions;

/** The gel pill body, everything sized as a FRACTION of the node's height:
 *  a deep→light vertical ramp, a dark band recessed from the top, a
 *  screen-blended glow rising from the bottom edge, and a luminous halo of
 *  the tint underneath. Because it reads the size at paint, one value
 *  dresses a pill of any dimensions; because it is value-comparable, a
 *  static button wearing it prunes without a memo. */
struct AquaBody {
  SkColor4f tint = detail::rgb(0x1E8FFF);
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
 *  pill — `box().corners({h/2}).style(styles::aquaGel(tint))` — and give
 *  the node NO fill: the body decoration paints the surface, and a fill
 *  would cover it. Pass options to retune the lens, the glow and the
 *  recess: `AquaGelOptions{.topBand = 1.0f}` is the deep cut, where the
 *  band under the top edge ends in a line and the lens reads as a second
 *  object laid on the pill. */
LayerStyle aquaGel(SkColor4f tint = detail::rgb(0x1E8FFF),
                   AquaGelOptions opts = {});

/** The sphere-tuned bundle: a domed lens inset further from the edges and
 *  confined to the upper half, over a hotter bottom glow — what reads as
 *  round rather than as a pill. Pass the diameter so the halo reserves the
 *  right cull reach. */
LayerStyle aquaOrb(SkColor4f tint = detail::rgb(0x1E8FFF),
                   float expectedDiameter = 128.0f);

/** Which chrome the bundle wears — the kit's options: `palette`,
 *  `horizonSliver`, `keylineWidth`, `keyline`, `bevelDepth`, `bevelSize`. */
using ChromeOptions = sigil::material::kit::ChromeOptions;

/** Where the chrome ramp's hard stop sits, as a fraction of the node's
 *  height. Position hand-added glints against `kChromeHorizonFrac * H` so
 *  they stay on the horizon at any size. */
inline constexpr float kChromeHorizonFrac =
    sigil::material::kit::kChromeHorizonFrac;

/** The chrome body: the palette's vertical ramp, with its hard stop at the
 *  horizon, drawn through the shape's outline. */
struct ChromeBody {
  ChromeOptions::Palette palette = ChromeOptions::Palette::Steel;
  bool operator==(const ChromeBody&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The finishing pass: a 1 px white top edge plus the white specular sliver
 *  straddling the horizon, both clipped inside the shape.
 *
 *  The sliver FADES OUT at both ends, and must. A specular band drawn as a
 *  hard rectangle ends in two blunt vertical stubs, and on a chrome
 *  wordmark — where the glyphs already chop the band into segments — those
 *  stubs read as an unfinished strikethrough rather than as light. */
struct ChromeSliver {
  float horizonFrac = kChromeHorizonFrac;
  /** Fraction of the width the highlight takes to reach full strength. */
  float falloff = 0.22f;
  bool operator==(const ChromeSliver&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The drop-in chrome bundle: drop shadow, palette ramp, horizon sliver,
 *  chisel bevel, and a dark keyline stroked OUTSIDE the silhouette. The
 *  Silver palette skips the dark inner top band, which would fight its
 *  white top edge. */
LayerStyle y2kChrome(ChromeOptions opts = {});

// ---------------------------------------------------------------------------
// Chrome type — unit-space Materials for Element::textFill()

/** The sunset-chrome ramp in UNIT space: hand it straight to textFill()
 *  and the hard horizon crosses the capitals at half cap height, whatever
 *  the size —
 *  `text(u8"CHROME", display).textFill(styles::sunsetChromeText())`.
 *  The kit's stops as a linear gradient. */
Material sunsetChromeText();

/** The silver-chrome ramp in unit space, for textFill(). */
Material silverChromeText();

// ---------------------------------------------------------------------------
// Gloss contour — the satin band that follows the shape

/** The shape's blurred coverage remapped through a 256-entry CONTOUR
 *  table, tinted and clipped inside the shape.
 *
 *  This is the light band in gel and chrome that a plain gradient cannot
 *  fake, because it follows the SHAPE's own distance field rather than a
 *  screen axis: on a blob it curves with the blob. It is one image-filter
 *  chain (blur, then an alpha table), so it composes with the node's other
 *  decorations inside a single paint rather than forcing a layer. */
struct GlossContour {
  SkColor4f color = {1, 1, 1, 0.85f};
  float sigma = 6.0f;
  SkVector offset = {0, -3};
  std::array<uint8_t, 256> table{};

  bool operator==(const GlossContour& o) const {
    return color == o.color && sigma == o.sigma && offset == o.offset &&
           table == o.table;
  }
  float bleed() const { return sigma * 3.0f; }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** A ring contour: the table peaks where blurred coverage crosses
 *  `center` — 0 is the rim, 1 the deep interior — over a band `width`
 *  wide, giving one bright ring inside the silhouette. */
std::array<uint8_t, 256> glossRing(float center = 0.55f, float width = 0.35f);

/** The drop-in gloss band. Attach as a foreground: it reads the node's
 *  outline and paints over the fill. */
GlossContour gloss(SkColor4f color = {1, 1, 1, 0.85f}, float sigma = 6.0f,
                   SkVector offset = {0, -3}, float ringCenter = 0.55f,
                   float ringWidth = 0.35f);

}  // namespace sigil::compose::styles
