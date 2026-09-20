#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: streaming decode, playback and encode.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers sigilVideo decode, playback, encode and the Compose video
 *  leaf on @p module. */
void bindVideo(pybind11::module_& module);

}  // namespace sigil::python
