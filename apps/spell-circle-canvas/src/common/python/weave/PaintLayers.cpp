#include <pybind11/stl.h>
#include <sigilgeometry/path/Stroke.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/skia/Values.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/kit/PaintLayers.h>

namespace sigil::python {

namespace py = pybind11;

void bindWeaveKitPaintLayers(py::module_& module) {
  py::module_ kit = submodule(module, "weave.kit");
  kit.def(
      "dropShadow",
      [](py::handle ink, py::handle offset, float blurSigma, float spread,
         float intensity) {
        return weave::kit::dropShadow(color(ink).toSkColor(), point(offset),
                                      blurSigma, spread, intensity);
      },
      py::arg("color") = "#00000066", py::arg("offset") = py::make_tuple(2, 2),
      py::arg("blurSigma") = 2.0f, py::arg("spread") = 0.0f,
      py::arg("intensity") = 1.0f);
  kit.def(
      "glow",
      [](py::handle ink, float blurSigma, float spread, float intensity) {
        return weave::kit::glow(color(ink).toSkColor(), blurSigma, spread,
                                intensity);
      },
      py::arg("color"), py::arg("blurSigma"), py::arg("spread") = 0.0f,
      py::arg("intensity") = 1.0f);
  kit.def(
      "outline",
      [](py::handle ink, float width, geometry::path::Join join) {
        return weave::kit::outline(color(ink).toSkColor(), width, join);
      },
      py::arg("color"), py::arg("width"),
      py::arg("join") = geometry::path::Join::Round);
}

}  // namespace sigil::python
