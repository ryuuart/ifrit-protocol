#pragma once

/** @file
 * Internal to the kernel's paint phase — what its translation units share:
 * the null-safe views into a description's rare-field blocks, the scalar
 * collection the volatility walk and the painter both read, the rest pose
 * of a glyph and the band it occupies, which the fx painter, the path
 * layout and the schedule query all resolve through one body.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/Shaper.h>

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Null-safe views into ElementNode's rare-field blocks (see ComposeInternal.h)

inline const Material* liveMaterialOf(const ElementNode& n) {
  return n.materialData && n.materialData->live ? &*n.materialData->live
                                                : nullptr;
}
inline const Material* metricFillOf(const ElementNode& n) {
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
  return hasTextFx(*inst.desc) || !inst.spanAxisTracks.empty();
}
/** The tracks a node's text draws with: the description's fx() tracks,
 *  then the axis tracks its span restyles folded into. Indexed as one
 *  list by the painter's selection cache; a folded track sits past the
 *  end of trackAnims and so reads its progress at rest. */
inline std::span<const Track> paintedTracksOf(const Instance& inst,
                                              std::vector<Track>& joined) {
  const std::span<const Track> declared = tracksOf(*inst.desc);
  if (inst.spanAxisTracks.empty()) return declared;
  joined.assign(declared.begin(), declared.end());
  joined.insert(joined.end(), inst.spanAxisTracks.begin(),
                inst.spanAxisTracks.end());
  return joined;
}
inline const sigil::image::ImageAsset* imageAssetOf(const ElementNode& n) {
  return n.imageData ? n.imageData->asset.get() : nullptr;
}
inline const Effect* layerEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->layerEffect ? &*n.fxData->layerEffect : nullptr;
}
inline const Effect* backdropEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->backdropEffect ? &*n.fxData->backdropEffect
                                              : nullptr;
}
inline const std::vector<Echo>& echoesOf(const ElementNode& n) {
  static const std::vector<Echo> kNoEchoes;
  return n.fxData ? n.fxData->echoes : kNoEchoes;
}

/** The group scalars of a subtree — every live float the memoized lane
 *  compares — appended to @p out; @p root marks the walk's own node. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out);

/** How many steps the tangent ladder offers a glyph rendered at
 *  @p pixelSize. */
int tangentLadderSteps(float pixelSize);

/** Everything the pose depends on beyond the glyph itself. */
struct PoseContext {
  const Instance* inst = nullptr;
  const sigil::weave::ParagraphLayout* layout = nullptr;
  const TextPath* onPath = nullptr;
  bool ridesPath = false;
  /** Where along the baseline the run sits this frame, as arc length — the
   *  delta on top of the `at` the path layout baked in. */
  float phaseArc = 0;
};

struct RestPose {
  SkPoint centre{0, 0};
  float cosine = 1, sine = 0;
  /// Glyph-local vector from the draw origin to `centre`. The horizontal
  /// convention, (halfAdvance, 0), is null here.
  std::optional<SkVector> centreOffset;
};

/** The rest pose of @p placed under @p ctx; false drops the glyph. */
bool restPoseOf(const PoseContext& ctx, const sigil::weave::PlacedGlyph& placed,
                RestPose& pose);

/** The band a glyph occupies either side of its own baseline, from the
 *  face's own metrics. Memoized per (face, size) across a walk: a
 *  paragraph is a handful of distinct fonts however many letters it has. */
struct GlyphBand {
  float ascent = 0, descent = 0;
};

/** The memo key is the face AND the size: metrics scale with the size, and
 *  a mixed-style paragraph is one face at several of them — keyed on the
 *  face alone, every run after the first would wear the first one's band. */
using BandKey = std::pair<const void*, float>;

GlyphBand bandOf(const sigil::weave::ShapedWord* shaped,
                 std::vector<std::pair<BandKey, GlyphBand>>& memo);
SkRect glyphBox(const sigil::weave::PlacedGlyph& placed, const RestPose& pose,
                const GlyphBand& band);

}  // namespace sigil::compose
