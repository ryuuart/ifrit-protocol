#pragma once

/** @file
 * The grain source: a texture the mark is laid through.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

namespace sigil::draw::brush {

/** Where a grain stands still.
 *
 *  In Stroke space the texture is fixed in the pen's space, so two marks
 *  crossing the same place meet the same texture and the stroke reads as
 *  pigment caught on a surface. In Dab space the texture rides each
 *  stamp, turning and travelling with it, so it reads as the tip's own
 *  material. A dab-space grain has to ride a stamp, so it applies to a
 *  shape tip; every other tip deposits as one sprite batch and takes its
 *  grain in stroke space whichever space is asked for. */
enum class GrainSpace { Stroke, Dab };

/** A texture applied to what a tool deposits.
 *
 *  The texture's LUMINANCE is its coverage: the mark survives where the
 *  texture is white and is taken away where it is black. `depth` is how
 *  much may be taken — zero leaves the mark untouched and one lets black
 *  erase it completely — and `scale` multiplies the texture's pixel size
 *  in the pen's space. The texture tiles in both axes, so a small tile
 *  covers a whole canvas. */
struct Grain {
  sk_sp<SkImage> image;
  GrainSpace space = GrainSpace::Stroke;
  float scale = 1.0f;
  float depth = 1.0f;
};

}  // namespace sigil::draw::brush
