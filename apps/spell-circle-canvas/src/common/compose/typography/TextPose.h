#pragma once

/** @file
 * Internal to the typography tier — what its painting translation units
 * share: the tracks a node's text draws with, the rest pose of a glyph and
 * the band it occupies, which the fx painter, the path layout and the
 * schedule query all resolve through one body.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilweave/fonts/Shaper.h>

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

/** The tracks a node's text draws with: the description's fx() tracks,
 *  then the axis tracks its span restyles folded into. Indexed as one
 *  list by the painter's selection cache; a folded track sits past the
 *  end of trackAnims and so reads its progress at rest. */
inline std::span<const Track> paintedTracksOf(const Instance& inst,
                                              std::vector<Track>& joined) {
  const std::span<const Track> declared = tracksOf(*inst.desc);
  if (!inst.textState || inst.textState->spanAxisTracks.empty())
    return declared;
  joined.assign(declared.begin(), declared.end());
  joined.insert(joined.end(), inst.textState->spanAxisTracks.begin(),
                inst.textState->spanAxisTracks.end());
  return joined;
}
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
