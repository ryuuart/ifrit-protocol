#pragma once

/** @file
 * @ingroup compose-core
 *
 * The compositing lanes: how opaque the node is, how its paint meets
 * what is beneath it, and the two filters over a node's layer.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilmaterial/core/BlendMode.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmotion/values/Animatable.h>

namespace sigil::compose {

/** HOW THE NODE'S LAYER MEETS THE PAGE. Each of these makes the node
 *  composite as a GROUP — its subtree resolves into one layer before
 *  the lane is applied — which is what fades a card as one picture and
 *  what ends a `preserve3d()` space at the node. */
template <class Derived>
class EffectVerbs {
 public:
  /** HOW OPAQUE THE WHOLE NODE IS, 0 clear to 1 solid, multiplying
   *  everything it and its children paint. 1 when unstated. */
  Derived& opacity(motion::Animatable<float> o);
  /** HOW THE NODE'S PAINT COMBINES with what is already beneath it —
   *  CSS `mix-blend-mode`, over any of the material's blend modes.
   *  `BlendMode::Normal` when unstated. */
  Derived& blendMode(material::BlendMode mode);
  /** POST-PROCESSES THIS NODE'S RENDERED LAYER, its subtree included —
   *  CSS `filter`. None when unstated. Baked once under
   *  `Cache::Texture`. */
  Derived& filter(material::Filter e);
  /** Filters what is already painted beneath this node's bounds before
   *  the node paints — CSS `backdrop-filter`. Incompatible with
   *  `Cache::Texture`, since the backdrop depends on the live
   *  destination; such nodes fall back to picture caching. */
  Derived& backdropFilter(material::Filter e);

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
