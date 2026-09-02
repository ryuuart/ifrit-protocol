#pragma once

/** @file
 * Internal to the kernel's paint phase — what its translation units share:
 * the null-safe views into a description's rare-field blocks, the scalar
 * collection the volatility walk and the painter both read.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilweave/choreograph/Choreograph.h>

#include <span>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Null-safe views into ElementNode's rare-field blocks (see ComposeInternal.h)

inline const material::skia::Paint* liveMaterialOf(const ElementNode& n) {
  return n.materialData && n.materialData->live ? &*n.materialData->live
                                                : nullptr;
}
inline const material::skia::Paint* metricFillOf(const ElementNode& n) {
  return n.textData && n.textData->metricFill ? &*n.textData->metricFill
                                              : nullptr;
}
/** The node's fx() tracks, or an empty span. */
inline std::span<const Track> tracksOf(const ElementNode& n) {
  return n.textData ? std::span<const Track>(n.textData->tracks)
                    : std::span<const Track>();
}
/** Does this node draw its text through the fx() path? A track list whose
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
  return hasTextFx(*inst.desc) ||
         (inst.textState && !inst.textState->spanAxisTracks.empty());
}
inline const sigil::image::ImageAsset* imageAssetOf(const ElementNode& n) {
  return n.imageData ? n.imageData->asset.get() : nullptr;
}
inline const material::skia::Effect* layerEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->layerEffect ? &*n.fxData->layerEffect : nullptr;
}
inline const material::skia::Effect* backdropEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->backdropEffect ? &*n.fxData->backdropEffect
                                              : nullptr;
}
inline const std::vector<Echo>& echoesOf(const ElementNode& n) {
  static const std::vector<Echo> kNoEchoes;
  // both arms are references to live objects
  // NOLINTNEXTLINE(clang-analyzer-core.uninitialized.UndefReturn)
  return n.fxData ? n.fxData->echoes : kNoEchoes;
}

/** The group scalars of a subtree — every live float the memoized lane
 *  compares — appended to @p out; @p root marks the walk's own node. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out);

}  // namespace sigil::compose
