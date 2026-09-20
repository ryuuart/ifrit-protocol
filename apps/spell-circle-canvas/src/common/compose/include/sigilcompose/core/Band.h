#pragma once

/** @file
 * @ingroup compose-core
 *
 * The band leaf's own verb: which side of its spine the band occupies.
 */

#include <sigilcompose/core/Declarations.h>

namespace sigil::compose {

/** A BAND IS A RIBBON ALONG A SPINE, and this is the one thing it says
 *  that no other leaf does. The formation is the offset-path lineage's:
 *  the band straddles the spine, or takes one side of it. */
template <class Derived>
class BandVerbs {
 public:
  /** The band straddles the spine — the default. No effect on a node
   *  that is not a band. */
  Derived& centered();
  /** The whole band sits on the LEFT of travel, which in screen space
   *  is outside a clockwise spine, so it exits a `shapes::` rect or
   *  circle. */
  Derived& outward();
  /** The whole band sits on the RIGHT of travel, which in screen space
   *  is inside a clockwise spine. */
  Derived& inward();

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
