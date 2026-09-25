/** @file
 * The blend modes' names, as CSS and Canvas spell them.
 */

#include "sigilmaterial/core/BlendMode.h"

namespace sigil::material {

std::string_view name(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal: return "normal";
    case BlendMode::Multiply: return "multiply";
    case BlendMode::Screen: return "screen";
    case BlendMode::Overlay: return "overlay";
    case BlendMode::Darken: return "darken";
    case BlendMode::Lighten: return "lighten";
    case BlendMode::ColorDodge: return "color-dodge";
    case BlendMode::ColorBurn: return "color-burn";
    case BlendMode::HardLight: return "hard-light";
    case BlendMode::SoftLight: return "soft-light";
    case BlendMode::Difference: return "difference";
    case BlendMode::Exclusion: return "exclusion";
    case BlendMode::Hue: return "hue";
    case BlendMode::Saturation: return "saturation";
    case BlendMode::Color: return "color";
    case BlendMode::Luminosity: return "luminosity";
    case BlendMode::PlusLighter: return "plus-lighter";
    case BlendMode::Modulate: return "modulate";
    case BlendMode::Clear: return "clear";
    case BlendMode::Source: return "copy";
    case BlendMode::Destination: return "destination";
    case BlendMode::SourceIn: return "source-in";
    case BlendMode::SourceOut: return "source-out";
    case BlendMode::SourceAtop: return "source-atop";
    case BlendMode::DestinationOver: return "destination-over";
    case BlendMode::DestinationIn: return "destination-in";
    case BlendMode::DestinationOut: return "destination-out";
    case BlendMode::DestinationAtop: return "destination-atop";
    case BlendMode::Xor: return "xor";
  }
  return "normal";
}

}  // namespace sigil::material
