#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: tables, schemas and codecs, and connections.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers tabular data on @p module. */
void bindData(pybind11::module_& module);
/** Registers data.connect and data.replay, the Connection they answer,
 *  its Message and its ConnectionState on @p module. */
void bindDataConnection(pybind11::module_& module);

}  // namespace sigil::python
