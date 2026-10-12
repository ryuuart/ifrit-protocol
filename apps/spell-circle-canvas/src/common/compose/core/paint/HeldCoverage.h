#pragma once

/** @file
 * A coverage decoration cast from a silhouette composed of held parts:
 * each part's cast — its coverage, filled or banded, and blurred — kept as
 * a mask in the part's own frame and placed where the part now stands.
 */

#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRegion.h>

#include <cstdint>
#include <span>
#include <vector>

class SkCanvas;
class SkPixmap;

namespace sigil::compose {
struct PaintContext;
}

namespace sigil::compose::detail {

/** ONE PART OF A SILHOUETTE COMPOSED FROM HELD PARTS: the part's own
 *  silhouette, in its own frame, and the matrix that stands it in the
 *  composing node's space. A node's own paint is a part placed by the
 *  identity; each rigidly moving child is a part placed by its box and its
 *  transform. */
struct CoveragePart {
  SkPath outline;
  SkMatrix placed;
};

/** WHAT A DECORATION CASTS FROM A SILHOUETTE before it is displaced and
 *  inked: the silhouette filled, or a band of `bandWidth` centred on its
 *  edge, widened outward by `spread` when filled, and blurred by a
 *  Gaussian of `sigma`, all in the node's own units. */
struct CoverageCast {
  float sigma = 0;
  float bandWidth = 0;  ///< zero fills the silhouette
  float spread = 0;     ///< a filled cast's outward growth
  bool operator==(const CoverageCast&) const = default;
};

/** THE CASTS ONE MARK HOLDS, by part: each kept as an alpha mask in the
 *  part's own frame with the region where it is not empty, and made again
 *  only when the part's silhouette, the cast or the density changes. A
 *  part that only turns or slides keeps its masks, which is exact because
 *  a Gaussian has no direction: blurring a turned silhouette and turning a
 *  blurred one are the same picture. */
class HeldCoverageMasks {
 public:
  struct Mask {
    SkPath source;
    float density = 0;
    CoverageCast cast;
    sk_sp<SkImage> image;
    /** The mask's pixels into the part's own frame. */
    SkMatrix toPart;
    /** Where the mask is not empty, in the part's own frame. */
    SkPath reach;
  };

  /** The cast @p cast of @p part at @p density, the @p index th part of
   *  the composed silhouette. */
  const Mask& maskFor(size_t index, const SkPath& part, float density,
                      const CoverageCast& cast);

 private:
  std::vector<std::vector<Mask>> m_byPart;
};

/** ONE INK A CAST IS PAINTED IN, and where it is displaced to in the
 *  node's space. */
struct CoverageInk {
  SkPaint ink;
  SkVector offset = {0, 0};
};

/** PAINTS A COVERAGE CAST through the held parts the mark at @p context is
 *  given: each of @p inks through each part's cast, displaced by its
 *  offset, either wherever the cast reaches (a drop shadow) or only
 *  inside the part's own silhouette (an inner shadow, where the inks are
 *  laid one over another in a single draw). Parts are painted one over
 *  another, which is the cast of their union wherever they do not
 *  overlap. False, painting nothing, where the mark's outline is not a
 *  silhouette composed from held parts; the mark then casts its outline
 *  itself. */
bool paintHeldCoverage(SkCanvas& canvas, const PaintContext& context,
                       std::span<const CoverageInk> inks,
                       const CoverageCast& cast, bool insideOnly);

/** THE ALPHA, AS A REGION: the pixels of @p alpha whose coverage is above
 *  zero and reaches @p covered. */
SkRegion regionOfCoveredPixels(const SkPixmap& alpha, uint8_t covered);

}  // namespace sigil::compose::detail
