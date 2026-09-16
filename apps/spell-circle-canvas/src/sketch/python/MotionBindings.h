#pragma once

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animatable.h>

namespace sigil::sketch::python {

void bindMotion(pybind11::module_& module);
motion::Animatable<float> motionAnimatable(pybind11::handle value);
motion::Animatable<SkColor4f> motionInk(pybind11::handle value);
motion::Animatable<compose::Fill> motionFill(pybind11::handle value);
choreograph::EaseFn motionEase(pybind11::handle value);
motion::Transition motionTransition(pybind11::handle value);

}  // namespace sigil::sketch::python
