#pragma once

/** @file
 * @ingroup media-core
 * `ColorSpace`, what a picture's values are: colour a display shows, or
 * numbers a shader reads as they were written.
 */

#include <cstdint>

namespace sigil::media {

/** WHAT A PICTURE'S VALUES ARE.
 *
 *  `Srgb` is colour as a display stores it: encoded along the standard
 *  display curve, its alpha multiplied into its colour once it is decoded,
 *  and converted to whatever colour space a canvas draws in. It is what
 *  every photograph, painting and page is.
 *
 *  `Data` is numbers: a normal, a roughness, a flow, a mask packed into a
 *  channel, a motion vector, a position. They are kept exactly as stored —
 *  no display curve, no colour conversion, no alpha multiplied into the
 *  other three channels, so a texel whose alpha is zero keeps its red,
 *  green and blue — and at the precision the file holds them in: sixteen
 *  bits a channel stays sixteen, and floating point stays floating point. */
enum class ColorSpace : uint8_t {
  Srgb,  ///< colour, display-encoded
  Data,  ///< numbers, exactly as stored
};

}  // namespace sigil::media
