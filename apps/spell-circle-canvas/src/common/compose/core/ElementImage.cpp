/** @file
 * The image leaf's own verb — the source region it draws.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::region(SkRect sourceRect) {
  m_node->imageData.ensure().region = sourceRect;
  return *this;
}

}  // namespace sigil::compose
