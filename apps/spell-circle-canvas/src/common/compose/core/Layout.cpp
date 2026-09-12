/** @file
 * Layout phase: Yoga's calculate pass and the bounded convergence loop
 * that settles custom layout() containers and centre pins against the
 * geometry Yoga produced. The text leaves it measures through are in
 * LayoutText.cpp, the derive pass the loop drives in Derive.cpp, and the
 * rects it all resolves to in Rects.cpp.
 */

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <iterator>
#include <optional>
#include <span>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Layout passes

bool Composer::Impl::phaseYoga() {
  YGNodeCalculateLayout(root->yoga, YGUndefined, YGUndefined, YGDirectionLTR);
  return false;
}

bool Composer::Impl::phaseCustomLayouts() {
  return hasCustomLayout && applyCustomLayouts(*root);
}

bool Composer::Impl::phaseCenterPins() {
  return hasCenterPins && applyCenterPins(*root);
}

bool Composer::Impl::phaseDerive() { return hasDerived && resolveDerived(); }

bool Composer::Impl::phasePathMarks() {
  // mark() on a path-laid run resolves HERE, not in measure with the flow
  // runs: the curve resolves against the node's final box, and only the
  // finished layout knows that box. The path layout underneath is memoized
  // against the box and the content; the mark walk itself is one pass over
  // the run's glyphs per layout. Runs before syncLayoutRects so a
  // mark-anchored child whose rect moved is seen by the same invalidation
  // walk as everything else.
  for (detail::Instance* marked : pathMarkInstances) resolveTextMarks(*marked);
  return false;
}

bool Composer::Impl::phaseSyncRects() {
  // Post-layout invalidation: recordings bake geometry (child offsets, text
  // lines, geometry-material uResolution), so any rect that moved or resized
  // must stale the recordings that captured it — even when NO prop changed
  // (setSize, sibling growth, measured-text reflow). Runs only when layout
  // ran; a stable layout is a no-op walk.
  syncLayoutRects(*root);
  return false;
}

void Composer::Impl::ensureLayout() {
  if (!root || (!needsLayout && !YGNodeIsDirty(root->yoga))) return;
  // The root fills the viewport (the CSS-root rule) — except under an empty
  // setSize(), which means "intrinsic": the root sizes to its content (the
  // snapshot()/stamp path).
  if (!size.isEmpty()) {
    YGNodeStyleSetWidth(root->yoga, size.width());
    YGNodeStyleSetHeight(root->yoga, size.height());
  }
  // The runner walks `phases` in order: a non-converging phase runs once,
  // and the converging phases form one contiguous group that repeats until
  // a round changes nothing or the round cap is reached. Custom layout()
  // placement, auto-sizing, centerAt pins and the derive phase all read
  // RESOLVED geometry and then write back into Yoga out of band, so each
  // can move what another already read: an auto-sized container changes the
  // box a pin centres in, and a pinned node moves an anchor a rail routed
  // through. Every writer here is idempotent and reports `changed` only on
  // an actual delta, so a stable layout costs one extra pass and exits, and
  // a settling one converges within the round cap.
  core::runPhases(*this, std::span<const core::Phase<Impl>>(phases),
                  kConvergeRounds, [this] {
                    phaseYoga();
                    // The settle step: refresh routes against the moved
                    // geometry. A route read before the relayout is a route
                    // on stale anchors, so the derive pass runs once more
                    // here rather than waiting for a round that may never
                    // come.
                    phaseDerive();
                  });
  needsLayout = false;
}

void Composer::Impl::syncLayoutRects(Instance& inst, bool movedAbove) {
  const SkRect r = instanceRect(inst);
  const bool rectChanged = r != inst.lastLayoutRect;
  if (rectChanged) {
    const bool sizeChanged = r.width() != inst.lastLayoutRect.width() ||
                             r.height() != inst.lastLayoutRect.height();
    inst.lastLayoutRect = r;
    if (sizeChanged)
      inst.markPaintDirtyUp();  // own content baked the old bounds
    else if (inst.parent)
      // The parent's RECORDING baked this child's old offset; the parent's
      // OWN paint did not — it never contained the child at all. The split
      // bake (own paint baked, volatile children drawn live over the blit)
      // is the cache tier that can tell those apart, and this is the case
      // it exists for: passing ownPaint=true here would remake the parent's
      // bake on every frame a child moves and the tier would silently never
      // pay off.
      inst.parent->markPaintDirtyUp(/*ownPaint=*/false);
    contentDirty = true;
  }
  // A world-space material samples its field in ROOT coordinates, so the
  // node's recording bakes the node-to-root matrix W. Any rect change at or
  // above the node changes W and must stale that recording:
  //   - its own move: the position-only branch above stales the PARENT, not
  //     this node, which is right for ordinary content and wrong here;
  //   - an ancestor's move: instanceRect is parent-relative, so this node's
  //     own rect compares equal while W has changed underneath it.
  // `movedAbove` carries any rect change, position or size, because a
  // resized ancestor with a centred transform origin moves its descendants'
  // W as surely as a repositioned one does.
  if (inst.hasWorldSpaceMaterial && (movedAbove || rectChanged)) {
    inst.markPaintDirtyUp();
    contentDirty = true;
  }
  for (auto& child : inst.children)
    syncLayoutRects(*child, movedAbove || rectChanged);
}

