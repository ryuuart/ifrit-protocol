#pragma once

/** @file
 * @ingroup material-core
 *
 * HOW ONE LAYER MEETS WHAT IS BENEATH IT — the one list every blend in
 * this library and its consumers answers to: a paint's layer stack, a
 * stack of materials through a mask, a filter's emitted light, and a
 * node's `blendMode`. The separable and non-separable modes are CSS's
 * `mix-blend-mode`; the rest are the Porter-Duff operators Canvas's
 * `globalCompositeOperation` names, and `Modulate`, the product of the
 * two premultiplied colours.
 */

#include <cstdint>
#include <string_view>

namespace sigil::material {

/** A blend mode. `Normal` — the layer over what is beneath it — is the
 *  default everywhere one is taken. */
enum class BlendMode : uint8_t {
  Normal,           ///< the layer over the backdrop (source-over)
  Multiply,         ///< darkens: the product of the two
  Screen,           ///< lightens: the inverse of the product of the inverses
  Overlay,          ///< multiply or screen, by the backdrop
  Darken,           ///< the darker of the two, per channel
  Lighten,          ///< the lighter of the two, per channel
  ColorDodge,       ///< brightens the backdrop toward the layer
  ColorBurn,        ///< darkens the backdrop toward the layer
  HardLight,        ///< multiply or screen, by the layer
  SoftLight,        ///< a gentler hard light
  Difference,       ///< the absolute difference
  Exclusion,        ///< difference with lower contrast
  Hue,              ///< the layer's hue, the backdrop's saturation and luminosity
  Saturation,       ///< the layer's saturation
  Color,            ///< the layer's hue and saturation
  Luminosity,       ///< the layer's luminosity
  PlusLighter,      ///< the sum of the two, clamped (additive light)
  Modulate,         ///< the product of the premultiplied colours
  Clear,            ///< nothing
  Source,           ///< the layer alone, replacing the backdrop (copy)
  Destination,      ///< the backdrop alone
  SourceIn,         ///< the layer where the backdrop is
  SourceOut,        ///< the layer where the backdrop is not
  SourceAtop,       ///< the layer over the backdrop, inside it
  DestinationOver,  ///< the backdrop over the layer
  DestinationIn,    ///< the backdrop where the layer is
  DestinationOut,   ///< the backdrop where the layer is not
  DestinationAtop,  ///< the backdrop over the layer, inside the layer
  Xor,              ///< each where the other is not
};

/** The mode's name as CSS and Canvas spell it — `"normal"`,
 *  `"soft-light"`, `"plus-lighter"`, `"destination-in"` — for messages
 *  and labels. */
std::string_view name(BlendMode mode);

}  // namespace sigil::material
