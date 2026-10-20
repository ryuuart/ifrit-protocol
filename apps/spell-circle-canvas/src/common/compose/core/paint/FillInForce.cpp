/** @file
 * resolveFillInForce: the fill a node's surface is painted with this
 * frame — a live material resolved per frame, a lit fill that reads its
 * unit answered as a surface map of the node's own outline, a paint
 * stretched over the content box, a fill partway along its ramp, or the
 * stored one.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "PaintInternal.h"
#include "UnitSurface.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

std::optional<Fill> resolveFillInForce(Composer::Impl& impl, Instance& inst,
                                       const PaintContext& paintContext,
                                       const SkRect& bounds,
                                       const SkPath* shapedSurface,
                                       const SkRRect& rrect) {
  const ComputedStyle& style = inst.computed;
  std::optional<Fill> resolvedFill;
  if (const material::Paint* live = liveMaterialOf(inst)) {
    // A LIT FILL THAT READS ITS UNIT, on a page painted as a surface map,
    // is that map of its material answered for the node's own outline:
    // the relief cut from it, the brushing along its long axis, sampled
    // where the fill's box places the material.
    std::optional<material::Paint> unitFill;
    if (impl.surfaceMap)
      if (const MaterialData* slot = fillSlotOf(inst);
          slot && slot->surfaced && material::readsUnit(*slot->surfaced)) {
        const glm::mat3& toRoot = paintContext.toRoot.matrix;
        detail::UnitShape unit;
        unit.outline = shapedSurface ? *shapedSurface : SkPath::RRect(rrect);
        unit.direction =
            detail::longAxisOf({bounds.width(), bounds.height()}, toRoot);
        unit.density = paintContext.bakeDensity > 0 ? paintContext.bakeDensity
                                                    : paintContext.contentScale;
        if (style.paint.fillBox == PaintBox::Canvas) {
          unit.localToPaint = toRoot;
        } else if (style.paint.fillBox == PaintBox::Content) {
          const detail::Insets inset = impl.paddingOf(inst);
          unit.localToPaint[2][0] = -inset.left;
          unit.localToPaint[2][1] = -inset.top;
        }
        if (!inst.unitSurfaces)
          inst.unitSurfaces = std::make_shared<detail::UnitSurfaces>();
        unitFill = inst.unitSurfaces->mapsFor(*slot->surfaced, unit)
                       .asMap(*impl.surfaceMap);
        live = &*unitFill;
      }
    // The fill's box: the paint's unit square is stretched over the
    // rectangle the fill names rather than the node's own. The painted
    // AREA does not move with it — a clip is what would move that, and
    // this library does not adopt one — so the paint is resolved against
    // that rectangle and then placed at its corner.
    // A border here is a stroke dressing the boundary rather than a box
    // lane, so the padding box IS the border box and only the content box
    // moves the paint.
    const bool pending = inst.hasPendingLiveFill && !unitFill;
    if (style.paint.fillBox != PaintBox::Content || pending) {
      resolvedFill =
          pending ? inst.pendingLiveFill : resolveFill(*live, paintContext);
    } else {
      const detail::Insets inset = impl.paddingOf(inst);
      PaintContext originCtx = paintContext;
      originCtx.size = {std::max(bounds.width() - inset.across(), 1.0f),
                        std::max(bounds.height() - inset.down(), 1.0f)};
      Fill placed = resolveFill(*live, originCtx);
      if (placed.kind == Fill::Kind::Paint)
        if (sk_sp<SkShader> shader =
                material::skia::staticShader(detail::paintOf(placed)))
          placed =
              detail::fillOf(material::skia::paint(shader->makeWithLocalMatrix(
                  SkMatrix::Translate(inset.left, inset.top))));
      resolvedFill = std::move(placed);
    }
  } else if (style.paint.fill) {
    Fill fill;
    if (style.paint.fill->identity())
      fill = style.paint.fill->value();
    else if (!inst.fillPath.empty() && inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->isRunning()) {
      const float t = inst.anims[Instance::kFillLerp]->value();
      // Any stop may be written as the ink in force or a custom property,
      // so each is resolved here before the colours are mixed.
      const auto resolve = [&paintContext](const Fill& stop) {
        return resolveFill(stop, paintContext);
      };
      fill = resolve(inst.fillPath.back().fill);
      fill.colorValue = colourAlong(inst.fillPath, t, resolve);
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProperty<Fill> resolved =
          resolveProperty(*style.paint.fill, inst.transitionInForce());
      fill = resolved.target;
    }
    resolvedFill = resolveFill(fill, paintContext);
  }
  return resolvedFill;
}

}  // namespace sigil::compose
