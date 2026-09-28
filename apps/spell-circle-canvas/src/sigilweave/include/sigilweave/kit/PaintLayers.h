#pragma once

/** @file
 * @ingroup weave-kit
 *
 * The three paint layers everyone writes: a shadow, a glow and an
 * outline — the arrangements of `PaintLayer` a caller would otherwise
 * assemble by hand every time, with the constants a shadow and a glow
 * are usually asked for already chosen.
 */

#include <sigilgeometry/path/Stroke.h>
#include <sigilmaterial/color/Color.h>

#include <glm/vec2.hpp>

#include "sigilweave/style/PaintLayer.h"

namespace sigil::weave::kit {

/** A blurred, offset solid copy, normally used as an underlay.
 *  @p spread dilates the source shape before the blur mask is applied,
 *  so a wide blur keeps a solid core; @p intensity scales @p color's
 *  alpha and clamps to fully opaque above 1. */
[[nodiscard]] PaintLayer dropShadow(material::Color color = {0, 0, 0,
                                                            0x66 / 255.0f},
                                    glm::vec2 offset = {2, 2},
                                    float blurSigma = 2.0f, float spread = 0.0f,
                                    float intensity = 1.0f);

/** A zero-offset blurred copy, normally used as an underlay. @p spread
 *  and @p intensity mean what they mean for `dropShadow`. */
[[nodiscard]] PaintLayer glow(material::Color color, float blurSigma,
                              float spread = 0.0f, float intensity = 1.0f);

/** A stroked copy, normally placed beneath the foreground, turning its
 *  corners by @p join. */
[[nodiscard]] PaintLayer outline(
    material::Color color, float width,
    geometry::path::Join join = geometry::path::Join::Round);

}  // namespace sigil::weave::kit
