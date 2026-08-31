/** @file
 * The Element verbs: copy-on-write mutations of one node's description.
 * Nothing here cooks geometry, resolves a material or touches a clock —
 * that is the Scene's side of the seam.
 */

#include <sigilworld/element/Element.h>
#include <sigilworld/element/Node.h>

#include <optional>
#include <utility>

namespace sigil::world {

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

// ---- identity and composition ---------------------------------------------

Element& Element::key(std::string_view k) {
  m_node->key.assign(k);
  return *this;
}

Element& Element::child(Element e) {
  m_node->children.push_back(std::move(e));
  return *this;
}

// ---- placement -------------------------------------------------------------

Element& Element::at(glm::vec3 position) {
  return translateX(position.x).translateY(position.y).translateZ(position.z);
}

Element& Element::translateX(motion::Animatable<float> v) {
  m_node->transform.translateX = std::move(v);
  return *this;
}
Element& Element::translateY(motion::Animatable<float> v) {
  m_node->transform.translateY = std::move(v);
  return *this;
}
Element& Element::translateZ(motion::Animatable<float> v) {
  m_node->transform.translateZ = std::move(v);
  return *this;
}
Element& Element::rotateX(motion::Animatable<float> degrees) {
  m_node->transform.rotateX = std::move(degrees);
  return *this;
}
Element& Element::rotateY(motion::Animatable<float> degrees) {
  m_node->transform.rotateY = std::move(degrees);
  return *this;
}
Element& Element::rotateZ(motion::Animatable<float> degrees) {
  m_node->transform.rotateZ = std::move(degrees);
  return *this;
}
Element& Element::rotate(glm::vec3 axis, motion::Animatable<float> degrees) {
  m_node->transform.axis = axis;
  m_node->transform.axisDegrees = std::move(degrees);
  return *this;
}
Element& Element::scale(motion::Animatable<float> factor) {
  m_node->transform.scaleX = factor;
  m_node->transform.scaleY = factor;
  m_node->transform.scaleZ = std::move(factor);
  return *this;
}
Element& Element::scaleX(motion::Animatable<float> factor) {
  m_node->transform.scaleX = std::move(factor);
  return *this;
}
Element& Element::scaleY(motion::Animatable<float> factor) {
  m_node->transform.scaleY = std::move(factor);
  return *this;
}
Element& Element::scaleZ(motion::Animatable<float> factor) {
  m_node->transform.scaleZ = std::move(factor);
  return *this;
}
Element& Element::transformOrigin(glm::vec3 origin) {
  m_node->transform.originX = origin.x;
  m_node->transform.originY = origin.y;
  m_node->transform.originZ = origin.z;
  return *this;
}
Element& Element::transform(const glm::mat4& matrix) {
  m_node->transform.matrix = matrix;
  return *this;
}
Element& Element::along(Spline3 spline, motion::Animatable<float> distance) {
  m_node->along = Along{std::move(spline), std::move(distance)};
  return *this;
}

// ---- what it is made of ----------------------------------------------------

Element& Element::fill(material::Material m) {
  ElementNode* node = m_node.operator->();
  node->material = std::move(m);
  node->slots.clear();
  return *this;
}

Element& Element::fill(std::span<const material::Material> slots) {
  ElementNode* node = m_node.operator->();
  node->slots.assign(slots.begin(), slots.end());
  node->material.reset();
  return *this;
}

// ---- geometry --------------------------------------------------------------

namespace {

/** The stamp a slot is carrying, taken out of it — so `cloud()` and
 *  `chain()` can replace what holds it without dropping the body that
 *  was already declared. */
Mesh takeStamp(Geometry& geometry) {
  if (auto* stamped = std::get_if<Stamped>(&geometry))
    return std::move(stamped->stamp);
  if (auto* chained = std::get_if<Chained>(&geometry))
    return std::move(chained->stamp);
  return {};
}

}  // namespace

Element& Element::mesh(Mesh m) {
  m_node->geometry = std::move(m);
  return *this;
}

Element& Element::cloud(Cloud c) {
  ElementNode* node = m_node.operator->();
  Mesh stamp = takeStamp(node->geometry);
  node->geometry = Stamped{std::move(c), std::move(stamp)};
  return *this;
}

Element& Element::chain(Chain c, PopRuntime runtime) {
  ElementNode* node = m_node.operator->();
  Mesh stamp = takeStamp(node->geometry);
  node->geometry = Chained{std::move(c), std::move(runtime), std::move(stamp)};
  return *this;
}

Element& Element::stamp(Mesh s) {
  ElementNode* node = m_node.operator->();
  Geometry& geometry = node->geometry;
  if (auto* stamped = std::get_if<Stamped>(&geometry))
    stamped->stamp = std::move(s);
  else if (auto* chained = std::get_if<Chained>(&geometry))
    chained->stamp = std::move(s);
  else if (std::holds_alternative<std::monostate>(geometry))
    // The empty slot holds the body until the cloud() or chain() that
    // follows takes it over, so the two calls read in either order.
    geometry = Stamped{{}, std::move(s)};
  return *this;
}

Element& Element::generate(Generator g) {
  m_node->geometry = std::move(g);
  return *this;
}

Element& Element::window(motion::Animatable<float> head,
                         motion::Animatable<float> span) {
  m_node->window = Window{std::move(head), std::move(span)};
  return *this;
}

// ---- membership, emitters, viewpoints --------------------------------------

Element& Element::tag(std::string word) {
  m_node->tags.push_back(std::move(word));
  return *this;
}

Element& Element::light(Light l) {
  m_node->light = l;
  return *this;
}

Element& Element::intensity(motion::Animatable<float> v) {
  std::optional<Emission>& emission = m_node->emission;
  if (!emission) emission.emplace();
  emission->intensity = std::move(v);
  return *this;
}

Element& Element::emission(motion::Animatable<float> red,
                           motion::Animatable<float> green,
                           motion::Animatable<float> blue) {
  std::optional<Emission>& emission = m_node->emission;
  if (!emission) emission.emplace();
  emission->red = std::move(red);
  emission->green = std::move(green);
  emission->blue = std::move(blue);
  return *this;
}

Element& Element::camera(Camera c) {
  m_node->camera = c;
  return *this;
}

// ---- caching and transitions -----------------------------------------------

Element& Element::cache(core::Cache c) {
  m_node->cachePolicy = c;
  return *this;
}

Element& Element::transition(const motion::Transition& t) {
  m_node->nodeTransition = t;
  return *this;
}

// ---- the memo --------------------------------------------------------------

Element detail::makeMemo(
    std::any props, std::function<bool(const std::any&, const std::any&)> equal,
    std::function<Element(const std::any&)> invoke) {
  Element element;
  element.node()->memo = Memo{std::move(props), std::move(equal),
                              std::move(invoke), core::detail::envStack()};
  return element;
}

}  // namespace sigil::world
