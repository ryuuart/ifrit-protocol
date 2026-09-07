#pragma once

/** @file
 * The pieces of the pen that more than one of its files reads: p5's words
 * for a cap, a join and a blend as Skia spells them, how an image is
 * sampled under the pen's smoothing, the arc angles made drawable, the
 * seed a pen starts on, and one material onto one SkPaint. Private to
 * SigilDraw.
 */

#include <include/core/SkBlender.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Constants.h>
#include <sigilmaterial/skia/Paint.h>

#include <cstdint>

namespace sigil::draw::detail {

/** The seed every pen starts on, so a sketch stepped from zero draws the
 *  same picture on every machine. `randomSeed` moves off it. */
inline constexpr uint64_t kDefaultSeed = 0x5EED5EED5EED5EEDull;

/** p5's SQUARE ends the stroke at the point and PROJECT carries it half a
 *  weight past, which are Skia's butt and square caps. */
SkPaint::Cap capOf(Constant cap);

/** p5's MITER, BEVEL and ROUND, which Skia spells the same way. */
SkPaint::Join joinOf(Constant join);

/** HOW AN IMAGE IS SAMPLED under the pen's smoothing: linear with
 *  mipmaps, or nearest with none, which is what a blown-up pixel source
 *  needs. */
SkSamplingOptions samplingFor(bool smooth);

/** p5's blend words onto @p paint, as Skia spells them. */
void setBlend(SkPaint& paint, Constant mode);

/** p5's arc angles made drawable: both brought into [0, 2π), corrected
 *  for the ellipse's own aspect, and ordered. */
void normalizeArc(float& startOut, float& stopOut, float w, float h,
                  bool& samePoint);

/** One material onto one SkPaint: a solid is a colour, anything else a
 *  shader. Answers whether the shader has to be resolved again on every
 *  draw, which a live or box-dependent material does. */
bool resolve(const material::skia::Paint& material, SkPaint& paint,
             const material::skia::PaintFrame& frame);

/** Whether @p box is a unit square a material can be measured against: a
 *  horizontal line and a zero-radius circle are not, and asking a
 *  material to divide by their extent is how a fitted fill turns into
 *  nothing. */
bool fittable(const SkRect* box);

}  // namespace sigil::draw::detail
