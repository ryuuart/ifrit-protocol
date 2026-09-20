#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: 3D scenes, their passes, views and device runtime.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers 3D scenes on @p module. */
void bindWorld(pybind11::module_& module);
/** Registers cache, staggerChildren, world.memo and the node read-back
 *  on @p module. */
void bindWorldDescription(pybind11::module_& module);
/** Registers the Diligent runtime, importNative and the native texture
 *  handle on @p module. */
void bindWorldDevice(pybind11::module_& module);
/** Registers backdrop, Environment and the six sky lanes on @p module. */
void bindWorldEnvironment(pybind11::module_& module);
/** Registers the geometry slot, the curve ride and the kit rails on @p
 *  module. */
void bindWorldGeometry(pybind11::module_& module);
/** Registers computePass, the pass and frame getters, readbacks and the
 *  Runtime on @p module. */
void bindWorldPasses(pybind11::module_& module);
/** Registers the frame graph as an owned snapshot on @p module. */
void bindWorldPlan(pybind11::module_& module);
/** Registers the frame's resources and a Python pass body on @p module. */
void bindWorldTargets(pybind11::module_& module);
/** Registers draw, View, the executor readings and selector inspection
 *  on @p module. */
void bindWorldView(pybind11::module_& module);

}  // namespace sigil::python
