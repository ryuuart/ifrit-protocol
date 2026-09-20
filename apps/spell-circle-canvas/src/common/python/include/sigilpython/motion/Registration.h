#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the clock, the animatable forms, schedules and physics.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers animation on @p module. */
void bindMotion(pybind11::module_& module);
/** Registers the four Animatable forms, Envelope and the BoundFloat
 *  record on @p module. */
void bindMotionAnimatableForms(pybind11::module_& module);
/** Registers frameClock, a constructible Ticker and the choreograph
 *  timeline on @p module. */
void bindMotionClock(pybind11::module_& module);
/** Registers animatedFloat, held-motion resolution and the lane
 *  retargets on @p module. */
void bindMotionLanes(pybind11::module_& module);
/** Registers roughly, Attribute, Particles, EmitFrom and the Emitter on
 *  @p module. */
void bindMotionParticles(pybind11::module_& module);
/** Registers vec2, Points, forces, Neighbourhood, constraints and
 *  Verlet on @p module. */
void bindMotionPhysics(pybind11::module_& module);
/** Registers spread, cascadeOrder, cascadeRanks, Cascade and Beat on @p
 *  module. */
void bindMotionSchedule(pybind11::module_& module);
/** Registers oscillator, Wave, Sequence and the closed-form Spring on
 *  @p module. */
void bindMotionSignals(pybind11::module_& module);

}  // namespace sigil::python
