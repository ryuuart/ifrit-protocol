/** @file
 * The image leaf's own verbs — how it samples, and the source region it
 * draws.
 */

#include "ComposeInternal.h"

namespace sigil::compose {

Element& Element::sampling(SkSamplingOptions options) {
  m_node->imageData.ensure().sampling = options;
  return *this;
}

Element& Element::region(SkRect sourceRect) {
  m_node->imageData.ensure().region = sourceRect;
  return *this;
}

}  // namespace sigil::compose
