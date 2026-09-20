#pragma once

/** @file
 * The whole describable side of motion in one include: the animation
 * values and the shaped bindings. A consumer of one feature includes
 * that feature's headers instead.
 */

/** @defgroup motion-clock The clock
 *  Wall-clock time turned into well-behaved per-frame deltas, and the
 *  ticker that steps a timeline plus the callbacks registered on it and
 *  answers whether anything is still moving (clock/FrameClock.h,
 *  clock/Ticker.h). */
/** @defgroup motion-values Animatable values
 *  How a property changes over time, as values with no renderer in them:
 *  the transition, the keyed track, the spring, the oscillator, the
 *  property that can hold any of them, and the lanes a held motion is
 *  retargeted through (values/Values.h). */
/** @defgroup motion-bind Bindings
 *  One number shaped into another: the chain builder, the evaluator it
 *  compiles to, the easing curves it is shaped by, and the wiggle noise
 *  laid over the result (bind/Bind.h). */
/** @defgroup motion-schedule Schedules
 *  How a run of units shares one progress: the spread, the orderings a
 *  cascade runs in, and a spread resolved against a frame's counts into
 *  a beat per unit (schedule/Schedule.h). */
/** @defgroup motion-physics Physics
 *  A point set that is stepped rather than read: the lanes a simulation
 *  is, the forces and constraints over them, the grid that answers what
 *  is near what, the Verlet stepper, and particles that are born, age
 *  and die (physics/Physics.h). */

#include "sigilmotion/bind/Bound.h"
#include "sigilmotion/bind/BoundFloat.h"
#include "sigilmotion/bind/WiggleNoise.h"
#include "sigilmotion/values/Animatable.h"
#include "sigilmotion/values/Keyframes.h"
#include "sigilmotion/values/Oscillator.h"
#include "sigilmotion/values/Sequence.h"
#include "sigilmotion/values/Time.h"
#include "sigilmotion/values/Transition.h"
