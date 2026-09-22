/** @file
 * The operator seam's bodies: a lane read as a number whichever type it
 * was written in, the scope an adding operator reads and attaches to,
 * and the adapter between the two shapes an arranging operator takes —
 * the table a `place(const LayoutInput&)` scheme reads, built from the
 * arrangement's records, and the rectangles it answers written back.
 */

#include "sigilcompose/core/Operator.h"

#include <include/core/SkMatrix.h>

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

SkPath Scope::Node::toLocal(const SkPath& path) const {
  return path.makeTransform(SkMatrix::Translate(-bounds.left(), -bounds.top()));
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

Scope Scope::snapshot() const {
  Scope copy;
  copy.box = box;
  copy.m_nodes = m_nodes;
  for (Node& node : copy.m_nodes) node.m_scope = nullptr;
  return copy;
}

namespace detail {

LayoutInput layoutInputOf(const Arrangement& arrangement) {
  LayoutInput input;
  input.container = {arrangement.box.width(), arrangement.box.height()};
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

void placeFromRects(Arrangement& arrangement, const std::vector<SkRect>& rects) {
  const size_t count = std::min(rects.size(), arrangement.children.size());
  for (size_t i = 0; i < count; ++i) arrangement.children[i].rect = rects[i];
}

}  // namespace detail

}  // namespace sigil::compose
