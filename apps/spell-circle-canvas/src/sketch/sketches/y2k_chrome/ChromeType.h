#pragma once

// Y2K CHROME: the era's plate and wordmark. The plate is one material —
// the palette's vertical ramp with its hard stop at the horizon, a drop
// shadow, the dark band pressed under a steel plate's top edge, a chisel
// bevel and a dark keyline stroked outside the silhouette — and the white
// specular sliver across the horizon is ornament the material cannot
// state, so it is a decoration the plate wears over its fill.
//
//     box().height(h).fill(y2k::y2kChrome()).overlay(y2k::ChromeSliver{})
//     text(u8"CHROME", display).ink(y2k::sunsetChromeType())
//
// `kChromeHorizonFraction` is where the hard stop sits, as a fraction of
// the node's height: position hand-added glints against it times the
// height and they stay on the horizon at any size. The Flash-portfolio
// study includes this header from here.

#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace y2k {

namespace material = sigil::material;

enum class ChromePalette : uint8_t {
  Steel,  ///< the dark ramp — heavy contrast, for plates and wordmarks
  Silver  ///< the light ramp — window and control chrome
};

/** Where the chrome ramps put their hard stop, as a fraction of height. */
inline constexpr float kChromeHorizonFraction = 0.50f;

inline std::vector<material::ColorStop> chromeRamp(ChromePalette palette) {
  using material::hexColor;
  if (palette == ChromePalette::Silver)
    return {{0.0f, hexColor(0xFDFDFD)},  {0.2f, hexColor(0xD2D8DD)},
            {0.48f, hexColor(0xA5ADB5)}, {0.5f, hexColor(0x6F7880)},
            {0.52f, hexColor(0xE9ECEF)}, {0.8f, hexColor(0xC6CDD3)},
            {1.0f, hexColor(0x9BA3AC)}};
  return {{0.0f, hexColor(0xF4F7FA)},  {0.35f, hexColor(0x97A1AC)},
          {0.49f, hexColor(0x3A4654)}, {0.51f, hexColor(0x1E2833)},
          {0.62f, hexColor(0x5C6B7C)}, {1.0f, hexColor(0xDCE4EA)}};
}

/** The dark band a steel plate is pressed with under its top edge. */
inline material::Color chromeSteelTopBand() {
  return material::hexColor(0x001020, 0.30f);
}

/** The sunset-chrome wordmark ramp: sky over a hard horizon over sand. */
inline std::vector<material::ColorStop> sunsetChromeText() {
  using material::hexColor;
  return {{0.0f, hexColor(0xEAF6FF)},   {0.12f, hexColor(0x9CCFF3)},
          {0.35f, hexColor(0x3C7FC0)},  {0.495f, hexColor(0x0B2A52)},
          {0.505f, hexColor(0x7A4A1A)}, {0.62f, hexColor(0xB98A46)},
          {0.82f, hexColor(0xE8CE9A)},  {1.0f, hexColor(0xFDF6E3)}};
}

inline std::vector<material::ColorStop> silverChromeText() {
  return chromeRamp(ChromePalette::Silver);
}

/** The sunset-chrome ramp in the box's own units: handed to an ink, the
 *  hard horizon crosses the capitals at half cap height at any size. */
inline material::Material sunsetChromeType() {
  return material::linearGradient({0, 0}, {0, 1}, sunsetChromeText());
}

/** The silver-chrome ramp in the box's own units, for an ink. */
inline material::Material silverChromeType() {
  return material::linearGradient({0, 0}, {0, 1}, silverChromeText());
}

/** The plate's knobs. */
struct ChromeOptions {
  ChromePalette palette = ChromePalette::Steel;
  float keylineWidth = 2.0f;
  material::Color keyline = material::hexColor(0x10141A);
  float bevelDepth = 3.0f, bevelSize = 5.0f;
  bool operator==(const ChromeOptions&) const = default;
};

/** The chrome plate: drop shadow, palette ramp, the steel top band, chisel
 *  bevel and a keyline stroked OUTSIDE the silhouette. The Silver palette
 *  skips the dark top band, which would fight its white top edge. */
inline material::Material y2kChrome(ChromeOptions options = {}) {
  material::Filter effects = material::Filter::shadow(
      {0, 0, 0, 0.45f}, {.blur = 10, .offset = {0, 6}});
  if (options.palette == ChromePalette::Steel)
    effects = effects.then(material::Filter::shadow(
        chromeSteelTopBand(), {.blur = 4, .offset = {0, 3}, .inside = true}));
  effects = effects.then(material::Filter::bevel({.depth = options.bevelDepth,
                                                  .size = options.bevelSize,
                                                  .angleDegrees = 120,
                                                  .highlight = {1, 1, 1, 0.5f},
                                                  .shadow = {0, 0, 0, 0.65f}}));
  if (options.keylineWidth > 0)
    effects = effects.then(material::Filter::stroke(
        options.keyline, {.width = options.keylineWidth,
                          .position = material::StrokePosition::Outside}));
  return material::from(
             material::linearGradient({0, 0}, {0, 1}, chromeRamp(options.palette)))
      .effects(effects);
}

/** The finishing pass: a 1 px white top edge plus the white specular sliver
 *  straddling the horizon, both clipped inside the shape. It goes over the
 *  plate and under the content: over the node's own type a white band at
 *  half height reads as a strikethrough rather than as a sheen.
 *
 *  The sliver FADES OUT at both ends, and must: a band drawn as a hard
 *  rectangle ends in two blunt stubs, which on a wordmark — where the
 *  glyphs already chop the band into segments — read as an unfinished
 *  strikethrough rather than as light. */
struct ChromeSliver {
  float horizonFrac = kChromeHorizonFraction;
  /** Fraction of the width the highlight takes to reach full strength. */
  float falloff = 0.22f;
  bool operator==(const ChromeSliver&) const = default;

  void paint(sigil::draw::Pen& pen, const sigil::compose::PaintContext& ctx) const {
    SkCanvas& c = *pen.canvas();
    const float W = ctx.size.x, H = ctx.size.y;
    c.save();
    c.clipPath(sigil::geometry::path::toSk(ctx.outline), true);
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f({1, 1, 1, 0.9f}, nullptr);
    c.drawRect(SkRect::MakeXYWH(0, 0, W, 1), p);  // the top edge

    // One horizontal alpha ramp reused for the hot line and the bloom
    // under it; the bloom also falls off vertically, so the pair reads as
    // light gathering along the horizon rather than as two drawn rules.
    const float horizon = H * horizonFrac;
    const float fade = std::clamp(falloff, 0.02f, 0.49f);
    const float mid[4] = {0.0f, fade, 1.0f - fade, 1.0f};
    auto band = [&](float y, float height, float alpha) {
      const SkColor4f colors[4] = {
          {1, 1, 1, 0}, {1, 1, 1, alpha}, {1, 1, 1, alpha}, {1, 1, 1, 0}};
      SkPoint pts[2] = {{0, y}, {W, y}};
      SkPaint bp;
      bp.setAntiAlias(true);
      bp.setShader(SkShaders::LinearGradient(
          pts, SkGradient({{colors, 4}, {mid, 4}, SkTileMode::kClamp}, {})));
      c.drawRect(SkRect::MakeXYWH(0, y, W, height), bp);
    };
    band(horizon - 1, 1, 0.85f);
    band(horizon, 1, 0.28f);
    band(horizon + 1, 1, 0.16f);
    band(horizon + 2, 2, 0.07f);
    c.restore();
  }
};

}  // namespace y2k
