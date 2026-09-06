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

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::compose {

/** The tracks a node's text draws with: the description's fx() tracks,
 *  then the axis tracks its span restyles folded into. Indexed as one
 *  list by the painter's selection cache; a folded track sits past the
 *  end of trackAnims and so reads its progress at rest. */
inline std::span<const Track> paintedTracksOf(const detail::Instance& inst,
                                              std::vector<Track>& joined) {
  const std::span<const Track> declared = tracksOf(*inst.description);
  if (!inst.textState || inst.textState->spanAxisTracks.empty())
    return declared;
  joined.assign(declared.begin(), declared.end());
  joined.insert(joined.end(), inst.textState->spanAxisTracks.begin(),
                inst.textState->spanAxisTracks.end());
  return joined;
}
/** HOW MANY STEPS A SIZE-CUT LADDER OFFERS a glyph rendered at @p pixelSize:
 *  @p perPixel steps for each pixel of em, clamped to
 *  [@p minSteps, @p maxSteps].
 *
 *  Every distinct value a driven quantity takes is a distinct batch bucket
 *  AND a distinct glyph-atlas strike, so a smooth sweep left unsnapped
 *  rasterizes every addressed letter afresh on every frame. What ONE STEP
 *  displaces grows with the size the glyph is drawn at — a variation design
 *  unit and a rotation both move a fixed fraction of the em — so a ladder
 *  that does not grow with the size disappears on a caption and shows on a
 *  headline; it rises in proportion instead. The floor is where a finer
 *  ladder buys nothing the eye can use at a legible size, and the ceiling is
 *  what bounds the retained population at all, which is the only reason a
 *  ladder exists rather than the raw value. How fine each ladder is, and
 *  where its ends sit, is the caller's — the two questions differ. */
inline int ladderSteps(float pixelSize, float perPixel, int minSteps,
                       int maxSteps) {
  return std::clamp((int)std::lround(pixelSize * perPixel), minSteps, maxSteps);
}

/** How many directions the tangent ladder offers a glyph rendered at
 *  @p pixelSize. */
int tangentLadderSteps(float pixelSize);

/** Everything the pose depends on beyond the glyph itself. */
struct PoseContext {
  const detail::Instance* inst = nullptr;
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

/** WHERE @p key WAS LAST FILED in @p keys, or `keys.size()` for one that is
 *  not filed yet — the join-or-append every per-unit walk does, in one
 *  place, so the unit lists a mark, a beat and an annotation are numbered
 *  against cannot be built two different ways.
 *
 *  Glyphs arrive in draw order and a unit's glyphs are contiguous in it, so
 *  the entry a glyph joins is the one appended last: the scan runs backwards
 *  and stops on the first hit, which is one comparison for every glyph but
 *  the first of its unit. */
template <class Key>
size_t indexOfKey(const std::vector<Key>& keys, const Key& key) {
  for (size_t i = keys.size(); i-- > 0;)
    if (keys[i] == key) return i;
  return keys.size();
}

}  // namespace sigil::compose
