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

void detail::markDeclared(ElementNode* node, Property property) {
  node->declared.set(property);
  // A VALUE WRITTEN AFTER A KEYWORD is the later statement of the two, and
  // the later statement wins: the keyword stops standing the moment the
  // property is written as a value again. Nearly every node carries no
  // keyword table at all, so what this costs the common case is one test
  // of a null pointer.
  if (node->keywords) node->keywords->clear(property);
}

void detail::markKeyword(ElementNode* node, Property property,
                         sigil::weave::Keyword keyword) {
  if (!answersKeyword(property)) {
    warnPropertyAnswersNoKeyword(property);
    return;
  }
  node->declared.set(property);
  node->keywords.ensure().set(property, keyword);
}

}  // namespace sigil::compose
