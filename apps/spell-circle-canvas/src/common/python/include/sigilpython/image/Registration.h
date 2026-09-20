#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: image meaning: decoding, encoding and distance fields.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers decode options, probes, channel planes, pixmap and layer
 *  encoding, asset frames, coverage masks and distance fields on @p
 *  module. */
void bindImageMeaning(pybind11::module_& module);
/** Registers the image values on @p module. */
void bindImageValues(pybind11::module_& module);

}  // namespace sigil::python
