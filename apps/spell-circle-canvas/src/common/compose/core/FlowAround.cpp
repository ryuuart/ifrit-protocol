/** @file
 * flowAround: what a node says its edge is, and the exclusions a text node
 * takes from the keyed nodes it flows around — plumbed into SigilWeave,
 * and answered as a request for another round of the layout loop when one
 * changes.
 */

#include <algorithm>
#include <boost/container/flat_set.hpp>

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

SkPath Composer::Impl::boundaryOutlineOf(Instance& target, float width,
                                         float height) {
  const ElementNode& node = *target.description;
  if (node.boundary == Boundary::Glyphs && node.kind == Kind::Text &&
      target.paragraph) {
    if (target.glyphOutlineRev != target.measuredRev) {
      target.glyphOutline = target.textLayout.glyphOutline();
      target.glyphOutlineRev = target.measuredRev;
    }
    if (!target.glyphOutline.isEmpty()) return target.glyphOutline;
  } else if (node.boundary == Boundary::Coverage && width > 0 && height > 0) {
    // Traced at the SAME device scale the painter traces at, so the two
    // readings are one cached answer and neither invalidates the other.
    const SkPath& traced = coverageOutline(target, {width, height}, hostScale);
    if (!traced.isEmpty()) return traced;
  }
  return hasResolvedSilhouette(target) ? resolvedShapeOf(target) : SkPath();
}

bool Composer::Impl::deriveFlow(Instance& inst) {
  bool relayout = false;
  const DeriveData* derive = &*inst.description->deriveData;

  if (inst.paragraph) {
    std::vector<Exclusion> exclusions;
    const SkRect own = absoluteRect(inst);
    for (const std::string& key : derive->flowAroundKeys) {
      auto it = byKey.find(key);
      if (it == byKey.end()) continue;
      if (borrowIsCyclic(inst, it->second)) continue;
      Instance& target = *it->second;
      const SkRect box = absoluteRect(target);
      Exclusion exclusion;
      exclusion.bounds = box.makeOffset(-own.left(), -own.top());
      // WHAT THE TARGET SAYS ITS EDGE IS — the one property it already
      // carries for its own decorations, read here for the same answer:
      // its glyph outlines on a text leaf under `Boundary::Glyphs`, the
      // silhouette of what it DREW under `Boundary::Coverage` (a cut-out,
      // a clip, a mask, a photograph's alpha, at the tolerance
      // `Element::threshold` set), its declared shape otherwise, and its
      // box when it declares none. The margin means the same thing in
      // every case — a disc of that radius round whatever edge is being
      // subtracted — so a truer edge is never a second rule.
      SkPath boundaryPath =
          boundaryOutlineOf(target, box.width(), box.height());
      if (!boundaryPath.isEmpty()) {
        exclusion.path = boundaryPath.makeTransform(SkMatrix::Translate(
            box.left() - own.left(), box.top() - own.top()));
        SkRect oval = SkRect::MakeEmpty();
        // A round silhouette is subtracted ANALYTICALLY: the same answer,
        // at one square root per line band instead of a walk of a
        // flattened outline. Only a true circle qualifies — an ellipse's
        // inscribed circle is a different shape, and this is the one place
        // where taking the near-enough answer would be silently wrong.
        if (exclusion.path.isOval(&oval) &&
            std::abs(oval.width() - oval.height()) <= 0.01f) {
          exclusion.circle = true;
          exclusion.bounds = oval;
          exclusion.path.reset();
        }
      }
      exclusions.push_back(std::move(exclusion));
    }
    if (exclusions != inst.exclusionsLocal) {
      inst.exclusionsLocal = std::move(exclusions);
      inst.contentRev++;
      if (inst.yoga)  // positioned text re-measures via positionedRect
        YGNodeMarkDirty(inst.yoga);
      inst.markPaintDirtyUp();
      relayout = true;
    }
  }

  return relayout;
}

}  // namespace sigil::compose
