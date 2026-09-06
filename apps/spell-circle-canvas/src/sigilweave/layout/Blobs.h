#pragma once

/** @file
 * A SHAPED WORD AS SOMETHING THE CANVAS DRAWS. Three answers, private to
 * the layout stage: the word's own shared blob (`wordBlob`, in the fonts
 * feature), the same glyphs re-laid under a justification fit, and the
 * glyphs baked one transform each for a rotated or contour line.
 */

#include <include/core/SkTextBlob.h>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/PositionedRun.h"

namespace sigil::weave::detail {

/** The advance @p word takes under @p fit. */
[[nodiscard]] float advanceUnder(const GlyphFit& fit, const ShapedWord& word);

/** The word's glyphs re-laid with the extra advance and the horizontal
 *  scale @p fit states, which is what a justified line spends what its
 *  word gaps could not on. */
[[nodiscard]] sk_sp<SkTextBlob> buildFittedBlob(const ShapedWord& shapedWord,
                                                const GlyphFit& fit);

/** The word's glyphs baked one transform each, for a line that is rotated
 *  or rides a contour: @p penOffset is where the word begins along the
 *  interval, and @p rotationSteps is the tangent snapping the layout
 *  asked for. */
[[nodiscard]] sk_sp<SkTextBlob> buildTransformedBlob(
    const ShapedWord& shapedWord, const LineInterval& interval, float penOffset,
    int rotationSteps);

}  // namespace sigil::weave::detail
