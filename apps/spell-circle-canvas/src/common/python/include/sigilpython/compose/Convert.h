#pragma once

/** @file
 * Binding the scene description: the element tree, and the readings
 * that take a dimension, a fill, a paint, an alignment, a shape, a
 * decoration or a custom property's value from the shapes Python spells
 * them as.
 */

#include <glm/vec2.hpp>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>

#include <optional>
#include <vector>

namespace sigil::python {
/** A length read from @p value: a number is pixels, and a string is
 *  "auto", a percentage, or a number with its unit after it. */
compose::Dimension dimension(pybind11::handle value);
/** A length in pixels read from @p value, for a slot nothing resolves a
 *  relative length in — a shadow's offset and blur, a stroke's width: a
 *  number is pixels, and text or a typed length is read by the one length
 *  grammar and taken where its unit is absolute (`"4px"`, `"3pt"`). A
 *  length against the font, the canvas or the box raises, naming
 *  @p slot. */
float pixels(pybind11::handle value, const char* slot);
/** A fraction read from @p value — CSS's number or percentage where the
 *  two are one amount: a number is itself, and `"50%"` or
 *  `compose.percent(50)` is 0.5. Anything else raises, naming @p slot. */
float fraction(pybind11::handle value, const char* slot);
/** An offset in pixels read from @p value: a pair whose two entries are
 *  each read as `pixels` reads one — `(0, 4)`, `("0px", "4px")` — or a
 *  point. Anything else raises, naming @p slot. */
glm::vec2 pixelOffset(pybind11::handle value, const char* slot);
/** A fill read from @p value: None is no fill, a paint or a material is
 *  that paint, a token is the variable it names, the text
 *  `currentcolor` is the ink in force and `var(--name)` the custom
 *  property, and anything else is a colour. */
compose::Fill fill(pybind11::handle value);
/** The custom property CSS text @p value names, `var(--name)` or
 *  `var(name)`, or nothing where @p value is not that text. */
std::optional<compose::VarRef> customPropertyText(pybind11::handle value);
/** A colour term read from what a fill takes: a colour, the ink in force
 *  (`Fill.currentInk()`) or a custom property — never a paint, which
 *  raises. */
compose::ColorTerm colorTerm(pybind11::handle value);
/** A cross-axis alignment read from @p value's name. */
compose::Align alignment(pybind11::handle value);
/** A main-axis justification read from @p value's name. */
compose::Justify justification(pybind11::handle value);
/** Whether content paints past the shape, read from @p value's name. */
compose::Overflow overflowing(pybind11::handle value);
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
/** What a custom property holds, read from @p value: a colour, a paint
 *  (a material), or a length. Text is told apart by which grammar reads
 *  it: what the length grammar reads is a length, what the colour
 *  grammar reads is a colour, and anything else raises. */
compose::VarValue variable(pybind11::handle value);
/** Whether @p value is a node: an element, or one of the typed leaves a
 *  factory hands back. Each leaf is a class of its own in Python rather
 *  than a subclass of Element, so asking after Element alone answers no
 *  for a text, an image or a band. */
bool isNode(pybind11::handle value);
/** The element @p value describes, whatever kind of node it is. Call it
 *  only where isNode answers yes; anything else raises. */
compose::Element node(pybind11::handle value);
/** The children in @p children as a list of elements, flattening a
 *  sequence passed as one argument. A typed leaf counts as one child. */
std::vector<compose::Element> elements(pybind11::args children);
}  // namespace sigil::python
