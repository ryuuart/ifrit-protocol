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
#include <deque>
#include <span>
#include <vector>

class SkCanvas;
class SkPixmap;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::compose {
struct PaintContext;
}

namespace sigil::compose::detail {

/** ONE PART OF A SILHOUETTE COMPOSED FROM HELD PARTS: the part's own
 *  silhouette, in its own frame, and the matrix that stands it in the
 *  composing node's space. A node's own paint is a part placed by the
 *  identity; each rigidly moving child is a part placed by its box and its
 *  transform; any other outline is one part placed by the identity. */
struct CoveragePart {
  SkPath outline;
  SkMatrix placed;
  /** The shape the part's inside is read from where it is not the outline
   *  itself — a run of an outline that only part of is shown keeps the
   *  shape it was cut from — and empty where it is. */
  SkPath inside;
};

/** WHAT A DECORATION CASTS FROM A SILHOUETTE before it is displaced and
 *  inked: the silhouette filled, or a band of `bandWidth` centred on its
 *  edge, widened outward by `spread` when filled, and blurred by a
 *  Gaussian of `sigma`, all in the node's own units. */
struct CoverageCast {
  float sigma = 0;
  float bandWidth = 0;  ///< zero fills the silhouette
  float spread = 0;     ///< a filled cast's outward growth
  /** Where the cast is displaced to in the part's own frame before it is
   *  drawn into its mask, so a part that only slides samples its mask on
   *  the grid it was drawn on. */
  SkVector offset = {0, 0};
  /** Where the mask's pixel grid stands within a pixel, at the density it
   *  is drawn at, so a part drawn between pixels samples its mask on the
   *  pixels it lands on. */
  SkVector grid = {0, 0};
  bool operator==(const CoverageCast&) const = default;
};

/** THE CASTS ONE MARK HOLDS, by part: each kept as an alpha mask in the
 *  part's own frame with the region where it is not empty, and made again
 *  only when the part's silhouette, the cast, the density or the device
 *  changes. A part that only turns or slides keeps its masks, which is
 *  exact because a Gaussian has no direction: blurring a turned silhouette
 *  and turning a blurred one are the same picture; a part whose ink
 *  changes keeps them too, since a cast is coverage and holds no colour.
 *
 *  A mask is drawn where the mark is drawn: on the device the recorder
 *  stands for, silhouette and blur alike, so a change of density costs the
 *  processor nothing; on the processor's raster where there is none. */
class HeldCoverageMasks {
 public:
  struct Mask {
    SkPath source;
    float density = 0;
    CoverageCast cast;
    /** The recorder the mask was asked for, null for the processor. */
    const skgpu::graphite::Recorder* recorder = nullptr;
    sk_sp<SkImage> image;
    /** The mask's pixels into the part's own frame. */
    SkMatrix toPart;
    /** Where the mask is not empty, in the part's own frame: the covered
     *  pixels where the processor drew it, the whole raster where the
     *  device did, since its pixels are not read back. */
    SkPath reach;
  };

  /** The cast @p cast of @p part at @p density, the @p index th part of
   *  the composed silhouette, drawn through @p recorder where there is one
   *  and on the processor where there is not. */
  const Mask& maskFor(size_t index, const SkPath& part, float density,
                      const CoverageCast& cast,
                      skgpu::graphite::Recorder* recorder);

 private:
  /** By part, then by cast. A deque, so a mask handed out stays where it
   *  is while a later cast of the same part is added beside it. */
  std::vector<std::deque<Mask>> m_byPart;
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
 *  inside the part's own silhouette (an inner shadow, each ink cut by
 *  the part's own coverage and laid over the one before). Parts are painted one
 * over another, which is the cast of their union wherever they do not overlap.
 * A cast inside any other outline — a bevel or an inner shadow on a glyph or a
 * shape — holds its masks as one part placed where the outline stands. False,
 * painting nothing, where the mark keeps nothing between paints, or where a
 * cast outside an outline is not composed from held parts; the mark then casts
 * its outline itself. */
bool paintHeldCoverage(SkCanvas& canvas, const PaintContext& context,
                       std::span<const CoverageInk> inks,
                       const CoverageCast& cast, bool insideOnly);

/** HOW MANY CASTS THIS PROCESS HAS DRAWN INTO A MASK, on the processor
 *  and on a device: what a mark that holds its masks does once, and again
 *  only for a new silhouette, cast, density or device. */
struct CoverageCastsDrawn {
  size_t processor = 0;
  size_t device = 0;
};
CoverageCastsDrawn coverageCastsDrawn();

/** HOW MANY COVERAGE TRACES THIS PROCESS HAS TAKEN: each one rasterises a
 *  node's content and traces the pixels it covered, which a node does once
 *  and again only when what it covers, its size, its scale or its
 *  threshold changes. */
size_t coverageTracesTaken();

/** THE ALPHA, AS A REGION: the pixels of @p alpha whose coverage is above
 *  zero and reaches @p covered. */
SkRegion regionOfCoveredPixels(const SkPixmap& alpha, uint8_t covered);

}  // namespace sigil::compose::detail
