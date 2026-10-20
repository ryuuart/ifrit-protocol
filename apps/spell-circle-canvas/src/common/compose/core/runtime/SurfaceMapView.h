#pragma once

/** @file
 * Internal to the kernel — what the page is drawn as: the one map of its
 * surface every node paints as and the scale its white stands for, and the
 * output view transform with the material it was described as and the
 * surface it was lowered for.
 */

#include <include/core/SkImageInfo.h>
#include <sigilmaterial/skia/Filter.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <optional>

#include "description/ComposeInternal.h"

namespace sigil::compose::detail {

/** THE SURFACE MAP AND THE VIEW: one part of the composer's retained state,
 *  read off it by each field's own name. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct SurfaceMapView {
  // Composer::setSurfaceMap: the one map of their surface every node paints
  // as, or none for the page as it is seen.
  std::optional<material::texture::Role> surfaceMap;
  // Composer::setSurfaceMapScale: what the map's white stands for.
  float surfaceMapScale = 1;
  // True while draw() runs through the surface map canvas it made.
  bool drawingSurfaceMap = false;
  material::Filter view;  // output view transform (no filter = pass-through)
  // The view as its author described it, when they described a Material.
  // Kept because how a Material LOWERS depends on the surface it lands
  // on, which is known only at draw: a view whose channels are
  // independent runs as a table on an eight-bit surface and as its
  // program anywhere else. `viewColorType` is the surface `view` was
  // lowered for, so a stable surface lowers once and every later frame
  // compares one enum.
  std::optional<material::Material> viewMaterial;
  SkColorType viewColorType = kUnknown_SkColorType;
};

}  // namespace sigil::compose::detail
