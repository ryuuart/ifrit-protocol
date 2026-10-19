#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the immediate-mode pen and the canvas it draws on.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the brush engine on @p module. */
void bindBrush(pybind11::module_& module);
/** Registers the borrowed canvas that lets a binding in another file
 *  draw on @p module. */
void bindDrawCanvasSeam(pybind11::module_& module);
/** Registers the immediate-mode pen on @p module. */
void bindPen(pybind11::module_& module);

}  // namespace sigil::python
