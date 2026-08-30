#pragma once

/** @file
 * The whole describable side of motion in one include: the animation
 * values and the shaped bindings. Transitional: a consumer that spelled
 * the headers by bare name before they moved under their features
 * includes this one and keeps compiling, then narrows to the feature
 * headers it actually uses.
 */

#include "sigilmotion/bind/Bound.h"
#include "sigilmotion/bind/BoundFloat.h"
#include "sigilmotion/bind/WiggleNoise.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Keyframes.h"
#include "sigilmotion/values/Time.h"
#include "sigilmotion/values/Transition.h"
