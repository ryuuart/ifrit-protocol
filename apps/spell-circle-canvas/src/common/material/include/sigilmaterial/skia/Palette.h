#pragma once

/** @file
 * THE PALETTE A PICTURE IS MADE OF, from a picture Skia holds: the one
 * crossing between an image and the colour leaf's extraction.
 *
 * The choosing is `material::palette` over a run of colours and knows
 * nothing about images. What is here is the read: the picture down to a
 * size worth reading, its pixels as straight sRGB, and the table that
 * comes back.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/color/Extract.h>

namespace sigil::material::skia {

/** THE TABLE @p image IS MADE OF.
 *
 *  @p longestSide is how big the picture is read at: it is scaled so its
 *  longer side is at most this, and the table is chosen from what comes
 *  back. Scaling rather than skipping pixels, because a filtered
 *  reduction averages the pixels it drops and a stride steps over them —
 *  a red pinstripe survives one and vanishes under the other. Zero reads
 *  the picture at its own size.
 *
 *  An empty palette for a null image or one Skia cannot read back. */
[[nodiscard]] Palette palette(const sk_sp<SkImage>& image,
                              const PaletteOptions& options = {},
                              int longestSide = 128);

}  // namespace sigil::material::skia
