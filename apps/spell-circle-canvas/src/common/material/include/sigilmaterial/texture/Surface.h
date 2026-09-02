#pragma once

/** @file
 * bevelNormals(): a normal map derived from an outline's coverage, so a
 * flat shape shades as though it had a rounded shoulder. It encodes
 * device-space normals (+y down, +z toward the viewer) and produces a
 * Texture a recipe's child slot takes — the other half of what a
 * reflective 2D surface is shaded from, beside an EnvironmentMap.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/texture/Texture.h>

namespace sigil::material {

/** A rounded-bevel normal map derived from a path's coverage, placed so
 *  a shader's device xy reads the normal under it. The map covers
 *  @p bounds (device px); pixels outside the shape encode flat (0,0,1).
 *  @p bevelPx is the shoulder width; @p heightScale steepens the bevel
 *  (1 = bevel as deep as wide). */
Texture bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                     float heightScale = 1);

/** `bevelNormals` over the path's own bounds outset by the bevel, so the
 *  shoulder has room on every side. */
Texture bevelNormals(const SkPath& path, float bevelPx, float heightScale = 1);

}  // namespace sigil::material
