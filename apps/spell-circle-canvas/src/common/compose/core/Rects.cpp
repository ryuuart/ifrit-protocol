/** @file
 * What a node's rect READS as, once layout has run: the box Yoga or a
 * positioned description gives it, and the same box in canvas
 * coordinates.
 */

#include <algorithm>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cmath>
#include <string>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Resolved-rect reads

SkRect Composer::Impl::instanceRect(const Instance& inst) const {
  if (inst.yoga)
    return SkRect::MakeXYWH(
        YGNodeLayoutGetLeft(inst.yoga), YGNodeLayoutGetTop(inst.yoga),
        YGNodeLayoutGetWidth(inst.yoga), YGNodeLayoutGetHeight(inst.yoga));
  return positionedRect(inst);
}

namespace {

/** A child keyed for a slot the text content DOES NOT DECLARE. Loud once,
 *  because the symptom — a pill that simply is not there — points at the
 *  child rather than at the misspelling in the caption that was supposed to
 *  reserve room for it.
 *
 *  A key the content does declare but the geometry could not place is a
 *  different answer and stays silent: it is the same "did not fit" a line
 *  clamp, an ellipsis or an exclusion gives every other word. */
void warnUnknownTextSlot(const Instance& text, const std::string& key) {
  for (const std::string& declared : text.textSlotKeys)
    if (declared == key)
      return;  // declared; the layout just could not place it
  static thread_local boost::unordered_flat_set<std::string> warned;
  if (!warned.insert(key).second) return;
  std::string have;
  for (const std::string& declared : text.textSlotKeys)
    have += (have.empty() ? "" : ", ") + declared;
  SkDebugf(
      "[compose] a child keyed \"%s\" sits on text that reserves no slot by "
      "that name, so it lays out at zero and draws nothing. Slots this "
      "text DOES reserve: [%s]. Room for one is reserved in the CONTENT, "
      "with rich(...).slot(name, size).\n",
      key.c_str(), have.empty() ? "none" : have.c_str());
}

}  // namespace

/** A positioned child's rect, straight from its description: px/pct
 *  left/top insets, px/pct dims, an open dim with an opposing
 *  right/bottom inset pinning the far edge, and text measuring against
 *  its resolved (or the parent's) width. O(depth) for pct/derived
 *  terms — arithmetic, no engine. */
SkRect Composer::Impl::positionedRect(const Instance& inst) const {
  // A MARK child: the rect its selector resolved is its PARENT BOX, not its
  // box. Everything the child says about its own placement is then read
  // against that rect exactly as a positioned child reads it against its
  // parent's — px or pct, in the rect's space, and free to land outside it
  // — and a child that says nothing at all simply IS the rect. That is the
  // whole difference from a slot below, whose box the CONTENT reserved and
  // whose child cannot argue with it. Looked up first for the same reason:
  // a text node may carry both, and a mark is not an unknown slot.
  const SkRect* anchor = nullptr;
  if (inst.parent && inst.parent->description->kind == Kind::Text &&
      inst.parent->description->textData && !inst.description->key.empty() &&
      std::ranges::any_of(inst.parent->description->textData->marks,
                          [&](const detail::MarkAnchor& mark) {
                            return mark.key == inst.description->key;
                          })) {
    for (const auto& [key, rect] : inst.parent->textMarkRects)
      if (key == inst.description->key) {
        anchor = &rect;
        break;
      }
    // A mark whose selector resolved NOTHING has no rect, and the honest
    // answer is an empty box rather than the child's own dims at the text
    // node's origin — which would be a mark drawn confidently in the wrong
    // place. resolveTextMarks() already said so once.
    if (!anchor) return SkRect::MakeEmpty();
  }
  // A TEXT SLOT child: its box is the placeholder rect the paragraph
  // reserved for its key, and nothing in the child's own description
  // decides it. Read here rather than written back into the tree because a
  // slot child has no Yoga node to write to — the paragraph IS its layout,
  // so a reflow that moves the placeholder moves the child with no second
  // pass and no convergence round.
  if (!anchor && inst.parent && inst.parent->description->kind == Kind::Text &&
      !inst.parent->textSlotKeys.empty() && !inst.description->key.empty()) {
    for (const auto& [key, rect] : inst.parent->textSlotRects)
      if (key == inst.description->key) return rect;
    warnUnknownTextSlot(*inst.parent, inst.description->key);
    return SkRect::MakeEmpty();
  }
  const LayoutProps& l = inst.description->layout;
  float parentW = 0, parentH = 0;
  if (anchor) {
    parentW = anchor->width();
    parentH = anchor->height();
  } else if (inst.parent) {
    const SkRect parentRect = instanceRect(*inst.parent);
    parentW = parentRect.width();
    parentH = parentRect.height();
  }
  auto resolve = [](const Dimension& d,
                    float parentExtent) -> std::optional<float> {
    switch (d.unit) {
      case Dimension::Unit::Px:
        return d.value;
      case Dimension::Unit::Pct:
        return parentExtent * d.value / 100.0f;
      case Dimension::Unit::Auto:
      default:
        return std::nullopt;
    }
  };
  const float left =
      l.hasInsets ? resolve(l.insets.left, parentW).value_or(0.0f) : 0.0f;
  const float top =
      l.hasInsets ? resolve(l.insets.top, parentH).value_or(0.0f) : 0.0f;
  std::optional<float> width = resolve(l.width, parentW);
  std::optional<float> height = resolve(l.height, parentH);
  if (!width && l.hasInsets)
    if (std::optional<float> right = resolve(l.insets.right, parentW))
      width = std::max(parentW - left - *right, 0.0f);
  if (!height && l.hasInsets)
    if (std::optional<float> bottom = resolve(l.insets.bottom, parentH))
      height = std::max(parentH - top - *bottom, 0.0f);
  // Text with an open extent: measure now, against the width we have.
  // The measure caches are logically mutable (measuredForWidth guards),
  // hence the casts.
  if (inst.description->kind == Kind::Text && inst.paragraph &&
      (!width || !height)) {
    const_cast<Composer::Impl*>(this)->layoutText(const_cast<Instance&>(inst),
                                                  width ? *width : parentW,
                                                  height ? *height : 1.0e6f);
    if (!width) width = inst.measuredSize.width;
    if (!height) height = inst.measuredSize.height;
  }
  if (anchor) {
    // An unstated extent on a mark is the ANCHOR's, which is what makes a
    // mark with no dims at all the unit's own rect. Text keeps the extent
    // it just measured for itself above.
    if (!width) width = parentW;
    if (!height) height = parentH;
    return SkRect::MakeXYWH(anchor->left() + left, anchor->top() + top, *width,
                            *height);
  }
  return SkRect::MakeXYWH(left, top, width.value_or(0.0f),
                          height.value_or(0.0f));
}

SkRect Composer::Impl::absoluteRect(const Instance& inst) const {
  SkRect rect = instanceRect(inst);
  for (Instance* p = inst.parent; p; p = p->parent) {
    const SkRect parentRect = instanceRect(*p);
    rect.offset(parentRect.left(), parentRect.top());
  }
  return rect;
}

}  // namespace sigil::compose
