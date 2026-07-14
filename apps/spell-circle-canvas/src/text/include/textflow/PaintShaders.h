#pragma once

/** @file
 * Reusable animated shader presets for PaintStyle foregrounds and layers.
 *
 * The water and mesh helpers compile their SkSL programs once per process;
 * the star helper records one reusable vector tile. Each call only creates a
 * small uniform block or local matrix plus an SkShader, so callers can replace
 * a paint's shader every frame without rebuilding text, shaping, or layout.
 */

#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>

namespace textflow::PaintShaders {

/** Animated, rippling blue water with fine caustic highlights. */
[[nodiscard]] sk_sp<SkShader> water(const SkRect &bounds, float timeSeconds);

/** Animated four-corner mesh-style gradient with softly moving control
 * regions. This is an SkShader suitable for glyph paint, rather than an
 * SkMesh geometry draw.
 */
[[nodiscard]] sk_sp<SkShader> meshGradient(const SkRect &bounds,
                                           float timeSeconds);

/** Transparent drifting star field, intended for a Screen/Plus overlay. */
[[nodiscard]] sk_sp<SkShader> starField(const SkRect &bounds,
                                        float timeSeconds);

} // namespace textflow::PaintShaders
