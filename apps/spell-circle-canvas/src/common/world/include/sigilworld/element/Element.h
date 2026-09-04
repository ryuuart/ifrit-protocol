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
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilworld/element/Environment.h>
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

/** WHICH SIDES OF A BODY'S TRIANGLES ARE DRAWN. Hidden is the default for
 *  closed solids; Visible keeps the reverse side of a sheet or panel when
 *  the viewpoint passes behind it. */
enum class Backface { Hidden, Visible };

// The emitters, under the name a tree spells them.
/** The emitter value, unchanged — `point`, `spot` and `sun` build one. */
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
  /** Appends @p e under this node. Children keep the order they were
   *  added in. */
  Element& child(Element e);
  /** Appends every element of @p range, in the range's order. */
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
  Element& along(geometry::mesh::curve::Spline3 spline,
                 motion::Animatable<float> distance);

  // ---- what it is made of ----
  /** The surface. */
  Element& fill(material::Material m);
  /** …and the per-face form: one material per slot, in slot order. */
  Element& fill(std::span<const material::Material> slots);

  // ---- geometry ----
  /** A formed mesh. */
  Element& mesh(geometry::mesh::Mesh m);
  /** Whether reverse-wound faces are culled or drawn. A flat panel that
   *  must survive an orbit uses `backface(Backface::Visible)`; a closed
   *  solid normally keeps the default `Hidden`. */
  Element& backface(Backface facing);
  /** Points, with `stamp()` standing at each of them. */
  Element& cloud(geometry::mesh::Cloud c);
  /** A point chain and the runtime that cooks it, with `stamp()`
   *  standing at each cooked point. */
  Element& chain(geometry::mesh::pop::Chain c,
                 geometry::mesh::pop::Runtime runtime =
                     geometry::mesh::pop::Runtime::cpu());
  /** The body standing at every point of a `cloud()` or a `chain()`. On
   *  a node whose slot holds neither, it is ignored. */
  Element& stamp(geometry::mesh::Mesh s);
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
  /** THE EMITTER'S STRENGTH, as a lane: it scales what `light()`
   *  declared, so binding it dims and lifts a lamp without describing a
   *  new one. A node with no emitter ignores it. */
  Element& intensity(motion::Animatable<float> v);
  /** …and its COLOUR, one lane per channel, on the same terms. The
   *  emitter's own colour stands on every channel the tree leaves out.
   *  On a node carrying an environment map these are its tint. */
  Element& emission(motion::Animatable<float> red,
                    motion::Animatable<float> green,
                    motion::Animatable<float> blue);

  /** THE ENVIRONMENT MAP THIS SET STANDS IN: the panorama every lit body
   *  samples by its normal for what falls on it from all around, and by
   *  its reflected view vector for what it mirrors. The node's transform
   *  ORIENTS it, the way a dome light is placed in every authoring tool,
   *  so `rotateY()` turns the sky.
   *
   *  A frame holds ONE. A second one described is a warning naming both
   *  keys, and the first in tree order is the one that shades — a silent
   *  no-op would be a set lit by whichever node happened to come last. */
  Element& environmentMap(Environment e);
  /** How much of the map reaches a surface as the light falling on it
   *  from everywhere, and how much of it a surface mirrors. Pushing one
   *  and not the other is a look, not a physical claim. */
  Element& diffuse(motion::Animatable<float> v);
  Element& specular(motion::Animatable<float> v);
  /** Added to every surface's roughness before it picks a prefiltered
   *  level, so a whole set softens without a material being edited. */
  Element& roughnessBias(motion::Animatable<float> v);
  /** Between the map and the second one: 0 is all of the first, 1 all of
   *  the second. Both are sampled and mixed, which is what lets a sky
   *  change while the frame is running. */
  Element& crossfade(motion::Animatable<float> v);
  /** THE EXPOSURE THE SET IS READ AT: what every radiance is multiplied
   *  by before the tone curve compresses it onto what a display can
   *  hold. Doubling it is one stop. It is the one dial here that means
   *  something in a set carrying no panorama at all, because a lit sum
   *  ends at the curve either way. */
  Element& exposure(motion::Animatable<float> v);
  /** THE SKY SHOWN behind the set, at this strength — zero draws none of
   *  it, so the dial is also the switch — blurred by @p blur in the same
   *  roughness units a reflection reads. */
  Element& backdrop(motion::Animatable<float> intensity);
  Element& backdropBlur(motion::Animatable<float> v);

  /** A viewpoint standing where this node stands, on the same terms as
   *  `light()`: the camera's eye and target are carried by the node's
   *  transform. */
  Element& camera(geometry::mesh::camera::Camera c);

  // ---- caching and transitions ----
  /** What the author asked of this node's cache. */
  Element& cache(core::Cache c);
  /** The node's default transition, for the plain constants on it. */
  Element& transition(const motion::Transition& t);
  /** CASCADES THE ENTRANCES of this node's children as they mount, on the
   *  schedule SigilMotion speaks — an even ladder, a fixed total divided
   *  across however many children there are, a cue table, an origin and a
   *  distribution curve. The delay compounds down the subtree, so a
   *  grandchild enters after its parent did. Only children that actually
   *  mount are delayed. */
  Element& staggerChildren(motion::Spread spread);

  // ---- integer-literal sugar --------------------------------------------
  // `rotateY(-8)` — an int does not convert into the Animatable variant on
  // its own and the resulting error is unreadable. Constrained to
  // std::integral so a float call can never land here: a plain int
  // overload would capture one through the standard conversion and recurse.
  // Each forwards to the lane setter of the same name and says nothing the
  // setter does not, so they stay out of the generated reference.
  /// @cond
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
  template <std::integral T>
  Element& intensity(T v) {
    return intensity(motion::Animatable<float>((float)v));
  }
  /// @endcond

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
