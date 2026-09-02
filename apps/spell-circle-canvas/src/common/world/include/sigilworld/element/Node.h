#pragma once

/** @file
 * The payload an Element carries: every field of one node's description,
 * and the comparison that decides whether two of them are provably the
 * same node described twice.
 *
 * This is a VALUE, and it is public because reading a description is
 * what a reconcile host does. Nothing here is retained: an entity, a
 * running motion and a cooked resource all live on the other side of the
 * seam, in the Scene.
 */

#include <sigilcore/cache/Policy.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Transition.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Geometry.h>
#include <sigilworld/element/Transform.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::world {

/** A node's ride along a curve: the spline, and how far along it the
 *  node stands. */
struct Along {
  Spline3 spline;
  motion::Animatable<float> distance{0.0f};
};

/** A window into a loop: the leading edge and the length trailing it,
 *  both in loop parameter. */
struct Window {
  motion::Animatable<float> head{1.0f};
  motion::Animatable<float> span{1.0f};
};

/** AN EMITTER'S LIVE DIALS: the strength it shines at and the three
 *  channels of the colour it shines in, each present only when the tree
 *  said something about it. A dial a description does not carry leaves
 *  the emitter's own field standing — which is why each is optional and
 *  why the emitter stays a plain comparable value with no animation in
 *  it. */
struct Emission {
  std::optional<motion::Animatable<float>> intensity;
  std::optional<motion::Animatable<float>> red;
  std::optional<motion::Animatable<float>> green;
  std::optional<motion::Animatable<float>> blue;
};

/** AN ENVIRONMENT MAP'S LIVE DIALS, on the same terms as an emitter's:
 *  each present only where the tree said something about it, so a dial
 *  the description drops leaves the environment's own field standing.
 *  Its strength and its tint are the emitter rows above, because a
 *  panorama placed in a set is an emitter of a kind and a node carries
 *  one or the other. */
struct SkyDials {
  std::optional<motion::Animatable<float>> diffuse;
  std::optional<motion::Animatable<float>> specular;
  std::optional<motion::Animatable<float>> roughnessBias;
  std::optional<motion::Animatable<float>> crossfade;
  std::optional<motion::Animatable<float>> backdrop;
  std::optional<motion::Animatable<float>> backdropBlur;
};

/** ONE NODE'S DESCRIPTION, field by field. */
struct ElementNode {
  /** What children are matched on; empty means positional. */
  std::string key;
  Transform transform;
  std::optional<Along> along;
  std::optional<Window> window;
  Geometry geometry;
  /** The one surface. */
  std::optional<::sigil::material::Material> material;
  /** …or the per-face slots, in slot order. A node carries one form or
   *  the other, never both. */
  std::vector<::sigil::material::Material> slots;
  std::vector<std::string> tags;
  std::optional<Light> light;
  /** …and the dials on it, when the tree put any there. */
  std::optional<Emission> emission;
  /** The panorama this node places, and its own dials. */
  std::optional<Environment> environment;
  std::optional<SkyDials> sky;
  std::optional<Camera> camera;
  core::Cache cachePolicy = core::Cache::Auto;
  std::optional<motion::Transition> nodeTransition;
  /** THE CASCADE OVER THIS NODE'S CHILDREN AS THEY MOUNT: each child's
   *  entrance is delayed by the start time the schedule gives its
   *  ordinal, and that delay compounds down the subtree, so a set that
   *  arrives arrives in an order rather than all at once.
   *
   *  It delays only children that actually MOUNT. The first describe
   *  cascades the whole list; a child appended to a live list is the only
   *  new mount in its patch and enters at once, and children already
   *  standing never re-enter. */
  std::optional<motion::Spread> childStagger;
  std::vector<Element> children;
  std::optional<Memo> memo;
};

/** THE STRUCTURAL PRUNE: are @p a and @p b provably the same node
 *  described twice?
 *
 *  Every field of ElementNode is ruled on, and anything that cannot be
 *  compared answers false — a field left out does not produce a wrong
 *  answer where the mistake is, it produces a node that never patches on
 *  that field again.
 *
 *  Two fields are deliberately excluded and both are compared elsewhere:
 *  `memo` is compared earlier and more strictly by the reconciler (the
 *  captured environment, then the author's own props comparison), and
 *  `children` are reconciled by key rather than compared. */
bool propsEqual(const ElementNode& a, const ElementNode& b);

}  // namespace sigil::world
