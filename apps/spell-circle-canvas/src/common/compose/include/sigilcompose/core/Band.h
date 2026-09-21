#pragma once

/** @file
 * @ingroup compose-core
 *
 * The band leaf's own verb: which side of its spine the band occupies.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/verbs/Node.h>
#include <sigilgeometry/path/Band.h>

#include <memory>
#include <utility>

namespace sigil::compose {

/** A BAND IS A RIBBON ALONG A SPINE, and this is the one thing it says
 *  that no other leaf does. */
template <class Derived>
class BandVerbs {
 public:
  /** WHICH SIDE OF THE SPINE the band occupies: `Center` straddles it
   *  (the default), `Outer` takes the LEFT of travel — outside a
   *  clockwise spine in screen space, so it exits a `shapes::` rect or
   *  circle — and `Inner` takes the right. The formation is the
   *  geometry spine's own, because that is what sweeps the ribbon; a
   *  stroke's `PathFormat::Align` says the same three words about a
   *  stroke on a node's outline, which is a different thing on a
   *  different outline. */
  Derived& bandAlignment(geometry::path::Formation formation);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

/** A BAND: the ribbon a spine sweeps out at a stated width across it.
 *  It lays out, fills, clips and takes stroke passes like any other
 *  node, and it alone says which side of its spine it takes. It
 *  CONVERTS to `Element`, so it drops into any `children({…})` block. */
class Band : public detail::Declaring,
             public NodeVerbs<Band>,
             public BandVerbs<Band> {
 public:
  /** @private the factories' door */
  explicit Band(std::shared_ptr<detail::ElementNode> n)
      : detail::Declaring(std::move(n)) {}

  operator Element() const;  // NOLINT: implicit by design (a leaf is a node)

 private:
  friend struct detail::NodeAccess;
};

}  // namespace sigil::compose
