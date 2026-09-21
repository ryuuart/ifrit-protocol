#pragma once

/** @file
 * Binding the scene description: the element tree, and the readings
 * that take a dimension, a fill, a paint, an alignment, a shape, a
 * decoration or a custom property's value from the shapes Python spells
 * them as.
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
/** A decoration read from @p value: a decoration, a path format or a
 *  shadow. Anything else raises. */
compose::Decoration decoration(pybind11::handle value);
/** One length of a transform or perspective origin. A bare number is
 *  refused as the native verb refuses it: it reads as a fraction of the
 *  box as readily as a pixel count. */
compose::Dimension originLength(pybind11::handle value);
/** What a custom property holds, read from @p value: a colour, or a
 *  length. */
compose::VarValue variable(pybind11::handle value);
/** The children in @p children as a list of elements, flattening a
 *  sequence passed as one argument. A typed leaf counts as one child. */
std::vector<compose::Element> elements(pybind11::args children);
}  // namespace sigil::python
