#pragma once

/** @file
 * @ingroup material-skia
 *
 * A PALETTE as Skia takes it. A palette is not a ramp — it says there is
 * nothing between its entries, so its crossing samples nearest and never
 * blends. A ramp's stops reach Skia through the gradients on `Paint`.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>

namespace sigil::material::skia {

/** @p palette as an N x 1 image, one texel per entry, straight (not
 *  premultiplied) so an entry's own alpha survives the crossing.
 *
 *  This is how a fixed palette reaches a SHADER: the picture is one
 *  channel of indices, the table is one texture, and the lookup is a
 *  sample rather than a branch over N literals. Null for an empty
 *  palette. */
sk_sp<SkImage> paletteImage(const Palette& palette);

/** The same table as a material a slot takes, sampled NEAREST
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
