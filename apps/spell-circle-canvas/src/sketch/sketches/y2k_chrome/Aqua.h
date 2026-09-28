#pragma once

// THE AQUA GEL: the era's lickable pill, as one material and one drawn
// lens. The material is the body — a deep-to-light ramp, a glow screened
// up from the bottom edge, a recess shaded under the top edge, a luminous
// halo of the tint beneath and a hairline inside the silhouette — and
// every length in it is a fraction of the pill's height, so one call
// dresses a pill of any size. The lens is ornament the material cannot
// state (a rounded highlight inset from the silhouette), so it is a
// decoration the pill wears in its foreground.
//
//     box().width(w).height(h).borderRadius({h / 2})
//         .fill(y2k::aquaGel(tint, h))
//         .foreground(y2k::aquaLens())
//
// The chrome type study and the surface components study wear the same
// look and include this header from here.

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRRect.h>
#include <sigilcompose/core/Paint.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <vector>

namespace y2k {

namespace material = sigil::material;

/** The body ramp: the tint deepened at the top, itself at the middle and
 *  lifted toward white at the bottom. */
inline std::vector<material::ColorStop> aquaBodyRamp(material::Color tint) {
  return {
      {0.0f, material::scale(tint, 0.18f)},
      {0.18f, material::scale(tint, 0.48f)},
      {0.49f, material::scale(tint, 0.78f)},
      {0.68f, tint},
      {0.88f, material::mixToward(tint, {0.78f, 0.96f, 1, 1}, 0.52f, tint.a)},
      {1.0f, material::scale(tint, 0.40f)}};
}

/** Light gathered near the lower rim, fading back into its dark edge. */
inline std::vector<material::ColorStop> aquaGlowRamp(material::Color tint,
                                                     float strength) {
  return {
      {0.0f, {1, 1, 1, 0}},
      {0.70f, material::mixToward(tint, {1, 1, 1, 1}, 0.86f, strength)},
      {1.0f, material::mixToward(tint, {1, 1, 1, 1}, 0.25f, strength * 0.24f)}};
}

inline material::Color aquaHalo(material::Color tint) {
  return material::mixToward(tint, {1, 1, 1, 1}, 0.15f, 0.24f);
}
inline material::Color aquaTopBand(material::Color tint) {
  return material::scale(tint, 0.36f, 0.45f);
}
inline material::Color aquaHairline(material::Color tint) {
  return material::scale(tint, 0.45f, 0.6f);
}
inline material::Color aquaTint() { return material::hexColor(0x1E8FFF); }

/** Knobs the gel exposes; the defaults dress a pill. */
struct AquaGelOptions {
  float lensAlphaTop = 0.90f;     ///< lens ramp: white at the top…
  float lensAlphaBottom = 0.0f;   ///< …and what it has faded to at its end
  float lensTopFrac = 0.045f;     ///< lens starts this far down the box
  float lensBottomFrac = 0.48f;   ///< …and ends here
  float lensInsetXFrac = 0.028f;  ///< lens inset each side; ~0.16 on spheres
  /** Where down the lens its ramp has reached its bottom value, as a
   *  fraction of the lens's own height. Below 1 the lens's lower arc is
   *  painted at that value and its outline never shows. */
  float lensFadeEnd = 0.92f;
  float bottomGlow = 0.64f;  ///< strength of the light from below
  /** How hard the recessed band under the top edge cuts, as a weight on
   *  `aquaTopBand`'s own alpha. */
  float topBand = 0.70f;
  bool halo = true;  ///< luminous tint drop beneath the shape
  bool operator==(const AquaGelOptions&) const = default;
};

/** The gel body for a pill @p height px tall: the ramp, the glow from
 *  below, the recess, the halo and the hairline. */
inline material::Material aquaGel(material::Color tint, float height,
                                  AquaGelOptions options = {}) {
  const float h = std::max(0.0f, height);
  material::Material gel = material::from(
      material::linearGradient({0, 0}, {0, 1}, aquaBodyRamp(tint)));
  if (options.bottomGlow > 0)
    gel.layer(material::linearGradient({0, 0.55f}, {0, 1},
                                       aquaGlowRamp(tint, options.bottomGlow)),
              {.blend = material::BlendMode::Screen});
  gel.layer(material::radialGradient({0.5f, 0.5f}, 0.67f,
                                     {{0.0f, {0, 0, 0, 0}},
                                      {0.55f, {0, 0, 0, 0}},
                                      {1.0f, {0.015f, 0.035f, 0.08f, 0.34f}}}));
  material::Filter effects = material::Filter::stroke(
      aquaHairline(tint),
      {.width = 1, .position = material::StrokePosition::Inside});
  if (options.halo)
    effects = material::Filter::shadow(
                  aquaHalo(tint), {.blur = h * 0.22f, .offset = {0, h * 0.14f}})
                  .then(effects);
  if (options.topBand > 0) {
    material::Color band = aquaTopBand(tint);
    band.a *= options.topBand;
    effects = effects.then(material::Filter::shadow(
        band, {.blur = h * 0.12f, .offset = {0, h * 0.06f}, .inside = true}));
  }
  effects = effects.then(
      material::Filter::bevel({.depth = 0.85f,
                               .size = 0.60f,
                               .angleDegrees = 115,
                               .highlight = {0.82f, 0.94f, 1, 0.85f},
                               .shadow = {0.015f, 0.035f, 0.09f, 0.72f}}));
  return gel.effects(effects);
}

/** The sphere-tuned gel: a hotter glow from below, for a lens inset further
 *  from the edges (`aquaOrbLens`). */
inline material::Material aquaOrb(material::Color tint, float diameter) {
  return aquaGel(tint, diameter, {.bottomGlow = 0.95f});
}

/** The highlight lens: a white ramp across the top half of the shape,
 *  clipped inside it — the wet-looking specular that sells the gel.
 *  `fadeEnd` is where the ramp reaches `alphaBottom`, as a fraction of the
 *  lens; below 1 the lens's own lower arc leaves no visible outline. */
struct AquaGloss {
  float insetXFrac = 0.05f;
  float topFrac = 0.04f, bottomFrac = 0.52f;
  float alphaTop = 0.72f, alphaBottom = 0.0f;
  float fadeEnd = 0.82f;

