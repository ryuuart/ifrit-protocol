#pragma once

/** @file
 * @ingroup draw-brush
 *
 * The grain source: a texture the mark is laid through.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

namespace sigil::draw::brush {

/** Where a grain stands still: in stroke space the texture is fixed in
 *  the pen's space, so two marks crossing one place meet it alike; in
 *  dab space it rides each stamp and reads as the tip's own material.
 *  @silent the tip is not a shape: every other tip deposits as one
 *  sprite batch and takes its grain in stroke space. */
enum class GrainSpace { Stroke, Dab };

/** A texture applied to what a tool deposits. Its LUMINANCE is its
 *  coverage: the mark survives where the texture is white and is taken
 *  away where it is black. `depth` is how much may be taken and `scale`
 *  multiplies the texture's pixel size in the pen's space; it tiles in
 *  both axes. */
struct Grain {
  sk_sp<SkImage> image;
  GrainSpace space = GrainSpace::Stroke;
  float scale = 1.0f;
  float depth = 1.0f;
};

}  // namespace sigil::draw::brush
