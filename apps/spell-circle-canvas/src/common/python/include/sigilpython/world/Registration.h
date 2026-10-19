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

}  // namespace sigil::python
