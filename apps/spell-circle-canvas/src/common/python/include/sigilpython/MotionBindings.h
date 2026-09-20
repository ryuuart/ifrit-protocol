#pragma once

/** @file
 * Binding animation: the clock and the schedules, and the readings
 * that take an animatable number, colour, fill, easing or transition
 * from the shapes Python spells them as.
 */

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animatable.h>

namespace sigil::python {

/** Registers animation on @p module. */
void bindMotion(pybind11::module_& module);
/** An animatable number read from @p value: an output, a binding or a
 *  transitioned value, or a plain number that stands still. */
motion::Animatable<float> motionAnimatable(pybind11::handle value);
/** An animatable colour read from @p value, on the same terms. */
motion::Animatable<SkColor4f> motionInk(pybind11::handle value);
/** An animatable fill read from @p value, on the same terms. */
motion::Animatable<compose::Fill> motionFill(pybind11::handle value);
/** An easing read from @p value: a named curve, a curve value, or a
 *  callable that shapes a fraction. None is the default ease-out. */
choreograph::EaseFn motionEase(pybind11::handle value);
/** A transition read from @p value; None is the default transition. */
motion::Transition motionTransition(pybind11::handle value);

}  // namespace sigil::python
