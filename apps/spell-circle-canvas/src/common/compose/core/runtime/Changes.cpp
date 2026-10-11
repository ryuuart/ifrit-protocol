/** @file
 * What a draw changes: the region of the canvas outside which the next draw
 * paints what the last one did, for a host that keeps its pixels and
 * repaints only that region, and the check after the draw that it painted
 * nothing outside it.
 */

#include <include/core/SkImageFilter.h>
#include <include/core/SkMatrix.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Filter.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ComposeRuntime.h"
#include "paint/PaintInternal.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** Where a change under a node reaches once the node's own layer effect has
 *  run over it: a blur, a glow or a shadow spreads what changed beneath it
 *  by its own reach. */
SkRect throughLayerEffect(const ElementNode& node, const SkRect& changed) {
  if (changed.isEmpty()) return changed;
  const material::Filter* effect = layerEffectOf(node);
  if (!effect) return changed;
  const sk_sp<SkImageFilter> filter =
      material::skia::resolvedImageFilter(*effect, nullptr);
  return filter ? filter->computeFastBounds(changed) : changed;
}

/** The children a node holds, in their order, as one number: a child added,
 *  removed or moved in the order changes it. */
uint64_t childrenSignature(const Instance& inst) {
  uint64_t signature = 1469598103934665603ull;
  for (const auto& child : inst.children) {
    signature ^= reinterpret_cast<uintptr_t>(child.get());
    signature *= 1099511628211ull;
  }
  return signature ^ inst.children.size();
}

/** The most separate rects a region keeps before it is held as the one
 *  rect around them all: past this a clip of many parts costs more than
 *  the pixels between them. */
constexpr size_t kMostParts = 8;

/** @p rect added to @p region: joined with every part it touches, so the
 *  parts stay apart, and the whole held as one rect once there are too
 *  many of them. */
void add(std::vector<SkRect>& region, SkRect rect) {
  if (rect.isEmpty()) return;
  for (size_t index = 0; index < region.size();) {
    if (SkRect::Intersects(region[index], rect)) {
      rect.join(region[index]);
      region.erase(region.begin() + (std::ptrdiff_t)index);
      index = 0;
      continue;
    }
    ++index;
  }
  region.push_back(rect);
  if (region.size() <= kMostParts) return;
  SkRect all = SkRect::MakeEmpty();
  for (const SkRect& part : region) all.join(part);
  region.assign(1, all);
}

}  // namespace

/** ONE NODE'S PART IN WHAT A DRAW CHANGES, added to @p region in its
 *  parent's space, with the node's extent written to @p extent: its own ink
 *  grown by its decorations, its children's extents unless it clips them,
 *  and its own layer effect's reach, placed by its layout rect and
 *  transform. A node whose own paint changed, whose own paint moves on the
 *  clock, that moved, or whose children changed adds the extent it had at
 *  the last draw and the one it has now; any other node adds what changed
 *  among its children, spread by its own layer effect. */
void Composer::Impl::changedUnder(Instance& inst, ChangeWalk& walk,
                                  std::vector<SkRect>& region, SkRect& extent) {
  const ElementNode& node = inst.painted();
  const ComputedStyle& style = inst.computed;
  const bool shown = style.layout.display != Display::None;
  extent = SkRect::MakeEmpty();
  SkMatrix placement = SkMatrix::I();
  const NodeTransform transform = transformOf(inst);
  // A backdrop reads what is beneath it, so a change anywhere under it
  // reaches as far as it does; a plane turned in depth or a shared space
  // places its ink through a projection this walk does not follow.
  if (backdropEffectOf(node) || hostsSpace(inst) || transform.spatial())
    walk.bounded = false;
  SkRect local = SkRect::MakeEmpty();
  if (shown) {
    const SkRect rect = instanceRect(inst);
    placement = transform.matrix({rect.left(), rect.top()}, rect.width(),
                                 rect.height());
    local = ownPaintBounds(inst);
  }
  std::vector<SkRect> childrenChanged;
  for (auto& child : inst.children) {
    SkRect childExtent = SkRect::MakeEmpty();
    changedUnder(*child, walk, childrenChanged, childExtent);
    if (shown && !style.clipContent) local.join(childExtent);
  }
  if (shown) extent = placement.mapRect(throughLayerEffect(node, local));
  const uint64_t children = childrenSignature(inst);
  const bool changed = !inst.extentSeen || inst.changedOwn || inst.ownVaries ||
                       placement != inst.lastPlacement ||
                       children != inst.lastChildren;
  // A custom program is handed the canvas and may draw anywhere on it.
  if (changed && node.kind == Kind::Custom) walk.bounded = false;
  if (changed) {
    add(region, extent);
    if (inst.extentSeen) add(region, inst.lastExtent);
  } else if (shown) {
    for (const SkRect& part : childrenChanged)
      add(region, placement.mapRect(throughLayerEffect(node, part)));
  }
  if (walk.commit) {
    inst.lastExtent = extent;
    inst.lastPlacement = placement;
    inst.lastChildren = children;
    inst.extentSeen = true;
    inst.changedOwn = false;
  }
}

std::optional<std::vector<SkRect>> Composer::Impl::changedRegion(bool commit) {
  if (!root) return std::nullopt;
  ChangeWalk walk{.commit = commit};
  std::vector<SkRect> region;
  SkRect extent = SkRect::MakeEmpty();
  changedUnder(*root, walk, region, extent);
  if (!walk.bounded || changesUnknown) return std::nullopt;
  return region;
}

}  // namespace sigil::compose
