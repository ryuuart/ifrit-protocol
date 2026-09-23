#pragma once

/** @file
 * Binding one node's vocabulary: the verbs every kind of node states,
 * over whichever value states them, and the verbs each typed leaf adds.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/StyleSheet.h>

namespace sigil::python {

/** Every verb a rule states as well as a node — the box, the flex line,
 *  the placement, the silhouette's corners and overflow, the paint, the
 *  compositing lanes, the 2D transform and the cascade — defined on
 *  @p element, which is a node's class or `compose.Rule`'s. One binding
 *  of each verb, so the two cannot drift apart. */
template <class Declaring>
void bindDeclarationVerbs(pybind11::class_<Declaring>& element);

extern template void bindDeclarationVerbs(pybind11::class_<compose::Element>&);
extern template void bindDeclarationVerbs(pybind11::class_<compose::Text>&);
extern template void bindDeclarationVerbs(pybind11::class_<compose::Image>&);
extern template void bindDeclarationVerbs(pybind11::class_<compose::Band>&);
extern template void bindDeclarationVerbs(pybind11::class_<compose::Rule>&);

/** Every verb any node may state, defined on @p element — the class of
 *  `Element` itself or of a typed leaf, whose chains then keep their
 *  own type exactly as the C++ ones do: the declarations above, and what
 *  only a node says — its structure, identity, decorations, filters,
 *  depth and callbacks. */
template <class Node>
void bindNodeVerbs(pybind11::class_<Node>& element);

extern template void bindNodeVerbs(pybind11::class_<compose::Element>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Text>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Image>&);
extern template void bindNodeVerbs(pybind11::class_<compose::Band>&);

/** The text properties — how a passage is set — which a text leaf and a
 *  rule state and an element cannot, defined on @p element. */
template <class Declaring>
void bindTextPropertyVerbs(pybind11::class_<Declaring>& element);

extern template void bindTextPropertyVerbs(pybind11::class_<compose::Text>&);
extern template void bindTextPropertyVerbs(pybind11::class_<compose::Rule>&);

/** The text leaf's own: the text properties and its content verbs. */
void bindTextVerbs(pybind11::class_<compose::Text>& element);
/** The image leaf's own: the region of its source it draws. */
void bindImageVerbs(pybind11::class_<compose::Image>& element);
/** The band's own: which side of its spine it takes. */
void bindBandVerbs(pybind11::class_<compose::Band>& element);

}  // namespace sigil::python
