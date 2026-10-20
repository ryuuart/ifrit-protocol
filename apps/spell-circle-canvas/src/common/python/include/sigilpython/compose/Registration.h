#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the element tree, its decorations and the component kit.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the scene description on @p module. */
void bindCompose(pybind11::module_& module);
/** Registers the stock adding operators on @p module: the connecting
 *  wires, pins, outline bands and hulls, the stamp whose maker is a
 *  Python callable, and drawWith. */
void bindComposeAdders(pybind11::module_& module);
/** Registers the arrangement an arranging operator is handed, and its
 *  children, on @p module. */
void bindComposeArrangement(pybind11::module_& module);
/** Registers the stamped and swept brush kinds: Scatter, Pattern,
 *  CornerArt, Ribbon, Art on @p module. */
void bindComposeBrushMarks(pybind11::module_& module);
/** Registers the composer surface: queries, dials, profiling and a
 *  standalone composer on @p module. */
void bindComposeComposer(pybind11::module_& module);
/** Registers the four sides around a node — padding, margin and inset in
 *  every arity, and the eight per-side verbs — on @p module. */
void bindComposeElementEdges(pybind11::module_& module);
/** Registers routers, anchors, tethers and bands on @p module. */
void bindComposeDerive(pybind11::module_& module);
/** Registers the facts a node states about itself on @p module. */
void bindComposeFacts(pybind11::module_& module);
/** Registers feed rings, options and the streaming column on @p module. */
void bindComposeFeed(pybind11::module_& module);
/** Registers the cell sheet, the stamping leaf and the pick on @p
 *  module. */
void bindComposeInstancing(pybind11::module_& module);
/** Registers the lanes an instance pool is filled through on @p module. */
void bindComposeInstancingLanes(pybind11::module_& module);
/** Registers the instance pool, its flights and its lane accessors on @p
 *  module. */
void bindComposeInstancingPool(pybind11::module_& module);
/** Registers the component kit's records on @p module. */
void bindComposeKit(pybind11::module_& module);
/** Registers the reading, the readout, the table and the bars on @p
 *  module. */
void bindComposeKitRows(pybind11::module_& module);
/** Registers the text paints — their fields, their record and the
 *  material each describes — on @p module. */
void bindComposeKitTextPaint(pybind11::module_& module);
/** Registers region, parts, by gates and Element.mask on @p module. */
void bindComposeMasks(pybind11::module_& module);
/** Registers paintContext, keyless paint programs, keyed shapes, asset
 *  and paragraph leaves on @p module. */
void bindComposePaintPrograms(pybind11::module_& module);
/** Registers the operator a node runs over its children on @p module. */
void bindComposeSchemes(pybind11::module_& module);
/** Registers the settled scope an adding operator is handed, and its
 *  nodes, on @p module. */
void bindComposeScope(pybind11::module_& module);
/** Registers the CSS selector grammar, the rule and the selector-keyed
 *  sheet a node applies, on @p module. */
void bindComposeSelectors(pybind11::module_& module);
/** Registers the material keyline's options and its options form on @p
 *  module. */
void bindComposeStrokeOptions(pybind11::module_& module);
/** Registers text effects, tracks, beats and text units on @p module. */
void bindComposeTextEffects(pybind11::module_& module);
/** Registers compose scenes painted into a texture on @p module. */
void bindComposeTextureScene(pybind11::module_& module);
/** Registers the document records on @p module. */
void bindDocument(pybind11::module_& module);

}  // namespace sigil::python
