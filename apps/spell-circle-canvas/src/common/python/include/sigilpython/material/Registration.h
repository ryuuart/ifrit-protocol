#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: recipes, textures, environments, patterns and colour.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the one colour class Python sees on @p module. */
void bindColor(pybind11::module_& module);
/** Registers materials on @p module. */
void bindMaterial(pybind11::module_& module);
/** Registers the value model: Material, Recipe authoring, Schema,
 *  UniformBlock, Bank, over and masking stacks on @p module. */
void bindMaterialCore(pybind11::module_& module);
/** Registers the equirectangular panorama, the two named skies, and
 *  gold, chrome and glass over a normal map on @p module. */
void bindMaterialEnvironment(pybind11::module_& module);
/** Registers the field parameter structs and recipe accessors, plus the
 *  CRT screen and its kit preset on @p module. */
void bindMaterialField(pybind11::module_& module);
/** Registers stone, timber, latten and board, the orthographic globe,
 *  and the girih panel on @p module. */
void bindMaterialKitGrained(pybind11::module_& module);
/** Registers the six animated text paints, the chrome-type ramps, the
 *  named colormaps and the gel tables on @p module. */
void bindMaterialKitText(pybind11::module_& module);
/** Registers the revisioned pixel buffer, raw shader interop, bound
 *  pans, live paint uniforms and Effect::filter on @p module. */
void bindMaterialPaintEffect(pybind11::module_& module);
/** Registers tile programs and mapping, the sequence and speckle
 *  generators, and the woven cloth on @p module. */
void bindMaterialPattern(pybind11::module_& module);
/** Registers signed-distance shapes and styles, OpenColorIO view
 *  transforms, and the Slang compiler on @p module. */
void bindMaterialShading(pybind11::module_& module);
/** Registers skia::fill and shader, the two ramp crossings, the palette
 *  images and the stock warm-up on @p module. */
void bindMaterialSkiaDraw(pybind11::module_& module);
/** Registers textures, their three sources, leaves in material slots,
 *  bevel normals and masks on @p module. */
void bindMaterialTexture(pybind11::module_& module);
/** Registers the authoring tools' texture folders by role, and the
 *  sprite atlas on @p module. */
void bindMaterialTextureSets(pybind11::module_& module);

}  // namespace sigil::python
