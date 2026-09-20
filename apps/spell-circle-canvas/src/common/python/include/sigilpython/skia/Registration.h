#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the Skia values, paints, paths, surfaces and faces.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the one colour class Python sees on @p module. */
void bindColor(pybind11::module_& module);
/** Registers the kernel values every binding is written in terms of on
 *  @p module. */
void bindCore(pybind11::module_& module);
/** Registers shaders, gradients, colour and image filters, mask
 *  filters, path effects, blenders on @p module. */
void bindSkiaEffects(pybind11::module_& module);
/** Registers skFontStyle, typeface style and variation axes, SkFont,
 *  SkTextBlob on @p module. */
void bindSkiaFonts(pybind11::module_& module);
/** Registers skRRect, SkM44, the rest of SkMatrix, arcTo, path queries,
 *  SVG strings, Simplify, FillPathWithPaint, contour measures on @p
 *  module. */
void bindSkiaPaths(pybind11::module_& module);
/** Registers owned raster surfaces, the canvas seam, image info and
 *  pixmaps, the missing canvas verbs, SigilSkia direct draws and pixel
 *  helpers on @p module. */
void bindSkiaSurfaces(pybind11::module_& module);
/** Registers the shared drawing values on @p module. */
void bindValues(pybind11::module_& module);

}  // namespace sigil::python
