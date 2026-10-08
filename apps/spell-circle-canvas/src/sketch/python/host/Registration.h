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
/** Registers the process painter runtime, without exposing the device
 *  on @p module. */
void bindSketchDeviceRuntimes(pybind11::module_& module);
/** Registers what a Python sketch entry is filed as and what it needs
 *  on @p module. */
void bindSketchEntries(pybind11::module_& module);
/** Registers the registry's rows and a catalog of sketch files on @p
 *  module. */
void bindSketchCatalog(pybind11::module_& module);
/** Registers the comparison of two directories of plates on @p module. */
void bindSketchPlates(pybind11::module_& module);
/** Registers a Python sketch's own schema, and a settled page on @p
 *  module. */
void bindSketchSchema(pybind11::module_& module);
/** Registers a page a baked 3D frame lights on @p module. */
void bindSketchPage(pybind11::module_& module);
/** Registers a Python scene3d sketch on @p module. */
void bindSketchScene3DSketches(pybind11::module_& module);
/** Registers the Python scene3d kind and a scene on the session engine on
 *  @p module. */
void bindSketchWorldScene3D(pybind11::module_& module);
/** Registers a sketch host in this process, spoken to through the
 *  protocol, on @p module. */
void bindSketchTesting(pybind11::module_& module);

}  // namespace sigil::sketch::python
