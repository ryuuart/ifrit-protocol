#pragma once

/** @file
 * A ramp of stops as Skia takes it.
 *
 * `RampStop` is the colour leaf's value — a position and a colour, and
 * nothing about how it is drawn. Two of these are the ramp's crossing:
 * the same stops as a shader over a vertical span in the coordinates a
 * node is painted in, and as a paint over the unit square, which is what
 * a text fill and a mask take. Both clamp outside their span, because a
 * ramp carries no answer for what lies beyond its ends.
 *
 * The other two are the PALETTE's, and a palette is not a ramp: it says
 * there is nothing between its entries, so its crossing samples nearest
 * and never blends. `Palette::at` is the same table read on the CPU —
 * one seam, two executors.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <vector>

namespace sigil::material::skia {

/** @p ramp as a gradient running down from @p y0 to @p y1. */
sk_sp<SkShader> verticalRamp(float y0, float y1,
                             const std::vector<RampStop>& ramp);

/** The same stops over the unit square, top to bottom. */
Paint unitRamp(const std::vector<RampStop>& ramp);

/** @p palette as an N x 1 image, one texel per entry, straight (not
 *  premultiplied) so an entry's own alpha survives the crossing.
 *
 *  This is how a fixed palette reaches a SHADER: the picture is one
 *  channel of indices, the table is one texture, and the lookup is a
 *  sample rather than a branch over N literals. Null for an empty
 *  palette. */
sk_sp<SkImage> paletteImage(const Palette& palette);

/** The same table as a material a `child()` slot takes, sampled NEAREST
 *  at texel centres, so entry n is entry n and not a blend of two.
 *
 *  The shader coordinate is in TEXELS, so the body reads it as
 *  `uPalette.eval(float2(index + 0.5, 0.5))` — the half is what puts the
 *  sample at the centre of the texel rather than on the seam between
 *  two, where the rounding decides the colour. Clamped both ways: an
 *  index past the end is the last entry, which keeps a mistake upstream
 *  visible as a flat band, exactly as `Palette::at` answers it on the
 *  CPU. An empty palette gives a material that paints nothing. */
Paint paletteLookup(const Palette& palette);

}  // namespace sigil::material::skia
