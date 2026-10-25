#pragma once

/** @file
 * Internal to the kernel's paint phase — what its translation units share:
 * the null-safe views into a description's rare-field blocks, the scalar
 * collection the volatility walk and the painter both read, and the parts
 * of a node's own paint the stacking painter hands off.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <optional>
#include <span>
#include <vector>

#include "runtime/ComposeRuntime.h"

class SkCanvas;
class SkPath;
class SkRRect;

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Null-safe views into ElementNode's optional rare-field blocks.

inline const material::Paint* liveMaterialOf(const ElementNode& n) {
  return n.materialData && n.materialData->live ? &*n.materialData->live
                                                : nullptr;
}
/** THE MATERIAL SLOT BEHIND THE FILL IN FORCE at this instance: the
 *  node's own where it states a fill, else the strongest matched rule's
 *  where one states it as a value, else null. */
inline const MaterialData* fillSlotOf(const Instance& inst) {
  const ElementNode& n = *inst.description;
  if (n.fields.declared().has(Property::Fill))
    return n.materialData ? &*n.materialData : nullptr;
  const RuleLayer* rules = inst.ruleLayer.get();
  if (rules == nullptr || !rules->material ||
      !rules->declared.has(Property::Fill) ||
      rules->keywords.find(Property::Fill))
    return nullptr;
  return &*rules->material;
}
/** The paint the fill in force resolves per frame or per size — a live
 *  or box-relative one, or the surface map of a lit fill on a page painted
 *  as one — or null where the fill is a plain `Fill`. */
inline const material::Paint* liveMaterialOf(const Instance& inst) {
  const MaterialData* slot = fillSlotOf(inst);
  if (slot && slot->surfaced)
    if (const material::Paint* lit = inst.surfaceMapFillOf(*slot)) return lit;
  return slot && slot->live ? &*slot->live : nullptr;
}
/** THE INK IN FORCE AS A PAINT at this instance, or null where the ink
 *  is a colour — which is the whole of nearly every tree. */
inline const material::Paint* inkPaintOf(const Instance& inst) {
  return inst.inkPaint.paint ? &*inst.inkPaint.paint : nullptr;
}
/** The node's textFx() tracks, or an empty span. */
inline std::span<const Track> tracksOf(const ElementNode& n) {
  return n.textData ? std::span<const Track>(n.textData->tracks)
                    : std::span<const Track>();
}
/** Does this node draw its text through the textFx() path? A track list whose
 *  every effect is empty is not fx text — the same test the volatility
 *  walk, the paint dispatch and the echo exclusion all read. The instance's
 *  folded span axes count too: they are tracks, only decided against the
 *  materialized paragraph rather than declared. */
inline bool hasTextFx(const ElementNode& n) {
  for (const Track& t : tracksOf(n))
    if (t.effect) return true;
  return false;
}
inline bool hasTextFx(const Instance& inst) {
  return hasTextFx(*inst.description) ||
         (inst.textState && !inst.textState->spanAxisTracks.empty());
}
inline const sigil::media::PixelSource* imageSourceOf(const ElementNode& n) {
  return n.imageData && n.imageData->source ? &n.imageData->source : nullptr;
}
inline const material::Filter* layerEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->layerEffect ? &*n.fxData->layerEffect : nullptr;
}
inline const material::Filter* backdropEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->backdropEffect ? &*n.fxData->backdropEffect
                                              : nullptr;
}
inline const std::vector<Echo>& echoesOf(const ElementNode& n) {
  static const std::vector<Echo> kNoEchoes;
  // both arms are references to live objects
  // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.UndefReturn)
  return n.fxData ? n.fxData->echoes : kNoEchoes;
}

/** THE EXTENT A LAYER EFFECT FILTERS, @p local grown out onto a grid of
 *  `kFilterGrid` units laid from the node's own origin. A filter reads the
 *  extent it is given — a wide blur is taken over a reduced copy laid
 *  about that extent's middle, and its faint far edge ends where the reach
 *  of the extent ends — so an extent that followed the marks' bleeds would
 *  move the picture with them. On the grid, every extent whose edges fall
 *  in the same cells is one extent, and its middle stands on a step of
 *  half a cell, which is where every reduction by a power of two up to
 *  half a cell lays the same texels. */
inline constexpr float kFilterGrid = 64.0f;
SkRect onFilterGrid(const SkRect& local);

/** A LOCAL BAKE'S RECT, @p local grown out onto the texel grid a bake at
 *  @p scale texels a unit lays from the node's own origin. A bake blitted
 *  under a turn or a filter is resampled from its texels, so a bake whose
 *  texels followed its bounds would move every pixel of its picture when
 *  a mark's bleed grew; on the grid, every bake of the same content holds
 *  the same texels wherever its bounds reach. */
SkRect onTexelGrid(const SkRect& local, float scale);

/** The group scalars of a subtree — every live float the memoized lane
 *  compares — appended to @p out; @p root marks the walk's own node. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out);

// ---------------------------------------------------------------------------
// The parts of a node's own paint the stacking painter hands off.

/** THE LEAF'S OWN CONTENT, painted into @p canvas across the node's
 *  @p bounds: a text leaf's passage with its echoes and readings, an image
 *  leaf's picture placed by its fit, a custom leaf's program. A box, a
 *  stack and a slot have none. */
void paintLeafContent(Composer::Impl& impl, Instance& inst, SkCanvas& canvas,
                      const PaintContext& paintContext, const SkRect& bounds);

/** THE FILL IN FORCE at this paint of a node of @p bounds — a live
 *  material resolved per frame, a lit fill that reads its unit, a paint
 *  stretched over the content box, a fill partway along its ramp, or the
 *  stored one — or nothing where the node states no fill.
 *  @p shapedSurface is the surface's path where it is not the corner box
 *  @p rrect, null where it is. */
std::optional<Fill> resolveFillInForce(Composer::Impl& impl, Instance& inst,
                                       const PaintContext& paintContext,
                                       const SkRect& bounds,
                                       const SkPath* shapedSurface,
                                       const SkRRect& rrect);

}  // namespace sigil::compose
