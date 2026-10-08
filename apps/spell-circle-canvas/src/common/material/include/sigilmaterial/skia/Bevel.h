#pragma once

/** @file
 * @ingroup material-skia
 *
 * bevelNormals(): a normal map derived from an outline's coverage, so a
 * flat shape shades as though it had a rounded shoulder. It encodes
 * device-space normals (+y down, +z toward the viewer) and produces a
 * Texture a recipe's slot takes; and bevelFace(), the face a bevel layer
 * style shades from those normals with the highlight direction and the
 * reflection it carries itself. Both stand in the Skia executor because the
 * outline arrives as a Skia path and its coverage is rasterised by Skia.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/texture/Texture.h>

#include <glm/mat3x3.hpp>

namespace sigil::material::skia {

/** A rounded-bevel normal map derived from a path's coverage, placed so
 *  a shader's device xy reads the normal under it. The map covers
 *  @p bounds (device px), including the blurred shoulder outside the path.
 *  Beyond that shoulder the field approaches a flat (0,0,1) normal.
 *  @p bevelPx is the shoulder width; @p heightScale steepens the bevel
 *  (1 = bevel as deep as wide). */
Texture bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                     float heightScale = 1);

/** `bevelNormals` over the path's own bounds outset by the bevel, so the
 *  shoulder has room on every side. */
Texture bevelNormals(const SkPath& path, float bevelPx, float heightScale = 1);

/** THE RELIEF @p options STATE, cut from the unit @p outline covers: its
 *  coverage softened over the shoulder as a height, differentiated into
 *  normals green down. @p outline is in logical pixels, rasterised at
 *  @p density device pixels to each, and the texture is placed through
 *  @p localToPaint, the logical pixels into the space the material it
 *  dresses is sampled in. Invalid — the unit left flat — for a shoulder or
 *  depth of zero or not finite, a density that is not positive, and an
 *  outline whose shoulder would need more than sixteen million pixels. */
Texture reliefNormals(const SkPath& outline, const ReliefOptions& options,
                      float density = 1,
                      const glm::mat3& localToPaint = glm::mat3(1.0f));

/** THE FACE OF A BEVEL SHADED FROM ITS SHOULDER (`BevelOptions::shadesFace`)
 *  as one shader in the node's pixels: @p shoulder is the normal map of the
 *  outline's softened coverage, green down, as `reliefNormals` cuts it with
 *  the shoulder `options.size` wide and `options.depth` high; the face's
 *  own `normal` and the `reflection` it shows resolve across the box
 *  @p frame states. Draw it over the outline; a slope leaning toward the
 *  style's highlight direction takes the highlight, one leaning away the
 *  shadow, a flat face neither. Null where the program cannot be made or
 *  the shoulder has no image. */
sk_sp<SkShader> bevelFace(const Texture& shoulder, const BevelOptions& options,
                          const FrameData& frame);

}  // namespace sigil::material::skia
