#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/skia/Values.h>

namespace sigil::python {
namespace py = pybind11;
namespace pattern = material::pattern;

void bindMaterialPattern(py::module_& module) {
  constexpr auto fluent = py::return_value_policy::reference_internal;
  auto patterns = submodule(module, "material.pattern");
  py::class_<pattern::Tile>(patterns, "Tile")
      .def("seed", &pattern::Tile::seed, py::arg("seed"), fluent)
      .def("scale", py::overload_cast<float>(&pattern::Tile::scale),
           py::arg("factor"), fluent)
      .def("rotate", py::overload_cast<float>(&pattern::Tile::rotate),
           py::arg("degrees"), fluent)
      .def("offset", py::overload_cast<SkPoint>(&pattern::Tile::offset),
           py::arg("offset"), fluent)
      .def("image", &pattern::Tile::image)
      .def("paint", [](const pattern::Tile& tile) {
        return material::skia::Paint::shader(tile.texture().shader());
      });
  patterns.def(
      "gridLines",
      [](float spacing, float width, py::handle value) {
        return pattern::gridLines(spacing, width, materialColor(value));
      },
      py::arg("spacing"), py::arg("width"), py::arg("color"));
  patterns.def(
      "stripes",
      [](float on, float off, py::handle value) {
        return pattern::stripes(on, off, materialColor(value));
      },
      py::arg("on"), py::arg("off"), py::arg("color"));
  patterns.def(
      "checker",
      [](float cell, py::handle first, py::handle second) {
        return pattern::checker(cell, materialColor(first),
                                materialColor(second));
      },
      py::arg("cell"), py::arg("a"), py::arg("b"));
  patterns.def(
      "halftone",
      [](float spacing, float radius, py::handle value, bool staggered) {
        return pattern::halftone(spacing, radius, materialColor(value),
                                 staggered);
      },
      py::arg("spacing"), py::arg("radius"), py::arg("color"),
      py::arg("staggered") = true);
}

}  // namespace sigil::python
