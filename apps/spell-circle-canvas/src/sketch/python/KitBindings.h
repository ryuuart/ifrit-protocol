#pragma once

#include <pybind11/pybind11.h>

namespace sigil::sketch::kit {
struct Stage;
}

namespace sigil::sketch::python {
void bindSketchKit(pybind11::module_& module);
/** Declares a stage through a validated session context. */
void stageContext(pybind11::handle context, const kit::Stage& stage);
}  // namespace sigil::sketch::python
