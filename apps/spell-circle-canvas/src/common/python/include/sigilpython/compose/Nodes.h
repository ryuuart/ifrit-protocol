#pragma once

/** @file
 * Binding one node's vocabulary: the verbs every kind of node states,
 * over whichever value states them, and the verbs each typed leaf adds.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>

namespace sigil::python {

/** Every verb any node may state, defined on @p element — the class of
 *  `Element` itself or of a typed leaf, whose chains then keep their
 *  own type exactly as the C++ ones do. */
template <class Node>
void bindNodeVerbs(pybind11::class_<Node>& element);

extern template void bindNodeVerbs(pybind11::class_<compose::Element>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Text>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Image>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Band>&);

/** The text leaf's own: the text properties and its content verbs. */
void bindTextVerbs(pybind11::class_<compose::Text>& element);
/** The image leaf's own: the region of its source it draws. */
void bindImageVerbs(pybind11::class_<compose::Image>& element);
/** The band's own: which side of its spine it takes. */
void bindBandVerbs(pybind11::class_<compose::Band>& element);

}  // namespace sigil::python
