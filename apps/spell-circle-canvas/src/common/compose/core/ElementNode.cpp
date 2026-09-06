/** @file
 * The paint values with bodies: a shader Fill, and the element node's
 * copy-on-write handle.
 */

#include <include/core/SkShader.h>

#include <memory>

#include "ComposeInternal.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

Fill Fill::shader(sk_sp<SkShader> s) {
  Fill f;
  f.kind = Fill::Kind::Shader;
  f.shaderValue = std::move(s);
  return f;
}

Element::Element() : m_node(std::make_shared<ElementNode>()) {}

ElementNode* Element::NodeHandle::operator->() {
  if (!value)
    value = std::make_shared<ElementNode>();
  else if (value.use_count() != 1)
    value = std::make_shared<ElementNode>(*value);
  return value.get();
}

const ElementNode* Element::NodeHandle::operator->() const {
  return value.get();
}

}  // namespace sigil::compose
