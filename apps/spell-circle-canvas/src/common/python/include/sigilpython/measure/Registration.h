#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: timing, statistics and check reporting.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers sigilMeasure: checks, tables, line fits, statistics and
 *  timing on @p module. */
void bindMeasure(pybind11::module_& module);

}  // namespace sigil::python
