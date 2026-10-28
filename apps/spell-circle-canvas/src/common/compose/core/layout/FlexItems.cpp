/** @file
 * The floor CSS's automatic minimum gives a text leaf in a column, and
 * the pass that writes it into each such leaf's flex style.
 */

#include "layout/FlexItems.h"

#include <sigilweave/paragraph/Paragraph.h>

#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace detail {

bool linesRunAcrossAColumn(const Instance& inst) {
  if (!inst.yoga || !inst.paragraph || inst.description->kind != Kind::Text)
    return false;
  if (inst.computed.clipContent) return false;
  if (inst.description->textData && inst.description->textData->onPath)
    return false;
  if (inst.paragraph->writingMode() != sigil::weave::WritingMode::kHorizontal)
    return false;
  // An absolute item, a pinned one and every child of a stack stand out
  // of the line whatever their container is. Read off the declarations
  // rather than Yoga's style, which a stack writes after its children's.
  const LayoutProps& layout = inst.computed.layout;
  if (layout.absolute || layout.centerAt.has_value() ||
      (inst.parent && inst.parent->description->kind == Kind::Stack))
    return false;
  const Instance* container = flexContainerOf(inst);
  return container && container->yoga && !container->description->arranges() &&
         !mainAxisHorizontal(container->computed.layout.direction);
}

bool keepsItsLines(const Instance& inst) {
  const LayoutProps& layout = inst.computed.layout;
  return layout.minHeight.unit == Dimension::Unit::Auto &&
         layout.basis.unit == Dimension::Unit::Auto &&
         linesRunAcrossAColumn(inst);
}

}  // namespace detail

void Composer::Impl::floorTextAtItsLines(Instance& inst) {
  // A leaf's floor reads its container's direction and its own paragraph,
  // which change without its own style being written again, so the floor
  // is settled over the whole tree each time layout runs. Writing the
  // value it already holds leaves the node clean.
  if (inst.yoga && inst.description->kind == Kind::Text)
    YGNodeStyleSetFlexShrink(
        inst.yoga, keepsItsLines(inst) ? 0.0f : inst.computed.layout.shrink);
  for (const auto& child : inst.children)
    if (child) floorTextAtItsLines(*child);
}

}  // namespace sigil::compose
