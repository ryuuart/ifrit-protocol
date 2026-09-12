#pragma once

/** @file
 * The stamp behind the instanced leaf: the one draw a pool becomes, the
 * size past which it culls each sprite against the clip, and the properties the
 * cached mode memoizes the leaf on.
 *
 * Private to the kernel. A consumer describes a pool and an atlas and
 * never names any of this: the leaf's shape is the value, and the draw is
 * how the value is painted.
 */

#include <include/core/SkBlendMode.h>
#include <sigilcompose/core/Instances.h>

#include <cstddef>
#include <memory>

class SkCanvas;

namespace sigil::compose {
struct PaintContext;
}  // namespace sigil::compose

namespace sigil::compose::instancing::detail {

/** Past this many instances the stamp culls each sprite against the
 *  canvas's clip: below it the test costs more than the draws it saves. */
inline constexpr size_t kCullThreshold = 2048;

/** Every instance of @p pool as ONE atlas draw off @p atlas's baked
 *  sheet. */
void stamp(SkCanvas& canvas, const PaintContext& ctx, Atlas& atlas,
           const Pool& pool, SkBlendMode blend);

/** What the cached mode memoizes the leaf on: the two values by identity,
 *  and each one's own count of what it has become — the pool's, so a
 *  committed mutation redraws, and the atlas's, so a cell registered after
 *  the first describe is not answered with the picture recorded from the
 *  sheet before it. */
struct DataProps {
  std::shared_ptr<Atlas> atlas;
  std::shared_ptr<const Pool> pool;
  uint64_t revision = 0;
  uint64_t atlasRevision = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool operator==(const DataProps&) const = default;  // ptr identity + rev
};

}  // namespace sigil::compose::instancing::detail
