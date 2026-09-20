#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: a headless web engine's pages as images.
 * This library is optional: the registration exists only where
 * SIGIL_PYTHON_HAS_SCRY is defined.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers sigilScry engine, views, image slots, the Compose web leaf
 *  and the sketch settling seam on @p module. */
void bindScry(pybind11::module_& module);

}  // namespace sigil::python
