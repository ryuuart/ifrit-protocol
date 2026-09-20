#pragma once

/** @file
 * Binding the immediate-mode pen, whose verbs draw through the pen
 * lent to a callback.
 */

#include <sigilpython/Bindings.h>

namespace sigil::python {
/** Registers the immediate-mode pen on @p module. */
void bindPen(pybind11::module_& module);
}  // namespace sigil::python
