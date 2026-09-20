#pragma once

/** @file
 * @ingroup draw-brush
 *
 * The shape source: the artwork a tool stamps at every dab.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

namespace sigil::draw::brush {

/** Which part of an image carries its coverage. Inverted luminance
 *  accepts the common dark-mark-on-white artwork directly; Alpha uses an
 *  authored alpha channel. */
enum class ImageMask { InvertedLuminance, Alpha };

/** An imported tip: the artwork stamped at every dab, with the parts of
 *  a stroke a travelling brush states against the stamp. `spacing`,
 *  `scatter` and `angleJitter` are FRACTIONS OF THE TOOL'S WIDTH — the
 *  jitter in radians — where the tool's own spacing and scatter are
 *  canvas units, and a tool carrying a shape is spaced and scattered by
 *  these instead. */
struct Shape {
  sk_sp<SkImage> image;
  ImageMask mask = ImageMask::InvertedLuminance;
  float spacing = 0.10f;
  float scatter = 0.0f;
  float angleJitter = 0.0f;
};

}  // namespace sigil::draw::brush
