#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>

#include <optional>
#include <utility>

namespace sigil::python {

void bindComposeBrushMarks(pybind11::module_& module) {
  namespace py = pybind11;
  using compose::brush::Ribbon;
  py::module_ brushes = submodule(module, "compose.brush");

  py::class_<Ribbon> ribbon(
      brushes, "Ribbon",
      "A filled band swept along the outline it strokes, as wide across "
      "the outline as its width law says at each place along it, joined at "
      "the corners.");
  ribbon
      .def_property(
          "fill", [](const Ribbon& self) { return self.fill; },
          [](Ribbon& self, py::object value) { self.fill = fill(value); })
      .def_property(
          "material", [](const Ribbon& self) { return self.fillMaterial; },
          [](Ribbon& self, std::optional<material::Material> value) {
            self.fillMaterial = std::move(value);
          },
          "The material the band is painted with, ahead of `fill`.")
      .def_readwrite("width", &Ribbon::width)
      .def_readwrite("step", &Ribbon::step)
      .def_readwrite("join", &Ribbon::join)
      .def_readwrite("miterLimit", &Ribbon::miterLimit)
      .def("bleed", py::overload_cast<>(&Ribbon::bleed, py::const_))
      .def("isRunning", &Ribbon::isRunning)
      .def(py::self == py::self);
  copyProtocol(ribbon);
  py::implicitly_convertible<Ribbon, compose::Decoration>();

  brushes.def(
      "ribbon",
      [](geometry::path::Profile width, py::object paint) {
        if (py::isinstance<material::Material>(paint))
          return compose::brush::ribbon(std::move(width),
                                        paint.cast<material::Material>());
        return compose::brush::ribbon(std::move(width), fill(paint));
      },
      py::arg("width"), py::arg("paint"),
      "A ribbon of `width` — a number, a list of (along, width) stops or a "
      "geometry.path.Profile — painted with `paint`: a colour, a fill or a "
      "material, whose layers, surface and effects it keeps. Give it to "
      "stroke() to sweep it along a node's outline.");
}

}  // namespace sigil::python
