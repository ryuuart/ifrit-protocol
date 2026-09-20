#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: USD stages written and read.
 * This library is optional: the registration exists only where
 * SIGIL_PYTHON_HAS_USD is defined.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers sigilUSD writer and readers over the values a scene is
 *  made of on @p module. */
void bindUsd(pybind11::module_& module);

}  // namespace sigil::python
