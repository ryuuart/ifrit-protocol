#pragma once

/** @file
 * @ingroup compose-core
 *
 * The image leaf's own verb: which part of its source it draws.
 */

#include <include/core/SkRect.h>
#include <sigilcompose/core/Declarations.h>

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
  Derived& region(SkRect sourceRect);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
