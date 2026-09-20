#pragma once

/** @file
 * Binding text shaping and layout: the type values and the
 * paragraph layout over them.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {
/** Registers the type values on @p module. */
void bindWeave(pybind11::module_& module);
/** Registers paragraph layout on @p module. */
void bindWeaveLayout(pybind11::module_& module);
}  // namespace sigil::python
