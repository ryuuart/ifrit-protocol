#include <pybind11/operators.h>
#include <sigilcore/compute/Angle.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/core/Registration.h>

#include <string>

namespace sigil::python {
namespace py = pybind11;

void bindCoreAngle(py::module_& module) {
  auto angle = submodule(module, "core.angle");
  angle.doc() =
      "An angle that names its own measure. Every angle slot reads a bare "
      "number in its own measure, degrees on compose, material, geometry "
      "and the canvas, the pen's angleMode beside a pen, radians inside a "
      "brush value; it also reads one of these, and CSS angle text such as "
      "'45deg', which name theirs.";
  using Angle = core::angle::Angle;
  py::class_<Angle> type(angle, "Angle",
                         "An angle and the measure it is written in. A slot "
                         "converts it to the measure that slot counts in, so "
                         "it means the same angle wherever it is passed.");
  type.def(py::init([](const std::string& text) { return angleFromText(text); }),
           py::arg("text"),
           "The angle CSS text writes: a number with its unit straight after "
           "it, 'deg', 'rad', 'grad' or 'turn', in any case. Text without a "
           "unit, or with another, raises ValueError.")
      .def("inDegrees", &Angle::inDegrees, "This angle in degrees.")
      .def("inRadians", &Angle::inRadians, "This angle in radians.")
      .def("inTurns", &Angle::inTurns, "This angle in whole turns.")
      .def_property_readonly(
          "value", [](const Angle& self) { return self.value; },
          "The number as written, in `unit`.")
      .def_property_readonly(
          "unit",
          [](const Angle& self) {
            return std::string(core::angle::unitName(self.unit));
          },
          "The CSS name of the measure the number is written in: 'deg', "
          "'rad', 'grad' or 'turn'.")
      .def(py::self == py::self)
      .def(py::self != py::self)
      .def("__hash__",
           [](const Angle& self) {
             return py::hash(py::make_tuple(self.value, (int)self.unit));
           })
      .def("__str__", [](const Angle& self) { return core::angle::text(self); })
      .def("__repr__",
           [](const Angle& self) {
             return "Angle('" + core::angle::text(self) + "')";
           })
      .def("__copy__", [](const Angle& self) { return self; })
      .def(
          "__deepcopy__",
          [](const Angle& self, const DeepCopyMemo&) { return self; },
          py::arg("memo"));
  angle.def("degrees", &Angle::degrees, py::arg("value"),
            "An angle of `value` degrees, 360 to the turn.");
  angle.def("radians", &Angle::radians, py::arg("value"),
            "An angle of `value` radians, two pi to the turn.");
  angle.def("gradians", &Angle::gradians, py::arg("value"),
            "An angle of `value` gradians, 400 to the turn.");
  angle.def("turns", &Angle::turns, py::arg("value"),
            "An angle of `value` whole turns.");
}

}  // namespace sigil::python
