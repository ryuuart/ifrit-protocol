#pragma once

/** @file
 * The description of one node of a 3D scene: where it stands, what it is
 * made of, what it is called, and the children under it, as chaining
 * setters over a copy-on-write value.
 */

#include <sigilcore/cache/Policy.h>
#include <sigilcore/reconcile/Memo.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilworld/element/Geometry.h>
#include <sigilworld/light/Light.h>

#include <any>
#include <concepts>
#include <functional>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::world {

struct ElementNode;

// The rest of the currency, under the names a tree spells it — the same
// entities their own libraries define, reached by a shorter word.
using Camera = geometry::mesh::camera::Camera;
using Spline3 = geometry::mesh::curve::Spline3;
using Light = light::Light;
/** The stock emitters, reached by the word a tree is written in. */
using light::point;
using light::spot;
using light::sun;

/** ONE NODE OF A 3D SCENE, as a value.
 *
 *  An Element is built fresh every frame and thrown away: it holds no
 *  device resources, no entity and no running motion, and the retained
 *  tree behind it is the Scene's business. The chaining setters return
 *  `*this`, so a node reads as one expression, and the value is
 *  copy-on-write, so passing one around costs a refcount.
 *
 *  Where a concept exists in two dimensions this spells it the way
 *  SigilCompose spells it — `key`, `child`, `children`, `at`, `scale`,
 *  `fill`, `cache`, `bind`, `animate` — and the new spellings are the
 *  ones a plane does not have: the z lanes, the axis turn, the geometry
 *  slot, tags, emitters and viewpoints. */
class Element {
 public:
  Element();

  // ---- identity and composition ----
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, and what a scene addresses the node by. */
  Element& key(std::string_view k);
  Element& child(Element e);
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Element& children(R&& range) {
    for (auto&& e : range) child(std::move(e));
    return *this;
  }

  // ---- placement ----
  /** The node's position — `translateX/Y/Z` in one call. */
  Element& at(glm::vec3 position);
  Element& translateX(motion::Animatable<float> v);
  Element& translateY(motion::Animatable<float> v);
  Element& translateZ(motion::Animatable<float> v);
  Element& rotateX(motion::Animatable<float> degrees);
  Element& rotateY(motion::Animatable<float> degrees);
  Element& rotateZ(motion::Animatable<float> degrees);
  /** A turn about a direction the three axis lanes cannot spell. It
   *  applies after them. */
  Element& rotate(glm::vec3 axis, motion::Animatable<float> degrees);
  /** Uniform scale — writes all three scale lanes, so binding it binds
   *  all three to one output. */
  Element& scale(motion::Animatable<float> factor);
  Element& scaleX(motion::Animatable<float> factor);
  Element& scaleY(motion::Animatable<float> factor);
  Element& scaleZ(motion::Animatable<float> factor);
  /** The point the rotations and scales turn about, in the node's own
   *  coordinates. */
  Element& transformOrigin(glm::vec3 origin);
  /** THE ESCAPE: a placement computed outside this vocabulary. A node
   *  carrying one ignores every lane above. */
  Element& transform(const glm::mat4& matrix);
  /** Ride a curve: the node stands at @p distance along @p spline,
   *  turned onto the curve's own moving frame. The distance is in the
   *  spline's units — `Spline3::length()` is the total — and it is a
   *  lane like any other, so `bind()` tows the node along.
   *
   *  It replaces the translation lanes and the axis turn, and composes
   *  with the rest: the three rotation lanes, the scales and the origin
   *  still apply, inside the frame the curve put the node in. */
  Element& along(Spline3 spline, motion::Animatable<float> distance);

  // ---- what it is made of ----
  /** The surface. */
  Element& fill(material::Material m);
  /** …and the per-face form: one material per slot, in slot order. */
  Element& fill(std::span<const material::Material> slots);

