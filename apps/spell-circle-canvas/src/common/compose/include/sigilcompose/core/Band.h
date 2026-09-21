#pragma once

/** @file
 * @ingroup compose-core
 *
 * The band leaf's own verb: which side of its spine the band occupies.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilgeometry/path/Band.h>

namespace sigil::compose {

/** A BAND IS A RIBBON ALONG A SPINE, and this is the one thing it says
 *  that no other leaf does. */
template <class Derived>
class BandVerbs {
 public:
  /** WHICH SIDE OF THE SPINE the band occupies: `Center` straddles it
   *  (the default), `Outer` takes the LEFT of travel — outside a
   *  clockwise spine in screen space, so it exits a `shapes::` rect or
   *  circle — and `Inner` takes the right. */
  Derived& bandAlignment(geometry::path::Formation formation);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
