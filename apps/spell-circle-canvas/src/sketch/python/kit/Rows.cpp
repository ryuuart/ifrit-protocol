#include <sigilmeasure/check/Check.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Rows.h>
#include <sigilsketch/kit/Verdict.h>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

void bindSketchKitRows(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  auto column = record<sketchKit::Column>(module, "Column");
  field(column, "head", &sketchKit::Column::head);
  field(column, "width", &sketchKit::Column::width);
  field(column, "figure", &sketchKit::Column::figure);

  py::enum_<sketchKit::VerdictRows>(module, "VerdictRows")
      .value("Every", sketchKit::VerdictRows::Every)
      .value("Judged", sketchKit::VerdictRows::Judged)
      .value("Failures", sketchKit::VerdictRows::Failures);
  auto verdict = record<sketchKit::Verdict>(module, "Verdict");
  field(verdict, "rows", &sketchKit::Verdict::rows);
  field(verdict, "columns", &sketchKit::Verdict::columns);
  field(verdict, "swatches", &sketchKit::Verdict::swatches);
  field(verdict, "ruled", &sketchKit::Verdict::ruled);
  field(verdict, "summary", &sketchKit::Verdict::summary);
  field(verdict, "passed", &sketchKit::Verdict::passed);
  field(verdict, "failed", &sketchKit::Verdict::failed);
  field(verdict, "unjudged", &sketchKit::Verdict::unjudged);
  module.def("verdict", &sketchKit::verdict, py::arg("table"),
             py::arg("how") = sketchKit::Verdict{});
}

}  // namespace sigil::sketch::python
