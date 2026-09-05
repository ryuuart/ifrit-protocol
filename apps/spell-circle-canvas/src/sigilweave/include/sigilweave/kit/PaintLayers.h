#pragma once

/** @file
 * @ingroup paint
 *
 * The three paint layers everyone writes: a shadow, a glow and an
 * outline.
 *
 * `PaintLayer` is the mechanism — an SkPaint and an offset, drawn under
 * or over the run, with `blurred()` for attaching a blur mask to one.
 * These three are the arrangements of it a caller would otherwise
 * assemble by hand every time, with the constants a shadow and a glow
 * are usually asked for already chosen.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPoint.h>

#include "sigilweave/style/PaintLayer.h"

namespace sigil::weave::kit {

/** A blurred, offset solid copy, normally used as an underlay.
 *
 *  @p spread dilates the source shape (stroke-and-fill) before the blur
 *  mask is applied, so a wide blur keeps a solid core instead of thinning
 *  a hairline glyph outline down to near-transparency. @p intensity
 *  scales @p color's alpha, letting a caller push a pass brighter without
 *  picking a new hex value; above 1 it clamps to fully opaque. */
[[nodiscard]] PaintLayer dropShadow(SkColor color = 0x66000000,
                                    SkVector offset = {2, 2},
                                    float blurSigma = 2.0f, float spread = 0.0f,
                                    float intensity = 1.0f);

/** A zero-offset blurred copy, normally used as an underlay. @p spread
 *  and @p intensity mean what they mean for `dropShadow`. */
[[nodiscard]] PaintLayer glow(SkColor color, float blurSigma,
                              float spread = 0.0f, float intensity = 1.0f);

/** A stroked copy, normally placed beneath the foreground. */
[[nodiscard]] PaintLayer outline(SkColor color, float width,
                                 SkPaint::Join join = SkPaint::kRound_Join);

}  // namespace sigil::weave::kit
