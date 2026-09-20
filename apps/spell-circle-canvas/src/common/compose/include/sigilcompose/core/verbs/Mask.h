#pragma once

/** @file
 * @ingroup compose-core
 *
 * The appearance-gating family, as verbs: what of a node's paint is
 * shown, and where.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Mask.h>

#include <utility>

namespace sigil::compose {

/** WHAT OF THIS NODE'S PAINT IS SHOWN. Paint-only and bindable, like
 *  the transforms: animating a mask never relayouts, and hit-testing
 *  keeps the UNMASKED shape. */
template <class Derived>
class MaskVerbs {
 public:
  /** Gate everything this node paints — the form to reach for first,
   *  and `mask(parts::all(), with)` written short. A gate addresses
   *  only the paint it CAN address. */
  Derived& mask(Gate with);
  /** Gate SOME of what this node paints. Repeated calls APPEND, and
   *  masks whose selections overlap INTERSECT on the overlap: both
   *  gates must pass. Each mask carries its own animation slots. */
  Derived& mask(Parts what, Gate with);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
