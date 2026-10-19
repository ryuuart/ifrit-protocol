#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: text shaping, layout, cascade and choreography.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the type values on @p module. */
void bindWeave(pybind11::module_& module);
/** Registers overlay, merge, apply, reshapes, defaultFace and the
 *  feature presets on @p module. */
void bindWeaveCascade(pybind11::module_& module);
/** Registers the kit's shadow, glow and outline paint layers on @p
 *  module, after geometry's join they turn corners by. */
void bindWeaveKitPaintLayers(pybind11::module_& module);
/** Registers fontHandle, the host's borrowed FontContext, TextContext
 *  and the label kit on @p module. */
void bindWeaveFonts(pybind11::module_& module);
/** Registers paragraph layout on @p module. */
void bindWeaveLayout(pybind11::module_& module);
/** Registers selector state and the Unicode leaf on @p module. */
void bindWeaveSelectorUnicode(pybind11::module_& module);
/** Registers hyphenator subclassing, HyphenationOptions.patterns and
 *  the line tables on @p module. */
void bindWeaveTables(pybind11::module_& module);

}  // namespace sigil::python