/** centerAt(): set the pinned node's left/top so its MEASURED box centres
 *  on the point. Runs after measurement, since the box is what it centres.
 *  Must stay idempotent — it reports a change only when the target position
 *  actually moved — or the bounded loop in ensureLayout would never see a
 *  quiet round and would burn its full round count every frame. */
bool Composer::Impl::applyCenterPins(Instance& inst) {
  bool applied = false;
  if (inst.description->layout.centerAt && inst.yoga) {
    const SkPoint p = *inst.description->layout.centerAt;
    // Correct by the observed layout delta rather than writing the target
    // into the style directly — converges whatever reference box Yoga
    // resolves absolute positions against (padding, borders).
    const float dl = (p.x() - YGNodeLayoutGetWidth(inst.yoga) / 2) -
                     YGNodeLayoutGetLeft(inst.yoga);
    const float dt = (p.y() - YGNodeLayoutGetHeight(inst.yoga) / 2) -
                     YGNodeLayoutGetTop(inst.yoga);
    if (std::abs(dl) > 0.25f || std::abs(dt) > 0.25f) {
      auto styleBase = [&](YGEdge edge) {
        const YGValue v = YGNodeStyleGetPosition(inst.yoga, edge);
        return v.unit == YGUnitPoint ? v.value : 0.0f;
      };
      YGNodeStyleSetPositionType(inst.yoga, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(inst.yoga, YGEdgeLeft, styleBase(YGEdgeLeft) + dl);
      YGNodeStyleSetPosition(inst.yoga, YGEdgeTop, styleBase(YGEdgeTop) + dt);
      applied = true;
    }
  }
  for (const auto& child : inst.children) applied |= applyCenterPins(*child);
  return applied;
}

/** THE SMALLEST @p child CAN BE without its content spilling — what a
 *  track-sizing rule floors a content-sized track at.
 *
 *  A text leaf is asked: laid out at a nil measure it wraps at every
 *  opportunity, so what it reports back is the widest run it cannot break,
 *  which is exactly the minimum. The measure it was standing at is then
 *  restored, because the layout the rest of the pass reads must be the one
 *  the node's own box produced and not this probe.
 *
 *  Everything else answers with what it measured. Layout measures once and
 *  never re-describes a child at a proposed width, so there is no honest
 *  smaller number to give for a box: reporting zero would let a content
 *  track collapse under content that cannot in fact shrink. */
SkSize Composer::Impl::minimumSizeOf(Instance& child) {
  const SkSize box{YGNodeLayoutGetWidth(child.yoga),
                   YGNodeLayoutGetHeight(child.yoga)};
  SkSize least = box;
  if (!child.paragraph) return least;
  const float wasWidth = child.measuredForWidth;
  const float wasHeight = child.measuredForHeight;
  layoutText(child, 0.0f, 1.0e6f);
  least.fWidth = child.measuredSize.width;
  // THE PROBE IS PUT BACK WHATEVER IT ANSWERED. A child that carries no
  // previous measure — the first pass, or a layout that degraded — is
  // laid out at the box it resolved to, because the alternative is
  // leaving it wrapped at nil width for the frame the probe ran in.
  if (wasWidth >= 0)
    layoutText(child, wasWidth, wasHeight);
  else
    layoutText(child, box.width(), box.height());
  return least;
}

bool Composer::Impl::applyCustomLayouts(Instance& inst) {
  bool applied = false;
  // layout() schemes are a flex-world feature; inside a positioned
  // subtree (no Yoga nodes) — or ON a positioned() container, whose
  // children have none — the placeFn is documented-unsupported.
  if (inst.yoga && !inst.description->layout.positioned &&
      inst.description->deriveData && inst.description->deriveData->placeFn &&
      !inst.children.empty()) {
    LayoutInput input;
    input.container = {YGNodeLayoutGetWidth(inst.yoga),
                       YGNodeLayoutGetHeight(inst.yoga)};
    for (const auto& child : inst.children) {
      input.childSizes.push_back({YGNodeLayoutGetWidth(child->yoga),
                                  YGNodeLayoutGetHeight(child->yoga)});
      // First-baseline offset from the child's top — measured text only
      // (pass one measured it); everything else has no baseline.
      float baseline = std::numeric_limits<float>::quiet_NaN();
      if (!child->lines.empty()) {
        const sigil::weave::LineMetrics& first = child->lines.front();
        baseline = first.baseline - first.rect().top();
      }
      input.childBaselines.push_back(baseline);
      input.childCells.push_back(child->description->layout.cells);
      // The region name is a rare field and lives in the child's derive
      // block; the scheme reads it beside the cell numbers.
      input.childAreas.push_back(child->description->deriveData
                                     ? child->description->deriveData->cellArea
                                     : std::string());
    }
    if (inst.description->deriveData->placeReadsMinSizes)
      for (const auto& child : inst.children)
        input.childMinSizes.push_back(minimumSizeOf(*child));
    std::vector<SkRect> rects = inst.description->deriveData->placeFn(input);
    const size_t count = std::min(rects.size(), inst.children.size());
    for (size_t i = 0; i < count; ++i) {
      // A centerAt() child opts OUT of the scheme's placement — the pin
      // wins (otherwise place() and the pin fight in a period-2
      // oscillation that never settles).
      if (inst.children[i]->description->layout.centerAt) continue;
      YGNodeRef child = inst.children[i]->yoga;
      // Count a change only on an actual delta: the convergence loop in
      // ensureLayout keys off this (idempotent writes are free).
      const SkRect cur = instanceRect(*inst.children[i]);
      if (std::abs(cur.left() - rects[i].left()) > 0.25f ||
          std::abs(cur.top() - rects[i].top()) > 0.25f ||
          std::abs(cur.width() - rects[i].width()) > 0.25f ||
          std::abs(cur.height() - rects[i].height()) > 0.25f)
        applied = true;
      YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
      YGNodeStyleSetPosition(child, YGEdgeLeft, rects[i].left());
      YGNodeStyleSetPosition(child, YGEdgeTop, rects[i].top());
      YGNodeStyleSetWidth(child, rects[i].width());
      YGNodeStyleSetHeight(child, rects[i].height());
    }
    // Auto-size an ABSOLUTE container from the placed extent, per axis,
    // when the author left that axis open (no explicit dim, no
    // opposing-inset pair) — an absolutely-positioned container has no flex
    // parent to size it, so without this it would collapse and the scheme
    // would place its children outside a zero box. Flex-embedded layout()
    // containers are left alone: their flex/stretch sizing already holds.
    //
    // A FLEX-EMBEDDED container is left alone on any axis its flex parent
    // already gave a size, and sized from the extent on an axis that
    // resolved to NOTHING — which is what a container whose children are
    // all absolutely placed collapses to, since none of them contributes
    // to it. Only the collapse is caught: a container that resolved to a
    // size has one for a reason, and overriding it here would fight
    // whatever gave it.
    const LayoutProps& l = inst.description->layout;
    SkRect extent = SkRect::MakeEmpty();
    for (size_t i = 0; i < count; ++i) extent.join(rects[i]);
    const bool widthPinned = l.hasInsets &&
                             l.insets.left.unit != Dimension::Unit::Auto &&
                             l.insets.right.unit != Dimension::Unit::Auto;
    const bool heightPinned = l.hasInsets &&
                              l.insets.top.unit != Dimension::Unit::Auto &&
                              l.insets.bottom.unit != Dimension::Unit::Auto;
    // …and it keeps sizing an axis it once sized. WHICH IT REMEMBERS: the
    // point width in the style is not evidence, because the placement loop
    // above writes point widths on every child, so a scheme nested in a
    // scheme would read its parent's placement as its own and override it.
    const bool sizesWidth = l.absolute ||
                            YGNodeLayoutGetWidth(inst.yoga) <= 0.25f ||
                            inst.schemeSizedWidth;
    const bool sizesHeight = l.absolute ||
                             YGNodeLayoutGetHeight(inst.yoga) <= 0.25f ||
                             inst.schemeSizedHeight;
    if (l.width.unit == Dimension::Unit::Auto && !widthPinned && sizesWidth &&
        extent.right() > 0 &&
        std::abs(YGNodeLayoutGetWidth(inst.yoga) - extent.right()) > 0.25f) {
      YGNodeStyleSetWidth(inst.yoga, extent.right());
      inst.schemeSizedWidth = true;
      applied = true;
    }
    if (l.height.unit == Dimension::Unit::Auto && !heightPinned &&
        sizesHeight && extent.bottom() > 0 &&
        std::abs(YGNodeLayoutGetHeight(inst.yoga) - extent.bottom()) > 0.25f) {
      YGNodeStyleSetHeight(inst.yoga, extent.bottom());
      inst.schemeSizedHeight = true;
      applied = true;
    }
  }
  for (const auto& child : inst.children) applied |= applyCustomLayouts(*child);
  return applied;
}

}  // namespace sigil::compose
