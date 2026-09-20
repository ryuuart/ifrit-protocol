#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: tables, schemas and connections.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers tabular data on @p module. */
void bindData(pybind11::module_& module);
/** Registers data::Connection: handlers, schema doors, replies and the
 *  checked session wrapper on @p module. */
void bindDataConnection(pybind11::module_& module);
/** Registers a schema-driven FlatBuffer value, its hub decoder, and the
 *  .bfbs a Python sketch can read on @p module. */
void bindDataSchema(pybind11::module_& module);
/** Registers typed column spans, Flag/Instant comparisons, deepcopy,
 *  and four FINDINGS data defects on @p module. */
void bindDataTables(pybind11::module_& module);

}  // namespace sigil::python
