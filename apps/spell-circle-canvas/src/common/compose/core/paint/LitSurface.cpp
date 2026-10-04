/** @file
 * A fill whose material states a lit surface, shaded under the lighting
 * in force. The prepared material inputs stand while lighting changes;
 * only the pass above them is replaced.
 */

#include <sigilmaterial/skia/Lit.h>

#include "PaintInternal.h"
#include "runtime/Instance.h"

namespace sigil::compose::detail {

namespace {
bool positioned(const material::Lighting& lighting) {
  return lighting && lighting.dependsOnPlacement();
}
}  // namespace

bool Instance::litUsesWorldSpace() const {
  if (inkPaint.paint && inkPaint.paint->usesWorldSpace()) return true;
  if (!inkPaint.paint) {
    const material::Lighting inForce =
        lighting ? *lighting : material::Lighting{};
    if ((!paragraph || textDirty) && description->textData) {
      // The cascade precedes materialization, so newly described spans
      // declare placement dependencies before their retained styles exist.
      for (const SpanRestyle& span : description->textData->spanRestyles)
        if (span.inkMaterial &&
            (material::skia::usesWorldSpace(*span.inkMaterial) ||
             positioned(
                 material::skia::lightingFor(*span.inkMaterial, inForce))))
          return true;
    } else if (textState && paragraph) {
      for (const TextState::Restyle& restyle : textState->restyles) {
        if (!restyle.paints(*paragraph)) continue;
        const material::Material& source =
            *restyle.style.paint.foregroundMaterial;
        if (material::skia::usesWorldSpace(source) ||
            positioned(material::skia::lightingFor(source, inForce)))
          return true;
      }
    }
  }
  if (lighting && positioned(*lighting)) {
    const ElementNode& node = painted();
    for (const Decoration& mark : node.backgrounds)
      if (mark.readsLighting()) return true;
    for (const Decoration& mark : node.foregrounds)
      if (mark.readsLighting()) return true;
    if (node.hasStrokePasses())
      for (const StrokePass& pass : node.strokeData->passes)
        if (pass.what.readsLighting()) return true;
    if (node.fxData)
      for (const Decoration& mark : node.fxData->overlays)
        if (mark.readsLighting()) return true;
  }
  const MaterialData* slot = fillSlotOf(*this);
  if (!slot || !slot->surfaced) return false;
  const material::Lighting under = material::skia::lightingFor(
      *slot->surfaced, lighting ? *lighting : material::Lighting{});
  return positioned(under);
}

const material::Paint* Instance::litFillOf(const MaterialData& slot) const {
  if (!slot.surfaced) return nullptr;
  const material::Material& from = *slot.surfaced;
  const bool materialChanged = !litMaterial || *litMaterial != from;
  if (materialChanged) {
    litMaterial = from;
    litFillInputs.reset();
    litFill.reset();
  }
  const material::Lighting under = material::skia::lightingFor(
      from, lighting ? *lighting : material::Lighting{});
  if (!under) {
    litFill.reset();
    litUnder.reset();
    return nullptr;
  }
  if (!litFillInputs) litFillInputs.emplace(from);
  if (!litFill || litUnder != lighting) {
    litFill = litFillInputs->under(under);
    litUnder = lighting;
  }
  return &*litFill;
}

}  // namespace sigil::compose::detail
