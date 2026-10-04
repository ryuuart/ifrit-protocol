#pragma once

// Folded silver for type and plates, with a chisel edge and a dark keyline.
// The sunset ramp is a separate painted finish for the typography catalog.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>

#include <cstdint>
#include <vector>

#include "Metal.h"

namespace y2k {

namespace material = sigil::material;

enum class ChromePalette : uint8_t {
  Steel,  ///< polished folds for plates and wordmarks
  Silver  ///< flatter, softer reflection for controls
};

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

/** The sunset-chrome ramp in the box's own units: handed to an ink, the
 *  hard horizon crosses the capitals at half cap height at any size. */
inline material::Material sunsetChromeType() {
  return material::linearGradient({0, 0}, {0, 1}, sunsetChromeText());
}

/** Reflective silver whose folds span the full word. */
inline material::Material silverChromeType() { return liquidMetal(); }

/** The plate's knobs. */
struct ChromeOptions {
  ChromePalette palette = ChromePalette::Steel;
  float keylineWidth = 1.0f;
  material::Color keyline = material::hexColor(0x10141A);
  float bevelDepth = 2.2f, bevelSize = 1.25f;
  bool operator==(const ChromeOptions&) const = default;
};

/** A reflective plate with a cast shadow, chisel edge and outer keyline.
 *  The steel face has deeper folds and a recessed upper edge. */
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
  return liquidMetal(options.palette == ChromePalette::Silver ? 0.25f : 0.58f,
                     options.palette == ChromePalette::Silver ? 0.25f : 0.16f)
      .effects(effects);
}

}  // namespace y2k
