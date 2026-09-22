#pragma once

/** @file
 * @ingroup compose-core
 *
 * The flex properties, as verbs: which way a node lays its children
 * out, whether they wrap, how the room left over is shared, and where
 * everything sits on each axis.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Layout.h>

namespace sigil::compose {

/** THE FLEX CONTAINER AND THE FLEX CHILD. Every one of these is CSS's
 *  property of the same meaning, and a node that says none of them is a
 *  column whose children stretch across it. */
template <class Derived>
class FlexVerbs {
 public:
  /** WHICH WAY THE MAIN AXIS RUNS and from which end the children are
   *  placed — CSS `flex-direction`. `FlexDirection::Column` when
   *  unstated.
   *
   *  @trap PHYSICAL, NOT LOGICAL. `Row` is horizontal and `Column`
   *  vertical whatever writing mode the text under this node is set in:
   *  a vertical passage does not turn its parent's flex axis. */
  Derived& flexDirection(FlexDirection direction);
  /** `flexDirection(FlexDirection::Row)`: the children run left to
   *  right.
   *
   *  @trap HORIZONTAL ALWAYS — it does not turn under a vertical writing
   *  mode. */
  Derived& row();
  /** `flexDirection(FlexDirection::Column)`: the children run top to
   *  bottom, which is what a node does when it says nothing.
   *
   *  @trap VERTICAL ALWAYS — it does not turn under a vertical writing
   *  mode, and it is not a column of TEXT: N frames of one story side by
   *  side are `kit::textColumns`, and the columns a vertical passage sets
   *  in are the passage's own. */
  Derived& column();
  /** WHAT BECOMES OF CHILDREN THAT OVERFLOW the main axis — CSS
   *  `flex-wrap`. `FlexWrap::NoWrap` when unstated; the bare call
   *  wraps. */
  Derived& flexWrap(FlexWrap wrap = FlexWrap::Wrap);
  /** THIS NODE'S SHARE OF THE ROOM LEFT OVER along the parent's main
   *  axis, as a weight against its siblings' — CSS `flex-grow`. Zero
   *  when unstated, so a node stays at its basis; the bare call is a
   *  weight of one. */
  Derived& flexGrow(float factor = 1.0f);
  /** THIS NODE'S SHARE OF THE OVERFLOW when the parent's main axis runs
   *  short — CSS `flex-shrink`. ONE when unstated, faithful to Yoga and
   *  CSS, which is why a stated width is a basis. */
  Derived& flexShrink(float factor);
  /** THE SIZE THE FLEX FACTORS START FROM along the parent's main axis
   *  — CSS `flex-basis`. Unstated, the node's own width or height on
   *  that axis is the basis. */
  Derived& flexBasis(Dimension d);
  /** WHERE THIS NODE'S CHILDREN SIT ACROSS its main axis — CSS
   *  `align-items`. `Align::Stretch` when unstated, so a child with no
   *  cross-axis size fills. */
  Derived& alignItems(Align a);
  /** WHERE THIS NODE SITS ACROSS its parent's main axis, overriding
   *  that parent's `alignItems()` for this child alone — CSS
   *  `align-self`. `Align::Auto` when unstated, which is to take the
   *  parent's. */
  Derived& alignSelf(Align a);
  /** HOW THIS NODE'S CHILDREN ARE DISTRIBUTED ALONG its main axis, and
   *  what becomes of the room left over — CSS `justify-content`.
   *  `Justify::Start` when unstated. */
  Derived& justifyContent(Justify j);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
  detail::ElementNode* declare(Property property) {
    return detail::NodeAccess::declare(self(), property);
  }
  detail::ElementNode* declare(std::initializer_list<Property> properties) {
    detail::ElementNode* node = detail::NodeAccess::declarations(self());
    for (Property property : properties) detail::markDeclared(node, property);
    return node;
  }
};

}  // namespace sigil::compose
