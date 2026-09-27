/** @file
 * The image leaf's own verb — the source region it draws.
 */

#include <sigilgeometry/advanced/Skia.h>
#include "ComposeInternal.h"

namespace sigil::compose {

template <class Derived>
Derived& ImageVerbs<Derived>::imageRegion(const geometry::path::Rect& source) {
  declarations()->imageData.ensure().region = geometry::path::toSk(source);
  return self();
}

template class ImageVerbs<Image>;

}  // namespace sigil::compose
