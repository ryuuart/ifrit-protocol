/** @file
 * The paint values with bodies: a shader Fill, the copy-on-write handle
 * every declaring value is, and the conversion each typed leaf makes
 * into the node any container takes.
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

detail::Declaring::Declaring() : m_node(std::make_shared<ElementNode>()) {}

void detail::Declaring::append(Element child) {
  m_node->children.push_back(std::move(child));
}

Element::Element() = default;

Text::operator Element() const { return Element{m_node.value}; }
Image::operator Element() const { return Element{m_node.value}; }
Band::operator Element() const { return Element{m_node.value}; }

// A description value is the handle and nothing else: the verb mixins are
// empty, so a node still costs exactly one shared pointer to describe.
static_assert(sizeof(Element) == sizeof(std::shared_ptr<ElementNode>),
              "a verb mixin grew a field, and every Element in every "
              "children() vector pays for it");
static_assert(sizeof(Text) == sizeof(std::shared_ptr<ElementNode>));
static_assert(sizeof(Image) == sizeof(std::shared_ptr<ElementNode>));
static_assert(sizeof(Band) == sizeof(std::shared_ptr<ElementNode>));

ElementNode* detail::NodeHandle::operator->() {
  if (!value)
    value = std::make_shared<ElementNode>();
  else if (value.use_count() != 1)
    value = std::make_shared<ElementNode>(*value);
  return value.get();
}

const ElementNode* detail::NodeHandle::operator->() const {
  return value.get();
}

}  // namespace sigil::compose
