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
/** Registers the Brush pipeline, weaves, strands, restyle and the
 *  layered stroke stack on @p module. */
void bindComposeBrushComposites(pybind11::module_& module);
/** Registers the stamped and swept brush kinds: Scatter, Pattern,
 *  CornerArt, Ribbon, Art on @p module. */
void bindComposeBrushMarks(pybind11::module_& module);
/** Registers the composer surface: queries, dials, profiling and a
 *  standalone composer on @p module. */
void bindComposeComposer(pybind11::module_& module);
/** Registers slice, ContourWalk, Wash, Border and Skia path effects on
 *  @p module. */
void bindComposeDecorationPrimitives(pybind11::module_& module);
/** Registers decoration seam, paint context and the outline adaptors on
 *  @p module. */
void bindComposeDecorationSeam(pybind11::module_& module);
/** Registers routers, anchors, tethers, connectors, rails and bands on
 *  @p module. */
void bindComposeDerive(pybind11::module_& module);
/** Registers feed rings, options and the streaming column on @p module. */
void bindComposeFeed(pybind11::module_& module);
/** Registers instance pools, atlases, flights and the stamping leaf on
 *  @p module. */
void bindComposeInstancing(pybind11::module_& module);
/** Registers the component kit's records on @p module. */
void bindComposeKit(pybind11::module_& module);
/** Registers annotate with Beside and Anchored, trackMeter and
 *  restGhost on @p module. */
void bindComposeKitAnnotations(pybind11::module_& module);
/** Registers bevel, the era token sets, y2k chrome, aqua gel and gloss
 *  on @p module. */
void bindComposeKitEras(pybind11::module_& module);
/** Registers the fx:: kinetic presets on @p module. */
void bindComposeKitKinetic(pybind11::module_& module);
/** Registers halo, shade, scrim, emboldened, and the two ground
 *  dressings on @p module. */
void bindComposeKitLegibility(pybind11::module_& module);
/** Registers the manuscript border pieces and the gilt flourish card on
 *  @p module. */
void bindComposeKitOrnament(pybind11::module_& module);
/** Registers the aliased bitmap-font bake on @p module. */
void bindComposeKitPixelType(pybind11::module_& module);
/** Registers the connector routers and the instance placers on @p
 *  module. */
void bindComposeKitRoutes(pybind11::module_& module);
/** Registers rows, table, bars and the bordered feed plate on @p
 *  module. */
void bindComposeKitRows(pybind11::module_& module);
/** Registers palette-indexed sprites, the pixel pen and the sprite
 *  sheet on @p module. */
void bindComposeKitSprites(pybind11::module_& module);
/** Registers the line, rail, hatch and brush presets, braid and groove
 *  on @p module. */
void bindComposeKitStrokes(pybind11::module_& module);
/** Registers ruby, kenten, bullets, columns, nested runs and block
 *  rules on @p module. */
void bindComposeKitTypeset(pybind11::module_& module);
/** Registers the blurred layer-style mechanisms: inner shadow, outer
 *  glow, bevel emboss, overlay, ripple on @p module. */
void bindComposeLayerStyles(pybind11::module_& module);
/** Registers the cartography stroke: Line, Rails, Hatch, RadialHatch
 *  and the path helpers on @p module. */
void bindComposeLines(pybind11::module_& module);
/** Registers region, parts, by gates and Element.mask on @p module. */
void bindComposeMasks(pybind11::module_& module);
/** Registers video and web leaves, behind their optional targets on @p
 *  module. */
void bindComposeMediaLeaves(pybind11::module_& module);
/** Registers paintContext, keyless paint programs, keyed shapes, asset
 *  and paragraph leaves on @p module. */
void bindComposePaintPrograms(pybind11::module_& module);
/** Registers the bitmap-era mechanisms: bevel pair, brackets, tick
 *  rail, scanlines, stipple on @p module. */
void bindComposePixelStyles(pybind11::module_& module);
/** Registers pattern fills, the auto Table and Python layout schemes on
 *  @p module. */
void bindComposeSchemes(pybind11::module_& module);
/** Registers the CSS selector grammar, the rule and the selector-keyed
 *  sheet a node applies, on @p module. */
void bindComposeSelectors(pybind11::module_& module);
/** Registers one-shot measurement, run metrics, tiles and shelves on @p
 *  module. */
void bindComposeSheets(pybind11::module_& module);
/** Registers text effects, tracks, beats and text units on @p module. */
void bindComposeTextEffects(pybind11::module_& module);
/** Registers compose scenes painted into a texture on @p module. */
void bindComposeTextureScene(pybind11::module_& module);
/** Registers the document records on @p module. */
void bindDocument(pybind11::module_& module);

}  // namespace sigil::python
