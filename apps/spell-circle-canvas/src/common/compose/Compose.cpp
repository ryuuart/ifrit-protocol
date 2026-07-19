// Element value builders — thin mutations of the shared description
// payload. Nothing here talks to Yoga, Skia surfaces, or Choreograph;
// that is the Composer's job.

#include "ComposeInternal.h"

#include <include/core/SkShader.h>

namespace ifrit::compose {

using detail::ElementNode;
using detail::Kind;

Fill Fill::shader(sk_sp<SkShader> s) {
  Fill f;
  f.kind = Fill::Kind::Shader;
  f.shaderValue = std::move(s);
  return f;
}

Element::Element() : m_node(std::make_shared<ElementNode>()) {}

// ---- layout ---------------------------------------------------------------

Element &Element::row() { m_node->layout.row = true; return *this; }
Element &Element::column() { m_node->layout.row = false; return *this; }
Element &Element::gap(float px) { m_node->layout.gap = px; return *this; }
Element &Element::padding(float all) {
  m_node->layout.padding = {all, all, all, all};
  return *this;
}
Element &Element::padding(float h, float v) {
  m_node->layout.padding = {h, v, h, v};
  return *this;
}
Element &Element::margin(float all) {
  m_node->layout.margin = {all, all, all, all};
  return *this;
}
Element &Element::width(Dim d) { m_node->layout.width = d; return *this; }
Element &Element::height(Dim d) { m_node->layout.height = d; return *this; }
Element &Element::minWidth(Dim d) { m_node->layout.minWidth = d; return *this; }
Element &Element::maxWidth(Dim d) { m_node->layout.maxWidth = d; return *this; }
Element &Element::minHeight(Dim d) { m_node->layout.minHeight = d; return *this; }
Element &Element::maxHeight(Dim d) { m_node->layout.maxHeight = d; return *this; }
Element &Element::aspect(float r) { m_node->layout.aspect = r; return *this; }
Element &Element::grow(float f) { m_node->layout.grow = f; return *this; }
Element &Element::shrink(float f) { m_node->layout.shrink = f; return *this; }
Element &Element::basis(Dim d) { m_node->layout.basis = d; return *this; }
Element &Element::alignItems(Align a) {
  m_node->layout.alignItems = a;
  return *this;
}
Element &Element::alignSelf(Align a) {
  m_node->layout.alignSelf = a;
  return *this;
}
Element &Element::justify(Justify j) { m_node->layout.justify = j; return *this; }
Element &Element::absolute() { m_node->layout.absolute = true; return *this; }
Element &Element::inset(float all) { return inset(all, all, all, all); }
Element &Element::inset(float l, float t, float r, float b) {
  m_node->layout.absolute = true;
  m_node->layout.hasInsets = true;
  m_node->layout.insets = {l, t, r, b};
  return *this;
}

// ---- shape ----------------------------------------------------------------

Element &Element::corners(Corners c) { m_node->corners = c; return *this; }
Element &Element::clip(bool on) { m_node->clipContent = on; return *this; }

// ---- paint ----------------------------------------------------------------

Element &Element::fill(PropValue<Fill> f) {
  m_node->paint.fill = std::move(f);
  return *this;
}
Element &Element::background(Decoration d) {
  m_node->backgrounds.push_back(std::move(d));
  return *this;
}
Element &Element::foreground(Decoration d) {
  m_node->foregrounds.push_back(std::move(d));
  return *this;
}
Element &Element::opacity(PropValue<float> o) {
  m_node->paint.opacity = std::move(o);
  return *this;
}
Element &Element::blend(SkBlendMode mode) {
  m_node->paint.blendMode = mode;
  return *this;
}
Element &Element::translateX(PropValue<float> v) {
  m_node->paint.translateX = std::move(v);
  return *this;
}
Element &Element::translateY(PropValue<float> v) {
  m_node->paint.translateY = std::move(v);
  return *this;
}
Element &Element::rotate(PropValue<float> v) {
  m_node->paint.rotate = std::move(v);
  return *this;
}
Element &Element::scale(PropValue<float> v) {
  m_node->paint.scale = std::move(v);
  return *this;
}
Element &Element::transformOrigin(float fx, float fy) {
  m_node->paint.originX = fx;
  m_node->paint.originY = fy;
  return *this;
}
Element &Element::zIndex(int z) { m_node->paint.zIndex = z; return *this; }

// ---- identity, caching, transitions --------------------------------------

Element &Element::key(std::string_view k) {
  m_node->key = std::string(k);
  return *this;
}
Element &Element::cache(Cache c) { m_node->cacheMode = c; return *this; }
Element &Element::transition(Transition t) {
  m_node->nodeTransition = std::move(t);
  return *this;
}

Element &Element::child(Element e) {
  m_node->children.push_back(std::move(e));
  return *this;
}

// ---- factories ------------------------------------------------------------

Element box() { return {}; }

Element stack() {
  Element e;
  e.node()->kind = Kind::Stack;
  return e;
}

Element text(std::u8string utf8, textflow::TextStyle style) {
  Element e;
  e.node()->kind = Kind::Text;
  e.node()->textUtf8 = std::move(utf8);
  e.node()->textStyle = std::move(style);
  // "The box fits the type": measured text must not stretch on the
  // cross axis (the spike's API lesson).
  e.node()->layout.alignSelf = Align::Start;
  return e;
}

Element image(std::shared_ptr<const ifrit::image::ImageAsset> asset) {
  Element e;
  e.node()->kind = Kind::Image;
  e.node()->imageAsset = std::move(asset);
  return e;
}

Element custom(PaintProgram program) {
  Element e;
  e.node()->kind = Kind::Custom;
  e.node()->program = std::move(program);
  return e;
}

Element slot(std::string_view name) {
  Element e;
  e.node()->kind = Kind::Slot;
  e.node()->key = std::string(name);
  return e;
}

namespace detail {
Element makeMemo(std::any props,
                 std::function<bool(const std::any &, const std::any &)> equal,
                 std::function<Element(const std::any &)> invoke) {
  Element e;
  e.node()->isMemo = true;
  e.node()->memoProps = std::move(props);
  e.node()->memoEqual = std::move(equal);
  e.node()->memoInvoke = std::move(invoke);
  return e;
}
} // namespace detail

} // namespace ifrit::compose
