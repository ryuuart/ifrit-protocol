#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the sketch context, the sets and what a Python entry declares.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::sketch::python {

/** Registers the sketch context, session and file rendering on @p
 *  module. */
void bindRuntime(pybind11::module_& module);
/** Registers everything a canvas sketch is handed on @p module. */
void bindSketchContextSurface(pybind11::module_& module);
/** Registers the process painter runtime, without exposing the device
 *  on @p module. */
void bindSketchDeviceRuntimes(pybind11::module_& module);
/** Registers what a Python sketch entry is filed as and what it needs
 *  on @p module. */
void bindSketchEntries(pybind11::module_& module);
/** Registers a Python sketch's own schema, and a settled page on @p
 *  module. */
void bindSketchSchema(pybind11::module_& module);
/** Registers a Python sketch that dresses a set on @p module. */
void bindSketchSetSketches(pybind11::module_& module);
/** Registers the Python Set kind and a scene on the session ticker on
 *  @p module. */
void bindSketchWorldSet(pybind11::module_& module);

}  // namespace sigil::sketch::python
