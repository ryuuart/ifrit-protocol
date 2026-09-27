#include <sigilcompose/kit/Rows.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/compose/Registration.h>

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
}

}  // namespace sigil::python