  bool operator==(const AquaGloss&) const = default;

  void paint(sigil::draw::Pen& pen,
             const sigil::compose::PaintContext& ctx) const {
    SkCanvas& c = *pen.canvas();
    const float W = ctx.size.x, H = ctx.size.y;
    const SkRect lens = SkRect::MakeLTRB(W * insetXFrac, H * topFrac,
                                         W * (1 - insetXFrac), H * bottomFrac);
    SkPaint p;
    p.setAntiAlias(true);
    const float fade = std::clamp(fadeEnd, 0.05f, 1.0f);
    p.setShader(material::skia::shader(
        sigil::material::skia::paint(sigil::material::linearGradient(
            {0, lens.top()}, {0, lens.bottom()},
            {{0.0f, {1, 1, 1, alphaTop}},
             {fade, {1, 1, 1, alphaBottom}},
             {1.0f, {1, 1, 1, alphaBottom}}},
            {.units = material::GradientUnits::Pixels}))));
    c.save();
    c.clipPath(sigil::geometry::path::toSk(ctx.outline), true);
    c.drawRRect(SkRRect::MakeRectXY(lens, lens.height() / 2, lens.height() / 2),
                p);
    c.restore();
  }
};

/** The gel's lens, from the same options the body took. */
inline AquaGloss aquaLens(const AquaGelOptions& options = {}) {
  return AquaGloss{.insetXFrac = options.lensInsetXFrac,
                   .topFrac = options.lensTopFrac,
                   .bottomFrac = options.lensBottomFrac,
                   .alphaTop = options.lensAlphaTop,
                   .alphaBottom = options.lensAlphaBottom,
                   .fadeEnd = options.lensFadeEnd};
}

/** The orb's lens: domed, inset further and confined to the upper half. */
inline AquaGloss aquaOrbLens() {
  return aquaLens({.lensBottomFrac = 0.50f, .lensInsetXFrac = 0.16f});
}

}  // namespace y2k
