#include <sigilmaterial/field/Field.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindMaterialField(py::module_& module) {
  using material::field::CrtOverlayParameters;
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
      [](float spacing, float rMin, float rMax, py::handle value,
         float angleDegrees, float rampFrom, float rampTo) {
        return material::field::halftoneRamp(spacing, rMin, rMax,
                                             materialColor(value),
                                             angleDegrees, rampFrom, rampTo);
      },
      py::arg("spacing"), py::arg("rMin"), py::arg("rMax"), py::arg("color"),
      py::arg("angleDeg") = 0.0f, py::arg("rampFrom") = 0.0f,
      py::arg("rampTo") = 1.0f);
  bindRecord<CrtOverlayParameters>(fields, "CrtOverlayParameters",
                                   "Unknown CRT field: ")
      .def_readwrite("uScanPitch", &CrtOverlayParameters::uScanPitch)
      .def_readwrite("uScanStrength", &CrtOverlayParameters::uScanStrength)
      .def_readwrite("uVigInner", &CrtOverlayParameters::uVigInner)
      .def_readwrite("uVigOuter", &CrtOverlayParameters::uVigOuter)
      .def_readwrite("uVigStrength", &CrtOverlayParameters::uVigStrength)
      .def_readwrite("uSqueeze", &CrtOverlayParameters::uSqueeze)
      .def_readwrite("uBeamPitch", &CrtOverlayParameters::uBeamPitch)
      .def_readwrite("uBeamFalloff", &CrtOverlayParameters::uBeamFalloff)
      .def_readwrite("uBeamStrength", &CrtOverlayParameters::uBeamStrength)
      .def_readwrite("uBeatPitch", &CrtOverlayParameters::uBeatPitch)
      .def_readwrite("uBeatFalloff", &CrtOverlayParameters::uBeatFalloff)
      .def_readwrite("uBeatStrength", &CrtOverlayParameters::uBeatStrength)
      .def_readwrite("uGrain", &CrtOverlayParameters::uGrain);
  fields.def("crtOverlay",
             py::overload_cast<const CrtOverlayParameters&>(
                 &material::field::crtOverlay),
             py::arg("parameters") = CrtOverlayParameters{});
}

}  // namespace sigil::python