  // ---- geometry ----
  /** A formed mesh. */
  Element& mesh(Mesh m);
  /** Points, with `stamp()` standing at each of them. */
  Element& cloud(Cloud c);
  /** A point chain and the runtime that cooks it, with `stamp()`
   *  standing at each cooked point. */
  Element& chain(Chain c, PopRuntime runtime = PopRuntime::cpu());
  /** The body standing at every point of a `cloud()` or a `chain()`. On
   *  a node whose slot holds neither, it is ignored. */
  Element& stamp(Mesh s);
  /** A value that builds its own mesh. */
  Element& generate(Generator g);
  /** A WINDOW INTO A LOOP: the leading edge and the length trailing it,
   *  both in loop parameter and both bindable, so advancing `head` alone
   *  tows the window round. It addresses the `chain()` in this node's
   *  slot, whose first operator carries the loop; a slot holding
   *  anything else ignores it.
   *
   *  A moving window is moving GEOMETRY: every distinct pair of values
   *  is a different chain and cooks its own points. */
  Element& window(motion::Animatable<float> head,
                  motion::Animatable<float> span);

  // ---- membership, emitters, viewpoints ----
  /** A word this node answers to, for whatever selects on it. Repeated
   *  calls append. */
  Element& tag(std::string word);
  /** An emitter standing where this node stands: the light's position
   *  and direction are carried by the node's transform, so `at()` and
   *  `along()` move it. */
  Element& light(Light l);
  /** A viewpoint standing where this node stands, on the same terms as
   *  `light()`: the camera's eye and target are carried by the node's
   *  transform. */
  Element& camera(Camera c);

  // ---- caching and transitions ----
  /** What the author asked of this node's cache. */
  Element& cache(core::Cache c);
  /** The node's default transition, for the plain constants on it. */
  Element& transition(const motion::Transition& t);

  // ---- integer-literal sugar --------------------------------------------
  // `rotateY(-8)` — an int does not convert into the Animatable variant on
  // its own and the resulting error is unreadable. Constrained to
  // std::integral so a float call can never land here: a plain int
  // overload would capture one through the standard conversion and recurse.
  template <std::integral T>
  Element& translateX(T v) {
    return translateX(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateY(T v) {
    return translateY(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateZ(T v) {
    return translateZ(motion::Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& rotateX(T deg) {
    return rotateX(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& rotateY(T deg) {
    return rotateY(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& rotateZ(T deg) {
    return rotateZ(motion::Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& scale(T f) {
    return scale(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleX(T f) {
    return scaleX(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleY(T f) {
    return scaleY(motion::Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleZ(T f) {
    return scaleZ(motion::Animatable<float>((float)f));
  }

  /** @private the reconciler's access to the description this value
   *  carries. */
  [[nodiscard]] const std::shared_ptr<ElementNode>& node() const {
    return m_node.value;
  }
  explicit Element(std::shared_ptr<ElementNode> n) : m_node(std::move(n)) {}

 private:
  /** Copy-on-write handle: an Element stays a cheap value, and a fluent
   *  mutation can never alter another copy or a description a Scene
   *  retains. */
  struct NodeHandle {
    explicit NodeHandle(std::shared_ptr<ElementNode> node)
        : value(std::move(node)) {}

    ElementNode* operator->();
    const ElementNode* operator->() const;

    std::shared_ptr<ElementNode> value;
  };

  NodeHandle m_node;
};

/** A deferred describe and its key — SigilCore's, over this library's
 *  description. */
using Memo = core::Memo<Element>;

namespace detail {
Element makeMemo(std::any props,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke);
}  // namespace detail

/** Deferred description: @p fn runs only when @p props changed (by
 *  `operator==`) since the last render at this position or key, AND the
 *  ambient `env::` bindings are unchanged — a memo is a pure function of
 *  (props, environment). The captured stack is re-established around the
 *  deferred call, so `env::inherited<T>()` inside @p fn reads what was
 *  bound where the memo was WRITTEN. */
template <class P, class F>
  requires std::equality_comparable<P> && std::invocable<F, const P&>
Element memo(P props, F fn) {
  return detail::makeMemo(
      std::any(std::move(props)),
      [](const std::any& a, const std::any& b) {
        return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
      },
      [fn = std::move(fn)](const std::any& p) -> Element {
        return fn(std::any_cast<const P&>(p));
      });
}

}  // namespace sigil::world
