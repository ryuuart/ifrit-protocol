#pragma once

/** @file
 * @ingroup compose-core
 *
 * The image leaf's own verb: which part of its source it draws.
 */

#include <include/core/SkRect.h>
#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/verbs/Node.h>

#include <memory>
#include <utility>

namespace sigil::compose {

/** WHAT AN IMAGE LEAF DRAWS. How it SAMPLES is a cascade property and
 *  is stated with the rest of those; this is the one thing about the
 *  source that belongs to the leaf alone. */
template <class Derived>
class ImageVerbs {
 public:
  /** Draw this sub-rect of the asset, in SOURCE pixels, instead of the
   *  whole image — atlas and sprite regions. Strictly constrained, so
   *  neighbouring atlas cells never bleed in. */
  Derived& imageRegion(SkRect sourceRect);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

/** AN IMAGE LEAF: a picture, and the one thing only a picture says.
 *  Everything else about it — the box it takes, how it is filtered,
 *  what dresses it — is an ordinary node's. It CONVERTS to `Element`,
 *  so it drops into any `children({…})` block. */
class Image : public detail::Declaring,
              public NodeVerbs<Image>,
              public ImageVerbs<Image> {
 public:
  /** @private the factories' door */
  explicit Image(std::shared_ptr<detail::ElementNode> n)
      : detail::Declaring(std::move(n)) {}

  operator Element() const;  // NOLINT: implicit by design (a leaf is a node)

 private:
  friend struct detail::NodeAccess;
};

}  // namespace sigil::compose
