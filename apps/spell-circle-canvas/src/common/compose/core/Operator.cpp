/** @file
 * The operator seam's bodies: a lane read as a number whichever type it
 * was written in, the scope an adding operator reads and attaches to,
 * and the adapter between the two shapes an arranging operator takes —
 * the table a `place(const LayoutInput&)` scheme reads, built from the
 * arrangement's records, and the rectangles it answers written back.
 */

#include "sigilcompose/core/Operator.h"
#include <sigilgeometry/advanced/Skia.h>

#include <sigilgeometry/path/Transform.h>

#include <algorithm>
#include <memory>

#include "sigilcompose/core/Element.h"

namespace sigil::compose {

namespace {

std::optional<float> numberIn(const Attributes& attributes,
                              std::string_view name) {
  if (auto value = attributes.get<float>(name)) return *value;
  if (auto value = attributes.get<int>(name)) return (float)*value;
  if (auto value = attributes.get<double>(name)) return (float)*value;
  if (auto value = attributes.get<unsigned int>(name)) return (float)*value;
  if (auto value = attributes.get<long>(name)) return (float)*value;
  if (auto value = attributes.get<unsigned long>(name)) return (float)*value;
  return std::nullopt;
}

}  // namespace

std::optional<float> Arrangement::Child::number(std::string_view name) const {
  return numberIn(attributes, name);
}

std::optional<float> Scope::Node::number(std::string_view name) const {
  return numberIn(attributes, name);
}

bool Scope::Node::hasClass(std::string_view name) const {
  return std::find(classes.begin(), classes.end(), name) != classes.end();
}

geometry::path::Outline Scope::Node::toLocal(
    const geometry::path::Outline& outline) const {
  return outline.transformed(geometry::path::Transform::translate(-bounds.min));
}

void Scope::Node::attach(Element element) const {
  if (!m_scope) return;
  m_scope->m_attachments.push_back(
      Attachment{m_index, std::make_shared<Element>(std::move(element))});
}

std::vector<Scope::Node>& Scope::mutableNodes() {
  for (size_t i = 0; i < m_nodes.size(); ++i) {
    m_nodes[i].m_scope = this;
    m_nodes[i].m_index = i;
  }
  return m_nodes;
}

const Scope::Node* Scope::find(std::string_view key) const {
  if (key.empty()) return nullptr;
  for (const Node& node : m_nodes)
    if (node.key == key) return &node;
  return nullptr;
}

std::vector<const Scope::Node*> Scope::having(std::string_view lane) const {
  std::vector<const Node*> out;
  for (const Node& node : m_nodes)
    if (node.attributes.has(lane)) out.push_back(&node);
  return out;
}

std::vector<const Scope::Node*> Scope::withClass(std::string_view name) const {
  std::vector<const Node*> out;
  for (const Node& node : m_nodes)
    if (node.hasClass(name)) out.push_back(&node);
  return out;
}

void Scope::attach(Element element) {
  m_attachments.push_back(Attachment{
      Attachment::kScope, std::make_shared<Element>(std::move(element))});
}

Scope::Scope(const Scope& other) : box(other.box), m_nodes(other.m_nodes) {
  for (Node& node : m_nodes) node.m_scope = nullptr;
}

Scope& Scope::operator=(const Scope& other) {
  if (this == &other) return *this;
  auto nodes = other.m_nodes;
  for (Node& node : nodes) node.m_scope = nullptr;
  box = other.box;
  m_nodes = std::move(nodes);
  m_attachments.clear();
  return *this;
}

Scope::Scope(Scope&& other) noexcept
    : box(other.box), m_nodes(std::move(other.m_nodes)),
      m_attachments(std::move(other.m_attachments)) {
  for (Node& node : m_nodes)
    if (node.m_scope) node.m_scope = this;
}

Scope& Scope::operator=(Scope&& other) noexcept {
  if (this == &other) return *this;
  box = other.box;
  m_nodes = std::move(other.m_nodes);
  m_attachments = std::move(other.m_attachments);
  for (Node& node : m_nodes)
    if (node.m_scope) node.m_scope = this;
  return *this;
}

Scope Scope::snapshot() const {
  return Scope(*this);
}

namespace detail {

LayoutInput layoutInputOf(const Arrangement& arrangement) {
  LayoutInput input;
  input.container = arrangement.box.size();
  input.childSizes.reserve(arrangement.children.size());
  input.childBaselines.reserve(arrangement.children.size());
  input.childCells.reserve(arrangement.children.size());
  input.childAreas.reserve(arrangement.children.size());
  input.childAttributes.reserve(arrangement.children.size());
  for (const Arrangement::Child& child : arrangement.children) {
    input.childSizes.push_back(child.size);
    input.childBaselines.push_back(child.baseline);
    input.childCells.push_back(child.cells);
    input.childAreas.push_back(child.area);
    input.childAttributes.push_back(child.attributes);
  }
  // The minima are filled only for a scheme that asked, and the table
  // says so by their presence: a scheme reading them where none were
  // measured would read an empty list, which is the contract it declares
  // against.
  if (arrangement.minSizesMeasured)
    for (const Arrangement::Child& child : arrangement.children)
      input.childMinSizes.push_back(child.minSize);
  return input;
}

void placeFromRects(Arrangement& arrangement,
                    const std::vector<geometry::path::Rect>& rects) {
  const size_t count = std::min(rects.size(), arrangement.children.size());
  for (size_t i = 0; i < count; ++i) arrangement.children[i].rect = rects[i];
}

}  // namespace detail

}  // namespace sigil::compose
