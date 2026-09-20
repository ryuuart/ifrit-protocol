#pragma once

/** @file
 * Binding the scene description: the element tree, and the readings
 * that take a dimension, a fill, a paint, an alignment or a shape
 * from the shapes Python spells them as.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>

#include <vector>

namespace sigil::python {
/** A length read from @p value: a number is pixels, and a string is
 *  "auto", a percentage, or a number with its unit after it. */
compose::Dimension dimension(pybind11::handle value);
/** A fill read from @p value: None is no fill, a shader is a shader, a
 *  token is the variable it names, and anything else is a colour. */
compose::Fill fill(pybind11::handle value);
/** What a node's surface is painted with, read from @p value: a fill,
 *  an animatable one, or a material paint. */
compose::SurfacePaint surfacePaint(pybind11::handle value);
/** A cross-axis alignment read from @p value's name. */
compose::Align alignment(pybind11::handle value);
/** A main-axis justification read from @p value's name. */
compose::Justify justification(pybind11::handle value);
/** A node's outline read from @p value: a shape, or a callable that
 *  builds one for the size it is given. */
compose::Shape shape(pybind11::handle value);
/** The children in @p children as a list of elements, flattening a
 *  sequence passed as one argument. */
std::vector<compose::Element> elements(pybind11::args children);
/** Registers the scene description on @p module. */
void bindCompose(pybind11::module_& module);
}  // namespace sigil::python
