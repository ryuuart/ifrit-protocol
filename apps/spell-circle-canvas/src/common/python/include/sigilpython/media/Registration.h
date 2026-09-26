#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: pictures, still and moving.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the image document, its frames, the one format list, the
 *  decode and encode doors and the pixel difference on @p module. */
void bindMediaImages(pybind11::module_& module);
/** Registers the video, its decode pool and the movie encoder on
 *  @p module. */
void bindMediaVideo(pybind11::module_& module);

}  // namespace sigil::python
