#pragma once

/** @file
 * Binding materials: the reading that takes a colour from the shapes
 * Python spells one as, for every material parameter declared as the
 * material colour itself rather than as Skia's.
 */

#include <pybind11/pybind11.h>
#include <sigilmaterial/color/Color.h>

namespace sigil::python {

/** A material colour read from @p value: the colour class, a CSS
 *  string, or a sequence of three or four channels between zero and
 *  one. Requires the interpreter lock. */
material::Color materialColor(pybind11::handle value);

}  // namespace sigil::python
