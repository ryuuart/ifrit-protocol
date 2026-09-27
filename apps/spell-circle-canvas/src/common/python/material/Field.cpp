#include <sigilmaterial/field/Field.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindMaterialField(py::module_& module) {
  auto fields = submodule(module, "material.field");
  fields.def("noise", &material::field::noise, py::arg("frequency"),
             py::arg("octaves") = 4, py::arg("seed") = 1,
             py::arg("turbulence") = false);
  fields.def("grain", &material::field::grain, py::arg("frequency"),
             py::arg("octaves") = 4, py::arg("seed") = 1,
             py::arg("contrast") = 1, py::arg("stretch") = 1);
  fields.def("ripple", &material::field::ripple, py::arg("amplitudePx"),
             py::arg("wavelengthPx"), py::arg("phase") = 0.0f,
             py::arg("vertical") = false);
  fields.def(
      "halftoneRamp",
      [](float spacing, float minimumRadius, float maximumRadius, py::handle value,
         float angleDegrees, float rampFrom, float rampTo) {
        return material::field::halftoneRamp(spacing, minimumRadius, maximumRadius,
                                             materialColor(value),
                                             angleDegrees, rampFrom, rampTo);
      },
      py::arg("spacing"), py::arg("minimumRadius"), py::arg("maximumRadius"), py::arg("color"),
      py::arg("angleDegrees") = 0.0f, py::arg("rampFrom") = 0.0f,
      py::arg("rampTo") = 1.0f);
}

}  // namespace sigil::python
