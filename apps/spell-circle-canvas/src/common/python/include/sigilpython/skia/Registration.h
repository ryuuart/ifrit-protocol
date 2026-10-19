#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the Skia values, paints, paths, surfaces and faces.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the shared drawing values on @p module. */
void bindValues(pybind11::module_& module);

}  // namespace sigil::python
