#pragma once

/** @file
 * Every public header of SigilWorld in one include, for a consumer that
 * takes the whole library rather than a feature of it. Narrowing to the
 * feature headers actually used is always available.
 *
 * The study harness is not here: `sigilworld/testing/Study.h` is what a
 * study is written against, not what a library consumer draws with.
 */

#include "sigilworld/diligent/Device.h"
#include "sigilworld/diligent/Pop.h"
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
