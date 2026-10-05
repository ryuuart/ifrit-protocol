/** @file
 * A fill whose material states a lit surface, on a page painted as a
 * surface map, read as that map; and whether the ink in force at a node
 * samples root coordinates. Every other draw paints a lit fill's colours
 * flat, through the fill beneath.
 */

#include <sigilmaterial/skia/SurfaceMaps.h>

#include "PaintInternal.h"
#include "runtime/Instance.h"

namespace sigil::compose::detail {

bool Instance::inkUsesWorldSpace() const {
  if (inkPaint.paint) return inkPaint.paint->usesWorldSpace();
  if ((!paragraph || textDirty) && description->textData) {
    // The cascade precedes materialization, so newly described spans
    // declare placement dependencies before their retained styles exist.
    for (const SpanRestyle& span : description->textData->spanRestyles)
      if (span.inkMaterial && material::skia::usesWorldSpace(*span.inkMaterial))
        return true;
  } else if (textState && paragraph) {
    for (const TextState::Restyle& restyle : textState->restyles)
      if (restyle.paints(*paragraph) &&
          material::skia::usesWorldSpace(
              *restyle.style.paint.foregroundMaterial))
        return true;
  }
  return false;
}

const material::Paint* Instance::surfaceMapFillOf(
    const MaterialData& slot) const {
  if (!slot.surfaced) return nullptr;
  const material::Material& from = *slot.surfaced;
  if (!surfaceMapMaterial || *surfaceMapMaterial != from) {
    surfaceMapMaterial = from;
    surfaceMapFillInputs.reset();
    surfaceMapFill.reset();
  }
  // A page painted as a surface map paints the fill as that map of its
  // surface; the held paint is dropped when the map changes. Every other
  // draw paints the fill beneath, the material's colours flat.
  if (!owner || !owner->surfaceMap || !material::skia::isLit(from)) {
    surfaceMapFill.reset();
    return nullptr;
  }
  if (!surfaceMapFillInputs) surfaceMapFillInputs.emplace(from);
  if (!surfaceMapFill)
    surfaceMapFill = surfaceMapFillInputs->asMap(*owner->surfaceMap);
  return &*surfaceMapFill;
}

}  // namespace sigil::compose::detail
