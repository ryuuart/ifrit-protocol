/** @file
 * The image leaf's own verb — the source region it draws.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& ImageVerbs<Derived>::imageRegion(SkRect sourceRect) {
  declarations()->imageData.ensure().region = sourceRect;
  return self();
}

template class ImageVerbs<Image>;

}  // namespace sigil::compose
