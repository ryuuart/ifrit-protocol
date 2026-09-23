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

void detail::DeclaredFields::keyword(Property property,
                                     sigil::weave::Keyword keyword) {
  if (!answersKeyword(property)) {
    warnPropertyAnswersNoKeyword(property);
    return;
  }
  m_storage.declared.set(property);
  m_storage.keywords.ensure().set(property, keyword);
}

void detail::DeclaredFields::cover() {
  const Absolute placed = absolute();
  placed.absolute = true;
  placed.hasInsets = true;
  top() = Dimension(0.0f);
  right() = Dimension(0.0f);
  bottom() = Dimension(0.0f);
  left() = Dimension(0.0f);
  m_storage.layout.covering = true;
}

void detail::DeclaredFields::leaveCover() {
  // The placement is withdrawn rather than overwritten: the node states
  // nothing about where it sits any more, and a mask that still said so
  // would make it unequal to a node that never covered.
  if (!m_storage.layout.covering) return;
  m_storage.layout.absolute = false;
  m_storage.layout.hasInsets = false;
  m_storage.layout.insets = EdgeDims{};
  m_storage.layout.covering = false;
  for (Property side : {Property::Absolute, Property::Left, Property::Top,
                        Property::Right, Property::Bottom})
    m_storage.declared.clear(side);
}

}  // namespace sigil::compose
