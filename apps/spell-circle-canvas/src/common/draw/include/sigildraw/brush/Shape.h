#pragma once

/** @file
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
 *  a stroke a travelling brush states against the stamp rather than in
 *  canvas units.
 *
 *  `spacing` is the distance between two stamps as a fraction of the
 *  tool's width — a tenth is a dense continuous mark, one is a chain of
 *  separate stamps — and it is what the tool's own `spacing` in canvas
 *  units means for a procedural tip. `scatter` moves each stamp off the
 *  centreline by up to that fraction of the width, in both axes.
 *  `angleJitter` turns each stamp by up to that many radians either way,
 *  on top of whatever the tool's rotation answers. */
struct Shape {
  sk_sp<SkImage> image;
  ImageMask mask = ImageMask::InvertedLuminance;
  float spacing = 0.10f;
  float scatter = 0.0f;
  float angleJitter = 0.0f;
};

}  // namespace sigil::draw::brush
