#include <pybind11/stl.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/compose/Registration.h>

#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace composeKit = compose::kit;
using kit::field;
using kit::record;

void bindComposeKitRows(py::module_& root) {
  auto module = root.attr("compose").attr("kit").cast<py::module_>();
  auto column = record<composeKit::Column>(module, "Column");
  field(column, "head", &composeKit::Column::head);
  field(column, "width", &composeKit::Column::width);
  field(column, "figure", &composeKit::Column::figure);

  auto reading = record<composeKit::Reading>(module, "Reading");
  reading.doc() =
      "One reading: what was measured, what it came to, and what that "
      "means where the figure alone would not say, with a swatch before "
      "the name for a row that is also a key.";
  field(reading, "name", &composeKit::Reading::name);
  field(reading, "value", &composeKit::Reading::value);
  field(reading, "note", &composeKit::Reading::note);
  field(reading, "swatch", &composeKit::Reading::swatch);
  field(reading, "ink", &composeKit::Reading::ink);
  auto rows = record<composeKit::Rows>(module, "Rows");
  rows.doc() =
      "How a row is set: the measure it ranges across, the name column's "
      "width, the air between rows and between a name and its figure, the "
      "divider and the swatch. No type: the lines are set by the sheet in "
      "force.";
  field(rows, "measure", &composeKit::Rows::measure);
  field(rows, "nameMeasure", &composeKit::Rows::nameMeasure);
  field(rows, "gap", &composeKit::Rows::gap);
  field(rows, "labelGap", &composeKit::Rows::labelGap);
  field(rows, "divider", &composeKit::Rows::divider);
  field(rows, "dividerWidth", &composeKit::Rows::dividerWidth);
  field(rows, "swatchSide", &composeKit::Rows::swatchSide);
  field(rows, "swatchCorners", &composeKit::Rows::swatchCorners);
  module.def("reading", &composeKit::reading, py::arg("one"),
             py::arg("how") = composeKit::Rows{});
  module.def(
      "readout",
      [](const std::vector<composeKit::Reading>& readings,
         const composeKit::Rows& how) {
        return composeKit::readout(readings, how);
      },
      py::arg("rows"), py::arg("how") = composeKit::Rows{});
}

}  // namespace sigil::python
