#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the sketch context, the scene3d sketches and what a Python entry
 * declares.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::sketch::python {

/** Registers the sketch context, session and file rendering on @p
 *  module. */
void bindRuntime(pybind11::module_& module);
/** Registers everything a scene2d sketch is handed on @p module. */
void bindSketchContextSurface(pybind11::module_& module);
/** Registers the registry's rows and a catalog of sketch files on @p
 *  module. */
void bindSketchCatalog(pybind11::module_& module);
/** Registers the comparison of two directories of plates on @p module. */
void bindSketchPlates(pybind11::module_& module);
/** Registers a page a baked 3D frame lights on @p module. */
void bindSketchPage(pybind11::module_& module);
/** Registers the context a Python scene3d sketch is handed at setup on
 *  @p module. */
void bindSketchScene3DSketches(pybind11::module_& module);
/** Registers a sketch host in this process, spoken to through the
 *  protocol, on @p module. */
void bindSketchTesting(pybind11::module_& module);
/** Registers a window in this interpreter, the window library loaded the
 *  first time one opens, on @p module. */
void bindSketchLive(pybind11::module_& module);

}  // namespace sigil::sketch::python
