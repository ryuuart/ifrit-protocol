#pragma once

/** @file
 * @ingroup compose-brush
 *
 * THE RENDERER'S OWN PATH EFFECT on a stroke — the escape from
 * `PathFormat`'s dash and stamp to anything the renderer ships (corner
 * rounding, a discrete jitter, a 1D or 2D path effect, a chain of them).
 * Included by name: no default Compose header reaches the renderer's
 * types.
 *
 *     PathFormat format = stroke(2, ink);
 *     format.effect = std::make_shared<const PathEffect>(
 *         PathEffect{SkCornerPathEffect::Make(6)});
 */

#include <include/core/SkPathEffect.h>
#include <sigilcompose/brush/Decorations.h>

#include <memory>

namespace sigil::compose {

/** The effect a `PathFormat` strokes through. Compared by identity, as the
 *  renderer's effect is: the same effect held again is the same format. */
struct PathEffect {
  sk_sp<SkPathEffect> skia;
};

}  // namespace sigil::compose
