#pragma once

/** @file
 * Binding materials: the reading that takes a colour from the shapes
 * Python spells one as, for every material parameter declared as the
 * material colour itself rather than as Skia's.
 */

#include <pybind11/pybind11.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>

#include <string_view>

namespace sigil::python {

/** A material colour read from @p value: the colour class, a CSS
 *  string, or a sequence of three or four channels between zero and
 *  one. Requires the interpreter lock. */
material::Color materialColor(pybind11::handle value);

/** Whether @p text is written as one of CSS's gradient functions —
 *  `linear-gradient(…)`, `repeating-conic-gradient(…)` — which a fill
 *  reads as a gradient rather than as a colour. */
bool isGradientText(std::string_view text);

/** The gradient CSS text writes, as `material::readGradient` reads it;
 *  text it cannot read raises ValueError naming the text and the forms. */
material::Material gradientFromText(std::string_view text);

/** The filter CSS text writes, as `material::readFilter` reads it; text it
 *  cannot read raises ValueError naming the text and the forms. */
material::Filter filterFromText(std::string_view text);

}  // namespace sigil::python
