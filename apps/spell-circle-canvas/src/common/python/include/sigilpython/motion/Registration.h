#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the engine, the animatable forms, schedules and physics.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the easings, the transition, the tween and its keyframes,
 *  animate, animatable, the animatable number, colour and fill, and the
 *  arithmetic over a clock reading on @p module. */
void bindMotion(pybind11::module_& module);
/** Registers Binding, Range, Envelope and its factories, Wiggle and bind
 *  on @p module. */
void bindMotionAnimatableForms(pybind11::module_& module);
/** Registers the Engine, its options and clock policy, the playbacks it
 *  hands back and the timeline positions on @p module. */
void bindMotionClock(pybind11::module_& module);
/** Registers held-motion resolution and the lane
 *  retargets on @p module. */
void bindMotionLanes(pybind11::module_& module);
/** Registers roughly, Attribute, Particles, EmitFrom and the Emitter on
 *  @p module. */
void bindMotionParticles(pybind11::module_& module);
/** Registers vec2, Points, forces, Neighbourhood, constraints and
 *  Verlet on @p module. */
void bindMotionPhysics(pybind11::module_& module);
/** Registers Place, stagger, cues, Staggered, Timing, Schedule and Beat
 *  on @p module. */
void bindMotionSchedule(pybind11::module_& module);
/** Registers Oscillator, Wave, SpringParameters and the closed-form
 *  Spring on @p module. */
void bindMotionSignals(pybind11::module_& module);

}  // namespace sigil::python
