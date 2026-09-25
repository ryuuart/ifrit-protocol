#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: resource access, feeds and publication.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers resource access on @p module. */
void bindIO(pybind11::module_& module);
/** Adds the io.frames submodule to @p module; it registers no names
 *  yet, so publishers and subscriptions are reachable only from C++. */
void bindIOFrames(pybind11::module_& module);

}  // namespace sigil::python
