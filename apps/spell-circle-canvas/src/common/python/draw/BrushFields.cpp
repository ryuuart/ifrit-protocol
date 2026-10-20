/** @file
 * The brush's direction fields in Python: curl, vortex and wave called
 * at a position and a time, the field any callable makes, and the stock
 * fields by name.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <utility>

#include "BrushBinding.h"

namespace sigil::python {
using namespace brush_binding;

void bindBrushFields(py::module_& module) {
  auto vortex =
      py::reinterpret_borrow<py::class_<brush::Vortex>>(module.attr("Vortex"));
  auto wave =
      py::reinterpret_borrow<py::class_<brush::Wave>>(module.attr("Wave"));
  py::class_<brush::Curl>(module, "Curl")
      .def(py::init<uint32_t, float, float>(), py::arg("seed") = 0,
           py::arg("scale") = 0.004f, py::arg("drift") = 0.08f)
      .def_property_readonly("seed", &brush::Curl::seed)
      .def_property_readonly("scale", &brush::Curl::scale)
      .def_property_readonly("drift", &brush::Curl::drift)
      .def(
          "__call__",
          [](const brush::Curl& field, py::handle position, float seconds) {
            return field(point(position), seconds);
          },
          py::arg("position"), py::arg("seconds") = 0.0f);
  vortex.def(
      "__call__",
      [](const brush::Vortex& field, py::handle position, float seconds) {
        return field(point(position), seconds);
      },
      py::arg("position"), py::arg("seconds") = 0.0f);
  wave.def(
      "__call__",
      [](const brush::Wave& field, py::handle position, float seconds) {
        return field(point(position), seconds);
      },
      py::arg("position"), py::arg("seconds") = 0.0f);
  py::class_<Direction>(module, "Direction")
      .def(py::init(
               [](py::handle field) { return Direction{direction(field)}; }),
           py::arg("field"))
      .def(
          "__call__",
          [](const Direction& field, py::handle position, float seconds) {
            return field.value ? field.value(point(position), seconds) : 0.0f;
          },
          py::arg("position"), py::arg("seconds") = 0.0f);
  module.def("stockFields", [] {
    py::dict fields;
    for (auto& [name, value] : brush::stockFields())
      fields[py::str(name)] = py::cast(Direction{std::move(value)});
    return fields;
  });
}

}  // namespace sigil::python
