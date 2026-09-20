#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the compute kernels and the record protocol every bound record
 * answers.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the kernel values every binding is written in terms of on
 *  @p module. */
void bindCore(pybind11::module_& module);
/** Registers distributions, shuffle, reservoir, the chance token, the
 *  noise field and mixers, the pinned hash folds, interval normal forms
 *  on @p module. */
void bindCoreCompute(pybind11::module_& module);
/** Registers the core values on @p module. */
void bindCoreValues(pybind11::module_& module);
/** Registers which optional native libraries this build carries on @p
 *  module. */
void bindOptionalLibraries(pybind11::module_& module);
/** Registers the copy protocol every bound record answers on @p module. */
void bindRecordProtocol(pybind11::module_& module);

}  // namespace sigil::python
