#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: Substance archives as texture sets.
 * This library is optional: the registration exists only where
 * SIGIL_PYTHON_HAS_SUBSTANCE is defined.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers sigilSubstance packages, graphs, parameters and outputs on
 *  @p module. */
void bindSubstance(pybind11::module_& module);

}  // namespace sigil::python
