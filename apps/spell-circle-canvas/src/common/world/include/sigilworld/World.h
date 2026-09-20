#pragma once

/** @file
 * Every public header of SigilWorld in one include, for a consumer that
 * takes the whole library rather than a feature of it. Narrowing to the
 * feature headers actually used is always available.
 */

/** @defgroup world-element The element model
 *  One node of a 3D scene as a value: where it stands, what it is made
 *  of, the body it carries, and the children under it.
 *  @{ */
/** @} */

/** @defgroup world-scene Scenes
 *  The retained tree a described element tree is reconciled into, and
 *  what it cost to keep.
 *  @{ */
/** @} */

/** @defgroup world-frame Frames and passes
 *  What one frame draws: the passes, what each reads and writes, the
 *  render targets they run against, and the runtime behind them.
 *  @{ */
/** @} */

/** @defgroup world-graph The plan
 *  The passes ordered, their resources placed and their barriers
 *  worked out before anything is recorded.
 *  @{ */
/** @} */

/** @defgroup world-light Lights
 *  The emitters a set is lit by.
 *  @{ */
/** @} */

/** @defgroup world-diligent The Diligent runtime
 *  The device executor: passes recorded on Diligent Engine, and the
 *  geometry brought across to it.
 *  @{ */
/** @} */

/** @defgroup world-kit The kit
 *  Stock values over the element model, composed rather than decided.
 *  @{ */
/** @} */

#include "sigilworld/diligent/Runtime.h"
#include "sigilworld/element/Element.h"
#include "sigilworld/element/Geometry.h"
#include "sigilworld/element/Lanes.h"
#include "sigilworld/element/Node.h"
#include "sigilworld/element/Selector.h"
#include "sigilworld/element/Transform.h"
#include "sigilworld/frame/Frame.h"
#include "sigilworld/frame/Pass.h"
#include "sigilworld/frame/Runtime.h"
#include "sigilworld/frame/Targets.h"
#include "sigilworld/frame/View.h"
#include "sigilworld/graph/Plan.h"
#include "sigilworld/kit/Kit.h"
#include "sigilworld/light/Light.h"
#include "sigilworld/scene/Scene.h"
#include "sigilworld/scene/Stats.h"
