#pragma once

/** @file
 * @ingroup paint
 *
 * Reusable animated shader presets for PaintStyle foregrounds and layers.
 *
 * Every helper compiles its SkSL program once per process; each call only
 * creates a small uniform block plus an SkShader, so callers can replace a
 * paint's shader every frame without rebuilding text, shaping, or layout.
 */

#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>

namespace sigil::weave::PaintShaders {

/** Animated, rippling blue water with fine caustic highlights. */
[[nodiscard]] sk_sp<SkShader> water(const SkRect &bounds, float timeSeconds);

/** Animated four-corner mesh-style gradient with softly moving control
 * regions. This is an SkShader suitable for glyph paint, rather than an
 * SkMesh geometry draw.
 */
[[nodiscard]] sk_sp<SkShader> meshGradient(const SkRect &bounds,
                                           float timeSeconds);

/** Transparent field of independently sized, tinted, twinkling points —
 * brightness and phase vary per point, so unlike a uniform pulse, points
 * fade in and out out of sync with their neighbors. Intended for a
 * Screen/Plus overlay. */
[[nodiscard]] sk_sp<SkShader> sparkle(const SkRect &bounds, float timeSeconds);

/** Volumetric "star nest" raymarch: a deep, drifting field of glowing
 * fractal dust. Heavier than the other presets (nested raymarch loop), but
 * still cheap per glyph since only covered mask pixels evaluate it. Based on
 * Pablo Roman Andrioli's "Star Nest" (MIT licensed).
 */
[[nodiscard]] sk_sp<SkShader> starNest(const SkRect &bounds,
                                       float timeSeconds);

/** Drifting painterly sky and clouds built from layered ridged/fbm value
 * noise.
 */
[[nodiscard]] sk_sp<SkShader> clouds(const SkRect &bounds, float timeSeconds);

/** Endless raymarched kaleidoscope tunnel, falling away from the camera.
 * Based on a shader by \@notargs.
 */
[[nodiscard]] sk_sp<SkShader> tunnel(const SkRect &bounds, float timeSeconds);

} // namespace sigil::weave::PaintShaders
