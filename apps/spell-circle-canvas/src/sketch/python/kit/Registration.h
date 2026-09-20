#pragma once

/** @file
 * Every registration this library's packages add to the extension
 * module: the specimen kit a sketch is dressed with.
 *
 * One package owns one of these functions and the source file that
 * defines it, so two authors never write in one file.
 */

#include <pybind11/pybind11.h>

namespace sigil::sketch::kit {
struct Stage;
}

namespace sigil::sketch::python {

/** Registers the specimen kit's records on @p module. */
void bindSketchKit(pybind11::module_& module);
/** Registers the plot frame and every chart layer on @p module. */
void bindSketchKitCharts(pybind11::module_& module);
/** Registers theme extras, passage, Document and Instrument on @p
 *  module. */
void bindSketchKitContent(pybind11::module_& module);
/** Registers legends, swatch strips, chips, meters and gauges on @p
 *  module. */
void bindSketchKitLegends(pybind11::module_& module);
/** Registers backdrop, panel, frame, scrollbar, ticker and timeline on
 *  @p module. */
void bindSketchKitPanels(pybind11::module_& module);
/** Registers title cards, section headers, readouts, tables and bars on
 *  @p module. */
void bindSketchKitRows(pybind11::module_& module);
/** Registers the console and the channel on @p module. */
void bindSketchKitStreams(pybind11::module_& module);

/** Declares a stage through a validated session context. */
void stageContext(pybind11::handle context, const kit::Stage& stage);

}  // namespace sigil::sketch::python
